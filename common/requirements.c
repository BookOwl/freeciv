/********************************************************************** 
 Freeciv - Copyright (C) 1996-2004 - The Freeciv Project
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
#include <config.h>
#endif

#include <assert.h>

#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "support.h"

#include "government.h"
#include "improvement.h"
#include "map.h"
#include "requirements.h"

/* Names of source types.  These must correspond to enum req_source_type in
 * requirements.h.  Do not change these unless you know what you're doing! */
static const char *req_source_type_names[] = {
  "None",
  "Tech",
  "Gov",
  "Building",
  "Special",
  "Terrain",
  "Nation",
  "UnitType",
  "MinSize"
};

/* Names of requirement ranges. These must correspond to enum req_range in
 * requirements.h.  Do not change these unless you know what you're doing! */
static const char *req_range_names[REQ_RANGE_LAST] = {
  "Local",
  "Adjacent",
  "City",
  "Continent",
  "Player",
  "World"
};

/**************************************************************************
  Convert a range name to an enumerated value.

  The check is case insensitive and returns REQ_RANGE_LAST if no match
  is found.
**************************************************************************/
enum req_range req_range_from_str(const char *str)
{
  enum req_range range;

  assert(ARRAY_SIZE(req_range_names) == REQ_RANGE_LAST);

  for (range = 0; range < REQ_RANGE_LAST; range++) {
    if (0 == mystrcasecmp(req_range_names[range], str)) {
      return range;
    }
  }

  return REQ_RANGE_LAST;
}

/**************************************************************************
  Parse a requirement type and value string into a requirement source
  structure.  Passing in a NULL type is considered REQ_NONE (not an error).

  Pass this some values like "Building", "Factory".
**************************************************************************/
struct req_source req_source_from_str(const char *type, const char *value)
{
  struct req_source source;
  const struct government *pgov;

  assert(ARRAY_SIZE(req_source_type_names) == REQ_LAST);
  if (type) {
    for (source.type = 0;
	 source.type < ARRAY_SIZE(req_source_type_names);
	 source.type++) {
      if (0 == mystrcasecmp(req_source_type_names[source.type], type)) {
	break;
      }
    }
  } else {
    source.type = REQ_NONE;
  }

  /* Finally scan the value string based on the type of the source. */
  switch (source.type) {
  case REQ_NONE:
    return source;
  case REQ_TECH:
    source.value.tech = find_tech_by_name(value);
    if (source.value.tech != A_LAST) {
      return source;
    }
    break;
  case REQ_GOV:
    pgov = find_government_by_name(value);
    if (pgov != NULL) {
      source.value.gov = pgov->index;
      return source;
    }
    break;
  case REQ_BUILDING:
    source.value.building = find_improvement_by_name(value);
    if (source.value.building != B_LAST) {
      return source;
    }
    break;
  case REQ_SPECIAL:
    source.value.special = get_special_by_name(value);
    if (source.value.special != S_NO_SPECIAL) {
      return source;
    }
    break;
  case REQ_TERRAIN:
    source.value.terrain = get_terrain_by_name(value);
    if (source.value.terrain != T_UNKNOWN) {
      return source;
    }
    break;
  case REQ_NATION:
    source.value.nation = find_nation_by_name(value);
    if (source.value.nation != NO_NATION_SELECTED) {
      return source;
    }
    break;
  case REQ_UNITTYPE:
    source.value.unittype = find_unit_type_by_name(value);
    if (source.value.unittype != U_LAST) {
      return source;
    }
    break;
  case REQ_MINSIZE:
    source.value.minsize = atoi(value);
    if (source.value.minsize > 0) {
      return source;
    }
    break;
  case REQ_LAST:
    break;
  }

  /* If we reach here there's been an error. */
  source.type = REQ_LAST;
  return source;
}

/**************************************************************************
  Parse some integer values into a req source.  This is for serialization
  of req sources and is the opposite of req_source_get_values().
**************************************************************************/
struct req_source req_source_from_values(int type, int value)
{
  struct req_source source;

