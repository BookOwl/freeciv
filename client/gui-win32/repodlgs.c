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

#include <assert.h>          
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "packets.h"
#include "shared.h"
#include "support.h"
 
#include "cityrep.h"
#include "civclient.h"
#include "climisc.h"
#include "dialogs.h"
#include "gui_stuff.h"
#include "gui_main.h"
#include "helpdlg.h"
#include "optiondlg.h"    
#include "repodlgs_common.h"
#include "repodlgs.h"
#include "text.h"

static HWND economy_dlg;
static HWND activeunits_dlg;
static HWND science_dlg;
extern HINSTANCE freecivhinst;
extern HWND root_window;
extern struct connection aconnection;               
int economy_improvement_type[B_LAST];   
int activeunits_type[U_LAST]; 

/**************************************************************************

**************************************************************************/
void
update_report_dialogs(void)
{
  if(is_report_dialogs_frozen()) return;
  activeunits_report_dialog_update();
  economy_report_dialog_update();
  city_report_dialog_update();
  science_dialog_update();    
}

/**************************************************************************

**************************************************************************/
void
science_dialog_update(void)
{
 
  char text[512];
  int i, hist, id, steps;

  if (!science_dlg) return;            
  sz_strlcpy(text, get_report_title(_("Science")));
  sz_strlcat(text, science_dialog_text());
  SetWindowText(GetDlgItem(science_dlg,ID_SCIENCE_TOP),text);
  ListBox_ResetContent(GetDlgItem(science_dlg,ID_SCIENCE_LIST));
  for (i=A_FIRST;i<game.num_tech_types;i++)
    {
      if ((get_invention(game.player_ptr,i)==TECH_KNOWN))
	{
	  id=ListBox_AddString(GetDlgItem(science_dlg,ID_SCIENCE_LIST),
			       advances[i].name);
	  ListBox_SetItemData(GetDlgItem(science_dlg,ID_SCIENCE_LIST),
			      id,i);
	}
    }
  my_snprintf(text, sizeof(text), "%d/%d",
              game.player_ptr->research.bulbs_researched,
	      total_bulbs_required(game.player_ptr));
  SetWindowText(GetDlgItem(science_dlg,ID_SCIENCE_PROG),text);
  ComboBox_ResetContent(GetDlgItem(science_dlg,ID_SCIENCE_RESEARCH));

  if (game.player_ptr->research.researching == A_UNSET) {
    id = ComboBox_AddString(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
			    advances[A_NONE].name);
    ComboBox_SetItemData(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
			 id, A_NONE);
    ComboBox_SetCurSel(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
		       id);
  }

  if (!is_future_tech(game.player_ptr->research.researching)) {
    for (i = A_FIRST; i < game.num_tech_types; i++) {
      if (get_invention(game.player_ptr, i) != TECH_REACHABLE) {
	continue;
      }

      id = ComboBox_AddString(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
			      advances[i].name);
      ComboBox_SetItemData(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
			   id, i);
      if (i == game.player_ptr->research.researching) {
	ComboBox_SetCurSel(GetDlgItem(science_dlg, ID_SCIENCE_RESEARCH),
			   id);
      }
    }
  } else {
    /* FIXME future tech */
  }
  ComboBox_ResetContent(GetDlgItem(science_dlg,ID_SCIENCE_GOAL));
    hist=0;
  for(i=A_FIRST; i<game.num_tech_types; i++) {
    if (tech_is_available(game.player_ptr, i)
        && get_invention(game.player_ptr, i) != TECH_KNOWN
        && advances[i].req[0] != A_LAST && advances[i].req[1] != A_LAST
        && (num_unknown_techs_for_goal(game.player_ptr, i) < 11
	    || i == game.player_ptr->ai.tech_goal)) {
      id=ComboBox_AddString(GetDlgItem(science_dlg,ID_SCIENCE_GOAL),
			    advances[i].name);
      ComboBox_SetItemData(GetDlgItem(science_dlg,ID_SCIENCE_GOAL),
			 id,i);
      if (i==game.player_ptr->ai.tech_goal)
	ComboBox_SetCurSel(GetDlgItem(science_dlg,ID_SCIENCE_GOAL),
			   id);
      
    }
  }
  steps = num_unknown_techs_for_goal(game.player_ptr,
                                     game.player_ptr->ai.tech_goal);
  my_snprintf(text, sizeof(text),
	      PL_("(%d step)", "(%d steps)", steps), steps);
  SetWindowText(GetDlgItem(science_dlg,ID_SCIENCE_STEPS),text);
  fcwin_redo_layout(science_dlg);
}

