/**********************************************************************
 Freeciv - Copyright (C) 1996-2013 - Freeciv Development Team
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

/* common */
#include "actions.h"
#include "city.h"
#include "game.h"
#include "unit.h"
#include "tile.h"

static struct action *actions[ACTION_COUNT];
static struct action_enabler_list *action_enablers_by_action[ACTION_COUNT];

static struct action *action_new(enum gen_action id,
                                 enum action_target_kind target_kind,
                                 const char *ui_name);

static bool is_enabler_active(const struct action_enabler *enabler,
			      const struct player *actor_player,
			      const struct city *actor_city,
			      const struct impr_type *actor_building,
			      const struct tile *actor_tile,
			      const struct unit_type *actor_unittype,
			      const struct output_type *actor_output,
			      const struct specialist *actor_specialist,
			      const struct player *target_player,
			      const struct city *target_city,
			      const struct impr_type *target_building,
			      const struct tile *target_tile,
			      const struct unit_type *target_unittype,
			      const struct output_type *target_output,
			      const struct specialist *target_specialist);

/**************************************************************************
  Initialize the actions and the action enablers.
**************************************************************************/
void actions_init(void)
{
  /* Hard code the actions */
  actions[ACTION_SPY_POISON] = action_new(ACTION_SPY_POISON, ATK_CITY,
                                          N_("Poison City"));
  actions[ACTION_SPY_SABOTAGE_UNIT] = action_new(ACTION_SPY_SABOTAGE_UNIT,
                                                 ATK_UNIT,
                                                 N_("Sabotage Enemy Unit"));
  actions[ACTION_SPY_BRIBE_UNIT] = action_new(ACTION_SPY_BRIBE_UNIT,
                                              ATK_UNIT,
                                              N_("Bribe Enemy Unit"));
  actions[ACTION_SPY_SABOTAGE_CITY] = action_new(ACTION_SPY_SABOTAGE_CITY,
                                                 ATK_CITY,
                                                 N_("Sabotage City"));
  actions[ACTION_SPY_TARGETED_SABOTAGE_CITY] =
      action_new(ACTION_SPY_TARGETED_SABOTAGE_CITY, ATK_CITY,
                 N_("Industrial Sabotage"));
  actions[ACTION_SPY_INCITE_CITY] = action_new(ACTION_SPY_INCITE_CITY,
                                               ATK_CITY,
                                               N_("Incite a Revolt"));
  actions[ACTION_ESTABLISH_EMBASSY] = action_new(ACTION_ESTABLISH_EMBASSY,
                                                 ATK_CITY,
                                                 N_("Establish Embassy"));
  actions[ACTION_SPY_STEAL_TECH] = action_new(ACTION_SPY_STEAL_TECH,
                                              ATK_CITY,
                                              N_("Steal Technology"));
  actions[ACTION_SPY_TARGETED_STEAL_TECH] =
      action_new(ACTION_SPY_TARGETED_STEAL_TECH, ATK_CITY,
                 N_("Industrial espionage"));
  actions[ACTION_SPY_INVESTIGATE_CITY] =
      action_new(ACTION_SPY_INVESTIGATE_CITY, ATK_CITY,
                 N_("Investigate City"));

  /* Initialize the action enabler list */
  action_iterate(act) {
    action_enablers_by_action[act] = action_enabler_list_new();
  } action_iterate_end;
}

/**************************************************************************
  Free the actions and the action enablers.
**************************************************************************/
void actions_free(void)
{
  action_iterate(act) {
    action_enabler_list_iterate(action_enablers_by_action[act], enabler) {
      requirement_vector_free(&enabler->actor_reqs);
      requirement_vector_free(&enabler->target_reqs);
      free(enabler);
    } action_enabler_list_iterate_end;

    action_enabler_list_destroy(action_enablers_by_action[act]);

    free(actions[act]);
  } action_iterate_end;
}

/**************************************************************************
  Create a new action.
**************************************************************************/
static struct action *action_new(enum gen_action id,
                                 enum action_target_kind target_kind,
                                 const char *ui_name)
{
  struct action *action;

  action = fc_malloc(sizeof(*action));

  action->id = id;
  action->target_kind = target_kind;
  sz_strlcpy(action->ui_name, ui_name);

  return action;
}

/**************************************************************************
  Get the target kind of an action.
**************************************************************************/
enum action_target_kind action_get_target_kind(int action_id)
{
  fc_assert_msg(actions[action_id], "Action %d don't exist.", action_id);

  return actions[action_id]->target_kind;
}

