/**********************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__API_FIND_H
#define FC__API_FIND_H

#include "api_types.h"

/* Object find module. */
Player *api_find_player(int player_id);
City *api_find_city(Player *pplayer, int city_id);
Unit *api_find_unit(Player *pplayer, int unit_id);
Tile *api_find_tile(int nat_x, int nat_y);
Impr_Type *api_find_impr_type(int impr_type_id);
Nation_Type *api_find_nation_type(int nation_type_id);
Unit_Type *api_find_unit_type(int unit_type_id);

#endif

