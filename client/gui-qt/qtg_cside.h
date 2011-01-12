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

#ifndef FC__QTG_CSIDE_H
#define FC__QTG_CSIDE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct gui_funcs {
  void (*ui_init)(void);
  void (*ui_main)(int argc, char *argv[]);
  void (*ui_exit)(void);

  bool (*isometric_view_supported)(void);
  bool (*overhead_view_supported)(void);
  void (*free_intro_radar_sprites)(void);

  void (*gui_set_rulesets)(int num_rulesets, char **rulesets);
  void (*gui_options_extra_init)(void);
  void (*gui_server_connect)(void);
  void (*add_net_input)(int sock);
  void (*remove_net_input)(void);
  void (*real_conn_list_dialog_update)(void);
  void (*close_connection_dialog)(void);
  void (*add_idle_callback)(void (callback)(void *), void *data);
  void (*sound_bell)(void);

  void (*set_unit_icon)(int idx, struct unit *punit);
  void (*set_unit_icons_more_arrow)(bool onoff);
  void (*gui_update_font)(const char *font_name, const char *font_value);
  void (*set_city_names_font_sizes)(int my_city_names_font_size,
                                    int my_city_productions_font_size);

  void (*editgui_refresh)(void);
  void (*editgui_notify_object_created)(int tag, int id);
  void (*editgui_notify_object_changed)(int objtype, int object_id, bool remove);
  void (*editgui_popup_properties)(const struct tile_list *tiles, int objtype);
  void (*editgui_tileset_changed)(void);
  void (*editgui_popdown_all)(void);

  void (*gui_ggz_embed_ensure_server)(void);
  void (*gui_ggz_embed_leave_table)(void);
  void (*remove_ggz_input)(void);

  void (*update_timeout_label)(void);
  void (*real_city_dialog_popup)(struct city *pcity);
  void (*real_city_dialog_refresh)(struct city *pcity);
  void (*popdown_city_dialog)(struct city *pcity);
  void (*popdown_all_city_dialogs)(void);
  void (*refresh_unit_city_dialogs)(struct unit *punit);
  bool (*city_dialog_is_open)(struct city *pcity);

  void (*gui_load_theme)(const char *directory, const char *theme_name);
  void (*gui_clear_theme)(void);
  char **(*get_gui_specific_themes_directories)(int *count);
  char **(*get_useable_themes_in_directory)(const char *directory, int *count);
};

struct gui_funcs *get_gui_funcs(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__QTG_CSIDE_H */
