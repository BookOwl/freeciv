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
#ifndef FC__EFFECTS_H
#define FC__EFFECTS_H


#include "shared.h"		/* bool */
#include "terrain.h"

/* Range of effects (used in equiv_range and effect.range fields)
 * These must correspond to effect_range_names[] in improvement.c. */
enum effect_range {
  EFR_LOCAL,
  EFR_CITY,
  EFR_ISLAND,
  EFR_PLAYER,
  EFR_WORLD,
  EFR_LAST   /* keep this last */
};

/* Type of effects. (Used in effect.type field)
 * These must correspond to effect_type_names[] in effects.c. */
enum effect_type {
  EFT_ADV_PARASITE,
  EFT_AIRLIFT,
  EFT_ANY_GOVERNMENT,
  EFT_BARB_ATTACK,
  EFT_BARB_DEFEND,
  EFT_CAPITAL_CITY,
  EFT_CAPITAL_EXISTS,
  EFT_ENABLE_NUKE,
  EFT_ENABLE_SPACE,
  EFT_ENEMY_PEACEFUL,
  EFT_FOOD_ADD_TILE,
  EFT_FOOD_BONUS,
  EFT_FOOD_INC_TILE,
  EFT_FOOD_PER_TILE,
  EFT_GIVE_IMM_ADV,
  EFT_GROWTH_FOOD,
  EFT_HAVE_EMBASSIES,
  EFT_IMPROVE_REP,
  EFT_LUXURY_BONUS,
  EFT_LUXURY_PCT,
  EFT_MAKE_CONTENT,
  EFT_MAKE_CONTENT_MIL,
  EFT_MAKE_CONTENT_PCT,
  EFT_MAKE_HAPPY,
  EFT_MAY_DECLARE_WAR,
  EFT_NO_ANARCHY,
  EFT_NO_SINK_DEEP,
  EFT_NUKE_PROOF,
  EFT_POLLU_ADJ,
  EFT_POLLU_ADJ_POP,
  EFT_POLLU_ADJ_PROD,
  EFT_POLLU_SET,
  EFT_POLLU_SET_POP,
  EFT_POLLU_SET_PROD,
  EFT_PROD_ADD_TILE,
  EFT_PROD_BONUS,
  EFT_PROD_INC_TILE,
  EFT_PROD_PER_TILE,
  EFT_PROD_TO_GOLD,
  EFT_REDUCE_CORRUPT,
  EFT_REDUCE_WASTE,
  EFT_REVEAL_CITIES,
  EFT_REVEAL_MAP,
  EFT_REVOLT_DIST,
  EFT_SCIENCE_BONUS,
  EFT_SCIENCE_PCT,
  EFT_SIZE_UNLIMIT,
  EFT_SLOW_NUKE_WINTER,
  EFT_SLOW_GLOBAL_WARM,
  EFT_SPACE_PART,
  EFT_SPY_RESISTANT,
  EFT_TAX_BONUS,
  EFT_TAX_PCT,
  EFT_TRADE_ADD_TILE,
  EFT_TRADE_BONUS,
  EFT_TRADE_INC_TILE,
  EFT_TRADE_PER_TILE,
  EFT_TRADE_ROUTE_PCT,
  EFT_UNIT_DEFEND,
  EFT_UNIT_MOVE,
  EFT_UNIT_NO_LOSE_POP,
  EFT_UNIT_RECOVER,
  EFT_UNIT_REPAIR,
  EFT_UNIT_VET_COMBAT,
  EFT_UNIT_VETERAN,
  EFT_UPGRADE_UNITS,
  EFT_UPGRADE_ONE_STEP,
  EFT_UPGRADE_ONE_LEAP,
  EFT_UPGRADE_ALL_STEP,
  EFT_UPGRADE_ALL_LEAP,
  EFT_UPKEEP_FREE,
  EFT_LAST	/* keep this last */
};

#define EFT_ALL EFT_LAST

/* lookups */
enum effect_range effect_range_from_str(const char *str);
const char *effect_range_name(enum effect_range id);
enum effect_type effect_type_from_str(const char *str);
const char *effect_type_name(enum effect_type id);

#endif  /* FC__EFFECTS_H */
