/********************************************************************** 
 Freeciv - Copyright (C) 2004 - Marcelo J. Burda
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

/* utilities */
#include "fcintl.h"
#include "log.h"
#include "rand.h"
#include "shared.h"		/* bool type */

/* common */
#include "map.h"

#include "utilities.h"

/****************************************************************************
 Map that contains, according to circumstances, information on whether
 we have already placed terrain (special, hut) here.
****************************************************************************/
static bool *placed_map;

/**************************************************************************
 return TRUE if initialized
*************************************************************************/ 
bool placed_map_is_initialized(void)
{
  return placed_map != NULL;
}

/****************************************************************************
  Create a clean pmap
****************************************************************************/
void create_placed_map(void)                               
{                                                          
  assert(!placed_map_is_initialized());                              
  placed_map = fc_malloc (sizeof(bool) * MAP_INDEX_SIZE);   
  INITIALIZE_ARRAY(placed_map, MAP_INDEX_SIZE, FALSE );     
}

/**************************************************************************** 
  Free the pmap
****************************************************************************/
void destroy_placed_map(void)   
{                              
  assert(placed_map_is_initialized()); 
  free(placed_map);            
  placed_map = NULL;           
}



#define pmap(_tile) (placed_map[tile_index(_tile)])

/**************************************************************************
  Checks if land has not yet been placed on pmap at (x, y)
**************************************************************************/
bool not_placed(const struct tile *ptile)
{
  return !pmap(ptile);
}

/**************************************************************************
  Mark tile terrain as placed.
**************************************************************************/
void map_set_placed(struct tile *ptile)
{
  pmap(ptile) = TRUE;
}

/**************************************************************************
  Mark tile terrain as not placed.
**************************************************************************/
void map_unset_placed(struct tile *ptile)
{
  pmap(ptile) = FALSE;
}

/**************************************************************************** 
  set all oceanics tiles in placed_map
****************************************************************************/
void set_all_ocean_tiles_placed(void) 
{
  whole_map_iterate(ptile) {
    if (is_ocean_tile(ptile)) {
      map_set_placed(ptile);
    }
  } whole_map_iterate_end;
}

/****************************************************************************
  Set all nearby tiles as placed on pmap. 
****************************************************************************/
void set_placed_near_pos(struct tile *ptile, int dist)
{
  square_iterate(ptile, dist, tile1) {
    map_set_placed(tile1);
  } square_iterate_end;
}

/**************************************************************************
  Change the values of the integer map, so that they contain ranking of each 
  tile scaled to [0 .. int_map_max].
  The lowest 20% of tiles will have values lower than 0.2 * int_map_max.

  If filter is non-null then it only tiles for which filter(ptile, data) is
  TRUE will be considered.
**************************************************************************/
void adjust_int_map_filtered(int *int_map, int int_map_max, void *data,
			     bool (*filter)(const struct tile *ptile,
					    const void *data))
{
  int minval = 0, maxval = 0, total = 0;
  bool first = TRUE;

  /* Determine minimum and maximum value. */
  whole_map_iterate_filtered(ptile, data, filter) {
    if (first) {
      minval = int_map[tile_index(ptile)];
      maxval = int_map[tile_index(ptile)];
    } else {
      maxval = MAX(maxval, int_map[tile_index(ptile)]);
      minval = MIN(minval, int_map[tile_index(ptile)]);
    }
    first = FALSE;
    total++;
  } whole_map_iterate_filtered_end;

  if (total == 0) {
    return;
  }

  {
    int const size = 1 + maxval - minval;
    int i, count = 0, frequencies[size];

    INITIALIZE_ARRAY(frequencies, size, 0);

    /* Translate value so the minimum value is 0
       and count the number of occurencies of all values to initialize the 
       frequencies[] */
    whole_map_iterate_filtered(ptile, data, filter) {
      int_map[tile_index(ptile)] -= minval;
      frequencies[int_map[tile_index(ptile)]]++;
    } whole_map_iterate_filtered_end;

    /* create the linearize function as "incremental" frequencies */
    for(i =  0; i < size; i++) {
      count += frequencies[i]; 
      frequencies[i] = (count * int_map_max) / total;
    }

    /* apply the linearize function */
    whole_map_iterate_filtered(ptile, data, filter) {
      int_map[tile_index(ptile)] = frequencies[int_map[tile_index(ptile)]];
    } whole_map_iterate_filtered_end;
  }
}

