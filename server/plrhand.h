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
#ifndef FC__PLRHAND_H
#define FC__PLRHAND_H

#include <stdarg.h>

#include "events.h"
#include "packets.h"
#include "shared.h"		/* fc__attribute */

#include "hand_gen.h"

struct player;
struct section_file;
struct connection;
struct conn_list;

enum plr_info_level { INFO_MINIMUM, INFO_MEETING, INFO_EMBASSY, INFO_FULL };

void server_player_init(struct player *pplayer, bool initmap);
void server_remove_player(struct player *pplayer);
void kill_player(struct player *pplayer);
void begin_player_turn(struct player *pplayer);
void kill_dying_players(void);
void update_revolution(struct player *pplayer);
void great_library(struct player *pplayer);
void check_player_government_rates(struct player *pplayer);
void make_contact(struct player *pplayer1, struct player *pplayer2, int x,
		  int y);
void maybe_make_contact(int x, int y, struct player *pplayer);

void send_player_info(struct player *src, struct player *dest);
void send_player_info_c(struct player *src, struct conn_list *dest);

void notify_conn_ex(struct conn_list *dest, int x, int y,
		    enum event_type event, const char *format, ...)
                    fc__attribute((format (printf, 5, 6)));
void vnotify_conn_ex(struct conn_list *dest, int x, int y,
		     enum event_type event, const char *format,
		     va_list vargs);
void notify_conn(struct conn_list *dest, const char *format, ...) 
                 fc__attribute((format (printf, 2, 3)));
void notify_player_ex(const struct player *pplayer, int x, int y,
		      enum event_type event, const char *format, ...)
                      fc__attribute((format (printf, 5, 6)));
void notify_player(const struct player *pplayer, const char *format, ...)
                   fc__attribute((format (printf, 2, 3)));
void notify_embassies(struct player *pplayer, struct player *exclude,
		      const char *format, ...)
		      fc__attribute((format (printf, 3, 4)));

struct conn_list *player_reply_dest(struct player *pplayer);

void found_new_tech(struct player *plr, int tech_found, bool was_discovery,
                    bool saving_bulbs, int next_tech);
void found_new_future_tech(struct player *pplayer);
void update_tech(struct player *plr, int bulbs);
void init_tech(struct player *plr, int tech);
void choose_random_tech(struct player *plr);
void choose_tech(struct player *plr, int tech);
void choose_tech_goal(struct player *plr, int tech);
void get_a_tech(struct player *pplayer, struct player *target);

void send_player_turn_notifications(struct conn_list *dest);

void do_dipl_cost(struct player *pplayer);
void do_free_cost(struct player *pplayer);
void do_conquer_cost(struct player *pplayer);

void shuffle_players(void);
struct player *shuffled_player(int i);

#define shuffled_players_iterate(pplayer)                                   \
{                                                                           \
  struct player *pplayer;                                                   \
  int i;                                                                    \
  for (i = 0; i < game.nplayers; i++) {                                     \
    pplayer = shuffled_player(i);                                           \
    {

#define shuffled_players_iterate_end                                        \
    }                                                                       \
  }                                                                         \
}

bool civil_war_triggered(struct player *pplayer);
void civil_war(struct player *pplayer);

#endif  /* FC__PLRHAND_H */
