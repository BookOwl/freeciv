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
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "packets.h"

#include "clinet.h"
#include "gui_stuff.h"

#include "chatline.h"

extern GtkWidget *text_scrollbar, *main_message_area;

/**************************************************************************
...
**************************************************************************/
void inputline_return(GtkWidget *w, gpointer data)
{
  struct packet_generic_message apacket;
  char *theinput;

  theinput = gtk_entry_get_text(GTK_ENTRY(w));
  
  if(*theinput) {
    strncpy(apacket.message, theinput, MAX_LEN_MSG-MAX_LEN_USERNAME);
    apacket.message[MAX_LEN_MSG-MAX_LEN_USERNAME]='\0';
    send_packet_generic_message(&aconnection, PACKET_CHAT_MSG, &apacket);
  }

  gtk_entry_set_text(GTK_ENTRY(w), "");
}

void append_output_window(char *astring)
{
  gtk_text_freeze(GTK_TEXT(main_message_area));
  gtk_text_insert(GTK_TEXT(main_message_area), NULL, NULL, NULL, astring, -1);
  gtk_text_insert(GTK_TEXT(main_message_area), NULL, NULL, NULL, "\n", -1);
  gtk_text_thaw(GTK_TEXT(main_message_area));

  /* move the scrollbar forward by a ridiculous amount */
  gtk_range_default_vmotion(GTK_RANGE(text_scrollbar), 0, 10000);
}

/**************************************************************************
 I have no idea what module this belongs in -- Syela
 I've decided to put output_window routines in chatline.c, because
 the are somewhat related and append_output_window is already here.  --dwp
**************************************************************************/
void log_output_window(void)
{ 
  char *theoutput;
  FILE *fp;
  
  append_output_window("Exporting output window to civgame.log ...");
  theoutput = gtk_editable_get_chars(GTK_EDITABLE(main_message_area), 0, -1);
  fp = fopen("civgame.log", "w"); /* should allow choice of name? */
  fprintf(fp, "%s", theoutput);
  fclose(fp);
  append_output_window("Export complete.");
}

/**************************************************************************
...
**************************************************************************/
void clear_output_window(void)
{
  gtk_text_freeze(GTK_TEXT(main_message_area));
  gtk_editable_delete_text(GTK_EDITABLE(main_message_area), 0, -1);
  gtk_text_insert(GTK_TEXT(main_message_area), NULL, NULL, NULL,
	"Cleared output window.\n", -1);
  gtk_text_thaw(GTK_TEXT(main_message_area));
}
