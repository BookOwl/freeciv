/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "capability.h"
#include "city.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "nation.h"
#include "packets.h"
#include "registry.h"
#include "tech.h"
#include "unit.h"

#include "ruleset.h"

static char *openload_ruleset_file(struct section_file *file,
				   char *subdir, char *whichset);
static char *check_ruleset_capabilities(struct section_file *file,
					char *required,	char *filename);
static int lookup_unit_type(struct section_file *file, char *prefix,
			    char *entry, int required, char *filename,
			    char *description);
static int lookup_tech(struct section_file *file, char *prefix,
		       char *entry, int required, char *filename,
		       char *description);
static char *lookup_helptext(struct section_file *file, char *prefix);

static enum tile_terrain_type lookup_terrain(char *name, int this);

static void load_ruleset_techs(char *ruleset_subdir);
static void load_ruleset_units(char *ruleset_subdir);
static void load_ruleset_buildings(char *ruleset_subdir);
static void load_ruleset_terrain(char *ruleset_subdir);
static void load_ruleset_governments(char *ruleset_subdir);
static void load_ruleset_nations(char *ruleset_subdir);

static void send_ruleset_techs(struct player *dest);
static void send_ruleset_units(struct player *dest);
static void send_ruleset_buildings(struct player *dest);
static void send_ruleset_terrain(struct player *dest);
static void send_ruleset_governments(struct player *dest);
static void send_ruleset_nations(struct player *dest);

/**************************************************************************
  Do initial section_file_load on a ruleset file.
  "subdir" = "default", "civ1", "custom", ...
  "whichset" = "techs", "units", "buildings", "terrain" (...)
  returns ptr to static memory with full filename including data directory. 
**************************************************************************/
static char *openload_ruleset_file(struct section_file *file,
				   char *subdir, char *whichset)
{
  char filename1[512], filename2[512], *dfilename;

  sprintf(filename1, "%s_%s.ruleset", subdir, whichset);
  dfilename = datafilename(filename1);
  if (!dfilename) {
    sprintf(filename2, "%s/%s.ruleset", subdir, whichset);
    dfilename = datafilename(filename2);
    if (!dfilename) {
      freelog(LOG_FATAL, "Could not find readable ruleset file \"%s\"",
	      filename1);
      freelog(LOG_FATAL, "or \"%s\" in data path", filename2);
      freelog(LOG_FATAL, "The data path may be set via"
	      " the environment variable FREECIV_PATH");
      freelog(LOG_FATAL, "Current data path is: \"%s\"", datafilename(NULL));
      exit(1);
    }
  }
  if (!section_file_load(file,dfilename)) {
    freelog(LOG_FATAL, "Could not load ruleset file %s", dfilename);
    exit(1);
  }
  return dfilename;
}

/**************************************************************************
  Ruleset files should have a capabilities string datafile.options
  This gets and returns that string, and checks that the required
  capabilites specified are satisified.
**************************************************************************/
static char *check_ruleset_capabilities(struct section_file *file,
					char *us_capstr, char *filename)
{
  char *datafile_options;
  
  datafile_options = secfile_lookup_str(file, "datafile.options");
  if (!has_capabilities(us_capstr, datafile_options)) {
    freelog(LOG_FATAL, "Ruleset datafile appears incompatible");
    freelog(LOG_FATAL, "file: \"%s\"", filename);
    freelog(LOG_FATAL, "file options: %s", datafile_options);
    freelog(LOG_FATAL, "supported options: %s", us_capstr);
    exit(1);
  }
  if (!has_capabilities(datafile_options, us_capstr)) {
    freelog(LOG_FATAL,
	    "Ruleset datafile claims required option(s) which we don't support");
    freelog(LOG_FATAL, "file: \"%s\"", filename);
    freelog(LOG_FATAL, "file options: %s", datafile_options);
    freelog(LOG_FATAL, "supported options: %s", us_capstr);
    exit(1);
  }
  return datafile_options;
}


/**************************************************************************
 Lookup a string prefix.entry in the file and return the corresponding
 unit_type id.  If (!required), return -1 if match "None" or can't match.
 If (required), die if can't match.
 If description is not NULL, it is used in the warning message
 instead of prefix (eg pass unit->name instead of prefix="units2.u27")
**************************************************************************/
static int lookup_unit_type(struct section_file *file, char *prefix,
			    char *entry, int required, char *filename,
			    char *description)
{
  char *sval;
  int i;
  
  sval = secfile_lookup_str(file, "%s.%s", prefix, entry);
  if (!required && strcmp(sval, "None")==0) {
    i = -1;
  } else {
    i = find_unit_type_by_name(sval);
    if (i==U_LAST) {
      freelog((required?LOG_FATAL:LOG_NORMAL),
	   "for %s %s couldn't match unit_type \"%s\" (%s)",
	   (description?description:prefix), entry, sval, filename);
      if (required) {
	exit(1);
      } else {
	i = -1;
      }
    }
  }
  return i;
}

/**************************************************************************
 Lookup a string prefix.entry in the file and return the corresponding
 advances id.  If (!required), return A_LAST if match "Never" or can't match.
 If (required), die if can't match.  Note the first tech should have
 name "None" so that will always match.
 If description is not NULL, it is used in the warning message
 instead of prefix (eg pass unit->name instead of prefix="units2.u27")
**************************************************************************/
static int lookup_tech(struct section_file *file, char *prefix,
		       char *entry, int required, char *filename,
		       char *description)
{
  char *sval;
  int i;
  
  sval = secfile_lookup_str(file, "%s.%s", prefix, entry);
  if (!required && strcmp(sval, "Never")==0) {
    i = A_LAST;
  } else {
    i = find_tech_by_name(sval);
    if (i==A_LAST) {
      freelog((required?LOG_FATAL:LOG_NORMAL),
	   "for %s %s couldn't match tech \"%s\" (%s)",
	   (description?description:prefix), entry, sval, filename);
      if (required) {
	exit(1);
      } else {
	i = A_LAST;
      }
    }
  }
  return i;
}

/**************************************************************************
 Lookup a prefix.entry string vector in the file and fill in the
 array, which should hold MAX_NUM_TECH_LIST items.  Output array is
 A_LAST terminated, and all entries before that are guaranteed to
 tech_exist().  There should be at least one value, but it may be
 "", meaning empty list.
**************************************************************************/
static void lookup_tech_list(struct section_file *file, char *prefix,
			     char *entry, int *output, char *filename)
{
  char **slist;
  int i, nval;

  /* pre-fill with A_LAST: */
  for(i=0; i<MAX_NUM_TECH_LIST; i++) {
    output[i] = A_LAST;
  }
  slist = secfile_lookup_str_vec(file, &nval, "%s.%s", prefix, entry);
  if (nval==0) {
    freelog(LOG_FATAL, "Missing string vector %s.%s (%s)",
	    prefix, entry, filename);
    exit(1);
  }
  if (nval>MAX_NUM_TECH_LIST) {
    freelog(LOG_FATAL, "String vector %s.%s too long (%d, max %d) (%s)",
	    prefix, entry, nval, MAX_NUM_TECH_LIST, filename);
    exit(1);
  }
  if (nval==1 && strcmp(slist[0], "")==0) {
    free(slist);
    return;
  }
  for (i=0; i<nval; i++) {
    char *sval = slist[i];
    int tech = find_tech_by_name(sval);
    if (tech==A_LAST) {
      freelog(LOG_FATAL, "For %s %s (%d) couldn't match tech \"%s\" (%s)",
	      prefix, entry, i, sval, filename);
      exit(1);
    }
    if (!tech_exists(tech)) {
      freelog(LOG_FATAL, "For %s %s (%d) tech \"%s\" is removed (%s)",
	      prefix, entry, i, sval, filename);
      exit(1);
    }
    output[i] = tech;
    freelog(LOG_DEBUG, "%s.%s,%d %s %d", prefix, entry, i, sval, tech);
  }
  free(slist);
  return;
}

/**************************************************************************
  Lookup entry in the file and return the corresponding government index;
  dies if can't find/match.  filename is for error message.
**************************************************************************/
static int lookup_government(struct section_file *file, char *entry,
			     char *filename)
{
  char *sval;
  struct government *gov;
  
  sval = secfile_lookup_str(file, "%s", entry);
  gov = find_government_by_name(sval);
  if (gov==NULL) {
    freelog(LOG_FATAL, "for %s couldn't match government \"%s\" (%s)",
	    entry, sval, filename);
    exit(1);
  }
  return gov->index;
}

