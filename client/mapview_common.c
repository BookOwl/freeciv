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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include "log.h"
#include "map.h"
#include "rand.h"
#include "support.h"
#include "timing.h"

#include "graphics_g.h"
#include "mapctrl_g.h"
#include "mapview_g.h"

#include "civclient.h"
#include "climap.h"
#include "control.h"
#include "goto.h"
#include "mapview_common.h"
#include "tilespec.h"

struct mapview_canvas mapview_canvas;
struct overview overview;

/* Arbitrary estimated maximums for width and height of a city description
 * text.  Eventually this may be determined dynamically. */
#define MAX_CITY_DESC_WIDTH 128
#define MAX_CITY_DESC_HEIGHT 32

/*
 * Set to TRUE if the backing store is more recent than the version
 * drawn into overview.window.
 */
static bool overview_dirty = FALSE;

static void base_canvas_to_map_pos(int *map_x, int *map_y,
				   int canvas_x, int canvas_y);
static void center_tile_overviewcanvas(int map_x, int map_y);
static void get_mapview_corners(int x[4], int y[4]);
static void redraw_overview(void);
static void dirty_overview(void);
static void flush_dirty_overview(void);

/**************************************************************************
 Refreshes a single tile on the map canvas.
**************************************************************************/
void refresh_tile_mapcanvas(int x, int y, bool write_to_screen)
{
  int canvas_x, canvas_y;

  assert(is_real_map_pos(x, y));
  if (!normalize_map_pos(&x, &y)) {
    return;
  }

  if (map_to_canvas_pos(&canvas_x, &canvas_y, x, y)) {
    canvas_y += NORMAL_TILE_HEIGHT - UNIT_TILE_HEIGHT;
    update_map_canvas(canvas_x, canvas_y, UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);

    if (write_to_screen) {
      flush_dirty();
    }
  }
  overview_update_tile(x, y);
}

/**************************************************************************
Returns the color the grid should have between tile (x1,y1) and
(x2,y2).
**************************************************************************/
enum color_std get_grid_color(int x1, int y1, int x2, int y2)
{
  enum city_tile_type city_tile_type1, city_tile_type2;
  struct city *dummy_pcity;
  bool pos1_is_in_city_radius =
      player_in_city_radius(game.player_ptr, x1, y1);
  bool pos2_is_in_city_radius = FALSE;

  assert(is_real_map_pos(x1, y1));

  if (is_real_map_pos(x2, y2)) {
    normalize_map_pos(&x2, &y2);
    assert(is_tiles_adjacent(x1, y1, x2, y2));

    if (map_get_tile(x2, y2)->known == TILE_UNKNOWN) {
      return COLOR_STD_BLACK;
    }

    pos2_is_in_city_radius =
	player_in_city_radius(game.player_ptr, x2, y2);
    get_worker_on_map_position(x2, y2, &city_tile_type2, &dummy_pcity);
  } else {
    city_tile_type2 = C_TILE_UNAVAILABLE;
  }

  if (!pos1_is_in_city_radius && !pos2_is_in_city_radius) {
    return COLOR_STD_BLACK;
  }

  get_worker_on_map_position(x1, y1, &city_tile_type1, &dummy_pcity);

  if (city_tile_type1 == C_TILE_WORKER || city_tile_type2 == C_TILE_WORKER) {
    return COLOR_STD_RED;
  } else {
    return COLOR_STD_WHITE;
  }
}

/****************************************************************************
  Translate from map to gui coordinate systems.

  GUI coordinates are comparable to canvas coordinates but extend in all
  directions.  gui(0,0) == map(0,0).
****************************************************************************/
static void map_to_gui_pos(int *gui_x, int *gui_y, int map_x, int map_y)
{
  if (is_isometric) {
    /*
     * Convert the map coordinates to isometric GUI
     * coordinates.  We'll make tile map(0,0) be the origin, and
     * transform like this:
     * 
     *                     3
     * 123                2 6
     * 456 -> becomes -> 1 5 9
     * 789                4 8
     *                     7
     */
    *gui_x = (map_x - map_y) * NORMAL_TILE_WIDTH / 2;
    *gui_y = (map_x + map_y) * NORMAL_TILE_HEIGHT / 2;
  } else {
    *gui_x = map_x * NORMAL_TILE_HEIGHT;
    *gui_y = map_y * NORMAL_TILE_WIDTH;
  }
}

/****************************************************************************
  Translate from gui to map coordinate systems.  See map_to_gui_pos().

  Note that you lose some information in this conversion.  If you convert
  from a gui position to a map position and back, you will probably not get
  the same value you started with.
****************************************************************************/
static void gui_to_map_pos(int *map_x, int *map_y, int gui_x, int gui_y)
{
  const int W = NORMAL_TILE_WIDTH, H = NORMAL_TILE_HEIGHT;

  if (is_isometric) {
    /* The basic operation here is a simple pi/4 rotation; however, we
     * have to first scale because the tiles have different width and
     * height.  Mathematically, this looks like
     *   | 1/W  1/H | |x|    |x`|
     *   |          | | | -> |  |
     *   |-1/W  1/H | |y|    |y`|
     *
     * Where W is the tile width and H the height.
     *
     * In simple terms, this is
     *   map_x = [   x / W + y / H ]
     *   map_y = [ - x / W + y / H ]
     * where [q] stands for integer part of q.
     *
     * Here the division is proper mathematical floating point division.
     *
     * A picture demonstrating this can be seen at
     * http://rt.freeciv.org/Ticket/Attachment/16782/9982/grid1.png.
     *
     * We have to subtract off a half-tile in the X direction before doing
     * the transformation.  This is because, although the origin of the tile
     * is the top-left corner of the bounding box, after the transformation
     * the top corner of the diamond-shaped tile moves into this position.
     *
     * The calculation is complicated somewhat because of two things: we
     * only use integer math, and C integer division rounds toward zero
     * instead of rounding down.
     *
     * For another example of this math, see canvas_to_city_pos().
     */
    gui_x -= W / 2;
    *map_x = DIVIDE(gui_x * H + gui_y * W, W * H);
    *map_y = DIVIDE(gui_y * W - gui_x * H, W * H);
  } else {			/* is_isometric */
    /* We use DIVIDE so that we will get the correct result even
     * for negative coordinates. */
    *map_x = DIVIDE(gui_x, W);
    *map_y = DIVIDE(gui_y, H);
  }
}

/**************************************************************************
  Finds the canvas coordinates for a map position. Beside setting the results
  in canvas_x, canvas_y it returns whether the tile is inside the
  visible mapview canvas.

  The result represents the upper left pixel (origin) of the bounding box of
  the tile.  Note that in iso-view this origin is not a part of the tile
  itself - so to make the operation reversible you would have to call
  canvas_to_map_pos on the center of the tile, not the origin.

  The center of a tile is defined as:
  {
    map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y);
    canvas_x += NORMAL_TILE_WIDTH / 2;
    canvas_y += NORMAL_TILE_HEIGHT / 2;
  }

  This pixel is one position closer to the lower right, which may be
  important to remember when doing some round-off operations. Other
  parts of the code assume NORMAL_TILE_WIDTH and NORMAL_TILE_HEIGHT
  to be even numbers.
**************************************************************************/
bool map_to_canvas_pos(int *canvas_x, int *canvas_y, int map_x, int map_y)
{
  int center_map_x, center_map_y, dx, dy;

  /*
   * First we wrap the coordinates to hopefully be within the the mapview
   * window.  We do this by finding the position closest to the center
   * of the window.
   */
  /* TODO: Cache the value of this position */
  base_canvas_to_map_pos(&center_map_x, &center_map_y,
			 mapview_canvas.width / 2,
			 mapview_canvas.height / 2);
  map_distance_vector(&dx, &dy, center_map_x, center_map_y, map_x, map_y);
  map_x = center_map_x + dx;
  map_y = center_map_y + dy;

  map_to_gui_pos(canvas_x, canvas_y, map_x, map_y);
  *canvas_x -= mapview_canvas.gui_x0;
  *canvas_y -= mapview_canvas.gui_y0;

  /*
   * Finally we clip; checking to see if _any part_ of the tile is
   * present on the backing store.  (Even if it's not visible on the canvas,
   * if it's present on the backing store we need to draw it in case the
   * canvas is resized.)
   */
  return (*canvas_x > -NORMAL_TILE_WIDTH
	  && *canvas_x < mapview_canvas.store_width
	  && *canvas_y > -NORMAL_TILE_HEIGHT
	  && *canvas_y < mapview_canvas.store_height);
}

/****************************************************************************
  Finds the map coordinates corresponding to pixel coordinates.  The
  resulting position is unwrapped and may be unreal.
****************************************************************************/
static void base_canvas_to_map_pos(int *map_x, int *map_y,
                                  int canvas_x, int canvas_y)
{
  gui_to_map_pos(map_x, map_y,
		 canvas_x + mapview_canvas.gui_x0,
		 canvas_y + mapview_canvas.gui_y0);
}


/**************************************************************************
  Finds the map coordinates corresponding to pixel coordinates.  Returns
  TRUE if the position is real; in this case it will be normalized. Returns
  FALSE if the tile is unreal - caller may use nearest_real_pos() if
  required.
**************************************************************************/
bool canvas_to_map_pos(int *map_x, int *map_y, int canvas_x, int canvas_y)
{
  base_canvas_to_map_pos(map_x, map_y, canvas_x, canvas_y);
  return normalize_map_pos(map_x, map_y);
}

/****************************************************************************
  Normalize (wrap) the GUI position.  This is equivalent to a map wrapping,
  but in GUI coordinates so that pixel accuracy is preserved.
****************************************************************************/
static void normalize_gui_pos(int *gui_x, int *gui_y)
{
  int map_x, map_y, nat_x, nat_y, gui_x0, gui_y0, diff_x, diff_y;

  /* Convert the (gui_x, gui_y) into a (map_x, map_y) plus a GUI offset
   * from this tile. */
  gui_to_map_pos(&map_x, &map_y, *gui_x, *gui_y);
  map_to_gui_pos(&gui_x0, &gui_y0, map_x, map_y);
  diff_x = *gui_x - gui_x0;
  diff_y = *gui_y - gui_y0;

  /* Perform wrapping without any realness check.  It's important that
   * we wrap even if the map position is unreal, which normalize_map_pos
   * doesn't necessarily do. */
  map_to_native_pos(&nat_x, &nat_y, map_x, map_y);
  if (topo_has_flag(TF_WRAPX)) {
    nat_x = FC_WRAP(nat_x, map.xsize);
  }
  if (topo_has_flag(TF_WRAPY)) {
    nat_y = FC_WRAP(nat_y, map.ysize);
  }
  native_to_map_pos(&map_x, &map_y, nat_x, nat_y);

  /* Now convert the wrapped map position back to a GUI position and add the
   * offset back on. */
  map_to_gui_pos(gui_x, gui_y, map_x, map_y);
  *gui_x += diff_x;
  *gui_y += diff_y;
}

