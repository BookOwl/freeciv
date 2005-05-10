/**********************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

/**************************************************************************
  Signals implementation.
  
  New signal types can be declared with script_signal_create. Each
  signal should have a unique name string.
  All signal declarations are in signals_create, for convenience.
  
  A signal may have any number of Lua callback functions connected to it
  at any given time.

  A signal emission invokes all associated callbacks in the order they were
  connected:

  * A callback can stop the current signal emission, preventing the callbacks
    connected after it from being invoked.

  * A callback can detach itself from its associated signal.
  
  Lua callbacks functions are able to do these via their return values.
  
  All Lua callback functions can return upto two values. Example:
    return false, true
    
  If the first value is 'true' the current signal emission will be stopped.
  If the second value is 'true' the callback detaches itself from the signal.
**************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hash.h"
#include "log.h"
#include "registry.h"

#include "script.h"

#include "script_signal.h"

struct signal;
struct signal_callback;

/* get 'struct signal_callback_list' and related functions: */
#define SPECLIST_TAG signal_callback
#define SPECLIST_TYPE struct signal_callback
#include "speclist.h"

#define signal_callback_list_iterate(list, pcallback) \
    TYPED_LIST_ITERATE(struct signal_callback, list, pcallback)
#define signal_callback_list_iterate_end  LIST_ITERATE_END

/**************************************************************************
  Signal datastructure.
**************************************************************************/
struct signal {
  int nargs;				  /* number of arguments to pass */
  struct signal_callback_list *callbacks; /* connected callbacks */
};

/**************************************************************************
  Signal callback datastructure.
**************************************************************************/
struct signal_callback {
  char *name;				  /* callback function name */
};

/**************************************************************************
  Signal and callback state datastructure.
**************************************************************************/
static struct hash_table *signals;


/**************************************************************************
  Prototypes.
**************************************************************************/
static void signals_create(void);


/**************************************************************************
  Connects a callback function to a certain signal (internal).
**************************************************************************/
static struct signal_callback *
internal_signal_callback_append(struct signal_callback_list *list, 
				const char *callback_name)
{
  struct signal_callback *callback;

  callback = fc_malloc(sizeof(*callback));
  callback->name = mystrdup(callback_name);

  signal_callback_list_append(list, callback);
  return callback;
}

/**************************************************************************
  Disconnects a callback function from a certain signal (internal).
**************************************************************************/
static void internal_signal_callback_remove(struct signal_callback_list *list,
					    struct signal_callback *callback)
{
  signal_callback_list_unlink(list, callback);

  free(callback->name);
  free(callback);
}

/**************************************************************************
  Create a new signal type (internal).
**************************************************************************/
static void internal_signal_create(const char *signal_name,
				   int nargs, enum api_types args[])
{
  if (hash_key_exists(signals, signal_name)) {
    freelog(LOG_ERROR, "Signal \"%s\" was already created.", signal_name);
  } else {
    char *name;
    struct signal *signal;

    name = mystrdup(signal_name);

    signal = fc_malloc(sizeof(*signal));
    signal->nargs = nargs;
    signal->callbacks = signal_callback_list_new();

    hash_insert(signals, name, signal);
  }
}

/**************************************************************************
  Free a signal type (internal).
**************************************************************************/
static void internal_signal_free(const char *signal_name)
{
  const void *pname;
  const void *psignal;

  if (hash_lookup(signals, signal_name, &pname, &psignal)) {
    char *name = (char *)pname;
    struct signal *signal = (struct signal *)psignal;

    signal_callback_list_iterate(signal->callbacks, pcallback) {
      internal_signal_callback_remove(signal->callbacks, pcallback);
    } signal_callback_list_iterate_end;

    signal_callback_list_unlink_all(signal->callbacks);
    signal_callback_list_free(signal->callbacks);
    free(signal);

    free(name);
  } else {
    freelog(LOG_ERROR, "Signal \"%s\" does not exist, so cannot be freed.",
	    signal_name);
  }
}

