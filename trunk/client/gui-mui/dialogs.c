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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <mui/NListview_MCC.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#include "version.h"

#include "capability.h"
#include "game.h"
#include "government.h"
#include "mem.h"
#include "player.h"
#include "packets.h"

#include "chatline.h"
#include "clinet.h"
#include "connectdlg.h"
#include "control.h"
#include "dialogs.h"


/* MUI Stuff */
#include "muistuff.h"
#include "mapclass.h"

IMPORT Object *app;
IMPORT Object *main_wnd;

extern struct connection aconnection;

/****************************************************************
 ...
*****************************************************************/
void request_player_revolution(void)
{
  struct packet_player_request packet;
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_REVOLUTION);
}
/****************************************************************
 ...
*****************************************************************/
void request_player_government(int g)
{
  struct packet_player_request packet;

  packet.government=g;
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_GOVERNMENT);
}


extern int flags_are_transparent;
extern int ai_popup_windows;

STATIC Object *unitsel_wnd;

int is_showing_government_dialog;

int is_showing_pillage_dialog = FALSE;
int unit_to_use_to_pillage;

int diplomat_id;
int diplomat_target_id;


/****************************************************************
 Called from the Application object so it is safe to dispose the
 window
*****************************************************************/
static void notify_close_real(Object **pwnd)
{
  set(*pwnd,MUIA_Window_Open,FALSE);
  DoMethod(app, OM_REMMEMBER, *pwnd);
  MUI_DisposeObject(*pwnd);
}

/****************************************************************
 Close the notify window
*****************************************************************/
static void notify_close(Object **pwnd)
{
  set(*pwnd, MUIA_Window_Open, FALSE);
  DoMethod(app, MUIM_Application_PushMethod, app, 4, MUIM_CallHook, &standart_hook, notify_close_real, *pwnd);
}

/****************************************************************
 Popup the notify window
*****************************************************************/
void popup_notify_dialog(char *caption, char *headline, char *lines)
{
  Object *wnd;
  Object *listview;
  Object *close_button;

  wnd = WindowObject,
    MUIA_Window_Title, strdup(caption),	/* Never freed! */
    WindowContents, VGroup,
      Child, TextObject,
        MUIA_Text_Contents, headline,
        MUIA_Text_PreParse, "\33c",
        End,
      Child, listview = NListviewObject,
        MUIA_NListview_NList, NListObject,
          ReadListFrame,
          MUIA_Font, MUIV_Font_Fixed,
          MUIA_NList_Input, FALSE,
          MUIA_NList_TypeSelect,MUIV_NList_TypeSelect_Char,
          MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
          MUIA_NList_DestructHook , MUIV_NList_DestructHook_String,
          MUIA_NList_AutoCopyToClip, TRUE,
          End,
        End,
        Child, close_button = MakeButton("_Close"),
      End,
    End;

  if(wnd)
  {
    DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 4, MUIM_CallHook, &standart_hook, notify_close, wnd);
    DoMethod(close_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, notify_close, wnd);
    DoMethod(listview, MUIM_NList_Insert, lines, -2, MUIV_NList_Insert_Bottom);
    DoMethod(app, OM_ADDMEMBER, wnd);
    set(wnd,MUIA_Window_Open, TRUE);
  }
}

/****************************************************************
...
*****************************************************************/
void popup_notify_goto_dialog(char *headline, char *lines,int x, int y)
{
  printf("popup_notify_goto_dialog(%s,%s,%ld,%ld)\n",headline,lines,x,y);

/*
  GtkWidget *notify_dialog_shell, *notify_command, *notify_goto_command;
  GtkWidget *notify_label;
  
  if (x == 0 && y == 0) {
    popup_notify_dialog("Message:", headline, lines);
    return;
  }
  notify_dialog_shell = gtk_dialog_new();
  gtk_signal_connect( GTK_OBJECT(notify_dialog_shell),"delete_event",
	GTK_SIGNAL_FUNC(notify_deleted_callback),NULL );

  gtk_window_set_title( GTK_WINDOW( notify_dialog_shell ), headline );

  notify_label=gtk_label_new(lines);
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->vbox ),
	notify_label, TRUE, TRUE, 0 );

  notify_command = gtk_button_new_with_label("Close");
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->action_area ),
	notify_command, TRUE, TRUE, 0 );

  notify_goto_command = gtk_button_new_with_label("Goto and Close");
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(notify_dialog_shell)->action_area ),
	notify_goto_command, TRUE, TRUE, 0 );
  
  gtk_signal_connect(GTK_OBJECT(notify_command), "clicked",
		GTK_SIGNAL_FUNC(notify_no_goto_command_callback), NULL);
  gtk_signal_connect(GTK_OBJECT(notify_goto_command), "clicked",
		GTK_SIGNAL_FUNC(notify_goto_command_callback), NULL);
  notify_goto_add_widget_coords(notify_goto_command, x, y);

  gtk_set_relative_position(toplevel, notify_dialog_shell, 25, 25);

  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->vbox );
  gtk_widget_show_all( GTK_DIALOG(notify_dialog_shell)->action_area );
  gtk_widget_show(notify_dialog_shell);

  gtk_widget_set_sensitive(toplevel, FALSE);
*/
}

/****************************************************************
 Callback for the Yes button in the bribe confirmation window
*****************************************************************/
static void diplomat_bribe_yes(struct popup_message_data *data)
{
  struct packet_diplomat_action req;

  req.action_type=DIPLOMAT_BRIBE;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;

  send_packet_diplomat_action(&aconnection, &req);

  message_close(data);
}

