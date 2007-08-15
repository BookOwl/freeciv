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
#ifndef FC__UNITTYPE_H
#define FC__UNITTYPE_H

#include "shared.h"

#include "fc_types.h"

#define U_LAST MAX_NUM_ITEMS
/*
  U_LAST is a value which is guaranteed to be larger than all
  actual Unit_type_id values.  It is used as a flag value;
  it can also be used for fixed allocations to ensure able
  to hold full number of unit types.
*/

struct move_params {
  bool terrain_affects; /* Move rate is subject to terrain and improvement effects */
  bool damage_slows;    /* Damaged unit is slowed down */
};

struct unit_class {
  Unit_Class_id id;
  struct name_translation name;
  struct move_params move;
  int hp_loss_pct;         /* Percentage of hitpoints lost each turn not in city or airbase */
};

/* Unit "special effects" flags:
   Note this is now an enumerated type, and not power-of-two integers
   for bits, though unit_type.flags is still a bitfield, and code
   which uses unit_has_type_flag() without twiddling bits is unchanged.
   (It is easier to go from i to (1<<i) than the reverse.)
   See data/default/units.ruleset for documentation of their effects.
*/
enum unit_flag_id { 
  F_TRADE_ROUTE=0,
  F_HELP_WONDER,
  F_MISSILE,   
  F_IGZOC,     
  F_NONMIL,      
  F_IGTER,       
  F_CARRIER,     
  F_ONEATTACK,   
  F_PIKEMEN,     
  F_HORSE,       
  F_IGWALL,      
  F_FIELDUNIT,   
  F_AEGIS,       
  F_FIGHTER,     
  F_MARINES,     
  F_PARTIAL_INVIS,    /* Invisibile except when adjacent (Submarine) */   
  F_SETTLERS,         /* Does not include ability to found cities */
  F_DIPLOMAT,    
  F_TRIREME,          /* Trireme sinking effect */
  F_NUCLEAR,          /* Nuclear attack effect */
  F_SPY,              /* Enhanced spy abilities */
  F_TRANSFORM,        /* Can transform terrain types (Engineers) */
  F_PARATROOPERS,
  F_AIRBASE,          /* Can build Airbases */
  F_CITIES,           /* Can build cities */
  F_MISSILE_CARRIER,  /* Like F_CARRIER, but missiles only (Submarine) */
  F_NO_LAND_ATTACK,   /* Cannot attack vs land squares (Submarine) */
  F_ADD_TO_CITY,      /* unit can add to city population */
  F_FANATIC,          /* Only Fundamentalist government can build
			 these units */
  F_GAMELOSS,         /* Losing this unit means losing the game */
  F_UNIQUE,           /* A player can only have one unit of this type */
  F_UNBRIBABLE,       /* Cannot be bribed */
  F_UNDISBANDABLE,    /* Cannot be disbanded, won't easily go away */
  F_SUPERSPY,         /* Always wins diplomatic contests */
  F_NOHOME,           /* Has no homecity */
  F_NO_VETERAN,       /* Cannot increase veteran level */
  F_BOMBARDER,        /* Has the ability to bombard */
  F_CITYBUSTER,       /* Gets double firepower against cities */
  F_NOBUILD,          /* Unit cannot be built (barb leader etc) */
  F_BADWALLATTACKER,  /* Firepower set to 1 when attacking city wall */
  F_BADCITYDEFENDER,  /* Firepower set to 1 and attackers x2 when in city */
  F_LAST
};
#define F_MAX 64

/* Unit "roles": these are similar to unit flags but differ in that
   they don't represent intrinsic properties or abilities of units,
   but determine which units are used (mainly by the server or AI)
   in various circumstances, or "roles".
   Note that in some cases flags can act as roles, eg, we don't need
   a role for "settlers", because we can just use F_SETTLERS.
   (Now have to consider F_CITIES too)
   So we make sure flag values and role values are distinct,
   so some functions can use them interchangably.
   See data/default/units.ruleset for documentation of their effects.
*/
#define L_FIRST F_MAX
enum unit_role_id {
  L_FIRSTBUILD=L_FIRST, /* is built first when city established */
  L_EXPLORER,           /* initial explorer unit */
  L_HUT,                /* can be found in hut */
  L_HUT_TECH,           /* can be found in hut, global tech required */
  L_PARTISAN,           /* is created in Partisan circumstances */
  L_DEFEND_OK,          /* ok on defense (AI) */
  L_DEFEND_GOOD,        /* primary purpose is defense (AI) */
  L_ATTACK_FAST,        /* quick attacking unit (Horse..Armor) (unused)*/
  L_ATTACK_STRONG,      /* powerful attacking unit (Catapult..) (unused) */
  L_FERRYBOAT,	        /* is useful for ferrying (AI) */
  L_BARBARIAN,          /* barbarians unit, land only */
  L_BARBARIAN_TECH,     /* barbarians unit, global tech required */
  L_BARBARIAN_BOAT,     /* barbarian boat */
  L_BARBARIAN_BUILD,    /* what barbarians should build */
  L_BARBARIAN_BUILD_TECH, /* barbarians build when global tech */
  L_BARBARIAN_LEADER,   /* barbarian leader */
  L_BARBARIAN_SEA,      /* sea raider unit */
  L_BARBARIAN_SEA_TECH, /* sea raider unit, global tech required */
  L_CITIES,		/* can found cities */
  L_SETTLERS,		/* can improve terrain */
  L_GAMELOSS,		/* loss results in loss of game */
  L_DIPLOMAT,		/* can do diplomat actions */
  L_HUNTER,             /* AI hunter type unit */
  L_LAST
};
#define L_MAX 64

