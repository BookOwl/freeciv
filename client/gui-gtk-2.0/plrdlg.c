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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "diptreaty.h"
#include "fcintl.h"
#include "game.h"
#include "packets.h"
#include "nation.h"
#include "player.h"
#include "support.h"

#include "chatline.h"
#include "civclient.h"
#include "climisc.h"
#include "clinet.h"
#include "connectdlg_common.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "inteldlg.h"
#include "spaceshipdlg.h"
#include "tilespec.h"
#include "colors.h"
#include "graphics.h"
#include "options.h"
#include "text.h"

#include "plrdlg.h"

struct gui_dialog *players_dialog_shell;
static GtkWidget *players_list;
static GtkTreeSelection *players_selection;
static GtkWidget *players_int_command;
static GtkWidget *players_meet_command;
static GtkWidget *players_war_command;
static GtkWidget *players_vision_command;
static GtkWidget *players_sship_command;
static GtkListStore *store;
static GtkTreeModel *model;

static gint ncolumns;

static void create_players_dialog(void);
static void players_meet_callback(GtkMenuItem *item, gpointer data);
static void players_war_callback(GtkMenuItem *item, gpointer data);
static void players_vision_callback(GtkMenuItem *item, gpointer data);
static void players_intel_callback(GtkMenuItem *item, gpointer data);
static void players_sship_callback(GtkMenuItem *item, gpointer data);
static void players_ai_toggle_callback(GtkMenuItem *item, gpointer data);
static void players_ai_skill_callback(GtkMenuItem *item, gpointer data);


static void update_views(void);

/**************************************************************************
popup the dialog 10% inside the main-window, and optionally raise it.
**************************************************************************/
void popup_players_dialog(bool raise)
{
  if (!players_dialog_shell){
    create_players_dialog();
  }
  gui_dialog_present(players_dialog_shell);
  if (raise) {
    gui_dialog_raise(players_dialog_shell);
  }
}

/****************************************************************
 Closes the players dialog.
*****************************************************************/
void popdown_players_dialog(void)
{
  if (players_dialog_shell) {
    gui_dialog_destroy(players_dialog_shell);
  }
}

/**************************************************************************
...
**************************************************************************/
static GdkPixbuf *create_player_icon(struct player *plr)
{
  int width, height;
  GdkPixbuf *tmp;
  GdkPixmap *pixmap;

  gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);

  pixmap = gdk_pixmap_new(root_window, width, height, -1);

  gdk_gc_set_foreground(civ_gc,
                        &get_color(tileset,
				   COLOR_PLAYER_COLOR_BACKGROUND)->color);
  gdk_draw_rectangle(pixmap, civ_gc, TRUE, 0, 0, width, height);

  gdk_gc_set_foreground(civ_gc, &get_player_color(tileset, plr)->color);
  gdk_draw_rectangle(pixmap, civ_gc, TRUE, 1, 1, width - 2, height - 2);

  tmp = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 
      0, 0, 0, 0, -1, -1);

  g_object_unref(pixmap);
  return tmp;
}

/**************************************************************************
...
**************************************************************************/
static void update_players_menu(void)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    struct player *plr;
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);
    plr = get_player(plrno);
  
    if (plr->spaceship.state != SSHIP_NONE) {
      gtk_widget_set_sensitive(players_sship_command, TRUE);
    } else {
      gtk_widget_set_sensitive(players_sship_command, FALSE);
    }

    if (game.player_ptr) {
      switch (pplayer_get_diplstate(game.player_ptr,
				    get_player(plrno))->type) {
      case DS_WAR:
      case DS_NO_CONTACT:
	gtk_widget_set_sensitive(players_war_command, FALSE);
	break;
      default:
	gtk_widget_set_sensitive(players_war_command,
				 can_client_issue_orders()
				 && game.info.player_idx != plrno);
      }
    } else {
      gtk_widget_set_sensitive(players_war_command, FALSE);
    }

    gtk_widget_set_sensitive(players_vision_command,
			     can_client_issue_orders()
			     && gives_shared_vision(game.player_ptr, plr));

    gtk_widget_set_sensitive(players_meet_command, can_meet_with_player(plr));
    gtk_widget_set_sensitive(players_int_command, can_intel_with_player(plr));
    return;
  }

  gtk_widget_set_sensitive(players_meet_command, FALSE);
  gtk_widget_set_sensitive(players_int_command, FALSE);
}

/**************************************************************************
...
**************************************************************************/
static void selection_callback(GtkTreeSelection *selection, gpointer data)
{
  update_players_menu();
}

