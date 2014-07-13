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
#include <fc_config.h>
#endif

#include <string.h>
#include <math.h> /* pow */

/* utility */
#include "rand.h"
#include "registry.h"

/* common */
#include "actions.h"
#include "game.h"
#include "government.h"
#include "research.h"
#include "specialist.h"

/* server */
#include "cityhand.h"
#include "citytools.h"
#include "cityturn.h"
#include "notify.h"
#include "plrhand.h"
#include "srv_log.h"
#include "unithand.h"

/* server/advisors */
#include "advdata.h"
#include "advtools.h"
#include "autosettlers.h"
#include "advbuilding.h"
#include "infracache.h"

/* ai */
#include "aitraits.h"
#include "handicaps.h"

/* ai/default */
#include "advdiplomacy.h"
#include "advdomestic.h"
#include "advmilitary.h"
#include "aidata.h"
#include "aihand.h"
#include "ailog.h"
#include "aiplayer.h"
#include "aisettler.h"
#include "aitools.h"
#include "aiunit.h"
#include "daieffects.h"

#include "aicity.h"

#define LOG_BUY LOG_DEBUG
#define LOG_EMERGENCY LOG_VERBOSE
#define LOG_WANT LOG_VERBOSE

/* TODO:  AI_CITY_RECALC_SPEED should be configurable to ai difficulty.
          -kauf  */
#define AI_CITY_RECALC_SPEED 5

#define AI_BA_RECALC_SPEED 5

#define SPECVEC_TAG tech
#define SPECVEC_TYPE struct advance *
#include "specvec.h"

#define SPECVEC_TAG impr
#define SPECVEC_TYPE struct impr_type *
#include "specvec.h"

/* Iterate over cities within a certain range around a given city
 * (city_here) that exist within a given city list. */
/* FIXME: should this include REQ_RANGE_ALLIANCE/WORLD? Gna bug #21415. */
#define city_range_iterate(city_here, list, range, city)		\
{									\
  city_list_iterate(list, city) {					\
    if (range == REQ_RANGE_PLAYER					\
     || range == REQ_RANGE_TEAM						\
     || (range == REQ_RANGE_TRADEROUTE                                  \
         && (city == city_here                                          \
             || have_cities_trade_route(city, city_here)))              \
     || ((range == REQ_RANGE_CITY || range == REQ_RANGE_LOCAL)		\
      && city == city_here)						\
     || (range == REQ_RANGE_CONTINENT					\
      && tile_continent(city->tile) ==					\
	 tile_continent(city_here->tile))) {

#define city_range_iterate_end						\
    }									\
  } city_list_iterate_end;						\
}

#define CITY_EMERGENCY(pcity)						\
 (pcity->surplus[O_SHIELD] < 0 || city_unhappy(pcity)			\
  || pcity->food_stock + pcity->surplus[O_FOOD] < 0)

#ifdef NDEBUG
#define ASSERT_CHOICE(c) /* Do nothing. */
#else
#define ASSERT_CHOICE(c)                                                 \
  do {                                                                   \
    if ((c).want > 0) {                                                  \
      fc_assert((c).type > CT_NONE && (c).type < CT_LAST);               \
      if ((c).type == CT_BUILDING) {                                     \
        int _iindex = improvement_index((c).value.building);             \
        fc_assert(_iindex >= 0 && _iindex < improvement_count());        \
      } else {                                                           \
        int _uindex = utype_index((c).value.utype);                      \
        fc_assert(_uindex >= 0 && _uindex < utype_count());              \
      }                                                                  \
    }                                                                    \
  } while(FALSE);
#endif /* NDEBUG */

static void dai_sell_obsolete_buildings(struct city *pcity);
static void resolve_city_emergency(struct ai_type *ait, struct player *pplayer,
                                   struct city *pcity);

/************************************************************************** 
  Increase want for a technology because of the value of that technology
  in providing an improvement effect.

  The input building_want gives the desire for the improvement;
  the value may be negative for technologies that produce undesirable
  effects.

  This function must convert from units of 'building want' to 'tech want'.
  We put this conversion in a function because the 'want' scales are
  unclear and kludged. Consequently, this conversion might require tweaking.
**************************************************************************/
static void want_tech_for_improvement_effect(struct ai_type *ait,
                                             struct player *pplayer,
                                             const struct city *pcity,
                                             const struct impr_type *pimprove,
                                             const struct advance *tech,
                                             int building_want)
{
  /* The conversion factor was determined by experiment,
   * and might need adjustment.
   */
  const int tech_want = building_want * def_ai_city_data(pcity, ait)->building_wait
                        * 14 / 8;
#if 0
  /* This logging is relatively expensive,
   * so activate it only while necessary. */
  TECH_LOG(LOG_DEBUG, pplayer, tech,
    "wanted by %s for building: %d -> %d",
    city_name(pcity), improvement_rule_name(pimprove),
    building_want, tech_want);
#endif /* 0 */
  if (tech) {
    def_ai_player_data(pplayer, ait)->tech_want[advance_index(tech)] += tech_want;
  }
}

/************************************************************************** 
  Increase want for a technologies because of the value of that technology
  in providing an improvement effect.
**************************************************************************/
void want_techs_for_improvement_effect(struct ai_type *ait,
                                       struct player *pplayer,
                                       const struct city *pcity,
                                       const struct impr_type *pimprove,
                                       struct tech_vector *needed_techs,
                                       int building_want)
{
  int t;
  int n_needed_techs = tech_vector_size(needed_techs);

  for (t = 0; t < n_needed_techs; t++) {
    want_tech_for_improvement_effect(ait, pplayer, pcity, pimprove,
                                     *tech_vector_get(needed_techs, t),
                                     building_want);
  }
}

/************************************************************************** 
  Decrease want for a technology because of the value of that technology
  in obsoleting an improvement effect.
**************************************************************************/
void dont_want_tech_obsoleting_impr(struct ai_type *ait,
                                    struct player *pplayer,
                                    const struct city *pcity,
                                    const struct impr_type *pimprove,
                                    int building_want)
{
  requirement_vector_iterate(&pimprove->obsolete_by, pobs) {
    if (pobs->source.kind == VUT_ADVANCE && pobs->present) {
      want_tech_for_improvement_effect(ait, pplayer, pcity, pimprove,
                                       pobs->source.value.advance,
                                       -building_want);
    }
  } requirement_vector_iterate_end;
}

/**************************************************************************
  Choose a build for the barbarian player.

  TODO: Move this into advmilitary.c
  TODO: It will be called for each city but doesn't depend on the city,
  maybe cache it?  Although barbarians don't normally have many cities, 
  so can be a bigger bother to cache it.
**************************************************************************/
static void dai_barbarian_choose_build(struct player *pplayer, 
                                       struct city *pcity,
                                       struct adv_choice *choice)
{
  struct unit_type *bestunit = NULL;
  int i, bestattack = 0;

  /* Choose the best unit among the basic ones */
  for(i = 0; i < num_role_units(L_BARBARIAN_BUILD); i++) {
    struct unit_type *iunit = get_role_unit(L_BARBARIAN_BUILD, i);

    if (iunit->attack_strength > bestattack
        && can_city_build_unit_now(pcity, iunit)) {
      bestunit = iunit;
      bestattack = iunit->attack_strength;
    }
  }

  /* Choose among those made available through other civ's research */
  for(i = 0; i < num_role_units(L_BARBARIAN_BUILD_TECH); i++) {
    struct unit_type *iunit = get_role_unit(L_BARBARIAN_BUILD_TECH, i);

    if (iunit->attack_strength > bestattack
        && can_city_build_unit_now(pcity, iunit)) {
      bestunit = iunit;
      bestattack = iunit->attack_strength;
    }
  }

  /* If found anything, put it into the choice */
  if (bestunit) {
    choice->value.utype = bestunit;
    /* FIXME: 101 is the "overriding military emergency" indicator */
    choice->want   = 101;
    choice->type   = CT_ATTACKER;
  } else {
    log_base(LOG_WANT, "Barbarians don't know what to build!");
  }
}

