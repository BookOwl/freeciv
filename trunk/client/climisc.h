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
#ifndef FC__CLIMISC_H
#define FC__CLIMISC_H

#include "shared.h"		/* MAX_LEN_NAME */

#include "fc_types.h"
#include "events.h"

struct Clause;

typedef int cid;
typedef int wid;

void client_remove_player(int plrno);
void client_remove_city(struct city *pcity);
void client_remove_unit(struct unit *punit);

void client_change_all(cid x, cid y);

const char *get_embassy_status(const struct player *me,
				const struct player *them);
const char *get_vision_status(const struct player *me,
				const struct player *them);
void client_diplomacy_clause_string(char *buf, int bufsiz,
				    struct Clause *pclause);

struct sprite *client_research_sprite(void);
struct sprite *client_warming_sprite(void);
struct sprite *client_cooling_sprite(void);
struct sprite *client_government_sprite(void);

void center_on_something(void);

/* 
 * A compound id (cid) can hold all objects a city can build:
 * improvements (with wonders) and units. This is achieved by
 * seperation the value set: a cid < B_LAST denotes a improvement
 * (including wonders). A cid >= B_LAST denotes a unit with the
 * unit_type_id of (cid - B_LAST).
 */

cid cid_encode(struct city_production target);
cid cid_encode_unit(const struct unit_type *punittype);
cid cid_encode_building(Impr_type_id building);
cid cid_encode_from_city(const struct city *pcity);

struct city_production cid_decode(cid cid);
#define cid_production cid_decode

/* 
 * A worklist id (wid) can hold all objects which can be part of a
 * city worklist: improvements (with wonders), units and global
 * worklists. This is achieved by seperation the value set: 
 *  - (wid < B_LAST) denotes a improvement (including wonders)
 *  - (B_LAST <= wid < B_LAST + U_LAST) denotes a unit with the
 *  unit_type_id of (wid - B_LAST)
 *  - (B_LAST + U_LAST<= wid) denotes a global worklist with the id of
 *  (wid - (B_LAST + U_LAST))
 */

#define WORKLIST_END (-1)

wid wid_encode(bool is_unit, bool is_worklist, int id);
bool wid_is_unit(wid wid);
bool wid_is_worklist(wid wid);
int wid_id(wid wid);

bool city_can_build_impr_or_unit(const struct city *pcity,
				 struct city_production target);
bool city_unit_supported(const struct city *pcity,
			 struct city_production target);
bool city_unit_present(const struct city *pcity,
		       struct city_production target);
bool city_building_present(const struct city *pcity,
			   struct city_production target);

struct item {
  cid cid;
  char descr[MAX_LEN_NAME + 40];
};

typedef bool (*TestCityFunc)(const struct city *, struct city_production);

void name_and_sort_items(int *pcids, int num_cids, struct item *items,
			 bool show_cost, struct city *pcity);
int collect_cids1(cid * dest_cids, struct city **selected_cities,
		 int num_selected_cities, bool append_units,
		 bool append_wonders, bool change_prod,
		  TestCityFunc test_func);
int collect_cids2(cid * dest_cids);
int collect_cids3(cid * dest_cids);
int collect_cids4(cid * dest_cids, struct city *pcity, bool advanced_tech);
int collect_cids5(cid * dest_cids, struct city *pcity);
int collect_wids1(wid * dest_wids, struct city *pcity, bool wl_first, 
		  bool advanced_tech);

/* the number of units in city */
int num_present_units_in_city(struct city* pcity);
int num_supported_units_in_city(struct city* pcity);	

void handle_event(char *message, struct tile *ptile,
		  enum event_type event, int conn_id);
void create_event(struct tile *ptile, enum event_type event,
		  const char *format, ...)
     fc__attribute((format (printf, 3, 4)));
void write_chatline_content(const char *txt);

void reports_freeze(void);
void reports_freeze_till(int request_id);
void reports_thaw(void);
void reports_force_thaw(void);

struct city *get_nearest_city(const struct unit *punit, int *sq_dist);

void cityrep_buy(struct city *pcity);
void common_taxrates_callback(int i);

#endif  /* FC__CLIMISC_H */
