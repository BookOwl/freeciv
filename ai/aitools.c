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

#include <stdio.h>
#include <stdlib.h>

#include "city.h"
#include "combat.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "shared.h"
#include "unit.h"

#include "path_finding.h"
#include "pf_tools.h"

#include "airgoto.h"
#include "barbarian.h"
#include "citytools.h"
#include "cityturn.h"
#include "gotohand.h"
#include "maphand.h"
#include "plrhand.h"
#include "settlers.h"
#include "unithand.h"
#include "unittools.h"

#include "aicity.h"
#include "aidata.h"
#include "ailog.h"
#include "aiunit.h"

#include "aitools.h"

/**************************************************************************
  Amortize a want modified by the shields (build_cost) we risk losing.
  We add the build time of the unit(s) we risk to amortize delay.  The
  build time is claculated as the build cost divided by the production
  output of the unit's homecity or the city where we want to produce
  the unit. If the city has less than average shield output, we
  instead use the average, to encourage long-term thinking.
**************************************************************************/
int military_amortize(struct player *pplayer, struct city *pcity,
                      int value, int delay, int build_cost)
{
  struct ai_data *ai = ai_data_get(pplayer);
  int city_output = (pcity ? pcity->shield_surplus : 1);
  int output = MAX(city_output, ai->stats.average_production);
  int build_time = build_cost / MAX(output, 1);

  if (value <= 0) {
    return 0;
  }

  return amortize(value, delay + build_time);
}

/**********************************************************************
  There are some signs that a player might be dangerous: We are at 
  war with him, he has lousy reputation, he has done lots of ignoble 
  things to us, he is an ally of one of our enemies (a ticking bomb
  to be sure), or he is our war target.
***********************************************************************/
bool is_player_dangerous(struct player *pplayer, struct player *aplayer)
{
  struct ai_data *ai = ai_data_get(pplayer);
  struct ai_dip_intel *adip 
    = &ai->diplomacy.player_intel[aplayer->player_no];

  return (pplayers_at_war(pplayer, aplayer)
          || ai->diplomacy.target == aplayer
          || pplayer->diplstates[aplayer->player_no].has_reason_to_cancel
          || ai->diplomacy.acceptable_reputation > aplayer->reputation
          || adip->is_allied_with_enemy);
}

/*************************************************************************
  This is a function to execute paths returned by the path-finding engine.
  It is analogous to goto_route_execute, only much simpler.

  We use ai_unit_attack which means "move if target unoccupied, attack
  otherwise" and also brings our bodyguard along.
*************************************************************************/
bool ai_unit_execute_path(struct unit *punit, struct pf_path *path)
{
  int i;

  /* We start with i = 1 for i = 0 is our present position */
  for (i = 1; i < path->length; i++) {
    int x = path->positions[i].x, y = path->positions[i].y;
    bool result;
    int id = punit->id;

    /* We use ai_unit_move() for everything but the last step
     * of the way so that we abort if unexpected opposition
     * shows up. Any enemy on the target tile is expected to
     * be our target and any attack there intentional. */
    if (i == path->length - 1) {
      result = ai_unit_attack(punit, x, y);
    } else {
      ai_unit_move(punit, x, y);
      result = (punit = find_unit_by_id(id));
    }
    if (!result) {
      /* Died... */
      return FALSE;
    }

    if (!same_pos(punit->x, punit->y, x, y)) {
      /* Stopped (or maybe fought) */
      return TRUE;
    }
  }

  return TRUE;
}

/**************************************************************************
  This will eventually become the ferry-enabled goto. For now, it just
  wraps ai_unit_goto()
**************************************************************************/
bool ai_unit_gothere(struct unit *punit)
{
  CHECK_UNIT(punit);
  if (ai_unit_goto(punit, goto_dest_x(punit), goto_dest_y(punit))) {
    return TRUE; /* ... and survived */
  } else {
    return FALSE; /* we died */
  }
}