/****************************************************************************
  Change the mapview origin, clip it, and update everything.
****************************************************************************/
void set_mapview_origin(int gui_x0, int gui_y0)
{
  int xmin, xmax, ymin, ymax, xsize, ysize;

  /* Normalize (wrap) the mapview origin. */
  normalize_gui_pos(&gui_x0, &gui_y0);

  /* First wrap/clip the position.  Wrapping is done in native positions
   * while clipping is done in scroll (native) positions. */
  get_mapview_scroll_window(&xmin, &ymin, &xmax, &ymax, &xsize, &ysize);

  if (!topo_has_flag(TF_WRAPX)) {
    gui_x0 = CLIP(xmin, gui_x0, xmax - xsize);
  }

  if (!topo_has_flag(TF_WRAPY)) {
    gui_y0 = CLIP(ymin, gui_y0, ymax - ysize);
  }

  /* Then update everything. */
  if (mapview_canvas.gui_x0 != gui_x0 || mapview_canvas.gui_y0 != gui_y0) {
    int map_center_x, map_center_y;
    int old_gui_x0 = mapview_canvas.gui_x0;
    int old_gui_y0 = mapview_canvas.gui_y0;
    const int width = mapview_canvas.width, height = mapview_canvas.height;
    int common_x0, common_x1, common_y0, common_y1;
    int update_x0, update_x1, update_y0, update_y1;

    mapview_canvas.gui_x0 = gui_x0;
    mapview_canvas.gui_y0 = gui_y0;

    /* Find the overlapping area of the new and old mapview.  This is
     * done in GUI coordinates.  Note that if the GUI coordinates wrap
     * no overlap will be found. */
    common_x0 = MAX(old_gui_x0, gui_x0);
    common_x1 = MIN(old_gui_x0, gui_x0) + width;
    common_y0 = MAX(old_gui_y0, gui_y0);
    common_y1 = MIN(old_gui_y0, gui_y0) + height;

    if (common_x1 > common_x0 && common_y1 > common_y0) {
      /* Do a partial redraw only.  This means the area of overlap (a
       * rectangle) is copied.  Then the remaining areas (two rectangles)
       * are updated through update_map_canvas. */
      struct canvas *target = mapview_canvas.tmp_store;

      if (old_gui_x0 < gui_x0) {
	update_x0 = MAX(old_gui_x0 + width, gui_x0);
	update_x1 = gui_x0 + width;
      } else {
	update_x0 = gui_x0;
	update_x1 = MIN(old_gui_x0, gui_x0 + width);
      }
      if (old_gui_y0 < gui_y0) {
	update_y0 = MAX(old_gui_y0 + height, gui_y0);
	update_y1 = gui_y0 + height;
      } else {
	update_y0 = gui_y0;
	update_y1 = MIN(old_gui_y0, gui_y0 + height);
      }

      dirty_all();
      canvas_copy(target, mapview_canvas.store,
		  common_x0 - old_gui_x0,
		  common_y0 - old_gui_y0,
		  common_x0 - gui_x0, common_y0 - gui_y0,
		  common_x1 - common_x0, common_y1 - common_y0);
      mapview_canvas.tmp_store = mapview_canvas.store;
      mapview_canvas.store = target;

      if (update_y1 > update_y0) {
	update_map_canvas(0, update_y0 - gui_y0,
			  width, update_y1 - update_y0);
      }
      if (update_x1 > update_x0) {
	update_map_canvas(update_x0 - gui_x0, common_y0 - gui_y0,
			  update_x1 - update_x0, common_y1 - common_y0);
      }
    } else {
      update_map_canvas_visible();
    }

    get_center_tile_mapcanvas(&map_center_x, &map_center_y);
    center_tile_overviewcanvas(map_center_x, map_center_y);
    update_map_canvas_scrollbars();
    if (hover_state == HOVER_GOTO || hover_state == HOVER_PATROL) {
      create_line_at_mouse_pos();
    }
  }
  if (rectangle_active) {
    update_rect_at_mouse_pos();
  }
}

/****************************************************************************
  Return the scroll dimensions of the clipping window for the mapview window..

  Imagine the entire map in scroll coordinates.  It is a rectangle.  Now
  imagine the mapview "window" sliding around through this rectangle.  How
  far can it slide?  In most cases it has to be able to slide past the
  ends of the map rectangle so that it's capable of reaching the whole
  area.

  This function gives constraints on how far the window is allowed to
  slide.  xmin and ymin are the minimum values for the window origin.
  xsize and ysize give the scroll dimensions of the mapview window.
  xmax and ymax give the maximum values that the bottom/left ends of the
  window may reach.  The constraints, therefore, are that:

    get_mapview_scroll_pos(&scroll_x, &scroll_y);
    xmin <= scroll_x < xmax - xsize
    ymin <= scroll_y < ymax - ysize

  This function should be used anywhere and everywhere that scrolling is
  constrained.

  Note that scroll coordinates, not map coordinates, are used.  Currently
  these correspond to native coordinates.
****************************************************************************/
void get_mapview_scroll_window(int *xmin, int *ymin, int *xmax, int *ymax,
			       int *xsize, int *ysize)
{
  *xsize = mapview_canvas.width;
  *ysize = mapview_canvas.height;

  if (topo_has_flag(TF_ISO) == is_isometric) {
    /* If the map and view line up, it's easy. */
    native_to_map_pos(xmin, ymin, 0, 0);
    map_to_gui_pos(xmin, ymin, *xmin, *ymin);

    native_to_map_pos(xmax, ymax, map.xsize - 1, map.ysize - 1);
    map_to_gui_pos(xmax, ymax, *xmax, *ymax);
    *xmax += NORMAL_TILE_WIDTH;
    *ymax += NORMAL_TILE_HEIGHT;

    /* To be able to center on positions near the edges, we have to be
     * allowed to scroll all the way to those edges.  To allow wrapping the
     * clipping boundary needs to extend past the edge - a half-tile in
     * iso-view or a full tile in non-iso view.  The above math already has
     * taken care of some of this so all that's left is to fix the corner
     * cases. */
    if (topo_has_flag(TF_WRAPX)) {
      *xmax += *xsize;

      /* We need to be able to scroll a little further to the left. */
      *xmin -= NORMAL_TILE_WIDTH;
    }
    if (topo_has_flag(TF_WRAPY)) {
      *ymax += *ysize;

      /* We need to be able to scroll a little further up. */
      *ymin -= NORMAL_TILE_HEIGHT;
    }
  } else {
    /* Otherwise it's hard.  Very hard.  Impossible, in fact.  This is just
     * an approximation - a huge bounding box. */
    int gui_x1, gui_y1, gui_x2, gui_y2, gui_x3, gui_y3, gui_x4, gui_y4;
    int map_x, map_y;

    native_to_map_pos(&map_x, &map_y, 0, 0);
    map_to_gui_pos(&gui_x1, &gui_y1, map_x, map_y);

    native_to_map_pos(&map_x, &map_y, map.xsize - 1, 0);
    map_to_gui_pos(&gui_x2, &gui_y2, map_x, map_y);

    native_to_map_pos(&map_x, &map_y, 0, map.ysize - 1);
    map_to_gui_pos(&gui_x3, &gui_y3, map_x, map_y);

    native_to_map_pos(&map_x, &map_y, map.xsize - 1, map.ysize - 1);
    map_to_gui_pos(&gui_x4, &gui_y4, map_x, map_y);

    *xmin = MIN(gui_x1, MIN(gui_x2, gui_x3)) - mapview_canvas.width / 2;
    *ymin = MIN(gui_y1, MIN(gui_y2, gui_y3)) - mapview_canvas.height / 2;

    *xmax = MAX(gui_x4, MAX(gui_x2, gui_x3)) + mapview_canvas.width / 2;
    *ymax = MAX(gui_y4, MAX(gui_y2, gui_y3)) + mapview_canvas.height / 2;
  }

  freelog(LOG_DEBUG, "x: %d<-%d->%d; y: %d<-%d->%d",
	  *xmin, *xsize, *xmax, *ymin, *ymax, *ysize);
}

/****************************************************************************
  Find the scroll step for the mapview.  This is the amount to scroll (in
  scroll coordinates) on each "step".  See also get_mapview_scroll_window.
****************************************************************************/
void get_mapview_scroll_step(int *xstep, int *ystep)
{
  *xstep = NORMAL_TILE_WIDTH;
  *ystep = NORMAL_TILE_HEIGHT;

  if (is_isometric) {
    *xstep /= 2;
    *ystep /= 2;
  }
}

/****************************************************************************
  Find the current scroll position (origin) of the mapview.
****************************************************************************/
void get_mapview_scroll_pos(int *scroll_x, int *scroll_y)
{
  *scroll_x = mapview_canvas.gui_x0;
  *scroll_y = mapview_canvas.gui_y0;
}

/****************************************************************************
  Set the scroll position (origin) of the mapview, and update the GUI.
****************************************************************************/
void set_mapview_scroll_pos(int scroll_x, int scroll_y)
{
  int gui_x0 = scroll_x, gui_y0 = scroll_y;

  set_mapview_origin(gui_x0, gui_y0);
}

/**************************************************************************
  Finds the current center tile of the mapcanvas.
**************************************************************************/
void get_center_tile_mapcanvas(int *map_x, int *map_y)
{
  /* This sets the pointers map_x and map_y */
  if (!canvas_to_map_pos(map_x, map_y,
          mapview_canvas.width / 2, mapview_canvas.height / 2)) {
    nearest_real_pos(map_x, map_y);
  }
}

/**************************************************************************
  Centers the mapview around (map_x, map_y).
**************************************************************************/
void center_tile_mapcanvas(int map_center_x, int map_center_y)
{
  int gui_x, gui_y;

  CHECK_MAP_POS(map_center_x, map_center_y);

  map_to_gui_pos(&gui_x, &gui_y, map_center_x, map_center_y);

  /* Put the center pixel of the tile at the exact center of the mapview. */
  gui_x -= (mapview_canvas.width - NORMAL_TILE_WIDTH) / 2;
  gui_y -= (mapview_canvas.height - NORMAL_TILE_HEIGHT) / 2;

  set_mapview_origin(gui_x, gui_y);
}

/**************************************************************************
  Return TRUE iff the given map position has a tile visible on the
  map canvas.
**************************************************************************/
bool tile_visible_mapcanvas(int map_x, int map_y)
{
  int dummy_x, dummy_y;		/* well, it needs two pointers... */

  return map_to_canvas_pos(&dummy_x, &dummy_y, map_x, map_y);
}

