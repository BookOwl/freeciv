/********************************************************************** 
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

#include <ctype.h>
#include <string.h>

/* utility */
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "support.h"
#include "shared.h" /* ARRAY_SIZE */

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "improvement.h"
#include "map.h"
#include "packets.h"
#include "player.h"
#include "tech.h"

#include "effects.h"


static bool initialized = FALSE;

/**************************************************************************
  The code creates a ruleset cache on ruleset load. This constant cache
  is used to speed up effects queries.  After the cache is created it is
  not modified again (though it may later be freed).

  Since the cache is constant, the server only needs to send effects data to
  the client upon connect. It also means that an AI can do fast searches in
  the effects space by trying the possible combinations of addition or
  removal of buildings with the effects it cares about.


  To know how much a target is being affected, simply use the convenience
  functions:

  * get_player_bonus
  * get_city_bonus
  * get_city_tile_bonus
  * get_building_bonus

  These functions require as arguments the target and the effect type to be
  queried.

  Effect sources are unique and at a well known place in the
  data structures.  This allows the above queries to be fast:
    - Look up the possible sources for the effect (O(1) lookup)
    - For each source, find out if it is present (O(1) lookup per source).
  The first is commonly called the "ruleset cache" and is stored statically
  in this file.  The second is the "sources cache" and is stored all over.

  Any type of effect range and "survives" is possible if we have a sources
  cache for that combination.  For instance
    - There is a sources cache of all existing buildings in a city; thus any
      building effect in a city can have city range.
    - There is a sources cache of all wonders in the world; thus any wonder
      effect can have world range.
    - There is a sources cache of all wonders for each player; thus any
      wonder effect can have player range.
    - There is a sources cache of all wonders ever built; thus any wonder
      effect that survives can have world range.
  However there is no sources cache for many of the possible sources.  For
  instance non-unique buildings do not have a world-range sources cahce, so
  you can't have a non-wonder building have a world-ranged effect.

  The sources caches could easily be extended by generalizing it to a set
  of arrays
    game.buildings[], pplayer->buildings[],
    pisland->builidngs[], pcity->buildings[]
  which would store the number of buildings of that type present by game,
  player, island (continent) or city.  This would allow non-surviving effects
  to come from any building at any range.  However to allow surviving effects
  a second set of arrays would be needed.  This should enable basic support
  for small wonders and satellites.

  No matter which sources caches are present, we should always know where
  to look for a source and so the lookups will always be fast even as the
  number of possible sources increases.
**************************************************************************/

/**************************************************************************
  Ruleset cache. The cache is created during ruleset loading and the data
  is organized to enable fast queries.
**************************************************************************/
static struct {
  /* A single list containing every effect. */
  struct effect_list *tracker;

  /* This array provides a full list of the effects of this type
   * (It's not really a cache, it's the real data.) */
  struct effect_list *effects[EFT_COUNT];

  struct {
    /* This cache shows for each building, which effects it provides. */
    struct effect_list *buildings[B_LAST];
    /* Same for governments */
    struct effect_list *govs[G_MAGIC];
    /* ...advances... */
    struct effect_list *advances[A_LAST];
  } reqs;
} ruleset_cache;


/**************************************************************************
  Get a list of effects of this type.
**************************************************************************/
struct effect_list *get_effects(enum effect_type effect_type)
{
  return ruleset_cache.effects[effect_type];
}

/**************************************************************************
  Get a list of effects with this requirement source.

  Note: currently only buildings and governments are supported.
**************************************************************************/
struct effect_list *get_req_source_effects(struct universal *psource)
{
  int type, value;

  universal_extraction(psource, &type, &value);

  switch (type) {
  case VUT_GOVERNMENT:
    if (value >= 0 && value < government_count()) {
      return ruleset_cache.reqs.govs[value];
    } else {
      return NULL;
    }
  case VUT_IMPROVEMENT:
    if (value >= 0 && value < improvement_count()) {
      return ruleset_cache.reqs.buildings[value];
    } else {
      return NULL;
    }
  case VUT_ADVANCE:
    if (value >= 0 && value < advance_count()) {
      return ruleset_cache.reqs.advances[value];
    } else {
      return NULL;
    }
  default:
    return NULL;
  }
}

