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

/********************************************************************** 
  Allocated/allocatable strings (and things?)
  original author: David Pfitzner <dwp@mso.anu.edu.au>

  A common technique is to have some memory dynamically allocated
  (using malloc etc), to avoid compiled-in limits, but only allocate
  enough space as initially needed, and then realloc later if/when
  require more space.  Typically, the realloc is made a bit more than
  immediately necessary, to avoid frequent reallocs if the object
  grows incementally.  Also, don't usually realloc at all if the
  object shrinks.  This is straightforward, but just requires a bit
  of book-keeping to keep track of how much has been allocated etc.
  This module provides some tools to make this a bit easier.

  This is deliberately simple and light-weight.  The user is allowed
  full access to the struct elements rather than use accessor
  functions etc.

  Note one potential hazard: when the size is increased (astr_minsize()),
  realloc (really fc_realloc) is used, which retains any data which
  was there previously, _but_: any external pointers into the allocated
  memory may then become wild.  So you cannot safely use such external
  pointers into the astring data, except strictly between times when
  the astring size may be changed.

***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include "mem.h"

#include "astring.h"

/**********************************************************************
  Initialize the struct.
***********************************************************************/
void astr_init(struct astring *astr)
{
  struct astring zero_astr = ASTRING_INIT;
  assert(astr != NULL);
  *astr = zero_astr;
}

/**********************************************************************
  Check that astr has enough size to hold n, and realloc to a bigger
  size if necessary.  Here n must be big enough to include the trailing
  ascii-null if required.  The requested n is stored in astr->n.
  The actual amount allocated may be larger than n, and is stored
  in astr->n_alloc.
***********************************************************************/
void astr_minsize(struct astring *astr, size_t n)
{
  int n1;
  
  assert(astr != NULL);
  
  astr->n = n;
  if (n <= astr->n_alloc) {
    return;
  }
  
  /* allocated more if this is only a small increase on before: */
  n1 = (3*(astr->n_alloc+10)) / 2;
  astr->n_alloc = (n > n1) ? n : n1;
  astr->str = (char *)fc_realloc(astr->str, astr->n_alloc);
}

/**********************************************************************
  Free the memory associated with astr, and return astr to same
  state as after astr_init.
***********************************************************************/
void astr_free(struct astring *astr)
{
  struct astring zero_astr = ASTRING_INIT;

  assert(astr != NULL);
  
  if (astr->n_alloc > 0) {
    assert(astr->str != NULL);
    free(astr->str);
  }
  *astr = zero_astr;
}
