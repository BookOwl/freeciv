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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <pwd.h>

#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

/*
#include <mui/tearoffpanel_mcc.h>
#include <mui/tearoffbay_mcc.h>
*/

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/muimaster_protos.h>
#include <clib/socket_protos.h>
#include <clib/utility_protos.h>
#include <clib/intuition_protos.h>

#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/socket.h>
#include <proto/utility.h>
#include <proto/intuition.h>

#include <proto/usergroup.h>

#include "capability.h"
#include "log.h"
#include "mem.h"
#include "version.h"

#include "chatline.h"
#include "citydlg_g.h"
#include "cityrep_g.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h"
#include "dialogs_g.h"
#include "gui_main.h"
#include "gotodlg.h"
#include "finddlg.h"
#include "helpdata.h"		/* boot_help_texts() */
#include "mapctrl.h"
#include "mapview.h"
#include "options.h"
#include "plrdlg.h"
#include "ratesdlg.h"
#include "repodlgs_g.h"
#include "spaceshipdlg_g.h"
#include "tilespec.h"

/* Amiga Client Stuff */
#include "muistuff.h"
#include "overviewclass.h"
#include "mapclass.h"

void popup_option_dialog(void);	/* gamedlg.c */


extern char metaserver[];
extern char server_host[];
extern char name[];
extern int server_port;
static char *tile_set_name;
extern int seconds_to_turndone;

/**************************************************************************
  Print the usage information, including one line help on each option,
  to stderr.
**************************************************************************/
static void print_usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s [option ...]\nValid options are:\n", argv0);
  fprintf(stderr, "  -h, --help\t\tPrint a summary of the options.\n");
  fprintf(stderr, "  -l, --log=FILE\tUse FILE as logfile.\n");
  fprintf(stderr, "  -N, --Name=NAME\tUse NAME.\n");
  fprintf(stderr, "  -p, --port=PORT\tConnect to PORT.\n");
  fprintf(stderr, "  -s, --server=SERVER\tConnect to the server SERVER.\n");
  fprintf(stderr, "  -m, --metaserver=METASERVER\tSet metaserver address to METASERVER.\n");
#ifdef DEBUG
  fprintf(stderr, "  -d, --debug=LEVEL\tSet debug log LEVEL (0,1,2,3,"
	  "or 3:file1,min,max:...)\n");
#else
  fprintf(stderr, "  -d, --debug=LEVEL\tSet debug log LEVEL (0,1,2).\n");
#endif
  fprintf(stderr, "  -t, --tiles=DIR\tLook in directory DIR for the tiles.\n");
  fprintf(stderr, "  -v, --version\t\tPrint the version number.\n");
}

/**************************************************************************
...
**************************************************************************/
static void parse_options(int argc, char **argv)
{
  char *logfile = NULL;
  char *option = NULL;		/* really use as a pointer */
  int loglevel;
  int i;

  /* set default argument values */
  loglevel = LOG_NORMAL;
  server_port = DEFAULT_SOCK_PORT;
  strcpy(server_host, "localhost");
  strcpy(metaserver, METALIST_ADDR);
  name[0] = '\0';

  i = 1;
  while (i < argc)
  {
    if (is_option("--help", argv[i]))
    {
      print_usage(argv[0]);
      exit(0);
    }
    else if (is_option("--version", argv[i]))
    {
      fprintf(stderr, "%s\n", FREECIV_NAME_VERSION);
      exit(0);
    }
    else if ((option = get_option("--log", argv, &i, argc)) != NULL)
      logfile = mystrdup(option);	/* never free()d */
    else if ((option = get_option("--name", argv, &i, argc)) != NULL)
      strcpy(name, option);
    else if ((option = get_option("--metaserver", argv, &i, argc)) != NULL)
      strcpy(metaserver, option);
    else if ((option = get_option("--port", argv, &i, argc)) != NULL)
      server_port = atoi(option);
    else if ((option = get_option("--server", argv, &i, argc)) != NULL)
      strcpy(server_host, option);
    else if ((option = get_option("--debug", argv, &i, argc)) != NULL)
    {
      loglevel = log_parse_level_str(option);
      if (loglevel == -1)
      {
	exit(1);
      }
    }
    else if ((option = get_option("--tiles", argv, &i, argc)) != NULL)
      tile_set_name = option;
    i += 1;
  }

  log_init(logfile, loglevel, NULL);

  /* after log_init: */
  if (name[0] == '\0')
  {
    strcpy(name, user_username());
  }
}

/**************************************************************************
 This function should be called every 500ms
 it lets the unit blink and update the timeout
 belongs maybe to civclient.c
**************************************************************************/
void handle_timer(void)
{
  static int flip;

  if (get_client_state() == CLIENT_GAME_RUNNING_STATE)
  {

    if (game.player_ptr->is_connected && game.player_ptr->is_alive &&
	!game.player_ptr->turn_done)
    {
      int i, is_waiting, is_moving;

      for (i = 0, is_waiting = 0, is_moving = 0; i < game.nplayers; i++)
	if (game.players[i].is_alive && game.players[i].is_connected)
	{
	  if (game.players[i].turn_done)
	    is_waiting++;
	  else
	    is_moving++;
	}

      if (is_moving == 1 && is_waiting)
	update_turn_done_button(0);	/* stress the slow player! */
    }

    blink_active_unit();

    if (flip)
    {
      update_timeout_label();
      if (seconds_to_turndone)
	seconds_to_turndone--;
    }

    flip = !flip;
  }
}


STATIC BOOL connected;		/* TRUE, if connected to the server */
STATIC int connected_sock;

