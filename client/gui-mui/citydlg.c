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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

#include "city.h"
#include "fcintl.h"
#include "game.h"
#include "genlist.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "shared.h"

#include "cityrep.h"
#include "colors.h"
#include "control.h"
#include "dialogs.h"
#include "graphics.h"
#include "helpdlg.h"
#include "mapctrl.h"
#include "mapview.h"
#include "repodlgs.h"
#include "tilespec.h"

#include "citydlg.h"

/* MUI Imports */

#include "muistuff.h"
#include "mapclass.h"

IMPORT Object *app;

/* TODO: o Rename of the city
 *       o Unit Context Menu
 */

#define NUM_CITYOPT_TOGGLES 5

struct city_dialog
{
  struct city *pcity;

  Object *wnd;

  Object *citizen_group;
  Object *citizen_left_space;
  Object *citizen_right_space;
  Object *citizen2_group;

  Object *title_text;
  Object *food_text;
  Object *shield_text;
  Object *trade_text;
  Object *gold_text;
  Object *luxury_text;
  Object *science_text;
  Object *granary_text;
  Object *pollution_text;

  Object *map_area;

  Object *prod_gauge;
  Object *buy_button;
  Object *change_button;
  struct Hook imprv_disphook;
  Object *imprv_listview;
  Object *sell_button;

  Object *present_group;
  Object *present2_group;
  Object *present_space;
  Object *supported_group;
  Object *supported2_group;
  Object *supported_space;

  Object *close_button;
  Object *rename_button;
  Object *trade_button;
  Object *activateunits_button;
  Object *unitlist_button;
  Object *configure_button;

  struct Hook prod_available_disphook;
  Object *prod_wnd;
  Object *prod_available_listview;
  Object *prod_change_button;
  Object *prod_help_button;
  Object *prod_cancel_button;

  int sell_id;

  Object *cityopt_wnd;
  Object *cityopt_cycle;
  Object *cityopt_checks[NUM_CITYOPT_TOGGLES];
};

extern struct connection aconnection;
extern int map_view_x0, map_view_y0;

/* GUI Independ */

static void activate_unit(struct unit *punit)
{
  if ((punit->activity != ACTIVITY_IDLE || punit->ai.control)
      && can_unit_do_activity(punit, ACTIVITY_IDLE))
    request_new_unit_activity(punit, ACTIVITY_IDLE);
  set_unit_focus(punit);
}

/****************************************************************
 ...
*****************************************************************/
void request_city_change_production(struct city *pcity, int id, int is_unit_id)
{
  struct packet_city_request packet;

  packet.city_id = pcity->id;
  packet.name[0] = '\0';
  packet.build_id = id;
  packet.is_build_id_unit_id = is_unit_id;

  send_packet_city_request(&aconnection, &packet, PACKET_CITY_CHANGE);
}
/****************************************************************
 ...
*****************************************************************/
void request_city_change_specialist(struct city *pcity, int from, int to)
{
  struct packet_city_request packet;

  packet.city_id = pcity->id;
  packet.name[0] = '\0';
  packet.specialist_from = from;
  packet.specialist_to = to;

  send_packet_city_request(&aconnection, &packet, PACKET_CITY_CHANGE_SPECIALIST);
}
/****************************************************************
 ...
*****************************************************************/
void request_city_toggle_worker(struct city *pcity, int xtile, int ytile)
{
  struct packet_city_request packet;
  packet.city_id = pcity->id;
  packet.worker_x = xtile;
  packet.worker_y = ytile;
  packet.name[0] = '\0';

  if (pcity->city_map[xtile][ytile] == C_TILE_WORKER)
    send_packet_city_request(&aconnection, &packet, PACKET_CITY_MAKE_SPECIALIST);
  else if (pcity->city_map[xtile][ytile] == C_TILE_EMPTY)
    send_packet_city_request(&aconnection, &packet, PACKET_CITY_MAKE_WORKER);
}
/****************************************************************
 ...
*****************************************************************/
void request_city_buy(struct city *pcity)
{
  struct packet_city_request packet;
  packet.city_id = pcity->id;
  packet.name[0] = '\0';
  send_packet_city_request(&aconnection, &packet, PACKET_CITY_BUY);
}
/****************************************************************
 ...
*****************************************************************/
void request_city_sell(struct city *pcity, int sell_id)
{
  struct packet_city_request packet;

  packet.city_id = pcity->id;
  packet.build_id = sell_id;
  packet.name[0] = '\0';
  send_packet_city_request(&aconnection, &packet, PACKET_CITY_SELL);
}

/* End GUI Independed */

static struct genlist dialog_list;
static int dialog_list_has_been_initialised;

struct city_dialog *get_city_dialog(struct city *pcity);
struct city_dialog *create_city_dialog(struct city *pcity, int make_modal);
void close_city_dialog(struct city_dialog *pdialog);

void city_dialog_update_improvement_list(struct city_dialog *pdialog);
void city_dialog_update_title(struct city_dialog *pdialog);
void city_dialog_update_supported_units(struct city_dialog *pdialog, int id);
void city_dialog_update_present_units(struct city_dialog *pdialog, int id);
void city_dialog_update_citizens(struct city_dialog *pdialog);
void city_dialog_update_map(struct city_dialog *pdialog);
void city_dialog_update_food(struct city_dialog *pdialog);
void city_dialog_update_production(struct city_dialog *pdialog);
void city_dialog_update_output(struct city_dialog *pdialog);
void city_dialog_update_building(struct city_dialog *pdialog);
void city_dialog_update_storage(struct city_dialog *pdialog);
void city_dialog_update_pollution(struct city_dialog *pdialog);

void open_cityopt_dialog(struct city_dialog *pdialog);

/****************************************************************
...
*****************************************************************/
struct city_dialog *get_city_dialog(struct city *pcity)
{
  struct genlist_iterator myiter;

  if (!dialog_list_has_been_initialised)
  {
    genlist_init(&dialog_list);
    dialog_list_has_been_initialised = 1;
  }

  genlist_iterator_init(&myiter, &dialog_list, 0);