/**************************************************************************

**************************************************************************/
static LONG CALLBACK science_proc(HWND hWnd,
				  UINT message,
				  WPARAM wParam,
				  LPARAM lParam)
{
  int to, steps;
  switch(message)
    {
    case WM_CREATE:
      break;
    case WM_COMMAND:
      switch LOWORD(wParam)
	{
	case ID_SCIENCE_CLOSE:
	  DestroyWindow(science_dlg);
	  science_dlg=NULL;
	  break;
	case ID_SCIENCE_RESEARCH:
	  if (HIWORD(wParam)==CBN_SELCHANGE) {
	    to=ComboBox_GetCurSel(GetDlgItem(hWnd,ID_SCIENCE_RESEARCH));
	    if (to!=LB_ERR) {
	      char text[512];

	      to = ComboBox_GetItemData(GetDlgItem(hWnd,
						   ID_SCIENCE_RESEARCH),
					to);
	      
	      if (IsDlgButtonChecked(hWnd, ID_SCIENCE_HELP)) {
		popup_help_dialog_typed(advances[to].name, HELP_TECH);
		science_dialog_update();
	      } else {
		my_snprintf(text, sizeof(text), "%d/%d",
			    game.player_ptr->research.bulbs_researched,
			    total_bulbs_required(game.player_ptr));
		SetWindowText(GetDlgItem(hWnd,ID_SCIENCE_PROG),text);
		dsend_packet_player_research(&aconnection, to);
	      }
	    }
	  }
	  break;
	case ID_SCIENCE_GOAL:
	  if (HIWORD(wParam)==CBN_SELCHANGE) {
	    to=ComboBox_GetCurSel(GetDlgItem(hWnd,ID_SCIENCE_GOAL));
	    if (to!=LB_ERR) {
	      char text[512];

	      to = ComboBox_GetItemData(GetDlgItem(hWnd, ID_SCIENCE_GOAL),
					to);
	      steps = num_unknown_techs_for_goal(game.player_ptr, to);
	      my_snprintf(text, sizeof(text), 
	                  PL_("(%d step)", "(%d steps)", steps),
			  steps);
	      SetWindowText(GetDlgItem(hWnd,ID_SCIENCE_STEPS), text);
	      dsend_packet_player_tech_goal(&aconnection, to);
	    }
	  }
	  break;
	}
      break;
    case WM_CLOSE:
      DestroyWindow(science_dlg);
      break;
    case WM_DESTROY:
      science_dlg=NULL;
      break;
    case WM_SIZE:
      break;
    case WM_GETMINMAXINFO:
      break;
    default:
      return DefWindowProc(hWnd,message,wParam,lParam);
    }
 return 0; 
}

/**************************************************************************

**************************************************************************/
void
popup_science_dialog(bool make_modal)
{
  if (!science_dlg)
    {
      struct fcwin_box *vbox;
      struct fcwin_box *hbox;
      science_dlg=fcwin_create_layouted_window(science_proc,
					       _("Science Report"),
					       WS_OVERLAPPEDWINDOW,
					       CW_USEDEFAULT,CW_USEDEFAULT,
					       root_window,
					       NULL,
					       JUST_CLEANUP,
					       NULL);
      vbox=fcwin_vbox_new(science_dlg,FALSE);
      fcwin_box_add_static(vbox,"",ID_SCIENCE_TOP,SS_CENTER,FALSE,FALSE,15);
      hbox=fcwin_hbox_new(science_dlg,FALSE);
      fcwin_box_add_groupbox(vbox,_("Researching"),hbox,0,FALSE,FALSE,5);
     
      fcwin_box_add_combo(hbox,10,ID_SCIENCE_RESEARCH,
			  CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL,
			  FALSE,FALSE,5);
      fcwin_box_add_static(hbox,"",ID_SCIENCE_PROG,SS_CENTER,TRUE,TRUE,25);
      fcwin_box_add_checkbox(hbox,_("Help"),ID_SCIENCE_HELP,0,
			     FALSE,FALSE,10);
      
      hbox=fcwin_hbox_new(science_dlg,FALSE);
      fcwin_box_add_groupbox(vbox,_("Goal"),hbox,0,FALSE,FALSE,5);
      fcwin_box_add_combo(hbox,10,ID_SCIENCE_GOAL,
			  CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL,
			  TRUE,TRUE,15);
      fcwin_box_add_static(hbox,"",ID_SCIENCE_STEPS,SS_CENTER,TRUE,TRUE,15);
 
      fcwin_box_add_list(vbox,15,ID_SCIENCE_LIST,
			 LBS_HASSTRINGS | LBS_STANDARD | 
			 WS_VSCROLL | LBS_NOSEL,
			 TRUE,TRUE,5);
      fcwin_box_add_button(vbox,_("Close"),ID_SCIENCE_CLOSE,0,FALSE,FALSE,5);
      fcwin_set_box(science_dlg,vbox);
    }
  science_dialog_update();
  ShowWindow(science_dlg,SW_SHOWNORMAL);
}
/**************************************************************************

**************************************************************************/