/**************************************************************************
  Get the name shown in the UI for the action.
**************************************************************************/
const char *action_get_ui_name(int action_id)
{
  fc_assert_msg(actions[action_id], "Action %d don't exist.", action_id);

  return actions[action_id]->ui_name;
}

/**************************************************************************
  Create a new action enabler.
**************************************************************************/
struct action_enabler *action_enabler_new(void)
{
  struct action_enabler *enabler;

  enabler = fc_malloc(sizeof(*enabler));
  requirement_vector_init(&enabler->actor_reqs);
  requirement_vector_init(&enabler->target_reqs);

  return enabler;
}

/**************************************************************************
  Add an action enabler.
**************************************************************************/
void action_enabler_add(struct action_enabler *enabler)
{
  action_enabler_list_append(
        action_enablers_for_action(enabler->action),
        enabler);
}

/**************************************************************************
  Some actions have hard requirements that can be expressed as normal
  requirement vectors. Append those to the action enabler so the action
  struct won't need those fields.

  Reconsider this choice if many enablers for each action should become
  common.
**************************************************************************/
void action_enabler_append_hard(struct action_enabler *enabler)
{
  /* All actions that currently use action enablers are spy actions. */
  requirement_vector_append(&enabler->actor_reqs,
                            req_from_str("UnitFlag", "Local", FALSE,
                                         TRUE, "Diplomat"));

  if (enabler->action == ACTION_ESTABLISH_EMBASSY) {
    requirement_vector_append(&enabler->actor_reqs,
                              req_from_str("DiplRel", "Local", FALSE,
                                           FALSE, "Has real embassy"));
  }

  /* The Freeciv code assumes that the victim is alone at its tile. */
  if (enabler->action == ACTION_SPY_BRIBE_UNIT
      || enabler->action == ACTION_SPY_SABOTAGE_UNIT) {
    requirement_vector_append(&enabler->target_reqs,
                              req_from_str("MaxUnitsOnTile", "Local",
                                           FALSE, TRUE, "1"));
  }
}

/**************************************************************************
  Get all enablers for an action.
**************************************************************************/
struct action_enabler_list *
action_enablers_for_action(enum gen_action action)
{
  return action_enablers_by_action[action];
}

/**************************************************************************
  Return TRUE iff the action enabler is active
**************************************************************************/
static bool is_enabler_active(const struct action_enabler *enabler,
			      const struct player *actor_player,
			      const struct city *actor_city,
			      const struct impr_type *actor_building,
			      const struct tile *actor_tile,
			      const struct unit_type *actor_unittype,
			      const struct output_type *actor_output,
			      const struct specialist *actor_specialist,
			      const struct player *target_player,
			      const struct city *target_city,
			      const struct impr_type *target_building,
			      const struct tile *target_tile,
			      const struct unit_type *target_unittype,
			      const struct output_type *target_output,
			      const struct specialist *target_specialist)
{
  return are_reqs_active(actor_player, target_player, actor_city,
                         actor_building, actor_tile, actor_unittype,
                         actor_output, actor_specialist,
                         &enabler->actor_reqs, RPT_CERTAIN)
      && are_reqs_active(target_player, actor_player, target_city,
                         target_building, target_tile, target_unittype,
                         target_output, target_specialist,
                         &enabler->target_reqs, RPT_CERTAIN);
}

/**************************************************************************
  Returns TRUE if the wanted action is enabled by an action enabler.

  Note that the action may disable it self by doing its own tests after
  this returns TRUE. This is because some actions have preconditions
  that can't be expressed in an action enabler's requirement vector.
  Should a precondition become expressible in an action enabler's
  requirement vector please move it.
**************************************************************************/
static bool is_action_enabled(const enum gen_action wanted_action,
			      const struct player *actor_player,
			      const struct city *actor_city,
			      const struct impr_type *actor_building,
			      const struct tile *actor_tile,
			      const struct unit_type *actor_unittype,
			      const struct output_type *actor_output,
			      const struct specialist *actor_specialist,
			      const struct player *target_player,
			      const struct city *target_city,
			      const struct impr_type *target_building,
			      const struct tile *target_tile,
			      const struct unit_type *target_unittype,
			      const struct output_type *target_output,
			      const struct specialist *target_specialist)
{
  action_enabler_list_iterate(action_enablers_for_action(wanted_action),
                              enabler) {
    if (is_enabler_active(enabler, actor_player, actor_city,
                          actor_building, actor_tile, actor_unittype,
                          actor_output, actor_specialist,
                          target_player, target_city,
                          target_building, target_tile, target_unittype,
                          target_output, target_specialist)) {
      return TRUE;
    }
  } action_enabler_list_iterate_end;

  return FALSE;
}

