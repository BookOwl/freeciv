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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "capability.h"
#include "events.h"
#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "rand.h"
#include "registry.h"
#include "shared.h"
#include "support.h"
#include "tech.h"
#include "unit.h"
#include "version.h"

#include "cityhand.h" 
#include "citytools.h"
#include "cityturn.h"
#include "civserver.h"
#include "gamelog.h"
#include "maphand.h"
#include "sernet.h"
#include "settlers.h"
#include "spacerace.h"
#include "unitfunc.h" 
#include "unittools.h"

#include "aicity.h"
#include "aihand.h"
#include "aitech.h"
#include "aiunit.h"

#include "plrhand.h"

enum historian_type {
        HISTORIAN_RICHEST=0, 
        HISTORIAN_ADVANCED=1,
        HISTORIAN_MILITARY=2,
        HISTORIAN_HAPPIEST=3,
        HISTORIAN_LARGEST=4};

static char *historian_message[]={
    N_("Herodot's report on the RICHEST Civilizations in the World."),
    N_("Herodot's report on the most ADVANCED Civilizations in the World."),
    N_("Herodot's report on the most MILITARIZED Civilizations in the World."),
    N_("Herodot's report on the HAPPIEST Civilizations in the World."),
    N_("Herodot's report on the LARGEST Civilizations in the World.")
};

static void update_player_aliveness(struct player *pplayer);

struct player_score_entry {
  int idx;
  int value;
};

static int secompare(const void *a, const void *b)
{
  return (((const struct player_score_entry *)b)->value -
	  ((const struct player_score_entry *)a)->value);
}

static char *greatness[] = {
  N_("Magnificent"),  N_("Glorious"), N_("Great"), N_("Decent"),
  N_("Mediocre"), N_("Hilarious"), N_("Worthless"), N_("Pathetic"),
  N_("Useless"), "Useless", "Useless", "Useless", "Useless", "Useless"
};

static void historian_generic(enum historian_type which_news)
{
  int i,j=0;
  char buffer[4096];
  struct player_score_entry *size=
    fc_malloc(sizeof(struct player_score_entry)*game.nplayers);

  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].is_alive && !is_barbarian(&game.players[i])) {
      switch(which_news) {
      case HISTORIAN_RICHEST:
	size[j].value=game.players[i].economic.gold;
	break;
      case HISTORIAN_ADVANCED:
	size[j].value = (game.players[i].score.techs
			 +game.players[i].future_tech);
	break;
      case HISTORIAN_MILITARY:
	size[j].value=game.players[i].score.units;
	break;
      case HISTORIAN_HAPPIEST: 
	size[j].value=
	  ((game.players[i].score.happy-game.players[i].score.unhappy)*1000)
	  /(1+total_player_citizens(&game.players[i]));
	break;
      case HISTORIAN_LARGEST:
	size[j].value=total_player_citizens(&game.players[i]);
	break;
      }
      size[j].idx=i;
      j++;
    } /* else the player is dead or barbarian */
  }
  qsort(size, j, sizeof(struct player_score_entry), secompare);
  buffer[0]=0;
  for (i=0;i<j;i++) {
    cat_snprintf(buffer, sizeof(buffer),
		 _("%2d: The %s %s\n"), i+1, _(greatness[i]),
		 get_nation_name_plural(game.players[size[i].idx].nation));
  }
  free(size);
  page_player_generic(0, _("Historian Publishes!"),
		      _(historian_message[which_news]),
		      buffer, BROADCAST_EVENT);

}

static int nr_wonders(struct city *pcity)
{
  int i;
  int res=0;
  for (i=0;i<B_LAST;i++)
    if (is_wonder(i) && city_got_building(pcity, i))
      res++;
  return res;
}

void top_five_cities(struct player *pplayer)
{
  struct player_score_entry *size=
    fc_malloc(sizeof(struct player_score_entry)*5);
  int i;
  char buffer[4096];
  struct city *pcity;
  buffer[0]=0;
  for (i=0;i<5;i++) {
    size[i].value=0;
    size[i].idx=0;
  }
  for (i=0;i<game.nplayers;i++) {
    city_list_iterate(game.players[i].cities, pcity) {
      if ((pcity->size+nr_wonders(pcity)*5)>size[4].value) {
	size[4].value=pcity->size+nr_wonders(pcity)*5;
	size[4].idx=pcity->id;
	qsort(size, 5, sizeof(struct player_score_entry), secompare);
      }
    }
    city_list_iterate_end;
  }
  for (i=0;i<5;i++) {
    pcity=find_city_by_id(size[i].idx);
    if (pcity) { 
      cat_snprintf(buffer, sizeof(buffer),
		   _("%2d: The %s City of %s of size %d, with %d wonders\n"),
		   i+1, get_nation_name(city_owner(pcity)->nation),pcity->name, 
		   pcity->size, nr_wonders(pcity));
    }
  }
  free(size);
  page_player(pplayer, _("Traveler's Report:"),
	      _("The Five Greatest Cities in the World!"), buffer);
}

void wonders_of_the_world(struct player *pplayer)
{
  int i;
  struct city *pcity;
  char buffer[4096];
  buffer[0]=0;
  for (i=0;i<B_LAST;i++) {
    if(is_wonder(i) && game.global_wonders[i]) {
      if((pcity=find_city_by_id(game.global_wonders[i]))) {
	cat_snprintf(buffer, sizeof(buffer), _("%s in %s (%s)\n"),
		     get_imp_name_ex(pcity, i), pcity->name,
		     get_nation_name(game.players[pcity->owner].nation));
      } else {
	cat_snprintf(buffer, sizeof(buffer), _("%s has been DESTROYED\n"),
		     get_improvement_type(i)->name);
      }
    }
  }
  page_player(pplayer, _("Traveler's Report:"),
	      _("Wonders of the World"), buffer);
}

/**************************************************************************
...
**************************************************************************/

