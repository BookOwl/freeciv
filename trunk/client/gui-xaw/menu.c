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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#include "capability.h"
#include "map.h"
#include "mem.h"
#include "unit.h"

#include "chatline.h"
#include "cityrep.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h" /* request_xxx and get_unit_in_focus */
#include "dialogs.h"
#include "finddlg.h"
#include "gotodlg.h"
#include "helpdlg.h"
#include "mapctrl.h" 
#include "messagedlg.h"
#include "messagewin.h"
#include "optiondlg.h"
#include "options.h"
#include "plrdlg.h"
#include "ratesdlg.h"
#include "repodlgs.h"
#include "spaceshipdlg.h"

#include "menu.h"

enum MenuID {
  MENU_END_OF_LIST=0,

  MENU_GAME_OPTIONS,
  MENU_GAME_MSG_OPTIONS,
  MENU_GAME_SAVE_SETTINGS,
  MENU_GAME_PLAYERS,
  MENU_GAME_MESSAGES,
  MENU_GAME_SERVER_OPTIONS1,
  MENU_GAME_SERVER_OPTIONS2,
  MENU_GAME_OUTPUT_LOG,
  MENU_GAME_CLEAR_OUTPUT,
  MENU_GAME_DISCONNECT,
  MENU_GAME_QUIT,

  MENU_KINGDOM_RATES,   
  MENU_KINGDOM_FIND_CITY,
  MENU_KINGDOM_REVOLUTION,

  MENU_VIEW_SHOW_MAP_GRID,
  MENU_VIEW_CENTER_VIEW,
  
  MENU_ORDER_CITY,
  MENU_ORDER_ROAD,
  MENU_ORDER_IRRIGATE,
  MENU_ORDER_MINE,
  MENU_ORDER_TRANSFORM,
  MENU_ORDER_FORTRESS,
  MENU_ORDER_POLLUTION,
  MENU_ORDER_FORTIFY,
  MENU_ORDER_SENTRY,
  MENU_ORDER_PILLAGE,
  MENU_ORDER_HOMECITY,
  MENU_ORDER_UNLOAD,
  MENU_ORDER_WAKEUP,
  MENU_ORDER_AUTO_SETTLER,
  MENU_ORDER_AUTO_ATTACK,
  MENU_ORDER_EXPLORE,
  MENU_ORDER_GOTO,
  MENU_ORDER_GOTO_CITY,
  MENU_ORDER_DISBAND,
  MENU_ORDER_BUILD_WONDER,
  MENU_ORDER_TRADE_ROUTE,
  MENU_ORDER_NUKE,
  MENU_ORDER_WAIT,
  MENU_ORDER_DONE,

  MENU_REPORT_CITY,
  MENU_REPORT_ACTIVE_UNITS,
  MENU_REPORT_TRADE,
  MENU_REPORT_SCIENCE,
  MENU_REPORT_WOW,
  MENU_REPORT_TOP_CITIES,
  MENU_REPORT_DEMOGRAPHIC,
  MENU_REPORT_SPACESHIP,

  MENU_HELP_CONTROLS,
  MENU_HELP_PLAYING,
  MENU_HELP_IMPROVEMENTS,
  MENU_HELP_UNITS,
  MENU_HELP_COMBAT,
  MENU_HELP_ZOC,
  MENU_HELP_TECH,
  MENU_HELP_TERRAIN,
  MENU_HELP_WONDERS,
  MENU_HELP_GOVERNMENT,
  MENU_HELP_HAPPINESS,
  MENU_HELP_SPACE_RACE,
  MENU_HELP_COPYING,
  MENU_HELP_ABOUT,
  MENU_HELP_CONNECTING,
  MENU_HELP_CHATLINE,

  MENU_SEPARATOR_LINE
};

struct MenuEntry {
  char *text;
  enum  MenuID id;
  Widget w;
};

struct Menu {
  Widget button, shell; 
  struct MenuEntry *entries;
};

struct Menu *game_menu, *kingdom_menu, *view_menu, *orders_menu, *reports_menu, *help_menu;