  for (; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
    if (((struct city_dialog *) ITERATOR_PTR(myiter))->pcity == pcity)
      return ITERATOR_PTR(myiter);

  return 0;
}

/****************************************************************
...
*****************************************************************/
void refresh_this_city_dialog(struct city_dialog *pdialog)
{
  struct city *pcity = pdialog->pcity;
  int units = (unit_list_size(&map_get_tile(pcity->x, pcity->y)->units) ? TRUE : FALSE);

  city_dialog_update_improvement_list(pdialog);
  city_dialog_update_title(pdialog);
  city_dialog_update_supported_units(pdialog, 0);
  city_dialog_update_present_units(pdialog, 0);
  city_dialog_update_citizens(pdialog);
  city_dialog_update_map(pdialog);
  city_dialog_update_production(pdialog);
  city_dialog_update_output(pdialog);
  city_dialog_update_building(pdialog);
  city_dialog_update_storage(pdialog);
  city_dialog_update_pollution(pdialog);

  set(pdialog->trade_button, MUIA_Disabled, city_num_trade_routes(pcity) ? FALSE : TRUE);
  set(pdialog->activateunits_button, MUIA_Disabled, !units);
  set(pdialog->unitlist_button, MUIA_Disabled, !units);
  set(pdialog->configure_button, MUIA_Disabled, FALSE);
}

/****************************************************************
 Refresh every city dialog displaying this city
*****************************************************************/
void refresh_city_dialog(struct city *pcity)
{
  /* from get_city_dialog() */
  struct genlist_iterator myiter;

  if (!dialog_list_has_been_initialised)
  {
    genlist_init(&dialog_list);
    dialog_list_has_been_initialised = 1;
  }

  genlist_iterator_init(&myiter, &dialog_list, 0);

  for (; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
  {
    if (((struct city_dialog *) ITERATOR_PTR(myiter))->pcity == pcity)
    {
      struct city_dialog *pdialog = (struct city_dialog *) ITERATOR_PTR(myiter);

      refresh_this_city_dialog(pdialog);
      if (pcity->owner != game.player_idx)
      {
	/* Set the buttons we do not want live while a Diplomat investigates */
	set(pdialog->buy_button, MUIA_Disabled, TRUE);
	set(pdialog->sell_button, MUIA_Disabled, TRUE);
	set(pdialog->change_button, MUIA_Disabled, TRUE);
	set(pdialog->rename_button, MUIA_Disabled, TRUE);
	set(pdialog->activateunits_button, MUIA_Disabled, TRUE);
	set(pdialog->unitlist_button, MUIA_Disabled, TRUE);
	set(pdialog->configure_button, MUIA_Disabled, TRUE);
      }
    }
  }

  if (pcity->owner == game.player_idx)
  {
    city_report_dialog_update_city(pcity);
    trade_report_dialog_update();
  }
}

/****************************************************************
...
*****************************************************************/
void refresh_unit_city_dialogs(struct unit *punit)
{
  struct city *pcity_sup, *pcity_pre;
  struct city_dialog *pdialog;

  pcity_sup = city_list_find_id(&game.player_ptr->cities, punit->homecity);
  pcity_pre = map_get_city(punit->x, punit->y);

  if (pcity_sup && (pdialog = get_city_dialog(pcity_sup)))
    city_dialog_update_supported_units(pdialog, 0);

  if (pcity_pre && (pdialog = get_city_dialog(pcity_pre)))
    city_dialog_update_present_units(pdialog, 0);
}

/****************************************************************
popup the dialog 10% inside the main-window 
*****************************************************************/
void popup_city_dialog(struct city *pcity, int make_modal)
{
  struct city_dialog *pdialog;

  if (!(pdialog = get_city_dialog(pcity)))
    pdialog = create_city_dialog(pcity, make_modal);

  if (pdialog)
    set(pdialog->wnd, MUIA_Window_Open, TRUE);

}

/****************************************************************
popdown the dialog 
*****************************************************************/
void popdown_city_dialog(struct city *pcity)
{
  struct city_dialog *pdialog;

  if ((pdialog = get_city_dialog(pcity)))
    close_city_dialog(pdialog);
}

/****************************************************************
popdown all dialogs
*****************************************************************/
void popdown_all_city_dialogs(void)
{
  if (!dialog_list_has_been_initialised)
  {
    return;
  }
  while (genlist_size(&dialog_list))
  {
    close_city_dialog(genlist_get(&dialog_list, 0));
  }
}

/****************************************************************/

struct city_node
{
  struct MinNode node;
  struct city *pcity;
};

struct city_browse_msg
{
  struct city_dialog *pdialog;
  ULONG direction;
};				/* used by city_browse() */

struct city_map_msg
{
  struct city_dialog *pdialog;
  struct Map_Click *click;
};				/* used by city_click() */

struct city_citizen_msg
{
  struct city_dialog *pdialog;
  ULONG type;
};				/* used by city_citizen() */

struct city_unit_msg
{
  struct city_dialog *pdialog;
  struct unit *punit;
};				/* used by city_present() */

/****************************************************************
 Sort the city list alphabetically
*****************************************************************/
static void city_list_sort(struct MinList *list)
{
  BOOL notfinished = TRUE;

  /* Sort list (quick & dirty bubble sort) */
  while (notfinished)
  {
    struct city_node *first;

    /* Reset not finished flag */
    notfinished = FALSE;

    /* Get first node */
    if ((first = (struct city_node *) List_First(list)))
    {
      struct city_node *second;

      /* One bubble sort round */
      while ((second = (struct city_node *) Node_Next(first)))
      {
	if (Stricmp(first->pcity->name, second->pcity->name) > 0)
	{
	  Remove((struct Node *) first);
	  Insert((struct List *) list, (struct Node *) first, (struct Node *) second);
	  notfinished = TRUE;
	}
	else
	  first = second;
      }
    }
  }
}

/**************************************************************************
 Calculates the turns which are needed to build the requested
 type in the city. GUI Independend
**************************************************************************/
int city_turns_to_build(struct city *pcity, int id, int id_is_unit)
{
  if (pcity->shield_surplus > 0)
  {
    int rounds, cost;
    int shield_stock = pcity->shield_stock;

    if (id_is_unit)
    {
      if (!pcity->is_building_unit)
	shield_stock /= 2;
      cost = get_unit_type(id)->build_cost;
    }
    else
    {
      if (pcity->is_building_unit || (is_wonder(pcity->currently_building) != is_wonder(id)))
	shield_stock /= 2;
      cost = get_improvement_type(id)->build_cost;
    }

    rounds = (cost - shield_stock + pcity->shield_surplus - 1) / pcity->shield_surplus;

    return (rounds > 0) ? rounds : 0;
  }
  return 999;
}

/**************************************************************************
 Display function for the listview in the production window
**************************************************************************/
__asm __saveds static int city_prod_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 ULONG which)
{
  static char name[256];
  static char info[32];
  static char cost[32];
  static char rounds[32];

  struct city_dialog *pdialog = (struct city_dialog *) h->h_Data;
  struct city *pcity = pdialog->pcity;

  if (which)
  {
    if (which >= 10000)
    {
      if (which == 20000)
      {
	strcpy(name, "\33u\338Units\33n");
	info[0] = cost[0] = rounds[0] = 0;
      }
      else
      {
	if (which == 20001)
	{
	  strcpy(name, "\33u\338Improvements\33n");
	  info[0] = cost[0] = rounds[0] = 0;
	}
	else
	{
	  /* Unit */
	  which -= 10000;
	  strcpy(name, unit_name(which));

	  {
	    /* from unit.h get_unit_name() */
	    struct unit_type *ptype;
	    ptype = get_unit_type(which);
	    if (ptype->fuel)
	      sprintf(info, "%d/%d/%d(%d)", ptype->attack_strength,
		      ptype->defense_strength,
		ptype->move_rate / 3, (ptype->move_rate / 3) * ptype->fuel);
	    else
	      sprintf(info, "%d/%d/%d", ptype->attack_strength,
		      ptype->defense_strength, ptype->move_rate / 3);

	  }

	  sprintf(cost, "%d", get_unit_type(which)->build_cost);
	  sprintf(rounds, "%d", city_turns_to_build(pcity, which, TRUE));
	}
      }
    }
    else
    {
      which--;
      strcpy(name, get_improvement_type(which)->name);
      info[0] = 0;

      {
	/* from city.c get_imp_name_ex() */
	if (wonder_replacement(pdialog->pcity, which))
	{
	  strcpy(info, "*");
	}
	else
	{
	  if (is_wonder(which))
	  {
	    strcpy(info, "Wonder");
	    if (game.global_wonders[which])
	      strcpy(info, "Built");
	    if (wonder_obsolete(which))
	      strcpy(info, "Obsolette");
	  }
	}
      }

      if (which != B_CAPITAL)
      {
	sprintf(cost, "%d", get_improvement_type(which)->build_cost);
	sprintf(rounds, "%d", city_turns_to_build(pcity, which, FALSE));
      }
      else
      {
	strcpy(cost, "--");
	strcpy(rounds, "--");
      }
    }
    *array++ = name;
    *array++ = info;
    *array++ = rounds;
    *array++ = cost;
    *array = NULL;
  }
  else
  {
    *array++ = "Type";
    *array++ = "Info";
    *array++ = "Rounds";
    *array++ = "Cost";
    *array = NULL;
  }
  return 0;
}

/**************************************************************************
 Display function for the listview in the city window
**************************************************************************/
__asm __saveds static int city_imprv_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 ULONG which)
{
  static char name[256];
  static char cost[32];
  if (which)
  {
    struct city_dialog *pdialog = (struct city_dialog *) h->h_Data;
    which--;
    sprintf(name, "%s", get_imp_name_ex(pdialog->pcity, which), get_improvement_type(which)->build_cost);
    sprintf(cost, "%d", improvement_upkeep(pdialog->pcity, which));
    *array++ = name;
    *array++ = cost;
  }
  else
  {
    *array++ = "Type";
    *array++ = "Upkeep";
  }
  return 0;
}

