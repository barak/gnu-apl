if WANT_ERLANG

AM_CFLAGS = $(CXX_RDYNAMIC) -g -O2
if DEVELOP
   AM_CFLAGS += $(WERROR) -Wall -Wno-strict-aliasing
endif   # if DEVELOP

pkglib_LTLIBRARIES = liberlang_APL_nif.la
liberlang_APL_nif_la_SOURCES = erlang_APL_nif.c README apl_example.erl
EXTRA_liberlang_APL_nif_la_SOURCES = apl.erl apl_example.erl
liberlang_APL_nif_la_CFLAGS = $(AM_CFLAGS) -I $(abs_top_srcdir) -fpic  -shared
liberlang_APL_nif_la_CFLAGS += -I $(pkgincludedir)  # e.g. /usr/local/include/apl
liberlang_APL_nif_la_CFLAGS += -I $(pkgincludedir)/..  # e.g. /usr/local/include
liberlang_APL_nif_la_LDFLAGS = -avoid-version -export-dynamic -E

# for some reason, using libtool causes Erlang to try to load the file
# erlang_APL_nif.so.so instead of erlang_APL_nif.so. We therefore do not
# use libtool but create the make targets manually. This seems to work on
# Ubuntu but may require manual fixes on other platforms.
#
FILES = apl.beam apl_example.beam

CLEANFILES = ${FILES}

all: apl.beam apl_example.beam
.erl.beam:
	erlc $<

else   # not WANT_ERLANG

.PHONY:	all
all:
	@echo "nothing to be done because GNU APL was NOT configured --with-erlang"

endif   # WANT_ERLANG

