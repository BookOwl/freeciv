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

/**********************************************************************
                          mapview.c  -  description
                             -------------------
    begin                : Aug 10 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
 *********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* #define SDL_CVS */

#include <SDL/SDL.h>

/* utility */
#include "fcintl.h"
#include "log.h"

/* common */
#include "game.h"
#include "government.h"
#include "unitlist.h"

/* client */
#include "civclient.h"
#include "climisc.h"
#include "overview_common.h"
#include "text.h"

/* gui-sdl */
#include "dialogs.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_mouse.h"
#include "gui_stuff.h"
#include "gui_zoom.h"
#include "mapctrl.h"

#include "mapview.h"

extern SDL_Event *pFlush_User_Event;
extern SDL_Rect *pInfo_Area;

int OVERVIEW_START_X;
int OVERVIEW_START_Y;

static enum {
  NORMAL = 0,
  BORDERS = 1,
  TEAMS
} overview_mode = NORMAL;

static struct canvas *overview_canvas;
static struct canvas *city_map_canvas;
static struct canvas *terrain_canvas;

/* ================================================================ */

void update_map_canvas_scrollbars_size()
{
  /* No scrollbars */
}

static bool is_flush_queued = FALSE;

/**************************************************************************
  Flush the given part of the buffer(s) to the screen.
**************************************************************************/
void flush_mapcanvas(int canvas_x , int canvas_y ,
		     int pixel_width , int pixel_height)
{
  SDL_Rect rect = {canvas_x, canvas_y, pixel_width, pixel_height};
  alphablit(mapview.store->surf, &rect, Main.map, &rect);
}

void flush_rect(SDL_Rect rect, bool force_flush)
{
  if (is_flush_queued && !force_flush) {
    sdl_dirty_rect(rect);
  } else {
    static SDL_Rect dst;
    if (correct_rect_region(&rect)) {
      static int i = 0;
      dst = rect;
      if (get_client_state() == CLIENT_GAME_RUNNING_STATE) {     
        flush_mapcanvas(dst.x, dst.y, dst.w, dst.h);
      }
      alphablit(Main.map, &rect, Main.screen, &dst);
      dst = rect;
      alphablit(Main.gui, &rect, Main.screen, &dst);
      if (Main.guis) {
        while((i < Main.guis_count) && Main.guis[i]) {
          dst = rect;
          alphablit(Main.guis[i++], &rect, Main.screen, &dst);
        }
      }
      i = 0;

      /* flush main buffer to framebuffer */
      SDL_UpdateRect(Main.screen, rect.x, rect.y, rect.w, rect.h);
    }
  }
}

/**************************************************************************
  A callback invoked as a result of a FLUSH event, this function simply
  flushes the mapview canvas.
**************************************************************************/
void unqueue_flush(void)
{
  flush_dirty();
  redraw_selection_rectangle();
  is_flush_queued = FALSE;
}

/**************************************************************************
  Called when a region is marked dirty, this function queues a flush event
  to be handled later by SDL.  The flush may end up being done
  by freeciv before then, in which case it will be a wasted call.
**************************************************************************/
void queue_flush(void)
{
  if (!is_flush_queued) {
    if (SDL_PushEvent(pFlush_User_Event) == 0) {
      is_flush_queued = TRUE;
    } else {
      /* We don't want to set is_flush_queued in this case, since then
       * the flush code would simply stop working.  But this means the
       * below message may be repeated many times. */
      freelog(LOG_ERROR,
	      _("The SDL event buffer is full; you may see drawing errors\n"
		"as a result.  If you see this message often, please\n"
		"report it to freeciv-dev@freeciv.org."));
    }
  }
}

/**************************************************************************
  Save Flush area used by "end" flush.
**************************************************************************/
void dirty_rect(int canvas_x, int canvas_y,
		     int pixel_width, int pixel_height)
{
  SDL_Rect Rect = {canvas_x, canvas_y, pixel_width, pixel_height};
  if ((Main.rects_count < RECT_LIMIT) && correct_rect_region(&Rect)) {
    Main.rects[Main.rects_count++] = Rect;
    queue_flush();
  }
}

/**************************************************************************
  Save Flush rect used by "end" flush.
**************************************************************************/
void sdl_dirty_rect(SDL_Rect Rect)
{
  if ((Main.rects_count < RECT_LIMIT) && correct_rect_region(&Rect)) {
    Main.rects[Main.rects_count++] = Rect;
  }
}

/**************************************************************************
  Sellect entire screen area to "end" flush and block "save" new areas.
**************************************************************************/
void dirty_all(void)
{
  Main.rects_count = RECT_LIMIT;
  queue_flush();
}

/**************************************************************************
  flush entire screen.
**************************************************************************/
void flush_all(void)
{
  is_flush_queued = FALSE;
  Main.rects_count = RECT_LIMIT;
  flush_dirty();
}

