/********************************************************************** 
 Freeciv Generator - Copyright (C) 2004 - Marcelo J. Burda
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__UTILITIES_H
#define FC__UTILITIES_H

/* Provide a block to convert from native to map coordinates.  For instance
 *   do_in_map_pos(mx, my, xn, yn) {
 *     map_set_terrain(mx, my, T_OCEAN);
 *   } do_in_map_pos_end;
 * Note: that the map position is declared as const and can't be changed
 * inside the block.
 */
#define do_in_map_pos(map_x, map_y, nat_x, nat_y)                           \
{                                                                           \
  int _tmp_x = (nat_x), _tmp_y = (nat_y);                                   \
  NATIVE_TO_MAP_POS(&_tmp_x, &_tmp_y, _tmp_x, _tmp_y);                      \
  {                                                                         \
      const int map_x = _tmp_x, map_y = _tmp_y;

#define do_in_map_pos_end                                                   \
  }                                                                         \
}

/***************************************************************************
 iterate axe iterate on selected axe ( x if Xaxe is TRUE) over a intervale
 of -dist to dist arround the tile indexed by index0
 this iterator create 2 vars:
 index : the map index of the iterate pointed tile
 i : the position in the intervale of iteration (from -dist to dist)
 index0, dist, Xaxe are side effect safe.
 ***************************************************************************/
#define iterate_axe(index, i, index0, dist, Xaxe) \
{ \
    const int ___dist = (dist), ___index0 = (index0); \
    const bool ___Xaxe = (Xaxe); \
    int i, index; \
    int ___x, ___y, ___x0, ___y0; \
  \
    index_to_native_pos(&___x0, &___y0, ___index0); \
    for (i = -___dist; i <= ___dist; i++) { \
	___x = ___x0 + (___Xaxe ? i : 0); \
	___y = ___y0 + (___Xaxe ? 0 : i); \
	if(!normalize_nat_pos(&___x, &___y)) { \
	    continue; \
	}; \
     index = native_pos_to_index( ___x, ___y);

#define iterate_axe_end \
    } \
} 

#define whole_map_iterate_index( index )  \
{                                 \
  int index = 0;                  \
		                  \
  for (; index < MAX_MAP_INDEX; index++) {

#define whole_map_iterate_index_end \
   } \
} 

bool normalize_nat_pos(int *x, int  *y);
bool is_normal_nat_pos(int x, int y);

/* int maps tools */
void adjust_int_map(int *int_map, int int_map_max);
void smooth_int_map(int *int_map, bool zeroes_at_edges);

/* placed_map tool*/
void create_placed_map(void);
void destroy_placed_map(void);
void map_set_placed(int x, int y);
void map_unset_placed(int x, int y);
bool not_placed(int x, int y);
bool placed_map_is_initialized(void);
void set_all_ocean_tiles_placed(void) ;
void set_placed_near_pos(int map_x, int map_y, int dist);



#endif  /* FC__UTILITIES_H */
