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

#include <SDL/SDL.h>

/* utility */
#include "fcintl.h"
#include "log.h"

/* common */
#include "game.h"

/* gui-sdl */
#include "citydlg.h"
#include "cma_fe.h"
#include "colors.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "gui_tilespec.h"
#include "mapctrl.h"
#include "mapview.h"
#include "repodlgs.h"
#include "themespec.h"
#include "wldlg.h"

#include "cityrep.h"

static struct ADVANCED_DLG *pCityRep = NULL;

static void real_info_city_report_dialog_update(void);

/* ==================================================================== */

static int city_report_windows_callback(struct widget *pWindow)
{
  return -1;
}

static int exit_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityRep) {
  /*    if (pUnits_Upg_Dlg) {
         del_group_of_widgets_from_gui_list(pUnits_Upg_Dlg->pBeginWidgetList,
                                pUnits_Upg_Dlg->pEndWidgetList);
         FC_FREE(pUnits_Upg_Dlg); 
      } */
      popdown_window_group_dialog(pCityRep->pBeginWidgetList,
                                        pCityRep->pEndWidgetList);
      FC_FREE(pCityRep->pScroll);
      FC_FREE(pCityRep);
      enable_and_redraw_find_city_button();
      flush_dirty();
    }
  }
  return -1;
}

static int popup_citydlg_from_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popup_city_dialog(pWidget->data.city);
  }
  return -1;
}

static int popup_worklist_from_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popup_worklist_editor(pWidget->data.city, &pWidget->data.city->worklist);
  }
  return -1;
}

static int popup_buy_production_from_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popup_hurry_production_dialog(pWidget->data.city, NULL);
  }
  return -1;
}

static int popup_cma_from_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct city *pCity = find_city_by_id(MAX_ID - pWidget->ID);
      
    /* state is changed before enter this function */  
    if(!get_checkbox_state(pWidget)) {
      cma_release_city(pCity);
      city_report_dialog_update_city(pCity);
    } else {
      popup_city_cma_dialog(pCity);
    }
  }  
  return -1;
}

#if 0
static int info_city_report_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    set_wstate(pWidget, FC_WS_NORMAL);
    pSellected_Widget = NULL;
    redraw_widget(pWidget);
    sdl_dirty_rect(pWidget->size);
    real_info_city_report_dialog_update();
  }
  return -1;
}
#endif

#define COL	17

/**************************************************************************
  rebuild the city info report.
**************************************************************************/
static void real_info_city_report_dialog_update(void)
{
  SDL_Color bg_color = {255, 255, 255, 136};
  
  struct widget *pBuf = NULL;
  struct widget *pWindow , *pLast;
  SDL_String16 *pStr;
  SDL_Surface *pText1, *pText2, *pText3, *pUnits_Icon, *pCMA_Icon, *pText4;
  SDL_Surface *pLogo;
  int togrow, w = 0 , count , h = 0, ww = 0, hh = 0, name_w = 0, prod_w = 0, H;
  char cBuf[128];
  const char *pName;
  SDL_Rect dst;

  if(pCityRep) {
    popdown_window_group_dialog(pCityRep->pBeginWidgetList,
			      			pCityRep->pEndWidgetList);
  } else {
    pCityRep = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  }
  
  my_snprintf(cBuf, sizeof(cBuf), _("size"));
  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->style |= SF_CENTER;
  pText1 = create_text_surf_from_str16(pStr);
    
  my_snprintf(cBuf, sizeof(cBuf), _("time\nto grow"));
  copy_chars_to_string16(pStr, cBuf);
  pText2 = create_text_surf_from_str16(pStr);
    
  my_snprintf(cBuf, sizeof(cBuf), _("City Name"));
  copy_chars_to_string16(pStr, cBuf);
  pText3 = create_text_surf_from_str16(pStr);
  name_w = pText3->w + adj_size(6);
      
  my_snprintf(cBuf, sizeof(cBuf), _("Production"));
  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYREP_TEXT);
  pText4 = create_text_surf_from_str16(pStr);
  FREESTRING16(pStr);
  
  pUnits_Icon = create_icon_from_theme(pTheme->UNITS_Icon, 0);
  pCMA_Icon = create_icon_from_theme(pTheme->CMA_Icon, 0);
  
  /* --------------- */
  pStr = create_str16_from_char(_("Cities Report"), adj_font(12));
  pStr->style |= TTF_STYLE_BOLD;

  pWindow = create_window(NULL, pStr, adj_size(40), adj_size(30), 0);
  pCityRep->pEndWidgetList = pWindow;
  w = MAX(w, pWindow->size.w);
  set_wstate(pWindow, FC_WS_NORMAL);
  pWindow->action = city_report_windows_callback;
  
  add_to_gui_list(ID_WINDOW, pWindow);
  
  /* ------------------------- */
  /* exit button */
  pBuf = create_themeicon(pTheme->CANCEL_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_DRAW_THEME_TRANSPARENT));

  pBuf->string16 = create_str16_from_char(_("Exit Report"), adj_font(12));
  pBuf->action = exit_city_report_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->key = SDLK_ESCAPE;
  
  add_to_gui_list(ID_BUTTON, pBuf);
  /* ------------------------- */
  pBuf = create_themeicon(pTheme->INFO_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_DRAW_THEME_TRANSPARENT));

  pBuf->string16 = create_str16_from_char(_("Information Report"), adj_font(12));
