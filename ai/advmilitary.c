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
#include <string.h>
#include <aitools.h>
#include <game.h>
#include <map.h>
#include <unitfunc.h>
#include <citytools.h>
#include <aicity.h>
#include <aiunit.h>
#include <unittools.h> /* for get_defender, amazingly */
#include <gotohand.h> /* warmap has been redeployed */
#include <settlers.h>
#include <cityturn.h>

extern struct move_cost_map warmap;

/********************************************************************** 
... this function should assign a value to choice and want, where 
    want is a value between 1 and 100.
    if choice is A_NONE this advisor doesn't want any tech researched at
    the moment
***********************************************************************/

void military_advisor_choose_tech(struct player *pplayer,
				  struct ai_choice *choice)
{
  choice->choice = A_NONE;
  choice->want   = 0;
  /* this function haven't been implemented yet */
}

int assess_defense_quadratic(struct city *pcity)
{ /* Need positive feedback in m_a_c_b and bodyguard routines. -- Syela */
  int v, def, l;
  int igwall = 0; /* this can be an arg if needed, but seems unneeded */
  def = 0;
  for (l = 0; l * l < pcity->ai.wallvalue * 10; l++) ;
/* wallvalue = 10, l = 10, wallvalue = 40, l = 20, wallvalue = 90, l = 30 */
  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, punit)
    v = get_defense_power(punit) * punit->hp *
        (is_sailing_unit(punit) ? 1 : get_unit_type(punit->type)->firepower);
    if (is_ground_unit(punit)) v *= 1.5;
    v /= 30;
    if (!igwall && city_got_citywalls(pcity) && is_ground_unit(punit)) {
      v *= l; v /= 10;
    }
    if (is_military_unit(punit)) def += v;
  unit_list_iterate_end;
  if (def > 1<<12) printf("Very large def in assess_defense_quadratic: %d in %s\n",
    def, pcity->name);
  if (def > 1<<15) def = 1<<15; /* more defense than we know what to do with! */
  return(def * def);
}

int assess_defense_unit(struct city *pcity, struct unit *punit, int igwall)
{ /* one unit only, mostly for findjob; handling boats correctly 980803 -- Syela */
  int v;
  v = get_defense_power(punit) * punit->hp *
      (is_sailing_unit(punit) ? 1 : get_unit_type(punit->type)->firepower);
  if (is_ground_unit(punit)) v *= 1.5;
  v /= 30;
  v *= v;
  if (!igwall && city_got_citywalls(pcity) && is_ground_unit(punit)) {
    v *= pcity->ai.wallvalue; v /= 10;
  }
  if (is_military_unit(punit)) return(v);
  return(0);
}

int assess_defense_backend(struct city *pcity, int igwall)
{ /* Most of the time we don't need/want positive feedback. -- Syela */
  int def;
  def = 0;
  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, punit)
    def += assess_defense_unit(pcity, punit, igwall);
  unit_list_iterate_end;
  /* def is an estimate of our total defensive might */
  /* now with regard to the IGWALL nature of the units threatening us -- Syela */
  return(def);
/* unclear whether this should treat settlers/caravans as defense -- Syela */
}

int assess_defense(struct city *pcity)
{
  return(assess_defense_backend(pcity, 0));
}

int assess_defense_igwall(struct city *pcity)
{
  return(assess_defense_backend(pcity, 1));
}

int dangerfunct(int v, int m, int dist)
{
#ifdef OLDCODE
  if (dist * dist < m * 3) { v *= m; v /= 3; } /* knights can't attack more than twice */
  else { v *= m * m; v /= dist * dist; }
  return(v);
#else
  int num, denom;
  num = m * 4; denom = m * 4;
  v *= 2;
  while (dist >= m) {
    v /= 2;
    dist -= m;
  }
  m /= 3;
  while (dist && dist >= m) {
    num *= 4;
    denom *= 5;
    dist -= m;
  }
  while (dist) {
    denom += (denom + m * 2) / (m * 4);
    dist--;
  }
  v = (v*num + (denom>>1)) / denom;
  return(v);
#endif
}

int assess_danger_unit(struct city *pcity, struct unit *punit)
{
  int v = get_unit_type(punit->type)->attack_strength * 10;
  if (punit->veteran) v *= 1.5;
  if (unit_flag(punit->type, F_SUBMARINE)) v = 0;
  if (is_sailing_unit(punit) && !is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN)) v = 0;
/* get_attack_power will be wrong if moves_left == 1 || == 2 */
  v *= punit->hp * get_unit_type(punit->type)->firepower;
  if (city_got_building(pcity, B_COASTAL) && is_sailing_unit(punit)) v >>= 1;
  if (city_got_building(pcity, B_SAM) && is_air_unit(punit)) v >>= 1;
  v /= 30; /* rescaling factor to stop the overflow nonsense */
  return(v);
}

int assess_distance(struct city *pcity, struct unit *punit, int m,
                    int boatid, int boatdist, int boatspeed)
{
  int x, y, dist;
  if (is_tiles_adjacent(punit->x, punit->y, pcity->x, pcity->y)) dist = 3;
  else if (is_sailing_unit(punit)) dist = warmap.seacost[punit->x][punit->y];
  else if (!is_ground_unit(punit))
    dist = real_map_distance(punit->x, punit->y, pcity->x, pcity->y) * 3;
  else if (unit_flag(punit->type, F_IGTER))
    dist = real_map_distance(punit->x, punit->y, pcity->x, pcity->y);
  else dist = warmap.cost[punit->x][punit->y];
/* if dist = 9, a chariot is 1.5 turns away.  NOT 2 turns away. */
/* Samarkand bug should be obsoleted by re-ordering of events */
  if (dist < 3) dist = 3;

  if (is_ground_unit(punit) && boatid &&
      find_beachhead(punit, pcity->x, pcity->y, &x, &y)) {
/* this bug is so obvious I can't believe it wasn't discovered sooner. -- Syela */
    y = warmap.seacost[punit->x][punit->y];
    if (y >= 6 * THRESHOLD)
      y = real_map_distance(pcity->x, pcity->y, punit->x, punit->y) * 3;
    x = MAX(y, boatdist) * m / boatspeed;
    if (dist > x) dist = x;
    if (dist < 3) dist = 3;
  }
  return(dist);
}

