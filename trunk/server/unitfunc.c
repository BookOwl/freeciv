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
#include <string.h>

#include <player.h>
#include <unithand.h>
#include <unittools.h>
#include <unitfunc.h>
#include <packets.h>
#include <civserver.h>
#include <map.h>
#include <maphand.h>
#include <cityhand.h>
#include <citytools.h>
#include <cityturn.h>
#include <unit.h>
#include <plrhand.h>
#include <city.h>
#include <log.h>
#include <mapgen.h>
#include <events.h>
#include <shared.h>
#include <aiunit.h>
void do_unit_goto(struct player *pplayer, struct unit *punit);

/******************************************************************************
  bribe an enemy unit
  rules:
  1) can't bribe a unit if owner runs a democracy
  2) if player don't have enough money
  3) can't bribe a unit unless it's the only unit on the square. 
     (this is handled outside this function)
  if these conditions are fulfilled, the unit will be bribed
****************************************************************************/
void diplomat_bribe(struct player *pplayer, struct unit *pdiplomat, struct unit *pvictim)
{
  if(pplayer->economic.gold>=pvictim->bribe_cost) {
    if(game.players[pvictim->owner].government==G_DEMOCRACY)
      notify_player_ex(pplayer, pdiplomat->x, pdiplomat->y, E_NOEVENT, 
	"Game: You can't bribe a unit from a democratic nation.");
    else {
      pplayer->economic.gold-=pvictim->bribe_cost;
      notify_player_ex(&game.players[pvictim->owner], 
		    pvictim->x, pvictim->y, E_DIPLOMATED, 
		    "Game: One of your units was bribed!");
      notify_player_ex(pplayer, pvictim->x, pvictim->y, E_NOEVENT, 
		    "Game: Succeeded in bribing the enemy unit.");
      
      create_unit(pplayer, pvictim->x, pvictim->y,
		  pvictim->type, pvictim->veteran, pdiplomat->homecity);
      wipe_unit(0, pvictim);
      pdiplomat->moves_left=0;
      send_unit_info(pplayer, pdiplomat, 0);
      send_player_info(pplayer, pplayer);
    }
  }
}

/****************************************************************************
  diplomat try to steal a tech from an enemy city
  rules:
  1) if there is a spy in the city the attempt will fail.
  2) if there has already been stolen from this city the attempt will fail
****************************************************************************/
void diplomat_get_tech(struct player *pplayer, struct unit *pdiplomat, 
		       struct city  *city)
{
  int tec;
  int i;
  int j=0;
  struct player *target=&game.players[city->owner];
  if (pplayer==target)
    return;

  if (diplomat_on_tile(city->x, city->y)) {
    notify_player_ex(pplayer, city->x, city->y, E_NOEVENT, 
		     "Game: Your spy has been eliminated by a defending spy in %s.", city->name);
    notify_player_ex(target, city->x, city->y, E_DIPLOMATED,
		     "Game: A%s spy has been eliminated in %s..", n_if_vowel(get_race_name(target->race)[0]), city->name);
    wipe_unit(0, pdiplomat);
    return;
  }
  
  for (i=1;i<A_LAST;i++) {
    if (get_invention(pplayer, i)!=TECH_KNOWN && get_invention(target, i)== TECH_KNOWN) {
      j++;
    }
  }
  if (!j) 
    return;
  if (city->steal) {
    notify_player_ex(pplayer, city->x, city->y, E_NOEVENT,
		       "Game: Your diplomat was caught in the attempt of stealing technology in %s.", city->name);
    notify_player_ex(target, city->x, city->y, E_DIPLOMATED,
		     "Game: %s diplomat failed in stealing technology in %s", pplayer->name, city->name);
    return;
  }

  j=myrand(j)+1;
  for (i=1;i<A_LAST;i++) {
    if (get_invention(pplayer, i)!=TECH_KNOWN && 
	get_invention(target, i)== TECH_KNOWN) 
      j--;
    if (!j) break;
  }
  if (i==A_LAST) {
    printf("Bug in diplomat_a_tech\n");
    return;
  }
  city->steal=1;
  set_invention(pplayer, i, TECH_KNOWN);
  update_research(pplayer);
  do_tech_cost(pplayer);
  pplayer->research.researchpoints++;
  notify_player_ex(pplayer, city->x, city->y, E_NOEVENT,
		   "Game: Your diplomat stole %s from %s",
		   advances[i].name, target->name); 
  notify_player_ex(target, city->x, city->y, E_DIPLOMATED,
		   "Game: %s diplomat stole %s in %s.", 
		   pplayer->name, advances[i].name, city->name); 
  if (pplayer->research.researching==i) {
    tec=pplayer->research.researched;
    if (!choose_goal_tech(pplayer))
      choose_random_tech(pplayer);
    pplayer->research.researched=tec;
  }

