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

#include <stdlib.h>

/* utility */
#include "log.h"
#include "mem.h"
#include "shared.h"  /* array_shuffle */

#include "genlist.h"

/* A single element of a genlist, storing the pointer to user
   data, and pointers to the next and previous elements:
*/
struct genlist_link {
  struct genlist_link *next, *prev; 
  void *dataptr;
};

/* A genlist, storing the number of elements (for quick retrieval and
   testing for empty lists), and pointers to the first and last elements
   of the list.
*/
struct genlist {
  int nelements;
  struct genlist_link *head_link;
  struct genlist_link *tail_link;
  genlist_free_fn_t free_data_func;
};

static struct genlist_link *
    find_genlist_position(const struct genlist *pgenlist, int pos);

/****************************************************************************
  Default function of type genlist_free_fn_t.
****************************************************************************/
static void genlist_default_free_data_func(void *data)
{
  /* Do nothing. */
}

/****************************************************************************
  Default function of type genlist_copy_fn_t.
****************************************************************************/
static void *genlist_default_copy_data_func(const void *data)
{
  return (void *) data; /* Returns the data. */
}

/****************************************************************************
  Default function of type genlist_comp_fn_t.
****************************************************************************/
static bool genlist_default_comp_data_func(const void *data1,
                                           const void *data2)
{
  return data1 == data2;
}

/****************************************************************************
  Create a new empty genlist.
****************************************************************************/
struct genlist *genlist_new(void)
{
  return genlist_new_full(NULL);
}

/****************************************************************************
  Create a new empty genlist with a free data function.
****************************************************************************/
struct genlist *genlist_new_full(genlist_free_fn_t free_data_func)
{
  struct genlist *pgenlist = fc_calloc(1, sizeof(*pgenlist));

#ifdef ZERO_VARIABLES_FOR_SEARCHING
  pgenlist->nelements = 0;
  pgenlist->head_link = NULL;
  pgenlist->tail_link = NULL;
#endif
  pgenlist->free_data_func = (free_data_func ? free_data_func
                              : genlist_default_free_data_func);

  return pgenlist;
}

/****************************************************************************
  Returns a new genlist that's a copy of the existing one.
****************************************************************************/
struct genlist *genlist_copy(const struct genlist *pgenlist)
{
  return genlist_copy(pgenlist);
}

/****************************************************************************
  Returns a new genlist that's a copy of the existing one.
****************************************************************************/
struct genlist *genlist_copy_full(const struct genlist *pgenlist,
                                  genlist_copy_fn_t copy_data_func,
                                  genlist_free_fn_t free_data_func)
{
  struct genlist *pcopy = genlist_new_full(free_data_func);

  if (pgenlist) {
    struct genlist_link *plink;

    if (NULL == copy_data_func) {
      copy_data_func = genlist_default_copy_data_func;
    }
    for (plink = pgenlist->head_link; plink; plink = plink->next) {
      genlist_append(pcopy, copy_data_func(plink->dataptr));
    }
  }

  return pcopy;
}

/****************************************************************************
  Destroys the genlist.
****************************************************************************/
void genlist_destroy(struct genlist *pgenlist)
{
  fc_assert_ret(NULL != pgenlist);
  genlist_clear(pgenlist);
  free(pgenlist);
}

/****************************************************************************
  Returns the number of elements stored in the genlist.
****************************************************************************/
int genlist_size(const struct genlist *pgenlist)
{
  fc_assert_ret_val(NULL != pgenlist, 0);
  return pgenlist->nelements;
}

/****************************************************************************
  Returns the user-data pointer stored in the genlist at the position
  given by 'idx'.  For idx out of range (including an empty list),
  returns NULL.
  Recall 'idx' can be -1 meaning the last element.
****************************************************************************/
void *genlist_get(const struct genlist *pgenlist, int idx)
{
  struct genlist_link *link = find_genlist_position(pgenlist, idx);

  if (link) {
    return link->dataptr;
  } else {
    return NULL;
  }
}

/****************************************************************************
  Frees all the internal data used by the genlist (but doesn't touch
  the user-data).  At the end the state of the genlist will be the
  same as when genlist_init() is called on a new genlist.
****************************************************************************/
void genlist_clear(struct genlist *pgenlist)
{
  fc_assert_ret(NULL != pgenlist);

  if (0 < pgenlist->nelements) {
    genlist_free_fn_t free_data_func = pgenlist->free_data_func;
    struct genlist_link *plink = pgenlist->head_link, *plink2;

    pgenlist->head_link = NULL;
    pgenlist->tail_link = NULL;

    pgenlist->nelements = 0;

    do {
      plink2 = plink->next;
      free_data_func(plink->dataptr);
      free(plink);
    } while ((plink = plink2) != NULL);
  }
}