int assess_danger(struct city *pcity)
{
  int i, danger = 0, v, dist, con, m;
  int danger2 = 0; /* linear for walls */
  int danger3 = 0; /* linear for coastal */
  int danger4 = 0; /* linear for SAM */
  int danger5 = 0; /* linear for SDI */
  struct player *aplayer, *pplayer;
  int pikemen = 0;
  int urgency = 0;
  int igwall;
  int badmojo = 0;
  int boatspeed;
  int boatid, boatdist;
  int x = pcity->x, y = pcity->y; /* dummy variables */
  struct unit virtualunit;
  struct unit *funit = &virtualunit; /* saves me a lot of typing. -- Syela */

  memset(&virtualunit, 0, sizeof(struct unit));
  pplayer = &game.players[pcity->owner];
  con = map_get_continent(pcity->x, pcity->y); /* Not using because of boats */

  generate_warmap(pcity, 0); /* generates both land and sea maps */

  pcity->ai.grave_danger = 0;

  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, punit)
    if (unit_flag(punit->type, F_PIKEMEN)) pikemen++;
  unit_list_iterate_end;

  for(i=0; i<game.nplayers; i++) {
    if (i != pcity->owner) {
      aplayer = &game.players[i];
      boatspeed = (get_invention(aplayer, A_NAVIGATION) == TECH_KNOWN ? 12 : 6);
      boatid = find_boat(aplayer, &x, &y, 0);
      if (boatid) boatdist = warmap.seacost[x][y];
      else boatdist = -1;
/* should I treat empty enemy cities as danger? */
/* After much contemplation, I've decided the answer is sometimes -- Syela */
      city_list_iterate(aplayer->cities, acity)
        if (acity->is_building_unit &&
            build_points_left(acity) <= acity->shield_surplus) {
          virtualunit.owner = i;
          virtualunit.x = acity->x;
          virtualunit.y = acity->y;
          v = acity->currently_building;
          virtualunit.type = v;
          virtualunit.veteran = do_make_unit_veteran(acity, v);
          virtualunit.hp = unit_types[v].hp;
/* yes, I know cloning all this code is bad form.  I don't really
want to write a funct that takes nine ints by reference. -- Syela */
          m = get_unit_type(funit->type)->move_rate;
          v = assess_danger_unit(pcity, funit);
          dist = assess_distance(pcity, funit, m, boatid, boatdist, boatspeed);
          igwall = 0;
          if (unit_ignores_citywalls(funit) || 
              (!is_heli_unit(funit) && !is_ground_unit(funit))) igwall++;
          if ((is_ground_unit(funit) && v) ||
             (get_transporter_capacity(funit) &&
              !unit_flag(funit->type, F_SUBMARINE) &&
              !unit_flag(funit->type, F_CARRIER))) {

            if (dist <= m * 3) urgency++;
            if (dist <= m) pcity->ai.grave_danger++;
/* NOTE: This should actually implement grave_danger, which is supposed
to be a feedback-sensitive formula for immediate danger.  I'm having
second thoughts about the best implementation, and therefore this
will have to wait until after 1.7.0.  I don't want to do anything I'm
not totally sure about and can't thoroughly test in the hours before
the release.  The AI is in fact vulnerable to gang-attacks, but I'm
content to let it remain that way for now. -- Syela 980805 */
          }

          if (unit_flag(funit->type, F_HORSE)) {
            if (pikemen) v >>= 1;
            else if (get_invention(pplayer, A_FEUDALISM) != TECH_KNOWN)
              pplayer->ai.tech_want[A_FEUDALISM] += v * m / (dist<<1);
          }

          v *= v;

          if (!igwall) danger2 += v * m / dist;
          else if (is_sailing_unit(funit)) danger3 += v * m / dist;
          else if (is_air_unit(funit) && !unit_flag(funit->type, F_NUCLEAR)) danger4 += v * m / dist;
          if (unit_flag(funit->type, F_MISSILE)) danger5 += v * m / MAX(m, dist);
          if (!unit_flag(funit->type, F_NUCLEAR)) { /* only SDI helps against NUCLEAR */
            v = dangerfunct(v, m, dist);
            danger += v;
            if (igwall) badmojo += v;
          }
        }
      city_list_iterate_end;

      unit_list_iterate(aplayer->units, punit)
        m = get_unit_type(punit->type)->move_rate;
        v = assess_danger_unit(pcity, punit);
        dist = assess_distance(pcity, punit, m, boatid, boatdist, boatspeed);
        igwall = 0;
        if (unit_ignores_citywalls(punit) || 
            (!is_heli_unit(punit) && !is_ground_unit(punit))) igwall++;
          
        if ((is_ground_unit(punit) && v) ||
           (get_transporter_capacity(punit) &&
            !unit_flag(punit->type, F_SUBMARINE) &&
            !unit_flag(punit->type, F_CARRIER))) {
          if (dist <= m * 3) urgency++;
          if (dist <= m) pcity->ai.grave_danger++;
/* NOTE: This should actually implement grave_danger, which is supposed
to be a feedback-sensitive formula for immediate danger.  I'm having
second thoughts about the best implementation, and therefore this
will have to wait until after 1.7.0.  I don't want to do anything I'm
not totally sure about and can't thoroughly test in the hours before
the release.  The AI is in fact vulnerable to gang-attacks, but I'm
content to let it remain that way for now. -- Syela 980805 */
        }

        if (unit_flag(punit->type, F_HORSE)) {
          if (pikemen) v >>= 1;
          else if (get_invention(pplayer, A_FEUDALISM) != TECH_KNOWN)
            pplayer->ai.tech_want[A_FEUDALISM] += v * m / (dist<<1);
        }

        v *= v;

        if (!igwall) danger2 += v * m / dist;
        else if (is_sailing_unit(punit)) danger3 += v * m / dist;
        else if (is_air_unit(punit) && !unit_flag(punit->type, F_NUCLEAR)) danger4 += v * m / dist;
        if (unit_flag(punit->type, F_MISSILE)) danger5 += v * m / MAX(m, dist);
        if (!unit_flag(punit->type, F_NUCLEAR)) { /* only SDI helps against NUCLEAR */
          v = dangerfunct(v, m, dist);
          danger += v;
          if (igwall) badmojo += v;
        }
      unit_list_iterate_end;
    }
  } /* end for */

  if (!badmojo) pcity->ai.wallvalue = 90;
  else pcity->ai.wallvalue = (danger * 9 - badmojo * 8) * 10 / (danger);
