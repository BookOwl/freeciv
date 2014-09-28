/**********************************************************************
 Freeciv - Copyright (C) 1996-2006 - Freeciv Development Team
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
#include <fc_config.h>
#endif

/* utility */
#include "astring.h"
#include "fcintl.h"
#include "log.h"

/* common */
#include "actions.h"
#include "game.h"
#include "movement.h"
#include "research.h"
#include "traderoutes.h"
#include "unitlist.h"

/* client */
#include "client_main.h"
#include "control.h"

/* client/gui-sdl */
#include "citydlg.h"
#include "colors.h"
#include "dialogs.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "repodlgs.h"
#include "themespec.h"
#include "widget.h"

#include "dialogs_g.h"

struct diplomat_dialog {
  int actor_unit_id;
  int target_ids[ATK_COUNT];
  struct ADVANCED_DLG *pdialog;
};

struct small_diplomat_dialog {
  int actor_unit_id;
  int target_id;
  struct SMALL_DLG *pdialog;
};
 
extern bool is_unit_move_blocked;

void popdown_diplomat_dialog(void);
void popdown_incite_dialog(void);
void popdown_bribe_dialog(void);

static struct diplomat_dialog *pDiplomat_Dlg = NULL;

/* ====================================================================== */
/* ============================ CARAVAN DIALOG ========================== */
/* ====================================================================== */

/****************************************************************
  User selected that caravan should enter marketplace.
*****************************************************************/
static int caravan_marketplace_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    dsend_packet_unit_establish_trade(&client.conn,
                                      pDiplomat_Dlg->actor_unit_id,
                                      pDiplomat_Dlg->target_ids[ATK_CITY],
                                      FALSE);

    popdown_diplomat_dialog();
    choose_action_queue_next();
  }
  return -1;
}

/****************************************************************
  User selected that caravan should establish traderoute.
*****************************************************************/
static int caravan_establish_trade_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    dsend_packet_unit_establish_trade(&client.conn,
                                      pDiplomat_Dlg->actor_unit_id,
                                      pDiplomat_Dlg->target_ids[ATK_CITY],
                                      TRUE);

    popdown_diplomat_dialog();
    choose_action_queue_next();
  }
  return -1;
}

/****************************************************************
  User selected that caravan should help in wonder building.
*****************************************************************/
static int caravan_help_build_wonder_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    dsend_packet_unit_help_build_wonder(&client.conn,
        pDiplomat_Dlg->actor_unit_id, pDiplomat_Dlg->target_ids[ATK_CITY]);

    popdown_diplomat_dialog();
    choose_action_queue_next();
  }
  return -1;
}

/**************************************************************************
  Updates caravan dialog
**************************************************************************/
void caravan_dialog_update(void)
{
  /* PORT ME */
}

/* ====================================================================== */
/* ============================ DIPLOMAT DIALOG ========================= */
/* ====================================================================== */

/****************************************************************
  User interacted with diplomat dialog.
*****************************************************************/
static int diplomat_dlg_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pDiplomat_Dlg->pdialog->pBeginWidgetList, pWindow);
  }
  return -1;
}

/****************************************************************
  User clicked "Establish Embassy"
*****************************************************************/
static int diplomat_embassy_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_city_by_number(
          pDiplomat_Dlg->target_ids[ATK_CITY])
        && NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)) {
      request_do_action(ACTION_ESTABLISH_EMBASSY,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        0);
    }

    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/****************************************************************
  User clicked "Investigate City"
*****************************************************************/
static int diplomat_investigate_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
          pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_INVESTIGATE_CITY,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        0);
    }
  
    popdown_diplomat_dialog();

    /* FIXME: Wait for the city display in stead? */
    choose_action_queue_next();
  }

  return -1;
}

/****************************************************************
  User clicked "Poison City"
*****************************************************************/
static int spy_poison_callback( struct widget *pWidget )
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
          pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_POISON, pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        0);
    }
  
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/********************************************************************
  User clicked "Steal Gold"
********************************************************************/
static int spy_steal_gold_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
          pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_STEAL_GOLD,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        0);
    }

    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/****************************************************************
 Requests up-to-date list of improvements, the return of
 which will trigger the popup_sabotage_dialog() function.
*****************************************************************/
static int spy_sabotage_request(struct widget *pWidget)
{
  if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
      && NULL != game_city_by_number(
              pDiplomat_Dlg->target_ids[ATK_CITY])) {
    request_action_details(ACTION_SPY_TARGETED_SABOTAGE_CITY,
                           pDiplomat_Dlg->actor_unit_id,
                           pDiplomat_Dlg->target_ids[ATK_CITY]);
    popdown_diplomat_dialog();
  } else {
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }
  
  return -1;
}

/****************************************************************
  User clicked "Sabotage City" for diplomat (not spy)
*****************************************************************/
static int diplomat_sabotage_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
                pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_SABOTAGE_CITY,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        B_LAST + 1);
    }
    
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/* --------------------------------------------------------- */

/****************************************************************
  User interacted with spy's steal dialog window.
*****************************************************************/
static int spy_steal_dlg_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pDiplomat_Dlg->pdialog->pBeginWidgetList, pWindow);
  }
  return -1;
}

/****************************************************************
  Exit spy's steal dialog.
*****************************************************************/
static int exit_spy_steal_dlg_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;  
}

