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
#ifndef FC__AITOOLS_H
#define FC__AITOOLS_H

/* utility */
#include "support.h"            /* bool type */

/* common */
#include "fc_types.h"
#include "unit.h"		/* enum ai_unit_task */

/* server/advisors */
#include "advgoto.h"

/* ai */
#include "aicity.h"
#include "aiunit.h"

struct pf_path;
struct pf_parameter;
struct pft_amphibious;

const char *ai_unit_task_rule_name(const enum ai_unit_task task);
const char *ai_choice_rule_name(const struct ai_choice *choice);

int military_amortize(struct player *pplayer, struct city *pcity, 
                      int value, int delay, int build_cost);
int stack_cost(struct unit *pattacker, struct unit *pdefender);

void ai_unit_move_or_attack(struct unit *punit, struct tile *ptile,
                            struct pf_path *path, int step);

void ai_fill_unit_param(struct pf_parameter *parameter,
			struct adv_risk_cost *risk_cost,
			struct unit *punit, struct tile *ptile);
bool ai_gothere(struct player *pplayer, struct unit *punit, 
                struct tile *dst_tile);
struct tile *immediate_destination(struct unit *punit,
				   struct tile *dest_tile);
void ai_log_path(struct unit *punit,
		 struct pf_path *path, struct pf_parameter *parameter);
bool ai_unit_goto_constrained(struct unit *punit, struct tile *ptile,
			      struct pf_parameter *parameter);
bool ai_unit_goto(struct unit *punit, struct tile *ptile);
bool goto_is_sane(struct unit *punit, struct tile *ptile, bool omni);

void ai_unit_new_task(struct unit *punit, enum ai_unit_task task, 
                      struct tile *ptile);
void ai_unit_new_adv_task(struct unit *punit, enum adv_unit_task task,
                          struct tile *ptile);
bool ai_unit_make_homecity(struct unit *punit, struct city *pcity);
bool ai_unit_attack(struct unit *punit, struct tile *ptile);
bool ai_unit_move(struct unit *punit, struct tile *ptile);

void ai_government_change(struct player *pplayer, struct government *gov);
int ai_gold_reserve(struct player *pplayer);

void init_choice(struct ai_choice *choice);
void adjust_choice(int value, struct ai_choice *choice);
void copy_if_better_choice(struct ai_choice *cur, struct ai_choice *best);

bool is_unit_choice_type(enum choice_type type);

bool ai_choose_role_unit(struct player *pplayer, struct city *pcity,
                         struct ai_choice *choice, enum choice_type type,
                         int role, int want, bool need_boat);
void ai_build_adv_override(struct city *pcity, struct ai_choice *choice);
bool ai_assess_military_unhappiness(struct city *pcity);

void ai_consider_plr_dangerous(struct player *plr1, struct player *plr2,
                               enum danger_consideration *result);

#endif  /* FC__AITOOLS_H */