bool is_normal_nat_pos(int x, int y)
{
  NATIVE_TO_MAP_POS(&x, &y, x, y);
  return is_normal_map_pos(x, y);
}

/****************************************************************************
 * Apply a Gaussian difusion filtre on the map
 * the size of the map is MAP_INDEX_SIZE and the map is indexed by 
 * native_pos_to_index function
 * if zeroes_at_edges is set, any unreal position on difusion has 0 value
 * if zeroes_at_edges in unset the unreal position are not counted.
 ****************************************************************************/
 void smooth_int_map(int *int_map, bool zeroes_at_edges)
{
  float weight[5] =  {0.35,  0.5 ,1 , 0.5, 0.35};
  float total_weight = 2.70;
  bool axe = TRUE;
  int alt_int_map[MAP_INDEX_SIZE];
  int *target_map, *source_map;

  assert(int_map != NULL);

  target_map = alt_int_map;
  source_map = int_map;

  do {
    whole_map_iterate(ptile) {
      int  N = 0, D = 0;

      axis_iterate(ptile, pnear, i, 2, axe) {
	D += weight[i + 2];
	N += weight[i + 2] * source_map[tile_index(pnear)];
      } axis_iterate_end;
      if(zeroes_at_edges) {
	D = total_weight;
      }
      target_map[tile_index(ptile)] = N / D;
    } whole_map_iterate_end;

    if (MAP_IS_ISOMETRIC) {
      weight[0] = weight[4] = 0.5;
      weight[1] = weight[3] = 0.7;
      total_weight = 3.4;  
    }

    axe = !axe;

    source_map = alt_int_map;
    target_map = int_map;

  } while (!axe);
}

/* These arrays are indexed by continent number (or negative of the
 * ocean number) so the 0th element is unused and the array is 1 element
 * larger than you'd expect.
 *
 * The lake surrounders array tells how many land continents surround each
 * ocean (or -1 if the ocean touches more than one continent).
 *
 * The _sizes arrays give the sizes (in tiles) of each continent and
 * ocean.
 */
static Continent_id *lake_surrounders;
static int *continent_sizes, *ocean_sizes;


/**************************************************************************
  Calculate lake_surrounders[] array
**************************************************************************/
static void recalculate_lake_surrounders(void)
{
  const size_t size = (map.num_oceans + 1) * sizeof(*lake_surrounders);

  lake_surrounders = fc_realloc(lake_surrounders, size);
  memset(lake_surrounders, 0, size);
  
  whole_map_iterate(ptile) {
    const struct terrain *pterrain = tile_terrain(ptile);
    Continent_id cont = tile_continent(ptile);

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      adjc_iterate(ptile, tile2) {
        Continent_id cont2 = tile_continent(tile2);
	if (is_ocean_tile(tile2)) {
	  if (lake_surrounders[-cont2] == 0) {
	    lake_surrounders[-cont2] = cont;
	  } else if (lake_surrounders[-cont2] != cont) {
	    lake_surrounders[-cont2] = -1;
	  }
	}
      } adjc_iterate_end;
    }
  } whole_map_iterate_end;
}

