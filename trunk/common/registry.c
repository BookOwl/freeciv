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

/**************************************************************************
  the idea with this file is to create something similar to the ms-windows
  .ini files functions.
  it also demonstrates how ugly code using the genlist class looks.
  however the interface is nice. ie:
  section_file_lookup_string(file, "player%d.unit%d.name", plrno, unitno); 
***************************************************************************/

/**************************************************************************
  Description of the file format:
  (This is based on a format by the original authors, with
  various incremental extensions. --dwp)
  
  - Whitespace lines are ignored, as are lines where the first
  non-whitespace character is ';' (comment lines).
  Optionally '#' can also be used for comments.

  - A line of the form:
       *include "filename"
  includes the named file at that point.  (The '*' must be the
  first character on the line.) The file is found by looking in
  FREECIV_PATH.  Non-infinite recursive includes are allowed.
  
  - A line with "[name]" labels the start of a section with
  that name; one of these must be the first non-comment line in
  the file.  Any spaces within the brackets are included in the
  name, but this feature (?) should probably not be used...

  - Within a section, lines have one of the following forms:
      subname = "stringvalue"
      subname = -digits
      subname = digits
  for a value with given name and string, negative integer, and
  positive integer values, respectively.  These entries are
  referenced in the following functions as "sectionname.subname".
  The section name should not contain any dots ('.'); the subname
  can, but they have no particular significance.  There can be
  optional whitespace before and/or after the equals sign.
  You can put a newline after (but not before) the equals sign.
  
  Backslash is an escape character in strings (double-quoted strings
  only, not names); recognised escapes are \n, \\, and \".
  (Any other \<char> is just treated as <char>.)

  - Gettext markings:  You can surround strings like so:
      foo = _("stringvalue")
  The registry just ignores these extra markings, but this is
  useful for marking strings for translations via gettext tools.

  - Multiline strings:  Strings can have embeded newlines, eg:
    foo = _("
    This is a string
    over multiple lines
    ")
  This is equivalent to:
    foo = _("\nThis is a string\nover multiple lines\n")
  Note that if you missplace the trailing doublequote you can
  easily end up with strange errors reading the file...

  - Vector format: An entry can have multiple values separated
  by commas, eg:
      foo = 10, 11, "x"
  These are accessed by names "foo", "foo,1" and "foo,2"
  (with section prefix as above).  So the above is equivalent to:
      foo   = 10
      foo,1 = 11
      foo,2 = "x"
  As in the example, in principle you can mix integers and strings,
  but the calling program will probably require elements to be the
  same type.   Note that the first element of a vector is not "foo,0",
  in order that the name of the first element is the same whether or
  not there are subsequent elements.  However as a convenience, if
  you try to lookup "foo,0" then you get back "foo".  (So you should
  never have "foo,0" as a real name in the datafile.)

  - Tabular format:  The lines:
      foo = { "bar",  "baz",   "bax"
              "wow",   10,     -5
              "cool",  "str"
              "hmm",    314,   99, 33, 11
      }
  are equivalent to the following:
      foo0.bar = "wow"
      foo0.baz = 10
      foo0.bax = -5
      foo1.bar = "cool"
      foo1.baz = "str"
      foo2.bar = "hmm"
      foo2.baz = 314
      foo2.bax = 99
      foo2.bax,1 = 33
      foo2.bax,2 = 11
  The first line specifies the base name and the column names, and the
  subsequent lines have data.  Again it is possible to mix string and
  integer values in a column, and have either more or less values
  in a row than there are column headings, but the code which uses
  this information (via the registry) may set more stringent conditions.
  If a row has more entries than column headings, the last column is
  treated as a vector (as above).  You can optionally put a newline
  after '=' and/or after '{'.

  The equivalence above between the new and old formats is fairly
  direct: internally, data is converted to the old format.
  In principle it could be a good idea to represent the data
  as a table (2-d array) internally, but the current method
  seems sufficient and relatively simple...
  
  There is a limited ability to save data in tabular:
  So long as the section_file is constructed in an expected way,
  tabular data (with no missing or extra values) can be saved
  in tabular form.  (See section_file_save().)

  - Multiline vectors: if the last non-comment non-whitespace
  character in a line is a comma, the line is considered to
  continue on to the next line.  Eg:
      foo = 10,
            11,
            "x"
  This is equivalent to the original "vector format" example above.
  Such multi-lines can occur for column headings, vectors, or
  table rows, again with some potential for strange errors...

***************************************************************************/

/**************************************************************************
  Hashing registry lookups: (by dwp)
  - Have a hash table direct to entries, bypassing sections division.
  - For convenience, store the key (the full section+entry name)
    in the hash table (some memory overhead).
  - The number of entries is fixed when the hash table is built.
  - Now uses hash.c
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "astring.h"
#include "genlist.h"
#include "hash.h"
#include "inputfile.h"
#include "ioz.h"
#include "log.h"
#include "mem.h"
#include "sbuffer.h"
#include "shared.h"
#include "support.h"

#include "registry.h"

#define MAX_LEN_BUFFER 1024

#define SAVE_TABLES 1		/* set to 0 for old-style savefiles */
#define SECF_DEBUG_ENTRIES 0	/* LOG_DEBUG each entry value */