/**************************************************************************
 Must be called from the Application object so it is safe to
 dispose the window
**************************************************************************/
static int city_close_real(struct city_dialog **ppdialog)
{
  close_city_dialog(*ppdialog);
  return 0;
}

/**************************************************************************
 Callback for the Close Button (or CloseGadget)
**************************************************************************/
static int city_close(struct city_dialog **ppdialog)
{
  set((*ppdialog)->wnd, MUIA_Window_Open, FALSE);
  set((*ppdialog)->prod_wnd, MUIA_Window_Open, FALSE);
  DoMethod(app, MUIM_Application_PushMethod, app, 4, MUIM_CallHook, &standart_hook, city_close_real, *ppdialog);
  return NULL;
}

/**************************************************************************
 Callback for the Change button in the production window
**************************************************************************/
static int city_prod_change(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  LONG sel = xget(pdialog->prod_available_listview, MUIA_NList_Active);
  if (sel >= 0)
  {
    LONG which = 0;
    LONG is_unit = 0;

    DoMethod(pdialog->prod_available_listview, MUIM_NList_GetEntry, sel, &which);
    if (which >= 10000)
    {
      if (which == 20000 || which == 20001)
	return NULL;
      which -= 10000;
      is_unit = 1;
    }
    else
      which--;

    request_city_change_production(pdialog->pcity, which, is_unit);
    set(pdialog->prod_wnd, MUIA_Window_Open, FALSE);
  }
  return NULL;
}

/**************************************************************************
 Callback for the Help button in the production window
**************************************************************************/
static int city_prod_help(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  LONG sel = xget(pdialog->prod_available_listview, MUIA_NList_Active);
  if (sel >= 0)
  {
    LONG which = 0;

    DoMethod(pdialog->prod_available_listview, MUIM_NList_GetEntry, sel, &which);
    if (which >= 10000)
    {
      which -= 10000;
      popup_help_dialog_typed(get_unit_type(which)->name, HELP_UNIT);
    }
    else
    {
      which--;
      if (is_wonder(which))
      {
	popup_help_dialog_typed(get_improvement_name(which), HELP_WONDER);
      }
      else
      {
	popup_help_dialog_typed(get_improvement_name(which), HELP_IMPROVEMENT);
      }
    }
  }
  return NULL;
}

/**************************************************************************
 Callback if a message window should be closed
**************************************************************************/
static int city_msg_no(struct popup_message_data *data)
{
  destroy_message_dialog(data->wnd);
  return NULL;
}

/**************************************************************************
 Callback for the Yes in the Buy confirmation window
**************************************************************************/
static city_buy_yes(struct popup_message_data *data)
{
  struct city_dialog *pdialog = (struct city_dialog *) data->data;
  request_city_buy(pdialog->pcity);
  destroy_message_dialog(data->wnd);
  return NULL;
}

