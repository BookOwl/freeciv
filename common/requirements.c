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
#include <fc_config.h>
#endif

/* utility */
#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "support.h"

/* common */
#include "achievements.h"
#include "citizens.h"
#include "government.h"
#include "improvement.h"
#include "player.h"
#include "map.h"
#include "road.h"
#include "specialist.h"

#include "requirements.h"

/**************************************************************************
  Parse requirement type (kind) and value strings into a universal
  structure.  Passing in a NULL type is considered VUT_NONE (not an error).

  Pass this some values like "Building", "Factory".
  FIXME: ensure that every caller checks error return!
**************************************************************************/
struct universal universal_by_rule_name(const char *kind,
					const char *value)
{
  struct universal source;

  source.kind = universals_n_by_name(kind, fc_strcasecmp);
  if (!universals_n_is_valid(source.kind)) {
    return source;
  }

  /* Finally scan the value string based on the type of the source. */
  switch (source.kind) {
  case VUT_NONE:
    return source;
  case VUT_ADVANCE:
    source.value.advance = advance_by_rule_name(value);
    if (source.value.advance != NULL) {
      return source;
    }
    break;
  case VUT_TECHFLAG:
    source.value.techflag
      = tech_flag_id_by_name(value, fc_strcasecmp);
    if (tech_flag_id_is_valid(source.value.techflag)) {
      return source;
    }
    break;
  case VUT_GOVERNMENT:
    source.value.govern = government_by_rule_name(value);
    if (source.value.govern != NULL) {
      return source;
    }
    break;
  case VUT_ACHIEVEMENT:
    source.value.achievement = achievement_by_rule_name(value);
    if (source.value.achievement != NULL) {
      return source;
    }
    break;
  case VUT_IMPROVEMENT:
    source.value.building = improvement_by_rule_name(value);
    if (source.value.building != NULL) {
      return source;
    }
    break;
  case VUT_EXTRA:
    source.value.extra = extra_type_by_rule_name(value);
    if (source.value.extra != NULL) {
      return source;
    }
    break;
  case VUT_TERRAIN:
    source.value.terrain = terrain_by_rule_name(value);
    if (source.value.terrain != T_UNKNOWN) {
      return source;
    }
    break;
  case VUT_TERRFLAG:
    source.value.terrainflag
      = terrain_flag_id_by_name(value, fc_strcasecmp);
    if (terrain_flag_id_is_valid(source.value.terrainflag)) {
      return source;
    }
    break;
  case VUT_RESOURCE:
    source.value.resource = resource_by_rule_name(value);
    if (source.value.resource != NULL) {
      return source;
    }
    break;
  case VUT_NATION:
    source.value.nation = nation_by_rule_name(value);
    if (source.value.nation != NO_NATION_SELECTED) {
      return source;
    }
    break;
  case VUT_NATIONALITY:
    source.value.nationality = nation_by_rule_name(value);
    if (source.value.nationality != NO_NATION_SELECTED) {
      return source;
    }
    break;
  case VUT_DIPLREL:
    source.value.diplrel = diplrel_by_rule_name(value);
    if (source.value.diplrel != diplrel_asym_invalid()) {
      return source;
    }
    break;
  case VUT_UTYPE:
    source.value.utype = unit_type_by_rule_name(value);
    if (source.value.utype) {
      return source;
    }
    break;
  case VUT_UTFLAG:
    source.value.unitflag = unit_type_flag_id_by_name(value, fc_strcasecmp);
    if (unit_type_flag_id_is_valid(source.value.unitflag)) {
      return source;
    }
    break;
  case VUT_UCLASS:
    source.value.uclass = unit_class_by_rule_name(value);
    if (source.value.uclass) {
      return source;
    }
    break;
  case VUT_UCFLAG:
    source.value.unitclassflag
      = unit_class_flag_id_by_name(value, fc_strcasecmp);
    if (unit_class_flag_id_is_valid(source.value.unitclassflag)) {
      return source;
    }
    break;
  case VUT_OTYPE:
    source.value.outputtype = output_type_by_identifier(value);
    if (source.value.outputtype != O_LAST) {
      return source;
    }
    break;
  case VUT_SPECIALIST:
    source.value.specialist = specialist_by_rule_name(value);
    if (source.value.specialist) {
      return source;
    }
  case VUT_MINSIZE:
    source.value.minsize = atoi(value);
    if (source.value.minsize > 0) {
      return source;
    }
    break;
  case VUT_AI_LEVEL:
    source.value.ai_level = ai_level_by_name(value);
    if (source.value.ai_level != AI_LEVEL_LAST) {
      return source;
    }
    break;
  case VUT_MAXTILEUNITS:
    source.value.maxTileUnits = atoi(value);
    if (0 <= source.value.maxTileUnits) {
      return source;
    }
    break;
  case VUT_TERRAINCLASS:
    source.value.terrainclass
      = terrain_class_by_name(value, fc_strcasecmp);
    if (terrain_class_is_valid(source.value.terrainclass)) {
      return source;
    }
    break;
  case VUT_BASEFLAG:
    source.value.baseflag = base_flag_id_by_name(value, fc_strcasecmp);
    if (base_flag_id_is_valid(source.value.baseflag)) {
      return source;
    }
    break;
  case VUT_ROADFLAG:
    source.value.roadflag = road_flag_id_by_name(value, fc_strcasecmp);
    if (road_flag_id_is_valid(source.value.roadflag)) {
      return source;
    }
    break;
  case VUT_MINYEAR:
    source.value.minyear = atoi(value);
    return source;
  case VUT_TERRAINALTER:
    source.value.terrainalter
      = terrain_alteration_by_name(value, fc_strcasecmp);
    if (terrain_alteration_is_valid(source.value.terrainalter)) {
      return source;
    }
    break;
  case VUT_CITYTILE:
    source.value.citytile = citytile_by_rule_name(value);
    if (source.value.citytile != CITYT_LAST) {
      return source;
    }
    break;
  case VUT_COUNT:
    break;
  }

  /* If we reach here there's been an error. */
  source.kind = universals_n_invalid();
  return source;
}

