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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>  
#include <X11/Xaw/List.h>

#include "mem.h"     /* mystrdup() */
#include "version.h"

#include "chatline.h"
#include "clinet.h"
#include "gui_stuff.h"

#include "connectdlg.h"

/****************************************************************/

/* extern AppResources appResources; */
extern Widget toplevel;
extern char name[];
extern char server_host[];
extern int  server_port;

Widget iname, ihost, iport;
Widget connw, metaw, quitw;

void server_address_ok_callback(Widget w, XtPointer client_data, 
				XtPointer call_data);
void quit_callback(Widget w, XtPointer client_data, XtPointer call_data);
void connect_callback(Widget w, XtPointer client_data, XtPointer call_data);
void connect_meta_callback(Widget w, XtPointer client_data, XtPointer call_data);

/****************************************************************/

/* Meta Server */
Widget meta_dialog_shell=0;
char *server_list[64]={NULL};

void create_meta_dialog(Widget caller);
int  update_meta_dialog(Widget meta_list);
void meta_list_callback(Widget w, XtPointer client_data, XtPointer call_data);
void meta_list_destroy(Widget w, XtPointer client_data, XtPointer call_data);
void meta_update_callback(Widget w, XtPointer client_data, XtPointer call_data);
void meta_close_callback(Widget w, XtPointer client_data, XtPointer call_data);

static int get_meta_list(char **list, char *errbuf);

/****************************************************************
...
*****************************************************************/
void gui_server_connect(void)
{
  Widget shell, form;
  char buf[512];
  
  XtTranslations textfieldtranslations;
  
  XtSetSensitive(toplevel, FALSE);
  
  I_T(shell=XtCreatePopupShell("connectdialog", transientShellWidgetClass,
			       toplevel, NULL, 0));
  
  form=XtVaCreateManagedWidget("cform", formWidgetClass, shell, NULL);

  I_LW(XtVaCreateManagedWidget("cheadline", labelWidgetClass, form, NULL));

  I_L(XtVaCreateManagedWidget("cnamel", labelWidgetClass, form, NULL));
  iname=XtVaCreateManagedWidget("cnamei", asciiTextWidgetClass, form, 
			  XtNstring, name, NULL);

  I_L(XtVaCreateManagedWidget("chostl", labelWidgetClass, form, NULL));
  ihost=XtVaCreateManagedWidget("chosti", asciiTextWidgetClass, form, 
			  XtNstring, server_host, NULL);

  sprintf(buf, "%d", server_port);
  
  I_L(XtVaCreateManagedWidget("cportl", labelWidgetClass, form, NULL));
  iport=XtVaCreateManagedWidget("cporti", asciiTextWidgetClass, form, 
			  XtNstring, buf, NULL);

  I_L(connw=XtVaCreateManagedWidget("cconnectc", commandWidgetClass,
				    form, NULL));
  I_L(metaw=XtVaCreateManagedWidget("cmetac", commandWidgetClass, form, NULL));
  I_L(quitw=XtVaCreateManagedWidget("cquitc", commandWidgetClass, form, NULL));

  if (IS_BETA_VERSION)
    I_LW(XtVaCreateManagedWidget("cbetaline", labelWidgetClass, form, NULL));

  XtAddCallback(connw, XtNcallback, connect_callback, NULL);
  XtAddCallback(quitw, XtNcallback, quit_callback, NULL);
  XtAddCallback(metaw, XtNcallback, connect_meta_callback, (XtPointer)shell);

  xaw_set_relative_position(toplevel, shell, 30, 0);
  XtPopup(shell, XtGrabNone);

  textfieldtranslations = 
    XtParseTranslationTable("<Key>Return: connect-dialog-returnkey()");
  XtOverrideTranslations(form, textfieldtranslations);
  XtOverrideTranslations(iname, textfieldtranslations);
  XtOverrideTranslations(ihost, textfieldtranslations);
  XtOverrideTranslations(iport, textfieldtranslations);

  XtSetKeyboardFocus(toplevel, shell);
}

/****************************************************************
...
*****************************************************************/
void connect_dialog_returnkey(Widget w, XEvent *event, String *params,
			    Cardinal *num_params)
{
  x_simulate_button_click(connw);
}

/****************************************************************
...
*****************************************************************/
void quit_callback(Widget w, XtPointer client_data, 
				    XtPointer call_data) 
{
  exit(0);
}

