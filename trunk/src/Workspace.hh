/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file
*/

#ifndef __WORKSPACE_HH_DEFINED__
#define __WORKSPACE_HH_DEFINED__

#include "Command.hh"
#include "LibPaths.hh"
#include "PrimitiveOperator.hh"
#include "PrintContext.hh"
#include "QuadFunction.hh"
#include "Quad_CR.hh"
#include "Quad_DLX.hh"
#include "Quad_FIO.hh"
#include "Quad_MX.hh"
#include "Quad_RE.hh"
#include "Quad_RL.hh"
#include "Quad_SVx.hh"
#include "Quad_WA.hh"
#include "ScalarFunction.hh"
#include "Symbol.hh"
#include "SymbolTable.hh"
#include "SystemVariable.hh"

class DerivedFunction;
class Executable;
class StateIndicator;
class UTF8_string;

//════════════════════════════════════════════════════════════════════════════
/**
 The symbol tables of the Workspace. We put them into a base class for
 Workspace, so that they are initialized before all other members of
 Workspace.
 **/
/// The symbol tables of an APL workspace
class Workspace_0
{
protected:
   /// the symbol table for user-defined names of this workspace.
   SymbolTable symbol_table;

   /// the symbol table for system names (aka. distinguished names) of
   /// this workspace.
   SystemSymTab distinguished_names;
};
//────────────────────────────────────────────────────────────────────────────
/**
    An APL workspace. This structure contains everyting (variables, functions,
    SI stack, etc.) belonging to a single APL workspace.
 */
/// An APL workspace
class Workspace : public Workspace_0
{
public:
   /// Construct an empty workspace.
   Workspace();

   /// add \b ufun to list of that were ⎕EX'ed while on the SI stack
   /// @param ufun the user function that was expunged
   static void add_expunged_function(const UserFunction * ufun)
      { the_workspace.expunged_functions.push_back(ufun); }

   /// increase the wait time for user input as reported in ⎕AI
   /// @param diff additional wait time in microseconds
   static void add_wait(APL_time_us diff)
      { the_workspace.v_Quad_AI.add_wait(diff); }

   /// erase the symbols in \b symbols from the symbol table
   /// @param out output stream for messages
   /// @param symbols list of symbol names to erase
   static void erase_symbols(ostream & out, const UCS_string_vector & symbols)
      { the_workspace.symbol_table.erase_symbols(CERR, symbols); }

   /// return the name to which \b lambda ia assigned (empty if not found)
   static UCS_string find_lambda_name(const UserFunction * lambda)
      { return the_workspace.symbol_table.find_lambda_name(lambda); }

   /// copy all allocated symbols into \b table of size \b table_size
   static std::vector<const Symbol *> get_all_symbols()
      { return the_workspace.symbol_table.get_all_symbols(); }

   /// return the current ⎕CT
   static APL_Float get_CT()
      { return the_workspace.v_Quad_CT.current(); }

   /// return element \b pos of the current ⎕FC (pos should be 0..5)
   /// @param p index into ⎕FC (0..5)
   static APL_Char get_FC(int p)
      { return the_workspace.v_Quad_FC.current()[p]; }

   /// return the current ⎕IO
   static APL_Integer get_IO()
      { return the_workspace.v_Quad_IO.current(); }

   /// return the current ⎕LX
   static UCS_string get_LX()
      { return UCS_string(*the_workspace.v_Quad_LX.get_apl_value()); }

   /// return the current ⎕PP
   static int get_PP()
      { return the_workspace.v_Quad_PP.current(); }

   /// return the current ⎕PR
   static const UCS_string get_PR()
      { return the_workspace.v_Quad_PR.current(); }

   /// return style and the current ⎕PP, and ⎕PW
   /// @param style the print style to embed in the context
   static PrintContext get_PrintContext(PrintStyle style)
      {
        return PrintContext(style, the_workspace.v_Quad_PP.current(),
                                   the_workspace.v_Quad_PW.current());
      }

   /// return the APL prompt
   static const UCS_string & get_prompt()
      { return the_workspace.prompt; }

   /// return the pushed command
   static const UCS_string & get_pushed_Command()
      { return the_workspace.pushed_command; }