struct MenuEntry game_menu_entries[]={
    { "Local Options",      MENU_GAME_OPTIONS, 0 },
    { "Message Options",    MENU_GAME_MSG_OPTIONS, 0 },
    { "Save Settings",      MENU_GAME_SAVE_SETTINGS, 0 },
    { "-" ,                 MENU_SEPARATOR_LINE, 0 },
    { "Players         F3", MENU_GAME_PLAYERS, 0 },
    { "Messages        F4", MENU_GAME_MESSAGES, 0 },
    { "-" ,                 MENU_SEPARATOR_LINE, 0 },
    { "Server Opt initial", MENU_GAME_SERVER_OPTIONS1, 0 },
    { "Server Opt ongoing", MENU_GAME_SERVER_OPTIONS2, 0 },
    { "-" ,                 MENU_SEPARATOR_LINE, 0 },
    { "Export Log",         MENU_GAME_OUTPUT_LOG, 0 }, /* added by Syela */
    { "Clear Log",          MENU_GAME_CLEAR_OUTPUT, 0 },
    { "-" ,                 MENU_SEPARATOR_LINE, 0 },
    { "Disconnect",         MENU_GAME_DISCONNECT, 0 }, /* added by Syela */
    { "Quit",               MENU_GAME_QUIT, 0 },
    { 0, MENU_END_OF_LIST, 0 }
};

struct MenuEntry kingdom_menu_entries[]={
    { "Rates       T",  MENU_KINGDOM_RATES, 0 },
    { "-" ,             MENU_SEPARATOR_LINE, 0 },
    { "Find City   C",  MENU_KINGDOM_FIND_CITY, 0 },
    { "-" ,             MENU_SEPARATOR_LINE, 0 },
    { "Revolution  R",  MENU_KINGDOM_REVOLUTION, 0 },
    { 0, MENU_END_OF_LIST, 0 }
};

struct MenuEntry view_menu_entries[]={
    { "Grid On/Off   ctl-g",   MENU_VIEW_SHOW_MAP_GRID, 0 },
    { "Center View       c",   MENU_VIEW_CENTER_VIEW, 0 },
    { 0, MENU_END_OF_LIST, 0 }
};

struct MenuEntry order_menu_entries[]={
    { "Build City          b", MENU_ORDER_CITY, 0},
    { "Build Road          r", MENU_ORDER_ROAD, 0},
    { "Build Irrigation    i", MENU_ORDER_IRRIGATE, 0},
    { "Build Mine          m", MENU_ORDER_MINE, 0},
    { "Transform Terrain   o", MENU_ORDER_TRANSFORM, 0},
    { "Build Fortress      f", MENU_ORDER_FORTRESS, 0},
    { "Clean Pollution     p", MENU_ORDER_POLLUTION, 0},
    { "-" ,                    MENU_SEPARATOR_LINE, 0 },
    { "Fortify             f", MENU_ORDER_FORTIFY, 0},
    { "Sentry              s", MENU_ORDER_SENTRY, 0},
    { "Pillage             P", MENU_ORDER_PILLAGE, 0},
    { "-" ,                    MENU_SEPARATOR_LINE, 0 },
    { "Make Homecity       h", MENU_ORDER_HOMECITY, 0},
    { "Unload              u", MENU_ORDER_UNLOAD, 0},
    { "Wake up Others      W", MENU_ORDER_WAKEUP, 0},
    { "-" ,                    MENU_SEPARATOR_LINE, 0 },
    { "Auto Settler        a", MENU_ORDER_AUTO_SETTLER, 0},
    { "Auto Attack         a", MENU_ORDER_AUTO_ATTACK, 0},
    { "Auto Explore        x", MENU_ORDER_EXPLORE, 0},
    { "Go to               g", MENU_ORDER_GOTO, 0},
    { "Go/Airlift to City  l", MENU_ORDER_GOTO_CITY, 0},
    { "-" ,                    MENU_SEPARATOR_LINE, 0 },
    { "Disband Unit        D", MENU_ORDER_DISBAND, 0},
    { "Help Build Wonder   b", MENU_ORDER_BUILD_WONDER, 0},
    { "Make Trade Route    r", MENU_ORDER_TRADE_ROUTE, 0},
    { "Explode Nuclear     N", MENU_ORDER_NUKE, 0},
    { "-" ,                    MENU_SEPARATOR_LINE, 0 },
    { "Wait                w", MENU_ORDER_WAIT, 0},
    { "Done              spc", MENU_ORDER_DONE, 0},
    { 0, MENU_END_OF_LIST, 0}
};

