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
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "mem.h"
#include "support.h"
#include "timing.h"

#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "unitlist.h"

#include "citytools.h"
#include "gotohand.h"
#include "maphand.h"
#include "plrhand.h"
#include "settlers.h"
#include "unithand.h"
#include "unittools.h"

#include "aicity.h"
#include "aidata.h"
#include "ailog.h"
#include "aisettler.h"
#include "aitools.h"
#include "aiunit.h"
#include "citymap.h"

/* This factor is multiplied on when calculating the want.  This is done
 * to avoid rounding errors in comparisons when looking for the best
 * possible work.  However before returning the final want we have to
 * divide by it again.  This loses accuracy but is needed since the want
 * values are used for comparison by the AI in trying to calculate the
 * goodness of building worker units. */
#define WORKER_FACTOR 1024

/* if an enemy unit gets within this many turns of a worker, the worker
 * flees */
#define WORKER_FEAR_FACTOR 2

struct settlermap {

  int enroute; /* unit ID of settler en route to this tile */
  int eta; /* estimated number of turns until enroute arrives */

};

/**************************************************************************
  Build a city and initialize AI infrastructure cache.
**************************************************************************/
static bool ai_do_build_city(struct player *pplayer, struct unit *punit)
{
  struct tile *ptile = punit->tile;
  struct city *pcity;

  assert(pplayer == unit_owner(punit));
  handle_unit_activity_request(punit, ACTIVITY_IDLE);

  /* Free city reservations */
  ai_unit_new_role(punit, AIUNIT_NONE, NULL);

  pcity = tile_get_city(ptile);
  if (pcity) {
    freelog(LOG_ERROR, "%s: There is already a city at (%d, %d)!", 
            pplayer->name, TILE_XY(ptile));
    return FALSE;
  }
  handle_unit_build_city(pplayer, punit->id,
			 city_name_suggestion(pplayer, ptile));
  pcity = tile_get_city(ptile);
  if (!pcity) {
    freelog(LOG_ERROR, "%s: Failed to build city at (%d, %d)", 
            pplayer->name, TILE_XY(ptile));
    return FALSE;
  }

  /* We have to rebuild at least the cache for this city.  This event is
   * rare enough we might as well build the whole thing.  Who knows what
   * else might be cached in the future? */
  assert(pplayer == city_owner(pcity));
  initialize_infrastructure_cache(pplayer);

  /* Init ai.choice. Handling ferryboats might use it. */
  init_choice(&pcity->ai.choice);

  return TRUE;
}

/**************************************************************************
  Amortize means gradually paying off a cost or debt over time. In freeciv
  terms this means we calculate how much less worth something is to us
  depending on how long it will take to complete.

  This is based on a global interest rate as defined by the MORT value.
**************************************************************************/
int amortize(int benefit, int delay)
{
  double discount = 1.0 - 1.0 / ((double)MORT);

  /* Note there's no rounding here.  We could round but it would probably
   * be better just to return (and take) a double for the benefit. */
  return benefit * pow(discount, delay);
}

/**************************************************************************
  Manages settlers.
**************************************************************************/
void ai_manage_settler(struct player *pplayer, struct unit *punit)
{
  punit->ai.control = TRUE;
  punit->ai.done = TRUE; /* we will manage this unit later... ugh */
  /* if BUILD_CITY must remain BUILD_CITY, otherwise turn into autosettler */
  if (punit->ai.ai_role == AIUNIT_NONE) {
    ai_unit_new_role(punit, AIUNIT_AUTO_SETTLER, NULL);
  }
  return;
}

/**************************************************************************
  Returns a measure of goodness of a tile to pcity.

  FIXME: foodneed and prodneed are always 0.
**************************************************************************/
int city_tile_value(struct city *pcity, int x, int y, 
		    int foodneed, int prodneed)
{
  int food = city_get_output_tile(x, y, pcity, O_FOOD);
  int shield = city_get_output_tile(x, y, pcity, O_SHIELD);
  int trade = city_get_output_tile(x, y, pcity, O_TRADE);
  int value = 0;

  /* Each food, trade, and shield gets a certain weighting.  We also benefit
   * tiles that have at least one of an item - this promotes balance and 
   * also accounts for INC_TILE effects. */
  value += food * FOOD_WEIGHTING;
  if (food > 0) {
    value += FOOD_WEIGHTING / 2;
  }
  value += shield * SHIELD_WEIGHTING;
  if (shield > 0) {
    value += SHIELD_WEIGHTING / 2;
  }
  value += trade * TRADE_WEIGHTING;
  if (trade > 0) {
    value += TRADE_WEIGHTING / 2;
  }

  return value;
}

/**************************************************************************
  Calculates the value of removing pollution at the given tile.

    (map_x, map_y) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.

  The return value is the goodness of the tile after the cleanup.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_pollution(struct city *pcity, int city_x, int city_y,
			     int best, struct tile *ptile)
{
  int goodness;

  if (!tile_has_special(ptile, S_POLLUTION)) {
    return -1;
  }
  tile_clear_special(ptile, S_POLLUTION);
  goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
  tile_set_special(ptile, S_POLLUTION);

  /* FIXME: need a better way to guarantee pollution is cleaned up. */
  goodness = (goodness + best + 50) * 2;

  return goodness;
}

/**************************************************************************
  Calculates the value of removing fallout at the given tile.

    (map_x, map_y) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.

  The return value is the goodness of the tile after the cleanup.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_fallout(struct city *pcity, struct player *pplayer,
			   int city_x, int city_y, int best,
			   struct tile *ptile)
{
  int goodness;

  if (!tile_has_special(ptile, S_FALLOUT)) {
    return -1;
  }
  tile_clear_special(ptile, S_FALLOUT);
  goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
  tile_set_special(ptile, S_FALLOUT);

  /* FIXME: need a better way to guarantee fallout is cleaned up. */
  if (!pplayer->ai.control) {
    goodness = (goodness + best + 50) * 2;
  }

  return goodness;
}

