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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "log.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "rand.h"
#include "support.h"

#include "chatline.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapview.h"
#include "options.h"
#include "tilespec.h"

#include "dialogs.h"

/******************************************************************/
static gint popup_mes_del_callback(GtkWidget *widget, GdkEvent *event,
				   gpointer data);

/******************************************************************/
static GtkWidget  *races_dialog_shell=NULL;
static GtkWidget  *races_toggles_form;
static GtkWidget  *races_sex_toggles_form;
static GtkWidget  *city_style_toggles_form;
static GtkWidget  *races_ok_command;            /* ok button */
static GtkWidget  *races_disc_command=NULL;     /* disc button */
static GtkWidget  *races_quit_command=NULL;     /* quit button */
static GtkWidget **races_toggles=NULL,          /* toggle race */
                  *races_sex_toggles[2],        /* Male/Female */
                 **city_style_toggles = NULL,
                  *races_name;                  /* leader name */
static GList      *leader_strings = NULL;
static GList      *sorted_races_list = NULL; /* contains a list of race
					      ids sorted by the race
					      name. Is valid as long
					      as the races_dialog is
					      poped up. */
/******************************************************************/
static GtkWidget  *spy_tech_shell;
static GtkWidget  *spy_advances_list;
static GtkWidget  *spy_steal_command;

static int         spy_tech_shell_is_modal;
static int         advance_type[A_LAST+1];
static int         steal_advance = 0;

/******************************************************************/
static GtkWidget  *spy_sabotage_shell;
static GtkWidget  *spy_improvements_list;
static GtkWidget  *spy_sabotage_command;

static int         spy_sabotage_shell_is_modal;
static int         improvement_type[B_LAST+1];
static int         sabotage_improvement = 0;

/******************************************************************/

#define MAX_SELECT_UNITS 100
static GtkWidget  *unit_select_dialog_shell;
static GtkWidget  *unit_select_commands[MAX_SELECT_UNITS];
static GtkWidget  *unit_select_labels[MAX_SELECT_UNITS];
static int         unit_select_ids[MAX_SELECT_UNITS];
static int         unit_select_no;

static int races_buttons_get_current(void);
static int sex_buttons_get_current(void);
static int city_style_get_current(void);

static void create_races_dialog	(void);
static void races_buttons_callback	( GtkWidget *w, gpointer data );
static void races_toggles_callback	( GtkWidget *w, gpointer data );
static void races_sex_toggles_callback ( GtkWidget *w, gpointer data );
static void races_name_callback	( GtkWidget *w, gpointer data );
static void city_style_toggles_callback ( GtkWidget *w, gpointer data );

static int selected_nation;
static int selected_leader;
static int selected_sex;
static int selected_city_style;
static int city_style_idx[64];        /* translation table basic style->city_style  */
static int city_style_ridx[64];       /* translation table the other way            */
                               /* they in fact limit the num of styles to 64 */
static int b_s_num; /* number of basic city styles, i.e. those that you can start with */

static int is_showing_government_dialog;

static int is_showing_pillage_dialog = FALSE;
static int unit_to_use_to_pillage;

static int caravan_city_id;
static int caravan_unit_id;

static int diplomat_dialog_open = 0;
static int diplomat_id;
static int diplomat_target_id;

static GtkWidget *caravan_dialog;

static int is_showing_unit_connect_dialog = FALSE;
static int unit_to_use_to_connect;
static int connect_unit_x;
static int connect_unit_y;

/****************************************************************
...
*****************************************************************/
static void notify_command_callback(GtkWidget *w, GtkWidget *t)
{
  gtk_widget_destroy( t );
  gtk_widget_set_sensitive( top_vbox, TRUE );
}

/****************************************************************
...
*****************************************************************/
gint deleted_callback(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  gtk_widget_set_sensitive( top_vbox, TRUE );
  return FALSE;
}


/****************************************************************
...
*****************************************************************/
void popup_notify_dialog(char *caption, char *headline, char *lines)
{
  GtkWidget *notify_dialog_shell, *notify_command;
  GtkWidget *notify_label, *notify_headline, *notify_scrolled;
  GtkAccelGroup *accel=gtk_accel_group_new();
  
  notify_dialog_shell = gtk_dialog_new();
  gtk_signal_connect( GTK_OBJECT(notify_dialog_shell),"delete_event",
	GTK_SIGNAL_FUNC(deleted_callback),NULL );
  gtk_accel_group_attach(accel, GTK_OBJECT(notify_dialog_shell));
  gtk_widget_set_name(notify_dialog_shell, "Freeciv");

  gtk_window_set_title( GTK_WINDOW( notify_dialog_shell ), caption );
  
  gtk_container_border_width( GTK_CONTAINER(GTK_DIALOG(notify_dialog_shell)->vbox), 5 );

  gtk_widget_realize (notify_dialog_shell);

  notify_headline = gtk_label_new( headline);   
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->vbox ),
	notify_headline, FALSE, FALSE, 0 );
  gtk_widget_set_name(notify_headline, "notify label");

  gtk_label_set_justify( GTK_LABEL( notify_headline ), GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment(GTK_MISC(notify_headline), 0.0, 0.0);

  notify_scrolled=gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(notify_scrolled),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  notify_label = gtk_label_new( lines );  
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW (notify_scrolled),
					notify_label);

  gtk_widget_set_name(notify_label, "notify label");
  gtk_label_set_justify( GTK_LABEL( notify_label ), GTK_JUSTIFY_LEFT );
  gtk_misc_set_alignment(GTK_MISC(notify_label), 0.0, 0.0);

  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->vbox ),
	notify_scrolled, TRUE, TRUE, 0 );


  notify_command = gtk_button_new_with_label( _("Close") );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->action_area ),
	notify_command, TRUE, TRUE, 0 );
  GTK_WIDGET_SET_FLAGS( notify_command, GTK_CAN_DEFAULT );
  gtk_widget_grab_default( notify_command );
  gtk_widget_add_accelerator(notify_command, "clicked",
	accel, GDK_Escape, 0, 0);

  gtk_signal_connect( GTK_OBJECT( notify_command ), "clicked",
	GTK_SIGNAL_FUNC( notify_command_callback ), notify_dialog_shell );

  
  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->vbox );
  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->action_area );

  gtk_widget_set_usize(notify_dialog_shell, 0, 265);
  gtk_set_relative_position (toplevel, notify_dialog_shell, 10, 10);
  gtk_widget_show( notify_dialog_shell );

  gtk_widget_set_sensitive( top_vbox, FALSE );
}

/****************************************************************
...
*****************************************************************/

/* surely this should use genlists??  --dwp */
struct widget_list {
  GtkWidget *w;
  int x,y;
  struct widget_list *next;
};
static struct widget_list *notify_goto_widget_list = NULL;

static void notify_goto_widget_remove(GtkWidget *w)
{
  struct widget_list *cur, *tmp;
  cur=notify_goto_widget_list;
  if (!cur)
    return;
  if (cur && cur->w == w) {
    cur = cur->next;
    free(notify_goto_widget_list);
    notify_goto_widget_list = cur;
    return;
  }
  for (; cur->next && cur->next->w!= w; cur=cur->next);
  if (cur->next) {
    tmp = cur->next;
    cur->next = cur->next->next;
    free(tmp);
  }
}

static void notify_goto_find_widget(GtkWidget *w, int *x, int *y)
{
  struct widget_list *cur;
  *x=0;
  *y=0;
  for (cur = notify_goto_widget_list; cur && cur->w !=w; cur = cur->next);
  if (cur) {
    *x = cur->x;
    *y = cur->y;
  }
}

static void notify_goto_add_widget_coords(GtkWidget *w, int  x, int y)
{
  struct widget_list *newwidget;
  newwidget = fc_malloc(sizeof(struct widget_list));
  newwidget->w = w;
  newwidget->x = x;
  newwidget->y = y;
  newwidget->next = notify_goto_widget_list;
  notify_goto_widget_list = newwidget;
}

static void notify_goto_command_callback(GtkWidget *w, gpointer data)
{
  int x,y;
  notify_goto_find_widget(w, &x, &y);
  center_tile_mapcanvas(x, y);
  notify_goto_widget_remove(w);

  gtk_widget_destroy(w->parent->parent->parent);
  gtk_widget_set_sensitive(top_vbox, TRUE);
}

static void notify_no_goto_command_callback(GtkWidget *w, gpointer data)
{
  notify_goto_widget_remove(w);
  gtk_widget_destroy(w->parent->parent->parent);
  gtk_widget_set_sensitive(top_vbox, TRUE);
}