/* afraid *100 would create integer overflow, but *10 surely won't */

  if (danger < 0 || danger > 1<<24) /* I hope never to see this! */
    printf("Dangerous danger (%d) in %s.  Beware of overflow.\n", danger, pcity->name);
  if (danger2 < 0 || danger2 > 1<<24) /* I hope never to see this! */
    printf("Dangerous danger2 (%d) in %s.  Beware of overflow.\n", danger2, pcity->name);
  if (danger3 < 0 || danger3 > 1<<24) /* I hope never to see this! */
    printf("Dangerous danger3 (%d) in %s.  Beware of overflow.\n", danger3, pcity->name);
  if (danger4 < 0 || danger4 > 1<<24) /* I hope never to see this! */
    printf("Dangerous danger4 (%d) in %s.  Beware of overflow.\n", danger4, pcity->name);
  if (danger5 < 0 || danger5 > 1<<24) /* I hope never to see this! */
    printf("Dangerous danger5 (%d) in %s.  Beware of overflow.\n", danger5, pcity->name);
  if (pcity->ai.grave_danger) urgency += 10; /* really, REALLY urgent to defend */
  pcity->ai.danger = danger;
  if (pcity->ai.building_want[B_CITY] > 0 && danger2) {
    i = assess_defense(pcity);
    if (!i) pcity->ai.building_want[B_CITY] = 100 + urgency;
    else if (urgency) {
      if (danger2 > i * 2) pcity->ai.building_want[B_CITY] = 200 + urgency;
      else pcity->ai.building_want[B_CITY] = danger2 * 100 / i;
    } else {
      if (danger2 > i) pcity->ai.building_want[B_CITY] = 100;
      else pcity->ai.building_want[B_CITY] = danger2 * 100 / i;
    }
  }
/* My first attempt to allow ng_wa >= 200 led to stupidity in cities with
no defenders and danger = 0 but danger > 0.  Capping ng_wa at 100 + urgency
led to a failure to buy walls as required.  Allowing want > 100 with !urgency
led to the AI spending too much gold and falling behind on science.  I'm
trying again, but this will require yet more tedious observation -- Syela */
  if (pcity->ai.building_want[B_COASTAL] > 0 && danger3) {
    i = assess_defense_igwall(pcity);
    if (!i) pcity->ai.building_want[B_COASTAL] = 100 + urgency;
    else if (urgency) {
      if (danger3 > i * 2) pcity->ai.building_want[B_COASTAL] = 200 + urgency;
      else pcity->ai.building_want[B_COASTAL] = danger3 * 100 / i;
    } else {
      if (danger3 > i) pcity->ai.building_want[B_COASTAL] = 100;
      else pcity->ai.building_want[B_COASTAL] = danger3 * 100 / i;
    }
  }
/* COASTAL and SAM are TOTALLY UNTESTED and could be VERY WRONG -- Syela */
/* update 980803; COASTAL seems to work; still don't know about SAM. -- Syela */
  if (pcity->ai.building_want[B_SAM] > 0 && danger4) {
    i = assess_defense_igwall(pcity);
    if (!i) pcity->ai.building_want[B_SAM] = 100 + urgency;
    else if (danger4 > i * 2) pcity->ai.building_want[B_SAM] = 200 + urgency;
    else pcity->ai.building_want[B_SAM] = danger4 * 100 / i;
  }
  if (pcity->ai.building_want[B_SDI] > 0 && danger5) {
    i = assess_defense_igwall(pcity);
    if (!i) pcity->ai.building_want[B_SDI] = 100 + urgency;
    else if (danger5 > i * 2) pcity->ai.building_want[B_SDI] = 200 + urgency;
    else pcity->ai.building_want[B_SDI] = danger5 * 100 / i;
  }
  pcity->ai.urgency = urgency; /* need to cache this for bodyguards now -- Syela */
  return(urgency);
}

int unit_desirability(int i, int def)
{
  int cur, a, d;   
  cur = get_unit_type(i)->hp;
  if (unit_types[i].move_type != SEA_MOVING || !def)
    cur *= get_unit_type(i)->firepower;
  if (def) cur *= 3;
  else cur *= get_unit_type(i)->move_rate;
  a = get_unit_type(i)->attack_strength;
  d = get_unit_type(i)->defense_strength;
  if (def) cur *= d;
  else if (d > a) return(0);
/*  else if (d < 2) cur = (cur * (a + d))>>1; Don't believe in this anymore */
  else cur *= a; /* wanted to rank Legion > Catapult > Archer */
/* which we will do by munging f in the attacker want equations */
  if (unit_flag(i, F_IGTER) && !def) cur *= 3;
  if (unit_flag(i, F_PIKEMEN) && def) cur *= 1.5;
  if (unit_types[i].move_type == LAND_MOVING && def) cur *= 1.5;
  return(cur);  
}

