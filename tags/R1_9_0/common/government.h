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
#ifndef FC__GOVERNMENT_H
#define FC__GOVERNMENT_H

#include "shared.h"

struct city;
struct player;
struct Sprite;			/* opaque; client-gui specific */

#define G_MAGIC (127)		/* magic constant, used as flag value */

/* special values for free_* fields -- SKi */
#define G_CITY_SIZE_FREE          G_MAGIC

enum government_flag_id {
  G_BUILD_VETERAN_DIPLOMAT=0,	/* and Spies (in general: all F_DIPLOMAT) */
  G_REVOLUTION_WHEN_UNHAPPY,
  G_HAS_SENATE,			/* not implemented */
  G_UNBRIBABLE,
  G_INSPIRES_PARTISANS,
  G_LAST_FLAG
};
enum government_hint_id {
  G_IS_NICE=0,			/* spaceship auto-placement, among others */
  G_FAVORS_GROWTH,
  G_LAST_HINT
};

/* each government has a list of ruler titles, where at least
 * one entry should have nation=DEFAULT_TITLE.
 */
#define DEFAULT_TITLE  MAX_NUM_ITEMS

struct ruler_title
{
  int  nation;
  char male_title[MAX_LEN_NAME];
  char female_title[MAX_LEN_NAME];
  
  /* untranslated copies: */
  char male_title_orig[MAX_LEN_NAME];    
  char female_title_orig[MAX_LEN_NAME];
};

/* This is struct government itself.  All information about
 * a form of government is contained inhere.
 * -- SKi */
struct government
{
  int   index;			/* index into governments[] array */
  char  name[MAX_LEN_NAME];	/* government name */
  char  name_orig[MAX_LEN_NAME]; /* untranslated copy */
  char  graphic_str[MAX_LEN_NAME];
  char  graphic_alt[MAX_LEN_NAME];
  int   required_tech;		/* tech required to change to this gov */
  int   subgoal;		/* for AI; another government or -1 */

  struct ruler_title *ruler_titles;
  int   num_ruler_titles;

  int   max_rate;		/* max individual Tax/Lux/Sci rate  */
  int   civil_war;              /* chance (from 100) of civil war in
				   right conditions */
  int   martial_law_max;	/* maximum number of units which can
				   enforce martial law */
  int   martial_law_per;        /* number of unhappy citizens made
				   content by each enforcer unit */
  int   empire_size_mod;	/* (signed) offset to game.cityfactor to
				   give city count when number of naturally
				   content citizens is decreased */
  int   rapture_size;		/* minimum city size for rapture; if 0,
				   rapture is not possible */

  /* unit cost modifiers */
  int   unit_happy_cost_factor;
  int   unit_shield_cost_factor;
  int   unit_food_cost_factor;
  int   unit_gold_cost_factor;
  
  /* base cost that a city does not have to "pay" for */
  int   free_happy;
  int   free_shield;
  int   free_food;
  int   free_gold;
  
  /* government production penalties -- SKi */
  int   trade_before_penalty;
  int   shields_before_penalty;
  int   food_before_penalty;

  /* government production penalties when celebrating */
  int   celeb_trade_before_penalty;
  int   celeb_shields_before_penalty;
  int   celeb_food_before_penalty;

  /* government production bonuses -- SKi */
  int   trade_bonus;
  int   shield_bonus;
  int   food_bonus;

  /* government production bonuses when celebrating */
  int   celeb_trade_bonus;
  int   celeb_shield_bonus;
  int   celeb_food_bonus;

  /* corruption modifiers -- SKi */
  int   corruption_level;
  int   corruption_modifier;
  int   fixed_corruption_distance;
  int   corruption_distance_factor;
  int   extra_corruption_distance;
  
  /* other flags: bits in enum government_flag_id order,
     use government_has_flag() to access */
  int   flags;
  /* other hints: bits in enum government_hint_id order,
     use government_has_hint() to access */
  int   hints;

  struct Sprite *sprite;
  
  char *helptext;
};

/* This should possibly disappear; we don't bother sending these to client;
   see code in ai_city.c: ai_manage_cities() for what they mean...
*/
struct ai_gov_tech_hint {
  int tech;
  int turns_factor;
  int const_factor;
  int get_first;
  int done;
};

extern struct government *governments;

extern struct ai_gov_tech_hint ai_gov_tech_hints[MAX_NUM_TECH_LIST];
/* like game.rtech lists, A_LAST terminated (for .tech)
   and techs before that are guaranteed to exist */

struct government *get_government(int gov);
struct government *get_gov_iplayer(int player_num);
struct government *get_gov_pplayer(struct player *pplayer);
struct government *get_gov_pcity(struct city *pcity);

struct government *find_government_by_name(char *name);

enum government_flag_id government_flag_from_str(const char *s);
int government_has_flag(const struct government *gov,
			enum government_flag_id flag);
enum government_hint_id government_hint_from_str(const char *s);
int government_has_hint(const struct government *gov,
			enum government_hint_id hint);

int get_government_max_rate(int type);
int get_government_civil_war_prob(int type);
char *get_government_name(int type);
char *get_ruler_title(int gov, int male, int nation);

int can_change_to_government(struct player *pplayer, int government);

void set_ruler_title(struct government *gov, int nation, 
                     char *male, char *female);

#endif  /* FC__GOVERNMENT_H */