   /// return the current ⎕PW
   static int get_PW()
      { return the_workspace.v_Quad_PW.current(); }

   /// return the symbol table of the current workspace.
   static const SymbolTable & get_symbol_table()
      { return the_workspace.symbol_table; }

   /// Return all user-defined commands
   static vector<Command::user_command> & get_user_commands()
      {  return the_workspace.user_commands; }

   /// return the name of the current workspace.
   static const LibRef_name & get_WSID()
      { return the_workspace.WS_id; }

   /// list all symbols (of category \b which) with names in \b from_to
   /// @param out output stream for the listing
   /// @param which category of symbols to list
   /// @param from_to name range filter
   static void list(ostream & out, ListCategory which, UCS_string from_to)
      { the_workspace.symbol_table.list(out, which, from_to); }

   /// list all symbols with names in \b buf
   /// @param out output stream for the listing
   /// @param buf symbol name to search for
   static ostream & list_symbol(ostream & out, const UCS_string & buf)
      { return the_workspace.symbol_table.list_symbol(out, buf); }

   /// lookup an existing user defined symbol. If not found, create one
   /// (unless this would be a quad symbol)
   /// @param symbol_name the name of the symbol to look up or create
   static Symbol * lookup_symbol(const UCS_string & symbol_name)
      { return the_workspace.symbol_table.lookup_symbol(symbol_name);}

   /// return reference to more info about last error
   static UCS_string & more_error()
      { return the_workspace.more_error_info; }

   /// pop ⎕FC
   static void pop_FC()
      {
        the_workspace.v_Quad_FC.pop();
      }

   /// push a command. This is done when ⍎Command is performed and the command
   /// would push the SI stack (i.e. )LOAD, )QLOAD, )CLEAR, or )SIC)
   static void push_Command(const UCS_string & command)
      { the_workspace.pushed_command = command; }

   /// push ⎕FC
   static void push_FC()
      {
        the_workspace.v_Quad_FC.push();
      }

   /// set the current ⎕PW
   /// @param PW new print width value
   /// @param loc caller location for diagnostics
   static void set_PW(int PW, const char * loc)
      { the_workspace.v_Quad_PW.assign(IntScalar(PW, loc), false, loc); }

   /// set the name of the current workspace.
   /// @param new_id new workspace identifier
   static void set_WSID(const LibRef_name & new_id)
      { the_workspace.WS_id = new_id; }

   /// the top of the SI stack (the SI pushed last)
   static StateIndicator * SI_top()
      { return the_workspace.top_SI; }

   /// backup an existing file \b filename, return true on error
   static bool backup_existing_file(const char * filename);

   /// clear ⎕EM and ⎕ET related errors (error entries on SI up to (including)
   /// the next user-defined function
   /// @param loc caller location for diagnostics
   static void clear_error(const char * loc);

   /// clear the SI
   /// @param out output stream for messages
   static void clear_SI(ostream & out);

   /// clear the workspace
   /// @param out output stream for messages
   /// @param silent if true, suppress informational output
   static void clear_WS(ostream & out, bool silent);

   /// maybe remove functions for which ⎕EX has failed
   static int cleanup_expunged(ostream & out, bool & erased);

   /// copy objects from another workspace
   static void copy_WS(ostream & out, ostream & err,
                      const LibRef_name & lib_name,
                       UCS_string_vector & objects, bool protection);

   /// dump the commands in this workspace
   static void dump_commands(ostream & out);

   /// dump this workspace
   static void dump_WS(ostream & out, const LibRef_name & lib_name, bool html,
                       bool silent);

   /// return information in SI_top()
   static inline Error * get_error();

   /// return a token for system function or variable \b ucs
   static Token get_quad(const UCS_string & ucs, int & len);

   /// return the Quad-RL (to be taken % mod)
   /// @param mod modulus to apply to the random value
   static uint64_t get_RL(uint64_t mod);

   /// create and execute one immediate execution context
   // (leave with TOK_ESCAPE)
   /// @param exit_on_error if true, exit the interpreter on error
   static Token immediate_execution(bool exit_on_error);

   /// return \b true if \b the_workspace is the CLEAR WS
   static bool is_CLEAR_WS();