/**************************************************************************
  Return TRUE iff the given map position has a tile visible within the
  interior of the map canvas. This information is used to determine
  when we need to recenter the map canvas.

  The logic of this function is simple: if a tile is within 1.5 tiles
  of a border of the canvas and that border is not aligned with the
  edge of the map, then the tile is on the "border" of the map canvas.

  This function is only correct for the current topology.
**************************************************************************/
bool tile_visible_and_not_on_border_mapcanvas(int map_x, int map_y)
{
  int canvas_x, canvas_y;
  int xmin, ymin, xmax, ymax, xsize, ysize, scroll_x, scroll_y;
  const int border_x = (is_isometric ? NORMAL_TILE_WIDTH / 2
			: 2 * NORMAL_TILE_WIDTH);
  const int border_y = (is_isometric ? NORMAL_TILE_HEIGHT / 2
			: 2 * NORMAL_TILE_HEIGHT);
  bool same = (is_isometric == topo_has_flag(TF_ISO));

  get_mapview_scroll_window(&xmin, &ymin, &xmax, &ymax, &xsize, &ysize);
  get_mapview_scroll_pos(&scroll_x, &scroll_y);

  if (!map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y)) {
    /* The tile isn't visible at all. */
    return FALSE;
  }

  /* For each direction: if the tile is too close to the mapview border
   * in that direction, and scrolling can get us any closer to the
   * border, then it's a border tile.  We can only really check the
   * scrolling when the mapview window lines up with the map. */
  if (canvas_x < border_x
      && (!same || scroll_x > xmin || topo_has_flag(TF_WRAPX))) {
    return FALSE;
  }
  if (canvas_y < border_y
      && (!same || scroll_y > ymin || topo_has_flag(TF_WRAPY))) {
    return FALSE;
  }
  if (canvas_x + NORMAL_TILE_WIDTH > mapview_canvas.width - border_x
      && (!same || scroll_x + xsize >= xmax || topo_has_flag(TF_WRAPX))) {
    return FALSE;
  }
  if (canvas_y + NORMAL_TILE_HEIGHT > mapview_canvas.height - border_y
      && (!same || scroll_y + ysize >= ymax || topo_has_flag(TF_WRAPY))) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Draw the given unit onto the canvas store at the given location.

  unit_offset_x, unit_offset_y, unit_width, unit_height are used
  in iso-view to draw only part of the tile.  Non-iso view should use
  put_unit_full instead.
**************************************************************************/
void put_unit(struct unit *punit, bool stacked, bool backdrop,
	      struct canvas *pcanvas,
	      int canvas_x, int canvas_y,
	      int unit_offset_x, int unit_offset_y,
	      int unit_width, int unit_height)
{
  struct drawn_sprite drawn_sprites[40];
  bool solid_bg;
  int count = fill_unit_sprite_array(drawn_sprites, punit, &solid_bg,
				     stacked, backdrop);
  int i;

  if (!is_isometric && solid_bg) {
    canvas_put_rectangle(pcanvas, player_color(unit_owner(punit)),
			 canvas_x, canvas_y,
			 UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);
  }

  for (i = 0; i < count; i++) {
    if (drawn_sprites[i].sprite) {
      int ox = drawn_sprites[i].offset_x, oy = drawn_sprites[i].offset_y;

      /* units are never fogged */
      canvas_put_sprite(pcanvas, canvas_x + ox, canvas_y + oy,
			drawn_sprites[i].sprite,
			unit_offset_x - ox, unit_offset_y - oy,
			unit_width, unit_height);
    }
  }

  if (punit->occupy != 0) {
    canvas_put_sprite(pcanvas, canvas_x, canvas_y,
		      sprites.unit.stack,
		      unit_offset_x, unit_offset_y, unit_width, unit_height);
  }
}

/**************************************************************************
  Draw the given unit onto the canvas store at the given location.
**************************************************************************/
void put_unit_full(struct unit *punit, struct canvas *pcanvas,
		   int canvas_x, int canvas_y)
{
  put_unit(punit, FALSE, TRUE, pcanvas, canvas_x, canvas_y,
	   0, 0, UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);
}

/****************************************************************************
  Draw food, shield, and trade output values on the tile.

  The proper way to do this is probably something like what Civ II does
  (one sprite drawn N times on top of itself), but we just use separate
  sprites (limiting the number of combinations).
****************************************************************************/
void put_city_tile_output(struct city *pcity, int city_x, int city_y,
			  struct canvas *pcanvas,
			  int canvas_x, int canvas_y)
{
  int food = city_get_food_tile(city_x, city_y, pcity);
  int shields = city_get_shields_tile(city_x, city_y, pcity);
  int trade = city_get_trade_tile(city_x, city_y, pcity);

  food = CLIP(0, food, NUM_TILES_DIGITS - 1);
  shields = CLIP(0, shields, NUM_TILES_DIGITS - 1);
  trade = CLIP(0, trade, NUM_TILES_DIGITS - 1);

  /* In iso-view the output sprite is a bit smaller than the tile, so we
   * have to use an offset. */
  if (is_isometric) {
    canvas_x += NORMAL_TILE_WIDTH / 3;
    canvas_y -= NORMAL_TILE_HEIGHT / 3;
  }

  canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			 sprites.city.tile_foodnum[food]);
  canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			 sprites.city.tile_shieldnum[shields]);
  canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			 sprites.city.tile_tradenum[trade]);
}

/****************************************************************************
  Draw food, gold, and shield upkeep values on the unit.

  The proper way to do this is probably something like what Civ II does
  (one sprite drawn N times on top of itself), but we just use separate
  sprites (limiting the number of combinations).
****************************************************************************/
void put_unit_city_overlays(struct unit *punit,
			    struct canvas *pcanvas,
			    int canvas_x, int canvas_y)
{
  int upkeep_food = CLIP(0, punit->upkeep_food, 2);
  int upkeep_gold = CLIP(0, punit->upkeep_gold, 2);
  int unhappy = CLIP(0, punit->unhappiness, 2);

  /* draw overlay pixmaps */
  if (punit->upkeep > 0) {
    canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			   sprites.upkeep.shield);
  }
  if (upkeep_food > 0) {
    canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			   sprites.upkeep.food[upkeep_food - 1]);
  }
  if (upkeep_gold > 0) {
    canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			   sprites.upkeep.gold[upkeep_gold - 1]);
  }
  if (unhappy > 0) {
    canvas_put_sprite_full(pcanvas, canvas_x, canvas_y,
			   sprites.upkeep.unhappy[unhappy - 1]);
  }
}

/****************************************************************************
  Return the vertices of the given edge of the tile.  This will return
  FALSE if the edge doesn't exist (or if it does exist but is part of an
  adjacent tile).

  The result is intended to be used to draw lines via canvas_put_line.

  inset specifies how many pixels inward the boundary is to be inset.  This
  is generally 0 or 1.  (Note that this doesn't work particularly well in
  iso-view.)

  width specifies the width of the boundary - generally 1 or 2.  For
  instance if the boundary is 2 pixels wide then the right and bottom
  positions must be translated so it's drawn in the right location.
****************************************************************************/
static bool get_tile_boundaries(enum direction8 dir,
				int inset, int width, enum draw_type draw,
				int *start_x, int *start_y,
				int *end_x, int *end_y)
{
  const int W = NORMAL_TILE_WIDTH, H = NORMAL_TILE_HEIGHT;
  const int HW = hex_width, HH = hex_height;
  const int overlap = (width > 1) ? 1 : 0;

  assert(inset >= 0);
  assert(width >= 1);

  width--;

  /* Note that the boundary (with inset 0) may actually cover some adjacent
   * tiles, since the left boundary for this tile is equivalent to the right
   * boundary for the tile to our left.  The north and west boundaries will
   * be on this tile while the south and east boundaries must be drawn as
   * part of the adjacent tiles. */

  /* "Width" and the "overlap" value: if a width of 1 is specified, then
   * boundaries of adjacent tiles are assumed to overlap and no extra
   * adjustment need be done.  However if width > 1 then not only do we have
   * to account for the width in calculating the position, but we also
   * assume that the caller does *not* want the boundaries to overlap with
   * adjacent tiles (note that if they did then individual lines would
   * cover multiple tiles, which would be tricky).
   *
   * The answer is to use two variables: width and overlap.
   *
   * width is the offset that must be done to account for the width of the
   * line.  Since lines are drawn upward and to the left of their origin,
   * we must displace the line by (width - 1) pixels in this direction.  And
   * for national borders we must do this in a consistent way, so that
   * adjacent borders will line up.  Thus in iso-view this displacement is
   * done entirely in the Y dimension.
   *
   * overlap is a value that accountes for the non-overlapping of the
   * boundaries.  If boundaries don't overlap then we have to pull the
   * bottom and right boundaries off by 1 pixel.  Like the width adjustment
   * this must be done in a consistent way in iso-view.
   *
   * As long as width==2 this difference is negligible, since the width
   * adjustment and the overlap adjustment are both 1.
   */

  if (!(draw & D_B) && !(draw & D_M)) {
    /* Nothing drawable. */
    return FALSE;
  }

  if (is_isometric) {
    switch (dir) {
    case DIR8_NORTH:
      /* Top right. */
      *start_x = (W + HW) / 2;
      *end_x = W - HW / 2 - inset;
      *start_y = HH / 2 + inset + width;
      *end_y = (H - HH) / 2 + width;
      return (draw & D_M_R) == D_M_R;
    case DIR8_SOUTH:
      /* Bottom left. */
      *start_x = (W - HW) / 2;
      *end_x = HW / 2 + inset;
      *start_y = H - HH / 2 - inset - overlap;
      *end_y = (H + HH) / 2 - overlap;
      return (draw & D_B_L) == D_B_L && (inset + overlap) > 0;
    case DIR8_EAST:
      /* Bottom right. */
      *start_x = W - HW / 2 - inset;
      *end_x = (W + HW) / 2;
      *start_y = (H + HH) / 2 - overlap;
      *end_y = H - HH / 2 - inset - overlap;
      return (draw & D_B_R) == D_B_R && (inset + overlap) > 0;
    case DIR8_WEST:
      /* Top left. */
      *start_x = HW / 2 + inset;
      *end_x = (W - HW) / 2;
      *start_y = (H - HH) / 2 + width;
      *end_y = HH / 2 + inset + width;
      return (draw & D_M_L) == D_M_L;
    case DIR8_NORTHEAST:
      *start_x = *end_x = W - HW / 2 - inset - overlap;
      *start_y = (draw & D_M_R) == D_M_R ? ((H - HH) / 2) : H / 2;
      *end_y = (draw & D_B_R) == D_B_R ? ((H + HH) / 2) : H / 2;
      return HH > 0 && (draw & D_R);
    case DIR8_SOUTHEAST:
      *start_x = (draw & D_B_R) == D_B_R ? ((W + HW) / 2) : W / 2;
      *end_x = (draw & D_B_L) == D_B_L ? ((W - HW) / 2) : W / 2;
      *start_y = *end_y = H - HH / 2 - inset - overlap;
      return HW > 0 && (draw & D_B);
    case DIR8_SOUTHWEST:
      *start_x = *end_x = HW / 2 + inset + width;
      *start_y = (draw & D_B_L) == D_B_L ? ((H + HH) / 2) : H / 2;
      *end_y = (draw & D_M_L) == D_M_L ? ((H - HH) / 2) : H / 2;
      return HH > 0 && (draw & D_L);
    case DIR8_NORTHWEST:
      *start_x = (draw & D_M_L) == D_M_L ? ((W - HW) / 2) : W / 2;
      *end_x = (draw & D_M_R) == D_M_R ? ((W + HW) / 2) : W / 2;
      *start_y = *end_y = HH / 2 + inset + width;
      return HW > 0 && (draw & D_M);
    }
  } else {
    switch (dir) {
    case DIR8_NORTH:
      *start_x = inset;
      *end_x = W - inset;
      *start_y = *end_y = inset + width;
      return TRUE;
    case DIR8_SOUTH:
      *start_x = inset;
      *end_x = W - inset;
      *start_y = *end_y = H - inset - overlap;
      return inset + overlap > 0;
    case DIR8_EAST:
      *start_x = *end_x = W - inset - overlap;
      *start_y = inset;
      *end_y = H - inset;
      return inset + overlap > 0;
    case DIR8_WEST:
      *start_x = *end_x = inset + width;
      *start_y = inset;
      *end_y = H - inset;
      return TRUE;
    case DIR8_NORTHEAST:
    case DIR8_SOUTHEAST:
    case DIR8_SOUTHWEST:
    case DIR8_NORTHWEST:
      return FALSE;
    }
  }

  assert(0);
  return FALSE;
}