/**************************************************************************
  Lookup entry in the file and return the corresponding city cost:
  value if int, or G_CITY_SIZE_FREE is entry is "City_Size".
  Dies if gets some other string.  filename is for error message.
**************************************************************************/
static int lookup_city_cost(struct section_file *file, char *prefix,
			    char *entry, char *filename)
{
  char *sval;
  int ival = 0;
  
  sval = secfile_lookup_str_int(file, &ival, "%s.%s", prefix, entry);
  if (sval) {
    if (mystrcasecmp(sval, "City_Size") == 0) {
      ival = G_CITY_SIZE_FREE;
    } else {
      freelog(LOG_FATAL, "Bad %s \"%s\" for %s (%s)",
	      entry, sval, prefix, filename);
      exit(1);
    }
  }
  return ival;
}

/**************************************************************************
  Lookup optional helptext, returning allocated memory or NULL.
**************************************************************************/
static char *lookup_helptext(struct section_file *file, char *prefix)
{
  char *sval;
  
  sval = secfile_lookup_str_default(file, NULL, "%s.helptext", prefix);
  if (sval) {
    sval = skip_leading_spaces(sval);
    if (strlen(sval)) {
      return mystrdup(sval);
    }
  }
  return NULL;
}

#ifdef UNUSED
/**************************************************************************
 Return the index of the name in the list, or -1
**************************************************************************/
static int match_name_from_list(char *name, char **list, int n_list)
{
  int i;

  for(i=0; i<n_list; i++) {
    if(strcmp(name,list[i])==0) {
      return i;
    }
  }
  return -1;
}
#endif


/**************************************************************************
 Lookup a terrain name in the tile_types array; return its index.
**************************************************************************/
static enum tile_terrain_type lookup_terrain(char *name, int this)
{
  int i;

  if ((!(*name)) || (0 == strcmp(name, "none")) || (0 == strcmp(name, "no")))
    {
      return (T_LAST);
    }
  else if (0 == strcmp(name, "yes"))
    {
      return (this);
    }

  for (i = T_FIRST; i < T_COUNT; i++)
    {
      if (0 == strcmp(name, tile_types[i].terrain_name))
	{
	  return (i);
	}
    }

  return (T_UNKNOWN);
}

/**************************************************************************
  ...  
  This must be done before the other load_ruleset_ functions,
  since other rulesets want to match tech names.
**************************************************************************/
static void load_ruleset_techs(char *ruleset_subdir)
{
  struct section_file file;
  char *filename, *datafile_options;
  char **sec;
  struct advance *a;
  int num_techs; /* number of techs in the ruleset (means without A_NONE)*/
  int i;
  
  filename = openload_ruleset_file(&file, ruleset_subdir, "techs");
  datafile_options = check_ruleset_capabilities(&file, "+1.8.2a +no_a_none", filename);
  section_file_lookup(&file,"datafile.description"); /* unused */

  /* The names: */
  sec = secfile_get_secnames_prefix(&file, "advance_", &num_techs);
  freelog(LOG_VERBOSE, "%d advances (including possibly unused)", num_techs);
  if(num_techs == 0) {
    freelog(LOG_FATAL, "No Advances?! (%s)", filename);
    exit(1);
  }

  if(num_techs + A_FIRST > A_LAST) {
    freelog(LOG_FATAL, "Too many advances (%d, max %d) (%s)",
	    num_techs, A_LAST-A_FIRST, filename);
    exit(1);
  }

  /* Initialize dummy tech A_NONE */
  strcpy(advances[A_NONE].name, "None");
  advances[A_NONE].req[0] = A_NONE;
  advances[A_NONE].req[1] = A_NONE;
  advances[A_NONE].flags = 0;

  game.num_tech_types = num_techs + 1; /* includes A_NONE */
  a = &advances[A_FIRST];

  /* have to read all names first, so can lookup names for reqs! */
  for( i=0; i<num_techs; i++ ) {
    strcpy(a->name, secfile_lookup_str(&file, "%s.name", sec[i]));
    a++;
  }

  a = &advances[A_FIRST];
  
  for( i=0; i<num_techs; i++ ) {
    char *sval, **slist;
    int j,ival,nval;

    a->req[0] = lookup_tech(&file, sec[i], "req1", 0, filename, a->name);
    a->req[1] = lookup_tech(&file, sec[i], "req2", 0, filename, a->name);
    
    if ((a->req[0]==A_LAST && a->req[1]!=A_LAST) ||
	(a->req[0]!=A_LAST && a->req[1]==A_LAST)) {
      freelog(LOG_NORMAL, "for tech %s: \"Never\" with non-\"Never\" (%s)",
	   a->name, filename);
      a->req[0] = a->req[1] = A_LAST;
    }
    if (a->req[0]==A_NONE && a->req[1]!=A_NONE) {
      freelog(LOG_NORMAL, "tech %s: should have \"None\" second (%s)",
	   a->name, filename);
      a->req[0] = a->req[1];
      a->req[1] = A_NONE;
    }

    a->flags = 0;

    slist = secfile_lookup_str_vec(&file, &nval, "%s.flags", sec[i]);
    for(j=0; j<nval; j++) {
      sval = slist[j];
      if(strcmp(sval,"")==0) {
        continue;
      }
      ival = tech_flag_from_str(sval);
      if (ival==TF_LAST) {
        freelog(LOG_NORMAL, "for advance_type \"%s\": bad flag name \"%s\" (%s)",
                a->name, sval, filename);
      }
      a->flags |= (1<<ival);
    }
    free(slist);

    a->helptext = lookup_helptext(&file, sec[i]);

    a++;
  }

  /* Some more consistency checking: 
     Non-removed techs depending on removed techs is too
     broken to fix by default, so die.
  */   
  for( i=A_FIRST; i<game.num_tech_types; i++ ) {
    if (tech_exists(i)) {
      a = &advances[i];
      if (!tech_exists(a->req[0])) {
	freelog(LOG_FATAL, "tech \"%s\": req1 leads to removed tech \"%s\" (%s)",
	     a->name, advances[a->req[0]].name, filename);
	exit(1);
      } 
      if (!tech_exists(a->req[1])) {
	freelog(LOG_FATAL, "tech \"%s\": req2 leads to removed tech \"%s\" (%s)",
	     a->name, advances[a->req[1]].name, filename);
	exit(1);
      }
    }
  } 

  /* Should be removed and use the flag directly in the future
     to allow more bonus techs -- sb */  
  game.rtech.get_bonus_tech = find_tech_by_flag(0,TF_BONUS_TECH);

  section_file_check_unused(&file, filename);
  section_file_free(&file);
}

