#!apl --script

      ⎕PW←5000          ⍝ avoid APL line wrapping
      COMMENT←{0 0⍴⍵}   ⍝ ignore ⍵ (multi-line comment)
      DBG←{0 0⍴⍵}       ⍝ debug print OFF
      DBG←{⍵}           ⍝ debug print ON

      )MORE AUTO ON     ⍝ automaticall display )MORE information

     COMMENT «««

This workspace contains functions for a simple, single-table SQL database
and a corrsponding GUI for maintaining the records into the database.

The GUI consist of 3 windows:

1. the Main window ∆gui_M with 5 buttons for choosing one of the 5 top-level
   workspace functions:

   1a. ADD a new database record,
   1b. MODIFY an existing database record,
   1c. DELETE an existing database record,
   1d. SHOW all database records, and
   1e. EXIT the program.

2. the Selection window ∆gui_S for displaying and/or selecting a database
   record in top-level functions 1b., 1c., and 1d..

3. a data entry window ∆gui_T1 (or gui_T2, ... for more tables) for every
   database table to enter a new database record (in 1a.) or to change
   an already existing database record (in 1b.)

These windows are sequenced like this:

                             ╔═══════╗
      ┌──────────────────────╢   1.  ║
      │                      ╚═══╤═══╝
      │                          │
      │      ┌─────────┬─────────┼─────────┬─────────┐
      │      │         │         │         │         │
      │     1a.       1b.       1c.        1d.      1e.
      │      │         │         │         │         │
      │      │      ╔══╧══╗   ╔══╧══╗   ╔══╧══╗      │
      │      │      ║  2. ║   ║  2. ║   ║  2. ║      │
      │      │      ╚══╤══╝   ╚══╤══╝   ╚══╤══╝      │
      │      │         │         │         │         │
      │   ╔══╧══╗   ╔══╧══╗   ╔══╧══╗      │         │
      │   ║  3. ║   ║  3. ║   ║  3. ║      │         │
      │   ╚══╤══╝   ╚══╤══╝   ╚══╤══╝      │         │
      │      │         │         │         │         │
      └──────┴─────────┴─────────┴─────────┘       DONE.

It can be easily adapted to a database with different (or multiple) database
tables.

Some general notes concerning GTK (aka. Gimp ToolKit):

1. A GTK GUI window contains a collection of widgets.
2. A widget can be identified by its ID or by its NAME (-property).
2a. The ID, as in <object ... id=ID> id used by Gtk_server, while
2b. The NAME, as in <property name="name">NAME</property> is used by CSS.
3a. The ID is optional, but is needed if Gtk_server shall call widget-specific
    functions. In that case the ID shall start with a lowercase Gtk class
    identification, followed by some character other than lowecase a-z.
3b. The NAME is optional, but is needed if CSS uses it, e.g. #NAME { ... }
4.  The GTK CSS differs slightly from regular CSS. For example use
    button instead of .button
             »»»

      COMMENT «««
      ┌──────────────────────────────────────────────────────────────────┐
      │ 0. FUNCTIONS...                                                  │
      └──────────────────────────────────────────────────────────────────┘
              »»»

∇Z←Text2APL B   ⍝ ------------------------------------------------------
 ⍝
 ⍝ B is a vector of strings (typically from a multi-line string ««« ... »»»).
 ⍝ Z is the corresponding APL value, with empty lines (comments) removed.
 ⍝
 Z←{⍎'⍬',⊃⍵}¨B    ⍝ text B to APL Z, prepending a ⍬ marker
 Z←(,⊃0≠⍴¨Z)/Z    ⍝ remove empty lines (for example: comments)
 Z←0 1↓⊃Z         ⍝ expand and discard the ⍬ marker prepended above
∇

∇Z←Cols2Text B   ⍝ -----------------------------------------------------
 ⍝ B is a vector of strings (column names).
 ⍝ Z is a single string with the column names, separated by ','.
 Z←¯1↓⊃,/{⍵,','}¨B
∇

∇DB←CREATE_DB B   ⍝ ----------------------------------------------------
 ⍝
 ⍝⍝ create a database from a database definition
 ⍝
 ⍝ B is a database definition. Create a database according to B and open it.
 ⍝ DB is an integer ⎕SQL database handle for subsequent ⎕SQL operations
 ⍝
 DB←B.provider ⎕SQL[1] B.filename
 "DATABASE CREATED: Handle" DB
 DB ADD_TABLE¨B.tables   ⍝ add all tables