void
economy_report_dialog_update(void)
{
   
  HWND lv;
  int tax, total, i, entries_used;
  char   buf0 [64];
  char   buf1 [64];
  char   buf2 [64];
  char   buf3 [64];     
  char *row[4];   
  char economy_total[48];
  struct improvement_entry entries[B_LAST];

  if(is_report_dialogs_frozen()) return;      
  if(!economy_dlg) return;
  lv=GetDlgItem(economy_dlg,ID_TRADEREP_LIST);
  SetWindowText(GetDlgItem(economy_dlg, ID_TRADEREP_TOP),
		get_report_title(_("Economy")));
  ListView_DeleteAllItems(lv);
  row[0] = buf0;
  row[1] = buf1;
  row[2] = buf2;
  row[3] = buf3;

  get_economy_report_data(entries, &entries_used, &total, &tax);

  for (i = 0; i < entries_used; i++) {
    struct improvement_entry *p = &entries[i];

    my_snprintf(buf0, sizeof(buf0), "%s", get_improvement_name(p->type));
    my_snprintf(buf1, sizeof(buf1), "%5d", p->count);
    my_snprintf(buf2, sizeof(buf2), "%5d", p->cost);
    my_snprintf(buf3, sizeof(buf3), "%6d", p->total_cost);

    fcwin_listview_add_row(lv, i, 4, row);

    economy_improvement_type[i] = p->type;
  }
  my_snprintf(economy_total, sizeof(economy_total),
	      _("Income:%6d    Total Costs: %6d"), tax, total);
  SetWindowText(GetDlgItem(economy_dlg,ID_TRADEREP_CASH),economy_total);
  ListView_SetColumnWidth(lv,0,LVSCW_AUTOSIZE);
  for(i=1;i<4;i++) {
    ListView_SetColumnWidth(lv,i,
			    LVSCW_AUTOSIZE_USEHEADER);	
  }
  fcwin_redo_layout(economy_dlg);
}
  
/**************************************************************************

**************************************************************************/
static void economy_dlg_sell(HWND hWnd,int data)
{
  HWND lv = GetDlgItem(hWnd, ID_TRADEREP_LIST);
  char str[1024];
  int row, n = ListView_GetItemCount(lv);

  for (row = 0; row < n; row++) {
    if (ListView_GetItemState(lv,row,LVIS_SELECTED)) {
      sell_all_improvements(economy_improvement_type[row],
			    data == 0, str, sizeof(str));
      ListView_SetItemState(lv,row,0,LVIS_SELECTED);
      popup_notify_dialog(_("Sell-Off:"),_("Results"),str);
    }
  }
}

/**************************************************************************

**************************************************************************/
static LONG CALLBACK economy_proc(HWND hWnd,
				UINT message,
				WPARAM wParam,
				LPARAM lParam) 
{
  
  switch(message)
    {
  
    case WM_SIZE:
    case WM_GETMINMAXINFO:
    case WM_DESTROY:
      break;
    case WM_CLOSE:
      DestroyWindow(hWnd);
      economy_dlg=NULL;
      break;
    case WM_COMMAND:
      switch (LOWORD(wParam))
	{
	case ID_TRADEREP_CLOSE:
	  DestroyWindow(hWnd);
	  economy_dlg=NULL;
	  break;
	case ID_TRADEREP_ALL:
	  economy_dlg_sell(hWnd,1);
	  break;
	case ID_TRADEREP_OBSOLETE:
	  economy_dlg_sell(hWnd,0);
	  break;
	}
      break;
    default:
      return DefWindowProc(hWnd,message,wParam,lParam);
    }
  return 0;
}

