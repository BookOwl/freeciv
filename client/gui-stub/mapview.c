/* mapview.c -- PLACEHOLDER */

#include "mapview.h"


bool tile_visible_mapcanvas(int x, int y)
{
	/* PORTME */
	return FALSE;
}

bool tile_visible_and_not_on_border_mapcanvas(int x, int y)
{
	/* PORTME */
	return 0;
}

void
update_info_label(void)
{
	/* PORTME */
}

void
update_unit_info_label(struct unit *punit)
{
	/* PORTME */
}

void
update_timeout_label(void)
{
	/* PORTME */
}

void
update_turn_done_button(bool do_restore)
{
	/* PORTME */
}

void
set_indicator_icons(int bulb, int sol, int flake, int gov)
{
	/* PORTME */
}

void
set_overview_dimensions(int x, int y)
{
	/* PORTME */
}

void
overview_update_tile(int x, int y)
{
	/* PORTME */
}

void
center_tile_mapcanvas(int x, int y)
{
	/* PORTME */
}

void
get_center_tile_mapcanvas(int *x, int *y)
{
	/* PORTME */
}

void
update_map_canvas(int tile_x, int tile_y, int width, int height,
		  bool write_to_screen)
{
	/* PORTME */
}

void
update_map_canvas_visible(void)
{
	/* PORTME */
}

void
update_map_canvas_scrollbars(void)
{
	/* PORTME */
}

void
update_city_descriptions(void)
{
	/* PORTME */
}

void
put_cross_overlay_tile(int x,int y)
{
	/* PORTME */
}

void
put_city_workers(struct city *pcity, int color)
{
	/* PORTME */
}

void
move_unit_map_canvas(struct unit *punit, int x0, int y0, int x1, int y1)
{
	/* PORTME */
}

/**************************************************************************
 This function is called to decrease a unit's HP smoothly in battle when
 combat_animation is turned on.
**************************************************************************/
void
decrease_unit_hp_smooth(struct unit *punit0, int hp0,
                             struct unit *punit1, int hp1)
{
	/* PORTME */
}

void
put_nuke_mushroom_pixmaps(int abs_x0, int abs_y0)
{
	/* PORTME */
}

void
refresh_overview_canvas(void)
{
	/* PORTME */
}

void
refresh_overview_viewrect(void)
{
	/* PORTME */
}

void draw_segment(int src_x, int src_y, int dir)
{
	/* PORTME */
}

void undraw_segment(int src_x, int src_y, int dir)
{
	/* PORTME */
}