/**************************************************************************
...  
**************************************************************************/
static void load_ruleset_units(char *ruleset_subdir)
{
  struct section_file the_file, *file = &the_file;
  char *filename, *datafile_options;
  struct unit_type *u;
  int i, j, ival, nval;
  int max_hp, max_firepower;
  char *sval, **slist, **sec;

  filename = openload_ruleset_file(file, ruleset_subdir, "units");
  datafile_options
    = check_ruleset_capabilities(file, "+1.8.2a +tilespec", filename);
  section_file_lookup(file,"datafile.description"); /* unused */

  max_hp =
    secfile_lookup_int_default(file, 0, "units_adjust.max_hitpoints");
  max_firepower =
    secfile_lookup_int_default(file, 0, "units_adjust.max_firepower");
  game.firepower_factor =
    secfile_lookup_int_default(file, 1, "units_adjust.firepower_factor");

  /* The names: */
  sec = secfile_get_secnames_prefix(file, "unit_", &nval);
  freelog(LOG_VERBOSE, "%d unit types (including possibly unused)", nval);
  if(nval == 0) {
    freelog(LOG_FATAL, "No units?! (%s)", filename);
    exit(1);
  }
  if(nval > U_LAST) {
    freelog(LOG_FATAL, "Too many units (%d, max %d) (%s)",
	    nval, U_LAST, filename);
    exit(1);
  }
  game.num_unit_types = nval;
  
  for( i=0; i<game.num_unit_types; i++ ) {
    u = &unit_types[i];
    strcpy(u->name, secfile_lookup_str(file, "%s.name", sec[i]));
  }

  /* Tech requirement is used to flag removed unit_types, which
     we might want to know for other fields.  After this we
     can use unit_type_exists()
  */
  for( i=0; i<game.num_unit_types; i++ ) {
    u = &unit_types[i];
    u->tech_requirement = lookup_tech(file, sec[i], "tech_req",
				      0, filename, u->name);
  }
  for( i=0; i<game.num_unit_types; i++ ) {
    u = &unit_types[i];
    if (unit_type_exists(i)) {
      u->obsoleted_by = lookup_unit_type(file, sec[i],
					 "obsolete_by", 0, filename, u->name);
    } else {
      section_file_lookup(file, "%s.obsolete_by", sec[i]);  /* unused */
      u->obsoleted_by = -1;
    }
  }

  /* main stats: */
  for( i=0; i<game.num_unit_types; i++ ) {
    u = &unit_types[i];
    
    sval = secfile_lookup_str(file, "%s.move_type", sec[i]);
    ival = unit_move_type_from_str(sval);
    if (ival==0) {
      freelog(LOG_FATAL, "for unit_type \"%s\": bad move_type %s (%s)",
	   u->name, sval, filename);
      exit(1);
    }
    u->move_type = ival;
    
    strncpy(u->graphic_str, secfile_lookup_str(file,"%s.graphic", sec[i]),
	    sizeof(u->graphic_str)-1);
    strncpy(u->graphic_alt, secfile_lookup_str(file,"%s.graphic_alt", sec[i]),
	    sizeof(u->graphic_alt)-1);
    
    u->build_cost =
      secfile_lookup_int(file,"%s.build_cost", sec[i]);
    u->attack_strength =
      secfile_lookup_int(file,"%s.attack", sec[i]);
    u->defense_strength =
      secfile_lookup_int(file,"%s.defense", sec[i]);
    u->move_rate =
      3*secfile_lookup_int(file,"%s.move_rate", sec[i]);
    
    u->vision_range =
      secfile_lookup_int(file,"%s.vision_range", sec[i]);
    u->transport_capacity =
      secfile_lookup_int(file,"%s.transport_cap", sec[i]);
    u->hp = secfile_lookup_int(file,"%s.hitpoints", sec[i]);
    if( max_hp && u->hp > max_hp ) {
      u->hp = max_hp;
    }
    u->firepower = secfile_lookup_int(file,"%s.firepower", sec[i]);
    if( max_firepower && u->firepower > max_firepower ) {
      u->firepower = max_firepower;
    }
    u->fuel = secfile_lookup_int(file,"%s.fuel", sec[i]);

    u->happy_cost  = secfile_lookup_int(file, "%s.uk_happy", sec[i]);
    u->shield_cost = secfile_lookup_int(file, "%s.uk_shield", sec[i]);
    u->food_cost   = secfile_lookup_int(file, "%s.uk_food", sec[i]);
    u->gold_cost   = secfile_lookup_int(file, "%s.uk_gold", sec[i]);

    u->helptext = lookup_helptext(file, sec[i]);
  }
  
  /* flags */
  for(i=0; i<game.num_unit_types; i++) {
    u = &unit_types[i];
    u->flags = 0;
    
    slist = secfile_lookup_str_vec(file, &nval, "%s.flags", sec[i]);
    for(j=0; j<nval; j++) {
      sval = slist[j];
      if(strcmp(sval,"")==0) {
	continue;
      }
      ival = unit_flag_from_str(sval);
      if (ival==F_LAST) {
	freelog(LOG_NORMAL, "for unit_type \"%s\": bad flag name \"%s\" (%s)",
	     u->name, sval, filename);
      }
      u->flags |= (1<<ival);

      if(ival == F_PARATROOPERS) {
        u->paratroopers_range = secfile_lookup_int(file,
            "%s.paratroopers_range", sec[i]);
        u->paratroopers_mr_req = 3*secfile_lookup_int(file,
            "%s.paratroopers_mr_req", sec[i]);
        u->paratroopers_mr_sub = 3*secfile_lookup_int(file,
            "%s.paratroopers_mr_sub", sec[i]);
      } else {
        u->paratroopers_range = 0;
        u->paratroopers_mr_req = 0;
        u->paratroopers_mr_sub = 0;
      }
    }
    free(slist);
  }
    
  /* roles */
  for(i=0; i<game.num_unit_types; i++) {
    u = &unit_types[i];
    u->roles = 0;
    
    slist = secfile_lookup_str_vec(file, &nval, "%s.roles", sec[i] );
    for(j=0; j<nval; j++) {
      sval = slist[j];
      if(strcmp(sval,"")==0) {
	continue;
      }
      ival = unit_role_from_str(sval);
      if (ival==L_LAST) {
	freelog(LOG_NORMAL, "for unit_type \"%s\": bad role name \"%s\" (%s)",
	     u->name, sval, filename);
      }
      u->roles |= (1<<ival);
    }
    free(slist);
  }
  free(sec);

  lookup_tech_list(file, "u_specials", "partisan_req",
		   game.rtech.partisan_req, filename);


  section_file_check_unused(file, filename);
  section_file_free(file);

  /* Some more consistency checking: */
  for( i=0; i<game.num_unit_types; i++ ) {
    if (unit_type_exists(i)) {
      u = &unit_types[i];
      if (!tech_exists(u->tech_requirement)) {
	freelog(LOG_NORMAL, "unit_type \"%s\" depends on removed tech \"%s\" (%s)",
	     u->name, advances[u->tech_requirement].name, filename);
	u->tech_requirement = A_LAST;
      }
      if (u->obsoleted_by!=-1 && !unit_type_exists(u->obsoleted_by)) {
	freelog(LOG_NORMAL, "unit_type \"%s\" obsoleted by removed unit \"%s\" (%s)",
	     u->name, unit_types[u->obsoleted_by].name, filename);
	u->obsoleted_by = -1;
      }
    }
  }

  /* Setup roles and flags pre-calcs: */
  role_unit_precalcs();
     
  /* Check some required flags and roles etc: */
  if(num_role_units(F_SETTLERS)==0) {
    freelog(LOG_FATAL, "No flag=settler units? (%s)", filename);
    exit(1);
  }
  if(num_role_units(L_EXPLORER)==0) {
    freelog(LOG_FATAL, "No role=explorer units? (%s)", filename);
    exit(1);
  }
  if(num_role_units(L_FERRYBOAT)==0) {
    freelog(LOG_FATAL, "No role=ferryboat units? (%s)", filename);
    exit(1);
  }
  if(num_role_units(L_HUT)==0) {
    freelog(LOG_FATAL, "No role=hut units? (%s)", filename);
    exit(1);
  }
  if(num_role_units(L_FIRSTBUILD)==0) {
    freelog(LOG_FATAL, "No role=firstbuild units? (%s)", filename);
    exit(1);
  }
  u = &unit_types[get_role_unit(L_FIRSTBUILD,0)];
  if(u->tech_requirement!=A_NONE) {
    freelog(LOG_FATAL, "First role=firstbuild unit (%s) should have req None (%s)",
	 u->name, filename);
    exit(1);
  }

  /* pre-calculate game.rtech.nav (tech for non-trireme ferryboat) */
  game.rtech.nav = A_LAST;
  for(i=0; i<num_role_units(L_FERRYBOAT); i++) {
    j = get_role_unit(L_FERRYBOAT,i);
    if (!unit_flag(j, F_TRIREME)) {
      j = get_unit_type(j)->tech_requirement;
      freelog(LOG_DEBUG, "nav tech is %s", advances[j].name);
      game.rtech.nav = j;
      break;
    }
  }

  /* pre-calculate game.rtech.u_partisan
     (tech for first partisan unit, or A_LAST */
  if (num_role_units(L_PARTISAN)==0) {
    game.rtech.u_partisan = A_LAST;
  } else {
    j = get_role_unit(L_PARTISAN, 0);
    j = game.rtech.u_partisan = get_unit_type(j)->tech_requirement;
    freelog(LOG_DEBUG, "partisan tech is %s", advances[j].name);
  }
}
  
