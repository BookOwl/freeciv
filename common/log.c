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
#include <stdarg.h>
#include <string.h>

#include "fcintl.h"
#include "mem.h"
#include "shared.h"
#include "support.h"

#include "log.h"

static int log_level;
static char *log_filename;
static log_callback_fn log_callback;
int logd_init_counter = 1;

struct logd_fileinfo {
  char *name;
  int min;
  int max;
};
static int logd_num_files;
static struct logd_fileinfo *logd_files;

/**************************************************************************
level_str should be either "0", "1", "2", "3", "4" or
"4:filename" or "4:file1:file2" or "4:filename,100,200" etc

If everything goes ok, returns the level.
If there was a parsing problem, prints to stderr, and returns -1.

Also sets up the logd_files data structure and increments
logd_init_counter.  Does _not_ set log_level.
**************************************************************************/
int log_parse_level_str(char *level_str)
{
  char *c, *dup, *tok;
  int n = 0;			/* number of filenames */
  int i;
  int level;
  
#ifdef DEBUG
  const int max_level = LOG_DEBUG;
#else
  const int max_level = LOG_VERBOSE;
#endif

  /* re-entrant: */
  logd_num_files = 0;
  if (logd_files) {
    free(logd_files);
    logd_files = NULL;
  }
  logd_init_counter++;
  
  c = level_str;
  n = 0;
  while((c = strchr(c, ':'))) {
    c++;
    n++;
  }
  if (n == 0) {
    if (sscanf(level_str, "%d", &level) != 1) {
      fprintf(stderr, _("Bad log level \"%s\".\n"), level_str);
      return -1;
    }
    if (level >= LOG_FATAL && level <= max_level) {
      return level;
    } else {
      fprintf(stderr, _("Bad log level %d in \"%s\".\n"), level, level_str);
      if (level == LOG_DEBUG && max_level < LOG_DEBUG) {
	fprintf(stderr, _("Freeciv must be compiled with the DEBUG flag"
			  " to use debug level %d.\n"), LOG_DEBUG);
      }
      return -1;
    }
  }

  c = level_str;
  if (c[0] == ('0' + LOG_DEBUG) && c[1] == ':') {
    level = LOG_DEBUG;
    if (max_level < LOG_DEBUG) {
      fprintf(stderr, _("Freeciv must be compiled with the DEBUG flag"
			" to use debug level %d.\n"), LOG_DEBUG);
      return -1;
    }
  } else {
    fprintf(stderr, _("Badly formed log level argument \"%s\".\n"), level_str);
    return -1;
  }
  logd_num_files = n;
  logd_files = (struct logd_fileinfo *)
    fc_malloc(n * sizeof(struct logd_fileinfo));
  
  dup = mystrdup(c+2);
  tok = strtok(dup, ":");
  
  if (!tok) {
    fprintf(stderr, _("Badly formed log level argument \"%s\".\n"), level_str);
    level = -1;
    goto out;
  }
  i = 0;
  do {
    logd_files[i].min = logd_files[i].max = 0;
    c = strchr(tok, ',');
    if (c) {
      char *pc;
      c[0] = '\0';
      c++;
      pc = c;
      c = strchr(c, ',');
      if (c && *pc != '\0' && c[1] != '\0') {
	c[0] = '\0';
	if (sscanf(pc, "%d", &logd_files[i].min) != 1) {
	  fprintf(stderr, _("Not an integer: '%s'\n"), pc);
	  level = -1;
	  goto out;
	}
	if (sscanf(c + 1, "%d", &logd_files[i].max) != 1) {
	  fprintf(stderr, _("Not an integer: '%s'\n"), c + 1);
	  level = -1;
	  goto out;
	}
      }
    }
    if(strlen(tok)==0) {
      fprintf(stderr, _("Empty filename in log level argument \"%s\".\n"),
	      level_str);
      level = -1;
      goto out;
    }
    logd_files[i].name = mystrdup(tok);
    i++;
    tok = strtok(NULL, ":");
  } while(tok);

  if (i!=logd_num_files) {
    fprintf(stderr, _("Badly formed log level argument \"%s\".\n"), level_str);
    level = -1;
    goto out;
  }

 out:
  free(dup);
  return level;
}

/**************************************************************************
Initialise the log module.
Either 'filename' or 'callback' may be NULL.
If both are NULL, print to stderr.
If both are non-NULL, both callback, and fprintf to file.
**************************************************************************/
void log_init(char *filename, int initial_level, log_callback_fn callback)
{
  log_level=initial_level;
  log_filename=filename;
  log_callback=callback;
  freelog(LOG_VERBOSE, "log started");
  freelog(LOG_DEBUG, "LOG_DEBUG test");
}

