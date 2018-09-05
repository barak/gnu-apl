
bin_PROGRAMS = Gtk_server
noinst_DATA = Gtk_functions.asciidoc Gtk_events1.asciidoc Gtk_events2.asciidoc

Gtk_server_SOURCES = Gtk_server.cc Gtk_map.def Gtk_enums.hh

AM_CXXFLAGS = -rdynamic $(GTK_CFLAGS) $(GTK_LIBS) -lX11
if DEVELOP
   AM_CXXFLAGS += $(WERROR) -Wall -Wno-strict-aliasing
endif

AM_MAKEFLAGS = -j $(MAKE_J)

Gtk_functions.asciidoc: Gtk_server Gtk_map.def
	./Gtk_server --funs > $@

Gtk_events1.asciidoc: Gtk_server Gtk_map.def
	./Gtk_server --ev1 > $@

Gtk_events2.asciidoc: Gtk_server Gtk_map.def
	./Gtk_server --ev2 > $@

