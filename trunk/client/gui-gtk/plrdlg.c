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
#include <ctype.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "fcintl.h"
#include "game.h"
#include "packets.h"
#include "nation.h"
#include "player.h"
#include "support.h"

#include "chatline.h"
#include "climisc.h"
#include "clinet.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "inteldlg.h"
#include "spaceshipdlg.h"
#include "colors.h"
#include "graphics.h"
#include "log.h"

#include "plrdlg.h"

static GtkWidget *players_dialog_shell;
static GtkWidget *players_list;
static GtkWidget *players_close_command;
static GtkWidget *players_int_command;
static GtkWidget *players_meet_command;
static GtkWidget *players_war_command;
static GtkWidget *players_vision_command;
static GtkWidget *players_sship_command;

/* the sortable columns need two indexes */
static int listindex_to_playerindex[MAX_NUM_PLAYERS];
static int playerindex_to_listindex[MAX_NUM_PLAYERS];

/* a simple macro that makes an often used construct look readable */
#define LI_2_PI(no) *(int*)(gtk_clist_get_row_data(GTK_CLIST(players_list), no))

static GdkPixmap *flags[MAX_NUM_PLAYERS];

static void create_players_dialog(void);
static void players_button_callback(GtkWidget *w, gpointer data);
static void players_meet_callback(GtkWidget *w, gpointer data);
static void players_war_callback(GtkWidget *w, gpointer data);
static void players_vision_callback(GtkWidget *w, gpointer data);
static void players_intel_callback(GtkWidget *w, gpointer data);
static void players_list_callback(GtkWidget *w, gint row, gint column);
static void players_list_ucallback(GtkWidget *w, gint row, gint column);
static void players_sship_callback(GtkWidget *w, gpointer data);

#define NUM_COLUMNS 11    /* number of columns in total */
#define DEF_SORT_COLUMN 2 /* default sort column (1 = nation) */

static int delay_plrdlg_update=0;

/******************************************************************
 Turn off updating of player dialog
*******************************************************************/
void plrdlg_update_delay_on(void)
{
  delay_plrdlg_update=1;
}

/******************************************************************
 Turn on updating of player dialog
*******************************************************************/
void plrdlg_update_delay_off(void)
{
  delay_plrdlg_update=0;
}


/****************************************************************
popup the dialog 10% inside the main-window 
*****************************************************************/
void popup_players_dialog(void)
{
  if(!players_dialog_shell){
    create_players_dialog();
    gtk_set_relative_position(toplevel, players_dialog_shell, 25, 25);

    gtk_widget_show(players_dialog_shell);
  }
}


/*
 * Sort plrs by column...
 */
static void sort_players_callback(GtkButton *button, gpointer *data)
{
  int sort_column = GPOINTER_TO_INT(data);
  int i;

  /* first - sort the clist */
  if (sort_column == GTK_CLIST(players_list)->sort_column) {
    if (GTK_CLIST(players_list)->sort_type == GTK_SORT_ASCENDING) {
      gtk_clist_set_sort_type(GTK_CLIST(players_list), GTK_SORT_DESCENDING);
    } else {
      gtk_clist_set_sort_type(GTK_CLIST(players_list), GTK_SORT_ASCENDING);
    }
  } else {
    gtk_clist_set_sort_type(GTK_CLIST(players_list), GTK_SORT_ASCENDING);
    gtk_clist_set_sort_column(GTK_CLIST(players_list), sort_column);
  }
  gtk_clist_sort( GTK_CLIST(players_list));

  /* second - update the index */
  for (i=0; i<GTK_CLIST(players_list)->rows; i++) {
     playerindex_to_listindex[LI_2_PI(i)]=i;
  }  
}