/**************************************************************************
  Returns TRUE if tile at (map_x,map_y) is useful as a source of
  irrigation.  This takes player vision into account, but allows the AI
  to cheat.

  This function should probably only be used by
  is_wet_or_is_wet_cardinal_around, below.
**************************************************************************/
static bool is_wet(struct player *pplayer, struct tile *ptile)
{
  /* FIXME: this should check a handicap. */
  if (!pplayer->ai.control && !map_is_known(ptile, pplayer)) {
    return FALSE;
  }

  if (is_ocean(ptile->terrain)) {
    /* TODO: perhaps salt water should not be usable for irrigation? */
    return TRUE;
  }

  if (tile_has_special(ptile, S_RIVER)
      || tile_has_special(ptile, S_IRRIGATION)) {
    return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Returns TRUE if there is an irrigation source adjacent to the given x, y
  position.  This takes player vision into account, but allows the AI to
  cheat. (See is_wet() for the definition of an irrigation source.)

  This function exactly mimics is_water_adjacent_to_tile, except that it
  checks vision.
**************************************************************************/
static bool is_wet_or_is_wet_cardinal_around(struct player *pplayer,
					     struct tile *ptile)
{
  if (is_wet(pplayer, ptile)) {
    return TRUE;
  }

  cardinal_adjc_iterate(ptile, tile1) {
    if (is_wet(pplayer, tile1)) {
      return TRUE;
    }
  } cardinal_adjc_iterate_end;

  return FALSE;
}

/**************************************************************************
  Calculate the benefit of irrigating the given tile.

    (map_x, map_y) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the irrigation.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_irrigate(struct city *pcity, struct player *pplayer,
			    int city_x, int city_y, struct tile *ptile)
{
  int goodness;
  struct terrain *old_terrain = ptile->terrain;
  bv_special old_special = ptile->special;
  struct terrain *new_terrain = old_terrain->irrigation_result;

  if (old_terrain != new_terrain && new_terrain != T_NONE) {
    /* Irrigation would change the terrain type, clearing the mine
     * in the process.  Calculate the benefit of doing so. */
    if (ptile->city && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
      return -1;
    }
    ptile->terrain = new_terrain;
    tile_clear_special(ptile, S_MINE);
    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
    ptile->terrain = old_terrain;
    ptile->special = old_special;
    return goodness;
  } else if (old_terrain == new_terrain
	     && !tile_has_special(ptile, S_IRRIGATION)
	     && is_wet_or_is_wet_cardinal_around(pplayer, ptile)) {
    /* The tile is currently unirrigated; irrigating it would put an
     * S_IRRIGATE on it replacing any S_MINE already there.  Calculate
     * the benefit of doing so. */
    tile_clear_special(ptile, S_MINE);
    tile_set_special(ptile, S_IRRIGATION);
    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
    ptile->special = old_special;
    assert(ptile->terrain == old_terrain);
    return goodness;
  } else if (old_terrain == new_terrain
	     && tile_has_special(ptile, S_IRRIGATION)
	     && !tile_has_special(ptile, S_FARMLAND)
	     && player_knows_techs_with_flag(pplayer, TF_FARMLAND)
	     && is_wet_or_is_wet_cardinal_around(pplayer, ptile)) {
    /* The tile is currently irrigated; irrigating it more puts an
     * S_FARMLAND on it.  Calculate the benefit of doing so. */
    assert(!tile_has_special(ptile, S_MINE));
    tile_set_special(ptile, S_FARMLAND);
    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
    tile_clear_special(ptile, S_FARMLAND);
    assert(ptile->terrain == old_terrain
	   && memcmp(&ptile->special, &old_special,
		     sizeof(old_special)) == 0);
    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Calculate the benefit of mining the given tile.

    (map_x, map_y) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the mining.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_mine(struct city *pcity,
			int city_x, int city_y, struct tile *ptile)
{
  int goodness;
  struct terrain *old_terrain = ptile->terrain;
  bv_special old_special = ptile->special;
  struct terrain *new_terrain = old_terrain->mining_result;

  if (old_terrain != new_terrain && new_terrain != T_NONE) {
    /* Mining would change the terrain type, clearing the irrigation
     * in the process.  Calculate the benefit of doing so. */
    if (ptile->city && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
      return -1;
    }
    ptile->terrain = new_terrain;
    tile_clear_special(ptile, S_IRRIGATION);
    tile_clear_special(ptile, S_FARMLAND);
    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
    ptile->terrain = old_terrain;
    ptile->special = old_special;
    return goodness;
  } else if (old_terrain == new_terrain
	     && !tile_has_special(ptile, S_MINE)) {
    /* The tile is currently unmined; mining it would put an S_MINE on it
     * replacing any S_IRRIGATION/S_FARMLAND already there.  Calculate
     * the benefit of doing so. */
    tile_clear_special(ptile, S_IRRIGATION);
    tile_clear_special(ptile, S_FARMLAND);
    tile_set_special(ptile, S_MINE);
    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);
    ptile->special = old_special;
    assert(ptile->terrain == old_terrain);
    return goodness;
  } else {
    return -1;
  }
  return goodness;
}

/**************************************************************************
  Calculate the benefit of transforming the given tile.

    (ptile) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the transform.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_transform(struct city *pcity,
			     int city_x, int city_y, struct tile *ptile)
{
  int goodness;
  struct terrain *old_terrain = ptile->terrain;
  bv_special old_special = ptile->special;
  struct terrain *new_terrain = old_terrain->transform_result;

  if (old_terrain == new_terrain || new_terrain == T_NONE) {
    return -1;
  }

  if (is_ocean(old_terrain) && !is_ocean(new_terrain)
      && !can_reclaim_ocean(ptile)) {
    /* Can't change ocean into land here. */
    return -1;
  }
  if (is_ocean(new_terrain) && !is_ocean(old_terrain)
      && !can_channel_land(ptile)) {
    /* Can't change land into ocean here. */
    return -1;
  }

  if (ptile->city && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
    return -1;
  }

  ptile->terrain = new_terrain;

  if (new_terrain->mining_result != new_terrain) {
    tile_clear_special(ptile, S_MINE);
  }
  if (new_terrain->irrigation_result != new_terrain) {
    tile_clear_special(ptile, S_FARMLAND);
    tile_clear_special(ptile, S_IRRIGATION);
  }
    
  goodness = city_tile_value(pcity, city_x, city_y, 0, 0);

  ptile->terrain = old_terrain;
  ptile->special = old_special;

  return goodness;
}