/****************************************************************************
  Draw a red frame around the tile.  (canvas_x, canvas_y) is the tile origin.
****************************************************************************/
void put_red_frame_tile(struct canvas *pcanvas,
			int canvas_x, int canvas_y)
{
  enum direction8 dir;

  for (dir = 0; dir < 8; dir++) {
    int start_x, start_y, end_x, end_y;

    /* We just draw an extra red line with an inset of 1 from the tile
     * boundary.  If the map grid is also drawn, it will be red as well
     * giving a width-2 frame (it may not line up perfectly in iso-view).
     * If not then the inset allows the user to distinguish which tile
     * is unavailable.
     *
     * Since the frames are drawn only for the citydlg and are put on
     * top of everything else, we don't have to worry about overlapping
     * tiles covering them up even in iso-view.  (See comments in
     * city_dialog_redraw_map and tile_draw_borders.) */
    if (get_tile_boundaries(dir, 1, 1, D_FULL,
			    &start_x, &start_y, &end_x, &end_y)) {
      canvas_put_line(pcanvas, COLOR_STD_RED, LINE_NORMAL,
		      canvas_x + start_x, canvas_y + start_y,
		      end_x - start_x, end_y - start_y);

    }
  }
}

/****************************************************************************
  Animate the nuke explosion at map(x, y).
****************************************************************************/
void put_nuke_mushroom_pixmaps(int map_x, int map_y)
{
  int canvas_x, canvas_y;
  struct Sprite *mysprite = sprites.explode.nuke;
  int width, height;

  /* We can't count on the return value of map_to_canvas_pos since the
   * sprite may span multiple tiles. */
  (void) map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y);
  get_sprite_dimensions(mysprite, &width, &height);

  canvas_x += (NORMAL_TILE_WIDTH - width) / 2;
  canvas_y += (NORMAL_TILE_HEIGHT - height) / 2;

  canvas_put_sprite_full(mapview_canvas.store, canvas_x, canvas_y, mysprite);
  dirty_rect(canvas_x, canvas_y, width, height);

  /* Make sure everything is flushed and synced before proceeding. */
  flush_dirty();
  gui_flush();

  myusleep(1000000);

  update_map_canvas_visible();
}

/**************************************************************************
   Draw the borders of the given map tile at the given canvas position.
**************************************************************************/
static void tile_draw_borders(struct canvas *pcanvas,
			      int map_x, int map_y,
			      int canvas_x, int canvas_y,
			      enum draw_type draw)
{
  struct player *this_owner = map_get_owner(map_x, map_y), *adjc_owner;
  int start_x, start_y, end_x, end_y;

  if (!draw_borders || game.borders == 0) {
    return;
  }

  if (is_isometric) {
    /* Isometric must be done differently or the borders will get overwritten
     * by other terrain graphics.  (This is because the tileset sprites'
     * edges don't line up exactly with the mathematical calculation of the
     * edges of the tiles.)  Of course this means the borders may
     * themselves overwrite units and cities.  The only real solution is
     * to do the drawing in layers rather than per-tile.  In the meantime
     * we use this hack. */
    adjc_dir_iterate(map_x, map_y, adjc_x, adjc_y, dir) {
      if ((dir == DIR8_WEST || dir == DIR8_NORTHWEST
	   || dir == DIR8_NORTH || dir == DIR8_SOUTHWEST)
	  && get_tile_boundaries(dir, 0, BORDER_WIDTH, draw,
				  &start_x, &start_y, &end_x, &end_y)
	  && tile_get_known(adjc_x, adjc_y) != TILE_UNKNOWN
	  && this_owner != (adjc_owner = map_get_owner(adjc_x, adjc_y))) {
	if (this_owner) {
	  canvas_put_line(pcanvas, player_color(this_owner), LINE_BORDER,
			  canvas_x + start_x, canvas_y + start_y,
			  end_x - start_x, end_y - start_y);
	}
	if (adjc_owner) {
	  canvas_put_line(pcanvas, player_color(adjc_owner), LINE_BORDER,
			  canvas_x + start_x,
			  canvas_y + start_y - BORDER_WIDTH,
			  end_x - start_x, end_y - start_y);
	}
      }
    } adjc_dir_iterate_end;
  } else {
    if (!this_owner) {
      return;
    }
    adjc_dir_iterate(map_x, map_y, adjc_x, adjc_y, dir) {
      if (get_tile_boundaries(dir, 0, BORDER_WIDTH, draw,
			      &start_x, &start_y, &end_x, &end_y)
	  && tile_get_known(adjc_x, adjc_y) != TILE_UNKNOWN
	  && this_owner != (adjc_owner = map_get_owner(adjc_x, adjc_y))) {
	canvas_put_line(pcanvas, player_color(this_owner), LINE_BORDER,
			canvas_x + start_x, canvas_y + start_y,
			end_x - start_x, end_y - start_y);
      }
    } adjc_dir_iterate_end;
  }
}