/****************************************************************
...
*****************************************************************/
void connect_callback(Widget w, XtPointer client_data, 
		      XtPointer call_data) 
{
  XtPointer dp;
  char errbuf[512];
  
  XtVaGetValues(iname, XtNstring, &dp, NULL);
  strcpy(name, (char*)dp);
  XtVaGetValues(ihost, XtNstring, &dp, NULL);
  strcpy(server_host, (char*)dp);
  XtVaGetValues(iport, XtNstring, &dp, NULL);
  sscanf((char*)dp, "%d", &server_port);
  
  if(connect_to_server(name, server_host, server_port, errbuf)!=-1) {
    XtDestroyWidget(XtParent(XtParent(w)));
    if(meta_dialog_shell) {
      XtDestroyWidget(meta_dialog_shell);
      meta_dialog_shell=0;
    }
    XtSetSensitive(toplevel, True);
    return;
  }
  
  append_output_window(errbuf);
}

/****************************************************************
...
*****************************************************************/
void connect_meta_callback(Widget w, XtPointer client_data,
                           XtPointer call_data)
{
  if(meta_dialog_shell) return;  /* Metaserver window already poped up */
  create_meta_dialog((Widget)client_data);
}

/****************************************************************
...
*****************************************************************/
void create_meta_dialog(Widget caller)
{
  Widget shell, form, label, list, update, close;
  Dimension width;

  shell=XtCreatePopupShell("metadialog", transientShellWidgetClass,
			   toplevel, NULL, 0);
  meta_dialog_shell=shell;

  form=XtVaCreateManagedWidget("metaform", formWidgetClass, shell, NULL);

  I_L(label=XtVaCreateManagedWidget("legend", labelWidgetClass, form, NULL));
  list=XtVaCreateManagedWidget("metalist", listWidgetClass, form, NULL);
  update=XtVaCreateManagedWidget("update", commandWidgetClass, form, NULL);
  I_L(close=XtVaCreateManagedWidget("closecommand", commandWidgetClass,
				    form, NULL));

  if(!update_meta_dialog(list))  {
    XtDestroyWidget(shell);
    meta_dialog_shell=0;
    return;
  }

  XtAddCallback(list, XtNcallback, meta_list_callback, NULL);
  XtAddCallback(list, XtNdestroyCallback, meta_list_destroy, NULL);
  XtAddCallback(update, XtNcallback, meta_update_callback, (XtPointer)list);
  XtAddCallback(close, XtNcallback, meta_close_callback, NULL);

  /* XtRealizeWidget(shell); */

  XtVaGetValues(list, XtNwidth, &width, NULL);
  XtVaSetValues(label, XtNwidth, width, NULL);

  xaw_set_relative_position(caller, shell, 0, 90);
  XtPopup(shell, XtGrabNone);

  XtSetKeyboardFocus(toplevel, shell);
}

/****************************************************************
...
*****************************************************************/
int update_meta_dialog(Widget meta_list)
{
  char errbuf[128];

  if(get_meta_list(server_list,errbuf)!=-1)  {
    XawListChange(meta_list,server_list,0,0,True);
    return 1;
  } else {
    append_output_window(errbuf);
    return 0;
  }
}

/****************************************************************
...
*****************************************************************/
void meta_update_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
  update_meta_dialog((Widget)client_data);
}

/****************************************************************
...
*****************************************************************/
void meta_close_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
  XtDestroyWidget(meta_dialog_shell);
  meta_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void meta_list_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(w);
  char name[64], port[16];

  sscanf(ret->string,"%s %s\n",name,port);
  XtVaSetValues(ihost, XtNstring, name, NULL);
  XtVaSetValues(iport, XtNstring, port, NULL);
}

/****************************************************************
...
*****************************************************************/
void meta_list_destroy(Widget w, XtPointer client_data, XtPointer call_data)
{
  int i;

  for(i=0;server_list[i]!=NULL;i++)  {
    free(server_list[i]);
    server_list[i]=NULL;
  }
}

/**************************************************************************
  Get the list of servers from the metaserver
**************************************************************************/
static int get_meta_list(char **list, char *errbuf)
{
  char line[256];
  struct server_list *server_list = create_server_list(errbuf);
  if(!server_list) return -1;

  server_list_iterate(*server_list,pserver)
    sprintf(line,"%-35s %-5s %-7s %-9s %2s   %s",
            pserver->name,pserver->port,pserver->version,
            pserver->status,pserver->players,pserver->metastring);
    if(*list) free(*list);
    *list=mystrdup(line);
    list++;
  server_list_iterate_end

  delete_server_list(server_list);
  *list=NULL;
  return 0;
}
