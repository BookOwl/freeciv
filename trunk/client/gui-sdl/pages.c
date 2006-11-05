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

/* utility */
#include "fcintl.h"
#include "log.h"

/* common */
#include "fc_types.h"

/* client */
#include "connectdlg_common.h"

/* gui-sdl */
#include "chatline.h"
#include "colors.h"
#include "connectdlg.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_tilespec.h"
#include "mapctrl.h"
#include "mapview.h"
#include "messagewin.h"
#include "optiondlg.h"
#include "themespec.h"
#include "widget.h"

#include "pages.h"

static enum client_pages old_page = PAGE_MAIN;

/**************************************************************************
                                  MAIN PAGE
**************************************************************************/
static struct SMALL_DLG *pStartMenu = NULL;

static void popdown_start_menu(void);

/**************************************************************************
  ...
**************************************************************************/
static int start_new_game_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (is_server_running() || client_start_server()) {
      char buf[512];
  
      /* Send new game defaults. */
      send_chat("/set aifill 5");
  
      my_snprintf(buf, sizeof(buf), "/%s", skill_level_names[0]);
      send_chat(buf);
    }
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static int join_game_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    set_client_page(PAGE_NETWORK);
    popup_join_game_dialog();
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static int servers_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    bool lan_scan = (pWidget->ID != ID_JOIN_META_GAME);
    popdown_start_menu();  
    popup_connection_dialog(lan_scan);
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static int options_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    queue_flush();
    popdown_start_menu();
    popup_optiondlg();
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static int quit_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_start_menu();
  }
  return 0;/* exit from main game loop */
}