/**************************************************************************
  Draw the given map tile at the given canvas position in non-isometric
  view.
**************************************************************************/
void put_one_tile(struct canvas *pcanvas, int map_x, int map_y,
		  int canvas_x, int canvas_y, bool citymode)
{
  struct drawn_sprite tile_sprs[80];
  bool solid_bg;
  enum color_std bg_color;
  bool is_real = normalize_map_pos(&map_x, &map_y);

  if (is_real && tile_get_known(map_x, map_y) != TILE_UNKNOWN) {
    int count = fill_tile_sprite_array(tile_sprs, &solid_bg, &bg_color,
				       map_x, map_y, citymode);
    int i = 0;

    if (solid_bg) {
      canvas_put_rectangle(pcanvas, bg_color, canvas_x, canvas_y,
			   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
    }

    for (i = 0; i < count; i++) {
      switch (tile_sprs[i].type) {
      case DRAWN_SPRITE:
	canvas_put_sprite_full(pcanvas,
			       canvas_x + tile_sprs[i].offset_x,
			       canvas_y + tile_sprs[i].offset_y,
			       tile_sprs[i].sprite);
	break;
      case DRAWN_GRID:
	/*** Grid (map grid, borders, coastline, etc.) ***/
	tile_draw_grid(pcanvas, map_x, map_y, canvas_x, canvas_y,
		       D_FULL, citymode);
	break;
      }
    }
  } else {
    /* tile is unknown */
    canvas_put_rectangle(pcanvas, COLOR_STD_BLACK,
			 canvas_x, canvas_y,
			 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
  }

  if (!citymode && goto_is_active()) {
    /* put any goto lines on the tile. */
    if (is_real) {
      enum direction8 dir;

      for (dir = 0; dir < 8; dir++) {
	if (is_drawn_line(map_x, map_y, dir)) {
	  draw_segment(map_x, map_y, dir);
	}
      }
    }

    /* Some goto lines overlap onto the tile... */
    if (NORMAL_TILE_WIDTH % 2 == 0 || NORMAL_TILE_HEIGHT % 2 == 0) {
      int line_x = map_x - 1, line_y = map_y;

      if (normalize_map_pos(&line_x, &line_y)
	  && is_drawn_line(line_x, line_y, DIR8_NORTHEAST)) {
	draw_segment(line_x, line_y, DIR8_NORTHEAST);
      }
    }
  }
}

/**************************************************************************
  Draw the unique tile for the given map position, in non-isometric view.
  The coordinates have not been normalized, and are not guaranteed to be
  real (we have to draw unreal tiles too).
**************************************************************************/
static void put_tile(int map_x, int map_y)
{
  int canvas_x, canvas_y;

  if (map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y)) {
    freelog(LOG_DEBUG, "putting (%d,%d) at (%d,%d)",
	    map_x, map_y, canvas_x, canvas_y);
    put_one_tile(mapview_canvas.store, map_x, map_y,
		 canvas_x, canvas_y, FALSE);
  }
}

/****************************************************************************
   Draw the map grid around the given map tile at the given canvas position.
****************************************************************************/
static void tile_draw_map_grid(struct canvas *pcanvas,
			       int map_x, int map_y,
			       int canvas_x, int canvas_y,
			       enum draw_type draw)
{
  enum direction8 dir;

  if (!draw_map_grid) {
    return;
  }

  for (dir = 0; dir < 8; dir++) {
    int start_x, start_y, end_x, end_y, dx, dy;

    if (get_tile_boundaries(dir, 0, 1, draw,
			    &start_x, &start_y, &end_x, &end_y)) {
      DIRSTEP(dx, dy, dir);
      canvas_put_line(pcanvas,
		      get_grid_color(map_x, map_y, map_x + dx, map_y + dy),
		      LINE_NORMAL,
		      canvas_x + start_x, canvas_y + start_y,
		      end_x - start_x, end_y - start_y);
    }
  }
}

/****************************************************************************
  Draw the coastline of the given map tile at the given canvas position.

  If the map grid is drawn this will cover it up.
****************************************************************************/
static void tile_draw_coastline(struct canvas *pcanvas,
				int map_x, int map_y,
				int canvas_x, int canvas_y,
				enum draw_type draw)
{
  enum tile_terrain_type t1 = map_get_terrain(map_x, map_y), t2;

  if (!draw_coastline || draw_terrain || t1 == T_UNKNOWN) {
    return;
  }

  adjc_dir_iterate(map_x, map_y, adjc_x, adjc_y, dir) {
    int start_x, start_y, end_x, end_y;

    if (get_tile_boundaries(dir, 0, 1, draw,
			    &start_x, &start_y, &end_x, &end_y)) {
      t2 = map_get_terrain(adjc_x, adjc_y);
      if (t2 != T_UNKNOWN && (is_ocean(t1) ^ is_ocean(t2))) {
	canvas_put_line(pcanvas, COLOR_STD_OCEAN, LINE_NORMAL,
			canvas_x + start_x, canvas_y + start_y,
			end_x - start_x, end_y - start_y);
      }
    }
  } adjc_dir_iterate_end;
}

/****************************************************************************
   Draw the selection rectangle the given map tile at the given canvas
   position.
****************************************************************************/
static void tile_draw_selection(struct canvas *pcanvas,
				int map_x, int map_y,
				int canvas_x, int canvas_y,
				enum draw_type draw, bool citymode)
{
  const int inset = (is_isometric ? 0 : 1);
  enum direction8 dir;

  if (citymode) {
    return;
  }

  for (dir = 0; dir < 8; dir++) {
    int start_x, start_y, end_x, end_y, adjc_x, adjc_y;

    /* In non-iso view we draw the rectangle with an inset of 1.  This makes
     * it easy to distinguish from the map grid.
     *
     * In iso-view the inset doesn't work perfectly (see comments about
     * this elsewhere) so we draw without an inset.  This may cover up the
     * map grid if it is drawn. */
    if (get_tile_boundaries(dir, inset, 1, draw,
			    &start_x, &start_y, &end_x, &end_y)) {
      if (map_get_tile(map_x, map_y)->client.hilite == HILITE_CITY
	  || (is_isometric
	      && MAPSTEP(adjc_x, adjc_y, map_x, map_y, dir)
	      && map_get_tile(adjc_x, adjc_y)->client.hilite == HILITE_CITY)) {
	canvas_put_line(pcanvas, COLOR_STD_YELLOW, LINE_NORMAL,
			canvas_x + start_x, canvas_y + start_y,
			end_x - start_x, end_y - start_y);
      }
    }
  }
}


/****************************************************************************
   Draw the grid lines of the given map tile at the given canvas position
   in isometric view.  (This include the map grid, borders, and coastline).
****************************************************************************/
void tile_draw_grid(struct canvas *pcanvas, int map_x, int map_y,
			int canvas_x, int canvas_y,
		    enum draw_type draw, bool citymode)
{
  tile_draw_map_grid(pcanvas, map_x, map_y, canvas_x, canvas_y, draw);
  tile_draw_borders(pcanvas, map_x, map_y, canvas_x, canvas_y, draw);
  tile_draw_coastline(pcanvas, map_x, map_y, canvas_x, canvas_y, draw);
  tile_draw_selection(pcanvas, map_x, map_y, canvas_x, canvas_y,
		      draw, citymode);
}

/**************************************************************************
  Draw the unique tile for the given map position, in isometric view.
  The coordinates have not been normalized, and are not guaranteed to be
  real (we have to draw unreal tiles too).
**************************************************************************/
static void put_tile_iso(int map_x, int map_y, enum draw_type draw)
{
  int canvas_x, canvas_y;

  if (map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y)) {
    int height, width, height_unit;
    int offset_x, offset_y, offset_y_unit;

    freelog(LOG_DEBUG, "putting (%d,%d) at (%d,%d), draw %x",
	    map_x, map_y, canvas_x, canvas_y, draw);

    if ((draw & D_L) && (draw & D_R)) {
      width = NORMAL_TILE_WIDTH;
    } else {
      width = NORMAL_TILE_WIDTH / 2;
    }

    if (draw & D_L) {
      offset_x = 0;
    } else {
      offset_x = NORMAL_TILE_WIDTH / 2;
    }

    height = 0;
    if (draw & D_M) {
      height += NORMAL_TILE_HEIGHT / 2;
    }
    if (draw & D_B) {
      height += NORMAL_TILE_HEIGHT / 2;
    }

    height_unit = height;
    if (draw & D_T) {
      height_unit += NORMAL_TILE_HEIGHT / 2;
    }

    offset_y = (draw & D_M) ? 0 : NORMAL_TILE_HEIGHT / 2;

    if (draw & D_T) {
      offset_y_unit = 0;
    } else if (draw & D_M) {
      offset_y_unit = NORMAL_TILE_HEIGHT / 2;
    } else {
      offset_y_unit = NORMAL_TILE_HEIGHT;
    }

    if (normalize_map_pos(&map_x, &map_y)) {
      put_one_tile_iso(mapview_canvas.store,
		       map_x, map_y, canvas_x, canvas_y,
		       offset_x, offset_y, offset_y_unit,
		       width, height, height_unit,
		       draw, FALSE);
    } else {
      canvas_put_sprite(mapview_canvas.store, canvas_x, canvas_y,
			sprites.black_tile,
			offset_x, offset_y, width, height);
    }
  }
}

/**************************************************************************
  Update (refresh) the map canvas starting at the given tile (in map
  coordinates) and with the given dimensions (also in map coordinates).

  In non-iso view, this is easy.  In iso view, we have to use the
  Painter's Algorithm to draw the tiles in back first.  When we draw
  a tile, we tell the GUI which part of the tile to draw - which is
  necessary unless we have an extra buffering step.

  After refreshing the backing store tile-by-tile, we write the store
  out to the display if write_to_screen is specified.

  x, y, width, and height are in map coordinates; they need not be
  normalized or even real.
**************************************************************************/
void update_map_canvas(int canvas_x, int canvas_y, int width, int height)
{
  const int gui_x0 = mapview_canvas.gui_x0 + canvas_x;
  const int gui_y0 = mapview_canvas.gui_y0 + canvas_y;

  freelog(LOG_DEBUG,
	  "update_map_canvas(pos=(%d,%d), size=(%d,%d))",
	  canvas_x, canvas_y, width, height);

  /* Clear the area.  This is necessary since some parts of the rectangle
   * may not actually have any tiles drawn on them.  This will happen when
   * the mapview is large enough so that the tile is visible in multiple
   * locations.  In this case it will only be drawn in one place.
   *
   * Of course it's necessary to draw to the whole area to cover up any old
   * drawing that was done there. */
  canvas_put_rectangle(mapview_canvas.store, COLOR_STD_BLACK,
		       canvas_x, canvas_y, width, height);

  /* FIXME: we don't have to draw black (unknown) tiles since they're already
   * cleared. */
  if (is_isometric) {
    gui_rect_iterate(gui_x0, gui_y0, width, height, map_x, map_y, draw) {
      put_tile_iso(map_x, map_y, draw);
    } gui_rect_iterate_end;

    /* Draw the goto lines on top of the whole thing. This is done last as
     * we want it completely on top. */
    gui_rect_iterate(gui_x0, gui_y0, width, height, map_x, map_y, draw) {
      if (normalize_map_pos(&map_x, &map_y)) {
	adjc_dir_iterate(map_x, map_y, adjc_x, adjc_y, dir) {
	  if (is_drawn_line(map_x, map_y, dir)) {
	    draw_segment(map_x, map_y, dir);
	  }
	} adjc_dir_iterate_end;
      }
    } gui_rect_iterate_end;
  } else {
    /* not isometric */
    gui_rect_iterate(gui_x0, gui_y0, width, height, map_x, map_y, draw) {
      /*
       * We don't normalize until later because we want to draw
       * black tiles for unreal positions.
       */
      put_tile(map_x, map_y);
    } gui_rect_iterate_end;
  }

  show_city_descriptions(canvas_x, canvas_y, width, height);

  dirty_rect(canvas_x, canvas_y, width, height);
}

/**************************************************************************
 Update (only) the visible part of the map
**************************************************************************/
void update_map_canvas_visible(void)
{
  dirty_all();
  update_map_canvas(0, 0, mapview_canvas.width, mapview_canvas.height);
}

/* The maximum city description width and height.  This gives the dimensions
 * of a rectangle centered directly beneath the tile a city is on, that
 * contains the city description.
 *
 * These values are increased when drawing is done.  This may mean that
 * the change (from increasing the value) won't take place until the
 * next redraw. */
static int max_desc_width = 0, max_desc_height = 0;

/**************************************************************************
  Update the city description for the given city.
**************************************************************************/
void update_city_description(struct city *pcity)
{
  int canvas_x, canvas_y;

  /* We update the entire map canvas area that this city description
   * might be covering.  This may, for instance, redraw other city
   * descriptions that overlap with this one. */
  (void) map_to_canvas_pos(&canvas_x, &canvas_y, pcity->x, pcity->y);
  update_map_canvas(canvas_x - (max_desc_width - NORMAL_TILE_WIDTH) / 2,
		    canvas_y + NORMAL_TILE_HEIGHT,
		    max_desc_width, max_desc_height);
}

/**************************************************************************
  Show descriptions for all cities visible on the map canvas.
**************************************************************************/
void show_city_descriptions(int canvas_x, int canvas_y,
			    int width, int height)
{
  const int dx = max_desc_width - NORMAL_TILE_WIDTH, dy = max_desc_height;

  if (!draw_city_names && !draw_city_productions) {
    return;
  }

  prepare_show_city_descriptions();

  /* A city description is shown below the city.  It has a specified
   * maximum width and height (although these are only estimates).  Thus
   * we need to update some tiles above the mapview and some to the left
   * and right.
   *
   *                    /--W1--\   (W1 = NORMAL_TILE_WIDTH)
   *                    -------- \
   *                    | CITY | H1 (H1 = NORMAL_TILE_HEIGHT)
   *                    |      | /
   *               ------------------ \
   *               |  DESCRIPTION   | H2  (H2 = MAX_CITY_DESC_HEIGHT)
   *               |                | /
   *               ------------------
   *               \-------W2-------/    (W2 = MAX_CITY_DESC_WIDTH)
   *
   * We must draw H2 extra pixels above and (W2 - W1) / 2 extra pixels
   * to each side of the mapview.
   */
  gui_rect_iterate(mapview_canvas.gui_x0 + canvas_x - dx / 2,
		   mapview_canvas.gui_y0 + canvas_y - dy,
		   width + dx, height + dy - NORMAL_TILE_HEIGHT,
		   map_x, map_y, draw) {
    int canvas_x, canvas_y;
    struct city *pcity;

    if (normalize_map_pos(&map_x, &map_y)
	&& (pcity = map_get_city(map_x, map_y))) {
      int width = 0, height = 0;

      (void) map_to_canvas_pos(&canvas_x, &canvas_y, map_x, map_y);
      show_city_desc(mapview_canvas.store, canvas_x, canvas_y,
		     pcity, &width, &height);

      max_desc_width = MAX(width, max_desc_width);
      max_desc_height = MAX(height, max_desc_height);
    }
  } gui_rect_iterate_end;
}