static gint notify_deleted_callback(GtkWidget *widget, GdkEvent *event,
				    gpointer data)
{
  notify_goto_widget_remove(widget);
  gtk_widget_set_sensitive(top_vbox, TRUE);
  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void popup_notify_goto_dialog(char *headline, char *lines,int x, int y)
{
  GtkWidget *notify_dialog_shell, *notify_command, *notify_goto_command;
  GtkWidget *notify_label;
  
  if (!is_real_tile(x, y)) {
    popup_notify_dialog(_("Message:"), headline, lines);
    return;
  }
  notify_dialog_shell = gtk_dialog_new();
  gtk_signal_connect( GTK_OBJECT(notify_dialog_shell),"delete_event",
	GTK_SIGNAL_FUNC(notify_deleted_callback),NULL );

  gtk_window_set_title( GTK_WINDOW( notify_dialog_shell ), headline );

  notify_label=gtk_label_new(lines);
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->vbox ),
	notify_label, TRUE, TRUE, 0 );

  notify_command = gtk_button_new_with_label(_("Close"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->action_area ),
	notify_command, TRUE, TRUE, 0 );

  notify_goto_command = gtk_button_new_with_label(_("Goto and Close"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->action_area ),
	notify_goto_command, TRUE, TRUE, 0 );
  
  gtk_signal_connect(GTK_OBJECT(notify_command), "clicked",
		GTK_SIGNAL_FUNC(notify_no_goto_command_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(notify_goto_command), "clicked",
		GTK_SIGNAL_FUNC(notify_goto_command_callback), NULL);
  notify_goto_add_widget_coords(notify_goto_command, x, y);

  gtk_set_relative_position(toplevel, notify_dialog_shell, 25, 25);

  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->vbox );
  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->action_area );
  gtk_widget_show(notify_dialog_shell);

  gtk_widget_set_sensitive(top_vbox, FALSE);
}


/****************************************************************
...
*****************************************************************/
static void diplomat_bribe_yes_callback(GtkWidget *w, gpointer data)
{
  struct packet_diplomat_action req;

  req.action_type=DIPLOMAT_BRIBE;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;

  send_packet_diplomat_action(&aconnection, &req);
  
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_bribe_no_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
}



/****************************************************************
...  Ask the server how much the bribe is
*****************************************************************/
static void diplomat_bribe_callback(GtkWidget *w, gpointer data)
{
  struct packet_generic_integer packet;

  destroy_message_dialog(w);
  
  if(find_unit_by_id(diplomat_id) && 
     find_unit_by_id(diplomat_target_id)) { 
    packet.value = diplomat_target_id;
    send_packet_generic_integer(&aconnection, PACKET_INCITE_INQ, &packet);
   }

}
 
/****************************************************************
...
*****************************************************************/
void popup_bribe_dialog(struct unit *punit)
{
  char buf[128];
  
  if(game.player_ptr->economic.gold>=punit->bribe_cost) {
    my_snprintf(buf, sizeof(buf),
		_("Bribe unit for %d gold?\nTreasury contains %d gold."), 
		punit->bribe_cost, game.player_ptr->economic.gold);
    popup_message_dialog(top_vbox, /*"diplomatbribedialog"*/_("Bribe Enemy Unit"), buf,
			_("_Yes"), diplomat_bribe_yes_callback, 0,
			_("_No"), diplomat_bribe_no_callback, 0, 0);
  } else {
    my_snprintf(buf, sizeof(buf),
		_("Bribing the unit costs %d gold.\n"
		  "Treasury contains %d gold."), 
		punit->bribe_cost, game.player_ptr->economic.gold);
    popup_message_dialog(top_vbox, /*"diplomatnogolddialog"*/_("Traitors Demand Too Much!"), buf,
			_("Darn"), diplomat_bribe_no_callback, 0, 
			0);
  }
}

