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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <support.h>

#include <intuition/sghooks.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "colortextclass.h"
#include "muistuff.h"

struct ColorText_Data
{
  LONG fgpen;
  LONG bgpen;

  LONG fgcol;
  LONG bgcol;
  LONG ownbg;

  STRPTR contents;

  LONG innerleft,innertop,innerright,innerbottom;
};


STATIC ULONG ColorText_New(struct IClass *cl, Object * o, struct opSet *msg)
{
  if ((o = (Object *) DoSuperNew(cl, o,
  				 MUIA_InnerLeft,0,
  				 MUIA_InnerTop,0,
  				 MUIA_InnerRight,0,
  				 MUIA_InnerBottom,0,
				 TAG_MORE, msg->ops_AttrList)))
  {
    struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
    struct TagItem *tl = msg->ops_AttrList;
    struct TagItem *ti;

    data->fgpen = -1;
    data->bgpen = -1;

    while ((ti = NextTagItem(&tl)))
    {
      switch (ti->ti_Tag)
      {
	case  MUIA_ColorText_Contents:
	      data->contents = StrCopy((STRPTR)ti->ti_Data);
      	      break;

        case  MUIA_ColorText_Background:
              data->bgcol = ti->ti_Data;
              data->ownbg = TRUE;
	      break;
      }
    }

    if (data->ownbg) set(o,MUIA_FillArea,FALSE);
  }
  return (ULONG) o;
}

STATIC ULONG ColorText_Dispose(struct IClass * cl, Object * o, Msg msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  if (data->contents) FreeVec(data->contents);
  return DoSuperMethodA(cl, o, msg);
}

/*
STATIC ULONG ColorText_Set(struct IClass *cl, Object * o, struct opSet *msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  struct TagItem *tl = msg->ops_AttrList;
  struct TagItem *ti;

  while ((ti = NextTagItem(&tl)))
  {
    switch (ti->ti_Tag)
    {
      case MUIA_ColorText_Contents:
      	   break;
    }
  }
  return DoSuperMethodA(cl, o, (Msg) msg);
}
*/

STATIC ULONG ColorText_Get(struct IClass * cl, Object * o, struct opGet * msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  switch (msg->opg_AttrID)
  {
    case  MUIA_ColorText_Contents:
          *msg->opg_Storage = (ULONG)data->contents;
          break;

    default:
          return DoSuperMethodA(cl, o, (Msg) msg);
  }
  return 1;
}


STATIC ULONG ColorText_Setup(struct IClass * cl, Object * o, Msg msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;
/*  Object *temp;*/

  if (!DoSuperMethodA(cl, o, msg))
    return FALSE;

  cm = _screen(o)->ViewPort.ColorMap;

  if (data->ownbg)
  {
    data->bgcol = ObtainBestPenA(cm,
                                 MAKECOLOR32(((data->bgcol >> 16)&0xff)),
                                 MAKECOLOR32(((data->bgcol >> 8)&0xff)),
                                 MAKECOLOR32((data->bgcol & 0xff)), NULL);
  } else data->bgcol = -1;

  data->innerleft=2;
  data->innertop=1;
  data->innerright=2;
  data->innerbottom=1;

/*
  temp = TextObject,End;

  if (temp)
  {
    data->innerleft = xget(temp, MUIA_InnerLeft);
    data->innertop = xget(temp, MUIA_InnerTop);
    data->innerright = xget(temp, MUIA_InnerRight);
    data->innerbottom = xget(temp, MUIA_InnerBottom);
    MUI_DisposeObject(temp);
  }*/

  return TRUE;
}

STATIC ULONG ColorText_Cleanup(struct IClass * cl, Object * o, Msg msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  struct ColorMap *cm;

  cm = _screen(o)->ViewPort.ColorMap;

  if (data->bgcol != -1) ReleasePen(cm, data->bgcol);

  return DoSuperMethodA(cl, o, msg);
}

STATIC ULONG ColorText_AskMinMax(struct IClass * cl, Object * o, struct MUIP_AskMinMax * msg)
{
  LONG width,height;
  struct RastPort rp;
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  DoSuperMethodA(cl, o, (Msg) msg);

  InitRastPort(&rp);
  SetFont(&rp,_font(o));

  width = TextLength(&rp,data->contents, strlen(data->contents)) + data->innerleft + data->innerright;
  height = _font(o)->tf_YSize + data->innertop + data->innerbottom;;

  msg->MinMaxInfo->MinWidth += width;
  msg->MinMaxInfo->DefWidth += width;
  msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;

  msg->MinMaxInfo->MinHeight += height;
  msg->MinMaxInfo->DefHeight += height;
  msg->MinMaxInfo->MaxHeight += height;
  return 0;
}

STATIC ULONG ColorText_Draw(struct IClass * cl, Object * o, struct MUIP_Draw * msg)
{
  struct ColorText_Data *data = (struct ColorText_Data *) INST_DATA(cl, o);
  DoSuperMethodA(cl,o,(Msg)msg);

  if (data->bgcol != -1)
  {
    SetAPen(_rp(o), data->bgcol);
    RectFill(_rp(o),_mleft(o), _mtop(o), _mright(o), _mbottom(o));
  }

  SetAPen(_rp(o), _pens(o)[MPEN_TEXT]);
  Move(_rp(o),_mleft(o)+data->innerleft,_mtop(o)+_font(o)->tf_Baseline+data->innertop);
  Text(_rp(o),data->contents, strlen(data->contents));
  return 0;
}

DISPATCHERPROTO(ColorText_Dispatcher)
{
  switch (msg->MethodID)
  {
  case OM_NEW:
    return ColorText_New(cl, obj, (struct opSet *) msg);
  case OM_DISPOSE:
    return ColorText_Dispose(cl, obj, msg);
  case OM_GET:
    return ColorText_Get(cl, obj, (struct opGet *) msg);
/*
  case OM_SET:
    return ColorText_Set(cl, obj, (struct opSet *) msg);
*/
  case MUIM_Setup:
    return ColorText_Setup(cl, obj, msg);
  case MUIM_Cleanup:
    return ColorText_Cleanup(cl, obj, msg);
  case MUIM_AskMinMax:
    return ColorText_AskMinMax(cl, obj, (struct MUIP_AskMinMax *) msg);
  case MUIM_Draw:
    return ColorText_Draw(cl, obj, (struct MUIP_Draw *) msg);

  }
  return (DoSuperMethodA(cl, obj, msg));
}

struct MUI_CustomClass *CL_ColorText;

BOOL create_colortext_class(void)
{
  if ((CL_ColorText = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct ColorText_Data), ColorText_Dispatcher)))
      return TRUE;
  return FALSE;
}

VOID delete_colortext_class(void)
{
  if (CL_ColorText)
    MUI_DeleteCustomClass(CL_ColorText);
}
