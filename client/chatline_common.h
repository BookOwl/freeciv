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
#ifndef FC__CHATLINE_COMMON_H
#define FC__CHATLINE_COMMON_H

#include "shared.h"		/* bool type */

void append_output_window(const char *astring);

void output_window_freeze(void);
void output_window_thaw(void);
void output_window_force_thaw(void);
bool is_output_window_frozen(void);

#endif  /* FC__CHATLINE_COMMON_H */
