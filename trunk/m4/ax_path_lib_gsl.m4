# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_path_lib_gsl.html
#     !!! modified for GNU APL needs: better use the original !!!
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PATH_LIB_GSL []
#
# DESCRIPTION
#
#   check for gsl lib and set GSL_LIBS and GSL_CFLAGS accordingly.
#
#   also provide --with-gsl option that may point to the $prefix of the
#   gsl installation - the macro will check $gsl/include and $gsl/lib to
#   contain the necessary files.
#
#   the usual two ACTION-IF-FOUND / ACTION-IF-NOT-FOUND are NOT supported and
#   they can NOT take advantage of the LIBS/CFLAGS additions.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <https://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

AC_DEFUN([AX_PATH_LIB_GSL],[dnl
AC_ARG_WITH(gsl,
            [  --with-gsl[[=prefix]]     enable ⎕MX (needs libgsl)],,
            with_gsl="yes")

case $with_gsl in
   "no")
         AC_MSG_RESULT([disabled by: --with-gsl=no])
         ;;
  "" | "yes")

       AC_CHECK_LIB([m],[cos])
       AC_CHECK_LIB([gslcblas],[cblas_dgemm])
       AC_CHECK_LIB([gsl],[gsl_blas_dgemm])
       if test "$ac_cv_lib_gsl_gsl_blas_dgemm" = "yes" ; then
          GSL_LIBS="-lgsl -lgslcblas"
       fi
       ;;
   *)
esac

AC_SUBST([GSL_LIBS])
AC_SUBST([GSL_CFLAGS])
])
