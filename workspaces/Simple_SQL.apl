#! /usr/local/bin/apl --script
# vim: et:ts=4:sw=4

      ∆DEBUG←1       ⍝ DO (1) or DO NOT (0) debug information
      DEBUG ← {⍵ ⊢[∼∆DEBUG] 0 0⍴⍵}

      0 0⍴ «««

This workspace  provides a high-level interface to SQL databases. It is a
wrapper around the low-level ⎕SQL and aims at hiding the differences between
SQL providers (like SQLite or PostgreSQL) and the details of the SQL language
itself.

SQL is a powerful query language with many features to select parts of a
database or to manipulate them. But so is APL. It is assumed that APL
programmers perfer to manipute the database records in APL rather than
using fine-grained SQL queries. For that reason only the most fundamental
database operations in this workspace, i.e.:

  ∘ creation of an SQL database,
  ∘ deletion of an SQL database,
  ∘ creation of a new database table in an SQL database,
  ∘ deletion of an existing database table in an SQL database,
  ∘ insert (append) an APL matrix into a table, and
  ∘ reading an entire database table back into APL.

NOTE: Before the functions in this workspace can be used, the databnase users 
(and their permissions) may need to be set up (a one-time procedure that
typically needs root permissions and is therefore not provided in this
workspace). A quick (but not very sophisticated as to database access
permission) is to create a single user that is allowed to do anything.

For example, for postgreSQL databases you may (as user root) want to install
postrgreSQL and then create a role USER that is allowed to create or delete
databases:

      $ apt install postgresql   # install postgreSQL database programs
      $ createuser --createdb --createrole --superuser --no-password USER

Many functions in this workspace have arguments named CTX, and/or DATA
and supposedly have the following semantics:

DATA is not used in functions that read from the database, but is required
     in functions that write to it. If needed, then DATA is the raw data
     (an APL vector or matrix) that shall be written to the database,

CTX  is a structured APL variable that contains meta data such as a
     database handle (for ⎕SQL), table column names, etc. that contain
     additional information needed by the function. Different functions
     use different members of CTX, but it simplifies matters if the
     same CTX can be used everywhere.

The following example shows the functions involved in the typical life-cycle
of a database:

   ⍝ 1. create a postgreSQL database named 'test_DB':
   ⍝
⍝  CTX←'SQLite' sSQL∆CREATE_NEW_DATABASE 'test_DB'
   CTX←'postgreSQL' sSQL∆CREATE_NEW_DATABASE 'test_DB'

   ⍝ 2. open the database
   ⍝
   CTX←sSQL∆OPEN_DATABASE CTX

   ⍝ 3. define the APL data to be stored in the database.
   ⍝
   APL_DATA←       ⍉⍪"John Doe" 94709 "Berkeley, CA" "1543 Spruce St." 87654.32
   APL_DATA←APL_DATA⍪"Jane Doe" 94709 "Berkeley, CA" "1543 Spruce St." 67890.12

   ⍝ 4. add a new table named ADDRESS to the new database
   ⍝
   CTX.table_name ← "ADDRESS"
   CTX.column_names ← "NAME" "ZIP" "CITY" "STREET" "SALARY"
   CTX.column_types ← APL_DATA sSQL∆COLUMN_TYPES CTX
   sSQL∆ADD_TABLE CTX

   ⍝ 5. insert APL_DATA into table ADDRESS
   ⍝
   APL_DATA sSQL∆INSERT_DATA CTX

   ⍝ 6. read entire table ADDRESS back
   ⍝
   DATA←sSQL∆READ_TABLE CTX

   ⍝ N-1: close the database
   ⍝
   sSQL∆CLOSE_DATABASE CTX

   ⍝ N. remove the database:
   ⍝
   sSQL∆REMOVE_DATABASE CTX

              »»»

⍝⍝⍝⍝⍝⍝ WORKSPACE METADATA ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
      ⍝ workspace meta data, see also:
      ⍝ https://www.gnu.org/software/apl/Library-Guidelines.html, and
      ⍝ https://www.gnu.org/software/apl/Library-Guidelines-GNU-APL.html
      ⍝
