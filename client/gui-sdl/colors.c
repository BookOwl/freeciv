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

/**********************************************************************
                          colors.c  -  description
                             -------------------
    begin                : Mon Jul 15 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 **********************************************************************/

/*#include <stdio.h>*/

#include <SDL/SDL.h>
/*#include <SDL/SDL_ttf.h>*/


/*#include "gui_mem.h"*/

/*#include "gui_main.h"*/
#include "log.h"

#include "colors.h"

/*
  r, g, b, a
*/
static SDL_Color colors_standard_rgb[COLOR_STD_LAST] = {
  {0, 0, 0, 255},		/* Black */
  {255, 255, 255, 255},		/* White */
  {255, 0, 0, 255},		/* Red */
  {255, 255, 0, 255},		/* Yellow */
  {0, 255, 200, 255},		/* Cyan */
  {0, 200, 0, 255},		/* Ground (green) */
  {0, 0, 200, 255},		/* Ocean (blue) */
  {86, 86, 86, 255},		/* Background (gray) */
  {128, 0, 0, 255},		/* race0 */
  {128, 255, 255, 255},		/* race1 */
  {255, 0, 0, 255},		/* race2 */
  {255, 0, 128, 255},		/* race3 */
  {0, 0, 128, 255},		/* race4 */
  {255, 0, 255, 255},		/* race5 */
  {255, 128, 0, 255},		/* race6 */
  {255, 255, 128, 255},		/* race7 */
  {255, 128, 128, 255},		/* race8 */
  {0, 0, 255, 255},		/* race9 */
  {0, 255, 0, 255},		/* race10 */
  {0, 128, 128, 255},		/* race11 */
  {0, 64, 64, 255},		/* race12 */
  {198, 198, 198, 255}		/* race13 */
};

/*
  r, g, b, a
*/
static SDL_Color SDLClient_standard_rgba_colors[SDLCLIENT_STD_COLOR_LAST -
						COLOR_STD_LAST] = {
  {138, 114, 82, 255},		/* Background2 (brown) */
  {190, 190, 190, 255},		/* Grid line color */
  {0, 200, 0, 150},		/* Quick info (green) */
};

/**************************************************************************
  Initialize colors for the game.
**************************************************************************/
void init_color_system(void)
{
  freelog(LOG_DEBUG, "init_color_system : PORT ME");
}

/**************************************************************************
  ...
**************************************************************************/
SDL_Color *get_game_colorRGB(Uint32 color_offset)
{
  if (color_offset >= COLOR_STD_LAST) {
    return &SDLClient_standard_rgba_colors[color_offset - COLOR_STD_LAST];
  }

  return &colors_standard_rgb[color_offset];
}

/**************************************************************************
  ...
**************************************************************************/
Uint32 get_game_color(Uint32 color_offset)
{
  SDL_Color *pColor = get_game_colorRGB(color_offset);
  return SDL_MapRGBA(SDL_GetVideoSurface()->format, pColor->r, pColor->g,
		     pColor->b, pColor->unused);
}