/**************************************************************************
  Combine values into a universal structure.  This is for serialization
  and is the opposite of universal_extraction().
  FIXME: ensure that every caller checks error return!
**************************************************************************/
struct universal universal_by_number(const enum universals_n kind,
				     const int value)
{
  struct universal source;

  source.kind = kind;

  switch (source.kind) {
  case VUT_NONE:
    /* Avoid compiler warning about unitialized source.value */
    source.value.advance = NULL;

    return source;
  case VUT_ADVANCE:
    source.value.advance = advance_by_number(value);
    if (source.value.advance != NULL) {
      return source;
    }
    break;
 case VUT_TECHFLAG:
    source.value.techflag = value;
    return source;
  case VUT_GOVERNMENT:
    source.value.govern = government_by_number(value);
    if (source.value.govern != NULL) {
      return source;
    }
    break;
  case VUT_ACHIEVEMENT:
    source.value.achievement = achievement_by_number(value);
    if (source.value.achievement != NULL) {
      return source;
    }
    break;
  case VUT_IMPROVEMENT:
    source.value.building = improvement_by_number(value);
    if (source.value.building != NULL) {
      return source;
    }
    break;
  case VUT_EXTRA:
    source.value.extra = extra_by_number(value);
    return source;
  case VUT_TERRAIN:
    source.value.terrain = terrain_by_number(value);
    if (source.value.terrain != NULL) {
      return source;
    }
    break;
  case VUT_TERRFLAG:
    source.value.terrainflag = value;
    return source;
  case VUT_RESOURCE:
    source.value.resource = resource_by_number(value);
    if (source.value.resource != NULL) {
      return source;
    }
    break;
  case VUT_NATION:
    source.value.nation = nation_by_number(value);
    if (source.value.nation != NULL) {
      return source;
    }
    break;
  case VUT_DIPLREL:
    source.value.diplrel = value;
    if (source.value.diplrel != diplrel_asym_invalid()) {
      return source;
    }
    break;
  case VUT_NATIONALITY:
    source.value.nationality = nation_by_number(value);
    if (source.value.nationality != NULL) {
      return source;
    }
    break;
  case VUT_UTYPE:
    source.value.utype = utype_by_number(value);
    if (source.value.utype != NULL) {
      return source;
    }
    break;
  case VUT_UTFLAG:
    source.value.unitflag = value;
    return source;
  case VUT_UCLASS:
    source.value.uclass = uclass_by_number(value);
    if (source.value.uclass != NULL) {
      return source;
    }
    break;
  case VUT_UCFLAG:
    source.value.unitclassflag = value;
    return source;
  case VUT_OTYPE:
    source.value.outputtype = value;
    return source;
  case VUT_SPECIALIST:
    source.value.specialist = specialist_by_number(value);
    return source;
  case VUT_MINSIZE:
    source.value.minsize = value;
    return source;
  case VUT_AI_LEVEL:
    source.value.ai_level = value;
    return source;
  case VUT_MAXTILEUNITS:
    source.value.maxTileUnits = value;
    return source;
  case VUT_TERRAINCLASS:
    source.value.terrainclass = value;
    return source;
  case VUT_BASEFLAG:
    source.value.baseflag = value;
    return source;
  case VUT_ROADFLAG:
    source.value.roadflag = value;
    return source;
  case VUT_MINYEAR:
    source.value.minyear = value;
    return source;
  case VUT_TERRAINALTER:
    source.value.terrainalter = value;
    return source;
  case VUT_CITYTILE:
    source.value.citytile = value;
    return source;
  case VUT_COUNT:
    break;
  }

  /* If we reach here there's been an error. */
  source.kind = universals_n_invalid();
  /* Avoid compiler warning about unitialized source.value */
  source.value.advance = NULL;

  return source;
}

/**************************************************************************
  Extract universal structure into its components for serialization;
  the opposite of universal_by_number().
**************************************************************************/
void universal_extraction(const struct universal *source,
			  int *kind, int *value)
{
  *kind = source->kind;
  *value = universal_number(source);
}

/**************************************************************************
  Return the universal number of the constituent.
**************************************************************************/
int universal_number(const struct universal *source)
{
  switch (source->kind) {
  case VUT_NONE:
    return 0;
  case VUT_ADVANCE:
    return advance_number(source->value.advance);
  case VUT_TECHFLAG:
    return source->value.techflag;
  case VUT_GOVERNMENT:
    return government_number(source->value.govern);
  case VUT_ACHIEVEMENT:
    return achievement_number(source->value.achievement);
  case VUT_IMPROVEMENT:
    return improvement_number(source->value.building);
  case VUT_EXTRA:
    return extra_number(source->value.extra);
  case VUT_TERRAIN:
    return terrain_number(source->value.terrain);
  case VUT_TERRFLAG:
    return source->value.terrainflag;
  case VUT_RESOURCE:
    return resource_number(source->value.resource);
  case VUT_NATION:
    return nation_number(source->value.nation);
  case VUT_NATIONALITY:
    return nation_number(source->value.nationality);
  case VUT_DIPLREL:
    return source->value.diplrel;
  case VUT_UTYPE:
    return utype_number(source->value.utype);
  case VUT_UTFLAG:
    return source->value.unitflag;
  case VUT_UCLASS:
    return uclass_number(source->value.uclass);
  case VUT_UCFLAG:
    return source->value.unitclassflag;
  case VUT_OTYPE:
    return source->value.outputtype;
  case VUT_SPECIALIST:
    return specialist_number(source->value.specialist);
  case VUT_MINSIZE:
    return source->value.minsize;
  case VUT_AI_LEVEL:
    return source->value.ai_level;
  case VUT_MAXTILEUNITS:
    return source->value.maxTileUnits;
  case VUT_TERRAINCLASS:
    return source->value.terrainclass;
  case VUT_BASEFLAG:
    return source->value.baseflag;
  case VUT_ROADFLAG:
    return source->value.roadflag;
  case VUT_MINYEAR:
    return source->value.minyear;
  case VUT_TERRAINALTER:
    return source->value.terrainalter;
  case VUT_CITYTILE:
    return source->value.citytile;
  case VUT_COUNT:
    break;
  }

  /* If we reach here there's been an error. */
  fc_assert_msg(FALSE, "universal_number(): invalid source kind %d.",
                source->kind);
  return 0;
}

/****************************************************************************
  Parse a requirement type and value string into a requrement structure.
  Returns the invalid element for enum universal_n on error. Passing in a
  NULL type is considered VUT_NONE (not an error).

  Pass this some values like "Building", "Factory".
****************************************************************************/
struct requirement req_from_str(const char *type, const char *range,
				bool survives, bool present,
				const char *value)
{
  struct requirement req;
  bool invalid = TRUE;

  req.source = universal_by_rule_name(type, value);

  /* Scan the range string to find the range.  If no range is given a
   * default fallback is used rather than giving an error. */
  req.range = req_range_by_name(range, fc_strcasecmp);
  if (!req_range_is_valid(req.range)) {
    switch (req.source.kind) {
    case VUT_NONE:
    case VUT_COUNT:
      break;
    case VUT_IMPROVEMENT:
    case VUT_EXTRA:
    case VUT_TERRAIN:
    case VUT_TERRFLAG:
    case VUT_RESOURCE:
    case VUT_UTYPE:
    case VUT_UTFLAG:
    case VUT_UCLASS:
    case VUT_UCFLAG:
    case VUT_OTYPE:
    case VUT_SPECIALIST:
    case VUT_TERRAINCLASS:
    case VUT_BASEFLAG:
    case VUT_ROADFLAG:
    case VUT_TERRAINALTER:
    case VUT_CITYTILE:
    case VUT_MAXTILEUNITS:
      req.range = REQ_RANGE_LOCAL;
      break;
    case VUT_MINSIZE:
    case VUT_NATIONALITY:
      req.range = REQ_RANGE_CITY;
      break;
    case VUT_GOVERNMENT:
    case VUT_ACHIEVEMENT:
    case VUT_ADVANCE:
    case VUT_TECHFLAG:
    case VUT_NATION:
    case VUT_DIPLREL:
    case VUT_AI_LEVEL:
      req.range = REQ_RANGE_PLAYER;
      break;
    case VUT_MINYEAR:
      req.range = REQ_RANGE_WORLD;
      break;
    }
  }

  req.survives = survives;
  req.present = present;

  /* These checks match what combinations are supported inside
   * is_req_active(). */
  switch (req.source.kind) {
  case VUT_TERRAIN:
  case VUT_EXTRA:
  case VUT_RESOURCE:
  case VUT_TERRAINCLASS:
  case VUT_TERRFLAG:
  case VUT_BASEFLAG:
  case VUT_ROADFLAG:
    invalid = (req.range != REQ_RANGE_LOCAL
               && req.range != REQ_RANGE_CADJACENT
	       && req.range != REQ_RANGE_ADJACENT
               && req.range != REQ_RANGE_CITY);
    break;
  case VUT_ADVANCE:
  case VUT_TECHFLAG:
  case VUT_ACHIEVEMENT:
    invalid = (req.range < REQ_RANGE_PLAYER);
    break;
  case VUT_GOVERNMENT:
  case VUT_AI_LEVEL:
    invalid = (req.range != REQ_RANGE_PLAYER);
    break;
  case VUT_IMPROVEMENT:
    invalid = ((req.range == REQ_RANGE_WORLD
		&& !is_great_wonder(req.source.value.building))
	       || (req.range > REQ_RANGE_CITY
		   && !is_wonder(req.source.value.building)));
    break;
  case VUT_MINSIZE:
  case VUT_NATIONALITY:
    invalid = (req.range != REQ_RANGE_CITY);
    break;
  case VUT_DIPLREL:
    invalid = (req.range != REQ_RANGE_LOCAL
               && req.range != REQ_RANGE_PLAYER
               && req.range != REQ_RANGE_ALLIANCE
               && req.range != REQ_RANGE_WORLD);
    break;
  case VUT_NATION:
    invalid = (req.range != REQ_RANGE_PLAYER
               && req.range != REQ_RANGE_ALLIANCE
	       && req.range != REQ_RANGE_WORLD);
    break;
  case VUT_UTYPE:
  case VUT_UTFLAG:
  case VUT_UCLASS:
  case VUT_UCFLAG:
  case VUT_OTYPE:
  case VUT_SPECIALIST:
  case VUT_TERRAINALTER: /* XXX could in principle support C/ADJACENT */
    invalid = (req.range != REQ_RANGE_LOCAL);
    break;
  case VUT_CITYTILE:
  case VUT_MAXTILEUNITS:
    invalid = (req.range != REQ_RANGE_LOCAL
               && req.range != REQ_RANGE_CADJACENT
               && req.range != REQ_RANGE_ADJACENT);
    break;
  case VUT_MINYEAR:
    invalid = (req.range != REQ_RANGE_WORLD);
    break;
  case VUT_NONE:
    invalid = FALSE;
    break;
  case VUT_COUNT:
    break;
  }

  if (invalid) {
    log_error("Invalid requirement %s | %s | %s | %s | %s",
              type, range, survives ? "survives" : "",
              present ? "present" : "", value);
    req.source.kind = universals_n_invalid();
  }

  return req;
}