/* An 'entry' is a single value, either a string or integer;
 * Whether string or int is determined by whether svalue is NULL.
 */
struct entry {
  char *name;			/* name, not including section prefix */
  int  ivalue;			/* value if integer */
  char *svalue;			/* value if string (in sbuffer) */
  int  used;			/* number of times entry looked up */
  char *comment;                /* comment, may be NULL */
};

/* create a 'struct entry_list' and related functions: */
#define SPECLIST_TAG entry
#include "speclist.h"
#define SPECLIST_TAG entry
#include "speclist_c.h"

#define entry_list_iterate(entlist, pentry) \
       TYPED_LIST_ITERATE(struct entry, entlist, pentry)
#define entry_list_iterate_end  LIST_ITERATE_END


struct section {
  char *name;
  struct entry_list entries;
};

/* create a 'struct section_list' and related functions: */
#define SPECLIST_TAG section
#include "speclist.h"
#define SPECLIST_TAG section
#include "speclist_c.h"

#define section_list_iterate(seclist, psection) \
       TYPED_LIST_ITERATE(struct section, seclist, psection)
#define section_list_iterate_end  LIST_ITERATE_END

#define section_list_iterate_rev(seclist, psection) \
       TYPED_LIST_ITERATE_REV(struct section, seclist, psection)
#define section_list_iterate_rev_end  LIST_ITERATE_REV_END

/* The hash table and some extra data: */
struct hash_data {
  struct hash_table *htbl;
  int num_entries_hashbuild;
  int allow_duplicates;
  int num_duplicates;
};

static void secfilehash_check(struct section_file *file);
static void secfilehash_insert(struct section_file *file,
			       char *key, struct entry *data);

static char *minstrdup(struct sbuffer *sb, const char *str);
static char *moutstr(char *str);

static struct entry*
section_file_lookup_internal(struct section_file *my_section_file,  
			     char *fullpath);
static struct entry*
section_file_insert_internal(struct section_file *my_section_file, 
			     char *fullpath);

/**************************************************************************
  Return the filename the sectionfile was loaded as, or "(anonymous)"
  if this sectionfile was created rather than loaded from file.
  The memory is managed internally, and should not be altered,
  nor used after section_file_free() called for the sectionfile.
**************************************************************************/
const char *secfile_filename(const struct section_file *file)
{
  if (file->filename != NULL) {
    return file->filename;
  } else {
    return "(anonymous)";
  }
}

/**************************************************************************
...
**************************************************************************/
void section_file_init(struct section_file *file)
{
  file->filename = NULL;
  file->sections = fc_malloc(sizeof(struct section_list));
  section_list_init(file->sections);
  file->num_entries = 0;
  file->hashd = NULL;
  file->sb = sbuf_new();
}

/**************************************************************************
...
**************************************************************************/
void section_file_free(struct section_file *file)
{
  /* all the real data is stored in the sbuffer;
     just free the list meta-data:
  */
  section_list_iterate(*file->sections, psection) {
    entry_list_unlink_all(&psection->entries);
  }
  section_list_iterate_end;
  
  section_list_unlink_all(file->sections);
  
  free(file->sections);
  file->sections = NULL;

  /* free the hash data: */
  if(secfilehash_hashash(file)) {
    secfilehash_free(file);
  }
  
  /* free the real data: */
  sbuf_free(file->sb);
  free(file->filename);

  file->sb = NULL;
}

/**************************************************************************
  Print log messages for any entries in the file which have
  not been looked up -- ie, unused or unrecognised entries.
  To mark an entry as used without actually doing anything with it,
  you could do something like:
     section_file_lookup(&file, "foo.bar");  / * unused * /
**************************************************************************/
void section_file_check_unused(struct section_file *file, const char *filename)
{
  int any = 0;

  section_list_iterate(*file->sections, psection) {
    entry_list_iterate(psection->entries, pentry) {
      if (!pentry->used) {
	if (any==0 && filename!=NULL) {
	  freelog(LOG_NORMAL, "Unused entries in file %s:", filename);
	  any = 1;
	}
	freelog(LOG_NORMAL, "  unused entry: %s.%s",
		psection->name, pentry->name);
      }
    }
    entry_list_iterate_end;
  }
  section_list_iterate_end;
}

/**************************************************************************
  Return a new entry struct, allocated from sb, with given name,
  and where tok is a "value" return token from inputfile.
  The entry value has any escaped double-quotes etc removed.
**************************************************************************/
static struct entry *new_entry(struct sbuffer *sb, const char *name,
			       const char *tok)
{
  struct entry *pentry;

  pentry = sbuf_malloc(sb, sizeof(struct entry));
  pentry->name = sbuf_strdup(sb, name);
  pentry->comment = NULL;
  if (tok[0] == '\"') {
    pentry->svalue = minstrdup(sb, tok+1);
    pentry->ivalue = 0;
    if (SECF_DEBUG_ENTRIES) {
      freelog(LOG_DEBUG, "entry %s '%s'", name, pentry->svalue);
    }
  } else {
    pentry->svalue = NULL;
    pentry->ivalue = atoi(tok);
    if (SECF_DEBUG_ENTRIES) {
      freelog(LOG_DEBUG, "entry %s %d", name, pentry->ivalue);
    }
  }
  pentry->used = 0;
  return pentry;
}
	

