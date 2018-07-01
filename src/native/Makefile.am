
AM_MAKEFLAGS = -j $(MAKE_J)

AM_CXXFLAGS = $(CXX_RDYNAMIC) -I $(srcdir)/.. -g -O2
if DEVELOP
   AM_CXXFLAGS += $(WERROR) -Wall -Wno-strict-aliasing
endif

pkglib_LTLIBRARIES =	lib_file_io.la		\
			lib_template_F0.la	\
			lib_template_F12.la	\
			lib_template_OP1.la	\
			lib_template_OP2.la	

# set common_LDFLAGS to the LDFLAGS needed by your platform. MacOS seems
# not to like one of the -module, -shared, or -export-dynamic options when
# asked to build dylibs.
#
common_LDFLAGS = -avoid-version
# common_LDFLAGS += -module
# common_LDFLAGS += -shared
# common_LDFLAGS += -export-dynamic

lib_file_io_la_SOURCES = file_io.cc
lib_file_io_la_CXXFLAGS = $(AM_CXXFLAGS)
lib_file_io_la_LDFLAGS = $(common_LDFLAGS)

lib_template_F0_la_SOURCES = template_F0.cc template.hh
lib_template_F0_la_CXXFLAGS = $(AM_CXXFLAGS)
lib_template_F0_la_LDFLAGS = $(common_LDFLAGS)

lib_template_F12_la_SOURCES = template_F12.cc template.hh
lib_template_F12_la_CXXFLAGS = $(AM_CXXFLAGS)
lib_template_F12_la_LDFLAGS = $(common_LDFLAGS)

lib_template_OP1_la_SOURCES = template_OP1.cc template.hh
lib_template_OP1_la_CXXFLAGS = $(AM_CXXFLAGS)
lib_template_OP1_la_LDFLAGS = $(common_LDFLAGS)

lib_template_OP2_la_SOURCES = template_OP2.cc template.hh
lib_template_OP2_la_CXXFLAGS = $(AM_CXXFLAGS)
lib_template_OP2_la_LDFLAGS = $(common_LDFLAGS)

