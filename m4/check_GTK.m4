
# check if libgtk-3 and related libraries are installed and usable

# wrap multiple tests into a single dash function from which we may
# return prematurely (which m4 can't) if a sub-test fails...
#
dash_test_GTK()
{ {
local apl_have_opt_lib   # result of apl_OPT_LIB()
apl_GTK3=no   # assume error

AC_ARG_WITH([gtk3],
    AS_HELP_STRING([--with-gtk3],
    [enable ⎕GTK (interface to GTK+ version 3; needs libgtk-3 and more)]))
    #
    # ./configure --with-gtk3="no"   →    $with_gtk3: "no"
    # ./configure --without-gtk3     →    $with_gtk3: "no"
    # ./configure                    →    $with_gtk3: "yes"
    # ./configure --with-gtk3        →    $with_gtk3: "yes"
    # ./configure --with-gtk3=yes    →    $with_gtk3: "yes"
    # ./configure --with-gtk3=path   →    $with_gtk3: "path"
    #
if apl_NO($with_gtk3); then       # user has explicitly disabled GTK
   AC_MSG_CHECKING([for GTK])
   AC_MSG_RESULT([no - (explicitly disabled by user)])
   return
fi

   # user allows GTK...

   # check for pkg-config
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)   # locate pkg-config
if apl_NO($PKG_CONFIG); then               # pkg-config missing
   AC_MSG_CHECKING([for pkg-config (GTK prerequisite)])
   AC_MSG_RESULT([no])
   return
fi

   # GTK needs libgtk-3
apl_OPT_LIB([gtk-3], [gtk_init], [])
   apl_NO($apl_have_opt_lib) && return     # libgtk-3 missing

   # GTK needs libgdk-3
apl_OPT_LIB([gdk-3], [gdk_init], [])
   apl_NYES($apl_have_opt_lib) && return   # libgdk-3 missing

   # GTK needs libcairo
apl_OPT_LIB([cairo], [cairo_fill], [])
   apl_NO($apl_have_opt_lib) && return     # libcairo missing

AC_SUBST(GTK_LDFLAGS)
AC_SUBST(GTK_CFLAGS)
GTK_LDFLAGS=
GTK_CFLAGS=
AC_MSG_CHECKING([for GTK (≥ version 3)])

if $PKG_CONFIG --exists gtk+-3.0; then         # package gtk+-3.0  exists
   AC_MSG_RESULT([yes])
   GTK_CFLAGS=`$PKG_CONFIG --cflags gtk+-3.0`
   GTK_LDFLAGS=`$PKG_CONFIG --libs gtk+-3.0`
   apl_GTK3=yes
else                                     # package gtk+-3.0 missing
      AC_MSG_RESULT([no (says pkg-config)])
fi

} }

# call dash_test_GTK() above and set result variables...
#
dash_test_GTK   # set apl_GTK3 to yes or no.

# export apl_GTK3 to config.h
#
if apl_YES($apl_GTK3); then
   AC_DEFINE_UNQUOTED(apl_GTK3, 1, [GTK+ version 3 installed ?])
else
   AC_DEFINE_UNQUOTED(apl_GTK3, 0)
fi


# export apl_GTK3 to Makefile.am
AM_CONDITIONAL(apl_GTK3, apl_YES($apl_GTK3))