/****************************************************************************
  Draw the goto route for the unit.  Return TRUE if anything is drawn.

  This duplicates drawing code that is run during the hover state.
****************************************************************************/
bool show_unit_orders(struct unit *punit)
{
  if (punit && unit_has_orders(punit)) {
    int unit_x = punit->x, unit_y = punit->y, i;

    for (i = 0; i < punit->orders.length; i++) {
      int index = (punit->orders.index + i) % punit->orders.length;
      struct unit_order *order;

      if (punit->orders.index + i >= punit->orders.length
	  && !punit->orders.repeat) {
	break;
      }

      order = &punit->orders.list[index];

      switch (order->order) {
      case ORDER_MOVE:
	draw_segment(unit_x, unit_y, order->dir);
	if (!MAPSTEP(unit_x, unit_y, unit_x, unit_y, order->dir)) {
	  /* This shouldn't happen unless the server gives us invalid
	   * data.  To avoid disaster we need to break out of the
	   * switch and the enclosing for loop. */
	  assert(0);
	  i = punit->orders.length;
	}
	break;
      default:
	/* TODO: graphics for other orders. */
	break;
      }
    }
    return TRUE;
  } else {
    return FALSE;
  }
}

#define ABS(x) (((x) >= 0) ? (x) : -(x))

/****************************************************************************
  Put a segment on the mapview (in iso-view only).  The segment is drawn
  from the source tile to the adjacent tile in the given direction.

  This method is faster than the one used for non-iso view, but there are
  two drawbacks:
  1.  Since each line spans multiple tiles, updating a single tile is
      harder (see update_map_canvas).
  2.  We don't know where the map wraps, so we have problems when we reach
      the wrapping point.  On very large mapviews this means segments might
      not be drawn near the wrapping points.
****************************************************************************/
static void draw_segment_iso(int src_x, int src_y, enum direction8 dir)
{
  int dest_x, dest_y;
  bool is_visible1, is_visible2;
  int canvas_src_x, canvas_src_y;
  int canvas_dest_x, canvas_dest_y;

  if (!MAPSTEP(dest_x, dest_y, src_x, src_y, dir)) {
    assert(0);
    return;
  }

  /* Find middle of tiles. y-1 to not undraw the the middle pixel of a
     horizontal line when we refresh the tile below-between. */
  is_visible1
    = map_to_canvas_pos(&canvas_src_x, &canvas_src_y, src_x, src_y);
  is_visible2
    = map_to_canvas_pos(&canvas_dest_x, &canvas_dest_y, dest_x, dest_y);

  if (!is_visible1 && !is_visible2) {
    return; /* No need to draw anything. */
  }

  canvas_src_x += NORMAL_TILE_WIDTH/2;
  canvas_src_y += NORMAL_TILE_HEIGHT/2-1;
  canvas_dest_x += NORMAL_TILE_WIDTH/2;
  canvas_dest_y += NORMAL_TILE_HEIGHT/2-1;

  /* HACK: sometimes we draw from a source tile on one side of the screen
   * to a destination tile on the other side.  In this case we don't want
   * to draw garbage.  Instead we just don't draw the line at all.  Note
   * that the non-iso-view method of drawing doesn't have this problem. */
  if (abs(canvas_dest_x - canvas_src_x) > NORMAL_TILE_WIDTH
      || abs(canvas_dest_y - canvas_src_y) > NORMAL_TILE_HEIGHT)
    return;

  /* draw it! */
  canvas_put_line(mapview_canvas.store, COLOR_STD_CYAN, LINE_GOTO,
		  canvas_src_x, canvas_src_y,
		  canvas_dest_x - canvas_src_x,
		  canvas_dest_y - canvas_src_y);
  dirty_rect(MIN(canvas_src_x, canvas_dest_x) - 1,
	     MIN(canvas_src_y, canvas_dest_y) - 1,
	     ABS(canvas_dest_x - canvas_src_x) + 2,
	     ABS(canvas_dest_y - canvas_src_y) + 2);
}

/****************************************************************************
  Put a half-segment (in non-iso view only).  The segment goes from the
  given tile *toward* the adjacent tile in the given direction.  However
  it only goes the the edge of the source tile.  Thus only one tile is
  spanned (making updates of single tiles easier) but two lines must be
  drawn to get a single segment (meaning more work & slower).
****************************************************************************/
static void put_line(int x, int y, enum direction8 dir)
{
  int canvas_src_x, canvas_src_y, canvas_dest_x, canvas_dest_y;

  if (!map_to_canvas_pos(&canvas_src_x, &canvas_src_y, x, y)) {
    return;
  }
  canvas_src_x += NORMAL_TILE_WIDTH/2;
  canvas_src_y += NORMAL_TILE_HEIGHT/2;
  DIRSTEP(canvas_dest_x, canvas_dest_y, dir);
  canvas_dest_x = canvas_src_x + (NORMAL_TILE_WIDTH * canvas_dest_x) / 2;
  canvas_dest_y = canvas_src_y + (NORMAL_TILE_WIDTH * canvas_dest_y) / 2;

  canvas_put_line(mapview_canvas.store, COLOR_STD_CYAN, LINE_GOTO,
		  canvas_src_x, canvas_src_y,
		  canvas_dest_x - canvas_src_x,
		  canvas_dest_y - canvas_src_y);

  dirty_rect(MIN(canvas_src_x, canvas_dest_x) - 1,
	     MIN(canvas_src_y, canvas_dest_y) - 1,
	     ABS(canvas_dest_x - canvas_src_x) + 2,
	     ABS(canvas_dest_y - canvas_src_y) + 2);
}

/****************************************************************************
  Draw a goto line at the given location and direction.  The line goes from
  the source tile to the adjacent tile in the given direction.
****************************************************************************/
void draw_segment(int src_x, int src_y, enum direction8 dir)
{
  if (is_isometric) {
    draw_segment_iso(src_x, src_y, dir);
  } else {
    int dest_x, dest_y;
    bool is_real = MAPSTEP(dest_x, dest_y, src_x, src_y, dir);

    assert(is_real);

    if (tile_visible_mapcanvas(src_x, src_y)) {
      put_line(src_x, src_y, dir);
    }
    if (tile_visible_mapcanvas(dest_x, dest_y)) {
      put_line(dest_x, dest_y, DIR_REVERSE(dir));
    }
  }
}

/**************************************************************************
  Remove the line from src_x, src_y in the given direction, and redraw
  the change if necessary.
**************************************************************************/
void undraw_segment(int src_x, int src_y, int dir)
{
  int dest_x, dest_y;

  assert(!is_drawn_line(src_x, src_y, dir));

  if (!MAPSTEP(dest_x, dest_y, src_x, src_y, dir)) {
    assert(0);
  }

  refresh_tile_mapcanvas(src_x, src_y, FALSE);
  refresh_tile_mapcanvas(dest_x, dest_y, FALSE);

  if (!is_isometric) {
    if (NORMAL_TILE_WIDTH % 2 == 0 || NORMAL_TILE_HEIGHT % 2 == 0) {
      if (dir == DIR8_NORTHEAST) {
	/* Since the tile doesn't have a middle we draw an extra pixel
	 * on the adjacent tile when drawing in this direction. */
	if (!MAPSTEP(dest_x, dest_y, src_x, src_y, DIR8_EAST)) {
	  assert(0);
	}
	refresh_tile_mapcanvas(dest_x, dest_y, FALSE);
      } else if (dir == DIR8_SOUTHWEST) {	/* the same */
	if (!MAPSTEP(dest_x, dest_y, src_x, src_y, DIR8_SOUTH)) {
	  assert(0);
	}
	refresh_tile_mapcanvas(dest_x, dest_y, FALSE);
      }
    }
  }
}

/****************************************************************************
  This function is called to decrease a unit's HP smoothly in battle
  when combat_animation is turned on.
****************************************************************************/
void decrease_unit_hp_smooth(struct unit *punit0, int hp0, 
			     struct unit *punit1, int hp1)
{
  static struct timer *anim_timer = NULL; 
  struct unit *losing_unit = (hp0 == 0 ? punit0 : punit1);
  int canvas_x, canvas_y, i;

  set_units_in_combat(punit0, punit1);

  while (punit0->hp > hp0 || punit1->hp > hp1) {
    const int diff0 = punit0->hp - hp0, diff1 = punit1->hp - hp1;

    anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

    if (myrand(diff0 + diff1) < diff0) {
      punit0->hp--;
      refresh_tile_mapcanvas(punit0->x, punit0->y, FALSE);
    } else {
      punit1->hp--;
      refresh_tile_mapcanvas(punit1->x, punit1->y, FALSE);
    }

    flush_dirty();
    gui_flush();

    usleep_since_timer_start(anim_timer, 10000);
  }

