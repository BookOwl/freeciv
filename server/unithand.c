/***********************************************************************
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
#include <fc_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* utility */
#include "astring.h"
#include "fcintl.h"
#include "mem.h"
#include "rand.h"
#include "shared.h"

/* common */
#include "actions.h"
#include "ai.h"
#include "city.h"
#include "combat.h"
#include "events.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "research.h"
#include "specialist.h"
#include "traderoutes.h"
#include "unit.h"
#include "unitlist.h"

/* common/scriptcore */
#include "luascript_types.h"

/* server */
#include "actiontools.h"
#include "barbarian.h"
#include "citizenshand.h"
#include "citytools.h"
#include "cityturn.h"
#include "diplomats.h"
#include "maphand.h"
#include "notify.h"
#include "plrhand.h"
#include "sanitycheck.h"
#include "spacerace.h"
#include "srv_main.h"
#include "techtools.h"
#include "unittools.h"

/* server/advisors */
#include "autoexplorer.h"
#include "autosettlers.h"

/* server/scripting */
#include "script_server.h"

#include "unithand.h"

/* A category of reasons why an action isn't enabled. */
enum ane_kind {
  /* Explanation: wrong actor unit. */
  ANEK_ACTOR_UNIT,
  /* Explanation: the action is redundant vs this target. */
  ANEK_BAD_TARGET,
  /* Explanation: bad actor terrain. */
  ANEK_BAD_TERRAIN_ACT,
  /* Explanation: bad target terrain. */
  ANEK_BAD_TERRAIN_TGT,
  /* Explanation: being transported. */
  ANEK_IS_TRANSPORTED,
  /* Explanation: not being transported. */
  ANEK_IS_NOT_TRANSPORTED,
  /* Explanation: transports a cargo unit. */
  ANEK_IS_TRANSPORTING,
  /* Explanation: doesn't transport a cargo unit. */
  ANEK_IS_NOT_TRANSPORTING,
  /* Explanation: actor unit has a home city. */
  ANEK_ACTOR_HAS_HOME_CITY,
  /* Explanation: actor unit has no a home city. */
  ANEK_ACTOR_HAS_NO_HOME_CITY,
  /* Explanation: must declare war first. */
  ANEK_NO_WAR,
  /* Explanation: can't be done to domestic targets. */
  ANEK_DOMESTIC,
  /* Explanation: can't be done to foreign targets. */
  ANEK_FOREIGN,
  /* Explanation: not enough MP left. */
  ANEK_LOW_MP,
  /* Explanation: can't be done to city centers. */
  ANEK_IS_CITY_CENTER,
  /* Explanation: can't be done to non city centers. */
  ANEK_IS_NOT_CITY_CENTER,
  /* Explanation: can't be done to claimed target tiles. */
  ANEK_TGT_IS_CLAIMED,
  /* Explanation: can't be done to unclaimed target tiles. */
  ANEK_TGT_IS_UNCLAIMED,
  /* Explanation: the action is disabled in this scenario. */
  ANEK_SCENARIO_DISABLED,
  /* Explanation: too close to a city. */
  ANEK_CITY_TOO_CLOSE_TGT,
  /* Explanation: the target city is too big. */
  ANEK_CITY_TOO_BIG,
  /* Explanation: the target city's population limit banned the action. */
  ANEK_CITY_POP_LIMIT,
  /* Explanation: the specified city don't have the needed capacity. */
  ANEK_CITY_NO_CAPACITY,
  /* Explanation: the target tile is unknown. */
  ANEK_TGT_TILE_UNKNOWN,
  /* Explanation: the action is blocked by another action. */
  ANEK_ACTION_BLOCKS,
  /* Explanation not detected. */
  ANEK_UNKNOWN,
};

/* An explanation why an action isn't enabled. */
struct ane_expl {
  /* The kind of reason why an action isn't enabled. */
  enum ane_kind kind;

  union {
    /* The city without the needed capacity- */
    struct city *capacity_city;

    /* The bad terrain in question. */
    struct terrain *no_act_terrain;

    /* The player to advice declaring war on. */
    struct player *no_war_with;

    /* The action that blocks the action. */
    struct action *blocker;
  };
};

static void illegal_action(struct player *pplayer,
                           struct unit *actor,
                           enum gen_action stopped_action,
                           struct player *tgt_player,
                           const struct tile *target_tile,
                           const struct city *target_city,
                           const struct unit *target_unit,
                           const enum action_requester requester);
static bool city_add_unit(struct player *pplayer, struct unit *punit,
                          struct city *pcity);
static bool city_build(struct player *pplayer, struct unit *punit,
                       struct tile *ptile, const char *name);
static bool do_unit_establish_trade(struct player *pplayer,
                                    struct unit *punit,
                                    struct city *pcity_dest,
                                    bool est_if_able);

static bool unit_do_recycle(struct player *pplayer,
                            struct unit *punit,
                            struct city *pcity);
static bool do_unit_help_build_wonder(struct player *pplayer,
                                      struct unit *punit,
                                      struct city *pcity_dest);
static bool unit_bombard(struct unit *punit, struct tile *ptile);
static bool unit_nuke(struct player *pplayer, struct unit *punit,
                      struct tile *def_tile);
static bool unit_do_destroy_city(struct player *act_player,
                                 struct unit *act_unit,
                                 struct city *tgt_city);
static bool do_unit_disband(struct player *pplayer, struct unit *punit);
static bool do_unit_change_homecity(struct unit *punit,
                                    struct city *pcity);
static bool do_unit_upgrade(struct player *pplayer,
                            struct unit *punit, struct city *pcity,
                            enum action_requester ordered_by);

/**************************************************************************
 Upgrade all units of a given type.
**************************************************************************/
void handle_unit_type_upgrade(struct player *pplayer, Unit_type_id uti)
{
  struct unit_type *to_unittype;
  struct unit_type *from_unittype = utype_by_number(uti);
  int number_of_upgraded_units = 0;

  if (NULL == from_unittype) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_type_upgrade() invalid unit type %d", uti);
    return;
  }

  to_unittype = can_upgrade_unittype(pplayer, from_unittype);
  if (!to_unittype) {
    notify_player(pplayer, NULL, E_BAD_COMMAND, ftc_server,
                  _("Illegal packet, can't upgrade %s (yet)."),
                  utype_name_translation(from_unittype));
    return;
  }

  /* 
   * Try to upgrade units. The order we upgrade in is arbitrary (if
   * the player really cared they should have done it manually). 
   */
  conn_list_do_buffer(pplayer->connections);
  unit_list_iterate(pplayer->units, punit) {
    if (unit_type_get(punit) == from_unittype) {
      struct city *pcity = tile_city(unit_tile(punit));

      if (is_action_enabled_unit_on_city(ACTION_UPGRADE_UNIT, punit, pcity)
          && unit_perform_action(pplayer, punit->id, pcity->id, 0, "",
                                 ACTION_UPGRADE_UNIT, ACT_REQ_SS_AGENT)) {
        number_of_upgraded_units++;
      } else if (UU_NO_MONEY == unit_upgrade_test(punit, FALSE)) {
        break;
      }
    }
  } unit_list_iterate_end;
  conn_list_do_unbuffer(pplayer->connections);

  /* Alert the player about what happened. */
  if (number_of_upgraded_units > 0) {
    const int cost = unit_upgrade_price(pplayer, from_unittype, to_unittype);
    notify_player(pplayer, NULL, E_UNIT_UPGRADED, ftc_server,
                  /* FIXME: plurality of number_of_upgraded_units ignored!
                   * (Plurality of unit names is messed up anyway.) */
                  /* TRANS: "2 Musketeers upgraded to Riflemen for 100 gold."
                   * Plurality is in gold (second %d), not units. */
                  PL_("%d %s upgraded to %s for %d gold.",
                      "%d %s upgraded to %s for %d gold.",
                      cost * number_of_upgraded_units),
                  number_of_upgraded_units,
                  utype_name_translation(from_unittype),
                  utype_name_translation(to_unittype),
                  cost * number_of_upgraded_units);
    send_player_info_c(pplayer, pplayer->connections);
  } else {
    notify_player(pplayer, NULL, E_UNIT_UPGRADED, ftc_server,
                  _("No units could be upgraded."));
  }
}

/**************************************************************************
  Upgrade the unit to a newer unit type.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool do_unit_upgrade(struct player *pplayer,
                            struct unit *punit, struct city *pcity,
                            enum action_requester ordered_by)
{
  char buf[512];

  if (UU_OK == unit_upgrade_info(punit, buf, sizeof(buf))) {
    struct unit_type *from_unit = unit_type_get(punit);
    struct unit_type *to_unit = can_upgrade_unittype(pplayer, from_unit);

    transform_unit(punit, to_unit, FALSE);
    send_player_info_c(pplayer, pplayer->connections);

    if (ordered_by == ACT_REQ_PLAYER) {
      int cost = unit_upgrade_price(pplayer, from_unit, to_unit);

      notify_player(pplayer, unit_tile(punit), E_UNIT_UPGRADED, ftc_server,
                    PL_("%s upgraded to %s for %d gold.",
                        "%s upgraded to %s for %d gold.", cost),
                    utype_name_translation(from_unit),
                    unit_link(punit),
                    cost);
    }

    return TRUE;
  } else {
    if (ordered_by == ACT_REQ_PLAYER) {
      notify_player(pplayer, unit_tile(punit), E_UNIT_UPGRADED, ftc_server,
                    "%s", buf);
    }

    return FALSE;
  }
}

/**************************************************************************
  Capture all the units at pdesttile using punit.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool do_capture_units(struct player *pplayer,
                             struct unit *punit,
                             struct tile *pdesttile)
{
  struct city *pcity;
  char capturer_link[MAX_LEN_LINK];
  const char *capturer_nation = nation_plural_for_player(pplayer);
  bv_unit_types unique_on_tile;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  /* Sanity check: make sure that the capture won't result in the actor
   * ending up with more than one unit of each unique unit type. */
  BV_CLR_ALL(unique_on_tile);
  unit_list_iterate(pdesttile->units, to_capture) {
    bool unique_conflict = FALSE;

    /* Check what the player already has. */
    if (utype_player_already_has_this_unique(pplayer,
                                             unit_type_get(to_capture))) {
      /* The player already has a unit of this kind. */
      unique_conflict = TRUE;
    }

    if (utype_has_flag(unit_type_get(to_capture), UTYF_UNIQUE)) {
      /* The type of the units at the tile must also be checked. Two allied
       * players can both have their unique unit at the same tile.
       * Capturing them both would give the actor two units of a kind that
       * is supposed to be unique. */

      if (BV_ISSET(unique_on_tile, utype_index(unit_type_get(to_capture)))) {
        /* There is another unit of the same kind at this tile. */
        unique_conflict = TRUE;
      } else {
        /* Remember the unit type in case another unit of the same kind is
         * encountered later. */
        BV_SET(unique_on_tile, utype_index(unit_type_get(to_capture)));
      }
    }

    if (unique_conflict) {
      log_debug("capture units: already got unique unit");
      notify_player(pplayer, pdesttile, E_UNIT_ILLEGAL_ACTION, ftc_server,
                    /* TRANS: You can only have one Leader. */
                    _("You can only have one %s."),
                    unit_link(to_capture));

      return FALSE;
    }
  } unit_list_iterate_end;

  /* N.B: unit_link() always returns the same pointer. */
  sz_strlcpy(capturer_link, unit_link(punit));

  pcity = tile_city(pdesttile);
  unit_list_iterate(pdesttile->units, to_capture) {
    struct player *uplayer = unit_owner(to_capture);
    const char *victim_link;

    unit_owner(to_capture)->score.units_lost++;
    to_capture = unit_change_owner(to_capture, pplayer,
                                   (game.server.homecaughtunits
                                    ? punit->homecity
                                    : IDENTITY_NUMBER_ZERO),
                                   ULR_CAPTURED);
    /* As unit_change_owner() currently remove the old unit and
       * replace by a new one (with a new id), we want to make link to
       * the new unit. */
    victim_link = unit_link(to_capture);

    /* Notify players */
    notify_player(pplayer, pdesttile, E_MY_DIPLOMAT_BRIBE, ftc_server,
                  /* TRANS: <unit> ... <unit> */
                  _("Your %s succeeded in capturing the %s %s."),
                  capturer_link, nation_adjective_for_player(uplayer),
                  victim_link);
    notify_player(uplayer, pdesttile,
                  E_ENEMY_DIPLOMAT_BRIBE, ftc_server,
                  /* TRANS: <unit> ... <Poles> */
                  _("Your %s was captured by the %s."),
                  victim_link, capturer_nation);

    /* May cause an incident */
    action_consequence_success(ACTION_CAPTURE_UNITS, pplayer,
                               unit_owner(to_capture),
                               pdesttile, victim_link);

    if (NULL != pcity) {
      /* The captured unit is in a city. Bounce it. */
      bounce_unit(to_capture, TRUE);
    }
  } unit_list_iterate_end;

  /* Subtract movement point from capturer */
  punit->moves_left -= SINGLE_MOVE;
  if (punit->moves_left < 0) {
    punit->moves_left = 0;
  }

  send_unit_info(NULL, punit);

  return TRUE;
}

/**************************************************************************
  Expel the target unit to his owner's capital.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool do_expel_unit(struct player *pplayer,
                          struct unit *actor,
                          struct unit *target)
{
  char target_link[MAX_LEN_LINK];
  struct player *uplayer;
  struct tile *target_tile;
  struct city *pcity;

  /* Maybe it didn't survive the Lua call back. Why wasn't this caught by
   * the caller? Check in the code that emits the signal. */
  fc_assert_ret_val(target, FALSE);

  uplayer = unit_owner(target);

  /* A unit is supposed to have an owner. */
  fc_assert_ret_val(uplayer, FALSE);

  /* Maybe it didn't survive the Lua call back. Why wasn't this caught by
   * the caller? Check in the code that emits the signal. */
  fc_assert_ret_val(actor, FALSE);

  /* Where is the actor player? */
  fc_assert_ret_val(pplayer, FALSE);

  /* The price of attempting an expulsion is a single move. Applies before
   * the player is told if the target has a capital. */
  actor->moves_left = MAX(0, actor->moves_left - SINGLE_MOVE);
  send_unit_info(NULL, actor);

  target_tile = unit_tile(target);

  /* Expel the target unit to his owner's capital. */
  pcity = player_capital(uplayer);

  /* N.B: unit_link() always returns the same pointer. */
  sz_strlcpy(target_link, unit_link(target));

  if (pcity == NULL) {
    /* No where to send the expelled unit. */

    /* Notify the actor player. */
    notify_player(pplayer, target_tile, E_UNIT_ACTION_FAILED, ftc_server,
                  /* TRANS: <Poles> <Spy> */
                  _("The %s don't have a capital to expel their %s to."),
                  nation_plural_for_player(uplayer), target_link);

    /* Nothing more could be done. */
    return FALSE;
  }

  /* Please review the code below and above (including the strings sent to
   * the players) before allowing expulsion to non capital cities. */
  fc_assert(is_capital(pcity));

  /* Notify everybody involved. */
  notify_player(pplayer, target_tile, E_UNIT_DID_EXPEL, ftc_server,
                /* TRANS: <Border Patrol> ... <Spy> */
                _("Your %s succeeded in expelling the %s %s."),
                unit_link(actor), nation_adjective_for_player(uplayer),
                target_link);
  notify_player(uplayer, target_tile, E_UNIT_WAS_EXPELLED, ftc_server,
                /* TRANS: <unit> ... <Poles> */
                _("Your %s was expelled by the %s."),
                target_link, nation_plural_for_player(pplayer));

  /* Being expelled destroys all remaining movement. */
  if (!teleport_unit_to_city(target, pcity, -1, FALSE)) {
    log_error("Bug in unit expulsion: unit can't teleport.");

    return FALSE;
  }

  /* This may cause a diplomatic incident */
  action_consequence_success(ACTION_EXPEL_UNIT, pplayer, uplayer,
                             target_tile, target_link);

  /* Mission accomplished. */
  return TRUE;
}

