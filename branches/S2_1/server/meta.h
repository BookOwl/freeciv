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
#ifndef FC__META_H
#define FC__META_H

#include "shared.h"		/* bool type */

#define DEFAULT_META_SERVER_NO_SEND  TRUE
#define DEFAULT_META_SERVER_ADDR     "http://meta.freeciv.org/metaserver.phtml"
#define METASERVER_REFRESH_INTERVAL   (3*60)
#define METASERVER_MIN_UPDATE_INTERVAL 7   /* not too short, not too long */

enum meta_flag {
  META_INFO,
  META_REFRESH,
  META_GOODBYE
};

const char *default_meta_patches_string(void);
const char *default_meta_message_string(void);

const char *get_meta_patches_string(void);
const char *get_meta_message_string(void);

void set_meta_patches_string(const char *string);
void set_meta_message_string(const char *string);

char *meta_addr_port(void);

void server_close_meta(void);
void server_open_meta(void);
bool is_metaserver_open(void);

bool send_server_info_to_metaserver(enum meta_flag flag);

#endif /* FC__META_H */
