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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "capability.h"
#include "capstr.h"
#include "events.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "spaceship.h"
#include "unit.h"

#include "chatline_g.h"
#include "citydlg_g.h"
#include "cityrep.h"
#include "civclient.h"
#include "climisc.h"
#include "clinet_g.h"		/* aconnection */
#include "dialogs_g.h"
#include "graphics_g.h"
#include "gui_main_g.h"
#include "helpdlg.h"		/* boot_help_texts */
#include "mapctrl_g.h"
#include "mapview_g.h"
#include "menu_g.h"
#include "meswindlg.h"
#include "options.h"
#include "plrdlg.h"
#include "repodlgs_g.h"
#include "spaceshipdlg_g.h"

#include "packhand.h"

extern int seconds_to_turndone;
extern int last_turn_gold_amount;
extern int did_advance_tech_this_turn;
extern char name[512];
extern struct Sprite *intro_gfx_sprite;
extern struct Sprite *radar_gfx_sprite;
extern struct city *city_workers_display;

/**************************************************************************
...
**************************************************************************/
void handle_join_game_reply(struct packet_join_game_reply *packet)
{
  char msg[MSG_SIZE];
  char *s_capability = aconnection.capability;

  strcpy(s_capability, packet->capability);

  if (packet->you_can_join) {
    freelog(LOG_VERBOSE, "join game accept:%s", packet->message);
  } else {
    sprintf(msg, "You were rejected from the game: %s", packet->message);
    append_output_window(msg);
  }
  if (strcmp(s_capability, our_capability)==0)
    return;
  sprintf(msg, "Client capability string: %s", our_capability);
  append_output_window(msg);
  sprintf(msg, "Server capability string: %s", s_capability);
  append_output_window(msg);
}

/**************************************************************************
...
**************************************************************************/
void handle_remove_city(struct packet_generic_integer *packet)
{
  struct city *pcity=find_city_by_id(packet->value);
  
  if(pcity!=NULL) client_remove_city(pcity);
}

/**************************************************************************
...
**************************************************************************/
void handle_remove_unit(struct packet_generic_integer *packet)
{
  client_remove_unit(packet->value);
}


/**************************************************************************
...
**************************************************************************/
void handle_nuke_tile(struct packet_nuke_tile *packet)
{
  put_nuke_mushroom_pixmaps(packet->x, packet->y);
}

