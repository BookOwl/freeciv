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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "city.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "shared.h"
#include "tech.h"
#include "unit.h"

#include "player.h"

extern int is_server;

/* Additional goals added by Syela 
 * Note not all of the preferences etc in the following are
 * actually used currently. -- dwp
 */
struct player_race races[]= { 
  {"Roman", "Romans", "f.italy",          1, 2, 2,
     {100,100,100,100,100,100,100},
   { {"The Republic", "The Wheel", "Iron Working", "Construction", "Railroad",
      "", "", "", "", ""},
     "Leonardo's Workshop",   /* wonder not actually checked in choose_build */
     "Democracy"}
  },
  {"Babylonian", "Babylonians", "f.iraq_old", 0, 0, 2,
     {100,100,100,100,100,100,100},
   { {"Monarchy", "Philosophy", "Mathematics", "Chivalry", "Theology",
      "Religion" /*civ1*/, "", "", "", ""},
     "Hanging Gardens",
     "Monarchy"}
  },
  {"German", "Germans", "f.germany",         2, 0, 2,        
   {100,100,100,100,100,100,100},
   { {"The Republic", "Iron Working", "Gunpowder", "Explosives", "Flight",
      "", "", "", "", ""},
     "J.S. Bach's Cathedral",
     "Republic"}
  },
  {"Egyptian", "Egyptians", "f.egypt",    1, 1, 2,    
   {100,100,100,100,100,100,100}, /* different order for experiment and flavor */
   { {"Monarchy", "Philosophy", "Navigation", "Iron Working", "Railroad",
      "", "", "", "", ""},
     "Pyramids"  ,
     "Monarchy"}
  },
  {"American", "Americans", "f.usa",     0, 1, 2,    
   {100,100,100,100,100,100,100},
   { {"The Republic", "Trade", "Engineering", "Democracy", "Railroad",
      "Explosives", "Automobile", "", "", ""},
     "Statue of Liberty",
     "Democracy"}
  },
  {"Greek", "Greeks", "f.greece",         1, 2, 0,          
   {100,100,100,100,100,100,100},
   { {"The Republic", "Philosophy", "Trade", "Engineering", "Iron Working",
      "Railroad", "", "", "", ""},
     "Lighthouse",
     "Republic"}
  },
  {"Indian", "Indians", "f.india",        0, 0, 1,        
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Philosophy", "The Republic", "Iron Working", "Engineering",
      "Railroad", "", "", "", ""},
     "Oracle",
     "Republic"}
  },
  {"Russian", "Russians", "f.russia",      2, 1, 0,      
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Philosophy", "Chivalry", "Trade", "Bridge Building",
      "Railroad", "Communism", "", "", ""},
     "Women's Suffrage",
     "Communism"}
  },
  {"Zulu", "Zulus", "f.rwanda",            2, 1, 1,            
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Philosophy", "Chivalry", "Trade", "Bridge Building",
      "Railroad", "Communism", "", "", ""},
     "Apollo Program",
     "Communism"}
  },
  {"French", "French", "f.france",         2, 2, 2,         
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Chivalry", "Philosophy", "The Republic", "Monotheism",
      "Religion" /*civ1*/, "Engineering", "Navigation", "Railroad", ""},
     "Magellan's Expedition",
     "Republic"}
  },
  {"Aztec", "Aztecs", "f.mexico",          1, 0, 2,          
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Chivalry", "Iron Working", "Trade", "Navigation",
      "Railroad", "Communism", "", "", ""},
     "Hoover Dam",
     "Communism"}
  },
  {"Chinese", "Chinese", "f.china",       1, 1, 2,     
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Trade", "Philosophy", "Bridge Building", "Railroad",
      "Communism", "", "", "", ""},
     "Great Wall",
     "Communism"}
  },
  {"English", "English", "f.united_kingdom",       1, 2, 1,     
     {100,100,100,100,100,100,100},
   { {"Monarchy", "Chivalry", "Trade", "Theology", "Religion" /*civ1*/,
      "Navigation", "Democracy", "Railroad", "", ""},
     "King Richard's Crusade",
     "Democracy"}
  },
  {"Mongol", "Mongols", "f.mongolia",        2, 2, 0,      
   {100,100,100,100,100,100,100},
   { {"Monarchy", "Chivalry", "Trade", "Bridge Building", "Railroad",
      "", "", "", "", ""},
     "Sun Tzu's War Academy",
     "Monarchy"}
  }
};

