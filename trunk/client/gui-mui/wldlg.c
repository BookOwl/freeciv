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

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include "city.h"
#include "fcintl.h"
#include "game.h"
#include "helpdlg.h"
#include "mem.h"
#include "packets.h"
#include "worklist.h"

#include "clinet.h"
#include "wldlg.h"

/* MUI Stuff */
#include "muistuff.h"
#include "worklistclass.h"

IMPORT Object *app;

#define WORKLIST_ADVANCED_TARGETS  1
#define WORKLIST_CURRENT_TARGETS   0
#define WORKLIST_LIST_WIDTH   220
#define WORKLIST_LIST_HEIGHT  150

/*
  The Worklist Report dialog shows all the global worklists that the
  player has defined.  There can be at most MAX_NUM_WORKLISTS global
  worklists.
  */
struct worklist_report_dialog {
  Object *wnd;
  Object *listview;
  Object *name_string;
  Object *edit_button;
  struct Hook listview_display_hook;
  struct player *pplr;
  int wl_idx;
};

static struct worklist_report_dialog *report_dialog;

static void commit_player_worklist(struct worklist *pwl, void *data);
static void populate_worklist_report_list(struct worklist_report_dialog *pdialog);

/****************************************************************
  Display function for the worklist listview
*****************************************************************/
HOOKPROTONH(worklist_report_display, void, char **array, APTR msg)
{
  LONG num = (LONG) msg;

  if (num)
  {
    num--;
    if (report_dialog)
    {
      *array = report_dialog->pplr->worklists[num].name;
    }  else *array = NULL;
  }
}

/****************************************************************
  Create a new worklist
*****************************************************************/
static void worklist_report_insert( struct worklist_report_dialog **ppdialog)
{
  struct worklist_report_dialog *pdialog = *ppdialog;
  struct packet_player_request packet;
  int j;

  /* Find the next free worklist for this player */
  for (j = 0; j < MAX_NUM_WORKLISTS; j++)
    if (!pdialog->pplr->worklists[j].is_valid)
      break;

  /* No more worklist slots free.  (!!!Maybe we should tell the user?) */
  if (j == MAX_NUM_WORKLISTS)
    return;

  /* Validate this slot. */
  init_worklist(&packet.worklist);
  packet.worklist.is_valid = 1;
  strcpy(packet.worklist.name, _("empty worklist"));
  packet.wl_idx = j;

  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_WORKLIST);
}

/****************************************************************
  Remove the current worklist.  This request is made by sliding
  up all lower worklists to fill in the slot that's being deleted.
*****************************************************************/
static void worklist_report_delete( struct worklist_report_dialog **ppdialog)
{
  struct worklist_report_dialog *pdialog = *ppdialog;
  struct packet_player_request packet;
  int i, j, selection;

  DoMethod(pdialog->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &selection);

  if (! selection)
    return;
  selection--;

  /* Look for the last free worklist */
  for (i = 0; i < MAX_NUM_WORKLISTS; i++)
    if (!pdialog->pplr->worklists[i].is_valid)
      break;

  for (j = selection; j < i-1; j++) {
    copy_worklist(&packet.worklist, &pdialog->pplr->worklists[j+1]);
    packet.wl_idx = j;

    send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_WORKLIST);
  }

  /* The last worklist in the set is no longer valid -- it's been slid up
     one slot. */
  packet.worklist.name[0] = '\0';
  packet.worklist.is_valid = 0;
  packet.wl_idx = i-1;
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_WORKLIST);
}

/****************************************************************
  Edit the current worklist
*****************************************************************/
static void worklist_report_edit( struct worklist_report_dialog **ppdialog)
{
  struct worklist_report_dialog *pdialog = *ppdialog;
  int selection;

  DoMethod(pdialog->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &selection);

  if (!selection)
    return;
  selection--;

  pdialog->wl_idx = selection;

  popup_worklist(&pdialog->pplr->worklists[selection], NULL, 
		 pdialog, commit_player_worklist, NULL);
}

/****************************************************************
 ...
*****************************************************************/
static void worklist_report_list( struct worklist_report_dialog **ppdialog)
{
  struct worklist_report_dialog *pdialog = *ppdialog;
  int selection;

  DoMethod(pdialog->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &selection);

  if (!selection)
  {
/*
    set(pdialog->edit_button, MUIA_Disabled, TRUE);
    set(pdialog->name_string, MUIA_Disabled, TRUE);
*/
    return;
  }
  selection--;

/*  set(pdialog->edit_button, MUIA_Disabled, FALSE);*/

  SetAttrs(pdialog->name_string,
     MUIA_String_Contents, pdialog->pplr->worklists[selection].name,
     /*MUIA_Disabled, FALSE,*/ TAG_DONE);
}

/****************************************************************
 Rename the worklist
*****************************************************************/
static void worklist_report_rename( struct worklist_report_dialog **ppdialog)
{
  struct worklist_report_dialog *pdialog = *ppdialog;
  struct packet_player_request packet;
  int selection;

  DoMethod(pdialog->listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &selection);

  if (!selection)
    return;
  selection--;


  packet.wl_idx = selection;
  copy_worklist(&packet.worklist, 
		&pdialog->pplr->worklists[selection]);
  strncpy(packet.worklist.name, getstring(pdialog->name_string), MAX_LEN_NAME);
  packet.worklist.name[MAX_LEN_NAME-1] = '\0';
  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_WORKLIST);
}