/**************************************************************************
  Chooses what the city will build.  Is called after the military advisor
  put it's choice into pcity->server.ai.choice and "settler advisor" put
  settler want into pcity->founder_*.

  Note that AI cheats -- it suffers no penalty for switching from unit to 
  improvement, etc.
**************************************************************************/
static void dai_city_choose_build(struct ai_type *ait, struct player *pplayer,
                                  struct city *pcity)
{
  struct adv_choice newchoice;
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  init_choice(&newchoice);

  if (has_handicap(pplayer, H_AWAY)
      && city_built_last_turn(pcity)
      && city_data->urgency == 0) {
    /* Don't change existing productions unless we have to. */
    return;
  }

  if (is_barbarian(pplayer)) {
    dai_barbarian_choose_build(pplayer, pcity, &(city_data->choice));
  } else {
    /* FIXME: 101 is the "overriding military emergency" indicator */
    if ((city_data->choice.want <= 100
         || city_data->urgency == 0)
        && !(dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0
             && pcity->id != adv->wonder_city)) {
      domestic_advisor_choose_build(ait, pplayer, pcity, &newchoice);
      copy_if_better_choice(&newchoice, &(city_data->choice));
    }
  }

  /* Fallbacks */
  if (city_data->choice.want == 0) {
    /* Fallbacks do happen with techlevel 0, which is now default. -- Per */
    CITY_LOG(LOG_WANT, pcity, "Falling back - didn't want to build soldiers,"
	     " workers, caravans, settlers, or buildings!");
    city_data->choice.want = 1;
    if (best_role_unit(pcity, UTYF_TRADE_ROUTE)) {
      city_data->choice.value.utype
        = best_role_unit(pcity, UTYF_TRADE_ROUTE);
      city_data->choice.type = CT_CIVILIAN;
    } else if (best_role_unit(pcity, UTYF_SETTLERS)) {
      city_data->choice.value.utype
        = dai_role_utype_for_terrain_class(pcity, UTYF_SETTLERS, TC_LAND);
      city_data->choice.type = CT_CIVILIAN;
    } else {
      CITY_LOG(LOG_ERROR, pcity, "Cannot even build a fallback "
	       "(caravan/coinage/settlers). Fix the ruleset!");
      city_data->choice.want = 0;
    }
  }

  if (city_data->choice.want != 0) {
    ASSERT_CHOICE(city_data->choice);

    CITY_LOG(LOG_DEBUG, pcity, "wants %s with desire " ADV_WANT_PRINTF ".",
	     dai_choice_rule_name(&city_data->choice),
	     city_data->choice.want);
    
    /* FIXME: parallel to citytools change_build_target() */
    if (VUT_IMPROVEMENT == pcity->production.kind
     && is_great_wonder(pcity->production.value.building)
     && (CT_BUILDING != city_data->choice.type
         || city_data->choice.value.building
            != pcity->production.value.building)) {
      notify_player(NULL, pcity->tile, E_WONDER_STOPPED, ftc_server,
		    _("The %s have stopped building The %s in %s."),
		    nation_plural_for_player(pplayer),
		    city_production_name_translation(pcity),
                    city_link(pcity));
    }
    if (CT_BUILDING == city_data->choice.type
      && is_great_wonder(city_data->choice.value.building)
      && (VUT_IMPROVEMENT != pcity->production.kind
          || pcity->production.value.building
             != city_data->choice.value.building)) {
      notify_player(NULL, pcity->tile, E_WONDER_STARTED, ftc_server,
		    _("The %s have started building The %s in %s."),
		    nation_plural_for_player(city_owner(pcity)),
		    city_improvement_name_translation(pcity,
                      city_data->choice.value.building),
                    city_link(pcity));
    }

    switch (city_data->choice.type) {
    case CT_CIVILIAN:
    case CT_ATTACKER:
    case CT_DEFENDER:
      pcity->production.kind = VUT_UTYPE;
      pcity->production.value.utype = city_data->choice.value.utype;
      break;
    case CT_BUILDING:
      pcity->production.kind = VUT_IMPROVEMENT;
      pcity->production.value.building 
        = city_data->choice.value.building;
      break;
    case CT_NONE:
      pcity->production.kind = VUT_NONE;
      break;
    case CT_LAST:
      pcity->production.kind = universals_n_invalid();
      break;
    };
  }
}

/************************************************************************** 
  Sell building from city
**************************************************************************/
static void try_to_sell_stuff(struct player *pplayer, struct city *pcity)
{
  improvement_iterate(pimprove) {
    if (can_city_sell_building(pcity, pimprove)
	&& !building_has_effect(pimprove, EFT_DEFEND_BONUS)) {
/* selling walls to buy defenders is counterproductive -- Syela */
      really_handle_city_sell(pplayer, pcity, pimprove);
      break;
    }
  } improvement_iterate_end;
}

/************************************************************************** 
  Increase maxbuycost.  This variable indicates (via ai_gold_reserve) to 
  the tax selection code how much money do we need for buying stuff.
**************************************************************************/
static void increase_maxbuycost(struct player *pplayer, int new_value)
{
  pplayer->ai_common.maxbuycost = MAX(pplayer->ai_common.maxbuycost, new_value);
}

/************************************************************************** 
  Try to upgrade a city's units. limit is the last amount of gold we can
  end up with after the upgrade. military is if we want to upgrade non-
  military or military units.
**************************************************************************/
static void dai_upgrade_units(struct city *pcity, int limit, bool military)
{
  struct player *pplayer = city_owner(pcity);
  int expenses;

  dai_calc_data(pplayer, NULL, &expenses, NULL);

  unit_list_iterate(pcity->tile->units, punit) {
    if (pcity->owner == punit->owner) {
      /* Only upgrade units you own, not allied ones */
      struct unit_type *punittype = can_upgrade_unittype(pplayer, unit_type(punit));

      if (military && !IS_ATTACKER(punit)) {
        /* Only upgrade military units this round */
        continue;
      } else if (!military && IS_ATTACKER(punit)) {
        /* Only civilians or tranports this round */
        continue;
      }
      if (punittype) {
        int cost = unit_upgrade_price(pplayer, unit_type(punit), punittype);
        int real_limit = limit;

        /* Triremes are DANGEROUS!! We'll do anything to upgrade 'em. */
        if (unit_has_type_flag(punit, UTYF_TRIREME)) {
          real_limit = expenses;
        }
        if (pplayer->economic.gold - cost > real_limit) {
          CITY_LOG(LOG_BUY, pcity, "Upgraded %s to %s for %d (%s)",
                   unit_rule_name(punit),
                   utype_rule_name(punittype),
                   cost,
                   military ? "military" : "civilian");
          handle_unit_upgrade(city_owner(pcity), punit->id);
        } else {
          increase_maxbuycost(pplayer, cost);
        }
      }
    }
  } unit_list_iterate_end;
}

