/********************************************************************** 
 Freeciv - Copyright (C) 2004 - The Freeciv Team
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

#include <stdio.h>  
#include <string.h>  
#include <assert.h>

#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "mem.h"
#include "log.h"
#include "packets.h"
#include "path_finding.h"
#include "pf_tools.h"
#include "player.h"
#include "support.h" 
#include "timing.h"

#include "citytools.h"
#include "gotohand.h"
#include "maphand.h"
#include "settlers.h"
#include "unittools.h"

#include "aicity.h"
#include "aidata.h"
#include "aiferry.h"
#include "ailog.h"
#include "aitools.h"
#include "aiunit.h"
#include "citymap.h"

#include "aisettler.h"

/* COMMENTS */
/* 
   This code tries hard to do the right thing, including looking
   into the future (wrt to government), and also doing this in a
   modpack friendly manner. However, there are some pieces missing.

   A tighter integration into the city management code would 
   give more optimal city placements, since existing cities could
   move their workers around to give a new city better placement.
   Occasionally you will see cities being placed sub-optimally
   because the best city center tile is taken when another tile
   could have been worked instead by the city that took it.

   The code is not generic enough. It assumes smallpox too much,
   and should calculate with a future city of a bigger size.

   We need to stop the build code from running this code all the
   time and instead try to complete what it has started.
*/

/* A big WAG to save a big amount of CPU: */
#define RESULT_IS_ENOUGH 250

#define FERRY_TECH_WANT 500

#define GROWTH_PRIORITY 15

/* Perfection gives us an idea of how long to search for optimal
 * solutions, instead of going for quick and dirty solutions that
 * waste valuable time. Decrease for maps where good city placements
 * are hard to find. Lower means more perfection. */
#define PERFECTION 3

/* How much to deemphasise the potential for city growth for city
 * placements. Decrease this value if large cities are important
 * or planned. Increase if running strict smallpox. */
#define GROWTH_POTENTIAL_DEEMPHASIS 8

/* Percentage bonus to city locations near an ocean. */
#define NAVAL_EMPHASIS 20

/* Modifier for defense bonus that is applied to city location want. 
 * This is % of defense % to increase want by. */
#define DEFENSE_EMPHASIS 20

static struct {
  int sum;
  char food;
  char trade;
  char shield;
} cachemap[MAP_MAX_WIDTH][MAP_MAX_HEIGHT];

/**************************************************************************
  Fill cityresult struct with useful info about the city spot. It must 
  contain valid x, y coordinates and total should be zero.

  We assume whatever best government we are aiming for.

  We always return valid other_x and other_y if total > 0.
**************************************************************************/
void cityresult_fill(struct player *pplayer,
                     struct ai_data *ai,
                     struct cityresult *result)
{
  struct city *pcity = map_get_city(result->x, result->y);
  struct tile *ptile = NULL;
  int sum = 0;
  bool virtual_city = FALSE;
  int curr_govt = pplayer->government;
  bool handicap = ai_handicap(pplayer, H_MAP);

  pplayer->government = ai->goal.govt.idx;

  result->best_other = 0;
  result->other_x = -1;
  result->other_y = -1;
  result->o_x = -1; /* as other_x but city-relative */
  result->o_y = -1;
  result->remaining = 0;
  result->overseas = FALSE;
  result->virt_boat = FALSE;
  result->result = -666;

  if (!pcity) {
    pcity = create_city_virtual(pplayer, result->x, result->y, "Virtuaville");
    virtual_city = TRUE;
  }