/**************************************************************************
...  
**************************************************************************/
static void load_ruleset_buildings(char *ruleset_subdir)
{
  struct section_file file;
  char *filename, *datafile_options;
  char **sec;
  int i, j, nval;
  struct improvement_type *b;

  filename = openload_ruleset_file(&file, ruleset_subdir, "buildings");
  datafile_options = check_ruleset_capabilities(&file, "+1.8.2", filename);
  section_file_lookup(&file,"datafile.description"); /* unused */

  /* The names: */
  sec = secfile_get_secnames_prefix(&file, "building_", &nval);
  if(nval != B_LAST) {
    /* sometime this restriction should be removed */
    freelog(LOG_FATAL, "Bad number of buildings %d (%s)", nval, filename);
    exit(1);
  }

  for( i=0, j=0; i<B_LAST; i++ ) {
    b = &improvement_types[i];
    
    strcpy(b->name, secfile_lookup_str(&file, "%s.name", sec[i]));
    b->is_wonder = secfile_lookup_int(&file, "%s.is_wonder", sec[i]);
    b->tech_requirement = lookup_tech(&file, sec[i], "tech_req",
				      0, filename, b->name);

    b->obsolete_by = lookup_tech(&file, sec[i], "obsolete_by", 
				 0, filename, b->name);
    if (b->obsolete_by==A_LAST || !tech_exists(b->obsolete_by)) {
      b->obsolete_by = A_NONE;
    }

    b->build_cost = secfile_lookup_int(&file, "%s.build_cost", sec[i]);
    b->shield_upkeep = secfile_lookup_int(&file, "%s.upkeep", sec[i]);
    b->variant = secfile_lookup_int(&file, "%s.variant", sec[i]);
    
    b->helptext = lookup_helptext(&file, sec[i]);
  }

  /* Some more consistency checking: */
  for( i=0; i<B_LAST; i++ ) {
    b = &improvement_types[i];
    if (improvement_exists(i)) {
      if (!tech_exists(b->tech_requirement)) {
	freelog(LOG_NORMAL, "improvement \"%s\": depends on removed tech \"%s\" (%s)",
	     b->name, advances[b->tech_requirement].name, filename);
	b->tech_requirement = A_LAST;
      }
      if (!tech_exists(b->obsolete_by)) {
	freelog(LOG_NORMAL, "improvement \"%s\": obsoleted by removed tech \"%s\" (%s)",
	     b->name, advances[b->obsolete_by].name, filename);
	b->obsolete_by = A_NONE;
      }
    }
  }

  game.aqueduct_size = secfile_lookup_int(&file, "b_special.aqueduct_size");
  game.sewer_size = secfile_lookup_int(&file, "b_special.sewer_size");
  
  game.rtech.cathedral_plus =
    lookup_tech(&file, "b_special", "cathedral_plus", 0, filename, NULL);
  game.rtech.cathedral_minus =
    lookup_tech(&file, "b_special", "cathedral_minus", 0, filename, NULL);
  game.rtech.colosseum_plus =
    lookup_tech(&file, "b_special", "colosseum_plus", 0, filename, NULL);
  game.rtech.temple_plus =
    lookup_tech(&file, "b_special", "temple_plus", 0, filename, NULL);

  section_file_check_unused(&file, filename);
  section_file_free(&file);
}

/**************************************************************************
...  
**************************************************************************/
static void load_ruleset_terrain(char *ruleset_subdir)
{
  struct section_file file;
  char *filename, *datafile_options;
  int nval;
  char **sec;
  int i, j;
  struct tile_type *t;

  filename = openload_ruleset_file(&file, ruleset_subdir, "terrain");
  datafile_options
    = check_ruleset_capabilities(&file, "+1.8.2 +tilespec", filename);
  section_file_lookup(&file,"datafile.description"); /* unused */

  /* options */

  terrain_control.river_style =
    secfile_lookup_int_default(&file, R_AS_SPECIAL, "options.river_style");

  terrain_control.may_road =
    secfile_lookup_int_default(&file, TRUE, "options.may_road");
  terrain_control.may_irrigate =
    secfile_lookup_int_default(&file, TRUE, "options.may_irrigate");
  terrain_control.may_mine =
    secfile_lookup_int_default(&file, TRUE, "options.may_mine");
  terrain_control.may_transform =
    secfile_lookup_int_default(&file, TRUE, "options.may_transform");

  /* parameters */

  terrain_control.river_move_mode =
    secfile_lookup_int_default(&file, RMV_FAST_STRICT, "parameters.river_move_mode");
  terrain_control.river_defense_bonus =
    secfile_lookup_int_default(&file, 50, "parameters.river_defense_bonus");
  terrain_control.river_trade_incr =
    secfile_lookup_int_default(&file, 1, "parameters.river_trade_incr");
  terrain_control.fortress_defense_bonus =
    secfile_lookup_int_default(&file, 100, "parameters.fortress_defense_bonus");
  terrain_control.road_superhighway_trade_bonus =
    secfile_lookup_int_default(&file, 50, "parameters.road_superhighway_trade_bonus");
  terrain_control.rail_food_bonus =
    secfile_lookup_int_default(&file, 0, "parameters.rail_food_bonus");
  terrain_control.rail_shield_bonus =
    secfile_lookup_int_default(&file, 50, "parameters.rail_shield_bonus");
  terrain_control.rail_trade_bonus =
    secfile_lookup_int_default(&file, 0, "parameters.rail_trade_bonus");
  terrain_control.farmland_supermarket_food_bonus =
    secfile_lookup_int_default(&file, 50, "parameters.farmland_supermarket_food_bonus");
  terrain_control.pollution_food_penalty =
    secfile_lookup_int_default(&file, 50, "parameters.pollution_food_penalty");
  terrain_control.pollution_shield_penalty =
    secfile_lookup_int_default(&file, 50, "parameters.pollution_shield_penalty");
  terrain_control.pollution_trade_penalty =
    secfile_lookup_int_default(&file, 50, "parameters.pollution_trade_penalty");

  /* terrain names */

  sec = secfile_get_secnames_prefix(&file, "terrain_", &nval);
  if (nval != (T_COUNT - T_FIRST))
    {
      /* sometime this restriction should be removed */
      freelog(LOG_FATAL, "Bad number of terrains %d (%s)", nval, filename);
      exit(1);
    }

  for (i = T_FIRST; i < T_COUNT; i++)
    {
      t = &(tile_types[i]);

      strcpy(t->terrain_name,
	     secfile_lookup_str(&file, "%s.terrain_name", sec[i]));
      if (0 == strcmp(t->terrain_name, "unused")) *(t->terrain_name) = '\0';
    }

  /* terrain details */

  for (i = T_FIRST; i < T_COUNT; i++)
    {
      t = &(tile_types[i]);

      strncpy(t->graphic_str, secfile_lookup_str(&file,"%s.graphic", sec[i]),
	      sizeof(t->graphic_str)-1);
      strncpy(t->graphic_alt, secfile_lookup_str(&file,"%s.graphic_alt", sec[i]),
	    sizeof(t->graphic_alt)-1);

      t->movement_cost = secfile_lookup_int(&file, "%s.movement_cost", sec[i]);
      t->defense_bonus = secfile_lookup_int(&file, "%s.defense_bonus", sec[i]);

      t->food = secfile_lookup_int(&file, "%s.food", sec[i]);
      t->shield = secfile_lookup_int(&file, "%s.shield", sec[i]);
      t->trade = secfile_lookup_int(&file, "%s.trade", sec[i]);

      strcpy(t->special_1_name, secfile_lookup_str(&file, "%s.special_1_name", sec[i]));
      if (0 == strcmp(t->special_1_name, "none")) *(t->special_1_name) = '\0';
      t->food_special_1 = secfile_lookup_int(&file, "%s.food_special_1", sec[i]);
      t->shield_special_1 = secfile_lookup_int(&file, "%s.shield_special_1", sec[i]);
      t->trade_special_1 = secfile_lookup_int(&file, "%s.trade_special_1", sec[i]);

      strcpy(t->special_2_name, secfile_lookup_str(&file, "%s.special_2_name", sec[i]));
      if (0 == strcmp(t->special_2_name, "none")) *(t->special_2_name) = '\0';
      t->food_special_2 = secfile_lookup_int(&file, "%s.food_special_2", sec[i]);
      t->shield_special_2 = secfile_lookup_int(&file, "%s.shield_special_2", sec[i]);
      t->trade_special_2 = secfile_lookup_int(&file, "%s.trade_special_2", sec[i]);
      for(j=0; j<2; j++) {
	strncpy(t->special[j].graphic_str,
		secfile_lookup_str(&file,"%s.graphic_special_%d", sec[i], j+1),
		sizeof(t->special[j].graphic_str)-1);
	strncpy(t->special[j].graphic_alt,
		secfile_lookup_str(&file,"%s.graphic_special_%da", sec[i], j+1),
		sizeof(t->special[j].graphic_alt)-1);
      }

      t->road_trade_incr =
	secfile_lookup_int(&file, "%s.road_trade_incr", sec[i]);
      t->road_time = secfile_lookup_int(&file, "%s.road_time", sec[i]);

      t->irrigation_result =
	lookup_terrain(secfile_lookup_str(&file, "%s.irrigation_result", sec[i]), i);
      t->irrigation_food_incr =
	secfile_lookup_int(&file, "%s.irrigation_food_incr", sec[i]);
      t->irrigation_time = secfile_lookup_int(&file, "%s.irrigation_time", sec[i]);

      t->mining_result =
	lookup_terrain(secfile_lookup_str(&file, "%s.mining_result", sec[i]), i);
      t->mining_shield_incr =
	secfile_lookup_int(&file, "%s.mining_shield_incr", sec[i]);
      t->mining_time = secfile_lookup_int(&file, "%s.mining_time", sec[i]);

      t->transform_result =
	lookup_terrain(secfile_lookup_str(&file, "%s.transform_result", sec[i]), i);
      t->transform_time = secfile_lookup_int(&file, "%s.transform_time", sec[i]);
      
      t->helptext = lookup_helptext(&file, sec[i]);
    }

  section_file_check_unused(&file, filename);
  section_file_free(&file);

  return;
}