/**************************************************************************
  Make "end" Flush "saved" parts/areas of the buffer(s) to the screen.
  Function is called in handle_procesing_finished and handle_thaw_hint
**************************************************************************/
void flush_dirty(void)
{
  static int j = 0;
  if(!Main.rects_count) {
    return;
  }

  if (get_client_state() == CLIENT_GAME_RUNNING_STATE) {
    refresh_overview();
  }    
  
  if(Main.rects_count >= RECT_LIMIT) {
    
    if (get_client_state() == CLIENT_GAME_RUNNING_STATE) {     
      flush_mapcanvas(0, 0, Main.screen->w, Main.screen->h);
    }
    alphablit(Main.map, NULL, Main.screen, NULL);
    alphablit(Main.gui, NULL, Main.screen, NULL);
    if (Main.guis) {
      while((j < Main.guis_count) && Main.guis[j]) {
        alphablit(Main.guis[j++], NULL, Main.screen, NULL);
      }
    }
    j = 0;

    draw_mouse_cursor();    
    
    /* flush main buffer to framebuffer */    
    SDL_UpdateRect(Main.screen, 0, 0, 0, 0);
  } else {
    static int i;
    static SDL_Rect dst;
    
    for(i = 0; i<Main.rects_count; i++) {
      
      dst = Main.rects[i];
      if (get_client_state() == CLIENT_GAME_RUNNING_STATE) {     
        flush_mapcanvas(dst.x, dst.y, dst.w, dst.h);
      }
      alphablit(Main.map, &Main.rects[i], Main.screen, &dst);
      dst = Main.rects[i];
      alphablit(Main.gui, &Main.rects[i], Main.screen, &dst);
      if (Main.guis) {
        while((j < Main.guis_count) && Main.guis[j]) {
          dst = Main.rects[i];
          alphablit(Main.guis[j++], &Main.rects[i], Main.screen, &dst);
        }
      }
      j = 0;
      
      /* restore widget info label if it overlaps with this area */
      if (pInfo_Area && !(((dst.x + dst.w) < pInfo_Area->x)
                          || (dst.x > (pInfo_Area->x + pInfo_Area->w)) 
                          || ((dst.y + dst.h) < pInfo_Area->y) 
                          || (dst.y > (pInfo_Area->y + pInfo_Area->h)))) {
                            
        redraw_widget_info_label(&dst);     
      }
      
    }

    draw_mouse_cursor();            
    
    /* flush main buffer to framebuffer */    
    SDL_UpdateRects(Main.screen, Main.rects_count, Main.rects);
  }
  Main.rects_count = 0;
}

/* ===================================================================== */

/**************************************************************************
  Set information for the indicator icons typically shown in the main
  client window.  The parameters tell which sprite to use for the
  indicator.
**************************************************************************/
void set_indicator_icons(struct sprite *bulb, struct sprite *sol,
			 struct sprite *flake, struct sprite *gov)
{
  struct GUI *pBuf = NULL;
  char cBuf[128];
  
  pBuf = get_widget_pointer_form_main_list(ID_WARMING_ICON);
  pBuf->theme = adj_surf(GET_SURF(sol));
  redraw_label(pBuf);
    
  pBuf = get_widget_pointer_form_main_list(ID_COOLING_ICON);
  pBuf->theme = adj_surf(GET_SURF(flake));
  redraw_label(pBuf);
    
  pBuf = get_revolution_widget();
  set_new_icon2_theme(pBuf, adj_surf(GET_SURF(gov)), FALSE);    
  
  if (game.player_ptr) {
    my_snprintf(cBuf, sizeof(cBuf), _("Revolution (Shift + R)\n%s"),
                              get_gov_pplayer(game.player_ptr)->name);
  } else {
    my_snprintf(cBuf, sizeof(cBuf), _("Revolution (Shift + R)\n%s"), "None");
  }        
  copy_chars_to_string16(pBuf->string16, cBuf);
      
  redraw_widget(pBuf);
  sdl_dirty_rect(pBuf->size);

  
  pBuf = get_tax_rates_widget();
  if(!pBuf->theme) {
    set_new_icon2_theme(pBuf,
                 adj_surf(GET_SURF(get_tax_sprite(tileset, O_GOLD))), FALSE);
  }
  
  pBuf = get_research_widget();
  
  if (!game.player_ptr) {
    my_snprintf(cBuf, sizeof(cBuf), _("Research (F6)\n%s (%d/%d)"),
                "None", 0, 0);
  } else if (get_player_research(game.player_ptr)->researching != A_UNSET) {  
    my_snprintf(cBuf, sizeof(cBuf), _("Research (F6)\n%s (%d/%d)"),
	      	get_tech_name(game.player_ptr,
                get_player_research(game.player_ptr)->researching),
	      	get_player_research(game.player_ptr)->bulbs_researched,
  		total_bulbs_required(game.player_ptr));
  } else {
    my_snprintf(cBuf, sizeof(cBuf), _("Research (F6)\n%s (%d/%d)"),
	      	get_tech_name(game.player_ptr,
			    get_player_research(game.player_ptr)->researching),
	      	get_player_research(game.player_ptr)->bulbs_researched,
  		0);
  }

  copy_chars_to_string16(pBuf->string16, cBuf);
  
  set_new_icon2_theme(pBuf, adj_surf(GET_SURF(bulb)), FALSE);  
  
  redraw_widget(pBuf);
  sdl_dirty_rect(pBuf->size);
  
}

/****************************************************************************
  Called when the map size changes. This may be used to change the
  size of the GUI element holding the overview canvas. The
  overview.width and overview.height are updated if this function is
  called.
****************************************************************************/
void overview_size_changed(void)
{
  map_canvas_resized(Main.screen->w, Main.screen->h);  	
  
  if (overview_canvas) {
    canvas_free(overview_canvas);
  }      
  
  overview_canvas = canvas_create(overview.width, overview.height);
  
  resize_minimap();
}

