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
#include <string.h>

#include <civserver.h>
#include <registry.h>
#include <game.h>
#include <gamehand.h>
#include <unitfunc.h>
#include <cityturn.h>
#include <maphand.h>
#include <plrhand.h>
#include <packets.h>
#include <map.h>
#include <meta.h>
#include <capability.h>
#include <time.h>
#include <log.h>
#include <ruleset.h>

extern char metaserver_info_line[];
extern RANDOM_TYPE RandomState[];
extern int iRandJ, iRandK, iRandX; 
extern int rand_init;

#define SAVEFILE_OPTIONS "1.7 startoptions unirandom spacerace rulesets"

/**************************************************************************
...
**************************************************************************/
void init_new_game(void)
{
  int i, x, y,j;
  if (game.scenario!=1 && game.scenario!=2) {
    /* except in a scenario which provides them,
       shuffle the start positions around... */
    for (i=0; i<game.nplayers;i++) { /* no advantage to the romans!! */
      j=myrand(game.nplayers);
      x=map.start_positions[j].x;
      y=map.start_positions[j].y;
      map.start_positions[j].x=map.start_positions[i].x;
      map.start_positions[j].y=map.start_positions[i].y;
      map.start_positions[i].x=x;
      map.start_positions[i].y=y;
    }
  }
  for(i=0; i<game.nplayers; i++) {
    if(game.scenario==1 || game.scenario==2) {
      /* in a scenario, choose starting positions by race */
      x=map.start_positions[game.players[i].race].x;
      y=map.start_positions[game.players[i].race].y;
    } else {
      x=map.start_positions[i].x;
      y=map.start_positions[i].y;
    }
    /* For scenarios, huts may coincide with player starts;
       remove any such hut: */
    if(map_get_special(x,y)&S_HUT) {
      map_clear_special(x,y,S_HUT);
      freelog(LOG_DEBUG, "Removed hut on start position for %s",
	  game.players[i].name);
    }
    /* Civ 1 exposes a single square radius of the map to start the game. */
    /* Civ 2 exposes a "city radius". -AJS */
    light_square(&game.players[i], x, y, 1);
    if (game.civstyle==2) {
      light_square(&game.players[i], x-1, y, 1);
      light_square(&game.players[i], x+1, y, 1);
      light_square(&game.players[i], x, y-1, 1);
      light_square(&game.players[i], x, y+1, 1);
    }
    for (j=0;j<game.settlers;j++) 
      create_unit(&game.players[i], x, y, get_role_unit(F_SETTLERS,0), 0, 0, -1);
    for (j=0;j<game.explorer;j++) 
      create_unit(&game.players[i], x, y, get_role_unit(L_EXPLORER,0), 0, 0, -1);
  }
}


/**************************************************************************
...
**************************************************************************/
void send_year_to_clients(int year)
{
  int i;
  struct packet_new_year apacket;
  apacket.year=year;

  for(i=0; i<game.nplayers; i++) {
    game.players[i].turn_done=0;
    game.players[i].nturns_idle++;
    send_packet_new_year(game.players[i].conn, &apacket);
  }
}


/**************************************************************************
...
**************************************************************************/
void send_game_state(struct player *dest, int state)
{
  int o;
  struct packet_generic_integer pack;

  pack.value=state;
  
  
  for(o=0; o<game.nplayers; o++)
    if(!dest || &game.players[o]==dest)
      send_packet_generic_integer(game.players[o].conn, 
				  PACKET_GAME_STATE, &pack);
}



