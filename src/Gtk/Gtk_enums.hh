
/// GTK function numbers: 1, 2, ...
enum Fnum
{
   FNUM_INVALID = -1,
   FNUM_0 = 0,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   FNUM_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Rx tags (commands) for function numbers: 1001, 1002, ...
enum Gtk_Command_Tag
{
   Command_0 = 1000,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   Command_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Tx tags (command responses) for function numbers: 2001, 2002, ...
enum Gtk_Response_Tag
{
   Response_0 = 2000,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _ZAname,_Z,_A,_help) \
   Response_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// tags specifying events
enum Event_tag
{
   Event_widget_fun = 3000,      ///< H:widget:callback
   Event_widget_fun_id_class,    ///< H:widget:callback:glade_id:name
   Event_toplevel_window_done,   ///< H:Done
};

