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
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h> 
#include <X11/Xaw/Command.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Toggle.h>

#include "city.h"
#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "packets.h"
#include "shared.h"
#include "support.h"

#include "cityrep.h"
#include "clinet.h"
#include "dialogs.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "helpdlg.h"
#include "optiondlg.h"

#include "repodlgs.h"

/******************************************************************/

void create_science_dialog(int make_modal);
void science_close_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data);
void science_help_callback(Widget w, XtPointer client_data, 
			   XtPointer call_data);
void science_change_callback(Widget w, XtPointer client_data, 
			     XtPointer call_data);
void science_goal_callback(Widget w, XtPointer client_data, 
			   XtPointer call_data);


/******************************************************************/
static Widget science_dialog_shell;
static Widget science_label;
static Widget science_current_label, science_goal_label;
static Widget science_change_menu_button, science_goal_menu_button;
static Widget science_list, science_help_toggle;
static Widget science_help_note;
static int science_dialog_shell_is_modal;
static Widget popupmenu, goalmenu;

/******************************************************************/
void create_economy_report_dialog(int make_modal);
void economy_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void economy_selloff_callback(Widget w, XtPointer client_data,
                            XtPointer call_data);
void economy_list_callback(Widget w, XtPointer client_data,
                         XtPointer call_data);
static int economy_improvement_type[B_LAST];

static Widget economy_dialog_shell;
static Widget economy_label, economy_label2;
static Widget economy_list, economy_list_label;
static Widget sellall_command, sellobsolete_command;
static int economy_dialog_shell_is_modal;

/******************************************************************/
void create_activeunits_report_dialog(int make_modal);
void activeunits_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void activeunits_upgrade_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void activeunits_refresh_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data);
void activeunits_list_callback(Widget w, XtPointer client_data, 
                           XtPointer call_data);
static int activeunits_type[U_LAST];

static Widget activeunits_dialog_shell;
static Widget activeunits_label, activeunits_label2;
static Widget activeunits_list, activeunits_list_label;
static Widget upgrade_command;

static int activeunits_dialog_shell_is_modal;
/******************************************************************/

int delay_report_update=0;

/******************************************************************
 Turn off updating of reports
*******************************************************************/
void report_update_delay_on(void)
{
  delay_report_update=1;
}

/******************************************************************
 Turn on updating of reports
*******************************************************************/
void report_update_delay_off(void)
{
  delay_report_update=0;
}

/******************************************************************
...
*******************************************************************/
void update_report_dialogs(void)
{
  if(delay_report_update) return;
  activeunits_report_dialog_update();
  economy_report_dialog_update();
  city_report_dialog_update(); 
  science_dialog_update();
}

/****************************************************************
...
****************************************************************/
char *get_report_title(char *report_name)
{
  char buf[512];
  
  my_snprintf(buf, sizeof(buf), _("%s\n%s of the %s\n%s %s: %s"),
	      report_name,
	      get_government_name(game.player_ptr->government),
	      get_nation_name_plural(game.player_ptr->nation),
	      get_ruler_title(game.player_ptr->government,
			      game.player_ptr->is_male, game.player_ptr->nation),
	      game.player_ptr->name,
	      textyear(game.year));

  return create_centered_string(buf);
}

/****************************************************************
...
****************************************************************/
static char *get_report_title_plus(char *report_name, char *additional)
{
  char buf[512];
  
  my_snprintf(buf, sizeof(buf), _("%s\n%s of the %s\n%s %s: %s%s"),
	      report_name,
	      get_government_name(game.player_ptr->government),
	      get_nation_name_plural(game.player_ptr->nation),
	      get_ruler_title(game.player_ptr->government,
			      game.player_ptr->is_male, game.player_ptr->nation),
	      game.player_ptr->name,
	      textyear(game.year),
	      additional);

  return create_centered_string(buf);
}

/****************************************************************
...
************************ ***************************************/
void popup_science_dialog(int make_modal)
{

  if(!science_dialog_shell) {
    Position x, y;
    Dimension width, height;
    
    science_dialog_shell_is_modal=make_modal;
    
    if(make_modal)
      XtSetSensitive(main_form, FALSE);
    
    create_science_dialog(make_modal);
    
    XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
    
    XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
		      &x, &y);
    XtVaSetValues(science_dialog_shell, XtNx, x, XtNy, y, NULL);
    
    XtPopup(science_dialog_shell, XtGrabNone);
  }

}