  if (num_tiles_explode_unit > 0
      && map_to_canvas_pos(&canvas_x, &canvas_y,
			   losing_unit->x, losing_unit->y)) {
    refresh_tile_mapcanvas(losing_unit->x, losing_unit->y, FALSE);
    canvas_copy(mapview_canvas.tmp_store, mapview_canvas.store,
		canvas_x, canvas_y, canvas_x, canvas_y,
		NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);

    for (i = 0; i < num_tiles_explode_unit; i++) {
      int w, h;

      get_sprite_dimensions(sprites.explode.unit[i], &w, &h);
      anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

      /* We first draw the explosion onto the unit and draw draw the
       * complete thing onto the map canvas window. This avoids
       * flickering. */
      canvas_copy(mapview_canvas.store, mapview_canvas.tmp_store,
		  canvas_x, canvas_y, canvas_x, canvas_y,
		  NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      canvas_put_sprite_full(mapview_canvas.store,
			     canvas_x + NORMAL_TILE_WIDTH / 2 - w / 2,
			     canvas_y + NORMAL_TILE_HEIGHT / 2 - h / 2,
			     sprites.explode.unit[i]);
      dirty_rect(canvas_x, canvas_y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);

      flush_dirty();
      gui_flush();

      usleep_since_timer_start(anim_timer, 20000);
    }
  }

  set_units_in_combat(NULL, NULL);
  refresh_tile_mapcanvas(punit0->x, punit0->y, FALSE);
  refresh_tile_mapcanvas(punit1->x, punit1->y, FALSE);
}

/**************************************************************************
  Animates punit's "smooth" move from (x0, y0) to (x0+dx, y0+dy).
  Note: Works only for adjacent-tile moves.
**************************************************************************/
void move_unit_map_canvas(struct unit *punit,
			  int map_x, int map_y, int dx, int dy)
{
  static struct timer *anim_timer = NULL;
  int dest_x, dest_y;

  /* only works for adjacent-square moves */
  if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0)) {
    return;
  }

  /* Go ahead and start the timer. */
  anim_timer = renew_timer_start(anim_timer, TIMER_USER, TIMER_ACTIVE);

  if (punit == get_unit_in_focus() && hover_state != HOVER_NONE) {
    set_hover_state(NULL, HOVER_NONE, ACTIVITY_LAST);
    update_unit_info_label(punit);
  }

  dest_x = map_x + dx;
  dest_y = map_y + dy;
  if (!normalize_map_pos(&dest_x, &dest_y)) {
    assert(0);
  }

  if (tile_visible_mapcanvas(map_x, map_y)
      || tile_visible_mapcanvas(dest_x, dest_y)) {
    int start_x, start_y;
    int canvas_dx, canvas_dy;
    double timing_sec = (double)smooth_move_unit_msec / 1000.0, mytime;

    assert(smooth_move_unit_msec > 0);

    /* See map_to_canvas_pos for an explanation. */
    if (is_isometric) {
      canvas_dx = (dx - dy) * NORMAL_TILE_WIDTH / 2;
      canvas_dy = (dx + dy) * NORMAL_TILE_HEIGHT / 2;
    } else {
      canvas_dx = NORMAL_TILE_WIDTH * dx;
      canvas_dy = NORMAL_TILE_HEIGHT * dy;
    }

    map_to_canvas_pos(&start_x, &start_y, map_x, map_y);
    if (is_isometric) {
      start_y -= NORMAL_TILE_HEIGHT / 2;
    }

    /* Flush before we start animating. */
    flush_dirty();
    gui_flush();

    do {
      int new_x, new_y;

      mytime = MIN(read_timer_seconds(anim_timer), timing_sec);

      new_x = start_x + canvas_dx * (mytime / timing_sec);
      new_y = start_y + canvas_dy * (mytime / timing_sec);

      /* Backup the canvas store to the temp store. */
      canvas_copy(mapview_canvas.tmp_store, mapview_canvas.store,
		  new_x, new_y, new_x, new_y,
		  UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);

      /* Draw */
      put_unit_full(punit, mapview_canvas.store, new_x, new_y);
      dirty_rect(new_x, new_y, UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);

      /* Flush. */
      flush_dirty();
      gui_flush();

      /* Restore the backup.  It won't take effect until the next flush. */
      canvas_copy(mapview_canvas.store, mapview_canvas.tmp_store,
		  new_x, new_y, new_x, new_y,
		  UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);
      dirty_rect(new_x, new_y, UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);
    } while (mytime < timing_sec);
  }
}

/**************************************************************************
  Find the "best" city to associate with the selected tile.
    a.  A city working the tile is the best
    b.  If another player is working the tile, return NULL.
    c.  If no city is working the tile, choose a city that could work
        the tile.
    d.  If multiple cities could work it, choose the most recently
        "looked at".
    e.  If none of the cities were looked at last, choose "randomly".
    f.  If no cities can work it, return NULL.
**************************************************************************/
struct city *find_city_near_tile(int x, int y)
{
  struct city *pcity = map_get_tile(x, y)->worked, *pcity2;
  static struct city *last_pcity = NULL;

  if (pcity) {
    if (pcity->owner == game.player_idx) {
      /* rule a */
      last_pcity = pcity;
      return pcity;
    } else {
      /* rule b */
      return NULL;
    }
  }

  pcity2 = NULL;		/* rule f */
  city_map_checked_iterate(x, y, city_x, city_y, map_x, map_y) {
    pcity = map_get_city(map_x, map_y);
    if (pcity && pcity->owner == game.player_idx
	&& get_worker_city(pcity, CITY_MAP_SIZE - 1 - city_x,
			   CITY_MAP_SIZE - 1 - city_y) == C_TILE_EMPTY) {
      /* rule c */
      /*
       * Note, we must explicitly check if the tile is workable (with
       * get_worker_city(), above) since it is possible that another
       * city (perhaps an unseen enemy city) may be working it,
       * causing it to be marked as C_TILE_UNAVAILABLE.
       */
      if (pcity == last_pcity) {
	return pcity;		/* rule d */
      }
      pcity2 = pcity;
    }
  }
  city_map_checked_iterate_end;

  /* rule e */
  last_pcity = pcity2;
  return pcity2;
}

/**************************************************************************
  Find the mapview city production text for the given city, and place it
  into the buffer.
**************************************************************************/
void get_city_mapview_production(struct city *pcity,
                                 char *buffer, size_t buffer_len)
{
  int turns = city_turns_to_build(pcity, pcity->currently_building,
				  pcity->is_building_unit, TRUE);
				
  if (pcity->is_building_unit) {
    struct unit_type *punit_type =
		get_unit_type(pcity->currently_building);
    if (turns < 999) {
      my_snprintf(buffer, buffer_len, "%s %d",
                  punit_type->name, turns);
    } else {
      my_snprintf(buffer, buffer_len, "%s -",
                  punit_type->name);
    }
  } else {
    struct impr_type *pimprovement_type =
		get_improvement_type(pcity->currently_building);
    if (pcity->currently_building == B_CAPITAL) {
      my_snprintf(buffer, buffer_len, "%s", pimprovement_type->name);
    } else if (turns < 999) {
      my_snprintf(buffer, buffer_len, "%s %d",
		  pimprovement_type->name, turns);
    } else {
      my_snprintf(buffer, buffer_len, "%s -",
                  pimprovement_type->name);
    }
  }
}

static enum update_type needed_updates = UPDATE_NONE;

/**************************************************************************
  This function, along with unqueue_mapview_update(), helps in updating
  the mapview when a packet is received.  Previously, we just called
  update_map_canvas when (for instance) a city update was received.
  Not only would this often end up with a lot of duplicated work, but it
  would also draw over the city descriptions, which would then just
  "disappear" from the mapview.  The hack is to instead call
  queue_mapview_update in place of this update, and later (after all
  packets have been read) call unqueue_mapview_update.  The functions
  don't track which areas of the screen need updating, rather when the
  unqueue is done we just update the whole visible mapqueue, and redraw
  the city descriptions.

  Using these functions, updates are done correctly, and are probably
  faster too.  But it's a bit of a hack to insert this code into the
  packet-handling code.
**************************************************************************/
void queue_mapview_update(enum update_type update)
{
  needed_updates |= update;
}

/**************************************************************************
  See comment for queue_mapview_update().
**************************************************************************/
void unqueue_mapview_updates(void)
{
  freelog(LOG_DEBUG, "unqueue_mapview_update: needed_updates=%d",
	  needed_updates);

  if (needed_updates & UPDATE_MAP_CANVAS_VISIBLE) {
    update_map_canvas_visible();
  } else if (needed_updates & UPDATE_CITY_DESCRIPTIONS) {
    update_city_descriptions();
  }
  needed_updates = UPDATE_NONE;

  flush_dirty();
  flush_dirty_overview();
}

/**************************************************************************
  Fill the two buffers which information about the city which is shown
  below it. It takes draw_city_names and draw_city_growth into account.
**************************************************************************/
void get_city_mapview_name_and_growth(struct city *pcity,
				      char *name_buffer,
				      size_t name_buffer_len,
				      char *growth_buffer,
				      size_t growth_buffer_len,
				      enum color_std *growth_color)
{
  if (!draw_city_names) {
    name_buffer[0] = '\0';
    growth_buffer[0] = '\0';
    *growth_color = COLOR_STD_WHITE;
    return;
  }

  my_snprintf(name_buffer, name_buffer_len, pcity->name);

  if (draw_city_growth && pcity->owner == game.player_idx) {
    int turns = city_turns_to_grow(pcity);

    if (turns == 0) {
      my_snprintf(growth_buffer, growth_buffer_len, "X");
    } else if (turns == FC_INFINITY) {
      my_snprintf(growth_buffer, growth_buffer_len, "-");
    } else {
      /* Negative turns means we're shrinking, but that's handled
         down below. */
      my_snprintf(growth_buffer, growth_buffer_len, "%d", abs(turns));
    }

    if (turns <= 0) {
      /* A blocked or shrinking city has its growth status shown in red. */
      *growth_color = COLOR_STD_RED;
    } else {
      *growth_color = COLOR_STD_WHITE;
    }
  } else {
    growth_buffer[0] = '\0';
    *growth_color = COLOR_STD_WHITE;
  }
}

/**************************************************************************
  Copies the overview image from the backing store to the window and
  draws the viewrect on top of it.
**************************************************************************/
static void redraw_overview(void)
{
  struct canvas *dest = get_overview_window();

  if (!dest || !overview.store) {
    return;
  }

  {
    struct canvas *src = overview.store;
    int x = overview.map_x0 * OVERVIEW_TILE_SIZE;
    int y = overview.map_y0 * OVERVIEW_TILE_SIZE;
    int ix = overview.width - x;
    int iy = overview.height - y;

    canvas_copy(dest, src, 0, 0, ix, iy, x, y);
    canvas_copy(dest, src, 0, y, ix, 0, x, iy);
    canvas_copy(dest, src, x, 0, 0, iy, ix, y);
    canvas_copy(dest, src, x, y, 0, 0, ix, iy);
  }

  {
    int i;
    int x[4], y[4];

    get_mapview_corners(x, y);

    for (i = 0; i < 4; i++) {
      int src_x = x[i];
      int src_y = y[i];
      int dest_x = x[(i + 1) % 4];
      int dest_y = y[(i + 1) % 4];

      canvas_put_line(dest, COLOR_STD_WHITE, LINE_NORMAL, src_x, src_y,
		      dest_x - src_x, dest_y - src_y);
    }
  }

  overview_dirty = FALSE;
}

/****************************************************************************
  Mark the overview as "dirty" so that it will be redrawn soon.
****************************************************************************/
static void dirty_overview(void)
{
  overview_dirty = TRUE;
}

/****************************************************************************
  Redraw the overview if it is "dirty".
****************************************************************************/
static void flush_dirty_overview(void)
{
  if (overview_dirty) {
    redraw_overview();
  }
}