/****************************************************************
...
*****************************************************************/
void create_players_dialog(void)
{
  static gchar *titles_[NUM_COLUMNS] =
      { N_("Name"), N_("Flag"), N_("Nation"), N_("Ai"),
    N_("Embassy"), N_("Dipl.State"), N_("Vision"), N_("Reputation"),
    N_("State"), N_("Host"), N_("Idle")
  };
  static gchar **titles;
  int i;
  GtkAccelGroup *accel = gtk_accel_group_new();

  /* initialises the indexes, necessary for update_players_dialog */
  for (i = 0; i < MAX_NUM_PLAYERS; i++) {
    playerindex_to_listindex[i] = -1;
    listindex_to_playerindex[i] = -1;
  }

  if (!titles) titles = intl_slist(NUM_COLUMNS, titles_);

  players_dialog_shell = gtk_dialog_new();
  gtk_signal_connect( GTK_OBJECT(players_dialog_shell),"delete_event",
        GTK_SIGNAL_FUNC(players_button_callback),NULL);
  gtk_accel_group_attach(accel, GTK_OBJECT(players_dialog_shell));

  gtk_window_set_title(GTK_WINDOW(players_dialog_shell), _("Players"));

  players_list=gtk_clist_new_with_titles(NUM_COLUMNS, titles);

  for(i=0; i<NUM_COLUMNS; i++)
    gtk_clist_set_column_auto_resize (GTK_CLIST (players_list), i, TRUE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->vbox),
	players_list, TRUE, TRUE, 0);

  players_close_command=gtk_accelbutton_new(_("C_lose"), accel);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_close_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_close_command, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(players_close_command);

  players_int_command=gtk_accelbutton_new(_("_Intelligence"), accel);
  gtk_widget_set_sensitive(players_int_command, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_int_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_int_command, GTK_CAN_DEFAULT);

  players_meet_command=gtk_accelbutton_new(_("_Meet"), accel);
  gtk_widget_set_sensitive(players_meet_command, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_meet_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_meet_command, GTK_CAN_DEFAULT);

  players_war_command=gtk_accelbutton_new(_("_Cancel Treaty"), accel);
  gtk_widget_set_sensitive(players_war_command, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_war_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_war_command, GTK_CAN_DEFAULT);

  players_vision_command=gtk_accelbutton_new(_("_Withdraw vision"), accel);
  gtk_widget_set_sensitive(players_vision_command, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_vision_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_vision_command, GTK_CAN_DEFAULT);

  players_sship_command=gtk_accelbutton_new(_("_Spaceship"), accel);
  gtk_widget_set_sensitive(players_sship_command, FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(players_dialog_shell)->action_area),
	players_sship_command, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(players_sship_command, GTK_CAN_DEFAULT);

  gtk_signal_connect(GTK_OBJECT(players_list), "select_row",
		GTK_SIGNAL_FUNC(players_list_callback), NULL);

  gtk_signal_connect(GTK_OBJECT(players_list), "unselect_row",
		GTK_SIGNAL_FUNC(players_list_ucallback), NULL);
  
  gtk_signal_connect(GTK_OBJECT(players_close_command), "clicked",
		GTK_SIGNAL_FUNC(players_button_callback), NULL);

  gtk_signal_connect(GTK_OBJECT(players_meet_command), "clicked",
		GTK_SIGNAL_FUNC(players_meet_callback), NULL);

  gtk_signal_connect(GTK_OBJECT(players_war_command), "clicked",
		GTK_SIGNAL_FUNC(players_war_callback), NULL);

  gtk_signal_connect(GTK_OBJECT(players_vision_command), "clicked",
		GTK_SIGNAL_FUNC(players_vision_callback), NULL);

  gtk_signal_connect(GTK_OBJECT(players_int_command), "clicked",
		GTK_SIGNAL_FUNC(players_intel_callback), NULL);
  
  gtk_signal_connect(GTK_OBJECT(players_sship_command), "clicked",
		GTK_SIGNAL_FUNC(players_sship_callback), NULL);

  gtk_widget_show_all(GTK_DIALOG(players_dialog_shell)->vbox);
  gtk_widget_show_all(GTK_DIALOG(players_dialog_shell)->action_area);

  gtk_widget_add_accelerator(players_close_command, "clicked",
	accel, GDK_Escape, 0, 0);

  /* default sort column is nation name */
  gtk_clist_set_sort_column(GTK_CLIST(players_list), DEF_SORT_COLUMN);

  /* all colums but flag column are clickable */
  for (i = 0; i < NUM_COLUMNS; i++) {
    if (i != 1) {
      gtk_signal_connect(GTK_OBJECT
			 (GTK_CLIST(players_list)->column[i].button),
			 "clicked", GTK_SIGNAL_FUNC(sort_players_callback),
			 GINT_TO_POINTER(i));
    }
  }

  gtk_clist_clear(GTK_CLIST(players_list));
  update_players_dialog();

  /* don't resize the state column */
  gtk_clist_set_column_auto_resize(GTK_CLIST(players_list), 8, FALSE);
}

