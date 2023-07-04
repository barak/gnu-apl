
# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_X11()
{ {
apl_X11=no

local have_x                   # 'yes', 'no', or 'disabled' by AC_PATH_X()
local x_includes x_libraries   # also set by AC_PATH_X(), normally empty

{ AC_PATH_X() } ; apl_NYES($have_x) && return

AC_CHECK_HEADERS([xcb/xcb.h X11/Xlib.h X11/Xlib-xcb.h X11/Xutil.h], , return)

# xcb/xproto.h needs xcb/xcb.h
AC_CHECK_HEADER(xcb/xproto.h, [], [return], [#include <xcb/xcb.h>])

apl_OPT_LIB([X11], [XOpenDisplay], [])
apl_NO($apl_have_opt_lib) && return

apl_OPT_LIB([xcb], [xcb_connect], [])
apl_NO($apl_have_opt_lib) && return

apl_OPT_LIB([X11-xcb], [XGetXCBConnection], [])
apl_NO($apl_have_opt_lib) && return

apl_X11=yes
} }

# call dash_test_X11() above and set result variables...
#
dash_test_X11   # set apl_X11 to yes or no.

# export apl_X11 to config.h
if apl_YES($apl_X11); then
   AC_DEFINE_UNQUOTED(apl_X11, 1, [X11 is available ?])
else
   AC_DEFINE_UNQUOTED(apl_X11, 0)
fi

# export apl_X11 to Makefile.am
AM_CONDITIONAL(apl_X11, apl_YES($apl_X11))