/**************************************************************************
  Calculate the attractiveness of building a road/rail at the given tile.

  This calculates the overall benefit of connecting the civilization; this
  is independent from the local tile (trade) bonus granted by the road.

  "special" must be either S_ROAD or S_RAILROAD.
**************************************************************************/
static int road_bonus(struct tile *ptile, enum tile_special_type special)
{
  int bonus = 0, i;
  bool has_road[12], is_slow[12];
  int dx[12] = {-1,  0,  1, -1, 1, -1, 0, 1,  0, -2, 2, 0};
  int dy[12] = {-1, -1, -1,  0, 0,  1, 1, 1, -2,  0, 0, 2};
  
  assert(special == S_ROAD || special == S_RAILROAD);

  for (i = 0; i < 12; i++) {
    struct tile *tile1 = map_pos_to_tile(ptile->x + dx[i], ptile->y + dy[i]);

    if (!tile1) {
      has_road[i] = FALSE;
      is_slow[i] = FALSE; /* FIXME: should be TRUE? */
    } else {
      struct terrain *pterrain = tile1->terrain;

      has_road[i] = tile_has_special(tile1, special);

      /* If TRUE, this value indicates that this tile does not need
       * a road connector.  This is set for terrains which cannot have
       * road or where road takes "too long" to build. */
      is_slow[i] = (pterrain->road_time == 0 || pterrain->road_time > 5);

      if (!has_road[i]) {
	unit_list_iterate(tile1->units, punit) {
	  if (punit->activity == ACTIVITY_ROAD 
              || punit->activity == ACTIVITY_RAILROAD) {
	    /* If a road is being built here, consider as if it's already
	     * built. */
	    has_road[i] = TRUE;
          }
	} unit_list_iterate_end;
      }
    }
  }

  /*
   * Consider the following tile arrangement (numbered in hex):
   *
   *   8
   *  012
   * 93 4A
   *  567
   *   B
   *
   * these are the tiles defined by the (dx,dy) arrays above.
   *
   * Then the following algorithm is supposed to determine if it's a good
   * idea to build a road here.  Note this won't work well for hex maps
   * since the (dx,dy) arrays will not cover the same tiles.
   *
   * FIXME: if you can understand the algorithm below please rewrite this
   * explanation!
   */
  if (has_road[0]
      && !has_road[1] && !has_road[3]
      && (!has_road[2] || !has_road[8])
      && (!is_slow[2] || !is_slow[4] || !is_slow[7]
	  || !is_slow[6] || !is_slow[5])) {
    bonus++;
  }
  if (has_road[2]
      && !has_road[1] && !has_road[4]
      && (!has_road[7] || !has_road[10])
      && (!is_slow[0] || !is_slow[3] || !is_slow[7]
	  || !is_slow[6] || !is_slow[5])) {
    bonus++;
  }
  if (has_road[5]
      && !has_road[6] && !has_road[3]
      && (!has_road[5] || !has_road[11])
      && (!is_slow[2] || !is_slow[4] || !is_slow[7]
	  || !is_slow[1] || !is_slow[0])) {
    bonus++;
  }
  if (has_road[7]
      && !has_road[6] && !has_road[4]
      && (!has_road[0] || !has_road[9])
      && (!is_slow[2] || !is_slow[3] || !is_slow[0]
	  || !is_slow[1] || !is_slow[5])) {
    bonus++;
  }

  /*   A
   *  B*B
   *  CCC
   *
   * We are at tile *.  If tile A has a road, and neither B tile does, and
   * one C tile is a valid destination, then we might want a road here.
   *
   * Of course the same logic applies if you rotate the diagram.
   */
  if (has_road[1] && !has_road[4] && !has_road[3]
      && (!is_slow[5] || !is_slow[6] || !is_slow[7])) {
    bonus++;
  }
  if (has_road[3] && !has_road[1] && !has_road[6]
      && (!is_slow[2] || !is_slow[4] || !is_slow[7])) {
    bonus++;
  }
  if (has_road[4] && !has_road[1] && !has_road[6]
      && (!is_slow[0] || !is_slow[3] || !is_slow[5])) {
    bonus++;
  }
  if (has_road[6] && !has_road[4] && !has_road[3]
      && (!is_slow[0] || !is_slow[1] || !is_slow[2])) {
    bonus++;
  }

  return bonus;
}

/**************************************************************************
  Calculate the benefit of building a road at the given tile.

    (map_x, map_y) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the road is built.
  This should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).

  This function does not calculate the benefit of being able to quickly
  move units (i.e., of connecting the civilization).  See road_bonus() for
  that calculation.
**************************************************************************/
static int ai_calc_road(struct city *pcity, struct player *pplayer,
			int city_x, int city_y, struct tile *ptile)
{
  int goodness;

  if (!is_ocean(ptile->terrain)
      && (!tile_has_special(ptile, S_RIVER)
	  || player_knows_techs_with_flag(pplayer, TF_BRIDGE))
      && !tile_has_special(ptile, S_ROAD)) {

    /* HACK: calling tile_set_special here will have side effects, so we
     * have to set it manually. */
    assert(!tile_has_special(ptile, S_ROAD));
    set_special(&ptile->special, S_ROAD);

    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);

    clear_special(&ptile->special, S_ROAD);

    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Calculate the benefit of building a railroad at the given tile.

    (ptile) is the map position of the tile.
    (city_x, city_y) is the city position of the tile with respect to pcity.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the railroad is built.
  This should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).

  This function does not calculate the benefit of being able to quickly
  move units (i.e., of connecting the civilization).  See road_bonus() for
  that calculation.