  wipe_unit(0, pdiplomat);
}

/**************************************************************************
 Inciting a revolution will fail if
 1) the attacked player is running a democracy
 2) the attacker don't have enough credits
 3) there is a defending spy in the city
**************************************************************************/
void diplomat_incite(struct player *pplayer, struct unit *pdiplomat, struct city *pcity)
{
  struct player *cplayer;
  struct city *pnewcity, *pc2;
  int i;
  if (!pcity)
    return;
 
  cplayer=city_owner(pcity);
  if (cplayer==pplayer || cplayer==NULL) 
    return;
  if(game.players[cplayer->player_no].government==G_DEMOCRACY) {
      notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT, 
		       "Game: You can't subvert a city from a democratic nation.");
      return;
  }
  if (pplayer->economic.gold<pcity->incite_revolt_cost) { 
    notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT, 
		     "Game: You don't have enough credits to subvert %s.", pcity->name);
    
    return;
  }
  if (diplomat_on_tile(pcity->x, pcity->y)) {
    notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT, 
		     "Game: Your spy has been eliminated by a defending spy in %s.", pcity->name);
    notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED,
		     "Game: A%s spy has been eliminated in %s..", n_if_vowel(get_race_name(cplayer->race)[0]), pcity->name);
    wipe_unit(0, pdiplomat);
    return;
  }

  pplayer->economic.gold-=pcity->incite_revolt_cost;
  if (pcity->size >1) {
    pcity->size--;
    city_auto_remove_worker(pcity);
  }
  notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT, 
		   "Game: Revolt incited in %s, you now rule the city!", 
		   pcity->name);
  notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED, 
		   "Game: %s has revolted, %s influence suspected", pcity->name, get_race_name(pplayer->race));
  pnewcity=(struct city *)malloc(sizeof(struct city));
  *pnewcity=*pcity;
  remove_city(pcity);
  for (i=0;i<4;i++) {
    pc2=find_city_by_id(pnewcity->trade[i]);
    if (can_establish_trade_route(pnewcity, pc2))    
      establish_trade_route(pnewcity, pc2);
  }
  pnewcity->id=get_next_id_number();
  pnewcity->owner=pplayer->player_no;
  unit_list_init(&pnewcity->units_supported);
  city_list_insert(&pplayer->cities, pnewcity);
  map_set_city(pnewcity->x, pnewcity->y, pnewcity);
  raze_city(pcity);
  city_refresh(pnewcity);
  send_city_info(0, pnewcity, 0);
  send_player_info(pplayer, pplayer);
  wipe_unit(0, pdiplomat);
}

/**************************************************************************
Sabotage of enemy building, build order will fail if:
 1) there is a defending spy in the city.
 2) there is a chance it'll fail if the sabotage is a building
 The sabotage will be 50/50 between wiping the current production or
 an existing building.
 NOTE: Wonders and palace can't be sabotaged
**************************************************************************/
void diplomat_sabotage(struct player *pplayer, struct unit *pdiplomat, struct city *pcity)
{
  struct player *cplayer;
  char *prod;
  if (!pcity)
    return;
  cplayer=city_owner(pcity);
  if (cplayer==pplayer ||cplayer==NULL) return;

  if (diplomat_on_tile(pcity->x, pcity->y)) {
    notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT, 
		     "Game: Your spy has been eliminated by a defending spy in %s.", pcity->name);
    notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED,
		     "Game: A%s spy has been eliminated in %s.", n_if_vowel(get_race_name(cplayer->race)[0]), pcity->name);
    wipe_unit(0, pdiplomat);
    return;
  }

  switch (myrand(2)) {
  case 0:
    pcity->shield_stock=0;
    if (pcity->is_building_unit) 
      prod=unit_name(pcity->currently_building);
    else
      prod=get_improvement_name(pcity->currently_building);
    notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT,
    "Game: Your Diplomat succeeded destroying the production of %s in %s", 
    prod, pcity->name);
    notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED, 
		     "Game: The production of %s was destroyed in %s, %s are suspected for the sabotage.", 
		     prod, pcity->name, get_race_name_plural(cplayer->race));

    break;
  case 1:
    {
      int building;
      int i;
      for (i=0;i<10;i++) {
	building=myrand(B_LAST);
	if (city_got_building(pcity, building) 
	    && !is_wonder(building) && building!=B_PALACE) {
	  pcity->improvements[building]=0;
	  break;
	}
      }
      if (i<10) {
	notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT,
			 "Game: Your Diplomat destroyed %s in %s.", 
			 get_improvement_name(building), pcity->name);
	notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED,
			 "Game: The %s destroyed %s in %s.", 
			 get_race_name(cplayer->race), get_improvement_name(building), pcity->name);
      } else {
	notify_player_ex(pplayer, pcity->x, pcity->y, E_NOEVENT,
		      "Game: Your Diplomat was caught in the attempt of industrial sabotage!");
	notify_player_ex(cplayer, pcity->x, pcity->y, E_DIPLOMATED,
			 "Game: You caught a%s %s diplomat in industrial sabotage!",
		n_if_vowel(get_race_name(cplayer->race)[0]),
		get_race_name(cplayer->race));
      }
    }
    break;
  }
  send_city_info(cplayer, pcity, 1);

  wipe_unit(0, pdiplomat);
}