/**************************************************************************
...
**************************************************************************/
static int section_file_load_dup(struct section_file *sf,
				 const char *filename,
				 int allow_duplicates)
{
  struct inputfile *inf;
  struct section *psection = NULL;
  struct entry *pentry;
  int table_state = 0;		/* 1 when within tabular format */
  int table_lineno = 0;		/* row number in tabular, 0=top data row */
  struct sbuffer *sb;
  const char *tok;
  int i;
  struct astring base_name = ASTRING_INIT;    /* for table or single entry */
  struct astring entry_name = ASTRING_INIT;
  struct athing columns_tab;	        /* astrings for column headings */
  struct astring *columns = NULL;	/* -> columns_tab.ptr */

  inf = inf_open(filename, datafilename);
  if (inf == NULL) {
    return 0;
  }
  section_file_init(sf);
  sf->filename = mystrdup(filename);
  ath_init(&columns_tab, sizeof(struct astring));
  sb = sf->sb;

  freelog(LOG_VERBOSE, "Reading file \"%s\"", filename);

  while(!inf_at_eof(inf)) {
    if (inf_token(inf, INF_TOK_EOL) != NULL)
      continue;
    if (inf_at_eof(inf)) {
      /* may only realise at eof after trying to read eol above */
      break;
    }
    tok = inf_token(inf, INF_TOK_SECTION_NAME);
    if (tok != NULL) {
      if (table_state) {
	inf_die(inf, "new section during table");
      }
      /* Check if we already have a section with this name.
	 (Could ignore this and have a duplicate sections internally,
	 but then secfile_get_secnames_prefix would return duplicates.)
	 Duplicate section in input are likely to be useful for includes.
	 This is slow if there are lots of sections; [cs]hould have a
	 hash on section names.
      */
      psection = NULL;
      section_list_iterate(*sf->sections, asection) {
	if (strcmp(asection->name, tok)==0) {
	  psection = asection;
	  break;
	}
      }
      section_list_iterate_end;
      if (psection==NULL) {
	psection = sbuf_malloc(sb, sizeof(struct section));
	psection->name = sbuf_strdup(sb, tok);
	entry_list_init(&psection->entries);
	section_list_insert_back(sf->sections, psection);
      }
      inf_token_required(inf, INF_TOK_EOL);
      continue;
    }
    if (psection == NULL) {
      inf_die(inf, "data before first section");
    }
    if (inf_token(inf, INF_TOK_TABLE_END) != NULL) {
      if (!table_state) {
	inf_die(inf, "misplaced \"}\"");
      }
      inf_token_required(inf, INF_TOK_EOL);
      table_state=0;
      continue;
    }
    if (table_state) {
      i = -1;
      do {
	i++;
	inf_discard_tokens(inf, INF_TOK_EOL);  	/* allow newlines */
	tok = inf_token_required(inf, INF_TOK_VALUE);
	if (i<columns_tab.n) {
	  astr_minsize(&entry_name, base_name.n + 10 + columns[i].n);
	  my_snprintf(entry_name.str, entry_name.n_alloc, "%s%d.%s",
		      base_name.str, table_lineno, columns[i].str);
	} else {
	  astr_minsize(&entry_name,
		       base_name.n + 20 +  columns[columns_tab.n-1].n);
	  my_snprintf(entry_name.str, entry_name.n_alloc, "%s%d.%s,%d",
		      base_name.str, table_lineno,
		      columns[columns_tab.n-1].str, i-columns_tab.n+1);
	}
	pentry = new_entry(sb, entry_name.str, tok);
	entry_list_insert_back(&psection->entries, pentry);
	sf->num_entries++;
      } while(inf_token(inf, INF_TOK_COMMA) != NULL);
      
      inf_token_required(inf, INF_TOK_EOL);
      table_lineno++;
      continue;
    }
    
    tok = inf_token_required(inf, INF_TOK_ENTRY_NAME);
    /* need to store tok before next calls: */
    astr_minsize(&base_name, strlen(tok)+1);
    strcpy(base_name.str, tok);

    inf_discard_tokens(inf, INF_TOK_EOL);  	/* allow newlines */
    
    if (inf_token(inf, INF_TOK_TABLE_START) != NULL) {
      i = -1;
      do {
	i++;
	inf_discard_tokens(inf, INF_TOK_EOL);  	/* allow newlines */
	tok = inf_token_required(inf, INF_TOK_VALUE);
	if( tok[0] != '\"' ) {
	  inf_die(inf, "table column header non-string");
	}
	{ 	/* expand columns_tab: */
	  int j, n_prev;
	  n_prev = columns_tab.n_alloc;
	  ath_minnum(&columns_tab, (i+1));
	  columns = columns_tab.ptr;
	  for(j=n_prev; j<columns_tab.n_alloc; j++) {
	    astr_init(&columns[j]);
	  }
	}
	astr_minsize(&columns[i], strlen(tok));
	strcpy(columns[i].str, tok+1);
	
      } while(inf_token(inf, INF_TOK_COMMA) != NULL);
      
      inf_token_required(inf, INF_TOK_EOL);
      table_state=1;
      table_lineno=0;
      continue;
    }
    /* ordinary value: */
    i = -1;
    do {
      i++;
      inf_discard_tokens(inf, INF_TOK_EOL);  	/* allow newlines */
      tok = inf_token_required(inf, INF_TOK_VALUE);
      if (i==0) {
	pentry = new_entry(sb, base_name.str, tok);
      } else {
	astr_minsize(&entry_name, base_name.n + 20);
	my_snprintf(entry_name.str, entry_name.n_alloc,
		    "%s,%d", base_name.str, i);
	pentry = new_entry(sb, entry_name.str, tok);
      }
      entry_list_insert_back(&psection->entries, pentry);
      sf->num_entries++;
    } while(inf_token(inf, INF_TOK_COMMA) != NULL);
    inf_token_required(inf, INF_TOK_EOL);
  }
  
  if (table_state) {
    freelog(LOG_FATAL, "finished file %s before end of table\n", filename);
    exit(EXIT_FAILURE);
  }

  inf_close(inf);
  
  astr_free(&base_name);
  astr_free(&entry_name);
  for(i=0; i<columns_tab.n_alloc; i++) {
    astr_free(&columns[i]);
  }
  ath_free(&columns_tab);
  
  secfilehash_build(sf, allow_duplicates);
    
  return 1;
}