**************************************************************************/
static int ai_calc_railroad(struct city *pcity, struct player *pplayer,
			    int city_x, int city_y, struct tile *ptile)
{
  int goodness;
  bv_special old_special;

  if (!is_ocean(ptile->terrain)
      && player_knows_techs_with_flag(pplayer, TF_RAILROAD)
      && !tile_has_special(ptile, S_RAILROAD)) {
    old_special = ptile->special;

    /* HACK: calling tile_set_special here will have side effects, so we
     * have to set it manually. */
    set_special(&ptile->special, S_ROAD);
    set_special(&ptile->special, S_RAILROAD);

    goodness = city_tile_value(pcity, city_x, city_y, 0, 0);

    ptile->special = old_special;

    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Tries to find a boat for our unit. Requires warmap to be initialized
  with respect to x, y. cap is the requested capacity on the transport.
  Note that it may return a transport with less than cap capacity if this
  transport has zero move cost to x, y.

  The "virtual boats" code is not used. It is probably too unreliable, 
  since the AI switches its production back and forth continously.

  FIXME: if there is a (free) boat in a city filled with units, 
  ground_unit_transporter_capacity will return negative.
  TODO: Kill me.  There is a reliable version of this, find_ferry.
**************************************************************************/
Unit_type_id find_boat(struct player *pplayer, struct tile **ptile, int cap)
{
  int best = 22; /* arbitrary maximum distance, I will admit! */
  Unit_type_id id = 0;
  unit_list_iterate(pplayer->units, aunit)
    if (is_ground_units_transport(aunit)) {
      if (WARMAP_COST(aunit->tile) < best &&
	  (WARMAP_COST(aunit->tile) == 0 ||
	   ground_unit_transporter_capacity(aunit->tile,
					    pplayer) >= cap)) {
        id = aunit->id;
        best = WARMAP_COST(aunit->tile);
	*ptile = aunit->tile;
      }
    }
  unit_list_iterate_end;
  if (id != 0) return(id);
  return(id);
}

/**************************************************************************
  Returns TRUE if there are (other) ground units than punit stacked on
  punit's tile.
**************************************************************************/
struct unit *other_passengers(struct unit *punit)
{
  unit_list_iterate(punit->tile->units, aunit)
    if (is_ground_unit(aunit) && aunit != punit) return aunit;
  unit_list_iterate_end;
  return NULL;
}

/****************************************************************************
  Compares the best known tile improvement action with improving the tile
  at (x,y) with activity act.  Calculates the value of improving the tile
  by discounting the total value by the time it would take to do the work
  and multiplying by some factor.
****************************************************************************/
static void consider_settler_action(struct player *pplayer, 
                                    enum unit_activity act, int extra, 
                                    int new_tile_value, int old_tile_value,
				    bool in_use, int move_turns, int delay,
				    int *best_value,
				    int *best_old_tile_value,
				    int *best_move_turns,
				    enum unit_activity *best_act,
				    struct tile **best_tile,
                                    struct tile *ptile)
{
  bool consider;
  int total_value = 0, base_value = 0;
  
  if (extra >= 0) {
    consider = TRUE;
  } else {
    consider = (new_tile_value > old_tile_value);
    extra = 0;
  }

  /* find the present value of the future benefit of this action */
  if (consider) {

    base_value = new_tile_value - old_tile_value;
    total_value = base_value * WORKER_FACTOR;
    if (!in_use) {
      total_value /= 2;
    }
    total_value += extra * WORKER_FACTOR;

    /* use factor to prevent rounding errors */
    total_value = amortize(total_value, delay);
  } else {
    total_value = 0;
  }

  if (total_value > *best_value
      || (total_value == *best_value
	  && old_tile_value > *best_old_tile_value)) {
    freelog(LOG_DEBUG,
	    "Replacing (%d, %d) = %d with %s (%d, %d) = %d [d=%d b=%d]",
	    TILE_XY(*best_tile), *best_value, get_activity_text(act),
	    TILE_XY(ptile), total_value,
            delay, base_value);
    *best_value = total_value;
    *best_old_tile_value = old_tile_value;
    *best_act = act;
    *best_tile = ptile;
    *best_move_turns = move_turns;
  }
}

/**************************************************************************
  Returns how much food a settler will consume out of the city's foodbox
  when created. If unit has id zero it is assumed to be a virtual unit
  inside a city.

  FIXME: This function should be generalised and then moved into 
  common/unittype.c - Per
**************************************************************************/
static int unit_foodbox_cost(struct unit *punit)
{
  int cost = 30;

  if (punit->id == 0) {
    /* It is a virtual unit, so must start in a city... */
    struct city *pcity = tile_get_city(punit->tile);

    /* The default is to lose 100%.  The growth bonus reduces this. */
    int foodloss_pct = 100 - get_city_bonus(pcity, EFT_GROWTH_FOOD);

    foodloss_pct = CLIP(0, foodloss_pct, 100);
    assert(pcity != NULL);
    cost = city_granary_size(pcity->size);
    cost = cost * foodloss_pct / 100;
  }

  return cost;
}

/**************************************************************************
  Calculates a unit's food upkeep (per turn).
**************************************************************************/
static int unit_food_upkeep(struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  int upkeep = utype_upkeep_cost(unit_type(punit), pplayer,
				 get_gov_pplayer(pplayer), O_FOOD);
  if (punit->id != 0 && punit->homecity == 0)
    upkeep = 0; /* thanks, Peter */

  return upkeep;
}

/****************************************************************************
  Finds tiles to improve, using punit.

  The returned value is the goodness of the best tile and action found.  If
  this return value is >0, then (gx,gy) indicates the tile chosen and bestact
  indicates the activity it wants to do.  If 0 is returned then there are no
  worthwhile activities available.

  completion_time is the time that would be taken by punit to travel to
  and complete work at best_tile

  state contains, for each tile, the unit id of the worker en route,
  and the eta of this worker (if any).  This information
  is used to possibly displace this previously assigned worker.
  if this array is NULL, workers are never displaced.
****************************************************************************/
static int evaluate_improvements(struct unit *punit,
				 enum unit_activity *best_act,
				 struct tile **best_tile,
				 int *travel_time,
				 struct settlermap *state)
{
  struct city *mycity = tile_get_city(punit->tile);
  struct player *pplayer = unit_owner(punit);
  bool in_use;			/* true if the target square is being used
				   by one of our cities */
  Continent_id ucont     = tile_get_continent(punit->tile);
  int mv_rate         = unit_type(punit)->move_rate;
  int mv_turns;			/* estimated turns to move to target square */
  int oldv;			/* current value of consideration tile */
  int best_oldv = 9999;		/* oldv of best target so far; compared if
				   newv==best_newv; not initialized to zero,
				   so that newv=0 activities are not chosen */
  bool can_rr = player_knows_techs_with_flag(pplayer, TF_RAILROAD);

  int best_newv = 0;

  /* closest worker, if any, headed towards target tile */
  struct unit *enroute = NULL;

  generate_warmap(mycity, punit);
  
  city_list_iterate(pplayer->cities, pcity) {
    /* try to work near the city */
    city_map_checked_iterate(pcity->tile, cx, cy, ptile) {
      bool consider = TRUE;

      if (get_worker_city(pcity, cx, cy) == C_TILE_UNAVAILABLE
	  || terrain_has_flag(pcity->tile->terrain, TER_UNSAFE)) {
	/* Don't risk bothering with this tile. */
	continue;
      }

      /* do not go to tiles that already have workers there */
      unit_list_iterate(ptile->units, aunit) {
	if (aunit->owner == pplayer
	    && aunit->id != punit->id
	    && unit_flag(aunit, F_SETTLERS)) {
	  consider = FALSE;
	}
      } unit_list_iterate_end;

      in_use = (get_worker_city(pcity, cx, cy) == C_TILE_WORKER);
      if (state) {
	enroute = player_find_unit_by_id(pplayer,
					 state[ptile->index].enroute);
      }
      if (consider 
	  && tile_get_continent(ptile) == ucont
	  && WARMAP_COST(ptile) <= THRESHOLD * mv_rate) {
	int eta = FC_INFINITY, inbound_distance = FC_INFINITY, time;

	if (enroute) {
	  eta = state[ptile->index].eta;
	  inbound_distance = real_map_distance(ptile, enroute->tile);
	}
	mv_turns = WARMAP_COST(ptile) / mv_rate;
	oldv = city_tile_value(pcity, cx, cy, 0, 0);

	/* only consider this tile if we are closer in time and space to
	 * it than our other worker (if any) travelling to the site */
	if ((enroute && enroute->id == punit->id)
	    || mv_turns < eta
	    || (mv_turns == eta
		&& (real_map_distance(ptile, punit->tile)
		    < inbound_distance))) {

	  if (enroute) {
	    UNIT_LOG(LOG_DEBUG, punit,
		     "Considering %d,%d because we're closer "
		     "(%d,%d) than %d (%d,%d)",
		     TILE_XY(ptile), mv_turns,
		     real_map_distance(ptile, punit->tile),
		     enroute->id, eta, inbound_distance);
	  }

	  /* now, consider various activities... */
	  activity_type_iterate(act) {
	    if (pcity->ai.act_value[act][cx][cy] >= 0
		&& can_unit_do_activity_targeted_at(punit, act, 
						    S_LAST, ptile)) {
	      int extra = 0;
	      int base_value = pcity->ai.act_value[act][cx][cy];

	      time = mv_turns + get_turns_for_activity_at(punit, act, ptile);
	      
	      if (act == ACTIVITY_ROAD) {
		extra = road_bonus(ptile, S_ROAD) * 5;
		if (can_rr) {
		  /* if we can make railroads eventually, consider making
		   * road here, and set extras and time to to consider
		   * railroads in main consider_settler_action call */
		  consider_settler_action(pplayer, ACTIVITY_ROAD,
				extra,
				pcity->ai.act_value[ACTIVITY_ROAD][cx][cy], 
				oldv, in_use, mv_turns, time,
				&best_newv, &best_oldv, travel_time,
				best_act, best_tile,
				ptile);
		  
		  base_value
		    = pcity->ai.act_value[ACTIVITY_RAILROAD][cx][cy];
		  
		  /* Count road time plus rail time. */
		  time += get_turns_for_activity_at(punit, ACTIVITY_RAILROAD, 
						    ptile);
		}
	      } else if (act == ACTIVITY_RAILROAD) {
		extra = road_bonus(ptile, S_RAILROAD) * 3;
	      } else if (act == ACTIVITY_FALLOUT) {
		extra = pplayer->ai.frost;
	      } else if (act == ACTIVITY_POLLUTION) {
		extra = pplayer->ai.warmth;
	      }    
	      
	      consider_settler_action(pplayer, act,
				      extra, 
				      base_value, oldv, 
				      in_use,
				      mv_turns, time,
				      &best_newv, &best_oldv, travel_time,
				      best_act, best_tile,
				      ptile);
	      
	    } /* endif: can the worker perform this action */
	  } activity_type_iterate_end;
	} /* endif: can we finish sooner than current worker, if any? */
      } /* endif: are we travelling to a legal destination? */
    } city_map_checked_iterate_end;
  } city_list_iterate_end;

  best_newv /= WORKER_FACTOR;

  best_newv = MAX(best_newv, 0); /* sanity */

  if (best_newv > 0) {
    freelog(LOG_DEBUG,
	    "Settler %d@(%d,%d) wants to %s at (%d,%d) with desire %d",
	    punit->id, TILE_XY(punit->tile), get_activity_text(*best_act),
	    TILE_XY(*best_tile), best_newv);
  } else {
    /* Fill in dummy values.  The callers should check if the return value
     * is > 0 but this will avoid confusing them. */
    *best_act = ACTIVITY_IDLE;
    *best_tile = NULL;
  }

  return best_newv;
}

/**************************************************************************
  Find some work for our settlers and/or workers.
**************************************************************************/
#define LOG_SETTLER LOG_DEBUG
static void auto_settler_findwork(struct player *pplayer, 
				  struct unit *punit,
				  struct settlermap *state,
				  int recursion)
{
  struct cityresult result;
  int best_impr = 0;            /* best terrain improvement we can do */
  enum unit_activity best_act;
  struct tile *best_tile = NULL;
  struct ai_data *ai = ai_data_get(pplayer);

  /* time it will take worker to complete its given task */
  int completion_time = 0;

  if (recursion > unit_list_size(pplayer->units)) {
    assert(recursion <= unit_list_size(pplayer->units));
    ai_unit_new_role(punit, AIUNIT_NONE, NULL);
    set_unit_activity(punit, ACTIVITY_IDLE);
    send_unit_info(NULL, punit);
    return; /* avoid further recursion. */
  }

  CHECK_UNIT(punit);

  result.total = 0;
  result.result = 0;

  assert(pplayer && punit);
  assert(unit_flag(punit, F_CITIES) || unit_flag(punit, F_SETTLERS));

  /*** If we are on a city mission: Go where we should ***/

  if (punit->ai.ai_role == AIUNIT_BUILD_CITY) {
    struct tile *ptile = punit->goto_tile;
    int sanity = punit->id;

    /* Check that the mission is still possible.  If the tile has become
     * unavailable or the player has been autotoggled, call it off. */
    if (!unit_owner(punit)->ai.control
	|| !city_can_be_built_here(ptile, punit)) {
      UNIT_LOG(LOG_SETTLER, punit, "city founding mission failed");
      ai_unit_new_role(punit, AIUNIT_NONE, NULL);
      set_unit_activity(punit, ACTIVITY_IDLE);
      send_unit_info(NULL, punit);
      return; /* avoid recursion at all cost */
    } else {
      /* Go there */
      if ((!ai_gothere(pplayer, punit, ptile) && !find_unit_by_id(sanity))
          || punit->moves_left <= 0) {
        return;
      }
      if (same_pos(punit->tile, ptile)) {
        if (!ai_do_build_city(pplayer, punit)) {
          UNIT_LOG(LOG_ERROR, punit, "could not make city on %s",
                   tile_get_info_text(punit->tile));
          ai_unit_new_role(punit, AIUNIT_NONE, NULL);
          return; /* Avoid infinite recursion at all costs! */
        } else {
          return; /* We came, we saw, we built... */
        }
      } else {
        UNIT_LOG(LOG_SETTLER, punit, "could not go to target");
        /* ai_unit_new_role(punit, AIUNIT_NONE, NULL); */
        return;
      }
    }
  }

  CHECK_UNIT(punit);

  /*** Try find some work ***/

  if (unit_flag(punit, F_SETTLERS)) {
    TIMING_LOG(AIT_WORKERS, TIMER_START);
    best_impr = evaluate_improvements(punit, &best_act, &best_tile, 
				      &completion_time, state);
    TIMING_LOG(AIT_WORKERS, TIMER_STOP);
  }

  if (unit_flag(punit, F_CITIES) && pplayer->ai.control) {
    /* may use a boat: */
    TIMING_LOG(AIT_SETTLERS, TIMER_START);
    find_best_city_placement(punit, &result, TRUE, FALSE);
    UNIT_LOG(LOG_SETTLER, punit, "city want %d (impr want %d)", result.result,
             best_impr);
    TIMING_LOG(AIT_SETTLERS, TIMER_STOP);
    if (result.result > best_impr) {
      if (tile_get_city(result.tile)) {
        UNIT_LOG(LOG_SETTLER, punit, "immigrates to %s (%d, %d)", 
                 tile_get_city(result.tile)->name, TILE_XY(result.tile));
      } else {
        UNIT_LOG(LOG_SETTLER, punit, "makes city at (%d, %d)", 
                 TILE_XY(result.tile));
        if (punit->debug) {
          print_cityresult(pplayer, &result, ai);
        }
      }
      /* Go make a city! */
      ai_unit_new_role(punit, AIUNIT_BUILD_CITY, result.tile);
      if (result.other_tile) {
	/* Reserve best other tile (if there is one). */
	/* FIXME: what is an "other tile" and why would we want to reserve
	 * it? */
	citymap_reserve_tile(result.other_tile, punit->id);
      }
      punit->goto_tile = result.tile; /* TMP */
    } else if (best_impr > 0) {
      UNIT_LOG(LOG_SETTLER, punit, "improves terrain instead of founding");
      /* Terrain improvements follows the old model, and is recalculated
       * each turn. */
      ai_unit_new_role(punit, AIUNIT_AUTO_SETTLER, best_tile);
    } else {
      UNIT_LOG(LOG_SETTLER, punit, "cannot find work");
      ai_unit_new_role(punit, AIUNIT_NONE, NULL);
      return;
    }
  } else {
    /* We are a worker or engineer */
    ai_unit_new_role(punit, AIUNIT_AUTO_SETTLER, best_tile);
  }

  /* Run the "autosettler" program */
  if (punit->ai.ai_role == AIUNIT_AUTO_SETTLER) {
    /* Mark the square as taken. */
    if (best_tile) {
      struct unit *displaced
	= player_find_unit_by_id(pplayer, state[best_tile->index].enroute);

      if (displaced) {
	assert(state[best_tile->index].enroute == displaced->id);
	assert(state[best_tile->index].eta > completion_time
	       || (state[best_tile->index].eta == completion_time
		   && (real_map_distance(best_tile, punit->tile)
		       < real_map_distance(best_tile, displaced->tile))));
	UNIT_LOG(LOG_DEBUG, punit,
		 "%d (%d,%d) has displaced %d (%d,%d) on %d,%d",
		punit->id, completion_time,
		real_map_distance(best_tile, punit->tile),
		displaced->id, state[best_tile->index].eta,
		real_map_distance(best_tile, displaced->tile),
		TILE_XY(best_tile));
      }

      state[best_tile->index].enroute = punit->id;
      state[best_tile->index].eta = completion_time;
      
      if (displaced) {
	displaced->goto_tile = NULL;
	auto_settler_findwork(pplayer, displaced, state, recursion + 1);
      }
    } else {
      UNIT_LOG(LOG_DEBUG, punit, "giving up trying to improve terrain");
      return; /* We cannot do anything */
    }
    punit->goto_tile = best_tile; /* TMP */
    if (do_unit_goto(punit, GOTO_MOVE_ANY, FALSE) == GR_DIED) {
      return;
    }
    if (punit->moves_left > 0
        && same_pos(best_tile, punit->tile)) {
      handle_unit_activity_request(punit, best_act);
      send_unit_info(NULL, punit);
      return;
    }
  }

  /*** Recurse if we want to found a city ***/

  if (punit->ai.ai_role == AIUNIT_BUILD_CITY
      && punit->moves_left > 0) {
    auto_settler_findwork(pplayer, punit, state, recursion + 1);
  }
}
#undef LOG_SETTLER

/**************************************************************************
  Returns city_tile_value of the best tile worked by or available to pcity.
**************************************************************************/
static int best_worker_tile_value(struct city *pcity)
{
  int best = 0;

  city_map_iterate(x, y) {
    if (is_free_worked_tile(x, y) 
	|| get_worker_city(pcity, x, y) == C_TILE_WORKER 
	|| get_worker_city(pcity, x, y) == C_TILE_EMPTY) {
      int tmp = city_tile_value(pcity, x, y, 0, 0);

      if (tmp > best) {
	best = tmp;
      }
    }
  } city_map_iterate_end;
  return best;
}

/**************************************************************************
  Do all tile improvement calculations and cache them for later.

  These values are used in evaluate_improvements() so this function must
  be called before doing that.  Currently this is only done when handling
  auto-settlers or when the AI contemplates building worker units.
**************************************************************************/
void initialize_infrastructure_cache(struct player *pplayer)
{
  city_list_iterate(pplayer->cities, pcity) {
    int best = best_worker_tile_value(pcity);

    city_map_iterate(city_x, city_y) {
      activity_type_iterate(act) {
	pcity->ai.act_value[act][city_x][city_y] = -1;
      } activity_type_iterate_end;
    } city_map_iterate_end;

    city_map_checked_iterate(pcity->tile,
			     city_x, city_y, ptile) {
#ifndef NDEBUG
      struct terrain *old_terrain = ptile->terrain;
      bv_special old_special = ptile->special;
#endif

      pcity->ai.act_value[ACTIVITY_POLLUTION][city_x][city_y] 
	= ai_calc_pollution(pcity, city_x, city_y, best, ptile);
      pcity->ai.act_value[ACTIVITY_FALLOUT][city_x][city_y]
	= ai_calc_fallout(pcity, pplayer, city_x, city_y, best, ptile);
      pcity->ai.act_value[ACTIVITY_MINE][city_x][city_y]
	= ai_calc_mine(pcity, city_x, city_y, ptile);
      pcity->ai.act_value[ACTIVITY_IRRIGATE][city_x][city_y]
        = ai_calc_irrigate(pcity, pplayer, city_x, city_y, ptile);
      pcity->ai.act_value[ACTIVITY_TRANSFORM][city_x][city_y]
	= ai_calc_transform(pcity, city_x, city_y, ptile);

      /* road_bonus() is handled dynamically later; it takes into
       * account settlers that have already been assigned to building
       * roads this turn. */
      pcity->ai.act_value[ACTIVITY_ROAD][city_x][city_y]
	= ai_calc_road(pcity, pplayer, city_x, city_y, ptile);
      pcity->ai.act_value[ACTIVITY_RAILROAD][city_x][city_y]
	= ai_calc_railroad(pcity, pplayer, city_x, city_y, ptile);

      /* Make sure nothing was accidentally changed by these calculations. */
      assert(old_terrain == ptile->terrain
	     && memcmp(&ptile->special, &old_special,
		       sizeof(old_special)) == 0);
    } city_map_checked_iterate_end;
  } city_list_iterate_end;
}

/************************************************************************** 
  Run through all the players settlers and let those on ai.control work 
  automagically.
**************************************************************************/
void auto_settlers_player(struct player *pplayer) 
{
  static struct timer *t = NULL;      /* alloc once, never free */
  struct settlermap state[MAP_INDEX_SIZE];
  
  t = renew_timer_start(t, TIMER_CPU, TIMER_DEBUG);

  if (pplayer->ai.control) {
    /* Set up our city map. */
    citymap_turn_init(pplayer);
  }

  whole_map_iterate(ptile) {
    state[ptile->index].enroute = -1;
    state[ptile->index].eta = FC_INFINITY;    
  } whole_map_iterate_end;

  /* Initialize the infrastructure cache, which is used shortly. */
  initialize_infrastructure_cache(pplayer);

  /* An extra consideration for the benefit of cleaning up pollution/fallout.
   * This depends heavily on the calculations in update_environmental_upset.
   * Aside from that it's more or less a WAG that simply grows incredibly
   * large as an environmental disaster approaches. */
  pplayer->ai.warmth
    = (WARMING_FACTOR * game.info.heating / ((game.info.warminglevel + 1) / 2)
       + game.info.globalwarming);
  pplayer->ai.frost
    = (COOLING_FACTOR * game.info.cooling / ((game.info.coolinglevel + 1) / 2)
       + game.info.nuclearwinter);

  freelog(LOG_DEBUG, "Warmth = %d, game.globalwarming=%d",
	  pplayer->ai.warmth, game.info.globalwarming);
  freelog(LOG_DEBUG, "Frost = %d, game.nuclearwinter=%d",
	  pplayer->ai.warmth, game.info.nuclearwinter);

  /* Auto-settle with a settler unit if it's under AI control (e.g. human
   * player auto-settler mode) or if the player is an AI.  But don't
   * auto-settle with a unit under orders even for an AI player - these come
   * from the human player and take precedence. */
  unit_list_iterate(pplayer->units, punit) {
    if ((punit->ai.control || pplayer->ai.control)
	&& (unit_flag(punit, F_SETTLERS)
	    || unit_flag(punit, F_CITIES))
	&& !unit_has_orders(punit)
        && punit->moves_left > 0) {
      freelog(LOG_DEBUG, "%s's settler at (%d, %d) is ai controlled.",
	      pplayer->name, TILE_XY(punit->tile)); 
      if (punit->activity == ACTIVITY_SENTRY) {
	handle_unit_activity_request(punit, ACTIVITY_IDLE);
      }
      if (punit->activity == ACTIVITY_GOTO && punit->moves_left > 0) {
        handle_unit_activity_request(punit, ACTIVITY_IDLE);
      }
      if (punit->activity == ACTIVITY_IDLE) {
        auto_settler_findwork(pplayer, punit, state, 0);
      }
    }
  } unit_list_iterate_end;

  if (timer_in_use(t)) {
    freelog(LOG_VERBOSE, "%s's autosettlers consumed %g milliseconds.",
 	    pplayer->name, 1000.0*read_timer_seconds(t));
  }
}

/**************************************************************************
  Return want for city settler. Note that we rely here on the fact that
  ai_settler_init() has been run while doing autosettlers.
**************************************************************************/
void contemplate_new_city(struct city *pcity)
{
  struct player *pplayer = city_owner(pcity);
  struct unit *virtualunit;
  struct unit_type *unit_type = best_role_unit(pcity, F_CITIES); 

  if (unit_type == NULL) {
    freelog(LOG_DEBUG, "No F_CITIES role unit available");
    return;
  }

  /* Create a localized "virtual" unit to do operations with. */
  virtualunit = create_unit_virtual(pplayer, pcity, unit_type, 0);
  virtualunit->tile = pcity->tile;

  assert(pplayer->ai.control);

  if (pplayer->ai.control) {
    struct cityresult result;
    bool is_coastal = is_ocean_near_tile(pcity->tile);

    find_best_city_placement(virtualunit, &result, is_coastal, is_coastal);
    assert(0 <= result.result);

    CITY_LOG(LOG_DEBUG, pcity, "want(%d) to establish city at"
	     " (%d, %d) and will %s to get there", result.result, 
	     TILE_XY(result.tile), 
	     (result.virt_boat ? "build a boat" : 
	      (result.overseas ? "use a boat" : "walk")));

    pcity->ai.founder_want = (result.virt_boat ? 
			      -result.result : result.result);
    pcity->ai.founder_boat = result.overseas;
  }
  free(virtualunit);
}

/**************************************************************************
  Estimates the want for a terrain improver (aka worker) by creating a 
  virtual unit and feeding it to evaluate_improvements.

  TODO: AI does not ship F_SETTLERS around, only F_CITIES - Per
**************************************************************************/
void contemplate_terrain_improvements(struct city *pcity)
{
  struct player *pplayer = city_owner(pcity);
  struct unit *virtualunit;
  int want;
  struct tile *best_tile = NULL; /* May be accessed by freelog() calls. */
  enum unit_activity best_act;
  struct tile *ptile = pcity->tile;
  struct ai_data *ai = ai_data_get(pplayer);
  struct unit_type *unit_type = best_role_unit(pcity, F_SETTLERS);
  int completion_time;

  if (unit_type == NULL) {
    freelog(LOG_DEBUG, "No F_SETTLERS role unit available");
    return;
  }

  /* Create a localized "virtual" unit to do operations with. */
  virtualunit = create_unit_virtual(pplayer, pcity, unit_type, 0);
  virtualunit->tile = pcity->tile;
  want = evaluate_improvements(virtualunit, &best_act,
			       &best_tile, &completion_time,
			       NULL);
  want = (want - unit_food_upkeep(virtualunit) * FOOD_WEIGHTING) * 100
         / (40 + unit_foodbox_cost(virtualunit));
  free(virtualunit);

  /* Massage our desire based on available statistics to prevent
   * overflooding with worker type units if they come cheap in
   * the ruleset */
  want /= MAX(1, ai->stats.workers[ptile->continent]
                 / (ai->stats.cities[ptile->continent] + 1));
  want -= ai->stats.workers[ptile->continent];
  want = MAX(want, 0);

  CITY_LOG(LOG_DEBUG, pcity, "wants %s with want %d to do %s at (%d,%d), "
           "we have %d workers and %d cities on the continent",
	   unit_name(unit_type), want, get_activity_text(best_act),
	   TILE_XY(best_tile),
           ai->stats.workers[ptile->continent], 
           ai->stats.cities[ptile->continent]);
  assert(want >= 0);
  pcity->ai.settler_want = want;
}