/***************************************************************************
Restore adds move points/hitpoints and upgrade units if player
have leonardo's workshop.
Air units have to be on carriers or cities or they'll be wiped.
Missiles will have to be on submarines or in citites or they'll be wiped
choppers out of city will loose 1 hitpoint every turn.
triremes will be wiped with 50% chance 
if they're not close to land, and player  don't have lighthouse.
****************************************************************************/
void player_restore_units(struct player *pplayer)
{
  struct city *pcity;
  int leonardo=0;

  pcity=city_list_find_id(&pplayer->cities, 
			  game.global_wonders[B_LEONARDO]);
  if ((pcity && !wonder_obsolete(B_LEONARDO))) 
    leonardo=1;
  unit_list_iterate(pplayer->units, punit) {
    if (leonardo && can_upgrade_unittype(pplayer, punit->type)!=-1) {
      if (punit->hp==get_unit_type(punit->type)->hp) 
	punit->hp = get_unit_type(can_upgrade_unittype(pplayer, punit->type))->hp;
      notify_player(pplayer, "Game: Leonardo's workshop has upgraded %s to %s ",
		    unit_types[punit->type].name, 
		    unit_types[can_upgrade_unittype(pplayer, punit->type)].name);
      punit->type = can_upgrade_unittype(pplayer, punit->type);
      punit->veteran = 0;
      leonardo = 0;
    }

    unit_restore_hitpoints(pplayer, punit);
    unit_restore_movepoints(pplayer, punit);
    
    if(is_air_unit(punit)) {
      punit->fuel--;
      if(map_get_city(punit->x, punit->y))
	punit->fuel=get_unit_type(punit->type)->fuel;
      else {
	unit_list_iterate(map_get_tile(punit->x, punit->y)->units, punit2)
	  if (unit_flag(punit2->type,F_CARRIER))
	    punit->fuel=get_unit_type(punit->type)->fuel;
	  else if (unit_flag(punit->type, F_MISSILE) && 
		   unit_flag(punit2->type, F_SUBMARINE))
	    punit->fuel=get_unit_type(punit->type)->fuel;
	unit_list_iterate_end;
      }
      if(punit->fuel<=0) {
	send_remove_unit(0, punit->id);
	notify_player_ex(pplayer, punit->x, punit->y, E_UNIT_LOST, 
			 "Game: Your %s has run out of fuel",
			 unit_name(punit->type));
	wipe_unit(0, punit);
      }
    } else if (punit->type==U_TRIREME) {
      struct city *pcity;
      pcity=city_list_find_id(&pplayer->cities, 
			    game.global_wonders[B_LIGHTHOUSE]);
      if (!(pcity && !wonder_obsolete(B_LIGHTHOUSE))) { 
	if (!is_coastline(punit->x, punit->y) && (myrand(100) > 50)) {
	  notify_player_ex(pplayer, punit->x, punit->y, E_UNIT_LOST, 
			   "Game: Your Trireme has been lost on the high seas");
	  wipe_unit(pplayer, punit);
	}
      }
    }
  }
  unit_list_iterate_end;
}

/****************************************************************************
  add hitpoints to the unit, hp_gain_coord returns the amount to add
  united nations will speed up the process by 2 hp's / turn, means helicopters
  will actually not loose hp's every turn if player have that wonder.
*****************************************************************************/
void unit_restore_hitpoints(struct player *pplayer, struct unit *punit)
{
  struct city *pcity;
  
  punit->hp+=hp_gain_coord(punit);
  
  pcity=city_list_find_id(&pplayer->cities, game.global_wonders[B_UNITED]);

  if(pcity && !wonder_obsolete(B_UNITED))
    punit->hp+=2;
    
  if(punit->hp>get_unit_type(punit->type)->hp)
    punit->hp=get_unit_type(punit->type)->hp;
}
  

/***************************************************************************
 move points are trivial, only modifiers to the base value is if it's
  sea units and the player has certain wonders/techs
***************************************************************************/
void unit_restore_movepoints(struct player *pplayer, struct unit *punit)
{
  punit->moves_left=unit_move_rate(punit);
}