struct MenuEntry reports_menu_entries[]={
    { "City Report           F1", MENU_REPORT_CITY, 0},
    { "Military Report       F2", MENU_REPORT_ACTIVE_UNITS, 0},
    { "Trade Report          F5", MENU_REPORT_TRADE, 0},
    { "Science Report        F6", MENU_REPORT_SCIENCE, 0},
    { "-" ,                       MENU_SEPARATOR_LINE, 0 },
    { "Wonders of the World  F7", MENU_REPORT_WOW, 0},
    { "Top Five Cities       F8", MENU_REPORT_TOP_CITIES, 0},
    { "Demographic          F11", MENU_REPORT_DEMOGRAPHIC, 0},
    { "Spaceship            F12", MENU_REPORT_SPACESHIP, 0},
    { 0, MENU_END_OF_LIST, 0}
};

struct MenuEntry help_menu_entries[]={
    { "Help Connecting",   MENU_HELP_CONNECTING, 0},
    { "Help Controls",     MENU_HELP_CONTROLS, 0},
    { "Help Chatline",     MENU_HELP_CHATLINE, 0},
    { "Help Playing",      MENU_HELP_PLAYING, 0},
    { "-" ,                MENU_SEPARATOR_LINE, 0 },
    { "Help Improvements", MENU_HELP_IMPROVEMENTS, 0},
    { "Help Units",        MENU_HELP_UNITS, 0},
    { "Help Combat",       MENU_HELP_COMBAT, 0},
    { "Help ZOC",          MENU_HELP_ZOC, 0},
    { "Help Technology",   MENU_HELP_TECH, 0},
    { "Help Terrain",      MENU_HELP_TERRAIN, 0},
    { "Help Wonders",      MENU_HELP_WONDERS, 0},
    { "Help Government",   MENU_HELP_GOVERNMENT, 0},
    { "Help Happiness",    MENU_HELP_HAPPINESS, 0},
    { "Help Space Race",   MENU_HELP_SPACE_RACE, 0},
    { "-" ,                MENU_SEPARATOR_LINE, 0 },
    { "Copying",           MENU_HELP_COPYING, 0},
    { "About",             MENU_HELP_ABOUT, 0},
    { 0, MENU_END_OF_LIST, 0},
};

enum unit_activity road_activity;

struct Menu *create_menu(char *name, struct MenuEntry entries[], 
			 void (*menucallback)(Widget, XtPointer, XtPointer),
			 Widget parent);
void menu_entry_sensitive(struct Menu *pmenu, enum MenuID id, Bool s);
void menu_entry_rename(struct Menu *pmenu, enum MenuID id, char *s);