/**************************************************************************
  Returns TRUE iff, from the point of view of the owner of the actor unit,
  it looks like the actor unit may be able to perform a non action
  enabler controlled action against a unit or city on the target_tile by
  "moving" to it.
**************************************************************************/
static bool may_non_act_move(struct unit *actor_unit,
                             struct city *target_city,
                             struct tile *target_tile,
                             bool igzoc)
{
  if (unit_can_move_to_tile(actor_unit, target_tile, igzoc)) {
    /* Move. Includes occupying a foreign city. */
    return TRUE;
  }

  if (can_unit_attack_tile(actor_unit, target_tile)) {
    /* Attack. Includes nuking. */
    return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Returns the first player that may enable the specified action if war is
  declared.

  Helper for need_war_player(). Use it in stead.
**************************************************************************/
static struct player *need_war_player_hlp(const struct unit *actor,
                                          const int act,
                                          const struct tile *target_tile,
                                          const struct city *target_city,
                                          const struct unit *target_unit)
{
  if (action_get_actor_kind(act) != AAK_UNIT) {
    /* No unit can ever do this action so it isn't relevant. */
    return NULL;
  }

  if (!unit_can_do_action(actor, act)) {
    /* The unit can't do the action no matter if there is war or not. */
    return NULL;
  }

  if (can_utype_do_act_if_tgt_diplrel(unit_type_get(actor),
                                      act, DS_WAR, FALSE)) {
    /* The unit can do the action even if there isn't war. */
    return NULL;
  }

  switch (action_get_target_kind(act)) {
  case ATK_CITY:
    if (target_city == NULL) {
      /* No target city. */
      return NULL;
    }

    if (player_diplstate_get(unit_owner(actor),
                             city_owner(target_city))->type != DS_WAR) {
      return city_owner(target_city);
    }
    break;
  case ATK_UNIT:
    if (target_unit == NULL) {
      /* No target unit. */
      return NULL;
    }

    if (player_diplstate_get(unit_owner(actor),
                             unit_owner(target_unit))->type != DS_WAR) {
      return unit_owner(target_unit);
    }
    break;
  case ATK_UNITS:
    if (target_tile == NULL) {
      /* No target units since no target tile. */
      return NULL;
    }

    unit_list_iterate(target_tile->units, tunit) {
      if (player_diplstate_get(unit_owner(actor),
                               unit_owner(tunit))->type != DS_WAR) {
        return unit_owner(tunit);
      }
    } unit_list_iterate_end;
    break;
  case ATK_TILE:
    if (target_tile == NULL) {
      /* No target tile. */
      return NULL;
    }

    if (player_diplstate_get(unit_owner(actor),
                             tile_owner(target_tile))->type != DS_WAR) {
      return tile_owner(target_tile);
    }
    break;
  case ATK_SELF:
    /* Can't declare war on it self. */
    return NULL;
    break;
  case ATK_COUNT:
    /* Nothing to check. */
    fc_assert(action_get_target_kind(act) != ATK_COUNT);
    return NULL;
  }

  /* Declaring war won't enable the specified action. */
  return NULL;
}

/**************************************************************************
  Returns the first player that may enable the specified action if war is
  declared. If the specified action is ACTION_ANY the first player that
  may enable any action at all if war is declared will be returned.
**************************************************************************/
static struct player *need_war_player(const struct unit *actor,
                                      const int action_id,
                                      const struct tile *target_tile,
                                      const struct city *target_city,
                                      const struct unit *target_unit)
{
  if (action_id == ACTION_ANY) {
    /* Any action at all will do. */
    action_iterate(act) {
      struct player *war_player;

      war_player = need_war_player_hlp(actor, act,
                                       target_tile, target_city,
                                       target_unit);

      if (war_player != NULL) {
        /* Declaring war on this player may enable this action. */
        return war_player;
      }
    } action_iterate_end;

    /* No action at all may be enabled by declaring war. */
    return NULL;
  } else {
    /* Look for the specified action. */
    return need_war_player_hlp(actor, action_id,
                               target_tile, target_city,
                               target_unit);
  }
}

/**************************************************************************
  Returns TRUE if the specified action can't be done now but would have
  been legal if the unit had full movement.
**************************************************************************/
static bool need_full_mp(const struct unit *actor, const int action_id)
{
  fc_assert(action_id_is_valid(action_id) || action_id == ACTION_ANY);

  /* Check if full movement points may enable the specified action. */
  return !utype_may_act_move_frags(unit_type_get(actor),
                                   action_id,
                                   actor->moves_left)
      && utype_may_act_move_frags(unit_type_get(actor),
                                  action_id,
                                  unit_move_rate(actor));
}

/**************************************************************************
  Returns TRUE iff the specified terrain type blocks the specified action.

  If the "action" is ACTION_ANY all actions are checked.
**************************************************************************/
static bool does_terrain_block_action(const int action_id,
                                      bool is_target,
                                      struct terrain *pterrain)
{
  struct universal univ_terr
      = {.kind = VUT_TERRAIN, .value = {.terrain = pterrain}};

  if (action_id == ACTION_ANY) {
    /* Any action is OK. */
    action_iterate(alt_act) {
      if (!does_terrain_block_action(alt_act, is_target, pterrain)) {
        /* Only one action has to be possible. */
        return FALSE;
      }
    } action_iterate_end;

    /* No action enabled. */
    return TRUE;
  }

  /* ACTION_ANY is handled above. */
  fc_assert_ret_val(action_id_is_valid(action_id), FALSE);

  action_enabler_list_iterate(action_enablers_for_action(action_id),
                              enabler) {
    if (universal_fulfills_requirement(FALSE,
                                       (is_target ? &enabler->target_reqs
                                                  : &enabler->actor_reqs),
                                       &univ_terr)) {
      /* This terrain kind doesn't block this action enabler. */
      return FALSE;
    }
  } action_enabler_list_iterate_end;

  return TRUE;
}

/**************************************************************************
  Returns an explaination why punit can't perform the specified action
  based on the current game state.
**************************************************************************/
static struct ane_expl *expl_act_not_enabl(struct unit *punit,
                                           const int action_id,
                                           const struct tile *target_tile,
                                           const struct city *target_city,
                                           const struct unit *target_unit)
{
  struct player *must_war_player;
  struct action *blocker;
  struct player *tgt_player = NULL;
  struct ane_expl *explnat = fc_malloc(sizeof(struct ane_expl));
  bool can_exist = can_unit_exist_at_tile(punit, unit_tile(punit));
  int action_custom;

  if (action_id == ACTION_ANY) {
    /* Find the target player of some actions. */
    if (target_city) {
      /* Individual city targets have the highest priority. */
      tgt_player = city_owner(target_city);
    } else if (target_unit) {
      /* Individual unit targets have the next priority. */
      tgt_player = unit_owner(target_unit);
    } else if (target_tile) {
      /* Tile targets have the lowest priority. */
      tgt_player = tile_owner(target_tile);
    }
  } else {
    /* Find the target player of this action. */
    switch (action_get_target_kind(action_id)) {
    case ATK_CITY:
      tgt_player = city_owner(target_city);
      break;
    case ATK_UNIT:
      tgt_player = unit_owner(target_unit);
      break;
    case ATK_TILE:
      tgt_player = tile_owner(target_tile);
      break;
    case ATK_UNITS:
      /* A unit stack may contain units with multiple owners. Pick the
       * first one. */
      if (target_tile
          && unit_list_size(target_tile->units) > 0) {
        tgt_player = unit_owner(unit_list_get(target_tile->units, 0));
      }
      break;
    case ATK_SELF:
      /* A unit acting against it self. */
      tgt_player = unit_owner(punit);
      break;
    case ATK_COUNT:
      fc_assert(action_get_target_kind(action_id) != ATK_COUNT);
      break;
    }
  }

  switch (action_id) {
  case ACTION_FOUND_CITY:
    /* Detects that the target is closer to a city than citymindist allows.
     * Detects that the target tile is claimed by a foreigner even when it
     * is legal to found a city on an unclaimed or domestic tile. */
    action_custom = city_build_here_test(target_tile, punit);
    break;
  case ACTION_AIRLIFT:
    action_custom = test_unit_can_airlift_to(NULL, punit, target_city);
    break;
  default:
    action_custom = 0;
    break;
  }

  if (!unit_can_do_action(punit, action_id)) {
    explnat->kind = ANEK_ACTOR_UNIT;
  } else if (action_id == ACTION_FOUND_CITY
             && tile_city(target_tile)) {
    explnat->kind = ANEK_BAD_TARGET;
  } else if ((!can_exist
       && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                        USP_LIVABLE_TILE, FALSE))
      || (can_exist
          && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                           USP_LIVABLE_TILE, TRUE))) {
    explnat->kind = ANEK_BAD_TERRAIN_ACT;
    explnat->no_act_terrain = tile_terrain(unit_tile(punit));
  } else if (punit
             && does_terrain_block_action(action_id, FALSE,
                 tile_terrain(unit_tile(punit)))) {
    /* No action enabler allows acting against this terrain kind. */
    explnat->kind = ANEK_BAD_TERRAIN_ACT;
    explnat->no_act_terrain = tile_terrain(unit_tile(punit));
  } else if (action_id == ACTION_FOUND_CITY
             && target_tile
             && terrain_has_flag(tile_terrain(target_tile),
                                 TER_NO_CITIES)) {
    explnat->kind = ANEK_BAD_TERRAIN_TGT;
    explnat->no_act_terrain = tile_terrain(target_tile);
  } else if (target_tile
             && does_terrain_block_action(action_id, TRUE,
                                          tile_terrain(target_tile))) {
    /* No action enabler allows acting against this terrain kind. */
    explnat->kind = ANEK_BAD_TERRAIN_TGT;
    explnat->no_act_terrain = tile_terrain(target_tile);
  } else if (unit_transported(punit)
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_TRANSPORTED, TRUE)) {
    explnat->kind = ANEK_IS_TRANSPORTED;
  } else if (!unit_transported(punit)
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_TRANSPORTED, FALSE)) {
    explnat->kind = ANEK_IS_NOT_TRANSPORTED;
  } else if (0 < get_transporter_occupancy(punit)
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_TRANSPORTING, TRUE)) {
    explnat->kind = ANEK_IS_TRANSPORTING;
  } else if ((action_id == ACTION_PARADROP || action_id == ACTION_AIRLIFT)
             && 0 < get_transporter_occupancy(punit)) {
    /* Hard action requirement. */
    explnat->kind = ANEK_IS_TRANSPORTING;
  } else if (!(0 < get_transporter_occupancy(punit))
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_TRANSPORTING, FALSE)) {
    explnat->kind = ANEK_IS_NOT_TRANSPORTING;
  } else if ((punit->homecity > 0)
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_HAS_HOME_CITY, TRUE)) {
    explnat->kind = ANEK_ACTOR_HAS_HOME_CITY;
  } else if ((punit->homecity <= 0)
             && !utype_can_do_act_when_ustate(unit_type_get(punit), action_id,
                                              USP_HAS_HOME_CITY, FALSE)) {
    explnat->kind = ANEK_ACTOR_HAS_NO_HOME_CITY;
  } else if ((punit->homecity <= 0)
             && (action_id == ACTION_TRADE_ROUTE
                 || action_id == ACTION_MARKETPLACE)) {
    explnat->kind = ANEK_ACTOR_HAS_NO_HOME_CITY;
  } else if ((must_war_player = need_war_player(punit,
                                                action_id,
                                                target_tile,
                                                target_city,
                                                target_unit))) {
    explnat->kind = ANEK_NO_WAR;
    explnat->no_war_with = must_war_player;
  } else if (need_full_mp(punit, action_id)) {
    explnat->kind = ANEK_LOW_MP;
  } else if (tgt_player
             && unit_owner(punit) != tgt_player
             && !can_utype_do_act_if_tgt_diplrel(unit_type_get(punit),
                                                 action_id,
                                                 DRO_FOREIGN,
                                                 TRUE)) {
    explnat->kind = ANEK_FOREIGN;
  } else if (action_id == ACTION_FOUND_CITY
             && action_custom == CB_BAD_BORDERS) {
    explnat->kind = ANEK_FOREIGN;
  } else if (tgt_player
             && unit_owner(punit) == tgt_player
             && !can_utype_do_act_if_tgt_diplrel(unit_type_get(punit),
                                                 action_id,
                                                 DRO_FOREIGN,
                                                 FALSE)) {
    explnat->kind = ANEK_DOMESTIC;
  } else if ((target_tile && tile_city(target_tile))
             && !utype_may_act_tgt_city_tile(unit_type_get(punit),
                                             action_id,
                                             CITYT_CENTER,
                                             TRUE)) {
    explnat->kind = ANEK_IS_CITY_CENTER;
  } else if ((target_tile && !tile_city(target_tile))
             && !utype_may_act_tgt_city_tile(unit_type_get(punit),
                                             action_id,
                                             CITYT_CENTER,
                                             FALSE)) {
    explnat->kind = ANEK_IS_NOT_CITY_CENTER;
  } else if ((target_tile && tile_owner(target_tile) != NULL)
             && !utype_may_act_tgt_city_tile(unit_type_get(punit),
                                             action_id,
                                             CITYT_CLAIMED,
                                             TRUE)) {
    explnat->kind = ANEK_TGT_IS_CLAIMED;
  } else if ((target_tile && tile_owner(target_tile) == NULL)
             && !utype_may_act_tgt_city_tile(unit_type_get(punit),
                                             action_id,
                                             CITYT_CLAIMED,
                                             FALSE)) {
    explnat->kind = ANEK_TGT_IS_UNCLAIMED;
  } else if (target_city
             && (action_id == ACTION_JOIN_CITY
                 && action_actor_utype_hard_reqs_ok(ACTION_JOIN_CITY,
                                                    unit_type_get(punit))
                 && (city_size_get(target_city) + unit_pop_value(punit)
                     > game.info.add_to_size_limit))) {
    /* TODO: Check max city size requirements from action enabler target
     * vectors. */
    explnat->kind = ANEK_CITY_TOO_BIG;
  } else if (target_city
             && (action_id == ACTION_JOIN_CITY
                 && action_actor_utype_hard_reqs_ok(ACTION_JOIN_CITY,
                                                    unit_type_get(punit))
                 && (!city_can_grow_to(target_city,
                                       city_size_get(target_city)
                                       + unit_pop_value(punit))))) {
    explnat->kind = ANEK_CITY_POP_LIMIT;
  } else if (action_id == ACTION_AIRLIFT
             && action_custom == AR_SRC_NO_FLIGHTS) {
    explnat->kind = ANEK_CITY_NO_CAPACITY;
    explnat->capacity_city = tile_city(unit_tile(punit));
  } else if (action_id == ACTION_AIRLIFT
             && action_custom == AR_DST_NO_FLIGHTS) {
    explnat->kind = ANEK_CITY_NO_CAPACITY;
    explnat->capacity_city = game_city_by_number(target_city->id);
  } else if (action_id == ACTION_FOUND_CITY
             && action_custom == CB_NO_MIN_DIST) {
    explnat->kind = ANEK_CITY_TOO_CLOSE_TGT;
  } else if (action_id == ACTION_PARADROP
             && target_tile
             && !map_is_known(target_tile, unit_owner(punit))) {
    explnat->kind = ANEK_TGT_TILE_UNKNOWN;
  } else if ((game.scenario.prevent_new_cities
              && utype_can_do_action(unit_type_get(punit), ACTION_FOUND_CITY))
             && (action_id == ACTION_FOUND_CITY
                 || action_id == ACTION_ANY)) {
    /* Please add a check for any new action forbidding scenario setting
     * above this comment. */
    explnat->kind = ANEK_SCENARIO_DISABLED;
  } else if (action_id_is_valid(action_id)
             && (blocker = action_is_blocked_by(action_id, punit,
                                                target_tile, target_city,
                                                target_unit))) {
    explnat->kind = ANEK_ACTION_BLOCKS;
    explnat->blocker = blocker;
  } else {
    explnat->kind = ANEK_UNKNOWN;
  }

  return explnat;
}

/**************************************************************************
  Explain why punit can't perform any action at all based on its current
  game state.
**************************************************************************/
static void explain_why_no_action_enabled(struct unit *punit,
                                          const struct tile *target_tile,
                                          const struct city *target_city,
                                          const struct unit *target_unit)
{
  struct player *pplayer = unit_owner(punit);
  struct ane_expl *explnat = expl_act_not_enabl(punit, ACTION_ANY,
                                                target_tile,
                                                target_city, target_unit);

  switch (explnat->kind) {
  case ANEK_ACTOR_UNIT:
    /* This shouldn't happen unless the client is buggy given the current
     * users. */
    fc_assert_msg(explnat->kind != ANEK_ACTOR_UNIT,
                  "Asked to explain why a non actor can't act.");

    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Unit cannot do anything."));
    break;
  case ANEK_BAD_TARGET:
    /* This shouldn't happen at the moment. Only specific action checks
     * will trigger bad target checks. This is a reply to a question about
     * any action. */
    fc_assert(explnat->kind != ANEK_BAD_TARGET);

    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Your %s found no suitable target."),
                  unit_name_translation(punit));
    break;
  case ANEK_BAD_TERRAIN_ACT:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Unit cannot act from %s."),
                  terrain_name_translation(explnat->no_act_terrain));
    break;
  case ANEK_BAD_TERRAIN_TGT:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Unit cannot act against %s."),
                  terrain_name_translation(explnat->no_act_terrain));
    break;
  case ANEK_IS_TRANSPORTED:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit is being transported, and"
                    " so cannot act."));
    break;
  case ANEK_IS_NOT_TRANSPORTED:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act when it isn't being "
                    "transported."));
    break;
  case ANEK_IS_TRANSPORTING:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit is transporting, and"
                    " so cannot act."));
    break;
  case ANEK_IS_NOT_TRANSPORTING:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act when it isn't transporting."));
    break;
  case ANEK_ACTOR_HAS_HOME_CITY:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit has a home city, and so cannot act."));
    break;
  case ANEK_ACTOR_HAS_NO_HOME_CITY:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act unless it has a home city."));
    break;
  case ANEK_NO_WAR:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("You must declare war on %s first.  Try using "
                    "the Nations report (F3)."),
                  player_name(explnat->no_war_with));
    break;
  case ANEK_DOMESTIC:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against domestic targets."));
    break;
  case ANEK_FOREIGN:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against foreign targets."));
    break;
  case ANEK_LOW_MP:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit has too few moves left to act."));
    break;
  case ANEK_IS_CITY_CENTER:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against city centers."));
    break;
  case ANEK_IS_NOT_CITY_CENTER:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against non city centers."));
    break;
  case ANEK_TGT_IS_CLAIMED:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against claimed tiles."));
    break;
  case ANEK_TGT_IS_UNCLAIMED:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit cannot act against unclaimed tiles."));
    break;
  case ANEK_SCENARIO_DISABLED:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Can't perform any action this scenario permits."));
    break;
  case ANEK_CITY_TOO_CLOSE_TGT:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Can't perform any action this close to a city."));
    break;
  case ANEK_CITY_TOO_BIG:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  /* TRANS: Settler ... Berlin */
                  _("%s can't do anything to %s. It is too big."),
                  unit_name_translation(punit),
                  city_name_get(target_city));
    break;
  case ANEK_CITY_POP_LIMIT:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  /* TRANS: London ... Settlers */
                  _("%s needs an improvement to grow, so "
                    "%s cannot do anything to it."),
                  city_name_get(target_city),
                  unit_name_translation(punit));
    break;
  case ANEK_CITY_NO_CAPACITY:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  /* TRANS: Paris ... Warriors (think: airlift) */
                  _("%s don't have enough capacity, so "
                    "%s cannot do anything."),
                  city_name_get(explnat->capacity_city),
                  unit_name_translation(punit));
    break;
  case ANEK_TGT_TILE_UNKNOWN:
    notify_player(pplayer, target_tile, E_BAD_COMMAND, ftc_server,
                  /* TRANS: Paratroopers ... */
                  _("%s can't do anything to an unknown target tile."),
                  unit_name_translation(punit));
    break;
  case ANEK_ACTION_BLOCKS:
    /* If an action blocked another action the blocking action must be
     * possible. */
    fc_assert(explnat->kind != ANEK_ACTION_BLOCKS);
    /* Fall through to unknown cause. */
  case ANEK_UNKNOWN:
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("No action possible."));
    break;
  }

  free(explnat);
}