/**************************************************************************
  Typically an info box is provided to tell the player about the state
  of their civilization.  This function is called when the label is
  changed.
**************************************************************************/
void update_info_label(void)
{
  SDL_Surface *pTmp = NULL;
  Uint32 color = 0x0;/* black */
  SDL_Color col = {0, 0, 0, 80};
  char buffer[512];
  
  #ifdef SMALL_SCREEN
  SDL_Rect area = {0, 0, 0, 0};
  SDL_String16 *pText = create_string16(NULL, 0, 8);
  #else
  SDL_Rect area = {0, 3, 0, 0};
  SDL_String16 *pText = create_string16(NULL, 0, 10);
  #endif

  /* set text settings */
  pText->style |= TTF_STYLE_BOLD;
  pText->fgcol = (SDL_Color) {255, 255, 255, 255};
  pText->bgcol = (SDL_Color) {0, 0, 0, 0};

  if (game.player_ptr) {
    my_snprintf(buffer, sizeof(buffer),
                _("%s Population: %s  Year: %s  "
                  "Gold %d Tax: %d Lux: %d Sci: %d "),
                get_nation_name(game.player_ptr->nation),
                population_to_text(civ_population(game.player_ptr)),
                textyear(game.info.year),
                game.player_ptr->economic.gold,
                game.player_ptr->economic.tax,
                game.player_ptr->economic.luxury,
                game.player_ptr->economic.science);
    
    /* convert to unistr and create text surface */
    copy_chars_to_string16(pText, buffer);
    pTmp = create_text_surf_from_str16(pText);
  
    area.x = (Main.gui->w - pTmp->w) / 2 - adj_size(5);
    area.w = pTmp->w + adj_size(8);
      area.h = pTmp->h + adj_size(4);
  
    color = SDL_MapRGBA(Main.gui->format, col.r, col.g, col.b, col.unused);
    SDL_FillRect(Main.gui, &area , color);
    
    color = SDL_MapRGB(Main.gui->format, col.r, col.g, col.b);
    
    /* Horizontal lines */
    putline(Main.gui, area.x + 1, area.y,
                    area.x + area.w - 2, area.y , color);
    putline(Main.gui, area.x + 1, area.y + area.h - 1,
                    area.x + area.w - 2, area.y + area.h - 1, color);
  
    /* vertical lines */
    putline(Main.gui, area.x + area.w - 1, area.y + 1 ,
          area.x + area.w - 1, area.y + area.h - 2, color);
    putline(Main.gui, area.x, area.y + 1, area.x,
            area.y + area.h - 2, color);
  
    /* blit text to screen */  
    blit_entire_src(pTmp, Main.gui, area.x + adj_size(5), area.y + adj_size(2));
    
    sdl_dirty_rect(area);
    
    FREESURFACE(pTmp);
  }

  set_indicator_icons(client_research_sprite(),
		      client_warming_sprite(),
		      client_cooling_sprite(),
		      client_government_sprite());

  update_timeout_label();

  FREESTRING16(pText);
  
  queue_flush();
}

static int fucus_units_info_callback(struct GUI *pWidget)
{
  struct unit *pUnit = pWidget->data.unit;
  if (pUnit) {
    request_new_unit_activity(pUnit, ACTIVITY_IDLE);
    set_unit_focus(pUnit);
  }
  return -1;
}