static int rank_population(struct player *pplayer)
{
  int basis=pplayer->score.population;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.population>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_population(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.population > pplayer->score.population &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_landarea(struct player *pplayer)
{
  int basis=pplayer->score.landarea;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.landarea>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_landarea(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.landarea > pplayer->score.landarea &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_settledarea(struct player *pplayer)
{
  int basis=pplayer->score.settledarea;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.settledarea>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_settledarea(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.settledarea > pplayer->score.settledarea &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_calc_research(struct player *pplayer)
{
  return (pplayer->score.techout*100)/(research_time(pplayer));
}

static int rank_research(struct player *pplayer)
{
  int basis=rank_calc_research(pplayer);
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (rank_calc_research(&game.players[i])>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_research(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(rank_calc_research(&game.players[i]) > rank_calc_research(pplayer) &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_calc_literacy(struct player *pplayer)
{
  int pop = civ_population(pplayer);

  if (pop <= 0) {
    return 0;
  } else if (pop >= 10000) {
    return pplayer->score.literacy/(pop/100);
  } else {
    return (pplayer->score.literacy*100)/pop;
  }
}

static int rank_literacy(struct player *pplayer)
{
  int basis=rank_calc_literacy(pplayer);
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (rank_calc_literacy(&game.players[i])>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_literacy(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(rank_calc_literacy(&game.players[i]) > rank_calc_literacy(pplayer) &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_production(struct player *pplayer)
{
  int basis=pplayer->score.mfg;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.mfg>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_production(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.mfg > pplayer->score.mfg &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_economics(struct player *pplayer)
{
  int basis=pplayer->score.bnp;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.bnp>basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_economics(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.bnp > pplayer->score.bnp &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_pollution(struct player *pplayer)
{
  int basis=pplayer->score.pollution;
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (game.players[i].score.pollution<basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_pollution(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(game.players[i].score.pollution < pplayer->score.pollution &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static int rank_calc_mil_service(struct player *pplayer)
{
 return (pplayer->score.units*50000)/(100+civ_population(pplayer)/100);
}

static int rank_mil_service(struct player *pplayer)
{
  int basis=rank_calc_mil_service(pplayer);
  int place=1;
  int i;
  for (i=0;i<game.nplayers;i++) {
    if (rank_calc_mil_service(&game.players[i])<basis &&
	!is_barbarian(&game.players[i]))
      place++;
  }
  return place;
}

static struct player *best_mil_service(void)
{
  struct player *pplayer = &game.players[0];
  int i;
  for(i = 1; i < game.nplayers; i++) {
    if(rank_calc_mil_service(&game.players[i]) < rank_calc_mil_service(pplayer) &&
       !is_barbarian(&game.players[i])) {
      pplayer = &game.players[i];
    }
  }
  return pplayer;
}

static char *value_units(char *val, char *uni)
{
  static char buf[64] = "??";

  if ((strlen (val) + strlen (uni) + 1) > sizeof (buf))
    {
      return (buf);
    }

  my_snprintf(buf, sizeof(buf), "%s%s", val, uni);

  return (buf);
}

static char *number_to_ordinal_string(int num, int parens)
{
  static char buf[16];
  char *fmt;

  fmt = parens ? "(%d%s)" : "%d%s";

  switch (num)
    {
    case 1:
      my_snprintf(buf, sizeof(buf), fmt, num, _("st"));
      break;
    case 2:
      my_snprintf(buf, sizeof(buf), fmt, num, _("nd"));
      break;
    case 3:
      my_snprintf(buf, sizeof(buf), fmt, num, _("rd"));
      break;
    default:
      if (num > 0)
	{
	  my_snprintf(buf, sizeof(buf), fmt, num, _("th"));
	}
      else
	{
	  my_snprintf(buf, sizeof(buf),
		      (parens ? "(%s%s)" : "%s%s"), "??", _("th"));
	}
      break;
    }

  return (buf);
}

void do_dipl_cost(struct player *pplayer)
{
  pplayer->research.researched -=(research_time(pplayer)*game.diplcost)/100;
}
void do_free_cost(struct player *pplayer)
{
  pplayer->research.researched -=(research_time(pplayer)*game.freecost)/100;
}
void do_conquer_cost(struct player *pplayer)
{
  pplayer->research.researched -=(research_time(pplayer)*game.conquercost)/100;
}

/**************************************************************************
...
**************************************************************************/

#define DEM_KEY_ROW_POPULATION        'N'
#define DEM_KEY_ROW_LAND_AREA         'A'
#define DEM_KEY_ROW_SETTLED_AREA      'S'
#define DEM_KEY_ROW_RESEARCH_SPEED    'R'
#define DEM_KEY_ROW_LITERACY          'L'
#define DEM_KEY_ROW_PRODUCTION        'P'
#define DEM_KEY_ROW_ECONOMICS         'E'
#define DEM_KEY_ROW_MILITARY_SERVICE  'M'
#define DEM_KEY_ROW_POLLUTION         'O'
#define DEM_KEY_COL_QUANTITY          'q'
#define DEM_KEY_COL_RANK              'r'
#define DEM_KEY_COL_BEST              'b'

enum dem_flag
{
  DEM_NONE = 0x00,
  DEM_COL_QUANTITY = 0x01,
  DEM_COL_RANK = 0x02,
  DEM_COL_BEST = 0x04,
  DEM_ROW = 0xFF
};

struct dem_key
{
  char key;
  char *name;
  enum dem_flag flag;
};

static void dem_line_item(char *outptr, int nleft, struct player *pplayer,
			  char key, enum dem_flag selcols)
{
  static char *fmt_quan = " %-18s";
  static char *fmt_rank = " %6s";
  static char *fmt_best = "   %s: %s";
  struct player *best_player;

  switch (key)
    {
    case DEM_KEY_ROW_POPULATION:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       int_to_text (pplayer->score.population));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_population(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_population())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       int_to_text(best_player->score.population) );
        }
      break;
    case DEM_KEY_ROW_LAND_AREA:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (pplayer->score.landarea),
				    _(" sq. mi.")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_landarea(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_landarea())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units (int_to_text(best_player->score.landarea),
				    _(" sq. mi.")));
        }
      break;
    case DEM_KEY_ROW_SETTLED_AREA:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (pplayer->score.settledarea),
				    _(" sq. mi.")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_settledarea(pplayer),
						TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_settledarea())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units (int_to_text(best_player->score.settledarea),
				    _(" sq. mi.")));
        }
      break;
    case DEM_KEY_ROW_RESEARCH_SPEED:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (rank_calc_research(pplayer)),
				    "%"));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_research(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_research())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units(int_to_text(rank_calc_research(best_player)),
				   "%"));
        }
      break;
    case DEM_KEY_ROW_LITERACY:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (rank_calc_literacy(pplayer)),
				    "%"));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_literacy(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_literacy())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units(int_to_text(rank_calc_literacy(best_player)),
				   "%"));
        }
      break;
    case DEM_KEY_ROW_PRODUCTION:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (pplayer->score.mfg),
				    _(" M tons")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_production(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_production())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units(int_to_text(best_player->score.mfg),
				   _(" M tons")));
        }
      break;
    case DEM_KEY_ROW_ECONOMICS:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (pplayer->score.bnp),
				    _(" M goods")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_economics(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_economics())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units(int_to_text(best_player->score.bnp),
				   _(" M goods")));
        }
      break;
    case DEM_KEY_ROW_MILITARY_SERVICE:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (rank_calc_mil_service(pplayer)),
				    _(" months")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_mil_service(pplayer),
						TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_mil_service())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units(int_to_text(rank_calc_mil_service(best_player)),
				   _(" months")));
        }
      break;
    case DEM_KEY_ROW_POLLUTION:
      if (selcols & DEM_COL_QUANTITY)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_quan,
		       value_units (int_to_text (pplayer->score.pollution),
				    _(" tons")));
	}
      if (selcols & DEM_COL_RANK)
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_rank,
		       number_to_ordinal_string(rank_pollution(pplayer), TRUE));
	}
      if (selcols & DEM_COL_BEST && 
          player_has_embassy(pplayer, (best_player = best_pollution())) )
        {
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_best,
		       get_nation_name_plural(best_player->nation),
		       value_units (int_to_text(best_player->score.pollution),
				    _(" tons")));
        }
      break;
    }
}

/*************************************************************************/

void demographics_report(struct player *pplayer)
{
  char civbuf[1024];
  char buffer[4096] = "";
  int inx;
  int anyrows;
  enum dem_flag selcols;
  char *outptr = buffer;
  int nleft = sizeof(buffer);
  static char *fmt_name = "%-18s";
  static struct dem_key keytable[] =
  {
    { DEM_KEY_ROW_POPULATION,       N_("Population"),       DEM_ROW },
    { DEM_KEY_ROW_LAND_AREA,        N_("Land Area"),        DEM_ROW },
    { DEM_KEY_ROW_SETTLED_AREA,     N_("Settled Area"),     DEM_ROW },
    { DEM_KEY_ROW_RESEARCH_SPEED,   N_("Research Speed"),   DEM_ROW },
    { DEM_KEY_ROW_LITERACY,         N_("Literacy"),         DEM_ROW },
    { DEM_KEY_ROW_PRODUCTION,       N_("Production"),       DEM_ROW },
    { DEM_KEY_ROW_ECONOMICS,        N_("Economics"),        DEM_ROW },
    { DEM_KEY_ROW_MILITARY_SERVICE, N_("Military Service"), DEM_ROW },
    { DEM_KEY_ROW_POLLUTION,        N_("Pollution"),        DEM_ROW },
    { DEM_KEY_COL_QUANTITY,         "",                     DEM_COL_QUANTITY },
    { DEM_KEY_COL_RANK,             "",                     DEM_COL_RANK },
    { DEM_KEY_COL_BEST,             "",                     DEM_COL_BEST }
  };

  anyrows = FALSE;
  selcols = DEM_NONE;
  for (inx = 0; inx < (sizeof (keytable) / sizeof (keytable[0])); inx++)
    {
      if (strchr (game.demography, keytable[inx].key))
	{
	  if (keytable[inx].flag == DEM_ROW)
	    {
	      anyrows = TRUE;
	    }
	  else
	    {
	      selcols |= keytable[inx].flag;
	    }
	}
    }

  if (!anyrows || (selcols == DEM_NONE))
    {
      page_player (pplayer,
		   _("Demographics Report:"),
		   _("Sorry, the Demographics report is unavailable."),
		   "");
      return;
    }

  my_snprintf (civbuf, sizeof(civbuf), _("The %s of the %s"),
	       get_government_name (pplayer->government),
	       get_nation_name_plural (pplayer->nation));

  for (inx = 0; inx < (sizeof (keytable) / sizeof (keytable[0])); inx++)
    {
      if ((strchr (game.demography, keytable[inx].key)) &&
	  (keytable[inx].flag == DEM_ROW))
	{
	  outptr = end_of_strn (outptr, &nleft);
	  my_snprintf (outptr, nleft, fmt_name, _(keytable[inx].name));
	  outptr = end_of_strn (outptr, &nleft);
	  dem_line_item (outptr, nleft, pplayer, keytable[inx].key, selcols);
	  outptr = end_of_strn (outptr, &nleft);
	  mystrlcpy (outptr, "\n", nleft);
	}
    }

  page_player (pplayer, _("Demographics Report:"), civbuf, buffer);
}

/**************************************************************************
Create a log file of the civilizations so you can see what was happening.
**************************************************************************/