/****************************************************************
...
*****************************************************************/
static void diplomat_sabotage_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_SABOTAGE;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    req.value = -1;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_investigate_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) && 
     (find_city_by_id(diplomat_target_id))) { 
    struct packet_diplomat_action req;

    req.action_type=DIPLOMAT_INVESTIGATE;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_sabotage_unit_callback(GtkWidget *w, gpointer data)
{

  struct packet_diplomat_action req;
  
  req.action_type=SPY_SABOTAGE_UNIT;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;
  
  send_packet_diplomat_action(&aconnection, &req);
  
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_embassy_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) && 
     (find_city_by_id(diplomat_target_id))) { 
    struct packet_diplomat_action req;

    req.action_type=DIPLOMAT_EMBASSY;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_poison_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) &&
     (find_city_by_id(diplomat_target_id))) {
    struct packet_diplomat_action req;

    req.action_type=SPY_POISON;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_steal_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;

    req.action_type=DIPLOMAT_STEAL;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    req.value=0;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_close_tech_callback(GtkWidget *w, gpointer data)
{

  if(spy_tech_shell_is_modal)
     gtk_widget_set_sensitive(top_vbox, TRUE);
   gtk_widget_destroy(spy_tech_shell);
   spy_tech_shell=0;

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_close_sabotage_callback(GtkWidget *w, gpointer data)
{

  if(spy_sabotage_shell_is_modal)
     gtk_widget_set_sensitive(top_vbox, TRUE);
   gtk_widget_destroy(spy_sabotage_shell);
   spy_sabotage_shell=0;

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_select_tech_callback(GtkWidget *w, gint row, gint column)
{
  if (advance_type[row] != -1){
    steal_advance = advance_type[row];
    gtk_widget_set_sensitive(spy_steal_command, TRUE);
    return;
  }
}

/****************************************************************
...
*****************************************************************/
static void spy_uselect_tech_callback(GtkWidget *w, gint row, gint column)
{
  gtk_widget_set_sensitive(spy_steal_command, FALSE);
}

/****************************************************************
...
*****************************************************************/
static void spy_select_improvement_callback(GtkWidget *w, gint row,
					    gint column)
{
  sabotage_improvement = improvement_type[row];
  gtk_widget_set_sensitive(spy_sabotage_command, TRUE);
}

/****************************************************************
...
*****************************************************************/
static void spy_uselect_improvement_callback(GtkWidget *w, gint row,
					     gint column)
{
  gtk_widget_set_sensitive(spy_sabotage_command, FALSE);
}

/****************************************************************
...
*****************************************************************/
static void spy_steal_callback(GtkWidget *w, gpointer data)
{  
  gtk_widget_destroy(spy_tech_shell);
  spy_tech_shell = 0l;
  
  if(!steal_advance){
    freelog(LOG_ERROR, "Bug in spy steal tech code.");
    process_diplomat_arrival(NULL, 0);
    return;
  }
  
  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_STEAL;
    req.value=steal_advance;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void spy_sabotage_callback(GtkWidget *w, gpointer data)
{  
  gtk_widget_destroy(spy_sabotage_shell);
  spy_sabotage_shell = 0l;
  
  if(!sabotage_improvement){
    freelog(LOG_ERROR, "Bug in spy sabotage code");
    process_diplomat_arrival(NULL, 0);
    return;
  }
  
  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_SABOTAGE;
    req.value=sabotage_improvement+1;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static int create_advances_list(struct player *pplayer,
				struct player *pvictim, int make_modal)
{  
  GtkWidget *close_command, *scrolled;
  int i, j;
  static gchar *title_[1] = { N_("Select Advance to Steal") };
  static gchar **title;
  GtkAccelGroup *accel=gtk_accel_group_new();

  if (!title) title = intl_slist(1, title_);
  
  spy_tech_shell = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(spy_tech_shell),_("Steal Technology"));
  gtk_window_set_position (GTK_WINDOW(spy_tech_shell), GTK_WIN_POS_MOUSE);
  gtk_accel_group_attach(accel, GTK_OBJECT(spy_tech_shell));
  
  spy_advances_list = gtk_clist_new_with_titles(1, title);
  gtk_clist_column_titles_passive(GTK_CLIST(spy_advances_list));

  scrolled = gtk_scrolled_window_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(scrolled),spy_advances_list);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_set_usize( scrolled, 180, 250 );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_tech_shell)->vbox ),
	scrolled, TRUE, TRUE, 0 );
  gtk_clist_set_column_width(GTK_CLIST(spy_advances_list), 0,
        GTK_CLIST(spy_advances_list)->clist_window_width);

  close_command = gtk_button_new_with_label(_("Close"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_tech_shell)->action_area ),
	close_command, TRUE, TRUE, 0 );
  gtk_widget_add_accelerator(close_command, "clicked", accel, GDK_Escape, 0, 0);

  spy_steal_command = gtk_button_new_with_label(_("Steal"));
  gtk_widget_set_sensitive(spy_steal_command, FALSE);
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_tech_shell)->action_area ),
	spy_steal_command, TRUE, TRUE, 0 );


  gtk_signal_connect(GTK_OBJECT(spy_advances_list), "select_row",
	GTK_SIGNAL_FUNC(spy_select_tech_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(spy_advances_list), "unselect_row",
	GTK_SIGNAL_FUNC(spy_uselect_tech_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(close_command), "clicked",
	GTK_SIGNAL_FUNC(spy_close_tech_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(spy_steal_command), "clicked",
	GTK_SIGNAL_FUNC(spy_steal_callback), NULL);

  /* Now populate the list */
  gtk_clist_freeze(GTK_CLIST(spy_advances_list));

  j = 0;
  advance_type[j] = -1;

  if (pvictim) { /* you don't want to know what lag can do -- Syela */
    gchar *row[1];

    for(i=A_FIRST; i<game.num_tech_types; i++) {
      if(get_invention(pvictim, i)==TECH_KNOWN && 
	 (get_invention(pplayer, i)==TECH_UNKNOWN || 
	  get_invention(pplayer, i)==TECH_REACHABLE)) {

	row[0] = advances[i].name;
	gtk_clist_append(GTK_CLIST(spy_advances_list), row);
        advance_type[j++] = i;
      }
    }

    if(j > 0) {
      row[0] = _("At Spy's Discretion");
      gtk_clist_append(GTK_CLIST(spy_advances_list), row);
      advance_type[j++] = game.num_tech_types;
    }
  }

  if(j == 0) {
    static gchar *row_[1] = { N_("NONE") };
    static gchar **row;
    
    if (!row) row = intl_slist(1, row_);
  
    gtk_clist_append(GTK_CLIST(spy_advances_list), row);
    j++;
  }
  gtk_clist_thaw(GTK_CLIST(spy_advances_list));

  gtk_widget_set_sensitive(spy_steal_command, FALSE);
  
  gtk_widget_show_all(GTK_DIALOG(spy_tech_shell)->vbox);
  gtk_widget_show_all(GTK_DIALOG(spy_tech_shell)->action_area);
  return j;
}

/****************************************************************
...
*****************************************************************/
static int create_improvements_list(struct player *pplayer,
				    struct city *pcity, int make_modal)
{  
  GtkWidget *close_command, *scrolled;
  int i, j;
  gchar *row[1];
  static gchar *title_[1] = { N_("Select Improvement to Sabotage") };
  static gchar **title;
  GtkAccelGroup *accel=gtk_accel_group_new();

  if (!title) title = intl_slist(1, title_);
  
  spy_sabotage_shell = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(spy_sabotage_shell),_("Sabotage Improvements"));
  gtk_window_set_position (GTK_WINDOW(spy_sabotage_shell), GTK_WIN_POS_MOUSE);
  gtk_accel_group_attach(accel, GTK_OBJECT(spy_sabotage_shell));
  
  spy_improvements_list = gtk_clist_new_with_titles(1, title);
  gtk_clist_column_titles_passive(GTK_CLIST(spy_improvements_list));
  scrolled = gtk_scrolled_window_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(scrolled), spy_improvements_list);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_set_usize( scrolled, 180, 250 );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_sabotage_shell)->vbox ),
	scrolled, TRUE, TRUE, 0 );
  gtk_clist_set_column_width(GTK_CLIST(spy_improvements_list), 0,
        GTK_CLIST(spy_improvements_list)->clist_window_width);

  close_command = gtk_button_new_with_label(_("Close"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_sabotage_shell)->action_area ),
	close_command, TRUE, TRUE, 0 );
  gtk_widget_add_accelerator(close_command, "clicked", accel, GDK_Escape, 0, 0);
  
  spy_sabotage_command = gtk_button_new_with_label(_("Sabotage"));
  gtk_widget_set_sensitive(spy_sabotage_command, FALSE);
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(spy_sabotage_shell)->action_area ),
	spy_sabotage_command, TRUE, TRUE, 0 );
  

  gtk_signal_connect(GTK_OBJECT(spy_improvements_list), "select_row",
	GTK_SIGNAL_FUNC(spy_select_improvement_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(spy_improvements_list), "unselect_row",
	GTK_SIGNAL_FUNC(spy_uselect_improvement_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(close_command), "clicked",
	GTK_SIGNAL_FUNC(spy_close_sabotage_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(spy_sabotage_command), "clicked",
	GTK_SIGNAL_FUNC(spy_sabotage_callback), NULL);

  /* Now populate the list */
  gtk_clist_freeze(GTK_CLIST(spy_improvements_list));

  j = 0;
  row[0] = _("City Production");
  gtk_clist_append(GTK_CLIST(spy_improvements_list), row);
  improvement_type[j++] = -1;

  for(i=0; i<game.num_impr_types; i++) {
    if(i != B_PALACE && pcity->improvements[i] && !is_wonder(i)) {
      row[0] = get_impr_name_ex(pcity, i);
      gtk_clist_append(GTK_CLIST(spy_improvements_list), row);
      improvement_type[j++] = i;
    }  
  }

  if(j > 1) {
    row[0] = _("At Spy's Discretion");
    gtk_clist_append(GTK_CLIST(spy_improvements_list), row);
    improvement_type[j++] = B_LAST;
  } else {
    improvement_type[0] = B_LAST; /* fake "discretion", since must be production */
  }

  gtk_clist_thaw(GTK_CLIST(spy_improvements_list));

  gtk_widget_set_sensitive(spy_sabotage_command, FALSE);

  gtk_widget_show_all(GTK_DIALOG(spy_sabotage_shell)->vbox);
  gtk_widget_show_all(GTK_DIALOG(spy_sabotage_shell)->action_area);
  return j;
}

/****************************************************************
...
*****************************************************************/
static void spy_steal_popup(GtkWidget *w, gpointer data)
{
  struct city *pvcity = find_city_by_id(diplomat_target_id);
  struct player *pvictim = NULL;

  if(pvcity)
    pvictim = city_owner(pvcity);

/* it is concievable that pvcity will not be found, because something
has happened to the city during latency.  Therefore we must initialize
pvictim to NULL and account for !pvictim in create_advances_list. -- Syela */
  
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(!spy_tech_shell){
    spy_tech_shell_is_modal=1;

    create_advances_list(game.player_ptr, pvictim, spy_tech_shell_is_modal);
    gtk_set_relative_position (toplevel, spy_tech_shell, 10, 10);

    gtk_widget_show(spy_tech_shell);
  }
}

/****************************************************************
 Requests up-to-date list of improvements, the return of
 which will trigger the popup_sabotage_dialog() function.
*****************************************************************/
static void spy_request_sabotage_list(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if(find_unit_by_id(diplomat_id) &&
     (find_city_by_id(diplomat_target_id))) {
    struct packet_diplomat_action req;

    req.action_type = SPY_GET_SABOTAGE_LIST;
    req.diplomat_id = diplomat_id;
    req.target_id = diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Pops-up the Spy sabotage dialog, upon return of list of
 available improvements requested by the above function.
*****************************************************************/
void popup_sabotage_dialog(struct city *pcity)
{
  if(!spy_sabotage_shell){
    spy_sabotage_shell_is_modal=1;

    create_improvements_list(game.player_ptr, pcity, spy_sabotage_shell_is_modal);
    gtk_set_relative_position (toplevel, spy_sabotage_shell, 10, 10);

    gtk_widget_show(spy_sabotage_shell);
  }
}

/****************************************************************
...
*****************************************************************/
static void diplomat_incite_yes_callback(GtkWidget *w, gpointer data)
{
  struct packet_diplomat_action req;

  req.action_type=DIPLOMAT_INCITE;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;

  send_packet_diplomat_action(&aconnection, &req);
  
  destroy_message_dialog(w);

  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_incite_no_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);

  process_diplomat_arrival(NULL, 0);
}


/****************************************************************
...  Ask the server how much the revolt is going to cost us
*****************************************************************/
static void diplomat_incite_callback(GtkWidget *w, gpointer data)
{
  struct city *pcity;
  struct packet_generic_integer packet;

  destroy_message_dialog(w);
  diplomat_dialog_open=0;
  
  if(find_unit_by_id(diplomat_id) && 
     (pcity=find_city_by_id(diplomat_target_id))) { 
    packet.value = diplomat_target_id;
    send_packet_generic_integer(&aconnection, PACKET_INCITE_INQ, &packet);
  }
}

/****************************************************************
Popup the yes/no dialog for inciting, since we know the cost now
*****************************************************************/
void popup_incite_dialog(struct city *pcity)
{
  char buf[128];

  if (pcity->incite_revolt_cost == INCITE_IMPOSSIBLE_COST) {
    my_snprintf(buf, sizeof(buf), _("You can't incite a revolt in %s."),
		pcity->name);
    popup_message_dialog(top_vbox, _("City can't be incited!"), buf,
			 _("Darn"), diplomat_incite_no_callback, 0, 0);
  } else if (game.player_ptr->economic.gold >= pcity->incite_revolt_cost) {
    my_snprintf(buf, sizeof(buf),
		_("Incite a revolt for %d gold?\nTreasury contains %d gold."), 
		pcity->incite_revolt_cost, game.player_ptr->economic.gold);
   diplomat_target_id = pcity->id;
   popup_message_dialog(top_vbox, /*"diplomatrevoltdialog"*/_("Incite a Revolt!"), buf,
		       _("_Yes"), diplomat_incite_yes_callback, 0,
		       _("_No"), diplomat_incite_no_callback, 0, 0);
  } else {
    my_snprintf(buf, sizeof(buf),
		_("Inciting a revolt costs %d gold.\n"
		  "Treasury contains %d gold."), 
		pcity->incite_revolt_cost, game.player_ptr->economic.gold);
   popup_message_dialog(top_vbox, /*"diplomatnogolddialog"*/_("Traitors Demand Too Much!"), buf,
		       _("Darn"), diplomat_incite_no_callback, 0, 
		       0);
  }
}


/****************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering allied city.)
*****************************************************************/
static void diplomat_keep_moving_callback(GtkWidget *w, gpointer data)
{
  struct unit *punit;
  struct city *pcity;
  
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  if( (punit=find_unit_by_id(diplomat_id))
      && (pcity=find_city_by_id(diplomat_target_id))
      && !same_pos(punit->x, punit->y, pcity->x, pcity->y)) {
    struct packet_diplomat_action req;
    req.action_type = DIPLOMAT_MOVE;
    req.diplomat_id = diplomat_id;
    req.target_id = diplomat_target_id;
    send_packet_diplomat_action(&aconnection, &req);
  }
  process_diplomat_arrival(NULL, 0);
}

/****************************************************************
...
*****************************************************************/
static void diplomat_cancel_callback(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
  diplomat_dialog_open=0;

  process_diplomat_arrival(NULL, 0);
}


/****************************************************************
...
*****************************************************************/
void popup_diplomat_dialog(struct unit *punit, int dest_x, int dest_y)
{
  struct city *pcity;
  struct unit *ptunit;
  GtkWidget *shl;
  char buf[128];

  diplomat_id=punit->id;

  if((pcity=map_get_city(dest_x, dest_y))){
    /* Spy/Diplomat acting against a city */

    diplomat_target_id=pcity->id;
    my_snprintf(buf, sizeof(buf),
		_("Your %s has arrived at %s.\nWhat is your command?"),
		unit_name(punit->type), pcity->name);

    if(!unit_flag(punit, F_SPY)){
      shl=popup_message_dialog(top_vbox, /*"diplomatdialog"*/
			       _(" Choose Your Diplomat's Strategy"), buf,
         		     _("Establish _Embassy"), diplomat_embassy_callback, 0,
         		     _("_Investigate City"), diplomat_investigate_callback, 0,
         		     _("_Sabotage City"), diplomat_sabotage_callback, 0,
         		     _("Steal _Technology"), diplomat_steal_callback, 0,
         		     _("Incite a _Revolt"), diplomat_incite_callback, 0,
         		     _("_Keep moving"), diplomat_keep_moving_callback, 0,
         		     _("_Cancel"), diplomat_cancel_callback, 0,
         		     0);
      
      if(!diplomat_can_do_action(punit, DIPLOMAT_EMBASSY, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button0",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_INVESTIGATE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button1",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_SABOTAGE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button2",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_STEAL, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button3",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_INCITE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button4",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_MOVE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button5",FALSE);
    }else{
       shl=popup_message_dialog(top_vbox, /*"spydialog"*/
				_("Choose Your Spy's Strategy"), buf,
 			      _("Establish _Embassy"), diplomat_embassy_callback, 0,
 			      _("_Investigate City (free)"), diplomat_investigate_callback, 0,
 			      _("_Poison City"), spy_poison_callback,0,
 			      _("Industrial _Sabotage"), spy_request_sabotage_list, 0,
 			      _("Steal _Technology"), spy_steal_popup, 0,
 			      _("Incite a _Revolt"), diplomat_incite_callback, 0,
 			      _("_Keep moving"), diplomat_keep_moving_callback, 0,
 			      _("_Cancel"), diplomat_cancel_callback, 0,
 			      0);
 
      if(!diplomat_can_do_action(punit, DIPLOMAT_EMBASSY, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button0",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_INVESTIGATE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button1",FALSE);
      if(!diplomat_can_do_action(punit, SPY_POISON, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button2",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_SABOTAGE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button3",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_STEAL, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button4",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_INCITE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button5",FALSE);
      if(!diplomat_can_do_action(punit, DIPLOMAT_MOVE, dest_x, dest_y))
       message_dialog_button_set_sensitive(shl,"button6",FALSE);
     }

    diplomat_dialog_open=1;
   }else{ 
     if((ptunit=unit_list_get(&map_get_tile(dest_x, dest_y)->units, 0))){
       /* Spy/Diplomat acting against a unit */ 
       
       diplomat_target_id=ptunit->id;
 
       shl=popup_message_dialog(top_vbox, /*"spybribedialog"*/_("Subvert Enemy Unit"),
                              (!unit_flag(punit, F_SPY))?
 			      _("Sir, the diplomat is waiting for your command"):
 			      _("Sir, the spy is waiting for your command"),
 			      _("_Bribe Enemy Unit"), diplomat_bribe_callback, 0,
 			      _("_Sabotage Enemy Unit"), spy_sabotage_unit_callback, 0,
 			      _("_Cancel"), diplomat_cancel_callback, 0,
 			      0);
        
       if(!diplomat_can_do_action(punit, DIPLOMAT_BRIBE, dest_x, dest_y))
        message_dialog_button_set_sensitive(shl,"button0",FALSE);
       if(!diplomat_can_do_action(punit, SPY_SABOTAGE_UNIT, dest_x, dest_y))
        message_dialog_button_set_sensitive(shl,"button1",FALSE);
    }
  }
}

/****************************************************************
...
*****************************************************************/
int diplomat_dialog_is_open(void)
{
  return diplomat_dialog_open;
}

/****************************************************************
...
*****************************************************************/
static void caravan_establish_trade_callback(GtkWidget *w, gpointer data)
{
  struct packet_unit_request req;
  req.unit_id=caravan_unit_id;
  req.city_id=caravan_city_id;
  req.name[0]='\0';
  send_packet_unit_request(&aconnection, &req, PACKET_UNIT_ESTABLISH_TRADE);
    
  destroy_message_dialog(w);
  caravan_dialog = 0;
  process_caravan_arrival(NULL);
}


/****************************************************************
...
*****************************************************************/
static void caravan_help_build_wonder_callback(GtkWidget *w, gpointer data)
{
  struct packet_unit_request req;
  req.unit_id=caravan_unit_id;
  req.city_id=caravan_city_id;
  req.name[0]='\0';
  send_packet_unit_request(&aconnection, &req, PACKET_UNIT_HELP_BUILD_WONDER);

  destroy_message_dialog(w);
  caravan_dialog = 0;
  process_caravan_arrival(NULL);
}


/****************************************************************
...
*****************************************************************/
static void caravan_keep_moving_callback(GtkWidget *w, gpointer data)
{
#if 0   /* Now don't want to move at all in this case --dwp */
  struct unit *punit;
  struct city *pcity;
  
  if((punit=find_unit_by_id(caravan_unit_id)) && 
     (pcity=find_city_by_id(caravan_city_id))) {
    struct unit req_unit;

    req_unit=*punit;
    req_unit.x=pcity->x;
    req_unit.y=pcity->y;
    send_unit_info(&req_unit);
  }
#endif
  
  destroy_message_dialog(w);
  caravan_dialog = 0;
  process_caravan_arrival(NULL);
}



/****************************************************************
...
*****************************************************************/
void popup_caravan_dialog(struct unit *punit,
			  struct city *phomecity, struct city *pdestcity)
{
  GtkWidget *b;
  char buf[128];
  
  my_snprintf(buf, sizeof(buf),
	      _("Your caravan from %s reaches the city of %s.\nWhat now?"),
	      phomecity->name, pdestcity->name);
  
  caravan_city_id=pdestcity->id; /* callbacks need these */
  caravan_unit_id=punit->id;
  
  caravan_dialog=popup_message_dialog(top_vbox,
			   /*"caravandialog"*/_("Your Caravan Has Arrived"), 
			   buf,
			   _("Establish _Traderoute"),caravan_establish_trade_callback, 0,
			   _("Help build _Wonder"),caravan_help_build_wonder_callback, 0,
			   _("_Keep moving"),caravan_keep_moving_callback, 0,
			   0);
  
  if(!can_establish_trade_route(phomecity, pdestcity))
  {
    b = gtk_object_get_data( GTK_OBJECT( caravan_dialog ), "button0" );
    gtk_widget_set_sensitive( b, FALSE );
  }
  
  if(!unit_can_help_build_wonder(punit, pdestcity))
  {
    b = gtk_object_get_data( GTK_OBJECT( caravan_dialog ), "button1" );
    gtk_widget_set_sensitive( b, FALSE );
  }
}

/****************************************************************
...
*****************************************************************/
int caravan_dialog_is_open(void)
{
  return BOOL_VAL(caravan_dialog);
}


/****************************************************************
...
*****************************************************************/
static void government_callback(GtkWidget *w, gpointer data)
{
  struct packet_player_request packet;

  packet.government=(size_t)data;
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_GOVERNMENT);

  destroy_message_dialog(w);
  is_showing_government_dialog=0;
}


/****************************************************************
...
*****************************************************************/
void popup_government_dialog(void)
{
  int i;
  GtkWidget *dshell, *button, *dlabel, *vbox;

  if(!is_showing_government_dialog) {
    is_showing_government_dialog=1;
  
    gtk_widget_set_sensitive(top_vbox, FALSE);
  
    dshell=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position (GTK_WINDOW(dshell), GTK_WIN_POS_MOUSE);

    gtk_signal_connect(
      GTK_OBJECT(dshell),
      "delete_event",
      GTK_SIGNAL_FUNC(gtk_true),
      (gpointer) toplevel
    );

    gtk_window_set_title(GTK_WINDOW(dshell), _("Choose Your New Government"));
#if 0
    gtk_widget_realize(dshell);
    gdk_window_set_decorations(dshell->window, GDK_DECOR_BORDER);
#endif

    vbox = gtk_vbox_new(0,TRUE);
    gtk_container_add(GTK_CONTAINER(dshell),vbox);

    gtk_container_border_width(GTK_CONTAINER(vbox),5);

    dlabel = gtk_label_new(_("Select government type:"));
    gtk_box_pack_start(GTK_BOX(vbox), dlabel, TRUE, FALSE, 0);

    gtk_object_set_data(GTK_OBJECT(vbox), "parent",(gpointer) top_vbox);

    for (i=0; i<game.government_count; ++i) {
      struct government *g = &governments[i];

      if (i == game.government_when_anarchy) continue;
      button=gtk_button_new_with_label(g->name);
      gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, FALSE, 0);
      gtk_signal_connect(
        GTK_OBJECT(button),
        "clicked",
        GTK_SIGNAL_FUNC(government_callback),
        (gpointer) g->index
      );
      if(!can_change_to_government(game.player_ptr, i))
	gtk_widget_set_sensitive(button, FALSE);
    }
 
    gtk_widget_show_all( vbox );
    gtk_widget_show(dshell);  
  }
}



/****************************************************************
...
*****************************************************************/
static void revolution_callback_yes(GtkWidget *w, gpointer data)
{
  struct packet_player_request packet;

  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_REVOLUTION);
  
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
static void revolution_callback_no(GtkWidget *w, gpointer data)
{
  destroy_message_dialog(w);
}



/****************************************************************
...
*****************************************************************/
void popup_revolution_dialog(void)
{
  popup_message_dialog(top_vbox, /*"revolutiondialog"*/_("Revolution!"), 
		       _("You say you wanna revolution?"),
		       _("_Yes"),revolution_callback_yes, 0,
		       _("_No"),revolution_callback_no, 0, 
		       0);
}


/****************************************************************
...
*****************************************************************/
static void pillage_callback(GtkWidget *w, gpointer data)
{
  if (!is_showing_pillage_dialog) {
    destroy_message_dialog (w);
    return;
  }

  if (data) {
    struct unit *punit = find_unit_by_id (unit_to_use_to_pillage);
    if (punit) {
      request_new_unit_activity_targeted (punit,
					  ACTIVITY_PILLAGE,
					  (int)data);
    }
  }

  destroy_message_dialog (w);
  is_showing_pillage_dialog = FALSE;
}

/****************************************************************
...
*****************************************************************/
static gint pillage_del_callback(GtkWidget *widget, GdkEvent *event,
				 gpointer data)
{
  pillage_callback((GtkWidget *)data, NULL);
  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void popup_pillage_dialog(struct unit *punit, int may_pillage)
{
  GtkWidget *dshell, *button, *dlabel, *vbox;
  int what;

  if (!is_showing_pillage_dialog) {
    is_showing_pillage_dialog = TRUE;
    unit_to_use_to_pillage = punit->id;

    gtk_widget_set_sensitive (top_vbox, FALSE);

    dshell = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position (GTK_WINDOW(dshell), GTK_WIN_POS_MOUSE);
    gtk_window_set_title (GTK_WINDOW(dshell), _("What To Pillage"));

    vbox = gtk_vbox_new (0,TRUE);
    gtk_container_add (GTK_CONTAINER (dshell), vbox);

    gtk_container_border_width (GTK_CONTAINER (vbox), 5);

    dlabel = gtk_label_new (_("Select what to pillage:"));
    gtk_box_pack_start (GTK_BOX (vbox), dlabel, TRUE, FALSE, 0);

    gtk_object_set_data (GTK_OBJECT (vbox), "parent", (gpointer)top_vbox);

    while (may_pillage) {
      what = get_preferred_pillage (may_pillage);
      button = gtk_button_new_with_label (map_get_infrastructure_text (what));
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, FALSE, 0);
      gtk_signal_connect (
        GTK_OBJECT (button),
        "clicked",
        GTK_SIGNAL_FUNC (pillage_callback),
        (gpointer)what
      );
      may_pillage &= (~(what | map_get_infrastructure_prerequisite (what)));
    }
    button = gtk_button_new_with_label (_("Cancel"));
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, FALSE, 0);
    gtk_signal_connect (
      GTK_OBJECT (button),
      "clicked",
      GTK_SIGNAL_FUNC (pillage_callback),
      NULL
    );

    gtk_signal_connect (
      GTK_OBJECT (dshell),
      "delete_event",
      GTK_SIGNAL_FUNC (pillage_del_callback),
      (gpointer)button
    );

    gtk_widget_show_all (vbox);
    gtk_widget_show (dshell);
  }
}

/****************************************************************
handle buttons in unit connect dialog
*****************************************************************/
static void unit_connect_callback (GtkWidget *w, gpointer data)
{
  struct unit *punit;
  int activity = GPOINTER_TO_INT(data);

  if (!is_showing_unit_connect_dialog) {
    destroy_message_dialog(w);
    return;
  }

  punit = find_unit_by_id(unit_to_use_to_connect);

  if (punit) {
    if (activity != ACTIVITY_IDLE) {
      struct packet_unit_connect req;
      req.activity_type = activity;
      req.unit_id = punit->id;
      req.dest_x = connect_unit_x;
      req.dest_y = connect_unit_y;
      send_packet_unit_connect(&aconnection, &req);
    }
    else {
      update_unit_info_label(punit);
    }
  }

  destroy_message_dialog(w);
  is_showing_unit_connect_dialog = FALSE;
}

/****************************************************************
...
*****************************************************************/
static gint unit_connect_del_callback(GtkWidget *widget, GdkEvent *event,
				      gpointer data)
{
  unit_connect_callback((GtkWidget *)data, (gpointer)ACTIVITY_IDLE);
  return FALSE;
}

/****************************************************************
popup dialog which prompts for activity type (unit connect)
*****************************************************************/
void popup_unit_connect_dialog(struct unit *punit, int dest_x, int dest_y)
{
  GtkWidget *dshell, *button, *dlabel, *vbox;
  int activity;

  if (is_showing_unit_connect_dialog) 
    return;

  is_showing_unit_connect_dialog = TRUE;
  unit_to_use_to_connect = punit->id;
  connect_unit_x = dest_x;
  connect_unit_y = dest_y;

  gtk_widget_set_sensitive (top_vbox, FALSE);

  dshell = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW(dshell), GTK_WIN_POS_MOUSE);

  gtk_window_set_title (GTK_WINDOW(dshell), _("Connect"));

  vbox = gtk_vbox_new (0,TRUE);
  gtk_container_add (GTK_CONTAINER (dshell), vbox);

  gtk_container_border_width (GTK_CONTAINER (vbox), 5);

  dlabel = gtk_label_new (_("Choose unit activity:"));
  gtk_box_pack_start (GTK_BOX (vbox), dlabel, TRUE, FALSE, 0);

  gtk_object_set_data (GTK_OBJECT (vbox), "parent", (gpointer)top_vbox);

  for (activity = ACTIVITY_IDLE + 1; activity < ACTIVITY_LAST; activity++) {
    if (! can_unit_do_connect (punit, activity)) continue;

    button = gtk_button_new_with_label (get_activity_text (activity));
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, FALSE, 0);
    gtk_signal_connect (
      GTK_OBJECT (button),
      "clicked",
      GTK_SIGNAL_FUNC (unit_connect_callback),
      (gpointer)activity
      );
  }
  button = gtk_button_new_with_label (_("Cancel"));
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, FALSE, 0);
  gtk_signal_connect (
    GTK_OBJECT (button),
    "clicked",
    GTK_SIGNAL_FUNC (unit_connect_callback),
    (gpointer)ACTIVITY_IDLE
    );

  gtk_signal_connect (
    GTK_OBJECT (dshell),
    "delete_event",
    GTK_SIGNAL_FUNC (unit_connect_del_callback),
    (gpointer)button
  );

  gtk_widget_show_all (vbox);
  gtk_widget_show (dshell);
}