/**************************************************************************
 Callback for the Yes in the Sell confirmation window
**************************************************************************/
static int city_sell_yes(struct popup_message_data *data)
{
  struct city_dialog *pdialog = (struct city_dialog *) data->data;

  if (pdialog->sell_id >= 0)
  {
    request_city_sell(pdialog->pcity, pdialog->sell_id);
  }
  else
    pdialog->sell_id = -1;

  destroy_message_dialog(data->wnd);
  return 0;
}

/**************************************************************************
 Callback to accept the options in the configure window
**************************************************************************/
static int city_opt_ok(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  struct city *pcity = pdialog->pcity;

  if (pcity)
  {
    struct packet_generic_values packet;
    int i, new, newcitizen_index = xget(pdialog->cityopt_cycle, MUIA_Cycle_Active);

    new = 0;
    for (i = 0; i < NUM_CITYOPT_TOGGLES; i++)
    {
      if (xget(pdialog->cityopt_checks[i], MUIA_Selected))
	new |= (1 << i);
    }
    if (newcitizen_index == 1)
    {
      new |= (1 << CITYO_NEW_EINSTEIN);
    }
    else if (newcitizen_index == 2)
    {
      new |= (1 << CITYO_NEW_TAXMAN);
    }

    packet.value1 = pcity->id;
    packet.value2 = new;
    send_packet_generic_values(&aconnection, PACKET_CITY_OPTIONS,
			       &packet);
  }
  set(pdialog->cityopt_wnd, MUIA_Window_Open, FALSE);
  return NULL;
}

/**************************************************************************
 Callback for the Trade button
**************************************************************************/
static int city_trade(struct city_dialog **ppdialog)
{
  int i;
  int x = 0, total = 0;
  char buf[512], *bptr = buf;
  struct city_dialog *pdialog = *ppdialog;

  sprintf(buf, "These trade routes have been established with %s:\n",
	  pdialog->pcity->name);
  bptr += strlen(bptr);

  for (i = 0; i < 4; i++)
  {
    if (pdialog->pcity->trade[i])
    {
      struct city *pcity;
      x = 1;
      total += pdialog->pcity->trade_value[i];
      if ((pcity = find_city_by_id(pdialog->pcity->trade[i])))
      {
	sprintf(bptr, "%s: %2d Trade/Turn\n", pcity->name, pdialog->pcity->trade_value[i]);
	bptr += strlen(bptr);
      }
      else
      {
	sprintf(bptr, "%s: %2d Trade/Turn\n", "Unknown", pdialog->pcity->trade_value[i]);
	bptr += strlen(bptr);
      }
    }
  }

  if (!x)
    sprintf(bptr, "No trade routes exist.\n");
  else
    sprintf(bptr, "\nTotal trade %d Trade/Turn\n", total);

  popup_message_dialog(pdialog->wnd, "Trade Routes", buf,
		       "_Done", message_close, 0,
		       0);

  return 0;
}

/**************************************************************************
 Callback for the List button
**************************************************************************/
static int city_unitlist(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  struct tile *ptile = map_get_tile(pdialog->pcity->x, pdialog->pcity->y);

  if (unit_list_size(&ptile->units))
    popup_unit_select_dialog(ptile);

  return NULL;
}

/**************************************************************************
 Callback for the Activate All button
**************************************************************************/
static int city_activate_units(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  int x = pdialog->pcity->x, y = pdialog->pcity->y;
  struct unit_list *punit_list = &map_get_tile(x, y)->units;

  if (unit_list_size(punit_list))
  {
    unit_list_iterate((*punit_list), punit)
    {
      if (game.player_idx == punit->owner)
      {
	request_new_unit_activity(punit, ACTIVITY_IDLE);
      }
    }
    unit_list_iterate_end;
    set_unit_focus(unit_list_get(punit_list, 0));
  }
  return 0;
}

/**************************************************************************
 Callback for the Configure button
**************************************************************************/
static int city_configure(struct city_dialog **ppdialog)
{
  open_cityopt_dialog(*ppdialog);
  return 0;
}

/**************************************************************************
 Callback for the Change button
**************************************************************************/
static int city_change(struct city_dialog **ppdialog)
{
  int i;
  int pos = 0;
  int current = -1;
  int improv = 0;
  struct city_dialog *pdialog = *ppdialog;
  struct city *pcity = pdialog->pcity;
  set(pdialog->prod_available_listview, MUIA_NList_Quiet, TRUE);

  DoMethod(pdialog->prod_available_listview, MUIM_NList_Clear);

  for (i = 0; i < B_LAST; i++)
  {
    if (can_build_improvement(pcity, i))
    {
      improv = TRUE;

      DoMethod(pdialog->prod_available_listview, MUIM_NList_InsertSingle, i + 1, MUIV_NList_Insert_Bottom);

      if (i == pcity->currently_building && !pcity->is_building_unit)
	current = ++pos;

      pos++;
    }
  }

  if (improv)
  {
    DoMethod(pdialog->prod_available_listview, MUIM_NList_InsertSingle, 20001, MUIV_NList_Insert_Top);
    if (current == -1)
      pos++;
  }
  DoMethod(pdialog->prod_available_listview, MUIM_NList_InsertSingle, 20000, MUIV_NList_Insert_Bottom);

  for (i = 0; i < game.num_unit_types; i++)
  {
    if (can_build_unit(pcity, i))
    {
      DoMethod(pdialog->prod_available_listview, MUIM_NList_InsertSingle, i + 10000, MUIV_NList_Insert_Bottom);

      if (i == pcity->currently_building && pcity->is_building_unit)
	current = ++pos;

      pos++;
    }
  }

  set(pdialog->prod_available_listview, MUIA_NList_Quiet, FALSE);

  if (current != -1)
  {
    set(pdialog->prod_available_listview, MUIA_NList_Active, current);
  }

  set(pdialog->prod_wnd, MUIA_Window_Open, TRUE);
  return NULL;
}