Object *app;
Object *main_menu;
Object *main_order_menu;
Object *main_wnd;
Object *main_output_listview;
Object *main_chatline_string;

Object *main_people_text;
Object *main_year_text;
Object *main_gold_text;
Object *main_tax_text;
Object *main_info_group;
Object *main_turndone_group;
Object *main_turndone_button;
Object *main_econ_sprite[10];
Object *main_bulb_sprite;
Object *main_sun_sprite;
Object *main_government_sprite;
Object *main_timeout_text;

Object *main_unitname_text;
Object *main_moves_text;
Object *main_terrain_text;
Object *main_hometown_text;
Object *main_unit_unit;

Object *main_map_area;
Object *main_map_hscroller;
Object *main_map_vscroller;

Object *main_overview_area;
Object *main_overview_group;

static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static ULONG timer_outstanding;


/****************************************************************
 This describes the pull down menu
*****************************************************************/
#define MAKE_TITLE(name,userdata) {NM_TITLE,name,NULL,0,0,(APTR)userdata}
#define MAKE_ITEM(name,userdata,shortcut,flags) {NM_ITEM,name,shortcut,flags,0,(APTR)userdata}
#define MAKE_SIMPLEITEM(name,userdata) {NM_ITEM,name,NULL, 0,0, (APTR)userdata}
#define MAKE_SEPERATOR {NM_ITEM, NM_BARLABEL, NULL, 0, 0, (APTR)0}
#define MAKE_END {NM_END,NULL,0,0,0,(APTR)0}

static struct NewMenu MenuData[] =
{
  MAKE_TITLE("Game", MENU_GAME),
  MAKE_SIMPLEITEM("Options...", MENU_GAME_OPTIONS),
  MAKE_SIMPLEITEM("Message Options...", MENU_GAME_MSG_OPTIONS),
  MAKE_SIMPLEITEM("Save Settings", MENU_GAME_SAVE_SETTINGS),
  MAKE_SEPERATOR,
  MAKE_ITEM("Players...", MENU_GAME_PLAYERS, "F3", NM_COMMANDSTRING),
  MAKE_SIMPLEITEM("Message...", MENU_GAME_MESSAGES),
  MAKE_SIMPLEITEM("Server opt initial...", MENU_GAME_SERVER_OPTIONS1),
  MAKE_SIMPLEITEM("Server opt ongoing...", MENU_GAME_SERVER_OPTIONS2),
  MAKE_SIMPLEITEM("Export Log", MENU_GAME_OUTPUT_LOG),
  MAKE_SIMPLEITEM("Clear Log", MENU_GAME_CLEAR_OUTPUT),
  MAKE_SIMPLEITEM("Disconnect...", MENU_GAME_DISCONNECT),
  MAKE_SEPERATOR,
  MAKE_ITEM("Quit", MENU_GAME_QUIT, "Q", 0),