  city_map_checked_iterate(result->x, result->y, i, j, map_x, map_y) {
    int reserved = citymap_read(map_x, map_y);
    bool city_center = is_city_center(i, j);

    ptile = map_get_tile(map_x, map_y);

    if (reserved < 0
        || (handicap && !map_is_known(map_x, map_y, pplayer))
        || ptile->worked != NULL) {
      /* Tile is reserved or we can't see it */
      result->tile[i][j].shield = 0;
      result->tile[i][j].trade = 0;
      result->tile[i][j].food = 0;
      sum = 0;
    } else if (cachemap[map_x][map_y].sum <= 0 || city_center) {
      /* We cannot read city center from cache */

      /* Food */
      result->tile[i][j].food = base_city_get_food_tile(i, j, pcity, FALSE);

      /* Shields */
      result->tile[i][j].shield =base_city_get_shields_tile(i, j, pcity, FALSE);

      /* Trade */
      result->tile[i][j].trade = base_city_get_trade_tile(i, j, pcity, FALSE);

      sum = result->tile[i][j].food * ai->food_priority
            + result->tile[i][j].trade * ai->science_priority
            + result->tile[i][j].shield * ai->shield_priority;

      /* Balance perfection */
      sum *= PERFECTION / 2;
      if (result->tile[i][j].food >= 2) {
        sum *= 2; /* we need this to grow */
      }

      if (!city_center && virtual_city) {
        /* real cities and any city center will give us spossibly
         * skewed results */
        cachemap[map_x][map_y].sum = sum;
        cachemap[map_x][map_y].trade = result->tile[i][j].trade;
        cachemap[map_x][map_y].shield = result->tile[i][j].shield;
        cachemap[map_x][map_y].food = result->tile[i][j].food;
      }
    } else {
      sum = cachemap[map_x][map_y].sum;
      result->tile[i][j].shield = cachemap[map_x][map_y].shield;
      result->tile[i][j].trade = cachemap[map_x][map_y].trade;
      result->tile[i][j].food = cachemap[map_x][map_y].food;
    }
    result->tile[i][j].reserved = reserved;

    /* Avoid crowdedness, except for city center. */
    if (sum > 0) {
      sum -= MIN(reserved * GROWTH_PRIORITY, sum - 1);
    }

    /* Calculate city center and best other than city center */
    if (city_center) {
      result->city_center = sum;
    } else if (sum > result->best_other) {
      /* First add other other to remaining */
      result->remaining += result->best_other
                           / GROWTH_POTENTIAL_DEEMPHASIS;
      /* Then make new best other */
      result->best_other = sum;
      result->other_x = map_x;
      result->other_y = map_y;
      result->o_x = i;
      result->o_y = j;
    } else {
      /* Save total remaining calculation, divided by crowdedness
       * of the area and the emphasis placed on space for growth. */
      result->remaining += sum / GROWTH_POTENTIAL_DEEMPHASIS;
    }
  } city_map_checked_iterate_end;

  if (virtual_city) {
    /* Baseline is a size one city (city center + best extra tile). */
    result->total = result->city_center + result->best_other;
  } else if (result->best_other != -1) {
    /* Baseline is best extra tile only. This is why making new cities 
     * is so darn good. */
    result->total = result->best_other;
  } else {
    /* There is no available tile in this city. All is worked. */
    result->total = 0;
    return;
  }

  if (virtual_city) {
    /* Corruption and waste of a size one city deducted. Notice that we
     * don't do this if 'fulltradesize' is changed, since then we'd
     * never make cities. */
    if (game.fulltradesize == 1) {
      result->corruption = ai->science_priority
	* city_corruption(pcity, 
			  result->tile[result->o_x][result->o_y].trade
			  + result->tile[2][2].trade);
    } else {
      result->corruption = 0;
    }
    result->waste = ai->shield_priority
      * city_waste(pcity,
		   result->tile[result->o_x][result->o_y].shield
		   + result->tile[2][2].shield);
  } else {
    /* Deduct difference in corruption and waste for real cities. Note that it
     * is possible (with notradesize) that we _gain_ value here. */
    pcity->size++;
    result->corruption = ai->science_priority
      * (city_corruption(pcity,
			 result->tile[result->o_x][result->o_y].trade)
	 - pcity->corruption);
    result->waste = ai->shield_priority
      * (city_waste(pcity,
		    result->tile[result->o_x][result->o_y].shield)
	 - pcity->shield_waste);
    pcity->size--;
  }
  result->total -= result->corruption;
  result->total -= result->waste;
  result->total = MAX(0, result->total);