/****************************************************************
 Popup the bribe confirmation window
*****************************************************************/
void popup_bribe_dialog(struct unit *punit)
{
  char buf[128];
  
  if(game.player_ptr->economic.gold>=punit->bribe_cost)
  {
    sprintf(buf, "Bribe unit for %d gold?\nTreasury contains %d gold.", punit->bribe_cost, game.player_ptr->economic.gold);

    popup_message_dialog(main_wnd, "Bribe Enemy Unit", buf,
                         "Yes", diplomat_bribe_yes, 0,
                         "No", message_close, 0,
                         0);
  } else
  {
    sprintf(buf, "Bribing the unit costs %d gold.\nTreasury contains %d gold.", punit->bribe_cost, game.player_ptr->economic.gold);

    popup_message_dialog(main_wnd, "Traitors Demand Too Much!", buf,
                         "Darn", message_close,0,
                         0);
  }
}

struct spy_data
{
  Object *wnd;
  Object *listview;
};

/****************************************************************
 Called from the Application object so it is safe to dispose the
 window
*****************************************************************/
static void spy_real_close(struct spy_data *data)
{
  Object *wnd = data->wnd;
  set(wnd,MUIA_Window_Open,FALSE);
  DoMethod(app,OM_REMMEMBER,data->wnd);
  MUI_DisposeObject(wnd);
}

/****************************************************************
 Close the spy window. Used as advance steal and improvemnet
 sabotage window
*****************************************************************/
static void spy_close(struct spy_data *data)
{
  DoMethod(app, MUIM_Application_PushMethod, app, 4, MUIM_CallHook, &standart_hook, spy_real_close, data->wnd);
}

/****************************************************************
 Callback for the steal button
*****************************************************************/
static void advance_steal(struct spy_data *data)
{
  LONG which;
  DoMethod(data->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active,&which);

  if(which)
  {
    which-=100;
    if(find_unit_by_id(diplomat_id) && 
       find_city_by_id(diplomat_target_id))
    { 
      struct packet_diplomat_action req;

      req.action_type=DIPLOMAT_STEAL;
      req.value=which;
      req.diplomat_id=diplomat_id;
      req.target_id=diplomat_target_id;
			
      send_packet_diplomat_action(&aconnection, &req);
    }
  }

  spy_close(data);
}

/****************************************************************
 Display function for the technology listview
*****************************************************************/
__asm __saveds static void advance_display( register __a0 struct Hook *h, register __a2 char **array, register __a1 LONG which)
{
  if(which)
    *array = advances[which-100].name;
  else
    *array = "Technology";
}


/****************************************************************
...
*****************************************************************/
static void create_advances_list(struct player *pplayer,
				struct player *pvictim, int make_modal)
{
  Object *listview,*steal_button,*cancel_button,*wnd;
  static struct Hook disphook;

  disphook.h_Entry = (HOOKFUNC)advance_display;

  wnd = WindowObject,
    MUIA_Window_Title,"Steal Technology",
    MUIA_Window_ID, 'SPST',
    WindowContents, VGroup,
      Child, TextObject,
        MUIA_Text_Contents,"Select Advance to Steal",
        MUIA_Text_PreParse,"\33c",
        End,
      Child, listview = NListviewObject,
          MUIA_NListview_NList, NListObject,
          MUIA_NList_DisplayHook, &disphook,
          End,
        End,
      Child, HGroup,
        Child, steal_button = MakeButton("_Steal"),
        Child, cancel_button = MakeButton("_Cancel"),
        End,
      End,
    End;

  if(wnd)
  {
    int i;
    if (pvictim)
    {
      /* you don't want to know what lag can do -- Syela */
      for(i=A_FIRST; i<game.num_tech_types; i++)
      {
        if(get_invention(pvictim, i)==TECH_KNOWN && (get_invention(pplayer, i)==TECH_UNKNOWN || get_invention(pplayer, i)==TECH_REACHABLE))
        {
          DoMethod(listview, MUIM_NList_InsertSingle, i+100,MUIV_NList_Insert_Bottom);
        }
      }
    }

    DoMethod(wnd,MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 5, MUIM_CallHook, &standart_hook, spy_close, wnd, listview);
    DoMethod(cancel_button,MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, spy_close, wnd, listview);
    DoMethod(steal_button,MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, advance_steal, wnd, listview);
    DoMethod(app,OM_ADDMEMBER,wnd);
    set(wnd,MUIA_Window_Open, TRUE);
  }
}

/****************************************************************
 Callback for the sabotage button
*****************************************************************/
static void imprv_sabotage(struct spy_data *data)
{
  LONG which;
  DoMethod(data->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active,&which);
  if(which)
  {
    if(find_unit_by_id(diplomat_id) && find_city_by_id(diplomat_target_id))
    { 
      struct packet_diplomat_action req;
    
      req.action_type=DIPLOMAT_SABOTAGE;
      req.value=which-100+1;
      req.diplomat_id=diplomat_id;
      req.target_id=diplomat_target_id;

      send_packet_diplomat_action(&aconnection, &req);
    }
  }

  spy_close(data);
}


/****************************************************************
 Display function for the sabotage listview
*****************************************************************/
__asm __saveds void imprv_display( register __a0 struct Hook *h, register __a2 char **array, register __a1 LONG which)
{
  *array = get_improvement_name(which-100);
}


/****************************************************************
...
*****************************************************************/
static void create_improvements_list(struct player *pplayer,
				     struct city *pcity, int make_modal)
{  
  Object *listview,*steal_button,*cancel_button,*wnd;
  static struct Hook disphook;

  disphook.h_Entry = (HOOKFUNC)imprv_display;