/**************************************************************************
  Read Function Name :)
**************************************************************************/
void redraw_unit_info_label(struct unit *pUnit)
{
  struct GUI *pInfo_Window = get_unit_info_window_widget();
  SDL_Rect src, area = {pInfo_Window->size.x, pInfo_Window->size.y,
                        pInfo_Window->size.w, pInfo_Window->size.h};
  SDL_Surface *pBuf_Surf;
  SDL_String16 *pStr;
  struct canvas *destcanvas;
  int infra_count;
  
  if (SDL_Client_Flags & CF_UNIT_INFO_SHOW) {   
    /* Unit Window is Show */

    /* blit theme surface */
    clear_surface(pInfo_Window->dst, &area);
    alphablit(pInfo_Window->theme, NULL, pInfo_Window->dst, &area);

    if (pUnit) {
      SDL_Surface *pName, *pVet_Name = NULL, *pInfo, *pInfo_II = NULL;
      int sy, y, sx, width, height, n;
      bool right;
      char buffer[512];
      struct city *pCity = player_find_city_by_id(game.player_ptr,
						  pUnit->homecity);
      struct tile *pTile = pUnit->tile;
      bv_special infrastructure = get_tile_infrastructure_set(pTile, &infra_count);

      pStr = pInfo_Window->string16;
      
      change_ptsize16(pStr, adj_font(12));

      /* get and draw unit name (with veteran status) */
      copy_chars_to_string16(pStr, unit_type(pUnit)->name);
            
      pStr->style |= TTF_STYLE_BOLD;
      pStr->bgcol = (SDL_Color) {0, 0, 0, 0};
      pName = create_text_surf_from_str16(pStr);
      pStr->style &= ~TTF_STYLE_BOLD;
      
      if (pInfo_Window->size.w > 1.8 * DEFAULT_UNITS_W) {
	width = pInfo_Window->size.w / 2;
	right = TRUE;
      } else {
	width = pInfo_Window->size.w;
	right = FALSE;
      }
      
      if(pUnit->veteran) {
	copy_chars_to_string16(pStr, _("veteran"));
        change_ptsize16(pStr, adj_font(10));
	pStr->fgcol.b = 255;
        pVet_Name = create_text_surf_from_str16(pStr);
        pStr->fgcol.b = 0;
      }

      /* get and draw other info (MP, terran, city, etc.) */
      change_ptsize16(pStr, adj_font(10));

      my_snprintf(buffer, sizeof(buffer), "%s\n%s\n%s%s%s",
		  (unit_list_get(hover_units, 0) == pUnit) ? _("Select destination") :
		  unit_activity_text(pUnit),
		  tile_get_info_text(pTile),
		  (infra_count > 0) ?
		  get_infrastructure_text(infrastructure) : "",
		  (infra_count > 0) ? "\n" : "", pCity ? pCity->name : _("NONE"));

      copy_chars_to_string16(pStr, buffer);
      pInfo = create_text_surf_from_str16(pStr);
      
      if (pInfo_Window->size.h > DEFAULT_UNITS_H || right) {
	int h = TTF_FontHeight(pInfo_Window->string16->font);
				     
	my_snprintf(buffer, sizeof(buffer),"%s",
				sdl_get_tile_defense_info_text(pTile));
	
        if (pInfo_Window->size.h > 2 * h + DEFAULT_UNITS_H || right) {
	  if (game.info.borders > 0 && !pTile->city) {
	    const char *diplo_nation_plural_adjectives[DS_LAST] =
                        {Q_("?nation:Neutral"), Q_("?nation:Hostile"),
     			"" /* unused, DS_CEASEFIRE*/,
     			Q_("?nation:Peaceful"), Q_("?nation:Friendly"), 
     			Q_("?nation:Mysterious")};
            if (pTile->owner == game.player_ptr) {
              cat_snprintf(buffer, sizeof(buffer), _("\nOur Territory"));
            } else {
	      if (pTile->owner) {
                if (game.player_ptr->diplstates[pTile->owner->player_no].type==DS_CEASEFIRE){
		  int turns = game.player_ptr->diplstates[pTile->owner->player_no].turns_left;
		  cat_snprintf(buffer, sizeof(buffer),
		  	PL_("\n%s territory (%d turn ceasefire)",
				"\n%s territory (%d turn ceasefire)", turns),
		 		get_nation_name(pTile->owner->nation), turns);
                } else {
	          cat_snprintf(buffer, sizeof(buffer), _("\nTerritory of the %s %s"),
		    diplo_nation_plural_adjectives[
		  	game.player_ptr->diplstates[pTile->owner->player_no].type],
		    		get_nation_name_plural(pTile->owner->nation));
                }
              } else { /* !pTile->owner */
                cat_snprintf(buffer, sizeof(buffer), _("\nUnclaimed territory"));
              }
	    }
          } /* game.info.borders > 0 && !pTile->city */
	  
          if (pTile->city) {
            /* Look at city owner, not tile owner (the two should be the same, if
             * borders are in use). */
            struct player *pOwner = city_owner(pTile->city);
            bool citywall;
/*            bool barrack = FALSE, airport = FALSE, port = FALSE;*/
	    const char *diplo_city_adjectives[DS_LAST] =
    			{Q_("?city:Neutral"), Q_("?city:Hostile"),
     			  "" /*unused, DS_CEASEFIRE */, Q_("?city:Peaceful"),
			  Q_("?city:Friendly"), Q_("?city:Mysterious")};
			  
	    cat_snprintf(buffer, sizeof(buffer), _("\nCity of %s"), pTile->city->name);
            	  
	    citywall = city_got_citywalls(pTile->city);
                          
#if 0                          
	    if (pplayers_allied(game.player_ptr, pOwner)) {
	      barrack = (get_city_bonus(pTile->city, EFT_LAND_REGEN) > 0);
	      airport = (get_city_bonus(pTile->city, EFT_AIR_VETERAN) > 0);
	      port = (get_city_bonus(pTile->city, EFT_SEA_VETERAN) > 0);
	    }
	  
	    if (citywall || barrack || airport || port) {
	      cat_snprintf(buffer, sizeof(buffer), _(" with "));
	      if (barrack) {
                cat_snprintf(buffer, sizeof(buffer), _("Barracks"));
	        if (port || airport || citywall) {
	          cat_snprintf(buffer, sizeof(buffer), ", ");
	        }
	      }
	      if (port) {
	        cat_snprintf(buffer, sizeof(buffer), _("Port"));
	        if (airport || citywall) {
	          cat_snprintf(buffer, sizeof(buffer), ", ");
	        }
	      }
	      if (airport) {
	        cat_snprintf(buffer, sizeof(buffer), _("Airport"));
	        if (citywall) {
	          cat_snprintf(buffer, sizeof(buffer), ", ");
	        }
	      }
	      if (citywall) {
	        cat_snprintf(buffer, sizeof(buffer), _("City Walls"));
              }
	    }
#endif
	    
	    if (pOwner && pOwner != game.player_ptr) {
              /* TRANS: (<nation>,<diplomatic_state>)" */
              cat_snprintf(buffer, sizeof(buffer), _("\n(%s,%s)"),
		  get_nation_name(pOwner->nation),
		  diplo_city_adjectives[game.player_ptr->
				   diplstates[pOwner->player_no].type]);
	    }
	    
	  }
        }
		
	if (pInfo_Window->size.h > 4 * h + DEFAULT_UNITS_H || right) {
          cat_snprintf(buffer, sizeof(buffer), _("\nFood/Prod/Trade: %s"),
				get_tile_output_text(pUnit->tile));
	}
	
	copy_chars_to_string16(pStr, buffer);
      
	pInfo_II = create_text_surf_smaller_that_w(pStr, width - BLOCKU_W - adj_size(10));
	
      }
      /* ------------------------------------------- */
      
      n = unit_list_size(pTile->units);
      y = 0;
      
      if (n > 1 && ((!right && pInfo_II
	 && (pInfo_Window->size.h - DEFAULT_UNITS_H - pInfo_II->h > 52))
         || (right && pInfo_Window->size.h - DEFAULT_UNITS_H > 52))) {
	height = DEFAULT_UNITS_H;
      } else {
	height = pInfo_Window->size.h;
        if (pInfo_Window->size.h > DEFAULT_UNITS_H) {
	  y = (pInfo_Window->size.h - DEFAULT_UNITS_H -
	                 (!right && pInfo_II ? pInfo_II->h : 0)) / 2;
        }
      }
      
      sy = y + DOUBLE_FRAME_WH;
      area.y = pInfo_Window->size.y + sy;
      area.x = pInfo_Window->size.x + FRAME_WH + BLOCKU_W +
			    (width - pName->w - BLOCKU_W - DOUBLE_FRAME_WH) / 2;
            
      alphablit(pName, NULL, pInfo_Window->dst, &area);
      sy += pName->h;
      if(pVet_Name) {
	area.y += pName->h - adj_size(3);
        area.x = pInfo_Window->size.x + FRAME_WH + BLOCKU_W +
		(width - pVet_Name->w - BLOCKU_W - DOUBLE_FRAME_WH) / 2;
        alphablit(pVet_Name, NULL, pInfo_Window->dst, &area);
	sy += pVet_Name->h - adj_size(3);
        FREESURFACE(pVet_Name);
      }
      FREESURFACE(pName);
      
      /* draw unit sprite */
      pBuf_Surf = adj_surf(GET_SURF(get_unittype_sprite(tileset, pUnit->type)));
      src = get_smaller_surface_rect(pBuf_Surf);
      sx = FRAME_WH + BLOCKU_W + adj_size(3) +
			      (width / 2 - src.w - adj_size(3) - BLOCKU_W - FRAME_WH) / 2;
                  
      area.x = pInfo_Window->size.x + sx + src.w +
      		(width - (sx + src.w) - FRAME_WH - pInfo->w) / 2;
      
      area.y = pInfo_Window->size.y + sy +
	      (DEFAULT_UNITS_H - (sy - y) - FRAME_WH - pInfo->h) / 2;
            
      /* blit unit info text */
      alphablit(pInfo, NULL, pInfo_Window->dst, &area);
      FREESURFACE(pInfo);
      
      area.x = pInfo_Window->size.x + sx;
      area.y = pInfo_Window->size.y + y +
      		(DEFAULT_UNITS_H - DOUBLE_FRAME_WH - src.h) / 2;
      alphablit(pBuf_Surf, &src, pInfo_Window->dst, &area);
      
      
      if (pInfo_II) {
        if (right) {
	  area.x = pInfo_Window->size.x + width +
      				(width - pInfo_II->w) / 2;
	  area.y = pInfo_Window->size.y + FRAME_WH +
	  		(height - FRAME_WH - pInfo_II->h) / 2;
        } else {
	  area.y = pInfo_Window->size.y + DEFAULT_UNITS_H + y;
          area.x = pInfo_Window->size.x + BLOCKU_W +
      		(width - BLOCKU_W - pInfo_II->w) / 2;
        }
      
        /* blit unit info text */
        alphablit(pInfo_II, NULL, pInfo_Window->dst, &area);
              
        if (right) {
          sy = DEFAULT_UNITS_H;
        } else {
	  sy = area.y - pInfo_Window->size.y + pInfo_II->h;
        }
        FREESURFACE(pInfo_II);
      } else {
	sy = DEFAULT_UNITS_H;
      }
      
      if (n > 1 && (pInfo_Window->size.h - sy > 52)) {
	struct ADVANCED_DLG *pDlg = pInfo_Window->private_data.adv_dlg;
	struct GUI *pBuf = NULL, *pEnd = NULL, *pDock;
	struct city *pHome_City;
        struct unit_type *pUType;
	int num_w, num_h;
	  
	if (pDlg->pEndActiveWidgetList && pDlg->pBeginActiveWidgetList) {
	  del_group(pDlg->pBeginActiveWidgetList, pDlg->pEndActiveWidgetList);
	}
	num_w = (pInfo_Window->size.w - BLOCKU_W - DOUBLE_FRAME_WH) / 68;
	num_h = (pInfo_Window->size.h - sy - FRAME_WH) / 52;
	pDock = pInfo_Window;
	n = 0;
        unit_list_iterate(pTile->units, aunit) {
          if (aunit == pUnit) {
	    continue;
	  }
	    
	  pUType = aunit->type;
          pHome_City = find_city_by_id(aunit->homecity);
          my_snprintf(buffer, sizeof(buffer), "%s (%d,%d,%d)%s\n%s\n(%d/%d)\n%s",
		pUType->name, pUType->attack_strength,
		pUType->defense_strength, pUType->move_rate / SINGLE_MOVE,
                (aunit->veteran ? _("\nveteran") : ""),
                unit_activity_text(aunit),
		aunit->hp, pUType->hp,
		pHome_City ? pHome_City->name : _("None"));
      
	  pBuf_Surf = create_surf(tileset_full_tile_width(tileset),
	    				tileset_full_tile_height(tileset), SDL_SWSURFACE);

          destcanvas = canvas_create(tileset_full_tile_width(tileset), tileset_full_tile_height(tileset));
  
          put_unit(aunit, destcanvas, 0, 0);
          alphablit(adj_surf(destcanvas->surf), NULL, pBuf_Surf, NULL);

          canvas_free(destcanvas);

          if (pBuf_Surf->w > 64) {
            float zoom = 64.0 / pBuf_Surf->w;    
            SDL_Surface *pZoomed = ZoomSurface(pBuf_Surf, zoom, zoom, 1);
            FREESURFACE(pBuf_Surf);
            pBuf_Surf = pZoomed;
          }
	    
	  pStr = create_str16_from_char(buffer, 10);
          pStr->style |= SF_CENTER;
    
          pBuf = create_icon2(pBuf_Surf, pInfo_Window->dst,
	             (WF_FREE_THEME | WF_DRAW_THEME_TRANSPARENT |
						    WF_WIDGET_HAS_INFO_LABEL));
	
    	  pBuf->string16 = pStr;
          pBuf->data.unit = aunit;
	  pBuf->ID = ID_ICON;
	  DownAdd(pBuf, pDock);
          pDock = pBuf;

          if (!pEnd) {
            pEnd = pBuf;
          }
    
          if (++n > num_w * num_h) {
             set_wflag(pBuf, WF_HIDDEN);
          }
  
          if (unit_owner(aunit) == game.player_ptr) {    
            set_wstate(pBuf, FC_WS_NORMAL);
          }
    
          pBuf->action = fucus_units_info_callback;
    
	} unit_list_iterate_end;	  
	    
	pDlg->pBeginActiveWidgetList = pBuf;
	pDlg->pEndActiveWidgetList = pEnd;
	pDlg->pActiveWidgetList = pEnd;
	  	  	  
	if (n > num_w * num_h) {
    
	  if (!pDlg->pScroll) {
	      
            pDlg->pScroll = fc_calloc(1, sizeof(struct ScrollBar));
            pDlg->pScroll->active = num_h;
            pDlg->pScroll->step = num_w;
            pDlg->pScroll->count = n;
	      
            create_vertical_scrollbar(pDlg, num_w, num_h, FALSE, TRUE);
          
	  } else {
	    pDlg->pScroll->active = num_h;
            pDlg->pScroll->step = num_w;
            pDlg->pScroll->count = n;
	    show_scrollbar(pDlg->pScroll);
	  }
	    
	  /* create up button */
          pBuf = pDlg->pScroll->pUp_Left_Button;
          pBuf->size.x = pInfo_Window->size.x +
			      pInfo_Window->size.w - FRAME_WH - pBuf->size.w;
          pBuf->size.y = pInfo_Window->size.y + sy +
	    			(pInfo_Window->size.h - sy - num_h * 52) / 2;
          pBuf->size.h = (num_h * 52) / 2;
        
          /* create down button */
          pBuf = pDlg->pScroll->pDown_Right_Button;
          pBuf->size.x = pDlg->pScroll->pUp_Left_Button->size.x;
          pBuf->size.y = pDlg->pScroll->pUp_Left_Button->size.y +
	      			pDlg->pScroll->pUp_Left_Button->size.h;
          pBuf->size.h = pDlg->pScroll->pUp_Left_Button->size.h;
	    	    
        } else {
	  if (pDlg->pScroll) {
	    hide_scrollbar(pDlg->pScroll);
	  }
	  num_h = (n + num_w - 1) / num_w;
	}
	  
	setup_vertical_widgets_position(num_w,
			pInfo_Window->size.x + FRAME_WH + BLOCKU_W + adj_size(2),
			pInfo_Window->size.y + sy +
	  			(pInfo_Window->size.h - sy - num_h * 52) / 2,
	  		0, 0, pDlg->pBeginActiveWidgetList,
			  pDlg->pEndActiveWidgetList);
	  	  
      } else {
	if (pInfo_Window->private_data.adv_dlg->pEndActiveWidgetList) {
	  del_group(pInfo_Window->private_data.adv_dlg->pBeginActiveWidgetList,
	    		pInfo_Window->private_data.adv_dlg->pEndActiveWidgetList);
	}
	if (pInfo_Window->private_data.adv_dlg->pScroll) {
	  hide_scrollbar(pInfo_Window->private_data.adv_dlg->pScroll);
	}
      }
    
    } else { /* pUnit */

      if (pInfo_Window->private_data.adv_dlg->pEndActiveWidgetList) {
	del_group(pInfo_Window->private_data.adv_dlg->pBeginActiveWidgetList,
	    		pInfo_Window->private_data.adv_dlg->pEndActiveWidgetList);
      }
      if (pInfo_Window->private_data.adv_dlg->pScroll) {
	hide_scrollbar(pInfo_Window->private_data.adv_dlg->pScroll);
      }

      if (game.player_ptr) {
        change_ptsize16(pInfo_Window->string16, adj_font(14));
        copy_chars_to_string16(pInfo_Window->string16/*pStr*/,
                                          _("End of Turn\n(Press Enter)"));
        pInfo_Window->string16->bgcol = (SDL_Color) {0, 0, 0, 0};
        pBuf_Surf = create_text_surf_from_str16(pInfo_Window->string16);
        area.x = pInfo_Window->size.x + BLOCKU_W +
                          (pInfo_Window->size.w - BLOCKU_W - pBuf_Surf->w)/2;
        area.y = pInfo_Window->size.y + (pInfo_Window->size.h - pBuf_Surf->h)/2;
        alphablit(pBuf_Surf, NULL, pInfo_Window->dst, &area);
        FREESURFACE(pBuf_Surf);
      }
    }

    redraw_group(pInfo_Window->private_data.adv_dlg->pBeginWidgetList,
	    	pInfo_Window->private_data.adv_dlg->pEndWidgetList->prev, 0);
    
  } else {
#if 0    
    /* draw hidden */
    area.x = Main.screen->w - pBuf_Surf->w - FRAME_WH;
    area.y = Main.screen->h - pBuf_Surf->h - FRAME_WH;
    
    alphablit(pInfo_Window->theme, NULL, pInfo_Window->dst, &area);
#endif    
  }
  
  sdl_dirty_rect(get_unit_info_window_widget()->size);
}