  MAKE_TITLE("Kingdom", MENU_KINGDOM),
MAKE_ITEM("Tax Rate...", MENU_KINGDOM_TAX_RATE, "SHIFT T", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Find City...", MENU_KINGDOM_FIND_CITY, "SHIFT C", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("REVOLUTION...", MENU_KINGDOM_REVOLUTION, "SHIFT R", NM_COMMANDSTRING),

  MAKE_TITLE("View", MENU_VIEW),
  MAKE_ITEM("Show Map Grid", MENU_VIEW_SHOW_MAP_GRID, "CTRL G", NM_COMMANDSTRING | MENUTOGGLE),
  MAKE_ITEM("Center View", MENU_VIEW_CENTER_VIEW, "c", NM_COMMANDSTRING),

  MAKE_TITLE("Order", MENU_ORDER),
  MAKE_ITEM("Auto Settler", MENU_ORDER_AUTO_SETTLER, "a", NM_COMMANDSTRING),
  MAKE_ITEM("Auto Attack", MENU_ORDER_AUTO_ATTACK, "SHIFT A", NM_COMMANDSTRING),
  MAKE_ITEM("Build City", MENU_ORDER_CITY, "b", NM_COMMANDSTRING),
  MAKE_ITEM("Build Road", MENU_ORDER_ROAD, "r", NM_COMMANDSTRING),
  MAKE_ITEM("Build Mine", MENU_ORDER_MINE, "m", NM_COMMANDSTRING),
  MAKE_ITEM("Build Irrigation", MENU_ORDER_IRRIGATE, "i", NM_COMMANDSTRING),
MAKE_ITEM("Transform Terrain", MENU_ORDER_TRANSFORM, "o", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Build Fortress", MENU_ORDER_FORTRESS, "SHIFT F", NM_COMMANDSTRING),
  MAKE_ITEM("Build Airbase", MENU_ORDER_AIRBASE, "e", NM_COMMANDSTRING),
  MAKE_ITEM("Clean Pollution", MENU_ORDER_POLLUTION, "p", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Auto Explore", MENU_ORDER_EXPLORE, "x", NM_COMMANDSTRING),
  MAKE_ITEM("Explode Nuclear", MENU_ORDER_NUKE, "SHIFT N", NM_COMMANDSTRING),
  MAKE_ITEM("Unload", MENU_ORDER_UNLOAD, "u", NM_COMMANDSTRING),
  MAKE_ITEM("Go to", MENU_ORDER_GOTO, "g", NM_COMMANDSTRING),
MAKE_ITEM("Go/Airlist to City", MENU_ORDER_GOTO_CITY, "l", NM_COMMANDSTRING),
  MAKE_ITEM("Make Homecity", MENU_ORDER_HOMECITY, "h", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Fortify", MENU_ORDER_FORTIFY, "f", NM_COMMANDSTRING),
  MAKE_ITEM("Sentry", MENU_ORDER_SENTRY, "s", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Disband Unit", MENU_ORDER_DISBAND, "SHIFT D", NM_COMMANDSTRING),
MAKE_ITEM("Wake up others", MENU_ORDER_WAKEUP, "SHIFT W", NM_COMMANDSTRING),
  MAKE_ITEM("Wait", MENU_ORDER_WAIT, "w", NM_COMMANDSTRING),
  MAKE_ITEM("Pillage", MENU_ORDER_PILLAGE, "SHIFT P", NM_COMMANDSTRING),
  MAKE_ITEM("Help Build Wonder", MENU_ORDER_BUILD_WONDER, "SHIFT B", NM_COMMANDSTRING),
  MAKE_ITEM("Make Trade Route", MENU_ORDER_TRADE_ROUTE, "SHIFT R", NM_COMMANDSTRING),
  MAKE_ITEM("Done", MENU_ORDER_DONE, "SPACE", NM_COMMANDSTRING),

  MAKE_TITLE("Report", MENU_REPORT),
  MAKE_ITEM("City Report...", MENU_REPORT_CITY, "F1", NM_COMMANDSTRING),
MAKE_ITEM("Science Report...", MENU_REPORT_SCIENCE, "F6", NM_COMMANDSTRING),
  MAKE_ITEM("Trade Report...", MENU_REPORT_TRADE, "F5", NM_COMMANDSTRING),
  MAKE_ITEM("Military Report...", MENU_REPORT_ACTIVE_UNITS, "F2", NM_COMMANDSTRING),
  MAKE_SEPERATOR,
  MAKE_ITEM("Wonders of the World", MENU_REPORT_WOW, "F7", NM_COMMANDSTRING),
  MAKE_ITEM("Top 5 Cities", MENU_REPORT_TOP_CITIES, "F8", NM_COMMANDSTRING),
  MAKE_ITEM("Demographics", MENU_REPORT_DEMOGRAPHIC, "F9", NM_COMMANDSTRING),
  MAKE_ITEM("Spaceship", MENU_REPORT_SPACESHIP, "F10", NM_COMMANDSTRING),

  MAKE_TITLE("Help", MENU_HELP),
  MAKE_SIMPLEITEM("Connecting", MENU_HELP_CONNECTING),
  MAKE_SIMPLEITEM("Controls", MENU_HELP_CONTROLS),
  MAKE_SIMPLEITEM("Chatline", MENU_HELP_CHATLINE),
  MAKE_SIMPLEITEM("Playing", MENU_HELP_PLAYING),
  MAKE_SIMPLEITEM("Improvements", MENU_HELP_IMPROVEMENTS),
  MAKE_SIMPLEITEM("Units", MENU_HELP_UNITS),
  MAKE_SIMPLEITEM("Combat", MENU_HELP_COMBAT),
  MAKE_SIMPLEITEM("ZOC", MENU_HELP_ZOC),
  MAKE_SIMPLEITEM("Technology", MENU_HELP_TECH),
  MAKE_SIMPLEITEM("Terrain", MENU_HELP_TERRAIN),
  MAKE_SIMPLEITEM("Wonders", MENU_HELP_WONDERS),
  MAKE_SIMPLEITEM("Government", MENU_HELP_GOVERNMENT),
  MAKE_SIMPLEITEM("Happiness", MENU_HELP_HAPPINESS),
  MAKE_SIMPLEITEM("Space Race", MENU_HELP_SPACE_RACE),
  MAKE_SEPERATOR,
  MAKE_SIMPLEITEM("Copying", MENU_HELP_COPYING),
  MAKE_SIMPLEITEM("About", MENU_HELP_ABOUT),

  MAKE_END
};

/****************************************************************
 Enables the Turn Done button
*****************************************************************/
void enable_turn_done_button(void)
{
  if (game.player_ptr->ai.control && !ai_manual_turn_done)
    user_ended_turn();
  set(main_turndone_button, MUIA_Disabled, !(!game.player_ptr->ai.control || ai_manual_turn_done));

  if(sound_bell_at_new_turn)
    DisplayBeep(NULL);
}


/****************************************************************
 Callback for the chatline
*****************************************************************/
static void inputline_return(void)	/* from chatline.c */
{
  struct packet_generic_message apacket;
  char *theinput;
  int contents;

  if (!(theinput = (char *) xget(main_chatline_string, MUIA_String_Contents)))
    return;

  if (*theinput)
  {
    strncpy(apacket.message, theinput, MAX_LEN_MSG - MAX_LEN_USERNAME);
    apacket.message[MAX_LEN_MSG - MAX_LEN_USERNAME] = '\0';
    send_packet_generic_message(&aconnection, PACKET_CHAT_MSG, &apacket);
    contents = TRUE;
  }
  else
    contents = FALSE;

  nnset(main_chatline_string, MUIA_String_Contents, "");
  if (contents)
    activate(main_chatline_string);
}

/****************************************************************
 General Callback for freeciv used by the menu or keyboard
*****************************************************************/
static void control_callback(ULONG * value)
{
  if (*value)
  {
    switch (*value)
    {
    case UNIT_NORTH:
      key_unit_north();
      break;
    case UNIT_SOUTH:
      key_unit_south();
      break;
    case UNIT_EAST:
      key_unit_east();
      break;
    case UNIT_WEST:
      key_unit_west();
      break;
    case UNIT_NORTH_EAST:
      key_unit_north_east();
      break;
    case UNIT_NORTH_WEST:
      key_unit_north_west();
      break;
    case UNIT_SOUTH_EAST:
      key_unit_south_east();
      break;
    case UNIT_SOUTH_WEST:
      key_unit_south_west();
      break;
    case UNIT_POPUP_CITY:
      {
	struct unit *punit;
	if ((punit = get_unit_in_focus()))
	{
	  struct city *pcity = map_get_city(punit->x, punit->y);
	  if (pcity)
	  {
	    popup_city_dialog(pcity, 0);
	  }
	}
      }
      break;

    case UNIT_POPUP_UNITLIST:
      {
	struct unit *punit;
	if ((punit = get_unit_in_focus()))
	{
	  struct tile *ptile = map_get_tile(punit->x, punit->y);
	  if (ptile)
	  {
	    popup_unit_select_dialog(ptile);
	  }
	}
      }
      break;

    case UNIT_ESCAPE:
      key_cancel_action();
      break;
    case END_TURN:
      key_end_turn();
      break;
    case NEXT_UNIT:
      advance_unit_focus();	/*focus_to_next_unit(); */
      break;

    case MENU_GAME_OPTIONS:
      popup_option_dialog();
      break;
    case MENU_GAME_MSG_OPTIONS:
      break;
    case MENU_GAME_SAVE_SETTINGS:
      break;
    case MENU_GAME_PLAYERS:
      popup_players_dialog();
      break;
    case MENU_GAME_MESSAGES:
      break;
    case MENU_GAME_SERVER_OPTIONS1:
      send_report_request(REPORT_SERVER_OPTIONS1);
      break;
    case MENU_GAME_SERVER_OPTIONS2:
      send_report_request(REPORT_SERVER_OPTIONS2);
      break;
    case MENU_GAME_OUTPUT_LOG:
      break;
    case MENU_GAME_CLEAR_OUTPUT:
      clear_output_window();
      break;
    case MENU_GAME_DISCONNECT:
      disconnect_from_server();
      break;
    case MENU_GAME_QUIT:
      DoMethod(app, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
      break;

    case MENU_KINGDOM_TAX_RATE:
      popup_rates_dialog();
      break;
    case MENU_KINGDOM_FIND_CITY:
      popup_find_dialog();
      break;
    case MENU_KINGDOM_REVOLUTION:
      popup_revolution_dialog();
      break;

    case MENU_VIEW_SHOW_MAP_GRID:
      key_map_grid();
      break;
    case MENU_VIEW_CENTER_VIEW:
      request_center_focus_unit() /*center_on_unit() */ ;
      break;

    case MENU_ORDER_AUTO_SETTLER:
      key_unit_auto();
      break;
    case MENU_ORDER_AUTO_ATTACK:
      key_unit_auto();
      break;
    case MENU_ORDER_MINE:
      key_unit_mine();
      break;
    case MENU_ORDER_IRRIGATE:
      key_unit_irrigate();
      break;
    case MENU_ORDER_TRANSFORM:
      key_unit_transform();
      break;
    case MENU_ORDER_FORTRESS:
      key_unit_fortify();
      break;
    case MENU_ORDER_AIRBASE:
      key_unit_airbase();
      break;

    case MENU_ORDER_CITY:
      key_unit_build_city();
      break;
    case MENU_ORDER_ROAD:
      key_unit_road();
      break;
    case MENU_ORDER_POLLUTION:
      key_unit_clean_pollution();
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
    case MENU_ORDER_EXPLORE:
      key_unit_explore();
      break;
    case MENU_ORDER_HOMECITY:
      key_unit_homecity();
      break;
    case MENU_ORDER_WAIT:
      key_unit_wait();
      break;
    case MENU_ORDER_UNLOAD:
      key_unit_unload();
      break;
    case MENU_ORDER_WAKEUP:
      key_unit_wakeup();
      break;
    case MENU_ORDER_GOTO:
      key_unit_goto();
      break;
    case MENU_ORDER_GOTO_CITY:
      if (get_unit_in_focus())
	popup_goto_dialog();
      break;
    case MENU_ORDER_DISBAND:
      key_unit_disband();
      break;
    case MENU_ORDER_BUILD_WONDER:
      key_unit_build_city();
      break;
    case MENU_ORDER_TRADE_ROUTE:
      key_unit_road();
      break;
    case MENU_ORDER_DONE:
      key_unit_done();
      break;
    case MENU_ORDER_NUKE:
      key_unit_nuke();
      break;

    case MENU_REPORT_CITY:
      popup_city_report_dialog(0);
      break;
    case MENU_REPORT_SCIENCE:
      popup_science_dialog(0);
      break;
    case MENU_REPORT_TRADE:
      popup_trade_report_dialog(0);
      break;
    case MENU_REPORT_ACTIVE_UNITS:
      popup_activeunits_report_dialog(0);
      break;
    case MENU_REPORT_DEMOGRAPHIC:
      send_report_request(REPORT_DEMOGRAPHIC);
      break;
    case MENU_REPORT_TOP_CITIES:
      send_report_request(REPORT_TOP_5_CITIES);
      break;
    case MENU_REPORT_WOW:
      send_report_request(REPORT_WONDERS_OF_THE_WORLD);
      break;
    case MENU_REPORT_SPACESHIP:
      popup_spaceship_dialog(game.player_ptr);
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
    case MENU_HELP_CONNECTING:
      popup_help_dialog_string(HELP_CONNECTING_ITEM);
      break;
    case MENU_HELP_CHATLINE:
      popup_help_dialog_string(HELP_CHATLINE_ITEM);
      break;
    }
  }
}

/****************************************************************
 Do a function on a special unit
*****************************************************************/
void do_unit_function(struct unit *punit, ULONG value)
{
  if (value != UNIT_ACTIVATE)
  {
    extern struct unit *punit_focus;
    struct unit *punit_oldfocus = punit_focus;
    punit_focus = punit;
    control_callback(&value);
    punit_focus = punit_oldfocus;
  }
  else
  {
    set_unit_focus(punit);
  }
}

/****************************************************************
 Callback for Taxrates sprites in the main window
*****************************************************************/
static void taxrates_callback(LONG * number)
{
  int tax_end, lux_end, sci_end;
  size_t i;
  int delta = 10;
  struct packet_player_request packet;

  if (get_client_state() != CLIENT_GAME_RUNNING_STATE)
    return;

  i = (size_t) * number;

  lux_end = game.player_ptr->economic.luxury;
  sci_end = lux_end + game.player_ptr->economic.science;
  tax_end = 100;

  packet.luxury = game.player_ptr->economic.luxury;
  packet.science = game.player_ptr->economic.science;
  packet.tax = game.player_ptr->economic.tax;

  i *= 10;
  if (i < lux_end)
  {
    packet.luxury -= delta;
    packet.science += delta;
  }
  else if (i < sci_end)
  {
    packet.science -= delta;
    packet.tax += delta;
  }
  else
  {
    packet.tax -= delta;
    packet.luxury += delta;
  }
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_RATES);
}

/****************************************************************
 Sends a timer
*****************************************************************/
static struct timerequest *send_timer(ULONG secs, ULONG mics)
{
  struct timerequest *treq = (struct timerequest *) AllocVec(sizeof(struct timerequest), MEMF_CLEAR | MEMF_PUBLIC);
  if (treq)
  {
    *treq = *timer_req;
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = secs;
    treq->tr_time.tv_micro = mics;
    SendIO((struct IORequest *) treq);
    timer_outstanding++;
  }
  return treq;
}

/****************************************************************
 Cleanup the timer initialisations
*****************************************************************/
static void free_timer(void)
{
  if (timer_req)
  {
    if (timer_req->tr_node.io_Device)
    {
      while (timer_outstanding)
      {
	if (Wait(1L << timer_port->mp_SigBit | 4096) & 4096)
	  break;
	timer_outstanding--;
      }

      CloseDevice((struct IORequest *) timer_req);
    }
    DeleteIORequest(timer_req);
  }

  if (timer_port)
    DeleteMsgPort(timer_port);
}

/****************************************************************
 Initialize the timer stuff
*****************************************************************/
static int init_timer(void)
{
  if ((timer_port = CreateMsgPort()))
  {
    if (timer_req = (struct timerequest *) CreateIORequest(timer_port, sizeof(struct timerequest)))
    {
      if (!OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *) timer_req, 0))
      {
	return TRUE;
      }
      free_timer();
    }
  }
  return FALSE;
}

/****************************************************************
 Remove the custom classes
*****************************************************************/
static void free_classes(void)
{
  delete_map_class();
  delete_overview_class();
}

/****************************************************************
 Add custom classes
*****************************************************************/
static int init_classes(void)
{
  if (create_overview_class())
    if (create_map_class())
      return TRUE;

  return FALSE;
}

/****************************************************************
 Cleanup the gui initialisations
*****************************************************************/
static void free_gui(void)
{
  if (app)
    MUI_DisposeObject(app);
  free_classes();
}

/****************************************************************
 Init gui
*****************************************************************/
static int init_gui(void)
{
  init_standart_hook();
  if (!init_classes())
    return FALSE;

  app = ApplicationObject,
    MUIA_Application_Title, "Freeciv Client",
    MUIA_Application_Version, "$VER: civclient 1.5" __AMIGADATE__,
    MUIA_Application_Copyright, "�1999 by Sebastian Bauer",
    MUIA_Application_Author, "Sebastian Bauer",
    MUIA_Application_Description, "Client for Freeciv",
    MUIA_Application_Base, "CIVCLIENT",

    SubWindow, main_wnd = WindowObject,
        MUIA_Window_Title, "Freeciv",
        MUIA_Window_ID, 'MAIN',
        MUIA_Window_Menustrip, main_menu = MakeMenu(MenuData),

        WindowContents, VGroup,
            Child, HGroup,
                MUIA_Weight, 100,
                Child, VGroup,
                    MUIA_HorizWeight, 0,
                    Child, main_overview_group = HGroup,
                        Child, main_overview_area = MakeOverview(80, 50),
                        End,
                    Child, VGroup,
                        TextFrame,
                        Child, main_people_text = TextObject, End,
                        Child, main_year_text = TextObject, End,
                        Child, main_gold_text = TextObject, End,
                        Child, main_tax_text = TextObject, End,
                        End,
                    Child, main_info_group = VGroup,
                        Child, main_turndone_group = HGroup,
                            Child, main_turndone_button = MakeButton("Turn Done"),
                            End,
                        End,
                    Child, HGroup,
                        TextFrame,
                        Child, VGroup,
                            Child, main_unitname_text = TextObject, End,
                            Child, main_moves_text = TextObject, End,
                            Child, main_terrain_text = TextObject, End,
                            Child, main_hometown_text = TextObject, End,
                            End,
                        Child, main_unit_unit = UnitObject, End,
                        End,
                    Child, HVSpace,
                    End,

                Child, ColGroup(2), /* Map and scrollers */
                    MUIA_Group_Spacing, 0,
                    Child, main_map_area = MakeMap(),
                    Child, main_map_vscroller = ScrollbarObject,
                        MUIA_Group_Horiz, FALSE,
                        End,

                    Child, main_map_hscroller = ScrollbarObject,
                        MUIA_Group_Horiz, TRUE,
                        End,
                    End,
                End,
            Child, NListviewObject,
                MUIA_Weight, 30,
                MUIA_NListview_NList, main_output_listview = NListObject,
                    ReadListFrame,
                    MUIA_NList_Input, FALSE,
                    MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Char,
                    MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                    MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                    MUIA_NList_AutoCopyToClip, TRUE,
                    End,
                End,
            Child, main_chatline_string = MakeString("", 64),
            End,
        End,
    End;

  if (app)
  {
    LONG i = 0;

    main_order_menu = (Object *) DoMethod(main_menu, MUIM_FindUData, MENU_ORDER);

    /* Main Wnd */
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    SetAttrs(main_map_area,
	     MUIA_Map_Overview, main_overview_area,
	     MUIA_Map_HScroller, main_map_hscroller,
	     MUIA_Map_VScroller, main_map_vscroller,
	     TAG_DONE);

    DoMethod(main_map_area, MUIM_Notify, MUIA_Map_Click, MUIV_EveryTime, app, 4, MUIM_CallHook, &standart_hook, main_map_click, MUIV_TriggerValue);
    DoMethod(main_chatline_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, app, 3, MUIM_CallHook, &standart_hook, inputline_return);
    DoMethod(main_turndone_button, MUIM_Notify, MUIA_Pressed, FALSE, main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, END_TURN);

    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 4", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_WEST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 6", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_EAST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 8", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_NORTH);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 2", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_SOUTH);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 7", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_NORTH_WEST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 9", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_NORTH_EAST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 1", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_SOUTH_WEST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 3", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_SOUTH_EAST);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad 5", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, NEXT_UNIT);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "numericpad enter", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, END_TURN);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "return", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_POPUP_CITY);
    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, "escape", main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, UNIT_ESCAPE);

    /* Menu */
    while (MenuData[i].nm_Type != NM_END)
    {
      if (MenuData[i].nm_Flags & NM_COMMANDSTRING && MenuData[i].nm_CommKey)
      {
	DoMethod(main_wnd, MUIM_Notify, MUIA_Window_InputEvent, MenuData[i].nm_CommKey, main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, MenuData[i].nm_UserData);
      }
      i++;
    }

    DoMethod(main_wnd, MUIM_Notify, MUIA_Window_MenuAction, MUIV_EveryTime, main_wnd, 4, MUIM_CallHook, &standart_hook, control_callback, MUIV_TriggerValue);

    append_output_window(
      "Freeciv is free software and you are welcome to distribute copies of"
       " it\nunder certain conditions; See the \"Copying\" item on the Help"
			  " menu.\nNow.. Go give'em hell!");

    return TRUE;
  }
  return FALSE;
}