/**************************************************************************
  Returns TRUE if actor_unit can do wanted_action to target_city as far as
  action enablers are concerned.

  See note in is_action_enabled for why the action may still be disabled.
**************************************************************************/
bool is_action_enabled_unit_on_city(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct city *target_city)
{
  fc_assert_ret_val_msg(ATK_CITY == action_get_target_kind(wanted_action),
                        FALSE, "Action %s is against %s not cities",
                        gen_action_name(wanted_action),
                        action_target_kind_name(
                          action_get_target_kind(wanted_action)));

  return is_action_enabled(wanted_action,
                           unit_owner(actor_unit), NULL, NULL,
                           unit_tile(actor_unit), unit_type(actor_unit),
                           NULL, NULL,
                           city_owner(target_city), target_city, NULL,
                           city_tile(target_city), NULL, NULL, NULL);
}

/**************************************************************************
  Returns TRUE if actor_unit can do wanted_action to target_unit as far as
  action enablers are concerned.

  See note in is_action_enabled for why the action may still be disabled.
**************************************************************************/
bool is_action_enabled_unit_on_unit(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct unit *target_unit)
{
  fc_assert_ret_val_msg(ATK_UNIT == action_get_target_kind(wanted_action),
                        FALSE, "Action %s is against %s not units",
                        gen_action_name(wanted_action),
                        action_target_kind_name(
                          action_get_target_kind(wanted_action)));

  return is_action_enabled(wanted_action,
                           unit_owner(actor_unit), NULL, NULL,
                           unit_tile(actor_unit), unit_type(actor_unit),
                           NULL, NULL,
                           unit_owner(target_unit),
                           tile_city(unit_tile(target_unit)), NULL,
                           unit_tile(target_unit), unit_type(target_unit),
                           NULL, NULL);
}

/**************************************************************************
  Find out if the action is enabled, may be enabled or isn't enabled given
  what the player owning the actor knowns.

  A player don't always know everything needed to figure out if an action
  is enabled or not. A server side AI with the same limits on its knowledge
  as a human player or a client should use this to figure out what is what.

  Assumes to be called from the point of view of the actor. Its knowledge
  is assumed to be given in the parameters.

  Returns TRI_YES if the action is enabled, TRI_NO if it isn't and
  TRI_MAYBE if the player don't know enough to tell.

  If meta knowledge is missing TRI_MAYBE will be returned.
**************************************************************************/
static enum fc_tristate
action_enabled_local(const enum gen_action wanted_action,
                     const struct player *actor_player,
                     const struct city *actor_city,
                     const struct impr_type *actor_building,
                     const struct tile *actor_tile,
                     const struct unit *actor_unit,
                     const struct output_type *actor_output,
                     const struct specialist *actor_specialist,
                     const struct player *target_player,
                     const struct city *target_city,
                     const struct impr_type *target_building,
                     const struct tile *target_tile,
                     const struct unit *target_unit,
                     const struct output_type *target_output,
                     const struct specialist *target_specialist)
{
  enum fc_tristate current;
  enum fc_tristate result;

  result = TRI_NO;
  action_enabler_list_iterate(action_enablers_for_action(wanted_action),
                              enabler) {
    current = tri_and(mke_eval_reqs(actor_player, actor_player,
                                    target_player, actor_city,
                                    actor_building, actor_tile,
                                    actor_unit, actor_output,
                                    actor_specialist,
                                    &enabler->actor_reqs),
                      mke_eval_reqs(actor_player, target_player,
                                    actor_player, target_city,
                                    target_building, target_tile,
                                    target_unit, target_output,
                                    target_specialist,
                                    &enabler->target_reqs));
    if (current == TRI_YES) {
      return TRI_YES;
    } else if (current == TRI_MAYBE) {
      result = TRI_MAYBE;
    }
  } action_enabler_list_iterate_end;

  return result;
}

/**************************************************************************
  Does the target has any techs the actor don't?
**************************************************************************/
static enum fc_tristate
tech_can_be_stolen(const struct player *actor_player,
                   const struct player *target_player)
{
  if (can_see_techs_of_target(actor_player, target_player)) {
    advance_iterate(A_FIRST, padvance) {
      Tech_type_id i = advance_number(padvance);

      if (player_invention_state(target_player, i) == TECH_KNOWN
          && player_invention_gettable(actor_player, i,
                                       game.info.tech_steal_allow_holes)
          && (player_invention_state(actor_player, i) == TECH_UNKNOWN
              || (player_invention_state(actor_player, i)
                  == TECH_PREREQS_KNOWN))) {
        return TRI_YES;
      }
    } advance_iterate_end;
  } else {
    return TRI_MAYBE;
  }

  return TRI_NO;
}