static bool is_anim_enabled(void)
{
  return (SDL_Client_Flags & CF_FOCUS_ANIMATION) == CF_FOCUS_ANIMATION;
}

/**************************************************************************
  Set one of the unit icons in the information area based on punit.
  NULL will be pased to clear the icon. idx==-1 will be passed to
  indicate this is the active unit, or idx in [0..num_units_below-1] for
  secondary (inactive) units on the same tile.
**************************************************************************/
void set_unit_icon(int idx, struct unit *punit)
{
/* FIXME */
/*  update_unit_info_label(punit);*/
}

/**************************************************************************
  Most clients use an arrow (e.g., sprites.right_arrow) to indicate when
  the units_below will not fit. This function is called to activate and
  deactivate the arrow.
**************************************************************************/
void set_unit_icons_more_arrow(bool onoff)
{
/* Balast */
}

/**************************************************************************
  Update the information label which gives info on the current unit and
  the square under the current unit, for specified unit.  Note that in
  practice punit is always the focus unit.

  Clears label if punit is NULL.

  Typically also updates the cursor for the map_canvas (this is related
  because the info label may includes  "select destination" prompt etc).
  And it may call update_unit_pix_label() to update the icons for units
  on this square.
**************************************************************************/
void update_unit_info_label(struct unit_list *punitlist)
{
  struct unit *pUnit = unit_list_get(punitlist, 0);
    
  if (get_client_state() != CLIENT_GAME_RUNNING_STATE) {
    return;
  }
  
  /* draw unit info window */
  redraw_unit_info_label(pUnit);
  
  if (pUnit) {
    if(!is_anim_enabled()) {
      enable_focus_animation();
    }
    if (unit_list_get(hover_units, 0) != pUnit) {
      set_hover_state(NULL, HOVER_NONE, ACTIVITY_LAST, ORDER_LAST);
    }
  } else {
    disable_focus_animation();
  }
}