static void log_civ_score(void)
{ 
  int fom; /* fom == figure-of-merit */
  char *ptr;
  char line[64];
  int i, n, ln, ni;
  int index, dummy;
  char name[64];
  int foms = 0;
  int plrs = -1;
  enum { SL_CREATE, SL_APPEND, SL_UNSPEC } oper = SL_UNSPEC;

  static char *magic = "#FREECIV SCORELOG %s\n";
  static char *logname = "civscore.log";
  static char *endmark = "end";
  static FILE *fp = NULL;
  static int disabled = 0;

  /* add new tags only at end of this list;
     maintaining the order of old tags is critical */
  static char *tags[] =
  {
    "pop",
    "bnp",
    "mfg",
    "cities",
    "techs",
    "munits",
    "settlers",     /* "original" tags end here */

    "wonders",
    "techout",
    "landarea",
    "settledarea",
    "pollution",
    "literacy",
    "spaceship",    /* new 1.8.2 tags end here */

    NULL            /* end of list */
  };

  if (disabled)
    {
      return;
    }

  if (!fp)
    {
      if (game.year == GAME_START_YEAR)
	{
	  oper = SL_CREATE;
	}
      else
	{
	  fp = fopen (logname, "r");
	  if (!fp)
	    {
	      oper = SL_CREATE;
	    }
	  else
	    {
	      for (ln = 1; ; ln++)
		{
		  if (!(fgets (line, sizeof (line), fp)))
		    {
		      if (ferror (fp))
			{
			  freelog (LOG_NORMAL,
				   "Can't read scorelog file header!");
			}
		      else
			{
			  freelog (LOG_NORMAL,
				   "Unterminated scorelog file header!");
			}
		      goto log_civ_score_disable;
		    }

		  ptr = strchr (line, '\n');
		  if (!ptr)
		    {
		      freelog (LOG_NORMAL,
			       "Scorelog file line %d is too long!", ln);
		      goto log_civ_score_disable;
		    }
		  *ptr = '\0';

		  if (plrs < 0)
		    {
		      if ((ln == 1) && (line[0] == '#'))
			{
			  continue;
			}

		      if (0 == strcmp (line, endmark))
			{
			  plrs++;
			}
		      else
			{
			  if (!(tags[foms]))
			    {
			      freelog (LOG_NORMAL,
				       "Too many entries in scorelog header!");
			      goto log_civ_score_disable;
			    }

			  ni = sscanf (line, "%d %s", &index, name);
			  if ((ni != 2) || (index != foms) ||
			      (0 != strcmp (name, tags[foms])))
			    {
			      freelog (LOG_NORMAL,
				       "Scorelog file line %d is bad!", ln);
			      goto log_civ_score_disable;
			    }

			  foms++;
			}
		    }
		  else
		    {
		      if (0 == strcmp (line, endmark))
			{
			  break;
			}

		      ni = sscanf (line, "%d %s", &index, name);
		      if ((ni != 2) || (index != plrs))
			{
			  freelog (LOG_NORMAL,
				   "Scorelog file line %d is bad!", ln);
			  goto log_civ_score_disable;
			}
		      if (0 != strcmp (name, game.players[plrs].name))
			{
			  oper = SL_CREATE;
			  break;
			}

		      plrs++;
		    }
		}

	      if (oper == SL_UNSPEC)
		{
		  if (fseek (fp, -100, SEEK_END))
		    {
		      freelog (LOG_NORMAL,
			       "Can't seek to end of scorelog file!");
		      goto log_civ_score_disable;
		    }

		  if (!(fgets (line, sizeof (line), fp)))
		    {
		      if (ferror (fp))
			{
			  freelog (LOG_NORMAL,
				   "Can't read scorelog file!");
			}
		      else
			{
			  freelog (LOG_NORMAL,
				   "Unterminated scorelog file!");
			}
		      goto log_civ_score_disable;
		    }
		  ptr = strchr (line, '\n');
		  if (!ptr)
		    {
		      freelog (LOG_NORMAL,
			       "Scorelog file line is too long!");
		      goto log_civ_score_disable;
		    }
		  *ptr = '\0';

		  if (!(fgets (line, sizeof (line), fp)))
		    {
		      if (ferror (fp))
			{
			  freelog (LOG_NORMAL,
				   "Can't read scorelog file!");
			}
		      else
			{
			  freelog (LOG_NORMAL,
				   "Unterminated scorelog file!");
			}
		      goto log_civ_score_disable;
		    }
		  ptr = strchr (line, '\n');
		  if (!ptr)
		    {
		      freelog (LOG_NORMAL,
			       "Scorelog file line is too long!");
		      goto log_civ_score_disable;
		    }
		  *ptr = '\0';

		  ni = sscanf (line, "%d %d %d %d",
			       &dummy, &dummy, &index, &dummy);
		  if (ni != 4)
		    {
		      freelog (LOG_NORMAL,
			       "Scorelog file line is bad!");
		      goto log_civ_score_disable;
		    }

		  if (index >= game.year)
		    {
		      freelog (LOG_NORMAL,
			       "Scorelog years overlap -- logging disabled!");
		      goto log_civ_score_disable;
		    }

		  tags[foms] = NULL;
		  oper = SL_APPEND;
		}

	      fclose (fp);
	      fp = NULL;
	    }
	}

      switch (oper)
	{
	case SL_CREATE:
	  fp = fopen (logname, "w");
	  if (!fp)
	    {
	      freelog (LOG_NORMAL, "Can't open scorelog file for creation!");
	      goto log_civ_score_disable;
	    }
	  fprintf (fp, magic, VERSION_STRING);
	  for (i = 0; tags[i]; i++)
	    {
	      fprintf (fp, "%d %s\n", i, tags[i]);
	    }
	  fprintf (fp, "%s\n", endmark);
	  for (n = 0; n < game.nplayers; n++)
	    {
	      if (is_barbarian (&(game.players[n])))
		{
		  continue;
		}
	      fprintf (fp, "%d %s\n", n, game.players[n].name);
	    }
	  fprintf (fp, "%s\n", endmark);
	  break;
	case SL_APPEND:
	  fp = fopen (logname, "a");
	  if (!fp)
	    {
	      freelog (LOG_NORMAL, "Can't open scorelog file for appending!");
	      goto log_civ_score_disable;
	    }
	  break;
	default:
	  freelog (LOG_VERBOSE, "log_civ_score: bad operation");
	  goto log_civ_score_disable;
	}
    }

  for (i = 0; tags[i]; i++)
    {
      for (n = 0; n < game.nplayers; n++)
	{
	  if (is_barbarian (&(game.players[n])))
	    {
	      continue;
	    }
	  switch (i)
	    {
	    case 0:
	      fom = total_player_citizens (&(game.players[n]));
	      break;
	    case 1:
	      fom = game.players[n].score.bnp;
	      break;
	    case 2:
	      fom = game.players[n].score.mfg;
	      break;
	    case 3:
	      fom = game.players[n].score.cities;
	      break;
	    case 4:
	      fom = game.players[n].score.techs;
	      break;
	    case 5:
	      fom = 0;
	      /* count up military units */
	      unit_list_iterate (game.players[n].units, punit)
		if (is_military_unit (punit))
		  fom++;
	      unit_list_iterate_end;
	      break;
	    case 6:
	      fom = 0;
	      /* count up settlers */
	      unit_list_iterate (game.players[n].units, punit)
		if (unit_flag (punit->type, F_CITIES))
		  fom++;
	      unit_list_iterate_end;
	      break;
	    case 7:
	      fom = game.players[n].score.wonders;
	      break;
	    case 8:
	      fom = game.players[n].score.techout;
	      break;
	    case 9:
	      fom = game.players[n].score.landarea;
	      break;
	    case 10:
	      fom = game.players[n].score.settledarea;
	      break;
	    case 11:
	      fom = game.players[n].score.pollution;
	      break;
	    case 12:
	      fom = game.players[n].score.literacy;
	      break;
	    case 13:
	      fom = game.players[n].score.spaceship;
	      break;
	    default:
	      fom = 0; /* -Wall demands we init this somewhere! */
	    }

	  fprintf (fp, "%d %d %d %d\n", i, n, game.year, fom);
	}
    }

  fflush (fp);

  return;

log_civ_score_disable:

  if (fp)
    {
      fclose (fp);
      fp = NULL;
    }

  disabled = 1;
}

/**************************************************************************
...
**************************************************************************/

void make_history_report(void)
{
  static int report=0;
  static int time_to_report=20;
  int i;

  for (i=0;i<game.nplayers;i++)
    civ_score(&game.players[i]);

  if (game.scorelog)
    log_civ_score();

  if (game.nplayers==1)
    return;

  time_to_report--;

  if (time_to_report>0) 
    return;

  time_to_report=myrand(20)+20;

  historian_generic(report);
  
  report=(report+1)%5;
}

/**************************************************************************
...
**************************************************************************/

void show_ending(void)
{
  int i,j=0;
  char buffer[4096];

  struct player_score_entry *size=
    fc_malloc(sizeof(struct player_score_entry)*game.nplayers);

  for (i=0;i<game.nplayers;i++) {
    size[i].value=civ_score(&game.players[i]);
    size[i].idx=i;
  }
  qsort(size, game.nplayers, sizeof(struct player_score_entry), secompare);
  buffer[0]=0;
  for (i=0;i<game.nplayers;i++) {
    if( !is_barbarian(&game.players[size[i].idx]) ) {
      cat_snprintf(buffer, sizeof(buffer),
		   _("%2d: The %s %s scored %d points\n"), j+1, _(greatness[j]),
		   get_nation_name_plural(game.players[size[i].idx].nation),
		   size[i].value);
      j++;
    }
  }
  free(size);
  page_player(0, _("Final Report:"),
	      _("The Greatest Civilizations in the world."), buffer);

}


/**************************************************************************
...
**************************************************************************/

static void great_library(struct player *pplayer)
{
  int i;
  if (wonder_obsolete(B_GREAT)) 
    return;
  if (find_city_wonder(B_GREAT)) {
    if (pplayer->player_no==find_city_wonder(B_GREAT)->owner) {
      for (i=0;i<game.num_tech_types;i++) {
	if (get_invention(pplayer, i)!=TECH_KNOWN 
	    && game.global_advances[i]>=2) {
	  notify_player_ex(pplayer,0,0, E_TECH_GAIN,
			   _("Game: %s acquired from The Great Library!"),
			   advances[i].name);
	  gamelog(GAMELOG_TECH,"%s discover %s (Library)",
		  get_nation_name_plural(pplayer->nation),advances[i].name);
	  notify_embassies(pplayer,(struct player*)0,
			   _("Game: The %s have aquired %s from the Great Library"),
			   get_nation_name_plural(pplayer->nation),
			   advances[i].name);

	  do_free_cost(pplayer);
	  found_new_tech(pplayer,i,0,0);
	  break;
	}
      }
    }
  }
}


/**************************************************************************
Count down if the player are in a revolution, notify him when revolution
has ended.
**************************************************************************/

static void update_revolution(struct player *pplayer)
{
  if(pplayer->revolution) 
    pplayer->revolution--;
}