/**************************************************************************
this piece of code below is about as bad as it can get
**************************************************************************/
void handle_unit_combat(struct packet_unit_combat *packet)
{
  struct unit *punit0=game_find_unit_by_id(packet->attacker_unit_id);
  struct unit *punit1=game_find_unit_by_id(packet->defender_unit_id);

  if(punit0 && tile_visible_mapcanvas(punit0->x, punit0->y) &&
     punit1 && tile_visible_mapcanvas(punit1->x, punit1->y)) {
    struct unit *pfocus;
    pfocus=get_unit_in_focus();
  
    decrease_unit_hp_smooth(punit0, packet->attacker_hp,
			    punit1, packet->defender_hp);
    set_unit_focus(pfocus);
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_game_state(struct packet_generic_integer *packet)
{
  
  if(get_client_state()==CLIENT_SELECT_RACE_STATE && 
     packet->value==CLIENT_GAME_RUNNING_STATE &&
     game.player_ptr->race == R_LAST) {
    popdown_races_dialog();
  }
  
  set_client_state(packet->value);

  if(get_client_state()==CLIENT_GAME_RUNNING_STATE) {
    refresh_overview_canvas();
    refresh_overview_viewrect();
    enable_turn_done_button();
    player_set_unit_focus_status(game.player_ptr);

    update_info_label();	/* get initial population right */
    update_unit_focus();
    update_unit_info_label(get_unit_in_focus());

    if(get_unit_in_focus())
      center_tile_mapcanvas(get_unit_in_focus()->x, get_unit_in_focus()->y);
    
    free_intro_radar_sprites();
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_city_info(struct packet_city_info *packet)
{
  int i, x, y, city_is_new;
  struct city *pcity;

  pcity=city_list_find_id(&game.players[packet->owner].cities, packet->id);

  if(!pcity) {
    city_is_new=1;
    pcity=fc_malloc(sizeof(struct city));
  }
  else
    city_is_new=0;
  
  pcity->id=packet->id;
  pcity->owner=packet->owner;
  pcity->x=packet->x;
  pcity->y=packet->y;
  strcpy(pcity->name, packet->name);
  
  pcity->size=packet->size;
  pcity->ppl_happy[4]=packet->ppl_happy;
  pcity->ppl_content[4]=packet->ppl_content;
  pcity->ppl_unhappy[4]=packet->ppl_unhappy;
  pcity->ppl_elvis=packet->ppl_elvis;
  pcity->ppl_scientist=packet->ppl_scientist;
  pcity->ppl_taxman=packet->ppl_taxman;

  pcity->city_options=packet->city_options;
  
  for (i=0;i<4;i++)  {
    pcity->trade[i]=packet->trade[i];
    pcity->trade_value[i]=packet->trade_value[i];
  }
  
  pcity->food_prod=packet->food_prod;
  pcity->food_surplus=packet->food_surplus;
  pcity->shield_prod=packet->shield_prod;
  pcity->shield_surplus=packet->shield_surplus;
  pcity->trade_prod=packet->trade_prod;
  pcity->corruption=packet->corruption;
  
  pcity->luxury_total=packet->luxury_total;
  pcity->tax_total=packet->tax_total;
  pcity->science_total=packet->science_total;
  
  pcity->food_stock=packet->food_stock;
  pcity->shield_stock=packet->shield_stock;
  pcity->pollution=packet->pollution;
    
  pcity->is_building_unit=packet->is_building_unit;
  pcity->currently_building=packet->currently_building;
  pcity->did_buy=packet->did_buy;
  pcity->did_sell=packet->did_sell;
  pcity->was_happy=packet->was_happy;
  pcity->airlift=packet->airlift;
  
  i=0;
  for(y=0; y<CITY_MAP_SIZE; y++)
    for(x=0; x<CITY_MAP_SIZE; x++)
      set_worker_city(pcity,x,y,packet->city_map[i++]-'0');
    
  for(i=0; i<B_LAST; i++)
    pcity->improvements[i]=(packet->improvements[i]=='1') ? 1 : 0;
  
  if(city_is_new) {
    unit_list_init(&pcity->units_supported);
    city_list_insert(&game.players[packet->owner].cities, pcity);
    map_set_city(pcity->x, pcity->y, pcity);
    if(pcity->owner==game.player_idx)
      city_report_dialog_update();

    for(i=0; i<game.nplayers; i++) {
      unit_list_iterate(game.players[i].units, punit) 
	if(punit->homecity==pcity->id)
	  unit_list_insert(&pcity->units_supported, punit);
      unit_list_iterate_end;
    }
  }
    
  refresh_tile_mapcanvas(pcity->x, pcity->y, 1);

  if(city_workers_display==pcity)  {
    put_city_workers(pcity, -1);
    city_workers_display=NULL;
  }

  if(((city_is_new && get_client_state()==CLIENT_GAME_RUNNING_STATE && 
      pcity->owner==game.player_idx) || packet->diplomat_investigate) &&
      (!game.player_ptr->ai.control || ai_popup_windows)) 
    popup_city_dialog(pcity, 0);
  
  if(!city_is_new && pcity->owner==game.player_idx) {
    struct unit *punit = get_unit_in_focus();
    if (punit && (punit->x == pcity->x) && (punit->y == pcity->y)) {
      update_menus();
    }
    refresh_city_dialog(pcity);
  }

  if(city_is_new) {
    freelog(LOG_DEBUG, "New %s city %s id %d (%d %d)",
	 get_race_name(city_owner(pcity)->race),
	 pcity->name, pcity->id, pcity->x, pcity->y);
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_new_year(struct packet_new_year *ppacket)
{
  update_turn_done_button(1);
  enable_turn_done_button();
  
  game.year=ppacket->year;
  update_info_label();

  player_set_unit_focus_status(game.player_ptr);
  
  update_unit_focus();
    
  update_unit_info_label(get_unit_in_focus());

  seconds_to_turndone=game.timeout;

  turn_gold_difference=game.player_ptr->economic.gold-last_turn_gold_amount;
  last_turn_gold_amount=game.player_ptr->economic.gold;

  report_update_delay_off();
  update_report_dialogs();

  meswin_update_delay_off();
  update_meswin_dialog();

  if(game.player_ptr->ai.control && !ai_manual_turn_done) user_ended_turn();
}

void handle_before_new_year(void)
{
  clear_notify_window();
  report_update_delay_on();
  meswin_update_delay_on();
}

/**************************************************************************
...
**************************************************************************/
void handle_chat_msg(struct packet_generic_message *packet)
{
  int where = MW_OUTPUT;	/* where to display the message */
  
  if (packet->event >= E_LAST)  {
    freelog(LOG_NORMAL,"Unknown event type %d!", packet->event);
  } else if (packet->event >= 0)  {
    where = messages_where[packet->event];
  }
  if (where & MW_OUTPUT)
    append_output_window(packet->message);
  if (where & MW_MESSAGES)
    add_notify_window(packet);
  if (where & MW_POPUP)
    popup_notify_goto_dialog("Popup Request", packet->message, 
			     packet->x, packet->y);
}
 
/**************************************************************************
...
**************************************************************************/
void handle_page_msg(struct packet_generic_message *packet)
{
  int i;
  char title[512];
  
  for(i=0; packet->message[i]!='\n'; i++)
    title[i]=packet->message[i];
  title[i]='\0';
  
  if (!game.player_ptr->ai.control || ai_popup_windows || 
       packet->event != BROADCAST_EVENT)
    popup_notify_dialog(title, packet->message+i+1);
}

/**************************************************************************
...
**************************************************************************/
void handle_move_unit(struct packet_move_unit *packet)
{
  /* should there be something here? - mjd */
}

/**************************************************************************
...
**************************************************************************/
void handle_unit_info(struct packet_unit_info *packet)
{
  struct city *pcity;
  struct unit *punit;
  int repaint_unit;
  int repaint_city;		/* regards unit's homecity */
  
  repaint_unit=0;
  repaint_city=0;
  punit=unit_list_find(&game.players[packet->owner].units, packet->id);
  
  if(punit) {
    if(punit->activity!=packet->activity) { /* change activity */
      repaint_unit=1;
      if(wakeup_focus && (punit->owner==game.player_idx)
                      && (punit->activity==ACTIVITY_SENTRY)) {
        set_unit_focus(punit);
        /* RP: focus on (each) activated unit (e.g. when unloading a ship) */
      }

      punit->activity=packet->activity;

      if(punit->owner==game.player_idx) 
        refresh_unit_city_dialogs(punit);
      /*      refresh_tile_mapcanvas(punit->x, punit->y, 1);
      update_unit_pix_label(punit);
      update_unit_focus(); */

      /* These two lines force the menus to be updated as appropriate when
	 the unit activity changes. */
      if(punit == get_unit_in_focus())
         update_menus();
    }
    
    if(punit->homecity!=packet->homecity) { /* change homecity */
      struct city *pcity;
      if((pcity=find_city_by_id(punit->homecity))) {
	unit_list_unlink(&pcity->units_supported, punit);
	refresh_city_dialog(pcity);
      }
      
      punit->homecity=packet->homecity;
      if((pcity=find_city_by_id(punit->homecity))) {
	unit_list_insert(&pcity->units_supported, punit);
	repaint_city=1;
      }
    }

    if(punit->hp!=packet->hp) {                      /* hp changed */
      punit->hp=packet->hp;
      repaint_unit=1;
    }
    if (punit->type!=packet->type) {
      struct city *pcity = map_get_city(punit->x, punit->y);
      
      punit->type=packet->type;
      repaint_unit=1;
      repaint_city=1;
      if (pcity && (pcity->id != punit->homecity)) {
	refresh_city_dialog(pcity);
      }
    }
    if (punit->ai.control!=packet->ai) {
      punit->ai.control=packet->ai;
      repaint_unit = 1;
    }

    if(punit->x!=packet->x || punit->y!=packet->y) { /* change position */
      struct city *pcity;
      pcity=map_get_city(punit->x, punit->y);
      
      if(tile_is_known(packet->x, packet->y)) {
	do_move_unit(punit, packet);
	update_unit_focus();
      }
      else {
	unit_list_unlink(&game.players[packet->owner].units, punit);
	unit_list_unlink(&map_get_tile(punit->x, punit->y)->units, punit);
	if(punit->homecity && (pcity=find_city_by_id(punit->homecity))) {
	  unit_list_unlink(&pcity->units_supported, punit);
	}
	refresh_tile_mapcanvas(punit->x, punit->y, 1);
	free(punit);
        return;
      }
      if(pcity)  {
        if(pcity->id==punit->homecity)
	  repaint_city=1;
	else
	  refresh_city_dialog(pcity);
      }
      
      if((pcity=map_get_city(punit->x, punit->y)))  {
        if(pcity->id == punit->homecity)
	  repaint_city=1;
	else
	  refresh_city_dialog(pcity);
	
	if(unit_flag(punit->type, F_CARAVAN)
	   && (!game.player_ptr->ai.control || ai_popup_windows)
	   && punit->owner==game.player_idx
	   && (punit->activity!=ACTIVITY_GOTO ||
	       same_pos(punit->goto_dest_x, punit->goto_dest_y,
			pcity->x, pcity->y))
	   && (unit_can_help_build_wonder_here(punit)
	       || unit_can_est_traderoute_here(punit))) {
	  process_caravan_arrival(punit);
	}
      }
      
      repaint_unit=0;
    }
    if (punit->unhappiness!=packet->unhappiness) {
      punit->unhappiness=packet->unhappiness;
      repaint_city=1;
    }
    if (punit->upkeep!=packet->upkeep) {
      punit->upkeep=packet->upkeep;
      repaint_city=1;
    }
    if (repaint_city) {
      if((pcity=find_city_by_id(punit->homecity))) {
	refresh_city_dialog(pcity);
      }
    }

    punit->veteran=packet->veteran;
    punit->moves_left=packet->movesleft;
    punit->bribe_cost=0;
    punit->fuel=packet->fuel;
    punit->goto_dest_x=packet->goto_dest_x;
    punit->goto_dest_y=packet->goto_dest_y;
  }
  
  else {      /* create new unit */
    punit=fc_malloc(sizeof(struct unit));
    
    punit->id=packet->id;
    punit->owner=packet->owner;
    punit->x=packet->x;
    punit->y=packet->y;
    punit->veteran=packet->veteran;
    punit->homecity=packet->homecity;
    punit->type=packet->type;
    punit->moves_left=packet->movesleft;
    punit->hp=packet->hp;
    punit->unhappiness=packet->unhappiness;
    punit->activity=packet->activity;
    punit->upkeep=packet->upkeep;
    punit->fuel=packet->fuel;
    punit->goto_dest_x=packet->goto_dest_x;
    punit->goto_dest_y=packet->goto_dest_y;
    punit->ai.control=packet->ai;
    
    punit->activity_count=0;	/* never used in client/ or common/  --dwp */
    punit->bribe_cost=0;	/* done by handle_incite_cost() */
    
    punit->focus_status=FOCUS_AVAIL;
    
    unit_list_insert(&game.players[packet->owner].units, punit);
    unit_list_insert(&map_get_tile(punit->x, punit->y)->units, punit);
    
    if((pcity=find_city_by_id(punit->homecity)))
      unit_list_insert(&pcity->units_supported, punit);

    freelog(LOG_DEBUG, "New %s %s id %d (%d %d) hc %d %s", 
	   get_race_name(get_player(punit->owner)->race),
	   unit_name(punit->type), punit->x, punit->y, punit->id,
	   punit->homecity, (pcity ? pcity->name : "(unknown)"));
    
    /* this is ugly - prevent unit from being drawn if it's moved into
     * screen by a transporter - only works for ground_units.. yak */
    if(!is_ground_unit(punit) || map_get_terrain(punit->x, punit->y)!=T_OCEAN)
      repaint_unit=1;
    else
      repaint_unit=0;
  }

  if(punit && punit==get_unit_in_focus())
    update_unit_info_label(punit);
  else if(get_unit_in_focus() && get_unit_in_focus()->x==punit->x &&
	  get_unit_in_focus()->y==punit->y) {
    update_unit_info_label(get_unit_in_focus());
  }

  if(repaint_unit)
    refresh_tile_mapcanvas(punit->x, punit->y, 1);

  update_unit_focus(); 
}

/**************************************************************************
...
**************************************************************************/
void handle_map_info(struct packet_map_info *pinfo)
{
  map.xsize=pinfo->xsize;
  map.ysize=pinfo->ysize;
  map.is_earth=pinfo->is_earth;

  map_allocate();
  climap_init_continents();
  
  set_overview_dimensions(map.xsize, map.ysize);
}

/**************************************************************************
...
**************************************************************************/
void handle_game_info(struct packet_game_info *pinfo)
{
  int i, boot_help;
  game.gold=pinfo->gold;
  game.tech=pinfo->tech;
  game.techlevel=pinfo->techlevel;
  game.skill_level=pinfo->skill_level;
  game.timeout=pinfo->timeout;
  
  game.rail_food = pinfo->rail_food;
  game.rail_trade = pinfo->rail_trade;
  game.rail_prod = pinfo->rail_prod;

  game.end_year=pinfo->end_year;
  game.year=pinfo->year;
  game.min_players=pinfo->min_players;
  game.max_players=pinfo->max_players;
  game.nplayers=pinfo->nplayers;
  game.globalwarming=pinfo->globalwarming;
  game.heating=pinfo->heating;
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    game.player_idx=pinfo->player_idx;
    game.player_ptr=&game.players[game.player_idx];
  }
  for(i=0; i<A_LAST; i++)
    game.global_advances[i]=pinfo->global_advances[i];
  for(i=0; i<B_LAST; i++)
    game.global_wonders[i]=pinfo->global_wonders[i];
  
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    if(get_client_state()==CLIENT_SELECT_RACE_STATE)
      popdown_races_dialog();
  }
  game.techpenalty=pinfo->techpenalty;
  game.foodbox=pinfo->foodbox;
  game.civstyle=pinfo->civstyle;

  game.aqueduct_size = pinfo->aqueduct_size;
  game.sewer_size = pinfo->sewer_size;
  game.rtech.get_bonus_tech = pinfo->rtech.get_bonus_tech;
  game.rtech.boat_fast = pinfo->rtech.boat_fast;
  game.rtech.cathedral_plus = pinfo->rtech.cathedral_plus;
  game.rtech.cathedral_minus = pinfo->rtech.cathedral_minus;
  game.rtech.colosseum_plus = pinfo->rtech.colosseum_plus;

  boot_help = (get_client_state() == CLIENT_GAME_RUNNING_STATE
	       && game.spacerace != pinfo->spacerace);
  game.spacerace=pinfo->spacerace;
  if (boot_help) {
    boot_help_texts();		/* reboot, after setting game.spacerace */
  }
  
  update_unit_focus();
}

/**************************************************************************
...
**************************************************************************/
void handle_player_info(struct packet_player_info *pinfo)
{
  int i;
  int poptechup;
  char msg[MSG_SIZE];
  struct player *pplayer=&game.players[pinfo->playerno];
  
  strcpy(pplayer->name, pinfo->name);
  pplayer->race=pinfo->race;
  
  pplayer->economic.gold=pinfo->gold;
  pplayer->economic.tax=pinfo->tax;
  pplayer->economic.science=pinfo->science;
  pplayer->economic.luxury=pinfo->luxury;
  pplayer->government=pinfo->government;
  pplayer->embassy=pinfo->embassy;

  for(i=0; i<A_LAST; i++)
    pplayer->research.inventions[i]=pinfo->inventions[i]-'0';
  update_research(pplayer);

  poptechup = (pplayer->research.researching!=pinfo->researching);
  pplayer->research.researched=pinfo->researched;
  pplayer->research.researchpoints=pinfo->researchpoints;
  pplayer->research.researching=pinfo->researching;
  pplayer->future_tech=pinfo->future_tech;
  pplayer->ai.tech_goal=pinfo->tech_goal;

  if(!pplayer->conn){
    /* It is only the client that does this */

    pplayer->conn = fc_malloc(sizeof(struct connection));
    pplayer->conn->sock = 0;
    pplayer->conn->used = 0;
    pplayer->conn->player = NULL;
  }

  strcpy(pplayer->conn->capability, pinfo->capability);
  
  if(get_client_state()==CLIENT_GAME_RUNNING_STATE && pplayer==game.player_ptr) {
    strcpy(name, pplayer->name);
    if(poptechup) {
      if(!game.player_ptr->ai.control || ai_popup_windows)
	popup_science_dialog(1);
      did_advance_tech_this_turn=game.year;
      science_dialog_update();
    } 
  }

  pplayer->turn_done=pinfo->turn_done;
  pplayer->nturns_idle=pinfo->nturns_idle;
  pplayer->is_alive=pinfo->is_alive;
  
  pplayer->is_connected=pinfo->is_connected;
  strcpy(pplayer->addr, pinfo->addr);

  pplayer->revolution=pinfo->revolution;
  if(pplayer->ai.control!=pinfo->ai)  {
    pplayer->ai.control=pinfo->ai;
    if(pplayer==game.player_ptr)  {
      sprintf(msg,"AI Mode is now %s.",game.player_ptr->ai.control?"ON":"OFF");
      append_output_window(msg);
    }
  }
  
  if(pplayer==game.player_ptr && 
     (pplayer->revolution < 1 || pplayer->revolution > 5) && 
     pplayer->government==G_ANARCHY &&
     (!game.player_ptr->ai.control || ai_popup_windows))
    popup_government_dialog();
  
  update_players_dialog();

  if(pplayer==game.player_ptr) {
    if(get_client_state()==CLIENT_GAME_RUNNING_STATE &&
       !game.player_ptr->turn_done)
      enable_turn_done_button();
    update_info_label();
  }
}

/**************************************************************************
Ideally the client should let the player choose which type of
modules and components to build, and (possibly) where to extend
structurals.  The protocol now makes this possible, but the
client is not yet that good (would require GUI improvements)
so currently the client choices stuff automatically if there
is anything unplaced.

This function makes a choice (sends spaceship_action) and
returns 1 if we placed something, else 0.

Do things one at a time; the server will send us an updated
spaceship_info packet, and we'll be back here to do anything
which is left.
**************************************************************************/
static int spaceship_autoplace(struct player *pplayer,
			       struct player_spaceship *ship)
{
  struct packet_spaceship_action packet;
  int i;
  
  if (ship->modules > (ship->habitation + ship->life_support
		       + ship->solar_panels)) {
    
    int nice = (pplayer->government >= G_REPUBLIC);
    /* "nice" governments prefer to keep success 100%;
     * others build habitation first (for score?)  (Thanks Massimo.)
     */

    packet.action =
      (ship->habitation==0)   ? SSHIP_ACT_PLACE_HABITATION :
      (ship->life_support==0) ? SSHIP_ACT_PLACE_LIFE_SUPPORT :
      (ship->solar_panels==0) ? SSHIP_ACT_PLACE_SOLAR_PANELS :
      ((ship->habitation < ship->life_support)
       && (ship->solar_panels*2 >= ship->habitation + ship->life_support + 1))
                              ? SSHIP_ACT_PLACE_HABITATION :
      (ship->solar_panels*2 < ship->habitation + ship->life_support)
                              ? SSHIP_ACT_PLACE_SOLAR_PANELS :
      (ship->life_support<ship->habitation)
                              ? SSHIP_ACT_PLACE_LIFE_SUPPORT :
      (nice && (ship->life_support <= ship->habitation)
       && (ship->solar_panels*2 >= ship->habitation + ship->life_support + 1))
                              ? SSHIP_ACT_PLACE_LIFE_SUPPORT :
      (nice)                  ? SSHIP_ACT_PLACE_SOLAR_PANELS :
                                SSHIP_ACT_PLACE_HABITATION;

    if (packet.action == SSHIP_ACT_PLACE_HABITATION) {
      packet.num = ship->habitation + 1;
    } else if(packet.action == SSHIP_ACT_PLACE_LIFE_SUPPORT) {
      packet.num = ship->life_support + 1;
    } else {
      packet.num = ship->solar_panels + 1;
    }
    assert(packet.num<=NUM_SS_MODULES/3);

    send_packet_spaceship_action(&aconnection, &packet);
    return 1;
  }
  if (ship->components > ship->fuel + ship->propulsion) {
    if (ship->fuel <= ship->propulsion) {
      packet.action = SSHIP_ACT_PLACE_FUEL;
      packet.num = ship->fuel + 1;
    } else {
      packet.action = SSHIP_ACT_PLACE_PROPULSION;
      packet.num = ship->propulsion + 1;
    }
    send_packet_spaceship_action(&aconnection, &packet);
    return 1;
  }
  if (ship->structurals > num_spaceship_structurals_placed(ship)) {
    /* Want to choose which structurals are most important.
       Else we first want to connect one of each type of module,
       then all placed components, pairwise, then any remaining
       modules, or else finally in numerical order.
    */
    int req = -1;
    
    if (!ship->structure[0]) {
      /* if we don't have the first structural, place that! */
      packet.action = SSHIP_ACT_PLACE_STRUCTURAL;
      packet.num = 0;
      send_packet_spaceship_action(&aconnection, &packet);
      return 1;
    }
    
    if (ship->habitation >= 1
	&& !ship->structure[modules_info[0].required]) {
      req = modules_info[0].required;
    } else if (ship->life_support >= 1
	       && !ship->structure[modules_info[1].required]) {
      req = modules_info[1].required;
    } else if (ship->solar_panels >= 1
	       && !ship->structure[modules_info[2].required]) {
      req = modules_info[2].required;
    } else {
      int i;
      for(i=0; i<NUM_SS_COMPONENTS; i++) {
	if ((i%2==0 && ship->fuel > (i/2))
	    || (i%2==1 && ship->propulsion > (i/2))) {
	  if (!ship->structure[components_info[i].required]) {
	    req = components_info[i].required;
	    break;
	  }
	}
      }
    }
    if (req == -1) {
      for(i=0; i<NUM_SS_MODULES; i++) {
	if ((i%3==0 && ship->habitation > (i/3))
	    || (i%3==1 && ship->life_support > (i/3))
	    || (i%3==2 && ship->solar_panels > (i/3))) {
	  if (!ship->structure[modules_info[i].required]) {
	    req = modules_info[i].required;
	    break;
	  }
	}
      }
    }
    if (req == -1) {
      for(i=0; i<NUM_SS_STRUCTURALS; i++) {
	if (!ship->structure[i]) {
	  req = i;
	  break;
	}
      }
    }
    /* sanity: */
    assert(req!=-1);
    assert(!ship->structure[req]);
    
    /* Now we want to find a structural we can build which leads to req.
       This loop should bottom out, because everything leads back to s0,
       and we made sure above that we do s0 first.
     */
    while(!ship->structure[structurals_info[req].required]) {
      req = structurals_info[req].required;
    }
    packet.action = SSHIP_ACT_PLACE_STRUCTURAL;
    packet.num = req;
    send_packet_spaceship_action(&aconnection, &packet);
    return 1;
  }
  return 0;
}

/**************************************************************************
...
**************************************************************************/
void handle_spaceship_info(struct packet_spaceship_info *p)
{
  int i;
  struct player *pplayer=&game.players[p->player_num];
  struct player_spaceship *ship = &pplayer->spaceship;
  
  ship->state        = p->sship_state;
  ship->structurals  = p->structurals;
  ship->components   = p->components;
  ship->modules      = p->modules;
  ship->fuel         = p->fuel;
  ship->fuel         = p->fuel;
  ship->propulsion   = p->propulsion;
  ship->habitation   = p->habitation;
  ship->life_support = p->life_support;
  ship->solar_panels = p->solar_panels;
  ship->launch_year  = p->launch_year;
  ship->population   = p->population;
  ship->mass         = p->mass;
  ship->support_rate = p->support_rate;
  ship->energy_rate  = p->energy_rate;
  ship->success_rate = p->success_rate;
  ship->travel_time  = p->travel_time;
  
  for(i=0; i<NUM_SS_STRUCTURALS; i++) {
    ship->structure[i] = p->structure[i]-'0';
  }

  if (pplayer != game.player_ptr) {
    refresh_spaceship_dialog(pplayer);
    return;
  }
  update_menus();

  if (!spaceship_autoplace(pplayer, ship)) {
    refresh_spaceship_dialog(pplayer);
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_tile_info(struct packet_tile_info *packet)
{
  int old_known;

  struct tile *ptile=map_get_tile(packet->x, packet->y);

  old_known=ptile->known;

  ptile->terrain=packet->type;
  ptile->special=packet->special;

  ptile->known=packet->known;

  if(packet->known >= TILE_KNOWN && old_known < TILE_KNOWN
     && ptile->terrain != T_OCEAN)
    climap_update_continents(packet->x, packet->y);
  
  if(get_client_state()==CLIENT_GAME_RUNNING_STATE && packet->known>=TILE_KNOWN) {
    refresh_tile_mapcanvas(packet->x, packet->y, 1); 
    if(old_known<TILE_KNOWN) { 
      int known;
      
      known=tile_is_known(packet->x-1, packet->y);
      if(known>=TILE_KNOWN && known!=ptile->known)
	refresh_tile_mapcanvas(packet->x-1, packet->y, 1); 

      known=tile_is_known(packet->x+1, packet->y);
      if(known>=TILE_KNOWN && known!=ptile->known)
	refresh_tile_mapcanvas(packet->x+1, packet->y, 1); 

      known=tile_is_known(packet->x, packet->y-1);
      if(known>=TILE_KNOWN && known!=ptile->known)
	refresh_tile_mapcanvas(packet->x, packet->y-1, 1); 

      known=tile_is_known(packet->x, packet->y+1);
      if(known>=TILE_KNOWN && known!=ptile->known)
	refresh_tile_mapcanvas(packet->x, packet->y+1, 1); 
    }
    else { /* happens so seldom(new roads etc) so refresh'em all */
      refresh_tile_mapcanvas(packet->x-1, packet->y, 1); 
      refresh_tile_mapcanvas(packet->x+1, packet->y, 1); 
      refresh_tile_mapcanvas(packet->x, packet->y-1, 1); 
      refresh_tile_mapcanvas(packet->x, packet->y+1, 1); 
      /* jjm@codewell.com 30dec1998a */
      refresh_tile_mapcanvas(packet->x-1, packet->y+1, 1); 
      refresh_tile_mapcanvas(packet->x+1, packet->y+1, 1); 
      refresh_tile_mapcanvas(packet->x+1, packet->y-1, 1); 
      refresh_tile_mapcanvas(packet->x-1, packet->y-1, 1); 
    }
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_remove_player(struct packet_generic_integer *packet)
{
  client_remove_player(packet->value);
}

/**************************************************************************
...
**************************************************************************/
void handle_select_race(struct packet_generic_integer *packet)
{
  if(get_client_state()==CLIENT_SELECT_RACE_STATE) {
    if(packet->value==-1) {
      set_client_state(CLIENT_WAITING_FOR_GAME_START_STATE);
      popdown_races_dialog();
    }
    else
      races_toggles_set_sensitive(packet->value);
  }
  else if(get_client_state()==CLIENT_PRE_GAME_STATE) {
    set_client_state(CLIENT_SELECT_RACE_STATE);
    popup_races_dialog();
    races_toggles_set_sensitive(packet->value);
  }
  else
    freelog(LOG_VERBOSE, "got a select race packet in an incompatible state");
}

/**************************************************************************
...
**************************************************************************/
void handle_ruleset_unit(struct packet_ruleset_unit *p)
{
  struct unit_type *u = &unit_types[p->id];

  strcpy(u->name, p->name);
  u->graphics           = p->graphics;
  u->move_type          = p->move_type;
  u->build_cost         = p->build_cost;
  u->attack_strength    = p->attack_strength;
  u->defense_strength   = p->defense_strength;
  u->move_rate          = p->move_rate;
  u->tech_requirement   = p->tech_requirement;
  u->vision_range       = p->vision_range;
  u->transport_capacity = p->transport_capacity;
  u->hp                 = p->hp;
  u->firepower          = p->firepower;
  u->obsoleted_by       = p->obsoleted_by;
  u->fuel               = p->fuel;
  u->flags              = p->flags;
  u->roles              = p->roles;
}

/**************************************************************************
...
**************************************************************************/
void handle_ruleset_tech(struct packet_ruleset_tech *p)
{
  struct advance *a = &advances[p->id];

  strcpy(a->name, p->name);
  a->req[0] = p->req[0];
  a->req[1] = p->req[1];
}

/**************************************************************************
...
**************************************************************************/
void handle_ruleset_building(struct packet_ruleset_building *p)
{
  struct improvement_type *b = &improvement_types[p->id];

  strcpy(b->name, p->name);
  b->is_wonder        = p->is_wonder;
  b->tech_requirement = p->tech_requirement;
  b->build_cost       = p->build_cost;      
  b->shield_upkeep    = p->shield_upkeep;   
  b->obsolete_by      = p->obsolete_by;     
  b->variant          = p->variant;         
 
}


/**************************************************************************
...
**************************************************************************/
void handle_incite_cost(struct packet_generic_values *packet)
{
  struct city *pcity=find_city_by_id(packet->id);
  struct unit *punit=find_unit_by_id(packet->id);

  if(pcity)  {
    pcity->incite_revolt_cost = packet->value1;
    popup_incite_dialog(pcity);
    return;
  }

  if(punit) {
    punit->bribe_cost = packet->value1;
    popup_bribe_dialog(punit);
  }
}

/**************************************************************************
...
**************************************************************************/
void handle_city_options(struct packet_generic_values *preq)
{
  struct city *pcity = find_city_by_id(preq->value1);
  
  if (!pcity || pcity->owner != game.player_idx) return;
  pcity->city_options = preq->value2;
}