  pplayer->government = curr_govt;
  if (virtual_city) {
    remove_city_virtual(pcity);
  }

  assert(result->total == 0 || result->best_other != -1);
  assert(result->total == 0 || result->other_x != -1);
  assert(result->total == 0 || result->other_y != -1);
  assert(result->city_center >= 0);
  assert(result->remaining >= 0);
}

/**************************************************************************
  Check if a city on this location would starve.
**************************************************************************/
static bool food_starvation(struct cityresult *result)
{
  /* Avoid starvation: We must have enough food to grow. */
  return (result->tile[2][2].food 
          + result->tile[result->o_x][result->o_y].food < 3);
}

/**************************************************************************
  Check if a city on this location would lack shields.
**************************************************************************/
static bool shield_starvation(struct cityresult *result)
{
  /* Avoid resource starvation. */
  return (result->tile[2][2].shield
          + result->tile[result->o_x][result->o_y].shield == 0);
}

/**************************************************************************
  Calculate defense bonus, which is a % of total results equal to a
  given % of the defense bonus %.
**************************************************************************/
static int defense_bonus(struct cityresult *result, struct ai_data *ai)
{
  /* Defense modification (as tie breaker mostly) */
  int defense_bonus = 
            get_tile_type(map_get_terrain(result->x, result->y))->defense_bonus;
  if (map_has_special(result->x, result->y, S_RIVER)) {
    defense_bonus +=
        (defense_bonus * terrain_control.river_defense_bonus) / 100;
  }

  return 100 / result->total * (100 / defense_bonus * DEFENSE_EMPHASIS);
}

/**************************************************************************
  Add bonus for coast.
**************************************************************************/
static int naval_bonus(struct cityresult *result, struct ai_data *ai)
{
  bool ocean_adjacent = is_terrain_near_tile(result->x, result->y, T_OCEAN);

  /* Adjust for ocean adjacency, which is nice */
  if (ocean_adjacent) {
    return (result->total * NAVAL_EMPHASIS) / 100;
  } else {
    return 0;
  }
}

/**************************************************************************
  For debugging, print the city result table.
**************************************************************************/
void print_cityresult(struct player *pplayer, struct cityresult *cr,
                      struct ai_data *ai)
{
  freelog(LOG_NORMAL, "Result=(%d, %d)\nReservations:\n"
          "     %4d %4d %4d   \n"
          "%4d %4d %4d %4d %4d\n"
          "%4d %4d %4d %4d %4d\n"
          "%4d %4d %4d %4d %4d\n"
          "     %4d %4d %4d", cr->x, cr->y,
          cr->tile[1][0].reserved, cr->tile[2][0].reserved, 
          cr->tile[3][0].reserved, cr->tile[0][1].reserved,
          cr->tile[1][1].reserved, cr->tile[2][1].reserved,
          cr->tile[3][1].reserved, cr->tile[4][1].reserved,
          cr->tile[0][3].reserved, cr->tile[1][1].reserved,
          cr->tile[2][3].reserved, cr->tile[3][1].reserved,
          cr->tile[4][3].reserved, cr->tile[0][3].reserved,
          cr->tile[1][3].reserved, cr->tile[2][3].reserved,
          cr->tile[3][3].reserved, cr->tile[4][3].reserved,
          cr->tile[1][4].reserved, cr->tile[2][4].reserved,
          cr->tile[3][4].reserved);
#define M(a,b) cr->tile[a][b].food, cr->tile[a][b].shield, cr->tile[a][b].trade
  freelog(LOG_NORMAL, "Tiles (food/shield/trade):\n"
          "      %d-%d-%d %d-%d-%d %d-%d-%d\n"
          "%d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d\n"
          "%d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d\n"
          "%d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d %d-%d-%d\n"
          "      %d-%d-%d %d-%d-%d %d-%d-%d",
          M(1,0), M(2,0), M(3,0), M(0,1), M(1,1), M(2,1), M(3,1), M(4,1),
          M(0,2), M(1,2), M(2,2), M(3,2), M(4,2), M(0,3), M(1,3), M(2,3),
          M(3,3), M(4,3), M(1,4), M(2,4), M(3,4));
#undef M
  freelog(LOG_NORMAL, "city center %d + best other(%d, %d) %d - corr %d "
          "- waste %d\n"
          "+ remaining %d + defense bonus %d + naval bonus %d = %d (%d)", 
          cr->city_center, cr->other_x, cr->other_y, cr->best_other,
          cr->corruption, cr->waste, cr->remaining, defense_bonus(cr, ai), 
          naval_bonus(cr, ai), cr->total, cr->result);
  if (food_starvation(cr)) {
    freelog(LOG_NORMAL, " ** FOOD STARVATION **");
  }
  if (shield_starvation(cr)) {
    freelog(LOG_NORMAL, " ** RESOURCE STARVATION **");
  }
}