/**************************************************************************
Main update loop, for each player at end of turn.
**************************************************************************/

void update_player_activities(struct player *pplayer) 
{
  if (pplayer->ai.control) {
    ai_do_last_activities(pplayer); /* why was this AFTER aliveness? */
  }
  notify_player(pplayer, _("Year: %s"), textyear(game.year));
  great_library(pplayer);
  update_revolution(pplayer);
  player_restore_units(pplayer);
  update_city_activities(pplayer);
#ifdef CITIES_PROVIDE_RESEARCH
  if (city_list_size(&pplayer->cities)) /* has to be below the above for got_tech */ 
    update_tech(pplayer, city_list_size(&pplayer->cities));
#endif
  update_unit_activities(pplayer);
  update_player_aliveness(pplayer);
}

/**************************************************************************
...
**************************************************************************/

static void update_player_aliveness(struct player *pplayer)
{
  if(pplayer->is_alive) {
    if(unit_list_size(&pplayer->units)==0 && 
       city_list_size(&pplayer->cities)==0) {
      pplayer->is_alive=0;
      if( !is_barbarian(pplayer) ) {
        notify_player_ex(0, 0,0, E_DESTROYED, _("Game: The %s are no more!"), 
		         get_nation_name_plural(pplayer->nation));
        gamelog(GAMELOG_GENO, "%s civilization destroyed",
                get_nation_name(pplayer->nation));
      }
      map_know_all(pplayer);
      send_all_known_tiles(pplayer);
    }
  }
}

/**************************************************************************
Player has a new technology (from somewhere)
was_discovery is passed on to upgrade_city_rails
Logging & notification is not done here as it depends on how the tech came.
**************************************************************************/

void found_new_tech(struct player *plr, int tech_found, char was_discovery,
		    char saving_bulbs)
{
  int philohack=0;
  int was_first=0;
  int saved_bulbs;

  plr->got_tech=1;
  plr->research.researchpoints++;
  was_first = !game.global_advances[tech_found];
  if (was_first)
    gamelog(GAMELOG_TECH,_("%s are first to learn %s"),
	    get_nation_name_plural(plr->nation),
	    advances[tech_found].name
	    );
    
  if (tech_found==game.rtech.get_bonus_tech && was_first)
    philohack=1;

  set_invention(plr, tech_found, TECH_KNOWN);
  update_research(plr);
  remove_obsolete_buildings(plr);
  if (tech_flag(tech_found,TF_RAILROAD)) {
    upgrade_city_rails(plr, was_discovery);
  }

  if (tech_found==plr->ai.tech_goal)
    plr->ai.tech_goal=A_NONE;

  if (tech_found==plr->research.researching) {
    /* need to pick new tech to research */

    saved_bulbs=plr->research.researched;

    if (choose_goal_tech(plr)) {
      notify_player(plr, 
		    _("Game: Learned %s.  "
		      "Our scientists focus on %s, goal is %s."),
		    advances[tech_found].name, 
		    advances[plr->research.researching].name,
		    advances[plr->ai.tech_goal].name);
    } else {
      choose_random_tech(plr);
      if (plr->research.researching!=A_NONE && tech_found != A_NONE)
	notify_player(plr,
		     _("Game: Learned %s.  Scientists choose to research %s."),
		     advances[tech_found].name,
		     advances[plr->research.researching].name);
      else if (tech_found != A_NONE)
	notify_player(plr,
		      _("Game: Learned %s.  Scientists choose to research "
			"Future Tech. 1."),
		      advances[tech_found].name);
      else {
	plr->future_tech++;
	notify_player(plr,
		      _("Game: Learned Future Tech. %d.  "
			"Researching Future Tech. %d."),
		      plr->future_tech,(plr->future_tech)+1);
      }
    }
    if(saving_bulbs)
      plr->research.researched=saved_bulbs;
  }

  if (philohack) {
    notify_player(plr, _("Game: Great philosophers from all the world join "
			 "your civilization; you get an immediate advance."));
    tech_researched(plr);
  }
}

/**************************************************************************
Player has researched a new technology
**************************************************************************/

void tech_researched(struct player* plr)
{
  /* plr will be notified when new tech is chosen */

  if (plr->research.researching != A_NONE) {
    notify_embassies(plr, (struct player*)0,
		     _("Game: The %s have researched %s."), 
		     get_nation_name_plural(plr->nation),
		     advances[plr->research.researching].name);

    gamelog(GAMELOG_TECH,_("%s discover %s"),
	    get_nation_name_plural(plr->nation),
	    advances[plr->research.researching].name
	    );
  } else {
    notify_embassies(plr, (struct player*)0,
		     _("Game: The %s have researched Future Tech. %d."), 
		     get_nation_name_plural(plr->nation),
		     plr->future_tech);
  
    gamelog(GAMELOG_TECH,_("%s discover Future Tech %d"),
	    get_nation_name_plural(plr->nation),
	    plr->future_tech
	    );
  }

  /* do all the updates needed after finding new tech */
  found_new_tech(plr, plr->research.researching, 1, 0);
}

/**************************************************************************
Called from each city to update the research.
**************************************************************************/

int update_tech(struct player *plr, int bulbs)
{
  plr->research.researched+=bulbs;
  if (plr->research.researched < research_time(plr)) {
    return 0;
  } else {
    
    tech_researched( plr );
    
    return 1;
  }
}

int choose_goal_tech(struct player *plr)
{
  int sub_goal;

  if (plr->research.researched >0)
    plr->research.researched = 0;
  update_research(plr);
  if (plr->ai.control) {
    ai_next_tech_goal(plr); /* tech-AI has been changed */
    sub_goal = get_next_tech(plr, plr->ai.tech_goal); /* should be changed */
  } else sub_goal = get_next_tech(plr, plr->ai.tech_goal);
  if (!sub_goal) {
    if (plr->ai.control || plr->research.researchpoints == 1) {
      ai_next_tech_goal(plr);
      sub_goal = get_next_tech(plr, plr->ai.tech_goal);
    } else {
      plr->ai.tech_goal = A_NONE; /* clear goal when it is achieved */
    }
  }

  if (sub_goal) {
    plr->research.researching=sub_goal;
  }   
  return sub_goal;
}


/**************************************************************************
...
**************************************************************************/

void choose_random_tech(struct player *plr)
{
  int researchable=0;
  int i;
  int choosen;
  if (plr->research.researched >0)
    plr->research.researched = 0;
  update_research(plr);
  for (i=0;i<game.num_tech_types;i++)
    if (get_invention(plr, i)==TECH_REACHABLE) 
      researchable++;
  if (researchable==0) { 
    plr->research.researching=A_NONE;
    return;
  }
  choosen=myrand(researchable)+1;
  
  for (i=0;i<game.num_tech_types;i++)
    if (get_invention(plr, i)==TECH_REACHABLE) {
      choosen--;
      if (!choosen) break;
    }
  plr->research.researching=i;
}

/**************************************************************************
...
**************************************************************************/

void choose_tech(struct player *plr, int tech)
{
  if (plr->research.researching==tech)
    return;
  update_research(plr);
  if (get_invention(plr, tech)!=TECH_REACHABLE) { /* can't research this */
    return;
  }
  plr->research.researching=tech;
  if (!plr->got_tech && plr->research.researched > 0)
    plr->research.researched -= ((plr->research.researched * game.techpenalty) / 100);     /* subtract a penalty because we changed subject */
}

void choose_tech_goal(struct player *plr, int tech)
{
  notify_player(plr, _("Game: Technology goal is %s."), advances[tech].name);
  plr->ai.tech_goal = tech;
}

/**************************************************************************
...
**************************************************************************/

void init_tech(struct player *plr, int tech)
{
  int i;
  for (i=0;i<game.num_tech_types;i++) 
    set_invention(plr, i, 0);
  set_invention(plr, A_NONE, TECH_KNOWN);

  plr->research.researchpoints=1;
  for (i=0;i<tech;i++) {
    choose_random_tech(plr); /* could be choose_goal_tech -- Syela */
    set_invention(plr, plr->research.researching, TECH_KNOWN);
  }
  choose_goal_tech(plr);
  update_research(plr);	
}

/**************************************************************************
...
**************************************************************************/

void handle_player_rates(struct player *pplayer, 
                         struct packet_player_request *preq)
{
  int maxrate;

  if (server_state!=RUN_GAME_STATE) {
    freelog(LOG_NORMAL, "received player_rates packet from %s before start",
	    pplayer->name);
    notify_player(pplayer, _("Game: Cannot change rates before game start."));
    return;
  }
	
  if (preq->tax+preq->luxury+preq->science!=100)
    return;
  if (preq->tax<0 || preq->tax >100) return;
  if (preq->luxury<0 || preq->luxury > 100) return;
  if (preq->science<0 || preq->science >100) return;
  maxrate=get_government_max_rate (pplayer->government);
  if (preq->tax>maxrate || preq->luxury>maxrate || preq->science>maxrate) {
    char *rtype;
    if (preq->tax > maxrate)
      rtype = _("Tax");
    else if (preq->luxury > maxrate)
      rtype = _("Luxury");
    else
      rtype = _("Science");
    notify_player(pplayer, _("Game: %s rate exceeds the max rate for %s."),
                  rtype, get_government_name(pplayer->government));
  } else {
    pplayer->economic.tax=preq->tax;
    pplayer->economic.luxury=preq->luxury;
    pplayer->economic.science=preq->science;
    gamelog(GAMELOG_EVERYTHING, "RATE CHANGE: %s %i %i %i", 
	    get_nation_name_plural(pplayer->nation), preq->tax, 
	    preq->luxury, preq->science);
    connection_do_buffer(pplayer->conn);
    send_player_info(pplayer, pplayer);
    global_city_refresh(pplayer);
    connection_do_unbuffer(pplayer->conn);
  }
}