/****************************************************************
...
*****************************************************************/
void create_science_dialog(int make_modal)
{
  Widget science_form;
  Widget  close_command;
  static char *tech_list_names_ptrs[A_LAST+1];
  int j, flag, num_list;
  size_t i;
  Dimension width;
  char rate_text[128];
  char current_text[512];
  char goal_text[512];
  char *report_title;
  int turns_to_advance;
  
  if (game.player_ptr->research.researching != A_NONE) {
    my_snprintf(current_text, sizeof(current_text),
		_("Researching %s: %d/%d"),
		advances[game.player_ptr->research.researching].name,
		game.player_ptr->research.bulbs_researched,
		total_bulbs_required(game.player_ptr));
  } else {
    my_snprintf(current_text, sizeof(current_text),
		_("Researching Future Tech. %d: %d/%d"),
		((game.player_ptr->future_tech) + 1),
		game.player_ptr->research.bulbs_researched,
		total_bulbs_required(game.player_ptr));
  }

  my_snprintf(goal_text, sizeof(goal_text),
	      _("Goal: %s (%d steps)"),
	      advances[game.player_ptr->ai.tech_goal].name,
	      num_unknown_techs_for_goal(game.player_ptr,
					 game.player_ptr->ai.tech_goal));
  
  for(i=A_FIRST, j=0; i<game.num_tech_types; i++)
    if(get_invention(game.player_ptr, i)==TECH_KNOWN) {
      tech_list_names_ptrs[j]=advances[i].name;
      j++;
    }
  tech_list_names_ptrs[j]=0;
  qsort(tech_list_names_ptrs, j, sizeof(char *), compare_strings_ptrs);
  num_list = j;
  /* printf("science list num: %d\n", num_list); */
  
  science_dialog_shell =
    I_T(XtVaCreatePopupShell("sciencepopup", 
			     (make_modal ? transientShellWidgetClass :
			      topLevelShellWidgetClass),
			     toplevel, 0));

  science_form = XtVaCreateManagedWidget("scienceform", 
					 formWidgetClass,
					 science_dialog_shell,
					 NULL);   
  
  turns_to_advance = tech_turns_to_advance(game.player_ptr);
  my_snprintf(rate_text, sizeof(rate_text),
	      PL_("\n(%d turn/advance)", "\n(%d turns/advance)",
		  turns_to_advance), turns_to_advance);
  report_title=get_report_title_plus(_("Science"), rate_text);
  science_label = XtVaCreateManagedWidget("sciencelabel", 
					  labelWidgetClass, 
					  science_form,
					  XtNlabel, 
					  report_title,
					  NULL);
  free(report_title);

  science_current_label = XtVaCreateManagedWidget("sciencecurrentlabel", 
						  labelWidgetClass, 
						  science_form,
						  XtNlabel, 
						  current_text,
						  NULL);

  science_goal_label = XtVaCreateManagedWidget("sciencegoallabel", 
					       labelWidgetClass, 
					       science_form,
					       XtNlabel, goal_text,
					       NULL);

  science_change_menu_button =
    I_L(XtVaCreateManagedWidget("sciencechangemenubutton", 
				menuButtonWidgetClass,
				science_form, NULL));

  science_goal_menu_button =
    I_L(XtVaCreateManagedWidget("sciencegoalmenubutton", 
				menuButtonWidgetClass,
				science_form, NULL));

  science_help_note =
    I_L(XtVaCreateManagedWidget("sciencehelpnote",
				labelWidgetClass,
				science_form, NULL));
  
  science_help_toggle = XtVaCreateManagedWidget("sciencehelptoggle", 
						toggleWidgetClass, 
						science_form,
						NULL);
  
  science_list = XtVaCreateManagedWidget("sciencelist", 
					 listWidgetClass,
					 science_form,
					 XtNlist, tech_list_names_ptrs,
					 NULL);

  close_command =
    I_L(XtVaCreateManagedWidget("scienceclosecommand", 
				commandWidgetClass,
				science_form, NULL));
  
  
  popupmenu=XtVaCreatePopupShell("menu", 
				 simpleMenuWidgetClass, 
				 science_change_menu_button, 
				 NULL);

  goalmenu=XtVaCreatePopupShell("menu", 
				simpleMenuWidgetClass, 
				science_goal_menu_button, 
				NULL);

  
  for(i=A_FIRST, flag=0; i<game.num_tech_types; i++)
    if(get_invention(game.player_ptr, i)==TECH_REACHABLE) {
      Widget entry=
      XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
			      popupmenu, NULL);
      XtAddCallback(entry, XtNcallback, science_change_callback, 
		    (XtPointer) i); 
      flag=1;
    }
  
  if(!flag)
    XtSetSensitive(science_change_menu_button, FALSE);
  
 for(i=A_FIRST, flag=0; i<game.num_tech_types; i++)
    if(get_invention(game.player_ptr, i) != TECH_KNOWN &&
       advances[i].req[0] != A_LAST && advances[i].req[1] != A_LAST &&
       num_unknown_techs_for_goal(game.player_ptr, i) < 11) {
      Widget entry=
      XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
			      goalmenu, NULL);
      XtAddCallback(entry, XtNcallback, science_goal_callback, 
		    (XtPointer) i); 
      flag=1;
    }
  
  if(!flag)
    XtSetSensitive(science_goal_menu_button, FALSE);

  XtAddCallback(close_command, XtNcallback, science_close_callback, NULL);
  XtAddCallback(science_list, XtNcallback, science_help_callback, NULL);
  XtAddCallback(science_help_toggle, XtNcallback, toggle_callback, NULL);

  if(num_list>60) {
    int ncol;
    XtVaGetValues(science_list, XtNdefaultColumns, &ncol, NULL);
    XtVaSetValues(science_list, XtNdefaultColumns, ncol+1, NULL);
  }

  XtRealizeWidget(science_dialog_shell);

  if(!make_modal)  {
    XSetWMProtocols(display, XtWindow(science_dialog_shell), 
		    &wm_delete_window, 1);
    XtOverrideTranslations(science_dialog_shell,
      XtParseTranslationTable("<Message>WM_PROTOCOLS: msg-close-science-report()"));
  }

  width=500;
  XtVaSetValues(science_label, XtNwidth, &width, NULL);

  toggle_callback(science_help_toggle, NULL, NULL);
}