/****************************************************************
...
*****************************************************************/
static gint popup_mes_del_callback(GtkWidget *widget, GdkEvent *event,
				   gpointer data)
{
  gtk_widget_set_sensitive( (GtkWidget *)data, TRUE );
  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void message_dialog_button_set_sensitive(GtkWidget *shl, char *bname,int state)
{
  GtkWidget *b;

  b = gtk_object_get_data( GTK_OBJECT( shl ), bname );
  gtk_widget_set_sensitive( b, state );
}

/****************************************************************
...
*****************************************************************/
GtkWidget *popup_message_dialog(GtkWidget *parent, char *dialogname,
				char *text, ...)
{
  va_list args;
  GtkWidget *dshell, *button, *dlabel, *vbox;
  GtkAccelGroup *accel = gtk_accel_group_new();
  void (*fcb)(GtkWidget *, gpointer);
  gpointer data;
  char *name;
  char button_name[512];
  int i;

  gtk_widget_set_sensitive(parent, FALSE);
  
  dshell=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW(dshell), GTK_WIN_POS_MOUSE);
  gtk_accel_group_attach(accel, GTK_OBJECT(dshell));

  gtk_signal_connect( GTK_OBJECT(dshell),"delete_event",
	GTK_SIGNAL_FUNC(popup_mes_del_callback),(gpointer)parent );
  gtk_window_set_title( GTK_WINDOW(dshell), dialogname );

  vbox = gtk_vbox_new(0,TRUE);
  gtk_container_add(GTK_CONTAINER(dshell),vbox);

  gtk_container_border_width(GTK_CONTAINER(vbox),5);

  dlabel = gtk_label_new(text);
  gtk_box_pack_start( GTK_BOX( vbox ), dlabel, TRUE, FALSE, 0 );

  gtk_object_set_data(GTK_OBJECT(vbox), "parent",(gpointer)parent);

  i=0;
  va_start(args, text);
  
  while((name=va_arg(args, char *))) {
    fcb=va_arg(args, void *);
    data=va_arg(args, gpointer);
    my_snprintf(button_name, sizeof(button_name), "button%d", i++);
    
    button=gtk_accelbutton_new(name, accel);
    gtk_box_pack_start( GTK_BOX( vbox ), button, TRUE, FALSE, 0 );
    
    gtk_object_set_data( GTK_OBJECT( dshell ), button_name, button );

    gtk_signal_connect(GTK_OBJECT(button),"clicked",GTK_SIGNAL_FUNC(fcb),data );

    if (i==1)
      gtk_widget_grab_focus(button);
  }
  
  va_end(args);

  gtk_widget_show_all( vbox );

  gtk_widget_show(dshell);  

  return dshell;
}