/**************************************************************************
  Handle a query for what actions a unit may do.

  MUST always send a reply so the client can move on in the queue. This
  includes when the client give invalid input. That the acting unit died
  before the server received a request for what actions it could do should
  not stop the client from processing the next unit in the queue.
**************************************************************************/
void handle_unit_get_actions(struct connection *pc,
                             const int actor_unit_id,
                             const int target_unit_id_client,
                             const int target_city_id_client,
                             const int target_tile_id,
                             const bool disturb_player)
{
  struct player *actor_player;
  struct unit *actor_unit;
  struct tile *target_tile;
  action_probability probabilities[ACTION_COUNT];

  struct unit *target_unit;
  struct city *target_city;

  /* No potentially legal action is known yet. If none is found the player
   * should get an explanation. */
  bool at_least_one_action = FALSE;

  /* A target should only be sent if it is possible to act against it */
  int target_city_id = IDENTITY_NUMBER_ZERO;
  int target_unit_id = IDENTITY_NUMBER_ZERO;

  actor_player = pc->playing;
  actor_unit = game_unit_by_number(actor_unit_id);
  target_tile = index_to_tile(target_tile_id);

  /* Check if the request is valid. */
  if (!target_tile || !actor_unit || !actor_player
      || actor_unit->owner != actor_player) {
    action_iterate(act) {
      /* No probability can exist when the request is invalid. */
      probabilities[act] = ACTPROB_NA;
    } action_iterate_end;

    dsend_packet_unit_actions(pc, actor_unit_id,
                              IDENTITY_NUMBER_ZERO, IDENTITY_NUMBER_ZERO,
                              target_tile_id,
                              disturb_player,
                              probabilities);
    return;
  }

  /* Select the targets. */

  if (target_unit_id_client == IDENTITY_NUMBER_ZERO) {
    /* Find a new target unit. */
    target_unit = action_tgt_unit(actor_unit, target_tile, TRUE);
  } else {
    /* Prepare the client selected target unit. */
    target_unit = game_unit_by_number(target_unit_id_client);
  }

  if (target_city_id_client == IDENTITY_NUMBER_ZERO) {
    /* Find a new target city. */
    target_city = action_tgt_city(actor_unit, target_tile, TRUE);
  } else {
    /* Prepare the client selected target city. */
    target_city = game_city_by_number(target_city_id_client);
  }

  /* Find out what can be done to the targets. */

  /* Set the probability for the actions. */
  action_iterate(act) {
    if (action_get_actor_kind(act) != AAK_UNIT) {
      /* Not relevant. */
      probabilities[act] = ACTPROB_NA;
      continue;
    }

    if (target_city && action_get_target_kind(act) == ATK_CITY) {
      probabilities[act] = action_prob_vs_city(actor_unit, act,
                                               target_city);
    } else if (target_unit && action_get_target_kind(act) == ATK_UNIT) {
      probabilities[act] = action_prob_vs_unit(actor_unit, act,
                                               target_unit);
    } else if (target_tile && action_get_target_kind(act) == ATK_UNITS) {
      probabilities[act] = action_prob_vs_units(actor_unit, act,
                                                target_tile);
    } else if (target_tile && action_get_target_kind(act) == ATK_TILE) {
      probabilities[act] = action_prob_vs_tile(actor_unit, act,
                                               target_tile);
    } else if (action_get_target_kind(act) == ATK_SELF) {
      probabilities[act] = action_prob_self(actor_unit, act);
    } else {
      probabilities[act] = ACTPROB_IMPOSSIBLE;
    }
  } action_iterate_end;

  /* Analyze the probabilities. Decide what targets to send and if an
   * explanation is needed. */
  action_iterate(act) {
    if (action_prob_possible(probabilities[act])) {
      /* An action can be done. No need to explain why no action can be
       * done. */
      at_least_one_action = TRUE;

      switch (action_get_target_kind(act)) {
      case ATK_CITY:
        /* The city should be sent as a target since it is possible to act
         * against it. */
        fc_assert(target_city != NULL);
        target_city_id = target_city->id;
        break;
      case ATK_UNIT:
        /* The unit should be sent as a target since it is possible to act
         * against it. */
        fc_assert(target_unit != NULL);
        target_unit_id = target_unit->id;
        break;
      case ATK_TILE:
      case ATK_UNITS:
        /* The target tile aren't selected here so it haven't changed. */
        fc_assert(target_tile != NULL);
        break;
      case ATK_SELF:
        /* The target unit is the actor unit. It is already sent. */
        fc_assert(actor_unit != NULL);
        break;
      case ATK_COUNT:
        fc_assert_msg(action_get_target_kind(act) != ATK_COUNT,
                      "Invalid action target kind.");
        break;
      }

      if (target_city_id != IDENTITY_NUMBER_ZERO
          && target_unit != IDENTITY_NUMBER_ZERO) {
        /* No need to find out more. */
        break;
      }
    }
  } action_iterate_end;

  /* Send possible actions and targets. */
  dsend_packet_unit_actions(pc,
                            actor_unit_id, target_unit_id, target_city_id,
                            target_tile_id,
                            disturb_player,
                            probabilities);

  if (disturb_player && !at_least_one_action) {
    /* The user should get an explanation why no action is possible. */
    explain_why_no_action_enabled(actor_unit,
                                  target_tile, target_city, target_unit);
  }
}

/**************************************************************************
  Try to explain to the player why an action is illegal.

  Event type should be E_BAD_COMMAND if the player should know that the
  action is illegal or E_UNIT_ILLEGAL_ACTION if the player potentially new
  information is being revealed.
**************************************************************************/
void illegal_action_msg(struct player *pplayer,
                        const enum event_type event,
                        struct unit *actor,
                        const int stopped_action,
                        const struct tile *target_tile,
                        const struct city *target_city,
                        const struct unit *target_unit)
{
  struct ane_expl *explnat;

  /* Explain why the action was illegal. */
  explnat = expl_act_not_enabl(actor, stopped_action,
                               target_tile, target_city, target_unit);
  switch (explnat->kind) {
  case ANEK_ACTOR_UNIT:
    {
      struct astring astr = ASTRING_INIT;

      if (role_units_translations(&astr,
                                  action_get_role(stopped_action),
                                  TRUE)) {
        notify_player(pplayer, unit_tile(actor),
                      event, ftc_server,
                      /* TRANS: Only Diplomat or Spy can do Steal Gold. */
                      _("Only %s can do %s."),
                      astr_str(&astr),
                      action_get_ui_name(stopped_action));
        astr_free(&astr);
      } else {
        notify_player(pplayer, unit_tile(actor),
                      event, ftc_server,
                      /* TRANS: Spy can't do Capture Units. */
                      _("%s can't do %s."),
                      unit_name_translation(actor),
                      action_get_ui_name(stopped_action));
      }
    }
    break;
  case ANEK_BAD_TARGET:
    notify_player(pplayer, unit_tile(actor), event, ftc_server,
                  _("Having your %s do %s to this target is redundant."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_BAD_TERRAIN_ACT:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s from %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  terrain_name_translation(explnat->no_act_terrain));
    break;
  case ANEK_BAD_TERRAIN_TGT:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  terrain_name_translation(explnat->no_act_terrain));
    break;
  case ANEK_IS_TRANSPORTED:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s while being transported."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_IS_NOT_TRANSPORTED:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s while not being transported."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_IS_TRANSPORTING:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s while transporting."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_IS_NOT_TRANSPORTING:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s while not transporting."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_ACTOR_HAS_HOME_CITY:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s because it has a home city."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_ACTOR_HAS_NO_HOME_CITY:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s because it is homeless."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_NO_WAR:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s while you"
                    " aren't at war with %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  player_name(explnat->no_war_with));
    break;
  case ANEK_DOMESTIC:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to domestic %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  action_target_kind_translated_name(
                    action_get_target_kind(stopped_action)));
    break;
  case ANEK_FOREIGN:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to foreign %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  action_target_kind_translated_name(
                    action_get_target_kind(stopped_action)));
    break;
  case ANEK_LOW_MP:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s has too few moves left to %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_IS_CITY_CENTER:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to city centers."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_IS_NOT_CITY_CENTER:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can only do %s to city centers."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_TGT_IS_CLAIMED:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to claimed tiles."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_TGT_IS_UNCLAIMED:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s can't do %s to unclaimed tiles."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_SCENARIO_DISABLED:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Can't do Build City in this scenario. */
                  _("Can't do %s in this scenario."),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_CITY_TOO_CLOSE_TGT:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Can't do Build City this close to a city. */
                  _("Can't do %s this close to a city."),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_CITY_TOO_BIG:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Settlers ... Join City ... London */
                  _("%s can't do %s to %s. It is too big."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  city_name_get(target_city));
    break;
  case ANEK_CITY_POP_LIMIT:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: London ... Settlers ... Join City */
                  _("%s needs an improvement to grow, so "
                    "%s cannot do %s."),
                  city_name_get(target_city),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_CITY_NO_CAPACITY:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Paris ... Airlift to City ... Warriors */
                  _("%s has no capacity to %s %s."),
                  city_name_get(explnat->capacity_city),
                  action_get_ui_name(stopped_action),
                  unit_name_translation(actor));
    break;
  case ANEK_TGT_TILE_UNKNOWN:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Paratroopers ... Drop Paratrooper */
                  _("%s can't do %s to an unknown tile."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  case ANEK_ACTION_BLOCKS:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  /* TRANS: Freight ... Recycle Unit ... Help Wonder ... */
                  _("Your %s can't do %s when %s is legal."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action),
                  action_get_ui_name(explnat->blocker->id));
    break;
  case ANEK_UNKNOWN:
    notify_player(pplayer, unit_tile(actor),
                  event, ftc_server,
                  _("Your %s was unable to %s."),
                  unit_name_translation(actor),
                  action_get_ui_name(stopped_action));
    break;
  }

  free(explnat);
}

/**************************************************************************
  Tell the client that the action it requested is illegal. This can be
  caused by the player (and therefore the client) not knowing that some
  condition of an action no longer is true.
**************************************************************************/
static void illegal_action(struct player *pplayer,
                           struct unit *actor,
                           enum gen_action stopped_action,
                           struct player *tgt_player,
                           const struct tile *target_tile,
                           const struct city *target_city,
                           const struct unit *target_unit,
                           const enum action_requester requester)
{
  /* Why didn't the game check before trying something illegal? Did a good
   * reason to not call is_action_enabled_unit_on...() appear? The game is
   * omniscient... */
  fc_assert(requester != ACT_REQ_RULES);

  /* Don't punish the player for something the game did. Don't tell the
   * player that the rules required the game to try to do something
   * illegal. */
  fc_assert_ret_msg((requester == ACT_REQ_PLAYER
                     || requester == ACT_REQ_SS_AGENT),
                    "The player wasn't responsible for this.");

  /* The mistake may have a cost. */
  actor->moves_left = MAX(0, actor->moves_left
      - get_target_bonus_effects(NULL,
                                 unit_owner(actor),
                                 tgt_player,
                                 NULL,
                                 NULL,
                                 NULL,
                                 actor,
                                 unit_type_get(actor),
                                 NULL,
                                 NULL,
                                 action_by_number(stopped_action),
                                 EFT_ILLEGAL_ACTION_MOVE_COST));

  send_unit_info(NULL, actor);

  illegal_action_msg(pplayer, E_UNIT_ILLEGAL_ACTION,
                     actor, stopped_action,
                     target_tile, target_city, target_unit);
}

/**************************************************************************
  Inform the client that something went wrong during a unit diplomat query
**************************************************************************/
static void unit_query_impossible(struct connection *pc,
				  const int diplomat_id,
				  const int target_id)
{
  dsend_packet_unit_action_answer(pc,
                                  diplomat_id, target_id,
                                  0,
                                  ACTION_COUNT);
}

/**************************************************************************
  Tell the client the cost of bribing a unit, inciting a revolt, or
  any other parameters needed for action.

  Only send result back to the requesting connection, not all
  connections for that player.
**************************************************************************/
void handle_unit_action_query(struct connection *pc,
			      const int actor_id,
			      const int target_id,
			      const enum gen_action action_type)
{
  struct player *pplayer = pc->playing;
  struct unit *pactor = player_unit_by_number(pplayer, actor_id);
  struct unit *punit = game_unit_by_number(target_id);
  struct city *pcity = game_city_by_number(target_id);

  if (!action_id_is_valid(action_type)) {
    /* Non existing action */
    log_error("handle_unit_action_query() the action %d doesn't exist.",
              action_type);

    unit_query_impossible(pc, actor_id, target_id);
    return;
  }

  if (NULL == pactor) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_action_query() invalid actor %d",
                actor_id);
    unit_query_impossible(pc, actor_id, target_id);
    return;
  }

  if (!utype_may_act_at_all(unit_type_get(pactor))) {
    /* Shouldn't happen */
    log_error("handle_unit_action_query() %s (%d) is not an actor",
              unit_rule_name(pactor), actor_id);
    unit_query_impossible(pc, actor_id, target_id);
    return;
  }

  switch (action_type) {
  case ACTION_SPY_BRIBE_UNIT:
    if (punit) {
      if (is_action_enabled_unit_on_unit(action_type,
                                         pactor, punit)) {
        dsend_packet_unit_action_answer(pc,
                                        actor_id, target_id,
                                        unit_bribe_cost(punit, pplayer),
                                        action_type);
      } else {
        illegal_action(pplayer, pactor, action_type, unit_owner(punit),
                       NULL, NULL, punit, ACT_REQ_PLAYER);
        unit_query_impossible(pc, actor_id, target_id);
        return;
      }
    }
    break;
  case ACTION_SPY_INCITE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         pactor, pcity)) {
        dsend_packet_unit_action_answer(pc,
                                        actor_id, target_id,
                                        city_incite_cost(pplayer, pcity),
                                        action_type);
      } else {
        illegal_action(pplayer, pactor, action_type, city_owner(pcity),
                       NULL, pcity, NULL, ACT_REQ_PLAYER);
        unit_query_impossible(pc, actor_id, target_id);
        return;
      }
    }
    break;
  case ACTION_SPY_TARGETED_SABOTAGE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         pactor, pcity)) {
        spy_send_sabotage_list(pc, pactor, pcity);
      } else {
        illegal_action(pplayer, pactor, action_type, city_owner(pcity),
                       NULL, pcity, NULL, ACT_REQ_PLAYER);
        unit_query_impossible(pc, actor_id, target_id);
        return;
      }
    }
    break;
  default:
    unit_query_impossible(pc, actor_id, target_id);
    return;
  };
}

/**************************************************************************
  Handle a request to do an action.

  action_type must be a valid action.
**************************************************************************/
void handle_unit_do_action(struct player *pplayer,
			   const int actor_id,
			   const int target_id,
			   const int value,
                           const char *name,
                           const enum gen_action action_type)
{
  (void) unit_perform_action(pplayer, actor_id, target_id, value, name,
                             action_type, ACT_REQ_PLAYER);
}

/**************************************************************************
  Execute a request to perform an action and let the caller know if it was
  performed or not.

  The action must be a valid action.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
bool unit_perform_action(struct player *pplayer,
                         const int actor_id,
                         const int target_id,
                         const int value,
                         const char *name,
                         const enum gen_action action_type,
                         const enum action_requester requester)
{
  struct unit *actor_unit = player_unit_by_number(pplayer, actor_id);
  struct tile *target_tile = index_to_tile(target_id);
  struct unit *punit = game_unit_by_number(target_id);
  struct city *pcity = game_city_by_number(target_id);

  if (!action_id_is_valid(action_type)) {
    /* Non existing action */
    log_error("unit_perform_action() the action %d doesn't exist.",
              action_type);

    return FALSE;
  }

  if (NULL == actor_unit) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_do_action() invalid actor %d",
                actor_id);
    return FALSE;
  }

  if (!utype_may_act_at_all(unit_type_get(actor_unit))) {
    /* Shouldn't happen */
    log_error("handle_unit_do_action() %s (%d) is not an actor unit",
              unit_rule_name(actor_unit), actor_id);
    return FALSE;
  }

  if (!unit_can_do_action_now(actor_unit)) {
    /* Action not possible due to unitwaittime setting. */
    return FALSE;
  }

