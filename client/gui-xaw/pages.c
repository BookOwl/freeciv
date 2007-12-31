/********************************************************************** 
 Freeciv - Copyright (C) 1996-2004 - The Freeciv Team
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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/List.h>

#include "game.h"

#include "fcintl.h"
#include "support.h"

#include "civclient.h"
#include "connectdlg_g.h"
#include "dialogs_g.h"

#include "chatline.h" /* for send_chat() */
#include "gui_main.h"
#include "gui_stuff.h" /* for xaw_set_relative_position() */

#include "pages.h"

static enum client_pages old_page;

static Widget start_page_shell;
static Widget start_page_form;
static Widget start_page_label;
static Widget start_page_players_list;
static Widget start_page_cancel_command;
static Widget start_page_nation_command;
static Widget start_page_start_command;
void start_page_cancel_callback(Widget w, XtPointer client_data,
				XtPointer call_data);
void start_page_nation_callback(Widget w, XtPointer client_data,
				XtPointer call_data);
void start_page_start_callback(Widget w, XtPointer client_data,
			       XtPointer call_data);


/***************************************************************************
  Returns current client page
***************************************************************************/
enum client_pages get_client_page(void)
{
  return old_page;
}

/**************************************************************************
  Sets the "page" that the client should show.  See documentation in
  pages_g.h.
**************************************************************************/
void set_client_page(enum client_pages page)
{
  /* PORTME */
  if (page == PAGE_GAME) {
    if (old_page == PAGE_START) {
      popdown_start_page();
    }
  } else {
    if (page == PAGE_START) {
      popup_start_page();
    }
  }

  old_page = page;
}

/****************************************************************************
  Set the list of available rulesets.  The default ruleset should be
  "default", and if the user changes this then set_ruleset() should be
  called.
****************************************************************************/
void gui_set_rulesets(int num_rulesets, char **rulesets)
{
  /* PORTME */
}

/**************************************************************************
                                  START PAGE
**************************************************************************/

/**************************************************************************
  Popup start page.
**************************************************************************/
void popup_start_page(void)
{
  if (!start_page_shell) {
    create_start_page();
  }

  xaw_set_relative_position(toplevel, start_page_shell, 5, 25);
  XtPopup(start_page_shell, XtGrabNone);
}

/**************************************************************************
  Close start page.
**************************************************************************/
void popdown_start_page(void)
{
  if (start_page_shell) {
    XtDestroyWidget(start_page_shell);
    start_page_shell = 0;
  }
}

/**************************************************************************
  Create start page.
**************************************************************************/
void create_start_page(void)
{
  start_page_shell =
    I_IN(I_T(XtCreatePopupShell("startpage",
				topLevelShellWidgetClass,
				toplevel, NULL, 0)));

  start_page_form = XtVaCreateManagedWidget("startpageform",
				       formWidgetClass,
				       start_page_shell, NULL);

  start_page_label = I_L(XtVaCreateManagedWidget("startpagelabel",
					labelWidgetClass,
					start_page_form, NULL));

  start_page_players_list =
    XtVaCreateManagedWidget("startpageplayerslist",
			    listWidgetClass,
			    start_page_form,
			    NULL);

  start_page_cancel_command =
    I_L(XtVaCreateManagedWidget("startpagecancelcommand",
				commandWidgetClass,
				start_page_form, NULL));

  start_page_nation_command =
    I_L(XtVaCreateManagedWidget("startpagenationcommand",
				commandWidgetClass,
				start_page_form,
				NULL));

  start_page_start_command =
    I_L(XtVaCreateManagedWidget("startpagestartcommand",
				commandWidgetClass,
				start_page_form,
				NULL));

/*
  XtAddCallback(start_page_players_list, XtNcallback,
		start_page_players_list_callback,
		NULL);
*/
  XtAddCallback(start_page_cancel_command, XtNcallback,
		start_page_cancel_callback,
		NULL);

  XtAddCallback(start_page_nation_command, XtNcallback,
		start_page_nation_callback,
		NULL);

  XtAddCallback(start_page_start_command, XtNcallback,
		start_page_start_callback,
		NULL);

  update_start_page();

  XtRealizeWidget(start_page_shell);
  
  XSetWMProtocols(display, XtWindow(start_page_shell),
		  &wm_delete_window, 1);
  XtOverrideTranslations(start_page_shell,
    XtParseTranslationTable("<Message>WM_PROTOCOLS: msg-close-start-page()"));
}

/**************************************************************************
  Update start page players list.
**************************************************************************/
void update_start_page(void)
{
  if (!start_page_shell) {
    return;
  }
  if (C_S_RUNNING != client_state()) {
    bool is_ready;
    const char *name, *nation, *leader;
    static char *namelist_ptrs[MAX_NUM_PLAYERS];
    static char namelist_text[MAX_NUM_PLAYERS][256];
    int j;
    Dimension width;

    j = 0;
    players_iterate(pplayer) {
      if (pplayer->ai.control) {
	name = _("<AI>");
      } else {
	name = pplayer->username;
      }
      is_ready = pplayer->ai.control ? TRUE: pplayer->is_ready;
      if (pplayer->nation == NO_NATION_SELECTED) {
	nation = _("Random");
	leader = "";
      } else {
	nation = nation_adjective_for_player(pplayer);
	leader = player_name(pplayer);
      }

      my_snprintf(namelist_text[j], sizeof(namelist_text[j]),
		  "%-16s %-5s %-16s %-16s %4d",
		  name,
		  is_ready ? " Yes " : " No  ",
		  leader,
		  nation,
		  pplayer->player_no);

      namelist_ptrs[j]=namelist_text[j];
      j++;
    } players_iterate_end;
    conn_list_iterate(game.est_connections, pconn) {
      if (pconn->player && !pconn->observer) {
	continue; /* Already listed above. */
      }
      name = pconn->username;
      nation = "";
      leader = "";

      my_snprintf(namelist_text[j], sizeof(namelist_text[j]),
		  "%-16s %-5s %-16s %-16s %4d",
		  name,
		  " No   ",
		  leader,
		  nation,
		  -1);

      namelist_ptrs[j]=namelist_text[j];
      j++;
    } conn_list_iterate_end;
    XawListChange(start_page_players_list, namelist_ptrs, j, 0, True);

    XtVaGetValues(start_page_players_list, XtNwidth, &width, NULL);
    XtVaSetValues(start_page_label, XtNwidth, width, NULL); 
  }
}

/**************************************************************************
  Callback for start page "Cancel" button
**************************************************************************/
void start_page_cancel_callback(Widget w, XtPointer client_data,
				XtPointer call_data)
{
  popdown_start_page();
}

/**************************************************************************
  Called when "Pick Nation" is clicked.
**************************************************************************/
void start_page_nation_callback(Widget w, XtPointer client_data,
				XtPointer call_data)
{
  if (game.player_ptr) {
    popup_races_dialog(game.player_ptr);
  }
}

/**************************************************************************
  Callback for start page "Start" button
**************************************************************************/
void start_page_start_callback(Widget w, XtPointer client_data,
			       XtPointer call_data)
{
  /* popdown_start_page(); */
  send_chat("/start");
}

/**************************************************************************
  Callback for start page window (x) button
**************************************************************************/
void start_page_msg_close(Widget w)
{
  popdown_start_page();
}
