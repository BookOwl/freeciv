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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "mem.h"
#include "shared.h"
#include "support.h"
#include "tech.h"

#include "city.h"

/* get 'struct city_list' functions: */
#define SPECLIST_TAG city
#define SPECLIST_TYPE struct city
#include "speclist_c.h"

static int improvement_upkeep_asmiths(struct city *pcity, int i, int asmiths);


/* Iterate a city map, from the center (the city) outwards */

int city_map_iterate_outwards_indices[(CITY_MAP_SIZE*CITY_MAP_SIZE)-4][2] =
{
  { 2, 2 },

  { 1, 2 }, { 2, 1 }, { 3, 2 }, { 2, 3 },
  { 1, 3 }, { 1, 1 }, { 3, 1 }, { 3, 3 },

  { 0, 2 }, { 2, 0 }, { 4, 2 }, { 2, 4 },
  { 0, 3 }, { 0, 1 },
  { 1, 0 }, { 3, 0 },
  { 4, 1 }, { 4, 3 },
  { 3, 4 }, { 1, 4 }
};

/****************************************************************
All the city improvements:
Use get_improvement_type(id) to access the array.
The improvement_types array is now setup in:
   server/ruleset.c (for the server)
   client/packhand.c (for the client)
*****************************************************************/

struct impr_type improvement_types[B_LAST];

char **misc_city_names; 

struct citystyle *city_styles = NULL;


/* Names of effect ranges.
 * (These must correspond to enum effect_range_id in city.h.)
 */
static char *effect_range_names[] = {
  "None",
  "Building",
  "City",
  "Island",
  "Player",
  "World"
};

/* Names of effect types.
 * (These must correspond to enum effect_type_id in city.h.)
 */
static char *effect_type_names[] = {
  "Adv_Parasite",
  "Airlift",
  "Any_Government",
  "Barb_Attack",
  "Barb_Defend",
  "Capital_City",
  "Capital_Exists",
  "Enable_Nuke",
  "Enable_Space",
  "Enemy_Peaceful",
  "Food_Add_Tile",
  "Food_Bonus",
  "Food_Inc_Tile",
  "Food_Per_Tile",
  "Give_Imm_Adv",
  "Growth_Food",
  "Have_Embassies",
  "Improve_Rep",
  "Luxury_Bonus",
  "Luxury_Pct",
  "Make_Content",
  "Make_Content_Mil",
  "Make_Content_Pct",
  "Make_Happy",
  "May_Declare_War",
  "No_Anarchy",
  "No_Sink_Deep",
  "Nuke_Proof",
  "Pollu_Adj",
  "Pollu_Adj_Pop",
  "Pollu_Adj_Prod",
  "Pollu_Decrease",
  "Prod_Add_Tile",
  "Prod_Bonus",
  "Prod_Inc_Tile",
  "Prod_Per_Tile",
  "Prod_To_Gold",
  "Reduce_Corrupt",
  "Reduce_Waste",
  "Reveal_Cities",
  "Reveal_Map",
  "Revolt_Dist",
  "Science_Bonus",
  "Science_Pct",
  "Size_Unlimit",
  "Slow_Global_Warm",
  "Space_Part",
  "Spy_Resistant",
  "Tax_Bonus",
  "Tax_Pct",
  "Trade_Add_Tile",
  "Trade_Bonus",
  "Trade_Inc_Tile",
  "Trade_Per_Tile",
  "Trade_Route_Pct",
  "Unit_Defend",
  "Unit_Move",
  "Unit_No_Lose_Pop",
  "Unit_Recover",
  "Unit_Repair",
  "Unit_Vet_Combat",
  "Unit_Veteran",
  "Upgrade_Units",
  "Upgrade_One_Step",
  "Upgrade_One_Leap",
  "Upgrade_All_Step",
  "Upgrade_All_Leap",
  "Upkeep_Free"
};

/**************************************************************************
  Set the worker on the citymap.  Also sets the worked field in the map.
**************************************************************************/
void set_worker_city(struct city *pcity, int x, int y, 
		     enum city_tile_type type) 
{
  struct tile *ptile=map_get_tile(pcity->x+x-2, pcity->y+y-2);
  if (pcity->city_map[x][y] == C_TILE_WORKER)
    ptile->worked = NULL;
  pcity->city_map[x][y]=type;
/* this function is called far less than is_worked here */
/* and these two ifs are a lot less CPU load then the iterates! */
  if (type == C_TILE_WORKER)
    ptile->worked = pcity;
}

/**************************************************************************
...
**************************************************************************/
enum city_tile_type get_worker_city(struct city *pcity, int x, int y)
{
  if ((x==0 || x==4) && (y == 0 || y == 4)) 
    return C_TILE_UNAVAILABLE;
  return(pcity->city_map[x][y]);
}

/**************************************************************************
...
**************************************************************************/
int is_worker_here(struct city *pcity, int x, int y) 
{
  if (x < 0 || x > 4 || y < 0 || y > 4 || ((x == 0 || x == 4) && (y == 0|| y==4))) {
    return 0;
  }
  return (get_worker_city(pcity,x,y)==C_TILE_WORKER); 
}