∇

∇Z←CLOSE_DB DB   ⍝ -----------------------------------------------------
 ⍝
 ⍝⍝ close the open database with handle DB
 ⍝
 ⎕SQL[2] DB
 Z←"Database" DB "CLOSED"
∇

∇SQL←FLD_EQ_VAL B;FLD;TYPE;VAL   ⍝ -------------------------------------
 ⍝
 ⍝⍝ convert the strings FLD, TYPE, and VAL into SQL term "FLD = VAL,"
 ⍝
 (FLD TYPE VAL)←B
 SQL ← FLD, "=", (TYPE GTK2SQL VAL), ","
∇

∇SQL←TYPE GTK2SQL VAL   ⍝ ----------------------------------------------
 ⍝
 ⍝⍝ convert string VAL to SQL format. Quote strings and set empty numbers to 0
 ⍝
 ⍝ GTK entries always return (unquoted) strings, for both numbers and for text.
 ⍝ SQL expects that the values: for string columns are quoted, and for number
 ⍝ columns are present (non-empty). This function does that conversion.
 ⍝
 →("CHAR" ≢ 4↑TYPE)/NUMBER
 SQL←"'", VAL, "'" ◊ →0                            ⍝ quote string VAL, or
NUMBER: SQL←"0" ◊ →(0=⍴VAL)/0                      ⍝ set empty VAL to 0, or
 SQL←VAL                                           ⍝ leave VAL as is
∇

∇DB ADD_TABLE B;SQL;PARAMS;⎕IO   ⍝ -------------------------------------
 ⍝
 ⍝⍝ add the table defined by B to database (-handle) DB
 ⍝
 PARAMS←""
 "ADD TABLE" B.name "TO DATABASE (-handle)" DB

 SQL← ⊂ "CREATE TABLE IF NOT EXISTS ", B.name, "("
 ⊣ {SQL←SQL, ⊂ "    ", (⊃⍵[1]), "  ", (⊃⍵[2]), ","}¨⊂[2]B.columns[;7 8]
 SQL←SQL, ⊂"    ROWID INTEGER PRIMARY KEY ASC);"
 SQL←36 ⎕CR SQL
 ⊣ SQL ⎕SQL[3, DB] PARAMS
∇

∇DB ADD_RECORD DATA;COLS;CT;F;QM;V;SQL;⎕IO   ⍝ -------------------------
 ⍝
 ⍝⍝ add DATA to database (-handle) DB
 ⍝
 COLS←∆TABLE_1.columns[;7]         ⍝ Column names
 CT←∆TABLE_1.columns[;8]           ⍝ Column types
 F←¯1↓⊃,/{(⊃⍵),","} ¨ COLS         ⍝ field names
 QM←¯1↓(2×⍴COLS)⍴"?,"              ⍝ comma separated question marks
 DATA←CT GTK2SQL¨DATA              ⍝ fix strings and missing numbers
 V←¯1↓⊃,/{(⊃⍵),","} ¨ DATA         ⍝ comma separated values

 SQL← "INSERT INTO ", ∆TABLE_1.name, "(", F, ") VALUES(", V, ")"
 SQL ⎕SQL[3, DB] V
∇

∇DB DELETE_RECORD ROWID;⎕IO;SQL   ⍝ ------------------------------------
 DBG "DELETE: ROWID" ROWID
 SQL←"DELETE FROM ", ∆TABLE_1.name, " WHERE ROWID = ", ROWID, ";"
 SQL ⎕SQL[3, DB] ""
∇

∇DB MODIFY_RECORD DATA;⎕IO;SQL;GTK;ROWID;COLS;CHANGED   ⍝ --------------
 ROWID←⍕⊃↑DATA ◊ DATA←⍕¨1↓DATA
 DBG "MODIFY: ROWID" ROWID

 COLS←∆TABLE_1.columns[;7]         ⍝ Column names

 ⍝ display current record data and let the user change it.
 ⍝
 PRESET←COLS,⍪DATA
 CHANGED←Data_input PRESET                   ⍝ get user data, wait for OK/CANCEL
 →(CHANGED≡⍬) / 0                            ⍝ CANCEL button pressed

 ⍝ update the database
 ⍝
 SQL←"UPDATE ", ∆TABLE_1.name, " SET "
 SQL←SQL,¯1↓⊃,/ FLD_EQ_VAL ¨ ⊂[2] ∆TABLE_1.columns[;7 8], CHANGED
 SQL←SQL, " WHERE ROWID=", ROWID, ";"

 SQL ⎕SQL[4, DB] ""