void update_timeout_label(void)
{
  freelog(LOG_DEBUG, "MAPVIEW: update_timeout_label : PORT ME");
}

void update_turn_done_button(bool do_restore)
{
  freelog(LOG_DEBUG, "MAPVIEW: update_turn_done_button : PORT ME");
}

/* ===================================================================== */
/* ========================== City Descriptions ======================== */
/* ===================================================================== */

/**************************************************************************
  Update (refresh) all of the city descriptions on the mapview.
**************************************************************************/
void update_city_descriptions(void)
{
  /* redraw buffer */
  show_city_descriptions(0, 0, mapview.store_width, mapview.store_height);
  dirty_all();  
}

/* ===================================================================== */
/* =============================== Mini Map ============================ */
/* ===================================================================== */

/**************************************************************************
...
**************************************************************************/
void center_minimap_on_minimap_window(void)
{
  int width, height;
  get_overview_area_dimensions(&width, &height);

  OVERVIEW_START_X = FRAME_WH + (width - overview.width)/2;
  OVERVIEW_START_Y = FRAME_WH + (height - overview.height)/2;
}

/**************************************************************************
...
**************************************************************************/
void toggle_overview_mode(void)
{
  /* FIXME: has no effect anymore */
  if (overview_mode == BORDERS) {
    overview_mode = NORMAL;
  } else {
    overview_mode = BORDERS;
  }
}

