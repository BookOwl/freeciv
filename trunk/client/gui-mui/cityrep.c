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
#include <ctype.h>
#include <stdarg.h>

#include <mui/NListview_MCC.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#include "city.h"
#include "game.h"
#include "packets.h"
#include "shared.h"
#include "unit.h"

#include "chatline.h"
#include "climisc.h"
#include "citydlg.h"
#include "cityrepdata.h"
#include "mapview.h"
#include "optiondlg.h"
#include "options.h"
#include "repodlgs.h"
#include "support.h"

#include "cityrep.h"

/* Amiga Client stuff */
#include "muistuff.h"

IMPORT Object *app;
IMPORT Object *main_wnd;

/* TODO: Change button must be implemented and several other things */

extern struct connection aconnection;
extern int delay_report_update;

/******************************************************************/
void create_city_report_dialog(int make_modal);

/****************************************************************
 Create the text for a line in the city report
*****************************************************************/
static void get_city_text(struct city *pcity, char *buf[])
{
  struct city_report_spec *spec;
  int i;

  for (i = 0, spec = city_report_specs; i < NUM_CREPORT_COLS; i++, spec++)
  {
    char *text;

    buf[i][0] = '\0';
/*
    if (!spec->show)
      continue;
*/
    text = (spec->func) (pcity);
    if (text)
    {
      strncpy(buf[i], text, 63);
      buf[i][63] = 0;
    }
  }
}

/****************************************************************
 Return text line for the column headers for the city report
*****************************************************************/
static void get_city_table_header(char *text[])
{
  struct city_report_spec *spec;
  int i;

  for (i = 0, spec = city_report_specs; i < NUM_CREPORT_COLS; i++, spec++)
  {
    if (spec->title1)
    {
      strcpy(text[i], spec->title1);
      strcat(text[i], " ");
    }
    else
      text[i][0] = 0;

    if (spec->title2)
    {
      strcat(text[i], spec->title2);
    }
  }
}


STATIC Object *cityrep_wnd;
STATIC Object *cityrep_title_text;
STATIC Object *cityrep_listview;
STATIC struct Hook cityreq_disphook;
STATIC struct Hook cityreq_comparehook;
STATIC Object *cityrep_close_button;
STATIC Object *cityrep_center_button;
STATIC Object *cityrep_popup_button;
STATIC Object *cityrep_buy_button;
STATIC Object *cityrep_change_button;
STATIC Object *cityrep_refresh_button;

/****************************************************************
...
****************************************************************/
void popup_city_report_dialog(int make_modal)
{
  if (!cityrep_wnd)
    create_city_report_dialog(make_modal);
  if (cityrep_wnd)
  {
    city_report_dialog_update();
    set(cityrep_wnd, MUIA_Window_Open, TRUE);
  }
}

/**************************************************************************
 Display function for the listview in the city report window
**************************************************************************/
__asm __saveds static int cityrep_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct city *pcity)
{
  static char buf[NUM_CREPORT_COLS][64];
  int i;

  if (pcity)
  {
    for (i = 0; i < NUM_CREPORT_COLS; i++)
      array[i] = buf[i];

    get_city_text(pcity, array);
  }
  else
  {
    for (i = 0; i < NUM_CREPORT_COLS; i++)
      array[i] = buf[i];

    get_city_table_header(array);
  }
  return 0;
}

/**************************************************************************
 Compare function for the listview in the city report window
**************************************************************************/
__asm __saveds static int cityrep_compare(register __a0 struct Hook *h, register __a1 struct city *pcity1,
register __a2 struct city *pcity2)
{
  return stricmp(pcity1->name, pcity2->name);
}

/**************************************************************************
 Callback if a new entry inside the listview is selected
**************************************************************************/
static int cityrep_active(void)
{
  struct city *pcity;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if (pcity)
  {
    int flag;
    size_t i;

    set(cityrep_center_button, MUIA_Disabled, FALSE);
    set(cityrep_popup_button, MUIA_Disabled, FALSE);
    set(cityrep_buy_button, MUIA_Disabled, FALSE);

    flag = 0;

    for (i = 0; i < B_LAST; i++)
    {
      if (can_build_improvement(pcity, i))
      {
	flag = 1;
      }
    }

    for (i = 0; i < game.num_unit_types; i++)
    {
      if (can_build_unit(pcity, i))
      {
	flag = 1;
      }
    }


    if (!flag)
      set(cityrep_change_button, MUIA_Disabled, TRUE);
    else
      set(cityrep_change_button, MUIA_Disabled, FALSE);
  }
  else
  {
    set(cityrep_change_button, MUIA_Disabled, TRUE);
    set(cityrep_center_button, MUIA_Disabled, TRUE);
    set(cityrep_popup_button, MUIA_Disabled, TRUE);
    set(cityrep_buy_button, MUIA_Disabled, TRUE);
  }
  return NULL;
}

/**************************************************************************
 Callback for the Buy Button
**************************************************************************/

static int cityrep_buy(void)
{
  struct city *pcity;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if (pcity)
  {
    int value;
    char *name;
    char buf[512];

    value=city_buy_cost(pcity);    
    if(pcity->is_building_unit)
      name=get_unit_type(pcity->currently_building)->name;
    else
      name=get_impr_name_ex(pcity, pcity->currently_building);

    if(game.player_ptr->economic.gold >= value)
    {
      struct packet_city_request packet;
      packet.city_id=pcity->id;
      packet.name[0]='\0';
      packet.worklist.name[0] = '\0';
      send_packet_city_request(&aconnection, &packet, PACKET_CITY_BUY);
    }
    else
    {
      my_snprintf(buf, sizeof(buf),
        "Game: %s costs %d gold and you only have %d gold.",
        name,value,game.player_ptr->economic.gold);
      append_output_window(buf);
    }
  }
  return NULL;
}