/**************************************************************************
  Calculates the desire for founding a new city at (x, y). The citymap 
  ensures that we do not build cities too close to each other. If we 
  return result->total == 0, then no place was found.
**************************************************************************/
static void city_desirability(struct player *pplayer, struct ai_data *ai,
                              struct unit *punit, int x, int y,
                              struct cityresult *result)
{  
  struct city *pcity = map_get_city(x, y);

  assert(punit && ai && pplayer && result);

  result->x = x;
  result->y = y;
  result->total = 0;

  if (!city_can_be_built_here(x, y, punit)
      || (ai_handicap(pplayer, H_MAP)
          && !map_is_known(x, y, pplayer))) {
    return;
  }

  /* Check if another settler has taken a spot within mindist */
  square_iterate(x, y, game.rgame.min_dist_bw_cities-1, x1, y1) {
    if (citymap_is_reserved(x1, y1)) {
      return;
    }
  } square_iterate_end;

  if (enemies_at(punit, x, y)) {
    return;
  }

  if (pcity && pcity->size >= game.add_to_size_limit) {
    return;
  }

  if (!pcity && citymap_is_reserved(x, y)) {
    return; /* reserved, go away */
  }

  cityresult_fill(pplayer, ai, result); /* Burn CPU, burn! */
  if (result->total == 0) {
    /* Failed to find a good spot */
    return;
  }

  /* If (x, y) is an existing city, consider immigration */
  if (pcity && pcity->owner == pplayer->player_no) {
    return;
  }

  /*** Alright: Now consider building a new city ***/

  if (food_starvation(result) || shield_starvation(result)) {
    result->total = 0;
    return;
  }
  result->total += defense_bonus(result, ai);
  result->total += naval_bonus(result, ai);

  /* Add remaining points, which is our potential */
  result->total += result->remaining;

  assert(result->total >= 0);

  return;
}

/**************************************************************************
  Prime settler engine.
**************************************************************************/
void ai_settler_init(struct player *pplayer)
{
  memset(&cachemap, -1, sizeof(cachemap));
}