/**************************************************************************
...
**************************************************************************/
static gboolean button_press_callback(GtkTreeView *view, GdkEventButton *ev)
{
  if (ev->type == GDK_2BUTTON_PRESS) {
    GtkTreePath *path;

    gtk_tree_view_get_cursor(view, &path, NULL);
    if (path) {
      GtkTreeIter it;
      gint id;
      struct player *plr;

      gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path);
      gtk_tree_path_free(path);

      gtk_tree_model_get(GTK_TREE_MODEL(store), &it,
	  ncolumns - 1, &id, -1);
      plr = get_player(id);

      if (ev->button == 1) {
	if (can_intel_with_player(plr)) {
	  popup_intel_dialog(plr);
	}
      } else {
	dsend_packet_diplomacy_init_meeting_req(&aconnection, id);
      }
    }
  }
  return FALSE;
}

/**************************************************************************
...
**************************************************************************/
static gint plrdlg_sort_func(GtkTreeModel *model,
			      GtkTreeIter *a, GtkTreeIter *b, gpointer data)
{
  GValue value = { 0, };
  struct player *player1;
  struct player *player2;
  gint n;

  n = GPOINTER_TO_INT(data);

  gtk_tree_model_get_value(model, a, num_player_dlg_columns + 2, &value);
  player1 = &game.players[g_value_get_int(&value)];
  g_value_unset(&value);
  
  gtk_tree_model_get_value(model, b, num_player_dlg_columns + 2, &value);
  player2 = &game.players[g_value_get_int(&value)];
  g_value_unset(&value);
  
  return player_dlg_columns[n].sort_func(player1, player2);
}

/**************************************************************************
...
**************************************************************************/
static void create_store(void)
{
  GType model_types[num_player_dlg_columns + 3];
  int i;

  for (i = 0; i < num_player_dlg_columns; i++) {
    switch (player_dlg_columns[i].type) {
    case COL_FLAG:
      model_types[i] = GDK_TYPE_PIXBUF;
      break;
    case COL_COLOR:
      model_types[i] = GDK_TYPE_PIXBUF;
      break;
    case COL_BOOLEAN:
      model_types[i] = G_TYPE_BOOLEAN;
      break;
    case COL_TEXT:
    case COL_RIGHT_TEXT:
      model_types[i] = G_TYPE_STRING;
      break;
    }
  }
  /* special (invisible rows) - Text style, weight and player id */
  model_types[i++] = G_TYPE_INT;
  model_types[i++] = G_TYPE_INT;
  model_types[i++] = G_TYPE_INT;
  
  ncolumns = i;
  store = gtk_list_store_newv(ncolumns, model_types);  
  model = GTK_TREE_MODEL(store);
  
  /* Set sort order */
  for (i = 0; i < num_player_dlg_columns; i++) {
    if (player_dlg_columns[i].sort_func != NULL) {
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), i,
	plrdlg_sort_func, GINT_TO_POINTER(i), NULL);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
static void toggle_view(GtkCheckMenuItem* item, gpointer data)
{
  struct player_dlg_column* pcol = data;

  pcol->show = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
  update_views();
}

/*************************************************************************
  Called whenever player toggles the 'Show/Dead Players' menu item
*************************************************************************/
static void toggle_dead_players(GtkCheckMenuItem* item, gpointer data)
{
  player_dlg_show_dead_players = 
    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
  update_players_dialog();
}

/**************************************************************************
...
**************************************************************************/
static GtkWidget* create_show_menu(void)
{
  int i;
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *item;    
  
  for (i = 1; i < num_player_dlg_columns; i++) {
    struct player_dlg_column *pcol;
    
    pcol = &player_dlg_columns[i];
    item = gtk_check_menu_item_new_with_label(pcol->title);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), pcol->show);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "toggled", G_CALLBACK(toggle_view), pcol);
  }
  
  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  
  item = gtk_check_menu_item_new_with_label(Q_("?show:Dead Players"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                 player_dlg_show_dead_players);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  g_signal_connect(item, "toggled", G_CALLBACK(toggle_dead_players), NULL);
  
  return menu;
}