/****************************************************************
...
*****************************************************************/
void update_menus(void)
{
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    XtVaSetValues(reports_menu->button, XtNsensitive, False, NULL);
    XtVaSetValues(orders_menu->button, XtNsensitive, False, NULL);
    XtVaSetValues(view_menu->button, XtNsensitive, False, NULL);
    XtVaSetValues(kingdom_menu->button, XtNsensitive, False, NULL);

    menu_entry_sensitive(game_menu, MENU_GAME_OPTIONS, 0);
    menu_entry_sensitive(game_menu, MENU_GAME_MSG_OPTIONS, 0);
    menu_entry_sensitive(game_menu, MENU_GAME_SAVE_SETTINGS, 0);
    menu_entry_sensitive(game_menu, MENU_GAME_PLAYERS, 0);
    menu_entry_sensitive(game_menu, MENU_GAME_MESSAGES, 0);
    menu_entry_sensitive(game_menu, MENU_GAME_SERVER_OPTIONS1, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_SERVER_OPTIONS2, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_OUTPUT_LOG, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_CLEAR_OUTPUT, 1);
  }
  else {
    struct unit *punit;
    int i;
    int any_cities = FALSE;

    punit=get_unit_in_focus();

    for(i=0; i<game.nplayers; i++) {
      if (city_list_size(&game.players[i].cities)) {
	any_cities = TRUE;
	break;
      }
    }

    XtVaSetValues(reports_menu->button, XtNsensitive, True, NULL);
    XtVaSetValues(orders_menu->button, XtNsensitive, (punit != NULL), NULL);
    XtVaSetValues(view_menu->button, XtNsensitive, True, NULL);
    XtVaSetValues(kingdom_menu->button, XtNsensitive, True, NULL);
  
    menu_entry_sensitive(game_menu, MENU_GAME_OPTIONS, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_MSG_OPTIONS, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_SAVE_SETTINGS, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_PLAYERS, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_MESSAGES, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_SERVER_OPTIONS1, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_SERVER_OPTIONS2, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_OUTPUT_LOG, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_CLEAR_OUTPUT, 1);
    menu_entry_sensitive(game_menu, MENU_GAME_DISCONNECT, 1);

    menu_entry_sensitive(reports_menu, MENU_REPORT_SPACESHIP,
			 (game.player_ptr->spaceship.state!=SSHIP_NONE));

    if(punit) {
      char * chgfmt      = "Change to %-9s %c";
      char irrtext[64]   = "Build Irrigation    i",
           mintext[64]   = "Build Mine          m",
	   transtext[64] = "Transform terrain   o";
      char *roadtext     = "Build Road          r";
      enum tile_terrain_type  ttype;
      struct tile_type *      tinfo;

      menu_entry_sensitive(orders_menu, MENU_ORDER_CITY, 
			   (can_unit_build_city(punit) ||
			    can_unit_add_to_city(punit)));
      menu_entry_sensitive(orders_menu, MENU_ORDER_ROAD, 
			   can_unit_do_activity(punit, ACTIVITY_ROAD) ||
			   can_unit_do_activity(punit, ACTIVITY_RAILROAD));
      menu_entry_sensitive(orders_menu, MENU_ORDER_IRRIGATE, 
			   can_unit_do_activity(punit, ACTIVITY_IRRIGATE));
      menu_entry_sensitive(orders_menu, MENU_ORDER_MINE, 
			   can_unit_do_activity(punit, ACTIVITY_MINE));
      menu_entry_sensitive(orders_menu, MENU_ORDER_TRANSFORM, 
			   can_unit_do_activity(punit, ACTIVITY_TRANSFORM));
      menu_entry_sensitive(orders_menu, MENU_ORDER_FORTRESS, 
			   can_unit_do_activity(punit, ACTIVITY_FORTRESS));

      if (can_unit_paradropped(punit)) {
        menu_entry_rename(orders_menu, MENU_ORDER_POLLUTION, "Paradrop            p");
        menu_entry_sensitive(orders_menu, MENU_ORDER_POLLUTION, TRUE);
      } else
      {
        menu_entry_rename(orders_menu, MENU_ORDER_POLLUTION, "Clean Pollution     p");
        menu_entry_sensitive(orders_menu, MENU_ORDER_POLLUTION, 
 	       can_unit_do_activity(punit, ACTIVITY_POLLUTION));
      }

      menu_entry_sensitive(orders_menu, MENU_ORDER_FORTIFY, 
			   can_unit_do_activity(punit, ACTIVITY_FORTIFY));
      menu_entry_sensitive(orders_menu, MENU_ORDER_SENTRY, 
			   can_unit_do_activity(punit, ACTIVITY_SENTRY));
      menu_entry_sensitive(orders_menu, MENU_ORDER_PILLAGE, 
			   can_unit_do_activity(punit, ACTIVITY_PILLAGE));
      menu_entry_sensitive(orders_menu, MENU_ORDER_HOMECITY, 
			   can_unit_change_homecity(punit));
      menu_entry_sensitive(orders_menu, MENU_ORDER_UNLOAD, 
			   get_transporter_capacity(punit)>0);
      menu_entry_sensitive(orders_menu, MENU_ORDER_WAKEUP, 
			   is_unit_activity_on_tile(ACTIVITY_SENTRY,
				punit->x,punit->y));
      menu_entry_sensitive(orders_menu, MENU_ORDER_AUTO_SETTLER,
			   (can_unit_do_auto(punit)
			    && unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(orders_menu, MENU_ORDER_AUTO_ATTACK, 
			   (can_unit_do_auto(punit)
			    && !unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(orders_menu, MENU_ORDER_EXPLORE, 
			   can_unit_do_activity(punit, ACTIVITY_EXPLORE));
      menu_entry_sensitive(orders_menu, MENU_ORDER_GOTO_CITY,
			   any_cities);
      menu_entry_sensitive(orders_menu, MENU_ORDER_BUILD_WONDER,
			   unit_can_help_build_wonder_here(punit));
      menu_entry_sensitive(orders_menu, MENU_ORDER_TRADE_ROUTE,
			   unit_can_est_traderoute_here(punit));
      menu_entry_sensitive(orders_menu, MENU_ORDER_NUKE,
                           unit_flag(punit->type, F_NUCLEAR) &&
                           has_capability("nuke", aconnection.capability));

      if (unit_flag(punit->type, F_SETTLERS)
	  && map_get_city(punit->x, punit->y)) {
	menu_entry_rename(orders_menu, MENU_ORDER_CITY, "Add to City         b");
      } else {
	menu_entry_rename(orders_menu, MENU_ORDER_CITY, "Build City          b");
      }

      ttype = map_get_tile(punit->x, punit->y)->terrain;
      tinfo = get_tile_type(ttype);
      if ((tinfo->irrigation_result != T_LAST) && (tinfo->irrigation_result != ttype))
	{
	  sprintf (irrtext, chgfmt,
		   (get_tile_type(tinfo->irrigation_result))->terrain_name, 'i');
	}
      else if ((map_get_tile(punit->x,punit->y)->special&S_IRRIGATION) &&
	       improvement_exists(B_SUPERMARKET))
	{
	  strcpy (irrtext, "Build Farmland      i");
	}
      if ((tinfo->mining_result != T_LAST) && (tinfo->mining_result != ttype))
	{
	  sprintf (mintext, chgfmt,
		   (get_tile_type(tinfo->mining_result))->terrain_name, 'm');
	}
      if ((tinfo->transform_result != T_LAST) && (tinfo->transform_result != ttype))
	{
	  sprintf (transtext, chgfmt,
		   (get_tile_type(tinfo->transform_result))->terrain_name, 'o');
	}

      if (map_get_tile(punit->x,punit->y)->special&S_ROAD) {
	roadtext="Build Railroad      r";
	road_activity=ACTIVITY_RAILROAD;  
      } else {
	roadtext="Build Road          r";
	road_activity=ACTIVITY_ROAD;  
      }
      menu_entry_rename(orders_menu, MENU_ORDER_ROAD, roadtext);

      menu_entry_rename(orders_menu, MENU_ORDER_IRRIGATE, irrtext);
      menu_entry_rename(orders_menu, MENU_ORDER_MINE, mintext);
      menu_entry_rename(orders_menu, MENU_ORDER_TRANSFORM, transtext);
    }
  }
}

/****************************************************************
...
*****************************************************************/
static void game_menu_callback(Widget w, XtPointer client_data,
			       XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
  case MENU_GAME_OPTIONS:
    popup_option_dialog();
    break;
  case MENU_GAME_MSG_OPTIONS:
    popup_messageopt_dialog();
    break;
  case MENU_GAME_SAVE_SETTINGS:
    save_options();
    break;
  case MENU_GAME_PLAYERS:
    popup_players_dialog();
    break;
  case MENU_GAME_MESSAGES:
    popup_meswin_dialog();
    break;
  case MENU_GAME_SERVER_OPTIONS1:
    send_report_request(REPORT_SERVER_OPTIONS1);
    break;
  case MENU_GAME_SERVER_OPTIONS2:
    send_report_request(REPORT_SERVER_OPTIONS2);
    break;
  case MENU_GAME_OUTPUT_LOG:
    log_output_window();
    break;
  case MENU_GAME_CLEAR_OUTPUT:
    clear_output_window();
    break;
  case MENU_GAME_DISCONNECT:
    disconnect_from_server();
    break;
  case MENU_GAME_QUIT:
    exit(0);
    break;
  }
}

/****************************************************************
...
*****************************************************************/
static void kingdom_menu_callback(Widget w, XtPointer client_data,
				  XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
  case MENU_KINGDOM_FIND_CITY:
    popup_find_dialog();
    break;
  case MENU_KINGDOM_RATES:
    popup_rates_dialog();
    break;
  case MENU_KINGDOM_REVOLUTION:
    popup_revolution_dialog();
    break;
  }
}

/****************************************************************
...
*****************************************************************/
static void view_menu_callback(Widget w, XtPointer client_data,
			       XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
  case MENU_VIEW_SHOW_MAP_GRID:
    request_toggle_map_grid();
    break;
  case MENU_VIEW_CENTER_VIEW:
    request_center_focus_unit();
    break;
  }
}


