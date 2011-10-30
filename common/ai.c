/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <string.h>

/* utility */
#include "fcintl.h"
#include "log.h"                /* fc_assert */
#include "mem.h"
#include "timing.h"

/* common */
#include "ai.h"
#include "player.h"

static struct ai_type ai_types[FC_AI_LAST];

static int ai_type_count = 0;

#ifdef DEBUG_AITIMERS
struct ai_timer {
  int count;
  struct timer *timer;
};

static struct ai_timer *ai_timer_get(const struct ai_type *ai);
static struct ai_timer *ai_timer_player_get(const struct player *pplayer);

static struct ai_timer *aitimers = NULL;
static struct ai_timer *aitimer_plrs = NULL;

/*****************************************************************************
  Allocate memory for Start the timer for the AI of a player.
*****************************************************************************/
void ai_timer_init(void)
{
  int i;

  fc_assert_ret(aitimers == NULL);
  fc_assert_ret(aitimer_plrs == NULL);

  aitimers = fc_calloc(FC_AI_LAST, sizeof(*aitimers));
  for (i = 0; i < FC_AI_LAST; i++) {
    struct ai_timer *aitimer = aitimers + i;
    aitimer->count = 0;
    aitimer->timer = NULL;
  }

  aitimer_plrs = fc_calloc(FC_AI_LAST * MAX_NUM_PLAYER_SLOTS,
                           sizeof(*aitimer_plrs));
  for (i = 0; i < FC_AI_LAST * MAX_NUM_PLAYER_SLOTS; i++) {
    struct ai_timer *aitimer = aitimer_plrs + i;
    aitimer->count = 0;
    aitimer->timer = NULL;
  }
}

/*****************************************************************************
  Allocate memory for Start the timer for the AI of a player.
*****************************************************************************/
void ai_timer_free(void)
{
  int i,j;
  struct ai_timer *aitimer;

  for (i = 0; i < FC_AI_LAST; i++) {
    struct ai_type *ai = get_ai_type(i);

    aitimer = aitimers + i;

    if (aitimer->timer) {
      log_normal("AI timer stats: [%15.3f] ---- (AI type: %s)",
                 read_timer_seconds(aitimer->timer), ai_name(ai));
      free_timer(aitimer->timer);
    }

    for (j = 0; j < MAX_NUM_PLAYER_SLOTS; j++) {
      aitimer = aitimer_plrs + j * FC_AI_LAST + i;

      if (aitimer->timer) {
        log_normal("AI timer stats: [%15.3f] P%03d (AI type: %s)",
                   read_timer_seconds(aitimer->timer), j, ai_name(ai));
        free_timer(aitimer->timer);
      }
    }
  }
  free(aitimers);
  aitimers = NULL;

  free(aitimer_plrs);
  aitimer_plrs = NULL;
}

/*****************************************************************************
  Get the timer for the AI.
*****************************************************************************/
static struct ai_timer *ai_timer_get(const struct ai_type *ai)
{
  struct ai_timer *aitimer;

  fc_assert_ret_val(ai != NULL, NULL);

  fc_assert_ret_val(aitimers != NULL, NULL);

  aitimer = aitimers + ai_type_number(ai);

  if (!aitimer->timer) {
    aitimer->timer = new_timer(TIMER_CPU, TIMER_DEBUG);
  }

  return aitimer;
}

/*****************************************************************************
  Get the timer for the AI of a player.
*****************************************************************************/
static struct ai_timer *ai_timer_player_get(const struct player *pplayer)
{
  struct ai_timer *aitimer;

  fc_assert_ret_val(pplayer != NULL, NULL);
  fc_assert_ret_val(pplayer->ai != NULL, NULL);

  fc_assert_ret_val(aitimer_plrs != NULL, NULL);

  aitimer = aitimer_plrs + (player_index(pplayer) * FC_AI_LAST
                            + ai_type_number(pplayer->ai));

  if (!aitimer->timer) {
    aitimer->timer = new_timer(TIMER_CPU, TIMER_DEBUG);
  }

  return aitimer;
}

/*****************************************************************************
  Start the timer for the AI.
*****************************************************************************/
void ai_timer_start(const struct ai_type *ai)
{
  struct ai_timer *aitimer = ai_timer_get(ai);

  fc_assert_ret(aitimer != NULL);
  fc_assert_ret(aitimer->timer != NULL);

  if (aitimer->count == 0) {
    log_debug("AI timer start  [%15.3f] ---- (AI type: %s)",
              read_timer_seconds(aitimer->timer), ai_name(ai));
    start_timer(aitimer->timer);
  } else {
    log_debug("AI timer =====> [depth: %3d]      ---- (AI type: %s)",
              aitimer->count, ai_name(ai));
  }
  aitimer->count++;
}