/**************************************************************************
dest can be NULL meaning all players
**************************************************************************/
void send_game_info(struct player *dest)
{
  int i, o;
  struct packet_game_info ginfo;
  
  ginfo.gold=game.gold;
  ginfo.tech=game.tech;
  ginfo.techlevel=game.techlevel;
  ginfo.skill_level=game.skill_level;
  ginfo.timeout=game.timeout;
  ginfo.end_year=game.end_year;
  ginfo.year=game.year;
  ginfo.min_players=game.min_players;
  ginfo.max_players=game.max_players;
  ginfo.nplayers=game.nplayers;
  ginfo.globalwarming=game.globalwarming;
  ginfo.heating=game.heating;
  ginfo.techpenalty=game.techpenalty;
  ginfo.foodbox = game.foodbox;
  ginfo.civstyle=game.civstyle;
  ginfo.rail_food = game.rail_food;
  ginfo.rail_trade = game.rail_trade;
  ginfo.rail_prod = game.rail_prod;
  ginfo.spacerace = game.spacerace;
  for(i=0; i<A_LAST; i++)
    ginfo.global_advances[i]=game.global_advances[i];
  for(i=0; i<B_LAST; i++)
    ginfo.global_wonders[i]=game.global_wonders[i];

  ginfo.aqueduct_size = game.aqueduct_size;
  ginfo.sewer_size = game.sewer_size;
  ginfo.rtech.get_bonus_tech = game.rtech.get_bonus_tech;
  ginfo.rtech.boat_fast = game.rtech.boat_fast;
  ginfo.rtech.cathedral_plus = game.rtech.cathedral_plus;
  ginfo.rtech.cathedral_minus = game.rtech.cathedral_minus;
  ginfo.rtech.colosseum_plus = game.rtech.colosseum_plus;

  for(o=0; o<game.nplayers; o++)           /* dests */
    if(!dest || &game.players[o]==dest) {
      struct player *pplayer=&game.players[o];
      ginfo.player_idx=o;
      send_packet_game_info(pplayer->conn, &ginfo);
    }
}

/***************************************************************
 Assign values to ord_city and ord_map for each unit, so the
 values can be saved.
***************************************************************/
void calc_unit_ordering(void)
{
  int i, j, x, y;
  
  for(i=0; i<game.nplayers; i++) {
    /* to avoid junk values for unsupported units: */
    unit_list_iterate(get_player(i)->units, punit) 
      punit->ord_city = 0;
    unit_list_iterate_end;
    city_list_iterate(get_player(i)->cities, pcity) {
      j = 0;
      unit_list_iterate(pcity->units_supported, punit) 
	punit->ord_city = j++;
      unit_list_iterate_end;
    }
    city_list_iterate_end;
  }

  for(y=0; y<map.ysize; y++) {
    for(x=0; x<map.xsize; x++) {
      j = 0;
      unit_list_iterate(map_get_tile(x,y)->units, punit) 
	punit->ord_map = j++;
      unit_list_iterate_end;
    }
  }
}
/***************************************************************
 For each city and tile, sort unit lists according to
 ord_city and ord_map values.
***************************************************************/
void apply_unit_ordering(void)
{
  int i, x, y;
  
  for(i=0; i<game.nplayers; i++) {
    city_list_iterate(get_player(i)->cities, pcity) {
      unit_list_sort_ord_city(&pcity->units_supported);
    }
    city_list_iterate_end;
  }

  for(y=0; y<map.ysize; y++) {
    for(x=0; x<map.xsize; x++) { 
      unit_list_sort_ord_map(&map_get_tile(x,y)->units);
    }
  }
}