/****************************************************************
  User selected which tech spy steals.
*****************************************************************/
static int spy_steal_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    int steal_advance = MAX_ID - pWidget->ID;
  
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
                pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_TARGETED_STEAL_TECH,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        steal_advance);
    }
    
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/**************************************************************************
  Popup spy tech stealing dialog.
**************************************************************************/
static int spy_steal_popup(struct widget *pWidget)
{
  const struct research *presearch, *vresearch;
  struct city *pVcity = pWidget->data.city;
  int id = MAX_ID - pWidget->ID;
  struct player *pVictim = NULL;
  struct CONTAINER *pCont;
  struct widget *pBuf = NULL;
  struct widget *pWindow;
  SDL_String16 *pStr;
  SDL_Surface *pSurf;
  int max_col, max_row, col, i, count = 0;
  SDL_Rect area;

  popdown_diplomat_dialog();
  
  if(pVcity)
  {
    pVictim = city_owner(pVcity);
  }
  
  fc_assert_ret_val_msg(!pDiplomat_Dlg, 1, "Diplomat dialog already open");

  if (!pVictim) {
    choose_action_queue_next();
    return 1;
  }
  
  count = 0;
  presearch = research_get(client_player());
  vresearch = research_get(pVictim);
  advance_index_iterate(A_FIRST, i) {
    if (research_invention_gettable(presearch, i,
                                    game.info.tech_steal_allow_holes)
        && TECH_KNOWN == research_invention_state(vresearch, i)
        && TECH_KNOWN != research_invention_state(presearch, i)) {
      count++;
    }
  } advance_index_iterate_end;
  
  if(!count) {    
    /* if there is no known tech to steal then 
       send steal order at Spy's Discretion */
    int target_id = pVcity->id;

    request_do_action(ACTION_SPY_TARGETED_STEAL_TECH,
                      id, target_id, A_UNSET);

    choose_action_queue_next();

    return -1;
  }
    
  pCont = fc_calloc(1, sizeof(struct CONTAINER));
  pCont->id0 = pVcity->id;
  pCont->id1 = id;/* spy id */
  
  pDiplomat_Dlg = fc_calloc(1, sizeof(struct diplomat_dialog));
  pDiplomat_Dlg->actor_unit_id = id;
  pDiplomat_Dlg->target_ids[ATK_CITY] = pVcity->id;
  pDiplomat_Dlg->pdialog = fc_calloc(1, sizeof(struct ADVANCED_DLG));
      
  pStr = create_str16_from_char(_("Select Advance to Steal"), adj_font(12));
  pStr->style |= TTF_STYLE_BOLD;

  pWindow = create_window_skeleton(NULL, pStr, 0);
  
  pWindow->action = spy_steal_dlg_window_callback;
  set_wstate(pWindow , FC_WS_NORMAL);
  
  add_to_gui_list(ID_DIPLOMAT_DLG_WINDOW, pWindow);
  pDiplomat_Dlg->pdialog->pEndWidgetList = pWindow;
  
  area = pWindow->area;
  area.w = MAX(area.w, adj_size(8));  
  
  /* ------------------ */
  /* exit button */
  pBuf = create_themeicon(pTheme->Small_CANCEL_Icon, pWindow->dst,
                          WF_WIDGET_HAS_INFO_LABEL | WF_RESTORE_BACKGROUND);
  pBuf->info_label = create_str16_from_char(_("Close Dialog (Esc)"),
                                            adj_font(12));
  area.w += pBuf->size.w + adj_size(10);
  pBuf->action = exit_spy_steal_dlg_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->key = SDLK_ESCAPE;
  
  add_to_gui_list(ID_TERRAIN_ADV_DLG_EXIT_BUTTON, pBuf);  
  /* ------------------------- */
  
  count++; /* count + at Spy's Discretion */
  /* max col - 104 is steal tech widget width */
  max_col = (Main.screen->w - (pWindow->size.w - pWindow->area.w) - adj_size(2)) / adj_size(104);
  /* max row - 204 is steal tech widget height */
  max_row = (Main.screen->h - (pWindow->size.h - pWindow->area.h)) / adj_size(204);
  
  /* make space on screen for scrollbar */
  if (max_col * max_row < count) {
    max_col--;
  }

  if (count < max_col + 1) {
    col = count;
  } else {
    if (count < max_col + adj_size(3)) {
      col = max_col - adj_size(2);
    } else {
      if (count < max_col + adj_size(5)) {
        col = max_col - 1;
      } else {
        col = max_col;
      }
    }
  }
  
  pStr = create_string16(NULL, 0, adj_font(10));
  pStr->style |= (TTF_STYLE_BOLD | SF_CENTER);
  
  count = 0;
  advance_index_iterate(A_FIRST, i) {
    if (research_invention_gettable(presearch, i,
                                    game.info.tech_steal_allow_holes)
        && TECH_KNOWN == research_invention_state(vresearch, i)
        && TECH_KNOWN != research_invention_state(presearch, i)) {
      count++;

      copy_chars_to_string16(pStr, advance_name_translation(advance_by_number(i)));
      pSurf = create_sellect_tech_icon(pStr, i, FULL_MODE);
      pBuf = create_icon2(pSurf, pWindow->dst,
      		WF_FREE_THEME | WF_RESTORE_BACKGROUND);

      set_wstate(pBuf, FC_WS_NORMAL);
      pBuf->action = spy_steal_callback;
      pBuf->data.cont = pCont;

      add_to_gui_list(MAX_ID - i, pBuf);
    
      if (count > (col * max_row)) {
        set_wflag(pBuf, WF_HIDDEN);
      }
    }
  } advance_index_iterate_end;
  
  /* Get Spy tech to use for "At Spy's Discretion" -- this will have the
   * side effect of displaying the unit's icon */
  i = advance_number(unit_type(game_unit_by_number(id))->require_advance);
  {
    struct astring str = ASTRING_INIT;
    /* TRANS: %s is a unit name, e.g., Spy */
    astr_set(&str, _("At %s's Discretion"),
             unit_name_translation(game_unit_by_number(id)));
    copy_chars_to_string16(pStr, astr_str(&str));
    astr_free(&str);
  }
  pSurf = create_sellect_tech_icon(pStr, i, FULL_MODE);
	
  pBuf = create_icon2(pSurf, pWindow->dst,
    	(WF_FREE_THEME | WF_RESTORE_BACKGROUND| WF_FREE_DATA));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = spy_steal_callback;
  pBuf->data.cont = pCont;
    
  add_to_gui_list(MAX_ID - A_UNSET, pBuf);
  count++;
  
  /* --------------------------------------------------------- */
  FREESTRING16(pStr);
  pDiplomat_Dlg->pdialog->pBeginWidgetList = pBuf;
  pDiplomat_Dlg->pdialog->pBeginActiveWidgetList = pDiplomat_Dlg->pdialog->pBeginWidgetList;
  pDiplomat_Dlg->pdialog->pEndActiveWidgetList = pDiplomat_Dlg->pdialog->pEndWidgetList->prev->prev;
  
  /* -------------------------------------------------------------- */
  
  i = 0;
  if (count > col) {
    count = (count + (col - 1)) / col;
    if (count > max_row) {
      pDiplomat_Dlg->pdialog->pActiveWidgetList = pDiplomat_Dlg->pdialog->pEndActiveWidgetList;
      count = max_row;
      i = create_vertical_scrollbar(pDiplomat_Dlg->pdialog, col, count, TRUE, TRUE);  
    }
  } else {
    count = 1;
  }

  area.w = MAX(area.w, (col * pBuf->size.w + adj_size(2) + i));
  area.h = count * pBuf->size.h + adj_size(2);

  /* alloca window theme and win background buffer */
  pSurf = theme_get_background(theme, BACKGROUND_SPYSTEALDLG);
  if (resize_window(pWindow, pSurf, NULL,
                    (pWindow->size.w - pWindow->area.w) + area.w,
                    (pWindow->size.h - pWindow->area.h) + area.h))
  {
    FREESURFACE(pSurf);
  }
  
  area = pWindow->area;
  
  widget_set_position(pWindow,
                      (Main.screen->w - pWindow->size.w) / 2,
                      (Main.screen->h - pWindow->size.h) / 2);
  
    /* exit button */
  pBuf = pWindow->prev;
  pBuf->size.x = area.x + area.w - pBuf->size.w - 1;
  pBuf->size.y = pWindow->size.y + adj_size(2);
  
  setup_vertical_widgets_position(col, area.x + 1,
		  area.y, 0, 0,
		  pDiplomat_Dlg->pdialog->pBeginActiveWidgetList,
  		  pDiplomat_Dlg->pdialog->pEndActiveWidgetList);
    
  if(pDiplomat_Dlg->pdialog->pScroll) {
    setup_vertical_scrollbar_area(pDiplomat_Dlg->pdialog->pScroll,
	area.x + area.w, area.y,
    	area.h, TRUE);
  }

  redraw_group(pDiplomat_Dlg->pdialog->pBeginWidgetList, pWindow, FALSE);
  widget_mark_dirty(pWindow);
  
  return -1;
}