/**************************************************************************
...
**************************************************************************/

void handle_player_research(struct player *pplayer,
			    struct packet_player_request *preq)
{
  choose_tech(pplayer, preq->tech);
  send_player_info(pplayer, pplayer);
}

void handle_player_tech_goal(struct player *pplayer,
			    struct packet_player_request *preq)
{
  choose_tech_goal(pplayer, preq->tech);
  send_player_info(pplayer, pplayer);
}


void handle_player_worklist(struct player *pplayer,
			    struct packet_player_request *preq)
{
  copy_worklist(&pplayer->worklists[preq->wl_idx], &preq->worklist);
  send_player_info(pplayer, pplayer);
}


/**************************************************************************
...
**************************************************************************/
void handle_player_government(struct player *pplayer,
			     struct packet_player_request *preq)
{
  if( pplayer->government!=game.government_when_anarchy || 
     !can_change_to_government(pplayer, preq->government) 
    )
    return;

  if((pplayer->revolution<=5) && (pplayer->revolution>0))
    return;

  pplayer->government=preq->government;
  notify_player(pplayer, _("Game: %s now governs the %s as a %s."), 
		pplayer->name, 
  	        get_nation_name_plural(pplayer->nation),
		get_government_name(preq->government));  
  gamelog(GAMELOG_GOVERNMENT,"%s form a %s",
          get_nation_name_plural(pplayer->nation),
          get_government_name(preq->government));

  if (!pplayer->ai.control) {
    /* Keep luxuries if we have any.  Try to max out science. -GJW */
    pplayer->economic.science = MIN (100 - pplayer->economic.luxury,
				     get_government_max_rate (pplayer->government));
    pplayer->economic.tax = 100 - (pplayer->economic.luxury +
				   pplayer->economic.science);
  }

  check_player_government_rates(pplayer);

  send_player_info(pplayer, pplayer);
}

/**************************************************************************
...
**************************************************************************/

void handle_player_revolution(struct player *pplayer)
{
  if ((pplayer->revolution<=5) &&
      (pplayer->revolution>0) &&
      ( pplayer->government==game.government_when_anarchy))
    return;
  pplayer->revolution=myrand(5)+1;
  pplayer->government=game.government_when_anarchy;
  notify_player(pplayer, _("Game: The %s have incited a revolt!"), 
		get_nation_name_plural(pplayer->nation));
  gamelog(GAMELOG_REVOLT,"The %s revolt!",
                get_nation_name_plural(pplayer->nation));

  check_player_government_rates(pplayer);

  send_player_info(pplayer, pplayer);
  if (player_owns_active_govchange_wonder(pplayer))
    pplayer->revolution=1;
}

/**************************************************************************
The following checks that government rates are acceptable for the present
form of government. Has to be called when switching governments or when
toggling from AI to human.
**************************************************************************/
void check_player_government_rates(struct player *pplayer)
{
  struct player_economic old_econ = pplayer->economic;
  int changed = FALSE;

  player_limit_to_government_rates(pplayer);

  if (pplayer->economic.tax != old_econ.tax) {
    changed = TRUE;
    notify_player(pplayer,
		  _("Game: Tax rate exceeded the max rate for %s; adjusted."), 
		  get_government_name(pplayer->government));
  }
  if (pplayer->economic.science != old_econ.science) {
    changed = TRUE;
    notify_player(pplayer,
		  _("Game: Science rate exceeded the max rate for %s; adjusted."), 
		  get_government_name(pplayer->government));
  }
  if (pplayer->economic.luxury != old_econ.luxury) {
    changed = TRUE;
    notify_player(pplayer,
		  _("Game: Luxury rate exceeded the max rate for %s; adjusted."), 
		  get_government_name(pplayer->government));
  }

  if (changed) {
    gamelog(GAMELOG_EVERYTHING, "RATE CHANGE: %s %i %i %i",
	    get_nation_name_plural(pplayer->nation), pplayer->economic.tax,
	    pplayer->economic.luxury, pplayer->economic.science);
  }
}

/**************************************************************************
...
**************************************************************************/

void notify_player_ex(const struct player *pplayer, int x, int y,
		      int event, const char *format, ...) 
{
  int i;
  struct packet_generic_message genmsg;
  va_list args;
  va_start(args, format);
  my_vsnprintf(genmsg.message, sizeof(genmsg.message), format, args);
  va_end(args);
  genmsg.event = event;
  for(i=0; i<game.nplayers; i++)
    if(!pplayer || pplayer==&game.players[i]) {
      if (server_state >= RUN_GAME_STATE
	  && map_get_known(x, y, &game.players[i])) {
	genmsg.x = x;
	genmsg.y = y;
      } else {
	genmsg.x = 0;
	genmsg.y = 0;
      }
      send_packet_generic_message(game.players[i].conn, PACKET_CHAT_MSG, &genmsg);
    }
}


/**************************************************************************
...
**************************************************************************/
void notify_player(const struct player *pplayer, const char *format, ...) 
{
  int i;
  struct packet_generic_message genmsg;
  va_list args;
  va_start(args, format);
  my_vsnprintf(genmsg.message, sizeof(genmsg.message), format, args);
  va_end(args);
  genmsg.x = -1;
  genmsg.y = -1;
  genmsg.event = -1;
  for(i=0; i<game.nplayers; i++)
    if(!pplayer || pplayer==&game.players[i])
      send_packet_generic_message(game.players[i].conn, PACKET_CHAT_MSG, &genmsg);
}

/**************************************************************************
...
**************************************************************************/
void notify_embassies(struct player *pplayer, struct player *exclude,
		      const char *format, ...) 
{
  int i;
  struct packet_generic_message genmsg;
  va_list args;
  va_start(args, format);
  my_vsnprintf(genmsg.message, sizeof(genmsg.message), format, args);
  va_end(args);
  genmsg.x = -1;
  genmsg.y = -1;
  genmsg.event = -1;
  for(i=0; i<game.nplayers; i++)
    if(player_has_embassy(&game.players[i],pplayer)&&exclude!=&game.players[i])
      send_packet_generic_message(game.players[i].conn, PACKET_CHAT_MSG, &genmsg);
}

/**************************************************************************
This function pops up a non-modal message dialog on the player's desktop
**************************************************************************/
void page_player(struct player *pplayer, char *caption, char *headline,
		 char *lines) {
    page_player_generic(pplayer,caption,headline,lines,-1);
}


/**************************************************************************
This function pops up a non-modal message dialog on the player's desktop
event == -1: message should not be ignored by clients watching AI players with 
             ai_popup_windows off.  Example: Server Options, Demographics 
             Report, etc.
event == BROADCAST_EVENT: message can safely be ignored by clients watching AI
                          players with ai_popup_windows off.
         For example: Herodot's report... and similar messages.
**************************************************************************/
void page_player_generic(struct player *pplayer, char *caption, char *headline,
			 char *lines, int event) 
{
  int i;
  struct packet_generic_message genmsg;

  if(strlen(caption)+1+strlen(headline)+1+strlen(lines) >=
     sizeof(genmsg.message)) {
    freelog(LOG_NORMAL, _("Message too long in page_player_generic!!"));
    return;
  }
  my_snprintf(genmsg.message, sizeof(genmsg.message),
	      "%s\n%s\n%s", caption, headline, lines);
  genmsg.event = event;
  
  for(i=0; i<game.nplayers; i++)
    if(!pplayer || pplayer==&game.players[i])
      send_packet_generic_message(game.players[i].conn, PACKET_PAGE_MSG, 
				  &genmsg);
}

/**************************************************************************
both src and dest can be NULL
NULL means all players
**************************************************************************/
void send_player_info(struct player *src, struct player *dest)
{
  int o, i, j;

  for(o=0; o<game.nplayers; o++)           /* dests */
     if(!dest || &game.players[o]==dest)
        for(i=0; i<game.nplayers; i++)     /* srcs  */
           if(!src || &game.players[i]==src) {
             struct packet_player_info info;
             info.playerno=i;
             sz_strlcpy(info.name, game.players[i].name);
             info.nation=game.players[i].nation;
             info.is_male=game.players[i].is_male;

             info.gold=game.players[i].economic.gold;
             info.tax=game.players[i].economic.tax;
             info.science=game.players[i].economic.science;
             info.luxury=game.players[i].economic.luxury;
	     info.government=game.players[i].government;             
	     info.embassy=game.players[i].embassy;             
             info.city_style=game.players[i].city_style;

	     info.researched=game.players[i].research.researched;
             info.researchpoints=game.players[i].research.researchpoints;
             info.researching=game.players[i].research.researching;
             info.tech_goal=game.players[i].ai.tech_goal;
             for(j=0; j<game.num_tech_types; j++)
               info.inventions[j]=game.players[i].research.inventions[j]+'0';
             info.inventions[j]='\0';
             info.future_tech=game.players[i].future_tech;
             info.turn_done=game.players[i].turn_done;
             info.nturns_idle=game.players[i].nturns_idle;
             info.is_alive=game.players[i].is_alive;
             info.is_connected=game.players[i].is_connected;
             sz_strlcpy(info.addr, game.players[i].addr);
	     info.revolution=game.players[i].revolution;
	     info.ai=game.players[i].ai.control;
	     info.is_barbarian=game.players[i].ai.is_barbarian;
	     if(game.players[i].conn)
	       sz_strlcpy(info.capability,game.players[i].conn->capability);
	     
	     for (j = 0; j < MAX_NUM_WORKLISTS; j++)
	       copy_worklist(&info.worklists[j], 
			     &game.players[i].worklists[j]);

             send_packet_player_info(game.players[o].conn, &info);
	   }
}