void process_defender_want(struct player *pplayer, struct city *pcity, int danger,
                           struct ai_choice *choice)
{
  int i, j, k, l, m, n;
  int best= 0;
  int bestid = 0;
  int walls = city_got_citywalls(pcity);
  int desire[U_LAST]; /* what you get is what you seek */
  int shore = is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN);
  int isdef = has_a_normal_defender(pcity);

  memset(desire, 0, sizeof(desire));
  for (i = U_WARRIORS; i <= U_BATTLESHIP; i++) {
    m = unit_types[i].move_type;
    if ((m == LAND_MOVING || m == SEA_MOVING)) {
      k = pplayer->ai.tech_turns[unit_types[i].tech_requirement];
      j = unit_desirability(i, 1);
      if (!isdef && unit_flag(i, F_FIELDUNIT)) j = 0;
      j /= 15; /* good enough, no rounding errors */
      j *= j;
      if (can_build_unit(pcity, i)) {
        if (walls && m == LAND_MOVING) { j *= pcity->ai.wallvalue; j /= 10; }
        if ((j > best || (j == best && get_unit_type(i)->build_cost <=
                               get_unit_type(bestid)->build_cost)) &&
            unit_types[i].build_cost <= pcity->shield_stock + 40) {
          best = j;
          bestid = i;
        }
      } else if (k > 0 && (shore || m == LAND_MOVING) &&
                unit_types[i].tech_requirement != A_LAST) {
        if (m == LAND_MOVING) { j *= pcity->ai.wallvalue; j /= 10; }
        l = k * (k + pplayer->research.researchpoints) * game.techlevel /
         (game.year > 0 ? 2 : 4); /* cost (shield equiv) of gaining these techs */
        l /= city_list_size(&pplayer->cities);
/* Katvrr advises that with danger high, l should be weighted more heavily */
        desire[i] = j * danger / (unit_types[i].build_cost + l);
      }
    }
  }
  if (!walls && unit_types[bestid].move_type == LAND_MOVING) {
    best *= pcity->ai.wallvalue;
    best /= 10;
  } /* was getting four-figure desire for battleships otherwise. -- Syela */
/* Phalanx would be 16 * danger / 20.  Pikemen would be 36 * danger / (20 + l) */
/* multiply by unit_types[bestid].build_cost / best */
  for (i = U_WARRIORS; i <= U_BATTLESHIP; i++) {
    if (desire[i]) {
      j = unit_types[i].tech_requirement;
      n = desire[i] * unit_types[bestid].build_cost / best;
      pplayer->ai.tech_want[j] += n; /* not the totally idiotic
      pplayer->ai.tech_want[j] += n * pplayer->ai.tech_turns[j];  I amaze myself. -- Syela */
/*      printf("%s wants %s for defense with desire %d <%d>\n",
pcity->name, advances[j].name, n, desire[i]); */
    }
  }
  choice->choice = bestid;
  choice->want = danger;
  choice->type = 3;
  return;
}

void process_attacker_want(struct player *pplayer, struct city *pcity, int b, int n,
                            int vet, int x, int y, int unhap, int *e0, int *v,
                            int bx, int by, int boatspeed, int needferry)
{ 
/* n is type of defender, vet is vet status, x,y is location of target */
/* I decided this funct wasn't confusing enough, so I made k_s_w send
it some more variables for it to meddle with -- Syela */
  int a, c, d, e, i, a0, b0, f, g, fprime;
  int j, k, l, m, q;
  int shore = is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN);
  struct city *acity = map_get_city(x, y);
  int movetype = unit_types[*v].move_type;

  for (i = U_WARRIORS; i <= U_BATTLESHIP; i++) {
    m = unit_types[i].move_type;
    j = unit_types[i].tech_requirement;
    if (j != A_LAST) k = pplayer->ai.tech_turns[j];
    else k = 0;
    if ((m == LAND_MOVING || (m == SEA_MOVING && shore)) && j != A_LAST &&
         (k || !can_build_unit_direct(pcity, unit_types[i].obsoleted_by)) &&
         unit_types[i].attack_strength && /* otherwise we get SIGFPE's */
         m == movetype) { /* I don't think I want the duplication otherwise -- Syela */
      l = k * (k + pplayer->research.researchpoints) * game.techlevel;
      if (game.year > 0) l >>= 1;
      else l >>= 2; /* cost (shield equiv) of gaining these techs */
      l /= city_list_size(&pplayer->cities);
/* Katvrr advises that with danger high, l should be weighted more heavily */

      a = unit_types[i].attack_strength *  ((m == LAND_MOVING ||
          get_invention(pplayer, A_AMPHIBIOUS) == TECH_KNOWN) ? 15 : 10) *
             unit_types[i].firepower * unit_types[i].hp;
      a /= 30; /* scaling factor to prevent integer overflows */
      if (acity) a += acity->ai.a;
      a *= a;
      /* b is unchanged */

      m = unit_types[i].move_rate;
      q = (acity ? 1 : unit_types[n].move_rate * (unit_flag(n, F_IGTER) ? 3 : 1));
      if (get_unit_type(i)->flags & F_IGTER) m *= 3; /* not quite right */
      if (unit_types[i].move_type == LAND_MOVING) {
        if (boatspeed) { /* has to be a city, so don't bother with q */
          c = (warmap.cost[bx][by] + m - 1) / m + 1 +
                warmap.seacost[x][y] / boatspeed; /* kluge */
          if (unit_flag(i, F_MARINES)) c -= 1;
        } else if (warmap.cost[x][y] <= m) c = 1;
        else c = (warmap.cost[x][y] * q + m - 1) / m;
      } else if (unit_types[i].move_type == SEA_MOVING) {
        if (warmap.seacost[x][y] <= m) c = 1;
        else c = (warmap.seacost[x][y] * q + m - 1) / m;
      } else if (real_map_distance(pcity->x, pcity->y, x, y) * 3 <= m) c = 1;
      else c = real_map_distance(pcity->x, pcity->y, x, y) * 3 * q / m;

      m = get_virtual_defense_power(i, n, x, y);
      m *= unit_types[n].hp * unit_types[n].firepower;
      if (vet) m *= 1.5;
      m /= 30;
      m *= m;
      d = m;

      if (unit_types[i].move_type == LAND_MOVING && acity &&
          c > (get_invention(get_player(acity->owner), A_MASONRY) == TECH_KNOWN ? 2 : 4) &&
          !unit_flag(i, F_IGWALL) && !city_got_citywalls(acity)) d *= 9; 

      f = unit_types[i].build_cost;
      fprime = f * 2 * unit_types[i].attack_strength /
           (unit_types[i].attack_strength +
            unit_types[i].defense_strength);

      if (acity) g = unit_list_size(&(map_get_tile(acity->x, acity->y)->units)) + 1;
      else g = 1;
      if (unit_types[i].move_type != LAND_MOVING && !d) b0 = 0;
/* not bothering to s/!d/!pdef here for the time being -- Syela */
      else if ((unit_types[i].move_type == LAND_MOVING ||
              unit_types[i].move_type == HELI_MOVING) && acity &&
              acity->ai.invasion == 2) b0 = f * SHIELD_WEIGHTING;
      else {
        b0 = (b * a - (f + (acity ? acity->ai.f : 0)) * d) * g * SHIELD_WEIGHTING / (a + g * d);
        if (acity && b * acity->ai.a * acity->ai.a > acity->ai.f * d)
          b0 -= (b * acity->ai.a * acity->ai.a - acity->ai.f * d) *
                           g * SHIELD_WEIGHTING / (acity->ai.a * acity->ai.a + g * d);
      }
      if (b0 > 0) {
        b0 -= l * SHIELD_WEIGHTING;
        b0 -= c * (unhap ? SHIELD_WEIGHTING + 2 * TRADE_WEIGHTING : SHIELD_WEIGHTING);
      }
      if (b0 > 0) {
        a0 = amortize(b0, MAX(1, c));
        e = ((a0 * b0) / (MAX(1, b0 - a0))) * 100 / ((fprime + needferry) * MORT);
      } else e = 0;  
      if (e > 0) {
        if (k) {
          pplayer->ai.tech_want[j] += e;
/*if (movetype == SEA_MOVING) printf("%s wants %s to punish %s@(%d, %d) with desire %d.\n", 
pcity->name, unit_name(i), (acity ? acity->name : "punit"), x, y, e);*/
        } else if (e > *e0) {
/*printf("%s overriding %s(%d) with %s(%d) [a=%d,b=%d,c=%d,d=%d,f=%d]\n",
pcity->name, unit_name(*v), *e0, unit_name(i), e, a, b, c, d, f);*/
          *v = i;
          *e0 = e;
        }
      }
    }
  }
}