/**************************************************************************
  Convert map coordinate into position in city map
**************************************************************************/
int map_to_city_x(struct city *pcity, int x)
{
	int t=map.xsize/2;
	x-=pcity->x;
	if(x > t) x-=map.xsize;
	else if(x < -t) x+=map.xsize;
	return x+2;
}
int map_to_city_y(struct city *pcity, int y)
{
	return y-pcity->y+2;
}

/**************************************************************************
  Convert effect range names to enum; case insensitive;
  returns EFR_LAST if can't match.
**************************************************************************/
Eff_Range_id effect_range_from_str(char *str)
{
  Eff_Range_id ret_id;

  assert(sizeof(effect_range_names)/sizeof(effect_range_names[0])==EFR_LAST);

  for (ret_id = 0; ret_id < EFR_LAST; ret_id++) {
    if (0 == mystrcasecmp(effect_range_names[ret_id], str)) {
      return ret_id;
    }
  }

  return EFR_LAST;
}

/**************************************************************************
  Return effect range name; NULL if bad id.
**************************************************************************/
char *effect_range_name(Eff_Range_id id)
{
  assert(sizeof(effect_range_names)/sizeof(effect_range_names[0])==EFR_LAST);

  if (id < EFR_LAST) {
    return effect_range_names[id];
  } else {
    return NULL;
  }
}

/**************************************************************************
  Convert effect type names to enum; case insensitive;
  returns EFT_LAST if can't match.
**************************************************************************/
Eff_Type_id effect_type_from_str(char *str)
{
  Eff_Type_id ret_id;

  assert(sizeof(effect_type_names)/sizeof(effect_type_names[0])==EFT_LAST);

  for (ret_id = 0; ret_id < EFT_LAST; ret_id++) {
    if (0 == mystrcasecmp(effect_type_names[ret_id], str)) {
      return ret_id;
    }
  }

  return EFT_LAST;
}

/**************************************************************************
  Return effect type name; NULL if bad id.
**************************************************************************/
char *effect_type_name(Eff_Type_id id)
{
  assert(sizeof(effect_type_names)/sizeof(effect_type_names[0])==EFT_LAST);

  if (id < EFT_LAST) {
    return effect_type_names[id];
  } else {
    return NULL;
  }
}

/****************************************************************
...
*****************************************************************/
int wonder_replacement(struct city *pcity, Impr_Type_id id)
{
  if(is_wonder(id)) return 0;
  switch (id) {
  case B_BARRACKS:
  case B_BARRACKS2:
  case B_BARRACKS3:
    if (city_affected_by_wonder(pcity, B_SUNTZU))
      return 1;
    break;
  case B_GRANARY:
    if (improvement_variant(B_PYRAMIDS)==0
	&& city_affected_by_wonder(pcity, B_PYRAMIDS))
      return 1;
    break;
  case B_CATHEDRAL:
    if (improvement_variant(B_MICHELANGELO)==0
	&& city_affected_by_wonder(pcity, B_MICHELANGELO))
      return 1;
    break;
  case B_CITY:  
    if (city_affected_by_wonder(pcity, B_WALL))
      return 1;
    break;
  case B_HYDRO:
  case B_POWER:
  case B_NUCLEAR:
    if (city_affected_by_wonder(pcity, B_HOOVER))
      return 1;
    break;
  case B_POLICE:
    if (city_affected_by_wonder(pcity, B_WOMENS))
      return 1;
    break;
  case B_RESEARCH:
    if (city_affected_by_wonder(pcity, B_SETI))
      return 1;
    break;
  default:
    break;
  }
  return 0;
}

/****************************************************************
...
*****************************************************************/
char *get_impr_name_ex(struct city *pcity, Impr_Type_id id)
{
  static char buffer[256];
  char *state = Q_("?wonder:w");
  if (wonder_replacement(pcity, id)) {
    my_snprintf(buffer, sizeof(buffer), "%s(*)",
		get_improvement_type(id)->name);
    return buffer;
  }
  if (!is_wonder(id)) 
    return get_improvement_name(id);

  if (game.global_wonders[id]) state = Q_("?built:B");
  if (wonder_obsolete(id)) state = Q_("?obsolete:O");
  my_snprintf(buffer, sizeof(buffer), "%s(%s)",
	      get_improvement_type(id)->name, state); 
  return buffer;
}

/****************************************************************
...
*****************************************************************/

char *get_improvement_name(Impr_Type_id id)
{
  return get_improvement_type(id)->name; 
}

/**************************************************************************
...
**************************************************************************/

int improvement_value(Impr_Type_id id)
{
  return (improvement_types[id].build_cost);
}

/**************************************************************************
...
**************************************************************************/

int is_wonder(Impr_Type_id id)
{
  return (improvement_types[id].is_wonder);
}

