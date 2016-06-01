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

#ifndef FC_ACTIONS_H
#define FC_ACTIONS_H

/* common */
#include "fc_types.h"
#include "metaknowledge.h"
#include "requirements.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SPECENUM_NAME action_actor_kind
#define SPECENUM_VALUE0 AAK_UNIT
#define SPECENUM_VALUE0NAME N_("a unit")
#define SPECENUM_COUNT AAK_COUNT
#include "specenum_gen.h"

#define SPECENUM_NAME action_target_kind
#define SPECENUM_VALUE0 ATK_CITY
#define SPECENUM_VALUE0NAME N_("individual cities")
#define SPECENUM_VALUE1 ATK_UNIT
#define SPECENUM_VALUE1NAME N_("individual units")
#define SPECENUM_COUNT ATK_COUNT
#include "specenum_gen.h"

/* Values used in the network protocol. */
/* Names used in file formats but not normally shown to users. */
#define SPECENUM_NAME gen_action
#define SPECENUM_VALUE0 ACTION_ESTABLISH_EMBASSY
#define SPECENUM_VALUE0NAME "Establish Embassy"
#define SPECENUM_VALUE1 ACTION_SPY_INVESTIGATE_CITY
#define SPECENUM_VALUE1NAME "Investigate City"
#define SPECENUM_VALUE2 ACTION_SPY_POISON
#define SPECENUM_VALUE2NAME "Poison City"
#define SPECENUM_VALUE3 ACTION_SPY_STEAL_GOLD
#define SPECENUM_VALUE3NAME "Steal Gold"
#define SPECENUM_VALUE4 ACTION_SPY_SABOTAGE_CITY
#define SPECENUM_VALUE4NAME "Sabotage City"
#define SPECENUM_VALUE5 ACTION_SPY_TARGETED_SABOTAGE_CITY
#define SPECENUM_VALUE5NAME "Targeted Sabotage City"
#define SPECENUM_VALUE6 ACTION_SPY_STEAL_TECH
#define SPECENUM_VALUE6NAME "Steal Tech"
#define SPECENUM_VALUE7 ACTION_SPY_TARGETED_STEAL_TECH
#define SPECENUM_VALUE7NAME "Targeted Steal Tech"
#define SPECENUM_VALUE8 ACTION_SPY_INCITE_CITY
#define SPECENUM_VALUE8NAME "Incite City"
#define SPECENUM_VALUE9 ACTION_TRADE_ROUTE
#define SPECENUM_VALUE9NAME "Establish Trade Route"
#define SPECENUM_VALUE10 ACTION_MARKETPLACE
#define SPECENUM_VALUE10NAME "Enter Marketplace"
#define SPECENUM_VALUE11 ACTION_HELP_WONDER
#define SPECENUM_VALUE11NAME "Help Wonder"
#define SPECENUM_VALUE12 ACTION_SPY_BRIBE_UNIT
#define SPECENUM_VALUE12NAME "Bribe Unit"
#define SPECENUM_VALUE13 ACTION_SPY_SABOTAGE_UNIT
#define SPECENUM_VALUE13NAME "Sabotage Unit"
#define SPECENUM_COUNT ACTION_COUNT
#include "specenum_gen.h"

/* Used in searches to signal that any action at all is OK. */
#define ACTION_ANY ACTION_COUNT

/* Used in the network protocol */
#define SPECENUM_NAME action_proto_signal
/* The player wants to be reminded to ask what actions the unit can perform
 * to a certain target tile. */
#define SPECENUM_VALUE0 ACTSIG_QUEUE
/* The player no longer wants the reminder to ask what actions the unit can
 * perform to a certain target tile. */
#define SPECENUM_VALUE1 ACTSIG_UNQUEUE
#include "specenum_gen.h"

struct action
{
  enum gen_action id;
  enum action_actor_kind actor_kind;
  enum action_target_kind target_kind;
  bool hostile; /* TODO: Should this be a scale in stead? */

  /* The name of the action shown in the UI */
  char ui_name[MAX_LEN_NAME];

  /* Suppress automatic help text generation about what enables and/or
   * disables this action. */
  bool quiet;
};

struct action_enabler
{
  enum gen_action action;
  struct requirement_vector actor_reqs;
  struct requirement_vector target_reqs;
};

#define SPECLIST_TAG action_enabler
#define SPECLIST_TYPE struct action_enabler
#include "speclist.h"
#define action_enabler_list_iterate(action_enabler_list, aenabler) \
  TYPED_LIST_ITERATE(struct action_enabler, action_enabler_list, aenabler)
#define action_enabler_list_iterate_end LIST_ITERATE_END

#define action_iterate(_act_)                          \
{                                                      \
  int _act_;                                           \
  for (_act_ = 0; _act_ < ACTION_COUNT; _act_++) {

#define action_iterate_end                             \
  }                                                    \
}

#define action_enablers_iterate(_enabler_)               \
{                                                        \
  action_iterate(_act_) {                                \
    action_enabler_list_iterate(                         \
      action_enablers_for_action(_act_), _enabler_) {

#define action_enablers_iterate_end                      \
    } action_enabler_list_iterate_end;                   \
  } action_iterate_end;                                  \
}

/* Initialization */
void actions_init(void);
void actions_free(void);

bool actions_are_ready(void);

bool action_id_is_valid(const int action_id);

struct action *action_by_number(int action_id);

enum action_actor_kind action_get_actor_kind(int action_id);
enum action_target_kind action_get_target_kind(int action_id);

bool action_is_hostile(int action_id);

int action_get_role(int action_id);

const char *action_get_rule_name(int action_id);
const char *action_get_ui_name(int action_id);
const char *action_get_ui_name_mnemonic(int action_id,
                                        const char* mnemonic);
const char *action_prepare_ui_name(int action_id, const char* mnemonic,
                                   const action_probability prob,
                                   const char *custom);
const char *action_get_tool_tip(const int action_id,
                                const action_probability act_prob);

struct action_enabler_list *
action_enablers_for_action(enum gen_action action);

struct action_enabler *action_enabler_new(void);
void action_enabler_add(struct action_enabler *enabler);

bool is_action_enabled_unit_on_city(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct city *target_city);

bool is_action_enabled_unit_on_unit(const enum gen_action wanted_action,
                                    const struct unit *actor_unit,
                                    const struct unit *target_unit);

action_probability action_prob_vs_city(const struct unit* actor,
                                       const int action_id,
                                       const struct city* victim);

action_probability action_prob_vs_unit(const struct unit* actor,
                                       const int action_id,
                                       const struct unit* victim);

bool action_prob_possible(action_probability probability);

/* Reasoning about actions */
bool action_immune_government(struct government *gov, int act);

bool is_action_possible_on_city(const enum gen_action action_id,
                                const struct player *actor_player,
                                const struct city* target_city);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC_ACTIONS_H */