∇

∇Z←IND_NAME PROPERTY_XML VALUE;IND;NAME   ⍝ ----------------------------
 ⍝
 ⍝⍝ return: string <property name="NAME">VALUE</property>, indented by IND.
 ⍝
 (IND NAME)←IND_NAME ◊ IND←(2×IND)⍴' '
 Z←⊂ (IND, '<property name="'), NAME, '">', VALUE, '</property>'
∇

∇Z←LABEL_XML B;⎕IO;X;Y;TXT;UN   ⍝ --------------------------------------
 ⍝
 ⍝⍝ return the UI XML for a GtkLabel
 ⍝
 (X Y TXT UN)←B
 Z←"""
        <child>
          <object class="GtkLabel">
        @3 -name-
            <property name="width_request">-1</property>
            <property name="height_request">20</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="halign">end</property>
        @9 -label-
          </object>
          <packing>
        @12 -x-
        @13 -y-
          </packing>
        </child>
       """
  Z[ 3] ← (6 "name")  PROPERTY_XML  "Li", UN
  Z[ 9] ← (6 "label") PROPERTY_XML  TXT
  Z[12] ← (6 "x")     PROPERTY_XML  ⍕X
  Z[13] ← (6 "y")     PROPERTY_XML  ⍕Y
∇

∇Z←ENTRY_XML B;⎕IO;X;Y;SIZE;UN   ⍝ -------------------------------------
 ⍝
 ⍝⍝ return the UI XML for a GtkEntry
 ⍝
 ⍝ NOTE: id= must start with "entry" so that Gtk_server understands it.
 (X Y SIZE UN)←B
 Z←"""
        <child>
          @2 -id-
          @3 -name-
          @4 -width_chars-
          <property name="height_request">20</property>
          <property name="visible">True</property>
          <property name="can_focus">True</property>
          </object>
          <packing>
            @10 -x-
            @11 -y-
          </packing>
        </child>

   """
  Z[ 2] ← ⊂ '          <object class="GtkEntry" id="entry-', UN, '">'
  Z[ 3] ← (6 "name")         PROPERTY_XML  "Li", UN
  Z[ 4] ← (6 "width_chars")  PROPERTY_XML  ⍕SIZE
  Z[10] ← (6 "x")            PROPERTY_XML  ⍕X
  Z[11] ← (6 "y")            PROPERTY_XML  ⍕Y
∇

∇Z←BUTTON_XML B;⎕IO;X;Y;SIZE;TXT;UN   ⍝ --------------------------------
 (X Y SIZE TXT UN)←B
 Z←"""
        <child>
          @2 -id-
            @3 -name-
            @4 -label-
            <property name="visible">True</property>
            <property name="can_focus">True</property>
            <property name="has_focus">True</property>
            <property name="receives_default">True</property>
        @9
            <property name="height_request">40</property>
            <signal name="clicked" handler="clicked"/>
          </object>
          <packing>
        @14 -x-
        @15 -y-
          </packing>
        </child>
   """
  Z[ 2] ← ⊂ '          <object class="GtkButton" id="Bi-', UN, '">'
  Z[ 3] ← (6 "name")           PROPERTY_XML  "Bn-", UN   ⍝ for _CSS
  Z[ 4] ← (6 "label")          PROPERTY_XML  TXT
  Z[ 9] ← (6 "width_request")  PROPERTY_XML  ⍕SIZE
  Z[14] ← (6 "x")              PROPERTY_XML  ⍕X
  Z[15] ← (6 "y")              PROPERTY_XML  ⍕Y
∇

∇Z←GET_INPUT_XML B;X;Y;TXT;dX;dY;SIZE;UN;LABEL;ENTRY   ⍝ ---------------
 ⍝
 ⍝ B defines a GTK label at (X/Y/TXT) and a GTK entry (X/size)
 ⍝ Z is the XML GUI description for both of them
 ⍝
 (X Y TXT dX dY SIZE UN)←B
 LABEL ← LABEL_XML  X Y TXT UN
 ENTRY ← ENTRY_XML (X + dX) (Y - dY) SIZE UN
 Z←LABEL, ENTRY
∇

∇Z←∆CSS SMD  ⍝ ---------------------------------------------------------
 ⍝
 ⍝⍝ return the CSS for all GUIs as a string
 ⍝
 Z←«««