BV_DEFINE(bv_flags, F_MAX);
BV_DEFINE(bv_roles, L_MAX);

struct veteran_type {
    /* client */
    char name[MAX_LEN_NAME];			/* level/rank name */

    /* server */
    double power_fact;				/* combat/work speed/diplomatic
  						   power factor */
    int move_bonus;
};

struct unit_type {
  int index;
  struct name_translation name;
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  char sound_move[MAX_LEN_NAME];
  char sound_move_alt[MAX_LEN_NAME];
  char sound_fight[MAX_LEN_NAME];
  char sound_fight_alt[MAX_LEN_NAME];
  enum unit_move_type move_type;
  int build_cost;			/* Use wrappers to access this. */
  int pop_cost;  /* number of workers the unit contains (e.g., settlers, engineers)*/
  int attack_strength;
  int defense_strength;
  int move_rate;
  int tech_requirement;
  int impr_requirement;		/* should be Impr_type_id */
  struct government *gov_requirement; /* may be NULL */
  int vision_radius_sq;
  int transport_capacity;
  int hp;
  int firepower;
#define U_NOT_OBSOLETED (NULL)
  struct unit_type *obsoleted_by;
  int fuel;

  bv_flags flags;
  bv_roles roles;

  int happy_cost;  /* unhappy people in home city */
  int upkeep[O_MAX];

  int paratroopers_range; /* only valid for F_PARATROOPERS */
  int paratroopers_mr_req;
  int paratroopers_mr_sub;

  /* Additional values for the expanded veteran system */
  struct veteran_type veteran[MAX_VET_LEVELS];

  /* Values for bombardment */
  int bombard_rate;

  struct unit_class *uclass;
  
  char *helptext;
};


#define CHECK_UNIT_TYPE(ut) (assert((ut) != NULL			    \
			     && (utype_by_number((ut)->index) == (ut))))

/* General unit and unit type (matched) routines */
struct unit_type *unit_type(const struct unit *punit);
struct unit_type *utype_by_number(const Unit_type_id id);

struct unit_type *find_unit_type_by_rule_name(const char *name);
struct unit_type *find_unit_type_by_translated_name(const char *name);

const char *unit_rule_name(const struct unit *punit);
const char *utype_rule_name(const struct unit_type *punittype);

const char *unit_name_translation(struct unit *punit);
const char *utype_name_translation(struct unit_type *punittype);

const char *utype_values_string(const struct unit_type *punittype);
const char *utype_values_translation(struct unit_type *punittype);

/* General unit type flag and role routines */
bool unit_has_type_flag(const struct unit *punit, enum unit_flag_id flag);
bool utype_has_flag(const struct unit_type *punittype, int flag);

bool unit_has_type_role(const struct unit *punit, enum unit_role_id role);
bool utype_has_role(const struct unit_type *punittype, int role);

enum unit_flag_id find_unit_flag_by_rule_name(const char *s);
enum unit_role_id find_unit_role_by_rule_name(const char *s);

const char *unit_flag_rule_name(enum unit_flag_id id);

/* Functions to operate on various flag and roles. */
void role_unit_precalcs(void);
int num_role_units(int role);
struct unit_type *get_role_unit(int role, int index);
struct unit_type *best_role_unit(const struct city *pcity, int role);
struct unit_type *best_role_unit_for_player(const struct player *pplayer,
					    int role);
struct unit_type *first_role_unit_for_player(const struct player *pplayer,
					     int role);
const char *role_units_translations(int flag);

/* General unit class routines */
struct unit_class *unit_class(const struct unit *punit);
struct unit_class *utype_class(const struct unit_type *punittype);
struct unit_class *uclass_by_number(const int id);

struct unit_class *find_unit_class_by_rule_name(const char *s);

const char *uclass_rule_name(const struct unit_class *pclass);
const char *uclass_name_translation(struct unit_class *pclass);

/* Ancillary routines */
int utype_upkeep_cost(const struct unit_type *ut, struct player *pplayer,
                      const struct government *g, Output_type_id otype);
int utype_happy_cost(const struct unit_type *ut, const struct player *pplayer);

struct unit_type *can_upgrade_unittype(const struct player *pplayer,
				       const struct unit_type *punittype);
int unit_upgrade_price(const struct player *pplayer,
		       const struct unit_type *from,
		       const struct unit_type *to);

int unit_build_shield_cost(const struct unit_type *punittype);
int unit_buy_gold_cost(const struct unit_type *punittype,
		       int shields_in_stock);
int unit_disband_shields(const struct unit_type *punittype);
int unit_pop_value(const struct unit_type *punittype);

/* player related unit functions */
bool can_player_build_unit_direct(const struct player *p,
				  const struct unit_type *punittype);
bool can_player_build_unit(const struct player *p,
			   const struct unit_type *punittype);
bool can_player_eventually_build_unit(const struct player *p,
				      const struct unit_type *punittype);

/* Initialization and iteration */
void unit_types_init(void);
void unit_types_free(void);

void unit_classes_init(void);

#define unit_type_iterate(punittype)					    \
{									    \
  int _index;								    \
									    \
  for (_index = 0; _index < game.control.num_unit_types; _index++) {	    \
    struct unit_type *punittype = utype_by_number(_index);

#define unit_type_iterate_end                                               \
  }                                                                         \
}

#endif  /* FC__UNITTYPE_H */