/**************************************************************************
...
**************************************************************************/
void create_players_dialog(void)
{
  int i;
  GtkWidget *sep, *sw;
  GtkWidget *menubar, *menu, *item, *vbox;

  gui_dialog_new(&players_dialog_shell, GTK_NOTEBOOK(top_notebook), NULL);
  gui_dialog_set_title(players_dialog_shell, _("Players"));

  gui_dialog_add_button(players_dialog_shell,
      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  gui_dialog_set_default_size(players_dialog_shell, -1, 270);

  create_store();

  players_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(store);
  gtk_widget_set_name(players_list, "small font");

  players_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(players_list));
  g_signal_connect(players_selection, "changed",
      G_CALLBACK(selection_callback), NULL);
  g_signal_connect(players_list, "button_press_event",
      G_CALLBACK(button_press_callback), NULL);

  for (i = 0; i < num_player_dlg_columns; i++) {
    struct player_dlg_column *pcol;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    pcol = &player_dlg_columns[i];
    col = NULL;

    switch (pcol->type) {
    case COL_FLAG:
      renderer = gtk_cell_renderer_pixbuf_new();

      col = gtk_tree_view_column_new_with_attributes(pcol->title,
         renderer, "pixbuf", i, NULL);
      break;
    case COL_BOOLEAN:
      renderer = gtk_cell_renderer_toggle_new();

      col = gtk_tree_view_column_new_with_attributes(pcol->title, renderer,
        "active", i, NULL);
      break;
    case COL_COLOR:
      renderer = gtk_cell_renderer_pixbuf_new();

      col = gtk_tree_view_column_new_with_attributes(pcol->title, renderer,
             "pixbuf", i, NULL);
      break;
    case COL_TEXT:
      renderer = gtk_cell_renderer_text_new();
      g_object_set(renderer, "style-set", TRUE, "weight-set", TRUE, NULL);

      col = gtk_tree_view_column_new_with_attributes(pcol->title, renderer,
	  "text", i,
	  "style", num_player_dlg_columns,
	  "weight", num_player_dlg_columns + 1,
	  NULL);
      gtk_tree_view_column_set_sort_column_id(col, i);
      break;
    case COL_RIGHT_TEXT:
      renderer = gtk_cell_renderer_text_new();
      g_object_set(renderer, "style-set", TRUE, "weight-set", TRUE, NULL);

      col = gtk_tree_view_column_new_with_attributes(pcol->title, renderer,
	  "text", i,
	  "style", num_player_dlg_columns,
	  "weight", num_player_dlg_columns + 1,
	  NULL);
      gtk_tree_view_column_set_sort_column_id(col, i);

      if (pcol->type == COL_RIGHT_TEXT) {
	g_object_set(renderer, "xalign", 1.0, NULL);
	gtk_tree_view_column_set_alignment(col, 1.0);
      }
      break;
    }
    
    if (col) {
      gtk_tree_view_append_column(GTK_TREE_VIEW(players_list), col);
    }
  }

  gtk_tree_view_set_search_column(GTK_TREE_VIEW(players_list),
                                  player_dlg_default_sort_column());

  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		                 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_add(GTK_CONTAINER(sw), players_list);

  gtk_box_pack_start(GTK_BOX(players_dialog_shell->vbox), sw,
		     TRUE, TRUE, 0);

  players_int_command = gtk_button_new_with_mnemonic(_("_Intelligence"));
  g_signal_connect(players_int_command, "clicked",
      G_CALLBACK(players_intel_callback), NULL);
  gui_dialog_add_widget(players_dialog_shell, players_int_command);  
  gtk_size_group_add_widget(players_dialog_shell->gui_button, 
      players_int_command);

  players_meet_command = gtk_button_new_with_mnemonic(_("_Meet"));
  g_signal_connect(players_meet_command, "clicked",
      G_CALLBACK(players_meet_callback), NULL);
  gui_dialog_add_widget(players_dialog_shell, players_meet_command);
  gtk_size_group_add_widget(players_dialog_shell->gui_button, 
      players_meet_command);

  players_war_command = gtk_button_new_with_mnemonic(_("_Cancel Treaty"));
  g_signal_connect(players_war_command, "clicked",
      G_CALLBACK(players_war_callback), NULL);
  gui_dialog_add_widget(players_dialog_shell, players_war_command);  
  gtk_size_group_add_widget(players_dialog_shell->gui_button,
      players_war_command);

  players_vision_command  = gtk_button_new_with_mnemonic(_("_Withdraw vision"));
  g_signal_connect(players_vision_command, "clicked",
      G_CALLBACK(players_vision_callback), NULL);
  gui_dialog_add_widget(players_dialog_shell, players_vision_command);  
  gtk_size_group_add_widget(players_dialog_shell->gui_button,
     players_vision_command);

  players_sship_command = gtk_button_new_with_mnemonic(_("_Spaceship"));
  g_signal_connect(players_sship_command, "clicked",
      G_CALLBACK(players_sship_callback), NULL);
  gui_dialog_add_widget(players_dialog_shell, players_sship_command);
  gtk_size_group_add_widget(players_dialog_shell->gui_button,
      players_sship_command);

  vbox = gtk_vbox_new(FALSE, 0);
  
  sep = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

  menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), menubar, TRUE, TRUE, 0);

  gui_dialog_add_widget(players_dialog_shell, vbox);
  gtk_box_set_child_packing(GTK_BOX(players_dialog_shell->action_area), 
      vbox, FALSE, FALSE, 0, GTK_PACK_END);

  item = gtk_menu_item_new_with_mnemonic(_("S_how"));
  menu = create_show_menu();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);

  item = gtk_menu_item_new_with_mnemonic(_("_AI"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);

  menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);

  item = gtk_menu_item_new_with_mnemonic(_("_Toggle AI Mode"));
  g_signal_connect(item, "activate",
      G_CALLBACK(players_ai_toggle_callback), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  sep = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);

  for (i = 0; i < NUM_SKILL_LEVELS; i++) {
    item = gtk_menu_item_new_with_label(_(skill_level_names[i]));
    g_signal_connect(item, "activate",
	G_CALLBACK(players_ai_skill_callback), GUINT_TO_POINTER(i));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  }
  gtk_widget_show_all(menu);

  gui_dialog_show_all(players_dialog_shell);

  gtk_list_store_clear(store);
  update_players_dialog();

  gui_dialog_set_default_response(players_dialog_shell,
    GTK_RESPONSE_CLOSE);

  gtk_tree_view_focus(GTK_TREE_VIEW(players_list));
}