/************************************************************************** 
  Buy and upgrade stuff!
**************************************************************************/
static void dai_spend_gold(struct ai_type *ait, struct player *pplayer)
{
  struct adv_choice bestchoice;
  int cached_limit = dai_gold_reserve(pplayer);
  int expenses;
  bool war_footing = dai_on_war_footing(ait, pplayer);

  /* Disband explorers that are at home but don't serve a purpose. 
   * FIXME: This is a hack, and should be removed once we
   * learn how to ferry explorers to new land. */
  city_list_iterate(pplayer->cities, pcity) {
    struct tile *ptile = pcity->tile;
    unit_list_iterate_safe(ptile->units, punit) {
      if (unit_has_type_role(punit, L_EXPLORER)
          && pcity->id == punit->homecity
          && def_ai_city_data(pcity, ait)->urgency == 0) {
        CITY_LOG(LOG_BUY, pcity, "disbanding %s to increase production",
                 unit_rule_name(punit));
	handle_unit_disband(pplayer,punit->id);
      }
    } unit_list_iterate_safe_end;
  } city_list_iterate_end;

  dai_calc_data(pplayer, NULL, &expenses, NULL);

  do {
    bool expensive; /* don't buy when it costs x2 unless we must */
    int buycost;
    int limit = cached_limit; /* cached_limit is our gold reserve */
    struct city *pcity = NULL;
    struct ai_city *city_data;

    /* Find highest wanted item on the buy list */
    init_choice(&bestchoice);
    city_list_iterate(pplayer->cities, acity) {
      struct ai_city *acity_data = def_ai_city_data(acity, ait);

      if (acity_data->choice.want
          > bestchoice.want && ai_fuzzy(pplayer, TRUE)) {
        bestchoice.value = acity_data->choice.value;
        bestchoice.want = acity_data->choice.want;
        bestchoice.type = acity_data->choice.type;
        pcity = acity;
      }
    } city_list_iterate_end;

    /* We found nothing, so we're done */
    if (bestchoice.want == 0) {
      break;
    }

    city_data = def_ai_city_data(pcity, ait);

    /* Not dealing with this city a second time */
    city_data->choice.want = 0;

    ASSERT_CHOICE(bestchoice);

    /* Try upgrade units at danger location (high want is usually danger) */
    if (city_data->urgency > 1) {
      if (bestchoice.type == CT_BUILDING
       && is_wonder(bestchoice.value.building)) {
        CITY_LOG(LOG_BUY, pcity, "Wonder being built in dangerous position!");
      } else {
        /* If we have urgent want, spend more */
        int upgrade_limit = limit;

        if (city_data->urgency > 1) {
          upgrade_limit = expenses;
        }
        /* Upgrade only military units now */
        dai_upgrade_units(pcity, upgrade_limit, TRUE);
      }
    }

    if (pcity->anarchy != 0 && bestchoice.type != CT_BUILDING) {
      continue; /* Nothing we can do */
    }

    /* Cost to complete production */
    buycost = city_production_buy_gold_cost(pcity);

    if (buycost <= 0) {
      continue; /* Already completed */
    }

    if (is_unit_choice_type(bestchoice.type)
        && utype_has_flag(bestchoice.value.utype, UTYF_CITIES)) {
      if (get_city_bonus(pcity, EFT_GROWTH_FOOD) == 0
          && bestchoice.value.utype->pop_cost > 0
          && city_size_get(pcity) <= bestchoice.value.utype->pop_cost) {
        /* Don't buy settlers in cities that cannot afford the population cost. */
        /* This used to check also if city is about to grow to required size
         * next turn and allow buying of settlers in that case, but current
         * order of end/start turn activities is such that settler building
         * fails already before city grows. */
        continue;
      } else if (city_list_size(pplayer->cities) > 6) {
        /* Don't waste precious money buying settlers late game
         * since this raises taxes, and we want science. Adjust this
         * again when our tax algorithm is smarter. */
        continue;
      } else if (war_footing) {
        continue;
      }
    } else {
      /* We are not a settler. Therefore we increase the cash need we
       * balance our buy desire with to keep cash at hand for emergencies 
       * and for upgrades */
      limit *= 2;
    }

    /* It costs x2 to buy something with no shields contributed */
    expensive = (pcity->shield_stock == 0)
                || (pplayer->economic.gold - buycost < limit);

    if (bestchoice.type == CT_ATTACKER
	&& buycost 
           > utype_build_shield_cost(bestchoice.value.utype) * 2
        && !war_footing) {
       /* Too expensive for an offensive unit */
       continue;
    }

    /* FIXME: Here Syela wanted some code to check if
     * pcity was doomed, and we should therefore attempt
     * to sell everything in it of non-military value */

    if (pplayer->economic.gold - expenses >= buycost
        && (!expensive 
            || (city_data->grave_danger != 0
                && assess_defense(ait, pcity) == 0)
            || (bestchoice.want > 200 && city_data->urgency > 1))) {
      /* Buy stuff */
      CITY_LOG(LOG_BUY, pcity, "Crash buy of %s for %d (want " ADV_WANT_PRINTF ")",
               dai_choice_rule_name(&bestchoice),
               buycost,
               bestchoice.want);
      really_handle_city_buy(pplayer, pcity);
    } else if (city_data->grave_danger != 0 
               && bestchoice.type == CT_DEFENDER
               && assess_defense(ait, pcity) == 0) {
      /* We have no gold but MUST have a defender */
      CITY_LOG(LOG_BUY, pcity, "must have %s but can't afford it (%d < %d)!",
               dai_choice_rule_name(&bestchoice),
               pplayer->economic.gold,
               buycost);
      try_to_sell_stuff(pplayer, pcity);
      if (pplayer->economic.gold - expenses >= buycost) {
        CITY_LOG(LOG_BUY, pcity, "now we can afford it (sold something)");
        really_handle_city_buy(pplayer, pcity);
      }
      increase_maxbuycost(pplayer, buycost);
    }
  } while (TRUE);

  if (!war_footing) {
    /* Civilian upgrades now */
    city_list_iterate(pplayer->cities, pcity) {
      dai_upgrade_units(pcity, cached_limit, FALSE);
    } city_list_iterate_end;
  }

  log_base(LOG_BUY, "%s wants to keep %d in reserve (tax factor %d)", 
           player_name(pplayer), cached_limit, pplayer->ai_common.maxbuycost);
}

/**************************************************************************
  Calculates a unit's food upkeep (per turn).
**************************************************************************/
static int unit_food_upkeep(struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  int upkeep = utype_upkeep_cost(unit_type(punit), pplayer, O_FOOD);
  if (punit->id != 0 && punit->homecity == 0)
    upkeep = 0; /* thanks, Peter */

  return upkeep;
}


/**************************************************************************
  Returns how much food a settler will consume out of the city's foodbox
  when created. If unit has id zero it is assumed to be a virtual unit
  inside a city.

  FIXME: This function should be generalised and then moved into 
  common/unittype.c - Per
**************************************************************************/
static int unit_foodbox_cost(struct unit *punit)
{
  int cost = 30;

  if (punit->id == 0) {
    /* It is a virtual unit, so must start in a city... */
    struct city *pcity = tile_city(unit_tile(punit));

    /* The default is to lose 100%.  The growth bonus reduces this. */
    int foodloss_pct = 100 - get_city_bonus(pcity, EFT_GROWTH_FOOD);

    foodloss_pct = CLIP(0, foodloss_pct, 100);
    fc_assert_ret_val(pcity != NULL, -1);
    cost = city_granary_size(city_size_get(pcity));
    cost = cost * foodloss_pct / 100;
  }

  return cost;
}

/**************************************************************************
  Estimates the want for a terrain improver (aka worker) by creating a 
  virtual unit and feeding it to settler_evaluate_improvements.

  TODO: AI does not ship UTYF_SETTLERS around, only UTYF_CITIES - Per
**************************************************************************/
static void contemplate_terrain_improvements(struct ai_type *ait,
                                             struct city *pcity)
{
  struct unit *virtualunit;
  int want;
  enum unit_activity best_act;
  struct extra_type *best_target;
  struct tile *best_tile = NULL; /* May be accessed by log_*() calls. */
  struct tile *pcenter = city_tile(pcity);
  struct player *pplayer = city_owner(pcity);
  struct adv_data *ai = adv_data_get(pplayer, NULL);
  struct unit_type *unit_type
           = dai_role_utype_for_terrain_class(pcity, UTYF_SETTLERS, TC_LAND);
  Continent_id place = tile_continent(pcenter);

  if (unit_type == NULL) {
    log_debug("No UTYF_SETTLERS role unit available");
    return;
  }

  /* Create a localized "virtual" unit to do operations with. */
  virtualunit = unit_virtual_create(pplayer, pcity, unit_type, 0);
  /* Advisors data space not allocated as it's not needed in the
     lifetime of the virtualunit. */
  unit_tile_set(virtualunit, pcenter);
  want = settler_evaluate_improvements(virtualunit, &best_act, &best_target,
                                       &best_tile,
                                       NULL, NULL);
  /* We consider unit_food_upkeep with only half FOOD_WEIGHTING to
   * balance the fact that unit can improve many tiles during its
   * lifetime, and want is calculated for just one of them.
   * Having full FOOD_WEIGHT here would mean that tile improvement of
   * +1 food would give just zero want for settler. Other weights
   * are lower, so +1 shield - unit food upkeep would be negative. */
  want = (want - unit_food_upkeep(virtualunit) * FOOD_WEIGHTING / 2) * 100
         / (40 + unit_foodbox_cost(virtualunit));
  unit_virtual_destroy(virtualunit);