/**************************************************************************
  Invoke all the callback functions attached to a given signal (internal).
**************************************************************************/
static void internal_signal_invoke_valist(const char *signal_name,
					  int nargs, va_list args)
{
  struct hash_table *hash;
  struct signal *signal;

  hash = signals;
  signal = hash_lookup_data(hash, signal_name);

  if (signal) {
    if (signal->nargs != nargs) {
      freelog(LOG_ERROR,
	      "Signal \"%s\" requires %d args, was passed %d on invoke.",
	      signal_name, signal->nargs, nargs);
    } else {
      signal_callback_list_iterate(signal->callbacks, pcallback) {
	if (script_callback_invoke(pcallback->name, nargs, args)) {
	  break;
	}
      } signal_callback_list_iterate_end;
    }
  } else {
    freelog(LOG_ERROR, "Signal \"%s\" does not exist, so cannot be invoked.",
	    signal_name);
  }
}

/**************************************************************************
  Invoke all the callback functions attached to a given signal.
**************************************************************************/
void script_signal_emit_valist(const char *signal_name,
			       int nargs, va_list args)
{
  internal_signal_invoke_valist(signal_name, nargs, args);
}

/**************************************************************************
  Invoke all the callback functions attached to a given signal.
**************************************************************************/
void script_signal_emit(const char *signal_name, int nargs, ...)
{
  va_list args;

  va_start(args, nargs);
  internal_signal_invoke_valist(signal_name, nargs, args);
  va_end(args);
}

/**************************************************************************
  Create a new signal type.
**************************************************************************/
void script_signal_create_valist(const char *signal_name,
				 int nargs, va_list args)
{
  struct signal *signal;

  signal = hash_lookup_data(signals, signal_name);
  if (signal) {
    freelog(LOG_ERROR, "Signal \"%s\" was already created.", signal_name);
  } else {
    enum api_types args_array[nargs];
    int i;

    for (i = 0; i < nargs; i++) {
      args_array[i] = va_arg(args, int);
    }
    internal_signal_create(signal_name, nargs, args_array);
  }
}

/**************************************************************************
  Create a new signal type.
**************************************************************************/
void script_signal_create(const char *signal_name, int nargs, ...)
{
  va_list args;

  va_start(args, nargs);
  script_signal_create_valist(signal_name, nargs, args);
  va_end(args);
}

/**************************************************************************
  Connects a callback function to a certain signal.
**************************************************************************/
void script_signal_connect(const char *signal_name, const char *callback_name)
{
  if (!signal_name) {
    script_error("nil string argument 'signal_name'.");
  } else if (!callback_name) {
    script_error("nil string argument 'callback_name'.");
  } else {
    struct signal *signal;

    signal = hash_lookup_data(signals, signal_name);
    if (signal) {
      internal_signal_callback_append(signal->callbacks, callback_name);
    } else {
      script_error("Signal \"%s\" does not exist.", signal_name);
    }
  }
}

/**************************************************************************
  Initialize script signals and callbacks.
**************************************************************************/
void script_signals_init(void)
{
  if (!signals) {
    signals = hash_new(hash_fval_string, hash_fcmp_string);

    signals_create();
  }
}

/**************************************************************************
  Free script signals and callbacks.
**************************************************************************/
void script_signals_free(void)
{
  if (signals) {
    unsigned int n = hash_num_entries(signals), i;

    for (i = 0; i < n; i++) {
      const char *signal_name;

      signal_name = hash_key_by_number(signals, i);
      internal_signal_free(signal_name);
    }
    hash_free(signals);
    signals = NULL;
  }
}

/**************************************************************************
  Declare any new signal types you need here.
**************************************************************************/
static void signals_create(void)
{
  script_signal_create("turn_started", 2, API_TYPE_INT, API_TYPE_INT);
  script_signal_create("unit_moved",
		       3, API_TYPE_UNIT, API_TYPE_TILE, API_TYPE_TILE);
  script_signal_create("hut_enter", 1, API_TYPE_UNIT);
}