/**************************************************************************
  Add effect to ruleset cache.
**************************************************************************/
struct effect *effect_new(enum effect_type type, int value)
{
  struct effect *peffect;

  /* Create the effect. */
  peffect = fc_malloc(sizeof(*peffect));
  peffect->type = type;
  peffect->value = value;

  requirement_vector_init(&peffect->reqs);

  /* Now add the effect to the ruleset cache. */
  effect_list_append(ruleset_cache.tracker, peffect);
  effect_list_append(get_effects(type), peffect);
  return peffect;
}

/**************************************************************************
  Append requirement to effect.
**************************************************************************/
void effect_req_append(struct effect *peffect, struct requirement *preq)
{
  struct effect_list *eff_list = get_req_source_effects(&preq->source);

  requirement_vector_append(&peffect->reqs, *preq);

  if (eff_list) {
    effect_list_append(eff_list, peffect);
  }
}

/**************************************************************************
  Initialize the ruleset cache.  The ruleset cache should be empty
  before this is done (so if it's previously been initialized, it needs
  to be freed (see ruleset_cache_free) before it can be reused).
**************************************************************************/
void ruleset_cache_init(void)
{
  int i;

  initialized = TRUE;

  ruleset_cache.tracker = effect_list_new();

  for (i = 0; i < ARRAY_SIZE(ruleset_cache.effects); i++) {
    ruleset_cache.effects[i] = effect_list_new();
  }
  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.buildings); i++) {
    ruleset_cache.reqs.buildings[i] = effect_list_new();
  }
  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.govs); i++) {
    ruleset_cache.reqs.govs[i] = effect_list_new();
  }
  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.advances); i++) {
    ruleset_cache.reqs.advances[i] = effect_list_new();
  }
}

/**************************************************************************
  Free the ruleset cache.  This should be called at the end of the game or
  when the client disconnects from the server.  See ruleset_cache_init.
**************************************************************************/
void ruleset_cache_free(void)
{
  int i;
  struct effect_list *plist = ruleset_cache.tracker;

  if (plist) {
    effect_list_iterate(plist, peffect) {
      requirement_vector_free(&peffect->reqs);
      free(peffect);
    } effect_list_iterate_end;
    effect_list_destroy(plist);
    ruleset_cache.tracker = NULL;
  }

  for (i = 0; i < ARRAY_SIZE(ruleset_cache.effects); i++) {
    struct effect_list *plist = ruleset_cache.effects[i];

    if (plist) {
      effect_list_destroy(plist);
      ruleset_cache.effects[i] = NULL;
    }
  }

  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.buildings); i++) {
    struct effect_list *plist = ruleset_cache.reqs.buildings[i];

    if (plist) {
      effect_list_destroy(plist);
      ruleset_cache.reqs.buildings[i] = NULL;
    }
  }

  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.govs); i++) {
    struct effect_list *plist = ruleset_cache.reqs.govs[i];

    if (plist) {
      effect_list_destroy(plist);
      ruleset_cache.reqs.govs[i] = NULL;
    }
  }

  for (i = 0; i < ARRAY_SIZE(ruleset_cache.reqs.advances); i++) {
    struct effect_list *plist = ruleset_cache.reqs.advances[i];

    if (plist) {
      effect_list_destroy(plist);
      ruleset_cache.reqs.advances[i] = NULL;
    }
  }

  initialized = FALSE;
}

/****************************************************************************
  Get the maximum effect value in this ruleset.
****************************************************************************/
int effect_cumulative_max(enum effect_type type)
{
  struct effect_list *plist = ruleset_cache.tracker;
  int value = 0;

  if (plist) {
    effect_list_iterate(plist, peffect) {
      if (peffect->type == type && peffect->value > 0) {
        value += peffect->value;
      }
    } effect_list_iterate_end;
  }

  return value;
}

