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

#ifndef FC__QTG_CXXSIDE_H
#define FC__QTG_CXXSIDE_H

#include "pages.h"

void setup_gui_funcs();

void qtg_ui_init();
void qtg_ui_main(int argc, char *argv[]);
void qtg_ui_exit();

void qtg_real_output_window_append(const char *astring,
                                   const struct text_tag_list *tags,
                                   int conn_id);

bool qtg_isometric_view_supported();
bool qtg_overhead_view_supported();
void qtg_free_intro_radar_sprites();
struct sprite *qtg_load_gfxfile(const char *filename);
void qtg_get_sprite_dimensions(struct sprite *sprite, int *width, int *height);
struct sprite *qtg_crop_sprite(struct sprite *source,
                               int x, int y, int width, int height,
                               struct sprite *mask,
                               int mask_offset_x, int mask_offset_y);
void qtg_free_sprite(struct sprite *s);

void qtg_gui_set_rulesets(int num_rulesets, char **rulesets);
void qtg_gui_options_extra_init();
void qtg_gui_server_connect();
void qtg_add_net_input(int sock);
void qtg_remove_net_input();
void qtg_real_conn_list_dialog_update();
void qtg_close_connection_dialog();
void qtg_add_idle_callback(void (callback)(void *), void *data);
void qtg_sound_bell();

void qtg_real_set_client_page(enum client_pages page);

void qtg_set_unit_icon(int idx, struct unit *punit);
void qtg_set_unit_icons_more_arrow(bool onoff);
void qtg_gui_update_font(const char *font_name, const char *font_value);
void qtg_set_city_names_font_sizes(int my_city_names_font_size,
                                   int my_city_productions_font_size);

void qtg_editgui_refresh();
void qtg_editgui_notify_object_created(int tag, int id);
void qtg_editgui_notify_object_changed(int objtype, int object_id, bool remove);
void qtg_editgui_popup_properties(const struct tile_list *tiles, int objtype);
void qtg_editgui_tileset_changed();
void qtg_editgui_popdown_all();

void qtg_gui_ggz_embed_ensure_server();
void qtg_gui_ggz_embed_leave_table();
void qtg_add_ggz_input(int sock);
void qtg_remove_ggz_input();

void qtg_update_timeout_label();
void qtg_real_city_dialog_popup(struct city *pcity);
void qtg_real_city_dialog_refresh(struct city *pcity);
void qtg_popdown_city_dialog(struct city *pcity);
void qtg_popdown_all_city_dialogs();
void qtg_refresh_unit_city_dialogs(struct unit *punit);
bool qtg_city_dialog_is_open(struct city *pcity);

void qtg_gui_load_theme(const char *directory, const char *theme_name);
void qtg_gui_clear_theme();
char **qtg_get_gui_specific_themes_directories(int *count);
char **qtg_get_useable_themes_in_directory(const char *directory, int *count);

#endif /* FC__QTG_CXXSIDE_H */
