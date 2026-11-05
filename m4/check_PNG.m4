
# checks related to ⎕PNG. Final verdict is: apl_PNG=yes or apl_PNG=no.

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_PNG()
{ {
local apl_have_opt_lib
apl_PNG=no

AC_ARG_WITH([png],
    AS_HELP_STRING([--with-png],
    [enable ⎕PNG (needs libpng and libz)]))
    #
    # ./configure --with-png="no"   →    $with_png: "no"
    # ./configure --without-png     →    $with_png: "no"
    # ./configure                   →    $with_png: "yes"
    # ./configure --with-png        →    $with_png: "yes"
    # ./configure --with-png=yes    →    $with_png: "yes"
    #
if apl_NO($with_png); then       # user has explicitly disabled PNG
   AC_MSG_CHECKING([for PNG])
   AC_MSG_RESULT([no - (explicitly disabled by user)])
   return
fi

AC_CHECK_HEADER([png.h], , return)
AC_CHECK_HEADER([zlib.h], , return)

apl_OPT_LIB([z], [uncompress], [will affect: ⎕PNG])
   apl_NYES($apl_have_opt_lib) && return        # libz missing

apl_OPT_LIB([png], [png_init_io], [will affect: ⎕PNG])
   apl_NYES($apl_have_opt_lib) && return        # libpng missing

apl_PNG=yes                                     # success
} }
dash_test_PNG   # set apl_PNG to yes or no.