char *default_race_leader_names[] = {
  "Caesar",
  "Hammurabi",
  "Frederick",
  "Rameses",
  "Lincoln",
  "Alexander",
  "Gandhi",
  "Stalin",
  "Shaka",
  "Napoleon",
  "Montezuma",
  "Mao",
  "Elizabeth",
  "Genghis"
};

/***************************************************************
...
***************************************************************/
int player_has_embassy(struct player *pplayer, struct player *pplayer2)
{
  return (pplayer->embassy & (1<<pplayer2->player_no)) ||
         (player_owns_active_wonder(pplayer, B_MARCO) && pplayer != pplayer2);
}

/****************************************************************
...
*****************************************************************/

int player_owns_city(struct player *pplayer, struct city *pcity)
{
  if (!pcity || !pplayer) return 0; /* better safe than sorry */
  return (pcity->owner==pplayer->player_no);
}

/***************************************************************
...
***************************************************************/
enum race_type find_race_by_name(char *name)
{
  int i;

  for(i=0; i<R_LAST; i++)
     if(!mystrcasecmp(name, get_race_name (i)))
	return i;

  return -1;
}

/***************************************************************
...
***************************************************************/
char *get_race_name(enum race_type race)
{
  return races[race].name;
}

/***************************************************************
...
***************************************************************/
char *get_race_name_plural(enum race_type race)
{
  return races[race].name_plural;
}

struct player_race *get_race(struct player *plr)
{
  return &races[plr->race];
}

/***************************************************************
...
***************************************************************/
void player_init(struct player *plr)
{
  plr->player_no=plr-game.players;

  strcpy(plr->name, "YourName");
  plr->is_male = 1;
  plr->government=game.default_government;
  plr->race=R_LAST;
  plr->capital=0;
  unit_list_init(&plr->units);
  city_list_init(&plr->cities);
  strcpy(plr->addr, "---.---.---.---");
  plr->is_alive=1;
  plr->embassy=0;
  plr->ai.control=0;
  plr->ai.tech_goal = A_NONE;
  plr->ai.handicap = 0;
  plr->ai.skill_level = 0;
  plr->ai.fuzzy = 0;
  plr->ai.expand = 100;
  plr->future_tech=0;
  plr->economic.tax=PLAYER_DEFAULT_TAX_RATE;
  plr->economic.science=PLAYER_DEFAULT_SCIENCE_RATE;
  plr->economic.luxury=PLAYER_DEFAULT_LUXURY_RATE;
  spaceship_init(&plr->spaceship);
}

/***************************************************************
...
***************************************************************/
void player_set_unit_focus_status(struct player *pplayer)
{
  unit_list_iterate(pplayer->units, punit) 
    punit->focus_status=FOCUS_AVAIL;
  unit_list_iterate_end;
}

/***************************************************************
...
***************************************************************/
struct player *find_player_by_name(char *name)
{
  int i;

  for(i=0; i<game.nplayers; i++)
     if(!mystrcasecmp(name, game.players[i].name))
	return &game.players[i];

  return 0;
}

/***************************************************************
no map visibility check here!
***************************************************************/
int player_can_see_unit(struct player *pplayer, struct unit *punit)
{
  if(punit->owner==pplayer->player_no)
    return 1;
  if(is_hiding_unit(punit)) {
    /* Search for units/cities that might be able to see the sub/missile */
    int x, y;
    struct city *pcity;
    struct unit *punit2;
    for(y=punit->y-1; y<=punit->y+1; ++y) { 
      if(y<0 || y>=map.ysize)
	continue;
      for(x=punit->x-1; x<=punit->x+1; ++x) { 
	if((punit2=unit_list_get(&map_get_tile(x, y)->units, 0)) &&
	   punit2->owner == pplayer->player_no ) return 1;

	if((pcity=map_get_city(x, y)) && pcity->owner==pplayer->player_no)
	  return 1;
      }
    }
    return 0;
  }
  
  return 1;
}