/**************************************************************************
  Center the overview around the mapview.
**************************************************************************/
static void center_tile_overviewcanvas(int map_x, int map_y)
{
  /* The overview coordinates are equivalent to (scaled) natural
   * coordinates. */
  do_in_natural_pos(ntl_x, ntl_y, map_x, map_y) {
    /* NOTE: this embeds the map wrapping in the overview code.  This is
     * basically necessary for the overview to be efficiently
     * updated. */
    if (topo_has_flag(TF_WRAPX)) {
      overview.map_x0 = FC_WRAP(ntl_x - NATURAL_WIDTH / 2, NATURAL_WIDTH);
    } else {
      overview.map_x0 = 0;
    }
    if (topo_has_flag(TF_WRAPY)) {
      overview.map_y0 = FC_WRAP(ntl_y - NATURAL_HEIGHT / 2, NATURAL_HEIGHT);
    } else {
      overview.map_y0 = 0;
    }
    redraw_overview();
  } do_in_natural_pos_end;
}

/**************************************************************************
  Finds the overview (canvas) coordinates for a given map position.
**************************************************************************/
void map_to_overview_pos(int *overview_x, int *overview_y,
			 int map_x, int map_y)
{
  /* The map position may not be normal, for instance when the mapview
   * origin is not a normal position.
   *
   * NOTE: this embeds the map wrapping in the overview code. */
  do_in_natural_pos(ntl_x, ntl_y, map_x, map_y) {
    int ovr_x = ntl_x - overview.map_x0, ovr_y = ntl_y - overview.map_y0;

    if (topo_has_flag(TF_WRAPX)) {
      ovr_x = FC_WRAP(ovr_x, NATURAL_WIDTH);
    } else {
      if (topo_has_flag(TF_ISO)) {
	/* HACK: For iso-maps that don't wrap in the X direction we clip
	 * a half-tile off of the left and right of the overview.  This
	 * means some tiles only are halfway shown.  However it means we
	 * don't show any unreal tiles, which we'd otherwise be doing.  The
	 * rest of the code can't handle unreal tiles in the overview. */
	ovr_x--;
      }
    }
    if (topo_has_flag(TF_WRAPY)) {
      ovr_y = FC_WRAP(ovr_y, NATURAL_HEIGHT);
    }
    *overview_x = OVERVIEW_TILE_SIZE * ovr_x;
    *overview_y = OVERVIEW_TILE_SIZE * ovr_y;
  } do_in_natural_pos_end;
}

/**************************************************************************
  Finds the map coordinates for a given overview (canvas) position.
**************************************************************************/
void overview_to_map_pos(int *map_x, int *map_y,
			 int overview_x, int overview_y)
{
  int ntl_x = overview_x / OVERVIEW_TILE_SIZE + overview.map_x0;
  int ntl_y = overview_y / OVERVIEW_TILE_SIZE + overview.map_y0;

  if (topo_has_flag(TF_ISO) && !topo_has_flag(TF_WRAPX)) {
    /* Clip half tile left and right.  See comment in map_to_overview_pos. */
    ntl_x++;
  }

  natural_to_map_pos(map_x, map_y, ntl_x, ntl_y);
  if (!normalize_map_pos(map_x, map_y)) {
    /* All positions on the overview should be valid. */
    assert(FALSE);
  }
}

/**************************************************************************
  Find the corners of the mapview, in overview coordinates.  Used to draw
  the "mapview window" rectangle onto the overview.
**************************************************************************/
static void get_mapview_corners(int x[4], int y[4])
{
  int map_x0, map_y0;

  canvas_to_map_pos(&map_x0, &map_y0, 0, 0);
  map_to_overview_pos(&x[0], &y[0], map_x0, map_y0);

  /* Note: these calculations operate on overview coordinates as if they
   * are natural.  Corners may be off by one tile, however. */

  if (is_isometric && !topo_has_flag(TF_ISO)) {
    /* We start with the west corner. */

    /* North */
    x[1] = x[0] + OVERVIEW_TILE_WIDTH * mapview_canvas.tile_width;
    y[1] = y[0] - OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_width;

    /* East */
    x[2] = x[1] + OVERVIEW_TILE_WIDTH * mapview_canvas.tile_height;
    y[2] = y[1] + OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_height;

    /* South */
    x[3] = x[0] + OVERVIEW_TILE_WIDTH * mapview_canvas.tile_height;
    y[3] = y[0] + OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_height;
  } else if (!is_isometric && topo_has_flag(TF_ISO)) {
    /* We start with the west corner.  Note the X scale is smaller. */

    /* North */
    x[1] = x[0] + OVERVIEW_TILE_WIDTH * mapview_canvas.tile_width / 2;
    y[1] = y[0] + OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_width;

    /* East */
    x[2] = x[1] - OVERVIEW_TILE_WIDTH * mapview_canvas.tile_height / 2;
    y[2] = y[1] + OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_height;

    /* South */
    x[3] = x[2] - OVERVIEW_TILE_WIDTH * mapview_canvas.tile_width / 2;
    y[3] = y[2] - OVERVIEW_TILE_HEIGHT * mapview_canvas.tile_width;
  } else {
    /* We start with the northwest corner. */
    int screen_width = mapview_canvas.tile_width;
    int screen_height = mapview_canvas.tile_height * (is_isometric ? 2 : 1);

    /* Northeast */
    x[1] = x[0] + OVERVIEW_TILE_WIDTH * screen_width - 1;
    y[1] = y[0];

    /* Southeast */
    x[2] = x[1];
    y[2] = y[0] + OVERVIEW_TILE_HEIGHT * screen_height - 1;

    /* Southwest */
    x[3] = x[0];
    y[3] = y[2];
  }

  freelog(LOG_DEBUG, "(%d,%d)->(%d,%x)->(%d,%d)->(%d,%d)",
	  x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
}

/**************************************************************************
  Redraw the entire backing store for the overview minimap.
**************************************************************************/
void refresh_overview_canvas(void)
{
  whole_map_iterate(x, y) {
    overview_update_tile(x, y);
  } whole_map_iterate_end;
  redraw_overview();
}

/**************************************************************************
  Redraw the given map position in the overview canvas.
**************************************************************************/
void overview_update_tile(int map_x, int map_y)
{
  /* Base overview positions are just like natural positions, but scaled to
   * the overview tile dimensions. */
  do_in_natural_pos(ntl_x, ntl_y, map_x, map_y) {
    int overview_y = ntl_y * OVERVIEW_TILE_SIZE;
    int overview_x = ntl_x * OVERVIEW_TILE_SIZE;

    if (topo_has_flag(TF_ISO)) {
      if (topo_has_flag(TF_WRAPX)) {
	if (overview_x > overview.width - OVERVIEW_TILE_WIDTH) {
	  /* This tile is shown half on the left and half on the right
	   * side of the overview.  So we have to draw it in two parts. */
	  canvas_put_rectangle(overview.store, 
			       overview_tile_color(map_x, map_y),
			       overview_x - overview.width, overview_y,
			       OVERVIEW_TILE_WIDTH, OVERVIEW_TILE_HEIGHT); 
	}     
      } else {
	/* Clip half tile left and right.
	 * See comment in map_to_overview_pos. */
	overview_x -= OVERVIEW_TILE_SIZE;
      }
    } 
    
    canvas_put_rectangle(overview.store,
			 overview_tile_color(map_x, map_y),
			 overview_x, overview_y,
			 OVERVIEW_TILE_WIDTH, OVERVIEW_TILE_HEIGHT);

    dirty_overview();
  } do_in_natural_pos_end;
}

/**************************************************************************
  Called if the map size is know or changes.
**************************************************************************/
void set_overview_dimensions(int width, int height)
{
  int shift = 0; /* used to calculate shift in iso view */

  /* Set the scale of the overview map.  Note, since only the width is
   * used to calculate the overview scale you can end up with a really
   * tall or short overview if your map is unusually sized. */

  OVERVIEW_TILE_SIZE = (120 / width) + 1;
  if (topo_has_flag(TF_ISO)) {
    OVERVIEW_TILE_SIZE = MAX(120 / width, 1);

    /* Clip half tile left and right.  See comment in map_to_overview_pos. */
    shift = (!topo_has_flag(TF_WRAPX) ? -OVERVIEW_TILE_SIZE : 0);
  }

  overview.height = OVERVIEW_TILE_HEIGHT * height;
  overview.width = OVERVIEW_TILE_WIDTH * width + shift; 

  if (overview.store) {
    canvas_free(overview.store);
  }
  overview.store = canvas_create(overview.width, overview.height);
  canvas_put_rectangle(overview.store, COLOR_STD_BLACK,
		       0, 0, overview.width, overview.height);
  update_map_canvas_scrollbars_size();

  /* Call gui specific function. */
  map_size_changed();
}

/**************************************************************************
  Called if the map in the GUI is resized.

  Returns TRUE iff the canvas was redrawn.
**************************************************************************/
bool map_canvas_resized(int width, int height)
{
  int old_tile_width = mapview_canvas.tile_width;
  int old_tile_height = mapview_canvas.tile_height;
  int old_width = mapview_canvas.width, old_height = mapview_canvas.height;
  int tile_width = (width + NORMAL_TILE_WIDTH - 1) / NORMAL_TILE_WIDTH;
  int tile_height = (height + NORMAL_TILE_HEIGHT - 1) / NORMAL_TILE_HEIGHT;
  int full_width = tile_width * NORMAL_TILE_WIDTH;
  int full_height = tile_height * NORMAL_TILE_HEIGHT;
  bool tile_size_changed, size_changed, redrawn = FALSE;

  /* Resized */

  /* Since a resize is only triggered when the tile_*** changes, the canvas
   * width and height must include the entire backing store - otherwise
   * small resizings may lead to undrawn tiles. */
  mapview_canvas.tile_width = tile_width;
  mapview_canvas.tile_height = tile_height;
  mapview_canvas.width = width;
  mapview_canvas.height = height;
  mapview_canvas.store_width = full_width;
  mapview_canvas.store_height = full_height;

  /* Check for what's changed. */
  tile_size_changed = (tile_width != old_tile_width
 		       || tile_height != old_tile_height);
  size_changed = (width != old_width || height != old_height);

  /* If the tile size has changed, resize the canvas. */
  if (tile_size_changed) {
    if (mapview_canvas.store) {
      canvas_free(mapview_canvas.store);
      canvas_free(mapview_canvas.tmp_store);
    }
    mapview_canvas.store = canvas_create(full_width, full_height);
    canvas_put_rectangle(mapview_canvas.store, COLOR_STD_BLACK, 0, 0,
			 full_width, full_height);

    mapview_canvas.tmp_store = canvas_create(full_width, full_height);
  }

  if (map_exists() && can_client_change_view()) {
    if (tile_size_changed) {
      update_map_canvas_visible();
      refresh_overview_canvas();
      redrawn = TRUE;
    }

    /* If the width/height has changed, update the scrollbars even if
     * the backing store is not resized. */
    if (size_changed) {
      update_map_canvas_scrollbars_size();
      update_map_canvas_scrollbars();
    }
  }

  return redrawn;
}

/**************************************************************************
  Sets up data for the mapview and overview.
**************************************************************************/
void init_mapcanvas_and_overview(void)
{
}