/****************************************************************
 The main loop
*****************************************************************/
static void loop(void)
{
  ULONG sigs = 0;
  ULONG secs, mics;
  CurrentTime(&secs, &mics);

  while (DoMethod(app, MUIM_Application_NewInput, &sigs) != MUIV_Application_ReturnID_Quit)
  {
    ULONG timer_sig = (1L << timer_port->mp_SigBit);

    if (sigs)
    {
      ULONG mask = sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | timer_sig;
      BOOL force_timer = FALSE;

      if (connected)
      {
	fd_set readfs;
	int sel;

	FD_ZERO(&readfs);
	FD_SET(aconnection.sock, &readfs);

	sel = WaitSelect(connected_sock + 1, &readfs, NULL, NULL, NULL, &mask);
	if (sel > 0)
	{
	  if (FD_ISSET(connected_sock, &readfs))	// handle_net_input(); /* in clinet.c */

	    input_from_server(connected_sock);
	}
	else if (sel < 0)
	{
	  printf("%s\n", strerror(errno));
	  break;
	}
      }
      else
	mask = Wait(mask);

      if (!(mask & timer_sig))
      {
	ULONG newsecs, newmics;
	CurrentTime(&newsecs, &newmics);
	if (newsecs - secs > 4)
	  force_timer = TRUE;
      }

      if ((mask & SIGBREAKF_CTRL_C) || (mask & SIGBREAKF_CTRL_D))
	break;
      if ((mask & timer_sig) || force_timer)
      {
	struct timerequest *tr;

	/* Remove the timer from the port */
	while ((tr = (struct timerequest *) GetMsg(timer_port)))
	{
	  FreeVec(tr);
	  timer_outstanding--;
	}

	send_timer(0, 500000);
	CurrentTime(&secs, &mics);

	handle_timer();
      }

      sigs = mask;
    }
  }

}

