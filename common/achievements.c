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

/* utility */
#include "fcintl.h"
#include "log.h"
#include "rand.h"
#include "shared.h"

/* common */
#include "citizens.h"
#include "culture.h"
#include "game.h"
#include "player.h"
#include "spaceship.h"

#include "achievements.h"

static struct achievement achievements[MAX_ACHIEVEMENT_TYPES];

/****************************************************************
  Initialize achievements.
****************************************************************/
void achievements_init(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(achievements); i++) {
    achievements[i].id = i;
    achievements[i].first = NULL;
    achievements[i].value = 0;
    achievements[i].culture = 0;
    BV_CLR_ALL(achievements[i].achievers);
  }
}

/****************************************************************************
  Free the memory associated with achievements
****************************************************************************/
void achievements_free(void)
{
}

/**************************************************************************
  Return the achievement id.
**************************************************************************/
int achievement_number(const struct achievement *pach)
{
  fc_assert_ret_val(NULL != pach, -1);

  return pach->id;
}

/**************************************************************************
  Return the achievement index.
**************************************************************************/
int achievement_index(const struct achievement *pach)
{
  fc_assert_ret_val(NULL != pach, -1);

  return pach - achievements;
}

/****************************************************************************
  Return achievements of given id.
****************************************************************************/
struct achievement *achievement_by_number(int id)
{
  fc_assert_ret_val(id >= 0 && id < game.control.num_achievement_types, NULL);

  return &achievements[id];
}

/****************************************************************************
  Return translated name of this achievement type.
****************************************************************************/
const char *achievement_name_translation(struct achievement *pach)
{
  return name_translation(&pach->name);
}

/****************************************************************************
  Return untranslated name of this achievement type.
****************************************************************************/
const char *achievement_rule_name(struct achievement *pach)
{
  return rule_name(&pach->name);
}

/**************************************************************************
  Returns achievement matching rule name or NULL if there is no achievement
  with such name.
**************************************************************************/
struct achievement *achievement_by_rule_name(const char *name)
{
  const char *qs = Qn_(name);

  achievements_iterate(pach) {
    if (!fc_strcasecmp(achievement_rule_name(pach), qs)) {
      return pach;
    }
  } achievements_iterate_end;

  return NULL;
}

/****************************************************************************
  Check if some player has now achieved the achievement and return the player
  in question.
****************************************************************************/
struct player *achievement_plr(struct achievement *ach,
                               struct player_list *achievers)
{
  struct player *credited = NULL;

  players_iterate(pplayer) {
    if (achievement_check(ach, pplayer)) {
      if (!ach->unique) {
        pplayer->culture += ach->culture;
        BV_SET(ach->achievers, player_index(pplayer));
      }
      player_list_append(achievers, pplayer);
    }
  } players_iterate_end;

  if (ach->first != NULL) {
    /* Already have first one credited. */
    return NULL;
  }

  if (player_list_size(achievers) > 0) {
    /* If multiple players achieved at the same turn, randomly select one
     * as the one who won the race. */
    credited = player_list_get(achievers, fc_rand(player_list_size(achievers)));

    ach->first = credited;
    credited->culture += ach->culture;

    /* Mark the selected player as the only one having the achievement */
    BV_SET(ach->achievers, player_index(credited));
  }

  return credited;
}

/****************************************************************************
  Check if player has now achieved the achievement.
****************************************************************************/
bool achievement_check(struct achievement *ach, struct player *pplayer)
{
  if ((ach->unique && ach->first != NULL)
      || (BV_ISSET(ach->achievers, player_index(pplayer)))) {
    /* It was already achieved */
    return FALSE;
  }

  switch(ach->type) {
  case ACHIEVEMENT_SPACESHIP:
    return pplayer->spaceship.state == SSHIP_LAUNCHED;
  case ACHIEVEMENT_MAP:
    {
      int max_unknown;
      int required;
      int total;
      int known = 0;
      int unknown = 0;

      /* We calculate max_unknown first for getting the
       * rounding correctly.
       * Consider 50 tile map from which we want 25% known.
       * 50 * 25% = 12.5. Would we round that number of tiles
       * down, we would get < 25% that's minimum requirement.
       * Instead we round down (50 - 12.5 = 37.5) -> 37 and then
       * get the minimum number of full tiles as 50 - 37 = 13. */
      total = map_num_tiles();
      max_unknown = (total * (100 - ach->value)) / 100;
      required = total - max_unknown;

      whole_map_iterate(ptile) {
        bool this_is_known = FALSE;

        if (is_server()) {
          if (dbv_isset(&pplayer->tile_known, tile_index(ptile))) {
            this_is_known = TRUE;
          }
        } else {
          /* Client */
          if (ptile->terrain != T_UNKNOWN) {
            this_is_known = TRUE;
          }
        }

        if (this_is_known) {
          known++;
          if (known >= required) {
            return TRUE;
          }
        } else {
          unknown++;
          if (unknown >= max_unknown) {
            return FALSE;
          }
        }
      } whole_map_iterate_end;
    }

    return FALSE;
  case ACHIEVEMENT_MULTICULTURAL:
    {
      bv_player seen_citizens;
      int count = 0;

      BV_CLR_ALL(seen_citizens);

      city_list_iterate(pplayer->cities, pcity) {
        citizens_iterate(pcity, pslot, pnat) {
          int idx = player_index(player_slot_get_player(pslot));

          if (!BV_ISSET(seen_citizens, idx)) {
            BV_SET(seen_citizens, idx);
            count++;
            if (count >= ach->value) {
              /* There's at least value different nationalities. */
              return TRUE;
            }
          }
        } citizens_iterate_end;
      } city_list_iterate_end;
    }

    return FALSE;
  case ACHIEVEMENT_CULTURED_CITY:
    city_list_iterate(pplayer->cities, pcity) {
      if (city_culture(pcity) >= ach->value) {
        return TRUE;
      }
    } city_list_iterate_end;

    return FALSE;
  case ACHIEVEMENT_CULTURED_NATION:
    if (player_culture(pplayer) >= ach->value) {
      return TRUE;
    }

    return FALSE;
  case ACHIEVEMENT_LUCKY:
    return (fc_rand(10000) < ach->value);
  case ACHIEVEMENT_HUTS:
    return pplayer->server.huts >= ach->value;
  case ACHIEVEMENT_COUNT:
    break;
  }

  log_error("achievement_check(): Illegal achievement type %d", ach->type);

  return FALSE;
}

