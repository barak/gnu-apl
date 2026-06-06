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