/**************************************************************************
...  
**************************************************************************/
static void load_ruleset_governments(char *ruleset_subdir)
{
  struct section_file file;
  char *filename, *datafile_options;
  struct government *g = NULL;
  int i, j, nval;
  char *c;
  char **sec, **slist;

  filename = openload_ruleset_file(&file, ruleset_subdir, "governments");
  datafile_options
    = check_ruleset_capabilities(&file, "+1.8.2 +tilespec +nontab", filename);
  section_file_lookup(&file,"datafile.description"); /* unused */

  sec = secfile_get_secnames_prefix(&file, "government_", &nval);
  if (nval == 0) {
    freelog(LOG_FATAL, "No governments!? (%s)", filename);
    exit(1);
  } else if(nval > G_MAGIC) {
    /* upper limit is really about 255 for 8-bit id values, but
       use G_MAGIC elsewhere as a sanity check, and should be plenty
       big enough --dwp */
    freelog(LOG_FATAL, "Too many governments! (%d, max %d; %s)",
	    nval, G_MAGIC, filename);
    exit(1);
  }
  game.government_count = nval;
  governments = fc_calloc(game.government_count, sizeof(struct government));

  /* first fill in government names so find_government_by_name will work -SKi */
  for(i = 0; i < game.government_count; i++) {
    g = &governments[i];
    strncpy(g->name, secfile_lookup_str(&file, "%s.name", sec[i]),
	    sizeof(g->name)-1);
    g->index = i;
  }

  game.default_government
    = lookup_government(&file, "governments.default", filename);
  
  game.government_when_anarchy
    = lookup_government(&file, "governments.when_anarchy", filename);
  
  /* Because player_init is called before rulesets are loaded we set
   * all players governments here, if they have not been previously
   * set (eg by loading game).
   */
  for(i=0; i<MAX_NUM_PLAYERS; i++) {
    if (game.players[i].government == G_MAGIC) {
      game.players[i].government = game.default_government;
    }
  }

  /* easy ones: */
  for(i=0; i<game.government_count; i++) {
    g = &governments[i];
    
    g->required_tech
      = lookup_tech(&file, sec[i], "tech_req", 0, filename, g->name);
    
    strncpy(g->graphic_str, secfile_lookup_str(&file, "%s.graphic", sec[i]),
	    sizeof(g->graphic_str)-1);
    strncpy(g->graphic_alt, secfile_lookup_str(&file, "%s.graphic_alt", sec[i]),
	    sizeof(g->graphic_alt)-1);
    
    g->martial_law_max = secfile_lookup_int(&file, "%s.martial_law_max", sec[i]);
    g->martial_law_per = secfile_lookup_int(&file, "%s.martial_law_per", sec[i]);
    g->max_rate = secfile_lookup_int(&file, "%s.max_single_rate", sec[i]);
    g->civil_war = secfile_lookup_int(&file, "%s.civil_war_chance", sec[i]);
    g->empire_size_mod = secfile_lookup_int(&file, "%s.empire_size_mod", sec[i]);
    g->rapture_size = secfile_lookup_int(&file, "%s.rapture_size", sec[i]);
    
    g->free_happy
      = lookup_city_cost(&file, sec[i], "unit_free_unhappy", filename);
    g->free_shield
      = lookup_city_cost(&file, sec[i], "unit_free_shield", filename);
    g->free_food
      = lookup_city_cost(&file, sec[i], "unit_free_food", filename);
    g->free_gold
      = lookup_city_cost(&file, sec[i], "unit_free_gold", filename);

    g->unit_happy_cost_factor
      = secfile_lookup_int(&file, "%s.unit_unhappy_factor", sec[i]);
    g->unit_shield_cost_factor
      = secfile_lookup_int(&file, "%s.unit_shield_factor", sec[i]);
    g->unit_food_cost_factor
      = secfile_lookup_int(&file, "%s.unit_food_factor", sec[i]);
    g->unit_gold_cost_factor
      = secfile_lookup_int(&file, "%s.unit_gold_factor", sec[i]);

    g->corruption_level
      = secfile_lookup_int(&file, "%s.corruption_level", sec[i]);
    g->corruption_modifier
      = secfile_lookup_int(&file, "%s.corruption_modifier", sec[i]);
    g->fixed_corruption_distance
      = secfile_lookup_int(&file, "%s.corruption_fixed_distance", sec[i]);
    g->corruption_distance_factor
      = secfile_lookup_int(&file, "%s.corruption_distance_factor", sec[i]);
    g->extra_corruption_distance
      = secfile_lookup_int(&file, "%s.corruption_extra_distance", sec[i]);

    g->trade_bonus
      = secfile_lookup_int(&file, "%s.production_trade_bonus", sec[i]);
    g->shield_bonus
      = secfile_lookup_int(&file, "%s.production_shield_bonus", sec[i]);
    g->food_bonus
      = secfile_lookup_int(&file, "%s.production_food_bonus", sec[i]);

    g->celeb_trade_bonus
      = secfile_lookup_int(&file, "%s.production_trade_bonus,1", sec[i]);
    g->celeb_shield_bonus
      = secfile_lookup_int(&file, "%s.production_shield_bonus,1", sec[i]);
    g->celeb_food_bonus
      = secfile_lookup_int(&file, "%s.production_food_bonus,1", sec[i]);

    g->trade_before_penalty
      = secfile_lookup_int(&file, "%s.production_trade_penalty", sec[i]);
    g->shields_before_penalty
      = secfile_lookup_int(&file, "%s.production_shield_penalty", sec[i]);
    g->food_before_penalty
      = secfile_lookup_int(&file, "%s.production_food_penalty", sec[i]);

    g->celeb_trade_before_penalty
      = secfile_lookup_int(&file, "%s.production_trade_penalty,1", sec[i]);
    g->celeb_shields_before_penalty
      = secfile_lookup_int(&file, "%s.production_shield_penalty,1", sec[i]);
    g->celeb_food_before_penalty
      = secfile_lookup_int(&file, "%s.production_food_penalty,1", sec[i]);
    
    g->helptext = lookup_helptext(&file, sec[i]);
  }

  
  /* flags: */
  for(i=0; i<game.government_count; i++) {
    g = &governments[i];
    g->flags = 0;
    slist = secfile_lookup_str_vec(&file, &nval, "%s.flags", sec[i]);
    for(j=0; j<nval; j++) {
      char *sval = slist[j];
      enum government_flag_id flag = government_flag_from_str(sval);
      if (strcmp(sval, "-") == 0) {
        continue;
      }
      if (flag == G_LAST_FLAG) {
        freelog(LOG_FATAL, "government %s has unknown flag %s", g->name, sval);
        exit(1);
      } else {
        g->flags |= (1<<flag);
      }
    }
    free(slist);
  }

  /* hints: */
  for(i=0; i<game.government_count; i++) {
    g = &governments[i];
    g->hints = 0;
    slist = secfile_lookup_str_vec(&file, &nval, "%s.hints", sec[i]);
    for(j=0; j<nval; j++) {
      char *sval = slist[j];
      enum government_hint_id hint = government_hint_from_str(sval);
      if (strcmp(sval, "-") == 0) {
        continue;
      }
      if (hint == G_LAST_FLAG) {
        freelog(LOG_FATAL, "government %s has unknown hint %s", g->name, sval);
        exit(1);
      } else {
        g->hints |= (1<<hint);
      }
    }
    free(slist);
  }

  /* titles */
  for(i=0; i<game.government_count; i++) {
    struct ruler_title *title;

    g = &governments[i];

    g->num_ruler_titles = 1;
    g->ruler_titles = fc_calloc(1, sizeof(struct ruler_title));
    title = &(g->ruler_titles[0]);

    title->nation = DEFAULT_TITLE;
    strncpy(title->male_title,
	    secfile_lookup_str(&file, "%s.ruler_male_title", sec[i]),
	    sizeof(title->male_title)-1);
    title->male_title[sizeof(title->male_title)-1] = '\0';
    strncpy(title->female_title,
	    secfile_lookup_str(&file, "%s.ruler_female_title", sec[i]),
	    sizeof(title->female_title)-1);
    title->female_title[sizeof(title->female_title)-1] = '\0';
  }

  /* ai tech_hints: */
  j = -1;
  while((c = secfile_lookup_str_default(&file, NULL,
					"governments.ai_tech_hints%d.tech",
					++j))) {
    struct ai_gov_tech_hint *hint = &ai_gov_tech_hints[j];

    if (j >= MAX_NUM_TECH_LIST) {
      freelog(LOG_FATAL, "Too many ai tech_hints in %s", filename);
      exit(1);
    }
    hint->tech = find_tech_by_name(c);
    if (hint->tech == A_LAST) {
      freelog(LOG_FATAL, "Could not match tech %s for gov ai_tech_hint %d (%s)",
	      c, j, filename);
      exit(1);
    }
    if (!tech_exists(hint->tech)) {
      freelog(LOG_FATAL, "For gov ai_tech_hint %d, tech \"%s\" is removed (%s)",
	      j, c, filename);
      exit(1);
    }
    hint->turns_factor =
      secfile_lookup_int(&file, "governments.ai_tech_hints%d.turns_factor", j);
    hint->const_factor =
      secfile_lookup_int(&file, "governments.ai_tech_hints%d.const_factor", j);
    hint->get_first =
      secfile_lookup_int(&file, "governments.ai_tech_hints%d.get_first", j);
    hint->done =
      secfile_lookup_int(&file, "governments.ai_tech_hints%d.done", j);
  }
  if (j<MAX_NUM_TECH_LIST) {
    ai_gov_tech_hints[j].tech = A_LAST;
  }

  section_file_check_unused(&file, filename);
  section_file_free(&file);
}

