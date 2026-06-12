
# check if SQLite3 is wanted and installed...

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_SQLITE3()
{ {
apl_SQLITE3=no   # assume neither wanted nor installed

# no point to proceed without sqlite3.h

m4_include([m4/ax_lib_sqlite3.m4])   # define AX_LIB_SQLITE3()
AX_LIB_SQLITE3([])                   # call AX_LIB_SQLITE3()
    #
    # ./configure --with-sqlite3="no"   →    $withval: "no"
    # ./configure --without-sqlite3     →    $withval: "no"
    # ./configure                       →    $withval: "yes"
    # ./configure --with-sqlite3        →    $withval: "yes"
    # ./configure --with-sqlite3=yes    →    $withval: "yes"
    # ./configure --with-sqlite3=path   →    $withval: "path"
    #
apl_NO($with_sqlite3) && return   # the user rejects sqlite3

apl_SQLITE3=$found_sqlite   # set to yes/no in m4/ax_lib_sqlite3.m4
   if apl_YES($sqlite_given); then
      AC_DEFINE_UNQUOTED(cfg_USER_WANTS_SQLITE3, 1,
                         [./configure with --with-sqlite3])
   fi
} }
dash_test_SQLITE3   # set apl_SQLITE3 to yes or no.


# export apl_SQLITE3 to config.h
if apl_YES($apl_SQLITE3); then
   AC_DEFINE_UNQUOTED(apl_SQLITE3, 1, [SQLite code did compile])
else
   AC_DEFINE_UNQUOTED(apl_SQLITE3, 0, [SQLite code did not compile])
fi


# export apl_SQLITE3 to Makefile.am
AM_CONDITIONAL(apl_SQLITE3, [apl_YES($apl_SQLITE3)])