/****************************************************************
...
*****************************************************************/
void destroy_message_dialog(GtkWidget *button)
{
  GtkWidget *parent;

  parent = gtk_object_get_data(GTK_OBJECT(button->parent),"parent");
  gtk_widget_set_sensitive(parent, TRUE);

  gtk_widget_destroy(button->parent->parent);
}


/**************************************************************************
...
**************************************************************************/
static void unit_select_all_callback(GtkWidget *w, gpointer data)
{
  int i;

  gtk_widget_set_sensitive(top_vbox, TRUE);
  gtk_widget_destroy(unit_select_dialog_shell);
  unit_select_dialog_shell=0;
  
  for(i=0; i<unit_select_no; i++) {
    struct unit *punit = player_find_unit_by_id(game.player_ptr,
						unit_select_ids[i]);
    if(punit) {
      request_new_unit_activity(punit, ACTIVITY_IDLE);
      set_unit_focus(punit);
    }
  }
}

/**************************************************************************
...
**************************************************************************/
static void unit_select_callback(GtkWidget *w, int id)
{
  struct unit *punit = player_find_unit_by_id(game.player_ptr, id);

  if (punit) {
    request_new_unit_activity(punit, ACTIVITY_IDLE);
    set_unit_focus(punit);
  }

  gtk_widget_set_sensitive(top_vbox, TRUE);
  gtk_widget_destroy(unit_select_dialog_shell);
  unit_select_dialog_shell=0;
}