/**************************************************************************
...
**************************************************************************/
#define MIN_DIMENSION 5

/**************************************************************************
 Builds the flag pixmap.
**************************************************************************/
GdkPixbuf *get_flag(const struct nation_type *nation)
{
  int x0, y0, x1, y1, w, h;
  GdkPixbuf *im, *im2;
  struct sprite *flag;

  flag = get_nation_flag_sprite(tileset, nation);

  /* calculate the bounding box ... */
  sprite_get_bounding_box(flag, &x0, &y0, &x1, &y1);

  assert(x0 != -1);
  assert(y0 != -1);
  assert(x1 != -1);
  assert(y1 != -1);

  w = (x1 - x0) + 1;
  h = (y1 - y0) + 1;

  /* if the flag is smaller then 5 x 5, something is wrong */
  assert(w >= MIN_DIMENSION && h >= MIN_DIMENSION);

  /* croping */
  im = gdk_pixbuf_new_subpixbuf(sprite_get_pixbuf(flag), x0, y0, w, h);
  im2 = gdk_pixbuf_copy(im);
  g_object_unref(im);

  /* and finaly store the scaled flag pixbuf in the static flags array */
  return im2;
}


/**************************************************************************
 Builds the text for the cells of a row in the player report. If
 update is TRUE, only the changable entries are build.
**************************************************************************/
static void build_row(GtkTreeIter *it, int i)
{
  struct player *plr = get_player(i);
  GdkPixbuf *pixbuf;
  gint style = PANGO_STYLE_NORMAL, weight = PANGO_WEIGHT_NORMAL;
  int k;
  gchar *p;

  for (k = 0; k < num_player_dlg_columns; k++) {
    struct player_dlg_column* pcol = &player_dlg_columns[k];
    switch(pcol->type) {
      case COL_TEXT:
      case COL_RIGHT_TEXT:
        p = (gchar*)(pcol->func(plr));
	gtk_list_store_set(store, it, k, p, -1);
	break;
      case COL_FLAG:
        pixbuf = get_flag(plr->nation);
        gtk_list_store_set(store, it, k, pixbuf, -1);
        g_object_unref(pixbuf);
	break;
      case COL_COLOR:
	pixbuf = create_player_icon(plr);
        gtk_list_store_set(store, it, k, pixbuf, -1);
	g_object_unref(pixbuf);
	break;
      case COL_BOOLEAN:
        gtk_list_store_set(store, it, k, (gboolean)pcol->bool_func(plr), -1);
	break;
    }
  }

  /* The playerid */
  gtk_list_store_set(store, it,
    ncolumns - 1, (gint)i,
    -1);

   /* now add some eye candy ... */
  if (game.player_ptr) {
    switch (pplayer_get_diplstate(game.player_ptr, plr)->type) {
    case DS_WAR:
      weight = PANGO_WEIGHT_NORMAL;
      style = PANGO_STYLE_ITALIC;
      break;
    case DS_ALLIANCE:
    case DS_TEAM:
      weight = PANGO_WEIGHT_BOLD;
      style = PANGO_STYLE_NORMAL;
      break;
    case DS_ARMISTICE:
    case DS_CEASEFIRE:
    case DS_PEACE:
    case DS_NO_CONTACT:
      weight = PANGO_WEIGHT_NORMAL;
      style = PANGO_STYLE_NORMAL;
      break;
    case DS_LAST:
      assert(0);
      break;
    }
  }
   gtk_list_store_set(store, it,
       num_player_dlg_columns, style,
       num_player_dlg_columns + 1, weight,
       -1);
}

