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
#ifndef FC__GRAPHICS_H
#define FC__GRAPHICS_H

#include <gtk/gtk.h>

#include "graphics_g.h"

typedef struct	Sprite			SPRITE;

struct Sprite
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  int	     has_mask;
  int	     width;
  int	     height;
};


void	create_overlay_unit	(GtkWidget *pixcomm, int i);

#endif  /* FC__GRAPHICS_H */