/**************************************************************************
Returns 1 if the improvement_type "exists" in this game, 0 otherwise.
An improvement_type doesn't exist if one of:
- id is out of range;
- the improvement_type has been flagged as removed by setting its
  tech_req to A_LAST;
- it is a space part, and the spacerace is not enabled.
Arguably this should be called improvement_type_exists, but that's too long.
**************************************************************************/
int improvement_exists(Impr_Type_id id)
{
  if (id<0 || id>=B_LAST)
    return 0;

  if ((id==B_SCOMP || id==B_SMODULE || id==B_SSTRUCTURAL)
      && !game.spacerace)
    return 0;

  return (improvement_types[id].tech_req!=A_LAST);
}

/**************************************************************************
Does a linear search of improvement_types[].name
Returns B_LAST if none match.
**************************************************************************/
Impr_Type_id find_improvement_by_name(char *s)
{
  int i;

  for( i=0; i<B_LAST; i++ ) {
    if (strcmp(improvement_types[i].name, s)==0)
      return i;
  }
  return B_LAST;
}

/**************************************************************************
FIXME: remove when gen-impr obsoletes
**************************************************************************/
int improvement_variant(Impr_Type_id id)
{
  return improvement_types[id].variant;
}

/**************************************************************************
...
**************************************************************************/
int improvement_obsolete(struct player *pplayer, Impr_Type_id id) 
{
  if (improvement_types[id].obsolete_by==A_NONE) 
    return 0;
  return (get_invention(pplayer, improvement_types[id].obsolete_by)
	  ==TECH_KNOWN);
}

/**************************************************************************
...
**************************************************************************/
  
int city_buy_cost(struct city *pcity)
{
  int total,cost;
  int build=pcity->shield_stock;

  if (pcity->is_building_unit) {
    total=unit_value(pcity->currently_building);
    if (build>=total)
      return 0;
    cost=(total-build)*2+(total-build)*(total-build)/20; 
  } else {
    total=improvement_value(pcity->currently_building);
    if (build>=total)
      return 0;
    cost=(total-build)*2;
    if(is_wonder(pcity->currently_building))
      cost*=2;
  }
  if(!build)
    cost*=2;
  return cost;
}

/**************************************************************************
...
**************************************************************************/

int wonder_obsolete(Impr_Type_id id)
{ 
  if (improvement_types[id].obsolete_by==A_NONE)
    return 0;
  return (game.global_advances[improvement_types[id].obsolete_by]);
}

/**************************************************************************
Barbarians don't get enough knowledges to be counted as normal players.
**************************************************************************/

int is_wonder_useful(Impr_Type_id id)
{
  if ((id == B_GREAT) && (get_num_human_and_ai_players () < 3)) return 0;
  return 1;
}

/**************************************************************************
...
**************************************************************************/

struct impr_type *get_improvement_type(Impr_Type_id id)
{
  return &improvement_types[id];
}

/**************************************************************************
...
**************************************************************************/

struct player *city_owner(struct city *pcity)
{
  return (&game.players[pcity->owner]);
}

/****************************************************************
...
*****************************************************************/

int could_build_improvement(struct city *pcity, Impr_Type_id id)
{ /* modularized so the AI can choose the tech it wants -- Syela */
  if (!improvement_exists(id))
    return 0;
  if (city_got_building(pcity, id))
    return 0;
  if ((city_got_building(pcity, B_HYDRO)|| city_got_building(pcity, B_POWER) ||
      city_got_building(pcity, B_NUCLEAR)) && (id==B_POWER || id==B_HYDRO || id==B_NUCLEAR))
    return 0;
  if (id==B_RESEARCH && !city_got_building(pcity, B_UNIVERSITY))
    return 0;
  if (id==B_UNIVERSITY && !city_got_building(pcity, B_LIBRARY))
    return 0;
  if (id==B_STOCK && !city_got_building(pcity, B_BANK))
    return 0;
  if (id == B_SEWER && !city_got_building(pcity, B_AQUEDUCT))
    return 0;
  if (id==B_BANK && !city_got_building(pcity, B_MARKETPLACE))
    return 0;
  if (id==B_MFG && !city_got_building(pcity, B_FACTORY))
    return 0;
  if ((id==B_HARBOUR || id==B_COASTAL || id == B_OFFSHORE || id == B_PORT) && !is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN))
    return 0;
  if (id == B_HYDRO
      && !(map_get_terrain(pcity->x, pcity->y) == T_RIVER)
      && !(map_get_special(pcity->x, pcity->y) & S_RIVER)
      && !(map_get_terrain(pcity->x, pcity->y) == T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_RIVER)
      && !is_special_near_tile(pcity->x, pcity->y, S_RIVER)
     )
    return 0;
  if (id == B_SSTRUCTURAL || id == B_SCOMP || id == B_SMODULE) {
    if (!game.global_wonders[B_APOLLO]) {
      return 0;
    } else {
      struct player *p=city_owner(pcity);
      if (p->spaceship.state >= SSHIP_LAUNCHED)
	return 0;
      if (id == B_SSTRUCTURAL && p->spaceship.structurals >= NUM_SS_STRUCTURALS)
	return 0;
      if (id == B_SCOMP && p->spaceship.components >= NUM_SS_COMPONENTS)
	return 0;
      if (id == B_SMODULE && p->spaceship.modules >= NUM_SS_MODULES)
	return 0;
    }
  }
  if (is_wonder(id)) {
    if (game.global_wonders[id]) return 0;
  } else {
    struct player *pplayer=city_owner(pcity);
    if (improvement_obsolete(pplayer, id)) return 0;
  }
  return !wonder_replacement(pcity, id);
}