/***************************************************************
...
***************************************************************/
void player_load(struct player *plr, int plrno, struct section_file *file)
{
  int i, j, x, y, nunits, ncities;
  char *p;
  char *savefile_options = " ";

  if ((game.version == 10604 && section_file_lookup(file,"savefile.options"))
      || (game.version > 10604))
    savefile_options = secfile_lookup_str(file,"savefile.options");  
  /* else leave savefile_options empty */
  
  /* Note -- as of v1.6.4 you should use savefile_options (instead of
     game.version) to determine which variables you can expect to 
     find in a savegame file.  Or alternatively you can use
     secfile_lookup_int_default() or secfile_lookup_str_default().
  */

  plr->ai.is_barbarian = secfile_lookup_int_default(file, 0, "player%d.ai.is_barbarian",
                                                    plrno);

  sz_strlcpy(plr->name, secfile_lookup_str(file, "player%d.name", plrno));
  sz_strlcpy(plr->username,
	     secfile_lookup_str_default(file, "", "player%d.username", plrno));
  plr->nation=secfile_lookup_int(file, "player%d.race", plrno);
  if (plr->ai.is_barbarian) {
    plr->nation=game.nation_count-1;
  }
  plr->government=secfile_lookup_int(file, "player%d.government", plrno);
  plr->embassy=secfile_lookup_int(file, "player%d.embassy", plrno);
  plr->city_style=secfile_lookup_int_default(file, get_nation_city_style(plr->nation),
                                             "player%d.city_style", plrno);
  for (j = 0; j < MAX_NUM_WORKLISTS; j++) {
    plr->worklists[j].is_valid = 
      secfile_lookup_int_default(file, 0, 
				 "player%d.worklist%d.is_valid", plrno, j);
    strcpy(plr->worklists[j].name, 
	   secfile_lookup_str_default(file, "",
				      "player%d.worklist%d.name", plrno, j));
    for (i = 0; i < MAX_LEN_WORKLIST; i++)
      plr->worklists[j].ids[i] = 
	secfile_lookup_int_default(file, WORKLIST_END,
				   "player%d.worklist%d.ids%d", plrno, j, i);
  }

  sz_strlcpy(plr->addr, "---.---.---.---");

  plr->nturns_idle=0;
  plr->is_male=secfile_lookup_int_default(file, 1, "player%d.is_male", plrno);
  plr->is_alive=secfile_lookup_int(file, "player%d.is_alive", plrno);
  plr->ai.control = secfile_lookup_int(file, "player%d.ai.control", plrno);
  plr->ai.tech_goal = secfile_lookup_int(file, "player%d.ai.tech_goal", plrno);
  plr->ai.handicap = 0;		/* set later */
  plr->ai.fuzzy = 0;		/* set later */
  plr->ai.expand = 100;		/* set later */
  plr->ai.skill_level =
    secfile_lookup_int_default(file, game.skill_level,
			       "player%d.ai.skill_level", plrno);
  if (plr->ai.control && plr->ai.skill_level==0) {
    plr->ai.skill_level = GAME_OLD_DEFAULT_SKILL_LEVEL;
  }

  plr->economic.gold=secfile_lookup_int(file, "player%d.gold", plrno);
  plr->economic.tax=secfile_lookup_int(file, "player%d.tax", plrno);
  plr->economic.science=secfile_lookup_int(file, "player%d.science", plrno);
  plr->economic.luxury=secfile_lookup_int(file, "player%d.luxury", plrno);

  plr->future_tech=secfile_lookup_int(file, "player%d.futuretech", plrno);

  plr->research.researched=secfile_lookup_int(file, 
					     "player%d.researched", plrno);
  plr->research.researchpoints=secfile_lookup_int(file, 
					     "player%d.researchpoints", plrno);
  plr->research.researching=secfile_lookup_int(file, 
					     "player%d.researching", plrno);

  p=secfile_lookup_str(file, "player%d.invs", plrno);
    
  plr->capital=secfile_lookup_int(file, "player%d.capital", plrno);

  for(i=0; i<game.num_tech_types; i++)
    set_invention(plr, i, (p[i]=='1') ? TECH_KNOWN : TECH_UNKNOWN);
  
  if (has_capability("spacerace", savefile_options)) {
    struct player_spaceship *ship = &plr->spaceship;
    char prefix[32];
    int arrival_year;
    
    my_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);
    spaceship_init(ship);
    arrival_year = secfile_lookup_int(file, "%s.arrival_year", prefix);
    ship->structurals = secfile_lookup_int(file, "%s.structurals", prefix);
    ship->components = secfile_lookup_int(file, "%s.components", prefix);
    ship->modules = secfile_lookup_int(file, "%s.modules", prefix);
    ship->state = secfile_lookup_int(file, "%s.state", prefix);
    if (ship->state >= SSHIP_LAUNCHED) {
      ship->launch_year = (arrival_year - 15);
    }
    /* auto-assign to individual parts: */
    ship->habitation = (ship->modules + 2)/3;
    ship->life_support = (ship->modules + 1)/3;
    ship->solar_panels = ship->modules/3;
    ship->fuel = (ship->components + 1)/2;
    ship->propulsion = ship->components/2;
    for(i=0; i<ship->structurals; i++) {
      ship->structure[i] = 1;
    }
    spaceship_calc_derived(ship);
  }
  else if(has_capability("spacerace2", savefile_options)) {
    struct player_spaceship *ship = &plr->spaceship;
    char prefix[32];
    char *st;
    
    my_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);
    spaceship_init(ship);
    ship->state = secfile_lookup_int(file, "%s.state", prefix);

    if (ship->state != SSHIP_NONE) {
      ship->structurals = secfile_lookup_int(file, "%s.structurals", prefix);
      ship->components = secfile_lookup_int(file, "%s.components", prefix);
      ship->modules = secfile_lookup_int(file, "%s.modules", prefix);
      ship->fuel = secfile_lookup_int(file, "%s.fuel", prefix);
      ship->propulsion = secfile_lookup_int(file, "%s.propulsion", prefix);
      ship->habitation = secfile_lookup_int(file, "%s.habitation", prefix);
      ship->life_support = secfile_lookup_int(file, "%s.life_support", prefix);
      ship->solar_panels = secfile_lookup_int(file, "%s.solar_panels", prefix);
      st = secfile_lookup_str(file, "%s.structure", prefix);
      for(i=0; i<NUM_SS_STRUCTURALS && *st; i++, st++) {
	ship->structure[i] = ((*st) == '1');
      }
      if (ship->state >= SSHIP_LAUNCHED) {
	ship->launch_year = secfile_lookup_int(file, "%s.launch_year", prefix);
      }
      spaceship_calc_derived(ship);
    }
  }

  city_list_init(&plr->cities);
  ncities=secfile_lookup_int(file, "player%d.ncities", plrno);
  
  /* this should def. be placed in city.c later - PU */
  for(i=0; i<ncities; i++) { /* read the cities */
    struct city *pcity;
    
    pcity=fc_malloc(sizeof(struct city));
    pcity->ai.ai_role = AICITY_NONE;
    pcity->ai.trade_want = TRADE_WEIGHTING;
    memset(pcity->ai.building_want, 0, sizeof(pcity->ai.building_want));
    pcity->ai.workremain = 1; /* there's always work to be done! */
    pcity->ai.danger = -1; /* flag, may come in handy later */
    pcity->corruption = 0;
    pcity->shield_bonus = 100;
    pcity->tax_bonus = 100;
    pcity->science_bonus = 100;
 
    pcity->id=secfile_lookup_int(file, "player%d.c%d.id", plrno, i);
    alloc_id(pcity->id);
    add_city_to_cache(pcity);
    pcity->owner=plrno;
    pcity->x=secfile_lookup_int(file, "player%d.c%d.x", plrno, i);
    pcity->y=secfile_lookup_int(file, "player%d.c%d.y", plrno, i);
    
    sz_strlcpy(pcity->name,
	       secfile_lookup_str(file, "player%d.c%d.name", plrno, i));
    if (section_file_lookup(file, "player%d.c%d.original", plrno, i))
      pcity->original = secfile_lookup_int(file, "player%d.c%d.original", 
					   plrno,i);
    else 
      pcity->original = plrno;
    pcity->size=secfile_lookup_int(file, "player%d.c%d.size", plrno, i);

    pcity->steal=secfile_lookup_int(file, "player%d.c%d.steal", plrno, i);

    pcity->ppl_elvis=secfile_lookup_int(file, "player%d.c%d.nelvis", plrno, i);
    pcity->ppl_scientist=secfile_lookup_int(file, 
					  "player%d.c%d.nscientist", plrno, i);
    pcity->ppl_taxman=secfile_lookup_int(file, "player%d.c%d.ntaxman",
					 plrno, i);

    for(j=0; j<4; j++)
      pcity->trade[j]=secfile_lookup_int(file, "player%d.c%d.traderoute%d",
					 plrno, i, j);
    
    pcity->food_stock=secfile_lookup_int(file, "player%d.c%d.food_stock", 
						 plrno, i);
    pcity->shield_stock=secfile_lookup_int(file, "player%d.c%d.shield_stock", 
						   plrno, i);
    pcity->tile_trade=pcity->trade_prod=0;
    pcity->anarchy=secfile_lookup_int(file, "player%d.c%d.anarchy", plrno,i);
    pcity->turn_last_built=secfile_lookup_int_default(file, -1,
						      "player%d.c%d.turn_last_built", plrno,i);
    pcity->turn_changed_target=secfile_lookup_int_default(file, -1,
							  "player%d.c%d.turn_changed_target", plrno,i);
    pcity->rapture=secfile_lookup_int_default(file, 0, "player%d.c%d.rapture", plrno,i);
    pcity->was_happy=secfile_lookup_int(file, "player%d.c%d.was_happy", plrno,i);
    pcity->is_building_unit=secfile_lookup_int(file, 
				    "player%d.c%d.is_building_unit", plrno, i);

    pcity->currently_building=secfile_lookup_int(file, 
						 "player%d.c%d.currently_building", plrno, i);

    pcity->did_buy=secfile_lookup_int(file,
				      "player%d.c%d.did_buy", plrno,i);
    pcity->did_sell =
      secfile_lookup_int_default(file, 0, "player%d.c%d.did_sell", plrno,i);
    
    if (game.version >=10300) 
      pcity->airlift=secfile_lookup_int(file,
					"player%d.c%d.airlift", plrno,i);
    else
      pcity->airlift=0;

    pcity->city_options =
      secfile_lookup_int_default(file, CITYOPT_DEFAULT,
				 "player%d.c%d.options", plrno, i);
    
    unit_list_init(&pcity->units_supported);

    /* Initialize pcity->city_map[][], using set_worker_city() so that
       ptile->worked gets initialized correctly.  The pre-initialisation
       to C_TILE_EMPTY is necessary because set_worker_city() accesses
       the existing value to possibly adjust ptile->worked, so need to
       initialize a non-worked value so ptile->worked (possibly already
       set from neighbouring city) does not get unset for C_TILE_EMPTY
       or C_TILE_UNAVAILABLE here.  -- dwp
    */
    p=secfile_lookup_str(file, "player%d.c%d.workers", plrno, i);
    for(y=0; y<CITY_MAP_SIZE; y++) {
      for(x=0; x<CITY_MAP_SIZE; x++) {
	pcity->city_map[x][y] = C_TILE_EMPTY;
        if (*p=='0') {
	  set_worker_city(pcity, x, y, C_TILE_EMPTY);
	} else if (*p=='1') {
	  if (map_get_tile(pcity->x+x-2, pcity->y+y-2)->worked) {
	    /* oops, inconsistent savegame; minimal fix: */
	    freelog(LOG_VERBOSE, "Inconsistent worked for %s (%d,%d), "
		    "converting to elvis", pcity->name, x, y);
	    pcity->ppl_elvis++;
	    set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	  } else {
	    set_worker_city(pcity, x, y, C_TILE_WORKER);
	  }
	} else {
	  set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	}
        p++;
      }
    }

    p=secfile_lookup_str(file, "player%d.c%d.improvements", plrno, i);
    
    for(x=0; x<B_LAST; x++)
      pcity->improvements[x]=(*p++=='1') ? 1 : 0;

    pcity->worklist = create_worklist();
    for(j=0; j<MAX_LEN_WORKLIST; j++)
      pcity->worklist->ids[j]=
	secfile_lookup_int_default(file, WORKLIST_END,
				   "player%d.c%d.worklist%d", plrno, i, j);

    map_set_city(pcity->x, pcity->y, pcity);

    pcity->incite_revolt_cost = -1; /* flag value */
    
    city_list_insert_back(&plr->cities, pcity);
  }

  unit_list_init(&plr->units);
  nunits=secfile_lookup_int(file, "player%d.nunits", plrno);
  
  /* this should def. be placed in unit.c later - PU */
  for(i=0; i<nunits; i++) { /* read the units */
    struct unit *punit;
    struct city *pcity;
    
    punit=fc_malloc(sizeof(struct unit));
    punit->id=secfile_lookup_int(file, "player%d.u%d.id", plrno, i);
    alloc_id(punit->id);
    punit->owner=plrno;
    punit->x=secfile_lookup_int(file, "player%d.u%d.x", plrno, i);
    punit->y=secfile_lookup_int(file, "player%d.u%d.y", plrno, i);

    punit->veteran=secfile_lookup_int(file, "player%d.u%d.veteran", plrno, i);
    punit->foul=secfile_lookup_int_default(file, 0, "player%d.u%d.foul",
					   plrno, i);
    punit->homecity=secfile_lookup_int(file, "player%d.u%d.homecity",
				       plrno, i);

    if((pcity=find_city_by_id(punit->homecity)))
      unit_list_insert(&pcity->units_supported, punit);
    else
      punit->homecity=0;
    
    punit->type=secfile_lookup_int(file, "player%d.u%d.type", plrno, i);

    punit->moves_left=secfile_lookup_int(file, "player%d.u%d.moves", plrno, i);
    punit->fuel= secfile_lookup_int(file, "player%d.u%d.fuel", plrno, i);
    set_unit_activity(punit, secfile_lookup_int(file, "player%d.u%d.activity",plrno, i));
/* need to do this to assign/deassign settlers correctly -- Syela */
/* was punit->activity=secfile_lookup_int(file, "player%d.u%d.activity",plrno, i); */
    punit->activity_count=secfile_lookup_int(file, 
					     "player%d.u%d.activity_count",
					     plrno, i);
    punit->activity_target=secfile_lookup_int_default(file, 0,
						"player%d.u%d.activity_target",
						plrno, i);

    punit->connecting=secfile_lookup_int_default(file, 0,
						"player%d.u%d.connecting",
						plrno, i);

    punit->goto_dest_x=secfile_lookup_int(file, 
					  "player%d.u%d.goto_x", plrno,i);
    punit->goto_dest_y=secfile_lookup_int(file, 
					  "player%d.u%d.goto_y", plrno,i);
    punit->ai.control=secfile_lookup_int(file, "player%d.u%d.ai", plrno,i);
    punit->ai.ai_role = AIUNIT_NONE;
    punit->ai.ferryboat = 0;
    punit->ai.passenger = 0;
    punit->ai.bodyguard = 0;
    punit->ai.charge = 0;
    punit->hp=secfile_lookup_int(file, "player%d.u%d.hp", plrno, i);
    punit->bribe_cost=-1;	/* flag value */
    
    punit->ord_map=secfile_lookup_int_default(file, 0, "player%d.u%d.ord_map",
					      plrno, i);
    punit->ord_city=secfile_lookup_int_default(file, 0, "player%d.u%d.ord_city",
					       plrno, i);
    punit->moved=secfile_lookup_int_default(file, 0, "player%d.u%d.moved",
					    plrno, i);
    punit->paradropped=secfile_lookup_int_default(file, 0, "player%d.u%d.paradropped",
                                                  plrno, i);

    unit_list_insert_back(&plr->units, punit);

    unit_list_insert(&map_get_tile(punit->x, punit->y)->units, punit);
  }

}

