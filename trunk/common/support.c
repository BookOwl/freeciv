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
  This module contains replacements for functions which are not
  available on all platforms.  Where the functions are available
  natively, these are (mostly) just wrappers.

  Notice the function names here are prefixed by, eg, "my".  An
  alternative would be to use the "standard" function name, and
  provide the implementation only if required.  However the method
  here has some advantages:
  
   - We can provide definite prototypes in support.h, rather than
   worrying about whether a system prototype exists, and if so where,
   and whether it is correct.  (Note that whether or not configure
   finds a function and defines HAVE_FOO does not necessarily say
   whether or not there is a _prototype_ for the function available.)

   - We don't have to include config.h in support.h, but can instead
   restrict it to this .c file.

   - We can add some extra stuff to these functions if we want.

  The main disadvantage is remembering to use these "my" functions on
  systems which have the functions natively.

**********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

#include "support.h"

#ifdef HAVE_VSNPRINTF
/**********************************************************************
 Convenience function used by check_native_vsnprintf() below.
 (Can test check_native_vsnprintf() by replacing vsnprintf call
 below with vsprintf(), or by changing return value to eg -1.)
***********************************************************************/
static int test_snprintf(char *str, size_t n, const char *format, ...)
{
  int ret;
  va_list ap;

  va_start(ap, format);
  ret = vsnprintf(str, n, format, ap);
  va_end(ap);
  return ret;
}

/**********************************************************************
 This function checks, at runtime, whether a native vsnprintf() meets
 our requirements; specifically:
 - Should actually obey the parameter n, rather than, eg, just
   calling sprintf and ignoring n.
 - On truncation, should return the number of chars (not counting
   trailing null) which would have been printed, rather than, eg, -1.
 Returns 1 if both ok.
 Also checks whether null-terminates on truncation, but we don't
 base return value on this since it is easy to force this behaviour.
 Reports to stderr if DEBUG set (cannot use freelog since that
 calls my_vsnprintf).
 Could do these checks at configure time, but seems to me easier to
 do at runtime on first call...
***********************************************************************/
static int check_native_vsnprintf(void)
{
  char buf[20]   = "abcdefghijkl";   /* 12 + null */
  char *test_str = "0123456789";
  const int ntrunc = 5;
  const char one_past = buf[ntrunc];
  int test_len = strlen(test_str);
  int ret;

  ret = test_snprintf(buf, ntrunc, "%s", test_str);
#if DEBUG
  if (buf[ntrunc] != one_past) {
    fprintf(stderr, "debug: test_snprintf wrote past n\n");
  } else if (buf[ntrunc-1] != '\0') {
    fprintf(stderr, "debug: test_snprintf did not null-terminate\n");
  }
  if (ret != test_len) {
    fprintf(stderr, "debug: test_snprintf returned %d,"
	    " not untruncated length %d\n", ret, test_len);
  }
#endif

  return (buf[ntrunc]==one_past && ret==test_len);
}
#endif

/**********************************************************************
 vsnprintf() replacement using a big malloc()ed internal buffer,
 originally by David Pfitzner <dwp@mso.anu.edu.au>

 Parameter n specifies the maximum number of characters to produce.
 This includes the trailing null, so n should be the actual number
 of characters allocated (or sizeof for char array).  If truncation
 occurs, the result will still be null-terminated.  (I'm not sure
 whether all native vsnprintf() functions null-terminate on
 truncation; this does so even if calls native function.)

 Return value: if there is no truncation, returns the number of
 characters printed, not including the trailing null.  If truncation
 does occur, returns the number of characters which would have been
 produced without truncation.
 (Linux man page says returns -1 on truncation, but glibc seems to
 do as above nevertheless; check_native_vsnprintf() above tests this.)

 The method is simply to malloc (first time called) a big internal
 buffer, longer than any result is likely to be (for non-malicious
 usage), then vsprintf to that buffer, and copy the appropriate
 number of characters to the destination.  Thus, this is not 100%
 safe.  But somewhat safe, and at least safer than using raw snprintf!
 :-) (And of course if you have the native version it is safe.)

 Before rushing to provide a 100% safe replacement version, consider
 the following advantages of this method:
 
 - It is very simple, so not likely to have many bugs (other than
 arguably the core design bug regarding absolute safety), nor need
 maintenance.

 - It uses native vsprintf() (which is required), thus exactly
 duplicates the native format-string parsing/conversions.

 - It is *very* portable.  Eg, it does not require mprotect(), nor
 does it do any of its own parsing of the format string, nor use
 any tricks to go through the va_list twice.

***********************************************************************/

/* buffer size: "64k should be big enough for anyone" ;-) */
#define VSNP_BUF_SIZE (64*1024)

int my_vsnprintf(char *str, size_t n, const char *format, va_list ap)
{
  /* This may be overzealous, but I suspect any triggering of these
   * to be bugs.  (Do this even if call native function.)
   */
  assert(str);
  assert(n>0);
  assert(format);

#ifdef HAVE_VSNPRINTF
  {
    static int native_is_ok = -1; /* set to 0 or 1 on first call */

    if (native_is_ok == -1) {
      native_is_ok = check_native_vsnprintf();
    }

    if (native_is_ok) {
      int ret = vsnprintf(str, n, format, ap);
      /* Guarantee null-terminated: (native_is_ok means can use ret like this) */
      if (ret >= n) {
	str[n-1] = '\0';
      }
      return ret;
    }
  }
#endif
  /* Following is used if don't have native, or if fall through
   * from above if native doesn't pass checks.
   */
  {
    static char *buf = NULL;
    int len;

    if (buf==NULL) {
      buf = fc_malloc(VSNP_BUF_SIZE);
    }

#ifdef HAVE_VSNPRINTF
    /* This occurs if have native, but didn't pass check:
     * may just be that native doesn't give the right return,
     * in which case may be slightly safer to use it here:
     */
    vsnprintf(buf, VSNP_BUF_SIZE, format, ap);
#else
    vsprintf(buf, format, ap);
#endif

    /* Check strlen of buf straight away: could be more efficient
       not to do this and step through instead (eg if n small and
       len long), but useful anyway to get the return value, and
       importantly want to abort if vsprintf clobbered the heap!
       (Don't just use return from vsprintf since not sure if
       that gives the right thing on all platforms?)
       Will maintain last char of buffer as null, and use SIZE-2
       as longest string which we can detect as untruncated.
       (Don't use freelog() for report since that uses vsnprintf...)
    */
    buf[VSNP_BUF_SIZE-1] = '\0';
    len = strlen(buf);
    if (len >= VSNP_BUF_SIZE-1) {
      fprintf(stderr, "Overflow in vsnprintf replacement!"
	      " (buffer size %d) aborting...\n", VSNP_BUF_SIZE);
      abort();
    }

    if (n > len) {
      memcpy(str, buf, len+1);	/* +1 for terminating null */
      return len;
    } else {
      memcpy(str, buf, n-1);
      str[n-1] = '\0';
      return len;		/* truncated */
    }
  }
}

int my_snprintf(char *str, size_t n, const char *format, ...)
{
  int ret;
  va_list ap;

  assert(format);
  
  va_start(ap, format);
  ret = my_vsnprintf(str, n, format, ap);
  va_end(ap);
  return ret;
}