/****************************************************************
  Whether player could build this improvement, assuming they had
  the tech req, and assuming a city with the right pre-reqs etc.
*****************************************************************/
int could_player_eventually_build_improvement(struct player *p,
					      Impr_Type_id id)
{
  if (!improvement_exists(id))
    return 0;

  /* You can't build an improvement if it's tech requirement is Never. */
  /*   if (improvement_types[id].tech_req == A_LAST) return 0; */
  /* This is what "exists" means, done by improvement_exists() above --dwp */
  
  if (id == B_SSTRUCTURAL || id == B_SCOMP || id == B_SMODULE) {
    if (!game.global_wonders[B_APOLLO]) {
      return 0;
    } else {
      if (p->spaceship.state >= SSHIP_LAUNCHED)
	return 0;
      if (id == B_SSTRUCTURAL && p->spaceship.structurals >= NUM_SS_STRUCTURALS)
	return 0;
      if (id == B_SCOMP && p->spaceship.components >= NUM_SS_COMPONENTS)
	return 0;
      if (id == B_SMODULE && p->spaceship.modules >= NUM_SS_MODULES)
	return 0;
    }
  }
  if (is_wonder(id)) {
    if (game.global_wonders[id]) return 0;
  } else {
    if (improvement_obsolete(p, id)) return 0;
  }
  return 1;
}

int could_player_build_improvement(struct player *p, Impr_Type_id id)
{
  if (!could_player_eventually_build_improvement(p, id))
    return 0;

  /* Make sure we have the tech /now/.*/
  if (get_invention(p, improvement_types[id].tech_req) == TECH_KNOWN)
    return 1;
  return 0;
}
  


/****************************************************************
Can this improvement get built in this city, by the player
who owns the city?
*****************************************************************/
int can_build_improvement(struct city *pcity, Impr_Type_id id)
{
  struct player *p=city_owner(pcity);
  if (!improvement_exists(id))
    return 0;
  if (!player_knows_improvement_tech(p,id))
    return 0;
  return(could_build_improvement(pcity, id));
}

/****************************************************************
Will this city ever be able to build this improvement?
*****************************************************************/
int can_eventually_build_improvement(struct city *pcity, Impr_Type_id id)
{
  /* Can the _player_ ever build the improvement? */
  if (!could_player_eventually_build_improvement(city_owner(pcity), id))
    return 0;
  
  /* The player can build the improvement.  Check for other requirements
     that are city-specific.  At this point, all we really care about is
     whether there are any terrain restrictions -- we figure that improvement
     preconditions (like building a marketplace before a bank) will 
     be settled out on their own. */
  
  /* Does the city already have one of these improvements? */
  if (city_got_building(pcity, id))
    return 0;

  /* At most one power plant per city. */
  if ((city_got_building(pcity, B_HYDRO) || 
       city_got_building(pcity, B_POWER) ||
       city_got_building(pcity, B_NUCLEAR)) && 
      (id==B_POWER || id==B_HYDRO || id==B_NUCLEAR))
    return 0;

  /* Costal improvements require a coast. */
  if ((id==B_HARBOUR || id==B_COASTAL || id == B_OFFSHORE || id == B_PORT) &&
      !is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN))
    return 0;
  
  /* A hydro dam has some fun terrain requirements. */
  if ((id == B_HYDRO)
      && !(map_get_terrain(pcity->x, pcity->y) == T_RIVER)
      && !(map_get_special(pcity->x, pcity->y) & S_RIVER)
      && !(map_get_terrain(pcity->x, pcity->y) == T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_RIVER)
      && !is_special_near_tile(pcity->x, pcity->y, S_RIVER)
     )
    return 0;

  /* Don't allow improvements that are supplanted by wonders. */
  if (wonder_replacement(pcity, id))
    return 0;

  return 1;
}

/****************************************************************
Can a player build this improvement somewhere?  Ignores the
fact that player may not have a city with appropriate prereqs.
*****************************************************************/
int can_player_build_improvement(struct player *p, Impr_Type_id id)
{
  if (!improvement_exists(id))
    return 0;
  if (!player_knows_improvement_tech(p,id))
    return 0;
  return(could_player_build_improvement(p, id));
}