∇Z←sSQL⍙metadata
 ⍝
 ⍝⍝ workspace meta data
 ⍝
 Z.Author        ← "Jürgen Sauermann"
 Z.BugEmail      ← "bug-apl@gnu.org"
 Z.Documentation ← "at the beginning of the workspace"
 Z.Download      ← "shipped with GNU APL"
 Z.License       ← "LGPL version 3 or later)"
 Z.Portability   ← "L3"
 Z.Provides      ← "high-level SQL interface"                                   
 Z.Requires      ← ""
 Z.Version       ← "1.0"
∇
⍝⍝⍝⍝⍝⍝ END OF WORKSPACE METADATA ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝


      ⍝ the name and type of a database is an APL string.
      ⍝
      ⍝ For SQLite databases the name is the name of a database file, usually
      ⍝ with a filename extension .db.
      ⍝
      ⍝ For postgreSQL databases the name is a directory which contains the
      ⍝ database files, typically located in ⍝ /var/lib/postgresql/14/ (for
      ⍝ postgreSQL version 14).
      ⍝
      ⍝ "sqlite"    CREATE_NEW_DATABASE  "./test.db"   ⍝ SQLite: file name
      ⍝ "postgres"  CREATE_NEW_DATABASE  "test"        ⍝ postgreSQL: directory

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇CTX←TYPE sSQL∆CREATE_NEW_DATABASE NAME
  ⍝
  ⍝⍝ create a new database, existing ones will be deleted without notice.
  ⍝
  CTX.database_type ← TYPE   ⍝ remember the database type
  CTX.database_name ← NAME   ⍝ remember the database name
  ⍝
  →('SQLite' 'postgreSQL' ≡¨ ⊂CTX.database_type) / SQLITE POSTGRES
  "*** " , (,⊃¯1↑⎕SI 1) , ": bad CTX.database_type: ", CTX.database_type ◊ →0
 
SQLITE:
 ⊣ ⍎ ")HOST rm -f ", CTX.database_name
 CTX.database_handle ← CTX.database_type ⎕SQL[1] CTX.database_name
 →0

POSTGRES:
 ⊣ ⍎ ")HOST dropdb -f ", CTX.database_name, " 2>/dev/null"
 ⊣ ⍎ ")HOST createdb -w ", CTX.database_name
 CTX.database_handle ← CTX.database_type ⎕SQL[1] CTX.database_name
 →0
∇
DEBUG '∇: sSQL∆CREATE_NEW_DATABASE'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇CTX←sSQL∆OPEN_DATABASE CTX
 CTX.database_handle←CTX.database_type ⎕SQL[1] CTX.database_name
∇
DEBUG '∇: sSQL∆OPEN_DATABASE'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇sSQL∆ADD_TABLE CTX;PARAMs;SQL;TC
 ⍝
 ⍝⍝ add an empty database table (aka. relation in postfreSQL)
 ⍝
 PARAMs←""
 SQL← ⊂ "CREATE TABLE IF NOT EXISTS ", CTX.table_name, "("
 TC←(CTX.column_names,⍪CTX.column_types),','
 ⊣ {SQL←SQL, ⊂ "    ", (⊃⍵[1]), "  ", (⊃⍵[2]), ","}¨⊂[2]TC

 SQL←SQL, ⊂"    ROWID serial primary key);"   ⍝ add a primary key
 SQL←36 ⎕CR SQL                                      ⍝ items to lines
 ⊣ SQL ⎕SQL[3, CTX.database_handle] PARAMs
∇
DEBUG '∇: sSQL∆ADD_TABLE'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←sSQL∆LIST_DATABASES CTX;PARAMs;SQL;PARAMs
 ⍝
 ⍝⍝ list the databases in the current (as per CTX) database. Depending on the
 ⍝
 ⍝ Some SQL providers (e.g. SQLite) may need additional measures to see more
 ⍝ than one database, while others (postgreSQL) provide provider-specific
 ⍝ SQL queries to get a list of databases.
 ⍝
 PARAMs←""
  →('SQLite' 'postgreSQL' ≡¨ ⊂CTX.database_type) / SQLITE POSTGRES

SQLITE:
  8⎕CR Z←⊃↑⍎")HOST echo '.databases' |sqlite3"       ⍝ .e.g 'main: "" r/w'
       Z←¯1↓(Z⍳':')↑Z                                ⍝ e.g. "main"
       →0