/* 
 * Builds the text for the cells of a row in the player report. If
 * update is 1, only the changable entries are build.
 */
static void build_row(char **row, int i, int update)
{
  static char namebuf[MAX_LEN_NAME], flagbuf[1], aibuf[2], dsbuf[32],
      repbuf[32], statebuf[32], idlebuf[32];
  const struct player_diplstate *pds;

  /* we cassume that neither name nor the nation of a player changes */
  if (update == 0) {
    /* the playername */
    my_snprintf(namebuf, sizeof(namebuf), "%-16s", game.players[i].name);
    row[0] = namebuf;

    /* since flag is a pixmap, this can be empty */
    flagbuf[0] = '\0';
    row[1] = flagbuf;

    /* the nation */
    row[2] = get_nation_name(game.players[i].nation);
  }

  /* text for name, plus AI marker */
  aibuf[0] = (game.players[i].ai.control ? '*' : '\0');
  aibuf[1] = '\0';

  /* text for diplstate type and turns -- not applicable if this is me */
  if (i == game.player_idx) {
    strcpy(dsbuf, "-");
  } else {
    pds = pplayer_get_diplstate(game.player_ptr, get_player(i));
    if (pds->type == DS_CEASEFIRE) {
      my_snprintf(dsbuf, sizeof(dsbuf), "%s (%d)",
		  diplstate_text(pds->type), pds->turns_left);
    } else {
      my_snprintf(dsbuf, sizeof(dsbuf), "%s", diplstate_text(pds->type));
    }
  }

  /* text for state */
  if (game.players[i].is_alive) {
    if (game.players[i].is_connected) {
      if (game.players[i].turn_done) {
	sz_strlcpy(statebuf, _("done"));
      } else {
	sz_strlcpy(statebuf, _("moving"));
      }
    } else {
      statebuf[0] = '\0';
    }
  } else {
    sz_strlcpy(statebuf, _("R.I.P"));
  }

  /* text for idleness */
  if (game.players[i].nturns_idle > 3) {
    my_snprintf(idlebuf, sizeof(idlebuf), _("(idle %d turns)"),
		game.players[i].nturns_idle - 1);
  } else {
    idlebuf[0] = '\0';
  }

  /* text for reputation */
  my_snprintf(repbuf, sizeof(repbuf),
	      reputation_text(game.players[i].reputation));

  /* assemble the whole lot */
  row[3] = aibuf;
  row[4] = get_embassy_status(game.player_ptr, &game.players[i]);
  row[5] = dsbuf;
  row[6] = get_vision_status(game.player_ptr, &game.players[i]);
  row[7] = repbuf;
  row[8] = statebuf;
  row[9] = (char *) player_addr_hack(&game.players[i]);	/* Fixme */
  row[10] = idlebuf;
}

/*
 * Method returns the bounding box of a sprite. It assumes a
 * rectangular object/mask. The bounding box contains the border
 * (pixel which have unset pixel as neighbours) pixel.
 */
static void get_sprite_bounding_box(SPRITE * sprite, int *start_x,
				    int *start_y, int *end_x, int *end_y)
{
  GdkImage *mask_image;
  int i;

  if (!sprite->has_mask || sprite->mask == NULL) {
    *start_x = 0;
    *start_y = 0;
    *end_x = sprite->width - 1;
    *end_y = sprite->height - 1;
    return;
  }

  mask_image =
      gdk_image_get(sprite->mask, 0, 0, sprite->width, sprite->height);

  for (i = 0; i < sprite->width; i++)
    if (gdk_image_get_pixel(mask_image, i, 0) != 0)
      break;
  *start_x = i;

  for (i = 0; i < sprite->height; i++)
    if (gdk_image_get_pixel(mask_image, 0, i) != 0)
      break;
  *start_y = i;

  for (i = sprite->width - 1; i >= 0; i--)
    if (gdk_image_get_pixel(mask_image, i, 0) != 0)
      break;
  *end_x = i;

  for (i = sprite->height - 1; i >= 0; i--)
    if (gdk_image_get_pixel(mask_image, 0, i) != 0)
      break;
  *end_y = i;

  gdk_image_destroy(mask_image);
}

#define MINIMAL_FLAG_WIDTH	5
#define MINIMAL_FLAG_HEIGHT	5