  source.type = type;

  switch (source.type) {
  case REQ_NONE:
    return source;
  case REQ_TECH:
    source.value.tech = value;
    return source;
  case REQ_GOV:
    source.value.gov = value;
    return source;
  case REQ_BUILDING:
    source.value.building = value;
    return source;
  case REQ_SPECIAL:
    source.value.special = value;
    return source;
  case REQ_TERRAIN:
    source.value.terrain = value;
    return source;
  case REQ_NATION:
    source.value.nation = value;
    return source;
  case REQ_UNITTYPE:
    source.value.unittype = value;
    return source;
  case REQ_MINSIZE:
    source.value.minsize = value;
    return source;
  case REQ_LAST:
    return source;
  }

  source.type = REQ_LAST;
  assert(0);
  return source;
}

/**************************************************************************
  Look at a req source and return some integer values describing it.  This
  is for serialization of req sources and is the opposite of
  req_source_from_values().
**************************************************************************/
void req_source_get_values(struct req_source *source, int *type, int *value)
{
  *type = source->type;

  switch (source->type) {
  case REQ_NONE:
    *value = 0;
    return;
  case REQ_TECH:
    *value = source->value.tech;
    return;
  case REQ_GOV:
    *value = source->value.gov;
    return;
  case REQ_BUILDING:
    *value = source->value.building;
    return;
  case REQ_SPECIAL:
    *value = source->value.special;
    return;
  case REQ_TERRAIN:
    *value = source->value.terrain;
    return;
  case REQ_NATION:
    *value = source->value.nation;
    return;
  case REQ_UNITTYPE:
    *value = source->value.unittype;
    return;
  case REQ_MINSIZE:
    *value = source->value.minsize;
    return;
  case REQ_LAST:
    break;
  }

  assert(0);
  *value = 0;
}

/****************************************************************************
  Parse a requirement type and value string into a requrement structure.
  Returns REQ_LAST on error.  Passing in a NULL type is considered REQ_NONE
  (not an error).

  Pass this some values like "Building", "Factory".
****************************************************************************/
struct requirement req_from_str(const char *type,
				const char *range, bool survives,
				const char *value)
{
  struct requirement req;
  bool invalid = TRUE;

  req.source = req_source_from_str(type, value);

  /* Scan the range string to find the range.  If no range is given a
   * default fallback is used rather than giving an error. */
  req.range = req_range_from_str(range);
  if (req.range == REQ_RANGE_LAST) {
    switch (req.source.type) {
    case REQ_NONE:
    case REQ_LAST:
      break;
    case REQ_BUILDING:
    case REQ_SPECIAL:
    case REQ_TERRAIN:
    case REQ_UNITTYPE:
      req.range = REQ_RANGE_LOCAL;
      break;
    case REQ_MINSIZE:
      req.range = REQ_RANGE_CITY;
      break;
    case REQ_GOV:
    case REQ_TECH:
    case REQ_NATION:
      req.range = REQ_RANGE_PLAYER;
      break;
    }
  }

  req.survives = survives;

  /* These checks match what combinations are supported inside
   * is_req_active(). */
  switch (req.source.type) {
  case REQ_SPECIAL:
  case REQ_TERRAIN:
    invalid = (req.range != REQ_RANGE_LOCAL
	       && req.range != REQ_RANGE_ADJACENT);
    break;
  case REQ_TECH:
    invalid = (req.range < REQ_RANGE_PLAYER);
    break;
  case REQ_GOV:
    invalid = (req.range != REQ_RANGE_PLAYER);
    break;
  case REQ_BUILDING:
    invalid = ((req.range == REQ_RANGE_WORLD
		&& !is_great_wonder(req.source.value.building))
	       || (req.range > REQ_RANGE_CITY
		   && !is_wonder(req.source.value.building)));
    break;
  case REQ_MINSIZE:
    invalid = (req.range != REQ_RANGE_CITY);
    break;
  case REQ_NATION:
    invalid = (req.range != REQ_RANGE_PLAYER
	       && req.range != REQ_RANGE_WORLD);
    break;
  case REQ_UNITTYPE:
    invalid = (req.range != REQ_RANGE_LOCAL);
    break;
  case REQ_NONE:
    invalid = FALSE;
    break;
  case REQ_LAST:
    break;
  }
  if (invalid) {
    freelog(LOG_ERROR, "Invalid requirement %s | %s | %s | %s",
	    type, range, survives ? "survives" : "", value);
    req.source.type = REQ_LAST;
  }

  return req;
}