/****************************************************************
  Technology stealing dialog, diplomat (not spy) version
*****************************************************************/
static int diplomat_steal_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
                pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_do_action(ACTION_SPY_STEAL_TECH,
                        pDiplomat_Dlg->actor_unit_id,
                        pDiplomat_Dlg->target_ids[ATK_CITY],
                        A_UNSET);
    }
    
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/****************************************************************
...  Ask the server how much the revolt is going to cost us
*****************************************************************/
static int diplomat_incite_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL != game_city_by_number(
                pDiplomat_Dlg->target_ids[ATK_CITY])) {
      request_action_details(ACTION_SPY_INCITE_CITY,
                             pDiplomat_Dlg->actor_unit_id,
                             pDiplomat_Dlg->target_ids[ATK_CITY]);
      popdown_diplomat_dialog();
    } else {
      popdown_diplomat_dialog();
      choose_action_queue_next();
    }
  }  
  return -1;
}

/********************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering a tile with an allied city
  or an allied unit.)
*********************************************************************/
static int diplomat_keep_moving_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *punit;

    if ((punit = game_unit_by_number(pDiplomat_Dlg->actor_unit_id))
        && !same_pos(unit_tile(punit), pWidget->data.tile)) {
      request_do_action(ACTION_MOVE, pDiplomat_Dlg->actor_unit_id,
                        pWidget->data.tile->index, 0);
    }
    
    popdown_diplomat_dialog();  
    choose_action_queue_next();
  }
  return -1;
}

/****************************************************************
...  Ask the server how much the bribe is
*****************************************************************/
static int diplomat_bribe_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pDiplomat_Dlg->actor_unit_id)
        && NULL !=
         game_unit_by_number(pDiplomat_Dlg->target_ids[ATK_UNIT])) {
      request_action_details(ACTION_SPY_BRIBE_UNIT,
                             pDiplomat_Dlg->actor_unit_id,
                             pDiplomat_Dlg->target_ids[ATK_UNIT]);
      popdown_diplomat_dialog();
    } else {
      popdown_diplomat_dialog();
      choose_action_queue_next();
    }
  }  
  return -1;
}

/****************************************************************
  User clicked "Sabotage Unit"
*****************************************************************/
static int spy_sabotage_unit_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    int diplomat_id = MAX_ID - pWidget->ID;
    int target_id = pWidget->data.unit->id;
    
    popdown_diplomat_dialog();
    request_do_action(ACTION_SPY_SABOTAGE_UNIT, diplomat_id, target_id, 0);

    choose_action_queue_next();
  }

  return -1;
}

/****************************************************************
  Close diplomat dialog.
*****************************************************************/
static int diplomat_close_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_diplomat_dialog();
    choose_action_queue_next();
  }

  return -1;
}

/**************************************************************************
  Popdown a dialog giving a diplomatic unit some options when moving into
  the target tile.
**************************************************************************/
void popdown_diplomat_dialog(void)
{
  if (pDiplomat_Dlg) {
    is_unit_move_blocked = FALSE;
    popdown_window_group_dialog(pDiplomat_Dlg->pdialog->pBeginWidgetList,
				pDiplomat_Dlg->pdialog->pEndWidgetList);
    FC_FREE(pDiplomat_Dlg->pdialog->pScroll);
    FC_FREE(pDiplomat_Dlg->pdialog);
    FC_FREE(pDiplomat_Dlg);
    queue_flush();
  }
}

/**************************************************************************
  Add an entry for an action in the action choise dialog.
**************************************************************************/
static void action_entry(const enum gen_action act,
                         const int *action_probabilities,
                         int (*callback) (struct widget *),
                         struct unit *act_unit,
                         struct city *tgt_city,
                         struct unit *tgt_unit,
                         struct widget *pWindow,
                         SDL_Rect *area)
{
  struct widget *pBuf;
  SDL_String16 *pStr;
  const char *ui_name;

  /* Don't show disabled actions */
  if (!action_prob_possible(action_probabilities[act])) {
    return;
  }

  ui_name = action_prepare_ui_name(act, "", action_probabilities[act]);

  create_active_iconlabel(pBuf, pWindow->dst, pStr,
                          ui_name, callback);

  switch(action_get_target_kind(act)) {
  case ATK_CITY:
    pBuf->data.city = tgt_city;
    break;
  case ATK_UNIT:
    pBuf->data.unit = tgt_unit;
    break;
  case ATK_COUNT:
    fc_assert_msg(FALSE, "Unsupported target kind");
  }

  set_wstate(pBuf, FC_WS_NORMAL);

  add_to_gui_list(MAX_ID - act_unit->id, pBuf);

  area->w = MAX(area->w, pBuf->size.w);
  area->h += pBuf->size.h;
}