/**************************************************************************
...
**************************************************************************/
static void send_ruleset_control(struct player *dest)
{
  struct packet_ruleset_control packet;
  int to, i;

  packet.aqueduct_size = game.aqueduct_size;
  packet.sewer_size = game.sewer_size;
  
  packet.rtech.get_bonus_tech = game.rtech.get_bonus_tech;
  packet.rtech.cathedral_plus = game.rtech.cathedral_plus;
  packet.rtech.cathedral_minus = game.rtech.cathedral_minus;
  packet.rtech.colosseum_plus = game.rtech.colosseum_plus;
  packet.rtech.temple_plus = game.rtech.temple_plus;

  for(i=0; i<MAX_NUM_TECH_LIST; i++) {
    packet.rtech.partisan_req[i] = game.rtech.partisan_req[i];
  }

  packet.government_count = game.government_count;
  packet.government_when_anarchy = game.government_when_anarchy;
  packet.default_government = game.default_government;

  packet.num_unit_types = game.num_unit_types;
  packet.num_tech_types = game.num_tech_types;

  packet.nation_count = game.nation_count;

  for(to=0; to<game.nplayers; to++) {           /* dests */
    if(dest==0 || get_player(to)==dest) {
      send_packet_ruleset_control(get_player(to)->conn, &packet);
    }
  }
}

/**************************************************************************
This checks if nations[pos] leader names are not already defined in any 
previous nation, or twice in its own leader name table.
If not return NULL, if yes return pointer to name which is repeated.
**************************************************************************/
static char *check_leader_names(int pos)
{
  int i, j, k;
  struct nation_type *n, *nation;
  char *leader;

  nation = get_nation_by_idx(pos);
  for( k = 0; k < nation->leader_count; k++) {
    leader = nation->leader_name[k];
    for( i=0; i<k; i++ ) {
      if( !strcmp(leader, nation->leader_name[i]) )
          return leader;
    }
    for( j = 0; j < pos; j++) {
      n = get_nation_by_idx(j);
      for( i=0; i < n->leader_count; i++) {
        if( !strcmp(leader, n->leader_name[i]) )
          return leader;
      }
    }
  }
  return NULL;
}