/**************************************************************************
  after a battle this routine is called to decide whether or not the unit
  should become a veteran, if unit isn't already.
  there is a 50/50% chance for it to happend, (100% if player got SUNTZU)
**************************************************************************/
void maybe_make_veteran(struct unit *punit)
{
    struct city *pcity;
    if (punit->veteran) 
      return;
    pcity=city_list_find_id(&game.players[punit->owner].cities, 
			    game.global_wonders[B_SUNTZU]);
    if(pcity && !wonder_obsolete(B_SUNTZU)) 
      punit->veteran = 1;
    else
      punit->veteran=myrand(2);
}

/**************************************************************************
  This is the basic unit versus unit combat routine.
  1) ALOT of modifiers bonuses etc is added to the 2 units rates.
  2) the combat loop, which continues until one of the units are dead
  3) the aftermath, the looser (and potentially the stack which is below it)
     is wiped, and the winner gets a chance of gaining veteran status
**************************************************************************/
void unit_versus_unit(struct unit *attacker, struct unit *defender)
{
  int attackpower=get_total_attack_power(attacker,defender);
  int defensepower=get_total_defense_power(attacker,defender);

  log(LOG_DEBUG, "attack:%d, defense:%d\n", attackpower, defensepower);
  if (!attackpower) {
      attacker->hp=0; 
  } else if (!defensepower) {
      defender->hp=0;
  }
  while (attacker->hp>0 && defender->hp>0) {
    if (myrand(attackpower+defensepower)>= defensepower) {
      defender->hp=defender->hp-get_unit_type(attacker->type)->firepower;
    } else {
      if (is_sailing_unit(defender) && map_get_city(defender->x, defender->y))
	attacker->hp=attacker->hp-1;              /* pearl harbour */
      else
	attacker->hp=attacker->hp-get_unit_type(defender->type)->firepower;
    }
  }
  if (attacker->hp<0) attacker->hp=0;
  if (defender->hp<0) defender->hp=0;

  if (attacker->hp)
    maybe_make_veteran(attacker); 
  else if (defender->hp)
    maybe_make_veteran(defender);
}

/***************************************************************************
 return the modified attack power of a unit.  Currently they aren't any
 modifications...
***************************************************************************/
int get_total_attack_power(struct unit *attacker, struct unit *defender)
{
  int attackpower=get_attack_power(attacker);

  return attackpower;
}

/***************************************************************************
 return the modified defense power of a unit.
 An veteran aegis cruiser in a mountain city with SAM and SDI defense 
 being attacked by a missile gets defense 288.
***************************************************************************/
int get_total_defense_power(struct unit *attacker, struct unit *defender)
{
  int defensepower=get_defense_power(defender);
  if (unit_flag(defender->type, F_PIKEMEN) && unit_flag(attacker->type, F_HORSE)) 
    defensepower*=2;
  if (unit_flag(defender->type, F_AEGIS) && (is_air_unit(attacker) || is_heli_unit(attacker)))
    defensepower*=2;
  if (is_air_unit(attacker)) {
    if (unit_behind_sam(defender))
      defensepower*=2;
    if (unit_behind_sdi(defender) && unit_flag(attacker->type, F_MISSILE))
      defensepower*=2;
  } else if (is_sailing_unit(attacker)) {
    if (unit_behind_coastal(defender))
      defensepower*=2;
  } else if (!unit_ignores_citywalls(attacker) && 
	     (is_heli_unit(attacker) || is_ground_unit(attacker)) && 
	     unit_behind_walls(defender)) 
    defensepower*=3;
  if (unit_on_fortress(defender) && 
      !map_get_city(defender->x, defender->y)) 
    defensepower*=2;
  if ((defender->activity == ACTIVITY_FORTIFY || 
       map_get_city(defender->x, defender->y)) && 
      is_ground_unit(defender))
    defensepower*=1.5;

  return defensepower;
}

/**************************************************************************
 creates a unit, and set it's initial values, and put it into the right 
 lists.
 TODO: Maybe this procedure should refresh its homecity? so it'll show up 
 immediately on the clients? (refresh_city + send_city_info)
**************************************************************************/
void create_unit(struct player *pplayer, int x, int y, enum unit_type_id type,
		 int make_veteran, int homecity_id)
{
  struct unit *punit;
  struct city *pcity;
  punit=(struct unit *)malloc(sizeof(struct unit));
  punit->type=type;
  punit->id=get_next_id_number();
  punit->owner=pplayer->player_no;
  punit->x=x;
  punit->y=y;
  punit->goto_dest_x=0;
  punit->goto_dest_y=0;
  
  pcity=game_find_city_by_id(homecity_id);
  punit->veteran=make_veteran;
  punit->homecity=homecity_id;
  punit->hp=get_unit_type(punit->type)->hp;
  punit->activity=ACTIVITY_IDLE;
  punit->activity_count=0;
  punit->upkeep=0;
  punit->unhappiness=0;
  punit->fuel=get_unit_type(punit->type)->fuel;
  punit->ai.control=0;
  punit->ai.ai_role = AIUNIT_NONE;
  unit_list_insert(&pplayer->units, punit);
  unit_list_insert(&map_get_tile(x, y)->units, punit);
  if (pcity)
    unit_list_insert(&pcity->units_supported, punit);
  punit->bribe_cost=unit_bribe_cost(punit);
  punit->moves_left=unit_move_rate(punit);
  send_unit_info(0, punit, 0);
}