/**************************************************************************
...
**************************************************************************/
static bool player_should_be_shown(int plrno) {
  return is_valid_player_id(plrno)
	 && (player_dlg_show_dead_players
	     || game.players[plrno].is_alive)
	 && (!is_barbarian(&game.players[plrno]));
}

/**************************************************************************
...
**************************************************************************/
void update_players_dialog(void)
{
  if (players_dialog_shell && !is_plrdlg_frozen()) {
    gboolean exists[game.info.nplayers];
    gint i;
    GtkTreeIter it, it_next;

    for (i = 0; i < game.info.nplayers; i++) {
      exists[i] = FALSE;
    }

    if (gtk_tree_model_get_iter_first(model, &it)) {
      gint plrno;
      bool more;

      do {
	it_next = it;
	more = gtk_tree_model_iter_next(model, &it_next);

	gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

	/*
	 * The nation already had a row in the player report. In that
	 * case we just update the row. If player is dead we remove him
	 * if necessary.
	 */
	if (player_should_be_shown(plrno)) {
	  exists[plrno] = TRUE;
	  build_row(&it, plrno);
	} else {
	  gtk_list_store_remove(store, &it);
	}
	it = it_next;
      } while (more);
    }

    players_iterate(pplayer) {
      /* skip barbarians */
      if (player_should_be_shown(pplayer->player_no)) {
	if (!exists[pplayer->player_no]) {
	  /* 
	   * A nation is not in the player report yet. This happens when
	   * the report is just opened and after a split.
	   */
	  gtk_list_store_append(store, &it);

	  build_row(&it, pplayer->player_no);
	}
      }
    } players_iterate_end;

    update_players_menu();
    update_views();
  }
}

/**************************************************************************
  Callback for diplomatic meetings button. This button is enabled iff
  we can meet with the other player.
**************************************************************************/
void players_meet_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

    dsend_packet_diplomacy_init_meeting_req(&aconnection, plrno);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_war_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

    /* can be any pact clause */
    dsend_packet_diplomacy_cancel_pact(&aconnection, plrno,
				       CLAUSE_CEASEFIRE);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_vision_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);
    dsend_packet_diplomacy_cancel_pact(&aconnection, plrno, CLAUSE_VISION);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_intel_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

    if (can_intel_with_player(&game.players[plrno])) {
      popup_intel_dialog(&game.players[plrno]);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
void players_sship_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);
    popup_spaceship_dialog(&game.players[plrno]);
  }
}

/**************************************************************************
  AI toggle callback.
**************************************************************************/
static void players_ai_toggle_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;
    char buf[512];

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

    my_snprintf(buf, sizeof(buf), "/aitoggle \"%s\"", get_player(plrno)->name);
    send_chat(buf);
  }
}

/**************************************************************************
  AI skill level setting callback.
**************************************************************************/
static void players_ai_skill_callback(GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(players_selection, &model, &it)) {
    gint plrno;
    char buf[512];

    gtk_tree_model_get(model, &it, ncolumns - 1, &plrno, -1);

    my_snprintf(buf, sizeof(buf), "/%s %s",
	skill_level_names[GPOINTER_TO_UINT(data)],
	get_player(plrno)->name);
    send_chat(buf);
  }
}

/**************************************************************************
...
**************************************************************************/
static void update_views(void)
{
  int i;

  for (i = 0; i < num_player_dlg_columns; i++) {
    GtkTreeViewColumn *col;

    col = gtk_tree_view_get_column(GTK_TREE_VIEW(players_list), i);
    gtk_tree_view_column_set_visible(col, player_dlg_columns[i].show);
  }
};