/**************************************************************************
 Callback for the Center Button
**************************************************************************/
static int cityrep_center(void)
{
  struct city *pcity;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if (pcity)
  {
    center_tile_mapcanvas(pcity->x, pcity->y);
  }
  return NULL;
}

/**************************************************************************
 Callback for the Popup Button
**************************************************************************/
static int cityrep_popup(void)
{
  struct city *pcity;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if (pcity)
  {
    if (center_when_popup_city) {
      center_tile_mapcanvas(pcity->x, pcity->y);
    }
    popup_city_dialog(pcity, 0);
  }
  return NULL;
}

/**************************************************************************
 Callback for the Change Button
**************************************************************************/
static int cityrep_change(void)
{
  struct city *pcity;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if(pcity)
    popup_city_production_dialog(pcity);

  return NULL;
}

/****************************************************************
 Callback for the Refresh Button
*****************************************************************/
static int cityrep_refresh(void)
{
  struct city *pcity;
  struct packet_generic_integer packet;

  DoMethod(cityrep_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &pcity);

  if(pcity) {
    packet.value = pcity->id;
    send_packet_generic_integer(&aconnection, PACKET_CITY_REFRESH, 
				&packet);
  }

  return NULL;
}

/****************************************************************
 Create and initialize the city report window
*****************************************************************/
void create_city_report_dialog(int make_modal)
{
  if (cityrep_wnd)
    return;

  cityreq_disphook.h_Entry = (HOOKFUNC) cityrep_display;
  cityreq_comparehook.h_Entry = (HOOKFUNC) cityrep_compare;

  cityrep_wnd = WindowObject,
    MUIA_Window_Title, "City Report",
    MUIA_Window_ID, 'CTYR',
    WindowContents, VGroup,
	Child, cityrep_title_text = TextObject,
	    MUIA_Text_PreParse, "\33c",
	    End,
	Child, cityrep_listview = NListviewObject,
	    MUIA_NListview_NList, NListObject,
		MUIA_NList_Format, ",,,,,,,,,,,,",
		MUIA_NList_DisplayHook, &cityreq_disphook,
		MUIA_NList_CompareHook, &cityreq_comparehook,
		MUIA_NList_Title, TRUE,
		End,
	    End,
	Child, HGroup,
	    Child, cityrep_close_button = MakeButton("_Close"),
	    Child, cityrep_center_button = MakeButton("Cen_ter"),
	    Child, cityrep_popup_button = MakeButton("_Popup"),
	    Child, cityrep_buy_button = MakeButton("_Buy"),
	    Child, cityrep_change_button = MakeButton("Chan_ge"),
	    Child, cityrep_refresh_button = MakeButton("_Refresh"),
	    End,
	End,
    End;

  if (cityrep_wnd)
  {
    char *report_title = get_report_title("City Report");
    if (report_title)
    {
      set(cityrep_title_text, MUIA_Text_Contents, report_title);
      free(report_title);
    }

    set(cityrep_change_button, MUIA_Disabled, TRUE);
    set(cityrep_center_button, MUIA_Disabled, TRUE);
    set(cityrep_popup_button, MUIA_Disabled, TRUE);
    set(cityrep_buy_button, MUIA_Disabled, TRUE);

    DoMethod(cityrep_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, cityrep_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(cityrep_close_button, MUIM_Notify, MUIA_Pressed, FALSE, cityrep_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(cityrep_center_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, cityrep_center);
    DoMethod(cityrep_popup_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, cityrep_popup);
    DoMethod(cityrep_buy_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, cityrep_buy);
    DoMethod(cityrep_change_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, cityrep_change);
    DoMethod(cityrep_refresh_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 3, MUIM_CallHook, &standart_hook, cityrep_refresh);
    DoMethod(cityrep_listview, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, app, 3, MUIM_CallHook, &standart_hook, cityrep_active);
    DoMethod(cityrep_listview, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, app, 3, MUIM_CallHook, &standart_hook, cityrep_center);

    DoMethod(app, OM_ADDMEMBER, cityrep_wnd);
  }
}

/****************************************************************
...
*****************************************************************/
void city_report_dialog_update(void)
{
  char *report_title;

  if (!cityrep_wnd)
    return;
  if (delay_report_update)
    return;

  report_title = get_report_title("City Advisor");
  set(cityrep_title_text, MUIA_Text_Contents, report_title);
  free(report_title);

  set(cityrep_listview, MUIA_NList_Quiet, TRUE);
  DoMethod(cityrep_listview, MUIM_NList_Clear);

  city_list_iterate(game.player_ptr->cities, pcity)
  {
    DoMethod(cityrep_listview, MUIM_NList_InsertSingle, pcity, MUIV_NList_Insert_Sorted);
  }
  city_list_iterate_end

    set(cityrep_listview, MUIA_NList_Quiet, FALSE);

  /* Update the buttons */
  cityrep_active();
}

/****************************************************************
  Update the text for a single city in the city report
*****************************************************************/
void city_report_dialog_update_city(struct city *pcity)
{
  int i, entries;
  struct city *pfindcity = 0;

  if (!cityrep_wnd)
    return;
  if (delay_report_update)
    return;

  entries = xget(cityrep_listview, MUIA_NList_Entries);

  for (i = 0; i < entries; i++)
  {
    DoMethod(cityrep_listview, MUIM_NList_GetEntry, i, &pfindcity);
    if (pfindcity == pcity)
      break;
  }

  if (pfindcity)
  {
    DoMethod(cityrep_listview, MUIM_NList_ReplaceSingle, pcity, i, NOWRAP, 0);
  }
  else
    city_report_dialog_update();
}