/****************************************************************************
  Set the values of a req from serializable integers.  This is the opposite
  of req_get_values.
****************************************************************************/
struct requirement req_from_values(int type, int range,
				   bool survives, int value)
{
  struct requirement req;

  req.source = req_source_from_values(type, value);
  req.range = range;
  req.survives = survives;
  return req;
}

/****************************************************************************
  Return the value of a req as a serializable integer.  This is the opposite
  of req_set_value.
****************************************************************************/
void req_get_values(struct requirement *req,
		    int *type, int *range, bool *survives, int *value)
{
  req_source_get_values(&req->source, type, value);
  *range = req->range;
  *survives = req->survives;
}

/****************************************************************************
  Returns the number of total world buildings (this includes buildings
  that have been destroyed).
****************************************************************************/
static int num_world_buildings_total(Impr_type_id building)
{
  if (is_great_wonder(building)) {
    return (great_wonder_was_built(building) ? 1 : 0);
  } else {
    freelog(LOG_ERROR,
	    /* TRANS: Obscure ruleset error. */
	    _("World-ranged requirements are only supported for wonders."));
    return 0;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type in the world.
****************************************************************************/
static int num_world_buildings(Impr_type_id id)
{
  if (is_great_wonder(id)) {
    return (find_city_from_great_wonder(id) ? 1 : 0);
  } else {
    freelog(LOG_ERROR,
	    /* TRANS: Obscure ruleset error. */
	    _("World-ranged requirements are only supported for wonders."));
    return 0;
  }
}

/**************************************************************************
  Returns the player city with the given wonder.
**************************************************************************/
static struct city *player_find_city_from_wonder(const struct player *plr,
						 Impr_type_id id)
{
  int city_id;
  struct city *pcity;

  if (is_great_wonder(id)) {
    city_id = game.info.great_wonders[id];
  } else if (is_small_wonder(id)) {
    city_id = plr->small_wonders[id];
  } else {
    return NULL;
  }

  pcity = find_city_by_id(city_id);
  if (pcity && (city_owner(pcity) == plr)) {
    return pcity;
  } else {
    return NULL;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type owned by plr.
****************************************************************************/
static int num_player_buildings(const struct player *pplayer,
				Impr_type_id building)
{
  if (is_wonder(building)) {
    return (player_find_city_from_wonder(pplayer, building) ? 1 : 0);
  } else {
    freelog(LOG_ERROR,
	    /* TRANS: Obscure ruleset error. */
	    _("Player-ranged requirements are only supported for wonders."));
    return 0;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type on a continent.
****************************************************************************/
static int num_continent_buildings(const struct player *pplayer,
				   int continent, Impr_type_id building)
{
  if (is_wonder(building)) {
    const struct city *pcity;

    pcity = player_find_city_from_wonder(pplayer, building);
    if (pcity && tile_get_continent(pcity->tile) == continent) {
      return 1;
    }
  } else {
    freelog(LOG_ERROR,
	    /* TRANS: Obscure ruleset error. */
	    _("Island-ranged requirements are only supported for wonders."));
  }
  return 0;
}

/****************************************************************************
  Returns the number of buildings of a certain type in a city.
****************************************************************************/
static int num_city_buildings(const struct city *pcity, Impr_type_id id)
{
  return (city_got_building(pcity, id) ? 1 : 0);
}

/****************************************************************************
  How many of the source building are there within range of the target?

  The target gives the type of the target.  The exact target is a player,
  city, or building specified by the target_xxx arguments.

  The range gives the range of the requirement.

  "Survives" specifies whether the requirement allows destroyed sources.
  If set then all source buildings ever built are counted; if not then only
  living buildings are counted.

  source gives the building type of the source in question.

  Note that this function does a lookup into the source caches to find
  the number of available sources.  However not all source caches exist: if
  the cache doesn't exist then we return 0.
****************************************************************************/
static int count_buildings_in_range(const struct player *target_player,
				    const struct city *target_city,
				    const struct impr_type * target_building,
				    enum req_range range, bool survives,
				    Impr_type_id source)
{
  if (improvement_obsolete(target_player, source)) {
    return 0;
  }

  if (survives) {
    if (range == REQ_RANGE_WORLD) {
      return num_world_buildings_total(source);
    } else {
      /* There is no sources cache for this. */
      freelog(LOG_ERROR,
	      /* TRANS: Obscure ruleset error. */
	      _("Surviving requirements are only "
		"supported at world range."));
      return 0;
    }
  }

  switch (range) {
  case REQ_RANGE_WORLD:
    return num_world_buildings(source);
  case REQ_RANGE_PLAYER:
    return target_player ? num_player_buildings(target_player, source) : 0;
  case REQ_RANGE_CONTINENT:
    if (target_player) {
      int continent = tile_get_continent(target_city->tile);

      return num_continent_buildings(target_player, continent, source);
    } else {
      return 0;
    }
  case REQ_RANGE_CITY:
    return target_city ? num_city_buildings(target_city, source) : 0;
  case REQ_RANGE_LOCAL:
    if (target_building && target_building->index == source) {
      return num_city_buildings(target_city, source);
    } else {
      /* TODO: other local targets */
      return 0;
    }
  case REQ_RANGE_ADJACENT:
    return 0;
  case REQ_RANGE_LAST:
    break;
  }
  assert(0);
  return 0;
}

/****************************************************************************
  Is there a source tech within range of the target?
****************************************************************************/
static bool is_tech_in_range(const struct player *target_player,
			     enum req_range range,
			     Tech_type_id tech)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    return (target_player
	    && get_invention(target_player, tech) == TECH_KNOWN);
  case REQ_RANGE_WORLD:
    return game.info.global_advances[tech] > 0;
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_LAST:
    break;
  }

  assert(0);
  return FALSE;
}

/****************************************************************************
  Is there a source special within range of the target?
****************************************************************************/
static bool is_special_in_range(const struct tile *target_tile,
				enum req_range range, bool survives,
				enum tile_special_type special)

{
  switch (range) {
  case REQ_RANGE_LOCAL:
    return target_tile && tile_has_special(target_tile, special);
  case REQ_RANGE_ADJACENT:
    return target_tile && is_special_near_tile(target_tile, special);
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_LAST:
    break;
  }

  assert(0);
  return FALSE;
}

/****************************************************************************
  Is there a source tile within range of the target?
****************************************************************************/
static bool is_terrain_in_range(const struct tile *target_tile,
				enum req_range range, bool survives,
				Terrain_type_id terrain)
{
  if (!target_tile) {
    return FALSE;
  }

  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has the terrain. */
    return target_tile->terrain == terrain;
  case REQ_RANGE_ADJACENT:
    return is_terrain_near_tile(target_tile, terrain);
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_LAST:
    break;
  }

  assert(0);
  return FALSE;
}

/****************************************************************************
  Is there a nation within range of the target?
****************************************************************************/
static bool is_nation_in_range(const struct player *target_player,
			       enum req_range range, bool survives,
			       Nation_type_id nation)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    return target_player && target_player->nation == nation;
  case REQ_RANGE_WORLD:
    /* FIXME: inefficient */
    players_iterate(pplayer) {
      if (pplayer->nation == nation && (pplayer->is_alive || survives)) {
	return TRUE;
      }
    } players_iterate_end;
    return FALSE;
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_LAST:
    break;
  }

  assert(0);
  return FALSE;
}

/****************************************************************************
  Is there a unit of the given type within range of the target?
****************************************************************************/
static bool is_unittype_in_range(const struct unit *target_unit,
				 enum req_range range, bool survives,
				 Unit_type_id unittype)
{
  return (range == REQ_RANGE_LOCAL
	  && target_unit
	  && target_unit->type == unittype);
}

/****************************************************************************
  Checks the requirement to see if it is active on the given target.

  target gives the type of the target
  (player,city,building,tile) give the exact target
  req gives the requirement itself

  Make sure you give all aspects of the target when calling this function:
  for instance if you have TARGET_CITY pass the city's owner as the target
  player as well as the city itself as the target city.
****************************************************************************/
bool is_req_active(const struct player *target_player,
		   const struct city *target_city,
		   const struct impr_type *target_building,
		   const struct tile *target_tile,
		   const struct unit *target_unit,
		   const struct requirement *req)
{
  /* Note the target may actually not exist.  In particular, effects that
   * have a REQ_SPECIAL or REQ_TERRAIN may often be passed to this function
   * with a city as their target.  In this case the requirement is simply
   * not met. */
  switch (req->source.type) {
  case REQ_NONE:
    return TRUE;
  case REQ_TECH:
    /* The requirement is filled if the player owns the tech. */
    return is_tech_in_range(target_player, req->range,
			    req->source.value.tech);
  case REQ_GOV:
    /* The requirement is filled if the player is using the government. */
    return (target_player
	    && (target_player->government == req->source.value.gov));
  case REQ_BUILDING:
    /* The requirement is filled if there's at least one of the building
     * in the city.  (This is a slightly nonstandard use of
     * count_sources_in_range.) */
    return (count_buildings_in_range(target_player, target_city,
				     target_building,
				     req->range, req->survives,
				     req->source.value.building) > 0);
  case REQ_SPECIAL:
    return is_special_in_range(target_tile,
			       req->range, req->survives,
			       req->source.value.special);
  case REQ_TERRAIN:
    return is_terrain_in_range(target_tile,
			       req->range, req->survives,
			       req->source.value.terrain);
  case REQ_NATION:
    return is_nation_in_range(target_player, req->range, req->survives,
			      req->source.value.nation);
  case REQ_UNITTYPE:
    return is_unittype_in_range(target_unit,
				req->range, req->survives,
				req->source.value.unittype);
  case REQ_MINSIZE:
    return target_city && target_city->size >= req->source.value.minsize;
  case REQ_LAST:
    break;
  }

  assert(0);
  return FALSE;
}

/****************************************************************************
  Checks the requirement(s) to see if they are active on the given target.

  target gives the type of the target
  (player,city,building,tile) give the exact target

  reqs gives the requirement array; num_reqs is the size of the array.  This
  array may be REQ_NONE-terminated, otherwise all requirements in it will
  be checked.  The function returns TRUE only if all requirements are active.

  Make sure you give all aspects of the target when calling this function:
  for instance if you have TARGET_CITY pass the city's owner as the target
  player as well as the city itself as the target city.
****************************************************************************/
bool are_reqs_active(const struct player *target_player,
		     const struct city *target_city,
		     const struct impr_type *target_building,
		     const struct tile *target_tile,
		     const struct unit *target_unit,
		     const struct requirement *reqs, int num_reqs)
{
  int i;

  for (i = 0; i < num_reqs; i++) {
    if (reqs[i].source.type == REQ_NONE) {
      break; /* Short-circuit any more checks. */
    } else if (!is_req_active(target_player, target_city,
			      target_building, target_tile, target_unit,
			      &reqs[i])) {
      return FALSE;
    }
  }

  return TRUE;
}

/****************************************************************************
  Return TRUE if this is an "unchanging" requirement.  This means that
  if a target can't meet the requirement now, it probably won't ever be able
  to do so later.  This can be used to do requirement filtering when checking
  if a target may "eventually" become available.

  Note this isn't absolute.  Returning TRUE here just means that the
  requirement probably can't be met.  In some cases (particularly terrains)
  it may be wrong.
*****************************************************************************/
bool is_req_unchanging(const struct requirement *req)
{
  switch (req->source.type) {
  case REQ_NATION:
  case REQ_NONE:
    return TRUE;
  case REQ_TECH:
  case REQ_GOV:
  case REQ_BUILDING:
  case REQ_MINSIZE:
  case REQ_UNITTYPE: /* Not sure about this one */
    return FALSE;
  case REQ_SPECIAL:
  case REQ_TERRAIN:
    /* Terrains and specials aren't really unchanging; in fact they're
     * practically guaranteed to change.  We return TRUE here for historical
     * reasons and so that the AI doesn't get confused (since the AI
     * doesn't know how to meet special and terrain requirements). */
    return TRUE;
  case REQ_LAST:
    break;
  }
  assert(0);
  return TRUE;
}

/****************************************************************************
  Return TRUE iff the two sources are equivalent.  Note this isn't the
  same as an == or memcmp check.
*****************************************************************************/
bool are_req_sources_equal(const struct req_source *psource1,
			   const struct req_source *psource2)
{
  if (psource1->type != psource2->type) {
    return FALSE;
  }
  switch (psource1->type) {
  case REQ_NONE:
    return TRUE;
  case REQ_TECH:
    return psource1->value.tech == psource2->value.tech;
  case REQ_GOV:
    return psource1->value.gov == psource2->value.gov;
  case REQ_BUILDING:
    return psource1->value.building == psource2->value.building;
  case REQ_SPECIAL:
    return psource1->value.special == psource2->value.special;
  case REQ_TERRAIN:
    return psource1->value.terrain == psource2->value.terrain;
  case REQ_NATION:
    return psource1->value.nation == psource2->value.nation;
  case REQ_UNITTYPE:
    return psource1->value.unittype == psource2->value.unittype;
  case REQ_MINSIZE:
    return psource1->value.minsize == psource2->value.minsize;
  case REQ_LAST:
    break;
  }
  assert(0);
  return FALSE;
}

/****************************************************************************
  Make user-friendly text for the source.  The text is put into a user
  buffer which is also returned.
*****************************************************************************/
char *get_req_source_text(const struct req_source *psource,
			  char *buf, size_t bufsz)
{
  buf[0] = '\0'; /* to be safe. */
  switch (psource->type) {
  case REQ_NONE:
    mystrlcat(buf, _("(none)"), bufsz);
    break;
  case REQ_TECH:
    mystrlcat(buf, advances[psource->value.tech].name, bufsz);
    break;
  case REQ_GOV:
    mystrlcat(buf, get_government_name(psource->value.gov), bufsz);
    break;
  case REQ_BUILDING:
    mystrlcat(buf, get_improvement_name(psource->value.building), bufsz);
    break;
  case REQ_SPECIAL:
    mystrlcat(buf, get_special_name(psource->value.special), bufsz);
    break;
  case REQ_TERRAIN:
    mystrlcat(buf, get_terrain_name(psource->value.terrain), bufsz);
    break;
  case REQ_NATION:
    mystrlcat(buf, get_nation_name(psource->value.nation), bufsz);
    break;
  case REQ_UNITTYPE:
    mystrlcat(buf, unit_name(psource->value.unittype), bufsz);
    break;
  case REQ_MINSIZE:
    cat_snprintf(buf, bufsz, "Size %d", psource->value.minsize);
    break;
  case REQ_LAST:
    assert(0);
    break;
  }

  return buf;
}
