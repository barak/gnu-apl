
# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_POSTGRES()
{ {
local found_postgresql   # 'yes' or 'no' by AX_LIB_POSTGRESQL()

apl_POSTGRES=no

m4_include([m4/ax_lib_postgresql.m4])   # define AX_LIB_POSTGRESQL()
AX_LIB_POSTGRESQL([])                   # call AX_LIB_POSTGRESQL()
    #
    # ./configure --with-postgresql="no"   →    $with_postgresql: "no"
    # ./configure --without-postgresql     →    $with_postgresql: "no"
    # ./configure                          →    $with_postgresql: "yes"
    # ./configure --with-postgresql        →    $with_postgresql: "yes"
    # ./configure --with-postgresql=yes    →    $with_postgresql: "yes"
    # ./configure --with-postgresql=path   →    $with_postgresql: "path"
    #
apl_NO($with_postgresql) && return   # the user rejects POSTSCRIPT

if apl_YES($postgresql_given); then
   AC_DEFINE_UNQUOTED(cfg_USER_WANTS_POSTGRES, 1,
                      [./configure with --with-postgresql])
fi

apl_NYES($found_postgresql) && return   # no POSTSCRIPT

    # POSTGRES is present. Check if it is usable (which is the final verdict).
    #
AC_MSG_CHECKING([for PostgreSQL usability])
   saved_CPPFLAGS="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS $POSTGRESQL_CFLAGS"
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[@%:@include <libpq-fe.h>]],
                     [[PGconn *db = PQconnectdbParams(0, 0, 1);]])],
                     [apl_POSTGRES=yes],
                     [apl_POSTGRES=no])

   CPPFLAGS="$saved_CPPFLAGS"
AC_MSG_RESULT($apl_POSTGRES)

} }
dash_test_POSTGRES   # set apl_POSTGRES to yes or no.

# export apl_POSTGRES to config.h
if apl_YES($apl_POSTGRES); then
   AC_DEFINE_UNQUOTED(apl_POSTGRES, 1, [PostgreSQL code did compile])
else
   AC_DEFINE_UNQUOTED(apl_POSTGRES, 0, [PostgreSQL code did not compile])
fi


# export apl_POSTGRES to Makefile.am
AM_CONDITIONAL([apl_POSTGRES], [apl_YES($apl_POSTGRES)])

