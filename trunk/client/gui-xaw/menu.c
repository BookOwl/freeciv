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
#include "fcintl.h"
#include "map.h"
#include "mem.h"
#include "support.h"
#include "unit.h"

#include "chatline.h"
#include "cityrep.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h" /* request_xxx and get_unit_in_focus */
#include "dialogs.h"
#include "finddlg.h"
#include "gotodlg.h"
#include "gui_stuff.h"
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


/* stuff for run-time mutable menu text */

#define MAX_LEN_TERR_NAME_DISP  9
#define MAX_VARIANTS  3
#define ACEL_SPACE  1

#define TEXT_ORDER_CITY_BUILD   0
#define TEXT_ORDER_CITY_ADD_TO  1

#define TEXT_ORDER_ROAD_ROAD      0
#define TEXT_ORDER_ROAD_RAILROAD  1

#define TEXT_ORDER_IRRIGATE_IRRIGATE   0
#define TEXT_ORDER_IRRIGATE_FARMLAND   1
#define TEXT_ORDER_IRRIGATE_CHANGE_TO  2

#define TEXT_ORDER_MINE_MINE       0
#define TEXT_ORDER_MINE_CHANGE_TO  1

#define TEXT_ORDER_TRANSFORM_TERRAIN       0
#define TEXT_ORDER_TRANSFORM_TRANSFORM_TO  1

#define TEXT_ORDER_POLLUTION_POLLUTION  0
#define TEXT_ORDER_POLLUTION_PARADROP   1

/* The acel strings, is the string that show which is the acelerator key
 * in the menus. No check is made to ensure that this is the acelerator
 * key. This is use for translation-layout porpouses in i18n texts
 * added by Serrada */

struct MenuEntry {
  char *text[MAX_VARIANTS+1];
  char *acel;               /* Acelerator key */
  enum  MenuID id;
  Widget w;
};

struct Menu {
  Widget button, shell;
  int maxitemlen;
  int maxacellen;
  struct MenuEntry *entries;
};

static struct Menu *menus[MENU_LAST];

