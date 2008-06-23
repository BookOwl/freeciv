/**********************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__FC_TYPES_H
#define FC__FC_TYPES_H

/* This file serves to reduce the cross-inclusion of header files which
 * occurs when a type which is defined in one file is needed for a function
 * definition in another file.
 *
 * Nothing in this file should require anything else from the common/
 * directory! */

/* MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS <= 32 !!!! */
#define MAX_NUM_PLAYERS  30
#define MAX_NUM_BARBARIANS   2
#define MAX_NUM_CONNECTIONS (2 * (MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS))
#define MAX_NUM_ITEMS   200     /* eg, unit_types */
#define MAX_NUM_TECH_LIST 10
#define MAX_NUM_UNIT_LIST 10
#define MAX_NUM_BUILDING_LIST 10
#define MAX_LEN_VET_SHORT_NAME 8
#define MAX_VET_LEVELS 10

/* Changing these will probably break network compatability. */
#define MAX_LEN_NAME     32
#define MAX_LEN_DEMOGRAPHY 16
#define MAX_LEN_ALLOW_TAKE 16
#define MAX_LEN_GAME_IDENTIFIER 33
#define MAX_GRANARY_INIS 24
#define MAX_LEN_STARTUNIT (20+1)

/* symbol to flag missing numbers for better debugging */
#define IDENTITY_NUMBER_ZERO (0)

/* Server setting types.  Changing these will break network compatability. */
enum sset_type {
  SSET_BOOL, SSET_INT, SSET_STRING
};

/* The following classes determine what can be changed when.
 * Actually, some of them have the same "changeability", but
 * different types are separated here in case they have
 * other uses.
 * Also, SSET_GAME_INIT/SSET_RULES separate the two sections
 * of server settings sent to the client.
 * See the settings[] array for what these correspond to and
 * explanations.
 */
enum sset_class {
  SSET_MAP_SIZE,
  SSET_MAP_GEN,
  SSET_MAP_ADD,
  SSET_PLAYERS,
  SSET_GAME_INIT,
  SSET_RULES,
  SSET_RULES_FLEXIBLE,
  SSET_META,
  SSET_LAST
};

enum output_type_id {
  O_FOOD, O_SHIELD, O_TRADE, O_GOLD, O_LUXURY, O_SCIENCE, O_LAST
};
#define O_COUNT num_output_types
#define O_MAX O_LAST /* Changing this breaks network compatibility. */

/* Changing this enum will break savegame and network compatability. */
enum unit_activity {
  ACTIVITY_IDLE, ACTIVITY_POLLUTION, ACTIVITY_ROAD, ACTIVITY_MINE,
  ACTIVITY_IRRIGATE, ACTIVITY_FORTIFIED, ACTIVITY_FORTRESS, ACTIVITY_SENTRY,
  ACTIVITY_RAILROAD, ACTIVITY_PILLAGE, ACTIVITY_GOTO, ACTIVITY_EXPLORE,
  ACTIVITY_TRANSFORM, ACTIVITY_UNKNOWN, ACTIVITY_AIRBASE, ACTIVITY_FORTIFYING,
  ACTIVITY_FALLOUT,
  ACTIVITY_PATROL_UNUSED, /* Needed for savegame compatability. */
  ACTIVITY_BASE,          /* Build base */
  ACTIVITY_LAST   /* leave this one last */
};

typedef signed short Continent_id;
typedef int Terrain_type_id;
typedef int Resource_type_id;
typedef int Specialist_type_id;
typedef int Impr_type_id;
typedef int Tech_type_id;
typedef enum output_type_id Output_type_id;
typedef enum unit_activity Activity_type_id;
typedef int Nation_type_id;
typedef int Team_type_id;
typedef int Unit_type_id;

struct advance;
struct city;
struct connection;
struct government;
struct impr_type;
struct nation_type;
struct output_type;
struct player;
struct specialist;
struct terrain;
struct tile;
struct unit;


/* Changing these will break network compatibility. */
#define SP_MAX 20
#define MAX_NUM_REQS 4

#define MAX_NUM_RULESETS 16
#define MAX_RULESET_NAME_LENGTH 64
#define RULESET_SUFFIX ".serv"

/* Unit Class List, also 32-bit vector? */
#define UCL_LAST 32
typedef int Unit_Class_id;

/* This has to be put here for now, otherwise movement.h and unittype.h
 * would have a recursive dependency.
 * Order must mach order in move_type_names array. */
enum unit_move_type {
  AIR_MOVING = 0,
  LAND_MOVING = 1,
  SEA_MOVING = 2,
  HELI_MOVING = 3,
  MOVETYPE_LAST = 4
};