/**************************************************************************

**************************************************************************/
void
popup_economy_report_dialog(bool make_modal)
{
  int i;
  struct fcwin_box *hbox;
  struct fcwin_box *vbox;
  if (!economy_dlg) {
    HWND lv;
    LV_COLUMN lvc;
    economy_dlg=fcwin_create_layouted_window(economy_proc,_("Trade Report"),
					     WS_OVERLAPPEDWINDOW,
					     CW_USEDEFAULT,CW_USEDEFAULT,
					     root_window,
					     NULL,
					     JUST_CLEANUP,
					     NULL);
    vbox=fcwin_vbox_new(economy_dlg,FALSE);
    hbox=fcwin_hbox_new(economy_dlg,TRUE);
    fcwin_box_add_static(vbox,"",ID_TRADEREP_TOP,SS_CENTER,
			  FALSE,FALSE,10);
    lv=fcwin_box_add_listview(vbox,10,ID_TRADEREP_LIST,
			      LVS_REPORT,TRUE,TRUE,10);
    fcwin_box_add_static(vbox,"",ID_TRADEREP_CASH,SS_CENTER,FALSE,FALSE,15);
    fcwin_box_add_button(hbox,_("Close"),ID_TRADEREP_CLOSE,
			 0,TRUE,TRUE,20);
    fcwin_box_add_button(hbox,_("Sell Obsolete"),ID_TRADEREP_OBSOLETE,
			 0,TRUE,TRUE,20);
    fcwin_box_add_button(hbox,_("Sell All"),ID_TRADEREP_ALL,
			 0,TRUE,TRUE,20);
    fcwin_box_add_box(vbox,hbox,FALSE,FALSE,10);
    lvc.pszText=_("Building Name");
    lvc.mask=LVCF_TEXT;
    ListView_InsertColumn(lv,0,&lvc);
    lvc.pszText=_("Count");
    lvc.mask=LVCF_TEXT | LVCF_FMT;
    lvc.fmt=LVCFMT_RIGHT;
    ListView_InsertColumn(lv,1,&lvc);
    lvc.pszText=_("Cost");
    ListView_InsertColumn(lv,2,&lvc);
    lvc.pszText=_("U Total");
    ListView_InsertColumn(lv,3,&lvc);
    ListView_SetColumnWidth(lv,0,LVSCW_AUTOSIZE);
    for(i=1;i<4;i++) {
      ListView_SetColumnWidth(lv,i,LVSCW_AUTOSIZE_USEHEADER);	
    }
    fcwin_set_box(economy_dlg,vbox);
  }
  economy_report_dialog_update();
  
  ShowWindow(economy_dlg,SW_SHOWNORMAL);
}
/****************************************************************
...
*****************************************************************/
static void upgrade_callback_yes(HWND w, void * data)
{
  dsend_packet_unit_type_upgrade(&aconnection, (size_t)data);
  destroy_message_dialog(w);
}
 
/****************************************************************
...
*****************************************************************/
static void upgrade_callback_no(HWND w, void * data)
{
  destroy_message_dialog(w);
}
 