/****************************************************************
 Find an item with the given Userdata
*****************************************************************/
Object *menu_find_item(ULONG udata)
{
  if (udata >= MENU_ORDER && udata < MENU_REPORT)
  {
    return (Object *) DoMethod(main_order_menu, MUIM_FindUData, udata);
  }
  return (Object *) DoMethod(main_menu, MUIM_FindUData, udata);
}

/****************************************************************
 Enable/Disable a menu entry
*****************************************************************/
void menu_entry_sensitive(ULONG udata, ULONG sens)
{
  Object *item = menu_find_item(udata);
  if (item)
  {
    if (xget(item, MUIA_Menuitem_Enabled) != sens)
      set(item, MUIA_Menuitem_Enabled, sens);
  }
}

/****************************************************************
 Rename a menu entry
*****************************************************************/
void menu_entry_rename(ULONG udata, char *newtitle, BOOL force)
{
  Object *item = menu_find_item(udata);
  if (item)
  {
    if (force || ((char *) xget(item, MUIA_Menuitem_Title) != newtitle))
      set(item, MUIA_Menuitem_Title, newtitle);
  }
}

/**************************************************************************
 This function is called after the client succesfully
 has connected to the server
**************************************************************************/
void add_net_input(int sock)
{
  connected = TRUE;
  connected_sock = sock;
}