/***************************************************************
...
***************************************************************/
void player_save(struct player *plr, int plrno, struct section_file *file)
{
  int i, j;
  char invs[A_LAST+1];
  struct player_spaceship *ship = &plr->spaceship;

  secfile_insert_str(file, plr->name, "player%d.name", plrno);
  secfile_insert_str(file, plr->username, "player%d.username", plrno);
  secfile_insert_int(file, plr->nation, "player%d.race", plrno);
  secfile_insert_int(file, plr->government, "player%d.government", plrno);
  secfile_insert_int(file, plr->embassy, "player%d.embassy", plrno);
   
  for (j = 0; j < MAX_NUM_WORKLISTS; j++) {
    secfile_insert_int(file, plr->worklists[j].is_valid, 
		       "player%d.worklist%d.is_valid", plrno, j);
    secfile_insert_str(file, plr->worklists[j].name, 
		       "player%d.worklist%d.name", plrno, j);
    for (i = 0; i < MAX_LEN_WORKLIST; i++)
      secfile_insert_int(file, plr->worklists[j].ids[i], 
			 "player%d.worklist%d.ids%d", plrno, j, i);
  }

  secfile_insert_int(file, plr->city_style, "player%d.city_style", plrno);
  secfile_insert_int(file, plr->is_male, "player%d.is_male", plrno);
  secfile_insert_int(file, plr->is_alive, "player%d.is_alive", plrno);
  secfile_insert_int(file, plr->ai.control, "player%d.ai.control", plrno);
  secfile_insert_int(file, plr->ai.tech_goal, "player%d.ai.tech_goal", plrno);
  secfile_insert_int(file, plr->ai.skill_level,
		     "player%d.ai.skill_level", plrno);
  secfile_insert_int(file, plr->ai.is_barbarian, "player%d.ai.is_barbarian", plrno);
  secfile_insert_int(file, plr->economic.gold, "player%d.gold", plrno);
  secfile_insert_int(file, plr->economic.tax, "player%d.tax", plrno);
  secfile_insert_int(file, plr->economic.science, "player%d.science", plrno);
  secfile_insert_int(file, plr->economic.luxury, "player%d.luxury", plrno);

  secfile_insert_int(file,plr->future_tech,"player%d.futuretech", plrno);

  secfile_insert_int(file, plr->research.researched, 
		     "player%d.researched", plrno);
  secfile_insert_int(file, plr->research.researchpoints,
		     "player%d.researchpoints", plrno);
  secfile_insert_int(file, plr->research.researching,
		     "player%d.researching", plrno);  

  secfile_insert_int(file, plr->capital, 
		      "player%d.capital", plrno);

  for(i=0; i<game.num_tech_types; i++)
    invs[i]=(get_invention(plr, i)==TECH_KNOWN) ? '1' : '0';
  invs[i]='\0';
  secfile_insert_str(file, invs, "player%d.invs", plrno);
  
  secfile_insert_int(file, ship->state, "player%d.spaceship.state", plrno);

  if (ship->state != SSHIP_NONE) {
    char prefix[32];
    char st[NUM_SS_STRUCTURALS+1];
    int i;
    
    my_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);

    secfile_insert_int(file, ship->structurals, "%s.structurals", prefix);
    secfile_insert_int(file, ship->components, "%s.components", prefix);
    secfile_insert_int(file, ship->modules, "%s.modules", prefix);
    secfile_insert_int(file, ship->fuel, "%s.fuel", prefix);
    secfile_insert_int(file, ship->propulsion, "%s.propulsion", prefix);
    secfile_insert_int(file, ship->habitation, "%s.habitation", prefix);
    secfile_insert_int(file, ship->life_support, "%s.life_support", prefix);
    secfile_insert_int(file, ship->solar_panels, "%s.solar_panels", prefix);
    
    for(i=0; i<NUM_SS_STRUCTURALS; i++) {
      st[i] = (ship->structure[i]) ? '1' : '0';
    }
    st[i] = '\0';
    secfile_insert_str(file, st, "%s.structure", prefix);
    if (ship->state >= SSHIP_LAUNCHED) {
      secfile_insert_int(file, ship->launch_year, "%s.launch_year", prefix);
    }
  }

  secfile_insert_int(file, unit_list_size(&plr->units), "player%d.nunits", 
		     plrno);
  secfile_insert_int(file, city_list_size(&plr->cities), "player%d.ncities", 
		     plrno);

  i = -1;
  unit_list_iterate(plr->units, punit) {
    i++;
    secfile_insert_int(file, punit->id, "player%d.u%d.id", plrno, i);
    secfile_insert_int(file, punit->x, "player%d.u%d.x", plrno, i);
    secfile_insert_int(file, punit->y, "player%d.u%d.y", plrno, i);
    secfile_insert_int(file, punit->veteran, "player%d.u%d.veteran", 
				plrno, i);
    secfile_insert_int(file, punit->foul, "player%d.u%d.foul", 
				plrno, i);
    secfile_insert_int(file, punit->hp, "player%d.u%d.hp", plrno, i);
    secfile_insert_int(file, punit->homecity, "player%d.u%d.homecity",
				plrno, i);
    secfile_insert_int(file, punit->type, "player%d.u%d.type",
				plrno, i);
    secfile_insert_int(file, punit->activity, "player%d.u%d.activity",
				plrno, i);
    secfile_insert_int(file, punit->activity_count, 
				"player%d.u%d.activity_count",
				plrno, i);
    secfile_insert_int(file, punit->activity_target, 
				"player%d.u%d.activity_target",
				plrno, i);
    secfile_insert_int(file, punit->connecting, 
				"player%d.u%d.connecting",
				plrno, i);
    secfile_insert_int(file, punit->moves_left, "player%d.u%d.moves",
		                plrno, i);
    secfile_insert_int(file, punit->fuel, "player%d.u%d.fuel",
		                plrno, i);

    secfile_insert_int(file, punit->goto_dest_x, "player%d.u%d.goto_x", plrno, i);
    secfile_insert_int(file, punit->goto_dest_y, "player%d.u%d.goto_y", plrno, i);
    secfile_insert_int(file, punit->ai.control, "player%d.u%d.ai", plrno, i);
    secfile_insert_int(file, punit->ord_map, "player%d.u%d.ord_map", plrno, i);
    secfile_insert_int(file, punit->ord_city, "player%d.u%d.ord_city", plrno, i);
    secfile_insert_int(file, punit->moved, "player%d.u%d.moved", plrno, i);
    secfile_insert_int(file, punit->paradropped, "player%d.u%d.paradropped", plrno, i);
  }
  unit_list_iterate_end;

  i = -1;
  city_list_iterate(plr->cities, pcity) {
    int j, x, y;
    char buf[512];

    i++;
    secfile_insert_int(file, pcity->id, "player%d.c%d.id", plrno, i);
    secfile_insert_int(file, pcity->x, "player%d.c%d.x", plrno, i);
    secfile_insert_int(file, pcity->y, "player%d.c%d.y", plrno, i);
    secfile_insert_str(file, pcity->name, "player%d.c%d.name", plrno, i);
    secfile_insert_int(file, pcity->original, "player%d.c%d.original", 
		       plrno, i);
    secfile_insert_int(file, pcity->size, "player%d.c%d.size", plrno, i);
    secfile_insert_int(file, pcity->steal, "player%d.c%d.steal", plrno, i);
    secfile_insert_int(file, pcity->ppl_elvis, "player%d.c%d.nelvis", plrno, i);
    secfile_insert_int(file, pcity->ppl_scientist, "player%d.c%d.nscientist", 
		       plrno, i);
    secfile_insert_int(file, pcity->ppl_taxman, "player%d.c%d.ntaxman", plrno, i);

    for(j=0; j<4; j++)
      secfile_insert_int(file, pcity->trade[j], "player%d.c%d.traderoute%d", 
			 plrno, i, j);
    
    secfile_insert_int(file, pcity->food_stock, "player%d.c%d.food_stock", 
		       plrno, i);
    secfile_insert_int(file, pcity->shield_stock, "player%d.c%d.shield_stock", 
		       plrno, i);
    secfile_insert_int(file, pcity->anarchy, "player%d.c%d.anarchy", plrno,i);
    secfile_insert_int(file, pcity->turn_last_built, "player%d.c%d.turn_last_built", plrno,i);
    secfile_insert_int(file, pcity->turn_changed_target, "player%d.c%d.turn_changed_target", plrno,i);
    secfile_insert_int(file, pcity->rapture, "player%d.c%d.rapture", plrno,i);
    secfile_insert_int(file, pcity->was_happy, "player%d.c%d.was_happy", plrno,i);
    secfile_insert_int(file, pcity->did_buy, "player%d.c%d.did_buy", plrno,i);
    secfile_insert_int(file, pcity->did_sell, "player%d.c%d.did_sell", plrno,i);
    secfile_insert_int(file, pcity->airlift, "player%d.c%d.airlift", plrno,i);

    /* for auto_attack */
    secfile_insert_int(file, pcity->city_options,
		       "player%d.c%d.options", plrno, i);
    
    j=0;
    for(y=0; y<CITY_MAP_SIZE; y++) {
      for(x=0; x<CITY_MAP_SIZE; x++) {
	switch (get_worker_city(pcity, x, y)) {
	  case C_TILE_EMPTY:       buf[j++] = '0'; break;
	  case C_TILE_WORKER:      buf[j++] = '1'; break;
	  case C_TILE_UNAVAILABLE: buf[j++] = '2'; break;
	}
      }
    }
    buf[j]='\0';
    secfile_insert_str(file, buf, "player%d.c%d.workers", plrno, i);

    secfile_insert_int(file, pcity->is_building_unit, 
		       "player%d.c%d.is_building_unit", plrno, i);
    secfile_insert_int(file, pcity->currently_building, 
		       "player%d.c%d.currently_building", plrno, i);

    for(j=0; j<B_LAST; j++)
      buf[j]=(pcity->improvements[j]) ? '1' : '0';
    buf[j]='\0';
    secfile_insert_str(file, buf, "player%d.c%d.improvements", plrno, i);

    for(j=0; j<MAX_LEN_WORKLIST; j++)
      secfile_insert_int(file, pcity->worklist->ids[j], 
			 "player%d.c%d.worklist%d", plrno, i, j);

  }
  city_list_iterate_end;
}