/**************************************************************************
Load nations.ruleset file
**************************************************************************/
static void load_ruleset_nations(char *ruleset_subdir)
{
  struct section_file file;
  char *filename, *datafile_options, *bad_leader, *g;
  struct nation_type *pl;
  struct government *gov;
  int *res, dim, val, i, j;
  char temp_name[MAX_LEN_NAME], male[MAX_LEN_NAME], female[MAX_LEN_NAME];
  char **cities, **techs, **leaders;

  filename = openload_ruleset_file(&file, ruleset_subdir, "nations");
  datafile_options = check_ruleset_capabilities(&file, "+1.8.2", filename);
  section_file_lookup(&file,"datafile.description"); /* unused */

  i=0; /* this here check how many nations are */
  while( strlen(secfile_lookup_str_default(&file, "", "nation%d.name", i++)) );
  game.nation_count = i-1;
  freelog(LOG_VERBOSE, "There are %d nations defined", game.nation_count);
  
  if ( game.nation_count >= MAX_NUM_NATIONS || game.nation_count == 0) {
    freelog(LOG_FATAL, "There can must be between 1 and %d nations", MAX_NUM_NATIONS);
    exit(1);    
  }
  alloc_nations(game.nation_count);

  for( i=0; i<game.nation_count; i++) {
    pl = get_nation_by_idx(i);

    /* nation name and leaders */

    strcpy( pl->name, secfile_lookup_str(&file, "nation%d.name", i));
    strcpy( pl->name_plural, secfile_lookup_str(&file, "nation%d.plural", i));
    leaders = secfile_lookup_str_vec(&file, &dim, "nation%d.leader", i);
    if( dim<1 || dim > MAX_NUM_LEADERS ) {
      freelog(LOG_FATAL, "Nation %s: number of leaders must be 1-%d", pl->name, 
              MAX_NUM_LEADERS);
      exit(1);
    }
    pl->leader_count = dim;
    for( j=0; j<dim; j++) {
      pl->leader_name[j] = fc_calloc( strlen(leaders[j])+1, sizeof(char));
      strcpy( pl->leader_name[j], leaders[j]);
    }
    free(leaders);

    /* check if nation or leader name is not already defined */
    for( j=0; j<i; j++) {
      if( !strcmp(get_nation_name(j), pl->name) ) {
        freelog(LOG_FATAL, "Nation %s already defined in section nation%d", pl->name, j);
        exit(1);
      }
      if( !strcmp(get_nation_name_plural(j), pl->name_plural) ) {
        freelog(LOG_FATAL, "Nation %s already defined in section nation%d", 
                pl->name_plural, j);
        exit(1);
      }
    }
    if( (bad_leader=check_leader_names(i)) ) {
        freelog(LOG_FATAL, "Nation %s leader %s already defined", pl->name, bad_leader);
        exit(1);
    }
    /* read leaders'sexes */
    leaders = secfile_lookup_str_vec(&file, &dim, "nation%d.leader_sex", i);
    if( dim != pl->leader_count ) {
      freelog(LOG_FATAL, "Nation %s: leader sex count not equal to number of leaders",
              pl->name);
      exit(1);
    }
    for(j=0; j<dim; j++) {
      if( strcmp(leaders[j], "Male")==0 )
        pl->leader_is_male[j] = 1;
      else if( strcmp(leaders[j], "Female")==0 )
        pl->leader_is_male[j] = 0;
      else {
        freelog( LOG_FATAL, "Nation %s leader sex must be Male or Female", pl->name);
        exit(1);
      }
    }
    free(leaders);

    /* Flags */

    strcpy(pl->flag_graphic_str,
	   secfile_lookup_str(&file, "nation%d.flag", i));
    strcpy(pl->flag_graphic_alt,
	   secfile_lookup_str(&file, "nation%d.flag_alt", i));

    /* Ruler titles */

    j = -1;
    while((g = secfile_lookup_str_default(&file, NULL, "nation%d.ruler_titles%d.government",
					  i, ++j))) {
      strncpy(male, secfile_lookup_str(&file, "nation%d.ruler_titles%d.male_title", i, j),
              sizeof(male)-1);
      male[sizeof(male)-1] = '\0';
      strncpy(female, secfile_lookup_str(&file, "nation%d.ruler_titles%d.female_title", i, j),
              sizeof(female)-1); 
      female[sizeof(female)-1] = '\0';
      if( (gov = find_government_by_name(g)) != NULL ) {
        set_ruler_title(gov, i, male, female);
      }
      else {
        freelog(LOG_VERBOSE,"Nation %s, government %s not found", pl->name, g);
      }
    }

    /* AI stuff */

    pl->attack = secfile_lookup_int_default(&file, 2, "nation%d.attack", i);
    pl->expand = secfile_lookup_int_default(&file, 2, "nation%d.expand", i);
    pl->civilized = secfile_lookup_int_default(&file, 2, "nation%d.civilized", i);

    res = secfile_lookup_int_vec(&file, &dim, "nation%d.advisors", i);
    if( dim != ADV_LAST ) {
      freelog( LOG_FATAL, "Nation %d number of advisors must be %d but is %d", 
               i, ADV_LAST, dim);
      exit(1); 
    }
    for ( j=0; j<ADV_LAST; j++) 
      pl->advisors[j] = res[j];
    if(res) free(res);

    /* AI techs */

    techs = secfile_lookup_str_vec(&file, &dim, "nation%d.tech_goals", i);
    if( dim > MAX_NUM_TECH_GOALS ) {
      freelog( LOG_VERBOSE, "Only %d techs can used from %d defined for nation %s",
               MAX_NUM_TECH_GOALS, dim, pl->name_plural);
      dim = MAX_NUM_TECH_GOALS;
    }
    for( j=0; j<dim; j++) {
      val = find_tech_by_name(techs[j]);
      if(val == A_LAST) {
	freelog(LOG_VERBOSE, "Didn't match goal tech %d \"%s\" for %s",
		j, techs[j], pl->name);
      } else if(!tech_exists(val)) {
	freelog(LOG_VERBOSE, "Goal tech %d \"%s\" for %s doesn't exist",
		j, techs[j], pl->name);
	val = A_LAST;
      }
      if(val != A_LAST && val != A_NONE) {
	freelog(LOG_DEBUG, "%s tech goal (%d) %3d %s", pl->name, j, val, techs[j]);
	pl->goals.tech[j] = val;
      }
    }
    freelog(LOG_DEBUG, "%s %d tech goals", pl->name, j);
    if(j==0) {
      freelog(LOG_VERBOSE, "No valid goal techs for %s", pl->name);
    }
    while( j < MAX_NUM_TECH_GOALS )
      pl->goals.tech[j++] = A_NONE;
    if (techs) free(techs);

    /* AI wonder & government */

    strcpy( temp_name, secfile_lookup_str(&file, "nation%d.wonder", i));
    val = find_improvement_by_name(temp_name);
    /* for any problems, leave as B_LAST */
    if(val == B_LAST) {
      freelog(LOG_VERBOSE, "Didn't match goal wonder \"%s\" for %s", temp_name, pl->name);
    } else if(!improvement_exists(val)) {
      freelog(LOG_VERBOSE, "Goal wonder \"%s\" for %s doesn't exist", temp_name, pl->name);
      val = B_LAST;
    } else if(!is_wonder(val)) {
      freelog(LOG_VERBOSE, "Goal wonder \"%s\" for %s not a wonder", temp_name, pl->name);
      val = B_LAST;
    }
    pl->goals.wonder = val;
    freelog(LOG_DEBUG, "%s wonder goal %d %s", pl->name, val, temp_name);

    strcpy( temp_name, secfile_lookup_str(&file, "nation%d.government", i));
    gov = find_government_by_name(temp_name);
    if(gov == NULL) {
      freelog(LOG_VERBOSE, "Didn't match goal government name \"%s\" for %s",
	      temp_name, pl->name);
      val = game.government_when_anarchy;  /* flag value (no goal) (?) */
    } else {
      val = gov->index;
    }
    pl->goals.government = val;

    /* read city names */

    cities = secfile_lookup_str_vec(&file, &dim, "nation%d.cities", i);
    pl->default_city_names = fc_calloc(dim+1, sizeof(char*));
    pl->default_city_names[dim] = NULL;
    for ( j=0; j<dim; j++) {
      pl->default_city_names[j] = fc_calloc( (strlen(cities[j])+1), sizeof(char));
      strcpy( pl->default_city_names[j], cities[j]);
    }
    if(cities) free(cities);
  }

  /* read miscellaneous city names */

  cities = secfile_lookup_str_vec(&file, &dim, "misc.cities");
  misc_city_names = fc_calloc(dim+1, sizeof(char*));
  for ( j=0; j<dim; j++) {
    misc_city_names[j] = fc_calloc((strlen(cities[j])+1), sizeof(char));
    strcpy( misc_city_names[j], cities[j]);      
  }
  misc_city_names[dim] = NULL;
  if(cities) free(cities);

  section_file_check_unused(&file, filename);
  section_file_free(&file);
}

/**************************************************************************
...
**************************************************************************/
static void send_ruleset_units(struct player *dest)
{
  struct packet_ruleset_unit packet;
  struct unit_type *u;
  int to;

  for(u=unit_types; u<unit_types+game.num_unit_types; u++) {
    packet.id = u-unit_types;
    strcpy(packet.name, u->name_orig);
    strcpy(packet.graphic_str, u->graphic_str);
    strcpy(packet.graphic_alt, u->graphic_alt);
    packet.move_type = u->move_type;
    packet.build_cost = u->build_cost;
    packet.attack_strength = u->attack_strength;
    packet.defense_strength = u->defense_strength;
    packet.move_rate = u->move_rate;
    packet.tech_requirement = u->tech_requirement;
    packet.vision_range = u->vision_range;
    packet.transport_capacity = u->transport_capacity;
    packet.hp = u->hp / game.firepower_factor;
    packet.firepower = u->firepower;
    packet.obsoleted_by = u->obsoleted_by;
    packet.fuel = u->fuel;
    packet.flags = u->flags;
    packet.roles = u->roles;
    packet.happy_cost = u->happy_cost;
    packet.shield_cost = u->shield_cost;
    packet.food_cost = u->food_cost;
    packet.gold_cost = u->gold_cost;
    packet.paratroopers_range = u->paratroopers_range;
    packet.paratroopers_mr_req = u->paratroopers_mr_req;
    packet.paratroopers_mr_sub = u->paratroopers_mr_sub;
    packet.helptext = u->helptext;   /* pointer assignment */

    for(to=0; to<game.nplayers; to++) {           /* dests */
      if(dest==0 || get_player(to)==dest) {
	send_packet_ruleset_unit(get_player(to)->conn, &packet);
      }
    }
  }
}

/**************************************************************************
...  
**************************************************************************/
static void send_ruleset_techs(struct player *dest)
{
  struct packet_ruleset_tech packet;
  struct advance *a;
  int to;

  for(a=advances; a<advances+game.num_tech_types; a++) {
    packet.id = a-advances;
    strcpy(packet.name, a->name_orig);
    packet.req[0] = a->req[0];
    packet.req[1] = a->req[1];
    packet.flags = a->flags;
    packet.helptext = a->helptext;   /* pointer assignment */

    for(to=0; to<game.nplayers; to++) {           /* dests */
      if(dest==0 || get_player(to)==dest) {
	send_packet_ruleset_tech(get_player(to)->conn, &packet);
      }
    }
  }
}

/**************************************************************************
...  
**************************************************************************/
static void send_ruleset_buildings(struct player *dest)
{
  struct packet_ruleset_building packet;
  struct improvement_type *b;
  int to;

  for(b=improvement_types; b<improvement_types+B_LAST; b++) {
    packet.id = b-improvement_types;
    strcpy(packet.name, b->name_orig);
    packet.is_wonder = b->is_wonder;
    packet.tech_requirement = b->tech_requirement;
    packet.build_cost = b->build_cost;
    packet.shield_upkeep = b->shield_upkeep;
    packet.obsolete_by = b->obsolete_by;
    packet.variant = b->variant;
    packet.helptext = b->helptext;   /* pointer assignment */

    for(to=0; to<game.nplayers; to++) {           /* dests */
      if(dest==0 || get_player(to)==dest) {
	send_packet_ruleset_building(get_player(to)->conn, &packet);
      }
    }
  }
}

