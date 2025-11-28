
# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
# The total result of this script is apl_X11 which tells if X11 and XCB
# are available or not.
#
# Unfortunately apl_X11 was kind of a misnomer and we therefore# migrate it
# to apl_XCB instead.
#
dash_test_X11()
{ {
apl_X11=no      # old name
apl_XCB=no      # new name

local have_x                   # 'yes', 'no', or 'disabled' by AC_PATH_X()
local x_includes x_libraries   # also set by AC_PATH_X(), normally empty

{ AC_PATH_X() } ; apl_NYES($have_x) && return

# check for header files (#include'd in Plot_xcb.cc)
AC_CHECK_HEADERS([ xcb/xcb.h
                   X11/Xlib.h
                   X11/Xlib-xcb.h
                   X11/Xutil.h], , return)

# xcb/xproto.h #include's xcb/xcb.h
AC_CHECK_HEADER(xcb/xproto.h, [], [return], [#include <xcb/xcb.h>])

apl_OPT_LIB([X11], [XOpenDisplay], [])  # sets apl_have_opt_lib
apl_NO($apl_have_opt_lib) && return

apl_OPT_LIB([xcb], [xcb_connect], [])   # sets apl_have_opt_lib
apl_NO($apl_have_opt_lib) && return

apl_OPT_LIB([X11-xcb], [XGetXCBConnection], [])
apl_NO($apl_have_opt_lib) && return # sets apl_have_opt_lib

# at this point all libraries needed for XCB are installed
apl_X11=yes
apl_XCB=yes
} }

# call dash_test_X11() above and set result variables...
#
dash_test_X11   # set apl_X11 to yes or no.

# export apl_X11 and apl_XCB to config.h
if apl_YES($apl_X11); then
   AC_DEFINE_UNQUOTED(apl_X11, 1, [X11 is available ?])
   AC_DEFINE_UNQUOTED(apl_XCB, 1, [XCB is available ?])
else
   AC_DEFINE_UNQUOTED(apl_X11, 0)
   AC_DEFINE_UNQUOTED(apl_XCB, 0)
fi

# export apl_X11 to Makefile.am
AM_CONDITIONAL(apl_X11, apl_YES($apl_X11))
AM_CONDITIONAL(apl_XCB, apl_YES($apl_XCB))