/****************************************************************
Whether given city can build given unit,
ignoring whether unit is obsolete.
*****************************************************************/
int can_build_unit_direct(struct city *pcity, Unit_Type_id id)
{  
  struct player *p=city_owner(pcity);
  if (!unit_type_exists(id))
    return 0;
  if (unit_flag(id, F_NUCLEAR) && !game.global_wonders[B_MANHATTEN])
    return 0;
  if (get_invention(p,unit_types[id].tech_requirement)!=TECH_KNOWN)
    return 0;
  if (!is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN) && is_water_unit(id))
    return 0;
  return 1;
}

/****************************************************************
Whether player can build given unit somewhere,
ignoring whether unit is obsolete and assuming the
player has a coastal city.
*****************************************************************/
int can_player_build_unit_direct(struct player *p, Unit_Type_id id)
{  
  if (!unit_type_exists(id))
    return 0;
  if (unit_flag(id, F_NUCLEAR) && !game.global_wonders[B_MANHATTEN])
    return 0;
  if (get_invention(p,unit_types[id].tech_requirement)!=TECH_KNOWN)
    return 0;
  return 1;
}

/****************************************************************
Whether given city can build given unit;
returns 0 if unit is obsolete.
*****************************************************************/
int can_build_unit(struct city *pcity, Unit_Type_id id)
{  
  if (!can_build_unit_direct(pcity, id))
    return 0;
  while(unit_type_exists((id = unit_types[id].obsoleted_by)))
    if (can_player_build_unit_direct(city_owner(pcity), id))
	return 0;
  return 1;
}

/****************************************************************
Whether player can eventually build given unit in the city;
returns 0 if unit can never possibily be built in this city.
*****************************************************************/
int can_eventually_build_unit(struct city *pcity, Unit_Type_id id)
{
  /* Can the _player_ ever build this unit? */
  if (!can_player_eventually_build_unit(city_owner(pcity), id))
    return 0;

  /* Some units can be built only in certain cities -- for instance,
     ships may be built only in cities adjacent to ocean. */
  if (!is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN) && is_water_unit(id))
    return 0;

  return 1;
}


/****************************************************************
Whether player can build given unit somewhere;
returns 0 if unit is obsolete.
*****************************************************************/
int can_player_build_unit(struct player *p, Unit_Type_id id)
{  
  if (!can_player_build_unit_direct(p, id))
    return 0;
  while(unit_type_exists((id = unit_types[id].obsoleted_by)))
    if (can_player_build_unit_direct(p, id))
	return 0;
  return 1;
}

/****************************************************************
Whether player can _eventually_ build given unit somewhere -- ie,
returns 1 if unit is available with current tech OR will be available
with future tech.  returns 0 if unit is obsolete.
*****************************************************************/
int can_player_eventually_build_unit(struct player *p, Unit_Type_id id)
{
  if (!unit_type_exists(id))
    return 0;
  while(unit_type_exists((id = unit_types[id].obsoleted_by)))
    if (can_player_build_unit_direct(p, id))
	return 0;
  return 1;
}

/**************************************************************************
...
**************************************************************************/

int city_population(struct city *pcity)
{
/*
  int i;
  int res=0;
  for (i=1;i<=pcity->size;i++) res+=i;
  return res*10000;
*/
  /*  Sum_{i=1}^{n} i  ==  n*(n+1)/2  */
  return pcity->size * (pcity->size+1) * 5000;
}

/**************************************************************************
...
**************************************************************************/

int city_got_building(struct city *pcity,  Impr_Type_id id) 
{
  if (!improvement_exists(id))
    return 0;
  else 
    return (pcity->improvements[id]);
}

/**************************************************************************
...
**************************************************************************/
int improvement_upkeep(struct city *pcity, int i) 
{
  if (!improvement_exists(i))
    return 0;
  if (is_wonder(i))
    return 0;
  if (improvement_types[i].upkeep == 1 &&
      city_affected_by_wonder(pcity, B_ASMITHS)) 
    return 0;
  return (improvement_types[i].upkeep);
}

/**************************************************************************
  Caller to pass asmiths = city_affected_by_wonder(pcity, B_ASMITHS)
**************************************************************************/
static int improvement_upkeep_asmiths(struct city *pcity, int i, int asmiths) 
{
  if (!improvement_exists(i))
    return 0;
  if (is_wonder(i))
    return 0;
  if (asmiths && improvement_types[i].upkeep == 1) 
    return 0;
  return (improvement_types[i].upkeep);
}

/**************************************************************************
...
**************************************************************************/