static int number_of_columns(int n)
{
#if 0
  /* This would require libm, which isn't worth it for this one little
   * function.  Since MAX_SELECT_UNITS is 100 already, the ifs
   * work fine.  */
  double sqrt(); double ceil();
  return ceil(sqrt((double)n/5.0));
#else
  assert(MAX_SELECT_UNITS == 100);
  if(n<=5) return 1;
  else if(n<=20) return 2;
  else if(n<=45) return 3;
  else if(n<=80) return 4;
  else return 5;
#endif
}
static int number_of_rows(int n)
{
  int c=number_of_columns(n);
  return (n+c-1)/c;
}

/****************************************************************
popup the dialog 10% inside the main-window 
*****************************************************************/
void popup_unit_select_dialog(struct tile *ptile)
{
  int i,n,r;
  char buffer[512];
  GtkWidget *pix, *hbox, *table;
  GtkWidget *unit_select_all_command, *unit_select_close_command;

  if (!unit_select_dialog_shell){
  gtk_widget_set_sensitive(top_vbox, FALSE);

  unit_select_dialog_shell = gtk_dialog_new();
  gtk_signal_connect(GTK_OBJECT(unit_select_dialog_shell), "delete_event",
		     unit_select_callback, NULL);
  gtk_window_set_position (GTK_WINDOW(unit_select_dialog_shell), GTK_WIN_POS_MOUSE);

  gtk_window_set_title(GTK_WINDOW(unit_select_dialog_shell),
	_("Unit selection") );

  n = MIN(MAX_SELECT_UNITS, unit_list_size(&ptile->units));
  r = number_of_rows(n);

  table=gtk_table_new(r, number_of_columns(n), FALSE);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(unit_select_dialog_shell)->vbox),
	table);  

  for(i=0; i<n; i++) {
    struct unit *punit=unit_list_get(&ptile->units, i);
    struct unit_type *punittemp=unit_type(punit);
    struct city *pcity;

    hbox = gtk_hbox_new(FALSE,10);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 
				(i/r), (i/r)+1,
				(i%r), (i%r)+1);
    gtk_container_border_width(GTK_CONTAINER(hbox),5);

    unit_select_ids[i]=punit->id;

    pcity=player_find_city_by_id(game.player_ptr, punit->homecity);

    my_snprintf(buffer, sizeof(buffer), "%s(%s)\n%s",
	    punittemp->name, 
	    pcity ? pcity->name : "",
	    unit_activity_text(punit));

    pix = gtk_pixcomm_new(root_window, UNIT_TILE_WIDTH, UNIT_TILE_HEIGHT);

    unit_select_commands[i]=gtk_button_new();
    gtk_widget_set_sensitive(unit_select_commands[i],
       can_unit_do_activity(punit, ACTIVITY_IDLE) );
    gtk_widget_set_usize(unit_select_commands[i],
			 UNIT_TILE_WIDTH+4, UNIT_TILE_HEIGHT+4);
    gtk_container_add(GTK_CONTAINER(unit_select_commands[i]), pix);
    gtk_box_pack_start(GTK_BOX(hbox),unit_select_commands[i],
       FALSE, FALSE, 0);

    if (flags_are_transparent) {
       gtk_pixcomm_clear(GTK_PIXCOMM(pix), FALSE);
    }
    put_unit_gpixmap(punit, GTK_PIXCOMM(pix));

    unit_select_labels[i]=gtk_label_new(buffer);
    gtk_box_pack_start(GTK_BOX(hbox),unit_select_labels[i],
       FALSE, FALSE, 0);

    gtk_signal_connect(GTK_OBJECT(unit_select_commands[i]), "clicked",
       GTK_SIGNAL_FUNC(unit_select_callback), (gpointer)punit->id);
  }
  unit_select_no=i;


  unit_select_close_command=gtk_button_new_with_label(_("Close"));
  gtk_box_pack_start( GTK_BOX(GTK_DIALOG(unit_select_dialog_shell)->action_area),
	unit_select_close_command, TRUE, TRUE, 0 );
  GTK_WIDGET_SET_FLAGS( unit_select_close_command, GTK_CAN_DEFAULT );
  gtk_widget_grab_default( unit_select_close_command );

  unit_select_all_command=gtk_button_new_with_label(_("Ready all"));
  gtk_box_pack_start( GTK_BOX(GTK_DIALOG(unit_select_dialog_shell)->action_area),
	unit_select_all_command, TRUE, TRUE, 0 );
  GTK_WIDGET_SET_FLAGS( unit_select_all_command, GTK_CAN_DEFAULT );

  gtk_signal_connect(GTK_OBJECT(unit_select_close_command), "clicked",
	GTK_SIGNAL_FUNC(unit_select_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(unit_select_all_command), "clicked",
	GTK_SIGNAL_FUNC(unit_select_all_callback), NULL);

  gtk_widget_show_all( GTK_DIALOG(unit_select_dialog_shell)->vbox );
  gtk_widget_show_all( GTK_DIALOG(unit_select_dialog_shell)->action_area );

  gtk_set_relative_position(toplevel, unit_select_dialog_shell, 15, 10);
  gtk_widget_show(unit_select_dialog_shell);
  }
}