POSTGRES:
 SQL←"SELECT datname FROM pg_database WHERE datistemplate = false;"
 Z←SQL ⎕SQL[4, CTX.database_handle] PARAMs
∇
DEBUG '∇: sSQL∆LIST_DATABASES'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←sSQL∆LIST_TABLES CTX;PARAMs;SQL
 ⍝
 ⍝⍝ list the database tables in the current (as per CTX) database
 ⍝
 PARAMs←""
  →('SQLite' 'postgreSQL' ≡¨ ⊂CTX.database_type) / SQLITE POSTGRES
SQLITE:
 SQL←     "SELECT name FROM sqlite_schema "
 SQL←SQL, "WHERE type IS 'table' "
 SQL←SQL, "AND name NOT LIKE 'sqlite_%' ORDER BY 1;"
 Z←SQL ⎕SQL[4, CTX.database_handle] PARAMs
 →0

POSTGRES:
 SQL←"SELECT * FROM pg_catalog.pg_tables "
 SQL←SQL, "WHERE schemaname != 'information_schema'"
 SQL←SQL, "AND schemaname != 'pg_catalog';"

 Z←SQL ⎕SQL[4, CTX.database_handle] PARAMs
 Z←Z[;2]
∇
DEBUG '∇: sSQL∆LIST_TABLES'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←sSQL∆QUOTE B
 ⍝
 ⍝⍝ return APL strings B 'quoted' and numbers unquoted.
 ⍝
 Z←⍕⊃B ◊ →(2≠∨/⊃⎕CR.cell_type_to_int B)⍴0   ⍝ don't quote numbers
 Z←'''',Z,''''                              ⍝ (single-) quote strings
∇
DEBUG '∇: sSQL∆QUOTE'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←A sSQL∆xSV B
 ⍝
 ⍝⍝ string Z is the items of (vector of strings) B, separated by A
 ⍝
 Z←"" ◊ ⊣{Z←Z, ⍵, A}¨B ◊ Z←(-⍴A)↓Z
∇
DEBUG '∇: sSQL∆XSV'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇DATA sSQL∆INSERT_DATA CTX;⎕IO;SQL;ROW;MAX_ROW;COLs;VALs
 ⍝
 ⍝⍝ add APL data DATA into the current (as per CTX) database table
 ⍝
 COLs←", " sSQL∆xSV CTX.column_names
 ROW←1 ◊ MAX_ROW←↑⍴DATA
ROW_LOOP:
 VALs←", " sSQL∆xSV sSQL∆QUOTE¨DATA[ROW;]
 SQL← "INSERT INTO ", CTX.table_name, "(",COLs,")\n  VALUES (", VALs, ");"
 ⊣SQL ⎕SQL[3, CTX.database_handle] ""
 →(MAX_ROW≥ROW←ROW+1)⍴ROW_LOOP
∇
DEBUG '∇: sSQL∆INSERT_DATA'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←sSQL∆READ_TABLE CTX;COLs;SQL
 ⍝
 ⍝⍝ return the current table (as per CTX) database table
 ⍝
 COLs←", " sSQL∆xSV CTX.column_names
 SQL← "SELECT ", COLs, " from ", CTX.table_name, ";"
 Z←SQL ⎕SQL[3, CTX.database_handle] ""
∇
DEBUG '∇: sSQL∆READ_DATA'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇sSQL∆CLOSE_DATABASE CTX
 [1] ⊣ ⎕SQL[2] CTX.database_handle
 [2] ⊣ ⎕EX 'CTX'
∇
DEBUG '∇: sSQL∆CLOSE_DATABASE'

⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
∇Z←sSQL∆REMOVE_DATABASE CTX;CMD
 ⍝
 ⍝⍝ remove a database
 ⍝
 ⍝ Some SQL providers have a DROP DATABASE commands, and some do not. The SQL
 ⍝ does not require it and therefore we use the command line in all cases.
 ⍝
  →('SQLite' 'postgreSQL' ≡¨ ⊂CTX.database_type) / SQLITE POSTGRES
  "*** " , (,⊃¯1↑⎕SI 1) , ": bad CTX.database_type: ", CTX.database_type ◊ →0