/*
  pBuf->action = info_city_report_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
*/
  add_to_gui_list(ID_BUTTON, pBuf);
  /* -------- */
  pBuf = create_themeicon(pTheme->Happy_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_DRAW_THEME_TRANSPARENT));

  pBuf->string16 = create_str16_from_char(_("Happiness Report"), adj_font(12));
/*
  pBuf->action = happy_city_report_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
*/
  add_to_gui_list(ID_BUTTON, pBuf);
  /* -------- */
  pBuf = create_themeicon(pTheme->Army_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_DRAW_THEME_TRANSPARENT));

  pBuf->string16 = create_str16_from_char(_("Garrison Report"), adj_font(12));
/*
  pBuf->action = army_city_dlg_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
*/
  add_to_gui_list(ID_BUTTON, pBuf);
  /* -------- */
  pBuf = create_themeicon(pTheme->Support_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_DRAW_THEME_TRANSPARENT));

  pBuf->string16 = create_str16_from_char(_("Maintenance Report"), adj_font(12));
/*
  pBuf->action = supported_unit_city_dlg_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
*/
  add_to_gui_list(ID_BUTTON, pBuf);
  /* ------------------------ */
    
  pLast = pBuf;
  count = 0; 
  city_list_iterate(game.player_ptr->cities, pCity) {
    
    pStr = create_str16_from_char(pCity->name, adj_font(12));
    pStr->style |= TTF_STYLE_BOLD;
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
			(WF_DRAW_THEME_TRANSPARENT|WF_SELLECT_WITHOUT_BAR));
    
    if (city_unhappy(pCity)) {
      pBuf->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_TRADE);
    } else {
      if (city_celebrating(pCity)) {
	pBuf->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_CELEB);
      } else {
        if (city_happy(pCity)) {
	  pBuf->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_HAPPY);
        }
      }
    }
    
    pBuf->action = popup_citydlg_from_city_report_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->data.city = pCity;
    if(count > 9 * COL) {
      set_wflag(pBuf , WF_HIDDEN);
    }
    hh = pBuf->size.h;
    name_w = MAX(pBuf->size.w, name_w);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);  

    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->size);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pText1->w + adj_size(8);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
  
    /* ----------- */
    pBuf = create_checkbox(pWindow->dst,
	    cma_is_city_under_agent(pCity, NULL), WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    assert(MAX_ID > pCity->id);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->action = popup_cma_from_city_report_callback;
        
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_FOOD] - pCity->surplus[O_FOOD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_OVERVIEW_LAND);	
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Food->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);

    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_FOOD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_FOOD_SURPLUS);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Food_Corr->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);

    /* ----------- */
    togrow = city_turns_to_grow(pCity);
    switch (togrow) {
      case 0:
        my_snprintf(cBuf, sizeof(cBuf), "#");
      break;
      case FC_INFINITY:
        my_snprintf(cBuf, sizeof(cBuf), "--");
      break;
      default:
        my_snprintf(cBuf, sizeof(cBuf), "%d", togrow);
      break;
    }
    
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    if(togrow < 0) {
      pStr->fgcol.r = 255;
    }
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pText2->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
   
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_TRADE]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_TRADE);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Trade->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->waste[O_TRADE]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Trade_Corr->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
            
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_GOLD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_GOLD);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Coin->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_SCIENCE]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_SCIENCE);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Colb->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_LUXURY]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_LUX);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Luxury->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
  
  /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d",
  			pCity->prod[O_SHIELD] + pCity->waste[O_SHIELD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PROD);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Shield->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
  
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->waste[O_SHIELD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Shield_Corr->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
  
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d",
	  pCity->prod[O_SHIELD] + pCity->waste[O_SHIELD] - pCity->surplus[O_SHIELD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_SUPPORT);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pUnits_Icon->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
  
    /* ----------- */
    my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_SHIELD]);
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_TRADE);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
					WF_DRAW_THEME_TRANSPARENT);
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    pBuf->size.w = pIcons->pBIG_Shield_Surplus->w + adj_size(6);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);

    /* ----------- */
    if(pCity->production.is_unit) {
      struct unit_type *pUnit = get_unit_type(pCity->production.value);
      pLogo = ResizeSurface(GET_SURF(get_unittype_sprite(tileset, get_unit_type(pCity->production.value))), adj_size(36), adj_size(24), 1);      
      togrow = unit_build_shield_cost(get_unit_type(pCity->production.value));
      pName = pUnit->name;
    } else {
      struct impr_type *pImprv = get_improvement_type(pCity->production.value);
      pLogo = ResizeSurface(GET_SURF(get_building_sprite(tileset, pCity->production.value)), adj_size(36), adj_size(24), 1);
      togrow = impr_build_shield_cost(pCity->production.value);
      pName = pImprv->name;
    }
    
    if(!worklist_is_empty(&(pCity->worklist))) {
      dst.x = pLogo->w - pIcons->pWorklist->w;
      dst.y = 0;
      alphablit(pIcons->pWorklist, NULL, pLogo, &dst);
      my_snprintf(cBuf, sizeof(cBuf), "%s\n(%d/%d)\n%s",
      	pName, pCity->shield_stock, togrow,
		pCity->worklist.name[0] != '\0' ?
			      pCity->worklist.name : _("worklist"));
    } else {
      my_snprintf(cBuf, sizeof(cBuf), "%s\n(%d/%d)%s",
      	pName, pCity->shield_stock, togrow,
	      pCity->shield_stock > togrow ? _("\nfinshed"): "" );
    }
    
    /* info string */
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    
    togrow = city_turns_to_build(pCity,
    	pCity->production, TRUE);
    if(togrow == 999)
    {
      my_snprintf(cBuf, sizeof(cBuf), "%s", _("never"));
    } else {
      my_snprintf(cBuf, sizeof(cBuf), "%d %s",
      			togrow, PL_("turn", "turns", togrow));
    }
  
    pBuf = create_icon2(pLogo, pWindow->dst,
    	(WF_WIDGET_HAS_INFO_LABEL|WF_DRAW_THEME_TRANSPARENT|WF_FREE_THEME));
    pBuf->string16 = pStr;
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->action = popup_worklist_from_city_report_callback;
    pBuf->data.city = pCity;
    
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYREP_TEXT);
    pBuf = create_iconlabel(NULL, pWindow->dst, pStr,
			(WF_SELLECT_WITHOUT_BAR|WF_DRAW_THEME_TRANSPARENT));
    if(count > 9 * COL) {
      set_wflag(pBuf, WF_HIDDEN);
    }
    hh = MAX(hh, pBuf->size.h);
    prod_w = MAX(prod_w, pBuf->size.w);
    add_to_gui_list(MAX_ID - pCity->id, pBuf);
    pBuf->data.city = pCity;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->action = popup_buy_production_from_city_report_callback;
    
    count += COL;
    h += (hh + adj_size(2));
  } city_list_iterate_end;
  
  H = hh;
  pCityRep->pBeginWidgetList = pBuf;
  /* setup window width */
  w = name_w + adj_size(6) + pText1->w + adj_size(8) + pCMA_Icon->w +
	(pIcons->pBIG_Food->w + adj_size(6)) * 10 + pText2->w + adj_size(6) +
	pUnits_Icon->w + adj_size(6) + prod_w + adj_size(170);
  
  if(count) {
    pCityRep->pBeginActiveWidgetList = pBuf;
    pCityRep->pEndActiveWidgetList = pLast->prev;
    if(count > 10 * COL) {
      pCityRep->pActiveWidgetList = pCityRep->pEndActiveWidgetList;
      if(pCityRep->pScroll) {
	pCityRep->pScroll->count = count;
      }
      ww = create_vertical_scrollbar(pCityRep, COL, 10, TRUE, TRUE);
      w += ww;
      h = (10 * (hh + adj_size(2))) + WINDOW_TITLE_HEIGHT + 1 + pTheme->FR_Bottom->h;
    } else {
      h += WINDOW_TITLE_HEIGHT + 1 + pTheme->FR_Bottom->h;
    }
  } else {
    h = WINDOW_TITLE_HEIGHT + 1 + pTheme->FR_Bottom->h;
  }
  
  h += pText2->h + adj_size(40);
  w += pTheme->FR_Left->w + pTheme->FR_Right->w + adj_size(2);
  pWindow->size.x = (Main.screen->w - w) / 2;
  pWindow->size.y = (Main.screen->h - h) / 2;
  set_window_pos(pWindow, pWindow->size.x, pWindow->size.y);
  
  pLogo = theme_get_background(theme, BACKGROUND_CITYREP);
  resize_window(pWindow, pLogo,	NULL, w, h);
  FREESURFACE(pLogo);

  pLogo = SDL_DisplayFormat(pWindow->theme);
  FREESURFACE(pWindow->theme);
  pWindow->theme = pLogo;
  pLogo = NULL;
  
  ww -= (pTheme->FR_Left->w + pTheme->FR_Right->w);
  
  /* exit button */
  pBuf = pWindow->prev;
  pBuf->size.x = pWindow->size.x + pWindow->size.w - pBuf->size.w - pTheme->FR_Right->w - adj_size(25);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - pTheme->FR_Bottom->h - adj_size(5);

  /* info button */
  pBuf = pBuf->prev;
  pBuf->size.x = pBuf->next->size.x - adj_size(5) - pBuf->size.w;
  pBuf->size.y = pBuf->next->size.y;

  /* happy button */
  pBuf = pBuf->prev;
  pBuf->size.x = pBuf->next->size.x - adj_size(5) - pBuf->size.w;
  pBuf->size.y = pBuf->next->size.y;
  
  /* army button */
  pBuf = pBuf->prev;
  pBuf->size.x = pBuf->next->size.x - adj_size(5) - pBuf->size.w;
  pBuf->size.y = pBuf->next->size.y;
  
  /* supported button */
  pBuf = pBuf->prev;
  pBuf->size.x = pBuf->next->size.x - adj_size(5) - pBuf->size.w;
  pBuf->size.y = pBuf->next->size.y;
  
  /* cities background and labels */
  dst.x = pTheme->FR_Left->w + adj_size(2);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  dst.w = (name_w + adj_size(6)) + (pText1->w + adj_size(8)) + adj_size(5);
  dst.h = h - WINDOW_TITLE_HEIGHT - adj_size(2) - pTheme->FR_Bottom->h - adj_size(32);
  SDL_FillRectAlpha(pWindow->theme, &dst, &bg_color);
  
  putframe(pWindow->theme, dst.x , dst.y, dst.x + dst.w, dst.y + dst.h - 1,
    map_rgba(pWindow->theme->format, *get_game_colorRGB(COLOR_THEME_CITYREP_FRAME)));
  
  dst.y += (pText2->h - pText3->h) / 2;
  dst.x += ((name_w + adj_size(6)) - pText3->w) / 2;
  alphablit(pText3, NULL, pWindow->theme, &dst);
  FREESURFACE(pText3);
  
  /* city size background and label */    
  dst.x = pTheme->FR_Left->w + adj_size(5) + name_w + adj_size(5 + 4);
  alphablit(pText1, NULL, pWindow->theme, &dst);
  ww = pText1->w;
  FREESURFACE(pText1);
  
  /* cma icon */
  dst.x += (ww + adj_size(9));
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (pText2->h - pCMA_Icon->h) / 2;
  alphablit(pCMA_Icon, NULL, pWindow->theme, &dst);
  ww = pCMA_Icon->w;
  FREESURFACE(pCMA_Icon);
    
  /* -------------- */
  /* populations food unkeep background and label */
  dst.x += (ww + 1);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  w = dst.x + adj_size(2);
  dst.w = (pIcons->pBIG_Food->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Food_Surplus->w + adj_size(6)) + adj_size(10) +
	  			pText2->w + adj_size(6 + 2);
  dst.h = h - WINDOW_TITLE_HEIGHT - adj_size(2) - pTheme->FR_Bottom->h - adj_size(32);
  SDL_FillRectAlpha(pWindow->theme, &dst, get_game_colorRGB(COLOR_THEME_CITYREP_FOODSTOCK));
  
  putframe(pWindow->theme, dst.x , dst.y, dst.x + dst.w, dst.y + dst.h - 1,
           map_rgba(pWindow->theme->format, *get_game_colorRGB(COLOR_THEME_CITYREP_FRAME)));
  
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (pText2->h - pIcons->pBIG_Food->h) / 2;
  dst.x += adj_size(5);
  alphablit(pIcons->pBIG_Food, NULL, pWindow->theme, &dst);
  
  /* food surplus Icon */
  w += (pIcons->pBIG_Food->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Food_Surplus, NULL, pWindow->theme, &dst);
  
  /* to grow label */
  w += (pIcons->pBIG_Food_Surplus->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  alphablit(pText2, NULL, pWindow->theme, &dst);
  hh = pText2->h;
  ww = pText2->w;
  FREESURFACE(pText2);
  /* -------------- */
  
  /* trade, corruptions, gold, science, luxury income background and label */
  dst.x = w + (ww + adj_size(8));
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  w = dst.x + adj_size(2);
  dst.w = (pIcons->pBIG_Trade->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Trade_Corr->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Coin->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Colb->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Luxury->w + adj_size(6)) + adj_size(4);
  dst.h = h - WINDOW_TITLE_HEIGHT - adj_size(2) - pTheme->FR_Bottom->h - adj_size(32);
  
  SDL_FillRectAlpha(pWindow->theme, &dst, get_game_colorRGB(COLOR_THEME_CITYREP_TRADE));
  
  putframe(pWindow->theme, dst.x , dst.y, dst.x + dst.w, dst.y + dst.h - 1,
           map_rgba(pWindow->theme->format, *get_game_colorRGB(COLOR_THEME_CITYREP_FRAME)));
  
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (hh - pIcons->pBIG_Trade->h) / 2;
  dst.x += adj_size(5);
  alphablit(pIcons->pBIG_Trade, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Trade->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Trade_Corr, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Food_Corr->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Coin, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Coin->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Colb, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Colb->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Luxury, NULL, pWindow->theme, &dst);
  /* --------------------- */
  
  /* total productions, waste, support, shields surplus background and label */
  w += (pIcons->pBIG_Luxury->w + adj_size(6)) + adj_size(4);
  dst.x = w;
  w += adj_size(2);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  dst.w = (pIcons->pBIG_Shield->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Shield_Corr->w + adj_size(6)) + adj_size(10) +
	  (pUnits_Icon->w + adj_size(6)) + adj_size(10) +
	  (pIcons->pBIG_Shield_Surplus->w + adj_size(6)) + adj_size(4);
  dst.h = h - WINDOW_TITLE_HEIGHT - adj_size(2) - pTheme->FR_Bottom->h - adj_size(32);
  
  SDL_FillRectAlpha(pWindow->theme, &dst, get_game_colorRGB(COLOR_THEME_CITYREP_PROD));
  
  putframe(pWindow->theme, dst.x , dst.y, dst.x + dst.w, dst.y + dst.h - 1,
    map_rgba(pWindow->theme->format, *get_game_colorRGB(COLOR_THEME_CITYREP_FRAME)));
  
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (hh - pIcons->pBIG_Shield->h) / 2;
  dst.x += adj_size(5);
  alphablit(pIcons->pBIG_Shield, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Shield->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  alphablit(pIcons->pBIG_Shield_Corr, NULL, pWindow->theme, &dst);
  
  w += (pIcons->pBIG_Shield_Corr->w + adj_size(6)) + adj_size(10);
  dst.x = w + adj_size(3);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (hh - pUnits_Icon->h) / 2;
  alphablit(pUnits_Icon, NULL, pWindow->theme, &dst);
  
  w += (pUnits_Icon->w + adj_size(6)) + adj_size(10);
  FREESURFACE(pUnits_Icon);
  dst.x = w + adj_size(3);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (hh - pIcons->pBIG_Shield_Surplus->h) / 2;
  alphablit(pIcons->pBIG_Shield_Surplus, NULL, pWindow->theme, &dst);
  /* ------------------------------- */ 
  
  w += (pIcons->pBIG_Shield_Surplus->w + adj_size(6)) + adj_size(10);
  dst.x = w;
  w += adj_size(2);
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2);
  dst.w = adj_size(36) + adj_size(5) + prod_w;
  dst.h = hh + adj_size(2);
    
  SDL_FillRectAlpha(pWindow->theme, &dst, get_game_colorRGB(COLOR_THEME_CITYREP_PROD));
  
  putframe(pWindow->theme, dst.x , dst.y, dst.x + dst.w, dst.y + dst.h - 1,
    map_rgba(pWindow->theme->format, *get_game_colorRGB(COLOR_THEME_CITYREP_FRAME)));
  
  dst.y = WINDOW_TITLE_HEIGHT + adj_size(2) + (hh - pText4->h) / 2;
  dst.x += (dst.w - pText4->w) / 2;;
  alphablit(pText4, NULL, pWindow->theme, &dst);
  FREESURFACE(pText4);
  
  if(count) {
    int start_x = pWindow->size.x + pTheme->FR_Left->w + adj_size(5);
    int start_y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(2) + hh + adj_size(2);
    H += adj_size(2);
    pBuf = pBuf->prev;
    while(TRUE) {
    
      /* city name */
      pBuf->size.x = start_x;
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      pBuf->size.w = name_w;
      
      /* cit size */      
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(5);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* cma */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(6);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* food cons. */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(6);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* food surplus */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* time to grow */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* trade */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(5);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* trade corruptions */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* net gold income */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* science income */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* luxuries income */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* total production */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(6);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;

      /* waste */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* units support */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* producrion surplus */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* currently build */
      /* icon */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(10);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      
      /* label */
      pBuf = pBuf->prev;
      pBuf->size.x = pBuf->next->size.x + pBuf->next->size.w + adj_size(5);
      pBuf->size.y = start_y + (H - pBuf->size.h) / 2;
      pBuf->size.w = prod_w;

      start_y += H;
      if(pBuf == pCityRep->pBeginActiveWidgetList) {
	break;
      }
      pBuf = pBuf->prev;
    }
    
    if(pCityRep->pScroll) {
      setup_vertical_scrollbar_area(pCityRep->pScroll,
	  pWindow->size.x + pWindow->size.w - pTheme->FR_Right->w,
    	  pWindow->size.y + WINDOW_TITLE_HEIGHT + 1,
    	  pWindow->size.h - (WINDOW_TITLE_HEIGHT + 1 + pTheme->FR_Bottom->h + 1), TRUE);      
    }
    
  }
  /* ----------------------------------- */
  redraw_group(pCityRep->pBeginWidgetList, pWindow, 0);
  sdl_dirty_rect(pWindow->size);
  flush_dirty();
}


