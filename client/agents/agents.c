/********************************************************************** 
 Freeciv - Copyright (C) 2001 - R. Falke
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#include <assert.h>
#include <string.h>

#include "capability.h"
#include "hash.h"
#include "mem.h"
#include "civclient.h"
#include "log.h"
#include "clinet.h"
#include "timing.h"
#include "mapctrl_g.h"

#include "cma_core.h"
#include "cma_fec.h"

#include "agents.h"

#define DEBUG_REQUEST_IDS		FALSE
#define DEBUG_TODO_LISTS		FALSE
#define META_CALLBACKS_LOGLEVEL		LOG_DEBUG
#define PRINT_STATS_LOGLEVEL		LOG_DEBUG
#define DEBUG_FREEZE			FALSE
#define MAX_AGENTS			10

struct my_agent;

struct call {
  struct my_agent *agent;
  enum oct { OCT_NEW_TURN, OCT_UNIT, OCT_CITY } type;
  enum callback_type cb_type;
  void *arg;
};

#define SPECLIST_TAG call
#define SPECLIST_TYPE struct call
#include "speclist.h"

#define SPECLIST_TAG call
#define SPECLIST_TYPE struct call
#include "speclist_c.h"

#define call_list_iterate(calllist, pcall) \
    TYPED_LIST_ITERATE(struct call, calllist, pcall)
#define call_list_iterate_end  LIST_ITERATE_END

/*
 * Main data structure. Contains all registered agents and all
 * outstanding calls.
 */
static struct {
  int entries_used;
  struct my_agent {
    struct agent agent;
    int first_outstanding_request_id, last_outstanding_request_id;
    struct {
      struct timer *network_wall_timer;
      int wait_at_network, wait_at_network_requests;
    } stats;
  } entries[MAX_AGENTS];
  struct call_list calls;
} agents;

static bool initialized = FALSE;
static int frozen_level;
static bool currently_running = FALSE;

/***********************************************************************
 Called once per client startup.
***********************************************************************/
void agents_init(void)
{
  agents.entries_used = 0;
  call_list_init(&agents.calls);

  /* Add init calls of agents here */
  cma_init();
  cmafec_init();
}

/***********************************************************************
 Registers an agent.
***********************************************************************/
void register_agent(const struct agent *agent)
{
  struct my_agent *priv_agent = &agents.entries[agents.entries_used];

  assert(agents.entries_used < MAX_AGENTS);
  assert(agent->level > 0);

  memcpy(&priv_agent->agent, agent, sizeof(struct agent));

  priv_agent->first_outstanding_request_id = 0;
  priv_agent->last_outstanding_request_id = 0;

  priv_agent->stats.network_wall_timer = new_timer(TIMER_USER, TIMER_ACTIVE);
  priv_agent->stats.wait_at_network = 0;
  priv_agent->stats.wait_at_network_requests = 0;

  agents.entries_used++;
}

/***********************************************************************
 If the call described by the given arguments isn't contained in
 agents.calls list add the call to this list.
***********************************************************************/
static void enqueue_call(struct my_agent *agent,
			 enum oct type,
			 enum callback_type cb_type, void *arg)
{
  struct call *pcall2;

  if (client_is_observer()) {
    return;
  }

  call_list_iterate(agents.calls, pcall) {
    if (pcall->type == type && pcall->cb_type == cb_type
	&& pcall->arg == arg && pcall->agent == agent) {
      return;
    }
  } call_list_iterate_end;

  set_turn_done_button_state(FALSE);

  pcall2 = fc_malloc(sizeof(struct call));

  pcall2->agent = agent;
  pcall2->type = type;
  pcall2->cb_type = cb_type;
  pcall2->arg = arg;

  call_list_insert(&agents.calls, pcall2);

  if (DEBUG_TODO_LISTS) {
    freelog(LOG_NORMAL, "A: adding call");
  }
}

