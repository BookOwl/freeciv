/**********************************************************************
 Freeciv - Copyright (C) 2002 - The Freeciv Poject
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__MAPCTRL_COMMON_H
#define FC__MAPCTRL_COMMON_H

#include "map.h"		/* enum direction8 */
#include "shared.h"		/* bool type */

#include "control.h"            /* quickselect_type */

extern bool rbutton_down;
extern bool rectangle_active;
extern bool tiles_hilited_cities;

extern bool keyboardless_goto_button_down;
extern bool keyboardless_goto_active;
extern int keyboardless_goto_start_x, keyboardless_goto_start_y;

void anchor_selection_rectangle(int canvas_x, int canvas_y);
void update_selection_rectangle(int canvas_x, int canvas_y);

bool is_city_hilited(struct city *pcity);

void cancel_tile_hiliting(void);
void toggle_tile_hilite(int tile_x, int tile_y);

void clipboard_copy_production(int tile_x, int tile_y);
void clipboard_paste_production(struct city *pcity);
void upgrade_canvas_clipboard(void);

void release_right_button(int canvas_x, int canvas_y);

void release_goto_button(int canvas_x, int canvas_y);
void maybe_activate_keyboardless_goto(int canvas_x, int canvas_y);

bool get_turn_done_button_state(void);
void scroll_mapview(enum direction8 gui_dir);
void action_button_pressed(int canvas_x, int canvas_y,
                enum quickselect_type qtype);
void wakeup_button_pressed(int canvas_x, int canvas_y);
void adjust_workers_button_pressed(int canvas_x, int canvas_y);
void recenter_button_pressed(int canvas_x, int canvas_y);
void update_turn_done_button_state(void);
void update_line(int canvas_x, int canvas_y);
void overview_update_line(int overview_x, int overview_y);
char* popup_info_text(int xtile, int ytile);

bool get_chance_to_win(int *att_chance, int *def_chance,
		       int map_x, int map_y);

extern struct city *city_workers_display;

#endif /* FC__MAPCTRL_COMMON_H */