/****************************************************************
...
*****************************************************************/
void science_change_callback(Widget w, XtPointer client_data, 
			     XtPointer call_data)
{
  char current_text[512];
  struct packet_player_request packet;
  size_t to;
  Boolean b;

  to=(size_t)client_data;

  XtVaGetValues(science_help_toggle, XtNstate, &b, NULL);
  if (b == TRUE) {
    popup_help_dialog_typed(advances[to].name, HELP_TECH);
  } else {
    my_snprintf(current_text, sizeof(current_text),
		_("Researching %s: %d/%d"),
		advances[to].name, game.player_ptr->research.bulbs_researched,
		total_bulbs_required(game.player_ptr));

    XtVaSetValues(science_current_label, XtNlabel, current_text, NULL);

    packet.tech = to;
    send_packet_player_request(&aconnection, &packet,
			       PACKET_PLAYER_RESEARCH);
  }
}

/****************************************************************
...
*****************************************************************/
void science_goal_callback(Widget w, XtPointer client_data, 
			   XtPointer call_data)
{
  char goal_text[512];
  struct packet_player_request packet;
  size_t to;
  Boolean b;

  to=(size_t)client_data;

  XtVaGetValues(science_help_toggle, XtNstate, &b, NULL);
  if (b == TRUE)
    popup_help_dialog_typed(advances[to].name, HELP_TECH);
  else {  
    my_snprintf(goal_text, sizeof(goal_text), _("Goal: %s (%d steps)"),
		advances[to].name,
		num_unknown_techs_for_goal(game.player_ptr, to));

    XtVaSetValues(science_goal_label, XtNlabel, goal_text, NULL);

    packet.tech=to;
    send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_TECH_GOAL);
  }
}


/****************************************************************
...
*****************************************************************/
void science_close_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data)
{

  if(science_dialog_shell_is_modal)
    XtSetSensitive(main_form, TRUE);
  XtDestroyWidget(science_dialog_shell);
  science_dialog_shell=0;
}


/****************************************************************
...
*****************************************************************/
void sciencereport_msg_close(Widget w)
{
  science_close_callback(w, NULL, NULL);
}


/****************************************************************
...
*****************************************************************/
void science_help_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data)
{
  XawListReturnStruct *ret=XawListShowCurrent(science_list);
  Boolean b;

  XtVaGetValues(science_help_toggle, XtNstate, &b, NULL);
  if (b == TRUE)
    {
      if(ret->list_index!=XAW_LIST_NONE)
	popup_help_dialog_typed(ret->string, HELP_TECH);
      else
	popup_help_dialog_string(HELP_TECHS_ITEM);
    }
}