/****************************************************************
...
*****************************************************************/              
static LONG CALLBACK activeunits_proc(HWND hWnd,
				      UINT message,
				      WPARAM wParam,
				      LPARAM lParam) 
{
  HWND lv;
  int n,sel,i;
  sel=-1;
  if ((message==WM_COMMAND)||(message==WM_NOTIFY)) {
    /* without totals line */
    lv=GetDlgItem(hWnd,ID_MILITARY_LIST);
    n=ListView_GetItemCount(lv)-1;
    for(i=0;i<n;i++) {
      if (ListView_GetItemState(lv,i,LVIS_SELECTED)) {
	sel=i;
	break;
      }
    } 
  }
  switch(message)
    {
    case WM_CREATE:
    case WM_SIZE:
    case WM_GETMINMAXINFO:
    case WM_DESTROY:
      break;
    case WM_CLOSE:
      DestroyWindow(activeunits_dlg);
      activeunits_dlg=NULL;
      return TRUE;
      break;
      
    case WM_NOTIFY:
      if (sel>=0) {
	if ((unit_type_exists(activeunits_type[sel])) &&
	    (can_upgrade_unittype(game.player_ptr,
				  activeunits_type[sel]) != -1))    
	  EnableWindow(GetDlgItem(activeunits_dlg,ID_MILITARY_UPGRADE),
		       TRUE);
	else
	  EnableWindow(GetDlgItem(activeunits_dlg,ID_MILITARY_UPGRADE),
		       FALSE);
      }
      break;
    case WM_COMMAND:    
      switch(LOWORD(wParam))
	{
	case IDCANCEL:
	  DestroyWindow(activeunits_dlg);
	  activeunits_dlg=NULL;
	  break;
	case ID_MILITARY_REFRESH:
	  activeunits_report_dialog_update();           
	  break;
	case ID_MILITARY_UPGRADE:
	  if (sel>=0)
	    {
	      char buf[512];
	      int ut1,ut2;     
	      ut1 = activeunits_type[sel];
	      if (!(unit_type_exists (ut1)))
		break;
	      ut2=can_upgrade_unittype(game.player_ptr,activeunits_type[sel]);
	      my_snprintf(buf, sizeof(buf),
			  _("Upgrade as many %s to %s as possible for %d gold each?\n"
			    "Treasury contains %d gold."),
			  unit_types[ut1].name, unit_types[ut2].name,
			  unit_upgrade_price(game.player_ptr, ut1, ut2),
			  game.player_ptr->economic.gold);    
	      
	      popup_message_dialog(NULL, 
				   /*"upgradedialog"*/
				   _("Upgrade Obsolete Units"), buf,
				   _("Yes"), upgrade_callback_yes,
				   (void *)(activeunits_type[sel]),
				   _("No"), upgrade_callback_no, 0, 0);
	    }                                                           
	  break;	
	}
      break;
    default:
      return DefWindowProc(hWnd,message,wParam,lParam);
    }
  return 0;
}

/**************************************************************************

**************************************************************************/
#define AU_COL 6
void
activeunits_report_dialog_update(void)
{
  struct repoinfo {
    int active_count;
    int upkeep_shield;
    int upkeep_food;
    /* int upkeep_gold;   FIXME: add gold when gold is implemented --jjm */
    int building_count;
  };                         
  if(activeunits_dlg) {
    HWND lv;
    int    i, k, can;

    struct repoinfo unitarray[U_LAST];
    struct repoinfo unittotals;

    char *row[AU_COL];
    char   buf[AU_COL][64];

    SetWindowText(GetDlgItem(activeunits_dlg, ID_MILITARY_TOP),
		  get_report_title(_("Units")));
    lv=GetDlgItem(activeunits_dlg,ID_MILITARY_LIST);
    ListView_DeleteAllItems(lv);
    for (i = 0; i < ARRAY_SIZE(row); i++) {
      row[i] = buf[i];
    }

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
    unit_type_iterate(i) {
      if ((unitarray[i].active_count > 0) || (unitarray[i].building_count > 0)) 
{
        can = (can_upgrade_unittype(game.player_ptr, i) != -1);
        my_snprintf(buf[0], sizeof(buf[0]), "%s", unit_name(i));
        my_snprintf(buf[1], sizeof(buf[1]), "%c", can ? '*': '-');
        my_snprintf(buf[2], sizeof(buf[2]), "%3d", unitarray[i].building_count);
        my_snprintf(buf[3], sizeof(buf[3]), "%3d", unitarray[i].active_count);
        my_snprintf(buf[4], sizeof(buf[4]), "%3d", unitarray[i].upkeep_shield);
        my_snprintf(buf[5], sizeof(buf[5]), "%3d", unitarray[i].upkeep_food);
	fcwin_listview_add_row(lv,k,AU_COL,row);
        activeunits_type[k]=(unitarray[i].active_count > 0) ? i : U_LAST;
        k++;
        unittotals.active_count += unitarray[i].active_count;
        unittotals.upkeep_shield += unitarray[i].upkeep_shield;
        unittotals.upkeep_food += unitarray[i].upkeep_food;
        unittotals.building_count += unitarray[i].building_count;
      }
    } unit_type_iterate_end;

    my_snprintf(buf[0],sizeof(buf[0]),"%s",_("Totals"));
    buf[1][0]='\0';
    my_snprintf(buf[2],sizeof(buf[2]),"%d",unittotals.building_count);
    my_snprintf(buf[3],sizeof(buf[3]),"%d",unittotals.active_count);
    my_snprintf(buf[4],sizeof(buf[4]),"%d",unittotals.upkeep_shield);
    my_snprintf(buf[5],sizeof(buf[5]),"%d",unittotals.upkeep_food);  
    fcwin_listview_add_row(lv,k,AU_COL,row);
    EnableWindow(GetDlgItem(activeunits_dlg,ID_MILITARY_UPGRADE),FALSE);
    ListView_SetColumnWidth(lv,0,LVSCW_AUTOSIZE);
    for(i=1;i<4;i++) {
      ListView_SetColumnWidth(lv,i,LVSCW_AUTOSIZE_USEHEADER);	
    }
    fcwin_redo_layout(activeunits_dlg);
  }
  
}