/**************************************************************************
 Callback for the browse buttons (switch the city which is display to
 the next or previous. Alphabetically)
**************************************************************************/
static int city_browse(struct city_browse_msg *msg)
{
  struct player *pplayer = get_player(msg->pdialog->pcity->owner);
  struct city *pcity_new;
  struct MinList list;
  struct city_node *node;
  NewList((struct List *) &list);

  city_list_iterate(pplayer->cities, pcity)
  {
    struct city_node *node = malloc_struct(struct city_node);
    if (node)
    {
      node->pcity = pcity;
      AddTail((struct List *) &list, (struct Node *) node);
    }
  }
  city_list_iterate_end

    city_list_sort(&list);

  node = (struct city_node *) List_First(&list);
  while (node)
  {
    if (node->pcity == msg->pdialog->pcity)
      break;
    node = (struct city_node *) Node_Next(node);
  }

  if (node)
  {
    if (msg->direction)
    {
      node = (struct city_node *) Node_Next(node);
      if (!node)
	node = (struct city_node *) List_First(&list);
    }
    else
    {
      node = (struct city_node *) Node_Prev(node);
      if (!node)
	node = (struct city_node *) List_Last(&list);
    }
  }

  if (node)
  {
    if ((pcity_new = node->pcity))
    {
      msg->pdialog->pcity = pcity_new;
      msg->pdialog->sell_id = -1;

      set(msg->pdialog->map_area, MUIA_CityMap_City, pcity_new);
      refresh_this_city_dialog(msg->pdialog);
    }
  }

  return NULL;
}

/**************************************************************************
 Callback for the Buy button
**************************************************************************/
static int city_buy(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  int value;
  char *name;
  char buf[512];

  if (pdialog->pcity->is_building_unit)
  {
    name = get_unit_type(pdialog->pcity->currently_building)->name;
  }
  else
  {
    name = get_imp_name_ex(pdialog->pcity, pdialog->pcity->currently_building);
  }

  value = city_buy_cost(pdialog->pcity);

  if (game.player_ptr->economic.gold >= value)
  {
    sprintf(buf, "Buy %s for %d gold?\nTreasury contains %d gold.",
	    name, value, game.player_ptr->economic.gold);

    popup_message_dialog(pdialog->wnd, "Buy It!", buf,
			 "_Yes", city_buy_yes, pdialog,
			 "_No", city_msg_no, pdialog,
			 NULL);
  }
  else
  {
    sprintf(buf, "%s costs %d gold.\nTreasury contains %d gold.",
	    name, value, game.player_ptr->economic.gold);

    popup_message_dialog(pdialog->wnd, "Buy It!", buf,
			 "_Darn", city_msg_no, pdialog,
			 NULL);
  }

  return NULL;
}

/**************************************************************************
 Callback for the Sell button
**************************************************************************/
static int city_sell(struct city_dialog **ppdialog)
{
  struct city_dialog *pdialog = *ppdialog;
  LONG sel = xget(pdialog->imprv_listview, MUIA_NList_Active);
  if (sel >= 0)
  {
    LONG i = 0;
    char buf[512];
    DoMethod(pdialog->imprv_listview, MUIM_NList_GetEntry, sel, &i);

    if (!i)
      return 0;
    i--;

    if (is_wonder(i))
      return 0;

    sprintf(buf, "Sell %s for %d gold?", get_imp_name_ex(pdialog->pcity, i),
	    improvement_value(i));

    pdialog->sell_id = i;
    popup_message_dialog(pdialog->wnd, "Sell It!", buf,
			 "_Yes", city_sell_yes, pdialog,
			 "_No", city_msg_no, pdialog,
			 NULL);
  }
  return 0;
}

/**************************************************************************
 Callback if the user clicked on a map
**************************************************************************/
static int city_click(struct city_map_msg *msg)
{
  struct city *pcity = msg->pdialog->pcity;
  int xtile = msg->click->x;
  int ytile = msg->click->y;

  request_city_toggle_worker(pcity, xtile, ytile);
  return 0;
}

/**************************************************************************
 Callback if the user clicked on a citizen
**************************************************************************/
static int city_citizen(struct city_citizen_msg *msg)
{
  struct city_dialog *pdialog = msg->pdialog;
  struct city *pcity = pdialog->pcity;

  switch (msg->type)
  {
  case 0:
    request_city_change_specialist(pcity, SP_ELVIS, SP_SCIENTIST);
    break;

  case 1:
    request_city_change_specialist(pcity, SP_SCIENTIST, SP_TAXMAN);
    break;

  default:
    request_city_change_specialist(pcity, SP_TAXMAN, SP_ELVIS);
    break;
  }

  return 0;
}

/**************************************************************************
 Callback if the user clicked on a present unit
**************************************************************************/
static int city_present(struct city_unit_msg *data)
{
  activate_unit(data->punit);
  return NULL;
}

/**************************************************************************
 Allocate and initialize a new city dialog
**************************************************************************/
struct city_dialog *create_city_dialog(struct city *pcity, int make_modal)
{
  struct city_dialog *pdialog;
  static char *newcitizen_labels[] =
  {"Workers", "Scientists", "Taxmen", NULL};

  Object *cityopt_ok_button;
  Object *cityopt_cancel_button, *next_button, *prev_button;

  pdialog = AllocVec(sizeof(struct city_dialog), 0x10000);
  if (!pdialog)
    return NULL;

  pdialog->pcity = pcity;
  pdialog->prod_available_disphook.h_Entry = (HOOKFUNC) city_prod_display;
  pdialog->prod_available_disphook.h_Data = pdialog;

  pdialog->imprv_disphook.h_Entry = (HOOKFUNC) city_imprv_display;
  pdialog->imprv_disphook.h_Data = pdialog;