/****************************************************************
...
*****************************************************************/
static void orders_menu_callback(Widget w, XtPointer client_data,
				 XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
   case MENU_ORDER_CITY:
    if(get_unit_in_focus())
      request_unit_build_city(get_unit_in_focus());
    break;
   case MENU_ORDER_ROAD:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), road_activity);
    break;
   case MENU_ORDER_IRRIGATE:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_IRRIGATE);
    break;
   case MENU_ORDER_MINE:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_MINE);
    break;
   case MENU_ORDER_TRANSFORM:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_TRANSFORM);
    break;
   case MENU_ORDER_FORTRESS:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_FORTRESS);
    break;
   case MENU_ORDER_POLLUTION:
    key_unit_clean_pollution();
/*    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_POLLUTION);
*/
    break;
   case MENU_ORDER_FORTIFY:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_FORTIFY);
    break;
   case MENU_ORDER_SENTRY:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_SENTRY);
    break;
   case MENU_ORDER_PILLAGE:
    if(get_unit_in_focus())
      request_unit_pillage(get_unit_in_focus());
    break;
   case MENU_ORDER_HOMECITY:
    if(get_unit_in_focus())
      request_unit_change_homecity(get_unit_in_focus());
    break;
   case MENU_ORDER_UNLOAD:
    if(get_unit_in_focus())
      request_unit_unload(get_unit_in_focus());
    break;
   case MENU_ORDER_WAKEUP:
    if(get_unit_in_focus())
      request_unit_wakeup(get_unit_in_focus());
    break;
   case MENU_ORDER_AUTO_SETTLER:
   case MENU_ORDER_AUTO_ATTACK:
    if(get_unit_in_focus())
      request_unit_auto(get_unit_in_focus());
    break;
   case MENU_ORDER_EXPLORE:
    if(get_unit_in_focus())
      request_new_unit_activity(get_unit_in_focus(), ACTIVITY_EXPLORE);
     break;
   case MENU_ORDER_GOTO:
    if(get_unit_in_focus())
      request_unit_goto();
    break;
   case MENU_ORDER_GOTO_CITY:
    if(get_unit_in_focus())
      popup_goto_dialog();
    break;
   case MENU_ORDER_DISBAND:
    if(get_unit_in_focus())
      request_unit_disband(get_unit_in_focus());
    break;
   case MENU_ORDER_BUILD_WONDER:
    if(get_unit_in_focus())
      request_unit_caravan_action(get_unit_in_focus(),
				  PACKET_UNIT_HELP_BUILD_WONDER);
     break;
   case MENU_ORDER_TRADE_ROUTE:
    if(get_unit_in_focus())
      request_unit_caravan_action(get_unit_in_focus(),
				  PACKET_UNIT_ESTABLISH_TRADE);
     break;
   case MENU_ORDER_NUKE:
    if(get_unit_in_focus())
      request_unit_nuke(get_unit_in_focus());
    break;
   case MENU_ORDER_WAIT:
    if(get_unit_in_focus())
      request_unit_wait(get_unit_in_focus());
    break;
   case MENU_ORDER_DONE:
    if(get_unit_in_focus())
      request_unit_move_done();
    break;
  }

}


