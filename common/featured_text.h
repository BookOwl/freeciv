/***********************************************************************
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
#ifndef FC__FEATURED_TEXT_H
#define FC__FEATURED_TEXT_H

#include "shared.h"             /* bool type. */

#include "fc_types.h"

/*
 * Some words about the featured text module.
 *
 * Bored to have black only chat, players used to hack their clients, to
 * obtain colored chat based patterns.  Also for strategic communication,
 * they added some clickable links on particular city or location.
 * The present code is not based on old warclient code, but it also contains
 * the same tools.  Whereas colors where determinated in client side only,
 * here the server also has the possibility to choose what color to display
 * to the clients.  Such embellishments are performed using escape sequences
 * (which are described bellow) in the strings.
 *
 * Plain text vs featured text.
 *
 * What is called plain text in those files is actually a string without any
 * escape sequence.  It is the text which should basically displayed in the
 * client, without taking account of the features.  On the other side, what
 * is called featured text is also a string, but which could include one of
 * many escape sequences.
 *
 * Text tag.
 *
 * The text_tag type is a structure which tag a particular modification for
 * a string.  It contains many informations, which the type of modification,
 * the start of the modification in bytes from the start of the string, and
 * the stop of it.  It also can contains many others fields, according to
 * sequence type.
 * Note that all offset datas are calculated in bytes and not in character
 * number, so the client must translate those offsets before using them.
 *
 * Escape sequences.
 *
 * - Bold.
 * Full name sequence: [bold] ... [/bold]
 * Abbreviation sequence: [b] ... [/b]
 * Text tag type: TTT_BOLD
 *
 * - Italic.
 * Full name sequence: [italic] ... [/italic]
 * Abbreviation sequence: [i] ... [/i]
 * Text tag type: TTT_ITALIC
 *
 * - Strike.
 * Full name sequence: [strike] ... [/strike]
 * Abbreviation sequence: [s] ... [/s]
 * Text tag type: TTT_STRIKE
 * 
 * - Underline.
 * Full name sequence: [underline] ... [/underline]
 * Abbreviation sequence: [u] ... [/u]
 * Text tag type: TTT_UNDERLINE
 *
 * - Color.
 * Full name sequence: [color] ... [/color]
 * Abbreviation sequence: [c] ... [/c]
 * Text tag type: TTT_COLOR
 * Option: foreground (abbreviation fg): a color name or a hex specification
 * such as #3050b2 or #35b.
 * Option: background (abbreviation bg): same type of data.
 *
 * - Link.
 * Full name sequence: [link] ... [/link] or [link/]
 * Abbreviation sequence: [l] ... [/l] or [l/]
 * Text tag type: TTT_LINK
 * Option: target (abbreviation tgt): absolutely needed.  It is set to
 * "city" (then link type is TLT_CITY), "tile" (TLT_TILE) or "unit"
 * (TLT_UNIT).
 * Option: id: if target type is TLT_CITY or TLT_UNIT, it is the id of
 * the target.
 * Option: name: if target type is TLT_CITY or TLT_UNIT, it is an additional
 * string to display if the target is not known by the receiver.
 * Options: x and y: if target type is TLT_TILE, they are the coordinates
 * of the target.
 */

/* Offset type (in bytes). */
typedef int offset_t;
/* Unset offset value. */
#define OFFSET_UNSET ((offset_t) -1)

/* Opaque type. */
struct text_tag;               

/* Define struct text_tag_list. */
#define SPECLIST_TAG text_tag
#define SPECLIST_TYPE struct text_tag
#include "speclist.h"           

#define text_tag_list_iterate(tags, ptag) \
  TYPED_LIST_ITERATE(struct text_tag, tags, ptag)
#define text_tag_list_iterate_end  LIST_ITERATE_END

/* The different text_tag types.
 * Chaning the order doesn't break the network compatiblity. */
enum text_tag_type {
  TTT_BOLD = 0,
  TTT_ITALIC,
  TTT_STRIKE,
  TTT_UNDERLINE,
  TTT_COLOR,
  TTT_LINK
};

/* The different text_tag link types.
 * Chaning the order doesn't break the network compatiblity. */
enum text_link_type {
  TLT_CITY,
  TLT_TILE,
  TLT_UNIT
};

/* Some event colors */
#define FTC_WARNING     "#FF0000"
#define FTC_LOG         "#7F7F7F"
#define FTC_SERVER_INFO "#8B0000"
#define FTC_EDITOR      "#0000FF"
#define FTC_COMMAND     "#006400"
#define FTC_PRIVATE_MSG "#A020F0"
#define FTC_ALLY_MSG    "#551166"
#define FTC_PUBLIC_MSG  "#00008B"

/* Main functions. */
size_t featured_text_to_plain_text(const char *featured_text,
                                   char *plain_text, size_t plain_text_len,
                                   struct text_tag_list *tags);
size_t featured_text_apply_tag(const char *text_source,
                               char *featured_text, size_t featured_text_len,
                               enum text_tag_type tag_type,
                               offset_t start_offset, offset_t stop_offset,
                               ...);

void text_tag_list_clear_all(struct text_tag_list *tags);
struct text_tag_list *text_tag_list_dup(const struct text_tag_list *tags);

/* Text tag functions. */
enum text_tag_type text_tag_type(const struct text_tag *ptag);
offset_t text_tag_start_offset(const struct text_tag *ptag);
offset_t text_tag_stop_offset(const struct text_tag *ptag);

/* Specific TTT_COLOR text tag type functions. */
const char *text_tag_color_foreground(const struct text_tag *ptag);
const char *text_tag_color_background(const struct text_tag *ptag);

/* Specific TTT_LINK text tag type functions. */
enum text_link_type text_tag_link_type(const struct text_tag *ptag);
int text_tag_link_id(const struct text_tag *ptag);

/* Tools. */
const char *city_link(const struct city *pcity);
const char *tile_link(const struct tile *ptile);
const char *unit_link(const struct unit *punit);

#endif /* FC__FEATURED_TEXT_H */