/**************************************************************************
  Popup a dialog that allows the player to select what action a unit
  should take.
**************************************************************************/
void popup_action_selection(struct unit *actor_unit,
                            struct city *target_city,
                            struct unit *target_unit,
                            struct tile *target_tile,
                            const action_probability *act_probs)
{
  struct widget *pWindow = NULL, *pBuf = NULL;
  SDL_String16 *pStr;
  SDL_Rect area;
  struct city *actor_homecity;

  char cBuf[128];
  bool can_marketplace;
  bool can_traderoute;
  bool can_wonder;
  
  fc_assert_ret_msg(!pDiplomat_Dlg, "Diplomat dialog already open");

  is_unit_move_blocked = TRUE;

  actor_homecity = game_city_by_number(actor_unit->homecity);

  can_marketplace = unit_has_type_flag(actor_unit, UTYF_TRADE_ROUTE)
                    && target_city
                    && can_cities_trade(actor_homecity, target_city);
  can_traderoute = can_marketplace
                   && can_establish_trade_route(actor_homecity,
                                                target_city);
  can_wonder = target_city
               && unit_can_help_build_wonder(actor_unit, target_city);
  
  pDiplomat_Dlg = fc_calloc(1, sizeof(struct diplomat_dialog));
  pDiplomat_Dlg->actor_unit_id = actor_unit->id;
  pDiplomat_Dlg->pdialog = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  
  /* window */
  if (target_city) {
    struct astring title = ASTRING_INIT;
    /* TRANS: %s is a unit name, e.g., Spy */
    astr_set(&title, _("Choose Your %s's Strategy"),
             unit_name_translation(actor_unit));
    pStr = create_str16_from_char(astr_str(&title), adj_font(12));
    astr_free(&title);
  } else {
    pStr = create_str16_from_char(_("Subvert Enemy Unit"), adj_font(12));
  }
  
  pStr->style |= TTF_STYLE_BOLD;
  
  pWindow = create_window_skeleton(NULL, pStr, 0);
    
  pWindow->action = diplomat_dlg_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);
  
  add_to_gui_list(ID_CARAVAN_DLG_WINDOW, pWindow);
  pDiplomat_Dlg->pdialog->pEndWidgetList = pWindow;
    
  area = pWindow->area;
  area.w = MAX(area.w, adj_size(8));
  area.h = MAX(area.h, adj_size(2));

  if (target_city) {
    pDiplomat_Dlg->target_ids[ATK_CITY] = target_city->id;
  } else {
    pDiplomat_Dlg->target_ids[ATK_CITY] = IDENTITY_NUMBER_ZERO;
  }

  if (target_unit) {
    pDiplomat_Dlg->target_ids[ATK_UNIT] = target_unit->id;
  } else {
    pDiplomat_Dlg->target_ids[ATK_UNIT] = IDENTITY_NUMBER_ZERO;
  }

  /* ---------- */
  /* Spy/Diplomat acting against a city */

  action_entry(ACTION_ESTABLISH_EMBASSY,
               act_probs,
               diplomat_embassy_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_INVESTIGATE_CITY,
               act_probs,
               diplomat_investigate_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_POISON,
               act_probs,
               spy_poison_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_STEAL_GOLD,
               act_probs,
               spy_steal_gold_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_SABOTAGE_CITY,
               act_probs,
               diplomat_sabotage_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_TARGETED_SABOTAGE_CITY,
               act_probs,
               spy_sabotage_request,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_STEAL_TECH,
               act_probs,
               diplomat_steal_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_TARGETED_STEAL_TECH,
               act_probs,
               spy_steal_popup,
               actor_unit, target_city, NULL,
               pWindow, &area);

  action_entry(ACTION_SPY_INCITE_CITY,
               act_probs,
               diplomat_incite_callback,
               actor_unit, target_city, NULL,
               pWindow, &area);

  /* ---------- */
  if (can_traderoute) {
    int revenue = get_caravan_enter_city_trade_bonus(actor_homecity,
                                                     target_city);

    fc_snprintf(cBuf, sizeof(cBuf),
                _("Establish Trade route with %s ( %d R&G + %d trade )"),
                city_name(actor_homecity),
                revenue,
                trade_between_cities(actor_homecity, target_city));

    create_active_iconlabel(pBuf, pWindow->dst, pStr,
                            cBuf, caravan_establish_trade_callback);
    set_wstate(pBuf, FC_WS_NORMAL);

    add_to_gui_list(ID_LABEL, pBuf);

    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }

  /* ---------- */
  if (can_marketplace) {
    int revenue = get_caravan_enter_city_trade_bonus(actor_homecity,
                                                     target_city);

    revenue = (revenue + 2) / 3;
    fc_snprintf(cBuf, sizeof(cBuf),
                _("Enter Marketplace ( %d R&G bonus )"), revenue);

    create_active_iconlabel(pBuf, pWindow->dst, pStr,
                            cBuf, caravan_marketplace_callback);
    set_wstate(pBuf, FC_WS_NORMAL);

    add_to_gui_list(ID_LABEL, pBuf);

    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }

  /* ---------- */
  if (can_wonder) {
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
        _("Help build Wonder"), caravan_help_build_wonder_callback);

    set_wstate(pBuf, FC_WS_NORMAL);

    add_to_gui_list(ID_LABEL, pBuf);

    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }

  /* Spy/Diplomat acting against a unit */

  action_entry(ACTION_SPY_BRIBE_UNIT,
               act_probs,
               diplomat_bribe_callback,
               actor_unit, NULL, target_unit,
               pWindow, &area);

  action_entry(ACTION_SPY_SABOTAGE_UNIT,
               act_probs,
               spy_sabotage_unit_callback,
               actor_unit, NULL, target_unit,
               pWindow, &area);

  /* ---------- */
  if (unit_can_move_to_tile(actor_unit, target_tile, FALSE)) {

    create_active_iconlabel(pBuf, pWindow->dst, pStr,
                            _("Keep moving"),
                            diplomat_keep_moving_callback);

    pBuf->data.tile = target_tile;

    set_wstate(pBuf, FC_WS_NORMAL);

    add_to_gui_list(MAX_ID - actor_unit->id, pBuf);

    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }

  /* ---------- */
  
  create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("Cancel"), diplomat_close_callback);
    
  set_wstate(pBuf , FC_WS_NORMAL);
  pBuf->key = SDLK_ESCAPE;
  
  add_to_gui_list(ID_LABEL , pBuf);
    
  area.w = MAX(area.w, pBuf->size.w);
  area.h += pBuf->size.h;
  /* ---------- */
  pDiplomat_Dlg->pdialog->pBeginWidgetList = pBuf;
  
  /* setup window size and start position */

  resize_window(pWindow, NULL, NULL, 
                (pWindow->size.w - pWindow->area.w) + area.w,
                (pWindow->size.h - pWindow->area.h) + area.h);
  
  area = pWindow->area;
  
  auto_center_on_focus_unit();
  put_window_near_map_tile(pWindow, pWindow->size.w, pWindow->size.h,
                           unit_tile(actor_unit));
 
  /* setup widget size and start position */
    
  pBuf = pWindow->prev;
  setup_vertical_widgets_position(1,
	area.x,
  	area.y + 1, area.w, 0,
	pDiplomat_Dlg->pdialog->pBeginWidgetList, pBuf);
  
  /* --------------------- */
  /* redraw */
  redraw_group(pDiplomat_Dlg->pdialog->pBeginWidgetList, pWindow, 0);

  widget_flush(pWindow);
  
}

