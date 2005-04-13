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
#ifndef FC__IMPROVEMENT_H
#define FC__IMPROVEMENT_H

/* City Improvements, including Wonders.  (Alternatively "Buildings".) */

#include "shared.h"		/* MAX_LEN_NAME */

#include "effects.h"
#include "fc_types.h"
#include "tech.h"		/* Tech_Type_id */
#include "terrain.h"		/* Terrain_type_id etc */
#include "unittype.h"		/* Unit_Class_id, Unit_Type_id */

/* B_LAST is a value which is guaranteed to be larger than all
 * actual Impr_Type_id values.  It is used as a flag value;
 * it can also be used for fixed allocations to ensure ability
 * to hold full number of improvement types.  */
#define B_LAST MAX_NUM_ITEMS

/* Improvement status (for cities' lists of improvements)
 * An enum or bitfield would be neater here, but we use a typedef for
 * a) less memory usage and b) compatibility with old behaviour */
typedef unsigned char Impr_Status;
#define I_NONE       0   /* Improvement not built */
#define I_ACTIVE     1   /* Improvement built, and having its effect */


/* B_LAST is a value which is guaranteed to be larger than all
 * actual Impr_Type_id values.  It is used as a flag value;
 * it can also be used for fixed allocations to ensure ability
 * to hold full number of improvement types.  */
#define B_LAST MAX_NUM_ITEMS

/* Range of equivalence (used in equiv_range fields)
 * These must correspond to impr_range_names[] in improvement.c. */
enum impr_range {
  IR_NONE,
  IR_CITY,
  IR_ISLAND,
  IR_PLAYER,
  IR_WORLD,
  IR_LAST      /* keep this last */
};

enum impr_genus_id {
  IG_GREAT_WONDER,
  IG_SMALL_WONDER,
  IG_IMPROVEMENT,
  IG_SPECIAL,
  IG_LAST
};


/* Type of improvement. (Read from buildings.ruleset file.) */
struct impr_type {
  const int index;  /* Index in improvement_types array */
  enum impr_genus_id genus;		/* genus; e.g. GreatWonder */
  const char *name; /* Translated string - doesn't need freeing. */
  char name_orig[MAX_LEN_NAME];		/* untranslated */
  char graphic_str[MAX_LEN_NAME];	/* city icon of improv. */
  char graphic_alt[MAX_LEN_NAME];	/* city icon of improv. */
  Tech_Type_id tech_req;		/* A_LAST = never; A_NONE = always */
  Impr_Type_id bldg_req;		/* B_LAST = none required */
  Terrain_type_id *terr_gate;	/* list; T_NONE terminated */
  enum tile_special_type *spec_gate;	/* list; S_NO_SPECIAL terminated */
  Tech_Type_id obsolete_by;		/* A_LAST = never obsolete */
  Impr_Type_id replaced_by;		/* B_LAST = never replaced */
  int build_cost;			/* Use wrappers to access this. */
  int upkeep;
  int sabotage;		/* Base chance of diplomat sabotage succeeding. */
  char *helptext;
  char soundtag[MAX_LEN_NAME];
  char soundtag_alt[MAX_LEN_NAME];
};


extern struct impr_type improvement_types[B_LAST];

/* impr range id/string converters */
enum impr_range impr_range_from_str(const char *str);
const char *impr_range_name(enum impr_range id);

/* impr genus id/string converters */
enum impr_genus_id impr_genus_from_str(const char *s);

/* improvement functions */
void improvements_init(void);
void improvements_free(void);
struct impr_type *get_improvement_type(Impr_Type_id id);
bool improvement_exists(Impr_Type_id id);

int impr_build_shield_cost(Impr_Type_id id);
int impr_buy_gold_cost(Impr_Type_id id, int shields_in_stock);
int impr_sell_gold(Impr_Type_id id);

bool is_wonder(Impr_Type_id id);
const char *get_improvement_name(Impr_Type_id id);
const char *get_improvement_name_orig(Impr_Type_id id);

bool improvement_obsolete(const struct player *pplayer, Impr_Type_id id);
Impr_Type_id find_improvement_by_name(const char *s);
Impr_Type_id find_improvement_by_name_orig(const char *s);

/* player related improvement and unit functions */
bool can_player_build_improvement_direct(const struct player *p,
					 Impr_Type_id id);
bool can_player_build_improvement(const struct player *p, Impr_Type_id id);
bool can_player_eventually_build_improvement(const struct player *p,
					     Impr_Type_id id);

/* Iterates over all improvements. Creates a new variable names m_i
 * with type Impr_Type_id which holds the id of the current improvement. */
#define impr_type_iterate(m_i)                                                \
{                                                                             \
  Impr_Type_id m_i;                                                           \
  for (m_i = 0; m_i < game.num_impr_types; m_i++) {

#define impr_type_iterate_end                                                 \
  }                                                                           \
}

bool is_great_wonder(Impr_Type_id id);
bool is_small_wonder(Impr_Type_id id);
bool is_improvement(Impr_Type_id id);

struct city *find_city_from_great_wonder(Impr_Type_id id);
struct city *find_city_from_small_wonder(const struct player *pplayer,
					 Impr_Type_id id);

bool great_wonder_was_built(Impr_Type_id id);

bool can_sell_building(Impr_Type_id id);
bool can_city_sell_building(struct city *pcity, Impr_Type_id id);

#endif  /* FC__IMPROVEMENT_H */