  /* Massage our desire based on available statistics to prevent
   * overflooding with worker type units if they come cheap in
   * the ruleset */
  want /= MAX(1, ai->stats.workers[place]
                 / (ai->stats.cities[place] + 1));
  want -= ai->stats.workers[place];
  want = MAX(want, 0);

  CITY_LOG(LOG_DEBUG, pcity, "wants %s with want %d to do %s at (%d,%d), "
           "we have %d workers and %d cities on the continent",
	   utype_rule_name(unit_type),
	   want,
	   get_activity_text(best_act),
	   TILE_XY(best_tile),
           ai->stats.workers[place], 
           ai->stats.cities[place]);
  fc_assert(want >= 0);

  def_ai_city_data(pcity, ait)->settler_want = want;
}

/**************************************************************************
  One of the top level AI functions.  It does (by calling other functions):
  worker allocations,
  build choices,
  extra gold spending.
**************************************************************************/
void dai_manage_cities(struct ai_type *ait, struct player *pplayer)
{
  pplayer->ai_common.maxbuycost = 0;

  TIMING_LOG(AIT_EMERGENCY, TIMER_START);
  city_list_iterate(pplayer->cities, pcity) {
    if (CITY_EMERGENCY(pcity)
        || city_granary_size(city_size_get(pcity)) == pcity->food_stock) {
      /* Having a full granary isn't an emergency, but we want to rearrange */
      auto_arrange_workers(pcity); /* this usually helps */
    }
    if (CITY_EMERGENCY(pcity)) {
      /* Fix critical shortages or unhappiness */
      resolve_city_emergency(ait, pplayer, pcity);
    }
    dai_sell_obsolete_buildings(pcity);
    sync_cities();
  } city_list_iterate_end;
  TIMING_LOG(AIT_EMERGENCY, TIMER_STOP);

  TIMING_LOG(AIT_BUILDINGS, TIMER_START);
  building_advisor(pplayer);
  TIMING_LOG(AIT_BUILDINGS, TIMER_STOP);

  /* Initialize the infrastructure cache, which is used shortly. */
  initialize_infrastructure_cache(pplayer);
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);
    /* Note that this function mungs the seamap, but we don't care */
    TIMING_LOG(AIT_CITY_MILITARY, TIMER_START);
    military_advisor_choose_build(ait, pplayer, pcity, &city_data->choice);
    TIMING_LOG(AIT_CITY_MILITARY, TIMER_STOP);
    if (dai_on_war_footing(ait, pplayer) && city_data->choice.want > 0) {
      continue; /* Go, soldiers! */
    }
    /* Will record its findings in pcity->settler_want */ 
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_START);
    contemplate_terrain_improvements(ait, pcity);
    TIMING_LOG(AIT_CITY_TERRAIN, TIMER_STOP);

    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_START);
    if (city_data->founder_turn <= game.info.turn) {
      /* Will record its findings in pcity->founder_want */ 
      contemplate_new_city(ait, pcity);
      /* Avoid recalculating all the time.. */
      city_data->founder_turn = 
        game.info.turn + fc_rand(AI_CITY_RECALC_SPEED) + AI_CITY_RECALC_SPEED;
    } else if (pcity->server.debug) {
      /* recalculate every turn */
      contemplate_new_city(ait, pcity);
    }
    TIMING_LOG(AIT_CITY_SETTLERS, TIMER_STOP);
    ASSERT_CHOICE(city_data->choice);
  } city_list_iterate_end;
  /* Reset auto settler state for the next run. */
  dai_auto_settler_reset(ait, pplayer);

  city_list_iterate(pplayer->cities, pcity) {
    dai_city_choose_build(ait, pplayer, pcity);
  } city_list_iterate_end;

  dai_spend_gold(ait, pplayer);
}

/**************************************************************************
  Are effects provided by this building not needed?
**************************************************************************/
static bool building_unwanted(struct player *plr, struct impr_type *pimprove)
{
#if 0 /* This check will become more complicated now. */ 
  return (ai_wants_no_science(plr)
          && building_has_effect(pimprove, EFT_SCIENCE_BONUS));
#endif
  return FALSE;
}

/**************************************************************************
  Sell an obsolete building if there are any in the city.
**************************************************************************/
static void dai_sell_obsolete_buildings(struct city *pcity)
{
  struct player *pplayer = city_owner(pcity);

  city_built_iterate(pcity, pimprove) {
    if (can_city_sell_building(pcity, pimprove) 
       && !building_has_effect(pimprove, EFT_DEFEND_BONUS)
	      /* selling city walls is really, really dumb -- Syela */
       && (is_improvement_redundant(pcity, pimprove)
	   || building_unwanted(pplayer, pimprove))) {
      do_sell_building(pplayer, pcity, pimprove);
      notify_player(pplayer, pcity->tile, E_IMP_SOLD, ftc_server,
                    _("%s is selling %s (not needed) for %d."), 
                    city_link(pcity),
                    improvement_name_translation(pimprove), 
                    impr_sell_gold(pimprove));
      return; /* max 1 building each turn */
    }
  } city_built_iterate_end;
}

/**************************************************************************
  This function tries desperately to save a city from going under by
  revolt or starvation of food or resources. We do this by taking
  over resources held by nearby cities and disbanding units.

  TODO: Try to move units into friendly cities to reduce unhappiness
  instead of disbanding. Also rather starve city than keep it in
  revolt, as long as we don't lose settlers.

  TODO: Make function that tries to save units by moving them into
  cities that can upkeep them and change homecity rather than just
  disband. This means we'll have to move this function to beginning
  of AI turn sequence (before moving units).

  "I don't care how slow this is; it will very rarely be used." -- Syela

  Syela is wrong. It happens quite too often, mostly due to unhappiness.
  Also, most of the time we are unable to resolve the situation. 
**************************************************************************/
static void resolve_city_emergency(struct ai_type *ait, struct player *pplayer,
                                   struct city *pcity)
{
  struct tile *pcenter = city_tile(pcity);

  log_base(LOG_EMERGENCY,
           "Emergency in %s (%s, angry%d, unhap%d food%d, prod%d)",
           city_name(pcity),
           city_unhappy(pcity) ? "unhappy" : "content",
           pcity->feel[CITIZEN_ANGRY][FEELING_FINAL],
           pcity->feel[CITIZEN_UNHAPPY][FEELING_FINAL],
           pcity->surplus[O_FOOD],
           pcity->surplus[O_SHIELD]);

  city_tile_iterate(city_map_radius_sq_get(pcity), pcenter, atile) {
    struct city *acity = tile_worked(atile);

    if (acity && acity != pcity && city_owner(acity) == city_owner(pcity))  {
      log_base(LOG_EMERGENCY, "%s taking over %s square in (%d, %d)",
               city_name(pcity), city_name(acity), TILE_XY(atile));

      int ax, ay;
      fc_assert_action(city_base_to_city_map(&ax, &ay, acity, atile),
                       continue);

      if (is_free_worked(acity, atile)) {
        /* Can't remove a worker here. */
        continue;
      }

      city_map_update_empty(acity, atile);
      acity->specialists[DEFAULT_SPECIALIST]++;
      city_freeze_workers_queue(acity);
    }
  } city_tile_iterate_end;

  auto_arrange_workers(pcity);

  if (!CITY_EMERGENCY(pcity)) {
    log_base(LOG_EMERGENCY, "Emergency in %s resolved", city_name(pcity));
    goto cleanup;
  }

  unit_list_iterate_safe(pcity->units_supported, punit) {
    if (city_unhappy(pcity)
        && (utype_happy_cost(unit_type(punit), pplayer) > 0
            && (unit_being_aggressive(punit) || is_field_unit(punit)))
        && def_ai_unit_data(punit, ait)->passenger == 0) {
      UNIT_LOG(LOG_EMERGENCY, punit, "is causing unrest, disbanded");
      handle_unit_disband(pplayer, punit->id);
      city_refresh(pcity);
    }
  } unit_list_iterate_safe_end;