/***************************************************************
...
***************************************************************/
int game_load(struct section_file *file)
{
  int i;
  enum server_states tmp_server_state;
  char *savefile_options=" ";
  char name[20],*string;

  game.version = secfile_lookup_int_default(file, 0, "game.version");
  tmp_server_state = (enum server_states)
    secfile_lookup_int_default(file, RUN_GAME_STATE, "game.server_state");
  strcpy(metaserver_info_line,
	 secfile_lookup_str_default(file, DEFAULT_META_SERVER_INFO_STRING,
				    "game.metastring"));
  
  game.gold          = secfile_lookup_int(file, "game.gold");
  game.tech          = secfile_lookup_int(file, "game.tech");
  game.skill_level   = secfile_lookup_int(file, "game.skill_level");
  if (game.skill_level==0)
    game.skill_level = GAME_OLD_DEFAULT_SKILL_LEVEL;
  game.timeout       = secfile_lookup_int(file, "game.timeout");
  game.end_year      = secfile_lookup_int(file, "game.end_year");
  game.techlevel     = secfile_lookup_int(file, "game.techlevel");
  game.year          = secfile_lookup_int(file, "game.year");
  game.min_players   = secfile_lookup_int(file, "game.min_players");
  game.max_players   = secfile_lookup_int(file, "game.max_players");
  game.nplayers      = secfile_lookup_int(file, "game.nplayers");
  game.globalwarming = secfile_lookup_int(file, "game.globalwarming");
  game.warminglevel  = secfile_lookup_int(file, "game.warminglevel");
  game.unhappysize   = secfile_lookup_int(file, "game.unhappysize");

  if (game.version >= 10100) {
    game.cityfactor = secfile_lookup_int(file, "game.cityfactor");
    game.civilwarsize = secfile_lookup_int_default(file, GAME_DEFAULT_CIVILWARSIZE, "game.civilwarsize");
    game.diplcost   = secfile_lookup_int(file, "game.diplcost");
    game.freecost   = secfile_lookup_int(file, "game.freecost");
    game.conquercost   = secfile_lookup_int(file, "game.conquercost");
    game.rail_food  = secfile_lookup_int(file, "game.rail_food");
    game.rail_prod  = secfile_lookup_int(file, "game.rail_prod");
    game.rail_trade = secfile_lookup_int(file, "game.rail_trade");

    game.foodbox     = secfile_lookup_int(file, "game.foodbox");
    game.techpenalty = secfile_lookup_int(file, "game.techpenalty");
    game.razechance  = secfile_lookup_int(file, "game.razechance");
  }
  if (game.version >= 10300) {
    game.civstyle = secfile_lookup_int(file, "game.civstyle");
    game.save_nturns = secfile_lookup_int(file, "game.save_nturns");
  }

  if ((game.version == 10604 && section_file_lookup(file,"savefile.options"))
      || (game.version > 10604)) 
    savefile_options = secfile_lookup_str(file,"savefile.options");
  /* else leave savefile_options empty */

  /* Note -- as of v1.6.4 you should use savefile_options (instead of
     game.version) to determine which variables you can expect to 
     find in a savegame file.  Or alternatively you can use
     secfile_lookup_int_default() or secfile_lookup_str_default().
  */

  game.aifill = secfile_lookup_int_default(file, 0, "game.aifill");

  game.scorelog = secfile_lookup_int_default(file, 0, "game.scorelog");
  
  game.diplchance = secfile_lookup_int_default(file, game.diplchance,
					       "game.diplchance");
  game.aqueductloss = secfile_lookup_int_default(file, game.aqueductloss,
						 "game.aqueductloss");
  
  if(has_capability("unirandom", savefile_options)) {
    game.randseed = secfile_lookup_int(file, "game.randseed");
    iRandJ = secfile_lookup_int(file,"random.index_J");
    iRandK = secfile_lookup_int(file,"random.index_K");
    iRandX = secfile_lookup_int(file,"random.index_X");
    for(i=0;i<8;i++) {
      sprintf(name,"random.table%d",i);
      string=secfile_lookup_str(file,name);
      sscanf(string,"%8X %8X %8X %8X %8X %8X %8X",&RandomState[7*i],
	     &RandomState[7*i+1],&RandomState[7*i+2],&RandomState[7*i+3],
	     &RandomState[7*i+4],&RandomState[7*i+5],&RandomState[7*i+6]);
    }
    rand_init=1;
  }

  if (has_capability("rulesets",savefile_options)) {
    strcpy(game.ruleset.techs,
	   secfile_lookup_str(file, "game.ruleset.techs"));
    strcpy(game.ruleset.units,
	   secfile_lookup_str(file, "game.ruleset.units"));
    strcpy(game.ruleset.buildings,
	   secfile_lookup_str(file, "game.ruleset.buildings"));
  }

  if (has_capability("spacerace", savefile_options))
    game.spacerace = secfile_lookup_int(file, "game.spacerace");

  game.heating=0;
  if(tmp_server_state==PRE_GAME_STATE 
     || has_capability("startoptions", savefile_options)) {
    if (game.version >= 10300) {
      game.settlers = secfile_lookup_int(file, "game.settlers");
      game.explorer = secfile_lookup_int(file, "game.explorer");
      
      map.riches = secfile_lookup_int(file, "map.riches");
      map.huts = secfile_lookup_int(file, "map.huts");
      map.generator = secfile_lookup_int(file, "map.generator");
      if (tmp_server_state==PRE_GAME_STATE &&
	  map.generator == 0) { /* generator 0 = map done with map editor */
        map_tiles_load(file);
        if (has_capability("startpos",savefile_options)) {
          map_startpos_load(file);
          return 2; /* make this a scenario */
        }
	return 3; /* make this some kind of scenario */
      }
      if (has_capability("startoptions", savefile_options)) {
	map.xsize = secfile_lookup_int(file, "map.width");
	map.ysize = secfile_lookup_int(file, "map.height");
      } else {
	/* old versions saved with these names in PRE_GAME_STATE: */
	map.xsize = secfile_lookup_int(file, "map.xsize");
	map.ysize = secfile_lookup_int(file, "map.ysize");
      }
      map.seed = secfile_lookup_int(file, "map.seed");
      map.landpercent = secfile_lookup_int(file, "map.landpercent");
      map.swampsize = secfile_lookup_int(file, "map.swampsize");
      map.deserts = secfile_lookup_int(file, "map.deserts");
      map.riverlength = secfile_lookup_int(file, "map.riverlength");
      map.mountains = secfile_lookup_int(file, "map.mountains");
      map.forestsize = secfile_lookup_int(file, "map.forestsize");
    }
    if(tmp_server_state==PRE_GAME_STATE) {
      return 0;
    }
  }
  map_load(file);

  for(i=0; i<game.nplayers; i++) {
    player_load(&game.players[i], i, file); 
  }
  
  /* Load rulesets: this has to be after finding game.ruleset strings,
   * (above) and before doing initialize_globals, city_refresh() below.
   * But we need to free the current savefile first, because of
   * the way strbuffermalloc is implemented in registry.c  -- dwp
   */
  section_file_check_unused(file, NULL);
  section_file_free(file);
  load_rulesets();

  initialize_globals();
  apply_unit_ordering();

  for(i=0; i<game.nplayers; i++) {
    /*    player_load(&game.players[i], i, file); */
    city_list_iterate(game.players[i].cities, pcity)
      city_refresh(pcity);
    city_list_iterate_end;
  }

  game.player_idx=0;
  game.player_ptr=&game.players[0];  

  return 1;
}