void server_remove_player(struct player *pplayer)
{
  struct packet_generic_integer pack;
  int o, x, y, idx;
  
  notify_player(pplayer, _("Game: You've been removed from the game!"));
  if(pplayer->conn)
    close_connection(pplayer->conn);
  pplayer->conn=NULL;
  notify_player(0, _("Game: %s has been removed from the game."),
		pplayer->name);
  check_for_full_turn_done();
  
  idx=pplayer->player_no;
  pack.value=pplayer->player_no;

  for(o=0; o<game.nplayers; o++)
    send_packet_generic_integer(game.players[o].conn, PACKET_REMOVE_PLAYER,
				&pack);
/* I can't put the worker stuff in game_remove_city because it's in common */
/* I can't use remove_city here because of traderoutes.  Therefore ... */
  city_list_iterate(pplayer->cities, pcity)
    city_map_iterate(x,y) {
      set_worker_city(pcity, x, y, C_TILE_EMPTY);
    }
    remove_city_from_minimap(x, y);
/* same stupid problem as above, related to the citycache.  Thanks, Anders. */
    remove_city_from_cache(pcity->id);
  city_list_iterate_end;

  game_remove_player(pplayer->player_no);
  game_renumber_players(pplayer->player_no);

  if(map.tiles) {
    for(y=0; y<map.ysize; ++y)
      for(x=0; x<map.xsize; ++x) {
	struct tile *ptile=map_get_tile(x, y);
	if(ptile)
	  ptile->known=WIPEBIT(ptile->known, idx);
      }
  }
}