  if (CITY_EMERGENCY(pcity)) {
    log_base(LOG_EMERGENCY, "Emergency in %s remains unresolved",
             city_name(pcity));
  } else {
    log_base(LOG_EMERGENCY,
             "Emergency in %s resolved by disbanding unit(s)",
             city_name(pcity));
  }

  cleanup:
  city_thaw_workers_queue();
  sync_cities();
}

/**************************************************************************
  Initialize city for use with default AI.
**************************************************************************/
void dai_city_alloc(struct ai_type *ait, struct city *pcity)
{
  struct ai_city *city_data = fc_calloc(1, sizeof(struct ai_city));

  city_data->building_wait = BUILDING_WAIT_MINIMUM;

  city_set_ai_data(pcity, ait, city_data);
}

/**************************************************************************
  Free city from use with default AI.
**************************************************************************/
void dai_city_free(struct ai_type *ait, struct city *pcity)
{
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  if (city_data != NULL) {
    city_set_ai_data(pcity, ait, NULL);
    FC_FREE(city_data);
  }
}

/**************************************************************************
  Write ai city segments to savefile
**************************************************************************/
void dai_city_save(struct ai_type *ait, const char *aitstr,
                   struct section_file *file, const struct city *pcity,
                   const char *citystr)
{
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  /* FIXME: remove this when the urgency is properly recalculated. */
  secfile_insert_int(file, city_data->urgency, "%s.%s.urgency", citystr, aitstr);

  /* avoid fc_rand recalculations on subsequent reload. */
  secfile_insert_int(file, city_data->building_turn, "%s.%s.building_turn",
                     citystr, aitstr);
  secfile_insert_int(file, city_data->building_wait, "%s.%s.building_wait",
                     citystr, aitstr);

  /* avoid fc_rand and expensive recalculations on subsequent reload. */
  secfile_insert_int(file, city_data->founder_turn, "%s.%s.founder_turn",
                     citystr, aitstr);
  secfile_insert_int(file, city_data->founder_want, "%s.%s.founder_want",
                     citystr, aitstr);
  secfile_insert_bool(file, city_data->founder_boat, "%s.%s.founder_boat",
                      citystr, aitstr);
}

/**************************************************************************
  Load ai city segment from savefile
**************************************************************************/
void dai_city_load(struct ai_type *ait, const char *aitstr,
                   const struct section_file *file,
                   struct city *pcity, const char *citystr)
{
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  /* FIXME: remove this when the urgency is properly recalculated. */
  city_data->urgency
    = secfile_lookup_int_default(file, 0, "%s.%s.urgency", citystr, aitstr);

  /* avoid fc_rand recalculations on subsequent reload. */
  city_data->building_turn
    = secfile_lookup_int_default(file, 0, "%s.%s.building_turn", citystr,
                                 aitstr);
  city_data->building_wait
    = secfile_lookup_int_default(file, BUILDING_WAIT_MINIMUM,
                                 "%s.%s.building_wait", citystr, aitstr);

  /* avoid fc_rand and expensive recalculations on subsequent reload. */
  city_data->founder_turn
    = secfile_lookup_int_default(file, 0, "%s.%s.founder_turn", citystr,
                                 aitstr);
  city_data->founder_want
    = secfile_lookup_int_default(file, 0, "%s.%s.founder_want", citystr,
                                 aitstr);
  city_data->founder_boat
    = secfile_lookup_bool_default(file, (city_data->founder_want < 0),
                                  "%s.%s.founder_boat", citystr, aitstr);
}

/**************************************************************************
  How undesirable for the owner of a particular city is the fact that it
  can be a target of a particular action?

  The negative utility (how undesirable it is) is given as a positive
  number. If it is desirable to be the target of an action make the
  negative utility a negative number since double negative is positive.

  Examples:
   * action_target_neg_util(Add to population) = -50
   * action_target_neg_util(Subtract from population) = 50
**************************************************************************/
static int action_target_neg_util(int action_id,
                                  const struct city *pcity)
{
  fc_assert_ret_val_msg(action_get_target_kind(action_id) == ATK_CITY,
                        0, "Action not aimed at cities");

  switch (action_id) {
  case ACTION_SPY_INCITE_CITY:
    /* Copied from the evaluation of the No_Incite effect */
    return MAX((game.server.diplchance * 2
                - game.server.incite_total_factor) / 2
               - game.server.incite_improvement_factor * 5
               - game.server.incite_unit_factor * 5, 0);

  /* Bad for the city */
  case ACTION_SPY_POISON:
  case ACTION_SPY_SABOTAGE_CITY:
  case ACTION_SPY_TARGETED_SABOTAGE_CITY:
    /* TODO: Individual and well balanced values */
    return 10;

  /* Good for an enemy */
  case ACTION_SPY_STEAL_TECH:
  case ACTION_SPY_TARGETED_STEAL_TECH:
    /* TODO: Individual and well balanced values */
    return 8;

  /* Could be worse */
  case ACTION_ESTABLISH_EMBASSY:
  case ACTION_SPY_INVESTIGATE_CITY:
    /* TODO: Individual and well balanced values */
    return 1;
  }

  return 0;
}

/************************************************************************** 
  Increase the degree to which we want to meet a set of requirements,
  because they will enable construction of an improvement
  with desirable effects.

  v is the desire for the improvement.

  Returns whether all the requirements are met.
**************************************************************************/
static bool adjust_wants_for_reqs(struct ai_type *ait,
                                  struct player *pplayer,
                                  struct city *pcity,
                                  struct impr_type *pimprove,
                                  const int v)
{
  bool all_met = TRUE;
  int n_needed_techs = 0;
  int n_needed_improvements = 0;
  struct tech_vector needed_techs;
  struct impr_vector needed_improvements;

  tech_vector_init(&needed_techs);
  impr_vector_init(&needed_improvements);

  requirement_vector_iterate(&pimprove->reqs, preq) {
    const bool active = is_req_active(pplayer, NULL, pcity, pimprove,
                                      pcity->tile, NULL, NULL, NULL, NULL,
                                      preq, RPT_POSSIBLE);

    if (VUT_ADVANCE == preq->source.kind && preq->present && !active) {
      /* Found a missing technology requirement for this improvement. */
      tech_vector_append(&needed_techs, preq->source.value.advance);
    } else if (VUT_IMPROVEMENT == preq->source.kind && preq->present && !active) {
      /* Found a missing improvement requirement for this improvement.
       * For example, in the default ruleset a city must have a Library
       * before it can have a University. */
      impr_vector_append(&needed_improvements, preq->source.value.building);
    }
    all_met = all_met && active;
  } requirement_vector_iterate_end;

  /* If v is negative, the improvement is not worth building,
   * but there is no need to punish research of the technologies
   * that would make it available.
   */
  n_needed_techs = tech_vector_size(&needed_techs);
  if (0 < v && 0 < n_needed_techs) {
    /* Tell AI module how much we want this improvement and what techs are
     * required to get it. */
    const int dv = v / n_needed_techs;

    want_techs_for_improvement_effect(ait, pplayer, pcity, pimprove,
                                      &needed_techs, dv);
  }

  /* If v is negative, the improvement is not worth building,
   * but there is no need to punish building the improvements
   * that would make it available.
   */
  n_needed_improvements = impr_vector_size(&needed_improvements);
  if (0 < v && 0 < n_needed_improvements) {
    /* Because we want this improvement,
     * we want the improvements that will make it possible */
    const int dv = v / (n_needed_improvements * 4); /* WAG */
    int i;

    for (i = 0; i < n_needed_improvements; i++) {
      struct impr_type *needed_impr = *impr_vector_get(&needed_improvements, i);
      /* TODO: increase the want for the needed_impr,
       * if we can build it now */
      /* Recurse */
      (void) adjust_wants_for_reqs(ait, pplayer, pcity, needed_impr, dv);
    }
  }

  /* TODO: use a similar method to increase wants for governments
   * that will make this improvement possible? */

  tech_vector_free(&needed_techs);
  impr_vector_free(&needed_improvements);

  return all_met;
}