/****************************************************************
...
*****************************************************************/
static void reports_menu_callback(Widget w, XtPointer client_data,
				  XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
   case MENU_REPORT_CITY:
    popup_city_report_dialog(0);
    break;
   case MENU_REPORT_ACTIVE_UNITS:
    popup_activeunits_report_dialog(0);
    break;
   case MENU_REPORT_TRADE:
    popup_trade_report_dialog(0);
    break;
   case MENU_REPORT_SCIENCE:
    popup_science_dialog(0);
    break;
   case MENU_REPORT_WOW:
    send_report_request(REPORT_WONDERS_OF_THE_WORLD);
    break;
   case MENU_REPORT_TOP_CITIES:
    send_report_request(REPORT_TOP_5_CITIES);
    break;
   case MENU_REPORT_DEMOGRAPHIC:
    send_report_request(REPORT_DEMOGRAPHIC);
    break;
   case MENU_REPORT_SPACESHIP:
    popup_spaceship_dialog(game.player_ptr);
    break;
  }
}


/****************************************************************
...
*****************************************************************/
static void help_menu_callback(Widget w, XtPointer client_data,
			       XtPointer garbage)
{
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
  case MENU_HELP_CONNECTING:
    popup_help_dialog_string(HELP_CONNECTING_ITEM);
    break;
  case MENU_HELP_CHATLINE:
    popup_help_dialog_string(HELP_CHATLINE_ITEM);
    break;
  case MENU_HELP_CONTROLS:
    popup_help_dialog_string(HELP_CONTROLS_ITEM);
    break;
  case MENU_HELP_PLAYING:
    popup_help_dialog_string(HELP_PLAYING_ITEM);
    break;
  case MENU_HELP_IMPROVEMENTS:
    popup_help_dialog_string(HELP_IMPROVEMENTS_ITEM);
    break;
  case MENU_HELP_UNITS:
    popup_help_dialog_string(HELP_UNITS_ITEM);
    break;
  case MENU_HELP_COMBAT:
    popup_help_dialog_string(HELP_COMBAT_ITEM);
    break;
  case MENU_HELP_ZOC:
    popup_help_dialog_string(HELP_ZOC_ITEM);
    break;
  case MENU_HELP_TECH:
    popup_help_dialog_string(HELP_TECHS_ITEM);
    break;
  case MENU_HELP_TERRAIN:
    popup_help_dialog_string(HELP_TERRAIN_ITEM);
    break;
  case MENU_HELP_WONDERS:
    popup_help_dialog_string(HELP_WONDERS_ITEM);
    break;
  case MENU_HELP_GOVERNMENT:
    popup_help_dialog_string(HELP_GOVERNMENT_ITEM);
    break;
  case MENU_HELP_HAPPINESS:
    popup_help_dialog_string(HELP_HAPPINESS_ITEM);
    break;
  case MENU_HELP_SPACE_RACE:
    popup_help_dialog_string(HELP_SPACE_RACE_ITEM);
    break;
  case MENU_HELP_COPYING:
    popup_help_dialog_string(HELP_COPYING_ITEM);
    break;
  case MENU_HELP_ABOUT:
    popup_help_dialog_string(HELP_ABOUT_ITEM);
    break;
  }
}