/**************************************************************************

**************************************************************************/
void
popup_activeunits_report_dialog(bool make_modal)
{
  if (!activeunits_dlg)
    {
      HWND lv;
      LV_COLUMN lvc;
      char *headers[] = {N_("Unit Type"), N_("U"),
			 N_("In-Prog"), N_("Active"),
			 N_("Shield"), N_("Food"), NULL};
      int i;
      struct fcwin_box *vbox;
      struct fcwin_box *hbox;
      activeunits_dlg=fcwin_create_layouted_window(activeunits_proc,
						   _("Units"),
						   WS_OVERLAPPEDWINDOW,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   root_window,NULL,
						   JUST_CLEANUP,
						   NULL);
      vbox=fcwin_vbox_new(activeunits_dlg,FALSE);
      hbox=fcwin_hbox_new(activeunits_dlg,TRUE);
      fcwin_box_add_static(vbox,get_report_title(_("Units")),
			   ID_MILITARY_TOP,SS_CENTER,
			   FALSE,FALSE,10);
      lv=fcwin_box_add_listview(vbox,10,ID_MILITARY_LIST,
			     LVS_REPORT | LVS_SINGLESEL,
			     TRUE,TRUE,10);
      fcwin_box_add_button(hbox,_("Close"),IDCANCEL,0,
			   TRUE,TRUE,20);
      fcwin_box_add_button(hbox,_("Upgrade"),ID_MILITARY_UPGRADE,0,
			   TRUE,TRUE,20);
      fcwin_box_add_button(hbox,_("Refresh"),ID_MILITARY_REFRESH,0,
			   TRUE,TRUE,20);
      fcwin_box_add_box(vbox,hbox,FALSE,FALSE,10);
      lvc.mask=LVCF_TEXT;
      lvc.pszText=_(headers[0]);
      ListView_InsertColumn(lv,0,&lvc);
      for(i=1;i<AU_COL;i++) {
	lvc.mask=LVCF_TEXT | LVCF_FMT;
	lvc.fmt=LVCFMT_RIGHT;
	lvc.pszText=_(headers[i]);
	ListView_InsertColumn(lv,i,&lvc);
      }
      for(i=0;i<AU_COL;i++) {
	ListView_SetColumnWidth(lv,i,i?LVSCW_AUTOSIZE_USEHEADER:
				LVSCW_AUTOSIZE);
      }
      fcwin_set_box(activeunits_dlg,vbox);
    
    }
  activeunits_report_dialog_update();
  ShowWindow(activeunits_dlg,SW_SHOWNORMAL);
}

/****************************************************************
  Show a dialog with player statistics at endgame.
  TODO: Display all statistics in packet_endgame_report.
*****************************************************************/
void popup_endgame_report_dialog(struct packet_endgame_report *packet)
{
  char buffer[150 * MAX_NUM_PLAYERS];
  int i;
 
  buffer[0] = '\0';
  for (i = 0; i < packet->nscores; i++) {
    cat_snprintf(buffer, sizeof(buffer),
                 PL_("%2d: The %s ruler %s scored %d point\n",
                     "%2d: The %s ruler %s scored %d points\n",
                     packet->score[i]),
                 i + 1,
                 get_nation_name(get_player(packet->id[i])->nation),
                 get_player(packet->id[i])->name,
                 packet->score[i]);
  }
  popup_notify_dialog(_("Final Report:"),
                      _("The Greatest Civilizations in the world."),
                      buffer);
}

/*************************************************************************
  Server options dialog
*************************************************************************/
void popup_settable_options_dialog(void)
{
  /* PORT ME */
}