/**************************************************************************
...
**************************************************************************/
int section_file_load(struct section_file *my_section_file,
		      const char *filename)
{
  return section_file_load_dup(my_section_file, filename, 1);
}

/**************************************************************************
  Load a section_file, but disallow (die on) duplicate entries.
**************************************************************************/
int section_file_load_nodup(struct section_file *my_section_file,
			    const char *filename)
{
  return section_file_load_dup(my_section_file, filename, 0);
}

/**************************************************************************
 Save the previously filled in section_file to disk.
 
 There is now limited ability to save in the new tabular format
 (to give smaller savefiles).
 The start of a table is detected by an entry with name of the form:
    (alphabetical_component)(zero)(period)(alphanumeric_component)
 Eg: u0.id, or c0.id, in the freeciv savefile.
 The alphabetical component is taken as the "name" of the table,
 and the component after the period as the first column name.
 This should be followed by the other column values for u0,
 and then subsequent u1, u2, etc, in strict order with no omissions,
 and with all of the columns for all uN in the same order as for u0.

 If compression_level is non-zero, then compress using zlib.  (Should
 only supply non-zero compression_level if already know that HAVE_LIBZ.)
 Below simply specifies FZ_ZLIB method, since fz_fopen() automatically
 changes to FZ_PLAIN method when level==0.
**************************************************************************/
int section_file_save(struct section_file *my_section_file, const char *filename,
		      int compression_level)
{
  fz_FILE *fs = fz_fopen(filename, "w", FZ_ZLIB, compression_level);

  struct genlist_iterator ent_iter, save_iter, col_iter;
  struct entry *pentry, *col_pentry;

  if (fs == NULL)
    return 0;

  section_list_iterate(*my_section_file->sections, psection) {
    fz_fprintf(fs, "\n[%s]\n", psection->name);

    /* Following doesn't use entry_list_iterate() because we want to do
     * tricky things with the iterators...
     */
    for(genlist_iterator_init(&ent_iter, &psection->entries.list, 0);
	(pentry = ITERATOR_PTR(ent_iter)) != NULL;
	ITERATOR_NEXT(ent_iter)) {

      /* Tables: break out of this loop if this is a non-table
       * entry (pentry and ent_iter unchanged) or after table (pentry
       * and ent_iter suitably updated, pentry possibly NULL).
       * After each table, loop again in case the next entry
       * is another table.
       */
      for(;;) {
	char *c, *first, base[64];
	int offset, irow, icol, ncol;
	
	/* Example: for first table name of "xyz0.blah":
	 *  first points to the original string pentry->name
	 *  base contains "xyz";
	 *  offset=5 (so first+offset gives "blah")
	 *  note strlen(base)=offset-2
	 */

	if(!SAVE_TABLES) break;
	
	c = first = pentry->name;
	if(!*c || !isalpha(*c)) break;
	for( ; *c && isalpha(*c); c++);
	if(strncmp(c,"0.",2) != 0) break;
	c+=2;
	if(!*c || !isalnum(*c)) break;

	offset = c - first;
	first[offset-2] = '\0';
	sz_strlcpy(base, first);
	first[offset-2] = '0';
	fz_fprintf(fs, "%s={", base);

	/* Save an iterator at this first entry, which we can later use
	 * to repeatedly iterate over column names:
	 */
	save_iter = ent_iter;

	/* write the column names, and calculate ncol: */
	ncol = 0;
	col_iter = save_iter;
	for( ; (col_pentry = ITERATOR_PTR(col_iter)) != NULL; ITERATOR_NEXT(col_iter)) {
	  if(strncmp(col_pentry->name, first, offset) != 0)
	    break;
	  fz_fprintf(fs, "%c\"%s\"", (ncol==0?' ':','), col_pentry->name+offset);
	  ncol++;
	}
	fz_fprintf(fs, "\n");

	/* Iterate over rows and columns, incrementing ent_iter as we go,
	 * and writing values to the table.  Have a separate iterator
	 * to the column names to check they all match.
	 */
	irow = icol = 0;
	col_iter = save_iter;
	for(;;) {
	  char expect[128];	/* pentry->name we're expecting */

	  pentry = ITERATOR_PTR(ent_iter);
	  col_pentry = ITERATOR_PTR(col_iter);

	  my_snprintf(expect, sizeof(expect), "%s%d.%s",
		      base, irow, col_pentry->name+offset);

	  /* break out of tabular if doesn't match: */
	  if (pentry == NULL || (strcmp(pentry->name, expect) != 0)) {
	    if(icol != 0) {
	      freelog(LOG_ERROR, "unexpected exit from tabular at %s (%s)",
		      pentry->name, filename);
	      fz_fprintf(fs, "\n");
	    }
	    fz_fprintf(fs, "}\n");
	    break;
	  }

	  if(icol>0)
	    fz_fprintf(fs, ",");
	  if(pentry->svalue != NULL) 
	    fz_fprintf(fs, "\"%s\"", moutstr(pentry->svalue));
	  else
	    fz_fprintf(fs, "%d", pentry->ivalue);
	  
	  ITERATOR_NEXT(ent_iter);
	  ITERATOR_NEXT(col_iter);
	  
	  icol++;
	  if(icol==ncol) {
	    fz_fprintf(fs, "\n");
	    irow++;
	    icol = 0;
	    col_iter = save_iter;
	  }
	}
	if(pentry == NULL) break;
      }
      if(pentry == NULL) break;

      if(pentry->svalue != NULL)
	fz_fprintf(fs, "%s=\"%s\"", pentry->name, moutstr(pentry->svalue));
      else
	fz_fprintf(fs, "%s=%d", pentry->name, pentry->ivalue);
      if (pentry->comment != NULL) {
	fz_fprintf(fs, "  # %s\n", pentry->comment);
      } else {
	fz_fprintf(fs, "\n");
      }
    }
  }
  section_list_iterate_end;
  
  moutstr(NULL);		/* free internal buffer */

  if (fz_ferror(fs)) {
    freelog(LOG_ERROR, "Error before closing %s: %s", filename,
	    fz_strerror(fs));
    fz_fclose(fs);
    return 0;
  }
  if (fz_fclose(fs) != 0) {
    freelog(LOG_ERROR, "Error closing %s", filename);
    return 0;
  }

  return 1;
}