/**************************************************************************
  Returns the id of the actor unit currently handled in action selection
  dialog when the action selection dialog is open.
  Returns IDENTITY_NUMBER_ZERO if no action selection dialog is open.
**************************************************************************/
int action_selection_actor_unit(void)
{
  if (!pDiplomat_Dlg) {
    return IDENTITY_NUMBER_ZERO;
  }

  return pDiplomat_Dlg->actor_unit_id;
}

/**************************************************************************
  Returns id of the target city of the actions currently handled in action
  selection dialog when the action selection dialog is open and it has a
  city target. Returns IDENTITY_NUMBER_ZERO if no action selection dialog
  is open or no city target is present in the action selection dialog.
**************************************************************************/
int action_selection_target_city(void)
{
  if (!pDiplomat_Dlg) {
    return IDENTITY_NUMBER_ZERO;
  }

  return pDiplomat_Dlg->target_ids[ATK_CITY];
}

/****************************************************************
  Closes the diplomat dialog
****************************************************************/
void close_diplomat_dialog(void)
{
  popdown_diplomat_dialog();
}

/* ====================================================================== */
/* ============================ SABOTAGE DIALOG ========================= */
/* ====================================================================== */

/****************************************************************
  User selected what to sabotage.
****************************************************************/
static int sabotage_impr_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    int sabotage_improvement = MAX_ID - pWidget->ID;
    int diplomat_target_id = pWidget->data.cont->id0;
    int diplomat_id = pWidget->data.cont->id1;
      
    popdown_advanced_terrain_dialog();
    
    if(sabotage_improvement == 1000)
    {
      sabotage_improvement = -1;
    }

    if (NULL != game_unit_by_number(diplomat_id)
        && NULL != game_city_by_number(diplomat_target_id)) {
      request_do_action(ACTION_SPY_TARGETED_SABOTAGE_CITY, diplomat_id,
                        diplomat_target_id, sabotage_improvement + 1);
    }

    choose_action_queue_next();
  }

  return -1;
}