static struct widget * real_city_report_dialog_update_city(struct widget *pWidget,
							  struct city *pCity)
{
  char cBuf[64];
  const char *pName;
  int togrow;
  SDL_Surface *pLogo;
  SDL_Rect dst;
    
  /* city name status */
  if (city_unhappy(pCity)) {
    pWidget->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_TRADE);
  } else {
    if (city_celebrating(pCity)) {
      pWidget->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_CELEB);
    } else {
      if (city_happy(pCity)) {
	pWidget->string16->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_HAPPY);
      }
    }
  }
   
  /* city size */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->size);
  copy_chars_to_string16(pWidget->string16, cBuf);
      
  /* cma check box */
  pWidget = pWidget->prev;
  if (cma_is_city_under_agent(pCity, NULL) != get_checkbox_state(pWidget)) {
    togle_checkbox(pWidget);
  }
  
  /* food consumptions */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_FOOD] - pCity->surplus[O_FOOD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
    
  /* food surplus */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_FOOD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* time to grow */
  pWidget = pWidget->prev;
  togrow = city_turns_to_grow(pCity);
  switch (togrow) {
    case 0:
      my_snprintf(cBuf, sizeof(cBuf), "#");
    break;
    case FC_INFINITY:
      my_snprintf(cBuf, sizeof(cBuf), "--");
    break;
    default:
      my_snprintf(cBuf, sizeof(cBuf), "%d", togrow);
    break;
  }
  copy_chars_to_string16(pWidget->string16, cBuf);
      
  if(togrow < 0) {
    pWidget->string16->fgcol.r = 255;
  } else {
    pWidget->string16->fgcol.r = 0;
  }
    
  /* trade production */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_TRADE]);
  copy_chars_to_string16(pWidget->string16, cBuf);
    
  /* corruptions */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->waste[O_TRADE]);
  copy_chars_to_string16(pWidget->string16, cBuf);
            
  /* gold surplus */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_GOLD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
    
  /* science income */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_SCIENCE]);
  copy_chars_to_string16(pWidget->string16, cBuf);
    
  /* lugury income */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_LUXURY]);
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* total production */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d",
  			pCity->prod[O_SHIELD] + pCity->waste[O_SHIELD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* waste */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->waste[O_SHIELD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* units support */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->prod[O_SHIELD] +
                            pCity->waste[O_SHIELD] - pCity->surplus[O_SHIELD]);          
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* production income */
  pWidget = pWidget->prev;
  my_snprintf(cBuf, sizeof(cBuf), "%d", pCity->surplus[O_SHIELD]);
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  /* change production */
  if(pCity->production.is_unit) {
    struct unit_type *pUnit = get_unit_type(pCity->production.value);
    pLogo = ResizeSurface(GET_SURF(get_unittype_sprite(tileset,
              get_unit_type(pCity->production.value))), adj_size(36), adj_size(24), 1);
    togrow = unit_build_shield_cost(get_unit_type(pCity->production.value));
    pName = pUnit->name;
  } else {
    struct impr_type *pImprv = get_improvement_type(pCity->production.value);
    pLogo = ResizeSurface(GET_SURF(get_building_sprite(tileset,
              pCity->production.value)), adj_size(36), adj_size(24), 1);
    togrow = impr_build_shield_cost(pCity->production.value);
    pName = pImprv->name;
  }
    
  if(!worklist_is_empty(&(pCity->worklist))) {
    dst.x = pLogo->w - pIcons->pWorklist->w;
    dst.y = 0;
    alphablit(pIcons->pWorklist, NULL, pLogo, &dst);
    my_snprintf(cBuf, sizeof(cBuf), "%s\n(%d/%d)\n%s",
      	pName, pCity->shield_stock, togrow,
		pCity->worklist.name[0] != '\0' ?
			      pCity->worklist.name : _("worklist"));
  } else {
    my_snprintf(cBuf, sizeof(cBuf), "%s\n(%d/%d)",
      		pName, pCity->shield_stock, togrow);
  }
    
  
  pWidget = pWidget->prev;
  copy_chars_to_string16(pWidget->string16, cBuf);
  FREESURFACE(pWidget->theme);
  pWidget->theme = pLogo;
  
  /* hurry productions */
  pWidget = pWidget->prev;
  togrow = city_turns_to_build(pCity,
    	pCity->production, TRUE);
  if(togrow == 999)
  {
    my_snprintf(cBuf, sizeof(cBuf), "%s", _("never"));
  } else {
    my_snprintf(cBuf, sizeof(cBuf), "%d %s",
      			togrow, PL_("turn", "turns", togrow));
  }
  
  copy_chars_to_string16(pWidget->string16, cBuf);
  
  return pWidget->prev;
}

