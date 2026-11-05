
bin_PROGRAMS = Gtk_server
noinst_DATA = Gtk_functions.adoc Gtk_events1.adoc Gtk_events2.adoc

Gtk_server_SOURCES = Gtk_server.cc Gtk_map.def Gtk_enum_map.def Gtk_enums.hh

AM_CXXFLAGS = $(GTK_CFLAGS) $(GTK_LIBS) -rdynamic
AM_LDFLAGS  = $(GTK_LDFLAGS) -lX11
if apl_DEVELOP
   AM_CXXFLAGS += $(WERROR) -Wall -Wno-strict-aliasing
endif

AM_MAKEFLAGS = -j $(MAKE_J)

Gtk_functions.adoc: Gtk_server Gtk_map.def
	./Gtk_server --funs > $@

Gtk_events1.adoc: Gtk_server Gtk_map.def
	./Gtk_server --ev1 > $@

Gtk_events2.adoc: Gtk_server Gtk_map.def
	./Gtk_server --ev2 > $@