/**************************************************************************
  Number this tile and nearby tiles (recursively) with the specified
  continent number nr, using a flood-fill algorithm.

  is_land tells us whether we are assigning continent numbers or ocean 
  numbers.
**************************************************************************/
static void assign_continent_flood(struct tile *ptile, bool is_land, int nr)
{
  const struct terrain *pterrain = tile_terrain(ptile);

  if (tile_continent(ptile) != 0) {
    return;
  }

  if (T_UNKNOWN == pterrain) {
    return;
  }

  if (!XOR(is_land, terrain_has_flag(pterrain, TER_OCEANIC))) {
    return;
  }

  tile_set_continent(ptile, nr);
  
  /* count the tile */
  if (nr < 0) {
    ocean_sizes[-nr]++;
  } else {
    continent_sizes[nr]++;
  }

  adjc_iterate(ptile, tile1) {
    assign_continent_flood(tile1, is_land, nr);
  } adjc_iterate_end;
}

/**************************************************************************
  Regenerate all oceanic tiles with coasts, lakes, and deeper oceans.
  Assumes assign_continent_numbers() and recalculate_lake_surrounders()
  have already been done!
  FIXME: insufficiently generalized, use terrain property.
  FIXME: Results differ from initially generated waters, but this is not
         used at all in normal map generation.
**************************************************************************/
void regenerate_water(tile_knowledge_cb knowledge_cb)
{
#define MAX_ALT_TER_TYPES 5
#define DEFAULT_LAKE_SEA_SIZE (4)  /* should be configurable */
#define DEFAULT_NEAR_COAST (6)
  struct terrain *lakes[MAX_ALT_TER_TYPES];
  struct terrain *sea = find_terrain_by_identifier(TERRAIN_SEA_IDENTIFIER);
  struct terrain *coast = find_terrain_by_identifier(TERRAIN_OCEAN_IDENTIFIER);
  struct terrain *shelf = find_terrain_by_identifier(TERRAIN_SHELF_IDENTIFIER);
  struct terrain *floor = find_terrain_by_identifier(TERRAIN_FLOOR_IDENTIFIER);
  int coast_depth = coast->property[MG_OCEAN_DEPTH];
  int coast_count = 0;
  int shelf_count = 0;
  int floor_count = 0;
  int num_laketypes;

  num_laketypes = terrains_by_flag(TER_FRESHWATER, lakes, sizeof(lakes));
  if (num_laketypes > MAX_ALT_TER_TYPES) {
    freelog(LOG_NORMAL, "Number of lake types in ruleset %d, considering only %d ones.",
            num_laketypes, MAX_ALT_TER_TYPES);
    num_laketypes = MAX_ALT_TER_TYPES;
  }

#undef MAX_ALT_TER_TYPES

  /* coasts, lakes, and seas */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    Continent_id here = tile_continent(ptile);

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    if (0 < lake_surrounders[-here]) {
      if (DEFAULT_LAKE_SEA_SIZE < ocean_sizes[-here]
          && num_laketypes > 0) {
        tile_change_terrain(ptile, lakes[myrand(num_laketypes)]);
      } else {
        tile_change_terrain(ptile, sea);
      }
      if (knowledge_cb) {
        knowledge_cb(ptile);
      }
      continue;
    }
    /* leave any existing deep features in place */
    if (pterrain->property[MG_OCEAN_DEPTH] > coast_depth) {
      continue;
    }

    /* default to shelf */
    tile_change_terrain(ptile, shelf);
    if (knowledge_cb) {
      knowledge_cb(ptile);
    }
    shelf_count++;

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2) {
        continue;
      }
      /* glacier not otherwise near land floats */
      if (TERRAIN_GLACIER_IDENTIFIER == terrain_identifier(pterrain2)) {
        continue;
      }
      /* any land makes coast */
      if (!terrain_has_flag(pterrain2, TER_OCEANIC)) {
        tile_change_terrain(ptile, coast);
        if (knowledge_cb) {
          knowledge_cb(ptile);
        }
        coast_count++;
        shelf_count--;
        break;
      }
    } adjc_iterate_end;
  } whole_map_iterate_end;

  /* continental shelf */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    int shallow = 0;

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    /* leave any other existing features in place */
    if (pterrain != shelf) {
      continue;
    }

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2)
        continue;

      switch (terrain_identifier(pterrain2)) {
      case TERRAIN_OCEAN_IDENTIFIER:
        shallow++;
        break;
      default:
        break;
      };
    } adjc_iterate_end;

    if (DEFAULT_NEAR_COAST < shallow) {
      /* smooth with neighbors */
      tile_change_terrain(ptile, coast);
      if (knowledge_cb) {
        knowledge_cb(ptile);
      }
      coast_count++;
      shelf_count--;
    } else if (0 == shallow) {
      tile_change_terrain(ptile, floor);
      if (knowledge_cb) {
        knowledge_cb(ptile);
      }
      floor_count++;
      shelf_count--;
    }
  } whole_map_iterate_end;

  /* deep ocean floor */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    int shallow = 0;

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    /* leave any other existing features in place */
    if (pterrain != floor) {
      continue;
    }

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2)
        continue;

      switch (terrain_identifier(pterrain2)) {
      case TERRAIN_GLACIER_IDENTIFIER:
      case TERRAIN_OCEAN_IDENTIFIER:
      case TERRAIN_SHELF_IDENTIFIER:
        shallow++;
        break;
      default:
        break;
      };
    } adjc_iterate_end;

    if (DEFAULT_NEAR_COAST < shallow) {
      /* smooth with neighbors */
      tile_change_terrain(ptile, shelf);
      if (knowledge_cb) {
        knowledge_cb(ptile);
      }
      floor_count--;
      shelf_count++;
    }
  } whole_map_iterate_end;

  freelog(LOG_VERBOSE, "Map has %d coast, %d shelf, and %d floor tiles", 
          coast_count,
          shelf_count,
          floor_count);
}

