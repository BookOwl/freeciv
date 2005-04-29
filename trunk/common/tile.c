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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include "tile.h"

/****************************************************************************
  Return the player who owns this tile (or NULL if none).
****************************************************************************/
struct player *tile_get_owner(const struct tile *ptile)
{
  return ptile->owner;
}

/****************************************************************************
  Set the owner of a tile (may be NULL).
****************************************************************************/
void tile_set_owner(struct tile *ptile, struct player *owner)
{
  ptile->owner = owner;
}

/****************************************************************************
  Return the city on this tile (or NULL if none).
****************************************************************************/
struct city *tile_get_city(const struct tile *ptile)
{
  return ptile->city;
}

/****************************************************************************
  Set the city on the tile (may be NULL).
****************************************************************************/
void tile_set_city(struct tile *ptile, struct city *pcity)
{
  ptile->city = pcity;
}

/****************************************************************************
  Return the terrain ID of the tile.  Terrains are defined in the ruleset
  (see terrain.h).
****************************************************************************/
Terrain_type_id tile_get_terrain(const struct tile *ptile)
{
  return ptile->terrain;
}

/****************************************************************************
  Set the terrain ID of the tile.  See tile_get_terrain.
****************************************************************************/
void tile_set_terrain(struct tile *ptile, Terrain_type_id ter)
{
  ptile->terrain = ter;
}

/****************************************************************************
  Return the specials of the tile.  See terrain.h.

  Note that this returns a mask of _all_ the specials on the tile.  To
  check a specific special use tile_has_special.
****************************************************************************/
enum tile_special_type tile_get_special(const struct tile *ptile)
{
  return ptile->special;
}

/****************************************************************************
  Returns TRUE iff the given tile has the given special.
****************************************************************************/
bool tile_has_special(const struct tile *ptile,
		      enum tile_special_type special)
{
  return contains_special(ptile->special, special);
}

/****************************************************************************
  Add the given special or specials to the tile.

  Note that this does not erase any existing specials already on the tile
  (see tile_clear_special or tile_clear_all_specials for that).  Also note
  the special to be set may be a mask, so you can set more than one
  special at a time (but this is not recommended).
****************************************************************************/
void tile_set_special(struct tile *ptile, enum tile_special_type spe)
{
  ptile->special |= spe;
}

/****************************************************************************
  Clear the given special or specials from the tile.

  This function clears all the specials set in the 'spe' mask from the
  tile's set of specials.  All other specials are unaffected.
****************************************************************************/
void tile_clear_special(struct tile *ptile, enum tile_special_type spe)
{
  ptile->special &= ~spe;
}

/****************************************************************************
  Remove any and all specials from this tile.
****************************************************************************/
void tile_clear_all_specials(struct tile *ptile)
{
  assert((int)S_NO_SPECIAL == 0);
  ptile->special = S_NO_SPECIAL;
}

/****************************************************************************
  Return the continent ID of the tile.  Typically land has a positive
  continent number and ocean has a negative number; no tile should have
  a 0 continent number.
****************************************************************************/
Continent_id tile_get_continent(const struct tile *ptile)
{
  return ptile->continent;
}

/****************************************************************************
  Set the continent ID of the tile.  See tile_get_continent.
****************************************************************************/
void tile_set_continent(struct tile *ptile, Continent_id val)
{
  ptile->continent = val;
}