/**************************************************************************
  Calculates city want from some input values.  Set pimprove to NULL when
  nothing in the city has changed, and you just want to know the
  base want of a city.
**************************************************************************/
static int city_want(struct player *pplayer, struct city *acity, 
                     struct adv_data *adv, struct impr_type *pimprove)
{
  int want = 0, prod[O_LAST], bonus[O_LAST], waste[O_LAST], i;

  memset(prod, 0, O_LAST * sizeof(*prod));
  if (NULL != pimprove
   && adv->impr_calc[improvement_index(pimprove)] == ADV_IMPR_CALCULATE_FULL) {
    struct tile *acenter = city_tile(acity);
    bool celebrating = base_city_celebrating(acity);

    /* The below calculation mostly duplicates get_worked_tile_output().
     * We do this only for buildings that we know may change tile
     * outputs. */
    city_tile_iterate(city_map_radius_sq_get(acity), acenter, ptile) {
      if (tile_worked(ptile) == acity) {
        output_type_iterate(o) {
          prod[o] += city_tile_output(acity, ptile, celebrating, o);
        } output_type_iterate_end;
      }
    } city_tile_iterate_end;

    add_specialist_output(acity, prod);
  } else {
    fc_assert(sizeof(*prod) == sizeof(*acity->citizen_base));
    memcpy(prod, acity->citizen_base, O_LAST * sizeof(*prod));
  }

  for (i = 0; i < MAX_TRADE_ROUTES; i++) {
    prod[O_TRADE] += acity->trade_value[i];
  }
  prod[O_GOLD] += get_city_tithes_bonus(acity);
  output_type_iterate(o) {
    bonus[o] = get_final_city_output_bonus(acity, o);
    waste[o] = city_waste(acity, o, prod[o] * bonus[o] / 100, NULL);
  } output_type_iterate_end;
  add_tax_income(pplayer,
		 prod[O_TRADE] * bonus[O_TRADE] / 100 - waste[O_TRADE],
		 prod);
  output_type_iterate(o) {
    prod[o] = prod[o] * bonus[o] / 100 - waste[o];
  } output_type_iterate_end;

  city_built_iterate(acity, pimprove) {
    prod[O_GOLD] -= city_improvement_upkeep(acity, pimprove);
  } city_built_iterate_end;
  /* Unit upkeep isn't handled here.  Unless we do a full city_refresh it
   * won't be changed anyway. */

  want += prod[O_FOOD] * adv->food_priority;
  if (prod[O_SHIELD] != 0) {
    want += prod[O_SHIELD] * adv->shield_priority;
    want -= city_pollution(acity, prod[O_SHIELD]) * adv->pollution_priority;
  }
  want += prod[O_LUXURY] * adv->luxury_priority;
  want += prod[O_SCIENCE] * adv->science_priority;
  want += prod[O_GOLD] * adv->gold_priority;

  return want;
}

/**************************************************************************
  Calculates want for some buildings by actually adding the building and
  measuring the effect.
**************************************************************************/
static int base_want(struct ai_type *ait, struct player *pplayer,
                     struct city *pcity, struct impr_type *pimprove)
{
  struct adv_data *adv = adv_data_get(pplayer, NULL);
  int final_want = 0;
  int wonder_player_id = WONDER_NOT_OWNED;
  int wonder_city_id = WONDER_NOT_BUILT;

  if (adv->impr_calc[improvement_index(pimprove)] == ADV_IMPR_ESTIMATE) {
    return 0; /* Nothing to calculate here. */
  }

  if (!can_city_build_improvement_now(pcity, pimprove)
      || (is_small_wonder(pimprove)
          && NULL != city_from_small_wonder(pplayer, pimprove))) {
    return 0;
  }

  if (is_wonder(pimprove)) {
    if (is_great_wonder(pimprove)) {
      wonder_player_id =
          game.info.great_wonder_owners[improvement_index(pimprove)];
    }
    wonder_city_id = pplayer->wonders[improvement_index(pimprove)];
  }
  /* Add the improvement */
  city_add_improvement(pcity, pimprove);

  /* Stir, then compare notes */
  city_range_iterate(pcity, pplayer->cities,
                     adv->impr_range[improvement_index(pimprove)], acity) {
    final_want += city_want(pplayer, acity, adv, pimprove)
      - def_ai_city_data(acity, ait)->worth;
  } city_range_iterate_end;

  /* Restore */
  city_remove_improvement(pcity, pimprove);
  if (is_wonder(pimprove)) {
    if (is_great_wonder(pimprove)) {
      game.info.great_wonder_owners[improvement_index(pimprove)] =
          wonder_player_id;
    }

    pplayer->wonders[improvement_index(pimprove)] = wonder_city_id;
  }

  return final_want;
}