/****************************************************************************
  Set the values of a req from serializable integers.  This is the opposite
  of req_get_values.
****************************************************************************/
struct requirement req_from_values(int type, int range,
				   bool survives, bool present,
				   int value)
{
  struct requirement req;

  req.source = universal_by_number(type, value);
  req.range = range;
  req.survives = survives;
  req.present = present;
  return req;
}

/****************************************************************************
  Return the value of a req as a serializable integer.  This is the opposite
  of req_set_value.
****************************************************************************/
void req_get_values(const struct requirement *req,
		    int *type, int *range,
		    bool *survives, bool *present,
		    int *value)
{
  universal_extraction(&req->source, type, value);
  *range = req->range;
  *survives = req->survives;
  *present = req->present;
}

/****************************************************************************
  Returns TRUE if req1 and req2 are equal.
****************************************************************************/
bool are_requirements_equal(const struct requirement *req1,
			    const struct requirement *req2)
{
  return (are_universals_equal(&req1->source, &req2->source)
	  && req1->range == req2->range
	  && req1->survives == req2->survives
	  && req1->present == req2->present);
}

/****************************************************************************
  Returns TRUE if req1 and req2 directly negate each other.
****************************************************************************/
bool are_requirements_opposites(const struct requirement *req1,
                                const struct requirement *req2)
{
  return (are_universals_equal(&req1->source, &req2->source)
          && req1->range == req2->range
          && req1->survives == req2->survives
          && req1->present != req2->present);
}