/* 
 * Builds the flag pixmap.
 */
static void build_flag(int playerindex)
{
  int start_x, start_y, end_x, end_y, flag_h, flag_w, newflag_h, newflag_w;
  GdkPixmap *flag_pixmap;
  SPRITE *flag_sprite;

  flag_sprite = get_nation_by_plr(&game.players[playerindex])->flag_sprite;

  get_sprite_bounding_box(flag_sprite, &start_x, &start_y, &end_x, &end_y);

  assert(start_x == 0);
  assert(start_y == 0);
  assert(end_x >= MINIMAL_FLAG_WIDTH);
  assert(end_y >= MINIMAL_FLAG_HEIGHT);

  flag_w = (end_x - start_x) + 1;
  flag_h = (end_y - start_y) + 1;

  /* now scaling the original pixmap */
  newflag_h = GTK_CLIST(players_list)->row_height;
  newflag_w = ((double) newflag_h / flag_h) * flag_w;

  freelog(LOG_DEBUG, "%dx%d %dx%d %dx%d", flag_sprite->width,
	  flag_sprite->height, flag_w, flag_h, newflag_w, newflag_h);

  flag_pixmap = gtk_scale_pixmap(flag_sprite->pixmap, flag_w, flag_h,
				 newflag_w, newflag_h);

  /* put a black border around it */
  gdk_gc_set_foreground(civ_gc, colors_standard[COLOR_STD_BLACK]);
  gdk_draw_rectangle(flag_pixmap, civ_gc, 0, 0, 0, newflag_w, newflag_h);

  /* and finaly store the pixmap in the static flags array */
  flags[playerindex] = flag_pixmap;
}