/************************************************************************** 
  Calculate effects of possible improvements and extra effects of existing
  improvements. Consequently adjust the desirability of those improvements
  or the technologies that would make them possible.

  This function may (indeed, should) be called even for improvements that a city
  already has, or can not (yet) build. For existing improvements,
  it will discourage research of technologies that would make the improvement
  obsolete or reduce its effectiveness, and encourages technologies that would
  improve its effectiveness. For improvements that the city can not yet build
  it will encourage research of the techs and building of the improvements
  that will make the improvement possible.

  A complexity is that there are two sets of requirements to consider:
  the requirements for the building itself, and the requirements for
  the effects for the building.

  A few base variables:
    c - number of cities we have in current range
    u - units we have of currently affected type
    v - the want for the improvement we are considering

  This function contains a whole lot of WAGs. We ignore cond_* for now,
  thinking that one day we may fulfil the cond_s anyway. In general, we
  first add bonus for city improvements, then for wonders.

  IDEA: Calculate per-continent aggregates of various data, and use this
  for wonders below for better wonder placements.
**************************************************************************/
static void adjust_improvement_wants_by_effects(struct ai_type *ait,
                                                struct player *pplayer,
                                                struct city *pcity,
                                                struct impr_type *pimprove,
                                                const bool already)
{
  int v = 0;
  int cities[REQ_RANGE_COUNT];
  int nplayers = normal_player_count();
  struct adv_data *ai = adv_data_get(pplayer, NULL);
  bool capital = is_capital(pcity);
  bool can_build = TRUE;
  struct government *gov = government_of_player(pplayer);
  struct universal source = {
    .kind = VUT_IMPROVEMENT,
    .value = {.building = pimprove}
  };
  const bool is_coinage = improvement_has_flag(pimprove, IF_GOLD);
  int turns = 9999;

  /* Remove team members from the equation */
  players_iterate(aplayer) {
    if (aplayer->team
        && aplayer->team == pplayer->team
        && aplayer != pplayer) {
      nplayers--;
    }
  } players_iterate_end;

  if (is_coinage) {
    /* Since coinage contains some entirely spurious ruleset values,
     * we need to hard-code a sensible want.
     * We must otherwise handle the special IF_GOLD improvement
     * like the others, so the AI will research techs that make it available,
     * for rulesets that do not provide it from the start.
     */
    v += TRADE_WEIGHTING / 10;
  } else {
    /* Base want is calculated above using a more direct approach. */
    v += base_want(ait, pplayer, pcity, pimprove);
    if (v != 0) {
      CITY_LOG(LOG_DEBUG, pcity, "%s base_want is %d (range=%d)", 
               improvement_rule_name(pimprove),
               v,
               ai->impr_range[improvement_index(pimprove)]);
    }
  }

  if (!is_coinage) {
    /* Adjust by building cost */
    /* FIXME: ought to reduce by upkeep cost and amortise by building cost */
    v -= (impr_build_shield_cost(pimprove)
         / (pcity->surplus[O_SHIELD] * 10 + 1));
  }

  /* Find number of cities per range.  */
  cities[REQ_RANGE_PLAYER] = city_list_size(pplayer->cities);
  /* kludge -- Number of *our* cities in these ranges. */
  cities[REQ_RANGE_WORLD] = cities[REQ_RANGE_ALLIANCE] = cities[REQ_RANGE_TEAM]
    = cities[REQ_RANGE_PLAYER];

  cities[REQ_RANGE_CONTINENT] = ai->stats.cities[tile_continent(pcity->tile)];

  /* All the trade partners and the city being considered. */
  cities[REQ_RANGE_TRADEROUTE] = city_num_trade_routes(pcity)+1;

  cities[REQ_RANGE_CITY] = cities[REQ_RANGE_LOCAL] = 1;

  /* Invalid building range */
  cities[REQ_RANGE_ADJACENT] = cities[REQ_RANGE_CADJACENT] = 0;

  players_iterate(aplayer) {
    int potential = aplayer->bulbs_last_turn
      + city_list_size(aplayer->cities) + 1;

    if (potential > 0) {
      requirement_vector_iterate(&pimprove->obsolete_by, pobs) {
        if (pobs->source.kind == VUT_ADVANCE && pobs->present) {
          turns = MIN(turns,
                      research_goal_bulbs_required(research_get(aplayer),
                          advance_number(pobs->source.value.advance))
                      / (potential + 1));
        }
      } requirement_vector_iterate_end;
    }
  } players_iterate_end;

  effect_list_iterate(get_req_source_effects(&source), peffect) {
    struct requirement *mypreq = NULL;
    bool active = TRUE;
    int n_needed_techs = 0;
    struct tech_vector needed_techs;
    bool present = TRUE;
    bool impossible_to_get = FALSE;

    if (is_effect_prevented(pplayer, NULL, pcity, pimprove,
                            NULL, NULL, NULL, NULL, NULL,
                            peffect, RPT_CERTAIN)) {
      /* We believe that effect is disabled only if there is no chance that it
       * is not. This should lead to AI using wider spectrum of improvements.
       *
       * TODO: Select between RPT_POSSIBLE and RPT_CERTAIN dynamically
       * depending how much AI can take risks. */
      continue;
    }

    tech_vector_init(&needed_techs);

    requirement_vector_iterate(&peffect->reqs, preq) {
      /* Check if all the requirements for the currently evaluated effect
       * are met, except for having the building that we are evaluating. */
      if (VUT_IMPROVEMENT == preq->source.kind
	  && preq->source.value.building == pimprove) {
	mypreq = preq;
        present = preq->present;
        continue;
      }
      if (!is_req_active(pplayer, NULL, pcity, pimprove, NULL, NULL, NULL,
                         NULL, NULL, preq, RPT_POSSIBLE)) {
	active = FALSE;
	if (VUT_ADVANCE == preq->source.kind) {
          if (preq->present) {
            /* This missing requirement is a missing tech requirement.
             * This will be for some additional effect
             * (For example, in the default ruleset, Mysticism increases
             * the effect of Temples). */
            tech_vector_append(&needed_techs, preq->source.value.advance);
          } else {
            /* Would require losing a tech - we're not going to do that. */
            impossible_to_get = TRUE;
          }
	}
      }
    } requirement_vector_iterate_end;

    n_needed_techs = tech_vector_size(&needed_techs);
    if ((active || n_needed_techs) && !impossible_to_get) {
      int v1 = dai_effect_value(pplayer, gov, ai, pcity, capital, 
                                turns, peffect, cities[mypreq->range],
                                nplayers, v);
      /* v1 could be negative (the effect could be undesirable),
       * although it is usually positive.
       * For example, in the default ruleset, Communism decreases the
       * effectiveness of a Cathedral. */

      if (!present) {
        /* Building removes the effect */
        /* But getting a tech will not force building the building,
         * so don't hold it against the tech. */
        if (active) {
          /* Currently v1 is (v + delta). Make it (v - delta) instead */ 
          v1 = v - (v1 - v);
        } else {
          /* Tech */
          v1 = MAX(v, -v1);
        }
      }

      if (active) {
	v = v1;
      } else { /* n_needed_techs */
	/* We might want the technology that will enable this
	 * (additional) effect.
	 * The better the effect, the more we want the technology.
	 * Use (v1 - v) rather than v1 in case there are multiple
	 * effects that have technology requirements and to eliminate the
	 * 'base' effect of the building.
         * We are more interested in (additional) effects that enhance
	 * buildings we already have.
	 */
        const int a = already? 5: 4; /* WAG */
        const int dv = (v1 - v) * a / (4 * n_needed_techs);

        want_techs_for_improvement_effect(ait, pplayer, pcity, pimprove,
                                          &needed_techs, dv);
      }
    }

    tech_vector_free(&needed_techs);
  } effect_list_iterate_end;

  /* Can the city be the target of an action? */
  action_iterate (action_id) {
    bool is_possible;
    bool will_be_possible = FALSE;
    enum req_range max_range;
    int act_neg_util;

    /* Is the action relevant? */
    if (action_get_target_kind(action_id) != ATK_CITY) {
      continue;
    }

    /* No range found yet. Local is the most narrow range. */
    max_range = REQ_RANGE_LOCAL;

    /* Is is possible to do the action to the city right now?
     *
     * (DiplRel requirements are ignored since actor_player is NULL) */
    is_possible = is_action_possible_on_city(action_id, NULL, pcity);

    /* Will it be possible to do the action to the city if the building is
     * built? */
    action_enabler_list_iterate(action_enablers_for_action(action_id),
                                enabler) {
      bool active = TRUE;
      enum req_range range = REQ_RANGE_LOCAL;

      requirement_vector_iterate(&(enabler->target_reqs), preq) {
        if (VUT_IMPROVEMENT == preq->source.kind
            && preq->source.value.building == pimprove) {
          /* Pretend the building is there */
          if (preq->present) {
            range = preq->range; /* Assumption: Max one pr vector */
            continue;
          } else {
            active = FALSE;
            break;
          }
        }

        if (!is_req_active(pplayer, NULL, pcity, pimprove,
                           city_tile(pcity), NULL, NULL, NULL, NULL,
                           preq, RPT_POSSIBLE)) {
          active = FALSE;
          break;
        }
      } requirement_vector_iterate_end;

      if (active) {
        will_be_possible = TRUE;

        /* Store the widest range that enables the action. */
        if (max_range < range) {
          max_range = range;
        }

        /* Don't break the iteration even if the action is enabled. There
         * could be a wider range in an active action enabler not yet seen.
         */
      }
    } action_enabler_list_iterate_end;

    /* Will the building significantly change the ability to target
     * the city? */
    if (is_possible == will_be_possible) {
      continue;
    }

    /* How undersireable is it that the city may be a target? */
    act_neg_util = action_target_neg_util(action_id, pcity);

    /* Multiply the desire by number of cities in range.
     * Note: This is a simplification. If the action can be done or not
     * _may_ be uncanged or changed in the opposite direction in the other
     * cities in the range. */
    act_neg_util = cities[max_range] * act_neg_util;

    /* Consider the utility of being a potential target.
     * Remember: act_util is the negative utility of being a target. */
    if (will_be_possible) {
      v -= act_neg_util;
    } else {
      v += act_neg_util;
    }
  } action_iterate_end;

  if (already) {
    /* Discourage research of the technology that would make this building
     * obsolete. The bigger the desire for this building, the more
     * we want to discourage the technology. */
    dont_want_tech_obsoleting_impr(ait, pplayer, pcity, pimprove, v);
  } else {
    /* Increase the want for technologies that will enable
     * construction of this improvement, if necessary.
     */
    const bool all_met = adjust_wants_for_reqs(ait, pplayer, pcity, pimprove, v);
    can_build = can_build && all_met;
  }

  if (is_coinage && can_build) {
    /* Could have a negative want for coinage,
     * if we have some stock in a building already. */
    pcity->server.adv->building_want[improvement_index(pimprove)] += v;
  } else if (!already && can_build) {
    const struct research *presearch = research_get(pplayer);

    /* Convert the base 'want' into a building want
     * by applying various adjustments */

    /* Would it mean losing shields? */
    if ((VUT_UTYPE == pcity->production.kind 
        || (is_wonder(pcity->production.value.building)
          && !is_wonder(pimprove))
        || (!is_wonder(pcity->production.value.building)
          && is_wonder(pimprove)))
     && pcity->turn_last_built != game.info.turn) {
      v -= (pcity->shield_stock / 2) * (SHIELD_WEIGHTING / 2);
    }

    /* Reduce want if building gets obsoleted soon */
    requirement_vector_iterate(&pimprove->obsolete_by, pobs) {
      if (pobs->source.kind == VUT_ADVANCE && pobs->present) {
        v -= v / MAX(1, research_goal_unknown_techs(presearch,
                            advance_number(pobs->source.value.advance)));
      }
    } requirement_vector_iterate_end;

    /* Are we wonder city? Try to avoid building non-wonders very much. */
    if (pcity->id == ai->wonder_city && !is_wonder(pimprove)) {
      v /= 5;
    }

    /* Set */
    pcity->server.adv->building_want[improvement_index(pimprove)] += v;
  }
  /* Else we either have the improvement already,
   * or we can not build it (yet) */
}