/**************************************************************************
  Removes the unit from the game, and notifies the clients.
  TODO: Find out if the homecity is refreshed and resend when this happends
  otherwise (refresh_city(homecity) + send_city(homecity))
**************************************************************************/
void send_remove_unit(struct player *pplayer, int unit_id)
{
  int o;
  
  struct packet_generic_integer packet;

  packet.value=unit_id;

  for(o=0; o<game.nplayers; o++)           /* dests */
    if(!pplayer || &game.players[o]==pplayer)
      send_packet_generic_integer(game.players[o].conn, PACKET_REMOVE_UNIT,
				  &packet);
}

/**************************************************************************
  iterate through all units and update them.
**************************************************************************/
void update_unit_activities(struct player *pplayer)
{
  unit_list_iterate(pplayer->units, punit)
    update_unit_activity(pplayer, punit);
  unit_list_iterate_end;
}

/**************************************************************************
  assign a new task to a unit.
**************************************************************************/
void set_unit_activity(struct unit *punit, enum unit_activity new_activity)
{
  punit->activity=new_activity;
  punit->activity_count=0;
}

/**************************************************************************
  progress settlers in their current tasks, 
  and units that is pillaging.
  also move units that is on a goto.
**************************************************************************/
void update_unit_activity(struct player *pplayer, struct unit *punit)
{
  punit->activity_count+= (get_unit_type(punit->type)->move_rate)/3;
  
   if(punit->activity==ACTIVITY_PILLAGE && punit->activity_count>=1) {
      if(map_get_special(punit->x, punit->y)&S_IRRIGATION)
	map_clear_special(punit->x, punit->y, S_IRRIGATION);
      else if(map_get_special(punit->x, punit->y)&S_RAILROAD)
	map_clear_special(punit->x, punit->y, S_RAILROAD);
      else 
	map_clear_special(punit->x, punit->y, S_ROAD);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    set_unit_activity(punit, ACTIVITY_IDLE);
   }

  if(punit->activity==ACTIVITY_POLLUTION && punit->activity_count>=3) {
    map_clear_special(punit->x, punit->y, S_POLLUTION);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    set_unit_activity(punit, ACTIVITY_IDLE);
  }

  if(punit->activity==ACTIVITY_FORTRESS && punit->activity_count>=3) {
    map_set_special(punit->x, punit->y, S_FORTRESS);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    set_unit_activity(punit, ACTIVITY_IDLE);
  }
  
  if(punit->activity==ACTIVITY_IRRIGATE && 
     punit->activity_count>=map_build_irrigation_time(punit->x, punit->y)) {
    map_irrigate_tile(punit->x, punit->y);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    punit->activity=ACTIVITY_IDLE;
    set_unit_activity(punit, ACTIVITY_IDLE);
  }

  if(punit->activity==ACTIVITY_ROAD && 
     punit->activity_count>map_build_road_time(punit->x, punit->y)) {
    map_set_special(punit->x, punit->y, S_ROAD);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    punit->activity=ACTIVITY_IDLE;
    set_unit_activity(punit, ACTIVITY_IDLE);
  }

  if(punit->activity==ACTIVITY_RAILROAD && punit->activity_count>=3) {
    map_set_special(punit->x, punit->y, S_RAILROAD);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    punit->activity=ACTIVITY_IDLE;
    handle_unit_activity_request(pplayer, punit, ACTIVITY_IDLE);
    set_unit_activity(punit, ACTIVITY_IDLE);
  }
  
  if(punit->activity==ACTIVITY_MINE && 
     punit->activity_count>=map_build_mine_time(punit->x, punit->y)) {
    map_mine_tile(punit->x, punit->y);
    send_tile_info(0, punit->x, punit->y, TILE_KNOWN);
    set_unit_activity(punit, ACTIVITY_IDLE);
  }

  if(punit->activity==ACTIVITY_GOTO) {
    do_unit_goto(pplayer, punit);
    return;
  }
  
  if(punit->activity==ACTIVITY_IDLE && 
     map_get_terrain(punit->x, punit->y)==T_OCEAN &&
     is_ground_unit(punit))
    set_unit_activity(punit, ACTIVITY_SENTRY);
  send_unit_info(0, punit, 0);
}