/****************************************************************
  Bring up the worklist report.
*****************************************************************/
void popup_worklists_dialog(struct player *pplr)
{
  struct worklist_report_dialog *pdialog;
  Object *close_button,*insert_button,*delete_button, *edit_button;
  if (report_dialog)
  {
    set(report_dialog->wnd, MUIA_Window_Open, TRUE);
    return;
  }

  pdialog = malloc(sizeof(struct worklist_report_dialog));
  if (!pdialog) return;

  pdialog->listview_display_hook.h_Entry = (HOOKFUNC)worklist_report_display;

  pdialog->pplr = pplr;
  pdialog->wnd = WindowObject,
    MUIA_Window_Title, _("Freeciv - Edit Worklists"),
    WindowContents, VGroup,
	Child, pdialog->listview = NListviewObject,
	    MUIA_CycleChain, 1,
	    MUIA_NListview_NList, NListObject,
	        MUIA_NList_Input, TRUE,
		MUIA_NList_DisplayHook, &pdialog->listview_display_hook,
		MUIA_NList_Title, _("Available Worklists"),
		End,
	    End,
	Child, HGroup,
	    Child, pdialog->name_string = MakeString(NULL,MAX_LEN_NAME-1),
	    Child, edit_button = MakeButton(_("_Edit")),
	    Child, insert_button = MakeButton(_("_New")),
	    Child, delete_button = MakeButton(_("_Remove")),
	    End,
	Child, close_button = MakeButton(_("_Ok")),
	End,
    End;

  if (pdialog->wnd)
  {
    SetAttrs(pdialog->name_string, MUIA_String_AttachedList, pdialog->listview,
    				   MUIA_Weight, 500, TAG_DONE);

    DoMethod(app,OM_ADDMEMBER,pdialog->wnd);
    DoMethod(pdialog->wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, pdialog->wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(close_button, MUIM_Notify, MUIA_Pressed, FALSE, pdialog->wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(insert_button, MUIM_Notify, MUIA_Pressed, FALSE, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_insert, pdialog);
    DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_delete, pdialog);
    DoMethod(edit_button, MUIM_Notify, MUIA_Pressed, FALSE, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_edit, pdialog);
    DoMethod(pdialog->listview, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_list, pdialog);
    DoMethod(pdialog->listview, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_edit, pdialog);
    DoMethod(pdialog->name_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, pdialog->wnd, 4, MUIM_CallHook, &civstandard_hook, worklist_report_rename, pdialog);

    report_dialog = pdialog;
    populate_worklist_report_list(pdialog);
    set(pdialog->wnd, MUIA_Window_Open, TRUE);
  }	else
  {
    free(pdialog);
  }
}

/*************************************************************************
  Bring up a dialog box to edit the given worklist.  If pcity is
  non-NULL, then use pcity to determine the set of units and improvements
  that can be made.  Otherwise, just list everything that technology
  will allow.
*************************************************************************/
Object *popup_worklist(struct worklist *pwl,struct city *pcity,
		       void *parent_data, WorklistOkCallback ok_cb,
		       WorklistCancelCallback cancel_cb)
{
  Object *workwnd = WorklistObject,
  	MUIA_Worklist_Worklist, pwl,
  	MUIA_Worklist_City, pcity,
  	MUIA_Worklist_PatentData, parent_data,
  	MUIA_Worklist_OkCallBack, ok_cb,
  	MUIA_Worklist_CancelCallBack, cancel_cb,
  	End;

  if (workwnd)
  {
    DoMethod(app, OM_ADDMEMBER, workwnd);
    set(workwnd, MUIA_Window_Open, TRUE);
  }
  return workwnd;
}

/****************************************************************

*****************************************************************/
void update_worklist_report_dialog(void)
{
  /* If the worklist report is open, force its contents to be 
     update. */
  if (report_dialog)
  {
    populate_worklist_report_list(report_dialog);
  }
}

/****************************************************************
  Commit the changes to the worklist for this player.
*****************************************************************/
static void commit_player_worklist(struct worklist *pwl, void *data)
{
  struct worklist_report_dialog *pdialog;
  struct packet_player_request packet;

  pdialog = (struct worklist_report_dialog *)data;

  copy_worklist(&packet.worklist, pwl);
  packet.wl_idx = pdialog->wl_idx;

  send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_WORKLIST);
}

/****************************************************************
  Fill in the worklist arrays in the pdialog.
*****************************************************************/
static void populate_worklist_report_list(struct worklist_report_dialog *pdialog)
{
  int i;
  int last_active = xget(pdialog->listview, MUIA_NList_Active);

  set(pdialog->listview, MUIA_NList_Quiet, TRUE);

  DoMethod(pdialog->listview, MUIM_NList_Clear);

  for (i = 0; i < MAX_NUM_WORKLISTS; i++)
  {
    if (pdialog->pplr->worklists[i].is_valid)
    {
      DoMethod(pdialog->listview, MUIM_NList_InsertSingle, i+1, MUIV_NList_Insert_Bottom);
    }
  }
  SetAttrs(pdialog->listview,
      MUIA_NList_Active, last_active,
      MUIA_NList_Quiet, FALSE,
      TAG_DONE);
}

