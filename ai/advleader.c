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

#include "player.h"

#include "aitools.h"

#include "advleader.h"

/********************************************************************** 
TODO: distmodifier should be assigned a value between 1 and 100
      100 = can research immediately, 1 = much too far away in the tech
      tree.
***********************************************************************/
void leader_adjust_tech_choice(struct player *pplayer, 
			       struct ai_choice *choice, int advisor)
{
  int distmodifier = 100;

  struct nation_type *prace = get_nation_by_plr(pplayer);
  /* adjust it according to nation attribute */
  adjust_choice(prace->advisors[advisor], choice); 
  /* distmod should be assigned a value according to how many techs player
     have to research to reach this goal, 100 for now */

  adjust_choice(distmodifier, choice);
}

/********************************************************************** 
  ...
***********************************************************************/
void leader_adjust_build_choice(struct player *pplayer, 
				struct ai_choice *choice, int advisor)
{
  struct nation_type *prace = get_nation_by_plr(pplayer);
  adjust_choice(prace->advisors[advisor], choice); 
  /* this function haven't been implemented yet */
}
