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
#ifndef FC__GUI_STUFF_H
#define FC__GUI_STUFF_H

#include <gtk/gtk.h>

GtkWidget *gtk_accelbutton_new(const gchar *label, GtkAccelGroup *accel);
void gtk_changed_pixmap(GtkWidget *w);
void gtk_clear_pixmap(GtkWidget *w);
GtkWidget *gtk_new_pixmap(gint width, gint height);
void gtk_set_label(GtkWidget *w, char *text);
void gtk_set_bitmap(GtkWidget *w, GdkPixmap *pm);
void gtk_expose_now(GtkWidget *w);
void gtk_set_relative_position(GtkWidget *ref, GtkWidget *w, int px, int py);

/* FIXME: disagrees with actual function in gui_stuff.c */
GdkPixmap *gtk_scale_pixmap(GdkPixmap *src, int src_w, int src_h,
			    int dst_w, int dst_h);

#endif  /* FC__GUI_STUFF_H */
