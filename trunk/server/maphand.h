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
#ifndef FC__MAPHAND_H
#define FC__MAPHAND_H

#include "map.h"
#include "packets.h"

struct player;
struct section_file;
struct conn_list;

struct dumb_city{
  int id;
  bool has_walls;
  bool occupied;
  char name[MAX_LEN_NAME];
  unsigned short size;
  unsigned char owner;
};

struct player_tile{
  enum tile_terrain_type terrain;
  enum tile_special_type special;
  unsigned short seen;
  unsigned short own_seen;
  /* If you build a city with an unknown square within city radius
     the square stays unknown. However, we still have to keep count
     of the seen points, so they are kept in here. When the tile
     then becomes known they are moved to seen. */
  unsigned short pending_seen;
  struct dumb_city* city;
  short last_updated;
};

void global_warming(int effect);
void nuclear_winter(int effect);
void give_map_from_player_to_player(struct player *pfrom, struct player *pdest);
void give_seamap_from_player_to_player(struct player *pfrom, struct player *pdest);
void give_citymap_from_player_to_player(struct city *pcity,
					struct player *pfrom, struct player *pdest);
void send_all_known_tiles(struct conn_list *dest);
void send_tile_info(struct conn_list *dest, int x, int y);
void reveal_hidden_units(struct player *pplayer, int x, int y);
void unfog_area(struct player *pplayer, int x, int y, int len);
void fog_area(struct player *pplayer, int x, int y, int len);
void upgrade_city_rails(struct player *pplayer, bool discovery);
void send_map_info(struct conn_list *dest);
void map_fog_city_area(struct city *pcity);
void map_unfog_city_area(struct city *pcity);
void remove_unit_sight_points(struct unit *punit);
void show_area(struct player *pplayer,int x, int y, int len);
void map_unfog_pseudo_city_area(struct player *pplayer, int x,int y);
void map_fog_pseudo_city_area(struct player *pplayer, int x,int y);

bool map_get_known_and_seen(int x, int y, struct player *pplayer);
void map_change_seen(int x, int y, struct player *pplayer, int change);
bool map_get_known(int x, int y, struct player *pplayer);
void map_set_known(int x, int y, struct player *pplayer);
void map_clear_known(int x, int y, struct player *pplayer);
void map_know_all(struct player *pplayer);
void map_know_and_see_all(struct player *pplayer);
void show_map_to_all(void);

void player_map_allocate(struct player *pplayer);
void player_map_free(struct player *pplayer);
struct player_tile *map_get_player_tile(int x, int y,
					struct player *pplayer);
void update_tile_knowledge(struct player *pplayer,int x, int y);
void update_player_tile_last_seen(struct player *pplayer, int x, int y);

void give_shared_vision(struct player *pfrom, struct player *pto);
void remove_shared_vision(struct player *pfrom, struct player *pto);
void handle_player_remove_vision(struct player *pplayer,
				 struct packet_generic_integer *packet);

void enable_fog_of_war(void);
void disable_fog_of_war(void);
bool is_coast_seen(int x, int y, struct player *pplayer);
#endif  /* FC__MAPHAND_H */