/**************************************************************************
  Go to specified destination but do not disturb existing role or activity
  and do not clear the role's destination. Return FALSE iff we died.

  FIXME: add some logging functionality to replace GOTO_LOG()
**************************************************************************/
bool ai_unit_goto(struct unit *punit, int x, int y)
{
  enum goto_result result;
  int oldx = -1, oldy = -1;
  enum unit_activity activity = punit->activity;
  bool is_set = is_goto_dest_set(punit);

  if (is_set) {
    oldx = goto_dest_x(punit);
    oldy = goto_dest_y(punit);
  }

  CHECK_UNIT(punit);
  /* TODO: log error on same_pos with punit->x|y */
  set_goto_dest(punit, x, y);
  handle_unit_activity_request(punit, ACTIVITY_GOTO);
  result = do_unit_goto(punit, GOTO_MOVE_ANY, FALSE);
  if (result != GR_DIED) {
    handle_unit_activity_request(punit, activity);
    if (is_set) {
      set_goto_dest(punit, oldx, oldy);
    } else {
      clear_goto_dest(punit);
    }
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
  Ensure unit sanity by telling charge that we won't bodyguard it anymore,
  tell bodyguard it can roam free if our job is done, add and remove city 
  spot reservation, and set destination.
**************************************************************************/
void ai_unit_new_role(struct unit *punit, enum ai_unit_task task, int x, int y)
{
  struct unit *charge = find_unit_by_id(punit->ai.charge);
  struct unit *bodyguard = find_unit_by_id(punit->ai.bodyguard);

  /* Free our ferry */
  if (task == AIUNIT_NONE || task == AIUNIT_DEFEND_HOME) {
    struct unit *ferryboat = find_unit_by_id(punit->ai.ferryboat);

    punit->ai.ferryboat = 0;
    if (ferryboat && ferryboat->ai.passenger == punit->id) {
      /* Do not free somebody else's ferry... */
      ferryboat->ai.passenger = 0;
    }
  }

  if (punit->activity == ACTIVITY_GOTO) {
    /* It would indicate we're going somewhere otherwise */
    handle_unit_activity_request(punit, ACTIVITY_IDLE);
  }

  if (punit->ai.ai_role == AIUNIT_BUILD_CITY) {
    remove_city_from_minimap(goto_dest_x(punit), goto_dest_y(punit));
  }

  if (charge && (charge->ai.bodyguard == punit->id)) {
    /* ensure we don't let the unit believe we bodyguard it */
    charge->ai.bodyguard = BODYGUARD_NONE;
  }
  punit->ai.charge = BODYGUARD_NONE;

  punit->ai.ai_role = task;

  /* Verify and set the goto destination.  Eventually this can be a lot more
   * stringent, but for now we don't want to break things too badly. */
  if (x == -1 && y == -1) {
    /* No goto_dest. */
    clear_goto_dest(punit);
  } else {
    set_goto_dest(punit, x, y);
  }

  if (punit->ai.ai_role == AIUNIT_NONE && bodyguard) {
    ai_unit_new_role(bodyguard, AIUNIT_NONE, -1, -1);
  }

  if (punit->ai.ai_role == AIUNIT_BUILD_CITY) {
    assert(is_normal_map_pos(x, y));
    add_city_to_minimap(x, y);
  }
}

/**************************************************************************
  Try to make pcity our new homecity. Fails if we can't upkeep it. Assumes
  success from server.
**************************************************************************/
bool ai_unit_make_homecity(struct unit *punit, struct city *pcity)
{
  CHECK_UNIT(punit);
  if (punit->homecity == 0 && !unit_has_role(punit->type, L_EXPLORER)) {
    /* This unit doesn't pay any upkeep while it doesn't have a homecity,
     * so it would be stupid to give it one. There can also be good reasons
     * why it doesn't have a homecity. */
    /* However, until we can do something more useful with them, we
       will assign explorers to a city so that they can be disbanded for 
       the greater good -- Per */
    return FALSE;
  }
  if (pcity->shield_surplus - unit_type(punit)->shield_cost >= 0
      && pcity->food_surplus - unit_type(punit)->food_cost >= 0) {
    struct packet_unit_request packet;
    packet.unit_id = punit->id;
    packet.city_id = pcity->id;
    handle_unit_change_homecity(unit_owner(punit), &packet);
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
  Move a bodyguard along with another unit. We assume that unit has already
  been moved to (x, y) which is a valid, safe coordinate, and that our
  bodyguard has not. This is an ai_unit_* auxiliary function, do not use 
  elsewhere.
**************************************************************************/
static void ai_unit_bodyguard_move(int unitid, int x, int y)
{
  struct unit *bodyguard = find_unit_by_id(unitid);
  struct unit *punit;
  struct player *pplayer;

  assert(bodyguard != NULL);
  pplayer = unit_owner(bodyguard);
  assert(pplayer != NULL);
  punit = find_unit_by_id(bodyguard->ai.charge);
  assert(punit != NULL);

  assert(punit->ai.bodyguard == bodyguard->id);
  assert(bodyguard->ai.charge == punit->id);

  if (!is_tiles_adjacent(x, y, bodyguard->x, bodyguard->y)) {
    return;
  }

  if (bodyguard->moves_left <= 0) {
    /* should generally should not happen */
    BODYGUARD_LOG(LOG_DEBUG, bodyguard, "was left behind by charge");
    return;
  }

  handle_unit_activity_request(bodyguard, ACTIVITY_IDLE);
  (void) ai_unit_move(bodyguard, x, y);
}

/**************************************************************************
  Check if we have a bodyguard with sanity checking and error recovery.
  Repair incompletely referenced bodyguards. When the rest of the bodyguard
  mess is cleaned up, this repairing should be replaced with an assert.
**************************************************************************/
static bool has_bodyguard(struct unit *punit)
{
  struct unit *guard;
  if (punit->ai.bodyguard > BODYGUARD_NONE) {
    if ((guard = find_unit_by_id(punit->ai.bodyguard))) {
      if (guard->ai.charge != punit->id) {
        BODYGUARD_LOG(LOG_VERBOSE, guard, "my charge didn't know about me!");
      }
      guard->ai.charge = punit->id; /* ensure sanity */
      return TRUE;
    } else {
      punit->ai.bodyguard = BODYGUARD_NONE;
      UNIT_LOG(LOGLEVEL_BODYGUARD, punit, "bodyguard disappeared!");
    }
  }
  return FALSE;
}

/**************************************************************************
  Move and attack with an ai unit. We do not wait for server reply.
**************************************************************************/
bool ai_unit_attack(struct unit *punit, int x, int y)
{
  struct packet_move_unit pmove;
  int sanity = punit->id;
  bool alive;

  CHECK_UNIT(punit);
  assert(unit_owner(punit)->ai.control);
  assert(is_normal_map_pos(x, y));
  assert(is_tiles_adjacent(punit->x, punit->y, x, y));

  pmove.x = x;
  pmove.y = y;
  pmove.unid = punit->id;
  handle_unit_activity_request(punit, ACTIVITY_IDLE);
  handle_move_unit(unit_owner(punit), &pmove);
  alive = (find_unit_by_id(sanity) != NULL);

  if (alive && same_pos(x, y, punit->x, punit->y)
      && has_bodyguard(punit)) {
    ai_unit_bodyguard_move(punit->ai.bodyguard, x, y);
    /* Clumsy bodyguard might trigger an auto-attack */
    alive = (find_unit_by_id(sanity) != NULL);
  }

  return alive;
}

/**************************************************************************
  Move an ai unit. Do not attack. Do not leave bodyguard.

  This function returns only when we have a reply from the server and
  we can tell the calling function what happened to the move request.
  (Right now it is not a big problem, since we call the server directly.)
**************************************************************************/
bool ai_unit_move(struct unit *punit, int x, int y)
{
  struct packet_move_unit pmove;
  struct unit *bodyguard;
  int sanity = punit->id;
  struct player *pplayer = unit_owner(punit);
  struct tile *ptile = map_get_tile(x,y);

  CHECK_UNIT(punit);
  assert(unit_owner(punit)->ai.control);
  assert(is_normal_map_pos(x, y));
  assert(is_tiles_adjacent(punit->x, punit->y, x, y));

  /* if enemy, stop and let ai attack function take this case */
  if (is_enemy_unit_tile(ptile, pplayer)
      || is_enemy_city_tile(ptile, pplayer)) {
    return FALSE;
  }

  /* barbarians shouldn't enter huts */
  if (is_barbarian(pplayer) && tile_has_special(ptile, S_HUT)) {
    return FALSE;
  }

  /* don't leave bodyguard behind */
  if (has_bodyguard(punit)
      && (bodyguard = find_unit_by_id(punit->ai.bodyguard))
      && same_pos(punit->x, punit->y, bodyguard->x, bodyguard->y)
      && bodyguard->moves_left == 0) {
    UNIT_LOG(LOGLEVEL_BODYGUARD, punit, "does not want to leave "
             "its bodyguard");
    return FALSE;
  }

  /* Try not to end move next to an enemy if we can avoid it by waiting */
  if (punit->moves_left <= map_move_cost(punit, x, y)
      && unit_type(punit)->move_rate > map_move_cost(punit, x, y)
      && enemies_at(punit, x, y)
      && !enemies_at(punit, punit->x, punit->y)) {
    UNIT_LOG(LOG_DEBUG, punit, "ending move early to stay out of trouble");
    return FALSE;
  }

  /* go */
  pmove.x = x;
  pmove.y = y;
  pmove.unid = punit->id;
  handle_unit_activity_request(punit, ACTIVITY_IDLE);
  handle_move_unit(unit_owner(punit), &pmove);

  /* handle the results */
  if (find_unit_by_id(sanity) && same_pos(x, y, punit->x, punit->y)) {
    if (has_bodyguard(punit)) {
      ai_unit_bodyguard_move(punit->ai.bodyguard, x, y);
    }
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
This looks for the nearest city:
If (x,y) is the land, it looks for cities only on the same continent
unless (everywhere != 0)
If (enemy != 0) it looks only for enemy cities
If (pplayer != NULL) it looks for cities known to pplayer
**************************************************************************/
struct city *dist_nearest_city(struct player *pplayer, int x, int y,
                               bool everywhere, bool enemy)
{ 
  struct city *pc=NULL;
  int best_dist = -1;
  int con = map_get_continent(x, y);

  players_iterate(pplay) {
    /* If "enemy" is set, only consider cities whose owner we're at
     * war with. */
    if (enemy && pplayer && !pplayers_at_war(pplayer, pplay)) {
      continue;
    }

    city_list_iterate(pplay->cities, pcity) {
      int city_dist = real_map_distance(x, y, pcity->x, pcity->y);

      /* Find the closest city known to the player with a matching
       * continent. */
      if ((best_dist == -1 || city_dist < best_dist)
	  && (everywhere || con == 0
	      || con == map_get_continent(pcity->x, pcity->y))
	  && (!pplayer || map_is_known(pcity->x, pcity->y, pplayer))) {
	best_dist = city_dist;
        pc = pcity;
      }
    } city_list_iterate_end;
  } players_iterate_end;

  return(pc);
}


/**************************************************************************
  Calculate the value of the target unit including the other units which
  will die in a successful attack
**************************************************************************/
int stack_cost(struct unit *pdef)
{
  int victim_cost = 0;

  if (is_stack_vulnerable(pdef->x, pdef->y)) {
    /* lotsa people die */
    unit_list_iterate(map_get_tile(pdef->x, pdef->y)->units, aunit) {
      victim_cost += unit_type(aunit)->build_cost;
    } unit_list_iterate_end;
  } else {
    /* Only one unit dies if attack is successful */
    victim_cost = unit_type(pdef)->build_cost;
  }
  
  return victim_cost;
}

/**************************************************************************
  Change government, pretty fast...
**************************************************************************/
void ai_government_change(struct player *pplayer, int gov)
{
  struct packet_player_request preq;

  if (gov == pplayer->government) {
    return;
  }
  preq.government = gov;
  pplayer->revolution = 0;
  pplayer->government = game.government_when_anarchy;
  handle_player_government(pplayer, &preq);
  pplayer->revolution = -1; /* yes, I really mean this. -- Syela */
}

/**************************************************************************
  Credits the AI wants to have in reserves. We need some gold to bribe
  and incite cities.

  "I still don't trust this function" -- Syela
**************************************************************************/
int ai_gold_reserve(struct player *pplayer)
{
  int i = total_player_citizens(pplayer)*2;
  return MAX(pplayer->ai.maxbuycost, i);
}

/**************************************************************************
  Sets the values of the choice to initial values.
**************************************************************************/
void init_choice(struct ai_choice *choice)
{
  choice->choice = A_UNSET;
  choice->want = 0;
  choice->type = CT_NONE;
}

/**************************************************************************
...
**************************************************************************/
void adjust_choice(int value, struct ai_choice *choice)
{
  choice->want = (choice->want *value)/100;
}

/**************************************************************************
...
**************************************************************************/
void copy_if_better_choice(struct ai_choice *cur, struct ai_choice *best)
{
  if (cur->want > best->want) {
    freelog(LOG_DEBUG, "Overriding choice (%s, %d) with (%s, %d)",
	    (best->type == CT_BUILDING ? 
	     get_improvement_name(best->choice) : unit_types[best->choice].name), 
	    best->want, 
	    (cur->type == CT_BUILDING ? 
	     get_improvement_name(cur->choice) : unit_types[cur->choice].name), 
	    cur->want);
    best->choice =cur->choice;
    best->want = cur->want;
    best->type = cur->type;
  }
}

/**************************************************************************
...
**************************************************************************/
void ai_advisor_choose_building(struct city *pcity, struct ai_choice *choice)
{ /* I prefer the ai_choice as a return value; gcc prefers it as an arg -- Syela */
  Impr_Type_id id = B_LAST;
  int danger = 0, downtown = 0, cities = 0;
  int want=0;
  struct player *plr;
        
  plr = city_owner(pcity);
     
  /* too bad plr->score isn't kept up to date. */
  city_list_iterate(plr->cities, acity)
    danger += acity->ai.danger;
    downtown += acity->ai.downtown;
    cities++;
  city_list_iterate_end;

  impr_type_iterate(i) {
    if (!is_wonder(i) ||
       (!pcity->is_building_unit && is_wonder(pcity->currently_building) &&
       pcity->shield_stock >= improvement_value(i) / 2) ||
       (!is_building_other_wonder(pcity) &&
        pcity->ai.grave_danger == 0 && /* otherwise caravans will be killed! */
        pcity->ai.downtown * cities >= downtown &&
        pcity->ai.danger * cities <= danger)) { /* too many restrictions? */
/* trying to keep wonders in safe places with easy caravan access -- Syela */
      if(pcity->ai.building_want[i]>want) {
/* we have to do the can_build check to avoid Built Granary.  Now Building Granary. */
        if (can_build_improvement(pcity, i)) {
          want=pcity->ai.building_want[i];
          id=i;
        } else {
	  freelog(LOG_DEBUG, "%s can't build %s", pcity->name,
		  get_improvement_name(i));
	}
      } /* id is the building we like the best */
    }
  } impr_type_iterate_end;

  if (want != 0) {
    freelog(LOG_DEBUG, "AI_Chosen: %s with desire = %d for %s",
	    get_improvement_name(id), want, pcity->name);
  } else {
    freelog(LOG_DEBUG, "AI_Chosen: None for %s", pcity->name);
  }
  choice->want = want;
  choice->choice = id;
  choice->type = CT_BUILDING;
}

/**********************************************************************
The following evaluates the unhappiness caused by military units
in the field (or aggressive) at a city when at Republic or Democracy

Now generalised somewhat for government rulesets, though I'm not sure
whether it is fully general for all possible parameters/combinations.
 --dwp
**********************************************************************/
bool ai_assess_military_unhappiness(struct city *pcity,
				   struct government *g)
{
  int free_happy;
  bool have_police;
  int variant;
  int unhap = 0;

  /* bail out now if happy_cost is 0 */
  if (g->unit_happy_cost_factor == 0) {
    return FALSE;
  }
  
  free_happy  = citygov_free_happy(pcity, g);
  have_police = city_got_effect(pcity, B_POLICE);
  variant = improvement_variant(B_WOMENS);

  if (variant==0 && have_police) {
    /* ??  This does the right thing for normal Republic and Democ -- dwp */
    free_happy += g->unit_happy_cost_factor;
  }
  
  unit_list_iterate(pcity->units_supported, punit) {
    int happy_cost = utype_happy_cost(unit_type(punit), g);

    if (happy_cost<=0)
      continue;

    /* See discussion/rules in cityturn.c:city_support() --dwp */
    if (!unit_being_aggressive(punit)) {
      if (is_field_unit(punit)) {
	happy_cost = 1;
      } else {
	happy_cost = 0;
      }
    }
    if (happy_cost<=0)
      continue;

    if (variant==1 && have_police) {
      happy_cost--;
    }
    adjust_city_free_cost(&free_happy, &happy_cost);
    
    if (happy_cost>0)
      unhap += happy_cost;
  }
  unit_list_iterate_end;
 
  if (unhap < 0) unhap = 0;
  return unhap > 0;
}

/**************************************************************************
  AI doesn't want the score for future techs.
**************************************************************************/
bool ai_wants_no_science(struct player *pplayer)
{
  return is_future_tech(pplayer->research.researching);
}