/**************************************************************************
...  
**************************************************************************/
static void send_ruleset_terrain(struct player *dest)
{
  struct packet_ruleset_terrain packet;
  struct tile_type *t;
  int i, j, to;

  for (to = 0; to < game.nplayers; to++)      /* dests */
    {
      if (dest==0 || get_player(to)==dest)
	{
	  send_packet_ruleset_terrain_control(get_player(to)->conn, &terrain_control);
	}
    }

  for (i = T_FIRST; i < T_COUNT; i++)
    {
      t = &(tile_types[i]);

      packet.id = i;

      strcpy (packet.terrain_name, t->terrain_name_orig);
      strcpy(packet.graphic_str, t->graphic_str);
      strcpy(packet.graphic_alt, t->graphic_alt);

      packet.movement_cost = t->movement_cost;
      packet.defense_bonus = t->defense_bonus;

      packet.food = t->food;
      packet.shield = t->shield;
      packet.trade = t->trade;

      strcpy (packet.special_1_name, t->special_1_name_orig);
      packet.food_special_1 = t->food_special_1;
      packet.shield_special_1 = t->shield_special_1;
      packet.trade_special_1 = t->trade_special_1;

      strcpy (packet.special_2_name, t->special_2_name_orig);
      packet.food_special_2 = t->food_special_2;
      packet.shield_special_2 = t->shield_special_2;
      packet.trade_special_2 = t->trade_special_2;

      for(j=0; j<2; j++) {
	strcpy(packet.special[j].graphic_str, t->special[j].graphic_str);
	strcpy(packet.special[j].graphic_alt, t->special[j].graphic_alt);
      }

      packet.road_trade_incr = t->road_trade_incr;
      packet.road_time = t->road_time;

      packet.irrigation_result = t->irrigation_result;
      packet.irrigation_food_incr = t->irrigation_food_incr;
      packet.irrigation_time = t->irrigation_time;

      packet.mining_result = t->mining_result;
      packet.mining_shield_incr = t->mining_shield_incr;
      packet.mining_time = t->mining_time;

      packet.transform_result = t->transform_result;
      packet.transform_time = t->transform_time;

      packet.helptext = t->helptext;   /* pointer assignment */
      
      for (to = 0; to < game.nplayers; to++)      /* dests */
	{
	  if (dest==0 || get_player(to)==dest)
	    {
	      send_packet_ruleset_terrain(get_player(to)->conn, &packet);
	    }
	}
    }
}

/**************************************************************************
...  
**************************************************************************/
static void send_ruleset_governments(struct player *dest)
{
  struct packet_ruleset_government gov;
  struct packet_ruleset_government_ruler_title title;
  struct ruler_title *p_title;
  struct government *g;
  int i, j, to;

  for (i = 0; i < game.government_count; ++i) {
    g = &governments[i];

    /* send one packet_government */
    gov.id                 = i;

    gov.required_tech    = g->required_tech;
    gov.max_rate         = g->max_rate;
    gov.civil_war        = g->civil_war;
    gov.martial_law_max  = g->martial_law_max;
    gov.martial_law_per  = g->martial_law_per;
    gov.empire_size_mod  = g->empire_size_mod;
    gov.rapture_size     = g->rapture_size;
    
    gov.unit_happy_cost_factor  = g->unit_happy_cost_factor;
    gov.unit_shield_cost_factor = g->unit_shield_cost_factor;
    gov.unit_food_cost_factor   = g->unit_food_cost_factor;
    gov.unit_gold_cost_factor   = g->unit_gold_cost_factor;
    
    gov.free_happy  = g->free_happy;
    gov.free_shield = g->free_shield;
    gov.free_food   = g->free_food;
    gov.free_gold   = g->free_gold;

    gov.trade_before_penalty = g->trade_before_penalty;
    gov.shields_before_penalty = g->shields_before_penalty;
    gov.food_before_penalty = g->food_before_penalty;

    gov.celeb_trade_before_penalty = g->celeb_trade_before_penalty;
    gov.celeb_shields_before_penalty = g->celeb_shields_before_penalty;
    gov.celeb_food_before_penalty = g->celeb_food_before_penalty;

    gov.trade_bonus = g->trade_bonus;
    gov.shield_bonus = g->shield_bonus;
    gov.food_bonus = g->food_bonus;

    gov.celeb_trade_bonus = g->celeb_trade_bonus;
    gov.celeb_shield_bonus = g->celeb_shield_bonus;
    gov.celeb_food_bonus = g->celeb_food_bonus;

    gov.corruption_level = g->corruption_level;
    gov.corruption_modifier = g->corruption_modifier;
    gov.fixed_corruption_distance = g->fixed_corruption_distance;
    gov.corruption_distance_factor = g->corruption_distance_factor;
    gov.extra_corruption_distance = g->extra_corruption_distance;
    
    gov.flags = g->flags;
    gov.hints = g->hints;
    gov.num_ruler_titles = g->num_ruler_titles;

    strcpy(gov.name, g->name_orig);
    strcpy(gov.graphic_str, g->graphic_str);
    strcpy(gov.graphic_alt, g->graphic_alt);
    
    gov.helptext = g->helptext;   /* pointer assignment */
      
    for(to=0; to<game.nplayers; to++) {           /* dests */
      if(dest==0 || get_player(to)==dest) {
	send_packet_ruleset_government(get_player(to)->conn, &gov);
      }
    }
    
    /* send one packet_government_ruler_title per ruler title */
    for(j=0; j<g->num_ruler_titles; j++) {
      p_title = &g->ruler_titles[j];

      title.gov = i;
      title.id = j;
      title.nation = p_title->nation;
      strcpy (title.male_title, p_title->male_title);
      strcpy (title.female_title, p_title->female_title);
    
      for(to=0; to<game.nplayers; to++) {           /* dests */
        if(dest==0 || get_player(to)==dest) {
	  send_packet_ruleset_government_ruler_title(get_player(to)->conn, &title);
        }
      }
    }
  }
}

/**************************************************************************
...  
**************************************************************************/
static void send_ruleset_nations(struct player *dest)
{
  struct packet_ruleset_nation packet;
  struct nation_type *n;
  int to, i, k;

  for( k=0; k<game.nation_count; k++) {
    n = get_nation_by_idx(k);
    packet.id = k;
    strcpy(packet.name, n->name_orig);
    strcpy(packet.name_plural, n->name_plural_orig);
    strcpy(packet.graphic_str, n->flag_graphic_str);
    strcpy(packet.graphic_alt, n->flag_graphic_alt);
    packet.leader_count = n->leader_count;
    for(i=0; i < n->leader_count; i++) {
      strcpy(packet.leader_name[i], n->leader_name[i]);
      packet.leader_sex[i] = n->leader_is_male[i];
    }

    for(to=0; to<game.nplayers; to++) {           /* dests */
      if(dest==0 || get_player(to)==dest) {
	send_packet_ruleset_nation(get_player(to)->conn, &packet);
      }
    }
  }
}

/**************************************************************************
...  
**************************************************************************/
void load_rulesets(void)
{
  freelog(LOG_NORMAL, "Loading rulesets");
  load_ruleset_techs(game.ruleset.techs);
  load_ruleset_governments(game.ruleset.governments);
  load_ruleset_units(game.ruleset.units);
  load_ruleset_buildings(game.ruleset.buildings);
  load_ruleset_nations(game.ruleset.nations);
  load_ruleset_terrain(game.ruleset.terrain);
  translate_data_names();
}

/**************************************************************************
 dest can be NULL meaning all players
**************************************************************************/
void send_rulesets(struct player *dest)
{
  int to;
  
  for(to=0; to<game.nplayers; to++) {
    if(dest==0 || get_player(to)==dest) {
      connection_do_buffer(get_player(to)->conn);
    }
  }

  send_ruleset_control(dest);
  send_ruleset_techs(dest);
  send_ruleset_governments(dest);
  send_ruleset_units(dest);
  send_ruleset_buildings(dest);
  send_ruleset_terrain(dest);
  send_ruleset_nations(dest);
  
  for(to=0; to<game.nplayers; to++) {
    if(dest==0 || get_player(to)==dest) {
      connection_do_unbuffer(get_player(to)->conn);
    }
  }
}