void kill_something_with(struct player *pplayer, struct city *pcity, 
                         struct unit *myunit, struct ai_choice *choice)
{
  int a, b, c, d, e, f, g; /* variables in the attacker-want equation */
  int m, n, vet, dist, v, a0, b0, fprime;
  int x, y, unhap = 0;
  struct unit *pdef, *aunit, *ferryboat;
  struct city *acity;
  int boatid = 0, harborcity = 0, bx, by;
  int needferry = 0, fstk, boatspeed, sanity;

  if (pcity->ai.danger && !assess_defense(pcity)) return;

  if (is_ground_unit(myunit)) boatid = find_boat(pplayer, &bx, &by, 2);
  if (is_ground_unit(myunit) && !myunit->id &&
      is_terrain_near_tile(myunit->x, myunit->y, T_OCEAN))
    harborcity++;

  ferryboat = unit_list_find(&pplayer->units, boatid);
  if (ferryboat) boatspeed = (unit_flag(ferryboat->type, F_TRIREME) ? 6 : 12);
  else boatspeed = (get_invention(pplayer, A_NAVIGATION) != TECH_KNOWN ? 6 : 12);

  fstk = find_something_to_kill(pplayer, myunit, &x, &y);

  acity = map_get_city(x, y);
  if (!acity) aunit = get_defender(pplayer, myunit, x, y);
  else aunit = 0;
  if (acity && acity->owner == pplayer->player_no) acity = 0;

/* this code block and the analogous block in f_s_t_k should be calls
to the same utility function, but I don't want to break anything right
before the 1.7.0 release so I'm letting this stay ugly. -- Syela */

/* logically we should adjust this for race attack tendencies */
  if ((acity || aunit)) {
    v = myunit->type;
    vet = myunit->veteran;

    a = unit_types[v].attack_strength * (vet ? 15 : 10) *
             unit_types[v].firepower * myunit->hp;
    a /= 30; /* scaling factor to prevent integer overflows */
    if (acity) a += acity->ai.a;
    a *= a;

    if (acity) {
      pdef = get_defender(pplayer, myunit, x, y);

      m = unit_types[v].move_rate;
      if (get_unit_type(v)->flags & F_IGTER) m *= 3; /* not quite right */

      sanity = (goto_is_sane(pplayer, myunit, acity->x, acity->y, 1) &&
              warmap.cost[x][y] <= (MIN(6, m) * THRESHOLD));

      if (is_ground_unit(myunit)) {
        if (!sanity) {
          if (boatid) c = (warmap.cost[bx][by] + m - 1) / m + 1 +
                warmap.seacost[acity->x][acity->y] / boatspeed; /* kluge */
          else c = warmap.seacost[acity->x][acity->y] / boatspeed + 1;
          if (unit_flag(myunit->type, F_MARINES)) c -= 1;
/*printf("%s attempting to attack via ferryboat (boatid = %d, c = %d)\n",
unit_types[v].name, boatid, c);*/
        } else c = (warmap.cost[acity->x][acity->y] + m - 1) / m;
      } else if (is_sailing_unit(myunit))
        c = (warmap.seacost[acity->x][acity->y] + m - 1) / m;
      else c = real_map_distance(myunit->x, myunit->y, acity->x, acity->y) * 3 / m;

      n = ai_choose_defender_versus(acity, v);
      m = get_virtual_defense_power(v, n, x, y);
      m *= unit_types[n].hp * unit_types[n].firepower;
      if (do_make_unit_veteran(acity, n)) m *= 1.5;
      m /= 30;
      if (c > 1) {
        d = m * m;
        b = unit_types[n].build_cost + 40;
        vet = do_make_unit_veteran(acity, n);
      } else {
        d = 0;
        b = 40;
        vet = 0;
      }
      if (pdef) {
/*        n = pdef->type;    Now, really, I could not possibly have written this.
Yet, somehow, this line existed, and remained here for months, bugging the AI
tech progression beyond all description.  Only when adding the override code
did I realize the magnitude of my transgression.  How despicable. -- Syela */
        m = get_virtual_defense_power(v, pdef->type, x, y);
        if (pdef->veteran) m *= 1.5; /* with real defenders, this must be before * hp -- Syela */
        m *= (myunit->id ? pdef->hp : unit_types[pdef->type].hp) *
              unit_types[pdef->type].firepower;
/*        m /= (pdef->veteran ? 20 : 30);  -- led to rounding errors.  Duh! -- Syela */
        m /= 30;
        if (d < m * m) {
          d = m * m;
          b = unit_types[pdef->type].build_cost + 40; 
          vet = pdef->veteran;
          n = pdef->type; /* and not before, or heinous things occur!! */
        }
      }
      if (!is_ground_unit(myunit) && !is_heli_unit(myunit) &&
         (!(acity->ai.invasion&1))) b -= 40; /* boats can't conquer cities */
      if (!myunit->id && (is_ground_unit(myunit) || is_heli_unit(myunit)) &&
          c > (get_invention(get_player(acity->owner), A_MASONRY) == TECH_KNOWN ? 2 : 4) &&
          !unit_flag(myunit->type, F_IGWALL) && !city_got_citywalls(acity)) d *= 9;
    } /* end dealing with cities */

    else {
      m = 0;
      pdef = aunit; /* we KNOW this is the get_defender -- Syela */

      m = unit_types[pdef->type].build_cost;
      b = m;
      sanity = 1;

      if (is_ground_unit(myunit)) dist = warmap.cost[x][y];
      else if (is_sailing_unit(myunit)) dist = warmap.seacost[x][y];
      else dist = real_map_distance(pcity->x, pcity->y, x, y) * 3;
      if (dist > m) {
        dist *= unit_types[pdef->type].move_rate;
        if (unit_flag(pdef->type, F_IGTER)) dist *= 3;
      }
      if (!dist) dist = 1;

      m = unit_types[v].move_rate;
      if (get_unit_type(v)->flags & F_IGTER) m *= 3; /* not quite right */
      c = ((dist + m - 1) / m);

      n = pdef->type;
      m = get_virtual_defense_power(v, n, x, y);
      if (pdef->veteran) m += m / 2;
      if (pdef->activity == ACTIVITY_FORTIFY) m += m / 2;
/* attempting to recreate the rounding errors in get_total_defense_power -- Syela */

      m *= (myunit->id ? pdef->hp : unit_types[n].hp) * unit_types[n].firepower;
/* let this be the LAST discrepancy!  How horribly many there have been! -- Syela */
/*     m /= (pdef->veteran ? 20 : 30);*/
      m /= 30;
      m *= m;
      d = m;
      vet = pdef->veteran;
    } /* end dealing with units */

    if (get_government(pplayer->player_no) == G_REPUBLIC &&
        (pcity->id == myunit->homecity || !myunit->id)) {
      unit_list_iterate(pcity->units_supported, punit)
        if (unit_being_aggressive(punit)) {
          unhap++;
        } else if (is_field_unit(punit)) {
          unhap++;
        }
      unit_list_iterate_end;
      if (city_got_effect(pcity, B_POLICE)) unhap--;
    } /* handle other governments later */

    if (is_ground_unit(myunit) && !sanity && !boatid)
      needferry = 40; /* cost of ferry */
    f = unit_types[v].build_cost;
    fprime = f * 2 * unit_types[v].attack_strength /
           (unit_types[v].attack_strength +
            unit_types[v].defense_strength);

    if (acity) g = unit_list_size(&(map_get_tile(acity->x, acity->y)->units)) + 1;
    else g = 1;
    if (unit_types[v].move_type != LAND_MOVING &&
        unit_types[v].move_type != HELI_MOVING && !pdef) b0 = 0;
/* !d instead of !pdef was horrible bug that led to yoyo-ing AI warships -- Syela */
    else if (c > THRESHOLD) b0 = 0;
    else if ((unit_types[v].move_type == LAND_MOVING ||
              unit_types[v].move_type == HELI_MOVING) && acity &&
              acity->ai.invasion == 2) b0 = f * SHIELD_WEIGHTING;
    else {
      b0 = (b * a - (f + (acity ? acity->ai.f : 0)) * d) * g * SHIELD_WEIGHTING / (a + g * d);
      if (acity && b * acity->ai.a * acity->ai.a > acity->ai.f * d)
        b0 -= (b * acity->ai.a * acity->ai.a - acity->ai.f * d) *
               g * SHIELD_WEIGHTING / (acity->ai.a * acity->ai.a + g * d);
    }
    b0 -= c * (unhap ? SHIELD_WEIGHTING + 2 * TRADE_WEIGHTING : SHIELD_WEIGHTING);
    if (b0 > 0) {
      a0 = amortize(b0, MAX(1, c));
      e = ((a0 * b0) / (MAX(1, b0 - a0))) * 100 / ((fprime + needferry) * MORT);
    } else e = 0;  

/*if (e != fstk && acity) {
printf("%s (%d, %d), %s#%d -> %s[%d] (%d, %d) [A=%d, B=%d, C=%d, D=%d, E=%d/%d, F=%d]\n",
pcity->name, pcity->x, pcity->y, unit_types[v].name, myunit->id,
acity->name, acity->ai.invasion, acity->x, acity->y, a, b, c, d, e, fstk, f);
} else if (e != fstk && aunit) {
printf("%s (%d, %d), %s#%d -> %s (%d, %d) [A=%d, B=%d, C=%d, D=%d, E=%d/%d, F=%d]\n", 
pcity->name, pcity->x, pcity->y, unit_types[v].name, myunit->id,
unit_types[pdef->type].name, pdef->x, pdef->y, a, b, c, d, e, fstk, f);
}*/

    if (!myunit->id) {  /* I don't want to know what happens otherwise -- Syela */
      if (sanity)
        process_attacker_want(pplayer, pcity, b, n, vet, x, y, unhap,
           &e, &v, 0, 0, 0, needferry);
      else if (boatid) process_attacker_want(pplayer, pcity, b, n, vet, x, y, unhap,
           &e, &v, bx, by, boatspeed, needferry);
      else process_attacker_want(pplayer, pcity, b, n, vet, x, y, unhap,
           &e, &v, myunit->x, myunit->y, boatspeed, needferry);
    }

    if (e > choice->want && /* Without this &&, the AI will try to make attackers */
        choice->want <= 100) { /* instead of defenders when being attacked -- Syela */
      if (!city_got_barracks(pcity) && is_ground_unit(myunit)) {
        if (get_invention(pplayer, A_COMBUSTION) == TECH_KNOWN)
          choice->choice = B_BARRACKS3;
        else if (get_invention(pplayer, A_GUNPOWDER) == TECH_KNOWN)
          choice->choice = B_BARRACKS2;
        else choice->choice = B_BARRACKS;
        choice->want = e;
        choice->type = 0;
      } else {
        if (!myunit->id) {
          choice->choice = v;
          choice->type = 2; /* type == 2 identifies attackers */
          choice->want = e;
          if (needferry) ai_choose_ferryboat(pplayer, pcity, choice);
/*printf("%s has chosen attacker, %s\n", pcity->name, unit_types[choice->choice].name);*/
        } else {
          choice->choice = ai_choose_defender(pcity);
/*printf("%s has chosen defender, %s\n", pcity->name, unit_types[choice->choice].name);*/
          choice->type = 1;
          choice->want = e;
        }
        if (is_sailing_unit(myunit) && !city_got_building(pcity, B_PORT)) {
          if (get_invention(pplayer, A_AMPHIBIOUS) == TECH_KNOWN) {
            choice->choice = B_PORT;
            choice->want = e;
            choice->type = 0;
          } else pplayer->ai.tech_want[A_AMPHIBIOUS] += e;
        }
      }
    }
  } 
}