/****************************************************************************
  Remove all duplicates of element from every consecutive group of equal
  elements in the list.
****************************************************************************/
void genlist_unique(struct genlist *pgenlist)
{
  genlist_unique_full(pgenlist, NULL);
}

/****************************************************************************
  Remove all duplicates of element from every consecutive group of equal
  elements in the list (equality is assumed from if the compare function
  return TRUE).
****************************************************************************/
void genlist_unique_full(struct genlist *pgenlist,
                         genlist_comp_fn_t comp_data_func)
{
  fc_assert_ret(NULL != pgenlist);

  if (0 < pgenlist->nelements) {
    genlist_free_fn_t free_data_func = pgenlist->free_data_func;
    struct genlist_link *plink = pgenlist->head_link, *plink2;

    if (NULL == comp_data_func) {
      comp_data_func = genlist_default_comp_data_func;
    }
    do {
      plink2 = plink->next;
      if (NULL != plink2 && comp_data_func(plink->dataptr,
                                           plink2->dataptr)) {
        /* Remove this element. */
        plink2->prev = plink->prev;
        if (NULL != plink->prev) {
          plink->prev->next = plink2;
        } else {
          pgenlist->head_link = plink2;
        }
        pgenlist->nelements--;
        free_data_func(plink->dataptr);
        free(plink);
      }
    } while ((plink = plink2) != NULL);
  }
}

/****************************************************************************
  Remove an element of the genlist with the specified user-data pointer
  given by 'punlink'.  If there is no such element, does nothing.
  If there are multiple such elements, removes the first one.  Return
  TRUE if one element has been removed.
****************************************************************************/
bool genlist_remove(struct genlist *pgenlist, void *punlink)
{
  fc_assert_ret_val(NULL != pgenlist, FALSE);

  if (pgenlist->nelements > 0) {
    struct genlist_link *plink = pgenlist->head_link;

    while (plink != NULL && plink->dataptr != punlink) {
      plink = plink->next;
    }

    if (plink) {
      if (pgenlist->head_link == plink) {
        pgenlist->head_link = plink->next;
      } else {
        plink->prev->next = plink->next;
      }

      if (pgenlist->tail_link == plink) {
        pgenlist->tail_link = plink->prev;
      } else {
        plink->next->prev = plink->prev;
      }
      pgenlist->free_data_func(plink->dataptr);
      free(plink);
      pgenlist->nelements--;
      return TRUE;
    }
  }

  return FALSE;
}

/****************************************************************************
  Insert a new element in the list, at position 'pos', with the specified
  user-data pointer 'data'.  Existing elements at >= pos are moved one
  space to the "right".  Recall 'pos' can be -1 meaning add at the
  end of the list.  For an empty list pos has no effect.
  A bad 'pos' value for a non-empty list is treated as -1 (is this
  a good idea?)
****************************************************************************/
void genlist_insert(struct genlist *pgenlist, void *data, int pos)
{
  fc_assert_ret(NULL != pgenlist);

  if (pgenlist->nelements == 0) { /*list is empty, ignore pos */
    
    struct genlist_link *plink = fc_malloc(sizeof(*plink));

    plink->dataptr = data;
    plink->next = NULL;
    plink->prev = NULL;

    pgenlist->head_link = plink;
    pgenlist->tail_link = plink;

  } else {
    struct genlist_link *plink = fc_malloc(sizeof(*plink));
    plink->dataptr = data;

    if (pos == 0) {
      plink->next = pgenlist->head_link;
      plink->prev = NULL;
      pgenlist->head_link->prev = plink;
      pgenlist->head_link = plink;
    } else if (pos <= -1 || pos >= pgenlist->nelements) {
      plink->next = NULL;
      plink->prev = pgenlist->tail_link;
      pgenlist->tail_link->next = plink;
      pgenlist->tail_link = plink;
    } else {
      struct genlist_link *left, *right;     /* left and right of new element */
      right = find_genlist_position(pgenlist, pos);
      left = right->prev;
      plink->next = right;
      plink->prev = left;
      right->prev = plink;
      left->next = plink;
    }
  }

  pgenlist->nelements++;
}

/****************************************************************************
  Insert an item at the start of the list.
****************************************************************************/
void genlist_prepend(struct genlist *pgenlist, void *data)
{
  genlist_insert(pgenlist, data, 0); /* beginning */
}

/****************************************************************************
  Insert an item at the end of the list.
****************************************************************************/
void genlist_append(struct genlist *pgenlist, void *data)
{
  genlist_insert(pgenlist, data, -1); /* end */
}

