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
                          gui_string.h  -  description
                             -------------------
    begin                : Thu May 30 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
**********************************************************************/

#ifndef FC__GUISTRING_H
#define FC__GUISTRING_H

/* SDL */
#include <SDL.h>
#include <SDL_ttf.h>

/* common */
#include "fc_types.h"

/* gui-sdl2 */
#include "gui_main.h"

#define SF_CENTER	0x10
#define SF_CENTER_RIGHT	0x20

/* styles:
	TTF_STYLE_NORMAL	0
	TTF_STYLE_BOLD		1
	TTF_STYLE_ITALIC	2
	TTF_STYLE_UNDERLINE	4
	SF_CENTER	 	0x10	- use with multi string, must be > 0x0f
	SF_CENTER_RIGHT		0x20	- use with multi string, must be > 0x0f
*/

typedef struct utf8_str {
  Uint8 style;
  Uint8 render;
  Uint16 ptsize;
  size_t n_alloc;  /* total allocated text memory */
  SDL_Color fgcol;
  SDL_Color bgcol;
  TTF_Font *font;
  char *text;
} utf8_str;

typedef struct SDL_String16 {
  Uint8 style;
  Uint8 render;
  Uint16 ptsize;
  size_t n_alloc;  /* total allocated text memory */
  SDL_Color fgcol;
  SDL_Color bgcol;
  TTF_Font *font;
  Uint16 *text;
} SDL_String16;

SDL_String16 *create_string16(Uint16 *pInTextString,
                              size_t n_alloc, Uint16 ptsize);
SDL_String16 *copy_chars_to_string16(SDL_String16 *pString,
                                     const char *pCharString);
bool convert_string_to_const_surface_width(SDL_String16 *pString,
                                           int width);
int write_text16(SDL_Surface *pDest, Sint16 x, Sint16 y,
                 SDL_String16 *pString);
SDL_Surface *create_text_surf_from_str16(SDL_String16 *pString);
SDL_Surface *create_text_surf_smaller_that_w(SDL_String16 *pString, int w);
SDL_Rect str16size(SDL_String16 *pString16);
void change_ptsize16(SDL_String16 *pString, Uint16 new_ptsize);

utf8_str *create_utf8_str(char *in_text, size_t n_alloc, Uint16 ptsize);
utf8_str *copy_chars_to_utf8_str(utf8_str *pstr, const char *pchars);
bool convert_utf8_str_to_const_surface_width(utf8_str *pstr,
                                             int width);
int write_utf8(SDL_Surface *dest, Sint16 x, Sint16 y,
               utf8_str *pstr);
SDL_Surface *create_text_surf_from_utf8(utf8_str *pstr);
SDL_Surface *create_text_surf_smaller_than_w(utf8_str *pstr, int w);
SDL_Rect utf8_str_size(utf8_str *pstr);
void change_ptsize_utf8(utf8_str *pstr, Uint16 new_ptsize);

void unload_font(Uint16 ptsize);
void free_font_system(void);

/* adjust font sizes on 320x240 screen */
#ifdef SMALL_SCREEN
  int adj_font(int size);
#else
  #define adj_font(size) size
#endif

#define str16len(pString16) str16size(pString16).w
#define str16height(pString16) str16size(pString16).h

#define utf8_str_height(pstr) utf8_str_size(pstr).h

/*
 *	here we use ordinary free( ... ) because check is made 
 *	on start.
 */
#define FREESTRING16( pString16 )		\
do {						\
	if (pString16) {			\
		FC_FREE(pString16->text);		\
		unload_font(pString16->ptsize);	\
		free(pString16); 		\
		pString16 = NULL;		\
	}					\
} while (FALSE)

/*
 *	here we use ordinary free( ... ) because check is made 
 *	on start.
 */
#define FREEUTF8STR( pstr )             \
  do {                                  \
    if (pstr != NULL) {                 \
      FC_FREE(pstr->text);              \
      unload_font(pstr->ptsize);        \
      free(pstr);                       \
      pstr = NULL;                      \
    }                                   \
} while (FALSE)

#define create_str16_from_char(pInCharString, iPtsize) \
  copy_chars_to_string16(create_string16(NULL, 0, iPtsize), pInCharString)

#define create_utf8_from_char(string_in, ptsize) \
  (string_in) == NULL ?                          \
    create_utf8_str(NULL, 0, ptsize) :           \
    copy_chars_to_utf8_str(create_utf8_str(NULL, 0, ptsize), string_in)
      

#endif /* FC__GUISTRING_H */