/****************************************************************************
  Return a canvas that is the overview window.
****************************************************************************/
struct canvas *get_overview_window(void)
{
  return overview_canvas;  
}

/****************************************************************************
  Return the dimensions of the area (container widget; maximum size) for
  the overview.
****************************************************************************/
void get_overview_area_dimensions(int *width, int *height)
{
  *width = MINI_MAP_W - BLOCKM_W - DOUBLE_FRAME_WH;
  *height = MINI_MAP_H - DOUBLE_FRAME_WH;  
}

/**************************************************************************
  Refresh (update) the viewrect on the overview. This is the rectangle
  showing the area covered by the mapview.
**************************************************************************/
void refresh_overview(void)
{
  struct GUI *pMMap, *pBuf;
  SDL_Rect map_area;

  if (get_client_state() != CLIENT_GAME_RUNNING_STATE) {
    return;
  }
  
  freelog(LOG_DEBUG, "MAPVIEW: refresh_overview_viewrect");
  
  pMMap = get_minimap_window_widget();
    
  map_area.x = 0;
  map_area.y = pMMap->dst->h - pMMap->theme->h;
    
  if (SDL_Client_Flags & CF_MINI_MAP_SHOW) {

    SDL_Rect dst = {OVERVIEW_START_X, OVERVIEW_START_Y, 0, 0};
    clear_surface(pMMap->dst, &pMMap->size);
    alphablit(overview_canvas->surf, NULL, pMMap->theme, &dst);
    alphablit(pMMap->theme, NULL, pMMap->dst, &map_area);
    
#if 0          
    map_area.x += OVERVIEW_START_X;
    map_area.y += OVERVIEW_START_Y;
    map_area.w = MIN(pMMap->size.w - BLOCKM_W - DOUBLE_FRAME_WH,
					    OVERVIEW_TILE_WIDTH * map.xsize);
    map_area.h = MIN(pMMap->size.h - DOUBLE_FRAME_WH,
					    OVERVIEW_TILE_HEIGHT * map.ysize);
    
    if(OVERVIEW_START_X != FRAME_WH || OVERVIEW_START_Y != FRAME_WH) {
      putframe(pMMap->dst , map_area.x - 1, map_area.y - 1,
			map_area.x + map_area.w + 1,
    			map_area.y + map_area.h + 1, 0xFFFFFFFF);
    }
#endif
    
    refresh_overview_canvas();
    /* ===== */


    /* redraw widgets */

    /* ID_NEW_TURN */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_NEW_TURN, SCAN_BACKWARD);
    if (!pBuf->gfx) {
      pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
    }

    real_redraw_icon(pBuf);

    /* ===== */
    /* ID_PLAYERS */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_PLAYERS, SCAN_BACKWARD);
    if (!pBuf->gfx) {
      pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
    }

    real_redraw_icon(pBuf);

    /* ===== */
    /* ID_CITIES */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_CITIES, SCAN_BACKWARD);    

    if (!pBuf->gfx) {
      pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
    }

    real_redraw_icon(pBuf);

    /* ===== */
    /* ID_UNITS */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_UNITS, SCAN_BACKWARD);        
    if((get_wflags(pBuf) & WF_HIDDEN) != WF_HIDDEN) {
      if (!pBuf->gfx) {
        pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
      }

      real_redraw_icon(pBuf);
    }
    
    /* ===== */
    /* ID_CHATLINE_TOGGLE_LOG_WINDOW_BUTTON */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_CHATLINE_TOGGLE_LOG_WINDOW_BUTTON,
                                                              SCAN_BACKWARD);        
    if((get_wflags(pBuf) & WF_HIDDEN) != WF_HIDDEN) {
      if (!pBuf->gfx) {
        pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
      }

      real_redraw_icon(pBuf);
    }
    
    /* ===== */
    
    /* Toggle minimap mode */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_TOGGLE_MINIMAP_MODE, SCAN_BACKWARD);        
    if((get_wflags(pBuf) & WF_HIDDEN) != WF_HIDDEN) {
      if (!pBuf->gfx) {
        pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
      }

      real_redraw_icon(pBuf);
    }

    #ifdef SMALL_SCREEN
    /* options */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_CLIENT_OPTIONS, SCAN_BACKWARD); 
    if((get_wflags(pBuf) & WF_HIDDEN) != WF_HIDDEN) {
      if (!pBuf->gfx) {
        pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
      }

      real_redraw_icon(pBuf);
    }
    #endif
    
    /* ===== */
    /* ID_TOGGLE_MAP_WINDOW_BUTTON */
    pBuf = get_widget_pointer_form_ID(pMMap, ID_TOGGLE_MAP_WINDOW_BUTTON,
                                                                 SCAN_BACKWARD);        
    if (!pBuf->gfx && pBuf->theme) {
      pBuf->gfx = crop_rect_from_surface(pBuf->dst, &pBuf->size);
    }

    real_redraw_icon(pBuf);

    sdl_dirty_rect(pMMap->size);
  }
  
}