/**************************************************************************
 This function is called if the client disconnects
 from the server
**************************************************************************/
void remove_net_input(void)
{
  connected_sock = 0;
  connected = FALSE;
}

/**************************************************************************
...
**************************************************************************/
void update_menus(void) /* from menu.c */
{
  if (get_client_state() != CLIENT_GAME_RUNNING_STATE)
  {
    menu_entry_sensitive(MENU_REPORT, FALSE);
    menu_entry_sensitive(MENU_ORDER, FALSE);
    menu_entry_sensitive(MENU_VIEW, FALSE);
    menu_entry_sensitive(MENU_KINGDOM, FALSE);

    menu_entry_sensitive(MENU_GAME_OPTIONS, 0);
    menu_entry_sensitive(MENU_GAME_MSG_OPTIONS, 0);
    menu_entry_sensitive(MENU_GAME_SAVE_SETTINGS, 0);
    menu_entry_sensitive(MENU_GAME_PLAYERS, 0);
    menu_entry_sensitive(MENU_GAME_MESSAGES, 0);
    menu_entry_sensitive(MENU_GAME_SERVER_OPTIONS1, 1);
    menu_entry_sensitive(MENU_GAME_SERVER_OPTIONS2, 1);
    menu_entry_sensitive(MENU_GAME_OUTPUT_LOG, 1);
    menu_entry_sensitive(MENU_GAME_CLEAR_OUTPUT, 1);
  }
  else
  {
    struct unit *punit;
    int i;
    int any_cities = FALSE;
    punit = get_unit_in_focus();

    for (i = 0; i < game.nplayers; i++)
    {
      if (city_list_size(&game.players[i].cities))
      {
	any_cities = TRUE;
	break;
      }
    }

    menu_entry_sensitive(MENU_REPORT, TRUE);
    menu_entry_sensitive(MENU_ORDER, punit ? TRUE : FALSE);
    menu_entry_sensitive(MENU_VIEW, TRUE);
    menu_entry_sensitive(MENU_KINGDOM, TRUE);

    menu_entry_sensitive(MENU_GAME_OPTIONS, TRUE);
    menu_entry_sensitive(MENU_GAME_MSG_OPTIONS, TRUE);
    menu_entry_sensitive(MENU_GAME_SAVE_SETTINGS, TRUE);
    menu_entry_sensitive(MENU_GAME_PLAYERS, TRUE);
    menu_entry_sensitive(MENU_GAME_MESSAGES, TRUE);
    menu_entry_sensitive(MENU_GAME_SERVER_OPTIONS1, TRUE);
    menu_entry_sensitive(MENU_GAME_SERVER_OPTIONS2, TRUE);
    menu_entry_sensitive(MENU_GAME_OUTPUT_LOG, TRUE);
    menu_entry_sensitive(MENU_GAME_CLEAR_OUTPUT, TRUE);
    menu_entry_sensitive(MENU_GAME_DISCONNECT, TRUE);

    menu_entry_sensitive(MENU_REPORT_SPACESHIP, (game.player_ptr->spaceship.state != SSHIP_NONE));

    if (punit)
    {
      const char *chgfmt = "Change to %s";
      static char irrtext[64];
      static char mintext[64];
      static char transtext[64];
      enum tile_terrain_type ttype;
      struct tile_type *tinfo;

      set(main_menu, MUIA_Window_Menustrip, NULL);

      menu_entry_sensitive(MENU_ORDER_CITY, (can_unit_build_city(punit) || can_unit_add_to_city(punit)));
      menu_entry_sensitive(MENU_ORDER_ROAD, can_unit_do_activity(punit, ACTIVITY_ROAD) || can_unit_do_activity(punit, ACTIVITY_RAILROAD));
      menu_entry_sensitive(MENU_ORDER_IRRIGATE, can_unit_do_activity(punit, ACTIVITY_IRRIGATE));
      menu_entry_sensitive(MENU_ORDER_MINE, can_unit_do_activity(punit, ACTIVITY_MINE));
      menu_entry_sensitive(MENU_ORDER_TRANSFORM, can_unit_do_activity(punit, ACTIVITY_TRANSFORM));
      menu_entry_sensitive(MENU_ORDER_FORTRESS, can_unit_do_activity(punit, ACTIVITY_FORTRESS));

      if (can_unit_paradropped(punit))
      {
	menu_entry_rename(MENU_ORDER_POLLUTION, "Paradrop", FALSE);
	menu_entry_sensitive(MENU_ORDER_POLLUTION, TRUE);
      }
      else
      {
	menu_entry_rename(MENU_ORDER_POLLUTION, "Clean Pollution", FALSE);
	menu_entry_sensitive(MENU_ORDER_POLLUTION, can_unit_do_activity(punit, ACTIVITY_POLLUTION));
      }

      menu_entry_sensitive(MENU_ORDER_FORTIFY, can_unit_do_activity(punit, ACTIVITY_FORTIFY));
      menu_entry_sensitive(MENU_ORDER_AIRBASE, can_unit_do_activity(punit, ACTIVITY_AIRBASE));
      menu_entry_sensitive(MENU_ORDER_SENTRY, can_unit_do_activity(punit, ACTIVITY_SENTRY));
      menu_entry_sensitive(MENU_ORDER_PILLAGE, can_unit_do_activity(punit, ACTIVITY_PILLAGE));
      menu_entry_sensitive(MENU_ORDER_HOMECITY, can_unit_change_homecity(punit));
      menu_entry_sensitive(MENU_ORDER_UNLOAD, get_transporter_capacity(punit) > 0);
      menu_entry_sensitive(MENU_ORDER_WAKEUP, is_unit_activity_on_tile(ACTIVITY_SENTRY, punit->x, punit->y));
      menu_entry_sensitive(MENU_ORDER_AUTO_SETTLER, (can_unit_do_auto(punit) && unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(MENU_ORDER_AUTO_ATTACK, (can_unit_do_auto(punit) && !unit_flag(punit->type, F_SETTLERS)));
      menu_entry_sensitive(MENU_ORDER_EXPLORE, can_unit_do_activity(punit, ACTIVITY_EXPLORE));
      menu_entry_sensitive(MENU_ORDER_GOTO_CITY, any_cities);
      menu_entry_sensitive(MENU_ORDER_BUILD_WONDER, unit_can_help_build_wonder_here(punit));
      menu_entry_sensitive(MENU_ORDER_TRADE_ROUTE, unit_can_est_traderoute_here(punit));
      menu_entry_sensitive(MENU_ORDER_NUKE, unit_flag(punit->type, F_NUCLEAR));

      if (unit_flag(punit->type, F_SETTLERS) && map_get_city(punit->x, punit->y))
      {
	menu_entry_rename(MENU_ORDER_CITY, "Add to City", FALSE);
      }
      else
      {
	menu_entry_rename(MENU_ORDER_CITY, "Build City", FALSE);
      }

      ttype = map_get_tile(punit->x, punit->y)->terrain;
      tinfo = get_tile_type(ttype);
      if ((tinfo->irrigation_result != T_LAST) && (tinfo->irrigation_result != ttype))
      {
	sprintf(irrtext, chgfmt, (get_tile_type(tinfo->irrigation_result))->terrain_name);
      }
      else if ((map_get_tile(punit->x, punit->y)->special & S_IRRIGATION) && improvement_exists(B_SUPERMARKET))
      {
	strcpy(irrtext, "Build Farmland");
      }
      else
	strcpy(irrtext, "Build Irrigation");

      if ((tinfo->mining_result != T_LAST) && (tinfo->mining_result != ttype))
      {
	sprintf(mintext, chgfmt, (get_tile_type(tinfo->mining_result))->terrain_name);
      }
      else
	strcpy(mintext, "Build Mine");
      if ((tinfo->transform_result != T_LAST) && (tinfo->transform_result != ttype))
      {
	sprintf(transtext, chgfmt, (get_tile_type(tinfo->transform_result))->terrain_name);
      }
      else
	strcpy(transtext, "Transform terrain");

      if (map_get_tile(punit->x, punit->y)->special & S_ROAD)
      {
	menu_entry_rename(MENU_ORDER_ROAD, "Build Railroad", FALSE);
      }
      else
      {
	menu_entry_rename(MENU_ORDER_ROAD, "Build Road", FALSE);
      }
      menu_entry_rename(MENU_ORDER_IRRIGATE, irrtext, FALSE);
      menu_entry_rename(MENU_ORDER_MINE, mintext, FALSE);
      menu_entry_rename(MENU_ORDER_TRANSFORM, transtext, FALSE);

      set(main_menu, MUIA_Window_Menustrip, main_menu);
    }
  }
}

/**************************************************************************
 Cleanup everything on exit
**************************************************************************/
void ui_exit(void)
{
  free_gui();
  free_all_sprites();
  free_timer();
}

/****************************************************************
 Entry for the client dependend part of the client
*****************************************************************/
void ui_main(int argc, char *argv[])
{
  parse_options(argc, argv);
  boot_help_texts();		/* after log_init */
  tilespec_read_toplevel(tile_set_name);	/* get tile sizes etc */

  atexit(ui_exit);

  if (init_timer() && load_all_sprites())	/* includes tilespec_load_tiles */
  {
    if (init_gui())
    {
      Object *econ_group;

      load_options();

      /* TODO: Move this into init_gui() */
      main_bulb_sprite = MakeSprite(sprites.bulb[0]);
      main_sun_sprite = MakeSprite(sprites.warming[0]);
      main_government_sprite = MakeSprite(sprites.citizen[7]);
      main_timeout_text = TextObject, End;

      econ_group = HGroup, GroupSpacing(0), End;
      if (econ_group)
      {
	int i;
	for (i = 0; i < 10; i++)
	{
	  Object *o = main_econ_sprite[i] = MakeSprite(sprites.warming[0]);
	  if (o)
	  {
	    DoMethod(econ_group, OM_ADDMEMBER, o);
	    DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, o, 4, MUIM_CallHook, &standart_hook, taxrates_callback, i);
	  }
	}
      }

      DoMethod(main_info_group, MUIM_Group_InitChange);
      DoMethod(main_turndone_group, MUIM_Group_InitChange);

      DoMethod(main_info_group, OM_ADDMEMBER, econ_group);
      DoMethod(main_turndone_group, OM_ADDMEMBER, main_bulb_sprite);
      DoMethod(main_turndone_group, OM_ADDMEMBER, main_sun_sprite);
      DoMethod(main_turndone_group, OM_ADDMEMBER, main_government_sprite);
      DoMethod(main_turndone_group, OM_ADDMEMBER, main_timeout_text);
      DoMethod(main_info_group, MUIM_Group_Sort, econ_group, main_turndone_group, NULL);

      DoMethod(main_turndone_group, MUIM_Group_ExitChange);
      DoMethod(main_info_group, MUIM_Group_ExitChange);

      set(main_wnd, MUIA_Window_Open, TRUE);

      if (xget(main_wnd, MUIA_Window_Open))
      {
	set_client_state(CLIENT_PRE_GAME_STATE);
	send_timer(0, 500000);
	loop();
      }
      else
	printf("Couldn't open the main window (Gfx memory problem or screensize too small)\n");
    }
  }
}