/************************************************************************** 
  Whether the AI should calculate the building wants for this city
  this turn, ahead of schedule.

  Always recalculate if the city just finished building,
  so we can make a sensible choice for the next thing to build.
  Perhaps the improvement we were building has become obsolete,
  or a different player built the Great Wonder we were building.
**************************************************************************/
static bool should_force_recalc(struct city *pcity)
{
  return city_built_last_turn(pcity)
      || (VUT_IMPROVEMENT == pcity->production.kind
       && !improvement_has_flag(pcity->production.value.building, IF_GOLD)
       && !can_city_build_improvement_later(pcity, pcity->production.value.building));
}

/************************************************************************** 
  Initialize building advisor. Calculates data of all players, not
  only of those controlled by current ai type.
**************************************************************************/
void dai_build_adv_init(struct ai_type *ait, struct player *pplayer)
{
  struct adv_data *ai = adv_data_get(pplayer, NULL);

  /* Find current worth of cities and cache this. */
  city_list_iterate(pplayer->cities, pcity) {
    def_ai_city_data(pcity, ait)->worth = city_want(pplayer, pcity, ai, NULL);
  } city_list_iterate_end;
}

/************************************************************************** 
  Calculate how much an AI player should want to build particular
  improvements, because of the effects of those improvements, and
  increase the want for technologies that will enable buildings with
  desirable effects.
**************************************************************************/
void dai_build_adv_adjust(struct ai_type *ait, struct player *pplayer,
                          struct city *wonder_city)
{
  /* Clear old building wants.
   * Do this separately from the iteration over improvement types
   * because each iteration could actually update more than one improvement,
   * if improvements have improvements as requirements.
   */
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);

    if (city_data->building_turn <= game.info.turn) {
      /* Do a scheduled recalculation this turn */
      improvement_iterate(pimprove) {
        pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
      } improvement_iterate_end;
    } else if (should_force_recalc(pcity)) {
      /* Do an emergency recalculation this turn. */
      city_data->building_wait = city_data->building_turn
                                        - game.info.turn;
      city_data->building_turn = game.info.turn;

      improvement_iterate(pimprove) {
        pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
      } improvement_iterate_end;
    }
  } city_list_iterate_end;

  improvement_iterate(pimprove) {
    const bool is_coinage = improvement_has_flag(pimprove, IF_GOLD);

    /* Handle coinage specially because you can never complete coinage */
    if (is_coinage
     || can_player_build_improvement_later(pplayer, pimprove)) {
      city_list_iterate(pplayer->cities, pcity) {
        struct ai_city *city_data = def_ai_city_data(pcity, ait);

        if (pcity != wonder_city && is_wonder(pimprove)) {
          /* Only wonder city should build wonders! */
          pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
        } else if ((!is_coinage
                    && !can_city_build_improvement_later(pcity, pimprove))
                   || is_improvement_redundant(pcity, pimprove)) {
          /* Don't consider impossible or redundant buildings */
          pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
        } else if (city_data->building_turn <= game.info.turn) {
          /* Building wants vary relatively slowly, so not worthwhile
           * recalculating them every turn.
           * We DO want to calculate (tech) wants because of buildings
           * we already have. */
          const bool already = city_has_building(pcity, pimprove);

          adjust_improvement_wants_by_effects(ait, pplayer, pcity, 
                                              pimprove, already);

          fc_assert(!(already
                      && 0 < pcity->server.adv->building_want[improvement_index(pimprove)]));

          /* If I am not an expansionist, I want buildings more than units */
          if (pcity->server.adv->building_want[improvement_index(pimprove)] > 0) {
            pcity->server.adv->building_want[improvement_index(pimprove)]
              = pcity->server.adv->building_want[improvement_index(pimprove)]
              * TRAIT_DEFAULT_VALUE
              / ai_trait_get_value(TRAIT_EXPANSIONIST, pplayer);
          }
        }
        /* else wait until a later turn */
      } city_list_iterate_end;
    } else {
      /* An impossible improvement */
      city_list_iterate(pplayer->cities, pcity) {
        pcity->server.adv->building_want[improvement_index(pimprove)] = 0;
      } city_list_iterate_end;
    }
  } improvement_iterate_end;

#ifdef DEBUG
  /* This logging is relatively expensive, so activate only if necessary */
  city_list_iterate(pplayer->cities, pcity) {
    improvement_iterate(pimprove) {
      if (pcity->server.adv->building_want[improvement_index(pimprove)] != 0) {
        CITY_LOG(LOG_DEBUG, pcity, "want to build %s with " ADV_WANT_PRINTF, 
                 improvement_rule_name(pimprove),
                 pcity->server.adv->building_want[improvement_index(pimprove)]);
      }
    } improvement_iterate_end;
  } city_list_iterate_end;
#endif /* DEBUG */

  /* Reset recalc counter */
  city_list_iterate(pplayer->cities, pcity) {
    struct ai_city *city_data = def_ai_city_data(pcity, ait);

    if (city_data->building_turn <= game.info.turn) {
      /* This will spread recalcs out so that no one turn end is 
       * much longer than others */
      city_data->building_wait = fc_rand(AI_BA_RECALC_SPEED) + AI_BA_RECALC_SPEED;
      city_data->building_turn = game.info.turn
        + city_data->building_wait;
    }
  } city_list_iterate_end;
}

/************************************************************************** 
  Is it ok for advisor code to consider given city as wonder city?
**************************************************************************/
void dai_consider_wonder_city(struct ai_type *ait, struct city *pcity, bool *result)
{
  if (def_ai_city_data(pcity, ait)->grave_danger > 0) {
    *result = FALSE;
  } else {
    *result = TRUE;
  }
}

/**************************************************************************
  Returns a buildable, non-obsolete building that can provide the effect.

  Note: this function is an inefficient hack to be used by the old AI.  It
  will never find wonders, since that's not what the AI wants.
**************************************************************************/
Impr_type_id dai_find_source_building(struct city *pcity,
                                      enum effect_type effect_type,
                                      struct unit_type *utype)
{
  int greatest_value = 0;
  struct impr_type *best_building = NULL;

  effect_list_iterate(get_effects(effect_type), peffect) {
    if (peffect->value > greatest_value) {
      struct impr_type *building = NULL;
      bool wrong_unit = FALSE;

      requirement_vector_iterate(&peffect->reqs, preq) {
        if (VUT_IMPROVEMENT == preq->source.kind && preq->present) {
          building = preq->source.value.building;

          if (!can_city_build_improvement_now(pcity, building)
              || !is_improvement(building)) {          
            building = NULL;
            break;
          }
        } else if (utype != NULL
                   && !is_req_active(city_owner(pcity), NULL, pcity, NULL, city_tile(pcity),
                                     NULL, utype, NULL, NULL, preq, RPT_POSSIBLE)) {
          /* Effect requires other kind of unit than what we are interested about */
          wrong_unit = TRUE;
          break;
        }
      } requirement_vector_iterate_end;
      if (!wrong_unit && building != NULL) {
        best_building = building;
	greatest_value = peffect->value;
      }
    }
  } effect_list_iterate_end;

  if (best_building) {
    return improvement_number(best_building);
  }
  return B_LAST;
}
