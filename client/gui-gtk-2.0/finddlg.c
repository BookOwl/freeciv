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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "fcintl.h"
#include "game.h"
#include "player.h"

#include "dialogs.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapview.h"

#include "finddlg.h"

static GtkWidget *find_dialog_shell;
static GtkWidget *find_view;

static void update_find_dialog(GtkListStore *store);

static void find_command_callback(GtkWidget *w, gint response_id);
static void find_destroy_callback(GtkWidget *w, gpointer data);
static void find_selection_callback(GtkTreeSelection *selection,
				    GtkTreeModel *model);

static int pos_x, pos_y;

/****************************************************************
popup the dialog 10% inside the main-window 
*****************************************************************/
void popup_find_dialog(void)
{
  GtkWidget         *shell, *label;
  GtkWidget         *sw;
  GtkListStore      *store;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  if (find_dialog_shell) {
    gtk_window_present(GTK_WINDOW(find_dialog_shell));
    return;
  }

  get_center_tile_mapcanvas(&pos_x, &pos_y);

  shell = gtk_dialog_new_with_buttons(_("Find City"),
  	GTK_WINDOW(toplevel),
	0,
	GTK_STOCK_CANCEL,
	GTK_RESPONSE_CANCEL,
	GTK_STOCK_FIND,
	GTK_RESPONSE_ACCEPT,
	NULL);
  find_dialog_shell = shell;
  gtk_dialog_set_default_response(GTK_DIALOG(shell),
	GTK_RESPONSE_ACCEPT);
  gtk_window_set_position(GTK_WINDOW(shell), GTK_WIN_POS_MOUSE);

  g_signal_connect(shell, "response",
		   G_CALLBACK(find_command_callback), NULL);
  g_signal_connect(shell, "destroy",
		   G_CALLBACK(find_destroy_callback), NULL);

  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
    0, GTK_SORT_ASCENDING);

  find_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(find_view));
  g_object_unref(store);
  gtk_tree_view_columns_autosize(GTK_TREE_VIEW(find_view));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(find_view), FALSE);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
  	"text", 0, NULL);
  gtk_tree_view_column_set_sort_order(column, GTK_SORT_ASCENDING);
  gtk_tree_view_append_column(GTK_TREE_VIEW(find_view), column);

  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                          GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(sw, -1, 200);
  gtk_container_add(GTK_CONTAINER(sw), find_view);

  label = g_object_new(GTK_TYPE_LABEL,
		       "use-underline", TRUE,
		       "mnemonic-widget", find_view,
		       "label", _("Ci_ties:"),
		       "xalign", 0.0, "yalign", 0.5, NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(shell)->vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(shell)->vbox), sw, TRUE, TRUE, 2);


  g_signal_connect(selection, "changed",
		   G_CALLBACK(find_selection_callback), store);

  update_find_dialog(store);
  gtk_tree_view_focus(GTK_TREE_VIEW(find_view));

  gtk_widget_show_all(GTK_DIALOG(shell)->vbox);
  gtk_window_present(GTK_WINDOW(shell));
}



/**************************************************************************
...
**************************************************************************/
static void update_find_dialog(GtkListStore *store)
{
  int i;
  GtkTreeIter it;

  gtk_list_store_clear(store);

  for(i = 0; i < game.nplayers; i++) {
    city_list_iterate(game.players[i].cities, pcity) {
	GValue value = { 0, };

	gtk_list_store_append(store, &it);

	g_value_init(&value, G_TYPE_STRING);
	g_value_set_static_string(&value, pcity->name);
	gtk_list_store_set_value(store, &it, 0, &value);
	g_value_unset(&value);

	gtk_list_store_set(store, &it, 1, pcity, -1);
    } city_list_iterate_end;
  }
}

/**************************************************************************
...
**************************************************************************/
static void find_command_callback(GtkWidget *w, gint response_id)
{
  if (response_id == GTK_RESPONSE_ACCEPT) {
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter it;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(find_view));

    if (gtk_tree_selection_get_selected(selection, &model, &it)) {
      struct city *pcity;

      gtk_tree_model_get(model, &it, 1, &pcity, -1);

      if (pcity) {
        pos_x = pcity->x;
        pos_y = pcity->y;
      }
    }
  }
  gtk_widget_destroy(find_dialog_shell);
}

/**************************************************************************
...
**************************************************************************/
static void find_destroy_callback(GtkWidget *w, gpointer data)
{
  center_tile_mapcanvas(pos_x, pos_y);
  find_dialog_shell = NULL;
}

/**************************************************************************
...
**************************************************************************/
static void find_selection_callback(GtkTreeSelection *selection,
				    GtkTreeModel *model)
{
  GtkTreeIter it;
  struct city *pcity;

  if (!gtk_tree_selection_get_selected(selection, NULL, &it))
    return;

  gtk_tree_model_get(model, &it, 1, &pcity, -1);

  if (pcity)
    center_tile_mapcanvas(pcity->x, pcity->y);
}