/****************************************************************
...
*****************************************************************/
void science_dialog_update(void)
{
  if(delay_report_update) return;
  if(science_dialog_shell) {
    char text[512];
    static char *tech_list_names_ptrs[A_LAST+1];
    int j, flag;
    size_t i;
    char rate_text[128];
    char *report_title;
    int turns_to_advance;
    
    turns_to_advance = tech_turns_to_advance(game.player_ptr);
    my_snprintf(rate_text, sizeof(rate_text),
		PL_("\n(%d turn/advance)", "\n(%d turns/advance)",
		    turns_to_advance), turns_to_advance);
    report_title=get_report_title_plus(_("Science"), rate_text);
    xaw_set_label(science_label, report_title);
    free(report_title);

    if (game.player_ptr->research.researching != A_NONE) {
      my_snprintf(text, sizeof(text), _("Researching %s: %d/%d"),
		  advances[game.player_ptr->research.researching].name,
		  game.player_ptr->research.bulbs_researched,
		  total_bulbs_required(game.player_ptr));
    } else {
      my_snprintf(text, sizeof(text),
		  _("Researching Future Tech. %d: %d/%d"),
		  ((game.player_ptr->future_tech) + 1),
		  game.player_ptr->research.bulbs_researched,
		  total_bulbs_required(game.player_ptr));
    }
    
    xaw_set_label(science_current_label, text);

    my_snprintf(text, sizeof(text), _("Goal: %s (%d steps)"),
		advances[game.player_ptr->ai.tech_goal].name,
		num_unknown_techs_for_goal(game.player_ptr,
					   game.player_ptr->ai.tech_goal));

    xaw_set_label(science_goal_label, text);

    for(i=A_FIRST, j=0; i<game.num_tech_types; i++)
      if(get_invention(game.player_ptr, i)==TECH_KNOWN) {
	tech_list_names_ptrs[j]=advances[i].name;
	j++;
      }
    tech_list_names_ptrs[j]=0;
    qsort(tech_list_names_ptrs, j, sizeof(char *), compare_strings_ptrs);

    XawListChange(science_list, tech_list_names_ptrs, 0/*j*/, 0, 1);

    XtDestroyWidget(popupmenu);
    
    popupmenu=XtVaCreatePopupShell("menu", 
				   simpleMenuWidgetClass, 
				   science_change_menu_button, 
				   NULL);
    
      for(i=A_FIRST, flag=0; i<game.num_tech_types; i++)
      if(get_invention(game.player_ptr, i)==TECH_REACHABLE) {
	Widget entry=
	  XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
				  popupmenu, NULL);
	XtAddCallback(entry, XtNcallback, science_change_callback, 
		      (XtPointer) i); 
	flag=1;
      }
    
    if(!flag)
      XtSetSensitive(science_change_menu_button, FALSE);

    XtDestroyWidget(goalmenu);
    
    goalmenu=XtVaCreatePopupShell("menu", 
				  simpleMenuWidgetClass, 
				  science_goal_menu_button, 
				  NULL);
    
    for(i=A_FIRST, flag=0; i<game.num_tech_types; i++)
      if(get_invention(game.player_ptr, i) != TECH_KNOWN &&
         advances[i].req[0] != A_LAST && advances[i].req[1] != A_LAST &&
         num_unknown_techs_for_goal(game.player_ptr, i) < 11) {
	Widget entry=
	  XtVaCreateManagedWidget(advances[i].name, smeBSBObjectClass, 
				  goalmenu, NULL);
	XtAddCallback(entry, XtNcallback, science_goal_callback, 
		      (XtPointer) i); 
	flag=1;
      }
    
    if(!flag)
      XtSetSensitive(science_goal_menu_button, FALSE);

  }
  
}