/* The direction8 gives the 8 possible directions.  These may be used in
 * a number of ways, for instance as an index into the DIR_DX/DIR_DY
 * arrays.  Not all directions may be valid; see is_valid_dir and
 * is_cardinal_dir. */
enum direction8 {
  /* The DIR8/direction8 naming system is used to avoid conflict with
   * DIR4/direction4 in client/tilespec.h
   *
   * Changing the order of the directions will break network compatability.
   *
   * Some code assumes that the first 4 directions are the reverses of the
   * last 4 (in no particular order).  See client/goto.c. */
  DIR8_NORTHWEST = 0,
  DIR8_NORTH = 1,
  DIR8_NORTHEAST = 2,
  DIR8_WEST = 3,
  DIR8_EAST = 4,
  DIR8_SOUTHWEST = 5,
  DIR8_SOUTH = 6,
  DIR8_SOUTHEAST = 7
};
#define DIR8_LAST 8
#define DIR8_COUNT DIR8_LAST

/* AI levels. This must correspond to ai_level_names[] in player.c */
enum ai_level {
  AI_LEVEL_AWAY         = 1,
  AI_LEVEL_NOVICE       = 2,
  AI_LEVEL_EASY         = 3,
  AI_LEVEL_NORMAL       = 5,
  AI_LEVEL_HARD         = 7,
  AI_LEVEL_CHEATING     = 8,
  AI_LEVEL_EXPERIMENTAL = 10,
  AI_LEVEL_LAST
};

#define AI_LEVEL_DEFAULT AI_LEVEL_NOVICE

enum editor_tech_mode {
  ETECH_ADD,
  ETECH_REMOVE,
  ETECH_TOGGLE,
  ETECH_LAST
};

/*
 * pplayer->ai.barbarian_type and nations use this enum. Note that the values
 * have to stay since they are used in savegames.
 */
enum barbarian_type {
  NOT_A_BARBARIAN = 0,
  LAND_BARBARIAN = 1,
  SEA_BARBARIAN = 2
};

/* Sometimes we don't know (or don't care) if some requirements for effect
 * are currently fulfilled or not. This enum tells lower level functions
 * how to handle uncertain requirements.
 */
enum req_problem_type {
  RPT_POSSIBLE, /* We want to know if it is possible that effect is active */
  RPT_CERTAIN   /* We want to know if it is certain that effect is active  */
};

#define REVERSED_RPT(x) \
  (x == RPT_CERTAIN ? RPT_POSSIBLE : RPT_CERTAIN)

/* ruleset strings (such as names) are kept in their original vernacular, 
 * translated upon first use.  The translation is cached for future use.
 */
struct name_translation
{
  const char *translated;		/* string doesn't need freeing */
  char vernacular[MAX_LEN_NAME];	/* original string for comparisons */
};

/* Originally in requirements.h, bumped up and revised to unify with
 * city_production and worklists.  Functions remain in requirements.c
 */
typedef union {
  struct advance *advance;
  struct government *govern;
  struct impr_type *building;
  struct nation_type *nation;
  struct specialist *specialist;
  struct terrain *terrain;
  struct unit_class *uclass;
  struct unit_type *utype;

  enum ai_level ai_level;
  int minsize;
  Output_type_id outputtype;
  int terrainclass;			/* enum terrain_class */
  int special;				/* enum tile_special_type */
  int unitclassflag;			/* enum unit_class_flag_id */
  int unitflag;				/* enum unit_flag_id */
} universals_u;

/* The kind of universals_u (value_union_type was req_source_type).
 * Note: order must correspond to universal_names[] in requirements.c.
 */
enum universals_n {
  VUT_NONE,
  VUT_ADVANCE,
  VUT_GOVERNMENT,
  VUT_IMPROVEMENT,
  VUT_SPECIAL,
  VUT_TERRAIN,
  VUT_NATION,
  VUT_UTYPE,
  VUT_UTFLAG,
  VUT_UCLASS,
  VUT_UCFLAG,
  VUT_OTYPE,
  VUT_SPECIALIST,
  VUT_MINSIZE,		/* Minimum size: at city range means city size */
  VUT_AI_LEVEL,		/* AI level of the player */
  VUT_TERRAINCLASS,	/* More generic terrain type, currently "Land" or "Ocean" */
  VUT_LAST
};

struct universal {
  universals_u value;
  enum universals_n kind;		/* formerly .type and .is_unit */
};

struct ai_choice;			/* incorporates universals_u */

#endif /* FC__FC_TYPES_H */