/**************************************************************************
 nuke a square
 1) remove all units on the square
 2) half the size of the city on the square
 if it isn't a city square or an ocean square then with 50% chance 
 add some pollution, then notify the client about the changes.
**************************************************************************/
void do_nuke_tile(int x, int y)
{
  struct unit_list *punit_list;
  struct city *pcity;
  punit_list=&map_get_tile(x, y)->units;
  
  while(unit_list_size(punit_list)) {
    struct unit *punit=unit_list_get(punit_list, 0);
    wipe_unit(0, punit);
  }

  if((pcity=game_find_city_by_coor(x,y))) {
    pcity->size/=2;
    auto_arrange_workers(pcity);
    send_city_info(0, pcity, 0);
  }
  else if ((map_get_terrain(x,y)!=T_OCEAN && map_get_terrain(x,y)<=T_TUNDRA) &&
           (!(map_get_special(x,y)&S_POLLUTION)) && myrand(2)) { 
    map_set_special(x,y, S_POLLUTION);
    send_tile_info(0, x, y, TILE_KNOWN);
  }
}

/**************************************************************************
  nuke all the squares in a 3x3 square around the center of the explosion
**************************************************************************/
void do_nuclear_explosion(int x, int y)
{
  int i,j;
  for (i=0;i<3;i++)
    for (j=0;j<3;j++)
      do_nuke_tile(x+i-1,y+j-1);
}


/**************************************************************************
Move the unit if possible 
  if the unit has less moves than it costs to enter a square, we roll the dice:
  1) either succeed
  2) or have it's moves set to 0
  a unit can always move atleast once
**************************************************************************/
int try_move_unit(struct unit *punit, int dest_x, int dest_y) 
{
  if (myrand(1+map_move_cost(punit, dest_x, dest_y))>punit->moves_left && punit->moves_left<unit_move_rate(punit)) {
    punit->moves_left=0;
    send_unit_info(&game.players[punit->owner], punit, 0);
  }
  return punit->moves_left;
}
/**************************************************************************
  go by airline, if both cities have an airport and neither has been used this
  turn the unit will be transported by it and have it's moves set to 0
**************************************************************************/
int do_airline(struct unit *punit, int x, int y)
{
  struct city *city1, *city2;
  
  if (!punit->moves_left)
    return 0;
  if (!(city1=map_get_city(punit->x, punit->y))) 
    return 0;
  if (!(city2=map_get_city(x,y)))
    return 0;
  if (city1->owner != city2->owner) 
    return 0;
  if (city1->airlift + city2->airlift < 2) 
    return 0;
  city1->airlift=0;
  city2->airlift=0;
  punit->moves_left = 0;
  unit_list_unlink(&map_get_tile(punit->x, punit->y)->units, punit);
  punit->x = x;
  punit->y = y;
  send_unit_info(&game.players[punit->owner], punit, 0);
  unit_list_insert(&map_get_tile(x, y)->units, punit);
  notify_player_ex(&game.players[punit->owner], punit->x, punit->y, E_NOEVENT,
		   "Game: unit transported succesfully.");
  return 1;
}


/**************************************************************************
  called when a player conquers a city, remove buildings (not wonders and always palace) with game.razechance% chance
  set the city's shield stock to 0
**************************************************************************/
void raze_city(struct city *pcity)
{
  int i;
  pcity->improvements[B_PALACE]=0;
  for (i=0;i<B_LAST;i++) {
    if (city_got_building(pcity, i) && !is_wonder(i) 
	&& (myrand(100) < game.razechance)) {
      pcity->improvements[i]=0;
    }
  }
  if (pcity->shield_stock > 0)
    pcity->shield_stock=0;
  /*  advisor_choose_build(pcity);  we add the civ bug here :)*/
}

/**************************************************************************
  if target has more techs than pplayer, pplayer will get a random of these
  the clients will both be notified.
  TODO: Players with embassies in these countries should be notified aswell
**************************************************************************/
void get_a_tech(struct player *pplayer, struct player *target)
{
  int tec;
  int i;
  int j=0;
  for (i=0;i<A_LAST;i++) {
    if (get_invention(pplayer, i)!=TECH_KNOWN && 
	get_invention(target, i)== TECH_KNOWN) {
      j++;
    }
  }
  if (!j) 
    return;
  j=myrand(j)+1;
  for (i=0;i<A_LAST;i++) {
    if (get_invention(pplayer, i)!=TECH_KNOWN && 
	get_invention(target, i)== TECH_KNOWN) 
      j--;
    if (!j) break;
  }
  if (i==A_LAST) 
    printf("Bug in get_a_tech\n");
  set_invention(pplayer, i, TECH_KNOWN);
  update_research(pplayer);
  do_tech_cost(pplayer);
  pplayer->research.researchpoints++;
  notify_player(pplayer, "Game: You acquired %s from %s",
		advances[i].name, target->name); 
  notify_player(target, "Game: %s discovered %s in the city.", pplayer->name, 
		advances[i].name); 
  if (pplayer->research.researching==i) {
    tec=pplayer->research.researched;
    if (!choose_goal_tech(pplayer))
      choose_random_tech(pplayer);
    pplayer->research.researched=tec;
  }
}