/**************************************************************************
  Update (refresh) the locations of the mapview scrollbars (if it uses
  them).
**************************************************************************/
void update_map_canvas_scrollbars(void)
{
  /* No scrollbars. */
}

/**************************************************************************
  Draw a cross-hair overlay on a tile.
**************************************************************************/
void put_cross_overlay_tile(struct tile *ptile)
{
  freelog(LOG_DEBUG, "MAPVIEW: put_cross_overlay_tile : PORT ME");
}

/**************************************************************************
 Area Selection
**************************************************************************/
void draw_selection_rectangle(int canvas_x, int canvas_y, int w, int h)
{
  /* PORTME */
  putframe(Main.map, canvas_x, canvas_y, canvas_x + w, canvas_y + h, 
           SDL_MapRGB(Main.map->format, 255, 255, 255));
}

/**************************************************************************
  This function is called when the tileset is changed.
**************************************************************************/
void tileset_changed(void)
{
  /* PORTME */
  /* Here you should do any necessary redraws (for instance, the city
   * dialogs usually need to be resized). */
  
}

/* =====================================================================
				City MAP
   ===================================================================== */

SDL_Surface *create_city_map(struct city *pCity)
{
  /* city map dimensions might have changed, so create a new canvas each time */

  if (city_map_canvas) {
    canvas_free(city_map_canvas);
  }

  city_map_canvas = canvas_create(get_citydlg_canvas_width(), 
             get_citydlg_canvas_height() + tileset_tile_height(tileset) / 2);

  city_dialog_redraw_map(pCity, city_map_canvas);  

  return city_map_canvas->surf;
}

SDL_Surface *get_terrain_surface(struct tile *ptile)
{
  /* tileset dimensions might have changed, so create a new canvas each time */
  
  if (terrain_canvas) {
    canvas_free(terrain_canvas);
  }
    
  terrain_canvas = canvas_create(tileset_full_tile_width(tileset),
                                      tileset_full_tile_height(tileset));

  SDL_SetColorKey(terrain_canvas->surf, SDL_SRCCOLORKEY, 0);
  
  put_terrain(ptile, terrain_canvas, 0, 0);
  
  return terrain_canvas->surf;
}