/****************************************************************

                      ECONOMY REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_economy_report_dialog(int make_modal)
{
  if(!economy_dialog_shell) {
      Position x, y;
      Dimension width, height;
      
      economy_dialog_shell_is_modal=make_modal;
    
      if(make_modal)
	XtSetSensitive(main_form, FALSE);
      
      create_economy_report_dialog(make_modal);
      
      XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
      
      XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
			&x, &y);
      XtVaSetValues(economy_dialog_shell, XtNx, x, XtNy, y, NULL);
      
      XtPopup(economy_dialog_shell, XtGrabNone);
   }
}


/****************************************************************
...
*****************************************************************/
void create_economy_report_dialog(int make_modal)
{
  Widget economy_form;
  Widget close_command;
  char *report_title;
  
  economy_dialog_shell =
    I_T(XtVaCreatePopupShell("reporteconomypopup", 
			     (make_modal ? transientShellWidgetClass :
			      topLevelShellWidgetClass),
			     toplevel, 0));

  economy_form = XtVaCreateManagedWidget("reporteconomyform", 
					 formWidgetClass,
					 economy_dialog_shell,
					 NULL);   

  report_title=get_report_title(_("Economy"));
  economy_label = XtVaCreateManagedWidget("reporteconomylabel", 
				       labelWidgetClass, 
				       economy_form,
				       XtNlabel, report_title,
				       NULL);
  free(report_title);

  economy_list_label =
    I_L(XtVaCreateManagedWidget("reporteconomylistlabel", labelWidgetClass, 
				economy_form, NULL));
  
  economy_list
    = XtVaCreateManagedWidget("reporteconomylist", listWidgetClass,
			      economy_form, NULL);

  economy_label2
    = XtVaCreateManagedWidget("reporteconomylabel2", labelWidgetClass, 
			      economy_form, NULL);

  close_command =
    I_L(XtVaCreateManagedWidget("reporteconomyclosecommand", commandWidgetClass,
				economy_form, NULL));

  sellobsolete_command =
    I_L(XtVaCreateManagedWidget("reporteconomysellobsoletecommand", 
				commandWidgetClass,
				economy_form,
				XtNsensitive, False,
				NULL));

  sellall_command  =
    I_L(XtVaCreateManagedWidget("reporteconomysellallcommand", 
				commandWidgetClass,
				economy_form,
				XtNsensitive, False,
				NULL));
	
  XtAddCallback(economy_list, XtNcallback, economy_list_callback, NULL);
  XtAddCallback(close_command, XtNcallback, economy_close_callback, NULL);
  XtAddCallback(sellobsolete_command, XtNcallback, economy_selloff_callback, (XtPointer)0);
  XtAddCallback(sellall_command, XtNcallback, economy_selloff_callback, (XtPointer)1);
  XtRealizeWidget(economy_dialog_shell);

  if(!make_modal)  {
    XSetWMProtocols(display, XtWindow(economy_dialog_shell), 
		    &wm_delete_window, 1);
    XtOverrideTranslations(economy_dialog_shell,
      XtParseTranslationTable("<Message>WM_PROTOCOLS: msg-close-economy-report()"));
  }

  economy_report_dialog_update();
}



/****************************************************************
...
*****************************************************************/
void economy_list_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret;
  int i;
  ret=XawListShowCurrent(economy_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    i=economy_improvement_type[ret->list_index];
    if(i>=0 && i<game.num_impr_types && !is_wonder(i))
      XtSetSensitive(sellobsolete_command, TRUE);
      XtSetSensitive(sellall_command, TRUE);
    return;
  }
  XtSetSensitive(sellobsolete_command, FALSE);
  XtSetSensitive(sellall_command, FALSE);
}

