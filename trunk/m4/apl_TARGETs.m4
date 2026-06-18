#
# vim: et:ts=4:sw=4
#
# This file (i.e. apl_TARGETs.m4) checks for different non-standard
# build targets (Android, Erlang, libapl, Python) that can be selected
# by command line options for ./configure
#

###############################################################################
# check if the user compiles for ANDROID
#
AC_MSG_CHECKING([if the build target is Android])
AC_ARG_WITH( [android],
             [AS_HELP_STRING([--with-android],
                             [enable when compiling for Android])],
             [apl_TARGET_ANDROID=$withval],
             [apl_TARGET_ANDROID=no])

if apl_YES($apl_TARGET_ANDROID); then
   AC_DEFINE([apl_TARGET_ANDROID], [1], [Define if compiling for Android])
fi
AM_CONDITIONAL(apl_TARGET_ANDROID, apl_YES($apl_TARGET_ANDROID))
AC_MSG_RESULT([$apl_TARGET_ANDROID])

###############################################################################
# check if the user wants the Erlang interface (and therefore also libapl)
#
AC_MSG_CHECKING(
   [if the build target is the Erlang interface (also requires libapl.so)])
AC_ARG_WITH( [erlang],
             [AS_HELP_STRING([--with-erlang],
                             [enable to build the Erlang interface])],
             [apl_TARGET_ERLANG=$withval],
             [apl_TARGET_ERLANG=no])


if apl_YES($apl_TARGET_ERLANG); then
   AC_DEFINE([apl_TARGET_ERLANG], [1], [Define if building the Erlang interface])
fi

AM_CONDITIONAL(apl_TARGET_ERLANG, apl_YES($apl_TARGET_ERLANG))
AC_MSG_RESULT([$apl_TARGET_ERLANG])


###############################################################################
# check if the user wants to build libapl.so
#
AC_MSG_CHECKING([if the build target is libapl.so])
apl_TARGET_LIBAPL=$apl_TARGET_ERLANG   # Erlang needs libapl
AC_ARG_WITH( [libapl],
             [AS_HELP_STRING([--with-libapl],
                             [enable to build libapl.so])],
             [apl_TARGET_LIBAPL=$withval])

if apl_YES($apl_TARGET_LIBAPL); then
   AC_DEFINE([apl_TARGET_LIBAPL], [1], [Define if building libapl.so])
fi

AM_CONDITIONAL(apl_TARGET_LIBAPL, apl_YES($apl_TARGET_LIBAPL))
AC_MSG_RESULT([$apl_TARGET_LIBAPL])


###############################################################################
# check if the user wants to build the python extension libpython_apl.so
#
AC_MSG_CHECKING([if the build target is libpython_apl.so])
apl_TARGET_PYTHON=no
AC_SUBST(PYTHON_CFLAGS)
PYTHON_CFLAGS=
AC_ARG_WITH( [python],
             [AS_HELP_STRING([--with-python],
             [enable to build python extension lib_gnu_apl.so])],
             [apl_TARGET_PYTHON=$withval])

if apl_YES($apl_TARGET_PYTHON); then
   AC_DEFINE([apl_TARGET_PYTHON], [1], [Define if building lib_gnu_apl.so])
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)  # locate pkg-config
    if apl_NNO($PKG_CONFIG); then             # we have pkg-config
       if $PKG_CONFIG --exists python3; then   # and pkg-config knows python3
            PYTHON_CFLAGS=$( $PKG_CONFIG --cflags python3 )
       fi
   fi
fi
AM_CONDITIONAL(apl_TARGET_PYTHON, apl_YES($apl_TARGET_PYTHON))
AC_MSG_RESULT([$apl_TARGET_PYTHON])


###############################################################################
# check if the user wants to cross-compile for Windows (MinGW-w64)
#
# This section runs early (before LT_INIT) so that setting host_alias here
# is seen by AC_CANONICAL_HOST (called inside LT_INIT).  AM_CONDITIONAL and
# AC_DEFINE for MINGW_SRC live in configure.ac's case $host_os block because
# $host_os is only available after AC_CANONICAL_HOST.
#
AC_MSG_CHECKING([if the build target is Windows (MinGW-w64 cross-compilation)])
AC_ARG_WITH([mingw],
            [AS_HELP_STRING([--with-mingw],
                            [cross-compile for Windows using MinGW-w64])],
            [apl_TARGET_MINGW=$withval],
            [apl_TARGET_MINGW=no])

if apl_YES($apl_TARGET_MINGW); then
   # Set cross-compilation defaults; override with environment variables if needed.
   : ${host_alias:=x86_64-w64-mingw32}
   : ${CC:=x86_64-w64-mingw32-gcc}
   : ${CXX:=x86_64-w64-mingw32-g++}
   : ${LDFLAGS:=-L/usr/x86_64-w64-mingw32/lib -lws2_32 -static-libgcc -static-libstdc++ -lwinpthread}
   : ${with_sqlite3:=no}
   : ${with_postgresql:=no}
   # Cross-compilation: AC_FUNC_MALLOC/AC_FUNC_REALLOC cannot run the test
   # binary, so they default to "broken" and emit #define malloc rpl_malloc
   # in config.h.  That macro then corrupts c++/stdlib.h's "using std::malloc"
   # into "using std::rpl_malloc" (undeclared).  Cache the result to tell
   # autoconf that Windows CRT malloc/realloc are standard-compliant.
   : ${ac_cv_func_malloc_0_nonnull=yes}
   : ${ac_cv_func_realloc_0_nonnull=yes}
   # GCC 10-win32: various Windows headers include <stdlib.h> in C++ mode;
   # GCC intercepts that with c++/stdlib.h which pulls in <cstdlib>; if
   # ::malloc is not yet declared at that point the build fails.  Force-
   # including <cstdlib> first ensures ::malloc and std::malloc are both
   # declared before any Windows header triggers c++/stdlib.h.
   CXXFLAGS="-include cstdlib $CXXFLAGS"
fi
AC_MSG_RESULT([$apl_TARGET_MINGW])

###############################################################################