#define ACTION_STARTED_UNIT_CITY(action, actor, target)                   \
  script_server_signal_emit("action_started_unit_city", 3,                \
                            API_TYPE_ACTION, action_by_number(action),    \
                            API_TYPE_UNIT, actor,                         \
                            API_TYPE_CITY, target);                       \
  if (!actor || !unit_is_alive(actor_id)) {                               \
    /* Actor unit was destroyed during pre action Lua. */                 \
    return FALSE;                                                         \
  }                                                                       \
  if (!target || !city_exist(target_id)) {                                \
    /* Target city was destroyed during pre action Lua. */                \
    return FALSE;                                                         \
  }

#define ACTION_STARTED_UNIT_SELF(action, actor)                           \
  script_server_signal_emit("action_started_unit_self", 2,                \
                            API_TYPE_ACTION, action_by_number(action),    \
                            API_TYPE_UNIT, actor);                        \
  if (!actor || !unit_is_alive(actor_id)) {                               \
    /* Actor unit was destroyed during pre action Lua. */                 \
    return FALSE;                                                         \
  }

#define ACTION_STARTED_UNIT_UNIT(action, actor, target)                   \
  script_server_signal_emit("action_started_unit_unit", 3,                \
                            API_TYPE_ACTION, action_by_number(action),    \
                            API_TYPE_UNIT, actor,                         \
                            API_TYPE_UNIT, target);                       \
  if (!actor || !unit_is_alive(actor_id)) {                               \
    /* Actor unit was destroyed during pre action Lua. */                 \
    return FALSE;                                                         \
  }                                                                       \
  if (!target || !unit_is_alive(target_id)) {                             \
    /* Target unit was destroyed during pre action Lua. */                \
    return FALSE;                                                         \
  }

#define ACTION_STARTED_UNIT_UNITS(action, actor, target)                  \
  script_server_signal_emit("action_started_unit_units", 3,               \
                            API_TYPE_ACTION, action_by_number(action),    \
                            API_TYPE_UNIT, actor,                         \
                            API_TYPE_TILE, target);                       \
  if (!actor || !unit_is_alive(actor_id)) {                               \
    /* Actor unit was destroyed during pre action Lua. */                 \
    return FALSE;                                                         \
  }

#define ACTION_STARTED_UNIT_TILE(action, actor, target)                   \
  script_server_signal_emit("action_started_unit_tile", 3,                \
                            API_TYPE_ACTION, action_by_number(action),    \
                            API_TYPE_UNIT, actor,                         \
                            API_TYPE_TILE, target);                       \
  if (!actor || !unit_is_alive(actor_id)) {                               \
    /* Actor unit was destroyed during pre action Lua. */                 \
    return FALSE;                                                         \
  }

  switch(action_type) {
  case ACTION_SPY_BRIBE_UNIT:
    if (punit) {
      if (is_action_enabled_unit_on_unit(action_type,
                                         actor_unit, punit)) {
        ACTION_STARTED_UNIT_UNIT(action_type, actor_unit, punit);

        return diplomat_bribe(pplayer, actor_unit, punit);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       unit_owner(punit), NULL, NULL, punit,
                       requester);
      }
    }
    break;
  case ACTION_SPY_SABOTAGE_UNIT:
    if (punit) {
      if (is_action_enabled_unit_on_unit(action_type,
                                         actor_unit, punit)) {
        ACTION_STARTED_UNIT_UNIT(action_type, actor_unit, punit);

        return spy_sabotage_unit(pplayer, actor_unit, punit);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       unit_owner(punit), NULL, NULL, punit,
                       requester);
      }
    }
    break;
  case ACTION_EXPEL_UNIT:
    if (punit) {
      if (is_action_enabled_unit_on_unit(action_type,
                                         actor_unit, punit)) {
        ACTION_STARTED_UNIT_UNIT(action_type, actor_unit, punit);

        return do_expel_unit(pplayer, actor_unit, punit);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       unit_owner(punit), NULL, NULL, punit,
                       requester);
      }
    }
    break;
  case ACTION_DISBAND_UNIT:
    if (actor_unit) {
      if (is_action_enabled_unit_on_self(action_type,
                                         actor_unit)) {
        ACTION_STARTED_UNIT_SELF(action_type, actor_unit);

        return do_unit_disband(pplayer, actor_unit);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       unit_owner(actor_unit), NULL, NULL, actor_unit,
                       requester);
      }
    }
    break;
  case ACTION_SPY_SABOTAGE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type, actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return diplomat_sabotage(pplayer, actor_unit, pcity, B_LAST,
                                 action_type);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_TARGETED_SABOTAGE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type, actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        /* packet value is improvement ID + 1 (or some special codes) */
        return diplomat_sabotage(pplayer, actor_unit, pcity, value - 1,
                                 action_type);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_POISON:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return spy_poison(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_INVESTIGATE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return diplomat_investigate(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_ESTABLISH_EMBASSY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return diplomat_embassy(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_INCITE_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return diplomat_incite(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_STEAL_TECH:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        /* packet value is technology ID (or some special codes) */
        return diplomat_get_tech(pplayer, actor_unit, pcity, A_UNSET,
                                 action_type);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_TARGETED_STEAL_TECH:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        /* packet value is technology ID (or some special codes) */
        return diplomat_get_tech(pplayer, actor_unit, pcity, value,
                                 action_type);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_STEAL_GOLD:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return spy_steal_gold(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_STEAL_MAPS:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return spy_steal_some_maps(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_TRADE_ROUTE:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_unit_establish_trade(pplayer, actor_unit, pcity,
                                       TRUE);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_MARKETPLACE:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_unit_establish_trade(pplayer, actor_unit, pcity,
                                       FALSE);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_HELP_WONDER:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_unit_help_build_wonder(pplayer,
                                         actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_SPY_NUKE:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return spy_nuke_city(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_JOIN_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return city_add_unit(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_DESTROY_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return unit_do_destroy_city(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_RECYCLE_UNIT:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return unit_do_recycle(pplayer, actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_HOME_CITY:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_unit_change_homecity(actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_UPGRADE_UNIT:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_unit_upgrade(pplayer, actor_unit, pcity,
                               requester);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_AIRLIFT:
    if (pcity) {
      if (is_action_enabled_unit_on_city(action_type,
                                         actor_unit, pcity)) {
        ACTION_STARTED_UNIT_CITY(action_type, actor_unit, pcity);

        return do_airline(actor_unit, pcity);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       city_owner(pcity), NULL, pcity, NULL,
                       requester);
      }
    }
    break;
  case ACTION_CAPTURE_UNITS:
    if (target_tile) {
      if (is_action_enabled_unit_on_units(action_type,
                                          actor_unit, target_tile)) {
        ACTION_STARTED_UNIT_UNITS(action_type, actor_unit, target_tile);

        return do_capture_units(pplayer, actor_unit, target_tile);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       NULL, target_tile, NULL, NULL,
                       requester);
      }
    }
    break;
  case ACTION_BOMBARD:
    if (target_tile) {
      if (is_action_enabled_unit_on_units(action_type,
                                          actor_unit, target_tile)) {
        ACTION_STARTED_UNIT_UNITS(action_type, actor_unit, target_tile);

        return unit_bombard(actor_unit, target_tile);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       NULL, target_tile, NULL, NULL,
                       requester);
      }
    }
    break;
  case ACTION_FOUND_CITY:
    if (target_tile) {
      if (is_action_enabled_unit_on_tile(action_type,
                                          actor_unit, target_tile)) {
        ACTION_STARTED_UNIT_TILE(action_type, actor_unit, target_tile);

        return city_build(pplayer, actor_unit, target_tile, name);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       NULL, target_tile, NULL, NULL,
                       requester);
      }
    }
    break;
  case ACTION_NUKE:
    if (target_tile) {
      if (is_action_enabled_unit_on_tile(action_type,
                                          actor_unit, target_tile)) {
        ACTION_STARTED_UNIT_TILE(action_type, actor_unit, target_tile);

        return unit_nuke(pplayer, actor_unit, target_tile);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       NULL, target_tile, NULL, NULL,
                       requester);
      }
    }
    break;
  case ACTION_PARADROP:
    if (target_tile) {
      if (is_action_enabled_unit_on_tile(action_type,
                                         actor_unit, target_tile)) {
        ACTION_STARTED_UNIT_TILE(action_type, actor_unit, target_tile);

        return do_paradrop(actor_unit, target_tile);
      } else {
        illegal_action(pplayer, actor_unit, action_type,
                       NULL, target_tile, NULL, NULL,
                       requester);
      }
    }
    break;
  case ACTION_COUNT:
    log_error("handle_unit_do_action() %s (%d) ordered to perform an "
              "invalid action.",
              unit_rule_name(actor_unit), actor_id);
    break;
  }

  /* Something must have gone wrong. */
  return FALSE;
}

/**************************************************************************
  Transfer a unit from one homecity to another. This new homecity must
  be valid for this unit.
**************************************************************************/
void unit_change_homecity_handling(struct unit *punit, struct city *new_pcity)
{
  struct city *old_pcity = game_city_by_number(punit->homecity);
  struct player *old_owner = unit_owner(punit);
  struct player *new_owner = city_owner(new_pcity);

  /* Calling this function when new_pcity is same as old_pcity should
   * be safe with current implementation, but it is not meant to
   * be used that way. */
  fc_assert_ret(new_pcity != old_pcity);

  if (old_owner != new_owner) {
    struct city *pcity = tile_city(punit->tile);

    fc_assert(!utype_player_already_has_this_unique(new_owner,
                                                    unit_type_get(punit)));

    vision_clear_sight(punit->server.vision);
    vision_free(punit->server.vision);

    if (pcity != NULL
        && !can_player_see_units_in_city(old_owner, pcity)) {
      /* Special case when city is being transfered. At this point city
       * itself has changed owner, so it's enemy city now that old owner
       * cannot see inside. All the normal methods of removing transfered
       * unit from previous owner's client think that there's no need to
       * remove unit as client should't have it in first place. */
      unit_goes_out_of_sight(old_owner, punit);
    }

    /* Remove AI control of the old owner. */
    CALL_PLR_AI_FUNC(unit_lost, old_owner, punit);

    unit_list_remove(old_owner->units, punit);
    unit_list_prepend(new_owner->units, punit);
    punit->owner = new_owner;

    /* Activate AI control of the new owner. */
    CALL_PLR_AI_FUNC(unit_got, new_owner, punit);

    punit->server.vision = vision_new(new_owner, unit_tile(punit));
    unit_refresh_vision(punit);
  }

  /* Remove from old city first and add to new city only after that.
   * This is more robust in case old_city == new_city (currently
   * prohibited by fc_assert in the beginning of the function).
   */
  if (old_pcity) {
    /* Even if unit is dead, we have to unlink unit pointer (punit). */
    unit_list_remove(old_pcity->units_supported, punit);
    /* update unit upkeep */
    city_units_upkeep(old_pcity);
  }

  unit_list_prepend(new_pcity->units_supported, punit);

  /* update unit upkeep */
  city_units_upkeep(new_pcity);

  punit->homecity = new_pcity->id;

  if (!can_unit_continue_current_activity(punit)) {
    /* This is mainly for cases where unit owner changes to one not knowing
     * Railroad tech when unit is already building railroad. */
    set_unit_activity(punit, ACTIVITY_IDLE);
  }

  /* Send info to players and observers. */
  send_unit_info(NULL, punit);

  city_refresh(new_pcity);
  send_city_info(new_owner, new_pcity);

  if (old_pcity) {
    fc_assert(city_owner(old_pcity) == old_owner);
    city_refresh(old_pcity);
    send_city_info(old_owner, old_pcity);
  }

  fc_assert(unit_owner(punit) == city_owner(new_pcity));
}

/**************************************************************************
  Change a unit's home city.

  Returns TRUE iff the action could be done, FALSE if it couldn't.
**************************************************************************/
static bool do_unit_change_homecity(struct unit *punit,
                                    struct city *pcity)
{
  unit_change_homecity_handling(punit, pcity);

  return punit->homecity == pcity->id;
}

/**************************************************************************
  Disband a unit.

  No shields spent to build the unit is added to the shield stock of any
  city even if the unit is located inside it.

  Returns TRUE iff the action could be done, FALSE if it couldn't. Even if
  this returns TRUE, the unit may have died during the action.
**************************************************************************/
static bool do_unit_disband(struct player *pplayer, struct unit *punit)
{
  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  wipe_unit(punit, ULR_DISBANDED, NULL);

  /* The unit is now disbanded. */
  return TRUE;
}

/**************************************************************************
  Recycle a unit in a city.

  1/2 of the shields used to build the unit is added to the city's shield
  stock for the current production.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool unit_do_recycle(struct player *pplayer,
                            struct unit *punit,
                            struct city *pcity)
{
  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  /* Sanity check: The target city still exists. */
  fc_assert_ret_val(pcity, FALSE);

  /* Add the shields from recycling the unit to the city's current
   * production. */
  pcity->shield_stock += unit_disband_shields(punit);

  if (unit_can_do_action(punit, ACTION_HELP_WONDER)) {
    /* Count this just like a caravan that was added to a wonder.
     * However don't actually give the city the extra shields. Switching
     * to a wonder later in the turn will give the extra shields back. */
    pcity->caravan_shields += unit_build_shield_cost(punit);
  } else {
    /* If we change production later at this turn. No penalty is added. */
    pcity->disbanded_shields += unit_disband_shields(punit);
  }

  notify_player(pplayer, city_tile(pcity), E_CARAVAN_ACTION, ftc_server,
                /* TRANS: ... Ironclad ... New York */
                _("Recyled your %s to help the current production in %s."),
                unit_link(punit),
                city_link(pcity));

  send_city_info(city_owner(pcity), pcity);

  wipe_unit(punit, ULR_DISBANDED, NULL);

  /* The unit is now recycled. */
  return TRUE;
}

/**************************************************************************
  This function assumes that the target city is valid. It should only be
  called after checking that the unit legally can join the target city.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool city_add_unit(struct player *pplayer, struct unit *punit,
                          struct city *pcity)
{
  int amount = unit_pop_value(punit);

  /* Sanity check: The actor is still alive. */
  fc_assert_ret_val(punit, FALSE);

  /* Sanity check: The target city still exists. */
  fc_assert_ret_val(pcity, FALSE);

  fc_assert_ret_val(amount > 0, FALSE);

  script_server_signal_emit("city_size_change", 3,
                            API_TYPE_CITY, pcity,
                            API_TYPE_INT, amount,
                            API_TYPE_STRING, "unit_added");
  city_size_add(pcity, amount);
  /* Make the new people something, otherwise city fails the checks */
  pcity->specialists[DEFAULT_SPECIALIST] += amount;
  citizens_update(pcity, unit_nationality(punit));
  /* Refresh the city data. */
  city_refresh(pcity);

  /* Notify the unit owner that the unit successfully joined the city. */
  notify_player(pplayer, city_tile(pcity), E_CITY_BUILD, ftc_server,
                _("%s added to aid %s in growing."),
                unit_tile_link(punit),
                city_link(pcity));
  if (pplayer != city_owner(pcity)) {
    /* Notify the city owner when a foreign unit joins a city. */
    notify_player(city_owner(pcity), city_tile(pcity), E_CITY_BUILD,
                  ftc_server,
                  /* TRANS: another player had his unit joint your city. */
                  _("%s adds %s to your city %s."),
                  player_name(unit_owner(punit)),
                  unit_tile_link(punit),
                  city_link(pcity));;
  }

  action_consequence_success(ACTION_JOIN_CITY, pplayer, city_owner(pcity),
                             city_tile(pcity), city_link(pcity));
  wipe_unit(punit, ULR_USED, NULL);

  sanity_check_city(pcity);

  send_city_info(NULL, pcity);

  return TRUE;
}

/**************************************************************************
  This function assumes a certain level of consistency checking: There
  is no city under punit->(x,y), and that location is a valid one on
  which to build a city. It should only be called after a call to a
  function like test_unit_add_or_build_city, which does the checking.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool city_build(struct player *pplayer, struct unit *punit,
                       struct tile *ptile, const char *name)
{
  char message[1024];
  int size;
  struct player *nationality;
  struct player *towner;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  towner = tile_owner(ptile);

  if (!is_allowed_city_name(pplayer, name, message, sizeof(message))) {
    notify_player(pplayer, ptile, E_BAD_COMMAND, ftc_server,
                  "%s", message);
    return FALSE;
  }

  nationality = unit_nationality(punit);

  create_city(pplayer, ptile, name, nationality);
  size = unit_type_get(punit)->city_size;
  if (size > 1) {
    struct city *pcity = tile_city(ptile);

    fc_assert_ret_val(pcity != NULL, FALSE);

    city_change_size(pcity, size, nationality, NULL);
  }
  wipe_unit(punit, ULR_USED, NULL);

  /* May cause an incident even if the target tile is unclaimed. A ruleset
   * could give everyone a casus belli against the city founder. A rule
   * like that would make sense in a story where deep ecology is on the
   * table. (See also Voluntary Human Extinction Movement) */
  action_consequence_success(ACTION_FOUND_CITY, pplayer, towner,
                             ptile, tile_link(ptile));

  return TRUE;
}

/**************************************************************************
  Handle change in unit activity.
**************************************************************************/
static void handle_unit_change_activity_real(struct player *pplayer,
                                             int unit_id,
                                             enum unit_activity activity,
                                             struct extra_type *activity_target)
{
  struct unit *punit = player_unit_by_number(pplayer, unit_id);

  if (NULL == punit) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_change_activity() invalid unit %d", unit_id);
    return;
  }

  if (punit->activity == activity
      && punit->activity_target == activity_target
      && !punit->ai_controlled) {
    /* Treat change in ai.control as change in activity, so
     * idle autosettlers behave correctly when selected --dwp
     */
    return;
  }

  /* Remove city spot reservations for AI settlers on city founding
   * mission, before goto_tile reset. */
  if (punit->server.adv->task != AUT_NONE) {
    adv_unit_new_task(punit, AUT_NONE, NULL);
  }

  punit->ai_controlled = FALSE;
  punit->goto_tile = NULL;

  if (activity == ACTIVITY_GOTO) {
    /* Don't permit a client to set a unit's activity to ACTIVITY_GOTO.
     * Setting ACTIVITY_GOTO from the client results in a unit indicating
     * it is going somewhere while it is standing still. The appearance of
     * the unit doing something can trick the user to not make use of it.
     *
     * Handled here because adv_follow_path() uses unit_activity_handling()
     * to set a unit's activity to ACTIVITY_GOTO. */
    return;
  }

  /* The activity can now be set. */
  unit_activity_handling_targeted(punit, activity, &activity_target);

  if (activity == ACTIVITY_EXPLORE) {
    /* Exploring is handled here explicitly, since the player expects to
     * see an immediate response from setting a unit to auto-explore.
     * Handling it deeper in the code leads to some tricky recursive loops -
     * see PR#2631. */
    if (punit->moves_left > 0) {
      do_explore(punit);
    }
  }
}

/**************************************************************************
  Handle change in unit activity.
**************************************************************************/
void handle_unit_change_activity(struct player *pplayer, int unit_id,
                                 enum unit_activity activity,
                                 int target_id)
{
  struct extra_type *activity_target;

  if (target_id < 0 || target_id >= game.control.num_extra_types) {
    activity_target = NULL;
  } else {
    activity_target = extra_by_number(target_id);
  }

#ifdef FREECIV_WEB
  /* Web-client is not capable of selecting target, so we do it server side */
  if (activity_target == NULL) {
    struct unit *punit = player_unit_by_number(pplayer, unit_id);
    bool required = TRUE;

    if (punit == NULL) {
      return;
    }

    if (activity == ACTIVITY_IRRIGATE) {
      struct tile *ptile = unit_tile(punit);
      struct terrain *pterrain = tile_terrain(ptile);

      if (pterrain->irrigation_result != pterrain) {
        required = FALSE;
      } else {
        activity_target = next_extra_for_tile(ptile, EC_IRRIGATION,
                                              pplayer, punit);
      }
    } else if (activity == ACTIVITY_MINE) {
      struct tile *ptile = unit_tile(punit);
      struct terrain *pterrain = tile_terrain(ptile);

      if (pterrain->mining_result != pterrain) {
        required = FALSE;
      } else {
        activity_target = next_extra_for_tile(ptile, EC_MINE,
                                              pplayer, punit);
      }
    } else if (activity == ACTIVITY_BASE) {
      struct tile *ptile = unit_tile(punit);
      struct base_type *pbase =
        get_base_by_gui_type(BASE_GUI_FORTRESS, punit, ptile);

      if (pbase != NULL) {
        activity_target = base_extra_get(pbase);
      }

    } else if (activity == ACTIVITY_POLLUTION) {
      activity_target = prev_extra_in_tile(unit_tile(punit), ERM_CLEANPOLLUTION,
                                           pplayer, punit);
    } else if (activity == ACTIVITY_FALLOUT) {
      activity_target = prev_extra_in_tile(unit_tile(punit), ERM_CLEANFALLOUT,
                                           pplayer, punit);
    } else {
      required = FALSE;
    }

    if (activity_target == NULL && required) {
      /* Nothing more we can do */
      return;
    }
  }
#endif /* FREECIV_WEB */

  handle_unit_change_activity_real(pplayer, unit_id, activity, activity_target);
}

/**************************************************************************
 Make sure everyone who can see combat does.
**************************************************************************/
static void see_combat(struct unit *pattacker, struct unit *pdefender)
{
  struct packet_unit_short_info unit_att_short_packet, unit_def_short_packet;
  struct packet_unit_info unit_att_packet, unit_def_packet;

  /* 
   * Special case for attacking/defending:
   * 
   * Normally the player doesn't get the information about the units inside a
   * city. However for attacking/defending the player has to know the unit of
   * the other side.  After the combat a remove_unit packet will be sent
   * to the client to tidy up.
   *
   * Note these packets must be sent out before unit_versus_unit is called,
   * so that the original unit stats (HP) will be sent.
   */
  package_short_unit(pattacker, &unit_att_short_packet,
                     UNIT_INFO_IDENTITY, 0);
  package_short_unit(pdefender, &unit_def_short_packet,
                     UNIT_INFO_IDENTITY, 0);
  package_unit(pattacker, &unit_att_packet);
  package_unit(pdefender, &unit_def_packet);

  conn_list_iterate(game.est_connections, pconn) {
    struct player *pplayer = pconn->playing;

    if (pplayer != NULL) {

      /* NOTE: this means the player can see combat between submarines even
       * if neither sub is visible.  See similar comment in send_combat. */
      if (map_is_known_and_seen(unit_tile(pattacker), pplayer, V_MAIN)
          || map_is_known_and_seen(unit_tile(pdefender), pplayer,
                                   V_MAIN)) {

        /* Units are sent even if they were visible already. They may
         * have changed orientation for combat. */
        if (pplayer == unit_owner(pattacker)) {
          send_packet_unit_info(pconn, &unit_att_packet);
        } else {
          send_packet_unit_short_info(pconn, &unit_att_short_packet, FALSE);
        }
        
        if (pplayer == unit_owner(pdefender)) {
          send_packet_unit_info(pconn, &unit_def_packet);
        } else {
          send_packet_unit_short_info(pconn, &unit_def_short_packet, FALSE);
        }
      }
    } else if (pconn->observer) {
      /* Global observer sees everything... */
      send_packet_unit_info(pconn, &unit_att_packet);
      send_packet_unit_info(pconn, &unit_def_packet);
    }
  } conn_list_iterate_end;
}

/**************************************************************************
 Send combat info to players.
**************************************************************************/
static void send_combat(struct unit *pattacker, struct unit *pdefender, 
			int veteran, int bombard)
{
  struct packet_unit_combat_info combat;

  combat.attacker_unit_id=pattacker->id;
  combat.defender_unit_id=pdefender->id;
  combat.attacker_hp=pattacker->hp;
  combat.defender_hp=pdefender->hp;
  combat.make_winner_veteran=veteran;

  players_iterate(other_player) {
    /* NOTE: this means the player can see combat between submarines even
     * if neither sub is visible.  See similar comment in see_combat. */
    if (map_is_known_and_seen(unit_tile(pattacker), other_player, V_MAIN)
        || map_is_known_and_seen(unit_tile(pdefender), other_player,
                                 V_MAIN)) {
      lsend_packet_unit_combat_info(other_player->connections, &combat);

      /* 
       * Remove the client knowledge of the units.  This corresponds to the
       * send_packet_unit_short_info calls up above.
       */
      if (!can_player_see_unit(other_player, pattacker)) {
	unit_goes_out_of_sight(other_player, pattacker);
      }
      if (!can_player_see_unit(other_player, pdefender)) {
	unit_goes_out_of_sight(other_player, pdefender);
      }
    }
  } players_iterate_end;

  /* Send combat info to non-player observers as well.  They already know
   * about the unit so no unit_info is needed. */
  conn_list_iterate(game.est_connections, pconn) {
    if (NULL == pconn->playing && pconn->observer) {
      send_packet_unit_combat_info(pconn, &combat);
    }
  } conn_list_iterate_end;
}

/**************************************************************************
  This function assumes the bombard is legal. The calling function should
  have already made all necessary checks.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool unit_bombard(struct unit *punit, struct tile *ptile)
{
  struct player *pplayer = unit_owner(punit);
  struct city *pcity = tile_city(ptile);

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  log_debug("Start bombard: %s %s to %d, %d.",
            nation_rule_name(nation_of_player(pplayer)),
            unit_rule_name(punit), TILE_XY(ptile));

  unit_list_iterate_safe(ptile->units, pdefender) {

    /* Sanity checks */
    fc_assert_ret_val_msg(!pplayers_non_attack(unit_owner(punit),
                                               unit_owner(pdefender)),
                          FALSE,
                          "Trying to attack a unit with which you have "
                          "peace or cease-fire at (%d, %d).",
                          TILE_XY(unit_tile(pdefender)));
    fc_assert_ret_val_msg(!pplayers_allied(unit_owner(punit),
                                           unit_owner(pdefender)),
                          FALSE,
                          "Trying to attack a unit with which you have "
                          "alliance at (%d, %d).",
                          TILE_XY(unit_tile(pdefender)));

    if (is_unit_reachable_at(pdefender, punit, ptile)) {
      bool adj;
      enum direction8 facing;
      int att_hp, def_hp;

      adj = base_get_direction_for_step(punit->tile, pdefender->tile, &facing);

      fc_assert(adj);
      if (adj) {
        punit->facing = facing;

        /* Unlike with normal attack, we don't change orientation of
         * defenders when bombarding */
      }

      unit_versus_unit(punit, pdefender, TRUE, &att_hp, &def_hp);

      see_combat(punit, pdefender);

      punit->hp = att_hp;
      pdefender->hp = def_hp;

      send_combat(punit, pdefender, 0, 1);
  
      send_unit_info(NULL, pdefender);

      /* May cause an incident */
      action_consequence_success(ACTION_BOMBARD, unit_owner(punit),
                                 unit_owner(pdefender),
                                 unit_tile(pdefender),
                                 unit_link(pdefender));
    }

  } unit_list_iterate_safe_end;

  punit->moves_left = 0;

  unit_did_action(punit);
  unit_forget_last_activity(punit);
  
  if (pcity
      && city_size_get(pcity) > 1
      && get_city_bonus(pcity, EFT_UNIT_NO_LOSE_POP) <= 0
      && kills_citizen_after_attack(punit)) {
    city_reduce_size(pcity, 1, pplayer, "bombard");
    city_refresh(pcity);
    send_city_info(NULL, pcity);
  }

  send_unit_info(NULL, punit);

  return TRUE;
}