/**************************************************************************
  Get continent surrounding lake, or -1 if there is multiple continents.
**************************************************************************/
int get_lake_surrounders(Continent_id cont)
{
  return lake_surrounders[-cont];
}

/*************************************************************************
  Return size in tiles of the given continent(not ocean)
*************************************************************************/
int get_continent_size(Continent_id id)
{
  assert(id > 0);
  return continent_sizes[id];
}

/*************************************************************************
  Return size in tiles of the given ocean. You should use positive ocean
  number.
*************************************************************************/
int get_ocean_size(Continent_id id) 
{
  assert(id > 0);
  return ocean_sizes[id];
}

/**************************************************************************
  Assigns continent and ocean numbers to all tiles, and set
  map.num_continents and map.num_oceans.  Recalculates continent and
  ocean sizes, and lake_surrounders[] arrays.

  Continents have numbers 1 to map.num_continents _inclusive_.
  Oceans have (negative) numbers -1 to -map.num_oceans _inclusive_.
**************************************************************************/
void assign_continent_numbers(void)
{
  /* Initialize */
  map.num_continents = 0;
  map.num_oceans = 0;

  whole_map_iterate(ptile) {
    tile_set_continent(ptile, 0);
  } whole_map_iterate_end;

  /* Assign new numbers */
  whole_map_iterate(ptile) {
    const struct terrain *pterrain = tile_terrain(ptile);

    if (tile_continent(ptile) != 0) {
      /* Already assigned. */
      continue;
    }

    if (T_UNKNOWN == pterrain) {
      continue; /* Can't assign this. */
    }

    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      map.num_continents++;
      continent_sizes = fc_realloc(continent_sizes,
		       (map.num_continents + 1) * sizeof(*continent_sizes));
      continent_sizes[map.num_continents] = 0;
      assign_continent_flood(ptile, TRUE, map.num_continents);
    } else {
      map.num_oceans++;
      ocean_sizes = fc_realloc(ocean_sizes,
		       (map.num_oceans + 1) * sizeof(*ocean_sizes));
      ocean_sizes[map.num_oceans] = 0;
      assign_continent_flood(ptile, FALSE, -map.num_oceans);
    }
  } whole_map_iterate_end;

  recalculate_lake_surrounders();

  freelog(LOG_VERBOSE, "Map has %d continents and %d oceans", 
	  map.num_continents, map.num_oceans);
}