/**************************************************************************
  An action's probability of success.
**************************************************************************/
static action_probability
action_prob(const enum gen_action wanted_action,
            const struct player *actor_player,
            const struct city *actor_city,
            const struct impr_type *actor_building,
            const struct tile *actor_tile,
            const struct unit *actor_unit,
            const struct output_type *actor_output,
            const struct specialist *actor_specialist,
            const struct player *target_player,
            const struct city *target_city,
            const struct impr_type *target_building,
            const struct tile *target_tile,
            const struct unit *target_unit,
            const struct output_type *target_output,
            const struct specialist *target_specialist)
{
  int known;
  int chance;

  chance = ACTPROB_NOT_IMPLEMENTED;

  known = action_enabled_local(wanted_action,
                               actor_player, actor_city,
                               actor_building, actor_tile, actor_unit,
                               actor_output, actor_specialist,
                               target_player, target_city,
                               target_building, target_tile, target_unit,
                               target_output, target_specialist);

  switch (wanted_action) {
  case ACTION_SPY_POISON:
    /* TODO */
    break;
  case ACTION_SPY_SABOTAGE_UNIT:
    /* TODO */
    break;
  case ACTION_SPY_BRIBE_UNIT:
    /* TODO */
    break;
  case ACTION_SPY_SABOTAGE_CITY:
    /* TODO */
    break;
  case ACTION_SPY_TARGETED_SABOTAGE_CITY:
    /* TODO */
    break;
  case ACTION_SPY_INCITE_CITY:
    /* TODO */
    break;
  case ACTION_ESTABLISH_EMBASSY:
    chance = 200;
    break;
  case ACTION_SPY_STEAL_TECH:
    /* Do the victim have anything worth taking? */
    known = tri_and(known,
                    tech_can_be_stolen(actor_player, target_player));

    /* TODO: Calculate actual chance */

    break;
  case ACTION_SPY_TARGETED_STEAL_TECH:
    /* Do the victim have anything worth taking? */
    known = tri_and(known,
                    tech_can_be_stolen(actor_player, target_player));

    /* TODO: Calculate actual chance */

    break;
  case ACTION_SPY_INVESTIGATE_CITY:
    /* There is no risk that the city won't get investigated. */
    chance = 200;
    break;
  case ACTION_COUNT:
    fc_assert(FALSE);
    break;
  }

  switch (known) {
  case TRI_NO:
    return ACTPROB_IMPOSSIBLE;
    break;
  case TRI_MAYBE:
    return ACTPROB_NOT_KNOWN;
    break;
  case TRI_YES:
    return chance;
    break;
  };

  fc_assert_ret_val_msg(FALSE, ACTPROB_NOT_IMPLEMENTED,
                        "Should be yes, mabe or no");
}

/**************************************************************************
  Get the actor unit's probability of successfully performing the chosen
  action on the target city.
**************************************************************************/
action_probability action_prob_vs_city(struct unit* actor_unit,
                                       int action_id,
                                       struct city* target_city)
{
  fc_assert_ret_val_msg(ATK_CITY == action_get_target_kind(action_id),
                        FALSE, "Action %s is against %s not cities",
                        gen_action_name(action_id),
                        action_target_kind_name(
                          action_get_target_kind(action_id)));

  return action_prob(action_id,
                     unit_owner(actor_unit), NULL, NULL,
                     unit_tile(actor_unit), actor_unit,
                     NULL, NULL,
                     city_owner(target_city), target_city, NULL,
                     city_tile(target_city), NULL, NULL, NULL);


}

/**************************************************************************
  Get the actor unit's probability of successfully performing the chosen
  action on the target unit.
**************************************************************************/
action_probability action_prob_vs_unit(struct unit* actor_unit,
                                       int action_id,
                                       struct unit* target_unit)
{
  fc_assert_ret_val_msg(ATK_UNIT == action_get_target_kind(action_id),
                        FALSE, "Action %s is against %s not units",
                        gen_action_name(action_id),
                        action_target_kind_name(
                          action_get_target_kind(action_id)));

  return action_prob(action_id,
                     unit_owner(actor_unit), NULL, NULL,
                     unit_tile(actor_unit), actor_unit,
                     NULL, NULL,
                     unit_owner(target_unit),
                     tile_city(unit_tile(target_unit)), NULL,
                     unit_tile(target_unit),
                     target_unit, NULL, NULL);
}