  pdialog->wnd = WindowObject,
    MUIA_Window_Title, "Freeciv - Cityview",
    MUIA_Window_ID, 'CITY',
    WindowContents, VGroup,
	Child, HGroup,
	Child, HVSpace,
	Child, prev_button = MakeButton("_<"),
	Child, pdialog->title_text = TextObject,
	    MUIA_Weight, 200,
	    MUIA_Text_PreParse, "\33c",
	    End,
	Child, next_button = MakeButton("_>"),
	Child, HVSpace,
	End,
    Child, pdialog->citizen_group = HGroup,
	Child, pdialog->citizen_left_space = HSpace(0),
	Child, pdialog->citizen_right_space = HSpace(0),
	End,
    Child, HGroup,
	Child, VGroup,
	    Child, VSpace(0),
		Child, ColGroup(2),
		    Child, MakeLabel("Food:"),
		    Child, pdialog->food_text = TextObject, End,
		    Child, MakeLabel("Shields:"),
		    Child, pdialog->shield_text = TextObject, End,
		    Child, MakeLabel("Trade:"),
		    Child, pdialog->trade_text = TextObject, End,
		    Child, MakeLabel("Gold:"),
		    Child, pdialog->gold_text = TextObject, End,
		    Child, MakeLabel("Luxury:"),
		    Child, pdialog->luxury_text = TextObject, End,
		    Child, MakeLabel("Science:"),
		    Child, pdialog->science_text = TextObject, End,
		    Child, MakeLabel("Granary:"),
		    Child, pdialog->granary_text = TextObject, End,
		    Child, MakeLabel("Pollution:"),
		    Child, pdialog->pollution_text = TextObject, End,
		    End,
		Child, VSpace(0),
		End,
	    Child, VGroup,
		Child, VSpace(0),
		Child, pdialog->map_area = MakeCityMap(pcity),
		Child, VSpace(0),
		End,
	    Child, VGroup,
		MUIA_Weight, 150,
		Child, pdialog->prod_gauge = MyGaugeObject,
		    GaugeFrame,
		    MUIA_Gauge_Horiz, TRUE,
		    End,
		Child, HGroup,
		    Child, pdialog->buy_button = MakeButton("_Buy"),
		    Child, pdialog->change_button = MakeButton("Chan_ge"),
		    End,
		Child, pdialog->imprv_listview = NListviewObject,
		    MUIA_CycleChain, 1,
		    MUIA_NListview_NList, NListObject,
			MUIA_NList_DisplayHook, &pdialog->imprv_disphook,
			MUIA_NList_Format, ",P=\33c",
			MUIA_NList_Title, TRUE,
			End,
		    End,
		Child, pdialog->sell_button = MakeButton("_Sell"),
		End,
	    End,
	Child, pdialog->supported_group = HGroupV,
	    GroupFrameT("Supported Units"),
	    Child, pdialog->supported_space = HSpace(0),
	    End,
	Child, HGroup,
	    GroupFrameT("Present Units"),
	    Child, pdialog->present_group = HGroupV,
		Child, pdialog->present_space = HSpace(0),
		End,
	    Child, VGroup,
		MUIA_HorizWeight, 0,
		Child, HVSpace,
		Child, pdialog->activateunits_button = MakeButton("_Activate all"),
		Child, pdialog->unitlist_button = MakeButton("_List"),
		Child, HVSpace,
		End,
	    End,

	Child, HGroup,
	    Child, pdialog->close_button = MakeButton("_Close"),
	    Child, pdialog->rename_button = MakeButton("_Rename"),
	    Child, pdialog->trade_button = MakeButton("_Trade"),
	    Child, pdialog->configure_button = MakeButton("Con_figure"),
	    End,
	End,
    End;

  pdialog->prod_wnd = WindowObject,
    MUIA_Window_Title, "Freeciv - Cityproduction",
    MUIA_Window_ID, 'PROD',
    WindowContents, VGroup,
	Child, pdialog->prod_available_listview = NListviewObject,
	    MUIA_CycleChain, 1,
	    MUIA_NListview_NList, NListObject,
		MUIA_NList_DisplayHook, &pdialog->prod_available_disphook,
		MUIA_NList_Format, "BAR,P=\33c BAR,P=\33c BAR,P=\33r NOBAR,",
		MUIA_NList_Title, TRUE,
		MUIA_NList_AutoVisible, TRUE,
		End,
	    End,
	Child, HGroup,
	    Child, pdialog->prod_change_button = MakeButton("Chan_ge"),
	    Child, pdialog->prod_help_button = MakeButton("_Help"),
	    Child, pdialog->prod_cancel_button = MakeButton("_Cancel"),
	    End,
	End,
    End;

  pdialog->cityopt_wnd = WindowObject,
    MUIA_Window_Title, "Freeciv - Cityoptions",
    MUIA_Window_ID, 'ID',
    WindowContents, VGroup,
    Child, HGroup,
    Child, HSpace(0),
    Child, ColGroup(2),
    Child, MakeLabel("_New specialists are"),
    Child, pdialog->cityopt_cycle = MakeCycle("_New specialists are", newcitizen_labels),

    Child, MakeLabel("_Disband if build settler at size 1"),
    Child, pdialog->cityopt_checks[4] = MakeCheck("_Disband if build settler at size 1", FALSE),

    Child, MakeLabel("Auto-attack vs _land units"),
    Child, pdialog->cityopt_checks[0] = MakeCheck("Auto-attack vs _land units", FALSE),

    Child, MakeLabel("Auto-attack vs _sea units"),
    Child, pdialog->cityopt_checks[1] = MakeCheck("Auto-attack vs _sea units", FALSE),

    Child, MakeLabel("Auto-attack vs _air units"),
    Child, pdialog->cityopt_checks[3] = MakeCheck("Auto-attack vs _air units", FALSE),

    Child, MakeLabel("Auto-attack vs _helicopters"),
    Child, pdialog->cityopt_checks[2] = MakeCheck("Auto-attack vs _helicopters", FALSE),
    End,
    Child, HSpace(0),
    End,
    Child, HGroup,
    Child, cityopt_ok_button = MakeButton("_Ok"),
    Child, cityopt_cancel_button = MakeButton("_Cancel"),
    End,
    End,
    End;