/***********************************************************************
 Helper.
***********************************************************************/
static int my_call_sort(const void *a, const void *b)
{
  const struct call *c1 = (const struct call *) *(const void **) a;
  const struct call *c2 = (const struct call *) *(const void **) b;

  return c1->agent->agent.level - c2->agent->agent.level;
}

/***********************************************************************
 Return an outstanding call. The call is removed from the agents.calls
 list. Returns NULL if there no more outstanding calls.
***********************************************************************/
static struct call *remove_and_return_a_call(void)
{
  struct call *result;

  if (call_list_size(&agents.calls) == 0) {
    return NULL;
  }

  /* get calls to agents with low levels first */
  call_list_sort(&agents.calls, my_call_sort);

  result = call_list_get(&agents.calls, 0);
  call_list_unlink(&agents.calls, result);

  if (DEBUG_TODO_LISTS) {
    freelog(LOG_NORMAL, "A: removed call");
  }
  return result;
}

/***********************************************************************
 Calls an callback of an agent as described in the given call.
***********************************************************************/
static void execute_call(const struct call *call)
{
  if (call->type == OCT_NEW_TURN) {
    call->agent->agent.turn_start_notify();
  } else if (call->type == OCT_UNIT) {
    call->agent->agent.unit_callbacks[call->
				      cb_type] ((struct unit *) call->arg);
  } else if (call->type == OCT_CITY) {
    call->agent->agent.city_callbacks[call->
				      cb_type] ((struct city *) call->arg);
  } else {
    assert(0);
  }
}

/***********************************************************************
 Execute all outstanding calls. This method will do nothing if the
 dispatching is frozen (frozen_level > 0). Also call_handle_methods
 will ensure that only one instance is running at any given time.
***********************************************************************/
static void call_handle_methods(void)
{
  if (currently_running) {
    return;
  }
  if (frozen_level > 0) {
    return;
  }
  currently_running = TRUE;

  /*
   * The following should ensure that the methods of agents which have
   * a lower level are called first.
   */
  for (;;) {
    struct call *pcall;

    pcall = remove_and_return_a_call();
    if (!pcall) {
      break;
    }

    execute_call(pcall);
    free(pcall);
  }

  currently_running = FALSE;

  if (!agents_busy() && !game.player_ptr->turn_done && !client_is_observer()) {
    set_turn_done_button_state(TRUE);
  }
}

/***********************************************************************
 Increase the frozen_level by one.
***********************************************************************/
static void freeze(void)
{
  if (!initialized) {
    frozen_level = 0;
    initialized = TRUE;
  }
  if (DEBUG_FREEZE) {
    freelog(LOG_NORMAL, "A: freeze() current level=%d", frozen_level);
  }
  frozen_level++;
}

/***********************************************************************
 Decrease the frozen_level by one. If the dispatching is not frozen
 anymore (frozen_level == 0) all outstanding calls are executed.
***********************************************************************/
static void thaw(void)
{
  if (DEBUG_FREEZE) {
    freelog(LOG_NORMAL, "A: thaw() current level=%d", frozen_level);
  }
  frozen_level--;
  assert(frozen_level >= 0);
  if (frozen_level == 0) {
    call_handle_methods();
  }
}

/***********************************************************************
 Helper.
***********************************************************************/
static struct my_agent *find_agent_by_name(char *agent_name)
{
  int i;

  for (i = 0; i < agents.entries_used; i++) {
    if (strcmp(agent_name, agents.entries[i].agent.name) == 0)
      return &agents.entries[i];
  }

  assert(0);
  return NULL;
}