/****************************************************************
popup the dialog 10% inside the main-window 
*****************************************************************/
void popup_races_dialog(void)
{
  gtk_widget_set_sensitive (top_vbox, FALSE);

  create_races_dialog ();

  gtk_widget_show (races_dialog_shell);
}

/****************************************************************
...
*****************************************************************/
void popdown_races_dialog(void)
{
  if (races_dialog_shell) {
    gtk_widget_set_sensitive (top_vbox, TRUE);
    gtk_widget_destroy (races_dialog_shell);
    races_dialog_shell = NULL;
    g_list_free(sorted_races_list);
    sorted_races_list = NULL;
  } /* else there is no dialog shell to destroy */
}

/****************************************************************
...
*****************************************************************/
static gint cmp_func(gconstpointer a_p, gconstpointer b_p)
{
  return strcmp(get_nation_name(GPOINTER_TO_INT(a_p)),
		get_nation_name(GPOINTER_TO_INT(b_p)));
}

/****************************************************************
Selectes a leader and the appropriate sex.
Updates the gui elements and the selected_* variables.
*****************************************************************/
static void select_random_leader(void)
{
  int j, leader_num;
  char **leaders;

  g_list_free(leader_strings);
  leader_strings = NULL;

  /* fill leader names combo box */
  leaders = get_nation_leader_names( selected_nation, &leader_num);
  for( j=0; j<leader_num; j++) {
    leader_strings = g_list_append(leader_strings, leaders[j]);
  }
  gtk_combo_set_value_in_list( GTK_COMBO(races_name), FALSE, FALSE);
  gtk_combo_set_popdown_strings( GTK_COMBO(races_name), leader_strings);

  /* initialize leader names */
  selected_leader = myrand(leader_num);
  gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(races_name)->entry),
		      leaders[selected_leader]);

  /* initialize leader sex */
  selected_sex = get_nation_leader_sex(selected_nation,
				       leaders[selected_leader]);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(
			races_sex_toggles[selected_sex?0:1]), TRUE);
}

/****************************************************************
Selectes a random race and the appropriate city style.
Updates the gui elements and the selected_* variables.
*****************************************************************/
static void select_random_race(void)
{
  /* try to find a free nation */
  while(1) {
    selected_nation = myrand(game.playable_nation_count);
    if(GTK_WIDGET_SENSITIVE(races_toggles[g_list_index(sorted_races_list,
				       GINT_TO_POINTER(selected_nation))]))
      break;
  }

  /* initialize nation toggle array */
  gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(
		 races_toggles[g_list_index(sorted_races_list,
			    GINT_TO_POINTER(selected_nation))] ), TRUE );

  /* initialize city style */
  selected_city_style =
    city_style_ridx[get_nation_city_style(selected_nation)];
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(
			city_style_toggles[selected_city_style] ), TRUE );
}

/****************************************************************
...
*****************************************************************/
void create_races_dialog(void)
{
  int       per_row = 5;
  int       i;
  GSList    *group = NULL;
  GSList    *sgroup = NULL;
  GSList    *cgroup = NULL;
  GtkWidget *f, *fs, *fa;

  races_dialog_shell = gtk_dialog_new();
    gtk_signal_connect( GTK_OBJECT(races_dialog_shell),"delete_event",
	GTK_SIGNAL_FUNC(deleted_callback),NULL );

  gtk_window_set_title( GTK_WINDOW( races_dialog_shell ),
			_(" What Nation Will You Be?") );

  f = gtk_frame_new(_("Select nation and name"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->vbox ),
	f, FALSE, FALSE, 0 );

  /* ------- nation name toggles ------- */

  races_toggles_form =
    gtk_table_new( per_row, ((game.playable_nation_count-1)/per_row)+1, FALSE );
  gtk_container_add( GTK_CONTAINER( f ), races_toggles_form );

  free(races_toggles);
  races_toggles = fc_calloc( game.playable_nation_count, sizeof(GtkWidget*) );

  for(i=0; i<game.playable_nation_count; i++) {
    sorted_races_list=g_list_append(sorted_races_list,GINT_TO_POINTER(i));
  }
  sorted_races_list=g_list_sort(sorted_races_list,cmp_func);
  for(i=0; i<g_list_length(sorted_races_list); i++) {
    gint nat_id=GPOINTER_TO_INT(g_list_nth_data(sorted_races_list,i));
    races_toggles[i]=gtk_radio_button_new_with_label(group,
						     get_nation_name(nat_id));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(races_toggles[i]), FALSE);
    group = gtk_radio_button_group( GTK_RADIO_BUTTON( races_toggles[i] ) );
    gtk_table_attach_defaults(GTK_TABLE(races_toggles_form),races_toggles[i],
			      i%per_row, i%per_row+1, i/per_row, i/per_row+1);
  }

  /* ------- nation leader combo ------- */

  races_name = gtk_combo_new();

  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->vbox ),
	races_name, FALSE, FALSE, 0 );
  GTK_WIDGET_SET_FLAGS( races_name, GTK_CAN_DEFAULT );
  gtk_widget_grab_default( races_name );

  /* ------- leader sex toggles ------- */

  fs = gtk_frame_new(_("Select your sex"));

  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->vbox ),
	fs, FALSE, FALSE, 0 );
  races_sex_toggles_form = gtk_table_new( 1, 2, FALSE );
  gtk_container_add( GTK_CONTAINER( fs ), races_sex_toggles_form ); 

  races_sex_toggles[0]= gtk_radio_button_new_with_label( sgroup, _("Male") );
  gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( races_sex_toggles[0] ),
			       FALSE );
  sgroup = gtk_radio_button_group( GTK_RADIO_BUTTON( races_sex_toggles[0] ) );
  gtk_table_attach_defaults( GTK_TABLE(races_sex_toggles_form),
			     races_sex_toggles[0],0,1,0,1); 
  races_sex_toggles[1]= gtk_radio_button_new_with_label( sgroup, _("Female") );
  gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( races_sex_toggles[1] ),
			       FALSE );
  sgroup = gtk_radio_button_group( GTK_RADIO_BUTTON( races_sex_toggles[1] ) );
  gtk_table_attach_defaults( GTK_TABLE(races_sex_toggles_form),
			     races_sex_toggles[1],1,2,0,1); 

  /* ------- city style toggles ------- */

  /* find out styles that can be used at the game beginning */
   
  for(i=0,b_s_num=0; i<game.styles_count && i<64; i++) {
    if(city_styles[i].techreq == A_NONE) {
      city_style_idx[b_s_num] = i;
      city_style_ridx[i] = b_s_num;
      b_s_num++;
    }
  }
  free(city_style_toggles);
  city_style_toggles = fc_calloc( b_s_num, sizeof(struct GtkWidget*) );

  fa = gtk_frame_new( _("Select your city style") );

  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->vbox ),
                      fa, FALSE, FALSE, 0 );
  city_style_toggles_form =
    gtk_table_new( per_row, ((b_s_num-1)/per_row)+1, FALSE );
  gtk_container_add( GTK_CONTAINER( fa ), city_style_toggles_form ); 

  for(i=0; i<b_s_num; i++) {
      city_style_toggles[i] =
	gtk_radio_button_new_with_label(cgroup,
					city_styles[city_style_idx[i]].name);
      gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON( city_style_toggles[i] ),
				   FALSE );
      cgroup = gtk_radio_button_group( GTK_RADIO_BUTTON( city_style_toggles[i] ) );
      gtk_table_attach_defaults( GTK_TABLE(city_style_toggles_form),
				 city_style_toggles[i],
                                 i%per_row, i%per_row+1, i/per_row, i/per_row+1 );
  }

  /* ------- OK/Disc/Quit buttons ------- */

  races_ok_command = gtk_button_new_with_label( _("Ok") );
  GTK_WIDGET_SET_FLAGS( races_ok_command, GTK_CAN_DEFAULT );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->action_area ),
	races_ok_command, TRUE, TRUE, 0 );

  races_disc_command = gtk_button_new_with_label( _("Disconnect") );
  GTK_WIDGET_SET_FLAGS( races_disc_command, GTK_CAN_DEFAULT );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->action_area ),
		      races_disc_command, TRUE, TRUE, 0 );

  races_quit_command = gtk_button_new_with_label( _("Quit") );
  GTK_WIDGET_SET_FLAGS( races_quit_command, GTK_CAN_DEFAULT );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( races_dialog_shell )->action_area ),
		      races_quit_command, TRUE, TRUE, 0 );

  /* ------- connect callback functions ------- */

   for(i=0; i<g_list_length(sorted_races_list); i++)
	gtk_signal_connect( GTK_OBJECT( races_toggles[i] ), "toggled",
                           GTK_SIGNAL_FUNC( races_toggles_callback ),
                           g_list_nth_data(sorted_races_list, i));

  for(i=0; i<2; i++)
        gtk_signal_connect( GTK_OBJECT( races_sex_toggles[i] ), "toggled",
	    GTK_SIGNAL_FUNC( races_sex_toggles_callback ), NULL );

  for(i=0; i<b_s_num; i++)
        gtk_signal_connect( GTK_OBJECT( city_style_toggles[i] ), "toggled",
	    GTK_SIGNAL_FUNC( city_style_toggles_callback ), NULL );

  gtk_signal_connect( GTK_OBJECT( GTK_COMBO(races_name)->list ), 
		      "selection_changed",
		      GTK_SIGNAL_FUNC( races_name_callback ), NULL );

  gtk_signal_connect( GTK_OBJECT( races_ok_command ), "clicked",
			GTK_SIGNAL_FUNC( races_buttons_callback ), NULL );

  gtk_signal_connect( GTK_OBJECT( races_disc_command ), "clicked",
		      GTK_SIGNAL_FUNC( races_buttons_callback ), NULL );

  gtk_signal_connect( GTK_OBJECT( races_quit_command ), "clicked",
		      GTK_SIGNAL_FUNC( races_buttons_callback ), NULL );

  /* ------- set initial selections ------- */

  select_random_race();
  select_random_leader();

  gtk_widget_grab_default(races_ok_command);

  gtk_widget_show_all( GTK_DIALOG(races_dialog_shell)->vbox );
  gtk_widget_show_all( GTK_DIALOG(races_dialog_shell)->action_area );
}