  if (pdialog->wnd && pdialog->prod_wnd && pdialog->cityopt_wnd)
  {
    set(next_button, MUIA_Weight, 0);
    set(prev_button, MUIA_Weight, 0);
    DoMethod(pdialog->wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 4, MUIM_CallHook, &standart_hook, city_close, pdialog);
    DoMethod(pdialog->close_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_close, pdialog);
    DoMethod(pdialog->trade_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_trade, pdialog);
    DoMethod(pdialog->configure_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_configure, pdialog);
    DoMethod(pdialog->unitlist_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_unitlist, pdialog);
    DoMethod(pdialog->activateunits_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_activate_units, pdialog);
    DoMethod(prev_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, city_browse, pdialog, 0);
    DoMethod(next_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, city_browse, pdialog, 1);

    DoMethod(pdialog->map_area, MUIM_Notify, MUIA_CityMap_Click, MUIV_EveryTime, app, 5, MUIM_CallHook, &standart_hook, city_click, pdialog, MUIV_TriggerValue);
    DoMethod(pdialog->change_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_change, pdialog);
    DoMethod(pdialog->buy_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_buy, pdialog);
    DoMethod(pdialog->sell_button, MUIM_Notify, MUIA_Pressed, FALSE, app, 4, MUIM_CallHook, &standart_hook, city_sell, pdialog);

    DoMethod(pdialog->prod_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(pdialog->prod_cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(pdialog->prod_available_listview, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, MUIV_Notify_Self, 4, MUIM_CallHook, &standart_hook, city_prod_change, pdialog);
    DoMethod(pdialog->prod_change_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Self, 4, MUIM_CallHook, &standart_hook, city_prod_change, pdialog);
    DoMethod(pdialog->prod_help_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Self, 4, MUIM_CallHook, &standart_hook, city_prod_help, pdialog);

    DoMethod(pdialog->cityopt_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(cityopt_cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(cityopt_ok_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Self, 4, MUIM_CallHook, &standart_hook, city_opt_ok, pdialog);

    DoMethod(app, OM_ADDMEMBER, pdialog->wnd);
    DoMethod(app, OM_ADDMEMBER, pdialog->prod_wnd);
    DoMethod(app, OM_ADDMEMBER, pdialog->cityopt_wnd);

    genlist_insert(&dialog_list, pdialog, 0);
    refresh_city_dialog(pdialog->pcity);
    return pdialog;
  }

  if (pdialog->wnd)
    MUI_DisposeObject(pdialog->wnd);
  if (pdialog->prod_wnd)
    MUI_DisposeObject(pdialog->prod_wnd);
  if (pdialog->cityopt_wnd)
    MUI_DisposeObject(pdialog->cityopt_wnd);

  FreeVec(pdialog);
  return NULL;
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_pollution(struct city_dialog *pdialog)
{
  /* TODO: differnt colors? */
  settextf(pdialog->pollution_text, "%3ld", pdialog->pcity->pollution);
}


/****************************************************************
...
*****************************************************************/
void city_dialog_update_storage(struct city_dialog *pdialog)
{
  struct city *pcity = pdialog->pcity;
  settextf(pdialog->granary_text, "%3ld/%3ld", pcity->food_stock, game.foodbox * (pcity->size + 1));
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_building(struct city_dialog *pdialog)
{
  char buf3[128];
  char buf[32], buf2[64];
  struct city *pcity = pdialog->pcity;

  int max_shield;
  int shield;

  set(pdialog->buy_button, MUIA_Disabled, pcity->did_buy);
  set(pdialog->sell_button, MUIA_Disabled, pcity->did_sell);

  if (pcity->is_building_unit)
  {
    sprintf(buf, "%d/%d", pcity->shield_stock, get_unit_type(pcity->currently_building)->build_cost);
    sprintf(buf2, "%s", get_unit_type(pcity->currently_building)->name);

    shield = pcity->shield_stock;
    max_shield = get_unit_type(pcity->currently_building)->build_cost;
  }
  else
  {
    if (pcity->currently_building == B_CAPITAL)
    {
      /* Capitalization is special, you can't buy it or finish making it */
      sprintf(buf, "%d/XXX", pcity->shield_stock);
      set(pdialog->buy_button, MUIA_Disabled, TRUE);

      shield = 0;
      max_shield = 1;
    }
    else
    {
      sprintf(buf, "%d/%d", pcity->shield_stock,
	      get_improvement_type(pcity->currently_building)->build_cost);

      shield = pcity->shield_stock;
      max_shield = get_improvement_type(pcity->currently_building)->build_cost;
    }
    sprintf(buf2, "%s", get_imp_name_ex(pcity, pcity->currently_building));
  }

  sprintf(buf3, "%s (%s)", buf2, buf);
  DoMethod(pdialog->prod_gauge, MUIM_MyGauge_SetGauge, shield, max_shield, buf3);
}


/****************************************************************
...
*****************************************************************/
void city_dialog_update_production(struct city_dialog *pdialog)
{
  struct city *pcity = pdialog->pcity;
  settextf(pdialog->food_text, "%2d (%+2d)", pcity->food_prod, pcity->food_surplus);
  settextf(pdialog->shield_text, "%2d (%+2d)", pcity->shield_prod, pcity->shield_surplus);
  settextf(pdialog->trade_text, "%2d (%+2d)", pcity->trade_prod + pcity->corruption, pcity->trade_prod);
}
/****************************************************************
...
*****************************************************************/
void city_dialog_update_output(struct city_dialog *pdialog)
{
  struct city *pcity = pdialog->pcity;
  settextf(pdialog->gold_text, "%2d (%+2d)", pcity->tax_total, city_gold_surplus(pcity));
  settextf(pdialog->luxury_text, "%2d", pcity->luxury_total);
  settextf(pdialog->science_text, "%2d", pcity->science_total);
}


/****************************************************************
...
*****************************************************************/
void city_dialog_update_map(struct city_dialog *pdialog)
{
  DoMethod(pdialog->map_area, MUIM_CityMap_Refresh);
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_citizens(struct city_dialog *pdialog)
{
  int i, n;
  struct city *pcity = pdialog->pcity;

  DoMethod(pdialog->citizen_group, MUIM_Group_InitChange);
  if (pdialog->citizen2_group)
  {
    DoMethod(pdialog->citizen_group, OM_REMMEMBER, pdialog->citizen2_group);
    MUI_DisposeObject(pdialog->citizen2_group);
  }

  pdialog->citizen2_group = HGroup, GroupSpacing(0), End;

  for (i = 0, n = 0; n < pcity->ppl_happy[4] /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(5 + i % 2));
    if (o)
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
  }

  for (i = 0, n = 0; n < pcity->ppl_content[4] /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(3 + i % 2));
    if (o)
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
  }


  for (i = 0, n = 0; n < pcity->ppl_unhappy[4] /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(7 + i % 2));
    if (o)
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
  }


  for (n = 0; n < pcity->ppl_elvis /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(0));
    if (o)
    {
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
      DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, o, 5, MUIM_CallHook, &standart_hook, city_citizen, pdialog, 0);
    }
  }

  for (n = 0; n < pcity->ppl_scientist /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(1));
    if (o)
    {
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
      DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, o, 5, MUIM_CallHook, &standart_hook, city_citizen, pdialog, 1);
    }
  }