/**************************************************************************
...
**************************************************************************/
char *secfile_lookup_str(struct section_file *my_section_file, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  pentry = section_file_lookup_internal(my_section_file, buf);
  if (pentry == NULL) {
    freelog(LOG_FATAL, "sectionfile %s doesn't contain a '%s' entry",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }

  if (pentry->svalue == NULL) {
    freelog(LOG_FATAL, "sectionfile %s entry '%s' doesn't contain a string",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }
  
  return pentry->svalue;
}

/**************************************************************************
  Lookup string or int value; if (char*) return is NULL, int value is
  put into (*ival).
**************************************************************************/
char *secfile_lookup_str_int(struct section_file *my_section_file, 
			     int *ival, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  assert(ival != NULL);
  
  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  if ((pentry = section_file_lookup_internal(my_section_file, buf)) == NULL) {
    freelog(LOG_FATAL, "sectionfile %s doesn't contain a '%s' entry",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }

  if(pentry->svalue != NULL) {
    return pentry->svalue;
  } else {
    *ival = pentry->ivalue;
    return NULL;
  }
}
      
/**************************************************************************
...
**************************************************************************/
void secfile_insert_int(struct section_file *my_section_file,
			int val, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  pentry=section_file_insert_internal(my_section_file, buf);

  pentry->ivalue=val;
  pentry->svalue = NULL;
  pentry->comment = NULL;
}

/**************************************************************************
...
**************************************************************************/
void secfile_insert_int_comment(struct section_file *my_section_file,
				int val, const char *const comment,
				char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  pentry = section_file_insert_internal(my_section_file, buf);

  pentry->ivalue = val;
  pentry->svalue = NULL;
  pentry->comment = sbuf_strdup(my_section_file->sb, comment);
}

/**************************************************************************
...
**************************************************************************/
void secfile_insert_str(struct section_file *my_section_file,
			char *sval, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  pentry = section_file_insert_internal(my_section_file, buf);
  pentry->svalue = sbuf_strdup(my_section_file->sb, sval);
  pentry->comment = NULL;
}

/**************************************************************************
...
**************************************************************************/
void secfile_insert_str_comment(struct section_file *my_section_file,
				char *sval, const char *const comment,
				char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  pentry = section_file_insert_internal(my_section_file, buf);
  pentry->svalue = sbuf_strdup(my_section_file->sb, sval);
  pentry->comment = sbuf_strdup(my_section_file->sb, comment);
}

/**************************************************************************
...
**************************************************************************/
int secfile_lookup_int(struct section_file *my_section_file, 
		       char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  if ((pentry = section_file_lookup_internal(my_section_file, buf)) == NULL) {
    freelog(LOG_FATAL, "sectionfile %s doesn't contain a '%s' entry",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }

  if(pentry->svalue != NULL) {
    freelog(LOG_FATAL, "sectionfile %s entry '%s' doesn't contain an integer",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }
  
  return pentry->ivalue;
}


/**************************************************************************
  As secfile_lookup_int(), but return a specified default value if the
  entry does not exist.  If the entry exists as a string, then die.
**************************************************************************/
int secfile_lookup_int_default(struct section_file *my_section_file,
			       int def, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  if((pentry=section_file_lookup_internal(my_section_file, buf)) == NULL) {
    return def;
  }
  if(pentry->svalue != NULL) {
    freelog(LOG_FATAL, "sectionfile %s contains a '%s', but string not integer",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }
  return pentry->ivalue;
}

/**************************************************************************
  As secfile_lookup_str(), but return a specified default (char*) if the
  entry does not exist.  If the entry exists as an int, then die.
**************************************************************************/
char *secfile_lookup_str_default(struct section_file *my_section_file, 
				 char *def, char *path, ...)
{
  struct entry *pentry;
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  if((pentry=section_file_lookup_internal(my_section_file, buf)) == NULL) {
    return def;
  }

  if(pentry->svalue == NULL) {
    freelog(LOG_FATAL, "sectionfile %s contains a '%s', but integer not string",
	    secfile_filename(my_section_file), buf);
    exit(EXIT_FAILURE);
  }
  
  return pentry->svalue;
}

/**************************************************************************
...
**************************************************************************/
int section_file_lookup(struct section_file *my_section_file, 
			char *path, ...)
{
  char buf[MAX_LEN_BUFFER];
  va_list ap;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  return BOOL_VAL(section_file_lookup_internal(my_section_file, buf) !=
		  NULL);
}


/**************************************************************************
...
**************************************************************************/
static struct entry*
section_file_lookup_internal(struct section_file *my_section_file,  
			     char *fullpath) 
{
  char *pdelim;
  char sec_name[MAX_LEN_BUFFER];
  char ent_name[MAX_LEN_BUFFER];
  char mod_fullpath[2*MAX_LEN_BUFFER];
  int len;
  struct entry *result;

  /* freelog(LOG_DEBUG, "looking up: %s", fullpath); */
  
  /* treat "sec.foo,0" as "sec.foo": */
  len = strlen(fullpath);
  if(len>2 && fullpath[len-2]==',' && fullpath[len-1]=='0') {
    assert(len<sizeof(mod_fullpath));
    strcpy(mod_fullpath, fullpath);
    fullpath = mod_fullpath;	/* reassign local pointer 'fullpath' */
    fullpath[len-2] = '\0';
  }
  
  if (secfilehash_hashash(my_section_file)) {
    result = hash_lookup_data(my_section_file->hashd->htbl, fullpath);
    if (result != NULL) {
      result->used++;
    }
    return result;
  }

  /* i dont like strtok */
  pdelim = strchr(fullpath, '.');
  if (pdelim == NULL) {
    return NULL;
  }

  mystrlcpy(sec_name, fullpath, MIN(pdelim-fullpath+1, sizeof(sec_name)));
  sz_strlcpy(ent_name, pdelim+1);

  section_list_iterate(*my_section_file->sections, psection) {
    if (strcmp(psection->name, sec_name) == 0 ) {
      entry_list_iterate(psection->entries, pentry) {
   	if (strcmp(pentry->name, ent_name) == 0) {
	  result = pentry;
	  result->used++;
	  return result;
	}
      }
      entry_list_iterate_end;
    }
  }
  section_list_iterate_end;

  return NULL;
}


/**************************************************************************
 The caller should ensure that "fullpath" should not refer to an entry
 which already exists in "my_section_file".  (Actually, in some cases
 now it is ok to have duplicate entries, but be careful...)
**************************************************************************/
static struct entry*
section_file_insert_internal(struct section_file *my_section_file, 
			     char *fullpath)
{
  char *pdelim;
  char sec_name[MAX_LEN_BUFFER];
  char ent_name[MAX_LEN_BUFFER];
  struct section *psection;
  struct entry *pentry;
  struct sbuffer *sb = my_section_file->sb;

  if((pdelim=strchr(fullpath, '.')) == NULL) { /* d dont like strtok */
    freelog(LOG_FATAL,
	    "Insertion fullpath \"%s\" missing '.' for sectionfile %s",
	    fullpath, secfile_filename(my_section_file));
    exit(EXIT_FAILURE);
  }
  mystrlcpy(sec_name, fullpath, MIN(pdelim-fullpath+1,sizeof(sec_name)));
  sz_strlcpy(ent_name, pdelim+1);
  my_section_file->num_entries++;
  
  if(strlen(sec_name)==0 || strlen(ent_name)==0) {
    freelog(LOG_FATAL,
	    "Insertion fullpath \"%s\" missing %s for sectionfile %s",
	    fullpath, (strlen(sec_name)==0 ? "section" : "entry"),
	    secfile_filename(my_section_file));
    exit(EXIT_FAILURE);
  }

  /* Do a reverse search of sections, since we're most likely
   * to be adding to the lastmost section.
   */
  section_list_iterate_rev(*my_section_file->sections, psection) {
    if(strcmp(psection->name, sec_name)==0) {
      /* This DOES NOT check whether the entry already exists in
       * the section, to avoid O(N^2) behaviour.
       */
      pentry = sbuf_malloc(sb, sizeof(struct entry));
      pentry->name = sbuf_strdup(sb, ent_name);
      entry_list_insert_back(&psection->entries, pentry);
      return pentry;
    }
  }
  section_list_iterate_rev_end;

  psection = sbuf_malloc(sb, sizeof(struct section));
  psection->name = sbuf_strdup(sb, sec_name);
  entry_list_init(&psection->entries);
  section_list_insert_back(my_section_file->sections, psection);
  
  pentry = sbuf_malloc(sb, sizeof(struct entry));
  pentry->name = sbuf_strdup(sb, ent_name);
  entry_list_insert_back(&psection->entries, pentry);

  return pentry;
}


/**************************************************************************
 Return 0 if the section_file has not been setup for hashing.
**************************************************************************/
int secfilehash_hashash(struct section_file *file)
{
  return (file->hashd!=NULL && hash_num_buckets(file->hashd->htbl)!=0);
}

/**************************************************************************
  Basic checks for existence/integrity of hash data and fail if bad.
**************************************************************************/
static void secfilehash_check(struct section_file *file)
{
  if (!secfilehash_hashash(file)) {
    freelog(LOG_FATAL, "sectionfile %s hash operation before setup",
	    secfile_filename(file));
    exit(EXIT_FAILURE);
  }
  if (file->num_entries != file->hashd->num_entries_hashbuild) {
    freelog(LOG_FATAL, "sectionfile %s has more entries than when hash built",
	    secfile_filename(file));
    exit(EXIT_FAILURE);
  }
}

/**************************************************************************
 Insert a entry into the hash table.  The key is malloced here (using sbuf;
 malloc somewhere required by hash implementation).
**************************************************************************/
static void secfilehash_insert(struct section_file *file,
			       char *key, struct entry *data)
{
  struct entry *hentry;

  key = sbuf_strdup(file->sb, key);
  hentry = hash_replace(file->hashd->htbl, key, data);
  if (hentry != NULL) {
    if (file->hashd->allow_duplicates) {
      hentry->used = 1;
      file->hashd->num_duplicates++;
      /* Subsequent entries replace earlier ones; could do differently so
	 that first entry would be used and following ones ignored.
	 Need to mark the replaced one as used or else it will show
	 up when we iterate the sections and entries (since hash
	 lookup will never find it to mark it as used).
      */
    } else {
      freelog(LOG_FATAL, "Tried to insert same value twice: %s (sectionfile %s)",
	      key, secfile_filename(file));
      exit(EXIT_FAILURE);
    }
  }
}

/**************************************************************************
 Build a hash table for the file.  Note that the section_file should
 not be modified (except to free it) subsequently.
 If allow_duplicates is true, then relax normal condition that
 all entries must have unique names; in this case for duplicates
 the hash ref will be to the _last_ entry.
**************************************************************************/
void secfilehash_build(struct section_file *file, int allow_duplicates)
{
  struct hash_data *hashd;
  char buf[256];

  hashd = file->hashd = fc_malloc(sizeof(struct hash_data));
  hashd->htbl = hash_new_nentries(hash_fval_string, hash_fcmp_string,
				  file->num_entries);
  
  hashd->num_entries_hashbuild = file->num_entries;
  hashd->allow_duplicates = allow_duplicates;
  hashd->num_duplicates = 0;
  
  section_list_iterate(*file->sections, psection) {
    entry_list_iterate(psection->entries, pentry) {
      my_snprintf(buf, sizeof(buf), "%s.%s", psection->name, pentry->name);
      secfilehash_insert(file, buf, pentry);
    }
    entry_list_iterate_end;
  }
  section_list_iterate_end;
  
  if (hashd->allow_duplicates) {
    freelog(LOG_DEBUG, "Hash duplicates during build: %d",
	    hashd->num_duplicates);
  }
}


/**************************************************************************
 Free the memory allocated for the hash table.
**************************************************************************/
void secfilehash_free(struct section_file *file)
{
  secfilehash_check(file);
  hash_free(file->hashd->htbl);
  free(file->hashd);
}

/**************************************************************************
 Returns the number of elements in a "vector".
 That is, returns the number of consecutive entries in the sequence:
 "path,0" "path,1", "path,2", ...
 If none, returns 0.
**************************************************************************/
int secfile_lookup_vec_dimen(struct section_file *my_section_file, 
			     char *path, ...)
{
  char buf[MAX_LEN_BUFFER];
  va_list ap;
  int j=0;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  while(section_file_lookup(my_section_file, "%s,%d", buf, j)) {
    j++;
  }
  return j;
}

/**************************************************************************
 Return a pointer for a list of integers for a "vector".
 The return value is malloced here, and should be freed by the user.
 The size of the returned list is returned in (*dimen).
 If the vector does not exist, returns NULL ands sets (*dimen) to 0.
**************************************************************************/
int *secfile_lookup_int_vec(struct section_file *my_section_file,
			    int *dimen, char *path, ...)
{
  char buf[MAX_LEN_BUFFER];
  va_list ap;
  int j, *res;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  *dimen = secfile_lookup_vec_dimen(my_section_file, buf);
  if (*dimen == 0) {
    return NULL;
  }
  res = fc_malloc((*dimen)*sizeof(int));
  for(j=0; j<(*dimen); j++) {
    res[j] = secfile_lookup_int(my_section_file, "%s,%d", buf, j);
  }
  return res;
}

/**************************************************************************
 Return a pointer for a list of "strings" for a "vector".
 The return value is malloced here, and should be freed by the user,
 but the strings themselves are contained in the sectionfile
 (as when a single string is looked up) and should not be altered
 or freed by the user.
 The size of the returned list is returned in (*dimen).
 If the vector does not exist, returns NULL ands sets (*dimen) to 0.
**************************************************************************/
char **secfile_lookup_str_vec(struct section_file *my_section_file,
			      int *dimen, char *path, ...)
{
  char buf[MAX_LEN_BUFFER];
  va_list ap;
  int j;
  char **res;

  va_start(ap, path);
  my_vsnprintf(buf, sizeof(buf), path, ap);
  va_end(ap);

  *dimen = secfile_lookup_vec_dimen(my_section_file, buf);
  if (*dimen == 0) {
    return NULL;
  }
  res = fc_malloc((*dimen)*sizeof(char*));
  for(j=0; j<(*dimen); j++) {
    res[j] = secfile_lookup_str(my_section_file, "%s,%d", buf, j);
  }
  return res;
}

/***************************************************************
 Copies a string and does '\n' -> newline translation.
 Other '\c' sequences (any character 'c') are just passed
 through with the '\' removed (eg, includes '\\', '\"')
 Backslash followed by a genuine newline removes the newline.
***************************************************************/
static char *minstrdup(struct sbuffer *sb, const char *str)
{
  char *dest = sbuf_malloc(sb, strlen(str)+1);
  char *d2=dest;
  if(dest != NULL) {
    while (*str) {
      if (*str=='\\') {
	str++;
	if (*str=='\\') {
	  *dest++='\\';
	  str++;
	} else if (*str=='n') {
	  *dest++='\n';
	  str++;
	} else if (*str=='\n') {
	  /* skip */
	  str++;
	}
      } else {
	*dest++=*str++;
      }

    }

    *dest='\0';
  }
  return d2;
}

/***************************************************************
 Returns a pointer to an internal buffer (can only get one
 string at a time) with str escaped the opposite of minstrdup.
 Specifically, any newline, backslash, or double quote is
 escaped with a backslash.
 The internal buffer is grown as necessary, and not normally
 freed (since this will be called frequently.)  A call with
 str=NULL frees the buffer and does nothing else (returns NULL).
***************************************************************/
static char *moutstr(char *str)
{
  static char *buf = NULL;
  static int nalloc = 0;

  int len;			/* required length, including terminator */
  char *c, *dest;

  if (str==NULL) {
    freelog(LOG_DEBUG, "moutstr alloc was %d", nalloc);
    free(buf);
    buf = NULL;
    nalloc = 0;
    return NULL;
  }
  
  len = strlen(str)+1;
  for(c=str; *c; c++) {
    if (*c == '\n' || *c == '\\' || *c == '\"') {
      len++;
    }
  }
  if (len > nalloc) {
    nalloc = 2 * len + 1;
    buf = fc_realloc(buf, nalloc);
  }
  
  dest = buf;
  while(*str) {
    if (*str == '\n' || *str == '\\' || *str == '\"') {
      *dest++ = '\\';
      if (*str == '\n') {
	*dest++ = 'n';
	str++;
      } else {
	*dest++ = *str++;
      }
    } else {
      *dest++ = *str++;
    }
  }
  *dest = '\0';
  return buf;
}

/***************************************************************
  Returns pointer to list of strings giving all section names
  which start with prefix, and sets the number of such sections
  in (*num).  If there are none such, returns NULL and sets
  (*num) to zero.  The returned pointer is malloced, and it is
  the responsibilty of the caller to free this pointer, but the
  actual strings pointed to are part of the section_file data,
  and should not be freed by the caller (nor used after the
  section_file has been freed or changed).  The section names
  returned are in the order they appeared in the original file.
***************************************************************/
char **secfile_get_secnames_prefix(struct section_file *my_section_file,
				   char *prefix, int *num)
{
  char **ret;
  int len, i;

  len = strlen(prefix);

  /* count 'em: */
  i = 0;
  section_list_iterate(*my_section_file->sections, psection) {
    if (strncmp(psection->name, prefix, len) == 0) {
      i++;
    }
  }
  section_list_iterate_end;
  (*num) = i;

  if (i==0) {
    return NULL;
  }
  
  ret = fc_malloc((*num)*sizeof(char*));

  i = 0;
  section_list_iterate(*my_section_file->sections, psection) {
    if (strncmp(psection->name, prefix, len) == 0) {
      ret[i++] = psection->name;
    }
  }
  section_list_iterate_end;
  return ret;
}