/****************************************************************************
  Return message to send to first player gaining the achievement.
****************************************************************************/
const char *achievement_first_msg(struct achievement *pach)
{
  static char buf[1024]; /* TODO: Get rid of this */

  switch(pach->type) {
  case ACHIEVEMENT_SPACESHIP:
    return _("You're the first one to launch spaceship towards Alpha Centauri!");
  case ACHIEVEMENT_MAP:
    if (pach->value >= 100) {
      return _("You're the first one to have entire world mapped!");
    } else {
      fc_snprintf(buf, sizeof(buf),
                  _("You're the first one to have %d%% of the world mapped!"),
                    pach->value);
      return buf;
    }
    break;
  case ACHIEVEMENT_MULTICULTURAL:
    fc_snprintf(buf, sizeof(buf),
                _("You're the first one to have %d different nationalities "
                  "in your cities!"), pach->value);
    return buf;
  case ACHIEVEMENT_CULTURED_CITY:
    fc_snprintf(buf, sizeof(buf),
                _("You're the first one to have city of at least %d culture points."),
                pach->value);
    return buf;
  case ACHIEVEMENT_CULTURED_NATION:
    fc_snprintf(buf, sizeof(buf),
                _("You're the first one to have at least %d culture points."),
                pach->value);
    return buf;
  case ACHIEVEMENT_LUCKY:
    fc_snprintf(buf, sizeof(buf),
                _("You beat %d in 10000 odds! You're the first one to be so lucky."),
                pach->value);
    return buf;
  case ACHIEVEMENT_HUTS:
    fc_snprintf(buf, sizeof(buf),
                _("You're the first one to have entered %d huts!"),
                pach->value);
    return buf;
  case ACHIEVEMENT_COUNT:
    break;
  }

  log_error("achievement_first_msg(): Illegal achievement type %d", pach->type);

  return NULL;
}

/****************************************************************************
  Return message to send to other players gaining the achievement.
****************************************************************************/
const char *achievement_later_msg(struct achievement *pach)
{
  static char buf[1024]; /* TODO: Get rid of this */

  switch(pach->type) {
  case ACHIEVEMENT_SPACESHIP:
    return _("You have launched spaceship towards Alpha Centauri!");
  case ACHIEVEMENT_MAP:
    if (pach->value >= 100) {
      return _("You have entire world mapped!");
    } else {
      fc_snprintf(buf, sizeof(buf),
                  _("You have %d%% of the world mapped!"),
                    pach->value);
      return buf;
    }
  case ACHIEVEMENT_MULTICULTURAL:
    fc_snprintf(buf, sizeof(buf),
                _("You have %d different nationalities "
                  "in your cities!"), pach->value);
    return buf;
  case ACHIEVEMENT_CULTURED_CITY:
    fc_snprintf(buf, sizeof(buf),
                _("You have city of %d culture points!"),
                pach->value);
    return buf;
  case ACHIEVEMENT_CULTURED_NATION:
    fc_snprintf(buf, sizeof(buf),
                _("You have %d culture points!"),
                pach->value);
    return buf;
  case ACHIEVEMENT_LUCKY:
    fc_snprintf(buf, sizeof(buf),
                _("You beat %d in 10000 odds!"),
                pach->value);
    return buf;
  case ACHIEVEMENT_HUTS:
    fc_snprintf(buf, sizeof(buf),
                _("You have entered %d huts!"),
                pach->value);
    return buf;
  case ACHIEVEMENT_COUNT:
    break;
  }

  log_error("achievement_later_msg(): Illegal achievement type %d", pach->type);

  return NULL;
}

/****************************************************************************
  Has the given player got the achievement?
****************************************************************************/
bool achievement_player_has(const struct achievement *pach,
                            const struct player *pplayer)
{
  if (pplayer == NULL) {
    return FALSE;
  }

  return BV_ISSET(pach->achievers, player_index(pplayer));
}

/****************************************************************************
  Has anybody got the achievement?
****************************************************************************/
bool achievement_claimed(const struct achievement *pach)
{
  return pach->first != NULL;
}