  for (n = 0; n < pcity->ppl_taxman /*&& i<NUM_CITIZENS_SHOWN */ ; n++, i++)
  {
    Object *o = MakeSprite(get_citizen_sprite(2));

    if (o)
    {
      DoMethod(pdialog->citizen2_group, OM_ADDMEMBER, o);
      DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, o, 5, MUIM_CallHook, &standart_hook, city_citizen, pdialog, 2);
    }
  }

  DoMethod(pdialog->citizen_group, OM_ADDMEMBER, pdialog->citizen2_group);

  DoMethod(pdialog->citizen_group, MUIM_Group_Sort,
	   pdialog->citizen_left_space, pdialog->citizen2_group, pdialog->citizen_right_space, NULL);

  DoMethod(pdialog->citizen_group, MUIM_Group_ExitChange);
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_supported_units(struct city_dialog *pdialog,
					int unitid)
{

  int i;
  struct genlist_iterator myiter;
  struct unit *punit;

  /* TODO: use unit id */

  DoMethod(pdialog->supported_group, MUIM_Group_InitChange);
  if (pdialog->supported2_group)
  {
    DoMethod(pdialog->supported_group, OM_REMMEMBER, pdialog->supported2_group);
    MUI_DisposeObject(pdialog->supported2_group);
  }

  pdialog->supported2_group = HGroup, End;

  genlist_iterator_init(&myiter, &pdialog->pcity->units_supported.list, 0);

  for (i = 0; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter), i++)
  {
    Object *o;

    punit = (struct unit *) ITERATOR_PTR(myiter);
    o = MakeUnit(punit, TRUE);
    if (o)
      DoMethod(pdialog->supported2_group, OM_ADDMEMBER, o);
  }

  DoMethod(pdialog->supported2_group, OM_ADDMEMBER, VSpace(get_normal_tile_height()));
  DoMethod(pdialog->supported_group, OM_ADDMEMBER, pdialog->supported2_group);
  DoMethod(pdialog->supported_group, MUIM_Group_Sort, pdialog->supported2_group, pdialog->supported_space, NULL);
  DoMethod(pdialog->supported_group, MUIM_Group_ExitChange);
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_present_units(struct city_dialog *pdialog, int unitid)
{
  int i;
  struct genlist_iterator myiter;
  struct unit *punit;

  /* TODO: use unit id */

  DoMethod(pdialog->present_group, MUIM_Group_InitChange);
  if (pdialog->present2_group)
  {
    DoMethod(pdialog->present_group, OM_REMMEMBER, pdialog->present2_group);
    MUI_DisposeObject(pdialog->present2_group);
  }


  pdialog->present2_group = HGroup, End;

  genlist_iterator_init(&myiter,
	&map_get_tile(pdialog->pcity->x, pdialog->pcity->y)->units.list, 0);

  for (i = 0; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter), i++)
  {
    Object *o;

    punit = (struct unit *) ITERATOR_PTR(myiter);
    o = MakeUnit(punit, FALSE);
    if (o)
    {
      DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, app, 5, MUIM_CallHook, &standart_hook, city_present, pdialog, punit);
      DoMethod(pdialog->present2_group, OM_ADDMEMBER, o);
    }
  }

  DoMethod(pdialog->present2_group, OM_ADDMEMBER, VSpace(get_normal_tile_height()));
  DoMethod(pdialog->present_group, OM_ADDMEMBER, pdialog->present2_group);
  DoMethod(pdialog->present_group, MUIM_Group_Sort, pdialog->present2_group, pdialog->present_space, NULL);
  DoMethod(pdialog->present_group, MUIM_Group_ExitChange);
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_title(struct city_dialog *pdialog)
{
  settextf(pdialog->title_text, "%s - %s citizens", pdialog->pcity->name, int_to_text(city_population(pdialog->pcity)));
}

/****************************************************************
...
*****************************************************************/
void city_dialog_update_improvement_list(struct city_dialog *pdialog)
{
  int i, j = 0, refresh = FALSE;

  for (i = 0; i < B_LAST; ++i)
  {
    if (pdialog->pcity->improvements[i])
    {
      LONG imprv;

      DoMethod(pdialog->imprv_listview, MUIM_NList_GetEntry, j, &imprv);
      if (!imprv)
	refresh = TRUE;
      else if (imprv - 1 != i)
	refresh = TRUE;
      j++;
    }
  }

  if (refresh)
  {
    set(pdialog->imprv_listview, MUIA_NList_Quiet, TRUE);
    DoMethod(pdialog->imprv_listview, MUIM_NList_Clear);

    for (i = 0; i < B_LAST; ++i)
    {
      if (pdialog->pcity->improvements[i])
      {
	DoMethod(pdialog->imprv_listview, MUIM_NList_InsertSingle, i + 1, MUIV_NList_Insert_Bottom);
      }
    }

    set(pdialog->imprv_listview, MUIA_NList_Quiet, FALSE);
  }
}

/****************************************************************
...
*****************************************************************/
void close_city_dialog(struct city_dialog *pdialog)
{
  if (pdialog)
  {
    genlist_unlink(&dialog_list, pdialog);
    set(pdialog->wnd, MUIA_Window_Open, FALSE);
    set(pdialog->prod_wnd, MUIA_Window_Open, FALSE);
    set(pdialog->cityopt_wnd, MUIA_Window_Open, FALSE);
    DoMethod(app, OM_REMMEMBER, pdialog->wnd);
    DoMethod(app, OM_REMMEMBER, pdialog->prod_wnd);
    DoMethod(app, OM_REMMEMBER, pdialog->cityopt_wnd);
    MUI_DisposeObject(pdialog->wnd);
    MUI_DisposeObject(pdialog->prod_wnd);
    MUI_DisposeObject(pdialog->cityopt_wnd);
    FreeVec(pdialog);
  }
}

/**************************************************************************
 Open the City Options dialog for this city
**************************************************************************/
void open_cityopt_dialog(struct city_dialog *pdialog)
{
  struct city *pcity = pdialog->pcity;
  int i, state, newcitizen_index;

  for (i = 0; i < NUM_CITYOPT_TOGGLES; i++)
  {
    state = (pcity->city_options & (1 << i));
    set(pdialog->cityopt_checks[i], MUIA_Selected, state);
  }
  if (pcity->city_options & (1 << CITYO_NEW_EINSTEIN))
  {
    newcitizen_index = 1;
  }
  else if (pcity->city_options & (1 << CITYO_NEW_TAXMAN))
  {
    newcitizen_index = 2;
  }
  else
  {
    newcitizen_index = 0;
  }

  set(pdialog->cityopt_cycle, MUIA_Cycle_Active, newcitizen_index);
  set(pdialog->cityopt_wnd, MUIA_Window_Open, TRUE);
}
