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
#ifndef FC__GOVERNMENT_H
#define FC__GOVERNMENT_H

#include "shared.h"

#include "fc_types.h"
#include "requirements.h"

#define G_MAGIC (127)		/* magic constant */

/* special values for free_* fields -- SKi */
#define G_CITY_SIZE_FREE          G_MAGIC

/* each government has a list of ruler titles, where at least
 * one entry should have nation=DEFAULT_TITLE.
 */
#define DEFAULT_TITLE NULL

struct ruler_title
{
  struct nation_type *nation;
  struct name_translation female;
  struct name_translation male;
};

/* This is struct government itself.  All information about
 * a form of government is contained inhere.
 * -- SKi */
struct government
{
  int index;
  struct name_translation name;
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  struct requirement_vector reqs;

  struct ruler_title *ruler_titles;
  int   num_ruler_titles;

  char *helptext;

  /* AI cached data for this government. */
  struct {
    struct government *better; /* hint: a better government (or NULL) */
  } ai;
};

extern struct government *governments;

/* General government accessor functions. */
struct government *government_by_number(const int government_id);
struct government *government_of_player(const struct player *pplayer);
struct government *government_of_city(const struct city *pcity);

const char *government_rule_name(const struct government *gov);
const char *government_name_translation(struct government *gov);
const char *government_name_for_player(const struct player *pplayer);

struct government *find_government_by_rule_name(const char *name);
struct government *find_government_by_translated_name(const char *name);

/* Ancillary routines */
const char *ruler_title_translation(const struct player *pplayer);

bool can_change_to_government(struct player *pplayer,
			      const struct government *gov);

/* Initialization and iteration */
void governments_alloc(int num);
void governments_free(void);

#define government_iterate(gov)                                             \
{                                                                           \
  int GI_index;                                                             \
                                                                            \
  for (GI_index = 0; GI_index < game.control.government_count; GI_index++) {\
    struct government *gov = government_by_number(GI_index);

#define government_iterate_end                                              \
  }                                                                         \
}

#endif  /* FC__GOVERNMENT_H */