/**************************************************************************
  finds a spot around pcity and place a partisan.
**************************************************************************/
int place_partisan(struct city *pcity)
{
  int x,y;
  int count;

  count = 0;
  for (x = pcity->x -2; x < pcity->x + 3; x++)
    for (y = pcity->y - 2; y < pcity->y + 3; y++) {
      if (can_place_partisan(x, y)) 
	count++;
    }
  if (count) {
    count = myrand(count) + 1;
    for (x = pcity->x - 2; x < pcity->x + 3; x++)
      for (y = pcity->y - 2; y < pcity->y + 3; y++) {
	if (can_place_partisan(x, y))
	  count--;
	if (!count) {
	  create_unit(&game.players[pcity->owner], x, y, U_PARTISAN, 0, 0);
	  return 1;
	}
      }
    printf("Bug in place partisan");
    return 0;
  } else 
    return 0;
}

/**************************************************************************
  if requirements to make partisans when a city is conquered is fullfilled
  this routine makes alot of partisan based on the city's size.
  To be candidate for partisans the following things must be satisfied:
  1) Guerilla warfare must be known by atleast 1 player
  2) The owner of the city is the original player.
  3) The player must know about communism and gunpowder
  4) the player must run either a democracy or a communist society.
**************************************************************************/
void make_partisans(struct city *pcity)
{
  int partisans;
  if (!game.global_advances[A_GUERILLA] || pcity->original != pcity->owner)
    return;
  if (get_invention(city_owner(pcity), A_COMMUNISM) != TECH_KNOWN 
      && get_invention(city_owner(pcity), A_GUNPOWDER) != TECH_KNOWN)
    return;
  if (get_government(pcity->owner)!=G_DEMOCRACY
      && get_government(pcity->owner)!=G_COMMUNISM) 
    return;
  
  partisans = myrand(1 + pcity->size/2) + 1;
  if (partisans > 8) 
    partisans = 8;
  
  while (partisans && place_partisan(pcity)) partisans--;
}


/**************************************************************************
this is a highlevel routine
**************************************************************************/
void wipe_unit(struct player *dest, struct unit *punit)
{
  if(get_transporter_capacity(punit) && 
     map_get_terrain(punit->x, punit->y)==T_OCEAN) {
    struct unit_list list;
    
    transporter_cargo_to_unitlist(punit, &list);
    
    unit_list_iterate(list, punit2) {
      send_remove_unit(0, punit2->id);
      game_remove_unit(punit2->id);
    }
    unit_list_iterate_end;
  }
  send_remove_unit(0, punit->id);
  game_remove_unit(punit->id);
}

/**************************************************************************
this is a highlevel routine
the unit has been killed in combat => all other units on the
tile dies unless ...
**************************************************************************/
void kill_unit(struct player *dest, struct unit *punit)
{
  int klaf;
  struct city *pcity = map_get_city(punit->x, punit->y);
  klaf=unit_list_size(&(map_get_tile(punit->x, punit->y)->units));
  if( (pcity) || 
      (map_get_special(punit->x, punit->y)&S_FORTRESS) || 
      (klaf == 1)) {
    if (pcity) 
      notify_player_ex(&game.players[punit->owner], 
		       punit->x, punit->y, E_UNIT_LOST,
		       "Game: You lost a%s %s under an attack from %s, in %s",
		       n_if_vowel(get_unit_type(punit->type)->name[0]),
		       get_unit_type(punit->type)->name, dest->name, pcity->name);
    else
      notify_player_ex(&game.players[punit->owner], 
		       punit->x, punit->y, E_UNIT_LOST,
		       "Game: You lost a%s %s under an attack from %s",
		       n_if_vowel(get_unit_type(punit->type)->name[0]),
		       get_unit_type(punit->type)->name, dest->name);
    send_remove_unit(0, punit->id);
    game_remove_unit(punit->id);
  }  else {
      notify_player_ex(&game.players[punit->owner], 
		       punit->x, punit->y, E_UNIT_LOST, 
		       "Game: You lost %d units under an attack from %s",
		       klaf, dest->name);
      unit_list_iterate(map_get_tile(punit->x, punit->y)->units, punit2) {
	notify_player_ex(&game.players[punit2->owner], 
			 punit->x, punit->y, E_UNIT_LOST,
			 "Game: You lost a%s %s under an attack from %s",
			 n_if_vowel(get_unit_type(punit2->type)->name[0]),
			 get_unit_type(punit2->type)->name, dest->name);
	send_remove_unit(0, punit2->id);
	game_remove_unit(punit2->id);
      }
      unit_list_iterate_end;
  } 
}