  wnd = WindowObject,
    MUIA_Window_Title,"Sabotage Improvements",
    MUIA_Window_ID, 'SPIP',
    WindowContents, VGroup,
      Child, TextObject,
        MUIA_Text_Contents,"Select Improvement to Sabotage",
        MUIA_Text_PreParse,"\33c",
        End,
      Child, listview = NListviewObject,
        MUIA_NListview_NList, NListObject,
          MUIA_NList_DisplayHook, &disphook,
          End,
        End,
      Child, HGroup,
        Child, steal_button = MakeButton("_Sabotage"),
        Child, cancel_button = MakeButton("_Cancel"),
        End,
      End,
    End;

  if(wnd)
  {
    int i;
    for(i=0; i<B_LAST; i++)
    {
      if(i != B_PALACE && pcity->improvements[i] && !is_wonder(i))
      {
        DoMethod(listview, MUIM_NList_InsertSingle, i+100,MUIV_NList_Insert_Bottom);
      }
    }  

    DoMethod(wnd,MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 4, MUIM_CallHook, &standart_hook, spy_close, wnd);
    DoMethod(cancel_button,MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, spy_close, wnd);
    DoMethod(steal_button,MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, imprv_sabotage, wnd, listview);
    DoMethod(app,OM_ADDMEMBER,wnd);
    set(wnd,MUIA_Window_Open, TRUE);
  }
}

/****************************************************************
 Callback for the Yes button in the incite window
*****************************************************************/
static void diplomat_incite_yes(struct popup_message_data *data)
{
  struct packet_diplomat_action req;

  req.action_type=DIPLOMAT_INCITE;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;

  send_packet_diplomat_action(&aconnection, &req);
  
  message_close(data);
}

/****************************************************************
 Popup the yes/no dialog for inciting, since we know the cost now
*****************************************************************/
void popup_incite_dialog(struct city *pcity)
{
  char buf[128];

  if(game.player_ptr->economic.gold>=pcity->incite_revolt_cost)
  {
    sprintf(buf, "Incite a revolt for %d gold?\nTreasury contains %d gold.", pcity->incite_revolt_cost, game.player_ptr->economic.gold);
            diplomat_target_id = pcity->id;

    popup_message_dialog(main_wnd, "Incite a Revolt!", buf,
                         "Yes", diplomat_incite_yes, 0,
                         "No", message_close,0,
                         0);

  } else
  {
    sprintf(buf, "Inciting a revolt costs %d gold.\nTreasury contains %d gold.", pcity->incite_revolt_cost, game.player_ptr->economic.gold);
    popup_message_dialog(main_wnd, "Traitors Demand Too Much!", buf,
                         "Darn", message_close,0,
                         0);
  }
}

