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

/***************************************************************************
                          menu.c  -  description
                             -------------------
    begin                : Wed Sep 04 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*#include <stdio.h>*/
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "astring.h"
#include "fcintl.h"
#include "map.h"

#include "gui_mem.h"

#include "support.h"
#include "unit.h"

#include "chatline.h"
#include "cityrep.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h"
#include "dialogs.h"
#include "finddlg.h"
#include "gotodlg.h"


#include "graphics.h"
#include "gui_string.h"
#include "gui_stuff.h"		/* gui */
#include "gui_id.h"
#include "gui_tilespec.h"
#include "gui_main.h"

#include "helpdlg.h"
#include "mapctrl.h"		/* center_on_unit */
#include "messagedlg.h"
#include "messagewin.h"
#include "optiondlg.h"
#include "options.h"
#include "plrdlg.h"
#include "ratesdlg.h"
#include "repodlgs.h"
#include "spaceshipdlg.h"
#include "wldlg.h"
#include "mapview.h"

#include "menu.h"


static struct GUI *pBeginOrderWidgetList;
static struct GUI *pEndOrderWidgetList;
  
static struct GUI *pOrder_Mine_Button;
static struct GUI *pOrder_Irrigation_Button;
static struct GUI *pOrder_Road_Button;
static struct GUI *pOrder_Transform_Button;

#define local_show(ID)                                                \
  clear_wflag(get_widget_pointer_form_ID(pBeginOrderWidgetList, ID ), \
	      WF_HIDDEN)

#define local_hide(ID)                                             \
  set_wflag(get_widget_pointer_form_ID(pBeginOrderWidgetList, ID), \
	    WF_HIDDEN )