int get_shields_tile(int x, int y, struct city *pcity)
{
  int s=0;
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);
  struct government *g = get_gov_pcity(pcity);
  int celeb = city_celebrating(pcity);
  int before_penalty = (celeb ? g->celeb_shields_before_penalty
			: g->shields_before_penalty);

  if (spec_t & S_SPECIAL_1) 
    s=get_tile_type(tile_t)->shield_special_1;
  else if (spec_t & S_SPECIAL_2) 
    s=get_tile_type(tile_t)->shield_special_2;
  else
    s=get_tile_type(tile_t)->shield;

  if (x == 2 &&  y == 2 && !s) /* Always give a single shield on city square */
    s=1;
  
  if (spec_t & S_MINE) {
    s += (get_tile_type(tile_t))->mining_shield_incr;
  }
  if (spec_t & S_RAILROAD)
    s+=(s*terrain_control.rail_shield_bonus)/100;
  if (city_affected_by_wonder(pcity, B_RICHARDS))
    s++;
  if (tile_t==T_OCEAN && city_got_building(pcity, B_OFFSHORE))
    s++;
  /* government shield bonus & penalty */
  if (s)
    s += (celeb ? g->celeb_shield_bonus : g->shield_bonus);
  if (before_penalty && s > before_penalty)
    s--;
  if (spec_t & S_POLLUTION)
    s-=(s*terrain_control.pollution_shield_penalty)/100; /* The shields here is icky */
  return s;
}

/**************************************************************************
...
**************************************************************************/

int get_trade_tile(int x, int y, struct city *pcity)
{
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);
  struct government *g = get_gov_pcity(pcity);
  int t;
 
  if (spec_t & S_SPECIAL_1) 
    t=get_tile_type(tile_t)->trade_special_1;
  else if (spec_t & S_SPECIAL_2) 
    t=get_tile_type(tile_t)->trade_special_2;
  else
    t=get_tile_type(tile_t)->trade;
    
  if (spec_t & S_RIVER) {
    t += terrain_control.river_trade_incr;
  }
  if (spec_t & S_ROAD) {
    t += (get_tile_type(tile_t))->road_trade_incr;
  }
  if (t) {
    int celeb = city_celebrating(pcity);
    int before_penalty = (celeb ? g->celeb_trade_before_penalty
			  : g->trade_before_penalty);
    
    if (spec_t & S_RAILROAD)
      t+=(t*terrain_control.rail_trade_bonus)/100;

    /* Civ1 specifically documents that Railroad trade increase is before 
     * Democracy/Republic [government in general now -- SKi] bonus  -AJS */
    if (t)
      t += (celeb ? g->celeb_trade_bonus : g->trade_bonus);

    if(city_affected_by_wonder(pcity, B_COLLOSSUS)) 
      t++;
    if((spec_t&S_ROAD) && city_got_building(pcity, B_SUPERHIGHWAYS))
      t+=(t*terrain_control.road_superhighway_trade_bonus)/100;
 
    /* government trade penalty -- SKi */
    if (before_penalty && t > before_penalty) 
      t--;
    if (spec_t & S_POLLUTION)
      t-=(t*terrain_control.pollution_trade_penalty)/100; /* The trade here is dirty */
  }
  return t;
}

/***************************************************************
Here the exact food production should be calculated. That is
including ALL modifiers. 
Center tile acts as irrigated...
***************************************************************/

int get_food_tile(int x, int y, struct city *pcity)
{
  int f;
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);
  struct tile_type *type;
  struct government *g = get_gov_pcity(pcity);
  int celeb = city_celebrating(pcity);
  int before_penalty = (celeb ? g->celeb_food_before_penalty
			: g->food_before_penalty);
  int city_auto_water;

  type=get_tile_type(tile_t);
  city_auto_water = (x==2 && y==2 && tile_t==type->irrigation_result
		     && terrain_control.may_irrigate);

  if (spec_t & S_SPECIAL_1) 
    f=get_tile_type(tile_t)->food_special_1;
  else if (spec_t & S_SPECIAL_2) 
    f=get_tile_type(tile_t)->food_special_2;
  else
    f=get_tile_type(tile_t)->food;

  if ((spec_t & S_IRRIGATION) || city_auto_water) {
    f += type->irrigation_food_incr;
    if (((spec_t & S_FARMLAND) ||
	 (city_auto_water &&
	  player_knows_techs_with_flag(city_owner(pcity), TF_FARMLAND))) &&
	city_got_building(pcity, B_SUPERMARKET)) {
      f += (f * terrain_control.farmland_supermarket_food_bonus) / 100;
    }
  }

  if (tile_t==T_OCEAN && city_got_building(pcity, B_HARBOUR))
    f++;

  if (spec_t & S_RAILROAD)
    f+=(f*terrain_control.rail_food_bonus)/100;

  if (game.civstyle==2 && tile_t==T_MOUNTAINS && x==2 && y==2)
    f++;

  if (f)
    f += (celeb ? g->celeb_food_bonus : g->food_bonus);
  if (before_penalty && f > before_penalty) 
    f--;

  if (spec_t & S_POLLUTION)
    f-=(f*terrain_control.pollution_food_penalty)/100; /* The food here is yucky */

  return f;
}

/**************************************************************************
...
**************************************************************************/