/****************************************************************************
  Returns a pointer to the genlist link structure at the specified
  position.  Recall 'pos' -1 refers to the last position.
  For pos out of range returns NULL.
  Traverses list either forwards or backwards for best efficiency.
****************************************************************************/
static struct genlist_link *find_genlist_position(const struct genlist *pgenlist,
                                                  int pos)
{
  struct genlist_link *plink;

  if (pos == 0) {
    return pgenlist->head_link;
  } else if (pos == -1) {
    return pgenlist->tail_link;
  } else if (pos < -1 || pos >= pgenlist->nelements) {
    return NULL;
  }

  if (pos < pgenlist->nelements / 2) {  /* fastest to do forward search */
    for (plink = pgenlist->head_link; pos != 0; pos--) {
      plink = plink->next;
    }
  } else {                              /* fastest to do backward search */
    for (plink = pgenlist->tail_link, pos = pgenlist->nelements-pos - 1;
         pos != 0; pos--) {
      plink = plink->prev;
    }
  }
 
  return plink;
}

/****************************************************************************
  Return TRUE iff this element is in the list.

  This is an O(n) operation.  Hence, "search".
****************************************************************************/
bool genlist_search(const struct genlist *pgenlist, const void *data)
{
  const struct genlist_link *plink;

  fc_assert_ret_val(NULL != pgenlist, FALSE);

  for (plink = pgenlist->head_link; plink; plink = plink->next) {
    if (plink->dataptr == data) {
      return TRUE;
    }
  }

  return FALSE;
}

/****************************************************************************
  Sort the elements of a genlist.

  The comparison function should be a function usable by qsort; note
  that the const void * arguments to compar should really be "pointers to
  void*", where the void* being pointed to are the genlist dataptrs.
  That is, there are two levels of indirection.
  To do the sort we first construct an array of pointers corresponding
  the the genlist dataptrs, then sort those and put them back into
  the genlist.
****************************************************************************/
void genlist_sort(struct genlist *pgenlist,
                  int (*compar)(const void *, const void *))
{
  const int n = genlist_size(pgenlist);
  void *sortbuf[n];
  struct genlist_link *myiter;
  int i;

  if (n <= 1) {
    return;
  }

  myiter = find_genlist_position(pgenlist, 0);  
  for (i = 0; i < n; i++, myiter = myiter->next) {
    sortbuf[i] = myiter->dataptr;
  }
  
  qsort(sortbuf, n, sizeof(*sortbuf), compar);
  
  myiter = find_genlist_position(pgenlist, 0);  
  for (i = 0; i < n; i++, myiter = myiter->next) {
    myiter->dataptr = sortbuf[i];
  }
}

/****************************************************************************
  Randomize the elements of a genlist using the Fisher-Yates shuffle.

  see: genlist_sort() and shared.c:array_shuffle()
****************************************************************************/
void genlist_shuffle(struct genlist *pgenlist)
{
  const int n = genlist_size(pgenlist);
  void *sortbuf[n];
  struct genlist_link *myiter;
  int i, shuffle[n];

  if (n <= 1) {
    return;
  }

  myiter = find_genlist_position(pgenlist, 0);
  for (i = 0; i < n; i++, myiter = myiter->next) {
    sortbuf[i] = myiter->dataptr;
    /* also create the shuffle list */
    shuffle[i] = i;
  }

  /* randomize it */
  array_shuffle(shuffle, n);

  /* create the shuffled list */
  myiter = find_genlist_position(pgenlist, 0);
  for (i = 0; i < n; i++, myiter = myiter->next) {
    myiter->dataptr = sortbuf[shuffle[i]];
  }
}

/****************************************************************************
  Returns the head link of the genlist.
****************************************************************************/
const struct genlist_link *genlist_head(const struct genlist *pgenlist)
{
  return pgenlist ? pgenlist->head_link : NULL;
}

/****************************************************************************
  Returns the tail link of the genlist.
****************************************************************************/
const struct genlist_link *genlist_tail(const struct genlist *pgenlist)
{
  return pgenlist ? pgenlist->tail_link : NULL;
}

/****************************************************************************
  Returns the pointer of this link.
****************************************************************************/
void *genlist_link_data(const struct genlist_link *plink)
{
  return plink ? plink->dataptr : NULL;
}

/****************************************************************************
  Returns the previous link.
****************************************************************************/
const struct genlist_link *
genlist_link_prev(const struct genlist_link *plink)
{
  return plink ? plink->prev : NULL;
}

/****************************************************************************
  Returns the next link.
****************************************************************************/
const struct genlist_link *
genlist_link_next(const struct genlist_link *plink)
{
  return plink ? plink->next : NULL;
}
