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
#ifndef FC__MAPVIEW_H
#define FC__MAPVIEW_H

#include "mapview_g.h"

void pixmap_put_tile(HDC hdc, int x, int y, int abs_x0, int abs_y0,
                     int citymode);
void put_city_tile_output(HDC hdc, int x, int y,
                          int food, int shield, int trade);    
void put_unit_pixmap(struct unit *punit, HDC hdc,int x,int y);
void pixmap_frame_tile_red(HDC hdc, int x, int y);
void put_unit_city_overlays(struct unit *punit, HDC hdc, int x, int y);
void put_one_tile_full(HDC hdc, int x, int y,
		       int canvas_x, int canvas_y, int citymode);
void check_mapstore(void);
void map_resize(void);
void init_map_win(void);
void map_expose(HDC hdc);
void overview_expose(HDC hdc);
void map_handle_hscroll(int pos);
void map_handle_vscroll(int pos);
extern int map_view_width;
extern int map_view_height;

/* These values are stored in the mapview_canvas struct now. */
#define map_view_x mapview_canvas.map_x0
#define map_view_y mapview_canvas.map_y0

#endif  /* FC__MAPVIEW_H */