int can_establish_trade_route(struct city *pc1, struct city *pc2)
{
  int i;
  int r=1;
  if(!pc1 || !pc2 || pc1==pc2 ||
     (pc1->owner==pc2->owner && 
      map_distance(pc1->x, pc1->y, pc2->x, pc2->y)<=8))
    return 0;
  
  for(i=0;i<4;i++) {
    r*=pc1->trade[i];
    if (pc1->trade[i]==pc2->id) return 0;
  }
  if (r) return 0;
  r = 1;
  for (i=0;i<4;i++) 
    r*=pc2->trade[i];
  return (!r);
}


/**************************************************************************
...
**************************************************************************/

int trade_between_cities(struct city *pc1, struct city *pc2)
{
  int bonus=0;

  if (pc2 && pc1) {
    bonus=(pc1->tile_trade+pc2->tile_trade+4)/8;
    if (map_get_continent(pc1->x, pc1->y) == map_get_continent(pc2->x, pc2->y))
      bonus/=2;
	
    if (pc1->owner==pc2->owner)
      bonus/=2;
  }
  return bonus;
}

/**************************************************************************
 Return number of trade route city has
**************************************************************************/
int city_num_trade_routes(struct city *pcity)
{
  int i,n;
  for(i=0,n=0; i<4; i++)
    if(pcity->trade[i]) n++;
  
  return n;
}

/*************************************************************************
Calculate amount of gold remaining in city after paying for buildings
*************************************************************************/

int city_gold_surplus(struct city *pcity)
{
  int asmiths = city_affected_by_wonder(pcity, B_ASMITHS);
  int cost=0;
  int i;
  for (i=0;i<B_LAST;i++) 
    if (city_got_building(pcity, i)) 
      cost+=improvement_upkeep_asmiths(pcity, i, asmiths);
  return pcity->tax_total-cost;
}


/**************************************************************************
 Whether a city has an improvement, or the same effect via a wonder.
 (The improvement_type_id should be an improvement, not a wonder.)
 Note also: city_got_citywalls(), and server/citytools:city_got_barracks()
**************************************************************************/
int city_got_effect(struct city *pcity, Impr_Type_id id)
{
  return city_got_building(pcity, id) || wonder_replacement(pcity, id);
}


/**************************************************************************
 Whether a city has its own City Walls, or the same effect via a wonder.
**************************************************************************/
int city_got_citywalls(struct city *pcity)
{
  if (city_got_building(pcity, B_CITY))
    return 1;
  return (city_affected_by_wonder(pcity, B_WALL));
}


/**************************************************************************
...
**************************************************************************/
int city_affected_by_wonder(struct city *pcity, Impr_Type_id id) /*FIX*/
{
  struct city *tmp;
  if (!improvement_exists(id))
    return 0;
  if (!is_wonder(id) || wonder_obsolete(id))
    return 0;
  if (city_got_building(pcity, id))
    return 1;
  
  /* For Manhatten it can be owned by anyone, and it doesn't matter
   * whether it is destroyed or not.
   *
   * (The same goes for Apollo, with respect to building spaceship parts,
   * but not for getting the map effect.  This function only returns true
   * for Apollo for the owner of a non-destroyed Apollo; for building
   * spaceship parts just check (game.global_wonders[id] != 0).
   * (Actually, this function is not currently used for either Manhatten
   * or Apollo.))
   *
   * Otherwise the player who owns the city needs to have it to
   * get the effect.
   */
  if (id==B_MANHATTEN) 
    return (game.global_wonders[id] != 0);
  
  tmp = player_find_city_by_id(get_player(pcity->owner),
			       game.global_wonders[id]);
  if (!tmp)
    return 0;
  switch (id) {
  case B_ASMITHS:
  case B_APOLLO:
  case B_CURE:
  case B_GREAT:
  case B_WALL:
  case B_HANGING:
  case B_ORACLE:
  case B_UNITED:
  case B_WOMENS:
  case B_DARWIN:
  case B_LIGHTHOUSE:
  case B_MAGELLAN:
  case B_MICHELANGELO:
  case B_SETI:
  case B_PYRAMIDS:
  case B_LIBERTY:
  case B_SUNTZU:
    return 1;
  case B_ISAAC:
  case B_COPERNICUS:
  case B_SHAKESPEARE:
  case B_COLLOSSUS:
  case B_RICHARDS:
    return 0;
  case B_HOOVER:
  case B_BACH:
    if (improvement_variant(id)==1) {
      return (map_get_continent(tmp->x, tmp->y) ==
	      map_get_continent(pcity->x, pcity->y));
    } else {
      return 1;
    }
  default:
    return 0;
  }
} 
/***************************************************************
...
***************************************************************/

int city_happy(struct city *pcity)
{
  return (pcity->ppl_happy[4]>=(pcity->size+1)/2 && pcity->ppl_unhappy[4]==0);
}

/**************************************************************************
...
**************************************************************************/
int city_unhappy(struct city *pcity)
{
  return (pcity->ppl_happy[4]<pcity->ppl_unhappy[4]);
}

/**************************************************************************
cities celebrate only after consecutive happy turns
**************************************************************************/
int city_celebrating(struct city *pcity)
{
  struct government *g = get_gov_pcity(pcity);
  return (pcity->size>=g->rapture_size && pcity->was_happy && city_happy(pcity));
}