/*************************************************************************
 Pops-up the Spy sabotage dialog, upon return of list of
 available improvements requested by the above function.
**************************************************************************/
void popup_sabotage_dialog(struct unit *actor, struct city *pCity)
{
  struct widget *pWindow = NULL, *pBuf = NULL , *pLast = NULL;
  struct CONTAINER *pCont;
  SDL_String16 *pStr;
  SDL_Rect area, area2;
  int n, w = 0, h, imp_h = 0, y;

  fc_assert_ret_msg(!pDiplomat_Dlg, "Diplomat dialog already open");

  if (!actor) {
    choose_action_queue_next();
    return;
  }
  
  is_unit_move_blocked = TRUE;
    
  pDiplomat_Dlg = fc_calloc(1, sizeof(struct diplomat_dialog));
  pDiplomat_Dlg->actor_unit_id = actor->id;
  pDiplomat_Dlg->target_ids[ATK_CITY] = pCity->id;
  pDiplomat_Dlg->pdialog = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  
  pCont = fc_calloc(1, sizeof(struct CONTAINER));
  pCont->id0 = pCity->id;
  pCont->id1 = actor->id; /* spy id */
    
  pStr = create_str16_from_char(_("Select Improvement to Sabotage") , adj_font(12));
  pStr->style |= TTF_STYLE_BOLD;
  
  pWindow = create_window_skeleton(NULL, pStr, 0);
    
  pWindow->action = diplomat_dlg_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);
  
  add_to_gui_list(ID_TERRAIN_ADV_DLG_WINDOW, pWindow);
  pDiplomat_Dlg->pdialog->pEndWidgetList = pWindow;
  
  area = pWindow->area;
  area.h = MAX(area.h, adj_size(2));
  
  /* ---------- */
  /* exit button */
  pBuf = create_themeicon(pTheme->Small_CANCEL_Icon, pWindow->dst,
                          WF_WIDGET_HAS_INFO_LABEL | WF_RESTORE_BACKGROUND);
  pBuf->info_label = create_str16_from_char(_("Close Dialog (Esc)"),
                                            adj_font(12));
  area.w += pBuf->size.w + adj_size(10);
  pBuf->action = diplomat_close_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->key = SDLK_ESCAPE;
  
  add_to_gui_list(ID_TERRAIN_ADV_DLG_EXIT_BUTTON, pBuf);
  /* ---------- */
  
  create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("City Production"), sabotage_impr_callback);
  pBuf->data.cont = pCont;  
  set_wstate(pBuf, FC_WS_NORMAL);
  set_wflag(pBuf, WF_FREE_DATA);
  add_to_gui_list(MAX_ID - 1000, pBuf);
    
  area.w = MAX(area.w, pBuf->size.w);
  area.h += pBuf->size.h;

  /* separator */
  pBuf = create_iconlabel(NULL, pWindow->dst, NULL, WF_FREE_THEME);
    
  add_to_gui_list(ID_SEPARATOR, pBuf);
  area.h += pBuf->next->size.h;

  pDiplomat_Dlg->pdialog->pEndActiveWidgetList = pBuf;

  /* ------------------ */
  n = 0;
  city_built_iterate(pCity, pImprove) {
    if (pImprove->sabotage > 0) {
      
      create_active_iconlabel(pBuf, pWindow->dst, pStr,
	      (char *) city_improvement_name_translation(pCity, pImprove),
				      sabotage_impr_callback);
      pBuf->data.cont = pCont;
      set_wstate(pBuf , FC_WS_NORMAL);
  
      add_to_gui_list(MAX_ID - improvement_number(pImprove), pBuf);
    
      area.w = MAX(area.w , pBuf->size.w);
      imp_h += pBuf->size.h;
    
      if (n > 9) {
        set_wflag(pBuf, WF_HIDDEN);
      }
      
      n++;    
      /* ----------- */
    }  
  } city_built_iterate_end;

  pDiplomat_Dlg->pdialog->pBeginActiveWidgetList = pBuf;
  
  if (n > 0) {
    /* separator */
    pBuf = create_iconlabel(NULL, pWindow->dst, NULL, WF_FREE_THEME);
    
    add_to_gui_list(ID_SEPARATOR, pBuf);
    area.h += pBuf->next->size.h;
  /* ------------------ */
  }
  
  {
    struct astring str = ASTRING_INIT;
    /* TRANS: %s is a unit name, e.g., Spy */
    astr_set(&str, _("At %s's Discretion"), unit_name_translation(actor));
    create_active_iconlabel(pBuf, pWindow->dst, pStr, astr_str(&str),
                            sabotage_impr_callback);
    astr_free(&str);
  }
  pBuf->data.cont = pCont;  
  set_wstate(pBuf, FC_WS_NORMAL);
  
  add_to_gui_list(MAX_ID - B_LAST, pBuf);
    
  area.w = MAX(area.w, pBuf->size.w);
  area.h += pBuf->size.h;
  /* ----------- */
  
  pLast = pBuf;
  pDiplomat_Dlg->pdialog->pBeginWidgetList = pLast;
  pDiplomat_Dlg->pdialog->pActiveWidgetList = pDiplomat_Dlg->pdialog->pEndActiveWidgetList;
  
  /* ---------- */
  if (n > 10)
  {
    imp_h = 10 * pBuf->size.h;
    
    n = create_vertical_scrollbar(pDiplomat_Dlg->pdialog,
		  1, 10, TRUE, TRUE);
    area.w += n;
  } else {
    n = 0;
  }
  /* ---------- */
  
  
  area.h += imp_h;

  resize_window(pWindow, NULL, NULL, 
                (pWindow->size.w - pWindow->area.w) + area.w,
                (pWindow->size.h - pWindow->area.h) + area.h);
  
  area = pWindow->area;
  
  auto_center_on_focus_unit();
  put_window_near_map_tile(pWindow, pWindow->size.w, pWindow->size.h,
                           unit_tile(actor));
  
  w = area.w;
  
  /* exit button */
  pBuf = pWindow->prev;
  pBuf->size.x = area.x + area.w - pBuf->size.w - 1;
  pBuf->size.y = pWindow->size.y + adj_size(2);
  
  /* Production sabotage */
  pBuf = pBuf->prev;
  
  pBuf->size.x = area.x;
  pBuf->size.y = y = area.y + 1;
  pBuf->size.w = w;
  h = pBuf->size.h;
  
  area2.x = adj_size(10);
  area2.h = adj_size(2);
  
  pBuf = pBuf->prev;
  while(pBuf)
  {
    
    if (pBuf == pDiplomat_Dlg->pdialog->pEndActiveWidgetList) {
      w -= n;
    }
    
    pBuf->size.w = w;
    pBuf->size.x = pBuf->next->size.x;
    pBuf->size.y = y = y + pBuf->next->size.h;
    
    if (pBuf->ID == ID_SEPARATOR)
    {
      FREESURFACE(pBuf->theme);
      pBuf->size.h = h;
      pBuf->theme = create_surf(w, h, SDL_SWSURFACE);
    
      area2.y = pBuf->size.h / 2 - 1;
      area2.w = pBuf->size.w - adj_size(20);
      
      SDL_FillRect(pBuf->theme , &area2, map_rgba(pBuf->theme->format, *get_theme_color(COLOR_THEME_SABOTAGEDLG_SEPARATOR)));
    }
    
    if (pBuf == pLast) {
      break;
    }
    if (pBuf == pDiplomat_Dlg->pdialog->pBeginActiveWidgetList) {
      /* Reset to end of scrolling area */
      y = MIN(y, pDiplomat_Dlg->pdialog->pEndActiveWidgetList->size.y
              + 9 * pBuf->size.h);
      w += n;
    }
    pBuf = pBuf->prev;  
  }
  
  if (pDiplomat_Dlg->pdialog->pScroll)
  {
    setup_vertical_scrollbar_area(pDiplomat_Dlg->pdialog->pScroll,
        area.x + area.w,
        pDiplomat_Dlg->pdialog->pEndActiveWidgetList->size.y,
        pDiplomat_Dlg->pdialog->pBeginActiveWidgetList->prev->size.y
          - pDiplomat_Dlg->pdialog->pEndActiveWidgetList->size.y,
        TRUE);
  }
  
  /* -------------------- */
  /* redraw */
  redraw_group(pDiplomat_Dlg->pdialog->pBeginWidgetList, pWindow, 0);

  widget_flush(pWindow);

}

/* ====================================================================== */
/* ============================== INCITE DIALOG ========================= */
/* ====================================================================== */
static struct small_diplomat_dialog *pIncite_Dlg = NULL;

/****************************************************************
  User interacted with Incite Revolt dialog window.
*****************************************************************/
static int incite_dlg_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pIncite_Dlg->pdialog->pBeginWidgetList, pWindow);
  }
  return -1;
}

/****************************************************************
  User confirmed incite
*****************************************************************/
static int diplomat_incite_yes_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pIncite_Dlg->actor_unit_id)
        && NULL != game_city_by_number(pIncite_Dlg->target_id)) {
      request_do_action(ACTION_SPY_INCITE_CITY, pIncite_Dlg->actor_unit_id,
                        pIncite_Dlg->target_id, 0);
    }
    popdown_incite_dialog();
    choose_action_queue_next();
  }  
  return -1;
}

/****************************************************************
  Close incite dialog.
*****************************************************************/
static int exit_incite_dlg_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_incite_dialog();
    choose_action_queue_next();
  }
  return -1;
}

/**************************************************************************
  Popdown a window asking a diplomatic unit if it wishes to incite the
  given enemy city.
**************************************************************************/
void popdown_incite_dialog(void)
{
  if (pIncite_Dlg) {
    is_unit_move_blocked = FALSE;
    popdown_window_group_dialog(pIncite_Dlg->pdialog->pBeginWidgetList,
				pIncite_Dlg->pdialog->pEndWidgetList);
    FC_FREE(pIncite_Dlg->pdialog);
    FC_FREE(pIncite_Dlg);
    flush_dirty();
  }
}