/****************************************************************
...
*****************************************************************/
/*
void races_dialog_returnkey(Widget w, XEvent *event, String *params,
			    Cardinal *num_params)
{
  x_simulate_button_click(XtNameToWidget(XtParent(w), "racesokcommand"));
}
*/

/**************************************************************************
...
**************************************************************************/
void races_toggles_set_sensitive(int bits1, int bits2)
{
  int i, mybits;

  mybits=bits1;

  for(i=0; i<game.playable_nation_count && i<32; i++) {
    if(mybits&1) {
      gtk_widget_set_sensitive( races_toggles[g_list_index(sorted_races_list,
						   GINT_TO_POINTER(i))], FALSE );
      if(i==selected_nation)
	select_random_race();
    } else {
      gtk_widget_set_sensitive( races_toggles[g_list_index(sorted_races_list,
						   GINT_TO_POINTER(i))], TRUE );
    }
    mybits>>=1;
  }

  mybits=bits2;

  for(i=32; i<game.playable_nation_count; i++) {
    if(mybits&1) {
      gtk_widget_set_sensitive( races_toggles[g_list_index(sorted_races_list,
						   GINT_TO_POINTER(i))], FALSE );
      if(i==selected_nation)
	select_random_race();
    } else {
      gtk_widget_set_sensitive( races_toggles[g_list_index(sorted_races_list,
						   GINT_TO_POINTER(i))], TRUE );
    }
    mybits>>=1;
  }
}

/**************************************************************************
...
**************************************************************************/
static void races_name_callback( GtkWidget *w, gpointer data )
{
  char *lead;
  int nat;

  nat = races_buttons_get_current();
  lead = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(races_name)->entry)); 
  selected_sex = get_nation_leader_sex(nat, lead);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(
                              races_sex_toggles[selected_sex?0:1]), TRUE);
}

/**************************************************************************
...
**************************************************************************/
static void races_toggles_callback( GtkWidget *w, gpointer race_id_p )
{
  selected_nation = GPOINTER_TO_INT(race_id_p);

  select_random_leader();

  selected_city_style = city_style_ridx[get_nation_city_style(selected_nation)];
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(
				city_style_toggles[selected_city_style] ), TRUE );
}

/**************************************************************************
...
**************************************************************************/
static void races_sex_toggles_callback( GtkWidget *w, gpointer data )
{
  if(w==races_sex_toggles[0]) 
      selected_sex = 1;
  else 
      selected_sex = 0;
}

/**************************************************************************
...
**************************************************************************/
static void city_style_toggles_callback( GtkWidget *w, gpointer data )
{
  int i;

  for(i=0; i<b_s_num; i++)
    if(w==city_style_toggles[i]) {
      selected_city_style = i;
      return;
    }
}

/**************************************************************************
...
**************************************************************************/
static int races_buttons_get_current(void)
{
  return selected_nation;
}

static int sex_buttons_get_current(void)
{
  return selected_sex;
}

static int city_style_get_current(void)
{
  return selected_city_style;
}

/**************************************************************************
...
**************************************************************************/
static void races_buttons_callback( GtkWidget *w, gpointer data )
{
  int selected, selected_sex, selected_style;
  char *s;

  struct packet_alloc_nation packet;

  if(w==races_quit_command) {
    exit(0);
  } else if(w==races_disc_command) {
    popdown_races_dialog();
    disconnect_from_server();
    return;
  }

  if((selected=races_buttons_get_current())==-1) {
    append_output_window(_("You must select a nation."));
    return;
  }

  if((selected_sex=sex_buttons_get_current())==-1) {
    append_output_window(_("You must select your sex."));
    return;
  }

  if((selected_style=city_style_get_current())==-1) {
    append_output_window(_("You must select your city style."));
    return;
  }

  s = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(races_name)->entry));

  /* perform a minimum of sanity test on the name */
  packet.nation_no=selected;
  packet.is_male = selected_sex;
  packet.city_style = city_style_idx[selected_style];
  sz_strlcpy(packet.name, (char*)s);
  
  if(!get_sane_name(packet.name)) {
    append_output_window(_("You must type a legal name."));
    return;
  }

  packet.name[0]=toupper(packet.name[0]);

  send_packet_alloc_nation(&aconnection, &packet);
}


/**************************************************************************
  Destroys its widget.  Usefull for a popdown callback on pop-ups that
  won't get resused.
**************************************************************************/
void destroy_me_callback( GtkWidget *w, gpointer data)
{
  gtk_widget_destroy(w);
}


/**************************************************************************
  Adjust tax rates from main window
**************************************************************************/
void taxrates_callback(GtkWidget *w, GdkEventButton *event, gpointer data)
{
  int tax_end,lux_end,sci_end;
  size_t i;
  int delta=10;
  struct packet_player_request packet;
  
  if (get_client_state()!=CLIENT_GAME_RUNNING_STATE)
    return;
  
  i= (size_t)data;
  
  lux_end= game.player_ptr->economic.luxury;
  sci_end= lux_end + game.player_ptr->economic.science;
  tax_end= 100;

  packet.luxury= game.player_ptr->economic.luxury;
  packet.science= game.player_ptr->economic.science;
  packet.tax= game.player_ptr->economic.tax;

  i*= 10;
  if(i<lux_end){
    packet.luxury-= delta; packet.science+= delta;
  }else if(i<sci_end){
    packet.science-= delta; packet.tax+= delta;
   }else{
    packet.tax-= delta; packet.luxury+= delta;
  }
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_RATES);

}