/****************************************************************
...
*****************************************************************/
void economy_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{

  if(economy_dialog_shell_is_modal)
     XtSetSensitive(main_form, TRUE);
  XtDestroyWidget(economy_dialog_shell);
  economy_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void economyreport_msg_close(Widget w)
{
  economy_close_callback(w, NULL, NULL);
}

/****************************************************************
...
*****************************************************************/
void economy_selloff_callback(Widget w, XtPointer client_data, 
			    XtPointer call_data)
{
  int i,count=0,gold=0;
  struct genlist_iterator myiter;
  struct city *pcity;
  struct packet_city_request packet;
  char str[64];
  XawListReturnStruct *ret=XawListShowCurrent(economy_list);

  if(ret->list_index==XAW_LIST_NONE) return;

  i=economy_improvement_type[ret->list_index];

  genlist_iterator_init(&myiter, &game.player_ptr->cities.list, 0);
  for(; ITERATOR_PTR(myiter);ITERATOR_NEXT(myiter)) {
    pcity=(struct city *)ITERATOR_PTR(myiter);
    if(!pcity->did_sell && city_got_building(pcity, i) && 
       (client_data ||
	improvement_obsolete(game.player_ptr,i) ||
        wonder_replacement(pcity, i) ))  {
	count++; gold+=improvement_value(i);
        packet.city_id=pcity->id;
        packet.build_id=i;
        packet.name[0]='\0';
	packet.worklist.name[0] = '\0';
        send_packet_city_request(&aconnection, &packet, PACKET_CITY_SELL);
    }
  }
  if(count)  {
    my_snprintf(str, sizeof(str), _("Sold %d %s for %d gold"),
		count, get_improvement_name(i), gold);
  } else {
    my_snprintf(str, sizeof(str), _("No %s could be sold"),
		get_improvement_name(i));
  }
  popup_notify_dialog(_("Sell-Off:"), _("Results"), str);
  return;
}

/****************************************************************
...
*****************************************************************/
void economy_report_dialog_update(void)
{
  if(delay_report_update) return;
  if(economy_dialog_shell) {
    int j, k, count, tax, cost, total;
    Dimension width; 
    static char *economy_list_names_ptrs[B_LAST+1];
    static char economy_list_names[B_LAST][200];
    char *report_title;
    char economy_total[48];
    struct city *pcity;
    
    report_title=get_report_title(_("Economy"));
    xaw_set_label(economy_label, report_title);
    free(report_title);
    total = 0;
    tax=0;
    k = 0;
    pcity = city_list_get(&game.player_ptr->cities,0);
    if(pcity)  {
      for (j=0;j<game.num_impr_types;j++)
      if(!is_wonder(j)) {
	count = 0; 
	city_list_iterate(game.player_ptr->cities,pcity)
	  if (city_got_building(pcity, j)) count++;
	city_list_iterate_end;
	if (!count) continue;
	cost = count * improvement_upkeep(pcity, j);
	my_snprintf(economy_list_names[k], sizeof(economy_list_names[k]),
		    "%-20s%5d%5d%6d", get_improvement_name(j),
		    count, improvement_upkeep(pcity, j), cost);
	total+=cost;
	economy_list_names_ptrs[k]=economy_list_names[k];
	economy_improvement_type[k]=j;
	k++;
      }
      city_list_iterate(game.player_ptr->cities,pcity) {
	tax+=pcity->tax_total;
	if (!pcity->is_building_unit && 
	    pcity->currently_building==B_CAPITAL)
	  tax+=pcity->shield_surplus;
      } city_list_iterate_end;
    }
    
    if(k==0) {
      sz_strlcpy(economy_list_names[0],
		 "                                          ");
      economy_list_names_ptrs[0]=economy_list_names[0];
      k=1;
    }
    economy_list_names_ptrs[k]=NULL;

    my_snprintf(economy_total, sizeof(economy_total),
		_("Income:%6d    Total Costs: %6d"), tax, total); 
    xaw_set_label(economy_label2, economy_total); 
    
    XawListChange(economy_list, economy_list_names_ptrs, 0, 0, 1);

    XtVaGetValues(economy_list, XtNwidth, &width, NULL);
    XtVaSetValues(economy_list_label, XtNwidth, width, NULL); 

    XtVaSetValues(economy_label2, XtNwidth, width, NULL); 

    XtVaSetValues(economy_label, XtNwidth, width, NULL); 

  }
  
}

/****************************************************************

                      ACTIVE UNITS REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_activeunits_report_dialog(int make_modal)
{
  if(!activeunits_dialog_shell) {
      Position x, y;
      Dimension width, height;
      
      activeunits_dialog_shell_is_modal=make_modal;
    
      if(make_modal)
	XtSetSensitive(main_form, FALSE);
      
      create_activeunits_report_dialog(make_modal);
      
      XtVaGetValues(toplevel, XtNwidth, &width, XtNheight, &height, NULL);
      
      XtTranslateCoords(toplevel, (Position) width/10, (Position) height/10,
			&x, &y);
      XtVaSetValues(activeunits_dialog_shell, XtNx, x, XtNy, y, NULL);
      
      XtPopup(activeunits_dialog_shell, XtGrabNone);
   }
}


/****************************************************************
...
*****************************************************************/
void create_activeunits_report_dialog(int make_modal)
{
  Widget activeunits_form;
  Widget close_command, refresh_command;
  char *report_title;
  
  activeunits_dialog_shell =
    I_T(XtVaCreatePopupShell("reportactiveunitspopup", 
			     (make_modal ? transientShellWidgetClass :
			      topLevelShellWidgetClass),
			     toplevel, 0));

  activeunits_form = XtVaCreateManagedWidget("reportactiveunitsform", 
					 formWidgetClass,
					 activeunits_dialog_shell,
					 NULL);   

  report_title=get_report_title(_("Units"));
  activeunits_label = XtVaCreateManagedWidget("reportactiveunitslabel", 
				       labelWidgetClass, 
				       activeunits_form,
				       XtNlabel, report_title,
				       NULL);
  free(report_title);

  activeunits_list_label =
    I_L(XtVaCreateManagedWidget("reportactiveunitslistlabel", 
				labelWidgetClass, 
				activeunits_form,
				NULL));
  
  activeunits_list = XtVaCreateManagedWidget("reportactiveunitslist", 
				      listWidgetClass,
				      activeunits_form,
				      NULL);

  activeunits_label2 = XtVaCreateManagedWidget("reportactiveunitslabel2", 
				       labelWidgetClass, 
				       activeunits_form,
                                       XtNlabel, 
				       _("Totals: ..."), 
				       NULL);

  close_command =
    I_L(XtVaCreateManagedWidget("reportactiveunitsclosecommand", 
				commandWidgetClass,
				activeunits_form,
				NULL));

  upgrade_command =
    I_L(XtVaCreateManagedWidget("reportactiveunitsupgradecommand", 
				commandWidgetClass,
				activeunits_form,
				XtNsensitive, False,
				NULL));

  refresh_command =
    I_L(XtVaCreateManagedWidget("reportactiveunitsrefreshcommand", 
				commandWidgetClass,
				activeunits_form,
				NULL));

  XtAddCallback(activeunits_list, XtNcallback, activeunits_list_callback, NULL);
  XtAddCallback(close_command, XtNcallback, activeunits_close_callback, NULL);
  XtAddCallback(upgrade_command, XtNcallback, activeunits_upgrade_callback, NULL);
  XtAddCallback(refresh_command, XtNcallback, activeunits_refresh_callback, NULL);

  XtRealizeWidget(activeunits_dialog_shell);

  if(!make_modal)  {
    XSetWMProtocols(display, XtWindow(activeunits_dialog_shell), 
		    &wm_delete_window, 1);
    XtOverrideTranslations(activeunits_dialog_shell,
      XtParseTranslationTable("<Message>WM_PROTOCOLS: msg-close-units-report()"));
  }

  activeunits_report_dialog_update();
}

/****************************************************************
...
*****************************************************************/
void activeunits_list_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  XawListReturnStruct *ret;
  int inx;
  int may_upgrade;

  ret = XawListShowCurrent (activeunits_list);
  inx = ret->list_index;

  may_upgrade =
    ((inx != XAW_LIST_NONE) &&
     (unit_type_exists (activeunits_type[inx])) &&
     (can_upgrade_unittype (game.player_ptr, activeunits_type[inx]) != -1));

  XtSetSensitive (upgrade_command, may_upgrade);
}

