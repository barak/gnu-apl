
/// GTK function numbers: 1, 2, ...
enum Fnum
{
   FNUM_INVALID = -1,
   FNUM_0 = 0,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _Z,_A) \
   FNUM_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Rx tags (commands) for function numbers: 1001, 1002, ...
enum Gtk_Command_Tag
{
   Command_0 = 1000,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _Z,_A) \
   Command_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// Tx tags (command responses) for function numbers: 2001, 2002, ...
enum Gtk_Response_Tag
{
   Response_0 = 2000,
#define gtk_fun_def(_glade_ID, gtk_class, gtk_function, _Z,_A) \
   Response_ ## gtk_class ## _ ## gtk_function,

#include "Gtk_map.def"
};

/// tags specifying events (
enum Event_tag
{
   Event_0 = 3000,
   Event_widget_fun,             ///< H:widget:fun
   Event_widget_fun_id_class,    ///< H:widget:fun:glade_id:name
   Event_toplevel_window_done,   ///< H:Done
   Event_widget_ev_X_Y_B_L,      ///< H:widget:fun:mouse-X:mouse-Y:button:line
};