/***************************************************************
  Return a pointer to a visible unit, if there is one.
***************************************************************/
struct unit *player_find_visible_unit(struct player *pplayer, struct tile *ptile)
{
  if(unit_list_size(&ptile->units)==0) return NULL;

  unit_list_iterate(ptile->units, punit)
    if(player_can_see_unit(pplayer, punit)) return punit;
  unit_list_iterate_end;

  return NULL;
}

/***************************************************************
 If the specified player owns the city with the specified id,
 return pointer to the city struct.  Else return 0.
 In the server we want to use find_city_by_id, which is fast due
 to the citycache, while in the client we just search the
 player's cities, rather than find_city_by_id which goes through
 all players.
***************************************************************/
struct city *player_find_city_by_id(struct player *pplayer, int city_id)
{
  struct city *pcity;
  
  if(is_server) {
    pcity = find_city_by_id(city_id);
    if(pcity && (pcity->owner==pplayer->player_no)) {
      return pcity;
    } else {
      return 0;
    }
  } else {
    return city_list_find_id(&pplayer->cities, city_id);
  }
}

/**************************************************************************
 Return 1 if one of the player's cities has the specified wonder,
 and it is not obsolete.
**************************************************************************/
int player_owns_active_wonder(struct player *pplayer,
			      enum improvement_type_id id)
{
  return (improvement_exists(id)
	  && is_wonder(id)
	  && (!wonder_obsolete(id))
	  && player_find_city_by_id(pplayer, game.global_wonders[id]));
}

/**************************************************************************
 ...
**************************************************************************/
int player_owns_active_govchange_wonder(struct player *pplayer)
{
  return ( player_owns_active_wonder(pplayer, B_LIBERTY) ||
	   ( (improvement_variant(B_PYRAMIDS)==1) &&
	     player_owns_active_wonder(pplayer, B_PYRAMIDS) ) ||
	   ( (improvement_variant(B_UNITED)==1) &&
	     player_owns_active_wonder(pplayer, B_UNITED) ) );
}

/**************************************************************************
Locate the city where the players palace is located, (NULL Otherwise) 
**************************************************************************/
struct city *find_palace(struct player *pplayer)
{
  city_list_iterate(pplayer->cities, pcity) 
    if (city_got_building(pcity, B_PALACE)) 
      return pcity;
  city_list_iterate_end;
  return NULL;
}

/**************************************************************************
...
**************************************************************************/
int player_knows_improvement_tech(struct player *pplayer,
				   enum improvement_type_id id)
{
  int t;
  if (!improvement_exists(id)) return 0;
  t = get_improvement_type(id)->tech_requirement;
  return (get_invention(pplayer, t) == TECH_KNOWN);
}

/**************************************************************************
...
**************************************************************************/
int ai_handicap(struct player *pplayer, enum handicap_type htype)
{
  if (!pplayer->ai.control) return -1;
  return(pplayer->ai.handicap&htype);
}

/**************************************************************************
Return the value normal_decision (a boolean), except if the AI is fuzzy,
then sometimes flip the value.  The intention of this is that instead of
    if (condition) { action }
you can use
    if (ai_fuzzy(pplayer, condition)) { action }
to sometimes flip a decision, to simulate an AI with some confusion,
indecisiveness, forgetfulness etc. In practice its often safer to use
    if (condition && ai_fuzzy(pplayer,1)) { action }
for an action which only makes sense if condition holds, but which a
fuzzy AI can safely "forget".  Note that for a non-fuzzy AI, or for a
human player being helped by the AI (eg, autosettlers), you can ignore
the "ai_fuzzy(pplayer," part, and read the previous example as:
    if (condition && 1) { action }
--dwp
**************************************************************************/
int ai_fuzzy(struct player *pplayer, int normal_decision)
{
  if (!pplayer->ai.control || !pplayer->ai.fuzzy) return normal_decision;
  if (myrand(1000) >= pplayer->ai.fuzzy) return normal_decision;
  return !normal_decision;
}