/****************************************************************************
  Get the minimum effect value in this ruleset.
****************************************************************************/
int effect_cumulative_min(enum effect_type type)
{
  struct effect_list *plist = ruleset_cache.tracker;
  int value = 0;

  if (plist) {
    effect_list_iterate(plist, peffect) {
      if (peffect->type == type && peffect->value < 0) {
        value += peffect->value;
      }
    } effect_list_iterate_end;
  }

  return value;
}

/****************************************************************************
  Receives a new effect.  This is called by the client when the packet
  arrives.
****************************************************************************/
void recv_ruleset_effect(const struct packet_ruleset_effect *packet)
{
  struct effect *peffect;
  int i;
  struct requirement *preq;

  peffect = effect_new(packet->effect_type, packet->effect_value);

  for (i = 0; i < packet->reqs_count; i++) {
    preq = fc_malloc(sizeof(*preq));
    *preq = packet->reqs[i];
    effect_req_append(peffect, preq);
  }
  fc_assert(peffect->reqs.size == packet->reqs_count);
}

/**************************************************************************
  Send the ruleset cache data over the network.
**************************************************************************/
void send_ruleset_cache(struct conn_list *dest)
{
  effect_list_iterate(ruleset_cache.tracker, peffect) {
    struct packet_ruleset_effect effect_packet;
    int counter;

    effect_packet.effect_type = peffect->type;
    effect_packet.effect_value = peffect->value;

    counter = 0;
    requirement_vector_iterate(&(peffect->reqs), req) {
      effect_packet.reqs[counter++] = *req;
    } requirement_vector_iterate_end;
    effect_packet.reqs_count = counter;

    lsend_packet_ruleset_effect(dest, &effect_packet);
  } effect_list_iterate_end;
}

/**************************************************************************
  Returns TRUE if the building has any effect bonuses of the given type.

  Note that this function returns a boolean rather than an integer value
  giving the exact bonus.  Finding the exact bonus requires knowing the
  effect range and may take longer.  This function should only be used
  in situations where the range doesn't matter.
**************************************************************************/
bool building_has_effect(const struct impr_type *pimprove,
			 enum effect_type effect_type)
{
  struct universal source = {
    .kind = VUT_IMPROVEMENT,
    /* just to bamboozle the annoying compiler warning */
    .value = {.building = improvement_by_number(improvement_number(pimprove))}
  };
  struct effect_list *plist = get_req_source_effects(&source);

  if (!plist) {
    return FALSE;
  }

  effect_list_iterate(plist, peffect) {
    if (peffect->type == effect_type) {
      return TRUE;
    }
  } effect_list_iterate_end;
  return FALSE;
}

/**************************************************************************
  Return TRUE iff any of the disabling requirements for this effect are
  active, which would prevent it from taking effect.
  (Assumes that any requirement specified in the ruleset with a negative
  sense is an impediment.)
**************************************************************************/
static bool is_effect_prevented(const struct player *target_player,
                                const struct player *other_player,
                                const struct city *target_city,
                                const struct impr_type *target_building,
                                const struct tile *target_tile,
                                const struct unit *target_unit,
                                const struct unit_type *target_unittype,
                                const struct output_type *target_output,
                                const struct specialist *target_specialist,
                                const struct effect *peffect,
                                const enum   req_problem_type prob_type)
{
  requirement_vector_iterate(&peffect->reqs, preq) {
    /* Only check present=FALSE requirements; these will return _FALSE_
     * from is_req_active() if met, and need reversed prob_type */
    if (!preq->present
        && !is_req_active(target_player, other_player, target_city,
                          target_building, target_tile,
                          target_unit, target_unittype,
                          target_output, target_specialist,
                          preq, REVERSED_RPT(prob_type))) {
      return TRUE;
    }
  } requirement_vector_iterate_end;
  return FALSE;
}