/**************************************************************************
  Do a "regular" nuclear attack.

  Can be stopped by an EFT_NUKE_PROOF (SDI defended) city.

  This function assumes the attack is legal. The calling function should
  have already made all necessary checks.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool unit_nuke(struct player *pplayer, struct unit *punit,
                      struct tile *def_tile)
{
  struct city *pcity;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  log_debug("Start nuclear attack: %s %s against (%d, %d).",
            nation_rule_name(nation_of_player(pplayer)),
            unit_rule_name(punit),
            TILE_XY(def_tile));

  if ((pcity = sdi_try_defend(pplayer, def_tile))) {
    /* FIXME: Remove the hard coded reference to SDI defense. */
    notify_player(pplayer, unit_tile(punit), E_UNIT_LOST_ATT, ftc_server,
                  _("Your %s was shot down by "
                    "SDI defenses, what a waste."), unit_tile_link(punit));
    notify_player(city_owner(pcity), def_tile, E_UNIT_WIN, ftc_server,
                  _("The nuclear attack on %s was avoided by"
                    " your SDI defense."), city_link(pcity));

    /* Trying to nuke something this close can be... unpopular. */
    action_consequence_caught(ACTION_NUKE, pplayer,
                              city_owner(pcity),
                              def_tile, unit_tile_link(punit));

    /* Remove the destroyed nuke. */
    wipe_unit(punit, ULR_SDI, city_owner(pcity));

    return FALSE;
  }

  dlsend_packet_nuke_tile_info(game.est_connections, tile_index(def_tile));

  wipe_unit(punit, ULR_DETONATED, NULL);
  do_nuclear_explosion(pplayer, def_tile);

  /* May cause an incident even if the target tile is unclaimed. A ruleset
   * could give everyone a casus belli against the tile nuker. A rule
   * like that would make sense in a story where detonating any nuke at all
   * could be forbidden. */
  action_consequence_success(ACTION_NUKE, pplayer,
                             tile_owner(def_tile),
                             def_tile,
                             tile_link(def_tile));

  return TRUE;
}

/**************************************************************************
  Destroy the target city.

  This function assumes the destruction is legal. The calling function
  should have already made all necessary checks.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool unit_do_destroy_city(struct player *act_player,
                                 struct unit *act_unit,
                                 struct city *tgt_city)
{
  int tgt_city_id;
  struct player *tgt_player;
  bool try_civil_war = FALSE;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(act_player, FALSE);
  fc_assert_ret_val(act_unit, FALSE);

  /* Sanity check: The target city still exists. */
  fc_assert_ret_val(tgt_city, FALSE);

  tgt_player = city_owner(tgt_city);

  /* How can a city be ownerless? */
  fc_assert_ret_val(tgt_player, FALSE);

  /* Save city ID. */
  tgt_city_id = tgt_city->id;

  if (is_capital(tgt_city)
      && (tgt_player->spaceship.state == SSHIP_STARTED
          || tgt_player->spaceship.state == SSHIP_LAUNCHED)) {
    /* Destroying this city destroys the victim's space ship. */
    spaceship_lost(tgt_player);
  }

  if (is_capital(tgt_city)
      && civil_war_possible(tgt_player, TRUE, TRUE)
      && normal_player_count() < MAX_NUM_PLAYERS
      && civil_war_triggered(tgt_player)) {
    /* Destroying this city can trigger a civil war. */
    try_civil_war = TRUE;
  }

  /* Let the actor know. */
  notify_player(act_player, city_tile(tgt_city),
                E_UNIT_WIN, ftc_server,
                _("You destroy %s completely."),
                city_tile_link(tgt_city));

  if (tgt_player != act_player) {
    /* This was done to a foreign city. Inform the victim player. */
    notify_player(tgt_player, city_tile(tgt_city),
                  E_CITY_LOST, ftc_server,
                  _("%s has been destroyed by %s."),
                  city_tile_link(tgt_city),
                  player_name(act_player));
  }

  /* May cause an incident */
  action_consequence_success(ACTION_DESTROY_CITY, act_player,
                             tgt_player, city_tile(tgt_city),
                             city_link(tgt_city));

  /* Run post city destruction Lua script. */
  script_server_signal_emit("city_destroyed", 3,
                            API_TYPE_CITY, tgt_city,
                            API_TYPE_PLAYER, tgt_player,
                            API_TYPE_PLAYER, act_player);

  /* Can't be sure of city existence after running script. */
  if (city_exist(tgt_city_id)) {
    remove_city(tgt_city);
  }

  if (try_civil_war) {
    /* Try to start the civil war. */
    (void) civil_war(tgt_player);
  }

  /* The city is no more. */
  return TRUE;
}