/***************************************************************
...
***************************************************************/
void game_save(struct section_file *file)
{
  int i;
  int version;
  char name[20],temp[100];

  version = MAJOR_VERSION *10000 + MINOR_VERSION *100 + PATCH_VERSION; 
  secfile_insert_int(file, version, "game.version");
  secfile_insert_int(file, (int) server_state, "game.server_state");
  secfile_insert_str(file, metaserver_info_line, "game.metastring");
  secfile_insert_str(file, SAVEFILE_OPTIONS, "savefile.options");

  
  secfile_insert_int(file, game.gold, "game.gold");
  secfile_insert_int(file, game.tech, "game.tech");
  secfile_insert_int(file, game.skill_level, "game.skill_level");
  secfile_insert_int(file, game.timeout, "game.timeout");
  secfile_insert_int(file, game.end_year, "game.end_year");
  secfile_insert_int(file, game.year, "game.year");
  secfile_insert_int(file, game.techlevel, "game.techlevel");
  secfile_insert_int(file, game.min_players, "game.min_players");
  secfile_insert_int(file, game.max_players, "game.max_players");
  secfile_insert_int(file, game.nplayers, "game.nplayers");
  secfile_insert_int(file, game.globalwarming, "game.globalwarming");
  secfile_insert_int(file, game.warminglevel, "game.warminglevel");
  secfile_insert_int(file, game.unhappysize, "game.unhappysize");
  secfile_insert_int(file, game.cityfactor, "game.cityfactor");
  secfile_insert_int(file, game.civilwarsize, "game.civilwarsize");
  secfile_insert_int(file, game.diplcost, "game.diplcost");
  secfile_insert_int(file, game.freecost, "game.freecost");
  secfile_insert_int(file, game.conquercost, "game.conquercost");
  secfile_insert_int(file, game.rail_food, "game.rail_food");
  secfile_insert_int(file, game.rail_prod, "game.rail_prod");
  secfile_insert_int(file, game.rail_trade, "game.rail_trade");
  secfile_insert_int(file, game.foodbox, "game.foodbox");
  secfile_insert_int(file, game.techpenalty, "game.techpenalty");
  secfile_insert_int(file, game.razechance, "game.razechance");
  secfile_insert_int(file, game.civstyle, "game.civstyle");
  secfile_insert_int(file, game.save_nturns, "game.save_nturns");
  secfile_insert_int(file, game.aifill, "game.aifill");
  secfile_insert_int(file, game.scorelog, "game.scorelog");
  secfile_insert_int(file, game.spacerace, "game.spacerace");
  secfile_insert_int(file, game.diplchance, "game.diplchance");
  secfile_insert_int(file, game.aqueductloss, "game.aqueductloss");
  secfile_insert_int(file, game.randseed, "game.randseed");
  secfile_insert_str(file, game.ruleset.techs, "game.ruleset.techs");
  secfile_insert_str(file, game.ruleset.units, "game.ruleset.units");
  secfile_insert_str(file, game.ruleset.buildings, "game.ruleset.buildings");

  if (1) {
    /* Now always save these, so the server options reflect the
     * actual values used at the start of the game.
     * The first two used to be saved as "map.xsize" and "map.ysize"
     * when PRE_GAME_STATE, but I'm standardizing on width,height --dwp
     */
    secfile_insert_int(file, map.xsize, "map.width");
    secfile_insert_int(file, map.ysize, "map.height");
    secfile_insert_int(file, game.settlers, "game.settlers");
    secfile_insert_int(file, game.explorer, "game.explorer");
    secfile_insert_int(file, map.seed, "map.seed");
    secfile_insert_int(file, map.landpercent, "map.landpercent");
    secfile_insert_int(file, map.riches, "map.riches");
    secfile_insert_int(file, map.swampsize, "map.swampsize");
    secfile_insert_int(file, map.deserts, "map.deserts");
    secfile_insert_int(file, map.riverlength, "map.riverlength");
    secfile_insert_int(file, map.mountains, "map.mountains");
    secfile_insert_int(file, map.forestsize, "map.forestsize");
    secfile_insert_int(file, map.huts, "map.huts");
    secfile_insert_int(file, map.generator, "map.generator");
  } 
  if (server_state==PRE_GAME_STATE) {
    return;
  } else {

    secfile_insert_int(file,iRandJ,"random.index_J");
    secfile_insert_int(file,iRandK,"random.index_K");
    secfile_insert_int(file,iRandX,"random.index_X");

    for(i=0;i<8;i++) {
      sprintf(name,"random.table%d",i);
      sprintf(temp,"%8X %8X %8X %8X %8X %8X %8X",RandomState[7*i],
              RandomState[7*i+1],RandomState[7*i+2],RandomState[7*i+3],
              RandomState[7*i+4],RandomState[7*i+5],RandomState[7*i+6]);
      secfile_insert_str(file,temp,name);
    }
     
  }

  calc_unit_ordering();
  
  for(i=0; i<game.nplayers; i++)
    player_save(&game.players[i], i, file);

  map_save(file);

}
