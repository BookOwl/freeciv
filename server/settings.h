/********************************************************************** 
 Freeciv - Copyright (C) 1996-2004 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__SETTINGS_H
#define FC__SETTINGS_H

#include "shared.h"

#include "game.h"

/* Whether settings are sent to the client when the client lists
 * server options; also determines whether clients can set them in principle.
 * Eg, not sent: seeds, saveturns, etc.
 */
#define SSET_TO_CLIENT TRUE
#define SSET_SERVER_ONLY FALSE

/* Categories allow options to be usefully organized when presented to the user
 */
enum sset_category {
  SSET_GEOLOGY,
  SSET_SOCIOLOGY,
  SSET_ECONOMICS,
  SSET_MILITARY,
  SSET_SCIENCE,
  SSET_INTERNAL,
  SSET_NETWORK,
  SSET_NUM_CATEGORIES
};

extern const char *sset_category_names[];

/* Levels allow options to be subdivided and thus easier to navigate */
enum sset_level {
  SSET_NONE,
  SSET_ALL,
  SSET_VITAL,
  SSET_SITUATIONAL,
  SSET_RARE,
  SSET_CHANGED
};

extern const char *sset_level_names[];
extern const int OLEVELS_NUM;

/* forward declaration */
struct setting;

extern const int SETTINGS_NUM;

struct setting *setting_by_number(int id);
int setting_number(const struct setting *pset);

const char *setting_name(const struct setting *pset);
const char *setting_short_help(const struct setting *pset);
const char *setting_extra_help(const struct setting *pset);
enum sset_type setting_type(const struct setting *pset);
enum sset_level setting_level(const struct setting *pset);

const char *setting_category_name(const struct setting *pset);
const char *setting_level_name(const struct setting *pset);

bool setting_is_changeable(const struct setting *pset,
                           struct connection *caller,
                           const char **reject_msg);
bool setting_is_visible(const struct setting *pset,
                        struct connection *caller);

bool setting_bool_get(const struct setting *pset);
bool setting_bool_def(const struct setting *pset);
bool setting_bool_set(struct setting *pset, bool val,
                      struct connection *caller, const char **reject_msg);
bool setting_bool_validate(const struct setting *pset, bool val,
                           struct connection *caller,
                           const char **reject_msg);

int setting_int_get(const struct setting *pset);
int setting_int_def(const struct setting *pset);
int setting_int_min(const struct setting *pset);
int setting_int_max(const struct setting *pset);
bool setting_int_set(struct setting *pset, int val,
                     struct connection *caller, const char **reject_msg);
bool setting_int_validate(const struct setting *pset, int val,
                          struct connection *caller,
                          const char **reject_msg);

const char *setting_str_get(const struct setting *pset);
const char *setting_str_def(const struct setting *pset);
bool setting_str_set(struct setting *pset, const char *val,
                     struct connection *caller, const char **reject_msg);
bool setting_str_validate(const struct setting *pset, const char *val,
                          struct connection *caller,
                          const char **reject_msg);
void setting_action(const struct setting *pset);

bool setting_locked(const struct setting *pset);
void setting_lock_set(struct setting *pset, bool lock);

/* iterate over all settings */
#define settings_iterate(_pset)                                            \
{                                                                          \
  int id;                                                                  \
  for (id = 0; id < SETTINGS_NUM; id++) {                                  \
    struct setting *_pset = setting_by_number(id);

#define settings_iterate_end                                               \
  }                                                                        \
}

void settings_init(void);
void settings_reset(void);
void settings_turn(void);
void settings_free(void);

bool settings_ruleset(struct section_file *file, const char *section);

void send_server_setting(struct conn_list *dest, const struct setting *pset);
void send_server_settings(struct conn_list *dest);
void send_server_hack_level_settings(struct conn_list *dest);
void send_server_setting_control(struct connection *pconn);

#endif				/* FC__SETTINGS_H */
