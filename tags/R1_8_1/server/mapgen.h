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
#ifndef FC__MAPGEN_H
#define FC__MAPGEN_H

void map_fractal_generate(void);
int is_water_adjacent(int x, int y);
void flood_it(int loaded);
void create_start_positions(void);
void adjust_terrain_param(void);

#endif  /* FC__MAPGEN_H */