   /// return true iff function \b funname is on the current call stack
   static bool is_called(const UCS_string & funname);

   /// print the SI on \b out
   /// @param out output stream for the listing
   /// @param mode controls which SI details to display
   static void list_SI(ostream & out, SI_mode mode);

   /// load )DUMPed file from open file descriptor fd (closes fd)
   static void load_DUMP(ostream & out, const UTF8_string & filename, int fd,
                         LX_mode with_LX, bool silent,
                         UCS_string_vector * object_filter);

   /// load \b lib_ws into the_workspace, maybe set ⎕LX of the new WS.
   static void load_WS(ostream & out, ostream & err,
                       const LibRef_name & lib_name,
                       UCS_string & quad_lx, bool silent);

   /// lookup an existing name (user defined or ⎕xx, var or function).
   /// return 0 if not found.
   static const NamedObject * lookup_existing_name(const UCS_string & name);

   /// lookup an existing symbol (user defined or ⎕xx).
   static Symbol * lookup_existing_symbol(const UCS_string & symbol_name);

   /// return the oldest SI entry that is running \b exex, or 0 if none
   static StateIndicator * oldest_exec(const Executable * exec);

   /// Remove the current SI-entry from the SI stack.
   /// @param loc caller location for diagnostics
   static void pop_SI(const char * loc);

   /// Create a new SI-entry on the SI stack.
   /// @param fun the executable (function or statement) being pushed
   /// @param loc caller location for diagnostics
   static void push_SI(const Executable * fun, const char * loc);

   /// save this workspace
   static void save_WS(ostream & out, const LibRef_name & lib_name,
                       bool name_from_WSID);

   /// print all owners of \b value
   static int show_owners(ostream & out, const Value & value);

   /// the number of )SI stack entries
   static inline int SI_entry_count();

   /// the topmost SI with an error, maybe require ⎕L, ⎕R, or ⎕X.
   static StateIndicator * SI_top_error(bool quad_LRX);

   /// the topmost SI with parse mode PM_FUNCTION
   static StateIndicator * SI_top_fun();

   /// clear the marked flag in all values known in this workspace
   static void unmark_all_values();

   /// write symbols for )OUT command
   static void write_OUT(FILE * out, const UCS_string_vector & objects);

   /// set or inquire the workspace ID
   static void wsid(ostream & out, UCS_string arg, LibRef lib, bool silent);

   // access to system variables.
   //
/// read-only system variable
#define ro_sv_def(x, _str, _txt) /** return x **/ static x & get_v_ ## x() \
   { return the_workspace.v_ ## x; }

/// read/write system variable
#define rw_sv_def(x, _str, _txt) /** return ## x **/ static x& get_v_ ## x() \
   { return the_workspace.v_ ## x; }

   rw_sv_def(Quad_Quad,  "", "⎕")   /**< ⎕ (Quad) */
   rw_sv_def(Quad_QUOTE, "", "⍞")   /**< ⍞⎕ (QuoteQuad) */
#include "SystemVariable.def"

protected:
   /// user defined functions that were ⎕EX'ed while on the SI stack
   std::vector<const UserFunction *> expunged_functions;

   /// more info about last error
   UCS_string more_error_info;

   /// the APL prompt (6 blanks by default)
   UCS_string prompt;

   /// )LOAD, )QLOAD, )CLEAR, or )SIC
   UCS_string pushed_command;

   /// the SI stack. Initially top_SI is 0 (empty stack)
   StateIndicator * top_SI;

   /// user defined commands
   std::vector<Command::user_command> user_commands;

   // system variables.
   //
/// read-only system variable
#define ro_sv_def(x, _str, _txt) /** x **/ x v_ ## x;
/// read/write system variable
#define rw_sv_def(x, _str, _txt) /** x **/ x v_ ## x;
   rw_sv_def(Quad_Quad,  "", "⎕")
   rw_sv_def(Quad_QUOTE, "", "⍞")
#include "SystemVariable.def"

   /// Optional library reference, mandatory workspace name
   LibRef_name WS_id;

   /// the current workspace (for objects that need one but don't have one).
   static Workspace the_workspace;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __WORKSPACE_HH_DEFINED__