/**************************************************************************
...
**************************************************************************/
void update_players_dialog(void)
{
  if (players_dialog_shell && !delay_plrdlg_update) {
    GdkColor *state_col;
    char *row_texts[NUM_COLUMNS];
    int i, j, row, sort_needed = 0;

    gtk_clist_freeze(GTK_CLIST(players_list));

    for (i = 0; i < game.nplayers; i++) {
      /* skip barbarians */
      if (is_barbarian(&game.players[i])) {
	continue;
      }
      row = playerindex_to_listindex[i];
      if (row == -1) {
	/* 
	 * A nation is not in the player report yet. This happens when
	 * the report is just opened and after a split.
	 */
	row = GTK_CLIST(players_list)->rows;
	build_row(row_texts, i, 0);
	gtk_clist_append(GTK_CLIST(players_list), row_texts);
	gtk_clist_set_row_data(GTK_CLIST(players_list), row,
			       &(listindex_to_playerindex[row]));

	build_flag(i);
	gtk_clist_set_pixmap(GTK_CLIST(players_list), row, 1, flags[i],
			     NULL);

	listindex_to_playerindex[row] = i;
	playerindex_to_listindex[i] = row;
	sort_needed = 1;
      } else {
	build_row(row_texts, i, 1);
	/* 
	 * The nation already had a row in the player report. In that
	 * case we just update the row. 
	 */
	for (j = 3; j < NUM_COLUMNS; j++) {
	  gtk_clist_set_text(GTK_CLIST(players_list), row, j,
			     row_texts[j]);
	}
      }

      /* now add some eye candy ... */
      switch (pplayer_get_diplstate(game.player_ptr, get_player(i))->type) {
      case DS_WAR:
	state_col = colors_standard[COLOR_STD_RED];
	break;
      case DS_ALLIANCE:
	state_col = colors_standard[COLOR_STD_GROUND];
	break;
      default:
	state_col = colors_standard[COLOR_STD_BLACK];
      }
      gtk_clist_set_foreground(GTK_CLIST(players_list), row, state_col);
    }

    if (sort_needed) {
      /* when new nations appear in the report, we have to sort it */
      gtk_clist_sort(GTK_CLIST(players_list));
      /* second - update the index */
      for (i = 0; i < GTK_CLIST(players_list)->rows; i++) {
	playerindex_to_listindex[LI_2_PI(i)] = i;
      }
    }

    gtk_clist_thaw(GTK_CLIST(players_list));
    gtk_widget_show_all(players_list);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_list_callback(GtkWidget * w, gint row, gint column)
{
  int player_index = LI_2_PI(row);
  struct player *pplayer = &game.players[player_index];

  if (pplayer->spaceship.state != SSHIP_NONE)
    gtk_widget_set_sensitive(players_sship_command, TRUE);
  else
    gtk_widget_set_sensitive(players_sship_command, FALSE);

  switch (pplayer_get_diplstate
	  (game.player_ptr, get_player(player_index))->type) {
  case DS_WAR:
  case DS_NO_CONTACT:
    gtk_widget_set_sensitive(players_war_command, FALSE);
    break;
  default:
    gtk_widget_set_sensitive(players_war_command,
			     game.player_idx != player_index);
  }

  if (game.player_ptr->gives_shared_vision & (1 << pplayer->player_no))
    gtk_widget_set_sensitive(players_vision_command, TRUE);
  else
    gtk_widget_set_sensitive(players_vision_command, FALSE);

  if (pplayer->is_alive && player_has_embassy(game.player_ptr, pplayer)) {
    if (pplayer->is_connected)
      gtk_widget_set_sensitive(players_meet_command, TRUE);
    else
      gtk_widget_set_sensitive(players_meet_command, FALSE);
    gtk_widget_set_sensitive(players_int_command, TRUE);
    return;
  }

  gtk_widget_set_sensitive(players_meet_command, FALSE);
  gtk_widget_set_sensitive(players_int_command, FALSE);
}

void players_list_ucallback(GtkWidget *w, gint row, gint column)
{
  gtk_widget_set_sensitive(players_meet_command, FALSE);
  gtk_widget_set_sensitive(players_int_command, FALSE);
}

/**************************************************************************
...
**************************************************************************/
void players_button_callback(GtkWidget *w, gpointer data)
{
  gtk_widget_destroy(players_dialog_shell);
  players_dialog_shell=0;
}

/**************************************************************************
...
**************************************************************************/
void players_meet_callback(GtkWidget *w, gpointer data)
{
  GList *selection;
  gint row;
  int player_index;

  if(!(selection=GTK_CLIST(players_list)->selection))
    return;

  row=(gint)selection->data;
  player_index = LI_2_PI(row);

  if(player_has_embassy(game.player_ptr, &game.players[player_index])) {
    struct packet_diplomacy_info pa;
  
    pa.plrno0=game.player_idx;
    pa.plrno1=player_index;
    send_packet_diplomacy_info(&aconnection, PACKET_DIPLOMACY_INIT_MEETING,
        		       &pa);
  }
  else {
    append_output_window(_("Game: You need an embassy to "
			   "establish a diplomatic meeting."));
  }
}

/**************************************************************************
...
**************************************************************************/
void players_war_callback(GtkWidget *w, gpointer data)
{
  GList *selection = GTK_CLIST(players_list)->selection;
  gint row;
  int player_index;

  if (!selection)
    return;
  else {
    struct packet_generic_integer pa;    

    row = (gint)selection->data;
    player_index = LI_2_PI(row);

    pa.value = player_index;
    send_packet_generic_integer(&aconnection, PACKET_PLAYER_CANCEL_PACT,
				&pa);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_vision_callback(GtkWidget *w, gpointer data)
{
  GList *selection = GTK_CLIST(players_list)->selection;
  gint row;
  int player_index;

  if (!selection)
    return;
  else {
    struct packet_generic_integer pa;    

    row = (gint)selection->data;
    player_index = LI_2_PI(row);

    pa.value = player_index;
    send_packet_generic_integer(&aconnection, PACKET_PLAYER_REMOVE_VISION,
				&pa);
  }
}

/**************************************************************************
...
**************************************************************************/
void players_intel_callback(GtkWidget *w, gpointer data)
{
  GList *selection;
  gint row;
  int player_index;

  if(!(selection=GTK_CLIST(players_list)->selection))
      return;

  row=(gint)selection->data;
  player_index = LI_2_PI(row);

  if(player_has_embassy(game.player_ptr, &game.players[player_index]))
    popup_intel_dialog(&game.players[player_index]);
}

/**************************************************************************
...
**************************************************************************/
void players_sship_callback(GtkWidget *w, gpointer data)
{
  GList *selection;
  gint row;
  int player_index;

  if(!(selection=GTK_CLIST(players_list)->selection))
      return;

  row=(gint)selection->data;
  player_index = LI_2_PI(row);

  popup_spaceship_dialog(&game.players[player_index]);
}