/****************************************************************************
  Returns the number of total world buildings (this includes buildings
  that have been destroyed).
****************************************************************************/
static int num_world_buildings_total(const struct impr_type *building)
{
  if (is_great_wonder(building)) {
    return (great_wonder_is_built(building)
            || great_wonder_is_destroyed(building) ? 1 : 0);
  } else {
    /* TRANS: Obscure ruleset error. */
    log_error(_("World-ranged requirements are only supported "
                "for wonders."));
    return 0;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type in the world.
****************************************************************************/
static int num_world_buildings(const struct impr_type *building)
{
  if (is_great_wonder(building)) {
    return (great_wonder_is_built(building) ? 1 : 0);
  } else {
    /* TRANS: Obscure ruleset error. */
    log_error(_("World-ranged requirements are only supported "
                "for wonders."));
    return 0;
  }
}

/****************************************************************************
  Returns whether a building of a certain type has ever been built by
  pplayer, even if it has subsequently been destroyed.

  Note: the implementation of this is no different in principle from
  num_world_buildings_total(), but the semantics are different because
  unlike a great wonder, a small wonder could be destroyed and rebuilt
  many times, requiring return of values >1, but there's no record kept
  to support that. Fortunately, the only current caller doesn't need the
  exact number.
****************************************************************************/
static bool player_has_ever_built(const struct player *pplayer,
                                  const struct impr_type *building)
{
  if (is_wonder(building)) {
    return (wonder_is_built(pplayer, building)
            || wonder_is_lost(pplayer, building) ? TRUE : FALSE);
  } else {
    /* TRANS: Obscure ruleset error. */
    log_error(_("Player-ranged requirements are only supported "
                "for wonders."));
    return FALSE;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type owned by plr.
****************************************************************************/
static int num_player_buildings(const struct player *pplayer,
				const struct impr_type *building)
{
  if (is_wonder(building)) {
    return (wonder_is_built(pplayer, building) ? 1 : 0);
  } else {
    /* TRANS: Obscure ruleset error. */
    log_error(_("Player-ranged requirements are only supported "
                "for wonders."));
    return 0;
  }
}

/****************************************************************************
  Returns the number of buildings of a certain type on a continent.
****************************************************************************/
static int num_continent_buildings(const struct player *pplayer,
				   int continent,
				   const struct impr_type *building)
{
  if (is_wonder(building)) {
    const struct city *pcity;

    pcity = city_from_wonder(pplayer, building);
    if (pcity && pcity->tile && tile_continent(pcity->tile) == continent) {
      return 1;
    }
  } else {
    /* TRANS: Obscure ruleset error. */
    log_error(_("Island-ranged requirements are only supported "
                "for wonders."));
  }
  return 0;
}

/****************************************************************************
  Returns the number of buildings of a certain type in a city.
****************************************************************************/
static int num_city_buildings(const struct city *pcity,
			      const struct impr_type *building)
{
  return (city_has_building(pcity, building) ? 1 : 0);
}

/****************************************************************************
  Is there any source buildings within range of the target?

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
static int buildings_in_range(const struct player *target_player,
                              const struct city *target_city,
                              const struct impr_type *target_building,
                              enum req_range range,
                              bool survives,
                              const struct impr_type *source)
{
  if (improvement_obsolete(target_player, source, target_city)) {
    return FALSE;
  }

  if (survives) {
    if (range == REQ_RANGE_WORLD) {
      return num_world_buildings_total(source);
    } else if (range == REQ_RANGE_PLAYER) {
      return player_has_ever_built(target_player, source);
    } else {
      /* There is no sources cache for this. */
      /* TRANS: Obscure ruleset error. */
      log_error(_("Surviving requirements are only supported "
                  "at world and player ranges."));
      return FALSE;
    }
  }

  switch (range) {
  case REQ_RANGE_WORLD:
    return num_world_buildings(source);
  case REQ_RANGE_ALLIANCE:
    players_iterate_alive(plr2) {
      if (pplayers_allied(target_player, plr2)
          && num_player_buildings(plr2, source) > 0) {
        return TRUE;
      }
    } players_iterate_alive_end;

    return FALSE;
  case REQ_RANGE_PLAYER:
    return target_player ? num_player_buildings(target_player, source) : FALSE;
  case REQ_RANGE_CONTINENT:
    if (target_player && target_city) {
      int continent = tile_continent(target_city->tile);

      return num_continent_buildings(target_player, continent, source);
    } else {
      /* At present, "Continent" effects can affect only
       * cities and units in cities. */
      return FALSE;
    }
  case REQ_RANGE_CITY:
    return target_city ? num_city_buildings(target_city, source) : FALSE;
  case REQ_RANGE_LOCAL:
    if (target_building && target_building == source) {
      return num_city_buildings(target_city, source);
    } else {
      /* TODO: other local targets */
      return FALSE;
    }
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
    return FALSE;
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);
  return FALSE;
}

/****************************************************************************
  Is there a source tech within range of the target?
****************************************************************************/
static enum fc_tristate is_tech_in_range(const struct player *target_player,
                                         enum req_range range,
                                         Tech_type_id tech)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    if (NULL != target_player) {
      return BOOL_TO_TRISTATE(TECH_KNOWN == player_invention_state(target_player, tech));
    } else {
      return TRI_MAYBE;
    }
  case REQ_RANGE_ALLIANCE:
   if (NULL == target_player) {
     return TRI_MAYBE;
   }
   players_iterate_alive(plr2) {
     if (pplayers_allied(target_player, plr2)) {
       if (player_invention_state(plr2, tech) == TECH_KNOWN) {
         return TRI_YES;
       }
     }
   } players_iterate_alive_end;

   return TRI_NO;
  case REQ_RANGE_WORLD:
    return BOOL_TO_TRISTATE(game.info.global_advances[tech]);
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a tech with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_techflag_in_range(const struct player *target_player,
                                             enum req_range range,
                                             enum tech_flag_id techflag)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    if (NULL != target_player) {
      return BOOL_TO_TRISTATE(player_knows_techs_with_flag(target_player, techflag));
    } else {
      return TRI_MAYBE;
    }
    break;
  case REQ_RANGE_ALLIANCE:
    if (NULL == target_player) {
      return TRI_MAYBE;
    }
    players_iterate_alive(plr2) {
      if (pplayers_allied(target_player, plr2)
          && player_knows_techs_with_flag(plr2, techflag)) {
        return TRI_YES;
      }
    } players_iterate_alive_end;
    return TRI_NO;
  case REQ_RANGE_WORLD:
    players_iterate(pplayer) {
      if (player_knows_techs_with_flag(pplayer, techflag)) {
        return TRI_YES;
      }
    } players_iterate_end;

    return TRI_NO;
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a tile with max X units within range of the target?
****************************************************************************/
static enum fc_tristate
is_tile_units_in_range(const struct tile *target_tile, enum req_range range,
                       int maxUnits)
{
  /* TODO: if can't see V_INVIS -> TRI_MAYBE */
  switch (range) {
  case REQ_RANGE_LOCAL:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(unit_list_size(target_tile->units) <= maxUnits);
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    cardinal_adjc_iterate(target_tile, adjc_tile) {
      if (unit_list_size(adjc_tile->units) <= maxUnits) {
        return TRI_YES;
      }
    } cardinal_adjc_iterate_end;
    return TRI_NO;
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    adjc_iterate(target_tile, adjc_tile) {
      if (unit_list_size(adjc_tile->units) <= maxUnits) {
        return TRI_YES;
      }
    } adjc_iterate_end;
    return TRI_NO;
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a source extra type within range of the target?
****************************************************************************/
static enum fc_tristate is_extra_type_in_range(const struct tile *target_tile,
                                               const struct city *target_city,
                                               enum req_range range, bool survives,
                                               struct extra_type *pextra)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has extra of requested type. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(tile_has_extra(target_tile, pextra));
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_extra_card_near(target_tile, pextra));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_extra_near_tile(target_tile, pextra));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    city_tile_iterate(city_map_radius_sq_get(target_city),
                      city_tile(target_city), ptile) {
      if (tile_has_extra(ptile, pextra)) {
        return TRI_YES;
      }
    } city_tile_iterate_end;

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a source tile within range of the target?
****************************************************************************/
static enum fc_tristate is_terrain_in_range(const struct tile *target_tile,
                                            const struct city *target_city,
                                            enum req_range range, bool survives,
                                            const struct terrain *pterrain)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has the terrain. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return pterrain && tile_terrain(target_tile) == pterrain;
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(pterrain && is_terrain_card_near(target_tile, pterrain, TRUE));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(pterrain && is_terrain_near_tile(target_tile, pterrain, TRUE));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    if (pterrain != NULL) {
      city_tile_iterate(city_map_radius_sq_get(target_city),
                        city_tile(target_city), ptile) {
        if (tile_terrain(ptile) == pterrain) {
          return TRI_YES;
        }
      } city_tile_iterate_end;
    }
    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a source tile within range of the target?
****************************************************************************/
static enum fc_tristate is_resource_in_range(const struct tile *target_tile,
                                             const struct city *target_city,
                                             enum req_range range, bool survives,
                                             const struct resource *pres)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    /* The requirement is filled if the tile has the terrain. */
    return BOOL_TO_TRISTATE(pres && tile_resource(target_tile) == pres);
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(pres && is_resource_card_near(target_tile, pres, TRUE));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(pres && is_resource_near_tile(target_tile, pres, TRUE));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    if (pres != NULL) {
      city_tile_iterate(city_map_radius_sq_get(target_city),
                        city_tile(target_city), ptile) {
        if (tile_resource(ptile) == pres) {
          return TRI_YES;
        }
      } city_tile_iterate_end;
    }

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a source terrain class within range of the target?
****************************************************************************/
static enum fc_tristate is_terrain_class_in_range(const struct tile *target_tile,
                                                  const struct city *target_city,
                                                  enum req_range range, bool survives,
                                                  enum terrain_class pclass)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has the terrain of correct class. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(terrain_type_terrain_class(tile_terrain(target_tile)) == pclass);
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_terrain_class_card_near(target_tile, pclass));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_terrain_class_near_tile(target_tile, pclass));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    city_tile_iterate(city_map_radius_sq_get(target_city),
                      city_tile(target_city), ptile) {
      if (terrain_type_terrain_class(tile_terrain(ptile)) == pclass) {
        return TRI_YES;
      }
    } city_tile_iterate_end;

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a terrain with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_terrainflag_in_range(const struct tile *target_tile,
                                                const struct city *target_city,
                                                enum req_range range, bool survives,
                                                enum terrain_flag_id terrflag)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has the terrain with correct flag. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(terrain_has_flag(tile_terrain(target_tile), terrflag));
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return is_terrain_flag_card_near(target_tile, terrflag);
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_terrain_flag_near_tile(target_tile, terrflag));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    city_tile_iterate(city_map_radius_sq_get(target_city),
                      city_tile(target_city), ptile) {
      if (terrain_has_flag(tile_terrain(ptile), terrflag)) {
        return TRI_YES;
      }
    } city_tile_iterate_end;

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a base with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_baseflag_in_range(const struct tile *target_tile,
                                             const struct city *target_city,
                                             enum req_range range, bool survives,
                                             enum base_flag_id baseflag)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has a base with correct flag. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(tile_has_base_flag(target_tile, baseflag));
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_base_flag_card_near(target_tile, baseflag));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_base_flag_near_tile(target_tile, baseflag));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    city_tile_iterate(city_map_radius_sq_get(target_city),
                      city_tile(target_city), ptile) {
      if (tile_has_base_flag(ptile, baseflag)) {
        return TRI_YES;
      }
    } city_tile_iterate_end;

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a road with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_roadflag_in_range(const struct tile *target_tile,
                                             const struct city *target_city,
                                             enum req_range range, bool survives,
                                             enum road_flag_id roadflag)
{
  switch (range) {
  case REQ_RANGE_LOCAL:
    /* The requirement is filled if the tile has a road with correct flag. */
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(tile_has_road_flag(target_tile, roadflag));
  case REQ_RANGE_CADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_road_flag_card_near(target_tile, roadflag));
  case REQ_RANGE_ADJACENT:
    if (!target_tile) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_road_flag_near_tile(target_tile, roadflag));
  case REQ_RANGE_CITY:
    if (!target_city) {
      return TRI_MAYBE;
    }
    city_tile_iterate(city_map_radius_sq_get(target_city),
                      city_tile(target_city), ptile) {
      if (tile_has_road_flag(ptile, roadflag)) {
        return TRI_YES;
      }
    } city_tile_iterate_end;

    return TRI_NO;
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a terrain which can support the specified infrastructure
  within range of the target?