/**************************************************************************
  Find nearest and best city placement in a PF iteration according to 
  "parameter".  The value in "boat_cost" is both the penalty to pay for 
  using a boat and an indicator (boat_cost!=0) if a boat was used at all. 
  The result is returned in "best".

  Return value is TRUE if found something better than what was originally 
  in "best".

  TODO: Transparently check if we should add ourselves to an existing city.
**************************************************************************/
static bool settler_map_iterate(struct pf_parameter *parameter,
				struct unit *punit,
				struct cityresult *best,
				struct player *pplayer, 
				int boat_cost)
{
  struct cityresult result;
  int best_turn = 0; /* Which turn we found the best fit */
  struct ai_data *ai = ai_data_get(pplayer);
  struct pf_map *map;
  bool found = FALSE; /* The return value */

  map = pf_create_map(parameter);
  pf_iterator(map, pos) {
    int turns;
    struct tile *ptile = map_get_tile(pos.x, pos.y);

    if (is_ocean(ptile->terrain)) {
      continue; /* This can happen if there is a ferry near shore. */
    }
    if (boat_cost == 0
        && ptile->continent != map_get_tile(punit->x, punit->y)->continent) {
      /* We have an accidential land bridge. Ignore it. It will in all
       * likelihood go away next turn, or even in a few nanoseconds. */
      continue;
    }

    /* Calculate worth */
    city_desirability(pplayer, ai, punit, pos.x, pos.y, &result);

    /* Check if actually found something */
    if (result.total == 0) {
      continue;
    }

    /* This algorithm punishes long treks */
    turns = pos.total_MC / parameter->move_rate;
    result.result = amortize(result.total, PERFECTION * turns);

    /* Reduce want by settler cost. Easier than amortize, but still
     * weeds out very small wants. ie we create a threshold here. */
    /* We also penalise here for using a boat (either virtual or real)
     * it's crude but what isn't? */
    result.result -= unit_type(punit)->build_cost + boat_cost;

    /* Find best spot */
    if (result.result > best->result) {
      *best = result;
      found = TRUE;
      best_turn = turns;
    }

    /* Can we terminate early? We have a 'good enough' spot, and
     * we don't block the establishment of a better city just one
     * further step away. */
    if (best->result > RESULT_IS_ENOUGH
        && turns > parameter->move_rate /* sic -- yeah what an explanation! */
        && best_turn < turns /*+ game.rgame.min_dist_bw_cities*/) {
      break;
    }
  } pf_iterator_end;

  pf_destroy_map(map);

  return found;
}

/**************************************************************************
  Find nearest and best city placement or (TODO) a city to immigrate to.

  Option look_for_boat forces to find a boat before cosidering going 
  overseas.  Option use_virt_boat allows to use virtual boat but only
  if punit is in a coastal city right now (should only be used by 
  virtual units).  I guess it won't hurt to remove this condition, PF 
  will just give no positions.
**************************************************************************/
void find_best_city_placement(struct unit *punit, struct cityresult *best,
			      bool look_for_boat, bool use_virt_boat)
{
  struct player *pplayer = unit_owner(punit);
  struct pf_parameter parameter;
  struct unit *ferry = NULL;

  assert(pplayer->ai.control);

  best->x = -1;
  best->y = -1;
  best->result = 0;
  best->total = 0;
  best->overseas = FALSE;
  best->virt_boat = FALSE;

  /* Phase 1: Consider building cities on our continent */

  pft_fill_unit_parameter(&parameter, punit);
  (void) settler_map_iterate(&parameter, punit, best, pplayer, 0);

  if (best->result > RESULT_IS_ENOUGH) {
    return;
  }

  /* Phase 2: Consider travelling to another continent */

  if (look_for_boat) {
    int ferry_id = aiferry_find_boat(punit, 1, NULL);

    ferry = find_unit_by_id(ferry_id);
  }

  if (ferry 
      || (use_virt_boat && is_ocean_near_tile(punit->x, punit->y) 
          && map_get_city(punit->x, punit->y))) {
    if (!ferry) {
      /* No boat?  Get a virtual one! */
      Unit_Type_id boattype
        = best_role_unit_for_player(pplayer, L_FERRYBOAT);

      if (boattype == U_LAST) {
        /* Sea travel not possible yet. Bump tech want for ferries. */
        Unit_Type_id boattype = get_role_unit(L_FERRYBOAT, 0);
        Tech_Type_id tech_req = unit_types[boattype].tech_requirement;

        pplayer->ai.tech_want[tech_req] += FERRY_TECH_WANT;
        return;
      }
      ferry = create_unit_virtual(pplayer, NULL, boattype, 0);
      ferry->x = punit->x;
      ferry->y = punit->y;
    }

    pft_fill_unit_overlap_param(&parameter, ferry);
    parameter.get_TB = no_fights_or_unknown;

    /* FIXME: Maybe penalty for using an existing boat is too high?
     * We shouldn't make the penalty for building a new boat too high though.
     * Building a new boat is like a war against a weaker enemy -- 
     * good for the economy. (c) Bush family */
    if (settler_map_iterate(&parameter, punit, best, pplayer, 
			    unit_type(ferry)->build_cost)) {
      best->overseas = TRUE;
      best->virt_boat = (ferry->id == 0);
    }
  }
}