window { background: #FFF; }

/* all widgets of style class button */
button       { color: white; background-color: blue }


button       { border-radius: 15px;  border: 2px solid black; }
button:hover { font-weight: bold;    }

/* the green OK button */
#Bn-OK           { color: #000; background-color: #3E3; }   /* button */
#Bn-OK label     { color: #000; background-color: #3E3; }   /* button label */

/* the red CANCEL button */
#Bn-CANCEL       { color: #FFF; background-color: #F44; }   /* button */
#Bn-CANCEL label { color: #FFF; background-color: #F44; }   /* button label */

/* the green add NEW record button */
#Bn-NEW          { color: #000; background-color: #3E3; }   /* button */
#Bn-NEW label    { color: #000; background-color: #3E3; }   /* button label */

/* the yellow MODIFY new record button */
#Bn-MODIFY       { color: #000; background-color: #FF0; }   /* button */
#Bn-MODIFY label { color: #000; background-color: #FF0; }   /* button label */

/* the red DELETE record button */
#Bn-DELETE       { color: #FFF; background-color: #E44; }   /* button */
#Bn-DELETE label { color: #FFF; background-color: #E44; }   /* button label */

/* the green SHOW records button */
#Bn-SHOW         { color: #000; background-color: #3E3; }   /* button */
#Bn-SHOW label   { color: #000; background-color: #3E3; }   /* button label */

/* the red EXIT program button */
#Bn-EXIT         { color: #FFF; background-color: #E44; }   /* button */
#Bn-EXIT label   { color: #FFF; background-color: #E44; }   /* button label */
   »»»

 ⍝ add some window-specific CSS...
 ⍝
 →(SMD='SMD1')/SHOW, MODIFY, DELETE, DONE ◊ ???
SHOW: Z←Z, «««
#text_view-MOUSE text           { color: #000; background-color: #CFC; }
#text_view-MOUSE text selection { color: #F00; background-color: #8F8; }
           »»»
   →DONE

MODIFY: Z←Z, «««
#text_view-MOUSE text           { color: #000; background-color: #FFC; }
#text_view-MOUSE text selection { color: #000; background-color: #FF4; }
           »»»
   →DONE

DELETE: Z←Z, «««
#text_view-MOUSE text           { color: #000; background-color: #FCC; }
#text_view-MOUSE text selection { color: #FFF; background-color: #F88; }
           »»»

DONE: Z←36 ⎕CR Z ◊ →0
∇

∇XML←guiM_XML;⎕IO;SoF;B1;B2;B3;B4;B5;EoF   ⍝ ----------------
 ⍝
 ⍝⍝ return the XML (string) for the main window (function buttons)
 ⍝

 ⍝ SoF: Start-of-File and Window size
 SoF←"""
<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated with GNU APL -->
<interface>
  <requires lib="gtk+" version="3.20"/>
  <object class="GtkWindow" id="window1">
    <property name="name">Window-M</property>
    @7 -title-
    @8 -width_request-
    @9 -height_request-
    <property name="can_focus">False</property>
    <child>
      <object class="GtkFixed">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
    """
  SoF[7] ← (6 "title")           PROPERTY_XML  "SELECT A FUNCTION"
  SoF[8] ← (6 "width_request")   PROPERTY_XML  ⍕∆gui_M.window_width
  SoF[9] ← (6 "height_request")  PROPERTY_XML  ⍕∆gui_M.window_height


 ⍝ XML for the buttons and End-of-File
 ⍝              ┌────────────────────────────────────────── X (pixels)
 ⍝              │   ┌────────────────────────────────────── Y (pixels)
 ⍝              │   │   ┌────────────────────────────────── width
 ⍝              │   │   │  ┌─────────────────────────────── label
 ⍝              │   │   │  │                        ┌────── ID/NAME suffix
 B1←BUTTON_XML 20  20 200 "ADD NEW RECORD"         "NEW"
 B2←BUTTON_XML 20  70 200 "MODIFY EXISTING RECORD" "MODIFY"
 B3←BUTTON_XML 20 120 200 "DELETE NEW RECORD"      "DELETE"
 B4←BUTTON_XML 20 170 200 "SHOW RECORDS"           "SHOW"
 B5←BUTTON_XML 50 260 140 "EXIT PROGRAM"           "EXIT"
 EoF←"""
      </object>
    </child>
  </object>
</interface>
    """

 ⍝ the entire UI
 XML←36 ⎕CR SoF, B1, B2, B3, B4, B5, EoF
∇

∇XML←gui1_XML;⎕IO;SoF;Inp;B1;B2;EoF   ⍝ -----------------------------
 ⍝
 ⍝⍝ return the XML for the ∆TABLE_1 data entry window
 ⍝
 ⍝ SoF: Start-of-File and Window size
 SoF←"""
<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated with GNU APL -->
<interface>
  <requires lib="gtk+" version="3.20"/>
  <object class="GtkWindow" id="window1">
    <property name="name">Window-1</property>
    @7 -title-
    @8 -width_request-
    @9 -height_request-
    <property name="can_focus">False</property>
    <child>
      <object class="GtkFixed">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
    """
  SoF[7] ← (6 "title")           PROPERTY_XML  "Data Entry (new record)"
  SoF[8] ← (6 "width_request")   PROPERTY_XML  ⍕∆gui_T1.window_width
  SoF[9] ← (6 "height_request")  PROPERTY_XML  ⍕∆gui_T1.window_height


 ⍝ Inp: XML for the input fields
 ⍝
 Inp← ,⊃ GET_INPUT_XML ¨ ⊂[2]∆gui_T1.fields

 ⍝ XML for the buttons and End-of-File
 B1←BUTTON_XML 220 (∆gui_T1.window_height - 50) 80 "OK"     "OK"
 B2←BUTTON_XML 420 (∆gui_T1.window_height - 50) 80 "Cancel" "CANCEL"
 EoF←"""
      </object>
    </child>
  </object>
</interface>
    """

 ⍝ the entire UI
 XML←36 ⎕CR SoF, Inp, B1, B2, EoF
∇

COMMENT «««
 ┌────────────────────────────── CSS LAZYDOG ────────────────────────────────┐
 │                                                                           │
 │ #mywidget   ←→   "name"  PROPERTY_XML "mywidget"                          │
 │ .mystyle    ←→   style class 'mystyle'                                    │
 │  window     ←→  Element "window" (CSS node of GtkWindow, e.g.             │
 │                 developer-old.gnome.org/gtk4/stable/GtkWindow.html        │
 │                                                                           │
 │ docs.gtk.org/gtk4/css-overview.html                                       │
 │ docs.gtk.org/gtk4/css-properties.html                                     │
 │ docs.gtk.org/gtk4/#classes                                                │
 │                                                                           │
 │ NOTE: We don't use gtk4,(but its documentation sometimes provides more    │
 │       insights than that of gtk3                                          │
 └───────────────────────────────────────────────────────────────────────────┘
        »»»

∇XML←SMD guiS_XML B;⎕IO;SoF;View;B1;B2;EoF   ⍝ -------------------------
 ⍝
 ⍝⍝ return the XML for the record selection window.
 ⍝ SMD (aka. SHOW/MODIFY/DELETE) controls minor details.
 ⍝

 ⍝ SoF: Start-of-File and Window size
 SoF←"""
<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated with GNU APL -->
<interface>
  <requires lib="gtk+" version="3.20"/>
  <object class="GtkWindow" id="window1">
    <property name="name">Window-S</property>
    @7 -title-
    @8 -width_request-
    @9 -height_request-
    <property name="can_focus">False</property>
    <child>
      <object class="GtkFixed">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
    """
  SoF[8] ← (2 "width_request")   PROPERTY_XML  ⍕∆gui_S.window_width
  SoF[9] ← (2 "height_request")  PROPERTY_XML  ⍕∆gui_S.window_height


 View← «««
        <child>
          <object class="GtkScrolledWindow">
            <property name="name">text_scroll_RECORDS</property>
            @4 -- width_request  --
            @5 -- height_request --
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="vscrollbar_policy">always</property>
            <child>
              <object class="GtkTextView" id="text_view-MOUSE">
                <property name="name">text_view-MOUSE</property>
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="hscroll_policy">natural</property>
                <property name="vscroll_policy">natural</property>
                <property name="monospace">True</property>
                <property name="editable">False</property>
                <signal name="button-press-event" handler="mouse_press"/>
              </object>
            </child>
          </object>
          <packing>
            <property name="x">40</property>
            <property name="y">20</property>
          </packing>
        </child>
         »»»

  View[4] ← (6 "width_request")  PROPERTY_XML ⍕∆gui_S.textview_width
  View[5] ← (6 "height_request") PROPERTY_XML ⍕∆gui_S.textview_height

 ⍝ XML for the OK and CANCEL buttons and End-of-File
 B1←BUTTON_XML  40 (∆gui_S.window_height - 50) 80 "OK"     "OK"
 B2←BUTTON_XML 220 (∆gui_S.window_height - 50) 80 "Cancel" "CANCEL"
 EoF←"""
      </object>
    </child>
  </object>
</interface>
    """

 ⍝ the entire UI. This function actually returns one of 3 sligtly different
 ⍝ UIs (determined by SMD).
 ⍝
 →(SMD='SMD')/SHOW, MODIFY, DELETE ◊ ???

SHOW:
 SoF[7] ← (2 "title")   PROPERTY_XML  "Show Database Records"
 XML←36 ⎕CR SoF, View, B1, EoF        ⍝ without CANCEL button
 →0

MODIFY:
 SoF[7] ← (2 "title")   PROPERTY_XML  "Change a Database Record"
 XML←36 ⎕CR SoF, View, B1, B2, EoF    ⍝ with CANCEL button
 →0

DELETE:
 SoF[7] ← (2 "title")   PROPERTY_XML  "Delete a Database Record"
 XML←36 ⎕CR SoF, View, B1, B2, EoF    ⍝ with CANCEL button
 →0
∇


∇Z←Main_window;GTK;EVENT   ⍝ -------------------------------------------
 ⍝
 ⍝⍝ display the ∆gui_M window and process user events from GTK
 ⍝
 GTK← (∆CSS 'M') ⎕GTK guiM_XML               ⍝ display the main GUI
LOOP:
 EVENT←⎕GTK 1                                ⍝ wait for user action
 → ("clicked" ≡ ⊃EVENT[3])/GUI_BUTTON
 → LOOP

GUI_BUTTON:
 → ("Bn-" ≡ 3↑⊃EVENT[2]) / Bn_CLICKED
 → LOOP

Bn_CLICKED:
 ⊣ GTK ⎕GTK 0                                ⍝ close the GUI window
 Z←3↓⊃EVENT[2]   ⍝
 DBG "@Main_window:" Z "button."
∇

∇Z←FLD SET_ENTRY[GTK] TEXT   ⍝ -----------------------------------------
 ⍝
 ⍝⍝ in the GUI with handle GTK: set the GtkEntry named entry-FLD to TEXT.
 ⍝
 Z←TEXT ⎕GTK[GTK, "entry-", FLD] "set_text"
∇

∇Z←Data_input PRESET;GTK;EVENT;EV;NAME;TYPE;ID;CLASS;Data   ⍝ ---------
 ⍝
 ⍝⍝ display the ∆gui_T1 window and process user events from GTK
 ⍝
 GTK ← (∆CSS '1') ⎕GTK gui1_XML                ⍝ display the ∆gui_T1

 ⊣ PRESET[;1] SET_ENTRY[GTK] ¨ PRESET[;2]      ⍝ preset some fields in ∆gui_T1
LOOP:
 EVENT←⎕GTK 1                                  ⍝ wait for user action
 → ("clicked" ≡ ⊃EVENT[3])/GUI_BUTTON
 → LOOP

GUI_BUTTON: DBG '@GUI_BUTTON'
 → (EVENT[2] ≡¨ "Bn-OK" "Bn-CANCEL")/OK, CANCEL
 → LOOP

OK: DBG 'The user has clicked the OK button.'
 Z←,{ 1↓⎕GTK[GTK, "entry-", ⍵] "get_text" } ¨ ∆gui_T1.field_names
 ⊣ GTK ⎕GTK 0                                  ⍝ close the gui1 window
 →0

CANCEL: DBG 'The user has clicked the CANCEL button. Discard her input.'
 ⊣ GTK ⎕GTK 0                                  ⍝ close the gui1 window
 Z←⍬
 →0
∇

∇SMD Select_window DB;⎕IO;GTK;EVENT;COLS;SQL;DATA;TEXT;LINE   ⍝ ------
 ⍝
 ⍝⍝ display the ∆gui_S window and process user events from GTK
 ⍝
 LINE←0
 COLS←(⊂"ROWID"),∆TABLE_1.columns[;7]   ⍝ Column names

 SQL←"SELECT ", (Cols2Text COLS), "  FROM Person"
 DATA←SQL ⎕SQL[4, DB] ''                ⍝ fetch record from DB
 TEXT←1⊂ ⍕ COLS⍪DATA                    ⍝ prepend column names and stringify

 GTK ← (∆CSS SMD) ⎕GTK ⊢ SMD guiS_XML ∆gui_S         ⍝ display the GUI
⊣ { (⍵, "\0") ⎕GTK[GTK, "text_view-MOUSE"] "add_row" } ¨ TEXT

LOOP:
 EVENT←⎕GTK 1                           ⍝ wait for some user action
 → (EVENT[3] ≡¨ "clicked" "mouse_press")/GUI_BUTTON, MOUSE_CLICK ◊ →LOOP

GUI_BUTTON: DBG '@GUI_BUTTON'
 → (EVENT[2] ≡¨ "Bn-OK" "Bn-CANCEL")/OK, CANCEL
 → LOOP

MOUSE_CLICK: LINE←⍎⊃EVENT[7]            ⍝ remember the line selected.
  DBG "@MOUSE_CLICK ON LINE:" LINE
 →LOOP

OK: DBG "@Select: Bn-OK"
 →(SMD='SMD')/ CANCEL, MODIFY, DELETE ◊ ???

MODIFY: DBG "@Select: MODIFY" LINE
 →LINE↓LOOP   ⍝ ignore command if LINE 0 (field headers) was selected
 ⊣ GTK ⎕GTK 0                               ⍝ close the guiS window
 DB MODIFY_RECORD DATA[LINE;]
 →0

DELETE: DBG "@Select: DELETE" LINE
 →LINE↓LOOP   ⍝ ignore command if LINE 0 (field headers) was selected
 DBG "DELETE LINE" LINE
 ⊣ GTK ⎕GTK 0                               ⍝ close the guiS window
 DB DELETE_RECORD ⍕DATA[LINE;1]
 →0

CANCEL: DBG "@Select: DONE"
 ⊣ GTK ⎕GTK 0                               ⍝ close the guiS window
∇

      COMMENT «««
      ╔══════════════════════════════════════════════════════════════════════╗
      ║ DEFINE SOME (CONSTANT) GLOBAL VARIABLES:                             ║
      ║ ────────────────────────────────────────                             ║
      ║ ∆TABLE_1...  : table(s) in ∆DB      (add more when needed)           ║
      ║ ∆DB          : SQL database with 1  (or more tables)                 ║
      ║ ∆gui_M       : GTK Main window      (action buttons)                 ║
      ║ ∆gui_S       : GTK Selection window (pick a database record)         ║
      ║ ∆gui_T1...   : GTK input window     (enter record data for ∆TABLE_1) ║
      ╚══════════════════════════════════════════════════════════════════════╝
              »»»

      COMMENT «««
      ┌──────────────────────────────────────────────────────────────────────┐
      │ DEFINE DATABASE TABLE(s): ∆TABLE_1...                                │
      └──────────────────────────────────────────────────────────────────────┘
              »»»
      ∆TABLE_1.name    ← "Person"   ⍝ table name in SQL
      ∆TABLE_1.columns ← Text2APL """
      ⍝ ┌───────────────────────────────────────── [1] GUI label pos X (pixel)
      ⍝ │   ┌───────────────────────────────────── [2] GUI label pos Y (pixel)
      ⍝ │   │  ┌────────────────────────────────── [3] GUI label (fixed text)
      ⍝ │   │  │             ┌──────────────────── [4] GUI input offset X
      ⍝ │   │  │             │ ┌────────────────── [5] GUI input offset Y
      ⍝ │   │  │             │ │  ┌─────────────── [5] GUI input size (chars)
      ⍝ │   │  │             │ │  │  ┌──────────── [6] SQL column name
      ⍝ │   │  │             │ │  │  │        ┌─── [7] SQL datatype
      ⍝ │   │  │             │ │  │  │        │
       20  20 "First Name:" 70 5 30 "FIRST"  "CHAR(50)"
      340  20 "Last Name:"  70 5 50 "LAST"   "CHAR(50) NOT NULL"
       20  60 "Zip Code:"   70 5  6 "ZIP"    "INTEGER DEFAULT 0"
      190  60 "City:"       40 5 60 "CITY"   "CHAR(50)"
       20 100 "Street:"     70 5 60 "STREET" "CHAR(50)"
       20 140 "Salary:"     70 5 10 "SALARY" "REAL DEFAULT 0.0"
                                  """
      COMMENT """
      ┌──────────────────────────────────────────────────────────────────────┐
      │ DEFINE THE DATABASE: ∆DB (and add table ∆TABLE_1 to it)              │
      └──────────────────────────────────────────────────────────────────────┘
              """
      ∆DB.filename ← "/tmp/SQL_GUI.db"   ⍝ database filename
      ∆DB.provider ← "sqlite"            ⍝ database type (sqlite or postgresql)
      ∆DB.tables   ← ⍬, (⊂∆TABLE_1)      ⍝ database schema (table(s))

      COMMENT """
      ┌──────────────────────────────────────────────────────────────────────┐
      │ DEFINE GUI window layouts:                                           │
      │        ∆gui_M        Main window (choose an operation)               │
      │        ∆gui_S        Selection window (select one database record)   │
      │        ∆gui_T1...    data entry for ∆TABLE_1...                      │
      └──────────────────────────────────────────────────────────────────────┘
              """
      ∆gui_M.window_width    ← 240                ⍝ window width (pixels)
      ∆gui_M.window_height   ← 320                ⍝ window height (pixels)

      ∆gui_T1.window_width    ← 800                ⍝ window width (pixels)
      ∆gui_T1.window_height   ← 220                ⍝ window height (pixels)
      ∆gui_T1.fields          ← 0 ¯1↓∆TABLE_1.columns
      ∆gui_T1.field_names     ← ∆gui_T1.fields[;1↓⍴∆gui_T1.fields]   ⍝ last col

      ∆gui_S.window_width    ← 500                ⍝ window width (pixels)
      ∆gui_S.window_height   ← 240                ⍝ window height (pixels)
      ∆gui_S.textview_width  ← 440                ⍝ textview height (pixels)
      ∆gui_S.textview_height ← 155                ⍝ textview height (pixels)

∇MAIN;DB;GTK;OPER;PRESET;DATA   ⍝ --------------------------------------
 ⍝
 ⍝⍝ The main program
 ⍝
 COMMENT «««
 ┌───────────────────────────────────────────────────────────────────────────┐
 │ CREATE DATABASE: DB                                                       │
 │ and ADD DATABASE TABLE(s)                                                 │
 └───────────────────────────────────────────────────────────────────────────┘
         »»»

 DB←CREATE_DB ∆DB

 COMMENT """
 ┌───────────────────────────────────────────────────────────────────────────┐
 │ TOP-LEVEL LOOP... HANDLE NEW, MODIFY, DELETE, SHOW, or EXIT REQUESTS      │
 └───────────────────────────────────────────────────────────────────────────┘
         """
TOP_LEVEL_LOOP:
 OPER←Main_window                            ⍝ NEW MODIFY DELETE SHOW EXIT
 →(OPER≡"NEW")   ⍴ OPER_NEW                  ⍝ add a NEW record
 →(OPER≡"MODIFY")⍴ OPER_MODIFY               ⍝ MODIFY one record
 →(OPER≡"DELETE")⍴ OPER_DELETE               ⍝ DELETE one record
 →(OPER≡"SHOW")  ⍴ OPER_SHOW                 ⍝ SHOW all records
 →(OPER≡"EXIT")  ⍴ EXIT_PROGRAM              ⍝ EXIT program
 →TOP_LEVEL_LOOP

OPER_NEW:
 COMMENT «««
 ┌───────────────────────────────────────────────────────────────────────────┐
 │ LET THE USER ENTER HER DATA and WRITE IT TO THE DATABASE                  │
 └───────────────────────────────────────────────────────────────────────────┘
         »»»

 ⍝ preset some fields in GTK.
 ⍝
 PRESET←Text2APL «««
  "FIRST" "Jane"
  "LAST"  "Doe"
 "ZIP"    "94709"
 "CITY"   "Berkeley"
 "STREET" "1543 Spruce St."
                 »»»

 DATA←Data_input PRESET                     ⍝ get user data, wait for OK
 →(DATA≡⍬) / TOP_LEVEL_LOOP                 ⍝ CANCEL button pressed

 DB ADD_RECORD DATA
 → TOP_LEVEL_LOOP

OPER_SHOW:   'S' Select_window DB  ◊ → TOP_LEVEL_LOOP
OPER_MODIFY: 'M' Select_window DB  ◊ → TOP_LEVEL_LOOP
OPER_DELETE: 'D' Select_window DB  ◊ → TOP_LEVEL_LOOP
EXIT_PROGRAM: CLOSE_DB DB
∇

      MAIN   ⍝ call the main program

      )VARS
      )OFF