/* ======================================================================== */

bool is_city_report_open(void)
{
  return (pCityRep != NULL);
}

/**************************************************************************
  Pop up or brings forward the city report dialog.  It may or may not
  be modal.
**************************************************************************/
void popup_city_report_dialog(bool make_modal)
{
  if(!pCityRep) {
    real_info_city_report_dialog_update();
  }
}

/**************************************************************************
  Update (refresh) the entire city report dialog.
**************************************************************************/
void city_report_dialog_update(void)
{
  if(pCityRep && !is_report_dialogs_frozen()) {
    struct widget *pWidget;
    int count;    
  
    /* find if the lists are identical (if not then rebuild all) */
    pWidget = pCityRep->pEndActiveWidgetList;/* name of first city */
    city_list_iterate(game.player_ptr->cities, pCity) {
      if (pCity->id == MAX_ID - pWidget->ID) {
        count = COL;
        while(count) {
	  count--;
	  pWidget = pWidget->prev;
        }
      } else {
        real_info_city_report_dialog_update();
        return;
      }
    } city_list_iterate_end;
    /* check it there are some city widgets left on list */
    if(pWidget && pWidget->next != pCityRep->pBeginActiveWidgetList) {
      real_info_city_report_dialog_update();
      return;
    }

    /* update widget city list (widget list is the same that city list) */
    pWidget = pCityRep->pEndActiveWidgetList;
    city_list_iterate(game.player_ptr->cities, pCity) {
      pWidget = real_city_report_dialog_update_city(pWidget, pCity);  
    } city_list_iterate_end;

    /* -------------------------------------- */
    redraw_group(pCityRep->pBeginWidgetList, pCityRep->pEndWidgetList, 0);
    sdl_dirty_rect(pCityRep->pEndWidgetList->size);
    
    flush_dirty();
  }
}

