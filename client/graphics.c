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
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#include <shared.h>
#include <log.h>
#include <unit.h>
#include <game.h>
#include <graphics.h>
#include <colors.h>
#include <climisc.h>

extern int display_depth;
extern Widget map_canvas;
extern Display *display;
extern XColor colors[MAX_COLORS];
extern GC fill_bg_gc;
extern GC civ_gc, font_gc;
extern Colormap cmap;
extern Widget toplevel;
extern Window root_window;
extern XFontStruct *main_font_struct;
extern int use_solid_color_behind_units;

#define FLAG_TILES       14*20

struct Sprite **tile_sprites;
struct Sprite *intro_gfx_sprite;
struct Sprite *radar_gfx_sprite;
int UNIT_TILES;
int NORMAL_TILE_WIDTH;
int NORMAL_TILE_HEIGHT;

/* jjm@codewell.com 30dec1998a
   Moved road and rail tiles to roads.xpm; added tiles for diagonals.
*/
int ROAD_TILES;
int RAIL_TILES;


/***************************************************************************
...
***************************************************************************/
struct Sprite *get_tile_sprite(int tileno)
{
  return tile_sprites[tileno];
}


/***************************************************************************
...
***************************************************************************/
void load_intro_gfx(void)
{
  int w;
  char s[64];

  intro_gfx_sprite=load_xpmfile(datafilename("intro.xpm"));
  radar_gfx_sprite=load_xpmfile(datafilename("radar.xpm"));

  w=XTextWidth(main_font_struct, WORD_VERSION, strlen(WORD_VERSION));
	
  XDrawString(display, radar_gfx_sprite->pixmap, font_gc, 
	      160/2-w/2, 40, 
	      WORD_VERSION, strlen(WORD_VERSION));
  
  sprintf(s, "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
  w=XTextWidth(main_font_struct, s, strlen(s));
  XDrawString(display, radar_gfx_sprite->pixmap, font_gc, 
	      160/2-w/2, 60, s, strlen(s));
}


/***************************************************************************
...
***************************************************************************/
void load_tile_gfx(void)
{
  int i, x, y, ntiles, a;
  struct Sprite *big_sprite, *small_sprite, *unit_sprite, *treaty_sprite;
  struct Sprite *roads_sprite;
  int row;

  big_sprite  = load_xpmfile(datafilename("tiles.xpm"));
  unit_sprite = load_xpmfile(datafilename("units.xpm"));
  small_sprite= load_xpmfile(datafilename("small.xpm"));
  treaty_sprite=load_xpmfile(datafilename("treaty.xpm"));
  roads_sprite=load_xpmfile(datafilename("roads.xpm"));

  ntiles= (20*19) + (20*3) + (31*1) + 3 + (16*4);

  if(!(tile_sprites=malloc(ntiles*sizeof(struct Sprite *)))) {
    flog(LOG_FATAL, "couldn't malloc tile_sprites array");
    exit(1);
  }

  NORMAL_TILE_WIDTH=big_sprite->width/20;
  NORMAL_TILE_HEIGHT=big_sprite->height/19;

  i=0;
  for(y=0, a=0; a<19 && y<big_sprite->height; a++, y+=NORMAL_TILE_HEIGHT)
    for(x=0; x<big_sprite->width; x+=NORMAL_TILE_WIDTH) {
      GC plane_gc;
      Pixmap mypixmap, mask;
      
      mypixmap=XCreatePixmap(display, root_window, NORMAL_TILE_WIDTH, 
			     NORMAL_TILE_HEIGHT, display_depth);
      XCopyArea(display, big_sprite->pixmap, mypixmap, civ_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      mask=XCreatePixmap(display, root_window, 
			 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 1);

      plane_gc = XCreateGC(display, mask, 0, NULL);

      XCopyArea(display, big_sprite->mask, mask, plane_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      tile_sprites[i++]=ctor_sprite_mask(mypixmap, mask, NORMAL_TILE_WIDTH,
                                         NORMAL_TILE_HEIGHT);

      XFreeGC(display, plane_gc);
    }

  for(x=0, y=0; x<small_sprite->width; x+=SMALL_TILE_WIDTH) {
    Pixmap mypixmap;
    
    mypixmap=XCreatePixmap(display, root_window, 
			   SMALL_TILE_WIDTH, 
			   SMALL_TILE_HEIGHT, 
			   display_depth);
    XCopyArea(display, small_sprite->pixmap, mypixmap, civ_gc, 
	      x, y, SMALL_TILE_WIDTH, SMALL_TILE_HEIGHT, 0 ,0);
    
    tile_sprites[i++]=ctor_sprite(mypixmap, 
				  SMALL_TILE_WIDTH, SMALL_TILE_HEIGHT);
  }

  {
    Pixmap mypixmap;

    mypixmap=XCreatePixmap(display, root_window, 30,30, display_depth);
    XCopyArea(display, treaty_sprite->pixmap, mypixmap, civ_gc, 0,0, 30,30, 0,0);
    tile_sprites[i++]=ctor_sprite(mypixmap, 30,30);
    mypixmap=XCreatePixmap(display, root_window, 30,30, display_depth);
    XCopyArea(display, treaty_sprite->pixmap, mypixmap, civ_gc, 30,0, 30,30, 0,0);
    tile_sprites[i++]=ctor_sprite(mypixmap, 30,30);
/*  mypixmap=XCreatePixmap(display, root_window, 300,30, display_depth);
    XCopyArea(display, treaty_sprite->pixmap, mypixmap, civ_gc, 60,0, 300,30, 0,0);
    tile_sprites[i++]=ctor_sprite(mypixmap, 300,30); */
  }

  UNIT_TILES = i;
  for(y=0; y<unit_sprite->height; y+=NORMAL_TILE_HEIGHT)
    for(x=0; x<unit_sprite->width; x+=NORMAL_TILE_WIDTH) {
      GC plane_gc;
      Pixmap mypixmap, mask;
      
      mypixmap=XCreatePixmap(display, root_window,
			     NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 
			     display_depth);
      XCopyArea(display, unit_sprite->pixmap, mypixmap, civ_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      mask=XCreatePixmap(display, root_window,
			 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 1);

      plane_gc = XCreateGC(display, mask, 0, NULL);

      XCopyArea(display, unit_sprite->mask, mask, plane_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      tile_sprites[i++]=ctor_sprite_mask(mypixmap, mask,
					 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);

      XFreeGC(display, plane_gc);
    }

  /* jjm@codewell.com 30dec1998a */
  for(y=0, row=0; y<roads_sprite->height; y+=NORMAL_TILE_HEIGHT, row++) {
    if (row==0) ROAD_TILES = i;
    if (row==2) RAIL_TILES = i;
    for(x=0; x<roads_sprite->width; x+=NORMAL_TILE_WIDTH) {
      GC plane_gc;
      Pixmap mypixmap, mask;
      
      mypixmap=XCreatePixmap(display, root_window,
			     NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 
			     display_depth);
      XCopyArea(display, roads_sprite->pixmap, mypixmap, civ_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      mask=XCreatePixmap(display, root_window,
			 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 1);

      plane_gc = XCreateGC(display, mask, 0, NULL);

      XCopyArea(display, roads_sprite->mask, mask, plane_gc, 
		x, y, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, 0 ,0);

      tile_sprites[i++]=ctor_sprite_mask(mypixmap, mask,
					 NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);

      XFreeGC(display, plane_gc);
    }
  }

  free_sprite(unit_sprite);
  free_sprite(big_sprite);
  free_sprite(small_sprite);
  free_sprite(treaty_sprite);
  free_sprite(roads_sprite); /* jjm@codewell.com 30dec1998a */
}


/***************************************************************************
...
***************************************************************************/
struct Sprite *ctor_sprite(Pixmap mypixmap, int width, int height)
{
  struct Sprite *mysprite=malloc(sizeof(struct Sprite));
  mysprite->pixmap=mypixmap;
  mysprite->width=width;
  mysprite->height=height;
  mysprite->has_mask=0;
  return mysprite;
}

/***************************************************************************
...
***************************************************************************/
struct Sprite *ctor_sprite_mask(Pixmap mypixmap, Pixmap mask, 
				int width, int height)
{
  struct Sprite *mysprite=malloc(sizeof(struct Sprite));
  mysprite->pixmap=mypixmap;
  mysprite->mask=mask;

  mysprite->width=width;
  mysprite->height=height;
  mysprite->has_mask=1;
  return mysprite;
}




/***************************************************************************
...
***************************************************************************/
void dtor_sprite(struct Sprite *mysprite)
{
  XFreePixmap(display, mysprite->pixmap);
  if(mysprite->has_mask)
    XFreePixmap(display, mysprite->mask);
  free_colors(mysprite->pcolorarray, mysprite->ncols);
  free(mysprite->pcolorarray);
  free(mysprite);

}



/***************************************************************************
...
***************************************************************************/
struct Sprite *load_xpmfile(char *filename)
{
  struct Sprite *mysprite;
  Pixmap mypixmap, mask_bitmap;
  int err;
  XpmAttributes attributes;
  
  attributes.extensions = NULL;
  attributes.valuemask = XpmCloseness|XpmColormap;
  attributes.colormap = cmap;
  attributes.closeness = 40000;

again:
  
  if((err=XpmReadFileToPixmap(display, root_window, filename, &mypixmap, 
			      &mask_bitmap, &attributes))!=XpmSuccess) {
    if(err==XpmColorError || err==XpmColorFailed) {
      color_error();
      goto again;
    }
    
    flog(LOG_FATAL, "Failed reading XPM file: %s", filename);
    flog(LOG_FATAL, "The environment variable FREECIV_DATADIR is '%s'",
	getenv("FREECIV_DATADIR") ? getenv("FREECIV_DATADIR") : "");
    flog(LOG_FATAL, "Check if you got read permissions to the file");
    exit(1);
  }

  if(!(mysprite=(struct Sprite *)malloc(sizeof(struct Sprite)))) {
    flog(LOG_FATAL, "failed mallocing sprite struct for %s", filename);
    exit(1);
  }
  
  mysprite->pixmap=mypixmap;
  mysprite->mask=mask_bitmap;
  mysprite->has_mask=(mask_bitmap!=0);
  mysprite->width=attributes.width;
  mysprite->height=attributes.height;

  return mysprite;
}

/***************************************************************************
   Deletes a sprite.  These things can use a lot of memory.
***************************************************************************/
void free_sprite(struct Sprite *s)
{
  if(s->pixmap) XFreePixmap(display,s->pixmap);
  if(s->has_mask) XFreePixmap(display,s->mask);
  free(s);
}

/***************************************************************************
...
***************************************************************************/
Pixmap create_overlay_unit(int i)
{
  Pixmap pm;
  
  pm=XCreatePixmap(display, root_window, NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, display_depth);
  if(use_solid_color_behind_units)  {
    XSetForeground(display, fill_bg_gc, colors_standard[COLOR_STD_RACE0+game.player_ptr->race]);
    XFillRectangle(display, pm, fill_bg_gc, 0,0, NORMAL_TILE_WIDTH,NORMAL_TILE_HEIGHT);
  } else {
	struct Sprite *flag=get_tile_sprite(game.player_ptr->race + FLAG_TILES);
	XCopyArea(display, flag->pixmap, pm, civ_gc, 0,0,
	          flag->width,flag->height, 0,0);
  };

  if(i<U_LAST) {
    struct Sprite *s=get_tile_sprite(get_unit_type(i)->graphics+UNIT_TILES);

    XSetClipOrigin(display,civ_gc,0,0);
    XSetClipMask(display,civ_gc,s->mask);
    XCopyArea(display, s->pixmap, pm, civ_gc,
	      0,0, s->width,s->height, 0,0 );
    XSetClipMask(display,civ_gc,None);
  }
  return(pm);
}