int port_is_within(struct player *pplayer, int d)
{
  city_list_iterate(pplayer->cities, pcity)
    if (city_got_building(pcity, B_PORT) &&
        warmap.seacost[pcity->x][pcity->y] <= d) return 1;
    if (!pcity->is_building_unit && pcity->currently_building == B_PORT &&
        pcity->shield_stock >= improvement_value(B_PORT) &&
        warmap.seacost[pcity->x][pcity->y] <= d) return 1;
    if (get_invention(pplayer, A_AMPHIBIOUS) != TECH_KNOWN &&
        pcity->is_building_unit &&
        is_water_unit(pcity->currently_building) &&
        unit_types[pcity->currently_building].attack_strength >
        unit_types[pcity->currently_building].transport_capacity) return 1;
  city_list_iterate_end;
  return 0;
}

/********************************************************************** 
... this function should assign a value to choice and want and type, 
    where want is a value between 1 and 100.
    if want is 0 this advisor doesn't want anything
    type = 1 means unit, type = 0 means building
***********************************************************************/

void military_advisor_choose_build(struct player *pplayer, struct city *pcity,
				    struct ai_choice *choice)
{
  int def, danger, v, urgency, want;
  struct unit *myunit = 0;
  struct tile *ptile = map_get_tile(pcity->x, pcity->y);
  struct unit virtualunit;
  struct city *acity = 0;
  struct unit *aunit = 0;
  choice->choice = 0;
  choice->want   = 0;
  choice->type   = 0;

/* TODO: recognize units that can DEFEND_HOME but are in the field. -- Syela */