/**************************************************************************
.rapture is checked instead of city_celebrating() because this function is
called after .was_happy was updated.
**************************************************************************/
int city_rapture_grow(struct city *pcity)
{
  struct government *g = get_gov_pcity(pcity);
  return (pcity->rapture>0 && pcity->food_surplus>0 &&
	  government_has_flag(g, G_RAPTURE_CITY_GROWTH));
}

#ifdef UNUSED
/**************************************************************************
  The slow method for find_city_by_id(), searches though the city lists in
  linear time.  Used in the client, which doesn't need to lookup city IDs that
  often.
**************************************************************************/
static struct city *find_city_by_id_list(int city_id)
{
  int i;
  struct city *pcity;

  for(i=0; i<game.nplayers; i++)
    if((pcity=city_list_find_id(&game.players[i].cities, city_id)))
      return pcity;

  return 0;
}
#endif

/**************************************************************************
...
**************************************************************************/
struct city *city_list_find_id(struct city_list *This, int id)
{
  if(id) {
    struct genlist_iterator myiter;

    genlist_iterator_init(&myiter, &This->list, 0);
    
    for(; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
    if(((struct city *)ITERATOR_PTR(myiter))->id==id)
	return ITERATOR_PTR(myiter);
  }
  return 0;
}

/**************************************************************************
...
**************************************************************************/
struct city *city_list_find_name(struct city_list *This, char *name)
{
  struct genlist_iterator myiter;

  genlist_iterator_init(&myiter, &This->list, 0);

  for(; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
    if(!mystrcasecmp(name, ((struct city *)ITERATOR_PTR(myiter))->name))
      return ITERATOR_PTR(myiter);

  return 0;
}


/**************************************************************************
Comparison function for qsort for city _pointers_, sorting by city name.
Args are really (struct city**), to sort an array of pointers.
(Compare with old_city_name_compare() in game.c, which use city_id's)
**************************************************************************/
int city_name_compare(const void *p1, const void *p2)
{
  return mystrcasecmp( (*(const struct city**)p1)->name,
		       (*(const struct city**)p2)->name );
}

/**************************************************************************
...
**************************************************************************/
int citygov_free_shield(struct city *pcity, struct government *gov)
{
  if (gov->free_shield == G_CITY_SIZE_FREE) {
    return pcity->size;
  } else {
    return gov->free_shield;
  }
}

/**************************************************************************
...
**************************************************************************/
int citygov_free_happy(struct city *pcity, struct government *gov)
{
  if (gov->free_happy == G_CITY_SIZE_FREE) {
    return pcity->size;
  } else {
    return gov->free_happy;
  }
}

/**************************************************************************
...
**************************************************************************/
int citygov_free_food(struct city *pcity, struct government *gov)
{
  if (gov->free_food == G_CITY_SIZE_FREE) {
    return pcity->size;
  } else {
    return gov->free_food;
  }
}

/**************************************************************************
...
**************************************************************************/
int citygov_free_gold(struct city *pcity, struct government *gov)
{
  if (gov->free_gold == G_CITY_SIZE_FREE) {
    return pcity->size;
  } else {
    return gov->free_gold;
  }
}

/**************************************************************************
Evaluate which style should be used to draw a city.
**************************************************************************/
int get_city_style(struct city *pcity)
{
  int replac, style;

  struct player *plr = &game.players[pcity->owner];
  style = plr->city_style;

  while( ((replac = city_styles[style].replaced_by) != -1) &&
         (get_invention( plr, city_styles[replac].techreq) == TECH_KNOWN) ) {
    style = replac;
  }
  return style;
}

/**************************************************************************
Get index to city_styles for style name.
**************************************************************************/
int get_style_by_name(char *style_name)
{
  int i;

  for( i=0; i<game.styles_count; i++) {
    if( !strcmp(style_name,city_styles[i].name) ) 
      break;
  }
  if( i < game.styles_count )
    return i;
  else
    return -1;
}

/**************************************************************************
 Calculates the turns which are needed to build the requested
 type in the city. GUI Independent.
**************************************************************************/
int city_turns_to_build(struct city *pcity, int id, int id_is_unit)
{
  int city_shield_surplus = pcity->shield_surplus;

  if (city_shield_surplus > 0)
  {
    int rounds, cost;
    int shield_stock = pcity->shield_stock;

    if (id_is_unit)
    {
      if (!pcity->is_building_unit)
	shield_stock /= 2;
      cost = get_unit_type(id)->build_cost;
    }
    else
    {
      if (pcity->is_building_unit ||
	  (is_wonder(pcity->currently_building) != is_wonder(id)))
	shield_stock /= 2;
      cost = get_improvement_type(id)->build_cost;
    }

    rounds = (cost - shield_stock + city_shield_surplus - 1) /
      city_shield_surplus;

    return (rounds > 0) ? rounds : 1;
  }

  return 999;
}
