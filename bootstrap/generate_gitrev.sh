#!/bin/sh

# Freeciv - Copyright (C) 2007 - The Freeciv Project

# This script generates fc_gitrev_gen.h from fc_gitrev_gen.h.tmpl.
# See fc_gitrev_gen.h.tmpl for details.

# Parameters - $1 - top srcdir
#              $2 - top builddir
#

# Absolete paths
SRCROOT="$(cd "$1" ; pwd)"
INPUTDIR="$(cd "$1/bootstrap" ; pwd)"
OUTPUTDIR="$(cd "$2/common" ; pwd)"

REVSTATE="OFF"
REV="dist"

(cd "$INPUTDIR"
 # Check that all commands required by this script are available
 # If not, we will not claim to know which git revision this is
 # (REVSTATE will be OFF)
 if which git && which tail && which wc ; then
   REVTMP="$(git rev-parse HEAD 2>/dev/null)"
   if test "x$REVTMP" != "x" ; then
     # This is git repository. Check for local modifications
     if test $(cd "$SRCROOT" ; git diff | wc -l) -eq 0 ; then
       REVSTATE=ON
       REV="$REVTMP"
     else
       REVSTATE=MOD
       REV="modified $REVTMP"
     fi
   fi
 fi

 sed -e "s,<GITREV>,$REV," -e "s,<GITREVSTATE>,$REVSTATE," fc_gitrev_gen.h.tmpl > "$OUTPUTDIR/fc_gitrev_gen.h.tmp"
 if ! test -f "$OUTPUTDIR/fc_fitrev_gen.h" ||
    ! cmp "$OUTPUTDIR/fc_gitrev_gen.h" "$OUTPUTDIR/fc_gitrev_gen.h.tmp"
 then
   mv "$OUTPUTDIR/fc_gitrev_gen.h.tmp" "$OUTPUTDIR/fc_gitrev_gen.h"
 fi
 rm -f "$OUTPUTDIR/fc_gitrev_gen.h.tmp"
) > /dev/null