  urgency = assess_danger(pcity); /* calling it now, rewriting old wall code */
  def = assess_defense_quadratic(pcity); /* has to be AFTER assess_danger thanks to wallvalue */
/* changing to quadratic to stop AI from building piles of small units -- Syela */
  danger = pcity->ai.danger; /* we now have our warmap and will use it! */
/*printf("Assessed danger for %s = %d, Def = %d\n", pcity->name, danger, def);*/

  if (danger) { /* otherwise might be able to wait a little longer to defend */
/* old version had danger -= def in it, which was necessary before disband/upgrade
code was added and walls got built, but now danger -= def would be very bad -- Syela */
    if (danger >= def) {
      if (!urgency) danger = 100; /* don't waste money otherwise */
      else if (danger >= def * 2) danger = 200 + urgency;
      else { danger *= 100; danger /= def; danger += urgency; }
/* without the += urgency, wasn't buying with danger == def.  Duh. -- Syela */
    } else { danger *= 100; danger /= def; }
    if (pcity->shield_surplus <= 0 && def) danger = 0;
/* this is somewhat of an ugly kluge, but polar cities with no ability to
increase prod were buying alpines, panicking, disbanding them, buying alpines
and so on every other turn.  This will fix that problem, hopefully without
creating any other problems that are worse. -- Syela */
    if (pcity->ai.building_want[B_CITY] && def && can_build_improvement(pcity, B_CITY)
        && (danger < 101 || unit_list_size(&ptile->units) > 1 ||
/* walls before a second defender, unless we need it RIGHT NOW */
         (!pcity->ai.grave_danger && /* I'm not sure this is optimal */
         pplayer->economic.gold > (80 - pcity->shield_stock) * 2))) {
/* or we can afford just to buy walls.  Added 980805 -- Syela */
      choice->choice = B_CITY; /* great wall is under domestic */
      choice->want = pcity->ai.building_want[B_CITY]; /* hacked by assess_danger */
      if (!urgency && choice->want > 100) choice->want = 100;
      choice->type = 0;
    } else if (pcity->ai.building_want[B_COASTAL] && def &&
        can_build_improvement(pcity, B_COASTAL) &&
        (danger < 101 || unit_list_size(&ptile->units) > 1)) {
      choice->choice = B_COASTAL; /* great wall is under domestic */
      choice->want = pcity->ai.building_want[B_COASTAL]; /* hacked by assess_danger */
      if (!urgency && choice->want > 100) choice->want = 100;
      choice->type = 0;
    } else if (pcity->ai.building_want[B_SAM] && def &&
        can_build_improvement(pcity, B_SAM) &&
        (danger < 101 || unit_list_size(&ptile->units) > 1)) {
      choice->choice = B_SAM; /* great wall is under domestic */
      choice->want = pcity->ai.building_want[B_SAM]; /* hacked by assess_danger */
      if (!urgency && choice->want > 100) choice->want = 100;
      choice->type = 0;
    } else if (danger > 0 && unit_list_size(&ptile->units) <= urgency) {
      process_defender_want(pplayer, pcity, danger, choice);
      if (!urgency && unit_types[choice->choice].defense_strength == 1) {
        if (city_got_barracks(pcity)) choice->want = MIN(49, danger); /* unlikely */
        else choice->want = MIN(25, danger);
      } else choice->want = danger;
/*    printf("%s wants %s to defend with desire %d.\n", pcity->name,
       get_unit_type(choice->choice)->name, choice->want); */
/*    return; - this is just stupid */
    }
  } /* ok, don't need to defend */