/**************************************************************************
  ...
**************************************************************************/
static void show_main_page()
{
  SDL_Color bg_color = {255, 255, 255, 136};
  
  int count = 0;
  struct widget *pWidget = NULL, *pWindow = NULL;
  SDL_Surface *pBackground;
  int h = 0;
  SDL_Rect area = {0, 0, 0, 0};
    
  /* create dialog */
  pStartMenu = fc_calloc(1, sizeof(struct SMALL_DLG));

  pWindow = create_window(NULL, NULL, 1, 1, 0);
  add_to_gui_list(ID_WINDOW, pWindow);
  pStartMenu->pEndWidgetList = pWindow;
  
  /* Start New Game */
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Start New Game"),
            adj_font(14),
            (WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND|WF_FREE_DATA));
  
  pWidget->action = start_new_game_callback;
  pWidget->string16->style |= SF_CENTER;
  set_wstate(pWidget, FC_WS_NORMAL);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  add_to_gui_list(ID_START_NEW_GAME, pWidget);
  
  /* Load Game */  
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Load Game"),
            adj_font(14),
	    (WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND));
  /*pWidget->action = popup_load_game_callback;*/
  pWidget->string16->style |= SF_CENTER;
  pWidget->string16->fgcol = *get_game_colorRGB(COLOR_THEME_WIDGET_DISABLED_TEXT);
  
  add_to_gui_list(ID_LOAD_GAME, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  /* Join Game */
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Join Game"),
            adj_font(14),
	    WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND);
  pWidget->action = join_game_callback;
  pWidget->string16->style |= SF_CENTER;  
  set_wstate(pWidget, FC_WS_NORMAL);
  
  add_to_gui_list(ID_JOIN_GAME, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
    
  /* Join Pubserver */  
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Join Pubserver"),
            adj_font(14),
	    WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND);
  pWidget->action = servers_callback;
  pWidget->string16->style |= SF_CENTER;  
  set_wstate(pWidget, FC_WS_NORMAL);
  
  add_to_gui_list(ID_JOIN_META_GAME, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  /* Join LAN Server */  
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Join LAN Server"),
            adj_font(14),
	    WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND);
  pWidget->action = servers_callback;
  pWidget->string16->style |= SF_CENTER;  
  set_wstate(pWidget, FC_WS_NORMAL);
  
  add_to_gui_list(ID_JOIN_GAME, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  /* Options */  
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Options"),
            adj_font(14),
	    WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND);
  pWidget->action = options_callback;
  pWidget->string16->style |= SF_CENTER;
  set_wstate(pWidget, FC_WS_NORMAL);
  
  add_to_gui_list(ID_CLIENT_OPTIONS_BUTTON, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  /* Quit */  
  pWidget = create_iconlabel_from_chars(NULL, pWindow->dst, _("Quit"),
            adj_font(14),
	    WF_SELLECT_WITHOUT_BAR|WF_RESTORE_BACKGROUND);
  pWidget->action = quit_callback;
  pWidget->string16->style |= SF_CENTER;
  pWidget->key = SDLK_ESCAPE;
  set_wstate(pWidget, FC_WS_NORMAL);
  add_to_gui_list(ID_QUIT, pWidget);
  
  area.w = MAX(area.w, pWidget->size.w);
  area.h = MAX(area.h, pWidget->size.h);
  count++;
  
  pStartMenu->pBeginWidgetList = pWidget;

  /* ------*/

  area.w += adj_size(30);
  area.h += adj_size(6);

  h = area.h;
  area.h *= count;

  /* ------*/
  area.x = pTheme->FR_Left->w;
  area.y = pTheme->FR_Top->h;

  group_set_area(pWidget, pWindow->prev, area);

  setup_vertical_widgets_position(1, area.x, area.y, area.w, h, pWidget, pWindow->prev);

  widget_set_position(pWindow,
    (pWindow->area.w - pTheme->FR_Right->w - area.w - pTheme->FR_Left->w) - adj_size(20),
    (pWindow->area.h - pTheme->FR_Bottom->h - area.h - pTheme->FR_Top->h) - adj_size(20));

  draw_intro_gfx();
  
  pBackground = theme_get_background(theme, BACKGROUND_STARTMENU);
  SDL_FillRectAlpha(pBackground, NULL, &bg_color);
  
  if (resize_window(pWindow, pBackground, NULL,
        pTheme->FR_Left->w + area.w + pTheme->FR_Right->w,
        pTheme->FR_Top->h + area.h + pTheme->FR_Bottom->h)) {
    FREESURFACE(pBackground);
  }

  redraw_group(pStartMenu->pBeginWidgetList, pStartMenu->pEndWidgetList, FALSE);

  set_output_window_text(_("SDLClient welcomes you..."));
  set_output_window_text(_("Freeciv is free software and you are welcome "
			   "to distribute copies of "
			   "it under certain conditions;"));
  set_output_window_text(_("See the \"Copying\" item on the Help"
			   " menu."));
  set_output_window_text(_("Now.. Go give'em hell!"));

  popup_meswin_dialog(true);  

  flush_all();
}

static void popdown_start_menu()
{
  if(pStartMenu) {
    popdown_window_group_dialog(pStartMenu->pBeginWidgetList,
                                pStartMenu->pEndWidgetList);
    FC_FREE(pStartMenu);
  }
}

/**************************************************************************
                             PUBLIC FUNCTIONS
**************************************************************************/

/**************************************************************************
  Sets the "page" that the client should show.  See documentation in
  pages_g.h.
**************************************************************************/
void set_client_page(enum client_pages page)
{

  switch (old_page) {
    case PAGE_MAIN:
      popdown_start_menu();
      break;
    case PAGE_GAME:
      close_game_page();
      break;
    default: 
      break;
  }
  
  switch (page) {
    case PAGE_MAIN:
      show_main_page();
      break;
    case PAGE_GAME:
/*      show_game_page();*/
      enable_main_widgets();
    default:
      break;
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
  Returns current client page
**************************************************************************/
enum client_pages get_client_page(void)
{
  return old_page;
}

/**************************************************************************
  update the start page.
**************************************************************************/
void update_start_page(void)
{
  /* PORTME*/    
}