/**************************************************************************
This function assumes the attack is legal. The calling function should have
already made all necessary checks.
**************************************************************************/
static void unit_attack_handling(struct unit *punit, struct unit *pdefender)
{
  char loser_link[MAX_LEN_LINK], winner_link[MAX_LEN_LINK];
  struct unit *ploser, *pwinner;
  struct city *pcity;
  int moves_used, def_moves_used; 
  int old_unit_vet, old_defender_vet, vet;
  int winner_id;
  struct tile *def_tile = unit_tile(pdefender);
  struct player *pplayer = unit_owner(punit);
  bool adj;
  enum direction8 facing;
  int att_hp, def_hp;
  
  log_debug("Start attack: %s %s against %s %s.",
            nation_rule_name(nation_of_player(pplayer)),
            unit_rule_name(punit), 
            nation_rule_name(nation_of_unit(pdefender)),
            unit_rule_name(pdefender));

  /* Sanity checks */
  fc_assert_ret_msg(!pplayers_non_attack(pplayer, unit_owner(pdefender)),
                    "Trying to attack a unit with which you have peace "
                    "or cease-fire at (%d, %d).", TILE_XY(def_tile));
  fc_assert_ret_msg(!pplayers_allied(pplayer, unit_owner(pdefender)),
                    "Trying to attack a unit with which you have alliance "
                    "at (%d, %d).", TILE_XY(def_tile));

  moves_used = unit_move_rate(punit) - punit->moves_left;
  def_moves_used = unit_move_rate(pdefender) - pdefender->moves_left;

  adj = base_get_direction_for_step(punit->tile, pdefender->tile, &facing);

  fc_assert(adj);
  if (adj) {
    punit->facing = facing;
    pdefender->facing = opposite_direction(facing);
  }

  old_unit_vet = punit->veteran;
  old_defender_vet = pdefender->veteran;
  unit_versus_unit(punit, pdefender, FALSE, &att_hp, &def_hp);

  if ((att_hp <= 0 || uclass_has_flag(unit_class_get(punit), UCF_MISSILE))
      && unit_transported(punit)) {
    /* Dying attacker must be first unloaded so it doesn't die insider transport */
    unit_transport_unload_send(punit);
  }

  see_combat(punit, pdefender);

  punit->hp = att_hp;
  pdefender->hp = def_hp;

  combat_veterans(punit, pdefender);

  /* Adjust attackers moves_left _after_ unit_versus_unit() so that
   * the movement attack modifier is correct! --dwp
   *
   * For greater Civ2 compatibility (and game balance issues), we recompute 
   * the new total MP based on the HP the unit has left after being damaged, 
   * and subtract the MPs that had been used before the combat (plus the 
   * points used in the attack itself, for the attacker). -GJW, Glip
   */
  punit->moves_left = unit_move_rate(punit) - moves_used - SINGLE_MOVE;
  pdefender->moves_left = unit_move_rate(pdefender) - def_moves_used;
  
  if (punit->moves_left < 0) {
    punit->moves_left = 0;
  }
  if (pdefender->moves_left < 0) {
    pdefender->moves_left = 0;
  }
  unit_did_action(punit);
  unit_forget_last_activity(punit);

  if (punit->hp > 0
      && (pcity = tile_city(def_tile))
      && city_size_get(pcity) > 1
      && get_city_bonus(pcity, EFT_UNIT_NO_LOSE_POP) <= 0
      && kills_citizen_after_attack(punit)) {
    city_reduce_size(pcity, 1, pplayer, "attack");
    city_refresh(pcity);
    send_city_info(NULL, pcity);
  }
  if (unit_has_type_flag(punit, UTYF_ONEATTACK)) {
    punit->moves_left = 0;
  }
  pwinner = (punit->hp > 0) ? punit : pdefender;
  winner_id = pwinner->id;
  ploser = (pdefender->hp > 0) ? punit : pdefender;

  vet = (pwinner->veteran == ((punit->hp > 0) ? old_unit_vet :
	old_defender_vet)) ? 0 : 1;

  send_combat(punit, pdefender, vet, 0);

  /* N.B.: unit_link always returns the same pointer. */
  sz_strlcpy(loser_link, unit_tile_link(ploser));
  sz_strlcpy(winner_link, uclass_has_flag(unit_class_get(pwinner), UCF_MISSILE)
             ? unit_tile_link(pwinner) : unit_link(pwinner));

  if (punit == ploser) {
    /* The attacker lost */
    log_debug("Attacker lost: %s %s against %s %s.",
              nation_rule_name(nation_of_player(pplayer)),
              unit_rule_name(punit),
              nation_rule_name(nation_of_unit(pdefender)),
              unit_rule_name(pdefender));

    notify_player(unit_owner(pwinner), unit_tile(pwinner),
                  E_UNIT_WIN, ftc_server,
                  /* TRANS: "Your Cannon ... the Polish Destroyer." */
                  _("Your %s survived the pathetic attack from the %s %s."),
                  winner_link,
                  nation_adjective_for_player(unit_owner(ploser)),
                  loser_link);
    if (vet) {
      notify_unit_experience(pwinner);
    }
    notify_player(unit_owner(ploser), def_tile,
                  E_UNIT_LOST_ATT, ftc_server,
                  /* TRANS: "... Cannon ... the Polish Destroyer." */
                  _("Your attacking %s failed against the %s %s!"),
                  loser_link,
                  nation_adjective_for_player(unit_owner(pwinner)),
                  winner_link);
    wipe_unit(ploser, ULR_KILLED, unit_owner(pwinner));
  } else {
    /* The defender lost, the attacker punit lives! */

    log_debug("Defender lost: %s %s against %s %s.",
              nation_rule_name(nation_of_player(pplayer)),
              unit_rule_name(punit),
              nation_rule_name(nation_of_unit(pdefender)),
              unit_rule_name(pdefender));

    punit->moved = TRUE;	/* We moved */
    kill_unit(pwinner, ploser,
              vet && !uclass_has_flag(unit_class_get(punit), UCF_MISSILE));
    if (unit_is_alive(winner_id)) {
      if (uclass_has_flag(unit_class_get(pwinner), UCF_MISSILE)) {
        wipe_unit(pwinner, ULR_MISSILE, NULL);
        return;
      }
    } else {
      return;
    }
  }

  /* If attacker wins, and occupychance > 0, it might move in.  Don't move in
   * if there are enemy units in the tile (a fortress, city or air base with
   * multiple defenders and unstacked combat). Note that this could mean 
   * capturing (or destroying) a city. */

  if (pwinner == punit && fc_rand(100) < game.server.occupychance &&
      !is_non_allied_unit_tile(def_tile, pplayer)) {

    /* Hack: make sure the unit has enough moves_left for the move to succeed,
       and adjust moves_left to afterward (if successful). */

    int old_moves = punit->moves_left;
    int full_moves = unit_move_rate(punit);

    punit->moves_left = full_moves;
    if (unit_move_handling(punit, def_tile, FALSE, FALSE, NULL)) {
      punit->moves_left = old_moves - (full_moves - punit->moves_left);
      if (punit->moves_left < 0) {
	punit->moves_left = 0;
      }
    } else {
      punit->moves_left = old_moves;
    }
  }

  /* The attacker may have died for many reasons */
  if (game_unit_by_number(winner_id) != NULL) {
    send_unit_info(NULL, pwinner);
  }
}

/**************************************************************************
  see also aiunit could_unit_move_to_tile()
**************************************************************************/
static bool can_unit_move_to_tile_with_notify(struct unit *punit,
					      struct tile *dest_tile,
					      bool igzoc,
                                              struct unit *embark_to)
{
  struct tile *src_tile = unit_tile(punit);
  enum unit_move_result reason =
      unit_move_to_tile_test(punit, punit->activity,
                             src_tile, dest_tile, igzoc, embark_to);

  switch (reason) {
  case MR_OK:
    return TRUE;

  case MR_BAD_TYPE_FOR_CITY_TAKE_OVER:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("This type of troops cannot take over a city."));
    break;

  case MR_BAD_TYPE_FOR_CITY_TAKE_OVER_FROM_NON_NATIVE:
    {
      const char *types[utype_count()];
      int i = 0;

      unit_type_iterate(utype) {
        if (can_attack_from_non_native(utype)
            && utype_can_take_over(utype)) {
          types[i++] = utype_name_translation(utype);
        }
      } unit_type_iterate_end;

      if (0 < i) {
        struct astring astr = ASTRING_INIT;

        notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                      /* TRANS: %s is a list of units separated by "or". */
                      _("Only %s can conquer from a non-native tile."),
                      astr_build_or_list(&astr, types, i));
        astr_free(&astr);
      } else {
        notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                      _("Cannot conquer from a non-native tile."));
      }
    }
    break;

  case MR_NO_WAR:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("Cannot attack unless you declare war first."));
    break;

  case MR_ZOC:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("%s can only move into your own zone of control."),
                  unit_link(punit));
    break;

  case MR_TRIREME:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("%s cannot move that far from the coast line."),
                  unit_link(punit));
    break;

  case MR_PEACE:
    if (tile_owner(dest_tile)) {
      notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                    _("Cannot invade unless you break peace with "
                      "%s first."),
                    player_name(tile_owner(dest_tile)));
    }
    break;

  case MR_CANNOT_DISEMBARK:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("%s cannot disembark outside of a city or a native base "
                    "for %s."),
                  unit_link(punit),
                  utype_name_translation(
                      unit_type_get(unit_transport_get(punit))));
    break;

  case MR_NON_NATIVE_MOVE:
    notify_player(unit_owner(punit), src_tile, E_BAD_COMMAND, ftc_server,
                  _("Terrain is unsuitable for %s units."),
                  uclass_name_translation(unit_class_get(punit)));
    break;

  default:
    /* FIXME: need more explanations someday! */
    break;
  };

  return FALSE;
}

/**************************************************************************
  Will try to move to/attack the tile dest_x,dest_y.  Returns TRUE if this
  was done, FALSE if it wasn't for some reason. Even if this returns TRUE,
  the unit may have died upon arrival to new tile.

  'igzoc' means ignore ZOC rules - not necessary for igzoc units etc, but
  done in some special cases (moving barbarians out of initial hut).
  Should normally be FALSE.

  'move_diplomat_city' is another special case which should normally be
  FALSE.  If TRUE, try to move diplomat (or spy) into city (should be
  allied) instead of telling client to popup diplomat/spy dialog.

  FIXME: This function needs a good cleaning.
**************************************************************************/
bool unit_move_handling(struct unit *punit, struct tile *pdesttile,
                        bool igzoc, bool move_diplomat_city,
                        struct unit *embark_to)
{
  struct player *pplayer = unit_owner(punit);
  struct city *pcity = tile_city(pdesttile);
  bool taking_over_city = FALSE;

  /*** Phase 1: Basic checks ***/

  /* this occurs often during lag, and to the AI due to some quirks -- Syela */
  if (!is_tiles_adjacent(unit_tile(punit), pdesttile)) {
    log_debug("tiles not adjacent in move request");
    return FALSE;
  }


  if (punit->moves_left <= 0) {
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("This unit has no moves left."));
    return FALSE;
  }

  if (!unit_can_do_action_now(punit)) {
    return FALSE;
  }

  /*** Phase 2: Special abilities checks ***/

  /* Actors. Pop up an action selection dialog in the client.
   * If the AI has used a goto to send an actor to a target do not
   * pop up a dialog in the client.
   * For tiles occupied by allied cities or units, keep moving if
   * move_diplomat_city tells us to, or if the unit is on goto and the tile
   * is not the final destination. */
  if (utype_may_act_at_all(unit_type_get(punit))) {
    bool can_not_move = !unit_can_move_to_tile(punit, pdesttile, igzoc);
    struct unit *tunit = action_tgt_unit(punit, pdesttile, can_not_move);
    struct city *tcity = action_tgt_city(punit, pdesttile, can_not_move);
    struct tile *ttile = action_tgt_tile(punit, pdesttile, can_not_move);

    /* Consider to pop up the action selection dialog if a potential city,
     * unit or units target exists at the destination tile. A tile target
     * will only trigger the pop up if it may be legal. */
    if ((0 < unit_list_size(pdesttile->units) || pcity || ttile)
        && !(move_diplomat_city
             && may_non_act_move(punit, pcity, pdesttile, igzoc))) {
      /* A target (unit or city) exists at the tile. If a target is an ally
       * it still looks like a target since move_diplomat_city isn't set.
       * Assume that the intention is to do an action. */

      /* If a tcity or a tunit exists it must be possible to act against it
       * since action_tgt_city() or action_tgt_unit() wouldn't have
       * targeted it otherwise. */
      if (tcity || tunit
          || action_tgt_tile_units(punit, pdesttile, can_not_move)
          || ttile) {
        if (is_ai(pplayer)) {
          return FALSE;
        }

        punit->action_decision_want = ACT_DEC_ACTIVE;
        punit->action_decision_tile = pdesttile;

        /* Let the client know that this unit needs the player to decide
         * what to do. */
        send_unit_info(player_reply_dest(pplayer), punit);

        /* The move wasn't done because the unit wanted the player to
         * decide what to do. */
        return FALSE;
      } else if (!may_non_act_move(punit, pcity, pdesttile, igzoc)) {
        /* No action can be done. No regular move can be done. Attack isn't
         * possible. Try to explain it to the player. */
        explain_why_no_action_enabled(punit, pdesttile, pcity,
                                      is_non_attack_unit_tile(pdesttile,
                                                              pplayer));

        /* The move wasn't done because the unit couldn't do anything. */
        return FALSE;
      }
    }
  }

  /*** Phase 3: Is it attack? ***/

  if (is_non_allied_unit_tile(pdesttile, pplayer)
      || is_non_allied_city_tile(pdesttile, pplayer)) {
    struct unit *victim = NULL;
    enum unit_attack_result ua_result;
    struct action *blocker;

    if ((blocker = action_blocks_attack(punit, pdesttile))) {
      /* Blocked by of the attacks that now are action enabler
       * controlled. */
      notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                    /* TRANS: ... Capture Units ... */
                    _("Regular attack not allowed when %s is legal."),
                    action_get_ui_name(blocker->id));
      return FALSE;
    }

    if (embark_to != NULL) {
      /* Can't both attack and embark. */
      return FALSE;
    }

    /* We can attack ONLY in enemy cities */
    if ((pcity && !pplayers_at_war(city_owner(pcity), pplayer))
        || (victim = is_non_attack_unit_tile(pdesttile, pplayer))) {
      notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                    _("You must declare war on %s first.  Try using "
                      "the Nations report (F3)."),
                    victim == NULL
                    ? player_name(city_owner(pcity))
                    : player_name(unit_owner(victim)));
      return FALSE;
    }

    /* Depending on 'unreachableprotects' setting, must be physically able
     * to attack EVERY unit there or must be physically able to attack SOME
     * unit there */
    ua_result = unit_attack_units_at_tile_result(punit, pdesttile);
    if (NULL == pcity && ua_result != ATT_OK) {
      struct tile *src_tile = unit_tile(punit);

      switch (ua_result) {
      case ATT_NON_ATTACK:
        notify_player(pplayer, src_tile, E_BAD_COMMAND, ftc_server,
                      _("%s is not an attack unit."), unit_name_translation(punit));
        break;
      case ATT_UNREACHABLE:
        notify_player(pplayer, src_tile, E_BAD_COMMAND, ftc_server,
                      _("You can't attack there since there's an unreachable unit."));
        break;
      case ATT_NONNATIVE_SRC:
        notify_player(pplayer, src_tile, E_BAD_COMMAND, ftc_server,
                      _("%s can't launch attack from %s."),
                        unit_name_translation(punit),
                        terrain_name_translation(tile_terrain(src_tile)));
        break;
      case ATT_NONNATIVE_DST:
        notify_player(pplayer, src_tile, E_BAD_COMMAND, ftc_server,
                      _("%s can't attack to %s."),
                        unit_name_translation(punit),
                        terrain_name_translation(tile_terrain(pdesttile)));
        break;
      case ATT_OK:
        fc_assert(ua_result != ATT_OK);
        break;
      }

      return FALSE;
    }

    /* The attack is legal wrt the alliances */
    victim = get_defender(punit, pdesttile);

    if (victim) {
      unit_attack_handling(punit, victim);
      return TRUE;
    } else {
      fc_assert_ret_val(is_enemy_city_tile(pdesttile, pplayer) != NULL,
                        TRUE);

      taking_over_city = TRUE;
      /* Taking over a city is considered a move, so fall through */
    }
  }

  /*** Phase 4: OK now move the unit ***/

  /* We cannot move a transport into a tile that holds
   * units or cities not allied with all of our cargo. */
  if (get_transporter_capacity(punit) > 0) {
    unit_list_iterate(unit_tile(punit)->units, pcargo) {
      if (unit_contained_in(pcargo, punit)
          && (is_non_allied_unit_tile(pdesttile, unit_owner(pcargo))
              || (!taking_over_city
                  && is_non_allied_city_tile(pdesttile,
                                             unit_owner(pcargo))))) {
         notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                       _("A transported unit is not allied to all "
                         "units or city on target tile."));
         return FALSE;
      }
    } unit_list_iterate_end;
  }

  if (can_unit_move_to_tile_with_notify(punit, pdesttile, igzoc,
                                        embark_to)) {
    int move_cost = map_move_cost_unit(punit, pdesttile);

    unit_move(punit, pdesttile, move_cost, embark_to);

    return TRUE;
  } else {
    return FALSE;
  }
}

/**************************************************************************
  Handle request to help in wonder building.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool do_unit_help_build_wonder(struct player *pplayer,
                                      struct unit *punit,
                                      struct city *pcity_dest)
{
  const char *work;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  /* Sanity check: The target city still exists. */
  fc_assert_ret_val(pcity_dest, FALSE);

  pcity_dest->shield_stock += unit_build_shield_cost(punit);
  pcity_dest->caravan_shields += unit_build_shield_cost(punit);

  conn_list_do_buffer(pplayer->connections);

  if (build_points_left(pcity_dest) >= 0) {
    /* TRANS: Your Caravan helps build the Pyramids in Bergen (4
     * remaining). You can reorder '4' and 'remaining' in the actual
     * format string. */
    work = _("remaining");
  } else {
    /* TRANS: Your Caravan helps build the Pyramids in Bergen (4
     * surplus). You can reorder '4' and 'surplus' in the actual
     * format string. */
    work = _("surplus");
  }

  /* Let the player that just donated shields to the wonder building know
   * the result of his donation. */
  notify_player(pplayer, city_tile(pcity_dest), E_CARAVAN_ACTION,
                ftc_server,
                /* TRANS: Your Caravan helps build the Pyramids in Bergen
                 * (4 surplus). */
                _("Your %s helps build the %s in %s (%d %s)."),
                unit_link(punit),
                improvement_name_translation(
                  pcity_dest->production.value.building),
                city_link(pcity_dest), 
                abs(build_points_left(pcity_dest)),
                work);

  /* May cause an incident */
  action_consequence_success(ACTION_HELP_WONDER, pplayer,
                             city_owner(pcity_dest),
                             city_tile(pcity_dest), city_link(pcity_dest));

  if (city_owner(pcity_dest) != unit_owner(punit)) {
    /* Tell the city owner about the gift he just received. */

    notify_player(city_owner(pcity_dest), city_tile(pcity_dest),
                  E_CARAVAN_ACTION, ftc_server,
                  /* TRANS: Help building the Pyramids in Bergen received
                   * from Persian Caravan (4 surplus). */
                  _("Help building the %s in %s received from %s %s "
                    "(%d %s)."),
                  improvement_name_translation(
                    pcity_dest->production.value.building),
                  city_link(pcity_dest),
                  nation_adjective_for_player(pplayer),
                  unit_link(punit),
                  abs(build_points_left(pcity_dest)),
                  work);
  }

  wipe_unit(punit, ULR_USED, NULL);
  send_player_info_c(pplayer, pplayer->connections);
  send_city_info(pplayer, pcity_dest);
  conn_list_do_unbuffer(pplayer->connections);

  return TRUE;
}