/**************************************************************************
  Update the city report dialog for a single city.
**************************************************************************/
void city_report_dialog_update_city(struct city *pCity)
{
  if(pCityRep && pCity && !is_report_dialogs_frozen()) {
    struct widget *pBuf = pCityRep->pEndActiveWidgetList;
    while(pCity->id != MAX_ID - pBuf->ID &&
	      pBuf != pCityRep->pBeginActiveWidgetList) {
	pBuf = pBuf->prev;	
    }
    if(pBuf == pCityRep->pBeginActiveWidgetList) {
      real_info_city_report_dialog_update();
      return;
    }
    real_city_report_dialog_update_city(pBuf, pCity);
    
    /* -------------------------------------- */
    redraw_group(pCityRep->pBeginWidgetList, pCityRep->pEndWidgetList, 0);
    sdl_dirty_rect(pCityRep->pEndWidgetList->size);
    
    flush_dirty();
  }
}

/****************************************************************
 After a selection rectangle is defined, make the cities that
 are hilited on the canvas exclusively hilited in the
 City List window.
*****************************************************************/
void hilite_cities_from_canvas(void)
{
  freelog(LOG_DEBUG, "hilite_cities_from_canvas : PORT ME");
}

/****************************************************************
 Toggle a city's hilited status.
*****************************************************************/
void toggle_city_hilite(struct city *pCity, bool on_off)
{
  freelog(LOG_DEBUG, "toggle_city_hilite : PORT ME");
}