****************************************************************************/
static enum fc_tristate is_terrain_alter_possible_in_range(const struct tile *target_tile,
                                                           enum req_range range,
                                                           bool survives,
                                                           enum terrain_alteration alteration)
{
  if (!target_tile) {
    return TRI_MAYBE;
  }

  switch (range) {
  case REQ_RANGE_LOCAL:
    return BOOL_TO_TRISTATE(terrain_can_support_alteration(tile_terrain(target_tile),
                                                           alteration));
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT: /* XXX Could in principle support ADJACENT. */
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a nation within range of the target?
****************************************************************************/
static enum fc_tristate is_nation_in_range(const struct player *target_player,
                                           enum req_range range, bool survives,
                                           const struct nation_type *nation)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    if (target_player == NULL) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(nation_of_player(target_player) == nation);
  case REQ_RANGE_ALLIANCE:
    if (target_player == NULL) {
      return TRI_MAYBE;
    }
    players_iterate_alive(plr2) {
      if (pplayers_allied(target_player, plr2)) {
        if (nation_of_player(plr2) == nation) {
          return TRI_YES;
        }
      }
    } players_iterate_alive_end;
    return TRI_NO;
  case REQ_RANGE_WORLD:
    return BOOL_TO_TRISTATE(NULL != nation->player
                            && (survives || nation->player->is_alive));
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a nationality within range of the target?
****************************************************************************/
static enum fc_tristate is_nationality_in_range(const struct city *target_city,
                                                enum req_range range,
                                                const struct nation_type *nationality)
{
  switch (range) {
  case REQ_RANGE_CITY:
   if (target_city == NULL) {
     return TRI_MAYBE;
   }
   citizens_iterate(target_city, slot, count) {
     if (player_slot_get_player(slot)->nation == nationality) {
       return TRI_YES;
     }
   } citizens_iterate_end;

   return TRI_NO;
  case REQ_RANGE_PLAYER:
  case REQ_RANGE_ALLIANCE:
  case REQ_RANGE_WORLD:
  case REQ_RANGE_LOCAL:
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/**************************************************************************
  Is the diplomatic state within range of the target?
**************************************************************************/
static enum fc_tristate is_diplrel_in_range(const struct player *target_player,
                                            const struct player *other_player,
                                            enum req_range range,
                                            int diplrel)
{
  switch (range) {
  case REQ_RANGE_PLAYER:
    if (target_player == NULL) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_diplrel_to_other(target_player, diplrel));
  case REQ_RANGE_ALLIANCE:
    if (target_player == NULL) {
      return TRI_MAYBE;
    }
    players_iterate_alive(plr2) {
      if (target_player != plr2 && pplayers_allied(target_player, plr2)) {
        if (is_diplrel_to_other(plr2, diplrel)) {
          return TRI_YES;
        }
      }
    } players_iterate_alive_end;
    return TRI_NO;
  case REQ_RANGE_WORLD:
    players_iterate_alive(player1) {
      if (is_diplrel_to_other(player1, diplrel)) {
        return TRI_YES;
      }
    } players_iterate_alive_end;

    return TRI_NO;
  case REQ_RANGE_LOCAL:
    if (target_player == NULL || other_player == NULL) {
      return TRI_MAYBE;
    }
    return BOOL_TO_TRISTATE(is_diplrel_between(target_player, other_player, diplrel));
  case REQ_RANGE_CADJACENT:
  case REQ_RANGE_ADJACENT:
  case REQ_RANGE_CITY:
  case REQ_RANGE_CONTINENT:
  case REQ_RANGE_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid range %d.", range);

  return TRI_MAYBE;
}

/****************************************************************************
  Is there a unit of the given type within range of the target?
****************************************************************************/
static enum fc_tristate is_unittype_in_range(const struct unit_type *target_unittype,
                                             enum req_range range, bool survives,
                                             struct unit_type *punittype)
{
  /* If no target_unittype is given, we allow the req to be met.  This is
   * to allow querying of certain effect types (like the presence of city
   * walls) without actually knowing the target unit. */
  return BOOL_TO_TRISTATE(range == REQ_RANGE_LOCAL
                          && (!target_unittype
                              || target_unittype == punittype));
}

/****************************************************************************
  Is there a unit with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_unitflag_in_range(const struct unit_type *target_unittype,
                                             enum req_range range, bool survives,
                                             enum unit_type_flag_id unitflag)
{
  /* If no target_unittype is given, we allow the req to be met.  This is
   * to allow querying of certain effect types (like the presence of city
   * walls) without actually knowing the target unit. */
  if (range != REQ_RANGE_LOCAL) {
    return TRI_NO;
  }
  if (!target_unittype) {
    return TRI_MAYBE;
  }

  return BOOL_TO_TRISTATE(utype_has_flag(target_unittype, unitflag));
}

/****************************************************************************
  Is there a unit with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_unitclass_in_range(const struct unit_type *target_unittype,
                                              enum req_range range, bool survives,
                                              struct unit_class *pclass)
{
  /* If no target_unittype is given, we allow the req to be met.  This is
   * to allow querying of certain effect types (like the presence of city
   * walls) without actually knowing the target unit. */
  return BOOL_TO_TRISTATE(range == REQ_RANGE_LOCAL
                          && (!target_unittype
                              || utype_class(target_unittype) == pclass));
}

/****************************************************************************
  Is there a unit with the given flag within range of the target?
****************************************************************************/
static enum fc_tristate is_unitclassflag_in_range(const struct unit_type *target_unittype,
                                                  enum req_range range, bool survives,
                                                  enum unit_class_flag_id ucflag)
{
  /* If no target_unittype is given, we allow the req to be met.  This is
   * to allow querying of certain effect types (like the presence of city
   * walls) without actually knowing the target unit. */
  return BOOL_TO_TRISTATE(range == REQ_RANGE_LOCAL
                          && (!target_unittype
                              || uclass_has_flag(utype_class(target_unittype), ucflag)));
}

/****************************************************************************
  Is center of given city in tile. If city is NULL, any city will do.
****************************************************************************/
static bool is_city_in_tile(const struct tile *ptile,
			    const struct city *pcity)
{
  if (pcity == NULL) {
    return tile_city(ptile) != NULL;
  } else {
    return is_city_center(pcity, ptile);
  }
}

/****************************************************************************
  Is center of given city in range. If city is NULL, any city will do.
****************************************************************************/
static enum fc_tristate is_citytile_in_range(const struct tile *target_tile,
                                             const struct city *target_city,
                                             enum req_range range,
                                             enum citytile_type citytile)
{
  if (target_tile) {
    if (citytile == CITYT_CENTER) {
      switch (range) {
      case REQ_RANGE_LOCAL:
        return BOOL_TO_TRISTATE(is_city_in_tile(target_tile, target_city));
      case REQ_RANGE_CADJACENT:
        cardinal_adjc_iterate(target_tile, adjc_tile) {
          if (is_city_in_tile(adjc_tile, target_city)) {
            return TRI_YES;
          }
        } cardinal_adjc_iterate_end;

        return TRI_NO;
      case REQ_RANGE_ADJACENT:
        adjc_iterate(target_tile, adjc_tile) {
          if (is_city_in_tile(adjc_tile, target_city)) {
            return TRI_YES;
          }
        } adjc_iterate_end;

        return TRI_NO;
      case REQ_RANGE_CITY:
      case REQ_RANGE_CONTINENT:
      case REQ_RANGE_PLAYER:
      case REQ_RANGE_ALLIANCE:
      case REQ_RANGE_WORLD:
      case REQ_RANGE_COUNT:
	break;
      }

      fc_assert_msg(FALSE, "Invalid range %d for citytile.", range);

      return TRI_MAYBE;
    } else {
      /* Not implemented */
      log_error("is_req_active(): citytile %d not supported.",
		citytile);
      return TRI_MAYBE;
    }
  } else {
    return TRI_MAYBE;
  }
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
		   const struct player *other_player,
		   const struct city *target_city,
		   const struct impr_type *target_building,
		   const struct tile *target_tile,
		   const struct unit_type *target_unittype,
		   const struct output_type *target_output,
		   const struct specialist *target_specialist,
		   const struct requirement *req,
                   const enum   req_problem_type prob_type)
{
  enum fc_tristate eval = TRI_NO;

  /* Note the target may actually not exist.  In particular, effects that
   * have a VUT_RESOURCE, or VUT_TERRAIN may often be passed
   * to this function with a city as their target.  In this case the
   * requirement is simply not met. */
  switch (req->source.kind) {
  case VUT_NONE:
    eval = TRI_YES;
    break;
  case VUT_ADVANCE:
    /* The requirement is filled if the player owns the tech. */
    eval = is_tech_in_range(target_player, req->range,
                            advance_number(req->source.value.advance));
    break;
 case VUT_TECHFLAG:
    eval = is_techflag_in_range(target_player, req->range,
                                req->source.value.techflag);
    break;
  case VUT_GOVERNMENT:
    /* The requirement is filled if the player is using the government. */
    if (target_player == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = BOOL_TO_TRISTATE(government_of_player(target_player) == req->source.value.govern);
    }
    break;
  case VUT_ACHIEVEMENT:
    if (req->range == REQ_RANGE_WORLD) {
      eval = BOOL_TO_TRISTATE(achievement_claimed(req->source.value.achievement));
    } else if (target_player == NULL) {
      eval = TRI_MAYBE;
    } else if (req->range == REQ_RANGE_ALLIANCE) {
      eval = TRI_NO;
      players_iterate_alive(plr2) {
        if (pplayers_allied(target_player, plr2)
            && achievement_player_has(req->source.value.achievement, plr2)) {
          eval = TRI_YES;
          break;
        }
      } players_iterate_alive_end;
    } else if (req->range == REQ_RANGE_PLAYER) {
      if (achievement_player_has(req->source.value.achievement,
                                 target_player)) {
        eval = TRI_YES;
      } else {
        eval = TRI_NO;
      }
    } else {
      log_error("is_req_active(): illegal range for achievement requirement.");
      eval = TRI_MAYBE;
    }
    break;
  case VUT_IMPROVEMENT:
    /* The requirement is filled if there's at least one of the building
     * in the city.  (This is a slightly nonstandard use of
     * count_sources_in_range.) */
    /* With NULL target_player buildings_in_range() would
     * return FALSE, which would cause evaluation to return FALSE
     * even for RPT_POSSIBLE */
    if ((req->range == REQ_RANGE_PLAYER && target_player == NULL)
        || (req->range == REQ_RANGE_CITY && target_city == NULL)) {
      eval = TRI_MAYBE;
    } else {
      eval = BOOL_TO_TRISTATE(buildings_in_range(target_player, target_city,
                                                 target_building,
                                                 req->range, req->survives,
                                                 req->source.value.building));
    }
    break;
  case VUT_EXTRA:
    eval = is_extra_type_in_range(target_tile, target_city,
                                  req->range, req->survives,
                                  req->source.value.extra);
    break;
  case VUT_TERRAIN:
    eval = is_terrain_in_range(target_tile, target_city,
                               req->range, req->survives,
                               req->source.value.terrain);
    break;
  case VUT_TERRFLAG:
    eval = is_terrainflag_in_range(target_tile, target_city,
                                   req->range, req->survives,
                                   req->source.value.terrainflag);
    break;
  case VUT_RESOURCE:
    eval = is_resource_in_range(target_tile, target_city,
                                req->range, req->survives,
                                req->source.value.resource);
    break;
  case VUT_NATION:
    eval = is_nation_in_range(target_player, req->range, req->survives,
                              req->source.value.nation);
    break;
  case VUT_NATIONALITY:
    eval = is_nationality_in_range(target_city, req->range,
                                   req->source.value.nationality);
    break;
  case VUT_DIPLREL:
    eval = is_diplrel_in_range(target_player, other_player, req->range,
                               req->source.value.diplrel);
    break;
  case VUT_UTYPE:
    if (target_unittype == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = is_unittype_in_range(target_unittype,
                                  req->range, req->survives,
                                  req->source.value.utype);
    }
    break;
  case VUT_UTFLAG:
    eval = is_unitflag_in_range(target_unittype,
				req->range, req->survives,
				req->source.value.unitflag);
    break;
  case VUT_UCLASS:
    if (target_unittype == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = is_unitclass_in_range(target_unittype,
                                   req->range, req->survives,
                                   req->source.value.uclass);
    }
    break;
  case VUT_UCFLAG:
    if (target_unittype == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = is_unitclassflag_in_range(target_unittype,
                                       req->range, req->survives,
                                       req->source.value.unitclassflag);
    }
    break;
  case VUT_OTYPE:
    eval = BOOL_TO_TRISTATE(target_output
                            && target_output->index == req->source.value.outputtype);
    break;
  case VUT_SPECIALIST:
    eval = BOOL_TO_TRISTATE(target_specialist
                            && target_specialist == req->source.value.specialist);
    break;
  case VUT_MINSIZE:
    if (target_city == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = BOOL_TO_TRISTATE(city_size_get(target_city) >= req->source.value.minsize);
    }
    break;
  case VUT_AI_LEVEL:
    if (target_player == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = BOOL_TO_TRISTATE(target_player->ai_controlled
                              && target_player->ai_common.skill_level == req->source.value.ai_level);
    }
    break;
  case VUT_MAXTILEUNITS:
    eval = is_tile_units_in_range(target_tile, req->range,
                                  req->source.value.maxTileUnits);
    break;
  case VUT_TERRAINCLASS:
    eval = is_terrain_class_in_range(target_tile, target_city,
                                     req->range, req->survives,
                                     req->source.value.terrainclass);
    break;
  case VUT_BASEFLAG:
    eval = is_baseflag_in_range(target_tile, target_city,
                                 req->range, req->survives,
                                 req->source.value.baseflag);
    break;
  case VUT_ROADFLAG:
    eval = is_roadflag_in_range(target_tile, target_city,
                                 req->range, req->survives,
                                 req->source.value.roadflag);
    break;
  case VUT_MINYEAR:
    eval = BOOL_TO_TRISTATE(game.info.year >= req->source.value.minyear);
    break;
  case VUT_TERRAINALTER:
    if (target_tile == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = is_terrain_alter_possible_in_range(target_tile,
                                                req->range, req->survives,
                                                req->source.value.terrainalter);
    }
    break;
  case VUT_CITYTILE:
    if (target_tile == NULL) {
      eval = TRI_MAYBE;
    } else {
      eval = is_citytile_in_range(target_tile, target_city,
                                  req->range,
                                  req->source.value.citytile);
    }
    break;
  case VUT_COUNT:
    log_error("is_req_active(): invalid source kind %d.", req->source.kind);
    return FALSE;
  }

  if (eval == TRI_MAYBE) {
    if (prob_type == RPT_POSSIBLE) {
      return TRUE;
    } else {
      return FALSE;
    }
  }
  if (req->present) {
    return (eval == TRI_YES);
  } else {
    return (eval == TRI_NO);
  }
}

/****************************************************************************
  Checks the requirement(s) to see if they are active on the given target.

  target gives the type of the target
  (player,city,building,tile) give the exact target

  reqs gives the requirement vector.
  The function returns TRUE only if all requirements are active.

  Make sure you give all aspects of the target when calling this function:
  for instance if you have TARGET_CITY pass the city's owner as the target
  player as well as the city itself as the target city.
****************************************************************************/
bool are_reqs_active(const struct player *target_player,
		     const struct player *other_player,
		     const struct city *target_city,
		     const struct impr_type *target_building,
		     const struct tile *target_tile,
		     const struct unit_type *target_unittype,
		     const struct output_type *target_output,
		     const struct specialist *target_specialist,
		     const struct requirement_vector *reqs,
                     const enum   req_problem_type prob_type)
{
  requirement_vector_iterate(reqs, preq) {
    if (!is_req_active(target_player, other_player, target_city,
                       target_building, target_tile, target_unittype,
                       target_output, target_specialist,
		       preq, prob_type)) {
      return FALSE;
    }
  } requirement_vector_iterate_end;
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
  switch (req->source.kind) {
  case VUT_NATION:
  case VUT_NONE:
  case VUT_OTYPE:
  case VUT_SPECIALIST:	/* Only so long as it's at local range only */
  case VUT_AI_LEVEL:
  case VUT_CITYTILE:
    return TRUE;
  case VUT_ADVANCE:
  case VUT_TECHFLAG:
  case VUT_GOVERNMENT:
  case VUT_ACHIEVEMENT:
  case VUT_IMPROVEMENT:
  case VUT_MINSIZE:
  case VUT_NATIONALITY:
  case VUT_DIPLREL:
  case VUT_MAXTILEUNITS:
  case VUT_UTYPE:	/* Not sure about this one */
  case VUT_UTFLAG:	/* Not sure about this one */
  case VUT_UCLASS:	/* Not sure about this one */
  case VUT_UCFLAG:	/* Not sure about this one */
  case VUT_ROADFLAG:
    return FALSE;
  case VUT_TERRAIN:
  case VUT_EXTRA:
  case VUT_RESOURCE:
  case VUT_TERRAINCLASS:
  case VUT_TERRFLAG:
  case VUT_TERRAINALTER:
  case VUT_BASEFLAG:
    /* Terrains, specials and bases aren't really unchanging; in fact they're
     * practically guaranteed to change.  We return TRUE here for historical
     * reasons and so that the AI doesn't get confused (since the AI
     * doesn't know how to meet special and terrain requirements). */
    return TRUE;
  case VUT_MINYEAR:
    /* Once year is reached, it does not change again */
    return req->source.value.minyear > game.info.year;
  case VUT_COUNT:
    break;
  }
  fc_assert_msg(FALSE, "Invalid source kind %d.", req->source.kind);
  return TRUE;
}

/****************************************************************************
  Return TRUE iff the two sources are equivalent.  Note this isn't the
  same as an == or memcmp check.
*****************************************************************************/
bool are_universals_equal(const struct universal *psource1,
			  const struct universal *psource2)
{
  if (psource1->kind != psource2->kind) {
    return FALSE;
  }
  switch (psource1->kind) {
  case VUT_NONE:
    return TRUE;
  case VUT_ADVANCE:
    return psource1->value.advance == psource2->value.advance;
  case VUT_TECHFLAG:
    return psource1->value.techflag == psource2->value.techflag;
  case VUT_GOVERNMENT:
    return psource1->value.govern == psource2->value.govern;
  case VUT_ACHIEVEMENT:
    return psource1->value.achievement == psource2->value.achievement;
  case VUT_IMPROVEMENT:
    return psource1->value.building == psource2->value.building;
  case VUT_EXTRA:
    return psource1->value.extra == psource2->value.extra;
  case VUT_TERRAIN:
    return psource1->value.terrain == psource2->value.terrain;
  case VUT_TERRFLAG:
    return psource1->value.terrainflag == psource2->value.terrainflag;
  case VUT_RESOURCE:
    return psource1->value.resource == psource2->value.resource;
  case VUT_NATION:
    return psource1->value.nation == psource2->value.nation;
  case VUT_NATIONALITY:
    return psource1->value.nationality == psource2->value.nationality;
  case VUT_DIPLREL:
    return psource1->value.diplrel == psource2->value.diplrel;
  case VUT_UTYPE:
    return psource1->value.utype == psource2->value.utype;
  case VUT_UTFLAG:
    return psource1->value.unitflag == psource2->value.unitflag;
  case VUT_UCLASS:
    return psource1->value.uclass == psource2->value.uclass;
  case VUT_UCFLAG:
    return psource1->value.unitclassflag == psource2->value.unitclassflag;
  case VUT_OTYPE:
    return psource1->value.outputtype == psource2->value.outputtype;
  case VUT_SPECIALIST:
    return psource1->value.specialist == psource2->value.specialist;
  case VUT_MINSIZE:
    return psource1->value.minsize == psource2->value.minsize;
  case VUT_AI_LEVEL:
    return psource1->value.ai_level == psource2->value.ai_level;
  case VUT_MAXTILEUNITS:
    return psource1->value.maxTileUnits == psource2->value.maxTileUnits;
  case VUT_TERRAINCLASS:
    return psource1->value.terrainclass == psource2->value.terrainclass;
  case VUT_BASEFLAG:
    return psource1->value.baseflag == psource2->value.baseflag;
  case VUT_ROADFLAG:
    return psource1->value.roadflag == psource2->value.roadflag;
  case VUT_MINYEAR:
    return psource1->value.minyear == psource2->value.minyear;
  case VUT_TERRAINALTER:
    return psource1->value.terrainalter == psource2->value.terrainalter;
  case VUT_CITYTILE:
    return psource1->value.citytile == psource2->value.citytile;
  case VUT_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid source kind %d.", psource1->kind);
  return FALSE;
}

/****************************************************************************
  Return the (untranslated) rule name of the universal.
  You don't have to free the return pointer.
*****************************************************************************/
const char *universal_rule_name(const struct universal *psource)
{
  static char buffer[10];

  switch (psource->kind) {
  case VUT_NONE:
    return "(none)";
  case VUT_CITYTILE:
    if (psource->value.citytile == CITYT_CENTER) {
      return "Center";
    } else {
      return "(none)";
    }
  case VUT_MINYEAR:
    fc_snprintf(buffer, sizeof(buffer), "%d", psource->value.minyear);

    return buffer;
  case VUT_ADVANCE:
    return advance_rule_name(psource->value.advance);
  case VUT_TECHFLAG:
    return tech_flag_id_name(psource->value.techflag);
  case VUT_GOVERNMENT:
    return government_rule_name(psource->value.govern);
  case VUT_ACHIEVEMENT:
    return achievement_rule_name(psource->value.achievement);
  case VUT_IMPROVEMENT:
    return improvement_rule_name(psource->value.building);
  case VUT_EXTRA:
    return extra_rule_name(psource->value.extra);
  case VUT_TERRAIN:
    return terrain_rule_name(psource->value.terrain);
  case VUT_TERRFLAG:
    return terrain_flag_id_name(psource->value.terrainflag);
  case VUT_RESOURCE:
    return resource_rule_name(psource->value.resource);
  case VUT_NATION:
    return nation_rule_name(psource->value.nation);
  case VUT_DIPLREL:
    return diplrel_rule_name(psource->value.diplrel);
  case VUT_NATIONALITY:
    return nation_rule_name(psource->value.nationality);
  case VUT_UTYPE:
    return utype_rule_name(psource->value.utype);
  case VUT_UTFLAG:
    return unit_type_flag_id_name(psource->value.unitflag);
  case VUT_UCLASS:
    return uclass_rule_name(psource->value.uclass);
  case VUT_UCFLAG:
    return unit_class_flag_id_name(psource->value.unitclassflag);
  case VUT_OTYPE:
    return get_output_name(psource->value.outputtype);
  case VUT_SPECIALIST:
    return specialist_rule_name(psource->value.specialist);
  case VUT_MINSIZE:
    fc_snprintf(buffer, sizeof(buffer), "%d", psource->value.minsize);

    return buffer;
  case VUT_AI_LEVEL:
    return ai_level_name(psource->value.ai_level);
  case VUT_MAXTILEUNITS:
    fc_snprintf(buffer, sizeof(buffer), "%d", psource->value.maxTileUnits);
    return buffer;
  case VUT_TERRAINCLASS:
    return terrain_class_name(psource->value.terrainclass);
  case VUT_BASEFLAG:
    return base_flag_id_name(psource->value.baseflag);
  case VUT_ROADFLAG:
    return road_flag_id_name(psource->value.roadflag);
  case VUT_TERRAINALTER:
    return terrain_alteration_name(psource->value.terrainalter);
  case VUT_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid source kind %d.", psource->kind);
  return NULL;
}

/****************************************************************************
  Make user-friendly text for the source.  The text is put into a user
  buffer which is also returned.
*****************************************************************************/
const char *universal_name_translation(const struct universal *psource,
				       char *buf, size_t bufsz)
{
  buf[0] = '\0'; /* to be safe. */
  switch (psource->kind) {
  case VUT_NONE:
    /* TRANS: missing value */
    fc_strlcat(buf, _("(none)"), bufsz);
    return buf;
  case VUT_ADVANCE:
    fc_strlcat(buf, advance_name_translation(psource->value.advance), bufsz);
    return buf;
  case VUT_TECHFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" tech"),
                 /* flag names are never translated */
                 tech_flag_id_name(psource->value.techflag));
    return buf;
  case VUT_GOVERNMENT:
    fc_strlcat(buf, government_name_translation(psource->value.govern),
               bufsz);
    return buf;
  case VUT_ACHIEVEMENT:
    fc_strlcat(buf, achievement_name_translation(psource->value.achievement),
               bufsz);
    return buf;
  case VUT_IMPROVEMENT:
    fc_strlcat(buf, improvement_name_translation(psource->value.building),
               bufsz);
    return buf;
  case VUT_EXTRA:
    fc_strlcat(buf, extra_name_translation(psource->value.extra), bufsz);
    return buf;
  case VUT_TERRAIN:
    fc_strlcat(buf, terrain_name_translation(psource->value.terrain), bufsz);
    return buf;
  case VUT_RESOURCE:
    fc_strlcat(buf, resource_name_translation(psource->value.resource), bufsz);
    return buf;
  case VUT_NATION:
    fc_strlcat(buf, nation_adjective_translation(psource->value.nation),
               bufsz);
    return buf;
  case VUT_NATIONALITY:
    fc_strlcat(buf, nation_adjective_translation(psource->value.nationality),
               bufsz);
    return buf;
  case VUT_DIPLREL:
    fc_strlcat(buf, diplrel_name_translation(psource->value.diplrel),
               bufsz);
    return buf;
  case VUT_UTYPE:
    fc_strlcat(buf, utype_name_translation(psource->value.utype), bufsz);
    return buf;
  case VUT_UTFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" units"),
                 /* flag names are never translated */
                 unit_type_flag_id_name(psource->value.unitflag));
    return buf;
  case VUT_UCLASS:
    cat_snprintf(buf, bufsz, _("%s units"),
		 uclass_name_translation(psource->value.uclass));
    return buf;
  case VUT_UCFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" units"),
                 /* flag names are never translated */
                 unit_class_flag_id_name(psource->value.unitclassflag));
    return buf;
  case VUT_OTYPE:
    /* FIXME */
    fc_strlcat(buf, get_output_name(psource->value.outputtype), bufsz);
    return buf;
  case VUT_SPECIALIST:
    fc_strlcat(buf, specialist_plural_translation(psource->value.specialist),
               bufsz);
    return buf;
  case VUT_MINSIZE:
    cat_snprintf(buf, bufsz, _("Size %d"),
		 psource->value.minsize);
    return buf;
  case VUT_AI_LEVEL:
    /* TRANS: "Hard AI" */
    cat_snprintf(buf, bufsz, _("%s AI"),
                 ai_level_name(psource->value.ai_level)); /* FIXME */
    return buf;
  case VUT_MAXTILEUNITS:
    cat_snprintf(buf, bufsz, _("Max %d unit(s) on the tile"),
                 psource->value.maxTileUnits);
    return buf;
  case VUT_TERRAINCLASS:
    /* TRANS: "Land terrain" */
    cat_snprintf(buf, bufsz, _("%s terrain"),
                 terrain_class_name_translation(psource->value.terrainclass));
    return buf;
  case VUT_TERRFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" terrain"),
                 /* flag names are never translated */
                 terrain_flag_id_name(psource->value.terrainflag));
    return buf;
  case VUT_BASEFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" base"),
                 /* flag names are never translated */
                 base_flag_id_name(psource->value.baseflag));
  case VUT_ROADFLAG:
    cat_snprintf(buf, bufsz, _("\"%s\" road"),
                 /* flag names are never translated */
                 road_flag_id_name(psource->value.roadflag));
  case VUT_MINYEAR:
    cat_snprintf(buf, bufsz, _("After %s"),
                 textyear(psource->value.minyear));
    return buf;
  case VUT_TERRAINALTER:
    /* TRANS: "Irrigation possible" */
    cat_snprintf(buf, bufsz, _("%s possible"),
                 Q_(terrain_alteration_name(psource->value.terrainalter)));
    return buf;
  case VUT_CITYTILE:
    fc_strlcat(buf, _("City center tile"), bufsz);
    return buf;
  case VUT_COUNT:
    break;
  }

  fc_assert_msg(FALSE, "Invalid source kind %d.", psource->kind);
  return buf;
}

/****************************************************************************
  Return untranslated name of the universal source name.
*****************************************************************************/
const char *universal_type_rule_name(const struct universal *psource)
{
  return universals_n_name(psource->kind);
}

/**************************************************************************
  Return the number of shields it takes to build this universal.
**************************************************************************/
int universal_build_shield_cost(const struct universal *target)
{
  switch (target->kind) {
  case VUT_IMPROVEMENT:
    return impr_build_shield_cost(target->value.building);
  case VUT_UTYPE:
    return utype_build_shield_cost(target->value.utype);
  default:
    break;
  }
  return FC_INFINITY;
}
