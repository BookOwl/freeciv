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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Toggle.h>     

#include "events.h"
#include "fcintl.h"
#include "game.h"
#include "packets.h"
#include "player.h"
#include "shared.h"

#include "chatline.h"
#include "cityrep.h"
#include "gui_stuff.h"
#include "mapview.h"
#include "options.h"

#include "optiondlg.h"

extern Widget toplevel, main_form;

extern struct connection aconnection;
extern Display	*display;

Widget option_dialog_shell;

/******************************************************************/
void create_option_dialog(void);

void option_ok_command_callback(Widget w, XtPointer client_data, 
			        XtPointer call_data);
void option_cancel_command_callback(Widget w, XtPointer client_data, 
				    XtPointer call_data);

/****************************************************************
... 
*****************************************************************/
void popup_option_dialog(void)
{
  client_option *o;
  
  create_option_dialog();

  for (o=options; o->name; ++o) {
    XtVaSetValues((Widget) o->p_gui_data, XtNstate, *(o->p_value), 
                XtNlabel, *(o->p_value) ? _("Yes") : _("No"), NULL);
  }

  xaw_set_relative_position(toplevel, option_dialog_shell, 25, 25);
  XtPopup(option_dialog_shell, XtGrabNone);
  XtSetSensitive(main_form, FALSE);
}




/****************************************************************
...
*****************************************************************/
void create_option_dialog(void)
{
  Widget option_form, option_label;
  Widget option_ok_command, option_cancel_command;
  client_option *o;
  char res_name[255];  /* is this big enough? */
  
  option_dialog_shell =
    I_T(XtCreatePopupShell("optionpopup", transientShellWidgetClass,
			   toplevel, NULL, 0));

  option_form = XtVaCreateManagedWidget("optionform", 
				        formWidgetClass, 
				        option_dialog_shell, NULL);   

  option_label =
    I_L(XtVaCreateManagedWidget("optionlabel", labelWidgetClass, 
				option_form, NULL));
  
  for (o=options; o->name; ++o) {
    strcpy (res_name, "option_");
    strcat (res_name, o->name);
    strcat (res_name, "_label");
    I_L(XtVaCreateManagedWidget(res_name, labelWidgetClass, option_form, NULL));

    strcpy (res_name, "option_");
    strcat (res_name, o->name);
    strcat (res_name, "_toggle");
    o->p_gui_data = (void *)XtVaCreateManagedWidget(res_name, toggleWidgetClass, option_form, NULL);

    XtAddCallback((Widget) o->p_gui_data, XtNcallback, toggle_callback, NULL);
  }

  option_ok_command =
    I_L(XtVaCreateManagedWidget("optionokcommand", commandWidgetClass,
				option_form, NULL));
  
  option_cancel_command =
    I_L(XtVaCreateManagedWidget("optioncancelcommand", commandWidgetClass,
				option_form, NULL));
	
  XtAddCallback(option_ok_command, XtNcallback, 
		option_ok_command_callback, NULL);
  XtAddCallback(option_cancel_command, XtNcallback, 
		option_cancel_command_callback, NULL);

  XtRealizeWidget(option_dialog_shell);

  xaw_horiz_center(option_label);
}


/**************************************************************************
 Changes the label of the toggle widget to Yes/No depending on the state of
 the toggle.
**************************************************************************/
void toggle_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
  Boolean b;

  XtVaGetValues(w, XtNstate, &b, NULL);
  XtVaSetValues(w, XtNlabel, b ? _("Yes") : _("No"), NULL);
}

/**************************************************************************
...
**************************************************************************/
void option_ok_command_callback(Widget w, XtPointer client_data, 
			       XtPointer call_data)
{
  Boolean b;
  client_option *o;

  for (o=options; o->name; ++o) {
    XtVaGetValues((Widget) o->p_gui_data, XtNstate, &b, NULL);
    *(o->p_value) = b;
  }
  
  XtSetSensitive(main_form, TRUE);
  XtDestroyWidget(option_dialog_shell);
}

/**************************************************************************
...
**************************************************************************/
void option_cancel_command_callback(Widget w, XtPointer client_data, 
				    XtPointer call_data)
{
  XtSetSensitive(main_form, TRUE);
  XtDestroyWidget(option_dialog_shell);
}
