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
#ifndef __MAPVIEW_H
#define __MAPVIEW_H

#include <gtk/gtk.h>
struct unit;
struct city;

void update_info_label(void);
GdkPixmap *get_thumb_pixmap(int onoff);
GdkPixmap *get_citizen_pixmap(int frame);
void set_bulb_sol_government(int bulb, int sol, int government);
void update_turn_done_button(int do_restore);
void update_timeout_label(void);
void update_unit_info_label(struct unit *punit);
int map_canvas_adjust_x(int x);
int map_canvas_adjust_y(int y);
void refresh_tile_mapcanvas(int x, int y, int write_to_screen);
int tile_visible_mapcanvas(int x, int y);
int tile_visible_and_not_on_border_mapcanvas(int x, int y);
void move_unit_map_canvas(struct unit *punit, int x0, int y0, int x1, int y1);
void get_center_tile_mapcanvas(int *x, int *y);
void center_tile_mapcanvas(int x, int y);
void overview_canvas_expose( GtkWidget *widget, GdkEventExpose *event );
void refresh_overview_canvas(void);
void overview_update_tile(int x, int y);
void refresh_overview_viewrect(void);
void map_canvas_expose( GtkWidget *widget, GdkEventExpose *event );
void update_map_canvas(int tile_x, int tile_y, int width, int height, int write_to_screen);

void put_city_pixmap(struct city *pcity, GdkPixmap *pm, int xtile, int ytile);
void put_city_tile_output(GdkPixmap *pm, int x, int y, 
			  int food, int shield, int trade);
void put_unit_pixmap(struct unit *punit, GdkPixmap *pm, int xtile, int ytile);

void put_unit_pixmap_city_overlays(struct unit *punit, GdkPixmap *pm,
				   int unhappiness, int upkeep);
void put_nuke_mushroom_pixmaps(int abs_x0, int abs_y0);
void pixmap_put_black_tile(GdkPixmap *pm, int x, int y);
void pixmap_frame_tile_red(GdkPixmap *pm, int x, int y);
void pixmap_put_tile(GdkPixmap *pm, int x, int y, int map_x, int map_y, 
		     int citymode);

void put_cross_overlay_tile(int x,int y);

void decrease_unit_hp_smooth(struct unit *punit0, int hp0, 
			     struct unit *punit1, int hp1);
void blink_active_unit(void);

void set_overview_dimensions(int x, int y);


/*
void update_unit_pix_label(struct unit *punit);






void my_XawScrollbarSetThumb(Widget w, float top, float shown);
void update_map_canvas_scrollbars(void);
void scrollbar_jump_callback(Widget scrollbar, XtPointer client_data,
			     XtPointer percent_ptr);
void scrollbar_scroll_callback(Widget w, XtPointer client_data,
			     XtPointer position_ptr);

void timer_callback(caddr_t client_data, XtIntervalId *id);
*/
#endif