/**************************************************************************
  Returns TRUE if a building is replaced.  To be replaced, all its effects
  must be made redundant by groups that it is in.
  prob_type CERTAIN or POSSIBLE is answer to function name.
**************************************************************************/
bool is_building_replaced(const struct city *pcity,
                          struct impr_type *pimprove,
                          const enum req_problem_type prob_type)
{
  struct effect_list *plist;
  struct universal source = {
    .kind = VUT_IMPROVEMENT,
    .value = {.building = pimprove}
  };

  plist = get_req_source_effects(&source);

  /* A building with no effects and no flags is always redundant! */
  if (!plist) {
    return TRUE;
  }

  effect_list_iterate(plist, peffect) {
    /* We use TARGET_BUILDING as the lowest common denominator.  Note that
     * the building is its own target - but whether this is actually
     * checked depends on the range of the effect. */
    /* Prob_type is not reversed here. disabled is equal to replaced, not
     * reverse */
    if (!is_effect_prevented(city_owner(pcity), NULL, pcity,
                             pimprove,
                             NULL, NULL, NULL, NULL, NULL,
                             peffect, prob_type)) {
      return FALSE;
    }
  } effect_list_iterate_end;
  return TRUE;
}

/**************************************************************************
  Returns the effect bonus of a given type for any target.

  target gives the type of the target
  (player,city,building,tile) give the exact target
  effect_type gives the effect type to be considered

  Returns the effect sources of this type _currently active_.

  The returned vector must be freed (building_vector_free) when the caller
  is done with it.
**************************************************************************/
int get_target_bonus_effects(struct effect_list *plist,
                             const struct player *target_player,
                             const struct player *other_player,
                             const struct city *target_city,
                             const struct impr_type *target_building,
                             const struct tile *target_tile,
                             const struct unit *target_unit,
                             const struct unit_type *target_unittype,
                             const struct output_type *target_output,
                             const struct specialist *target_specialist,
                             enum effect_type effect_type)
{
  int bonus = 0;

  /* Loop over all effects of this type. */
  effect_list_iterate(get_effects(effect_type), peffect) {
    /* For each effect, see if it is active. */
    if (are_reqs_active(target_player, other_player, target_city,
                        target_building, target_tile,
                        target_unit, target_unittype,
                        target_output, target_specialist,
			&peffect->reqs, RPT_CERTAIN)) {
      /* And if so add on the value. */
      bonus += peffect->value;

      if (plist) {
	effect_list_append(plist, peffect);
      }
    }
  } effect_list_iterate_end;

  return bonus;
}

/**************************************************************************
  Returns the effect bonus for the whole world.
**************************************************************************/
int get_world_bonus(enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  return get_target_bonus_effects(NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL,
				  effect_type);
}

/**************************************************************************
  Returns the effect bonus for a player.
**************************************************************************/
int get_player_bonus(const struct player *pplayer,
		     enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  return get_target_bonus_effects(NULL,
                                  pplayer, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL,
                                  NULL, effect_type);
}

/**************************************************************************
  Returns the effect bonus at a city.
**************************************************************************/
int get_city_bonus(const struct city *pcity, enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  return get_target_bonus_effects(NULL,
                                  city_owner(pcity), NULL, pcity, NULL,
                                  city_tile(pcity), NULL, NULL, NULL,
                                  NULL, effect_type);
}

/**************************************************************************
  Returns the effect bonus of a specialist in a city.
**************************************************************************/
int get_city_specialist_output_bonus(const struct city *pcity,
				     const struct specialist *pspecialist,
				     const struct output_type *poutput,
				     enum effect_type effect_type)
{
  fc_assert_ret_val(pcity != NULL, 0);
  fc_assert_ret_val(pspecialist != NULL, 0);
  fc_assert_ret_val(poutput != NULL, 0);
  return get_target_bonus_effects(NULL,
                                  city_owner(pcity), NULL, pcity, NULL,
                                  NULL, NULL, NULL, poutput, pspecialist,
				  effect_type);
}

/**************************************************************************
  Returns the effect bonus at a city tile.

  FIXME: this is now used both for tile bonuses, tile-output bonuses,
  and city-output bonuses.  Thus ptile or poutput may be NULL for
  certain callers.  This could be changed by adding 2 new functions to
  the interface but they'd be almost identical and their likely names
  would conflict with functions already in city.c.
**************************************************************************/
int get_city_tile_output_bonus(const struct city *pcity,
			       const struct tile *ptile,
			       const struct output_type *poutput,
			       enum effect_type effect_type)
{
  fc_assert_ret_val(pcity != NULL, 0);
  return get_target_bonus_effects(NULL,
                                  city_owner(pcity), NULL, pcity, NULL,
                                  ptile, NULL, NULL, poutput, NULL,
				  effect_type);
}

