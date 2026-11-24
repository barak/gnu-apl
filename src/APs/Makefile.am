
bin_PROGRAMS = AP100 AP210 APserver

common_files =	APmain.cc APmain.hh			\
                ../Backtrace.cc ../Backtrace.hh		\
		../CDR_string.hh ../Svar_signals.hh	\
		../Svar_DB.cc ../Svar_DB.hh		\
		../Svar_record.cc ../Svar_record.hh

AP100_SOURCES = AP100.cc $(common_files)
AP210_SOURCES = AP210.cc $(common_files)

APserver_SOURCES = APserver.cc				\
		Svar_DB_server.cc Svar_DB_server.hh	\
		../Svar_record.cc ../Svar_record.hh

# installation directories, expanded at make time (see configure.ac)
AM_CPPFLAGS = -Dapl_DIR__bin='"$(bindir)"'             \
              -Dapl_DIR__doc='"$(docdir)"'             \
              -Dapl_DIR__sysconf='"$(sysconfdir)"'     \
              -Dapl_DIR__pkglib='"$(pkglibdir)"'       \
              -Dapl_DIR__pkgdata='"$(pkgdatadir)"'

AM_CXXFLAGS = $(CXX_RDYNAMIC) -I $(srcdir)/.. -g -O2
if apl_DEVELOP
   AM_CXXFLAGS += $(WERROR) -Wall -Wno-strict-aliasing
endif

AP100_CXXFLAGS    = $(AM_CXXFLAGS) -DAP_NUM=100
AP210_CXXFLAGS    = $(AM_CXXFLAGS) -DAP_NUM=210
APmain_CXXFLAGS   = $(AM_CXXFLAGS)
APserver_CXXFLAGS = $(AM_CXXFLAGS)

if apl_TARGET_MINGW
AM_LDFLAGS = -L/usr/x86_64-w64-mingw32/lib -lws2_32
endif

AM_MAKEFLAGS = -j $(MAKE_J)