  if (pcity->shield_surplus <= 0 || /* must be able to upkeep units */
      pcity->ppl_unhappy[4] > pcity->ppl_unhappy[2]) return; /* and no disorder! */

  memset(&virtualunit, 0, sizeof(struct unit));
/* this memset didn't work because of syntax that
the intrepid David Pfitzner discovered was in error. -- Syela */
  virtualunit.owner = pplayer->player_no;
  virtualunit.x = pcity->x;
  virtualunit.y = pcity->y;
  virtualunit.id = 0;
  v = ai_choose_defender_by_type(pcity, LAND_MOVING);  /* Temporary -- Syela */
  virtualunit.type = v;
  virtualunit.veteran = do_make_unit_veteran(pcity, v);
  virtualunit.hp = unit_types[v].hp;

  if (choice->want < 100) {
    want = look_for_charge(pplayer, &virtualunit, &aunit, &acity);
    if (want > choice->want) {
      choice->want = want;
      choice->choice = v;
      choice->type = 1;
    }
  }

  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, punit)
    if (((get_unit_type(punit->type)->attack_strength * 4 >
        get_unit_type(punit->type)->defense_strength * 5) ||
        unit_flag(punit->type, F_FIELDUNIT)) &&
        punit->activity != ACTIVITY_GOTO) /* very important clause, this -- Syela */
     myunit = punit;
  unit_list_iterate_end;
/* if myunit is non-null, it is an attacker forced to defend */
/* and we will use its attack values, otherwise we will use virtualunit */
  if (myunit) kill_something_with(pplayer, pcity, myunit, choice);
  else {
/*printf("Killing with virtual unit in %s\n", pcity->name);*/
    v = ai_choose_attacker_sailing(pcity);
    if (v > 0 && /* have to put sailing first before we mung the seamap */
      (city_got_building(pcity, B_PORT) || /* only need a few ports */
      !port_is_within(pplayer, 18))) { /* using move_rate is quirky -- Syela */
      virtualunit.type = v;
/*     virtualunit.veteran = do_make_unit_veteran(pcity, v); */
      virtualunit.veteran = (get_invention(pplayer, A_AMPHIBIOUS) == TECH_KNOWN);
      virtualunit.hp = unit_types[v].hp;
      kill_something_with(pplayer, pcity, &virtualunit, choice);
    } /* ok.  can now mung seamap for ferryboat code.  Proceed! */
    v = ai_choose_attacker_ground(pcity);
    virtualunit.type = v;
/*    virtualunit.veteran = do_make_unit_veteran(pcity, v);*/
    virtualunit.veteran = 1;
    virtualunit.hp = unit_types[v].hp;
    kill_something_with(pplayer, pcity, &virtualunit, choice);
  }
  return;
}

void establish_city_distances(struct player *pplayer, struct city *pcity)
{
  int dist = 0;
  int wondercity;
  int freight;
/* at this moment, our warmap is intact.  we need to do two things: */
/* establish faraway for THIS city, and establish d_t_w_c for ALL cities */

  if (!pcity->is_building_unit && is_wonder(pcity->currently_building))
    wondercity = map_get_continent(pcity->x, pcity->y);
  else wondercity = 0;
  if (get_invention(pplayer, A_CORPORATION) == TECH_KNOWN)
    freight = 6;
  else freight = 3;

  pcity->ai.downtown = 0;
  city_list_iterate(pplayer->cities, othercity)
    dist = warmap.cost[othercity->x][othercity->y];
    if (wondercity && map_get_continent(othercity->x, othercity->y) == wondercity)
      othercity->ai.distance_to_wonder_city = dist;
    dist += freight - 1; dist /= freight;
    pcity->ai.downtown += MAX(0, 5 - dist); /* four three two one fire */
  city_list_iterate_end;
  return;
}
