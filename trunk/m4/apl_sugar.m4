#                                               -*- Autoconf -*-
# vim: et:ts=4:sw=4
#
#  This file (i.e. apl_sugar.m4) defines some shortcut macros that aim at
#  making some configre.ac constructs (primarily conditionals) ia little
#  more readable.
#

# m4 macro that prints the topic for subsequent ./configure tests
#
AC_DEFUN([apl_TOPIC],
         [ { AS_ECHO(); AS_ECHO(["    *** $1... ***"]); AS_ECHO(); } ])

# The m4 macros apl_EQ() and apl_NEQ() are safe string comparisons that,
# unlike the vanilla 'test A = B' and test A != B, also work for empty
# strings A and/or B.
#
AC_DEFUN([apl_EQ],  [test "x$1" = "x$2"])
AC_DEFUN([apl_NEQ], [test "x$1" != "x$2"])

# The m4 macros apl_NO(), apl_NNO, apl_YES(), and apl_NYES() convert autoconf
# yes/no results into binary 0/1 values that are suitable for 'if' conditions
# in dash. NOTE: On GNU/Linus is /usr/bin/sh a symlink to /usr/bin/dash so that
# dash the shell used by ./configure
#
AC_DEFUN([apl_NO],   [apl_EQ($1,  no)])
AC_DEFUN([apl_NNO],  [apl_NEQ($1, no)])
AC_DEFUN([apl_YES],  [apl_EQ($1,  yes)])
AC_DEFUN([apl_NYES], [apl_NEQ($1, yes)])

# check for an optional library...
#
# $1: library name (e/o the lib prefix). E.g. png for libpng
# $2: a function in that library
# $3: a comment to be printed if the library is not present
#
# CAUTION: It is important that AC_CHECK_LIB() takes its default action when
#          the library was found! This works only if  action-if-found is empty!
AC_DEFUN([apl_OPT_LIB],
         [ apl_have_opt_lib=yes
           AC_CHECK_LIB($1, $2, ,
         [test -n "$3" && AS_ECHO(["   └──── $3 ";]) apl_have_opt_lib=no])])

