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

#ifndef FC__REPODLGS_H
#define FC__REPODLGS_H

#include "repodlgs_g.h"

void free_auxiliary_tech_icons(void);
void setup_auxiliary_tech_icons(void);
SDL_Surface * create_sellect_tech_icon( SDL_String16 *pStr, int tech_id );
void popdown_all_science_dialogs(void);
void popdown_economy_report_dialog(void);
void popdown_activeunits_report_dialog(void);

#endif				/* FC__REPODLGS_H */