SQLITE:   CMD←")HOST rm -f "      , CTX.database_name               ◊ →COMMON
POSTGRES: CMD←")HOST dropdb -f ", CTX.database_name, " 2>/dev/null" ◊ →COMMON
COMMON:   Z←⍎CMD   ⍝ Z[2] is the result code aka. $? of CMD, plus a blank
 Z←"NOT removed." ⊢['0'=↑⊃Z[2]] "removed."                        ⍝ success ?
 Z←CTX.database_type, " database '", CTX.database_name, "' ", Z
∇
DEBUG '∇: sSQL∆REMOVE_DATABASE'

∇Z←DATA sSQL∆COLUMN_TYPES CTX;C
 ⍝
 ⍝⍝ return the SQL column types for APL value (vector or matrix) DATA
 ⍝
 Z←⎕CR.cell_type_to_int DATA              ⍝ APL DATA → APL type
 Z←⊤∨/¨Z                                  ⍝ (nested) string types → char type
 Z←⊤∨⌿Z                                   ⍝ APL type matrix → APL type vector
 Z←Z sSQL∆COLTYPE¨ ⊂CTX.database_type     ⍝ APL types → SQL types
∇
DEBUG '∇: sSQL∆COLUMN_TYPES'

∇Z←CT sSQL∆COLTYPE DB_TYPE
 ⍝
 ⍝⍝ return the SQL column types for one column type CT
 ⍝
 →(CT∈ 2 16 32)⍴VALID_TYPE
 "*** sSQL∆COLTYPE: Bad APL type" CT ◊ →0

VALID_TYPE:
 →('SQLite' 'postgreSQL' ≡¨ ⊂DB_TYPE) / SQLITE POSTGRES
 "*** " , (,⊃¯1↑⎕SI 1) , ": bad (mixed?) CTX.database_type: ", DB_TYPE ◊ →0

SQLITE:   Z←,⊃(CT = 2 16 32) / 'TEXT'        'INT'    'REAL' ◊ →0
POSTGRES: Z←,⊃(CT = 2 16 32) / 'varchar(40)' 'bigint' 'numeric'
∇
DEBUG '∇: sSQL∆COLTYPE'

DEBUG 'EXAMPLE:'

   ⍝ 1. create a postgreSQL database named 'test_DB'. This step is usually
   ⍝ performed only once, and possibly outside of APL with command line tools
   ⍝ that come with the database (sqlite3 for SQLite or psql for postgreSQL).
   ⍝
⍝  CTX←'SQLite' sSQL∆CREATE_NEW_DATABASE 'test_DB'
   CTX←'postgreSQL' sSQL∆CREATE_NEW_DATABASE 'test_DB'

   ⍝ 2. open the database
   ⍝
   CTX←sSQL∆OPEN_DATABASE CTX

   ⍝ 3. define the data to be stored in the database.
   ⍝
   APL_DATA←       ⍉⍪"John Doe" 94709 "Berkeley, CA" "1543 Spruce St." 87654.32
   APL_DATA←APL_DATA⍪"Jane Doe" 94709 "Berkeley, CA" "1543 Spruce St." 67890.12

   ⍝ 4. add a new table named ADDRESS to the new database. Like step 1. above,
   ⍝ this is usually performed only once, and possibly outside of APL using
   ⍝ the command line tools mentioned.
   ⍝
   CTX.table_name ← "ADDRESS"
   CTX.column_names ← "NAME" "ZIP" "CITY" "STREET" "SALARY"
   CTX.column_types ← APL_DATA sSQL∆COLUMN_TYPES CTX
   sSQL∆ADD_TABLE CTX
DEBUG 'database table(s):' (sSQL∆LIST_TABLES CTX)
DEBUG 'database(s):' (sSQL∆LIST_DATABASES CTX)

   ⍝ 5. insert APL_DATA into database table ADDRESS
   APL_DATA sSQL∆INSERT_DATA CTX

   ⍝ 6. optionally verify: read entire table ADDRESS back
  DATA←sSQL∆READ_TABLE CTX

   ⍝ N-1: close the database.
   ⍝
   sSQL∆CLOSE_DATABASE CTX

   ⍝ N. remove the database. WARNING: data lost with no questions asked.
   ⍝
   sSQL∆REMOVE_DATABASE CTX

   "\n      )FNS"
   )FNS
   "\n      )VARS"
   )VARS