/****************************************************************************
  Time to complete the given activity on the given tile.
****************************************************************************/
int tile_activity_time(enum unit_activity activity, const struct tile *ptile)
{
  switch (activity) {
  case ACTIVITY_POLLUTION:
    return tile_types[ptile->terrain].clean_pollution_time * ACTIVITY_FACTOR;
  case ACTIVITY_ROAD:
    return tile_types[ptile->terrain].road_time * ACTIVITY_FACTOR;
  case ACTIVITY_MINE:
    return tile_types[ptile->terrain].mining_time * ACTIVITY_FACTOR;
  case ACTIVITY_IRRIGATE:
    return tile_types[ptile->terrain].irrigation_time * ACTIVITY_FACTOR;
  case ACTIVITY_FORTRESS:
    return tile_types[ptile->terrain].fortress_time * ACTIVITY_FACTOR;
  case ACTIVITY_RAILROAD:
    return tile_types[ptile->terrain].rail_time * ACTIVITY_FACTOR;
  case ACTIVITY_TRANSFORM:
    return tile_types[ptile->terrain].transform_time * ACTIVITY_FACTOR;
  case ACTIVITY_AIRBASE:
    return tile_types[ptile->terrain].airbase_time * ACTIVITY_FACTOR;
  case ACTIVITY_FALLOUT:
    return tile_types[ptile->terrain].clean_fallout_time * ACTIVITY_FACTOR;
  default:
    return 0;
  }
}

/****************************************************************************
  Clear all infrastructure (man-made specials) from the tile.
****************************************************************************/
static void tile_clear_infrastructure(struct tile *ptile)
{
  tile_clear_special(ptile, S_INFRASTRUCTURE_MASK);
}

/****************************************************************************
  Clear all "dirtiness" (pollution and fallout) from the tile.
****************************************************************************/
static void tile_clear_dirtiness(struct tile *ptile)
{
  tile_clear_special(ptile, S_POLLUTION);
  tile_clear_special(ptile, S_FALLOUT);
}

/****************************************************************************
  Change the terrain to the given type.  This does secondary tile updates to
  the tile (as will happen when mining/irrigation/transforming changes the
  tile's terrain).
****************************************************************************/
void tile_change_terrain(struct tile *ptile, Terrain_type_id type)
{
  tile_set_terrain(ptile, type);
  if (is_ocean(type)) {
    tile_clear_infrastructure(ptile);
    tile_clear_dirtiness(ptile);

    /* FIXME: When rest of code can handle
     * rivers in oceans, then maybe we can leave it in place. */
    tile_clear_special(ptile, S_RIVER);
  }

  /* Clear mining/irrigation if resulting terrain type cannot support
   * that feature. */
  
  if (tile_types[type].mining_result != type) {
    tile_clear_special(ptile, S_MINE);
  }

  if (tile_types[type].irrigation_result != type) {
    tile_clear_special(ptile, S_IRRIGATION);
    tile_clear_special(ptile, S_FARMLAND);
  }
}

/****************************************************************************
  Build irrigation on the tile.  This may change the specials of the tile
  or change the terrain type itself.
****************************************************************************/
void tile_irrigate(struct tile *ptile)
{
  Terrain_type_id now, result;
  
  now = ptile->terrain;
  result = tile_types[now].irrigation_result;

  if (now == result) {
    if (tile_has_special(ptile, S_IRRIGATION)) {
      tile_set_special(ptile, S_FARMLAND);
    } else {
      tile_set_special(ptile, S_IRRIGATION);
    }
    tile_clear_special(ptile, S_MINE);
  } else if (result != T_NONE) {
    tile_change_terrain(ptile, result);
  }
}

/****************************************************************************
  Build a mine on the tile.  This may change the specials of the tile
  or change the terrain type itself.
****************************************************************************/
void tile_mine(struct tile *ptile)
{
  Terrain_type_id now, result;
  
  now = ptile->terrain;
  result = tile_types[now].mining_result;
  
  if (now == result) {
    tile_set_special(ptile, S_MINE);
    tile_clear_special(ptile, S_FARMLAND);
    tile_clear_special(ptile, S_IRRIGATION);
  } else if (result != T_NONE) {
    tile_change_terrain(ptile, result);
  }
}

/****************************************************************************
  Transform (ACTIVITY_TRANSFORM) the tile.  This usually changes the tile's
  terrain type.
****************************************************************************/
void tile_transform(struct tile *ptile)
{
  Terrain_type_id now, result;
  
  now = ptile->terrain;
  result = tile_types[now].transform_result;
  
  if (result != T_NONE) {
    tile_change_terrain(ptile, result);
  }
}