/**************************************************************************
  Popup a window asking a diplomatic unit if it wishes to incite the
  given enemy city.
**************************************************************************/
void popup_incite_dialog(struct unit *actor, struct city *pCity, int cost)
{
  struct widget *pWindow = NULL, *pBuf = NULL;
  SDL_String16 *pStr;
  char tBuf[255], cBuf[255];
  bool exit = FALSE;
  SDL_Rect area;
  
  if (pIncite_Dlg) {
    return;
  }
  
  if (!actor || !is_actor_unit(actor)) {
    choose_action_queue_next();
    return;
  }
  
  is_unit_move_blocked = TRUE;
  
  pIncite_Dlg = fc_calloc(1, sizeof(struct small_diplomat_dialog));
  pIncite_Dlg->actor_unit_id = actor->id;
  pIncite_Dlg->target_id = pCity->id;
  pIncite_Dlg->pdialog = fc_calloc(1, sizeof(struct SMALL_DLG));  

  fc_snprintf(tBuf, ARRAY_SIZE(tBuf), PL_("Treasury contains %d gold.",
                                          "Treasury contains %d gold.",
                                          client_player()->economic.gold),
              client_player()->economic.gold);

  /* window */
  pStr = create_str16_from_char(_("Incite a Revolt!"), adj_font(12));
    
  pStr->style |= TTF_STYLE_BOLD;
  
  pWindow = create_window_skeleton(NULL, pStr, 0);
    
  pWindow->action = incite_dlg_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);
  
  add_to_gui_list(ID_INCITE_DLG_WINDOW, pWindow);
  pIncite_Dlg->pdialog->pEndWidgetList = pWindow;
  
  area = pWindow->area;
  area.w  =MAX(area.w, adj_size(8));
  area.h = MAX(area.h, adj_size(2));
  
  if (INCITE_IMPOSSIBLE_COST == cost) {
    
    /* exit button */
    pBuf = create_themeicon(pTheme->Small_CANCEL_Icon, pWindow->dst,
                            WF_WIDGET_HAS_INFO_LABEL
                            | WF_RESTORE_BACKGROUND);
    pBuf->info_label = create_str16_from_char(_("Close Dialog (Esc)"),
                                              adj_font(12));
    area.w += pBuf->size.w + adj_size(10);
    pBuf->action = exit_incite_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->key = SDLK_ESCAPE;
  
    add_to_gui_list(ID_INCITE_DLG_EXIT_BUTTON, pBuf);
    exit = TRUE;
    /* --------------- */
    
    fc_snprintf(cBuf, sizeof(cBuf), _("You can't incite a revolt in %s."),
		city_name(pCity));

    create_active_iconlabel(pBuf, pWindow->dst, pStr, cBuf, NULL);
        
    add_to_gui_list(ID_LABEL , pBuf);
    
    area.w = MAX(area.w , pBuf->size.w);
    area.h += pBuf->size.h;
    /*------------*/
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("City can't be incited!"), NULL);
        
    add_to_gui_list(ID_LABEL , pBuf);
    
    area.w = MAX(area.w , pBuf->size.w);
    area.h += pBuf->size.h;
    
  } else if (cost <= client_player()->economic.gold) {
    fc_snprintf(cBuf, sizeof(cBuf),
                /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
                PL_("Incite a revolt for %d gold?\n%s",
                    "Incite a revolt for %d gold?\n%s", cost), cost, tBuf);

    create_active_iconlabel(pBuf, pWindow->dst, pStr, cBuf, NULL);
        
  
    add_to_gui_list(ID_LABEL , pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
    /*------------*/
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("Yes") , diplomat_incite_yes_callback);
    
    pBuf->data.city = pCity;
    set_wstate(pBuf, FC_WS_NORMAL);
  
    add_to_gui_list(MAX_ID - actor->id, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    /* ------- */
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("No") , exit_incite_dlg_callback);
    
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->key = SDLK_ESCAPE;
    
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
  } else {
    /* exit button */
    pBuf = create_themeicon(pTheme->Small_CANCEL_Icon, pWindow->dst,
                            WF_WIDGET_HAS_INFO_LABEL
                            | WF_RESTORE_BACKGROUND);
    pBuf->info_label = create_str16_from_char(_("Close Dialog (Esc)"),
                                              adj_font(12));
    area.w += pBuf->size.w + adj_size(10);
    pBuf->action = exit_incite_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->key = SDLK_ESCAPE;
  
    add_to_gui_list(ID_INCITE_DLG_EXIT_BUTTON, pBuf);
    exit = TRUE;
    /* --------------- */

    fc_snprintf(cBuf, sizeof(cBuf),
                /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
                PL_("Inciting a revolt costs %d gold.\n%s",
                    "Inciting a revolt costs %d gold.\n%s", cost), cost, tBuf);

    create_active_iconlabel(pBuf, pWindow->dst, pStr, cBuf, NULL);
        
  
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
    /*------------*/
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("Traitors Demand Too Much!"), NULL);
        
    add_to_gui_list(ID_LABEL , pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }
  pIncite_Dlg->pdialog->pBeginWidgetList = pBuf;
  
  /* setup window size and start position */

  resize_window(pWindow, NULL, NULL,
                (pWindow->size.w - pWindow->area.w) + area.w,
                (pWindow->size.h - pWindow->area.h) + area.h);
  
  area = pWindow->area;
  
  auto_center_on_focus_unit();
  put_window_near_map_tile(pWindow, pWindow->size.w, pWindow->size.h,
                           pCity->tile);
  
  /* setup widget size and start position */
  pBuf = pWindow;
  
  if (exit)
  {/* exit button */
    pBuf = pBuf->prev;
    pBuf->size.x = area.x + area.w - pBuf->size.w - 1;
    pBuf->size.y = pWindow->size.y + adj_size(2);
  }
  
  pBuf = pBuf->prev;
  setup_vertical_widgets_position(1,
	area.x,
  	area.y + 1, area.w, 0,
	pIncite_Dlg->pdialog->pBeginWidgetList, pBuf);
    
  /* --------------------- */
  /* redraw */
  redraw_group(pIncite_Dlg->pdialog->pBeginWidgetList, pWindow, 0);

  widget_flush(pWindow);
  
}

/* ====================================================================== */
/* ============================ BRIBE DIALOG ========================== */
/* ====================================================================== */
static struct small_diplomat_dialog *pBribe_Dlg = NULL;

/**************************************************************************
  User interacted with bribe dialog window.
**************************************************************************/
static int bribe_dlg_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pBribe_Dlg->pdialog->pBeginWidgetList, pWindow);
  }
  return -1;
}

