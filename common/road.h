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
#ifndef FC__ROAD_H
#define FC__ROAD_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Used in the network protocol. */
#define SPECENUM_NAME road_flag_id
#define SPECENUM_VALUE0 RF_REQUIRES_BRIDGE
/* TRANS: this and following strings are 'road flags', which may rarely
 * be presented to the player in ruleset help text */
#define SPECENUM_VALUE0NAME N_("RequiresBridge")
#define SPECENUM_VALUE1 RF_PREVENTS_OTHER_ROADS
#define SPECENUM_VALUE1NAME N_("PreventsOtherRoads")
#define SPECENUM_VALUE2 RF_RIVER
#define SPECENUM_VALUE2NAME N_("River")
#define SPECENUM_VALUE3 RF_UNRESTRICTED_INFRA
#define SPECENUM_VALUE3NAME N_("UnrestrictedInfra")
#define SPECENUM_COUNT RF_COUNT
#define SPECENUM_BITVECTOR bv_road_flags
#include "specenum_gen.h"

/* Used in the network protocol. */
#define SPECENUM_NAME road_move_mode
#define SPECENUM_VALUE0 RMM_CARDINAL
#define SPECENUM_VALUE0NAME "Cardinal"
#define SPECENUM_VALUE1 RMM_RELAXED
#define SPECENUM_VALUE1NAME "Relaxed"
#define SPECENUM_VALUE2 RMM_FAST_ALWAYS
#define SPECENUM_VALUE2NAME "FastAlways"
#include "specenum_gen.h"

struct road_type;

/* get 'struct road_type_list' and related functions: */
#define SPECLIST_TAG road_type
#define SPECLIST_TYPE struct road_type
#include "speclist.h"

#define road_type_list_iterate(roadlist, proad) \
    TYPED_LIST_ITERATE(struct road_type, roadlist, proad)
#define road_type_list_iterate_end LIST_ITERATE_END

struct extra_type;

struct road_type {
  int id;

  int move_cost;
  enum road_move_mode move_mode;
  int tile_incr_const[O_LAST];
  int tile_incr[O_LAST];
  int tile_bonus[O_LAST];
  enum road_compat compat;

  struct requirement_vector first_reqs;

  bv_roads integrates;
  bv_road_flags flags;

  /* Same information as in integrates, but iterating through this list is much
   * faster than through all road types to check for compatible roads. */
  struct road_type_list *integrators;

  struct extra_type *self;
};

#define ROAD_NONE (-1)

/* General road type accessor functions. */
Road_type_id road_count(void);
Road_type_id road_index(const struct road_type *proad);
Road_type_id road_number(const struct road_type *proad);

struct road_type *road_by_number(Road_type_id id);
struct extra_type *road_extra_get(const struct road_type *proad);

enum road_compat road_compat_special(const struct road_type *proad);
struct road_type *road_by_compat_special(enum road_compat compat);

const char *road_name_translation(struct road_type *proad);
const char *road_rule_name(const struct road_type *proad);
struct road_type *road_type_by_rule_name(const char *name);
struct road_type *road_type_by_translated_name(const char *name);

int count_road_near_tile(const struct tile *ptile, const struct road_type *proad);
int count_river_near_tile(const struct tile *ptile,
                          const struct road_type *priver);
int count_river_type_tile_card(const struct tile *ptile,
                               const struct road_type *priver,
                               bool percentage);
int count_river_type_near_tile(const struct tile *ptile,
                               const struct road_type *priver,
                               bool percentage);

/* Functions to operate on a road flag. */
bool road_has_flag(const struct road_type *proad, enum road_flag_id flag);
bool is_road_flag_card_near(const struct tile *ptile,
                            enum road_flag_id flag);
bool is_road_flag_near_tile(const struct tile *ptile,
                            enum road_flag_id flag);

bool road_can_be_built(const struct road_type *proad, const struct tile *ptile);
bool can_build_road_base(const struct road_type *proad,
                         const struct player *pplayer,
                         const struct tile *ptile);
bool can_build_road(struct road_type *proad,
		    const struct unit *punit,
		    const struct tile *ptile);
bool player_can_build_road(const struct road_type *proad,
			   const struct player *pplayer,
			   const struct tile *ptile);

bool is_native_tile_to_road(const struct road_type *proad,
                            const struct tile *ptile);

bool is_cardinal_only_road(const struct extra_type *pextra);

bool road_provides_move_bonus(const struct road_type *proad);

/* Sorting */
int compare_road_move_cost(const struct road_type *const *p,
                           const struct road_type *const *q);

/* Initialization and iteration */
void road_type_init(struct extra_type *pextra, int idx);
void road_integrators_cache_init(void);
void road_types_free(void);

/* TODO: Change users to use extra_type_by_cause_iterate(EC_ROAD...)
 * directly instead. Especially if they need the 'extra' again and
 * get it back from the _p with road_extra_get() */
#define road_type_iterate(_p)                    \
{                                                \
  extra_type_by_cause_iterate(EC_ROAD, _e_) {    \
    struct road_type *_p = extra_road_get(_e_);

#define road_type_iterate_end                    \
  } extra_type_by_cause_iterate_end              \
}

#define road_deps_iterate(_reqs, _dep)                                 \
{                                                                      \
  requirement_vector_iterate(_reqs, preq) {                            \
    if (preq->source.kind == VUT_EXTRA                                 \
        && preq->present                                               \
        && is_extra_caused_by(preq->source.value.extra, EC_ROAD)) {    \
      struct road_type *_dep = extra_road_get(preq->source.value.extra);


#define road_deps_iterate_end                           \
    }                                                   \
  } requirement_vector_iterate_end;                     \
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__ROAD_H */