/*****************************************************************************
  Stop the timer for the AI.
*****************************************************************************/
void ai_timer_stop(const struct ai_type *ai)
{
  struct ai_timer *aitimer = ai_timer_get(ai);

  fc_assert_ret(aitimer != NULL);
  fc_assert_ret(aitimer->timer != NULL);

  if (aitimer->count > 0) {
    if (aitimer->count == 1) {
      stop_timer(aitimer->timer);
      log_debug("AI timer stop   [%15.3f] ---- (AI type: %s)",
                read_timer_seconds(aitimer->timer), ai_name(ai));
    } else {
      log_debug("AI timer =====> [depth: %3d]      ---- (AI type: %s)",
                aitimer->count, ai_name(ai));
    }
    aitimer->count--;
  } else {
    log_debug("AI timer missing?                 ---- (AI type: %s)",
              ai_name(ai));
  }
}

/*****************************************************************************
  Start the timer for the AI of a player.
*****************************************************************************/
void ai_timer_player_start(const struct player *pplayer)
{
  struct ai_timer *aitimer = ai_timer_player_get(pplayer);

  fc_assert_ret(aitimer != NULL);
  fc_assert_ret(aitimer->timer != NULL);

  if (aitimer->count == 0) {
    log_debug("AI timer start  [%15.3f] P%03d (AI type: %s) %s",
              read_timer_seconds(aitimer->timer), player_index(pplayer),
              pplayer->ai->name, player_name(pplayer));
    start_timer(aitimer->timer);
  } else {
    log_debug("AI timer =====> [depth: %3d]      P%03d (AI type: %s) %s",
              aitimer->count, player_index(pplayer), pplayer->ai->name,
              player_name(pplayer));
  }
  aitimer->count++;
}

/*****************************************************************************
  Stop the timer for the AI of a player.
*****************************************************************************/
void ai_timer_player_stop(const struct player *pplayer)
{
  struct ai_timer *aitimer = ai_timer_player_get(pplayer);

  fc_assert_ret(aitimer != NULL);
  fc_assert_ret(aitimer->timer != NULL);

  if (aitimer->count > 0) {
    if (aitimer->count == 1) {
      stop_timer(aitimer->timer);
      log_debug("AI timer stop   [%15.3f] P%03d (AI type: %s) %s",
                read_timer_seconds(aitimer->timer), player_index(pplayer),
                pplayer->ai->name, player_name(pplayer));
    } else {
      log_debug("AI timer =====> [depth: %3d]      P%03d (AI type: %s) %s",
                aitimer->count, player_index(pplayer), pplayer->ai->name,
                player_name(pplayer));
    }
    aitimer->count--;
  } else {
    log_debug("AI timer missing?                 P%03d (AI type: %s) %s",
              player_index(pplayer), pplayer->ai->name,
              player_name(pplayer));
  }
}
#endif /* DEBUG_AITIMERS */

/***************************************************************
  Returns ai_type of given id.
***************************************************************/
struct ai_type *get_ai_type(int id)
{
  fc_assert(id >= 0 && id < FC_AI_LAST);

  return &ai_types[id];
}

/***************************************************************
  Initializes AI structure.
***************************************************************/
void init_ai(struct ai_type *ai)
{
  memset(ai, 0, sizeof(*ai));
}

/***************************************************************
  Returns id of the given ai_type.
***************************************************************/
int ai_type_number(const struct ai_type *ai)
{
  int ainbr = ai - ai_types;

  fc_assert_ret_val(ainbr >= 0 && ainbr < FC_AI_LAST, 0);

  return ainbr;
}

/***************************************************************
  Find ai type with given name.
***************************************************************/
struct ai_type *ai_type_by_name(const char *search)
{
  size_t len = strlen(search);

  ai_type_iterate(ai) {
    if (!strncmp(ai->name, search, len)) {
      return ai;
    }
  } ai_type_iterate_end;

  return NULL;
}

/***************************************************************
  Return next free ai_type
***************************************************************/
struct ai_type *ai_type_alloc(void)
{
  if (ai_type_count >= FC_AI_LAST) {
    log_error(_("Too many AI modules. Max is %d."), FC_AI_LAST);
    return NULL;
  }

  return get_ai_type(ai_type_count++);
}

/***************************************************************
  Return number of ai types
***************************************************************/
int ai_type_get_count(void)
{
  return ai_type_count;
}