static struct MenuEntry game_menu_entries[]={
    { { N_("Local Options"), 0        },      "", MENU_GAME_OPTIONS, 0 },
    { { N_("Message Options"), 0      },      "", MENU_GAME_MSG_OPTIONS, 0 },
    { { N_("Save Settings"), 0        },      "", MENU_GAME_SAVE_SETTINGS, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Players"), 0              },    "F3", MENU_GAME_PLAYERS, 0 },
    { { N_("Messages"), 0             },   "F10", MENU_GAME_MESSAGES, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Server Opt initial"), 0   },      "", MENU_GAME_SERVER_OPTIONS1, 0 },
    { { N_("Server Opt ongoing"), 0   },      "", MENU_GAME_SERVER_OPTIONS2, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Export Log"), 0           },      "", MENU_GAME_OUTPUT_LOG, 0 },
    { { N_("Clear Log"), 0            },      "", MENU_GAME_CLEAR_OUTPUT, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Disconnect"), 0           },      "", MENU_GAME_DISCONNECT, 0 },
    { { N_("Quit"), 0                 },      "", MENU_GAME_QUIT, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};

static struct MenuEntry kingdom_menu_entries[]={
    { { N_("Tax Rates"), 0            },     "T", MENU_KINGDOM_RATES, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Find City"), 0            }, "ctl-f", MENU_KINGDOM_FIND_CITY, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Revolution"), 0           },     "R", MENU_KINGDOM_REVOLUTION, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};

static struct MenuEntry view_menu_entries[]={
    { { N_("Map Grid"), 0             }, "ctl-g", MENU_VIEW_SHOW_MAP_GRID, 0 },
    { { N_("Center View"), 0          },     "c", MENU_VIEW_CENTER_VIEW, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};

static struct MenuEntry order_menu_entries[]={
    { { N_("Build City"),
        N_("Add to City"), 0          },     "b", MENU_ORDER_BUILD_CITY, 0 },
    { { N_("Build Road"),
        N_("Build Railroad"), 0       },     "r", MENU_ORDER_ROAD, 0 },
    { { N_("Build Irrigation"),
        N_("Build Farmland"),
        N_("Change to %s"), 0         },     "i", MENU_ORDER_IRRIGATE, 0 },
    { { N_("Build Mine"),
        N_("Change to %s"), 0         },     "m", MENU_ORDER_MINE, 0 },
    { { N_("Transform Terrain"),
        N_("Transform to %s"), 0      },     "o", MENU_ORDER_TRANSFORM, 0 },
    { { N_("Build Fortress"), 0       },     "f", MENU_ORDER_FORTRESS, 0 },
    { { N_("Build Airbase"), 0        },     "e", MENU_ORDER_AIRBASE, 0 },
    { { N_("Clean Pollution"),
	N_("Paradrop"), 0             },     "p", MENU_ORDER_POLLUTION, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Fortify"), 0              },     "f", MENU_ORDER_FORTIFY, 0 },
    { { N_("Sentry"), 0               },     "s", MENU_ORDER_SENTRY, 0 },
    { { N_("Pillage"), 0              },     "P", MENU_ORDER_PILLAGE, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Make Homecity"), 0        },     "h", MENU_ORDER_HOMECITY, 0 },
    { { N_("Unload"), 0               },     "u", MENU_ORDER_UNLOAD, 0 },
    { { N_("Wake up others"), 0       },     "W", MENU_ORDER_WAKEUP_OTHERS, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Auto Settler"), 0         },     "a", MENU_ORDER_AUTO_SETTLER, 0 },
    { { N_("Auto Attack"), 0          },     "a", MENU_ORDER_AUTO_ATTACK, 0 },
    { { N_("Auto Explore"), 0         },     "x", MENU_ORDER_AUTO_EXPLORE, 0 },
    { { N_("Connect"), 0              },     "C", MENU_ORDER_CONNECT, 0 },
    { { N_("Go to"), 0                },     "g", MENU_ORDER_GOTO, 0 },
    { { N_("Go/Airlift to City"), 0   },     "l", MENU_ORDER_GOTO_CITY, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Disband Unit"), 0         },     "D", MENU_ORDER_DISBAND, 0 },
    { { N_("Help Build Wonder"), 0    },     "b", MENU_ORDER_BUILD_WONDER, 0 },
    { { N_("Make Trade Route"), 0     },     "r", MENU_ORDER_TRADEROUTE, 0 },
    { { N_("Explode Nuclear"), 0      },     "N", MENU_ORDER_NUKE, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Wait"), 0                 },     "w", MENU_ORDER_WAIT, 0 },
    { { N_("Done"), 0                 },   "spc", MENU_ORDER_DONE, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};

static struct MenuEntry reports_menu_entries[]={
    { { N_("City Report"), 0          },    "F1", MENU_REPORT_CITY, 0 },
    { { N_("Military Report"), 0      },    "F2", MENU_REPORT_MILITARY, 0 },
    { { N_("Trade Report"), 0         },    "F5", MENU_REPORT_TRADE, 0 },
    { { N_("Science Report"), 0       },    "F6", MENU_REPORT_SCIENCE, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Wonders of the World"), 0 },    "F7", MENU_REPORT_WOW, 0 },
    { { N_("Top Five Cities"), 0      },    "F8", MENU_REPORT_TOP_CITIES, 0 },
    { { N_("Demographics"), 0         },   "F11", MENU_REPORT_DEMOGRAPHIC, 0 },
    { { N_("Spaceship"), 0            },   "F12", MENU_REPORT_SPACESHIP, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};

static struct MenuEntry help_menu_entries[]={
    { { N_("Help Languages"), 0       },      "", MENU_HELP_LANGUAGES, 0 },
    { { N_("Help Connecting"), 0      },      "", MENU_HELP_CONNECTING, 0 },
    { { N_("Help Controls"), 0        },      "", MENU_HELP_CONTROLS, 0 },
    { { N_("Help Chatline"), 0        },      "", MENU_HELP_CHATLINE, 0 },
    { { N_("Help Playing"), 0         },      "", MENU_HELP_PLAYING, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Help Improvements"), 0    },      "", MENU_HELP_IMPROVEMENTS, 0 },
    { { N_("Help Units"), 0           },      "", MENU_HELP_UNITS, 0 },
    { { N_("Help Combat"), 0          },      "", MENU_HELP_COMBAT, 0 },
    { { N_("Help ZOC"), 0             },      "", MENU_HELP_ZOC, 0 },
    { { N_("Help Technology"), 0      },      "", MENU_HELP_TECH, 0 },
    { { N_("Help Terrain"), 0         },      "", MENU_HELP_TERRAIN, 0 },
    { { N_("Help Wonders"), 0         },      "", MENU_HELP_WONDERS, 0 },
    { { N_("Help Government"), 0      },      "", MENU_HELP_GOVERNMENT, 0 },
    { { N_("Help Happiness"), 0       },      "", MENU_HELP_HAPPINESS, 0 },
    { { N_("Help Space Race"), 0      },      "", MENU_HELP_SPACE_RACE, 0 },
    { { 0                             },      "", MENU_SEPARATOR_LINE, 0 },
    { { N_("Copying"), 0              },      "", MENU_HELP_COPYING, 0 },
    { { N_("About"), 0                },      "", MENU_HELP_ABOUT, 0 },
    { { 0,                            },       0, MENU_END_OF_LIST, 0 }
};


static void create_menu(enum MenuIndex menu, char *name, struct MenuEntry entries[], 
			void (*menucallback)(Widget, XtPointer, XtPointer),
			Widget parent);
static void menu_entry_sensitive(enum MenuIndex menu, enum MenuID id, Bool s);
static void menu_entry_rename(enum MenuIndex menu, enum MenuID id, int var, char *terr);
static char *menu_entry_text(enum MenuIndex menu, int ent, int var, char *terr);

/****************************************************************
...
*****************************************************************/
void update_menus(void)
{
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    XtSetSensitive(menus[MENU_REPORT]->button, False);
    XtSetSensitive(menus[MENU_ORDER]->button, False);
    XtSetSensitive(menus[MENU_VIEW]->button, False);
    XtSetSensitive(menus[MENU_KINGDOM]->button, False);

    menu_entry_sensitive(MENU_GAME, MENU_GAME_OPTIONS, 0);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_MSG_OPTIONS, 0);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SAVE_SETTINGS, 0);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_PLAYERS, 0);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_MESSAGES, 0);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SERVER_OPTIONS1, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SERVER_OPTIONS2, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_OUTPUT_LOG, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_CLEAR_OUTPUT, 1);
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

    XtSetSensitive(menus[MENU_REPORT]->button, True);
    XtSetSensitive(menus[MENU_ORDER]->button, (punit != NULL));
    XtSetSensitive(menus[MENU_VIEW]->button, True);
    XtSetSensitive(menus[MENU_KINGDOM]->button, True);

    menu_entry_sensitive(MENU_GAME, MENU_GAME_OPTIONS, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_MSG_OPTIONS, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SAVE_SETTINGS, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_PLAYERS, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_MESSAGES, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SERVER_OPTIONS1, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_SERVER_OPTIONS2, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_OUTPUT_LOG, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_CLEAR_OUTPUT, 1);
    menu_entry_sensitive(MENU_GAME, MENU_GAME_DISCONNECT, 1);

    menu_entry_sensitive(MENU_REPORT, MENU_REPORT_SPACESHIP,
			 (game.player_ptr->spaceship.state!=SSHIP_NONE));

    if(punit) {
      enum tile_terrain_type  ttype;
      struct tile_type *      tinfo;

      ttype = map_get_tile(punit->x, punit->y)->terrain;
      tinfo = get_tile_type(ttype);

      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_BUILD_CITY, 
			   (can_unit_build_city(punit) ||
			    can_unit_add_to_city(punit)));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_ROAD, 
			   can_unit_do_activity(punit, ACTIVITY_ROAD) ||
			   can_unit_do_activity(punit, ACTIVITY_RAILROAD));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_IRRIGATE, 
			   can_unit_do_activity(punit, ACTIVITY_IRRIGATE));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_MINE, 
			   can_unit_do_activity(punit, ACTIVITY_MINE));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_TRANSFORM, 
			   can_unit_do_activity(punit, ACTIVITY_TRANSFORM));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_FORTRESS, 
			   can_unit_do_activity(punit, ACTIVITY_FORTRESS));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_AIRBASE,
			   can_unit_do_activity(punit, ACTIVITY_AIRBASE));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_POLLUTION, 
			   can_unit_do_activity(punit, ACTIVITY_POLLUTION) ||
			   can_unit_paradropped(punit));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_FORTIFY, 
			   can_unit_do_activity(punit, ACTIVITY_FORTIFY));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_SENTRY, 
			   can_unit_do_activity(punit, ACTIVITY_SENTRY));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_PILLAGE, 
			   can_unit_do_activity(punit, ACTIVITY_PILLAGE));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_HOMECITY, 
			   can_unit_change_homecity(punit));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_UNLOAD, 
			   get_transporter_capacity(punit)>0);
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_WAKEUP_OTHERS, 
			   is_unit_activity_on_tile(ACTIVITY_SENTRY,
				punit->x,punit->y));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_AUTO_SETTLER,
			   (can_unit_do_auto(punit)
			    && unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_AUTO_ATTACK, 
			   (can_unit_do_auto(punit)
			    && !unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_AUTO_EXPLORE, 
			   can_unit_do_activity(punit, ACTIVITY_EXPLORE));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_CONNECT, 
			   can_unit_do_connect(punit, ACTIVITY_IDLE)
			   && has_capability ("unit_connect", aconnection.capability));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_GOTO_CITY,
			   any_cities);
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_BUILD_WONDER,
			   unit_can_help_build_wonder_here(punit));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_TRADEROUTE,
			   unit_can_est_traderoute_here(punit));
      menu_entry_sensitive(MENU_ORDER, MENU_ORDER_NUKE,
                           unit_flag(punit->type, F_NUCLEAR));

      if (unit_flag(punit->type, F_CITIES) && map_get_city(punit->x, punit->y)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_BUILD_CITY,
			  TEXT_ORDER_CITY_ADD_TO, NULL);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_BUILD_CITY,
			  TEXT_ORDER_CITY_BUILD, NULL);
      }

      if ((tinfo->irrigation_result != T_LAST) && (tinfo->irrigation_result != ttype)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_IRRIGATE,
			  TEXT_ORDER_IRRIGATE_CHANGE_TO,
			  (get_tile_type(tinfo->irrigation_result))->terrain_name);
      }
      else if ((map_get_tile(punit->x,punit->y)->special&S_IRRIGATION) &&
	       improvement_exists(B_SUPERMARKET)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_IRRIGATE,
			  TEXT_ORDER_IRRIGATE_FARMLAND, NULL);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_IRRIGATE,
			  TEXT_ORDER_IRRIGATE_IRRIGATE, NULL);
      }

      if ((tinfo->mining_result != T_LAST) && (tinfo->mining_result != ttype)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_MINE,
			  TEXT_ORDER_MINE_CHANGE_TO,
			  (get_tile_type(tinfo->mining_result))->terrain_name);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_MINE,
			  TEXT_ORDER_MINE_MINE, NULL);
      }

      if ((tinfo->transform_result != T_LAST) && (tinfo->transform_result != ttype)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_TRANSFORM,
			  TEXT_ORDER_TRANSFORM_TRANSFORM_TO,
			  (get_tile_type(tinfo->transform_result))->terrain_name);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_TRANSFORM,
			  TEXT_ORDER_TRANSFORM_TERRAIN, NULL);
      }

      if (unit_flag(punit->type, F_PARATROOPERS)) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_POLLUTION,
			  TEXT_ORDER_POLLUTION_PARADROP, NULL);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_POLLUTION,
			  TEXT_ORDER_POLLUTION_POLLUTION, NULL);
      }

      if (map_get_tile(punit->x,punit->y)->special&S_ROAD) {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_ROAD,
			  TEXT_ORDER_ROAD_RAILROAD, NULL);
      } else {
	menu_entry_rename(MENU_ORDER, MENU_ORDER_ROAD,
			  TEXT_ORDER_ROAD_ROAD, NULL);
      }
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
    key_map_grid_toggle();
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





  struct unit *punit;
  size_t pane_num = (size_t)client_data;

  switch(pane_num) {
  case MENU_ORDER_BUILD_CITY:
    key_unit_build_city();
    break;
  case MENU_ORDER_ROAD:
    key_unit_road();
    break;
  case MENU_ORDER_IRRIGATE:
    key_unit_irrigate();
    break;
  case MENU_ORDER_MINE:
    key_unit_mine();
    break;
  case MENU_ORDER_TRANSFORM:
    key_unit_transform();
    break;
  case MENU_ORDER_FORTRESS:
    key_unit_fortress();
    break;
  case MENU_ORDER_AIRBASE:
    key_unit_airbase(); 
    break;
  case MENU_ORDER_POLLUTION: /* or MENU_ORDER_PARADROP */
    if((punit = get_unit_in_focus())) {
      if (unit_flag(punit->type, F_SETTLERS))
	key_unit_pollution();
      else
	key_unit_paradrop();
    }
    break;
  case MENU_ORDER_FORTIFY:
    key_unit_fortify();
    break;
  case MENU_ORDER_SENTRY:
    key_unit_sentry();
    break;
  case MENU_ORDER_PILLAGE:
    key_unit_pillage();
    break;
  case MENU_ORDER_HOMECITY:
    key_unit_homecity();
    break;
  case MENU_ORDER_UNLOAD:
    key_unit_unload();
    break;
  case MENU_ORDER_WAKEUP_OTHERS:
    key_unit_wakeup_others();
    break;
  case MENU_ORDER_AUTO_SETTLER:
    key_unit_auto_settle();
    break;
  case MENU_ORDER_AUTO_ATTACK:
    key_unit_auto_attack();
    break;
  case MENU_ORDER_AUTO_EXPLORE:
    key_unit_auto_explore();
    break;
  case MENU_ORDER_CONNECT:
    key_unit_connect();
    break;
  case MENU_ORDER_GOTO:
    key_unit_goto();
    break;
  case MENU_ORDER_GOTO_CITY:
    if(get_unit_in_focus())
      popup_goto_dialog();
    break;
  case MENU_ORDER_DISBAND:
    key_unit_disband();
    break;
  case MENU_ORDER_BUILD_WONDER:
    key_unit_build_wonder();
    break;
  case MENU_ORDER_TRADEROUTE:
    key_unit_traderoute();
    break;
  case MENU_ORDER_NUKE:
    key_unit_nuke();
    break;
  case MENU_ORDER_WAIT:
    key_unit_wait();
    break;
  case MENU_ORDER_DONE:
    key_unit_done();
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
   case MENU_REPORT_MILITARY:
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
  case MENU_HELP_LANGUAGES:
    popup_help_dialog_string(HELP_LANGUAGES_ITEM);
    break;
  case MENU_HELP_CONNECTING:
    popup_help_dialog_string(HELP_CONNECTING_ITEM);
    break;
  case MENU_HELP_CONTROLS:
    popup_help_dialog_string(HELP_CONTROLS_ITEM);
    break;
  case MENU_HELP_CHATLINE:
    popup_help_dialog_string(HELP_CHATLINE_ITEM);
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
 Initialize menus.
*****************************************************************/
void setup_menus(Widget parent_form)
{
  create_menu(MENU_GAME, "gamemenu", 
	      game_menu_entries, game_menu_callback, 
	      parent_form);
  create_menu(MENU_KINGDOM, "kingdommenu", 
	      kingdom_menu_entries, kingdom_menu_callback, 
	      parent_form);
  create_menu(MENU_VIEW, "viewmenu", 
	      view_menu_entries, view_menu_callback, 
	      parent_form);
  create_menu(MENU_ORDER, "ordersmenu", 
	      order_menu_entries, orders_menu_callback, 
	      parent_form);
  create_menu(MENU_REPORT, "reportsmenu", 
	      reports_menu_entries, reports_menu_callback, 
	      parent_form);
  create_menu(MENU_HELP, "helpmenu", 
	      help_menu_entries, help_menu_callback, 
	      parent_form);
}

/****************************************************************
 Determine whether menu item is active or not.
*****************************************************************/
int is_menu_item_active(enum MenuIndex menu, enum MenuID id)
{
  struct Menu *pmenu = menus[menu];
  int i;

  for(i=0; pmenu->entries[i].id != MENU_END_OF_LIST; ++i) {
    if(pmenu->entries[i].id==id) {
      return XtIsSensitive(pmenu->entries[i].w);
    }
  }

  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void create_menu(enum MenuIndex menu, char *name, struct MenuEntry entries[], 
		 void (*menucallback)(Widget, XtPointer, XtPointer),
		 Widget parent)
{
  int i, j;
  struct Menu *mymenu;
  char *xlt;
  int lstr;
  int litem;
  int lacel;

  mymenu=fc_malloc(sizeof(struct Menu));
  menus[menu]=mymenu;
  mymenu->entries=entries;

  /* Calculate the longest string in this menu so if the font
   * is fixed then we will know where to put the accelerator key.- Serrada */
  litem=lacel=0;
  for(i=0; entries[i].id != MENU_END_OF_LIST; ++i) {
    if (entries[i].id != MENU_SEPARATOR_LINE) {
      lstr=strlen(entries[i].acel);
      if (lacel<lstr) {
	lacel=lstr;
      }
      for(j=0; entries[i].text[j]; ++j) {
	xlt=_(entries[i].text[j]);
	lstr=strlen(xlt);
	if (strstr(xlt, "%s")) {
	  lstr+=MAX_LEN_TERR_NAME_DISP;
	}
	if (litem<lstr) {
	  litem=lstr;
	}
      }
    }
  }
  if ((litem>0) && (lacel>0)) {
    litem+=(ACEL_SPACE/2);
    lacel+=(ACEL_SPACE-(ACEL_SPACE/2));
  }
  mymenu->maxitemlen=litem;
  mymenu->maxacellen=lacel;

  I_L(mymenu->button=XtVaCreateManagedWidget(name, 
					 menuButtonWidgetClass, 
					 parent,
					 NULL));

  mymenu->shell=XtCreatePopupShell("menu", simpleMenuWidgetClass, 
				   mymenu->button, NULL, 0);

  for(i=0; entries[i].id != MENU_END_OF_LIST; ++i) {
    if (entries[i].id == MENU_SEPARATOR_LINE) {
      entries[i].w = XtCreateManagedWidget(NULL, smeLineObjectClass, 
					   mymenu->shell, NULL, 0);
    } else {
      xlt=menu_entry_text(menu, i, 0, "");
      entries[i].w = XtCreateManagedWidget(xlt, smeBSBObjectClass, 
					   mymenu->shell, NULL, 0);
      XtAddCallback(entries[i].w, XtNcallback, menucallback, 
		    (XtPointer)entries[i].id );
    }
  }

  return;
}

/****************************************************************
...
*****************************************************************/
void menu_entry_rename(enum MenuIndex menu, enum MenuID id, int var, char *terr)
{
  struct Menu *pmenu = menus[menu];
  int i;
  char *item;

  for(i=0; pmenu->entries[i].id != MENU_END_OF_LIST; ++i) {
    if(pmenu->entries[i].id==id) {
      item=menu_entry_text(menu, i, var, terr);
      XtVaSetValues(pmenu->entries[i].w, XtNlabel, item, NULL);
      return;
    }
  }
}

/****************************************************************
...
*****************************************************************/
void menu_entry_sensitive(enum MenuIndex menu, enum MenuID id, Bool s)
{
  struct Menu *pmenu = menus[menu];
  int i;

  for(i=0; pmenu->entries[i].id != MENU_END_OF_LIST; ++i) {
    if(pmenu->entries[i].id==id) {
      XtSetSensitive(pmenu->entries[i].w, (s ? True : False));
      return;
    }
  }
}

/****************************************************************
...
*****************************************************************/
char *menu_entry_text(enum MenuIndex menu, int ent, int var, char *terr)
{
  struct Menu *pmenu = menus[menu];
  static char retbuf[256];
  char tmp[256];
  char *xlt;

  xlt=_(pmenu->entries[ent].text[var]);

  if (strstr(xlt, "%s")) {
    my_snprintf(tmp, sizeof(tmp), xlt, terr);
    xlt=tmp;
  }

  my_snprintf(retbuf, sizeof(retbuf), "%*.*s%*.*s",
	  -pmenu->maxitemlen, pmenu->maxitemlen, xlt,
	  pmenu->maxacellen, pmenu->maxacellen, pmenu->entries[ent].acel);

  return (retbuf);
}