/**************************************************************************
  Returns the player effect bonus of an output.
**************************************************************************/
int get_player_output_bonus(const struct player *pplayer,
                            const struct output_type *poutput,
                            enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(pplayer != NULL, 0);
  fc_assert_ret_val(poutput != NULL, 0);
  fc_assert_ret_val(effect_type != EFT_COUNT, 0);
  return get_target_bonus_effects(NULL, pplayer, NULL, NULL, NULL, NULL,
                                  NULL, NULL, poutput, NULL, effect_type);
}

/**************************************************************************
  Returns the player effect bonus of an output.
**************************************************************************/
int get_city_output_bonus(const struct city *pcity,
                          const struct output_type *poutput,
                          enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(pcity != NULL, 0);
  fc_assert_ret_val(poutput != NULL, 0);
  fc_assert_ret_val(effect_type != EFT_COUNT, 0);
  return get_target_bonus_effects(NULL, city_owner(pcity), NULL, pcity,
                                  NULL, NULL, NULL, NULL, poutput, NULL,
                                  effect_type);
}

/**************************************************************************
  Returns the effect bonus at a building.
**************************************************************************/
int get_building_bonus(const struct city *pcity,
		       const struct impr_type *building,
		       enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(NULL != pcity && NULL != building, 0);
  return get_target_bonus_effects(NULL,
                                  city_owner(pcity), NULL, pcity,
				  building,
				  NULL, NULL, NULL, NULL,
                                  NULL, effect_type);
}

/**************************************************************************
  Returns the effect bonus that applies at a tile for a given unittype.

  For instance with EFT_DEFEND_BONUS the attacker's unittype and the
  defending tile should be passed in.  Slightly counter-intuitive!
  See doc/README.effects to see how the unittype applies for each effect
  here.
**************************************************************************/
int get_unittype_bonus(const struct player *pplayer,
		       const struct tile *ptile,
		       const struct unit_type *punittype,
		       enum effect_type effect_type)
{
  struct city *pcity;

  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(pplayer != NULL && punittype != NULL, 0);

  if (ptile != NULL) {
    pcity = tile_city(ptile);
  } else {
    pcity = NULL;
  }

  return get_target_bonus_effects(NULL,
                                  pplayer, NULL, pcity, NULL, ptile,
                                  NULL, punittype, NULL,
                                  NULL, effect_type);
}

/**************************************************************************
  Returns the effect bonus at a unit
**************************************************************************/
int get_unit_bonus(const struct unit *punit, enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(punit != NULL, 0);
  return get_target_bonus_effects(NULL,
                                  unit_owner(punit),
                                  NULL,
                                  unit_tile(punit)
                                    ? tile_city(unit_tile(punit)) : NULL,
                                  NULL, unit_tile(punit),
                                  punit, unit_type(punit), NULL, NULL,
                                  effect_type);
}

/**************************************************************************
  Returns the effect bonus at a tile
**************************************************************************/
int get_tile_bonus(const struct tile *ptile, const struct unit *punit,
                   enum effect_type etype)
{
  struct player *pplayer = NULL;
  struct unit_type *utype = NULL;

  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(ptile != NULL, 0);

  if (punit != NULL) {
    pplayer = unit_owner(punit);
    utype = unit_type(punit);
  }

  return get_target_bonus_effects(NULL,
                                  pplayer,
                                  NULL,
                                  tile_city(ptile),
                                  NULL,
                                  ptile,
                                  punit,
                                  utype,
                                  NULL, NULL,
                                  etype);
}

/**************************************************************************
  Returns the effect sources of this type _currently active_ at the player.

  The returned vector must be freed (building_vector_free) when the caller
  is done with it.
**************************************************************************/
int get_player_bonus_effects(struct effect_list *plist,
			     const struct player *pplayer,
			     enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(pplayer != NULL, 0);
  return get_target_bonus_effects(plist,
                                  pplayer, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL,
				  effect_type);
}