/**************************************************************************
  User confirmed bribe.
**************************************************************************/
static int diplomat_bribe_yes_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (NULL != game_unit_by_number(pBribe_Dlg->actor_unit_id)
        && NULL != game_unit_by_number(pBribe_Dlg->target_id)) {
      request_do_action(ACTION_SPY_BRIBE_UNIT, pBribe_Dlg->actor_unit_id,
                        pBribe_Dlg->target_id, 0);
    }
    popdown_bribe_dialog();
    choose_action_queue_next();
  }  
  return -1;
}

/**************************************************************************
  Close bribe dialog.
**************************************************************************/
static int exit_bribe_dlg_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_bribe_dialog();
    choose_action_queue_next();
  }
  return -1;
}

/**************************************************************************
  Popdown a dialog asking a diplomatic unit if it wishes to bribe the
  given enemy unit.
**************************************************************************/
void popdown_bribe_dialog(void)
{
  if (pBribe_Dlg) {
    is_unit_move_blocked = FALSE;
    popdown_window_group_dialog(pBribe_Dlg->pdialog->pBeginWidgetList,
				pBribe_Dlg->pdialog->pEndWidgetList);
    FC_FREE(pBribe_Dlg->pdialog);
    FC_FREE(pBribe_Dlg);
    flush_dirty();
  }
}

/**************************************************************************
  Popup a dialog asking a diplomatic unit if it wishes to bribe the
  given enemy unit.
**************************************************************************/
void popup_bribe_dialog(struct unit *actor, struct unit *pUnit, int cost)
{
  struct widget *pWindow = NULL, *pBuf = NULL;
  SDL_String16 *pStr;
  char tBuf[255], cBuf[255];
  bool exit = FALSE;
  SDL_Rect area;
  
  if (pBribe_Dlg) {
    return;
  }
  
  if (!actor || !is_actor_unit(actor)) {
    choose_action_queue_next();
    return;
  }
  
  is_unit_move_blocked = TRUE;
  
  pBribe_Dlg = fc_calloc(1, sizeof(struct small_diplomat_dialog));
  pBribe_Dlg->actor_unit_id = actor->id;
  pBribe_Dlg->target_id = pUnit->id;
  pBribe_Dlg->pdialog = fc_calloc(1, sizeof(struct SMALL_DLG));

  fc_snprintf(tBuf, ARRAY_SIZE(tBuf), PL_("Treasury contains %d gold.",
                                          "Treasury contains %d gold.",
                                          client_player()->economic.gold),
              client_player()->economic.gold);

  /* window */
  pStr = create_str16_from_char(_("Bribe Enemy Unit"), adj_font(12));
    
  pStr->style |= TTF_STYLE_BOLD;
  
  pWindow = create_window_skeleton(NULL, pStr, 0);
    
  pWindow->action = bribe_dlg_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);

  add_to_gui_list(ID_BRIBE_DLG_WINDOW, pWindow);
  pBribe_Dlg->pdialog->pEndWidgetList = pWindow;
  
  area = pWindow->area;
  area.w = MAX(area.w, adj_size(8));
  area.h = MAX(area.h, adj_size(2));
  
  if (cost <= client_player()->economic.gold) {
    fc_snprintf(cBuf, sizeof(cBuf),
                /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
                PL_("Bribe unit for %d gold?\n%s",
                    "Bribe unit for %d gold?\n%s", cost), cost, tBuf);

    create_active_iconlabel(pBuf, pWindow->dst, pStr, cBuf, NULL);
  
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
    /*------------*/
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("Yes"), diplomat_bribe_yes_callback);
    pBuf->data.unit = pUnit;
    set_wstate(pBuf, FC_WS_NORMAL);
  
    add_to_gui_list(MAX_ID - actor->id, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    /* ------- */
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("No") , exit_bribe_dlg_callback);
    
    set_wstate(pBuf , FC_WS_NORMAL);
    pBuf->key = SDLK_ESCAPE;
    
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
  } else {
    /* exit button */
    pBuf = create_themeicon(pTheme->Small_CANCEL_Icon, pWindow->dst,
                            WF_WIDGET_HAS_INFO_LABEL
                            | WF_RESTORE_BACKGROUND);
    pBuf->info_label = create_str16_from_char(_("Close Dialog (Esc)"),
                                              adj_font(12));
    area.w += pBuf->size.w + adj_size(10);
    pBuf->action = exit_bribe_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->key = SDLK_ESCAPE;
  
    add_to_gui_list(ID_BRIBE_DLG_EXIT_BUTTON, pBuf);
    exit = TRUE;
    /* --------------- */

    fc_snprintf(cBuf, sizeof(cBuf),
                /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
                PL_("Bribing the unit costs %d gold.\n%s",
                    "Bribing the unit costs %d gold.\n%s", cost), cost, tBuf);

    create_active_iconlabel(pBuf, pWindow->dst, pStr, cBuf, NULL);
  
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
    
    /*------------*/
    create_active_iconlabel(pBuf, pWindow->dst, pStr,
	    _("Traitors Demand Too Much!"), NULL);
        
    add_to_gui_list(ID_LABEL, pBuf);
    
    area.w = MAX(area.w, pBuf->size.w);
    area.h += pBuf->size.h;
  }
  pBribe_Dlg->pdialog->pBeginWidgetList = pBuf;
  
  /* setup window size and start position */

  resize_window(pWindow, NULL, NULL,
                (pWindow->size.w - pWindow->area.w) + area.w,
                (pWindow->size.h - pWindow->area.h) + area.h);
  
  area = pWindow->area;
  
  auto_center_on_focus_unit();
  put_window_near_map_tile(pWindow, pWindow->size.w, pWindow->size.h,
                           unit_tile(actor));

  /* setup widget size and start position */
  pBuf = pWindow;
  
  if (exit)
  {/* exit button */
    pBuf = pBuf->prev;
    pBuf->size.x = area.x + area.w - pBuf->size.w - 1;
    pBuf->size.y = pWindow->size.y + adj_size(2);
  }
  
  pBuf = pBuf->prev;
  setup_vertical_widgets_position(1,
	area.x,
  	area.y + 1, area.w, 0,
	pBribe_Dlg->pdialog->pBeginWidgetList, pBuf);
  
  /* --------------------- */
  /* redraw */
  redraw_group(pBribe_Dlg->pdialog->pBeginWidgetList, pWindow, 0);

  widget_flush(pWindow);
}