/***********************************************************************
 Returns TRUE iff currently handled packet was caused by the given
 agent.
***********************************************************************/
static bool is_outstanding_request(struct my_agent *agent)
{
  if (agent->first_outstanding_request_id != 0 &&
      aconnection.client.request_id_of_currently_handled_packet != 0 &&
      agent->first_outstanding_request_id <=
      aconnection.client.request_id_of_currently_handled_packet &&
      agent->last_outstanding_request_id >=
      aconnection.client.request_id_of_currently_handled_packet) {
    freelog(LOG_DEBUG,
	    "A:%s: ignoring packet; outstanding [%d..%d] got=%d",
	    agent->agent.name,
	    agent->first_outstanding_request_id,
	    agent->last_outstanding_request_id,
	    aconnection.client.request_id_of_currently_handled_packet);
    return TRUE;
  }
  return FALSE;
}

/***********************************************************************
 Print statistics for the given agent.
***********************************************************************/
static void print_stats(struct my_agent *agent)
{
  freelog(PRINT_STATS_LOGLEVEL,
	  "A:%s: waited %fs in total for network; "
	  "requests=%d; waited %d times",
	  agent->agent.name,
	  read_timer_seconds(agent->stats.network_wall_timer),
	  agent->stats.wait_at_network_requests,
	  agent->stats.wait_at_network);
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_disconnect(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_disconnect()");
  initialized = FALSE;
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_processing_started(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_processing_started()");
  freeze();
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_processing_finished(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_processing_finished()");
  thaw();
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_game_joined(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_game_joined()");
  freeze();
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_game_start(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_game_start()");
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_before_new_turn(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_before_new_turn()");
  freeze();
}

/***********************************************************************
 Called from client/packhand.c.
***********************************************************************/
void agents_start_turn(void)
{
  freelog(META_CALLBACKS_LOGLEVEL, "agents_start_turn()");
  thaw();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_new_turn(void)
{
  int i;

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.turn_start_notify) {
      enqueue_call(agent, OCT_NEW_TURN, CB_LAST, NULL);
    }
  }
  /*
   * call_handle_methods() isn't called here because the agents are
   * frozen anyway.
   */
}

/***********************************************************************
 Called from client/packhand.c. A call is created and added to the
 list of outstanding calls if an agent wants to be informed about this
 event and the change wasn't caused by the agent. We then try (this
 may no be successfull in every case since we can be frozen or another
 call_handle_methods is running higher up on the stack) to execute
 all outstanding calls.
***********************************************************************/
void agents_unit_changed(struct unit *punit)
{
  int i;

  freelog(LOG_DEBUG,
	  "A: agents_unit_changed(unit=%d) type=%s pos=(%d,%d) owner=%s",
	  punit->id, unit_types[punit->type].name, punit->x, punit->y,
	  unit_owner(punit)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.unit_callbacks[CB_CHANGE]) {
      enqueue_call(agent, OCT_UNIT, CB_CHANGE, punit);
    }
  }
  call_handle_methods();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_unit_new(struct unit *punit)
{
  int i;

  freelog(LOG_DEBUG,
	  "A: agents_new_unit(unit=%d) type=%s pos=(%d,%d) owner=%s",
	  punit->id, unit_types[punit->type].name, punit->x, punit->y,
	  unit_owner(punit)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.unit_callbacks[CB_NEW]) {
      enqueue_call(agent, OCT_UNIT, CB_NEW, punit);
    }
  }

  call_handle_methods();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_unit_remove(struct unit *punit)
{
  int i;

  freelog(LOG_DEBUG,
	  "A: agents_remove_unit(unit=%d) type=%s pos=(%d,%d) owner=%s",
	  punit->id, unit_types[punit->type].name, punit->x, punit->y,
	  unit_owner(punit)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.unit_callbacks[CB_REMOVE]) {
      enqueue_call(agent, OCT_UNIT, CB_REMOVE, punit);
    }
  }

  call_handle_methods();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_city_changed(struct city *pcity)
{
  int i;

  freelog(LOG_DEBUG, "A: agents_city_changed(city='%s'(%d)) owner=%s",
	  pcity->name, pcity->id, city_owner(pcity)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.city_callbacks[CB_CHANGE]) {
      enqueue_call(agent, OCT_CITY, CB_CHANGE, pcity);
    }
  }
  call_handle_methods();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_city_new(struct city *pcity)
{
  int i;

  freelog(LOG_DEBUG,
	  "A: agents_city_new(city='%s'(%d)) pos=(%d,%d) owner=%s",
	  pcity->name, pcity->id, pcity->x, pcity->y,
	  city_owner(pcity)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.city_callbacks[CB_NEW]) {
      enqueue_call(agent, OCT_CITY, CB_NEW, pcity);
    }
  }

  call_handle_methods();
}

/***********************************************************************
 Called from client/packhand.c. See agents_unit_changed for a generic
 documentation.
***********************************************************************/
void agents_city_remove(struct city *pcity)
{
  int i;

  freelog(LOG_DEBUG,
	  "A: agents_city_remove(city='%s'(%d)) pos=(%d,%d) owner=%s",
	  pcity->name, pcity->id, pcity->x, pcity->y,
	  city_owner(pcity)->name);

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (is_outstanding_request(agent)) {
      continue;
    }
    if (agent->agent.city_callbacks[CB_REMOVE]) {
      enqueue_call(agent, OCT_CITY, CB_REMOVE, pcity);
    }
  }

  call_handle_methods();
}

/***********************************************************************
 Called from an agent. This function will return until the last
 request has been processed by the server.
***********************************************************************/
void wait_for_requests(char *agent_name, int first_request_id,
		       int last_request_id)
{
  struct my_agent *agent = find_agent_by_name(agent_name);

  if (DEBUG_REQUEST_IDS) {
    freelog(LOG_NORMAL, "A:%s: wait_for_request(ids=[%d..%d])",
	    agent->agent.name, first_request_id, last_request_id);
  }

  assert(first_request_id != 0 && last_request_id != 0
	 && first_request_id <= last_request_id);
  assert(agent->first_outstanding_request_id == 0);
  agent->first_outstanding_request_id = first_request_id;
  agent->last_outstanding_request_id = last_request_id;

  start_timer(agent->stats.network_wall_timer);
  wait_till_request_got_processed(last_request_id);
  stop_timer(agent->stats.network_wall_timer);

  agent->stats.wait_at_network++;
  agent->stats.wait_at_network_requests +=
      (1 + (last_request_id - first_request_id));

  if (DEBUG_REQUEST_IDS) {
    freelog(LOG_NORMAL, "A:%s: wait_for_request: ids=[%d..%d]; got it",
	    agent->agent.name, first_request_id, last_request_id);
  }

  agent->first_outstanding_request_id = 0;

  print_stats(agent);
}

/***********************************************************************
 Adds a specific call for the given agent.
***********************************************************************/
void cause_a_unit_changed_for_agent(char *name_of_calling_agent,
				    struct unit *punit)
{
  struct my_agent *agent = find_agent_by_name(name_of_calling_agent);

  assert(agent->agent.unit_callbacks[CB_CHANGE] != NULL);
  enqueue_call(agent, OCT_UNIT, CB_CHANGE, punit);
  call_handle_methods();
}

/***********************************************************************
 Adds a specific call for the given agent.
***********************************************************************/
void cause_a_city_changed_for_agent(char *name_of_calling_agent,
				    struct city *pcity)
{
  struct my_agent *agent = find_agent_by_name(name_of_calling_agent);

  assert(agent->agent.city_callbacks[CB_CHANGE] != NULL);
  enqueue_call(agent, OCT_CITY, CB_CHANGE, pcity);
  call_handle_methods();
}

/***********************************************************************
 Returns TRUE iff some agent is currently busy.
***********************************************************************/
bool agents_busy(void)
{
  int i;

  if (!initialized || call_list_size(&agents.calls) > 0 || frozen_level > 0
      || currently_running) {
    return TRUE;
  }

  for (i = 0; i < agents.entries_used; i++) {
    struct my_agent *agent = &agents.entries[i];

    if (agent->first_outstanding_request_id != 0) {
      return TRUE;
    }
  }
  return FALSE;
}