/**************************************************************************
  Handle request to establish traderoute. If pcity_dest is NULL, assumes
  that unit is inside target city.

  Returns TRUE iff action could be done, FALSE if it couldn't. Even if
  this returns TRUE, unit may have died during the action.
**************************************************************************/
static bool do_unit_establish_trade(struct player *pplayer,
                                    struct unit *punit,
                                    struct city *pcity_dest,
                                    bool est_if_able)
{
  char homecity_link[MAX_LEN_LINK], destcity_link[MAX_LEN_LINK];
  char punit_link[MAX_LEN_LINK];
  int revenue;
  bool can_establish;
  int home_overbooked = 0;
  int dest_overbooked = 0;
  int home_max;
  int dest_max;
  struct city *pcity_homecity;
  struct trade_route_list *routes_out_of_dest;
  struct trade_route_list *routes_out_of_home;
  enum traderoute_bonus_type bonus_type;
  const char *bonus_str;
  struct goods_type *goods;
  const char *goods_str;

  /* Sanity check: The actor still exists. */
  fc_assert_ret_val(pplayer, FALSE);
  fc_assert_ret_val(punit, FALSE);

  /* Sanity check: The target city still exists. */
  fc_assert_ret_val(pcity_dest, FALSE);

  pcity_homecity = player_city_by_number(pplayer, punit->homecity);

  if (!pcity_homecity) {
    notify_player(pplayer, unit_tile(punit), E_BAD_COMMAND, ftc_server,
                  _("Sorry, your %s cannot establish"
                    " a trade route because it has no home city."),
                  unit_link(punit));
    return FALSE;
   
  }

  sz_strlcpy(homecity_link, city_link(pcity_homecity));
  sz_strlcpy(destcity_link, city_link(pcity_dest));

  if (!can_cities_trade(pcity_homecity, pcity_dest)) {
    notify_player(pplayer, city_tile(pcity_dest), E_BAD_COMMAND, ftc_server,
                  _("Sorry, your %s cannot establish"
                    " a trade route between %s and %s."),
                  unit_link(punit),
                  homecity_link,
                  destcity_link);
    return FALSE;
  }

  sz_strlcpy(punit_link, unit_tile_link(punit));
  routes_out_of_home = trade_route_list_new();
  routes_out_of_dest = trade_route_list_new();

  /* This part of code works like can_establish_trade_route, except
   * that we actually do the action of making the trade route. */

  /* If we can't make a new trade route we can still get the trade bonus. */
  can_establish = est_if_able
                  && !have_cities_trade_route(pcity_homecity, pcity_dest);

  if (can_establish) {
    home_max = max_trade_routes(pcity_homecity);
    dest_max = max_trade_routes(pcity_dest);
    home_overbooked = city_num_trade_routes(pcity_homecity) - home_max;
    dest_overbooked = city_num_trade_routes(pcity_dest) - dest_max;
  }

  if (can_establish && (home_overbooked >= 0 || dest_overbooked >= 0)) {
    int trade = trade_between_cities(pcity_homecity, pcity_dest);

    /* See if there's a trade route we can cancel at the home city. */
    if (home_overbooked >= 0) {
      if (home_max <= 0
          || (city_trade_removable(pcity_homecity, routes_out_of_home)
              >= trade)) {
        notify_player(pplayer, city_tile(pcity_dest),
                      E_BAD_COMMAND, ftc_server,
                     _("Sorry, your %s cannot establish"
                       " a trade route here!"),
                       punit_link);
        if (home_max > 0) {
          notify_player(pplayer, city_tile(pcity_dest),
                        E_BAD_COMMAND, ftc_server,
                        PL_("      The city of %s already has %d "
                            "better trade route!",
                            "      The city of %s already has %d "
                            "better trade routes!", home_max),
                        homecity_link,
                        home_max);
        }
	can_establish = FALSE;
      }
    }

    /* See if there's a trade route we can cancel at the dest city. */
    if (can_establish && dest_overbooked >= 0) {
      if (dest_max <= 0
          || (city_trade_removable(pcity_dest, routes_out_of_dest)
              >= trade)) {
        notify_player(pplayer, city_tile(pcity_dest),
                      E_BAD_COMMAND, ftc_server,
                      _("Sorry, your %s cannot establish"
                        " a trade route here!"),
                      punit_link);
        if (dest_max > 0) {
          notify_player(pplayer, city_tile(pcity_dest),
                        E_BAD_COMMAND, ftc_server,
                        PL_("      The city of %s already has %d "
                            "better trade route!",
                            "      The city of %s already has %d "
                            "better trade routes!", dest_max),
                        destcity_link,
                        dest_max);
        }
	can_establish = FALSE;
      }
    }
  }

  /* We now know for sure whether we can establish a trade route. */

  /* Calculate and announce initial revenue. */
  revenue = get_caravan_enter_city_trade_bonus(pcity_homecity, pcity_dest,
                                               can_establish);

  bonus_type = trade_route_settings_by_type(cities_trade_route_type(pcity_homecity, pcity_dest))->bonus_type;
  bonus_str = NULL;

  switch (bonus_type) {
  case TBONUS_NONE:
    break;
  case TBONUS_GOLD:
    /* TRANS: used as part of caravan revenue sentence. */
    bonus_str = _("gold");
    break;
  case TBONUS_SCIENCE:
    /* TRANS: used as part of caravan revenue sentence. */
    bonus_str = _("research");
    break;
  case TBONUS_BOTH:
    /* TRANS: used as part of caravan revenue sentence. */
    bonus_str = _("gold and research");
    break;
  }

  conn_list_do_buffer(pplayer->connections);

  goods = goods_for_new_route(pcity_homecity, pcity_dest);
  goods_str = goods_name_translation(goods);

  if (bonus_str != NULL) {
    notify_player(pplayer, city_tile(pcity_dest),
                  E_CARAVAN_ACTION, ftc_server,
                  /* TRANS: ... Caravan ... Paris ... Stockholm, ... Goods... gold and research. */
                  PL_("Your %s from %s has arrived in %s carrying %s,"
                      " and revenues amount to %d in %s.",
                      "Your %s from %s has arrived in %s carrying %s,"
                      " and revenues amount to %d in %s.",
                      revenue),
                  punit_link,
                  homecity_link,
                  destcity_link,
                  goods_str,
                  revenue,
                  bonus_str);
  } else {
    notify_player(pplayer, city_tile(pcity_dest),
                  E_CARAVAN_ACTION, ftc_server,
                  /* TRANS: ... Caravan ... Paris ... Stockholm ... Goods */
                  _("Your %s from %s has arrived in %s carrying %s."),
                  punit_link,
                  homecity_link,
                  destcity_link,
                  goods_str);
  }
  wipe_unit(punit, ULR_USED, NULL);

  if (bonus_type == TBONUS_GOLD || bonus_type == TBONUS_BOTH) {
    pplayer->economic.gold += revenue;

    send_player_info_c(pplayer, pplayer->connections);
  }

  if (bonus_type == TBONUS_SCIENCE || bonus_type == TBONUS_BOTH) {
    /* add bulbs and check for finished research */
    update_bulbs(pplayer, revenue, TRUE);

    /* Inform everyone about tech changes */
    send_research_info(research_get(pplayer), NULL);
  }

  if (can_establish) {
    struct trade_route *proute;
    struct city_list *cities_out_of_home;
    struct city_list *cities_out_of_dest;

    /* Announce creation of trade route (it's not actually created until
     * later in this function, as we have to cancel existing routes, but
     * it makes more sense to announce in this order) */

    /* Always tell the unit owner */
    notify_player(pplayer, NULL,
                  E_CARAVAN_ACTION, ftc_server,
                  _("New trade route established from %s to %s."),
                  homecity_link,
                  destcity_link);
    if (pplayer != city_owner(pcity_dest)) {
      notify_player(city_owner(pcity_dest), city_tile(pcity_dest),
                    E_CARAVAN_ACTION, ftc_server,
                    _("The %s established a trade route between their "
                      "city %s and %s."),
                    nation_plural_for_player(pplayer),
                    homecity_link,
                    destcity_link);
    }

    cities_out_of_home = city_list_new();
    cities_out_of_dest = city_list_new();

    /* Now cancel any less profitable trade route from the home city. */
    trade_route_list_iterate(routes_out_of_home, premove) {
      struct trade_route *pback;

      city_list_append(cities_out_of_home, game_city_by_number(premove->partner));

      pback = remove_trade_route(pcity_homecity, premove, TRUE, FALSE);
      free(premove);
      free(pback);
    } trade_route_list_iterate_end;

    /* And the same for the dest city. */
    trade_route_list_iterate(routes_out_of_dest, premove) {
      struct trade_route *pback;

      city_list_append(cities_out_of_dest, game_city_by_number(premove->partner));

      pback = remove_trade_route(pcity_dest, premove, TRUE, FALSE);
      free(premove);
      free(pback);
    } trade_route_list_iterate_end;

    /* Actually create the new trade route */
    proute = fc_malloc(sizeof(struct trade_route));
    proute->partner = pcity_dest->id;
    proute->dir = RDIR_FROM;
    proute->goods = goods;
    trade_route_list_append(pcity_homecity->routes, proute);

    proute = fc_malloc(sizeof(struct trade_route));
    proute->partner = pcity_homecity->id;
    proute->dir = RDIR_TO;
    proute->goods = goods;
    trade_route_list_append(pcity_dest->routes, proute);

    /* Refresh the cities. */
    city_refresh(pcity_homecity);
    city_refresh(pcity_dest);
    city_list_iterate(cities_out_of_home, pcity) {
      city_refresh(pcity);
    } city_list_iterate_end;
    city_list_iterate(cities_out_of_dest, pcity) {
      city_refresh(pcity);
    } city_list_iterate_end;

    /* Notify the owners of the cities. */
    send_city_info(pplayer, pcity_homecity);
    send_city_info(city_owner(pcity_dest), pcity_dest);
    city_list_iterate(cities_out_of_home, pcity) {
      send_city_info(city_owner(pcity), pcity);
    } city_list_iterate_end;
    city_list_iterate(cities_out_of_dest, pcity) {
      send_city_info(city_owner(pcity), pcity);
    } city_list_iterate_end;

    /* Notify each player about the other cities so that they know about
     * its size for the trade calculation . */
    if (pplayer != city_owner(pcity_dest)) {
      send_city_info(city_owner(pcity_dest), pcity_homecity);
      send_city_info(pplayer, pcity_dest);
    }

    city_list_iterate(cities_out_of_home, pcity) {
      if (city_owner(pcity_dest) != city_owner(pcity)) {
        send_city_info(city_owner(pcity_dest), pcity);
        send_city_info(city_owner(pcity), pcity_dest);
      }
      if (pplayer != city_owner(pcity)) {
        send_city_info(pplayer, pcity);
        send_city_info(city_owner(pcity), pcity_homecity);
      }
    } city_list_iterate_end;

    city_list_iterate(cities_out_of_dest, pcity) {
      if (city_owner(pcity_dest) != city_owner(pcity)) {
        send_city_info(city_owner(pcity_dest), pcity);
        send_city_info(city_owner(pcity), pcity_dest);
      }
      if (pplayer != city_owner(pcity)) {
        send_city_info(pplayer, pcity);
        send_city_info(city_owner(pcity), pcity_homecity);
      }
    } city_list_iterate_end;

    city_list_destroy(cities_out_of_home);
    city_list_destroy(cities_out_of_dest);
  }

  /* May cause an incident */
  action_consequence_success(est_if_able ?
                               ACTION_TRADE_ROUTE :
                               ACTION_MARKETPLACE,
                             pplayer, city_owner(pcity_dest),
                             city_tile(pcity_dest),
                             city_link(pcity_dest));

  conn_list_do_unbuffer(pplayer->connections);

  /* Free data. */
  trade_route_list_destroy(routes_out_of_home);
  trade_route_list_destroy(routes_out_of_dest);

  return TRUE;
}

/**************************************************************************
  Change various unit server side client state.

  The server keeps various unit state that is owned by the client. The only
  consequence this state has for the game is how the client reacts to it.
  The state may be server side because the server writes to it or simply to
  have it end up in the save game.
**************************************************************************/
void handle_unit_sscs_set(struct player *pplayer,
                          int unit_id,
                          enum unit_ss_data_type type,
                          int value)
{
  struct unit *punit = player_unit_by_number(pplayer, unit_id);

  if (NULL == punit) {
    /* Being asked to unqueue a "spent" unit because the client haven't
     * been told that it's gone is expected. */
    if (type != USSDT_UNQUEUE) {
      /* Probably died or bribed. */
      log_verbose("handle_unit_sscs_set() invalid unit %d", unit_id);
    }

    return;
  }

  switch (type) {
  case USSDT_QUEUE:
    /* Reminds the client to ask the server about what actions the unit can
     * perform against the target tile. Action decision state can be set by
     * the server it self too. */

    if (index_to_tile(value) == NULL) {
      /* Asked to be reminded to ask what actions the unit can do to a non
       * existing target tile. */
      log_verbose("unit_sscs_set() invalid target tile %d for unit %d",
                  value, unit_id);
      break;
    }

    punit->action_decision_want = ACT_DEC_ACTIVE;
    punit->action_decision_tile = index_to_tile(value);

    /* Let the client know that this unit needs the player to decide
     * what to do. */
    send_unit_info(player_reply_dest(pplayer), punit);

    break;
  case USSDT_UNQUEUE:
    /* Delete the reminder for the client to ask the server about what
     * actions the unit can perform against a certain target tile.
     * Action decision state can be set by the server it self too. */

    punit->action_decision_want = ACT_DEC_NOTHING;
    punit->action_decision_tile = NULL;

    /* Let the client know that this unit no longer needs the player to
     * decide what to do. */
    send_unit_info(player_reply_dest(pplayer), punit);

    break;
  case USSDT_BATTLE_GROUP:
    /* Battlegroups are handled entirely by the client, so all we have to
       do here is save the battlegroup ID so that it'll be persistent. */

    punit->battlegroup = CLIP(-1, value, MAX_NUM_BATTLEGROUPS);

    break;
  }
}

/**************************************************************************
  Handle request to set unit to autosettler mode.
**************************************************************************/
void handle_unit_autosettlers(struct player *pplayer, int unit_id)
{
  struct unit *punit = player_unit_by_number(pplayer, unit_id);

  if (NULL == punit) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_autosettlers() invalid unit %d", unit_id);
    return;
  }

  if (!can_unit_do_autosettlers(punit)) {
    return;
  }

  punit->ai_controlled = TRUE;
  send_unit_info(NULL, punit);
}

/**************************************************************************
  Update everything that needs changing when unit activity changes from
  old activity to new one.
**************************************************************************/
static void unit_activity_dependencies(struct unit *punit,
				       enum unit_activity old_activity,
                                       struct extra_type *old_target)
{
  switch (punit->activity) {
  case ACTIVITY_IDLE:
    switch (old_activity) {
    case ACTIVITY_PILLAGE: 
      {
        if (old_target != NULL) {
          unit_list_iterate_safe(unit_tile(punit)->units, punit2) {
            if (punit2->activity == ACTIVITY_PILLAGE) {
              extra_deps_iterate(&(punit2->activity_target->reqs), pdep) {
                if (pdep == old_target) {
                  set_unit_activity(punit2, ACTIVITY_IDLE);
                  send_unit_info(NULL, punit2);
                  break;
                }
              } extra_deps_iterate_end;
            }
          } unit_list_iterate_safe_end;
        }
        break;
      }
    case ACTIVITY_EXPLORE:
      /* Restore unit's control status */
      punit->ai_controlled = FALSE;
      break;
    default: 
      ; /* do nothing */
    }
    break;
  case ACTIVITY_EXPLORE:
    punit->ai_controlled = TRUE;
    set_unit_activity(punit, ACTIVITY_EXPLORE);
    send_unit_info(NULL, punit);
    break;
  default:
    /* do nothing */
    break;
  }
}

/**************************************************************************
  Handle request for changing activity.
**************************************************************************/
void unit_activity_handling(struct unit *punit,
                            enum unit_activity new_activity)
{
  /* Must specify target for ACTIVITY_BASE */
  fc_assert_ret(new_activity != ACTIVITY_BASE
                && new_activity != ACTIVITY_GEN_ROAD);
  
  if (new_activity == ACTIVITY_PILLAGE) {
    struct extra_type *target = NULL;

    /* Assume untargeted pillaging if no target specified */
    unit_activity_handling_targeted(punit, new_activity, &target);
  } else if (can_unit_do_activity(punit, new_activity)) {
    enum unit_activity old_activity = punit->activity;
    struct extra_type *old_target = punit->activity_target;

    free_unit_orders(punit);
    set_unit_activity(punit, new_activity);
    send_unit_info(NULL, punit);
    unit_activity_dependencies(punit, old_activity, old_target);
  }
}

/**************************************************************************
  Handle request for targeted activity.
**************************************************************************/
void unit_activity_handling_targeted(struct unit *punit,
                                     enum unit_activity new_activity,
                                     struct extra_type **new_target)
{
  if (!activity_requires_target(new_activity)) {
    unit_activity_handling(punit, new_activity);
  } else if (can_unit_do_activity_targeted(punit, new_activity, *new_target)) {
    enum unit_activity old_activity = punit->activity;
    struct extra_type *old_target = punit->activity_target;
    enum unit_activity stored_activity = new_activity;

    free_unit_orders(punit);
    unit_assign_specific_activity_target(punit,
                                         &new_activity, new_target);
    if (new_activity != stored_activity
        && !activity_requires_target(new_activity)) {
      /* unit_assign_specific_activity_target() changed our target activity
       * (to ACTIVITY_IDLE in practice) */
      unit_activity_handling(punit, new_activity);
    } else {
      set_unit_activity_targeted(punit, new_activity, *new_target);
      send_unit_info(NULL, punit);    
      unit_activity_dependencies(punit, old_activity, old_target);
    }
  }
}