/****************************************************************
...
*****************************************************************/
void setup_menus(Widget parent_form)
{
  game_menu=create_menu("gamemenu", 
			game_menu_entries, game_menu_callback, 
			parent_form);
  kingdom_menu=create_menu("kingdommenu", 
			   kingdom_menu_entries, kingdom_menu_callback, 
			   parent_form);
  view_menu=create_menu("viewmenu", 
			view_menu_entries, view_menu_callback, 
			parent_form);
  orders_menu=create_menu("ordersmenu", 
			  order_menu_entries, orders_menu_callback, 
			  parent_form);
  reports_menu=create_menu("reportsmenu", 
			   reports_menu_entries, reports_menu_callback, 
			   parent_form);
  help_menu=create_menu("helpmenu", 
			help_menu_entries, help_menu_callback, 
			parent_form);
}

/****************************************************************
...
*****************************************************************/
struct Menu *create_menu(char *name, struct MenuEntry entries[], 
			 void (*menucallback)(Widget, XtPointer, XtPointer),
			 Widget parent)
{
  int i;
  struct Menu *mymenu;

  mymenu=fc_malloc(sizeof(struct Menu));

  mymenu->button=XtVaCreateManagedWidget(name, 
					 menuButtonWidgetClass, 
					 parent,
					 NULL);
    
  mymenu->shell=XtCreatePopupShell("menu", simpleMenuWidgetClass, 
				   mymenu->button, NULL, 0);

  
  for(i=0; entries[i].text && (entries[i].id != MENU_END_OF_LIST); ++i) {
    if (entries[i].id == MENU_SEPARATOR_LINE) {
      entries[i].w = XtCreateManagedWidget(NULL, smeLineObjectClass, 
					   mymenu->shell, NULL, 0);
    } else {
      entries[i].w = XtCreateManagedWidget(entries[i].text, smeBSBObjectClass, 
					   mymenu->shell, NULL, 0);
      XtAddCallback(entries[i].w, XtNcallback, menucallback, 
		    (XtPointer)entries[i].id );
    }
  }

  mymenu->entries=entries;

  return mymenu;
}

/****************************************************************
...
*****************************************************************/
void menu_entry_rename(struct Menu *pmenu, enum MenuID id, char *s)
{
  int i;
  
  for(i=0; pmenu->entries[i].text; ++i)
    if(pmenu->entries[i].id==id) {
      XtVaSetValues(pmenu->entries[i].w, XtNlabel, s, NULL);
      return;
    }
}

/****************************************************************
...
*****************************************************************/
void menu_entry_sensitive(struct Menu *pmenu, enum MenuID id, Bool s)
{
  int i;
  
  for(i=0; pmenu->entries[i].text; ++i)
    if(pmenu->entries[i].id==id) {
      XtVaSetValues(pmenu->entries[i].w, XtNsensitive, (s ? 1 : 0), NULL);
      return;
    }
}