/****************************************************************
 Callback for Establish button
*****************************************************************/
static void diplomant_establish(struct popup_message_data *data)
{
  message_close(data);

   if(find_unit_by_id(diplomat_id) && 
     (find_city_by_id(diplomat_target_id))) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_EMBASSY;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    
    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Callback for Investigate button
*****************************************************************/
static void diplomant_investigate(struct popup_message_data *data)
{
  message_close(data);

  if(find_unit_by_id(diplomat_id) && 
     (find_city_by_id(diplomat_target_id))) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_INVESTIGATE;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    
    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Callback for Sabotage button
*****************************************************************/
static void diplomant_sabotage(struct popup_message_data *data)
{
  message_close(data);

  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_SABOTAGE;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    req.value = -1;

    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Callback for Steal button
*****************************************************************/
static void diplomant_steal(struct popup_message_data *data)
{
  message_close(data);

  if(find_unit_by_id(diplomat_id) && 
     find_city_by_id(diplomat_target_id)) { 
    struct packet_diplomat_action req;
    
    req.action_type=DIPLOMAT_STEAL;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;
    req.value=0;
    
    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Callback for Incite button
*****************************************************************/
static void diplomant_incite(struct popup_message_data *data)
{
  struct city *pcity;
  struct packet_generic_integer packet;

  message_close(data);
  
  if(find_unit_by_id(diplomat_id) && 
     (pcity=find_city_by_id(diplomat_target_id))) { 
    packet.value = diplomat_target_id;
    send_packet_generic_integer(&aconnection, PACKET_INCITE_INQ, &packet);
  }
}

/****************************************************************
 Callback for Bribe button for Units
*****************************************************************/
static void diplomant_bribe_unit(struct popup_message_data *data)
{
  struct packet_generic_integer packet;

  message_close(data);
  
  if(find_unit_by_id(diplomat_id) && 
     find_unit_by_id(diplomat_target_id)) { 
    packet.value = diplomat_target_id;
    send_packet_generic_integer(&aconnection, PACKET_INCITE_INQ, &packet);
   }

}

/****************************************************************
 Callback for Poison button for Units
*****************************************************************/
static void spy_poison(struct popup_message_data *data)
{
  message_close(data);

  if(find_unit_by_id(diplomat_id) &&
    (find_city_by_id(diplomat_target_id))) {
    struct packet_diplomat_action req;

    req.action_type=SPY_POISON;
    req.diplomat_id=diplomat_id;
    req.target_id=diplomat_target_id;

    send_packet_diplomat_action(&aconnection, &req);
  }
}

/****************************************************************
 Callback for Sabotage button for Improvments
*****************************************************************/
static void spy_sabotage(struct popup_message_data *data)
{
  struct city *pvcity = find_city_by_id(diplomat_target_id);
  message_close(data);
  create_improvements_list(game.player_ptr, pvcity, 1);
}

/****************************************************************
 Callback for Steal button
*****************************************************************/
static void spy_steal(struct popup_message_data *data)
{
  struct city *pvcity = find_city_by_id(diplomat_target_id);
  struct player *pvictim = NULL;

  message_close(data);

  if(pvcity)
  {
    pvictim = city_owner(pvcity);
/* it is concievable that pvcity will not be found, because something
has happened to the city during latency.  Therefore we must initialize
pvictim to NULL and account for !pvictim in create_advances_list. -- Syela */

    create_advances_list(game.player_ptr, pvictim, 1);//spy_tech_shell_is_modal);
  }
}

/****************************************************************
 Callback for Sabotage button (for units)
*****************************************************************/
static void spy_sabotage_unit(struct popup_message_data *data)
{
  struct packet_diplomat_action req;
  
  req.action_type=SPY_SABOTAGE_UNIT;
  req.diplomat_id=diplomat_id;
  req.target_id=diplomat_target_id;
  
  send_packet_diplomat_action(&aconnection, &req);
  
  message_close(data);
}

/****************************************************************
...
*****************************************************************/
void popup_diplomat_dialog(struct unit *punit, int dest_x, int dest_y)
{
  /* Vereinfachen! */
  struct city *pcity;
  struct unit *ptunit;

  struct New_Msg_Dlg msg_dlg[9];
  int i=0;

  diplomat_id = punit->id;
  
  if((pcity=map_get_city(dest_x, dest_y)))
  {
    /* Spy/Diplomat acting against a city */ 
 
    diplomat_target_id=pcity->id;

    if(!unit_flag(punit->type, F_SPY))
    {
      if(diplomat_can_do_action(punit, DIPLOMAT_EMBASSY, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Establish embassy";
      	msg_dlg[i].function = (APTR)diplomant_establish;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_INVESTIGATE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Investigate city",
      	msg_dlg[i].function = (APTR)diplomant_investigate;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_SABOTAGE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Sabotage city";
        msg_dlg[i].function = (APTR)diplomant_sabotage;
        msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_STEAL, dest_x, dest_y))
      {
      	msg_dlg[i].label = "Steal _technology";
      	msg_dlg[i].function = (APTR)diplomant_steal;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_INCITE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "Incite a _revolt";
      	msg_dlg[i].function = (APTR)diplomant_incite;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      msg_dlg[i].label = "_Cancel";
      msg_dlg[i].function = (APTR)message_close;
      msg_dlg[i++].data = NULL;

      msg_dlg[i].label = NULL;

      popup_message_dialog_args(main_wnd, "Choose Your Diplomat's Strategy",
                                "Sir, the diplomat is waiting for your command",
                                msg_dlg);
    } else
    {
      if(diplomat_can_do_action(punit, DIPLOMAT_EMBASSY, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Establish embassy";
      	msg_dlg[i].function = (APTR)diplomant_establish;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_INVESTIGATE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Investigate city (free)";
      	msg_dlg[i].function = (APTR)diplomant_investigate;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, SPY_POISON, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Poison city";
      	msg_dlg[i].function = (APTR)spy_poison;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_SABOTAGE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "Industrial _sabotage";
      	msg_dlg[i].function = (APTR)spy_sabotage;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_STEAL, dest_x, dest_y))
      {
      	msg_dlg[i].label = "Steal _technology";
      	msg_dlg[i].function = (APTR)spy_steal;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, DIPLOMAT_INCITE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "Incite a _revolt";
      	msg_dlg[i].function = (APTR)diplomant_incite;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      msg_dlg[i].label = "_Cancel";
      msg_dlg[i].function = (APTR)message_close;
      msg_dlg[i++].data = NULL;

      msg_dlg[i].label = NULL;

      popup_message_dialog_args(main_wnd, " Choose Your Spy's Strategy",
                                "Sir, the spy is waiting for your command",
                                msg_dlg);

    }
  } else
  {
    if((ptunit=unit_list_get(&map_get_tile(dest_x, dest_y)->units, 0)))
    {
      /* Spy/Diplomat acting against a unit */ 
      diplomat_target_id=ptunit->id;
 
      if(diplomat_can_do_action(punit, DIPLOMAT_BRIBE, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Bribe Enemy Unit";
      	msg_dlg[i].function = (APTR)diplomant_bribe_unit;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      if(diplomat_can_do_action(punit, SPY_SABOTAGE_UNIT, dest_x, dest_y))
      {
      	msg_dlg[i].label = "_Sabotage Enemy Unit";
      	msg_dlg[i].function = (APTR)spy_sabotage_unit;
      	msg_dlg[i].data = NULL;
      	i++;
      }

      msg_dlg[i].label = "_Cancel";
      msg_dlg[i].function = (APTR)message_close;
      msg_dlg[i++].data = NULL;

      msg_dlg[i].label = NULL;

      popup_message_dialog_args(main_wnd, "Subvert Enemy Unit",
                               (!unit_flag(punit->type, F_SPY))?
                                "Sir, the diplomat is waiting for your command":"Sir, the spy is waiting for your command",
                                msg_dlg);

    }
  }
}


static int caravan_dialog;

struct caravan_data
{
  LONG caravan_city_id;
  LONG caravan_unit_id;
};


/****************************************************************
 Callback for the Keep Moving button. Closes the window
*****************************************************************/
static void caravan_keep(struct popup_message_data *data)
{
  message_close(data);
  caravan_dialog = 0;
  process_caravan_arrival(NULL);
}

/****************************************************************
 Callback for the Help build wonder button
*****************************************************************/
static void caravan_help(struct popup_message_data *data)
{
  struct caravan_data *cd = (struct caravan_data*)data->data;
  if(cd)
  {
    /* Should be made to request_xxx */
    struct packet_unit_request req;
    req.unit_id=cd->caravan_unit_id;
    req.city_id=cd->caravan_city_id;
    req.name[0]='\0';
    send_packet_unit_request(&aconnection, &req, PACKET_UNIT_HELP_BUILD_WONDER);
  }
  caravan_keep(data);
}

/****************************************************************
 Callback for the Establish Trade Route button
*****************************************************************/
static void caravan_establish(struct popup_message_data *data)
{
  struct caravan_data *cd = (struct caravan_data*)data->data;
  if(cd)
  {
    /* Should be made to request_xxx */
    struct packet_unit_request req;
    req.unit_id=cd->caravan_unit_id;
    req.city_id=cd->caravan_city_id;
    req.name[0]='\0';
    send_packet_unit_request(&aconnection, &req, PACKET_UNIT_ESTABLISH_TRADE);
  }
  caravan_keep(data);
}


/****************************************************************
...
*****************************************************************/
void popup_caravan_dialog(struct unit *punit, struct city *phomecity, struct city *pdestcity)
{
  struct New_Msg_Dlg msg_dlg[4];
  int i=0;
  char buf[128];

  sprintf(buf, "Your caravan from %s reaches the city of %s.\nWhat now?",
          phomecity->name, pdestcity->name);

  if(can_establish_trade_route(phomecity, pdestcity))
  {
    /* Note: Never free'd */
    struct caravan_data *cd = malloc_struct(struct caravan_data);
    if(cd)
    {
      cd->caravan_city_id = pdestcity->id;
      cd->caravan_unit_id = punit->id;

      msg_dlg[i].label = "_Establish traderoute";
      msg_dlg[i].function = (APTR)caravan_establish;
      msg_dlg[i].data = (APTR)cd;
      i++;
    }
  }

  if(unit_can_help_build_wonder(punit, pdestcity))
  {
    /* Note: Never free'd */
    struct caravan_data *cd = malloc_struct(struct caravan_data);
    if(cd)
    {
      cd->caravan_city_id = pdestcity->id;
      cd->caravan_unit_id = punit->id;

      msg_dlg[i].label = "_Help build Wonder";
      msg_dlg[i].function = (APTR)caravan_help;
      msg_dlg[i].data = (APTR)cd;
      i++;
    }
  }

  msg_dlg[i].label = "_Keep moving";
  msg_dlg[i].function = (APTR)caravan_keep;
  msg_dlg[i].data = NULL;
  i++; msg_dlg[i].label = NULL;

  caravan_dialog = 1;
  popup_message_dialog_args(main_wnd, "Your Caravan Has Arrived",
                            buf,msg_dlg);
}

/****************************************************************
 Callback for a government button
*****************************************************************/
static void government_button(struct popup_message_data *msg)
{
  is_showing_government_dialog = 0;
  request_player_government((int)msg->data);
  message_close(msg);
}


/****************************************************************
 Fills the New_Msg_Dlg for the requested government
 TRUE, if player can choose the governemt, otherwise FALSE
*****************************************************************/
int fill_government_dialog(struct New_Msg_Dlg *dlg, int government)
{
  struct government *g = &governments[government];

  if(can_change_to_government(game.player_ptr, government))
  {
    dlg->label = g->name;
    dlg->function = (APTR)government_button;
    dlg->data = (APTR)g->index;
    return TRUE;
  }
  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void popup_government_dialog(void)
{
  struct New_Msg_Dlg *msg_dlg = (struct New_Msg_Dlg*)malloc(sizeof(struct New_Msg_Dlg)*(game.government_count+1));
  if(msg_dlg)
  {
    if(!is_showing_government_dialog)
    {
      int i,j;
      is_showing_government_dialog=1;
      j=0;

      for(i=0;i<game.government_count;i++)
      {
        if(i == game.government_when_anarchy) continue;
        if(fill_government_dialog(&msg_dlg[j],i)) j++;
      }

      msg_dlg[j].label = NULL;

      popup_message_dialog_args(main_wnd, "Choose Your New Government",
                                "Select government type:",msg_dlg);
    }
    free(msg_dlg);
  }
}

/****************************************************************
 Callback for the Yes button in the revolution confirmation
 window
*****************************************************************/
static void revolution_yes( struct popup_message_data *data)
{
  request_player_revolution();
  destroy_message_dialog( data->wnd);
}

/****************************************************************
...
*****************************************************************/
void popup_revolution_dialog(void)
{
  popup_message_dialog(main_wnd, "Revolution!", "You say you wanna revolution?",
                       "_Yes",revolution_yes, 0,
                       "_No",message_close, 0,
                       NULL);
}

/****************************************************************
 Callback for a pillage button
*****************************************************************/
static void pillage_button(struct popup_message_data *msg)
{
  is_showing_pillage_dialog = FALSE;

  if(msg->data)
  {
    struct unit *punit = find_unit_by_id (unit_to_use_to_pillage);
    if (punit) {
      request_new_unit_activity_targeted(punit,
                                         ACTIVITY_PILLAGE,
                                         (int)msg->data);
    }
  }
  message_close(msg);
}

/****************************************************************
...
*****************************************************************/
void popup_pillage_dialog(struct unit *punit, int may_pillage)
{
  if(!is_showing_pillage_dialog)
  {
    int may_pillage_save = may_pillage;
    int count=0;

    while (may_pillage)
    {
      int what = get_preferred_pillage(may_pillage);
      may_pillage &= (~(what | map_get_infrastructure_prerequisite (what)));
      count++;
    }

    if(count)
    {
      struct New_Msg_Dlg *msg_dlg;
      int i;

      is_showing_pillage_dialog = TRUE;
      unit_to_use_to_pillage = punit->id;
      may_pillage = may_pillage_save;

      if((msg_dlg = (struct New_Msg_Dlg*)malloc(sizeof(struct New_Msg_Dlg)*(count+2))))
      {
        for(i=0;i<count;i++)
        {
          int what = get_preferred_pillage (may_pillage);
	
          msg_dlg[i].label = strdup(map_get_infrastructure_text(what));
          msg_dlg[i].function = (APTR)pillage_button;
          msg_dlg[i].data = (APTR)what;
	
          may_pillage &= (~(what | map_get_infrastructure_prerequisite (what)));
        }
	
        msg_dlg[i].label = "_Cancel";
        msg_dlg[i].function = (APTR)pillage_button;
        msg_dlg[i].data = NULL;
	
        msg_dlg[++i].label = NULL;
	
        popup_message_dialog_args(main_wnd, "What to Pillage",
                                  "Select what to pillage:",msg_dlg);

        for(i=0;i<count;i++)
        {
          if(msg_dlg[i].label)
            free(msg_dlg[i].label);
        }

        free(msg_dlg);
      }
    }
  }
}


/****************************************************************
 Must be called from the Application object so it is safe to
 dispose the window
*****************************************************************/
static void message_close_real( Object **ppwnd)
{
  set(*ppwnd,MUIA_Window_Open,FALSE);
  DoMethod(app, OM_REMMEMBER, *ppwnd);
  MUI_DisposeObject(*ppwnd);
}

/****************************************************************
 This function can be used as a New_Msg_Dlg.function
*****************************************************************/
void message_close( struct popup_message_data *data)
{
  destroy_message_dialog( data->wnd);
}

/****************************************************************
 Popups a window which contains a (window) title, a text which
 information and buttons
*****************************************************************/
Object *popup_message_dialog_args( Object *parent, char *title, char *text, struct New_Msg_Dlg *msg )
{
  Object *button_group;
  Object *wnd;

  int i=0;
  while(msg[i].label) i++;

  if(!i) return NULL;

  wnd = WindowObject,
    MUIA_Window_Title,title,
    WindowContents, VGroup,
      Child, TextObject,
        MUIA_Text_Contents,text,
        MUIA_Text_PreParse, "\33c",
        End,
      Child, button_group = GroupObject,
        MUIA_Group_Horiz, i<4?TRUE:FALSE,
        End,
      End,
    End;

  if(wnd)
  {
    while(msg->label)
    {
      Object *o = MakeButton(msg->label);
      if(msg->function)
      {
        DoMethod(button_group,OM_ADDMEMBER, o);
        DoMethod(o,MUIM_Notify,MUIA_Pressed,FALSE,MUIV_Notify_Self, 5, MUIM_CallHook,&standart_hook, msg->function, wnd, msg->data);
      }
      msg++;
    }
    DoMethod(app,OM_ADDMEMBER,wnd);
    set(wnd,MUIA_Window_Open,TRUE);
  }
  return wnd;
}

/****************************************************************
 popup_message_dialog_args with variable parameters
*****************************************************************/
Object *popup_message_dialog( Object *parent, char *title, char *text, ... )
{
  return popup_message_dialog_args(parent, title, text, (struct New_Msg_Dlg *)((&text)+1));
}

/****************************************************************
 popup_message_dialog_args with variable parameters
*****************************************************************/
void destroy_message_dialog( Object *wnd)
{
  if(wnd)
  {
    set(wnd, MUIA_Window_Open, FALSE);
    /* Close the window better in the Application Object */
    DoMethod(app, MUIM_Application_PushMethod, app, 4, MUIM_CallHook, &standart_hook, message_close_real, wnd);
  }
}

/****************************************************************
 Callback if a unit has been clicked
*****************************************************************/
static void unitsel_unit(struct unit **ppunit)
{
  struct unit *punit = *ppunit;

  if(punit) {
    request_new_unit_activity(punit, ACTIVITY_IDLE);
    set_unit_focus(punit);
  }
  set(unitsel_wnd, MUIA_Window_Open, FALSE);
}

/****************************************************************
 Callback for the Ready All button
*****************************************************************/
static void unitsel_ready_all(struct tile **pptile)
{
  struct tile *ptile = *pptile;

  if(ptile)
  {
    int n,i;
    n=unit_list_size(&ptile->units);

    if(n)
    {
      for(i=0;i<n;i++)
      {
        struct unit *punit=unit_list_get(&ptile->units, i);
        if(punit) {
          request_new_unit_activity(punit, ACTIVITY_IDLE);
          set_unit_focus(punit);
        }
      }
    }
  }
  set(unitsel_wnd, MUIA_Window_Open, FALSE);
}

/****************************************************************
popup the unit selection dialog
*****************************************************************/
void popup_unit_select_dialog(struct tile *ptile)
{
  static Object *window_group;
  static Object *unit_group;
  static Object *button_group;
  static Object *readyall_button;

  if(!unitsel_wnd)
  {
    Object *close_button;

    unitsel_wnd = WindowObject,
      MUIA_Window_Title, "Select Unit",
      WindowContents, window_group = VGroup,
        Child, button_group = HGroup,
          Child, close_button = MakeButton("_Close"),
          Child, readyall_button = MakeButton("_Ready All"),
          End,
        End,
      End;

    if(unitsel_wnd)
    {
      DoMethod(unitsel_wnd, MUIM_Notify, MUIA_Window_CloseRequest,TRUE,unitsel_wnd, 3, MUIM_Set, MUIA_Window_Open,FALSE);
      DoMethod(close_button, MUIM_Notify, MUIA_Pressed,FALSE,unitsel_wnd, 3, MUIM_Set, MUIA_Window_Open,FALSE);
      DoMethod(app,OM_ADDMEMBER,unitsel_wnd);
    }
  }

  if(unitsel_wnd)
  {
    int i,n;
    char buffer[512];

    DoMethod(readyall_button, MUIM_KillNotify, MUIA_Pressed);
    DoMethod(readyall_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, unitsel_ready_all, ptile);

    n=unit_list_size(&ptile->units);

    DoMethod(window_group, MUIM_Group_InitChange);
    if(unit_group)
    {
      DoMethod(window_group, OM_REMMEMBER, unit_group);
      MUI_DisposeObject(unit_group);
    }

    if(n)
    {
      unit_group = VGroupV, End;

      for(i=0;i<n;i++)
      {
        struct unit *punit=unit_list_get(&ptile->units, i);
        struct unit_type *punittemp=get_unit_type(punit->type);
        struct city *pcity;

        Object *o;
        Object *unit_obj;

        pcity = city_list_find_id(&game.player_ptr->cities, punit->homecity);

        sprintf(buffer, "%s%s\n%s\n%s", punittemp->name,
               (punit->veteran) ? " (veteran)" : "", unit_activity_text(punit),pcity ? pcity->name : "");

        o = HGroup,
          Child, unit_obj = UnitObject,
            ButtonFrame,
            MUIA_InputMode, MUIV_InputMode_RelVerify,
            MUIA_Unit_Unit,punit,
            MUIA_Disabled, !can_unit_do_activity(punit, ACTIVITY_IDLE),
            End,
          Child, TextObject,
            MUIA_Font, MUIV_Font_Tiny,
            MUIA_Text_Contents, buffer,
            End,
          End;

        if(o)
        {
          DoMethod(unit_obj,MUIM_Notify, MUIA_Pressed, FALSE, unit_obj, 4, MUIM_CallHook, &standart_hook, unitsel_unit, punit);
          DoMethod(unit_group, OM_ADDMEMBER, o);
        }
      }
      DoMethod(window_group, OM_ADDMEMBER, unit_group);
      DoMethod(window_group, MUIM_Group_Sort, unit_group, button_group, NULL);
    } else unit_group = NULL;

    DoMethod(window_group, MUIM_Group_ExitChange);

    set(unitsel_wnd,MUIA_Window_Open,TRUE);
  }
}


STATIC STRPTR nation_entries[64];
STATIC STRPTR styles_entries[64];
STATIC int styles_basic_index[64];
STATIC int styles_basic_nums;

Object *nations_wnd;
Object *nations_leader_string;
Object *nations_leader_poplist;
Object *nations_nation_cycle;
Object *nations_sex_radio;
Object *nations_styles_cycle;


/****************************************************************
...
*****************************************************************/
static void nations_nation_active(void)
{
  int i, leader_count;
  char **leaders;
  Object *list = (Object*)xget(nations_leader_poplist,MUIA_Popobject_Object);
  LONG nation = xget(nations_nation_cycle,MUIA_Cycle_Active);

  leaders = get_nation_leader_names( nation, &leader_count);
  setstring(nations_leader_string, leaders[0]);

  set(nations_sex_radio, MUIA_Radio_Active, get_nation_leader_sex(nation,leaders[0])?0:1);

  if(list)
  {
    DoMethod(list,MUIM_List_Clear);
    for(i=0;i<leader_count;i++)
    {
      DoMethod(list,MUIM_List_InsertSingle, leaders[i], MUIV_List_Insert_Bottom);
    }
  }
}

/****************************************************************
...
*****************************************************************/
static void nations_ok(void)
{
  int selected, selected_sex, selected_style;
  char *s;
  struct packet_alloc_nation packet;

  selected = xget(nations_nation_cycle, MUIA_Cycle_Active);
  selected_sex = xget(nations_sex_radio, MUIA_Radio_Active);
  selected_style = xget(nations_styles_cycle, MUIA_Cycle_Active);
  s = getstring(nations_leader_string);

  /* perform a minimum of sanity test on the name */
  packet.nation_no=selected;
  packet.is_male = !selected_sex;
  packet.city_style = styles_basic_index[selected_style];

  strncpy(packet.name, (char*)s, MAX_LEN_NAME);
  packet.name[MAX_LEN_NAME-1]='\0';
  
  if(!get_sane_name(packet.name)) {
    append_output_window("You must type a legal name.");
    return;
  }

  packet.name[0]=toupper(packet.name[0]);

  send_packet_alloc_nation(&aconnection, &packet);
}

/****************************************************************
...
*****************************************************************/
static void nations_disconnect(void)
{
  popdown_races_dialog();
  disconnect_from_server();
}

/****************************************************************
...
*****************************************************************/
__asm __saveds static void nations_obj2str( register __a2 Object *list, register __a1 Object *str, register __a0 struct Hook *hook)
{
  char *x;
  LONG nation = xget(nations_nation_cycle,MUIA_Cycle_Active);
  DoMethod(list,MUIM_List_GetEntry,MUIV_List_GetEntry_Active,&x);
  set(str,MUIA_String_Contents,x);
  if(x) set(nations_sex_radio, MUIA_Radio_Active, get_nation_leader_sex(nation,x)?0:1);
}

/****************************************************************
...
*****************************************************************/
__asm __saveds static ULONG nations_str2obj( register __a2 Object *list, register __a1 Object *str, register __a0 struct Hook *hook)
{
  char *x,*s;
  int i;

  get(str,MUIA_String_Contents,&s);

  for (i=0;;i++)
  {
    DoMethod(list,MUIM_List_GetEntry,i,&x);
    if (!x)
    {
      set(list,MUIA_List_Active,MUIV_List_Active_Off);
      break;
    }
    else if (!stricmp(x,s))
    {
      set(list,MUIA_List_Active,i);
      break;
    }
  }
  return TRUE;
}

/****************************************************************
 Popup the nations (races) window
*****************************************************************/
void popup_races_dialog(void)
{
  if(!nations_wnd)
  {
    Object *nations_ok_button,*list;
    Object *nations_disconnect_button=NULL;
    Object *nations_quit_button=NULL;

    int i;
    static char *sex_labels[] = {"Male","Female",NULL};
    static struct Hook objstr_hook;
    static struct Hook strobj_hook;

    objstr_hook.h_Entry = (HOOKFUNC)nations_obj2str;
    strobj_hook.h_Entry = (HOOKFUNC)nations_str2obj;

    styles_basic_nums = 0;

    for(i=0;i<game.playable_nation_count && i<64;i++) nation_entries[i] = get_nation_name(i);
    for(i=0;i<game.styles_count && i<64;i++)
    {
      if(city_styles[i].techreq == A_NONE)
      {
        styles_basic_index[styles_basic_nums] = i;
        styles_entries[styles_basic_nums] = city_styles[i].name;
        styles_basic_nums++;
      }
    }

    styles_entries[styles_basic_nums] = NULL;

    nations_wnd = WindowObject,
        MUIA_Window_Title,  "What Nation Will You Be?",
        WindowContents, VGroup,
            Child, HGroup,
                Child, MakeLabel("_Nation"),
                Child, nations_nation_cycle = MakeCycle("_Nation",nation_entries),
                End,
            Child, HGroup,
                Child, MakeLabel("_Leader Name"),
                Child, nations_leader_poplist = PopobjectObject,
                    MUIA_Popstring_String, nations_leader_string = MakeString(NULL,40),
                    MUIA_Popstring_Button, PopButton(MUII_PopUp),
                    MUIA_Popobject_ObjStrHook, &objstr_hook,
                    MUIA_Popobject_StrObjHook, &strobj_hook,
                    MUIA_Popobject_Object, list = ListviewObject,
                        MUIA_Listview_List, ListObject,
                            InputListFrame,
                            End,
                        End,
                    End,
                Child, nations_sex_radio = MakeRadio(NULL,sex_labels),
                End,
            Child, HGroup,
                Child, MakeLabel("_City Style"),
                Child, nations_styles_cycle = MakeCycle("_City Style",styles_entries),
                End,
            Child, HGroup,
                Child, nations_ok_button = MakeButton("_Ok"),
                Child, nations_disconnect_button = MakeButton("_Disconnect"),
                Child, nations_quit_button = MakeButton("_Quit"),
                End,
            End,
        End;

    if(nations_wnd)
    {
      DoMethod(nations_nation_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, app, 3, MUIM_CallHook, &standart_hook, nations_nation_active);
      DoMethod(nations_ok_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, nations_ok);
      DoMethod(nations_disconnect_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, nations_disconnect);
      DoMethod(nations_quit_button, MUIM_Notify, MUIA_Pressed, FALSE, app,2,MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
      DoMethod(list,MUIM_Notify,MUIA_Listview_DoubleClick,TRUE, nations_leader_poplist,2,MUIM_Popstring_Close,TRUE);
      DoMethod(app, OM_ADDMEMBER, nations_wnd);
    }
  }

  if(nations_wnd)
  {
    nations_nation_active();
    set(nations_wnd,MUIA_Window_Open,TRUE);
  }
}

/****************************************************************
...
*****************************************************************/
void popdown_races_dialog(void)
{
  if(nations_wnd) set(nations_wnd,MUIA_Window_Open,FALSE);
}

/****************************************************************
...
*****************************************************************/
void races_toggles_set_sensitive(int bits1, int bits2)
{
/*
  int i, selected, mybits;

  mybits=bits1;

  for(i=0; i<game.playable_nation_count && i<32; i++) {
    if(mybits&1)
      gtk_widget_set_sensitive( races_toggles[i], FALSE );
    else
      gtk_widget_set_sensitive( races_toggles[i], TRUE );
    mybits>>=1;
  }

  mybits=bits2;

  for(i=32; i<game.playable_nation_count; i++) {
    if(mybits&1)
      gtk_widget_set_sensitive( races_toggles[i], FALSE );
    else
      gtk_widget_set_sensitive( races_toggles[i], TRUE );
    mybits>>=1;
  }

  if((selected=races_buttons_get_current())==-1)
     return;
*/
}

/****************************************************************
...
*****************************************************************/
void process_caravan_arrival(struct unit *punit)
{
  static struct genlist arrival_queue;
  static int is_init_arrival_queue = 0;
  int *p_id;

  /* arrival_queue is a list of individually malloc-ed ints with
     punit.id values, for units which have arrived. */

  if (!is_init_arrival_queue) {
    genlist_init(&arrival_queue);
    is_init_arrival_queue = 1;
  }

  if (punit) {
    p_id = fc_malloc(sizeof(int));
    *p_id = punit->id;
    genlist_insert(&arrival_queue, p_id, -1);
  }

  /* There can only be one dialog at a time: */
  if (caravan_dialog) {
    return;
  }
  
  while (genlist_size(&arrival_queue)) {
    int id;
    
    p_id = genlist_get(&arrival_queue, 0);
    genlist_unlink(&arrival_queue, p_id);
    id = *p_id;
    free(p_id);
    punit = unit_list_find(&game.player_ptr->units, id);

    if (punit && (unit_can_help_build_wonder_here(punit)
		  || unit_can_est_traderoute_here(punit))
	&& (!game.player_ptr->ai.control || ai_popup_windows)) {
      struct city *pcity_dest = map_get_city(punit->x, punit->y);
      struct city *pcity_homecity = find_city_by_id(punit->homecity);
      if (pcity_dest && pcity_homecity) {
	popup_caravan_dialog(punit, pcity_homecity, pcity_dest);
	return;
      }
    }
  }
}