/**************************************************************************
  ...
**************************************************************************/
static int unit_order_callback(struct GUI *pOrder_Widget)
{
  struct unit *pUnit = get_unit_in_focus();

  set_wstate(pOrder_Widget, WS_SELLECTED);
  pSellected_Widget = pOrder_Widget;

  if (!pUnit) {
    goto END;
  }

  switch (pOrder_Widget->ID) {
  case ID_UNIT_ORDER_BUILD_CITY:
    /* Enable the button for adding to a city in all cases, so we
       get an eventual error message from the server if we try. */
    key_unit_build_city();
    break;
  case ID_UNIT_ORDER_BUILD_WONDER:
    key_unit_build_wonder();
    break;
  case ID_UNIT_ORDER_ROAD:
    key_unit_road();
    break;
  case ID_UNIT_ORDER_TRADEROUTE:
    key_unit_traderoute();
    break;
  case ID_UNIT_ORDER_IRRIGATE:
    key_unit_irrigate();
    break;
  case ID_UNIT_ORDER_MINE:
    key_unit_mine();
    break;
  case ID_UNIT_ORDER_TRANSFORM:
    key_unit_transform();
    break;
  case ID_UNIT_ORDER_FORTRESS:
    key_unit_fortress();
    break;
  case ID_UNIT_ORDER_FORTIFY:
    key_unit_fortify();
    break;
  case ID_UNIT_ORDER_AIRBASE:
    key_unit_airbase();
    break;
  case ID_UNIT_ORDER_POLLUTION:
    key_unit_pollution();
    break;
  case ID_UNIT_ORDER_PARADROP:
    key_unit_paradrop();
    break;
  case ID_UNIT_ORDER_FALLOUT:
    key_unit_fallout();
    break;
  case ID_UNIT_ORDER_SENTRY:
    key_unit_sentry();
    break;
  case ID_UNIT_ORDER_PILLAGE:
    key_unit_pillage();
    break;
  case ID_UNIT_ORDER_HOMECITY:
    key_unit_homecity();
    break;
  case ID_UNIT_ORDER_UNLOAD:
    key_unit_unload();
    break;
  case ID_UNIT_ORDER_WAKEUP_OTHERS:
    key_unit_wakeup_others();
    break;
  case ID_UNIT_ORDER_AUTO_SETTLER:
  case ID_UNIT_ORDER_AUTO_AUTO_ATTACK:
    request_unit_auto(pUnit);
    break;
  case ID_UNIT_ORDER_AUTO_EXPLORE:
    key_unit_auto_explore();
    break;
  case ID_UNIT_ORDER_CONNECT:
    key_unit_connect();
    break;
  case ID_UNIT_ORDER_PATROL:
    key_unit_patrol();
    break;
  case ID_UNIT_ORDER_GOTO:
    key_unit_goto();
    break;
  case ID_UNIT_ORDER_AIRLIFT:
    /*      popup_goto_dialog(); */
    break;
  case ID_UNIT_ORDER_GOTO_CITY:
    popup_goto_dialog();
    break;
  case ID_UNIT_ORDER_DISBAND:
    key_unit_disband();
    break;
  case ID_UNIT_ORDER_DIPLOMAT_DLG:
    key_unit_diplomat_actions();
    break;
  case ID_UNIT_ORDER_NUKE:
    key_unit_nuke();
    break;
  case ID_UNIT_ORDER_WAIT:
    key_unit_wait();
    flush_dirty();
    break;
  case ID_UNIT_ORDER_DONE:
    key_unit_done();
    flush_dirty();
    break;

  default:
    break;
  }

END:

  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static bool has_city_airport(struct city *pCity)
{
  if (!pCity) {
    return FALSE;
  }

  if (pCity->improvements[B_AIRPORT] == I_ACTIVE) {
    return TRUE;
  }

  return FALSE;
}

static Uint16 redraw_order_widgets(void)
{
  Uint16 count = 0;
  struct GUI *pTmpWidget = pBeginOrderWidgetList;

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {

      refresh_widget_background(pTmpWidget , Main.gui );
      real_redraw_icon(pTmpWidget, Main.gui);
      sdl_dirty_rect(pTmpWidget->size);
      count++;
    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }

  return count;

}

/**************************************************************************
  ...
**************************************************************************/
static void set_new_order_widget_start_pos(void)
{
  struct GUI *pMiniMap =
      get_widget_pointer_form_main_list(ID_MINI_MAP_WINDOW);
  struct GUI *pInfoWind =
      get_widget_pointer_form_main_list(ID_UNITS_WINDOW);
  struct GUI *pTmpWidget = pBeginOrderWidgetList;
  Sint16 sx, sy, xx;
  Uint16 count = 0, lines = 1, w, count_on_line;

  if (SDL_Client_Flags & CF_MINI_MAP_SHOW) {
    xx = pMiniMap->size.x + pMiniMap->size.w;
  } else {
    xx = pMiniMap->size.x + 36;
  }

  w = pInfoWind->size.x - xx;
  count_on_line = w / (pTmpWidget->size.w + 5);

  /* find how many to reposition */
  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {
      count++;
    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;

  }

  pTmpWidget = pBeginOrderWidgetList;

  if (count - count_on_line > 0) {

    if (div(count, count_on_line).rem) {
      lines = (count / count_on_line) + 1;
    } else {
      lines = (count / count_on_line);
    }

    count = count_on_line - ((count_on_line * lines) - count);

  }

  sx = xx + (w - count * (pTmpWidget->size.w + 5)) / 2;

  sy = Main.gui->h - lines * (pTmpWidget->size.h + 5);

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {

      pTmpWidget->size.x = sx;
      pTmpWidget->size.y = sy;

      count--;
      sx += (pTmpWidget->size.w + 5);
      if (!count) {
	count = count_on_line;
	lines--;

	sx = xx + (w - count * (pTmpWidget->size.w + 5)) / 2;

	sy = Main.gui->h - lines * (pTmpWidget->size.h + 5);
      }

    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;

  }
}

/* ================================ PUBLIC ================================ */

/**************************************************************************
  ...
**************************************************************************/
void create_units_order_widgets(void)
{
  struct GUI *pBuf = NULL;
  char cBuf[128];
  
  
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("No orders"), " (Space)");
  pBuf = create_themeicon(pTheme->ODone_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_SPACE;
  add_to_gui_list(ID_UNIT_ORDER_DONE, pBuf);
  pEndOrderWidgetList = pBuf;

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Wait"), " (W)");
  pBuf = create_themeicon(pTheme->OWait_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_w;
  add_to_gui_list(ID_UNIT_ORDER_WAIT, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Explode Nuclear"), " (Shift + N)");
  pBuf = create_themeicon(pTheme->ONuke_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_n;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_NUKE, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Diplomat|Spy Actions"), " (D)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_d;
  add_to_gui_list(ID_UNIT_ORDER_DIPLOMAT_DLG, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Disband"), " (Shift + D)");
  pBuf = create_themeicon(pTheme->ODisband_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_d;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_DISBAND, pBuf);
  
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Goto City"), " (L)");
  pBuf = create_themeicon(pTheme->OGotoCity_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_l;
  add_to_gui_list(ID_UNIT_ORDER_GOTO_CITY, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Airlift"), " (L)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_l;
  add_to_gui_list(ID_UNIT_ORDER_AIRLIFT, pBuf);
  
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Goto location"), " (G)");
  pBuf = create_themeicon(pTheme->OGoto_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_g;
  add_to_gui_list(ID_UNIT_ORDER_GOTO, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Patrol"), " (Q)");
  pBuf = create_themeicon(pTheme->OPatrol_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_q;
  add_to_gui_list(ID_UNIT_ORDER_PATROL, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Connect"), " (Shift + C)");
  pBuf = create_themeicon(pTheme->OAutoConet_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_c;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_CONNECT, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Explore"), " (X)");
  pBuf = create_themeicon(pTheme->OAutoExp_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_x;
  add_to_gui_list(ID_UNIT_ORDER_AUTO_EXPLORE, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Attack"), " (A)");
  pBuf = create_themeicon(pTheme->OAutoAtt_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_a;
  add_to_gui_list(ID_UNIT_ORDER_AUTO_AUTO_ATTACK, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Settler"), " (A)");
  pBuf = create_themeicon(pTheme->OAutoSett_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_a;
  add_to_gui_list(ID_UNIT_ORDER_AUTO_SETTLER, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Wake Up Others"), " (Shift + W)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_w;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_WAKEUP_OTHERS, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Unload"), " (U)");
  pBuf = create_themeicon(pTheme->OUnload_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_u;
  add_to_gui_list(ID_UNIT_ORDER_UNLOAD, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Find Homecity"), " (H)");
  pBuf = create_themeicon(pTheme->OHomeCity_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_h;
  add_to_gui_list(ID_UNIT_ORDER_HOMECITY, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Pillage"), " (Shift + P)");
  pBuf = create_themeicon(pTheme->OPillage_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_p;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_PILLAGE, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Sentry"), " (S)");
  pBuf = create_themeicon(pTheme->OSentry_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_s;
  add_to_gui_list(ID_UNIT_ORDER_SENTRY, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Clean Nuclear Fallout"), " (N)");
  pBuf = create_themeicon(pTheme->ONuke_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_n;
  add_to_gui_list(ID_UNIT_ORDER_FALLOUT, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Paradrop"), " (P)");
  pBuf = create_themeicon(pTheme->OParaDrop_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_p;
  add_to_gui_list(ID_UNIT_ORDER_PARADROP, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Clean Pollution"), " (P)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_p;
  add_to_gui_list(ID_UNIT_ORDER_POLLUTION, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Airbase"), " (E)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_e;
  add_to_gui_list(ID_UNIT_ORDER_AIRBASE, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Fortify"), " (F)");
  pBuf = create_themeicon(pTheme->OFortify_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_f;
  add_to_gui_list(ID_UNIT_ORDER_FORTIFY, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Fortress"), " (F)");
  pBuf = create_themeicon(pTheme->OFortify_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_f;
  add_to_gui_list(ID_UNIT_ORDER_FORTRESS, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Transform Tile"), " (O)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_o;
  pOrder_Transform_Button = pBuf;
  add_to_gui_list(ID_UNIT_ORDER_TRANSFORM, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Mine"), " (M)");
  pBuf = create_themeicon(pTheme->OMine_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_m;
  add_to_gui_list(ID_UNIT_ORDER_MINE, pBuf);
  pOrder_Mine_Button = pBuf;

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Irrigation"), " (I)");
  pBuf = create_themeicon(pTheme->OIrigation_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->key = SDLK_i;
  add_to_gui_list(ID_UNIT_ORDER_IRRIGATE, pBuf);
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pOrder_Irrigation_Button = pBuf;

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Form Traderoute"), " (R)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_r;
  add_to_gui_list(ID_UNIT_ORDER_TRADEROUTE, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Road"), " (R)");
  pBuf = create_themeicon(pTheme->ORoad_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_r;
  pOrder_Road_Button = pBuf;;
  add_to_gui_list(ID_UNIT_ORDER_ROAD, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Help Build Wonder"), " (B)");
  pBuf = create_themeicon(pTheme->Order_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_b;
  add_to_gui_list(ID_UNIT_ORDER_BUILD_WONDER, pBuf);

  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build New City"), " (B)");
  pBuf = create_themeicon(pTheme->OBuildCity_Icon,
			  (WF_HIDDEN | WF_DRAW_THEME_TRANSPARENT |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, 10);
  pBuf->key = SDLK_b;
  add_to_gui_list(ID_UNIT_ORDER_BUILD_CITY, pBuf);
  pBeginOrderWidgetList = pBuf;


  SDL_Client_Flags |= CF_ORDERS_WIDGETS_CREATED;
}

/**************************************************************************
  ...
**************************************************************************/
void update_order_widget(void)
{
  set_new_order_widget_start_pos();
  redraw_order_widgets();
}

/**************************************************************************
  ...
**************************************************************************/
void undraw_order_widgets(void)
{
  struct GUI *pTmpWidget = pBeginOrderWidgetList;
  SDL_Rect dest;

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN) && (pTmpWidget->gfx)) {

      dest.x = pTmpWidget->size.x;
      dest.y = pTmpWidget->size.y;

      SDL_SetAlpha(pTmpWidget->gfx, 0x0, 0x0);
      SDL_BlitSurface(pTmpWidget->gfx, NULL, Main.gui, &dest);
      SDL_SetAlpha(pTmpWidget->gfx, SDL_SRCALPHA, 255);
      
      sdl_dirty_rect(pTmpWidget->size);

    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }
}

/**************************************************************************
  ...
**************************************************************************/
void free_bcgd_order_widgets(void)
{
  struct GUI *pTmpWidget = pBeginOrderWidgetList;

  while (TRUE) {

    FREESURFACE(pTmpWidget->gfx);

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }
}


/* ============================== Native =============================== */

/**************************************************************************
  Update all of the menus (sensitivity, etc.) based on the current state.
**************************************************************************/
void update_menus(void)
{
  static Uint16 counter = 0;
  struct unit *pUnit = NULL;
  static char cBuf[128];
  /* call by set_client_state(...) */

  if (!aconnection.established) {
    hide(ID_CLIENT_OPTIONS);
  }

  if (get_client_state() != CLIENT_GAME_RUNNING_STATE ||
      (SDL_Client_Flags & CF_OPTION_OPEN) ||
      (SDL_Client_Flags & CF_CITY_DIALOG_IS_OPEN)) {

    SDL_Client_Flags |= CF_GANE_JUST_STARTED;

    /*hide( ID_CLIENT_OPTIONS ); */
    hide(ID_CHATLINE_TOGGLE_LOG_WINDOW_BUTTON);

    if (SDL_Client_Flags & CF_MAP_UNIT_W_CREATED) {
      hide(ID_TOGGLE_UNITS_WINDOW_BUTTON);
      hide(ID_TOGGLE_MAP_WINDOW_BUTTON);
      hide(ID_FIND_CITY);
      hide(ID_REVOLUTION);
      hide(ID_RESEARCH);
      hide(ID_ECONOMY);
      hide(ID_NEW_TURN);
    }

    if (SDL_Client_Flags & CF_ORDERS_WIDGETS_CREATED) {
      hide_group(pBeginOrderWidgetList, pEndOrderWidgetList);
    }

  } else {
    if (counter) {
      undraw_order_widgets();
    }

    if (SDL_Client_Flags & CF_GANE_JUST_STARTED) {

      SDL_Client_Flags &= ~CF_GANE_JUST_STARTED;

      show(ID_CLIENT_OPTIONS);

      show(ID_TOGGLE_UNITS_WINDOW_BUTTON);
      show(ID_REVOLUTION);
      show(ID_RESEARCH);
      show(ID_ECONOMY);

      show(ID_TOGGLE_MAP_WINDOW_BUTTON);
      show(ID_FIND_CITY);
      show(ID_CHATLINE_TOGGLE_LOG_WINDOW_BUTTON);
      show(ID_NEW_TURN);

      counter = 0;
    }

    pUnit = get_unit_in_focus();

    if (pUnit && !pUnit->ai.control) {
      int time;
      enum tile_terrain_type terrain = map_get_terrain(pUnit->x, pUnit->y);
      
      if (!counter) {
	local_show(ID_UNIT_ORDER_GOTO);
	local_show(ID_UNIT_ORDER_DISBAND);

	local_show(ID_UNIT_ORDER_WAIT);
	local_show(ID_UNIT_ORDER_DONE);
      }

      /* Enable the button for adding to a city in all cases, so we
       * get an eventual error message from the server if we try. */

      if (can_unit_add_or_build_city(pUnit)) {
	local_show(ID_UNIT_ORDER_BUILD_CITY);
      } else {
	local_hide(ID_UNIT_ORDER_BUILD_CITY);
      }

      if (unit_can_help_build_wonder_here(pUnit)) {
	local_show(ID_UNIT_ORDER_BUILD_WONDER);
      } else {
	local_hide(ID_UNIT_ORDER_BUILD_WONDER);
      }

      time = can_unit_do_activity(pUnit, ACTIVITY_RAILROAD);
      if (can_unit_do_activity(pUnit, ACTIVITY_ROAD) || time) {
	FREE(pOrder_Road_Button->string16->text);
	if(time) {
	  time = map_build_rail_time(pUnit->x, pUnit->y);
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Railroad"), " (R)", time , 
			PL_("turn", "turns", time));
	  pOrder_Road_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Road_Button->theme = pTheme->ORailRoad_Icon;
	} else {
	  time = map_build_road_time(pUnit->x, pUnit->y);
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Road"), " (R)", time , 
			PL_("turn", "turns", time));
	  pOrder_Road_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Road_Button->theme = pTheme->ORoad_Icon;
	}
	    
	clear_wflag(pOrder_Road_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Road_Button, WF_HIDDEN);
      }

      if (unit_can_est_traderoute_here(pUnit)) {
	local_show(ID_UNIT_ORDER_TRADEROUTE);
      } else {
	local_hide(ID_UNIT_ORDER_TRADEROUTE);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_IRRIGATE)) {
	FREE(pOrder_Irrigation_Button->string16->text);
	time = map_build_irrigation_time(pUnit->x, pUnit->y);
	switch (terrain) {
	  /* set Crop Forest Icon */
	case T_FOREST:
	case T_JUNGLE:  
	  my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
			_("Cut Down to"),
			tile_types[tile_types[terrain].irrigation_result
				].terrain_name
			," (I)", time , PL_("turn", "turns", time));
	  pOrder_Irrigation_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Irrigation_Button->theme = pTheme->OCropForest_Icon;
	  break;
	  
	  /* set Irrigation Icon */
	default:
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Irrigation"), " (I)", time , 
			PL_("turn", "turns", time));
	  pOrder_Irrigation_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Irrigation_Button->theme = pTheme->OIrigation_Icon;
	  break;
	}

	/*show( ID_UNIT_ORDER_MINE ); */
	clear_wflag(pOrder_Irrigation_Button, WF_HIDDEN);
	
      } else {
	local_hide(ID_UNIT_ORDER_IRRIGATE);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_MINE)) {
        FREE(pOrder_Mine_Button->string16->text);
	time = map_build_mine_time(pUnit->x, pUnit->y);
	switch (terrain) {
	  /* set Irrigate Icon -> make swamp*/
	case T_FOREST:
	  my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
			_("Irrigate to"),
			tile_types[tile_types[terrain].mining_result
				].terrain_name
			," (M)", time , PL_("turn", "turns", time));
	  pOrder_Mine_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Mine_Button->theme = pTheme->OIrigation_Icon;
	  break;
	  /* set Forest Icon -> plant Forrest*/
	case T_JUNGLE:
	case T_PLAINS:
	case T_GRASSLAND:
	case T_SWAMP:
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Plant Forest"), " (M)", time , 
			PL_("turn", "turns", time));
	  pOrder_Mine_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Mine_Button->theme = pTheme->OForest_Icon;
	  break;
	  /* set Mining Icon */
	default:
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Mine"), " (M)", time , 
			PL_("turn", "turns", time));
	  pOrder_Mine_Button->string16->text = convert_to_utf16(cBuf);
	  pOrder_Mine_Button->theme = pTheme->OMine_Icon;
	  break;
	}

	clear_wflag(pOrder_Mine_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Mine_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_TRANSFORM)) {
	FREE(pOrder_Transform_Button->string16->text);
	time = map_transform_time(pUnit->x, pUnit->y);
	my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
	  _("Transform to"),
	  tile_types[tile_types[terrain].transform_result].terrain_name,
			" (M)", time , 
			PL_("turn", "turns", time));
	pOrder_Transform_Button->string16->text = convert_to_utf16(cBuf);
	clear_wflag(pOrder_Transform_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Transform_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_FORTRESS)) {
	local_show(ID_UNIT_ORDER_FORTRESS);
      } else {
	local_hide(ID_UNIT_ORDER_FORTRESS);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_FORTIFYING)) {
	local_show(ID_UNIT_ORDER_FORTIFY);
      } else {
	local_hide(ID_UNIT_ORDER_FORTIFY);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_AIRBASE)) {
	local_show(ID_UNIT_ORDER_AIRBASE);
      } else {
	local_hide(ID_UNIT_ORDER_AIRBASE);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_POLLUTION)) {
	local_show(ID_UNIT_ORDER_POLLUTION);
      } else {
	local_hide(ID_UNIT_ORDER_POLLUTION);
      }

      if (can_unit_paradrop(pUnit)) {
	local_show(ID_UNIT_ORDER_PARADROP);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_FALLOUT)) {
	local_show(ID_UNIT_ORDER_FALLOUT);
      } else {
	local_hide(ID_UNIT_ORDER_FALLOUT);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_SENTRY)) {
	local_show(ID_UNIT_ORDER_SENTRY);
      } else {
	local_hide(ID_UNIT_ORDER_SENTRY);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_PILLAGE)) {
	local_show(ID_UNIT_ORDER_PILLAGE);
      } else {
	local_hide(ID_UNIT_ORDER_PILLAGE);
      }

      if (can_unit_change_homecity(pUnit)) {
	local_show(ID_UNIT_ORDER_HOMECITY);
      } else {
	local_hide(ID_UNIT_ORDER_HOMECITY);
      }

      if (get_transporter_capacity(pUnit) > 0) {
	local_show(ID_UNIT_ORDER_UNLOAD);
      } else {
	local_hide(ID_UNIT_ORDER_UNLOAD);
      }

      if (is_unit_activity_on_tile(ACTIVITY_SENTRY, pUnit->x, pUnit->y)) {
	local_show(ID_UNIT_ORDER_WAKEUP_OTHERS);
      } else {
	local_hide(ID_UNIT_ORDER_WAKEUP_OTHERS);
      }

      if (can_unit_do_auto(pUnit)) {
	if (unit_flag(pUnit, F_CITIES)) {
	  local_show(ID_UNIT_ORDER_AUTO_SETTLER);
	  hide(ID_UNIT_ORDER_AUTO_AUTO_ATTACK);
	} else {
	  local_hide(ID_UNIT_ORDER_AUTO_SETTLER);
	  local_show(ID_UNIT_ORDER_AUTO_AUTO_ATTACK);
	}
      } else {
	local_hide(ID_UNIT_ORDER_AUTO_SETTLER);
	local_hide(ID_UNIT_ORDER_AUTO_AUTO_ATTACK);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_EXPLORE)) {
	local_show(ID_UNIT_ORDER_AUTO_EXPLORE);
      } else {
	local_hide(ID_UNIT_ORDER_AUTO_EXPLORE);
      }

      if (can_unit_do_connect(pUnit, ACTIVITY_IDLE)) {
	local_show(ID_UNIT_ORDER_CONNECT);
      } else {
	local_hide(ID_UNIT_ORDER_CONNECT);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_PATROL)) {
	local_show(ID_UNIT_ORDER_PATROL);
      } else {
	local_hide(ID_UNIT_ORDER_PATROL);
      }

      if (is_diplomat_unit(pUnit) &&
	  diplomat_can_do_action(pUnit, DIPLOMAT_ANY_ACTION, pUnit->x,
				 pUnit->y)) {
	local_show(ID_UNIT_ORDER_DIPLOMAT_DLG);
      } else {
	local_hide(ID_UNIT_ORDER_DIPLOMAT_DLG);
      }

      if (unit_flag(pUnit, F_NUCLEAR)) {
	local_show(ID_UNIT_ORDER_NUKE);
      } else {
	local_hide(ID_UNIT_ORDER_NUKE);
      }

      if (has_city_airport(map_get_city(pUnit->x, pUnit->y))) {
	local_show(ID_UNIT_ORDER_AIRLIFT);
	hide(ID_UNIT_ORDER_GOTO_CITY);
      } else {
	local_show(ID_UNIT_ORDER_GOTO_CITY);
	local_hide(ID_UNIT_ORDER_AIRLIFT);
      }


      set_new_order_widget_start_pos();
      counter = redraw_order_widgets();

    } else {
      if (counter) {
	hide_group(pBeginOrderWidgetList, pEndOrderWidgetList);
      }

      counter = 0;
    }
  }
}