/**************************************************************************
Adjust the logging level after initial log_init().
**************************************************************************/
void log_set_level(int level)
{
  log_level=level;
}

/**************************************************************************
Adjust the callback function after initial log_init().
**************************************************************************/
void log_set_callback(log_callback_fn callback)
{
  log_callback=callback;
}

/**************************************************************************
  Return an updated struct logdebug_afile_info: 
**************************************************************************/
struct logdebug_afile_info logdebug_update(const char *file)
{
  struct logdebug_afile_info ret;
  int i;

  ret.tthis = 1;
  ret.min = 0;
  ret.max = 0;
  if (logd_num_files==0) {
    return ret;
  }
  ret.tthis = 0;
  for (i = 0; i < logd_num_files; i++) {
    if((strstr(file, logd_files[i].name))) {
      ret.tthis = 1;
      ret.min = logd_files[i].min;
      ret.max = logd_files[i].max;
      return ret;
    }
  }
  return ret;
}

/**************************************************************************
Unconditionally print a simple string.
Let the callback do its own level formating and add a '\n' if it wants.
**************************************************************************/
static void log_write(FILE *fs, int level, char *message)
{
  if ((!log_filename) && log_callback) {
    log_callback(level, message);
  }
  if (log_filename || (!log_callback)) {
    fprintf(fs, "%d: %s\n", level, message);
  }
}

/**************************************************************************
Print a log message.
Only prints if level <= log_level.
For repeat message, may wait and print instead
"last message repeated ..." at some later time.
Calls log_callback if non-null, else prints to stderr.
**************************************************************************/
#define MAX_LEN_LOG_LINE 512
void vreal_freelog(int level, const char *message, va_list ap)
{
  static char bufbuf[2][MAX_LEN_LOG_LINE];
  char buf[MAX_LEN_LOG_LINE];
  static bool bufbuf1 = FALSE;
  static unsigned int repeated=0; /* total times current message repeated */
  static unsigned int next=2;	/* next total to print update */
  static unsigned int prev=0;	/* total on last update */
  static int prev_level=-1;	/* only count as repeat if same level  */

  if(level<=log_level) {
    FILE *fs;

    if(log_filename) {
      if(!(fs=fopen(log_filename, "a"))) {
	fprintf(stderr, _("Couldn't open logfile: %s for appending.\n"), 
		log_filename);
	exit(EXIT_FAILURE);
      }
    }
    else fs=stderr;

    my_vsnprintf(bufbuf1 ? bufbuf[1] : bufbuf[0], MAX_LEN_LOG_LINE, message, ap);
    
    if(level==prev_level && 0==strncmp(bufbuf[0],bufbuf[1],MAX_LEN_LOG_LINE-1)){
      repeated++;
      if(repeated==next){
	my_snprintf(buf, sizeof(buf),
		    _("last message repeated %d times"), repeated-prev);
	if (repeated>2) {
	  cat_snprintf(buf, sizeof(buf), _(" (total %d repeats)"), repeated);
	}
	log_write(fs, prev_level, buf);
	prev=repeated;
	next*=2;
      }
    }else{
      if(repeated>0 && repeated!=prev){
	if(repeated==1) {
	  /* just repeat the previous message: */
	  log_write(fs, prev_level, bufbuf1 ? bufbuf[0] : bufbuf[1]);
	} else {
	  if(repeated-prev==1) {
	    sz_strlcpy(buf, _("last message repeated once"));
	  } else {
	    my_snprintf(buf, sizeof(buf),
			_("last message repeated %d times"), repeated-prev);
	  }
	  if (repeated>2) {
	    cat_snprintf(buf, sizeof(buf), _(" (total %d repeats)"), repeated);
	  }
	  log_write(fs, prev_level, buf);
	}
      }
      prev_level=level;
      repeated=0;
      next=2;
      prev=0;
      log_write(fs, level, bufbuf1 ? bufbuf[1] : bufbuf[0]);
    }
    bufbuf1 = !bufbuf1;
    fflush(fs);
    if(log_filename)
      fclose(fs);
  }
}
void real_freelog(int level, const char *message, ...)
{
  va_list ap;
  va_start(ap, message);
  vreal_freelog(level, message, ap);
  va_end(ap);
}