/**************************************************************************
  send the unit to the players who need the info 
  dest = NULL means all players, dosend means send even if the player 
  can't see the unit.
**************************************************************************/
void send_unit_info(struct player *dest, struct unit *punit, int dosend)
{
  int o;
  struct packet_unit_info info;

  info.id=punit->id;
  info.owner=punit->owner;
  info.x=punit->x;
  info.y=punit->y;
  info.homecity=punit->homecity;
  info.veteran=punit->veteran;
  info.type=punit->type;
  info.movesleft=punit->moves_left;
  info.hp=punit->hp;
  info.activity=punit->activity;
  info.activity_count=punit->activity_count;
  info.unhappiness=punit->unhappiness;
  info.upkeep=punit->upkeep;
  info.bribe_cost=punit->bribe_cost;
  info.ai=punit->ai.control;
  info.goto_dest_x=punit->goto_dest_x;
  info.goto_dest_y=punit->goto_dest_y;
    
  for(o=0; o<game.nplayers; o++)           /* dests */
    if(!dest || &game.players[o]==dest)
      if(dosend || map_get_known(info.x, info.y, &game.players[o]))
	 send_packet_unit_info(game.players[o].conn, &info);
}

/**************************************************************************
  simply puts the settler unit into goto
**************************************************************************/
int auto_settler_do_goto(struct player *pplayer, struct unit *punit, int x, int y)
{
  punit->goto_dest_x=map_adjust_x(x);
  punit->goto_dest_y=map_adjust_y(y);
  punit->activity=ACTIVITY_GOTO;
  punit->activity_count=0;
  send_unit_info(0, punit, 0);
  do_unit_goto(pplayer, punit);
  return 1;
}

/********************************************************************
  find some work for the settler
*********************************************************************/
void auto_settler_findwork(struct player *pplayer, struct unit *punit) 
{
  int gx,gy;
  int co=map_get_continent(punit->x, punit->y);
  int t=0;
  int v=0;
  int v2;
  int x, y,z;
  gx=-1;
  gy=-1;
  for (x=0;x<map.xsize;x++)
    for (y=0;y<map.ysize;y++) 
      if (map_get_continent(x, y)==co) {
	z=map_distance(x,y, punit->x, punit->y);
	v2=dist_mod(z, ai_calc_irrigate(punit, pplayer, x, y));
	if (v2>v) {
	  t=ACTIVITY_IRRIGATE;
	  v=v2; gx=x; gy=y;
	}

	v2=dist_mod(z, ai_calc_road(punit, pplayer, x, y));
	if (v2>v) {
	  if (map_get_special(punit->x, punit->y) & S_ROAD)
	    t=ACTIVITY_RAILROAD;
	  else
	    t=ACTIVITY_ROAD;
	  v=v2; gx=x; gy=y;
	}

	v2=dist_mod(z, ai_calc_mine(punit, pplayer, x, y));
	if (v2>v) {
	  t=ACTIVITY_MINE;
	  v=v2; gx=x; gy=y;
	}

	v2=dist_mod(z, ai_calc_pollution(punit, pplayer, x, y));
	if (v2>v) {
	  t=ACTIVITY_POLLUTION;
	  v=v2; gx=x; gy=y;
	}
      }
  if (same_pos(gx, gy, punit->x, punit->y)) {
    set_unit_activity(punit, t);
    send_unit_info(0, punit, 0);
    return;
  }
  if (gx!=-1 && gy!=-1) 
    auto_settler_do_goto(pplayer, punit,gx, gy);
  else 
    punit->ai.control=0;
}

/************************************************************************** 
  run through all the players settlers and let those on ai.control work 
  automagically
**************************************************************************/
void auto_settlers_player(struct player *pplayer) 
{
  unit_list_iterate(pplayer->units, punit) {
    if (punit->ai.control) {
      if(punit->activity == ACTIVITY_SENTRY && 
	 !map_get_terrain(punit->x, punit->y) == T_OCEAN &&
	 is_ground_unit(punit))
	set_unit_activity(punit, ACTIVITY_IDLE);
      if (punit->activity == ACTIVITY_IDLE)
	auto_settler_findwork(pplayer, punit);
      if (punit->ai.control && punit->moves_left && punit->activity == ACTIVITY_IDLE) 
	/* fix for the lost turn */
	auto_settler_findwork(pplayer, punit);
    }
  }
  unit_list_iterate_end;
}

/**************************************************************************
  Do the auto_settler stuff for all the players. 
  TODO: This should be moved into the update_player loop i think
**************************************************************************/
void auto_settlers()
{
  int i;
  for (i = 0; i < game.nplayers; i++) {
    auto_settlers_player(&game.players[i]);
  }
}