/**************************************************************************
  Returns the effect sources of this type _currently active_ at the city.

  The returned vector must be freed (building_vector_free) when the caller
  is done with it.
**************************************************************************/
int get_city_bonus_effects(struct effect_list *plist,
			   const struct city *pcity,
			   const struct output_type *poutput,
			   enum effect_type effect_type)
{
  if (!initialized) {
    return 0;
  }

  fc_assert_ret_val(pcity != NULL, 0);
  return get_target_bonus_effects(plist,
                                  city_owner(pcity), NULL, pcity, NULL,
                                  NULL, NULL, NULL, poutput, NULL,
				  effect_type);
}

/**************************************************************************
  Returns the effect bonus the currently-in-construction-item will provide.

  Note this is not called get_current_production_bonus because that would
  be confused with EFT_PROD_BONUS.

  Problem type tells if we need to be CERTAIN about bonus before counting
  it or is POSSIBLE bonus enough.
**************************************************************************/
int get_current_construction_bonus(const struct city *pcity,
				   enum effect_type effect_type,
                                   const enum req_problem_type prob_type)
{
  if (!initialized) {
    return 0;
  }

  if (VUT_IMPROVEMENT == pcity->production.kind) {
    return get_potential_improvement_bonus(pcity->production.value.building,
                                           pcity, effect_type, prob_type);
  }
  return 0;
}

/**************************************************************************
  Returns the effect bonus the improvement would or does provide if present.

  Problem type tells if we need to be CERTAIN about bonus before counting
  it or is POSSIBLE bonus enough.
**************************************************************************/
int get_potential_improvement_bonus(struct impr_type *pimprove,
                                    const struct city *pcity,
                                    enum effect_type effect_type,
                                    const enum req_problem_type prob_type)
{
  struct universal source = { .kind = VUT_IMPROVEMENT,
                                .value = {.building = pimprove}};
  struct effect_list *plist = get_req_source_effects(&source);


  if (plist) {
    int power = 0;

    effect_list_iterate(plist, peffect) {
      bool present = TRUE;
      bool useful = TRUE;

      if (peffect->type != effect_type) {
        continue;
      }

      requirement_vector_iterate(&peffect->reqs, preq) {
        if (VUT_IMPROVEMENT == preq->source.kind
            && preq->source.value.building == pimprove) {
          present = preq->present;
          continue;
        }

        if (!is_req_active(city_owner(pcity), NULL, pcity, pimprove,
                           NULL, NULL, NULL, NULL, NULL,
                           preq, prob_type)) {
          useful = FALSE;
          break;
        } 
      } requirement_vector_iterate_end;

      if (useful) {
        if (present) {
          power += peffect->value;
        } else {
          power -= peffect->value;
        }
      }
    } effect_list_iterate_end;

    return power;
  }
  return 0;
}

/**************************************************************************
  Make user-friendly text for the source.  The text is put into a user
  buffer.
**************************************************************************/
void get_effect_req_text(struct effect *peffect, char *buf, size_t buf_len)
{
  buf[0] = '\0';

  /* FIXME: should we do something for present==FALSE reqs?
   * Currently we just ignore them. */
  requirement_vector_iterate(&peffect->reqs, preq) {
    if (!preq->present) {
      continue;
    }
    if (buf[0] != '\0') {
      fc_strlcat(buf, "+", buf_len);
    }

    universal_name_translation(&preq->source,
			buf + strlen(buf), buf_len - strlen(buf));
  } requirement_vector_iterate_end;
}

/**************************************************************************
  Iterate through all the effects in cache, and call callback for each.
  This is currently not very generic implementation, as we have only one user;
  ruleset sanity checking. If any callback returns FALSE, there is no
  further checking and this will return FALSE.
**************************************************************************/
bool iterate_effect_cache(iec_cb cb, void *data)
{
  fc_assert_ret_val(cb != NULL, FALSE);

  effect_list_iterate(ruleset_cache.tracker, peffect) {
    if (!cb(peffect, data)) {
      return FALSE;
    }
  } effect_list_iterate_end;

  return TRUE;
}
