
/// GTK function numbers: 1, 2, ...
enum Fnum
{
   FNUM_INVALID = -1,
   FNUM_0 = 0,
#define gtk_event_def(ev_name, ...)
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   FNUM_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Rx tags (commands) for function numbers: 1001, 1002, ...
enum Gtk_Command_Tag
{
   Command_0 = 1000,
#define gtk_event_def(ev_name, ...)
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   Command_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Tx tags (command responses) for function numbers: 2001, 2002, ...
enum Gtk_Response_Tag
{
   Response_0 = 2000,
#define gtk_event_def(ev_name, ...)
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   Response_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

enum Event_tag
{
   Event_widget = 3000,   // widget argument of the callback
   Event_fun,             // function (callback) name
};