/****************************************************************************
  Handle a client request to load the given unit into the given transporter.
****************************************************************************/
void handle_unit_load(struct player *pplayer, int cargo_id, int trans_id,
                      int ttile_idx)
{
  struct unit *pcargo = player_unit_by_number(pplayer, cargo_id);
  struct unit *ptrans = game_unit_by_number(trans_id);
  struct tile *ptile = index_to_tile(ttile_idx);
  struct tile *ctile;
  struct tile *ttile;
  bool moves = FALSE;
  bool leave = FALSE;

  if (NULL == pcargo) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_load() invalid cargo %d", cargo_id);
    return;
  }

  if (NULL == ptrans) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_load() invalid transport %d", trans_id);
    return;
  }

  ttile = unit_tile(ptrans);
  if (!same_pos(ttile, ptile)) {
    /* Transport no longer in where client assumed it to be. */
    return;
  }

  ctile = unit_tile(pcargo);

  if (!same_pos(ctile, ttile)) {
    if (pcargo->moves_left <= 0 || !unit_can_move_to_tile(pcargo, ttile, FALSE)) {
      return;
    }

    moves = TRUE;
  }

  if (unit_transported(pcargo)) {
    if (!can_unit_unload(pcargo, ptrans)) {
      /* Can't leave current transport */
      return;
    }

    leave = TRUE;
  }

  /* A player may only load their units, but they may be loaded into
   * other player's transporters, depending on the rules in
   * could_unit_load(). */
  if (!could_unit_load(pcargo, ptrans)) {
    return;
  }

  /* It's possible. Let's make all the necessary steps. */
  if (leave) {
    unit_transport_unload(pcargo);
  }

  if (moves) {
    unit_move_handling(pcargo, ttile, FALSE, FALSE, ptrans);
    return;
  }

  /* Load the unit and send out info to clients. */
  unit_transport_load_send(pcargo, ptrans);
}

/****************************************************************************
  Handle a client request to unload the given unit from the given
  transporter.
****************************************************************************/
void handle_unit_unload(struct player *pplayer, int cargo_id, int trans_id)
{
  struct unit *pcargo = game_unit_by_number(cargo_id);
  struct unit *ptrans = game_unit_by_number(trans_id);

  if (NULL == pcargo) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_unload() invalid cargo %d", cargo_id);
    return;
  }

  if (NULL == ptrans) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_unload() invalid transport %d", trans_id);
    return;
  }

  /* You are allowed to unload a unit if it is yours or if the transporter
   * is yours. */
  if (unit_owner(pcargo) != pplayer && unit_owner(ptrans) != pplayer) {
    return;
  }

  if (!can_unit_unload(pcargo, ptrans)) {
    return;
  }

  if (!can_unit_survive_at_tile(pcargo, unit_tile(pcargo))) {
    return;
  }

  /* Unload the unit and send out info to clients. */
  unit_transport_unload_send(pcargo);
}

/****************************************************************************
  Receives route packages.
****************************************************************************/
void handle_unit_orders(struct player *pplayer,
                        const struct packet_unit_orders *packet)
{
  int length = packet->length, i;
  struct unit *punit = player_unit_by_number(pplayer, packet->unit_id);
  struct tile *src_tile = index_to_tile(packet->src_tile);

  if (NULL == punit) {
    /* Probably died or bribed. */
    log_verbose("handle_unit_orders() invalid unit %d", packet->unit_id);
    return;
  }

  if (0 > length || MAX_LEN_ROUTE < length) {
    /* Shouldn't happen */
    log_error("handle_unit_orders() invalid %s (%d) "
              "packet length %d (max %d)", unit_rule_name(punit),
              packet->unit_id, length, MAX_LEN_ROUTE);
    return;
  }

  if (src_tile != unit_tile(punit)) {
    /* Failed sanity check.  Usually this happens if the orders were sent
     * in the previous turn, and the client thought the unit was in a
     * different position than it's actually in.  The easy solution is to
     * discard the packet.  We don't send an error message to the client
     * here (though maybe we should?). */
    log_verbose("handle_unit_orders() invalid %s (%d) tile (%d, %d) "
                "!= (%d, %d)", unit_rule_name(punit), punit->id,
                TILE_XY(src_tile), TILE_XY(unit_tile(punit)));
    return;
  }

  if (ACTIVITY_IDLE != punit->activity) {
    /* New orders implicitly abandon current activity */
    unit_activity_handling(punit, ACTIVITY_IDLE);
  }

  for (i = 0; i < length; i++) {
    if (packet->orders[i] < 0 || packet->orders[i] > ORDER_LAST) {
      log_error("%s() %s (player nb %d) has sent an invalid order %d "
                "at index %d, truncating", __FUNCTION__,
                player_name(pplayer), player_number(pplayer),
                packet->orders[i], i);
      length = i;
      break;
    }
    switch (packet->orders[i]) {
    case ORDER_MOVE:
    case ORDER_ACTION_MOVE:
      if (!map_untrusted_dir_is_valid(packet->dir[i])) {
        log_error("handle_unit_orders() %d isn't a valid move direction. "
                  "Sent in order number %d from %s to unit number %d.",
                  packet->dir[i], i,
                  player_name(pplayer), packet->unit_id);

	return;
      }
      break;
    case ORDER_ACTIVITY:
      switch (packet->activity[i]) {
      case ACTIVITY_FALLOUT:
      case ACTIVITY_POLLUTION:
      case ACTIVITY_PILLAGE:
      case ACTIVITY_MINE:
      case ACTIVITY_IRRIGATE:
      case ACTIVITY_TRANSFORM:
      case ACTIVITY_CONVERT:
	/* Simple activities. */
	break;
      case ACTIVITY_FORTIFYING:
      case ACTIVITY_SENTRY:
        if (i != length - 1) {
          /* Only allowed as the last order. */
          log_error("handle_unit_orders() activity %d is only allowed in "
                    "the last order. "
                    "Sent in order number %d from %s to unit number %d.",
                    packet->activity[i], i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      case ACTIVITY_BASE:
        if (!is_extra_caused_by(extra_by_number(packet->target[i]), EC_BASE)) {
          log_error("handle_unit_orders() %s isn't a base. "
                    "Sent in order number %d from %s to unit number %d.",
                    extra_rule_name(extra_by_number(packet->target[i])), i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      case ACTIVITY_GEN_ROAD:
        if (!is_extra_caused_by(extra_by_number(packet->target[i]), EC_ROAD)) {
          log_error("handle_unit_orders() %s isn't a road. "
                    "Sent in order number %d from %s to unit number %d.",
                    extra_rule_name(extra_by_number(packet->target[i])), i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      /* Not supported yet. */
      case ACTIVITY_EXPLORE:
      case ACTIVITY_IDLE:
      /* Not set from the client. */
      case ACTIVITY_GOTO:
      case ACTIVITY_FORTIFIED:
      /* Compatiblity, used in savegames. */
      case ACTIVITY_OLD_ROAD:
      case ACTIVITY_OLD_RAILROAD:
      case ACTIVITY_FORTRESS:
      case ACTIVITY_AIRBASE:
      /* Unused. */
      case ACTIVITY_PATROL_UNUSED:
      case ACTIVITY_LAST:
      case ACTIVITY_UNKNOWN:
        log_error("handle_unit_orders() unsupported activity %d. "
                  "Sent in order number %d from %s to unit number %d.",
                  packet->activity[i], i,
                  player_name(pplayer), packet->unit_id);

        return;
      }

      if (packet->target[i] == EXTRA_NONE
          && unit_activity_needs_target_from_client(packet->activity[i])) {
        /* The orders system can't do server side target assignment for
         * this activity. */
        log_error("handle_unit_orders() can't assign target for %d. "
                  "Sent in order number %d from %s to unit number %d.",
                  packet->activity[i], i,
                  player_name(pplayer), packet->unit_id);

        return;
      }

      break;
    case ORDER_PERFORM_ACTION:
      if (!action_id_is_valid(packet->action[i])) {
        /* Non existing action */
        log_error("handle_unit_orders() the action %d doesn't exist. "
                  "Sent in order number %d from %s to unit number %d.",
                  packet->action[i], i,
                  player_name(pplayer), packet->unit_id);

        return;
      }

      if (action_by_number(packet->action[i])->max_distance > 1) {
        /* Long range actions aren't supported in unit orders. Clients
         * should order them performed via the unit_do_action packet.
         *
         * Reason: A unit order stores an action's target as the tile it is
         * located on. The tile is stored as a direction (when the target
         * is at a tile adjacent to the actor unit tile) or as no
         * direction (when the target is at the same tile as the actor
         * unit). The order system will pick a suitable target at the
         * specified tile during order execution. This makes it impossible
         * to target something that isn't at or next to the actors tile.
         * Being unable to exploit the full range of an action handicaps
         * it.
         *
         * A patch that allows a distant target in an order should remove
         * this check and update the comment in the Qt client's
         * go_act_menu::create(). */

        log_error("handle_unit_orders() the action %s isn't supported in "
                  "unit orders. "
                  "Sent in order number %d from %s to unit number %d.",
                  action_get_ui_name(packet->action[i]), i,
                  player_name(pplayer), packet->unit_id);

        return;
      }

      /* Validate individual actions. */
      switch ((enum gen_action) packet->action[i]) {
      case ACTION_FOUND_CITY:
        if (map_untrusted_dir_is_valid(packet->dir[i])) {
          /* Actor must be on the target tile. */
          log_error("handle_unit_orders() can't do %s to a neighbor tile. "
                    "Sent in order number %d from %s to unit number %d.",
                    action_get_rule_name(packet->action[i]), i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      case ACTION_SPY_TARGETED_SABOTAGE_CITY:
        /* Sabotage target is production (-1) or a building. */
        if (!(packet->target[i] - 1 == -1
              || improvement_by_number(packet->target[i] - 1))) {
          /* Sabotage target is invalid. */

          log_error("handle_unit_orders() can't do %s without a target. "
                    "Sent in order number %d from %s to unit number %d.",
                    action_get_rule_name(packet->action[i]), i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      case ACTION_SPY_TARGETED_STEAL_TECH:
        if (packet->target[i] == A_NONE
            || (!valid_advance_by_number(packet->target[i])
                && packet->target[i] != A_FUTURE)) {
          /* Target tech is invalid. */

          log_error("handle_unit_orders() can't do %s without a target. "
                    "Sent in order number %d from %s to unit number %d.",
                    action_get_rule_name(packet->action[i]), i,
                    player_name(pplayer), packet->unit_id);

          return;
        }
        break;
      case ACTION_ESTABLISH_EMBASSY:
      case ACTION_SPY_INVESTIGATE_CITY:
      case ACTION_SPY_POISON:
      case ACTION_SPY_STEAL_GOLD:
      case ACTION_SPY_SABOTAGE_CITY:
      case ACTION_SPY_STEAL_TECH:
      case ACTION_SPY_INCITE_CITY:
      case ACTION_TRADE_ROUTE:
      case ACTION_MARKETPLACE:
      case ACTION_HELP_WONDER:
      case ACTION_SPY_BRIBE_UNIT:
      case ACTION_SPY_SABOTAGE_UNIT:
      case ACTION_CAPTURE_UNITS:
      case ACTION_JOIN_CITY:
      case ACTION_STEAL_MAPS:
      case ACTION_BOMBARD:
      case ACTION_SPY_NUKE:
      case ACTION_NUKE:
      case ACTION_DESTROY_CITY:
      case ACTION_EXPEL_UNIT:
      case ACTION_RECYCLE_UNIT:
      case ACTION_DISBAND_UNIT:
      case ACTION_HOME_CITY:
      case ACTION_UPGRADE_UNIT:
        /* No validation required. */
        break;
      case ACTION_PARADROP:
      case ACTION_AIRLIFT:
        /* Can't specify target tile in the current order packet format.
         * Should have been caught above. */
        log_error("Action %s isn't supported in orders. "
                  "It was sent in order number %d from %s to "
                  "unit number %d.",
                  action_get_ui_name(packet->action[i]), i,
                  player_name(pplayer), packet->unit_id);
        break;
      /* Invalid action. Should have been caught above. */
      case ACTION_COUNT:
        fc_assert_ret_msg(packet->action[i] != ACTION_COUNT,
                          "ACTION_COUNT in order number %d from %s to "
                          "unit number %d.",
                          i, player_name(pplayer), packet->unit_id);
      }

      switch (action_get_target_kind(packet->action[i])) {
      case ATK_CITY:
        /* Don't validate that the target tile really contains a city or
         * that the actor player's map think the target tile has one.
         * The player may target a city from its player map that isn't
         * there any more and a city that he think is there even if his
         * player map doesn't have it.
         *
         * With that said: The client should probably at least have an
         * option to only aim city targeted actions at cities. */
        break;
      case ATK_UNIT:
        break;
      case ATK_UNITS:
        break;
      case ATK_TILE:
        break;
      case ATK_SELF:
        break;
      case ATK_COUNT:
        fc_assert(action_get_target_kind(packet->action[i]) != ATK_COUNT);
        break;
      }

      break;
    case ORDER_FULL_MP:
      break;
    case ORDER_OLD_HOMECITY:
    case ORDER_OLD_DISBAND:
    case ORDER_OLD_BUILD_CITY:
    case ORDER_OLD_BUILD_WONDER:
    case ORDER_OLD_TRADE_ROUTE:
      /* This order has been replaced with ORDER_PERFORM_ACTION and the
       * action it performs. */
      log_error("handle_unit_orders(): outdated client. "
                "The order %d has been replaced. "
                "Sent in order number %d from %s to unit number %d.",
                packet->orders[i], i,
                player_name(pplayer), packet->unit_id);

      return;
    case ORDER_LAST:
      /* An invalid order.  This is handled in execute_orders. */
      break;
    }
  }

  /* This must be before old orders are freed. If this is is
   * settlers on city founding mission, city spot reservation
   * from goto_tile must be freed, and free_unit_orders() loses
   * goto_tile information */
  adv_unit_new_task(punit, AUT_NONE, NULL);

  free_unit_orders(punit);
  /* If we waited on a tile, reset punit->done_moving */
  punit->done_moving = (punit->moves_left <= 0);

  /* Make sure that the unit won't keep its old ai_controlled state after
   * it has recieved new orders from the client. */
  punit->ai_controlled = FALSE;

  if (length == 0) {
    fc_assert(!unit_has_orders(punit));
    send_unit_info(NULL, punit);
    return;
  }

  punit->has_orders = TRUE;
  punit->orders.length = length;
  punit->orders.index = 0;
  punit->orders.repeat = packet->repeat;
  punit->orders.vigilant = packet->vigilant;
  punit->orders.list
    = fc_malloc(length * sizeof(*(punit->orders.list)));
  for (i = 0; i < length; i++) {
    punit->orders.list[i].order = packet->orders[i];
    punit->orders.list[i].dir = packet->dir[i];
    punit->orders.list[i].activity = packet->activity[i];
    punit->orders.list[i].target = packet->target[i];
    punit->orders.list[i].action = packet->action[i];
  }

  if (!packet->repeat) {
    punit->goto_tile = index_to_tile(packet->dest_tile);
  } else {
    /* Make sure that no old goto_tile remains. */
    punit->goto_tile = NULL;
  }

#ifdef DEBUG
  log_debug("Orders for unit %d: length:%d", packet->unit_id, length);
  for (i = 0; i < length; i++) {
    log_debug("  %d,%s", packet->orders[i], dir_get_name(packet->dir[i]));
  }
#endif

  if (!is_player_phase(unit_owner(punit), game.info.phase)
      || execute_orders(punit, TRUE)) {
    /* Looks like the unit survived. */
    send_unit_info(NULL, punit);
  }
}

/**************************************************************************
  Handle worker task assigned to the city
**************************************************************************/
void handle_worker_task(struct player *pplayer,
                        const struct packet_worker_task *packet)
{
  struct city *pcity = game_city_by_number(packet->city_id);
  struct worker_task *ptask = NULL;
  struct tile *ptile = index_to_tile(packet->tile_id);

  if (pcity == NULL || pcity->owner != pplayer || ptile == NULL) {
    return;
  }

  worker_task_list_iterate(pcity->task_reqs, ptask_old) {
    if (tile_index(ptask_old->ptile) == packet->tile_id) {
      ptask = ptask_old;
    }
  } worker_task_list_iterate_end;

  if (ptask == NULL) {
    if (packet->activity == ACTIVITY_LAST) {
      return;
    }

    ptask = fc_malloc(sizeof(struct worker_task));
    worker_task_init(ptask);
    worker_task_list_append(pcity->task_reqs, ptask);
  } else {
    if (packet->activity == ACTIVITY_LAST) {
      worker_task_list_remove(pcity->task_reqs, ptask);
      free(ptask);
      ptask = NULL;
    }
  }

  if (ptask != NULL) {
    ptask->ptile = ptile;
    ptask->act = packet->activity;
    if (packet->tgt >= 0) {
      ptask->tgt = extra_by_number(packet->tgt);
    } else {
      ptask->tgt = NULL;
    }
    ptask->want = packet->want;
  }

  lsend_packet_worker_task(pplayer->connections, packet);
}