/****************************************************************
...
*****************************************************************/
static void upgrade_callback_yes(Widget w, XtPointer client_data, 
                                 XtPointer call_data)
{
  send_packet_unittype_info(&aconnection, (size_t)client_data,
			    PACKET_UNITTYPE_UPGRADE);
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
static void upgrade_callback_no(Widget w, XtPointer client_data, 
                                XtPointer call_data)
{
  destroy_message_dialog(w);
}

/****************************************************************
...
*****************************************************************/
void activeunits_upgrade_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{
  char buf[512];
  int ut1,ut2;

  XawListReturnStruct *ret;
  ret=XawListShowCurrent(activeunits_list);

  if(ret->list_index!=XAW_LIST_NONE) {
    ut1 = activeunits_type[ret->list_index];
    if (!(unit_type_exists (ut1))) {
      return;
    }
    /* puts(unit_types[ut1].name); */

    ut2 = can_upgrade_unittype(game.player_ptr,
			       activeunits_type[ret->list_index]);

    my_snprintf(buf, sizeof(buf),
	    _("Upgrade as many %s to %s as possible for %d gold each?\n"
	      "Treasury contains %d gold."),
	    unit_types[ut1].name, unit_types[ut2].name,
	    unit_upgrade_price(game.player_ptr, ut1, ut2),
	    game.player_ptr->economic.gold);

    popup_message_dialog(toplevel, "upgradedialog", buf,
			 upgrade_callback_yes,
			 (XtPointer)(activeunits_type[ret->list_index]), 0,
			 upgrade_callback_no, 0, 0,
			 NULL);
  }
}

/****************************************************************
...
*****************************************************************/
void activeunits_close_callback(Widget w, XtPointer client_data, 
			 XtPointer call_data)
{

  if(activeunits_dialog_shell_is_modal)
     XtSetSensitive(main_form, TRUE);
   XtDestroyWidget(activeunits_dialog_shell);
   activeunits_dialog_shell=0;
}

/****************************************************************
...
*****************************************************************/
void activeunits_refresh_callback(Widget w, XtPointer client_data, 
				  XtPointer call_data)
{
  activeunits_report_dialog_update();
}

/****************************************************************
...
*****************************************************************/
void activeunits_msg_close(Widget w)
{
  activeunits_close_callback(w, NULL, NULL);
}

/****************************************************************
...
*****************************************************************/
void activeunits_report_dialog_update(void)
{
  struct repoinfo {
    int active_count;
    int upkeep_shield;
    int upkeep_food;
    /* int upkeep_gold;   FIXME: add gold when gold is implemented --jjm */
    int building_count;
  };
  if(delay_report_update) return;
  if(activeunits_dialog_shell) {
    int i, k;
    Dimension width; 
    static char *activeunits_list_names_ptrs[U_LAST+1];
    static char activeunits_list_names[U_LAST][200];
    struct repoinfo unitarray[U_LAST];
    struct repoinfo unittotals;
    char *report_title;
    char activeunits_total[100];
    
    report_title=get_report_title(_("Units"));
    xaw_set_label(activeunits_label, report_title);
    free(report_title);

    memset(unitarray, '\0', sizeof(unitarray));
    unit_list_iterate(game.player_ptr->units, punit) {
      (unitarray[punit->type].active_count)++;
      if (punit->homecity) {
	unitarray[punit->type].upkeep_shield += punit->upkeep;
	unitarray[punit->type].upkeep_food += punit->upkeep_food;
      }
    }
    unit_list_iterate_end;
    city_list_iterate(game.player_ptr->cities,pcity) {
      if (pcity->is_building_unit &&
	  (unit_type_exists (pcity->currently_building)))
	(unitarray[pcity->currently_building].building_count)++;
    }
    city_list_iterate_end;

    k = 0;
    memset(&unittotals, '\0', sizeof(unittotals));
    for (i=0;i<game.num_unit_types;i++) {
      if ((unitarray[i].active_count > 0) || (unitarray[i].building_count > 0)) {
	my_snprintf
	  (
	   activeunits_list_names[k],
	   sizeof(activeunits_list_names[k]),
	   "%-27s%c%9d%9d%9d%9d",
	   unit_name(i),
	   can_upgrade_unittype(game.player_ptr, i) != -1 ? '*': '-',
	   unitarray[i].building_count,
	   unitarray[i].active_count,
	   unitarray[i].upkeep_shield,
	   unitarray[i].upkeep_food
	  );
	activeunits_list_names_ptrs[k]=activeunits_list_names[k];
	activeunits_type[k]=(unitarray[i].active_count > 0) ? i : U_LAST;
	k++;
	unittotals.active_count += unitarray[i].active_count;
	unittotals.upkeep_shield += unitarray[i].upkeep_shield;
	unittotals.upkeep_food += unitarray[i].upkeep_food;
	unittotals.building_count += unitarray[i].building_count;
      }
    }
    if (k==0) {
      sz_strlcpy(activeunits_list_names[0], "                                ");
      activeunits_list_names_ptrs[0]=activeunits_list_names[0];
      k=1;
    }

    my_snprintf(activeunits_total, sizeof(activeunits_total),
	    _("Totals:                     %9d%9d%9d%9d"),
	    unittotals.building_count, unittotals.active_count,
	    unittotals.upkeep_shield, unittotals.upkeep_food);
    xaw_set_label(activeunits_label2, activeunits_total); 

    activeunits_list_names_ptrs[k]=0;
    XawListChange(activeunits_list, activeunits_list_names_ptrs, 0, 0, 1);

    XtVaGetValues(activeunits_list, XtNwidth, &width, NULL);
    XtVaSetValues(activeunits_list_label, XtNwidth, width, NULL); 
    XtVaSetValues(activeunits_label2, XtNwidth, width, NULL); 
    XtVaSetValues(activeunits_label, XtNwidth, width, NULL); 

    activeunits_list_callback(NULL, NULL, NULL);
  }
}
