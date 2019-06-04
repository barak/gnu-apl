/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <fstream>

using namespace std;

#include "Archive.hh"
#include "Command.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "LibPaths.hh"
#include "Output.hh"
#include "Quad_FFT.hh"
#include "Quad_FX.hh"
#include "Quad_GTK.hh"
#include "Quad_PLOT.hh"
#include "Quad_SQL.hh"
#include "Quad_TF.hh"
#include "SystemVariable.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
Workspace::Workspace()
   : WS_name("CLEAR WS"),
//   prompt("-----> "),
     prompt("      "),
     top_SI(0)
{
#define ro_sv_def(x, str, _txt)                                   \
   if (*str) { UCS_string q(UNI_Quad_Quad);   q.append_UTF8(str); \
   distinguished_names.add_variable(q, ID_ ## x, &v_ ## x); }

#define rw_sv_def ro_sv_def

#define sf_def(x, str, _txt)                                      \
   if (*str) { UCS_string q(UNI_Quad_Quad);   q.append_UTF8(str); \
   distinguished_names.add_function(q, ID_ ## x, x::fun); }

#include "SystemVariable.def"
}
//-----------------------------------------------------------------------------
void
Workspace::push_SI(Executable * fun, const char * loc)
{
   Assert1(fun);

   if (Quad_SYL::si_depth_limit && SI_top() &&
       Quad_SYL::si_depth_limit <= SI_top()->get_level())
      {
        MORE_ERROR() <<
        "the system limit on SI depth (as set in ⎕SYL) was reached\n"
        "(and to avoid lock-up, the limit in ⎕SYL was automatically cleared).";

        Quad_SYL::si_depth_limit = 0;
        set_attention_raised(LOC);
        set_interrupt_raised(LOC);
      }

   if (Value::check_WS_FULL(__FUNCTION__, 1000, LOC))
      {
        if (SI_top() && SI_top()->get_executable() &&
             the_workspace.SI_entry_count() > 100)
            MORE_ERROR() <<
          "Value::check_WS_FULL() complained when calling a defined function.\n"
          "This was most likely caused by some infinite recursion involving "
             << SI_top()->get_executable()->get_name();
        else
           MORE_ERROR() <<
           "Value::check_WS_FULL() complained when calling a defined function";
        WS_FULL;
      }

   try
      {
         StateIndicator * old_top = SI_top();
         the_workspace.top_SI = new StateIndicator(fun, old_top);
         if (the_workspace.top_SI == 0)
            {
              MORE_ERROR() <<
              "malloc() → 0 when calling a defined function";
              the_workspace.top_SI = old_top;
              WS_FULL;
            }
      }
   catch (const std::bad_alloc & e)
      {
         if (SI_top() && SI_top()->get_executable() &&
             the_workspace.SI_entry_count() > 100)
            MORE_ERROR() <<
            "std::bad_alloc exception when calling a defined function\n"
            "This was most likely caused by some infinite recursion involving "
            << SI_top()->get_executable()->get_name();
         else
            MORE_ERROR() <<
            "std::bad_alloc exception when calling a defined function";
         WS_FULL;
      }

   Log(LOG_StateIndicator__push_pop)
      {
        CERR << "Push  SI[" <<  SI_top()->get_level() << "] "
             << "pmode=" << fun->get_parse_mode()
             << " exec=" << fun << " "
             << fun->get_name();

        CERR << " new SI is " << voidP(SI_top())
             << " at " << loc << endl;
      }
}
//-----------------------------------------------------------------------------
void
Workspace::pop_SI(const char * loc)
{
   Assert(SI_top());
const Executable * exec = SI_top()->get_executable();
   Assert1(exec);

   Log(LOG_StateIndicator__push_pop)
      {
        CERR << "Pop  SI[" <<  SI_top()->get_level() << "] "
             << "pmode=" << exec->get_parse_mode()
             << " exec=" << exec << " ";

        if (exec->get_ufun())   CERR << exec->get_ufun()->get_name();
        else                    CERR << SI_top()->get_parse_mode_name();
        CERR << " " << voidP(SI_top())
             << " at " << loc << endl;
      }

   // remove the top SI
   //
StateIndicator * del = SI_top();
   the_workspace.top_SI = del->get_parent();
   delete del;
}
//-----------------------------------------------------------------------------
uint64_t
Workspace::get_RL(uint64_t mod)
{
   // we discard random numbers >= max_rand in order to avoid a bias
   // towards small numbers
   //
const uint64_t max_rand = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % mod);
uint64_t rand = the_workspace.v_Quad_RL.get_random();

   do { rand ^= rand >> 11;
        rand ^= rand << 29;
        rand ^= rand >> 14;
      } while (rand >= max_rand);

   return rand % mod;
}
//-----------------------------------------------------------------------------
void
Workspace::clear_error(const char * loc)
{
   // clear errors up to (including) next user defined function (see ⎕ET)
   //
   for (StateIndicator * si = the_workspace.SI_top(); si; si = si->get_parent())
       {
         new (&StateIndicator::get_error(si)) Error(E_NO_ERROR, LOC);
         if (si->get_parse_mode() == PM_FUNCTION)   break;
       }
}
//-----------------------------------------------------------------------------
StateIndicator *
Workspace::SI_top_fun()
{
   for (StateIndicator * si = SI_top(); si; si = si->get_parent())
       {
         if (si->get_parse_mode() == PM_FUNCTION)   return si;
       }

   return 0;   // no context wirh parse mode PM_FUNCTION
}
//-----------------------------------------------------------------------------
StateIndicator *
Workspace::SI_top_error()
{
   for (StateIndicator * si = SI_top(); si; si = si->get_parent())
       {
         if (StateIndicator::get_error(si).get_error_code() != E_NO_ERROR)
            return si;
       }

   return 0;   // no context with an error
}
//-----------------------------------------------------------------------------
Token
Workspace::immediate_execution(bool exit_on_error)
{
   for (;;)
       {
         try
           {
              Command::process_line();
           }
         catch (Error & err)
            {
              if (!err.get_print_loc())
                 {
                   if (err.get_error_code() != E_DEFN_ERROR)
                      {
                        err.print_em(UERR, LOC);
                        CERR << __FUNCTION__ << "() caught APL error "
                             << HEX(err.get_error_code()) << " ("
                             << err.error_name(err.get_error_code()) << ")"
                             << endl;

                        IO_Files::apl_error(LOC);
                      }
                 }
              if (exit_on_error)   return Token(TOK_OFF);
            }
         catch (const std::bad_alloc & e)
            {
              CERR << "*** " << __FUNCTION__
                   << "() caught bad_alloc: " << e.what() << " ***" << endl;
              if (the_workspace.SI_entry_count() > 100)
                 CERR << ")SI depth is " << the_workspace.SI_entry_count()
                      <<  "; )SIC is strongly recommended !!!"
                      << endl;
              try { WS_FULL } catch (...) {}
              if (exit_on_error)   return Token(TOK_OFF);
            }
         catch (...)
            {
              CERR << "*** " << __FUNCTION__
                   << "() caught other exception ***" << endl;
              IO_Files::apl_error(LOC);
              if (exit_on_error)   return Token(TOK_OFF);
            }
      }

   return Token(TOK_ESCAPE);
}
//-----------------------------------------------------------------------------
NamedObject *
Workspace::lookup_existing_name(const UCS_string & name)
{
   if (name.size() == 0)   return 0;

   if (Avec::is_quad(name[0]))   // distinguished name
      {
        int len;
        Token tok = get_quad(name, len);
        if (len == 1)                          return 0;
        if (name.size() != len)                return 0;
        if (tok.get_Class() == TC_SYMBOL)      return tok.get_sym_ptr();
        if (tok.get_Class() == TC_FUN0)        return tok.get_function();
        if (tok.get_Class() == TC_FUN1)        return tok.get_function();
        if (tok.get_Class() == TC_FUN2)        return tok.get_function();

        Assert(0);
      }

   // user defined variable or function
   //
Symbol * sym = the_workspace.symbol_table.lookup_existing_symbol(name);
   if (sym == 0)   return 0;

   switch(sym->get_nc())
      {
        case NC_VARIABLE: return sym;

        case NC_FUNCTION:
        case NC_OPERATOR: return sym->get_function();
        default:          return 0;
      }
}
//-----------------------------------------------------------------------------
Symbol *
Workspace::lookup_existing_symbol(const UCS_string & symbol_name)
{
   if (symbol_name.size() == 0)   return 0;

   if (Avec::is_quad(symbol_name[0]))   // distinguished name
      {
        int len;
        Token tok = get_quad(symbol_name, len);
        if (symbol_name.size() != len)         return 0;
        if (tok.get_Class() != TC_SYMBOL)      return 0;   // system function

        return tok.get_sym_ptr();
      }

   return the_workspace.symbol_table.lookup_existing_symbol(symbol_name);
}
//-----------------------------------------------------------------------------
Token
Workspace::get_quad(const UCS_string & ucs, int & len)
{
   if (ucs.size() == 0 || !Avec::is_quad(ucs[0]))
     {
       len = 0;
       return Token();
     }

UCS_string name(UNI_Quad_Quad);
   len = 1;

SystemName * longest = 0;

   for (ShapeItem u = 1; u < ucs.size(); ++u)
      {
        Unicode uni = ucs[u];
        if (!Avec::is_symbol_char(uni))   break;

        if (uni >= 'a' && uni <= 'z')   uni = Unicode(uni - 0x20);
        name.append(uni);
        SystemName * dn =
               the_workspace.distinguished_names.lookup_existing_symbol(name);

        if (dn)   // ss is a valid distinguished name
           {
             longest = dn;
             len = name.size();
           }
      }

   if (longest == 0)   return Token(TOK_Quad_Quad, &the_workspace.v_Quad_Quad);

   if (longest->get_variable())   return longest->get_variable()->get_token();
   else                           return longest->get_function()->get_token();
}
//-----------------------------------------------------------------------------
StateIndicator *
Workspace::oldest_exec(const Executable * exec)
{
StateIndicator * ret = 0;

   for (StateIndicator * si = SI_top(); si; si = si->get_parent())
       if (exec == si->get_executable())   ret = si;   // maybe not yet oldest

   return ret;
}
//-----------------------------------------------------------------------------
bool
Workspace::is_called(const UCS_string & funname)
{
   // return true if the current-referent of funname is pendant
   //
Symbol * current_referent = lookup_existing_symbol(funname);
   if (current_referent == 0)   return false;   // no such symbol

   Assert(current_referent->get_Id() == ID_USER_SYMBOL);

const NameClass nc = current_referent->get_nc();
   if (nc != NC_FUNCTION && nc != NC_OPERATOR)   return false;

const Function * fun = current_referent->get_function();
   Assert(fun);

const UserFunction * ufun = fun->get_ufun1();
   if (!ufun)   return false;         // not a defined function

   for (const StateIndicator * si = SI_top(); si; si = si->get_parent())
      {
        if (si->uses_function(ufun))   return true;
      }

   return false;
}
//-----------------------------------------------------------------------------
void
Workspace::write_OUT(FILE * out, uint64_t & seq, const UCS_string_vector
                     & objects)
{
   // if objects is empty then write all user define objects and some system
   // variables
   //
   if (objects.size() == 0)   // all objects plus some system variables
      {
        get_v_Quad_CT().write_OUT(out, seq);
        get_v_Quad_FC().write_OUT(out, seq);
        get_v_Quad_IO().write_OUT(out, seq);
        get_v_Quad_LX().write_OUT(out, seq);
        get_v_Quad_PP().write_OUT(out, seq);
        get_v_Quad_PR().write_OUT(out, seq);
        get_v_Quad_RL().write_OUT(out, seq);

        get_symbol_table().write_all_symbols(out, seq);
      }
   else                       // only specified objects
      {
         loop(o, objects.size())
            {
              NamedObject * obj = lookup_existing_name(objects[o]);
              if (obj == 0)   // not found
                 {
                   COUT << ")OUT: " << objects[o] << " NOT SAVED (not found)"
                        << endl;
                   continue;
                 }

              if (obj->get_Id() == ID_USER_SYMBOL)   // user defined name
                 {
                   const Symbol * sym = lookup_existing_symbol(objects[o]);
                   Assert(sym);
                   sym->write_OUT(out, seq);
                 }
              else                            // distinguished name
                 {
                   const Symbol * sym = obj->get_symbol();
                   if (sym == 0)
                      {
                        COUT << ")OUT: " << objects[o]
                             << " NOT SAVED (not a variable)" << endl;
                        continue;
                      }

                   sym->write_OUT(out, seq);
                 }
            }
      }
}
//-----------------------------------------------------------------------------
void
Workspace::unmark_all_values()
{
   // unmark user defined symbols
   //
   the_workspace.symbol_table.unmark_all_values();

   // unmark system variables
   //
#define rw_sv_def(x, _str, _txt) the_workspace.v_ ## x.unmark_all_values();
#define ro_sv_def(x, _str, _txt) the_workspace.v_ ## x.unmark_all_values();
#include "SystemVariable.def"
   the_workspace.v_Quad_Quad .unmark_all_values();
   the_workspace.v_Quad_QUOTE.unmark_all_values();

   // unmark token reachable vi SI stack
   //
   for (StateIndicator * si = SI_top(); si; si = si->get_parent())
       si->unmark_all_values();

   // unmark token in (failed) ⎕EX functions
   //
   loop(f, the_workspace.expunged_functions.size())
      the_workspace.expunged_functions[f]->unmark_all_values();
}
//-----------------------------------------------------------------------------
int
Workspace::show_owners(ostream & out, const Value & value)
{
int count = 0;

   // user defined variabes
   //
   count += the_workspace.symbol_table.show_owners(out, value);

   // system variabes
   //
#define rw_sv_def(x, _str, _txt) count += get_v_ ## x().show_owners(out, value);
#define ro_sv_def(x, _str, _txt) count += get_v_ ## x().show_owners(out, value);
#include "SystemVariable.def"
   count += the_workspace.v_Quad_Quad .show_owners(out, value);
   count += the_workspace.v_Quad_QUOTE.show_owners(out, value);

   for (StateIndicator * si = SI_top(); si; si = si->get_parent())
       count += si->show_owners(out, value);

   loop(f, the_workspace.expunged_functions.size())
       {
         char cc[100];
         const long long lf(f);
         snprintf(cc, sizeof(cc), "    ⎕EX[%lld] ", lf);
         count += the_workspace.expunged_functions[f]
                               ->show_owners(cc, out, value);
       }

   return count;
}
//-----------------------------------------------------------------------------
int
Workspace::cleanup_expunged(ostream & out, bool & erased)
{
const int ret = the_workspace.expunged_functions.size();

   if (SI_entry_count() > 0)
      {
        out << "SI not cleared (size " << SI_entry_count()
            << "): not deleting ⎕EX'ed functions (try )SIC first)" << endl;
        erased = false;
        return ret;
      }

   while(the_workspace.expunged_functions.size())
       {
         const UserFunction * ufun = the_workspace.expunged_functions.back();
         the_workspace.expunged_functions.pop_back();
         out << "finally deleting " << ufun->get_name() << "...";
         delete ufun;
         out << " OK" << endl;
       }

   erased = true;
   return ret;
}
//-----------------------------------------------------------------------------
void
Workspace::clear_WS(ostream & out, bool silent)
{
   // remove user-defined commands
   //
   get_user_commands().clear();

   // clear the SI (pops all localized symbols)
   //
   clear_SI(out);

   // clear the symbol tables
   //
   the_workspace.symbol_table.clear(out);

   // clear the )MORE error info
   //
   more_error().clear();

   // ⎕PW and ⎕TZ shall survive )CLEAR (lrm p. 260);
   //
const int pw = the_workspace.v_Quad_PW[0].apl_val->get_sole_integer();
const int tz = the_workspace.v_Quad_TZ.get_offset();

   // clear the value stacks of read/write system variables...
   //
#define rw_sv_def(x, _str, _txt) get_v_ ## x().clear_vs();
#define ro_sv_def(x, _str, _txt)
#include "SystemVariable.def"

   // at this point all values should have been gone.
   // complain about those that still exist, and remove them.
   //
// Value::erase_all(out);

#define rw_sv_def(x, _str, _txt) \
   { get_v_ ##x().~x();   new (&get_v_ ##x()) x; }
#define ro_sv_def(x, _str, _txt)
#include "SystemVariable.def"

   get_v_Quad_RL().reset_seed();
   Workspace::set_PW(pw, LOC);
   the_workspace.v_Quad_TZ.set_offset(tz);

   // close open windows in ⎕GTK
   Quad_GTK::fun->clear();

   // close open files in ⎕FIO
   Quad_FIO::fun->clear();

   set_WS_name(UCS_string("CLEAR WS"));
   if (!silent)   out << "CLEAR WS" << endl;
}
//-----------------------------------------------------------------------------
void
Workspace::clear_SI(ostream & out)
{
   // clear the SI (pops all localized symbols)
   while (SI_top())
      {
        SI_top()->escape();
        pop_SI(LOC);
      }
}
//-----------------------------------------------------------------------------
void
Workspace::list_SI(ostream & out, SI_mode mode)
{
   for (const StateIndicator * si = SI_top(); si; si = si->get_parent())
       si->list(out, mode);

   if (mode & SIM_debug)   out << endl;
}
//-----------------------------------------------------------------------------
void
Workspace::save_WS(ostream & out, LibRef libref, const UCS_string & wsname,
                   bool name_from_WSID)
{
UTF8_string filename = LibPaths::get_lib_filename(libref, wsname, false,
                                                  ".xml", 0);

   // dont )SAVE if wsname differs from wsid and the file exists
   //
const bool file_exists = access(filename.c_str(), W_OK) == 0;
   if (file_exists)
      {
        if (wsname.compare(the_workspace.WS_name) != 0)   // names differ
           {
             out << "NOT SAVED: THIS WS IS "
                 << the_workspace.WS_name << endl;

             MORE_ERROR() << "the workspace was not saved because:\n"
                  << "   the workspace name '" << the_workspace.WS_name
                  << "' of )WSID\n   does not match the name '" << wsname
                  << "' used in the )SAVE command\n"
                  << "   and the workspace file\n   " << filename
                  << "\n   already exists. Use )WSID " << wsname << " first."; 
             return;
           }
      }

   if (uprefs.backup_before_save && backup_existing_file(filename.c_str()))
      {
        COUT << "NOT SAVED: COULD NOT CREATE BACKUP FILE "
             << filename << endl;
        return;
      }

   // at this point it is OK to rename and save the workspace
   //
ofstream outf(filename.c_str(), ofstream::out);
   if (!outf.is_open())   // open failed
      {
        CERR << "Unable to )SAVE workspace '" << wsname
             << "'. " << strerror(errno) << endl;
        return;
      }

   the_workspace.WS_name = wsname;

XML_Saving_Archive ar(outf);
   ar.save();

   // print time and date to COUT
   get_v_Quad_TZ().print_timestamp(out, now());
   if (name_from_WSID)   out << " " << the_workspace.WS_name;
   out << endl;
}
//-----------------------------------------------------------------------------
bool
Workspace::backup_existing_file(const char * filename)
{
   // 1. if file 'filename' does not exist then no backup is needed.
   //
   if (access(filename, F_OK) != 0)   return false;   // OK

UTF8_string backup_filename = filename;
   backup_filename.append_ASCII(".bak");

   // 2. if backup file exists then remove it...
   //
   if (access(backup_filename.c_str(), F_OK) == 0)   // backup exists
      {
        const int err = unlink(backup_filename.c_str());
        if (err)
           {
             CERR << "Could not remove backup file " << backup_filename
                  << ": " << strerror(errno) << endl;
             return true;   // error
           }

         if (access(backup_filename.c_str(), F_OK) == 0)   // still exists
            {
             CERR << "Could not remove backup file " << backup_filename << endl;
             return true;   // error
            }
      }

   // 3. rename file to file.bak
   //
const int err = rename(filename, backup_filename.c_str());
   if (err)   // rename failed
      {
        CERR << "Could not rename file " << filename
             << " to " << backup_filename
             << ": " << strerror(errno) << endl;
        return true;   // error
      }

   if (access(filename, F_OK) == 0)   // file still exists
      {
        CERR << "Could not rename file " << filename
             << " to " << backup_filename << endl;
        return true;   // error
      }

   return false; // OK
}
//-----------------------------------------------------------------------------
void
Workspace::load_DUMP(ostream & out, const UTF8_string & filename, int fd,
                     LX_mode with_LX, bool silent,
                     UCS_string_vector * object_filter)
{
   Log(LOG_command_IN)
      CERR << "loading )DUMP file " << filename << "..." << endl;

   {
     struct stat st;
     fstat(fd, &st);
     const APL_time_us when = 1000000ULL * st.st_mtime;
     Workspace::get_v_Quad_TZ().print_timestamp(out << "DUMPED ", when) << endl;
   }

FILE * file = fdopen(fd, "r");

   // make sure that filename is not already open (which would indicate
   // )COPY recursion
   //
   loop(f, InputFile::files_todo.size())
      {
        if (filename == InputFile::files_todo[f].filename)   // same filename
           {
             CERR << "*** )COPY " << filename
                  << " would cause recursion (NOT COPIED)" << endl;
             return;
           }
      }

InputFile fam(filename, file, false, false, true, with_LX);
   if (object_filter)   // therefore )COPY, not )LOAD
      {
        loop(o, object_filter->size())
            fam.add_filter_object((*object_filter)[o]);
        fam.set_COPY();
        ++Bif_F1_EXECUTE::copy_pending;
      }
   InputFile::files_todo.insert(InputFile::files_todo.begin(), fam);
}
//-----------------------------------------------------------------------------
/// a streambuf that escapes certain HTML characters
class HTML_streambuf : public streambuf
{
public:
   /// constructor
   HTML_streambuf(ofstream & outf)
   : out_file(outf)
   {}

   /// overloaded streambuf::overflow()
   virtual int overflow(int c)
      {
        switch(c & 0xFF)
           {
              case '#':  out_file << "&#35;";   break;
              case '%':  out_file << "&#37;";   break;
              case '&':  out_file << "&#38;";   break;
              case '<':  out_file << "&lt;";    break;
              case '>':  out_file << "&gt;";    break;
              default:   out_file << char(c & 0xFF);
           }
        return 0;
      }

protected:
   /// the output file
   ofstream & out_file;
};

/// a stream that escapes certain HTML characters
class HTML_stream : public ostream
{
public:
   /// constructor
   HTML_stream(HTML_streambuf * html_out)
   : ostream(html_out)
   {}
};
//-----------------------------------------------------------------------------
void
Workspace::dump_WS(ostream & out, LibRef libref, const UCS_string & wsname,
                   bool html, bool silent)
{
   // )DUMP
   // )DUMP wsname
   // )DUMP libnum wsname

const char * extension = html ? ".html" : ".apl";
UTF8_string filename = LibPaths::get_lib_filename(libref, wsname, false,
                                                  extension, 0);
   if (wsname.compare(UCS_string("CLEAR WS")) == 0)   // don't save CLEAR WS
      {
        COUT << "NOT DUMPED: THIS WS IS " << wsname << endl;
        MORE_ERROR() <<
        "the workspace was not dumped because 'CLEAR WS' is a special\n"
        "workspace name that cannot be dumped. Use )WSID <name> first.";
        return;
      }

   if (uprefs.backup_before_save && backup_existing_file(filename.c_str()))
      {
        COUT << "NOT DUMPED: COULD NOT CREATE BACKUP FILE "
             << filename << endl;
        return;
      }

ofstream outf(filename.c_str(), ofstream::out);

   if (!outf.is_open())   // open failed
      {
        CERR << "Unable to )DUMP workspace '" << wsname
             << "': " << strerror(errno) << "." << endl
             << "    NOTE: filename: " << filename << endl;
        return;
      }

HTML_streambuf hout_buf(outf);
HTML_stream hout(&hout_buf);
ostream * sout = &outf;
   if (html)   sout = &hout;
   // print header line, workspace name, time, and date to outf
   //
const APL_time_us offset = get_v_Quad_TZ().get_offset();
const APL_time_us gmt = now();
const YMDhmsu time(gmt + 1000000*offset);
   {
     if (html)
        {
          outf <<
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\""               << endl <<
"                      \"http://www.w3.org/TR/html4/strict.dtd\">"  << endl <<
"<html>"                                                            << endl <<
"  <head>"                                                          << endl <<
"    <title>" << wsname << ".apl</title> "                          << endl <<
"    <meta http-equiv=\"content-type\" "                            << endl <<
"          content=\"text/html; charset=UTF-8\">"                   << endl <<
"    <meta name=\"author\" content=\"??????\">"                     << endl <<
"    <meta name=\"copyright\" content=\"&copy; " << time.year
                                                 <<" by ??????\">"  << endl <<
"    <meta name=\"date\" content=\"" << time.year << "-"
                                     << time.month << "-"
                                     << time.day << "\">"           << endl <<
"    <meta name=\"description\""                                    << endl <<
"          content=\"??????\">"                                     << endl <<
"    <meta name=\"keywords\" lang=\"en\""                           << endl <<
"          content=\"??????, APL, GNU\">"                           << endl <<
" </head>"                                                          << endl <<
" <body><pre>"                                                      << endl <<
"⍝"                                                                 << endl <<
"⍝ Author:      ??????"                                             << endl <<
"⍝ Date:        " << time.year << "-"
                  << time.month << "-"
                  << time.day                                       << endl <<
"⍝ Copyright:   Copyright (C) " << time.year << " by ??????"        << endl <<
"⍝ License:     GPL see http://www.gnu.org/licenses/gpl-3.0.en.html"<< endl <<
"⍝ Support email: ??????@??????"                                    << endl <<
"⍝ Portability:   L3 (GNU APL)"                                     << endl <<
"⍝"                                                                 << endl <<
"⍝ Purpose:"                                                        << endl <<
"⍝ ??????"                                                          << endl <<
"⍝"                                                                 << endl <<
"⍝ Description:"                                                    << endl <<
"⍝ ??????"                                                          << endl <<
"⍝"                                                                 << endl <<
                                                                       endl;
        }
     else
        {
          outf << "#!" << LibPaths::get_APL_bin_path()
               << "/" << LibPaths::get_APL_bin_name()
               << " --script" << endl;
        }

     *sout << " ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝" << endl
          << "⍝" << endl
          << "⍝ " << wsname << " ";
     get_v_Quad_TZ().print_timestamp(*sout, gmt) << endl
          << " ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝" << endl
          << endl;
   }

int function_count = 0;
int variable_count = 0;
   the_workspace.symbol_table.dump(*sout, function_count, variable_count);
   the_workspace.dump_commands(*sout);

   // system variables
   //
#define ro_sv_def(x, _str, _txt)
#define rw_sv_def(x, _str, _txt) if (ID_ ## x != ID_Quad_SYL) \
   { get_v_ ## x().dump(*sout);   ++variable_count; }
#include "SystemVariable.def"

   if (html)   outf << endl << "⍝ EOF </pre></body></html>" << endl;

   if (silent)
      {
        get_v_Quad_TZ().print_timestamp(out, gmt) << endl;
      }
   else
      {
        out << "DUMPED WORKSPACE '" << wsname << "'" << endl
            << " TO FILE '" << filename << "'" << endl
            << " (" << function_count << " FUNCTIONS, " << variable_count
            << " VARIABLES)" << endl;
      }
}
//-----------------------------------------------------------------------------
void
Workspace::dump_commands(ostream & out)
{
vector<Command::user_command> & cmds = get_user_commands();

   loop(c, cmds.size())
      out << "]USERCMD " << cmds[c].prefix
          << " " << cmds[c].apl_function
          << " " << cmds[c].mode << endl;

   if (cmds.size())   out << endl;
}
//-----------------------------------------------------------------------------
// )LOAD WS, set ⎕LX of loaded WS on success
void
Workspace::load_WS(ostream & out, LibRef libref, const UCS_string & wsname,
                   UCS_string & quad_lx, bool silent)
{
   // )LOAD wsname                              wsname /absolute or relative
   // )LOAD libnum wsname                       wsname relative
   //
   if (UTF8_string(wsname).ends_with(".bak"))
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << wsname <<
        " is a backup file! You need to rename it before it can be )LOADed";
        return;
      }

UTF8_string filename = LibPaths::get_lib_filename(libref, wsname, true,
                                                  ".xml", ".apl");

int dump_fd = -1;
XML_Loading_Archive in(filename.c_str(), dump_fd);

   if (dump_fd != -1)   // wsname.apl
      {
        the_workspace.clear_WS(out, true);

        Log(LOG_command_IN)   out << "LOADING " << wsname << " from file '"
                                  << filename << "' ..." << endl;

        load_DUMP(out, filename, dump_fd, do_LX, silent, 0);   // closes dump_fd

        // )DUMP files have no )WSID so create one from the filename
        //
        const char * wsid_start = strrchr(filename.c_str(), '/');
        if (wsid_start == 0)   wsid_start = filename.c_str();
        else                   ++wsid_start;   // skip /
        const char * wsid_end = filename.c_str() + filename.size();
        if (wsid_end > (wsid_start - 4) &&
           wsid_end[-4] == '.' &&
           wsid_end[-3] == 'a' &&
           wsid_end[-2] == 'p' &&
           wsid_end[-1] == 'l')   wsid_end -= 4;   // skip .apl extension
        const UTF8_string wsid_utf8(utf8P(wsid_start), wsid_end - wsid_start);
        const UCS_string wsid_ucs(wsid_utf8);
        wsid(out, wsid_ucs, libref, true);

        // we cant set ⎕LX because it was not executed yet.
        return;
      }
   else   // wsname.xml
      {
        if (!in.is_open())   // open failed
           {
             out << ")LOAD " << wsname << " (file " << filename
                 << ") failed: " << strerror(errno) << endl;
             return;
           }

        Log(LOG_command_IN)   out << "LOADING " << wsname << " from file '"
                                  << filename << "' ..." << endl;

        // got open file. We assume that from here on everything will be fine.
        // clear current WS and load it from file
        //
        the_workspace.clear_WS(out, true);
        in.read_Workspace(silent);
      }

   if (Workspace::get_LX().size())  quad_lx = Workspace::get_LX();
}
//-----------------------------------------------------------------------------
void
Workspace::copy_WS(ostream & out, LibRef libref, const UCS_string & wsname,
                   UCS_string_vector & lib_ws_objects, bool protection)
{
   // )COPY wsname                              wsname /absolute or relative
   // )COPY libnum wsname                       wsname relative
   // )COPY wsname objects...                   wsname /absolute or relative
   // )COPY libnum wsname objects...            wsname relative

UTF8_string filename = LibPaths::get_lib_filename(libref, wsname, true,
                                                  ".xml", ".apl");

int dump_fd = -1;
XML_Loading_Archive in(filename.c_str(), dump_fd);
   if (dump_fd != -1)
      {
        load_DUMP(out, filename, dump_fd, no_LX, false, &lib_ws_objects);
        // load_DUMP closes dump_fd
        return;
      }

   if (!in.is_open())   // open failed: try filename.xml unless already .xml
      {
        CERR << ")COPY " << wsname << " (file " << filename
             << ") failed: " << strerror(errno) << endl;
        return;
      }

   Log(LOG_command_IN)   CERR << "LOADING " << wsname << " from file '"
                              << filename << "' ..." << endl;

   in.set_protection(protection, lib_ws_objects);
   in.read_vids();
   in.read_Workspace(false);
}
//-----------------------------------------------------------------------------
void
Workspace::wsid(ostream & out, UCS_string arg, LibRef lib, bool silent)
{
   arg.remove_leading_and_trailing_whitespaces();

   if (arg.size() == 0)   // inquire workspace name
      {
        out << "IS " << the_workspace.WS_name << endl;
        return;
      }

   if (lib == LIB_WSNAME)     // i.e. arg may start with a library number
      {
        if (arg[0] >= '0' && arg[0] <= '9')   // it does
           {
             lib = LibRef(arg[0] - '0');
             arg.erase(0);                       // skip the library number
             arg.remove_leading_whitespaces();   // and blanks after it
           }
        else lib = LIB0;
      }

   loop(a, arg.size())
      {
        if (arg[a] <= ' ')
           {
             out << "Bad WS name '" << arg << "'" << endl;
             return;
           }
      }

   if (!silent)   out << "WAS " << the_workspace.WS_name << endl;

   // prepend the lib number unless it is 0.
   //
   if (lib != LIB0)
      {
        arg.prepend(UNI_ASCII_SPACE);
        arg.prepend(Unicode('0' + lib));
      }
   the_workspace.WS_name = arg;
}
//-----------------------------------------------------------------------------
UCS_string &
MORE_ERROR()
{
   Workspace::more_error().clear();
   return Workspace::more_error();
}
//-----------------------------------------------------------------------------