/**************************************************************************
  This converts the goal strings in races[] to integers.
  This should only be done after rulesets are loaded!
  Messages are LOG_VERBOSE because its quite possible they
  are not real errors.
**************************************************************************/
void init_race_goals(void)
{
  char *str, *name;
  int val, i, j;
  struct player_race *prace;
  struct government *gov;

  for(prace=races; prace<races+R_LAST; prace++) {
    name = prace->name_plural;
    str = prace->goals_str.government;
    gov = find_government_by_name(str);
    if(gov == NULL) {
      freelog(LOG_VERBOSE, "Didn't match goal government name \"%s\" for %s",
	      str, name);
      val = game.government_when_anarchy;  /* flag value (no goal) (?) */
    } else {
      val = gov->index;
    }
    freelog(LOG_DEBUG, "%s gov goal %d %s", name, val, str);
    prace->goals.government = val;
    
    str = prace->goals_str.wonder;
    val = find_improvement_by_name(str);
    /* for any problems, leave as B_LAST */
    if(val == B_LAST) {
      freelog(LOG_VERBOSE, "Didn't match goal wonder \"%s\" for %s", str, name);
    } else if(!improvement_exists(val)) {
      freelog(LOG_VERBOSE, "Goal wonder \"%s\" for %s doesn't exist", str, name);
      val = B_LAST;
    } else if(!is_wonder(val)) {
      freelog(LOG_VERBOSE, "Goal wonder \"%s\" for %s not a wonder", str, name);
      val = B_LAST;
    }
    prace->goals.wonder = val;
    freelog(LOG_DEBUG, "%s wonder goal %d %s", name, val, str);

    /* i is index is goals_str, j is index (good values) in goals */
    j = 0;			
    for(i=0; i<MAX_NUM_TECH_GOALS; i++) {
      str = prace->goals_str.tech[i];
      if(str[0] == '\0')
	continue;
      val = find_tech_by_name(str);
      if(val == A_LAST) {
	freelog(LOG_VERBOSE, "Didn't match goal tech %d \"%s\" for %s",
		i, str, name);
      } else if(!tech_exists(val)) {
	freelog(LOG_VERBOSE, "Goal tech %d \"%s\" for %s doesn't exist",
		i, str, name);
	val = A_LAST;
      }
      if(val != A_LAST && val != A_NONE) {
	freelog(LOG_DEBUG, "%s tech goal (%d) %3d %s", name, j, val, str);
	prace->goals.tech[j++] = val;
      }
    }
    freelog(LOG_DEBUG, "%s %d tech goals", name, j);
    if(j==0) {
      freelog(LOG_VERBOSE, "No valid goal techs for %s", name);
    }
    while(j<MAX_NUM_TECH_GOALS) {
      prace->goals.tech[j++] = A_NONE;
    }
  }
}

/**************************************************************************
Command access levels for client-side use; at present, they are 
only used to control access to server commands typed at the client chatline.
**************************************************************************/
static char *levelnames[] = {
  "none",
  "info",
  "ctrl",
  "hack"
};

char *cmdlevel_name(enum cmdlevel_id lvl)
{
  assert (lvl >= 0 && lvl < ALLOW_NUM);
  return levelnames[lvl];
}

enum cmdlevel_id cmdlevel_named (char *token)
{
  enum cmdlevel_id i;
  int len = strlen(token);

  for (i = 0; i < ALLOW_NUM; ++i) {
    if (strncmp(levelnames[i], token, len) == 0) {
      return i;
    }
  }

  return ALLOW_UNRECOGNIZED;
}
