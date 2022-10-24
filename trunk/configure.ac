#                                               -*- Autoconf -*-
# vim: et:ts=4:sw=4
#
m4_ignore([

  For newcomers to autoconf (and those who keep forgetting the following).

  Process this file (i.e. 'configure.ac') with command autoconf (or, even
  better: with command autoreconf) as to produce the script 'configure'.
  The script 'configure' will then be processed with /bin/sh (which is a
  symlink to /bin/dash on GNU/Linux).

  A GNU APL designer typically does this:

  autoreconf
  ./configure
  make

  This requires that the autoconf and automake packages are installed. The
  command autoconf and autoreconf come with the autoconf package, but
  installing autoconf without automake makes little sense.

  A GNU APL user, however, needs neither autoconf nor automake, but simply does:

./configure
make
sudo make install

  The AM_CONDITIONAL(VAR, VALUE) macro exports the VALUE of variable VAR
  into all Makefile.am where it can be used like so:

if VAR
...
else
...
endif

  in any Makefile.am. It does NOT export VAR into any Makefile.in or Makefile.
  Instead it generates variables @VAR_TRUE@ and @VAR_FALSE@ in all
  Makefile.in, where one of them is empty and the other is # (making it
  a Makefile comment).

  The AC_SUBST(VAR) macro exports variable VAR into Makefile.am, Makefile.in,
  and finally into Makefile as a Makefile variable for various purposes.

  The AC_DEFINE_UNQUOTED() macro exports variable VAR into config.h, either
  as an uncommented #define (if VAR was defined beforehand in ./configure),
  or else as a commented #undef (if VAR was not defined). For example:

#define VAR value   # VAR has value in ./configure
/* #undef VAR */    # VAR has no value in ./configure

  Many files in directory src #include ../config.h (either directly or
  indirectly), which propagates the value of VAR into those source files.
  Like above,

  The following commands are useful to locate some or all of the macros above:

autoconf -t AM_CONDITIONAL
autoconf -t AC_SUBST
autoconf -t AC_DEFINE_UNQUOTED
autoconf -t AM_CONDITIONAL -t AC_SUBST -t AC_DEFINE_UNQUOTED

  A final NOTE: autoconf quotes with [ ] rather than ` '.
          ])

configure_options="$0 $*"

m4_include(m4/apl_sugar.m4)

AC_PREREQ([2.65])
AC_INIT([GNU APL], [1.8], [bug-apl@gnu.org])
AC_PREFIX_DEFAULT( /usr/local )
AC_CONFIG_AUX_DIR([.])

apl_TOPIC([core tests])

AC_USE_SYSTEM_EXTENSIONS

# always enable dependency tracking regardless of the user's choice.
# This avoids stale .o files after SVN updates. enable_dependency_tracking
# needs to be set early in order to be recognised!
#
# enable_dependency_tracking=no

AM_INIT_AUTOMAKE([subdir-objects])
AM_MAINTAINER_MODE([disable])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# use g++ for tests
#
AC_LANG(C++)

# initialize libtool in dlopen mode
#
LT_INIT([dlopen])

apl_MAINTAINER_MODE=$USE_MAINTAINER_MODE

apl_TOPIC([basic checks])

# Checks for programs.
#
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

AC_GNU_SOURCE

# add the current directory (i.e. from where ./configure was called) to the
# include path, so that the files created by ./configure are found. This
# is (only) needed for VPATH builds since otherwise the directory from which
# ./configure is run is the same as the directory where ./configure places
# its output files (i.e. directory '.').
#
CXXFLAGS="$CXXFLAGS -I $(pwd)"

##############################################################################
# Checks for various header files.
#

apl_TOPIC([checks for the presence and usablility of header files])

# check for ncurses.h first and for curses.h only if ncurses.h was not found.

AC_CHECK_HEADER(
    [ncurses.h],
    [AC_DEFINE_UNQUOTED( [HAVE_NCURSES_H], [1],
                         [Define to 1 iff <ncurses.h> is present])],
    [AC_CHECK_HEADER(
        [curses.h],
        [AC_DEFINE_UNQUOTED( [HAVE_CURSES_H], [1],
                             [Define to 1 iff <curses.h> is present])])
    ])

AC_CHECK_HEADER(
    [term.h],
    [AC_DEFINE_UNQUOTED( [HAVE_TERM_H], [1],
                         [Define to 1 iff <term.h> is present])],
    [],
    [ [ # if HAVE_NCURSES_H   ]
      [ # include <ncurses.h> ]
      [ # elif HAVE_CURSES_H  ]
      [ # include <curses.h>  ]
      [ # endif ]
    ])

apl_missing_headers=0
AC_CHECK_HEADERS([                              \
   dirent.h                                     \
   execinfo.h                                   \
   fcntl.h fenv.h fftw3.h                       \
   inttypes.h                                   \
   limits.h                                     \
   malloc.h                                     \
   netdb.h                                      \
   png.h                                        \
   stdint.h stdlib.h string.h strings.h         \
   termios.h                                    \
   unistd.h utmpx.h                             \
   zlib.h                                       \
   netinet/in.h                                 \
   sys/ioctl.h sys/socket.h sys/time.h sys/un.h \
   ext/atomicity.h                              \
   xcb/xcb.h                                    \
   X11/Xlib.h X11/Xlib-xcb.h X11/Xutil.h        \
                 ],,apl_missing_headers=$((1+$apl_missing_headers)))

# xcb/xproto.h needs xcb/xcb.h
AC_CHECK_HEADER(xcb/xproto.h, [], [], [#include <xcb/xcb.h>])

AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
#
apl_TOPIC([checks for standard C/C++ types])

AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# export the ./configure args into config.h
#
AC_DEFINE_UNQUOTED([CONFIGURE_ARGS],
                   "$0 $ac_configure_args",
                   [how ./configure was called])

##############################################################################
#
# Various checks for library functions. We use inverse alphabetical order
# although the order should be least-dependent to most-dependent (or vice
# versa?). The checks that come earlier in this file come later on the linker
# command line.
#
# That is, if e.g. lib_A requires lib_B, then the linker command line should
# be # ... lib_A ... lib_B and the test for lib_B should come before the test
# for lib_A in this file.
#
# Most libraries are not dependent and in that case we use the alphabetical
# order as to improve the reability of the linker command
#
##############################################################################

AC_CHECK_MEMBERS(stat.st_blksize stat.st_rdev)

AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_STRTOD
AC_FUNC_MMAP
apl_missing_functions=0
AC_CHECK_FUNCS([                                             \
   dup2                                                      \
   floor                                                     \
   getcwd gethostbyname gettimeofday                         \
   memset mkdir modf munmap                                  \
   pow                                                       \
   realpath rint rmdir                                       \
   select setenv setlocale socket sqrt strcasecmp strchr     \
   strdup strncasecmp strndup strerror strrchr strstr strtol \
   uname                                                     \
               ],,apl_missing_functions=$((1+$apl_missing_functions)))

AC_CHECK_LIB([tinfo],     [tparm])
AC_CHECK_LIB([socket],    [socket])   # for Solaris
AC_CHECK_LIB([pthread],   [sem_init])
AC_CHECK_LIB([nsl],       [yp_all])   # for Solaris
AC_CHECK_LIB([curses],    [setupterm])
AC_CHECK_LIB([ncurses],   [setupterm])
AC_CHECK_LIB([m],         [acosh])
AC_CHECK_LIB([execinfo],  [backtrace])
AC_CHECK_LIB([dl],        [dlopen])
AC_CHECK_LIB([c],         [initstate_r])
AC_CHECK_LIB([atomicity], [__atomic_fetch_add_4])

AC_STRUCT_ST_BLOCKS
AC_PATH_X

apl_TOPIC([checks for optional (not strictly required) libraries])

AC_DEFUN([OPT_LIB], [ AC_CHECK_LIB($1, $2, , [AS_ECHO(["   └──── $3 "]); ])])

#       library,   function-in library, impact-if-missing
OPT_LIB([cairo],   [cairo_fill],        [may affect: ⎕PLOT])
OPT_LIB([fftw3],   [fftw_plan_dft],     [will affect ⎕FFT])
   apl_FFT=$ac_cv_lib_fftw3_fftw_plan_dft
OPT_LIB([gtk-3],   [gtk_init],          [will affect: ⎕PNG])
OPT_LIB([gdk-3],   [gdk_init],          [may affect: ⎕PLOT, will affect: ⎕PNG])
OPT_LIB([png],     [png_init_io],       [may affect: ⎕PLOT, will affect: ⎕PNG])
   apl_PNG=$ac_cv_lib_png_png_init_io
OPT_LIB([X11-xcb], [XGetXCBConnection], [may affect: ⎕PLOT])
OPT_LIB([X11],     [XOpenDisplay],      [may affect: ⎕PLOT])
OPT_LIB([xcb],     [xcb_connect],       [may affect: ⎕PLOT])
OPT_LIB([z],       [uncompress],        [will affect: ⎕PNG])
m4_include([m4/check_SQLITE.m4])
if apl_NEQ($apl_SQLITE3, yes); then
   AS_ECHO(["   └──── may affect: ⎕SQL"]);
fi
m4_include([m4/check_POSTGRES.m4])
if apl_NYES($apl_POSTGRES); then
   AS_ECHO(["   └──── may affect: ⎕SQL"]);
fi
m4_include([m4/check_PCRE.m4])
if apl_NYES($apl_PCRE); then
   AS_ECHO(["   └──── may affect: ⎕RE"]);
fi
m4_include([m4/check_GTK.m4])
if apl_NYES($apl_GTK3); then
   AS_ECHO(["   └──── will affect: ⎕PLOT, ⎕PNG"]);
fi

apl_TOPIC([checks for functions that are missing on some platforms])

################################# rdtsc check ################################
# check if rdtsc (read CPU cycle counter is available.
# This is expected to succeed only on Intel CPUs
#
AC_MSG_CHECKING([whether CPU has rdtsc (read CPU cycle counter) opcode])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[]],
        [[unsigned int lo, hi;
          __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));]]
    )], [HAVE_rdtsc=yes], [HAVE_rdtsc=no])
if apl_YES($HAVE_rdtsc); then
   AC_DEFINE_UNQUOTED([HAVE_RDTSC], [1],
                      [Define to 1 iff the CPU has rdtsc opcode])
fi
AC_MSG_RESULT([$HAVE_rdtsc])

################################# gettimeofday check #########################
# check if gettimeofday() counter is available on the OS
#
AC_MSG_CHECKING([whether gettimeofday exists])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[#include <sys/time.h>]],
        [[timeval tv; gettimeofday(&tv, 0); ]]
    )], [HAVE_gettimeofday=yes], [HAVE_gettimeofday=no])
if apl_YES($HAVE_gettimeofday); then
   AC_DEFINE_UNQUOTED([HAVE_GETTIMEOFDAY], [1],
                      [Define to 1 iff the OS has gettimeofday()])
fi
AC_MSG_RESULT([$HAVE_gettimeofday])

############################### sem_init() check ##############################
# check if sem_init() works (it does not on OS-X)
#
apl_save_CXXFLAGS=$CXXFLAGS
CXXFLAGS="-Wall -Werror"

# This is expected only on Intel CPUs
AC_MSG_CHECKING([whether sem_init() works])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[#include <semaphore.h>]],
        [[sem_t sema;
          sem_init(&sema, 0, 0);
        ]]
    )], [HAVE_SEM_INIT=yes], [HAVE_SEM_INIT=no])
if apl_YES($HAVE_SEM_INIT); then
   AC_DEFINE_UNQUOTED([HAVE_SEM_INIT], [1], [Define to 1 iff sem_init() works])
fi
AM_CONDITIONAL(HAVE_SEM_INIT, apl_YES($HAVE_SEM_INIT))
CXXFLAGS=$apl_save_CXXFLAGS
AC_MSG_RESULT([$HAVE_SEM_INIT])


######################## pthread_getaffinity_np() check #######################
# see if pthread_getaffinity_np() and pthread_setaffinity_np are available
#
AC_MSG_CHECKING([whether pthread_setaffinity_np is available])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([ [#include <pthread.h>]
                       [#include <ext/atomicity.h>]
                     ],
                     [ [cpu_set_t cpuset;]
                       [pthread_setaffinity_np(0, sizeof(cpuset), &cpuset);]
                       [pthread_getaffinity_np(0, sizeof(cpuset), &cpuset);]
                       [_Atomic_word count = 0;]
                       [__gnu_cxx::__exchange_and_add_dispatch(&count, 1);]
                       [__gnu_cxx::__atomic_add_dispatch(&count, 1);]
                     ])],
    [HAVE_AFFINITY_NP=yes
     AC_DEFINE_UNQUOTED([HAVE_AFFINITY_NP], [1],
                        [Define to 1 iff pthread_setaffinity_np() is available])
    ],
    [HAVE_AFFINITY_NP=no])
AC_MSG_RESULT([$HAVE_AFFINITY_NP])

######################## pthread_setname_np() check #######################
# see if pthread_setname_np is available
#
AC_MSG_CHECKING([whether pthread_setname_np is available])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([ [#include <pthread.h>]
                     ],
                     [ [pthread_t thread = 0;]
                       [pthread_setname_np(thread, "thread_name");]
                     ])],
    [HAVE_PTHREAD_SETNAME_NP=yes
     AC_DEFINE_UNQUOTED([HAVE_PTHREAD_SETNAME_NP], [1],
                        [Define to 1 iff pthread_setname_np() is available])
    ],
    [HAVE_PTHREAD_SETNAME_NP=no])
AC_MSG_RESULT([$HAVE_PTHREAD_SETNAME_NP])

############################### atomicity checks ##############################
# check for alternatives for ext/atomicity.h on OS/X 
#
AC_MSG_CHECKING([whether OSAtomicAdd32Barrier() is available])
apl_save_CXXFLAGS=$CXXFLAGS
CXXFLAGS="-Wall -Werror"
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([ [#include <libkern/OSAtomic.h>] ],
                     [ [OSAtomicAdd32Barrier(0, 0);]   ])],
    [HAVE_OSX_ATOMIC=yes
     AC_DEFINE_UNQUOTED([HAVE_OSX_ATOMIC], [1],
                          [Define to 1 iff OSAtomicAdd32Barrier() is available])
    ],
    [HAVE_OSX_ATOMIC=no])
CXXFLAGS=$apl_save_CXXFLAGS
AC_MSG_RESULT([$HAVE_OSX_ATOMIC])


# check for alternatives for ext/atomicity.h on Solaris
#
AC_MSG_CHECKING([whether atomic_add_32_nv() is available])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([ [#include <atomic.h>] ],
                     [ [atomic_add_32_nv(0, 0);]
                       [atomic_add_32   (0, 0);]
                     ])],
    [HAVE_SOLARIS_ATOMIC=yes
     AC_DEFINE_UNQUOTED([HAVE_SOLARIS_ATOMIC], [1],
                        [Define to 1 iff atomic_add_32_nv() is available])
    ],
    [HAVE_SOLARIS_ATOMIC=no])
AC_MSG_RESULT([$HAVE_SOLARIS_ATOMIC])


# check if we are on i686 (to fix a strange rpmbuild problem)
AC_MSG_CHECKING([whether we compile for an i686 CPU])
if apl_EQ($(uname -m), i686); then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi


apl_TOPIC([choices made by the user (via ./configure arguments)])

# the standard convention is that ^D means EOF aka. end-of-file. Some users
# prefer ^D to delete one character and ^Z to mean EOF.
AC_MSG_CHECKING([if ctrl-D shall delete one char (and ctrl-Z be EOF)])
AC_ARG_WITH( [ctrld_del],
             [AS_HELP_STRING([--with-ctrld_del],
                             [enable to use ^D as del-char, ^Z as EOT])],
             [user_wants_ctrlD_del=$withval],
             [user_wants_ctrlD_del=no])

if apl_YES($user_wants_ctrlD_del); then
    AC_DEFINE([WANT_CTRLD_DEL], [1],
              [Define to make ^D a del-char and ^Z be EOF])
fi
AC_MSG_RESULT([$user_wants_ctrlD_del])

m4_include(m4/apl_TARGETs.m4)

AC_CONFIG_FILES([Makefile
                 build/Makefile
                 debian/Makefile
                 debian/source/Makefile
                 doc/Makefile
                 doc/DALY/Makefile
                 erlang/Makefile
                 erlang/apl.erl
                 gnu-apl.d/Makefile
                 gnu-apl.d/preferences
                 HOWTOs/Makefile
                 rpm/Makefile
                 rpm/apl.spec
                 src/Makefile
                 src/APs/Makefile
                 src/Gtk/Makefile
                 src/emacs_mode/Makefile
                 src/native/Makefile
                 src/sql/Makefile
                 src/testcases/Makefile
                 src/workspaces/Makefile
                 support-files/Makefile
                 support-files/105-key-International-Keyboard/Makefile
                 support-files/Bill_Daly_Ubuntu/Makefile
                 support-files/Dirk/Makefile
                 support-files/Dyalog-Keyboard/Makefile
                 support-files/Juergen-Mint-19/Makefile
                 support-files/old-Keyboard/Makefile
                 support-files/OS-X-Keyboard/Makefile
                 support-files/Unicomp-Keyboard/Makefile
                 support-files/WASD-Keyboard/Makefile
                 tools/Makefile
                 websock/Makefile
                 websock/client/Makefile
                 websock/server/Makefile
                 workspaces/Makefile
                 wslib3/Makefile
                 wslib3/DALY/Makefile
                 wslib4/Makefile
                 wslib5/Makefile
                 wslib5/APLComponentFiles/Makefile
                 wslib5/DALY/Makefile
                 wslib5/iso-apl-cf/Makefile
                ])

# AM_CXXFLAGS: do/dont -Werror ?
AC_ARG_VAR(CXX_WERROR,
   [ turn all warnings into errors (compiler flag -Werror) default: no) ])
if apl_YES($CXX_WERROR); then CXX_WERROR="-Werror";
                         else CXX_WERROR=""; fi

# APL: development configuration desired ? Set this before other xxx_WANTED
# options so that the implied xxx_WANTED options set here can be overridden
# further down
AC_ARG_VAR(DEVELOP_WANTED,
   [ set development options (see README-2-configure) default: no])
if apl_YES($DEVELOP_WANTED); then
   AC_DEFINE_UNQUOTED([DEVELOP_WANTED], [yes],
                      [Define to set options typical for software development])
   ASSERT_LEVEL_WANTED=2
   DYNAMIC_LOG_WANTED=yes
   PERFORMANCE_COUNTERS_WANTED=yes
   RATIONAL_NUMBERS_WANTED=yes
   VALUE_CHECK_WANTED=yes
   VALUE_HISTORY_WANTED=yes
fi
AM_CONDITIONAL([apl_DEVELOP],  [apl_YES($DEVELOP_WANTED)])


# APL: max. rank ?
AC_ARG_VAR(MAX_RANK_WANTED,
   [ the max. Rank of APL values (see README-2-configure) default: 8) ])
if apl_EQ($MAX_RANK_WANTED,); then MAX_RANK_WANTED=8; fi   # set default to 8
AC_DEFINE_UNQUOTED([MAX_RANK_WANTED], [$MAX_RANK_WANTED],
                   [max. rank of APL values])


# APL: CHECK() macro desired ?
AC_ARG_VAR(VALUE_CHECK_WANTED,
   [ enable CHECK() macro (see README-2-configure) default: no ])
if apl_YES($VALUE_CHECK_WANTED); then
   AC_DEFINE_UNQUOTED([VALUE_CHECK_WANTED], [yes],
                      [ Define to enable CHECK macro ])
fi


# APL: performance counters desired ?
AC_MSG_CHECKING([whether internal performance counters shall be enabled)])
AC_ARG_VAR(PERFORMANCE_COUNTERS_WANTED,
   [ enable performance counters (see README-2-configure) default: no ])
if apl_YES($PERFORMANCE_COUNTERS_WANTED); then
   AC_DEFINE_UNQUOTED([PERFORMANCE_COUNTERS_WANTED], [yes],
                   [ Define to enable performance counters])
else PERFORMANCE_COUNTERS_WANTED="no"
fi
AC_MSG_RESULT([$PERFORMANCE_COUNTERS_WANTED])


# APL: ADD_EVENT() macro and friends desired ?
AC_MSG_CHECKING([whether to remember the history of APL values (debug feature)])
AC_ARG_VAR(VALUE_HISTORY_WANTED,
   [ enable value events (a debug facility, see README-2-configure) default: no ])
if apl_YES($VALUE_HISTORY_WANTED); then
   AC_DEFINE_UNQUOTED([VALUE_HISTORY_WANTED], [yes],
                      [ Define to enable value events])
else VALUE_HISTORY_WANTED="no"
fi
AC_MSG_RESULT([$VALUE_HISTORY_WANTED])


# APL: gprof profiling ?
AC_MSG_CHECKING([whether gprof profiling shall be enabled])
AC_ARG_VAR(GPROF_WANTED,
   [ enable gprof (-pg) (see README-2-configure) default: no ])
if apl_YES($GPROF_WANTED); then
   GPROF_WANTED="-pg"
   AC_MSG_RESULT([yes])
else
   GPROF_WANTED=
   AC_MSG_RESULT([no])
fi
AC_DEFINE_UNQUOTED([GPROF_WANTED], [-pg],
                   [ Define to enable gprof profiling ])


# APL: gcov coverage testing ?
AC_MSG_CHECKING([whether gcov coverage testing shall be enabled])
AC_ARG_VAR(GCOV_WANTED,
   [ enable gcov (-fprofile-arcs and -ftest-coverage) (see README-2-configure) default: no ])
if apl_YES($GCOV_WANTED); then
   GCOV_WANTED="--coverage"
   AC_MSG_RESULT([yes])
else
   GCOV_WANTED=
   AC_MSG_RESULT([no])
fi


# APL: dynamic logging desired ?
AC_MSG_CHECKING([whether dynamic logging shall be enabled])
AC_ARG_VAR(DYNAMIC_LOG_WANTED,
   [ enable dynamic logging (see README-2-configure) default: no ])
if apl_YES($DYNAMIC_LOG_WANTED); then
AC_DEFINE_UNQUOTED([DYNAMIC_LOG_WANTED], [yes],
                   [ Define to enable dynamic logging ])
else DYNAMIC_LOG_WANTED="no"
fi
AC_MSG_RESULT([$DYNAMIC_LOG_WANTED])


# APL: tracing of value flags (produces lots of messages) desired ?
AC_MSG_CHECKING([whether value flags shall be traced])
AC_ARG_VAR(VF_TRACING_WANTED,
   [ enable tracing of value flags (see README-2-configure) default: no ])
if apl_YES($VF_TRACING_WANTED); then
   AC_DEFINE_UNQUOTED([VF_TRACING_WANTED], [yes],
                      [ Define to enable tracing of value flags ])
else VF_TRACING_WANTED="no"
fi
AC_MSG_RESULT([$VF_TRACING_WANTED])


# APL: desired assert level ?
AC_MSG_CHECKING([how picky (0=not, 1=somwhat, 2=very) internal tests shall be])
AC_ARG_VAR(ASSERT_LEVEL_WANTED,
   [ Assert level (see README-2-configure) default: 1) ])
if apl_EQ($ASSERT_LEVEL_WANTED, ); then ASSERT_LEVEL_WANTED=1; fi   # default: 1
AC_DEFINE_UNQUOTED([ASSERT_LEVEL_WANTED], [$ASSERT_LEVEL_WANTED],
                   [desired ASSERT() level])
AC_MSG_RESULT([$ASSERT_LEVEL_WANTED])


# APL: desired security level ?
AC_MSG_CHECKING([security level (0=none, 1=configured in preferences, 2=all)])
AC_ARG_VAR(SECURITY_LEVEL_WANTED,
   [ Assert level (see README-2-configure) default: 0) ])
if apl_EQ($SECURITY_LEVEL_WANTED, ); then SECURITY_LEVEL_WANTED=0; fi
AC_DEFINE_UNQUOTED([SECURITY_LEVEL_WANTED], [$SECURITY_LEVEL_WANTED],
                   [security level])
AC_MSG_RESULT([$SECURITY_LEVEL_WANTED])


# APL: number of cores ?
AC_MSG_CHECKING([how many CPU cores shall be used by APL (experimental!)])
AC_ARG_VAR(CORE_COUNT_WANTED,
           [ Core count (see README-2-configure) default: 0) ])
  if apl_EQ($CORE_COUNT_WANTED,     ); then CORE_COUNT_WANTED=0;
elif apl_EQ($CORE_COUNT_WANTED, all ); then CORE_COUNT_WANTED=-1;
elif apl_EQ($CORE_COUNT_WANTED, ALL ); then CORE_COUNT_WANTED=-1;
elif apl_EQ($CORE_COUNT_WANTED, argv); then CORE_COUNT_WANTED=-2;
elif apl_EQ($CORE_COUNT_WANTED, ARGV); then CORE_COUNT_WANTED=-2;
elif apl_EQ($CORE_COUNT_WANTED, syl ); then CORE_COUNT_WANTED=-3;
elif apl_EQ($CORE_COUNT_WANTED, SYL ); then CORE_COUNT_WANTED=-3;
  fi
AC_DEFINE_UNQUOTED([CORE_COUNT_WANTED], [$CORE_COUNT_WANTED],
                   [core count])
AC_MSG_RESULT([$CORE_COUNT_WANTED])


# APL: APserver transport?
AC_MSG_CHECKING([which type of socket APserver shall use])
AC_ARG_VAR(APSERVER_TRANSPORT,
   [ APserver transport (see README-2-configure) default: TCP) ])
  if apl_EQ($APSERVER_TRANSPORT,      );   then APSERVER_TRANSPORT=0;
elif apl_EQ($APSERVER_TRANSPORT, TCP  );   then APSERVER_TRANSPORT=0;
elif apl_EQ($APSERVER_TRANSPORT, LINUX);   then APSERVER_TRANSPORT=1;
elif apl_EQ($APSERVER_TRANSPORT, UNIX );   then APSERVER_TRANSPORT=2;
  fi
AC_DEFINE_UNQUOTED([APSERVER_TRANSPORT], [$APSERVER_TRANSPORT],
                   [APserver transport: TCP=0, LINUX=1, UNIX=2])
AC_MSG_RESULT([$APSERVER_TRANSPORT])


# APL: APserver TCP port number?
AC_MSG_CHECKING([which port number APserver shall use for TCP or UDP sockets])
AC_ARG_VAR(APSERVER_PORT,
   [ APserver transport (see README-2-configure) default: 16366) ])
if apl_EQ($APSERVER_PORT, );   then APSERVER_PORT=16366;  fi
AC_DEFINE_UNQUOTED([APSERVER_PORT], [$APSERVER_PORT],
                   [APserver TCP listen port])
AC_MSG_RESULT([$APSERVER_PORT])


# APL: APserver port name ?
AC_MSG_CHECKING([which unix port name APserver shall use])
AC_ARG_VAR(APSERVER_PATH,
   [ APserver transport (see README-2-configure) default: /tmp/GNU-APL/APserver) ])
if apl_EQ($APSERVER_PATH, );   then APSERVER_PATH="/tmp/GNU-APL/APserver";  fi
AC_DEFINE_UNQUOTED([APSERVER_PATH], ["$APSERVER_PATH"],
                   [APserver listen port name])
AC_MSG_RESULT([$APSERVER_PATH])


# APL: desired cellcount of short values ?
AC_MSG_CHECKING([how long a short APL value shall be at most])
AC_ARG_VAR(SHORT_VALUE_LENGTH_WANTED,
    [ Cellcount of short values (see README-2-configure) default: 12) ])
if apl_EQ($SHORT_VALUE_LENGTH_WANTED, ); then SHORT_VALUE_LENGTH_WANTED=12; fi
AC_DEFINE_UNQUOTED([SHORT_VALUE_LENGTH_WANTED], [$SHORT_VALUE_LENGTH_WANTED],
                   [short value cellcount])
AC_MSG_RESULT([$SHORT_VALUE_LENGTH_WANTED])


# APL: visible markers (debug only option) ?
AC_MSG_CHECKING([whether visible markers shall be used])
AC_ARG_VAR(VISIBLE_MARKERS_WANTED,
   [ enable visible formatting markers (debug facility, see README-2-configure) default: no ])
if apl_YES($VISIBLE_MARKERS_WANTED); then
AC_DEFINE_UNQUOTED([VISIBLE_MARKERS_WANTED], [yes],
                   [ Define to have visible markers (DONT!)])
else VISIBLE_MARKERS_WANTED="no"
fi
AC_MSG_RESULT([$VISIBLE_MARKERS_WANTED])


# APL: support for rational numbers?
AC_MSG_CHECKING([whether support for rational numbers is desired ])
AC_ARG_VAR(RATIONAL_NUMBERS_WANTED,
   [ enable support for rational numbers (see README-2-configure) default: no ])
if apl_YES($RATIONAL_NUMBERS_WANTED); then
AC_DEFINE_UNQUOTED([RATIONAL_NUMBERS_WANTED], [yes],
                   [ Define to have support for rational numbers (EXPERIMENTAL!)])
else RATIONAL_NUMBERS_WANTED="no"
fi
AC_MSG_RESULT([$RATIONAL_NUMBERS_WANTED])


apl_TOPIC([./configure checks done, producing the output files])

AC_OUTPUT

AS_ECHO()

# create buildtag.hh and set apl_top to the directory that contains buildtag.sh
#
AS_ECHO(["configure: creating buildtag.hh..."])
abs_apl_top="not-found"   # assume not found
apl_top=$(pwd)            # start search at current directory
for q in 1 2 3 4; do      # at most 4 times...
   if test -f "$apl_top/buildtag.sh"    # does buildtag.sh exist in $apl_top ?
   then                                 # yes...
      . "$apl_top/buildtag.sh"              # source buildtag.sh
      abs_apl_top="$apl_top"                # and remember the directory
      break                                 # done.
   else                                 # no...
      apl_top=$(dirname $apl_top)           # move upwards towards /
   fi
done

if apl_EQ($abs_apl_top, not-found); then
   AS_ECHO_N(["configure: *** could not find top-level GNU APL directory,"])
   AS_ECHO(  [" using '.' ***"])
   apl_top="."
else
   AS_ECHO(["configure: top-level apl directory is: $abs_apl_top"])
fi

# remove old makefile.h
#
if test -f "$apl_top/makefile.h"
then
   AS_ECHO(["
configure: removing old $apl_top/makefile.h (if any).
           NOTE: ./Makefile will create a fresh makefile.h automatically.
           However, running 'make' in directory $apl_top/src
           BEFORE running 'make' in $apl_top may yield a
           compile error (which can be fixed easily by running 'make'
           in directory $apl_top first)."])
   rm -f "$apl_top/makefile.h"
fi

# print a summary or maybe complain
#
AS_ECHO([" "])
if apl_YES($want1_postgresql); then
   if apl_YES($apl_POSTGRES); then
      AS_ECHO(["      PostgreSQL looks good."])
   else
      AS_ECHO(["NOTE: PostgreSQL support was requested, but the handler function"])
      AS_ECHO(["      for accessing PostgreSQL cannot be built. Your PostgreSQL"])
      AS_ECHO(["      installation is missing or incomplete. See details above."])
      AS_ECHO([" "])
   fi
fi

if apl_YES($WANT1_SQLITE3); then
   if apl_YES($apl_SQLITE3); then
      AS_ECHO(["      SQLite looks good."])
   else
      AS_ECHO(["NOTE: SQLite support was requested, but the handler function for "])
      AS_ECHO(["      accessing SQLite cannot not be built. Your SQLite"])
      AS_ECHO(["      installation is missing or incomplete. See details above."])
      AS_ECHO([" "])
   fi
fi

AS_ECHO(["
configure:    ---- SUMMARY OF TESTS (please include in error reports) ----

    apl_MAINTAINER_MODE:   $apl_MAINTAINER_MODE
    apl_missing_headers:   $apl_missing_headers
    apl_missing_functions: $apl_missing_functions
    apl_FFT:               $apl_FFT		(affects: ⎕FFT)
    apl_GTK3:              $apl_GTK3		(affects: ⎕PLOT, ⎕PNG, and ⎕GTK)
    apl_PCRE:              $apl_PCRE		(affects: ⎕RE)
    apl_PNG:               $apl_PNG		(affects: ⎕PNG)
    apl_POSTGRES:          $apl_POSTGRES		(affects: ⎕SQL)
    apl_SQLITE3:           $apl_SQLITE3		(affects: ⎕SQL)
"])
