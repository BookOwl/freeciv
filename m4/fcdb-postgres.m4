# Check for Freeciv Authentication using postgres
#
# Called without any parameters.

AC_DEFUN([FC_FCDB_POSTGRES],
[
  if test "x$fcdb_all" = "xyes" || test "x$fcdb_postgres" = "xyes" ; then

    FC_CHECK_POSTGRES(
    [
      FCDB_POSTGRES_CFLAGS="$POSTGRESQL_CFLAGS"
      FCDB_POSTGRES_LIBS="$POSTGRESQL_LDFLAGS"

      AC_SUBST(FCDB_POSTGRES_CFLAGS)
      AC_SUBST(FCDB_POSTGRES_LIBS)

      AC_DEFINE(HAVE_FCDB_POSTGRES,1,[ ])
      fcdb_postgres=yes
    ],
    [
      if test "x$fcdb_postgres" = "xyes" ; then
        AC_MSG_ERROR([fcdb database postgres not available])
      fi
      fcdb_postgres=no
    ])

  fi

  AM_CONDITIONAL(FCDB_POSTGRES, test "x$fcdb_postgres" = "xyes")
])
