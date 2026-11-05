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

#include "Sys.hh"

#include <fcntl.h>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Archive.hh"
#include "Bif_F0_ZILDE.hh"
#include "Bif_F12_COMMA.hh"
#include "Bif_F12_DOMINO.hh"
#include "Bif_F12_ELEMENT.hh"
#include "Bif_F12_ENCODE_DECODE.hh"
#include "Bif_F12_EQUIV.hh"
#include "Bif_F12_FORMAT.hh"
#include "Bif_F12_INDEX_OF.hh"
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_RHO.hh"
#include "Bif_F12_ROTATE.hh"
#include "Bif_F12_SORT.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_F12_TRANSPOSE.hh"
#include "Bif_F12_UNION_INTER.hh"
#include "Bif_F1_EXECUTE.hh"
#include "Bif_F2_INDEX.hh"
#include "Bif_F2_LEFT_RIGHT.hh"
#include "Bif_OPER1_COMMUTE.hh"
#include "Bif_OPER1_EACH.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "Bif_OPER1_SCAN.hh"
#include "Bif_OPER2_INNER.hh"
#include "Bif_OPER2_OUTER.hh"
#include "Bif_OPER2_POWER.hh"
#include "Bif_OPER2_RANK.hh"
#include "CRC32.hh"
#include "Cmd_DIAG.hh"
#include "Common.hh"
#include "Command.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Executable.hh"
#include "FloatCell.hh"
#include "Function.hh"
#include "Heapsort.hh"
#include "IndexExpr.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "Macro.hh"
#include "NativeFunction.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "PrimitiveFunction.hh"
#include "Quad_CC.hh"
#include "Quad_FFT.hh"
#include "Quad_FX.hh"
#include "Quad_GTK.hh"
#include "Quad_JSON.hh"
#include "Quad_MAP.hh"
#include "Quad_PLOT.hh"
#include "Quad_PNG.hh"
#include "Quad_RVAL.hh"
#include "Quad_SQL.hh"
#include "Quad_TF.hh"
#include "Quad_XML.hh"
#include "StateIndicator.hh"
#include "Symbol.hh"
#include "Token.hh"
#include "UCS_string.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

#include "Workspace.icc"
#include "u8_string.hh"

using namespace std;

/// if there is less than x chars left on the current line then leave
/// char mode and start a new line indented.
#define NEED(x)   if (space < int(x)) \
   { leave_char_mode();   outf << "\n";   space = do_indent(); } outf

//════════════════════════════════════════════════════════════════════════════
string
XML_Archive::normalize_for_checksum(const char * data, size_t len)
{
string result;
   result.reserve(len);

size_t line_start = 0;
   for (;;)
       {
         size_t line_end = line_start;
         while (line_end < len && data[line_end] != '\n'
                                && data[line_end] != '\r')   ++line_end;

         // trim leading/trailing horizontal whitespace of this line
         size_t s = line_start;
         size_t e = line_end;
         while (s < e && (data[s] == ' ' || data[s] == '\t'))     ++s;
         while (e > s && (data[e-1] == ' ' || data[e-1] == '\t')) --e;

         result.append(data + s, e - s);

         if (line_end >= len)   break;   // was the last line

         // skip the line ending (\n, \r, or \r\n) as a single boundary;
         // it contributes nothing to the normalized result.
         line_start = line_end + 1;
         if (data[line_end] == '\r' && line_start < len
                                     && data[line_start] == '\n')
            ++line_start;
       }

   return result;
}
//════════════════════════════════════════════════════════════════════════════
XML_Saving_Archive::XML_Saving_Archive(ostream & of, ostream & ef,
                                       const char * filename)
  : XML_Archive(of, ef),
     char_mode(false),
     indent(0),
     filename(filename),
     save_success(false)
{
   // Write to a temporary file and rename it onto the real target only
   // once save() has fully succeeded, rather than truncating filename
   // directly: save() can throw partway through (e.g. Bugs28 #29's
   // value-operand derived function on the )SI, or any other future
   // write-time error), which used to leave filename itself half
   // written -- destroying a perfectly good previous workspace file
   // with a truncated, unloadable one. rename() within the same
   // directory is atomic, so a crash between the write and the rename
   // still leaves either the old file or the fully-written new one,
   // never a partial file under the real name.
   //
   tmp_path = string(filename) + ".tmp";
   outf.open(tmp_path.c_str(), ofstream::out);
   if (!outf.is_open())   // open() failed
      {
        err << "Unable to )SAVE workspace '" << filename
            << "'. " << strerror(errno) << endl;
        return;
      }

   Log(LOG_archive)   err << "saving XML_Saving_Archive..." << endl;
   try
      {
        save();
      }
   catch (...)
      {
        outf.close();
        unlink(tmp_path.c_str());
        throw;
      }
   Log(LOG_archive)   err << "done XML_Saving_Archive." << endl;

   outf.close();
   if (rename(tmp_path.c_str(), filename) != 0)
      {
        err << "Unable to rename temporary workspace file '" << tmp_path
            << "' to '" << filename << "'. " << strerror(errno) << endl;
        unlink(tmp_path.c_str());
        return;
      }

   save_success = true;
}
//────────────────────────────────────────────────────────────────────────────
XML_Saving_Archive &
XML_Saving_Archive::save()
{
   Log(LOG_archive)   err << "save()" << endl;

   write_XML_header();

   ++indent;

   Log(LOG_archive)   err << "save() saves functions..." << endl;
   save_functions();

   // collect all values to be saved. We mark the values to avoid
   // saving of stale values and unmark the used values
   //
   Log(LOG_archive)   err << "save() marks values..." << endl;
   Value::mark_all_dynamic_values();

   Log(LOG_archive)   err << "save() unmarks values..." << endl;
   Workspace::unmark_all_values();

   ShapeItem value_count = 0;
   for (const DynamicObject * dob = DynamicObject::get_all_values()->get_next();
        dob != DynamicObject::get_all_values(); dob = dob->get_next())
       {
         // WARNING: do not use pValue() here !
         const cValue * val = static_cast<const Value *>(dob);

         if (val->is_marked())    continue;   // stale

         ++value_count;
       }

   Log(LOG_archive)
      err << "save() allocates values[" << value_count << "]..." << endl;

   try
      {
        val_pars.reserve(value_count);
      }
   catch(std::bad_alloc &)
      {
        MORE_ERROR() <<
           "XML_Saving_Archive::save() could not allocate values["
               << value_count << "]: (lack of memory).\n"
            "Your workspace may be corrupt (possibly due "
               "to a previous WS FULL ?).\n"
            "You should not )SAVE it. Maybe )DUMP the workspace instead?."
           ;
         WS_FULL;
      }
   catch(...)
      { FIXME; }
   Log(LOG_archive)   err << "save() done allocating values[]..." << endl;

   for (const DynamicObject * dob = DynamicObject::get_all_values()->get_next();
        dob != DynamicObject::get_all_values(); dob = dob->get_next())
       {
         // WARNING: do not use pValue() here !
         const cValue * val = static_cast<const Value *>(dob);

         if (val->is_marked())    continue;   // stale

         val->unmark();
         const _val_par vp(val, INVALID_VID);   // a value with no parent
         val_pars.push_back(vp);
       }

   // some people use an excessive number of values. We therefore sort them
   // by the address of the value as to speed up finding them later on
   //
   Heapsort<_val_par>::sort(val_pars, &_val_par::greater, 0);
   loop(v, (val_pars.size() - 1))
       {
         Assert(&val_pars[v]._val < &val_pars[v + 1]._val);
       }

   // set up parents of values
   //
   loop(p, value_count)   // for every (parent-) value
      {
        const cValue & parent = *val_pars[p]._val;
        const ShapeItem ec = parent.nz_element_count();
        loop(e, ec)   // for every ravel cell of the (parent-) value
            {
              if (parent.is_lval_cell(e))
                 {
                   Log(LOG_archive)
                      err << "LVAL CELL in " << p << " at " LOC << endl;
                   continue;
                 }

              if (!parent.is_pointer_cell(e))   continue;

              // from here on, e is the index of a PointerCell of the parent...
              //
              const cValue * sub = parent.get_pointer_value(e).get();
              Assert1(sub);
              const Vid sub_idx = find_vid(sub);
              Assert(sub_idx < value_count);

              // check for multiple parents of the same value. This is an
              // error in the old cloning scheme, but not in the new one.
              //
#ifdef NEW_CLONE
              if (false)   // never (new scheme)
#else
              if (val_pars[sub_idx]._par != INVALID_VID)
#endif
                 {
                   // sub already has a parent, which supposedly cannot
                   // happen. Print some more information about this
                   // case.
                   //
                   err << "*** Sub-Value "
                        << voidP(sub) << " has two parents."      << endl
                        << "Child: vid=" << sub_idx << ", _val="
                        << val_pars[sub_idx]._val << ", _par="
                        << val_pars[sub_idx]._par                 << endl
                        << "Parent 2: vid=" << p <<  ", _val="
                        << val_pars[p]._val << "_par="
                        << val_pars[p]._par                       << endl
                        << "Call stack:"                          << endl;
                        BACKTRACE
                   err << endl << " Running )CHECK..." << endl;
                   UCS_string no_arg;
                   Cmd_DIAG::cmd_CHECK(err, no_arg);
                   err << endl;

#if cfg_VALUE_HISTORY_WANTED
   VH_entry::print_history(err, *sub, 0);
   VH_entry::print_history(err, *val_pars[sub_idx]._val, 0);
   VH_entry::print_history(err, *val_pars[p]._val, 0);
#endif

   err << endl <<
"The workspace will be )SAVEd, but using it for anything other than for\n"
" recovering its content (i.e. defined functions or variables) means\n"
" asking for BIG trouble!" << endl;
                 }

              val_pars[sub_idx] = _val_par(val_pars[sub_idx]._val, Vid(p));
            } //   end of loop(e, ec)   (parent ravel cells_
      }   // end of loop(p, value_count) (parent values)


   // save all values (without their ravel)
   //
   do_indent();
   outf << "<!-- APL values... -->" << endl;
   loop(vid, value_count)   save_shape(Vid(vid));

   // save ravels of all values. Ordered by increasing level
   //
   outf << endl;
   do_indent();
   outf << "<!-- Ravels of APL values... -->" << endl;
ShapeItem done_count = 0;
   for (ShapeItem depth = 0; done_count != value_count; ++depth)
       {
          loop(vid, value_count)
              {
                if (val_pars[vid]._depth != depth)   continue;
                save_Ravel(Vid(vid));
                ++done_count;
              }
       }

   // save user defined symbols
   //
   save_symtab(Workspace::get_symbol_table());

   // save all system variables. Just for completeness, not all of
   // them will be restired on )LOAD.
   //
   do_indent();
   outf << "<!-- APL system variables... -->" << endl;

#define rw_sv_def(x, _str, _txt) save_Symbol(Workspace::get_v_ ## x());
#define ro_sv_def(x, _str, _txt) save_Symbol(Workspace::get_v_ ## x());
#include "SystemVariable.def"

   // save user-defined commands (if any)
   //
   save_user_commands(Workspace::get_user_commands());

   // save state indicator
   //
   {
     outf << endl;
     do_indent();
     outf << "<!-- State Indicator -->" << endl;
     const int levels = Workspace::SI_entry_count();
     do_indent();
     outf << "<StateIndicator levels=\"" << levels << "\">" << endl;

     ++indent;

     loop(l, levels)
        {
          for (const StateIndicator * si = Workspace::SI_top();
               si; si = si->get_parent())
              {
                if (si->get_level() == l)
                   {
                     save_SI_entry(*si);
                     break;
                   }
              }
        }

     --indent;

     do_indent();
     outf << "</StateIndicator>" << endl << endl;
   }

   --indent;

   do_indent();

   // write closing tag ...
   //
   outf << "</Workspace>" << endl;

   write_checksum();

   // ... and a few 0's so that string functions can be used if the
   // file should be mmap()ed.
   //
   outf << char(0) << char(0) << char(0) << char(0) << endl;

   return *this;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::write_checksum()
{
   outf.flush();

   // re-read the file just written -- simplest way to get at the exact
   // bytes without threading a tee through every outf << call above.
   //
ifstream in(tmp_path, ifstream::binary);
   if (!in.is_open())   return;   // best effort; )SAVE itself succeeded

string content((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());

const char * const ws_open  = "<Workspace ";
const size_t open_pos = content.find(ws_open);
   if (open_pos == string::npos)   return;   // shouldn't happen

const char * const ws_close = "</Workspace>";
const size_t close_pos = content.rfind(ws_close);
   if (close_pos == string::npos || close_pos < open_pos)   return;

const size_t ws_len = close_pos + strlen(ws_close) - open_pos;
const string normalized = normalize_for_checksum(content.data() + open_pos,
                                                 ws_len);
const uint32_t crc = apl_crc::crc32(normalized.data(), normalized.size());

char cc[20];
   SPRINTF(cc, "%8.8X", crc);
   outf << checksum_prefix() << cc << " -->" << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_Derived(const DerivedFunctionCache & fns)
{
   loop(f, fns.size())
      {
        const DerivedFunction & derived = fns[f];
        save_Function(derived);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_Function(const Function & fun)
{
   do_indent();
   outf << "<Function fid=\"" << HEX(&fun) << "\"";

   if (is_saved(&fun))
      {
         outf << " ref=\"true\"/>"    << endl;
      }
   else if (fun.is_macro())
      {
        const UserFunction * ufun = fun.get_func_ufun();
        Assert(ufun);

        outf << " macro=\"" << ufun->get_macnum() << "\"/>" << endl;
      }
   else if (fun.is_derived())
      {
        err << endl <<
"WARNING: The )SI stack contains a derived function. )SAVEing a workspace in\n"
"         such a state is currently not supported and WILL cause problems\n"
"         when )LOADing the workspace. Please perform )SIC (or →) and then\n"
"         )SAVE this workspace again.\n"
"\n"
"         As a precaution for loosing data or function definitions, please\n"
"         also )DUMP this workspace (before )SIC or →).\n"
             << endl;

        const DerivedFunction & dfn = static_cast<const DerivedFunction &>(fun);

        // LO/RO can each be bound to a VALUE instead of a function (e.g.
        // ⍤'s own right operand in F⍤y, y a plain value) -- get_LO()/
        // get_RO() call Token::get_function() unconditionally once the
        // tag isn't TOK_VOID, which SYNTAX_ERRORs on a value token. That
        // used to abort )SAVE half-way through writing this element,
        // leaving a truncated file that then clobbered a perfectly good
        // previous one on rename (Bugs28 #29). )LOADing a derived
        // function is already documented above as unsupported/lossy, so
        // a value-bound operand is written as a "*-vid" reference (like
        // AXIS already is) rather than reproduced exactly -- the goal
        // here is only that )SAVE itself completes.
        //
        const Value_P lo_val = dfn.get_bound_LO_value();
        const Function * lo = lo_val ? 0 : dfn.get_LO();
        outf << " OPER-fid=\"" << HEX(dfn.get_OPER()) << "\"";
        if (lo)        outf << " LO-fid=\""  << HEX(lo) << "\"";
        else if (lo_val)   outf << " LO-vid=\""  << HEX(find_vid(lo_val.get())) << "\"";

        const Value_P ro_val = dfn.get_bound_RO_value();
        const Function * ro = ro_val ? 0 : dfn.get_RO();
        const cValue * axis = dfn.get_AXIS();
        if (ro || ro_val || axis)
           {
             outf << endl;
             do_indent();
             outf << "        ";
           }
        if (ro)            outf << " RO-fid=\""   << HEX(ro) << "\"";
        else if (ro_val)   outf << " RO-vid=\""   << HEX(find_vid(ro_val.get())) << "\"";
        if (axis)   outf << " AXIS-vid=\"" << HEX(find_vid(axis)) << "\"";

        outf << "/>" << endl;
      }
   else if (fun.is_defined())   // defined APL function
      {
        const int * eprops = fun.get_exec_properties();
        const APL_time_us creation_time = fun.get_creation_time();

        outf << " creation-time=\"" << creation_time << "\"" << endl
            << "                exec-properties=\""
            << eprops[0] << "," << eprops[1] << ","
            << eprops[2] << "," << eprops[3] << "\"";

        if (fun.is_native())   outf << " native=\"1\"";

        // ⎕STOP and ⎕TRACE settings are workspace state that )SAVE and
        // )LOAD keep (only )COPY drops them, LRM p. 423, Bugs34 #27)
        //
        if (const UserFunction * ufun = fun.get_func_ufun())
           {
             const std::vector<Function_Line> & stop  = ufun->get_stop_lines();
             const std::vector<Function_Line> & trace = ufun->get_trace_lines();
             if (stop.size())
                {
                  outf << " stop-lines=\"";
                  loop(s, stop.size())   outf << (s ? "," : "") << stop[s];
                  outf << "\"";
                }
             if (trace.size())
                {
                  outf << " trace-lines=\"";
                  loop(t, trace.size())   outf << (t ? "," : "") << trace[t];
                  outf << "\"";
                }
           }
        outf << ">" << endl;
        ++indent;

        save_UCS(fun.canonical(false));

        --indent;
        do_indent();
        outf << "</Function>" << endl;
        saved_Functions.push_back(&fun);
      }
   else // primitive APL function
      {
        outf << " tag=\"" << fun.get_tag()  << "\""
               "/>   <!-- primitive: " << ID::get_name_UCS(fun.get_Id())
            << " -->" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
int
XML_Saving_Archive::save_Function_name(const Function & fun)
{
   if (fun.is_derived())
      {
        err << endl <<
"WARNING: The )SI stack contains a derived function. )SAVEing a workspace in\n"
"         such a state is currently not supported and WILL cause problems\n"
"         when )LOADing the workspace. Please perform )SIC (or →) and then\n"
"         )SAVE this workspace again.\n"
"\n"
"         As an alternative (and to be on the safe side), you should also\n"
"         consider to )DUMP this workspace.\n"
             << endl;
      }

const UserFunction * ufun = fun.get_func_ufun();
   if (ufun)   // user defined function
      {
        const UCS_string & fname = ufun->get_name();
        Symbol * sym = Workspace::lookup_symbol(fname);
        Assert(sym);
        const int sym_depth = sym->get_exec_ufun_depth(ufun);
        outf << " ufun-name=\""  << fname     << "\""
            << " symbol-level=\"" << sym_depth << "\"";
        return 2;   // two attributes
      }
   else        // primitive or quad function
      {
        outf << " fun-id=\"" << HEX(fun.get_Id()) << "\"";
        return 1;   // one attribute
      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_functions()
{
   // save build-in functions
   //
   do_indent();
   outf << "<!-- system functions and primitives... -->" << endl;
#define sf_def(x, _str, _txt)   save_Function(x::fun);
#define pf_def(x, _str, _txt)   save_Function(x::fun);
#include "SystemVariable.def"

   outf << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_Parser(const StateIndicator & si)
{
const Prefix & prefix = si.current_stack;

   do_indent();
    outf << "<Parser size=\""      << prefix.ssize()
        << "\" assign-pending=\"" << prefix.get_assign_state()
        << "\" action=\""         << prefix.action
        << "\" lookahead-high=\"" << prefix.get_lookahead_PC()
        << "\" prefix-len=\""     << prefix.prefix_len   // for ⎕L/⎕R
        << "\">" << endl;

   ++indent;

   // write the derived functions cache...
   //
   save_Derived(si.fun_oper_cache);

   // write the lookahead token, starting at the fifo's get position
   //
   loop(s, prefix.ssize())
      {
        const Token_loc & tloc = prefix.at(prefix.ssize() - s - 1);
        save_token_loc(tloc);
      }

   // write the remaining token
   //
   save_token_loc(prefix.saved_MISC);

   --indent;

   do_indent();
   outf << "</Parser>" << endl;
}
//────────────────────────────────────────────────────────────────────────────
XML_Saving_Archive &
XML_Saving_Archive::save_Ravel(Vid vid)
{
   Log(LOG_archive)   err << "save_Ravel(Vid " << vid << ")" << endl;

const cValue & v = *val_pars[vid]._val;
const APL_types::Depth depth = val_pars[vid]._depth;
const ShapeItem len = v.nz_element_count();
int space = do_indent();
   {
     // print the start of the XML element
     char cc[80];
     SPRINTF(cc, "<Ravel vid=\"%d\" depth=\"%d\" cells=\"", int(vid), depth);
     outf << decr(space, cc);
   }

   // print the data of the 'cells' attribute
   // get_cravel() materialises packed elements (any RPT_) via the fetcher
   ++indent;
   Cell cache;
   loop(l, len)   emit_cell(v.get_cravel(l, cache), space);

   space -= leave_char_mode();
   space -= 2;

   outf << "\"/>" << endl;
   --indent;

   return *this;
}
//────────────────────────────────────────────────────────────────────────────
XML_Saving_Archive &
XML_Saving_Archive::save_shape(Vid vid)
{
const cValue & v = *val_pars[vid]._val;
const Vid parent_vid = val_pars[vid]._par;

   do_indent();
   outf << "<Value flg=\"" << HEX(v.get_flags()) << "\" "
                 "vid=\"" << vid                << "\" "
                 "parent=\"" << parent_vid      << "\" "
                 "rk=\"" << v.get_rank()        << "\"";

   loop (r, v.get_rank())
      {
        outf << " sh-" << r << "=\"" << v.get_shape_item(r) << "\"";
      }

   outf << "/>" << endl;
   return *this;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_SI_entry(const StateIndicator & si)
{
   Log(LOG_archive)   err << "save_SI_entry()" << endl;

const Executable & exec = *si.get_executable();

   do_indent();
   outf << "<SI-entry level=\"" << si.get_level()
       << "\" pc=\"" << si.get_PC()
       << "\" line=\"" << exec.get_line(si.get_PC()) << "\""
       <<">" << endl << flush;

   ++indent;
   do_indent();
   switch(exec.get_parse_mode())
      {
        case PM_FUNCTION:
             {
               Symbol * sym = Workspace::lookup_symbol(exec.get_name());
               Assert(sym);
               const UserFunction * ufun = exec.get_exec_ufun();
               Assert(ufun);
               const int sym_depth = sym->get_exec_ufun_depth(ufun);

               if (ufun->is_macro())
                  outf << "<UserFunction macro-num=\"" << ufun->get_macnum()
                      << "\"/>" << endl;
               else if (ufun->is_lambda())
                  {
                    outf << "<UserFunction lambda-name=\"" << ufun->get_name()
                        << "\">" << endl;
                    ++indent;
                    save_UCS(ufun->canonical(false));
                    --indent;
                    do_indent();
                    outf << "</UserFunction>" << endl;
                  }
               else
                  outf << "<UserFunction ufun-name=\"" << sym->get_name()
                      << "\" symbol-level=\"" << sym_depth << "\"/>" << endl;
             }
             break;

        case PM_STATEMENT_LIST:
             outf << "<Statements>" << endl;
             ++indent;
             save_UCS(exec.get_text(0));
             --indent;
             do_indent();
             outf << "</Statements>" << endl;
               break;

        case PM_EXECUTE:
             outf << "<Execute>" << endl;
             ++indent;
             save_UCS(exec.get_text(0));
             --indent;
             do_indent();
             outf << "</Execute>" << endl;
             break;

          default: FIXME;
      }

   // print the parser states...
   //
   save_Parser(si);

   // the error (if any) that has suspended this SI entry (Bugs37 #51)
   //
   {
     const Error & error =
           StateIndicator::get_error(&si);
     if (error.get_error_code() != E_NO_ERROR)
        {
          do_indent();
          outf << "<Error code=\"" << int(error.get_error_code())
               << "\" left-caret=\"" << error.get_left_caret()
               << "\" right-caret=\"" << error.get_right_caret()
               << "\" show-locked=\"" << int(error.get_show_locked())
               << "\">" << endl;
          ++indent;
          save_UCS(error.get_error_line_1());
          save_UCS(error.get_error_line_2());
          --indent;
          do_indent();
          outf << "</Error>" << endl;
        }
   }

   --indent;

   do_indent();
   outf << "</SI-entry>" << endl << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_Symbol(const Symbol & sym)
{
   Log(LOG_archive)   err << "save_Symbol(Vid "
                           << sym.get_name() << ")" << endl;

   do_indent();
   outf << "<Symbol name=\"" << sym.get_name() << "\" stack-size=\""
       << sym.value_stack_size() << "\">" << endl;

   ++indent;
   loop(v, sym.value_stack_size())  save_vstack_item(sym[v]);
   --indent;

   do_indent();
   outf << "</Symbol>" << endl << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_symtab(const SymbolTable & symtab)
{
   Log(LOG_archive)   err << "save_symtab()" << endl;

std::vector<const Symbol *> symbols = symtab.get_all_symbols();

   // remove erased symbols
   //
   for (size_t s = 0; s < symbols.size();)
      {
        const Symbol * sym = symbols[s];
        if (sym->is_erased())
            {
              symbols[s] = symbols.back();
              symbols.pop_back();
              continue;
            }

        ++s;
      }

   outf << endl;
   do_indent();
   outf << "<!-- Symbol Table -->" << endl;
   do_indent();
   outf << "<SymbolTable size=\"" << symbols.size() << "\">" << endl;

   ++indent;

   while (symbols.size() > 0)
      {
        // set idx to the alphabetically smallest name
        //
        int idx = 0;
        for (size_t i = 1; i < symbols.size(); ++i)
            {
              if (symbols[idx]->compare(*symbols[i]) > 0)   idx = i;
            }

        const Symbol * sym = symbols[idx];
        save_Symbol(*sym);

        symbols[idx] = symbols.back();
        symbols.pop_back();
      }

   --indent;

   do_indent();
   outf << "</SymbolTable>" << endl << endl;
}
//════════════════════════════════════════════════════════════════════════════
void
XML_Saving_Archive::save_token_loc(const Token_loc & tloc)
{
   do_indent();
   outf << "<Token pc=\"" << tloc.get_PC()
       << "\" tag=\"" << HEX(tloc.get_token().get_tag()) << "\"";
   emit_token_val(tloc.get_token());

   outf << "/>" << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_UCS(const UCS_string & ucs)
{
int space = do_indent();
   outf << decr(space, "<UCS uni=\"");
   Assert(char_mode == false);
   indent += 5;
   loop(u, ucs.size())   emit_unicode(ucs[u], space);
   leave_char_mode();
   outf << "\"/>" << endl;
   space -= 2;
   indent -= 5;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_user_commands(
               const std::vector<Command::user_command> & cmds)
{
   Log(LOG_archive)   err << "save_user_commands()" << endl;
   if (cmds.size() == 0)   return;

   do_indent();
   outf << "<Commands size=\"" << cmds.size() << "\">" << endl;

   ++indent;
   loop(u, cmds.size())
      {
        const Command::user_command & ucmd = cmds[u];
        do_indent();
        outf << "<Command name=\"" << ucmd.prefix
            << "\" mode=\"" << ucmd.mode
            << "\" fun=\"" <<  ucmd.apl_function << "\"/>" << endl;
      }

   --indent;
   do_indent();
   outf << "</Commands>" << endl << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::save_vstack_item(const ValueStackItem & vsi)
{
   Log(LOG_archive)   err << "    save_vstack_item(name_class "
                           << vsi.get_NC() << ")" << endl;
   switch(vsi.get_NC())
      {
        case NC_UNUSED_USER_NAME:
             do_indent();
             outf << "<unused-name/>" << endl;
             break;

        case NC_LABEL:
             do_indent();
             outf << "<Label value=\"" << vsi.get_label() << "\"/>" << endl;
             break;

        case NC_VARIABLE:
             do_indent();
             outf << "<Variable vid=\"" << find_vid(vsi.get_val_cptr())
                 << "\"/>" << endl;
             break;

        case NC_FUNCTION:
        case NC_OPERATOR:
             save_Function(*vsi.get_function());
             break;

        case NC_SYSTEM_VAR:
             do_indent();
             outf << "<Shared-Variable key=\"" << vsi.get_key()
                 << "\"/>" << endl;
             break;

        default: Assert(0);
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Saving_Archive::is_saved(const Function * fun) const
{
   loop(f, saved_Functions.size())
       if (fun == saved_Functions[f])   return true;

   return false;
}
//────────────────────────────────────────────────────────────────────────────
int
XML_Saving_Archive::do_indent()
{
const int spaces = indent * INDENT_LEN;
   loop(s, spaces)   outf << " ";

   return 72 - spaces;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::emit_cell(const Cell & cell, int & space)
{
char cc[80];
   switch(cell.get_cell_type())
      {
        case CT_CHAR:   // uses UNI_PAD_U0, UNI_PAD_U1, and UNI_PAD_U2
             emit_unicode(cell.get_char_value(), space);
             break;

        case CT_INT:   // uses UNI_PAD_U3
             space -= leave_char_mode();
             SPRINTF(cc, "%lld", long_long(cell.get_int_value()));
             NEED(1 + strlen(cc)) << UNI_PAD_U3 << decr(--space, cc);
             break;

        case CT_FLOAT:   // uses UNI_PAD_U4 or UNI_PAD_U8
             space -= leave_char_mode();
#ifdef cfg_RATIONAL_NUMBERS_WANTED
             {
               const FloatCell & flt =
                                 reinterpret_cast<const FloatCell &>(cell);
               if (const APL_Integer denom = flt.get_denominator())
                  {
                    // a non-zero denominator indicates a rational quotient)
                    //
                    const APL_Integer numer = flt.get_numerator();
                    SPRINTF(cc, "%lld÷%lld", long_long(numer),
                                             long_long(denom));
                    NEED(1 + strlen(cc)) << UNI_PAD_U8 << decr(--space, cc);
                    break;
                  }
             }
#endif
             // Bugs10 #3 (Blake McBride): an IEEE-754 double needs 17
             // significant digits (DBL_DIG + 2) to round-trip exactly;
             // %.16g silently drops the last bit.
             SPRINTF(cc, "%.17g", double(cell.get_real_value()));
             NEED(1 + strlen(cc)) << UNI_PAD_U4 << decr(--space, cc);
             break;

        case CT_COMPLEX:   // uses UNI_PAD_U5
             space -= leave_char_mode();
             SPRINTF(cc, "%.17gJ%.17g", double(cell.get_real_value()),
                                        double(cell.get_imag_value()));
             NEED(1 + strlen(cc)) << UNI_PAD_U5 << decr(--space, cc);
             break;

        case CT_POINTER:   // uses UNI_PAD_U6
             space -= leave_char_mode();
             {
               const Vid vid = find_vid(cell.get_pointer_value().get());
               SPRINTF(cc, "%d", int(vid));
               NEED(1 + strlen(cc)) << UNI_PAD_U6 << decr(--space, cc);
             }
             break;

        case CT_CELLREF:   // uses UNI_PAD_U7
             {
             space -= leave_char_mode();
             if (cell.try_lval_value())   // cell has a valid target
                {
                  const cValue * owner = cell.get_cell_owner();
                  const long long offset = owner->get_offset(&cell);
                  const Vid vid = find_vid(owner);
                  SPRINTF(cc, "%d[%lld]", int(vid), offset);
                  NEED(1 + strlen(cc)) << UNI_PAD_U7 << decr(--space, cc);
                }
             else     // 0-cell-pointer (from selective assignment)
                {
                  SPRINTF(cc, "0");
                  NEED(2) << UNI_PAD_U7 << "0" << decr(--space, cc);
                }
             }
             break;

        default: Assert(0);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::emit_token_val(const Token & tok)
{
   switch(tok.get_ValueType())
      {
        case TV_NONE:  break;

        case TV_CHAR:  Log(LOG_archive)   err << "Saving TV_SYM Token" << endl;
                       outf << " char=\"" << int(tok.get_char_val()) << "\"";
                       break;

        case TV_INT:   Log(LOG_archive)   err << "Saving TV_INT Token" << endl;
                       outf << " int=\"" << tok.get_int_val() << "\"";
                       break;

        case TV_FLT:   Log(LOG_archive)   err << "Saving TV_FLT Token" << endl;
                       outf << " float=\"" << tok.get_flt_val() << "\"";
                       break;

        case TV_CPX:   Log(LOG_archive)   err << "Saving TV_CPX Token" << endl;
                       outf << " real=\"" << tok.get_cpx_real()
                           << "\" imag=\"" << tok.get_cpx_imag() << "\"";
                       break;

        case TV_SYM:   Log(LOG_archive)   err << "Saving TV_SYM Token" << endl;
                       {
                         Symbol * sym = tok.get_sym_ptr();
                         const UCS_string name = sym->get_name();
                         outf << " sym=\"" << name << "\"";
                       }
                       break;

        case TV_LIN:   Log(LOG_archive)   err << "Saving TV_LIN Token" << endl;
                       outf << " line=\"" << tok.get_fun_line() << "\"";
                       break;

        case TV_VAL:   {
                         Log(LOG_archive)
                            err << "Saving TV_VAL Token" << endl;

                         const Vid vid = find_vid(tok.get_apl_val().get());
                         outf << " vid=\"" << vid << "\"";
                       }
                       break;

        case TV_INDEX: {
                         Log(LOG_archive)
                            err << "Saving TV_INDEX Token" << endl;
                         const IndexExpr & idx = tok.get_index_val();
                         const int rank = idx.get_rank();
                         outf << " index=\"";
                         loop(i, rank)
                             {
                               if (i)   outf << ",";
                               const cValue * val = idx.values[i].get();
                               if (val)   outf << "vid_" << find_vid(val);
                               else       outf << "-";
                                outf << "\"";
                             }
                       }
                       break;

        case TV_FUN:   {
                         Log(LOG_archive)
                            err << "Saving TV_FUN Token" << endl;

                         cFunction_P fun = tok.get_function();
                         Assert1(fun);
                         save_Function_name(*fun);
                       }
                       break;

        default:       FIXME;

      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::emit_unicode(Unicode uni, int & space)
{
   if (uni == UNI_LF)
      {
        leave_char_mode();
        outf << UNI_PAD_U1 << "A" << "\n";
        space = do_indent();
      }
   else if (!xml_allowed(uni))
      {
        space -= leave_char_mode();
        char cc[40];
        SPRINTF(cc, "%X", uni);
        NEED(1 + strlen(cc)) << UNI_PAD_U1 << decr(space, cc);
        space--;   // PAD_U1
      }
   else 
      {
        NEED(1) << "";
        space -= enter_char_mode();
        outf << uni;
        space--;   // uni
      }
}
//────────────────────────────────────────────────────────────────────────────
XML_Saving_Archive::Vid
XML_Saving_Archive::find_vid(const cValue * value)
{
const void * item = Heapsort<_val_par>
                    ::search<const cValue *>
                    (value, val_pars, _val_par::compare, 0);
   Assert(item);
   return Vid(reinterpret_cast<const _val_par *>(item) - val_pars.data());
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Saving_Archive::write_XML_header()
{
tm t;
   {
     // gmtime() returns a pointer into a static buffer that the next
     // gmtime()/localtime()/ctime()/asctime() call anywhere in the
     // process (e.g. from another thread) may overwrite; since t's
     // fields are not read until much further down (after writing the
     // whole DTD), use the reentrant form and copy the result onto our
     // own stack immediately instead of holding onto that pointer.
     timeval now;   gettimeofday(&now, 0);
     time_t seconds = now.tv_sec;
#if MINGW_SRC
     // Windows CRT has no gmtime_r(); its closest equivalent, gmtime_s(),
     // takes the same two pointers in the opposite order.
     gmtime_s(&t, &seconds);
#else
     gmtime_r(&seconds, &t);
#endif
   }

const int offset = Workspace::get_v_Quad_TZ().get_offset();   // timezone offset

   // check with: xmllint --valid workspace.xml >/dev/null
   //
   outf <<
"<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n"
"\n"
"<!DOCTYPE Workspace\n"
"[\n"
"    <!ELEMENT Workspace (Function*, Value*, Ravel*, SymbolTable,\n"
"                         Symbol*, Commands?, StateIndicator)>\n"
"    <!ATTLIST Workspace  wsid       CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  year       CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  month      CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  day        CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  hour       CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  minute     CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  second     CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  timezone   CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  saving_SVN CDATA #REQUIRED>\n"
"    <!ATTLIST Workspace  syntax     CDATA #IMPLIED>\n"
"\n"
"        <!ELEMENT Value (#PCDATA)>\n"
"        <!ATTLIST Value flg    CDATA #REQUIRED>\n"
"        <!ATTLIST Value vid    CDATA #REQUIRED>\n"
"        <!ATTLIST Value parent CDATA #IMPLIED>\n"
"        <!ATTLIST Value rk     CDATA #REQUIRED>\n"
"        <!ATTLIST Value sh-0   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-1   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-2   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-3   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-4   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-5   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-6   CDATA #IMPLIED>\n"
"        <!ATTLIST Value sh-7   CDATA #IMPLIED>\n"
"\n"
"        <!ELEMENT Ravel (#PCDATA)>\n"
"        <!ATTLIST Ravel vid    CDATA #REQUIRED>\n"
"        <!ATTLIST Ravel bytes  CDATA #IMPLIED>\n"
"        <!ATTLIST Ravel cells  CDATA #IMPLIED>\n"
"        <!ATTLIST Ravel depth  CDATA #IMPLIED>\n"
"\n"
"        <!ELEMENT SymbolTable (Symbol*)>\n"
"        <!ATTLIST SymbolTable size CDATA #REQUIRED>\n"
"\n"
"            <!ELEMENT Symbol (unused-name | Variable | Function|\n"
"                              Label       | Shared-Variable)*>\n"
"            <!ATTLIST Symbol name       CDATA #REQUIRED>\n"
"            <!ATTLIST Symbol stack-size CDATA #REQUIRED>\n"
"\n"
"                <!ELEMENT unused-name EMPTY>\n"
"\n"
"                <!ELEMENT Variable (#PCDATA)>\n"
"                <!ATTLIST Variable vid CDATA #REQUIRED>\n"
"\n"
"                <!ELEMENT Function (UCS*)>\n"
"                <!ATTLIST Function fid             CDATA #REQUIRED>\n"
"                <!ATTLIST Function creation-time   CDATA #IMPLIED>\n"
"                <!ATTLIST Function LO-fid          CDATA #IMPLIED>\n"
"                <!ATTLIST Function OPER-fid        CDATA #IMPLIED>\n"
"                <!ATTLIST Function RO-fid          CDATA #IMPLIED>\n"
"                <!ATTLIST Function AXIS-vid        CDATA #IMPLIED>\n"
"                <!ATTLIST Function exec-properties CDATA #IMPLIED>\n"
"                <!ATTLIST Function stop-lines      CDATA #IMPLIED>\n"
"                <!ATTLIST Function trace-lines     CDATA #IMPLIED>\n"
"                <!ATTLIST Function macro           CDATA #IMPLIED>\n"
"                <!ATTLIST Function tag             CDATA #IMPLIED>\n"
"\n"
"                <!ELEMENT Label (#PCDATA)>\n"
"                <!ATTLIST Label value CDATA #REQUIRED>\n"
"\n"
"                <!ELEMENT Shared-Variable (#PCDATA)>\n"
"                <!ATTLIST Shared-Variable key CDATA #REQUIRED>\n"
"\n"
"        <!ELEMENT UCS (#PCDATA)>\n"
"        <!ATTLIST UCS uni CDATA #REQUIRED>\n"
"\n"
"        <!ELEMENT Commands (Command*)>\n"
"        <!ATTLIST Commands size CDATA #REQUIRED>\n"
"\n"
"            <!ELEMENT Command (#PCDATA)>\n"
"            <!ATTLIST Command name       CDATA #REQUIRED>\n"
"            <!ATTLIST Command mode       CDATA #REQUIRED>\n"
"            <!ATTLIST Command fun        CDATA #REQUIRED>\n"
"\n"
"        <!ELEMENT StateIndicator (SI-entry*)>\n"
"        <!ATTLIST StateIndicator levels CDATA #REQUIRED>\n"
"\n"
"            <!ELEMENT SI-entry ((Execute | Statements | UserFunction),\n"
"                                Parser+, Error?)>\n"
"            <!ATTLIST SI-entry level     CDATA #REQUIRED>\n"
"            <!ATTLIST SI-entry pc        CDATA #REQUIRED>\n"
"            <!ATTLIST SI-entry line      CDATA #REQUIRED>\n"
"\n"
"                <!ELEMENT Error (UCS, UCS)>\n"
"                <!ATTLIST Error code         CDATA #REQUIRED>\n"
"                <!ATTLIST Error left-caret   CDATA #REQUIRED>\n"
"                <!ATTLIST Error right-caret  CDATA #REQUIRED>\n"
"                <!ATTLIST Error show-locked  CDATA #REQUIRED>\n"
"\n"
"                <!ELEMENT Statements (UCS)>\n"
"\n"
"                <!ELEMENT Execute (UCS)>\n"
"\n"
"                <!ELEMENT UserFunction (#PCDATA)>\n"
"                <!ATTLIST UserFunction ufun-name       CDATA #IMPLIED>\n"
"                <!ATTLIST UserFunction macro-num       CDATA #IMPLIED>\n"
"                <!ATTLIST UserFunction lambda-name     CDATA #IMPLIED>\n"
"                <!ATTLIST UserFunction symbol-level    CDATA #IMPLIED>\n"
"\n"
"                <!ELEMENT Parser (Token*, Function*)>\n"
"                <!ATTLIST Parser size           CDATA #REQUIRED>\n"
"                <!ATTLIST Parser assign-pending CDATA #REQUIRED>\n"
"                <!ATTLIST Parser lookahead-high CDATA #REQUIRED>\n"
"                <!ATTLIST Parser action         CDATA #REQUIRED>\n"
"                <!ATTLIST Parser prefix-len     CDATA #IMPLIED>\n"

"                    <!ELEMENT Token (#PCDATA)>\n"
"                    <!ATTLIST Token pc           CDATA #REQUIRED>\n"
"                    <!ATTLIST Token tag          CDATA #REQUIRED>\n"
"                    <!ATTLIST Token char         CDATA #IMPLIED>\n"
"                    <!ATTLIST Token int          CDATA #IMPLIED>\n"
"                    <!ATTLIST Token float        CDATA #IMPLIED>\n"
"                    <!ATTLIST Token real         CDATA #IMPLIED>\n"
"                    <!ATTLIST Token imag         CDATA #IMPLIED>\n"
"                    <!ATTLIST Token sym          CDATA #IMPLIED>\n"
"                    <!ATTLIST Token line         CDATA #IMPLIED>\n"
"                    <!ATTLIST Token vid          CDATA #IMPLIED>\n"
"                    <!ATTLIST Token index        CDATA #IMPLIED>\n"
"                    <!ATTLIST Token fun-id       CDATA #IMPLIED>\n"
"                    <!ATTLIST Token ufun-name    CDATA #IMPLIED>\n"
"                    <!ATTLIST Token symbol-level CDATA #IMPLIED>\n"
"                    <!ATTLIST Token comment      CDATA #IMPLIED>\n"
"\n"
"]>\n"
"\n"
"    <!-- hour/minute/second is )SAVE time in UTC (aka. GMT).\n"
"         timezone is +/- offset to UTC in seconds.\n"
"         local time is UTC + offset -->\n"
"\n"
"<Workspace wsid=\"";

   /// libref if provided
   {
     const LibRef lib = Workspace::get_WSID().get_libref();
     if (lib != LIB_NONE)   outf << lib << " ";
   }
   outf <<
        Workspace::get_WSID().get_name()
     << "\" year=\""       << (t.tm_year + 1900)
     << "\" month=\""      << (t.tm_mon  + 1)
     << "\" day=\""        <<  t.tm_mday << "\"" << endl <<
"           hour=\""       <<  t.tm_hour
     << "\" minute=\""     <<  t.tm_min
     << "\" second=\""     <<  t.tm_sec
     << "\" timezone=\""   << offset << "\"" << endl <<
"           saving_SVN=\"" << ARCHIVE_SVN
     << "\" syntax=\"" << ASX_MAJOR << "."
                       << ASX_MINOR << "."
                       << ASX_OTHER << "\">\n" << endl;
}
//════════════════════════════════════════════════════════════════════════════
const char *
XML_Saving_Archive::decr(int & counter, const char * str)
{
   counter -= strlen(str);
   return str;
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Saving_Archive::xml_allowed(Unicode uni)
{
   if (uni < ' ')    return false;    // control chars and negative
   if (uni == '<')   return false;   // < not allowed
   if (uni == '&')   return false;   // < not allowed
   if (uni == '"')   return false;   // not allowed in "..."
   if (uni > '~')
      {
        // non-ASCII character. This is, in principle, allowed in XML but
        // may not print properly if the font used does not provide the
        // character. We therefore allow characters in ⎕AV and their alternate
        // characters except our own type markers (⁰¹²...)
        //
        if (is_iPAD_char(uni))   return false;
        if (Avec::find_char(uni) != Avec::Invalid_CHT)              return true;
        if (Avec::map_alternative_char(uni) != Avec::Invalid_CHT)   return true;
        return false;    // allowed, but may
      }
   return true;
}
//════════════════════════════════════════════════════════════════════════════
XML_Loading_Archive::XML_Loading_Archive(ostream & of, ostream & ef,
                                         const char * _filename, int & dump_fd)
   : XML_Archive(of, ef),
     attributes(0),
     copying(false),
     current_char(UNI_SPACE),
     data(0),
     end_attr(0),
     file_end(0),
     file_is_complete(false),
     file_length(0),
     file_start(0),
     filename(_filename),
     have_allowed_objects(false),
     line_no(1),
     line_start(0),
     protection(false),
     reading_vids(false),
     tag_name(0),
     valid_format(true)
{
   Log(LOG_archive)   err << "Loading workspace file " << filename << endl;

   errno = 0;

   // 1. open the workspace file. At this point, the file could be an .xml
   //    file (handled by this XML_Loading_Archive) or an .apl file processed
   //    by the caller after this constructor returns).
   //
   // 1a. .xml files are closed by this constructor, while
   // 1b. .apl files are left open by this constructo and closed by the caller.
   // 1c. the caller uses this->is_open() to detect errors in this constructor.
   //
const int fd = open(filename, O_RDONLY);
   if (fd == -1)
      {
        err << "open(" << filename << ") failed: " << strerror(errno) << endl;
        return;
      }

   // 2. determine the workspace file lenght
   //
   {
     struct stat st;
     if (fstat(fd, &st))
        {
          err << "fstat(" << filename << ") failed: "
              << strerror(errno) << endl;
          close(fd);
          return;
        }

     // a directory opens() and fstat()s just fine (with a non-zero
     // st_size), but cannot be mmap()ed (Linux: EACCES, "fd refers to a
     // non-regular file") -- handle it directly here rather than let the
     // resulting mmap() failure be misdiagnosed as WS_FULL below (Roy
     // Tobin, )COPY / )LOAD on /, a directory instead of a workspace
     // file). Same shape as the empty-file case just below.
     //
     if (S_ISDIR(st.st_mode))
        {
          close(fd);
          static const UTF8 empty_file[1] = { 0 };
          file_start = empty_file;
          file_end = empty_file;
          err << "file " << filename << " is a directory, not " << endl
               << "a GNU APL .xml or .apl file" << endl;
          valid_format = false;
          return;
        }

     file_length = st.st_size;
   }

   // an empty file trivially cannot be a valid .xml/.apl file, and
   // mmap()ping zero bytes is invalid (POSIX: EINVAL) -- handle it
   // directly here rather than let the resulting mmap() failure be
   // misdiagnosed as WS_FULL below (Roy Tobin, )COPY / )LOAD on
   // /dev/null). file_start is set to a static, safely-dereferenceable
   // (if empty) buffer rather than left 0, so is_open() (which merely
   // means "the file could be opened/read", same as for the "wrong
   // format" case below) still returns true; is_valid_format() is what
   // distinguishes this from an actually-usable file.
   //
   if (file_length == 0)
      {
        close(fd);
        static const UTF8 empty_file[1] = { 0 };
        file_start = empty_file;
        file_end = empty_file;
        err << "file " << filename << " does not " << endl
             << "have the format of a GNU APL .xml or .apl file" << endl;
        valid_format = false;
        return;
      }

   // 3. read the workspace file and set file_start on success
   //
   {
     file_start = Sys::mmap(fd, file_length);
     if (file_start == 0)
        {
          close(fd);
          WS_FULL;
        }
   }
   file_end = file_start + file_length;

   // 4. check for files NOT handled by this XML_Loading_Archive,
   //
   reset();

   if (!u8::strncmp(file_start, "#!", 2) ||   // )DUMP file
       !u8::strncmp(file_start, "<!", 2) ||   // )DUMP-HTML file
       !u8::strncmp(file_start, "⍝!", 4))     // a library
      {
        // the file was either written with )DUMP or is a library.
        // Return the open file descriptor (the destructor will unmap())
        //
        lseek(fd, 0, SEEK_SET);
        dump_fd = fd;   // leave fd open
        return;
      }

   close(fd);

   if (u8::strncmp(file_start, "<?xml", 5))   // not an xml file
      {
        err << "file " << filename << " does not " << endl
             << "have the format of a GNU APL .xml or .apl file" << endl;
        valid_format = false;
      }
}
//────────────────────────────────────────────────────────────────────────────
XML_Loading_Archive::~XML_Loading_Archive()
{
   Sys::munmap(file_start, file_length);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::check_compatibility()
{
   // new compatibility mechanism?
   //
   if (const UTF8 * syntax = find_optional_attr("syntax"))   // yes
      {
        unsigned int major = 0;
        unsigned int minor = 0;
        unsigned int other  = 0;
        // safe: read_Workspace() already verified the file ends with
        // real NUL bytes before calling us, so syntax is a genuinely
        // NUL-terminated C string within the mmap'd buffer.
        ::sscanf(reinterpret_cast<const char *>(syntax),
                 "%u.%u.%u", &major, &minor, &other);
        if (major == ASX_MAJOR && minor == ASX_MINOR)   return;
        if (major == ASX_MAJOR)
           {
              // a minor difference is compatible in both directions: new
              // attributes are optional, and unknown ones are ignored
              //
              const bool newer_file = minor > ASX_MINOR;
              err << "NOTE: this workspace was )SAVEd with syntax version "
                   << major << "." << minor << "." << other << endl
                   << "but is now )LOADed with " << (newer_file ? "an older"
                                                                : "a newer")
                   << " version " << ASX_MAJOR << "." << ASX_MINOR << "."
                   << ASX_OTHER << "." << endl
                   << "That should be OK, but features of the newer version"
                   << " will not be supported." << endl;
           }
        return;
      }

bool mismatch = false;

   // old (SVN-version based) compatibility mechanism

UCS_string saving_SVN;   // from the XML file
UCS_string current_SVN(UTF8_string(ARCHIVE_SVN));
   {
     const UTF8 * saving = find_optional_attr("saving_SVN");
     while (saving && saving < file_end && *saving != '"')
        saving_SVN << Unicode(*saving++);
   }

   if (saving_SVN.size() == 0)   // saved with very old version
      {
        mismatch = true;
        err << "WARNING: this workspace was )SAVEd with a VERY "
             << "old SVN version of GNU APL." << endl;
      }
   else if (saving_SVN != current_SVN)   // saved with different version
      {
        mismatch = true;
        err << "WARNING: this workspace was )SAVEd with SVN version "
             << saving_SVN << endl <<
        "          but is now being )LOADed with a SVN version "
             << current_SVN << " or greater" << endl;
      }

   if (mismatch)
      {
        err << "Expect problems, in particular when the )SI was not clear.\n";
        if (!copying)
           err << "In case of problems, please try )COPY instead of )LOAD."
                << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
XML_Loading_Archive::ChecksumStatus
XML_Loading_Archive::get_checksum_status(uint32_t & stored_crc,
                                         uint32_t & computed_crc) const
{
const char * const ws_open = "<Workspace ";
const UTF8 * open_pos = u8::strstr(file_start, ws_open);
   if (open_pos == 0)   return CS_NO_WORKSPACE;

   // find the LAST "</Workspace>" (mirrors the file_is_complete scan in
   // read_Workspace(), just with a pointer kept and a wider window: a
   // checksum comment plus the NUL trailer can sit between it and file_end).
   //
const char * const ws_close = "</Workspace>";
const size_t ws_close_len = strlen(ws_close);
const UTF8 * close_pos = 0;
   // file_end - ws_close_len would form a pointer before file_start (UB,
   // even though the loop's own c > open_pos guard would stop it from
   // ever being dereferenced) for a file shorter than ws_close_len bytes
   // (Blake McBride, Bugs15 #3, minor note).
   //
   if (size_t(file_end - file_start) >= ws_close_len)
      {
        for (const UTF8 * c = file_end - ws_close_len;
             (c > open_pos) && (c > file_end - 1000); --c)
            {
              if (!u8::strncmp(c, ws_close, ws_close_len))
                 { close_pos = c; break; }
            }
      }
   if (close_pos == 0)   return CS_NO_WORKSPACE;

const UTF8 * after_close = close_pos + ws_close_len;
const UTF8 * cs = u8::strstr(after_close, checksum_prefix());
   if (cs == 0)   return CS_NO_CHECKSUM;   // no checksum in this file

   cs += strlen(checksum_prefix());
   if (cs + 8 > file_end)                          return CS_NO_CHECKSUM;

   // "%8X" already bounds the conversion to exactly 8 characters; safe
   // now that has_trailing_NUL() guarantees a real NUL terminator exists
   // within the mapped file.
   //
   if (::sscanf(charP(cs), "%8X", &stored_crc) != 1)    return CS_NO_CHECKSUM;

const size_t ws_len = (after_close - open_pos);
const string normalized =
   normalize_for_checksum(charP(open_pos), ws_len);
   computed_crc = apl_crc::crc32(normalized.data(), normalized.size());

   return (computed_crc == stored_crc) ? CS_OK : CS_MISMATCH;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::verify_checksum()
{
uint32_t stored_crc = 0;
uint32_t computed_crc = 0;
   if (get_checksum_status(stored_crc, computed_crc) != CS_MISMATCH)   return;

   err << "WARNING: checksum mismatch in workspace file " << filename
       << endl
       << "         (stored crc32=" << HEX8(stored_crc)
       << ", computed crc32=" << HEX8(computed_crc) << ")." << endl
       << "         The file may have been edited outside of GNU APL. "
          "Loading it anyway." << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::check_checksum(ostream & out)
{
   // see has_trailing_NUL(); this is the OTHER public entry point (besides
   // read_Workspace()) that ends up scanning the mmap'd file via
   // get_checksum_status(), so it needs the same guard.
   //
   if (!has_trailing_NUL())
      {
        out << "WARNING - " << filename
            << ": corrupt or truncated (missing the trailing NUL bytes"
               " that )SAVE always writes)" << endl;
        return;
      }

uint32_t stored_crc = 0;
uint32_t computed_crc = 0;
   switch (get_checksum_status(stored_crc, computed_crc))
      {
        case CS_NO_WORKSPACE:
             out << "WARNING - " << filename
                 << ": not a GNU APL workspace file" << endl;
             break;

        case CS_NO_CHECKSUM:
             out << "WARNING - " << filename
                 << ": no checksum (old workspace)" << endl;
             break;

        case CS_OK:
             out << "OK      - " << filename << ": checksum OK" << endl;
             break;

        case CS_MISMATCH:
             out << "WARNING - " << filename
                 << ": checksum mismatch (stored crc32=" << HEX8(stored_crc)
                 << ", computed crc32=" << HEX8(computed_crc) << ")" << endl;
             break;
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Loading_Archive::next_tag(const char * loc)
{
   /* loc points into a read-only string. Set the variables:

      tag_name:    char after next '<'
      attributes:  char after tag_name + blank
      end_attr:    next >
    */

again:

   // read chars up to (including) '<'
   //
   while (current_char != '<')
      {
         if (get_uni())   return true;   // EOF
      }

   tag_name = data;

   // read char after <
   //
   if (get_uni())   return true;   // EOF

   if (current_char == '?')   goto again;   // processing instruction
   if (current_char == '!')                 // comment
      {
        // u8::strstr() is an unbounded C strstr() and this buffer is
        // mmap()'d, not guaranteed NUL-terminated on every )LOAD/)COPY
        // path (e.g. a file truncated mid-)SAVE) -- an unterminated "<!"
        // with no following "-->" would run off the mapping. Search
        // manually, bounded by file_end.
        const UTF8 * comment_end = 0;
        for (const UTF8 * p = tag_name; p + 3 <= file_end; ++p)
            {
              if (p[0] == '-' && p[1] == '-' && p[2] == '>')
                 { comment_end = p;   break; }
            }
        if (!comment_end)   return true;   // EOF: no closing "-->"
        comment_end += 3;
        while (data < comment_end)
           {
             if (get_uni())   return true;   // EOF
           }
        goto again;
      }
   if (current_char == '/')   get_uni();    // / start of end tag

   // read chars before attributes (if any)
   //
   for (;;)
       {
         if (current_char == ' ')   break;
         if (current_char == '/')   break;
         if (current_char == '>')   break; 
         if (get_uni())   return true;   // EOF
       }

   attributes = data;

   // read chars before end of tag
   //
   while (current_char != '>')
      {
         if (get_uni())   return true;   // EOF
      }

   end_attr = data;

/*
   err << "See tag ";
   for (const UTF8 * t = tag_name; t < attributes; ++t)   err << char(*t);
   err << " at " << loc << " line " << line_no << endl;
*/

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_vids()
{
   reset();   // skips to <Workspace>
   reading_vids = true;
   read_Workspace(true);
   reading_vids = false;

   reset();
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Workspace(bool silent)
{
   // see has_trailing_NUL()
   //
   if (!has_trailing_NUL())
      {
        err << "*** workspace file " << filename << endl
            << "    is corrupt or truncated (missing the trailing NUL"
               " bytes that )SAVE always writes)." << endl
            << "NOT LOADED/COPIED" << endl;
        return;
      }

   expect_tag("Workspace", LOC);

const int offset   = find_int_attr("timezone", false, 10);
const UTF8 * wsid  = find_mandatory_attr("wsid");

int year  = find_int_attr("year",     false, 10);
int mon   = find_int_attr("month",    false, 10);
int day   = find_int_attr("day",      false, 10);
int hour  = find_int_attr("hour",     false, 10);
int min   = find_int_attr("minute",   false, 10);
int sec   = find_int_attr("second",   false, 10);

   check_compatibility();

   sec  += offset          % 60;
   min  += (offset /   60) % 60;
   hour += (offset / 3600) % 60;
   if      (sec  >= 60)   { sec  -= 60;   ++min;  }
   else if (sec  <  0)    { sec  += 60;   --min;  }

   if      (min  >= 60)   { min  -= 60;   ++hour; }
   else if (min  <   0)   { min  += 60;   --hour; }

   if      (hour >= 24)   { hour -= 24;   ++day;  }
   else if (hour <   0)   { hour += 24;   --day;  }

bool next_month = false;
bool prev_month = false;
   switch(day)
      {
        case 32: next_month = true;
                 break;

        case 31: if (mon == 4 || mon == 6 || mon == 9 || mon == 11)
                    next_month = true;
                 break;

        case 30: if (mon == 2)   next_month = true;
                 break;

        case 29: if (mon != 2)         break;               // not february
                 if (year & 3)         next_month = true;   // not leap year
                 // the above fails if someone loads a workspace that
                 // was saved around midnight on 2/28/2100. Dont do that!
                 break;

        case 0:  prev_month = true;
                 break;

        default: break;
      }

   if      (next_month)   { day = 1; ++mon;  }
   else if (prev_month)
           {
             day = 31;   --mon;
             if (mon == 4 || mon == 6 || mon == 9 || mon == 11)   day = 30;
             else if (mon == 2)                 day = (year & 3) ? 28 : 29;
           }

   if      (mon > 12)   { mon =  1; ++year; }
   else if (mon <  1)   { mon = 12; --year; }

   Log(LOG_archive)   err << "read_Workspace() " << endl;

   // quick check that the file is complete. We look for </Workspace>",
   // starting at the end of the file and moving backwards (!). Note that
   // there is a crc32 checksum between </Workspace> and the final NUL bytes.
   //
   {
     const char * close_tag = "</Workspace>";
     const size_t close_tag_len = strlen(close_tag);
     for (const UTF8 * c = file_end - close_tag_len;
          (c > data) && (c > file_end - 200); --c)
         {
           if (!u8::strncmp(c, close_tag, 12))
              {
                file_is_complete = true;
                break;
              }
         }
   }

   if (!file_is_complete && !copying)
      {
        err <<
"*** workspace file " << filename << endl <<
"    seems to be incomplete (possibly caused by a crash on )SAVE?)\n"
"    You may still be able to )COPY from it.\n"
"\nNOT COPIED" << endl;
        return;
      }

   verify_checksum();

   // the order in which tags are written to the xml file
   //
const char * tag_order[] =
{
  "Function",
  "Value",
  "Ravel",
  "SymbolTable",
  "Symbol",
  "Commands",
  "StateIndicator",
  "/Workspace",
  0
};
const char ** tag_pos = tag_order;

   for (;;)
       {
         if (next_tag(LOC))   break;   // EOF / undecodable: stop

         // make sure that we do not move backwards in tag_order
         //
         if (!is_tag(*tag_pos))   // new tag
            {
              tag_pos++;
              for (;;)
                  {
                     if (*tag_pos == 0)
                        {
                          MORE_ERROR() << "Unexpected Tag "
                                       << UTF8_string(tag_name,
                                                      attributes - tag_name);
                          DOMAIN_ERROR;   // end of list
                        }
                     if (is_tag(*tag_pos))   break;          // found
                     ++tag_pos;
                  }
            }

         if      (is_tag("Function"))         read_Function();
         else if (is_tag("Value"))            read_Value();
         else if (is_tag("Ravel"))            read_Ravel();
         else if (is_tag("SymbolTable"))      read_SymbolTable();
         else if (is_tag("Symbol"))           read_Symbol();
         else if (is_tag("Commands"))         read_Commands();
         else if (copying)                    break;
         else if (is_tag("StateIndicator"))   read_StateIndicator();
         else if (is_tag("/Workspace"))       break;
         else    /* complain */               expect_tag("UNEXPECTED", LOC);
       }

   // loaded workspace can contain stale variables, e.g. shared vars.
   // remove them.
   //
   Value::erase_stale(LOC);

   if (reading_vids)   return;

   if (!silent)   out
        << "SAVED "
        << setfill('0') << year        << "-"
        << setw(2)      << mon         << "-"
        << setw(2)      << day         << " "
        << setw(2)      << hour        << ":"
        << setw(2)      << min         << ":"
        << setw(2)      << sec         << " "
        << Quad_TZ::gmt_label(offset)
        << setfill(' ') <<  endl;

   if (!copying)
      {
        const UTF8 * end = scan_for(wsid, '"');

        const UTF8_string wsname_utf(wsid, end - wsid);
        const UCS_string wsname_ucs(wsname_utf);

        // the wsid attribute is "lib name" if )SAVEd with a libref (see
        // XML_Saving_Archive::save()) or else "name". Bugs37 #23.
        //
        if (wsname_ucs.size() > 2               &&
            Avec::is_digit(wsname_ucs[0])       &&
            wsname_ucs[1] == UNI_SPACE)
           {
             const LibRef lib = LibRef(wsname_ucs[0] - '0');
             const UCS_string name(wsname_ucs, 2, wsname_ucs.size() - 2);
             Workspace::set_WSID(LibRef_name(lib, name));
           }
        else
           {
             const LibRef_name lib_name(wsname_ucs, true);
             Workspace::set_WSID(lib_name);
           }
      }

   if (have_allowed_objects && allowed_objects.size())
      {
        err << "NOT COPIED:";
        loop(a, allowed_objects.size())   err << " " << allowed_objects[a];
        err << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::reset()
{
   line_start = data = utf8P(file_start);
   line_no = 1;
   next_tag(LOC);
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Loading_Archive::skip_to_tag(const char * tag)
{
   for (;;)
      {
         if (next_tag(LOC))   return true;
         if (is_tag(tag))     break;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::expect_tag(const char * prefix, const char * loc) const
{
   if (!is_tag(prefix))
      {
        err << "   Got tag ";
        print_tag();
        err << " when expecting tag " << prefix
            << " at " << loc << "  line " << line_no << endl;
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Loading_Archive::is_tag(const char * prefix) const
{
   // tag_name is 0 if next_tag() has never successfully found a '<' at
   // all (e.g. reset()'s initial call hit EOF on an empty/near-empty
   // file) -- u8::strncmp(0, ...) would be a null-pointer dereference.
   if (tag_name == 0)   return false;
   return !u8::strncmp(tag_name, prefix, strlen(prefix));
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::print_tag() const
{
   loop(t, attributes - tag_name)   err << tag_name[t];
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::add_fid_function(Fid fid, cFunction_P new_fun,
                                      const char * loc)
{
   if (fun_map * map = find_fun_map(fid))   // fid exists
      {
        if (map->new_fun)   // function also exists.
           {
             // function already exists. Setting it multiple times is OK
             // as long as the pointers are the same (e.g. pointer to
             // primitive APL functions).
             //
             if (new_fun == map->new_fun)   return;

             err << "*** OVERRIDING fid " << HEX(fid)
                  << " (from " << map->loc << ") at " << loc << "***" << endl;
           }
        else
           {
             map->new_fun = new_fun;
             map->loc = loc;
           }

        return;
      }

   // new fid/function
   //
const fun_map fm =  { fid, new_fun, loc };
   fid_to_function.push_back(fm);
}
//────────────────────────────────────────────────────────────────────────────
const UTF8 *
XML_Loading_Archive::find_attr(const char * att_name, bool optional)
{
const int att_len = strlen(att_name);

   for (const UTF8 * d = attributes; d < end_attr; ++d)
       {
         if (u8::strncmp(att_name, d, att_len))   continue;
         const UTF8 * dd = d + att_len;
         // Bounded by file_end: for a tag truncated right at/after this
         // attribute name, the expected '='/'"' sequence may never
         // appear, and *x <= ' ' is true for NUL too, so an unbounded
         // scan here would run past end_attr (itself possibly stale,
         // see below) and past the mmap'd (exactly file_length bytes,
         // not NUL-terminated) file buffer.
         while (dd < file_end && *dd <= ' ')   ++dd;   // skip whitespaces
         if (dd >= file_end)   break;
         if (*dd++ != '=')   continue;

         // attribute= found. find value.
         while (dd < file_end && *dd <= ' ')   ++dd;   // skip whitespaces
         if (dd >= file_end)   break;
         if (*dd != '"')
            {
              const int offset = d - line_start;
              MORE_ERROR() << "Attribute '" << att_name
                   << "' is not properly quoted in line " << line_no << ":"
                   << offset << " of file " << filename;
              DOMAIN_ERROR;
            }
         return dd + 1;
       }

   // not found (including a truncated attribute value that ran into
   // file_end above)
   //
   if (!optional)
      {
         const int offset = attributes - line_start;
         MORE_ERROR() << "mandatory Attribute '" << att_name
              << "' is missing in line " << line_no << ":" << offset
              << " of file " << filename;
         DOMAIN_ERROR;
      }
   return 0;   // not found
}
//────────────────────────────────────────────────────────────────────────────
const UTF8 *
XML_Loading_Archive::scan_for(const UTF8 * from, char stop)
{
const UTF8 * p = from;
   while (p < file_end && *p != stop)   ++p;
   if (p >= file_end)
      {
        MORE_ERROR() << "unterminated attribute value (missing '" << stop
                     << "') in line " << line_no << " of file " << filename;
        DOMAIN_ERROR;
      }
   return p;
}
//────────────────────────────────────────────────────────────────────────────
APL_Float
XML_Loading_Archive::find_float_attr(const char * attrib)
{
const UTF8 * value = find_mandatory_attr(attrib);
const APL_Float val = u8::strtod(value, nullptr);
   return val;
}
//────────────────────────────────────────────────────────────────────────────
XML_Loading_Archive::fun_map *
XML_Loading_Archive::find_fun_map(Fid fid)
{
   loop(f, fid_to_function.size())
       {
         if (fid_to_function[f].old_fid == fid)   return &fid_to_function[f];
       }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
XML_Loading_Archive::find_function(Fid fid)
{
   if (fun_map * map = find_fun_map(fid))   return map->new_fun;
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
int64_t
XML_Loading_Archive::find_int_attr(const char * attrib, bool optional, int base)
{
const UTF8 * value = find_attr(attrib, optional);
   if (value == 0)   return -1;   // not found

const int64_t val = u8::strtoll(value, nullptr, base);
   return val;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
XML_Loading_Archive::find_lambda(const UCS_string & lambda)
{
const StateIndicator & si = *Workspace::SI_top();
const Executable & exec = *si.get_executable();
const Token_string & body = exec.get_body();

   loop(b, body.size())
      {
        const Token & tok = body[b];
        if (tok.get_ValueType() == TV_SYM)
           {
             const Symbol * sym = tok.get_sym_ptr();
             Assert(sym);
             loop(v, sym->value_stack_size())
                {
                  const ValueStackItem & vs = (*sym)[v];
                  if (vs.get_NC() & NC_FUN_OPER)
                     {
                       if (vs.get_function()->get_name() == lambda)
                          {
                            return vs.get_function();
                          }
                     }
                }
             continue;   // not found
           }
        else if (tok.get_ValueType() != TV_FUN)   continue;

        cFunction_P fun = tok.get_function();
        Assert1(fun);
        const UserFunction * ufun = fun->get_func_ufun();
        if (!ufun)   continue;   // not a user defined function

        const UCS_string & fname = ufun->get_name();
        if (fname == lambda)   return fun;
      }

   err << "find_lambda() failed for " << lambda
        << " at )SI level=" << si.get_level() << endl;
   return 0;
}
//════════════════════════════════════════════════════════════════════════════
bool
XML_Loading_Archive::get_uni()
{
   if (data >= file_end)   return true;   // EOF

int len = 0;
   // pass file_end so toUni()'s own bounds check can actually reject a
   // multi-byte lead byte whose continuation bytes would run past the
   // mmap'd (exactly file_length bytes, not NUL-terminated) buffer --
   // without it (the 3-arg call this used to be), that check is inert.
   current_char = UTF8_string::toUni(data, len, true, file_end);

   // toUni() returns len == 0 (with current_char == Invalid_Unicode) on
   // exactly the truncation it was just asked to detect via file_end.
   // data must still be treated as exhausted here: advancing by 0 would
   // otherwise leave data unchanged and current_char permanently
   // Invalid_Unicode, so every caller loop of the form
   // "while (current_char != X) { if (get_uni()) return/break; }" would
   // spin forever (get_uni() never returning true, data never moving)
   // instead of ever reaching this EOF/error path.
   if (len == 0)   return true;   // truncated/invalid -- treat as EOF

   data += len;
   if (current_char == 0x0A)   { ++line_no;   line_start = data; }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::instantiate_derived_functions(bool allocate)
{
   /* derived functions are allocated i 2 phases:

      phase 1: allocate=true and todo.cache set: create the function
      phase 2: allocate=false and todo.symptr set: store pointer
    */

   loop(d, derived_todos.size())
      {
        _derived_todo & todo = derived_todos[d];
        Assert(todo.fid != -1);

        cFunction_P fun = find_function(todo.fid);
        Assert(fun);

        if (!allocate)
           {
             if (todo.symptr)   *todo.symptr = fun;
             continue;
           }

        if (!todo.cache)   continue;
DerivedFunction * const slot = static_cast<DerivedFunction *>(todo.cache);

        Assert(todo.OPER_fid != -1);
        cFunction_P OPER = find_function(todo.OPER_fid);
        if (!OPER)
           {
             // e.g. an anonymous lambda operator ({⍶ ...}) that was on the
             // )SI: it has no <Function> element of its own (Bugs35 #43).
             // Refuse cleanly rather than asserting.
             //
             MORE_ERROR() << "the operator of a derived function on the )SI"
                             " (e.g. an anonymous lambda operator) cannot"
                             " be restored";
             // not DOMAIN_ERROR: that would build (and print) an error
             // display from the half-loaded )SI
             Error::throw_parse_error(E_DOMAIN_ERROR, LOC, LOC);
           }

        // LO_fid is -1 for a plain function bound only to an axis (no
        // operator, no left operand) -- mirrors how RO_fid/AXIS_vid are
        // already handled as optional just below. LO_arg stays 0 (no
        // left operand passed to construct_at()) in that case; tok_LO
        // is still constructed either way (Token's TOK_FUN2 constructor
        // does not itself validate the function pointer, only stores
        // it -- see Token.hh), but is only ever referenced via LO_arg,
        // so a null LO here is never actually dereferenced.
        // find_function() can legitimately come up empty here even
        // though LO_fid/RO_fid were recorded: an operand that was an
        // anonymous lambda (bound only as a suspended derived
        // function's operand, never given a name) has no symbol-table
        // entry of its own to drive saving its *own* <Function>
        // element, so nothing to resolve it against exists in the
        // loaded file. )SAVEing a derived function on the )SI is
        // already documented (the WARNING above) as producing an
        // imperfect reload -- degrade gracefully (drop the operand, as
        // an absent LO_fid/RO_fid already does just below) rather than
        // crashing the whole )LOAD (Bugs28 #29).
        //
        cFunction_P LO = 0;
        if (todo.LO_fid != -1)   LO = find_function(todo.LO_fid);
        Token tok_LO(TOK_FUN2, LO);
        Token * const LO_arg = LO ? &tok_LO : 0;

        cFunction_P RO = (todo.RO_fid != -1) ? find_function(todo.RO_fid) : 0;
        if (RO)   // dyadic operator
           {
             Token tok_RO(TOK_FUN2, RO);

             if (todo.AXIS_vid == -1)   // dyadic operator without axis
                {
                  DerivedFunctionCache::construct_at(slot, LO_arg, OPER,
                                                     &tok_RO, Value_P(), LOC);
                }
             else                       // dyadic operator with axis
                {
                  // AXIS_vid comes straight from the file index (an
                  // attacker-controlled "<Derived AXIS-vid=...>" attribute)
                  // with only the -1 (no axis) sentinel filtered above --
                  // any other out-of-range value was an OOB vector read.
                  if (todo.AXIS_vid < 0 ||
                      size_t(todo.AXIS_vid) >= values.size())
                     DOMAIN_ERROR;
                  Value_P val_X = values[todo.AXIS_vid];
                  DerivedFunctionCache::construct_at(slot, LO_arg, OPER,
                                                     &tok_RO, val_X, LOC);
                }
           }
        else                       // monadic operator
           {
             if (todo.AXIS_vid == -1)   // monadic operator without axis
                {
                  DerivedFunctionCache::construct_at(slot, LO_arg, OPER,
                                                     0, Value_P(), LOC);
                }
             else                       // monadic operator with axis
                {
                  // see the dyadic-operator-with-axis branch above.
                  if (todo.AXIS_vid < 0 ||
                      size_t(todo.AXIS_vid) >= values.size())
                     DOMAIN_ERROR;
                  Value_P X = values[todo.AXIS_vid];
                  DerivedFunctionCache::construct_at(slot, LO_arg, OPER,
                                                     0, X, LOC);
                }
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
const UTF8 *
XML_Loading_Archive::read_Cells(Value & Z, const UTF8 * input)
{
   // skip leading whitespace. Bounded by file_end: input comes from
   // find_*_attr() into the mmap'd (exactly file_length bytes, PROT_READ,
   // NOT NUL-terminated) workspace file, so an unterminated/truncated
   // attribute value would otherwise run this scan past the mapped
   // region -- *x <= ' ' is even true for a NUL byte, so a single
   // trailing NUL does not stop it either.
   while (input < file_end && *input <= ' ')   ++input;
   if (input >= file_end)
      {
        MORE_ERROR() << "corrupt/truncated workspace: unexpected end of "
                        "file while reading Ravel cells";
        DOMAIN_ERROR;
      }

int type_len = 0;
const Unicode type = UTF8_string::toUni(input, type_len, true, file_end);
   input += type_len;   // skip type in input

   // result pointer for u8::strtoll() / u8::strtod()
UTF8 * end = 0;

   switch (type)
      {
        case UNI_PAD_U0: // end of UNI_PAD_U2,      e.g. ²ASCII⁰
        case '\n':       // end of UNI_PAD_U2 (fix old bug)
             return input;;

        case UNI_PAD_U1: // hex Unicode:            e.g. ¹
        case UNI_PAD_U2: // printable ASCII,        e.g. ²ASCII⁰
             {
               // read_XML_string() needs type, so we restore it.
               input -= type_len;

               UCS_string ucs;
               input = read_XML_string(ucs, input);
               // bound by Z.more(): an over-long cells="..." attribute
               // (crafted or corrupted )LOAD/)COPY file) must not run past
               // Z's allocation -- next_ravel() returns 0 once Z is full,
               // and next_ravel_Char()/CharCell::zU() placement-new that
               // pointer unconditionally (SIGSEGV at address 0).
               loop(u, ucs.size())
                   {
                     if (!Z.more())   break;
                     Z.next_ravel_Char(ucs[u]);
                   }
             }
             return input;

        case UNI_PAD_U3: // integer,                e.g. ³
             {
               const APL_Integer val = u8::strtoll(input, &end, 10);
               Z.next_ravel_Int(val);
               input = end;
             }
             return input;

        case UNI_PAD_U4: // real,                   e.g. ⁴6675.79
             {
               const APL_Float val = u8::strtod(input, &end);
               Z.next_ravel_Float(val);
               input = end;
             }
             return input;

        case UNI_PAD_U5: // complex,                e.g. ⁵
             {
               const APL_Float real = u8::strtod(input, &end);
               // Assert() alone is a no-op at ASSERT_LEVEL 0: a missing
               // 'J' separator (crafted/corrupted workspace XML) left
               // the imaginary part parsed from whatever byte followed
               // instead of being caught. `end` can legitimately equal
               // file_end (number ran to the very end of the mapping), so
               // check that before dereferencing it.
               if (end >= file_end || *end != 'J')
                  {
                    MORE_ERROR() << "corrupt workspace: expected 'J' in "
                                    "complex cell";
                    DOMAIN_ERROR;
                  }
               ++end;
               const APL_Float imag = u8::strtod(end, &end);
               Z.next_ravel_Complex(real, imag);
               input = end;
             }
             return input;

        case UNI_PAD_U6: // pointer,                e.g. ⁶1525 (vid)
             {
               const int vid = u8::strtoll(input, &end, 10);
               // Assert() is a no-op at the documented default assert
               // level, so it cannot be relied on to reject an
               // out-of-range vid coming from (possibly hand-edited or
               // corrupted) workspace XML; use an always-enforced check.
               if (vid < 0 || vid >= int(values.size()))
                  {
                    MORE_ERROR() << "corrupt workspace: pointer vid="
                                 << vid << " out of range (0.."
                                 << (int(values.size()) - 1) << ")";
                    DOMAIN_ERROR;
                  }
               // next_ravel_Value() implements strand-literal semantics
               // (reuse a scalar's own cell instead of boxing it) -- right
               // for Parser::create_vector_value(), wrong here: a saved
               // pointer cell must be re-boxed unconditionally regardless
               // of whether the pointee happens to be a scalar, or every
               // nesting depth >= 3 silently collapses to 2 on )LOAD/)COPY
               // (Bugs9 #1, Blake McBride).
               //
               Z.next_ravel_Pointer(values[vid].get());
               input = end;
             }
             return input;

        case UNI_PAD_U7: // cellref,                e.g. ⁷
             if (input[0] == '0')    // 0-cell-pointer
                {
                  Z.next_ravel_Lval(0, 0);
                  input++;
                }
             else
                {
                  // base 10, matching the writer (Archive.cc, CT_CELLREF:
                  // SPRINTF(cc, "%d[%lld]", vid, offset) -- both %d and
                  // %lld are decimal). This used to read base 16 here,
                  // silently misinterpreting any vid/offset >= 10 -- with
                  // the range check below now enforced (rather than the
                  // former no-op Assert), that base mismatch turned a
                  // "silently wrong" bug into a hard DOMAIN ERROR on any
                  // workspace GNU APL itself saved containing such a
                  // selective-assignment lvalue.
                  const int vid = u8::strtoll(input, &end, 10);
                  if (vid < 0 || vid >= int(values.size()))
                     {
                       MORE_ERROR() << "corrupt workspace: cellref vid="
                                    << vid << " out of range (0.."
                                    << (int(values.size()) - 1) << ")";
                       DOMAIN_ERROR;
                     }
                  if (end >= file_end || *end != '[')
                     {
                       MORE_ERROR() << "corrupt workspace: malformed "
                                       "cellref (expected '[')";
                       DOMAIN_ERROR;
                     }
                  ++end;
                  const ShapeItem offset = u8::strtoll(end, &end, 10);
                  if (end >= file_end || *end != ']')
                     {
                       MORE_ERROR() << "corrupt workspace: malformed "
                                       "cellref (expected ']')";
                       DOMAIN_ERROR;
                     }
                  ++end;
                  Value * target = values[vid].get();
                  // get_wravel()'s own bounds check is Assert1(), a no-op
                  // at the default assert level too, so validate offset
                  // here (with an always-enforced check) against
                  // attacker-controlled workspace XML.
                  //
                  if (!target || offset < 0 ||
                      offset >= target->nz_element_count())
                     {
                       MORE_ERROR() << "corrupt workspace: cellref offset="
                                    << offset << " out of range";
                       DOMAIN_ERROR;
                     }
                  Z.next_ravel_Lval(&target->get_wravel(offset), target);
                  input = end;
                }
             return input;

        case UNI_PAD_U8: // rational quotient,      e.g. ⁸
             //
             // we should understand rational quotients even if we are
             // not ./configured for them.
             //
             {
               const uint64_t numer = u8::strtoll(input, &end, 10);

               // skip ÷ (which is is C3 B7 in UTF8). Assert() alone is a
               // no-op at ASSERT_LEVEL 0, and (even when enabled) its
               // *end++ side effects advance end regardless of whether
               // the byte actually matched -- a missing/malformed
               // separator let strtoll() below read from a shifted,
               // essentially arbitrary position instead of being caught.
               // `end + 1` can legitimately reach file_end, so bound the
               // 2-byte read before dereferencing it.
               if (end + 1 >= file_end ||
                   (*end & 0xFF) != 0xC3 || (*(end+1) & 0xFF) != 0xB7)
                  {
                    MORE_ERROR() << "corrupt workspace: expected '÷' in "
                                    "rational cell";
                    DOMAIN_ERROR;
                  }
               end += 2;
               const uint64_t denom = u8::strtoll(end, &end, 10);
               if (denom == 0)
                  {
                    MORE_ERROR() << "corrupt workspace: zero denominator "
                                    "in rational cell";
                    DOMAIN_ERROR;
                  }
#ifdef cfg_RATIONAL_NUMBERS_WANTED
               Z.next_ravel_Float(numer, denom);
#else
               Z.next_ravel_Float((1.0*numer)/denom);
#endif
               input = end;
             }
             return input;

        case UNI_PAD_U9: // packed boolean,         e.g. ⁹
             {
               // Assert() alone is a no-op at ASSERT_LEVEL 0: a ⁹ cell
               // marker on a value that is not bool-packed would make
               // dst below point at the first (polymorphic) Cell of an
               // ordinary ravel, and the loop then overwrites it with raw
               // bytes -- a type-confusion primitive reachable via a
               // hand-edited or corrupted )LOAD/)COPY workspace.
               if (!Z.is_bool_packed())
                  {
                    MORE_ERROR() << "corrupt workspace: packed-boolean "
                                    "cell in a non-packed value";
                    DOMAIN_ERROR;
                  }

               // Z's packed ravel was allocated (by its constructor) to hold
               // exactly ⌈element_count()/64⌉ 64-bit words == max_bytes
               // bytes. The loop below writes at most that many bytes,
               // regardless of how many hex pairs a (possibly hand-crafted)
               // workspace file supplies -- without this bound, an
               // over-long cells="..." attribute would walk past the
               // allocation (a heap buffer overflow reachable via a
               // crafted )LOAD/)COPY, since this branch, unlike the
               // per-element cases above, is not bounded by Z.more()).
               //
               const ShapeItem max_bytes = ((Z.element_count() + 63) >> 6) << 3;
               // byte n of the archive holds items 8n...8n+7, i.e. bits
               // 8(n&7)... of uint64_t word n>>3 of the ravel.
               //
               uint64_t * const dst = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
               memset(dst, 0, max_bytes);
               ShapeItem count = 0;

               for (;;)
                   {
                     if (input >= file_end)   break;   // truncated file
                     const uint8_t c0 = input[0];
                     if (c0 <= ' ')    { ++input;   continue; }   // whitespace
                     if (c0 == '"')   { ++input;   break; }   // end of ravel

                     const uint8_t c1 = (input + 1 < file_end) ? input[1] : 0;
                     const char cc[3] = { char(c0), char(c1), 0 };
                     char * end = 0;
                     const uint8_t byte = strtoll(cc, &end, 16);
                     input += end - cc;

                     const int converted = end - cc;
                     if (converted == 0)   break;   // nothing converted
                     if (count < max_bytes)   // bounded
                        {
                          dst[count >> 3] |= uint64_t(byte) << ((count & 7) << 3);
                          ++count;
                        }
                     if (converted == 1)   { ++input;   break; }
                   }
               return input;
             }

        default: Q1(type) Q1(line_no) DOMAIN_ERROR;
      }
}
//════════════════════════════════════════════════════════════════════════════
void
XML_Loading_Archive::read_Command()
{
   expect_tag("Command", LOC);

const UTF8 * name = find_mandatory_attr("name");
const UTF8 * name_end = scan_for(name, '"');
UTF8_string name_UTF(name, name_end - name);
UCS_string  name_UCS(name_UTF);

const UTF8 * fun = find_mandatory_attr("fun");
const UTF8 * fun_end = scan_for(fun, '"');
UTF8_string fun_UTF(fun, fun_end - fun);
UCS_string  fun_UCS(fun_UTF);

const int mode = find_int_attr("mode", false, 10);

Command::user_command ucmd = { name_UCS, fun_UCS, mode };
   Workspace::get_user_commands().push_back(ucmd);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Commands()
{
const int size = find_int_attr("size", false, 10);

   Log(LOG_archive)   err << "  read_Commands()" << endl;

   loop(s, size)
      {
        next_tag(LOC);
        read_Command();
      }

   next_tag(LOC);
   expect_tag("/Commands", LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Derived(StateIndicator & si, int lev)
{
   Log(LOG_archive)   err << "  read_Derived()" << endl;

const Fid fid      = find_Fid_attr("fid", false, 16);   // in the )SAVEing WS
// LO-fid is legitimately absent for a plain function bound only to an
// axis (no operator, no left operand) -- OPER-fid is the field that is
// always present for a genuine derived function (DerivedFunction's own
// constructor asserts its `oper` is never null), so that is what the
// sanity check below must key on instead.
const Fid LO_fid   = find_Fid_attr("LO-fid",   true,  16);
const Fid OPER_fid = find_Fid_attr("OPER-fid", true,  16);
const Fid RO_fid   = find_Fid_attr("RO-fid",   true,  16);
const Vid AXIS_vid = find_Vid_attr("AXIS-vid", true,  10);
Function * derived = si.fun_oper_cache.reserve_slot(LOC);

   if (OPER_fid == NO_FID)
      {
        err << "got non-derived function when expecting a derived one" << endl;
        return;
      }

_derived_todo td = { derived, 0, fid, LO_fid, OPER_fid, RO_fid,
                     AXIS_vid, LOC };
   add_fid_function(fid, derived, LOC);
   derived_todos.push_back(td);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Function()
{
const Fid fid = find_Fid_attr("fid", false, 16);
const TokenTag primitive_tag = TokenTag(find_int_attr("tag", true, 16));
   if (primitive_tag == TOK_NONE)
      {
        MORE_ERROR() << "Function fid=\"" << int(fid) << "\" has an "
             "invalid or missing 'tag' attribute in line " << line_no
             << " of file " << filename;
        DOMAIN_ERROR;
      }

cFunction_P pfun = ID::get_system_function(primitive_tag);
   if (pfun == 0)
      {
        MORE_ERROR() << "Function fid=\"" << int(fid) << "\", tag="
             << int(primitive_tag) << " is not a known system function "
             "(line " << line_no << " of file " << filename << ")";
        DOMAIN_ERROR;
      }
   add_fid_function(fid, pfun, LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Function(int d, Symbol & symbol)
{
const Fid fid = find_Fid_attr("fid", false, 16);
const Macro::Macro_num macnum =
                       Macro::Macro_num(find_int_attr("macro", true, 10));
   if (macnum != Macro::MAC_NONE)   // function is a macro
      {
        Macro * macro = Macro::get_macro(macnum);
        Assert(macro);
        symbol.push_function(macro);
        add_fid_function(fid, macro, LOC);
        return;
      }

const TokenTag primitive_tag = TokenTag(find_int_attr("tag", true, 16));
   if (primitive_tag != TOK_NONE)   // function is an APL primitive
      {
        cFunction_P prim = ID::get_system_function(Id(primitive_tag >> 16));
        Assert(prim);
        symbol.push_function(prim);
        add_fid_function(fid, prim, LOC);
        return;
      }

// OPER-fid, not LO-fid, is the reliable "this is a derived function"
// signal: LO-fid is legitimately absent for a plain function bound only
// to an axis (no operator, no left operand), whereas OPER-fid is always
// present for a genuine derived function (DerivedFunction's constructor
// asserts its `oper` is never null).
const Fid OPER_fid_probe = find_Fid_attr("OPER-fid", true, 16);
   if (OPER_fid_probe != NO_FID)   // derived function
      {
        const Fid LO_fid = find_Fid_attr("LO-fid", true, 16);
        const Fid OPER_fid = OPER_fid_probe;
        const Fid RO_fid = find_Fid_attr("RO-fid", true, 16);
        const Vid AXIS_vid = find_Vid_attr("AXIS-vid", true, 16);

        symbol.push();   // placeholder (for now)
        add_fid_function(fid, 0, LOC);

        _derived_todo td = { 0, symbol.top_of_stack()->get_function_P(),
                             fid, LO_fid, OPER_fid, RO_fid, AXIS_vid, LOC };
        derived_todos.push_back(td);
        return;
      }

   if (find_optional_attr("ref"))   // function pointer
      {
        cFunction_P fun = find_function(fid);
        Assert(fun);
        symbol.push_function(fun);
        return;
      }

const int native                = find_int_attr("native", true, 10);
const APL_time_us creation_time = find_int_attr("creation-time", true, 10);
int eprops[4] = { 0, 0, 0, 0 };

   if (const UTF8 * ep = find_optional_attr("exec-properties"))
      {
        // safe: read_Workspace() already verified has_trailing_NUL().
        ::sscanf(charP(ep), "%d,%d,%d,%d",
               eprops, eprops + 1, eprops + 2, eprops+ 3);
      }

   // the ⎕STOP and ⎕TRACE line numbers (see save_Function())
   //
std::vector<Function_Line> stop_lines;
std::vector<Function_Line> trace_lines;
   {
     const UTF8 * attr[2] = { find_optional_attr("stop-lines"),
                              find_optional_attr("trace-lines") };
     std::vector<Function_Line> * lines[2] = { &stop_lines, &trace_lines };
     loop(a, 2)
        {
          for (const char * p = charP(attr[a]); p && *p;)
              {
                char * end = 0;
                const long line = strtol(p, &end, 10);
                if (end == p)   break;   // malformed: ignore the rest
                if (line > 0)   lines[a]->push_back(Function_Line(line));
                p = end;
                if (*p == ',')   ++p;
              }
        }
   }

   Log(LOG_archive)
      err << "      [" << d << "] read_Function(" << symbol.get_name()
           << ") native=" << native << endl;

   next_tag(LOC);
   expect_tag("UCS", LOC);
const UTF8 * utf = find_mandatory_attr("uni");
   next_tag(LOC);
   expect_tag("/Function", LOC);

UCS_string text;
    read_XML_string(text, utf);

   if (native == 1)
      {
        NativeFunction * nfun = NativeFunction::fix(text, symbol.get_name());
        if (nfun)   // fix succeeded
           {
             nfun->set_creation_time(creation_time);
             if (d == 0)   symbol.pop();
             symbol.push_function(nfun);
             add_fid_function(fid, nfun, LOC);
           }
        else        // fix failed
           {
             err << "   *** loading of native function " << text
                  << " failed" << endl << endl;
             // keep an existing level 0 (e.g. )COPY into a workspace that
             // has the name), Bugs35 #59
             if (d > 0)   symbol.push();
           }
      }
   else
      {
        int error = 0;
        UTF8_string filename_utf(filename);
        UCS_string creator_UCS(filename_utf);
        creator_UCS << UNI_COLON << line_no;
        UTF8_string creator(creator_UCS);

        // fix() binds the function to the current top of the symbol's
        // value stack. For a level d > 0 (a function fixed into a
        // localized name of a suspended function) that top is still level
        // d-1, e.g. the global function or variable that the local
        // shadows: push level d first, or fix() replaces (resp. refuses)
        // the level d-1 entry (Bugs35 #48).
        //
        if (d > 0)   symbol.push();

        // )COPY of a function over a (global) variable of the same name:
        // replace the variable (LRM p.423), like read_Variable() does for
        // a variable over a function. Otherwise fix() throws a DEFN ERROR
        // which aborts the entire )COPY. Bugs37 #26.
        //
        Value_P replaced_var;
        if (copying && d == 0 && symbol.value_stack_size() == 1 &&
            symbol.get_NC() == NC_VARIABLE)
           {
             replaced_var = symbol.get_apl_value();
             symbol.expunge();
           }

        UserFunction * ufun = 0;
        if (text[0] == UNI_LAMBDA)
           {
             const char * creator = ")LOAD λ";
             ufun = UserFunction::fix_lambda(symbol, text);
             ufun->increment_refcount(LOC, creator);
           }
        else
           {
             ufun = UserFunction::fix(text, error, false, LOC, creator,
                                     /* quiet = */ true);
           }

        if (ufun)
           {
             symbol.pop();   // level d (bound by fix())
             ufun->set_creation_time(creation_time);
             symbol.push_function(ufun);
             add_fid_function(fid, ufun, LOC);
             ufun->set_exec_properties(eprops);
             if (!copying && (stop_lines.size() || trace_lines.size()))
                {
                  ufun->set_trace_stop(stop_lines,  true);
                  ufun->set_trace_stop(trace_lines, false);

                  // set_trace_stop() re-parses the body
                  ufun->optimize_labels();
                  ufun->optimize_label_vectors();
                }
           }
        else
           {
             err << "    ⎕FX " << symbol.get_name() << " failed: "
                  << Workspace::more_error() << endl;
             // level d > 0 is the empty entry pushed above. Level 0 is
             // left as it was: e.g. a )COPY refused because the existing
             // function is on the )SI must not erase it (Bugs35 #59).
             if (!!replaced_var)   symbol.assign(replaced_var, false, LOC);
             add_fid_function(fid, 0, LOC);
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
XML_Loading_Archive::read_Function_name()
{
const UTF8 * fun_name = find_optional_attr("ufun-name");

   if (fun_name)   // user defined function
      {
        const int level = find_int_attr("symbol-level", false, 10);
        const UTF8 * end = scan_for(fun_name, '"');
        UTF8_string name_UTF(fun_name, end - fun_name);
        UCS_string name_UCS(name_UTF);
        if (name_UCS == ID::get_name_UCS(ID_LAMBDA))
           {
             Assert(level == -1);
             return find_lambda(name_UCS);
           }

        const Symbol & symbol = *Workspace::lookup_symbol(name_UCS);

        if (level < 0)   // e.g. λ1 of an unreduced lambda operator (Bugs35 #43)
           {
             MORE_ERROR() << "function " << name_UCS << " on the )SI cannot"
                             " be restored";
             Error::throw_parse_error(E_DOMAIN_ERROR, LOC, LOC);
           }
        Assert(level < symbol.value_stack_size());
        const ValueStackItem & vsi = symbol[level];
        Assert(vsi.get_NC() == NC_FUNCTION);
        cFunction_P fun = vsi.get_function();
        Assert(fun);
        return fun;
      }

const int fun_id = find_int_attr("fun-id", true, 16);
   if (fun_id != -1)
      {
        cFunction_P sysfun = ID::get_system_function(Id(fun_id));
        Assert(sysfun);
        return sysfun;
      }

   // not found. This can happen when the function is optional.
   //
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Label(int d, Symbol & symbol)
{
const int value = find_int_attr("value", false, 10);
   if (d == 0)   symbol.pop();
   symbol.push_label(Function_Line(value));
}
//────────────────────────────────────────────────────────────────────────────
Executable *
XML_Loading_Archive::read_lambda(const UTF8 * lambda_name)
{
UCS_string lambda = read_UCS();

Symbol dummy(ID_No_ID);
UserFunction * ufun = UserFunction::fix_lambda(dummy, lambda);
   Assert(ufun);
const char * creator = ")LOAD )SI λ";
   ufun->increment_refcount(LOC, creator);

   next_tag(LOC);
   expect_tag("/UserFunction", LOC);
   return ufun;
}
//════════════════════════════════════════════════════════════════════════════
void
XML_Loading_Archive::read_Parser(StateIndicator & si, int lev)
{
   next_tag(LOC);
   expect_tag("Parser", LOC);

   Log(LOG_archive)   err << "        read_Parser() level=" << lev << endl;

const int stack_size = find_int_attr("size",           false, 10);
const int ass_state  = find_int_attr("assign-pending", false, 10);
const int lah_high   = find_int_attr("lookahead-high", false, 10);
const int action     = find_int_attr("action",         false, 10);
const int prefix_len = find_int_attr("prefix-len",     true,  10);

Prefix & parser = si.current_stack;
   // the length of the last phrase, needed by ⎕L and ⎕R (Bugs37 #51).
   // Optional (-1) in workspaces saved by older versions.
   //
   if (prefix_len >= 0)   parser.prefix_len = prefix_len;

   parser.set_assign_state(Assign_state(ass_state));
   parser.action = R_action(action);
   parser.set_lookahead_PC(Prefix_PC(lah_high));

   // read derived functions cache
   //
   for (;;)
       {
         next_tag(LOC);
         if (is_tag("/Parser"))   break;
         if (is_tag("Token"))     break;

          read_Derived(si, lev);
       }

   // read parser token. The loop above has already read the first tag
   // (the first <Token> or </Parser>): handle it before reading the next
   // one, or the first token (the bottom of the stack) is lost (Bugs35
   // #58).
   //
   for (;;)
       {
         if (is_tag("/Parser"))   break;

         if (is_tag("Token"))
            {
              Token_loc tl;
              read_Token(tl);

              if (parser.ssize() < stack_size)   parser.push(tl);
              else                             parser.saved_MISC.copy(tl, LOC);
            }
         next_tag(LOC);
       }

   Log(LOG_archive)
      {
         err << "        ";
         parser.print_stack(err, LOC);
      }

   expect_tag("/Parser", LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Ravel()
{
   if (reading_vids)   return;

const int vid = find_int_attr("vid", false, 10);
const UTF8 * cells_utf = find_optional_attr("cells");
   if (!cells_utf)   cells_utf = find_mandatory_attr("bytes");

   Log(LOG_archive)
      err << "    read_Ravel() vid=" << vid
           << ", XML line " << line_no << " - ";

   // Assert() is a no-op at the documented default assert level, so it
   // cannot be relied on to reject an out-of-range vid coming from
   // (possibly hand-edited or corrupted) workspace XML; use an
   // always-enforced check.
   if (vid < 0 || vid >= int(values.size()))
      {
        MORE_ERROR() << "corrupt workspace: Ravel vid=" << vid
                     << " out of range (0.." << (int(values.size()) - 1) << ")";
        DOMAIN_ERROR;
      }
Value_P Z = values[vid];

   if (!Z)
      {
        Log(LOG_archive) err << "NO (vids_COPY or static)" << endl;
        return;   // )COPY with vids_COPY or static value
      }

   if (Z->is_bool_packed())   // so it can't be short and ravel is a utf8_t *
      {
        read_Cells(*Z, cells_utf);
        Log(LOG_archive)   err << "YES (packed)" << endl;
        return;
      }

   if (Z->element_count() == 0)   // then Z->has_more() is 0 and can't be used
      {
        Value_P Z0(LOC);   // a scalar to read the prototype
        read_Cells(*Z0, cells_utf);   // prototype
        Z->set_default(*Z0, LOC);
        Log(LOG_archive)   err << "YES (empty)" << endl;
      }
   else
      {
        while (Z->more())
            {
              cells_utf = read_Cells(*Z, cells_utf);
           // while (*cells_utf <= ' ')   ++cells_utf;   // trailing whitespace
            }
        Log(LOG_archive) err << "YES (" << Z->element_count()
                              << " items)" << endl;
         }
   Z->check_value(LOC);
   Z->try_pack();
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Shared_Variable(int d, Symbol & symbol)
{
// const SV_key key = find_int_attr("key", false, 10);
   if (d != 0)   symbol.push();

   err << "WARNING: workspace was )SAVEd with a shared variable "
        << symbol.get_name() << endl
        << " (shared variables are not restored by )LOAD or )COPY)" << endl;

   // symbol.share_var(key);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_SI_entry(int lev)
{
const int level = find_int_attr("level", false, 10);
int pc = find_int_attr("pc", false, 10);

   Log(LOG_archive)   err << "    read_SI_entry() level=" << level << endl;

const Executable * exec = 0;
   next_tag(LOC);
   if      (is_tag("Execute"))        exec = read_SI_Execute();
   else if (is_tag("Statements"))     exec = read_SI_Statement();
   else if (is_tag("UserFunction"))   exec = read_SI_UserFunction();
   else    Assert(0 && "Bad tag at " LOC); 

   // lev is this entry's actual position in the file (the loop counter
   // in read_StateIndicator()); level is the file-supplied <SI-entry
   // level="..."> attribute, which a hand-edited or otherwise corrupt
   // workspace file can set to anything (Bugs28 #99). A raw Assert()
   // here takes down the whole session with a C++ backtrace instead of
   // the graceful "SORRY! ... )SI stack was reconstructed to the
   // extent possible" recovery read_StateIndicator() already performs
   // for any other Error thrown while reading an <SI-entry> -- throw
   // one here too instead of asserting on a file-supplied value.
   //
   if (lev != level)
      {
        MORE_ERROR() << "corrupt workspace: SI-entry level=" << level
                     << " does not match its position (" << lev
                     << ") in the )SI stack";
        DOMAIN_ERROR;
      }
   Assert(exec);

   // pc comes straight from the file (an attacker-controlled or simply
   // hand-edited "<SI-entry pc=...>" attribute); StateIndicator::list()
   // (Workspace::list_SI(), reached by )SI/)SIS/)SINL, which can also run
   // automatically after an error) indexes the executable's body with it
   // unconditionally, so an out-of-range value must be rejected here, not
   // left to crash whichever command happens to list the SI later.
   //
   if (pc < 0 || size_t(pc) >= exec->get_body().size())
      {
        MORE_ERROR() << "corrupt workspace: SI-entry pc=" << pc
                     << " out of range (body has "
                     << exec->get_body().size() << " tokens)";
        pc = 0;
      }

   Workspace::push_SI(exec, LOC);
StateIndicator * si = Workspace::SI_top();
   Assert(si);
   si->get_prefix().goto_PC(Prefix_PC(pc));
   read_Parser(*si, lev);

   for (;;)
       {
         // skip old EOC tags
         //
         next_tag(LOC);
         if (is_tag("/SI-entry"))   break;
         if (is_tag("Error"))       read_SI_Error(*si);
       }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_SI_Error(StateIndicator & si)
{
   // the error that has suspended si (Bugs37 #51)
   //
const int code        = find_int_attr("code",        false, 10);
const int left_caret  = find_int_attr("left-caret",  false, 10);
const int right_caret = find_int_attr("right-caret", false, 10);
const int show_locked = find_int_attr("show-locked", false, 10);

UCS_string line_1;
   next_tag(LOC);
   expect_tag("UCS", LOC);
   read_XML_string(line_1, find_mandatory_attr("uni"));

UCS_string line_2;
   next_tag(LOC);
   expect_tag("UCS", LOC);
   read_XML_string(line_2, find_mandatory_attr("uni"));

   next_tag(LOC);
   expect_tag("/Error", LOC);

Error error(ErrorCode(code), LOC);
   error.set_error_line_1(UTF8_string(line_1).c_str());
   error.set_error_line_2(line_2, left_caret, right_caret);
   error.set_show_locked(show_locked != 0);
   StateIndicator::get_error(&si) = error;
}
//════════════════════════════════════════════════════════════════════════════
const Executable *
XML_Loading_Archive::read_SI_Execute()
{
   next_tag(LOC);
   expect_tag("UCS", LOC);

const UTF8 * utf = find_mandatory_attr("uni");
UCS_string text;
   read_XML_string(text, utf);
   next_tag(LOC);
   expect_tag("/Execute", LOC);

ExecuteList * exec = ExecuteList::fix(text, LOC);
   Assert(exec);
   return exec;
}
//════════════════════════════════════════════════════════════════════════════
const Executable *
XML_Loading_Archive::read_SI_Statement()
{
   next_tag(LOC);
   expect_tag("UCS", LOC);
const UTF8 * utf = find_mandatory_attr("uni");
UCS_string text;
   read_XML_string(text, utf);

   next_tag(LOC);
   expect_tag("/Statements", LOC);

StatementList * exec = StatementList::fix(text, Value_P(), LOC);
   Assert(exec);
   return exec;
}
//════════════════════════════════════════════════════════════════════════════
const Executable *
XML_Loading_Archive::read_SI_UserFunction()
{
const int macro_num = find_int_attr("macro-num", true, 10);
   if (macro_num != -1)
      return Macro::get_macro(Macro::Macro_num(macro_num));

const UTF8 * lambda_name = find_optional_attr("lambda-name");
   if (lambda_name)   return read_lambda(lambda_name);

const int level     = find_int_attr("symbol-level", false, 10);
const UTF8 * name   = find_mandatory_attr("ufun-name");
const UTF8 * n  = scan_for(name, '"');
UTF8_string name_UTF(name, n - name);
UCS_string name_UCS(name_UTF);

Symbol * symbol = Workspace::lookup_symbol(name_UCS);
   Assert(symbol);
   Assert(level >= 0);
   Assert(level < symbol->value_stack_size());
const ValueStackItem & vsi = (*symbol)[level];
   Assert(vsi.get_NC() == NC_FUNCTION || vsi.get_NC() == NC_OPERATOR);
cFunction_P fun = vsi.get_function();
   Assert(fun);
const UserFunction * ufun = fun->get_func_ufun();
   Assert(fun == ufun);

   return ufun;
}
//════════════════════════════════════════════════════════════════════════════
void
XML_Loading_Archive::read_StateIndicator()
{
   if (copying)
      {
        skip_to_tag("/StateIndicator");
        return;
      }

   Log(LOG_archive)   err << "read_StateIndicator()" << endl;

const int levels = find_int_attr("levels", false, 10);

   // Bugs28 #99 (per user direction, stricter than the report's own
   // "want"): a corrupt <SI-entry> (e.g. a level="..." that disagrees
   // with its position in the stack) used to be caught right here,
   // print a "SORRY ... )SI stack was reconstructed to the extent
   // possible" diagnostic, and then let the caller go on to treat the
   // )LOAD as having succeeded with a partially-reconstructed )SI --
   // no test ever relied on that partial reconstruction. Do not catch
   // here at all: let the Error propagate out of read_Workspace()
   // entirely, so Workspace::load_WS() can refuse the whole workspace
   // (clear it back out and report failure) instead of leaving an
   // inconsistent one active.
   //
   loop(l, levels)
      {
        next_tag(LOC);
        expect_tag("SI-entry", LOC);
        read_SI_entry(l);
        // the parsers loop eats the terminating /SI-entry
      }

   instantiate_derived_functions(true);
   instantiate_derived_functions(false);

   next_tag(LOC);
   expect_tag("/StateIndicator", LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::copy_snapshot_symbol(Symbol * symbol)
{
   if (!copying)   return;

_copy_snapshot snap;
   snap.symbol = symbol;
   snap.size = symbol->value_stack_size();
   snap.nc = snap.size ? symbol->top_of_stack()->get_NC()
                        : NC_UNUSED_USER_NAME;
   snap.value = (snap.nc == NC_VARIABLE) ? symbol->get_var_value() : Value_P();
   snap.function = (snap.nc == NC_FUNCTION || snap.nc == NC_OPERATOR)
                 ? symbol->get_function() : 0;

   copy_snapshots.push_back(snap);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::copy_restore_snapshots()
{
   loop(s, copy_snapshots.size())
      {
        _copy_snapshot & snap = copy_snapshots[s];

        // undo any net growth/shrinkage of the value stack left behind
        // by the (possibly only partially executed) d==0 mutator...
        //
        while (snap.symbol->value_stack_size() > snap.size)
              snap.symbol->pop();
        while (snap.symbol->value_stack_size() < snap.size)
              snap.symbol->push();

        // ... then rebuild the original top-of-stack frame, if it had one.
        //
        if (snap.size == 0)   continue;

        snap.symbol->pop();
        switch (snap.nc)
           {
             case NC_VARIABLE:
                  snap.symbol->push_value(snap.value);
                  break;

             case NC_FUNCTION:
             case NC_OPERATOR:
                  snap.symbol->push_function(snap.function);
                  break;

             default:
                  snap.symbol->push();
                  break;
           }
      }

   copy_snapshots.clear();
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Symbol()
{
   expect_tag("Symbol", LOC);

const UTF8 * name = find_mandatory_attr("name");
const UTF8 * name_end = scan_for(name, '"');
const int name_len = name_end - name;

UTF8_string name_UTF(name, name_len);
UCS_string  name_UCS(name_UTF);
   if (name_UCS.size() == 0)
      {
        err << "*** Warning: empty Symbol name in XML archive " << filename
             << " around line " << line_no << endl;
        skip_to_tag("/Symbol");
        return;
      }

   Log(LOG_archive)   err << "    read_Symbol() name=" << name_UCS << endl;

   // ⎕NLT and ⎕PT were removed, but could lurk around in old workspaces.
   // ⎕PW and ⎕TZ are session variables that must not )LOADed (but might be
   // )COPYd)
   //
   if (!u8::strncmp(name, "⎕NLT", name_len) ||
       !u8::strncmp(name, "⎕PT", name_len))
      {
        Log(LOG_archive)   err << "        skipped at " << LOC << endl;
        skip_to_tag("/Symbol");
        return;
      }

const int SI_depth = find_int_attr("stack-size", false, 10);

   // lookup symbol, trying ⎕xx first
   //
Symbol * symbol;
   if (name_UCS == ID::get_name_UCS(ID_LAMBDA))
      symbol = &Workspace::get_v_LAMBDA();
   else if (name_UCS == ID::get_name_UCS(ID_ALPHA))
      symbol = &Workspace::get_v_ALPHA();
   else if (name_UCS == ID::get_name_UCS(ID_ALPHA_U))
      symbol = &Workspace::get_v_ALPHA_U();
   else if (name_UCS == ID::get_name_UCS(ID_CHI))
      symbol = &Workspace::get_v_CHI();
   else if (name_UCS == ID::get_name_UCS(ID_OMEGA))
      symbol = &Workspace::get_v_OMEGA();
   else if (name_UCS == ID::get_name_UCS(ID_OMEGA_U))
      symbol = &Workspace::get_v_OMEGA_U();
   else
      symbol = Workspace::lookup_existing_symbol(name_UCS);

   // we do NOT copy if:
   //
   // 1. )PCOPY and the symbol exists, or 
   // 2.  there is an object list and this symbol is not contained in the list
   //
const bool is_protected = symbol && protection;
const bool is_selected = allowed_objects.contains(name_UCS);
bool no_copy = is_protected || (have_allowed_objects && !is_selected);

   if (reading_vids)
      {
        // we prepare vids for )COPY or )PCOPY, so we do not create a symbol
        // and care only for the top level
        //
        if (no_copy || (SI_depth == 0))
           {
             Log(LOG_archive)   err << "        skipped at " << LOC << endl;
             skip_to_tag("/Symbol");
             return;
           }

        // we have entries and copying is allowed
        //
        next_tag(LOC);
        if (is_tag("Variable"))
           {
             const Vid vid = find_Vid_attr("vid", false, 10);
             vids_COPY.push_back(vid);
           }
        skip_to_tag("/Symbol");
        return;
      }

   // in a )COPY without dedicated objects, the only system variables that
   // shall be copied are ⎕CT, ⎕FC, ⎕IO, ⎕LX, ⎕PP, ⎕PR, and ⎕RL (LRM p.
   // 423). In particular not the session variables ⎕PW and ⎕TZ. Bugs37 #25.
   //
   if (!have_allowed_objects       &&   // no dedicated object list
        copying                    &&   // )COPY
        name_UCS.size()            &&
        Avec::is_quad(name_UCS[0]) &&   // system variable
       !(name_UCS == ID::get_name_UCS(ID_Quad_CT) ||
         name_UCS == ID::get_name_UCS(ID_Quad_FC) ||
         name_UCS == ID::get_name_UCS(ID_Quad_IO) ||
         name_UCS == ID::get_name_UCS(ID_Quad_LX) ||
         name_UCS == ID::get_name_UCS(ID_Quad_PP) ||
         name_UCS == ID::get_name_UCS(ID_Quad_PR) ||
         name_UCS == ID::get_name_UCS(ID_Quad_RL)
        ))
      {
        Log(LOG_archive)   err << name_UCS << " not copied at " << LOC << endl;
        no_copy = true;
      }

   // in a )LOAD silently ignore session variables (⎕PW and ⎕TZ)
   //
   if (!copying &&   // )LOAD
        (name_UCS == ID::get_name_UCS(ID_Quad_PW) ||
         name_UCS == ID::get_name_UCS(ID_Quad_TZ)))
      {
        skip_to_tag("/Symbol");
        return;
      }

   if (copying)
      {
        if (no_copy || (SI_depth == 0))
           {
             skip_to_tag("/Symbol");
             return;
           }
      }

   // remove this symbol from allowed_objects so that we can print NOT COPIED
   // at the end for all objects that are still in the list.
   //
   loop(a, allowed_objects.size())
      {
        if (allowed_objects[a] == name_UCS)
             {
               allowed_objects[a] = allowed_objects.back();
               allowed_objects.pop_back();
               break;
           }
      }

   if (symbol == 0)
      {
        symbol = Workspace::lookup_symbol(name_UCS);
      }
   Assert(symbol);

   // remember symbol's pre-)COPY state so a )COPY that fails on a later
   // <Symbol> can restore it (Workspace::copy_WS()). No-op for )LOAD.
   //
   copy_snapshot_symbol(symbol);

   loop(d, SI_depth)
      {
        // for )COPY skip d > 0
        //
        if (copying && (d > 0))
           {
             skip_to_tag("/Symbol");
             return;
           }

        next_tag(LOC);
        if      (is_tag("unused-name"))       read_unused_name(d, *symbol);
        else if (is_tag("Variable"))          read_Variable(d, *symbol);
        else if (is_tag("Function"))          read_Function(d, *symbol);
        else if (is_tag("Label"))             read_Label(d, *symbol);
        else if (is_tag("Shared-Variable"))   read_Shared_Variable(d, *symbol);
      }

   next_tag(LOC);
   expect_tag("/Symbol", LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_SymbolTable()
{
const int size = find_int_attr("size", false, 10);

   Log(LOG_archive)   err << "  read_SymbolTable()" << endl;

   loop(s, size)
      {
        next_tag(LOC);
        read_Symbol();
      }

   next_tag(LOC);
   expect_tag("/SymbolTable", LOC);
}
//────────────────────────────────────────────────────────────────────────────
bool
XML_Loading_Archive::read_Token(Token_loc & tloc)
{
   expect_tag("Token", LOC);

   // pc comes straight from the file (see read_SI_entry()'s own pc check
   // above for the same concern); a negative value is never valid
   // regardless of which executable this saved parser-stack token belongs
   // to, so reject that much here even though the upper bound can only be
   // checked where the owning executable's body length is known.
   //
int64_t tok_pc = find_int_attr("pc", false, 10);
   if (tok_pc < 0)   tok_pc = 0;

const TokenTag tag = TokenTag(find_int_attr("tag", false, 16));
Token tok;

   switch(TokenValueType(int(tag) & int(TV_MASK)))   // cannot call get_ValueType() yet
      {
        case TV_NONE: 
               new (&tok) Token(tag);
             break;

        case TV_CHAR:
             {
               const Unicode uni = Unicode(find_int_attr("char", false, 10));
               new (&tok) Token(tag, uni);
             }
             break;

        case TV_INT:   
             {
               const int64_t ival = find_int_attr("int", false, 10);
               new (&tok) Token(tag, ival);
             }
             break;

        case TV_FLT:   
             {
               const APL_Float val = find_float_attr("float");
               new (&tok) Token(tag, val);
             }
             break;

        case TV_CPX:
             {
               const APL_Float real = find_float_attr("real");
               const APL_Float imag = find_float_attr("imag");
               new (&tok) Token(tag, real, imag);
             }
             break;

        case TV_SYM:   
             {
               const UTF8 * sym_name = find_mandatory_attr("sym");
               const UTF8 * end = scan_for(sym_name, '"');
               UTF8_string name_UTF(sym_name, end - sym_name);
               UCS_string name_UCS(name_UTF);

               Symbol * symbol = Avec::is_quad(name_UCS[0])
                               ? Workspace::lookup_existing_symbol(name_UCS)
                               : Workspace::lookup_symbol(name_UCS);
               new (&tok) Token(tag, symbol);
             }
             break;

        case TV_LIN:   
             {
               const int ival = find_int_attr("line", false, 10);
               new (&tok) Token(tag, Function_Line(ival));
             }
             break;

        case TV_VAL:   
             {
               const int vid = find_int_attr("vid", false, 10);
               // Assert() alone is a no-op at ASSERT_LEVEL 0: vid comes
               // straight from the file (a crafted/corrupted
               // )LOAD/)COPY), so an out-of-range value must be an
               // enforced error, not merely asserted.
               if (vid < 0 || vid >= int(values.size()))   DOMAIN_ERROR;
               new (&tok) Token(tag, values[vid]);
             }
             break;

        case TV_INDEX: 
             {
               const UTF8 * vids = find_mandatory_attr("index");
               IndexExpr & idx = *new IndexExpr(ASS_none, LOC);
               while (vids < file_end && *vids != '"')
                  {
                    if (*vids == ',')   ++vids;
                    if (*vids == '-')   // elided index
                       {
                         idx.add_index(Value_P());
                         ++vids;   // was missing: re-read '-' forever (OOM)
                       }
                    else                // value
                       {
                         UTF8 * end = 0;
                         Assert1(*vids == 'v');   ++vids;
                         Assert1(*vids == 'i');   ++vids;
                         Assert1(*vids == 'd');   ++vids;
                         Assert1(*vids == '_');   ++vids;
                         const int vid = u8::strtoll(vids, &end, 10);
                         if (vid < 0 || vid >= int(values.size()))
                            DOMAIN_ERROR;
                         idx.add_index(values[vid]);
                         vids = end;
                       }
                  }
               new (&tok) Token(tag, idx);
             }
             break;

        case TV_FUN:
             {
               cFunction_P fun = read_Function_name();
               Assert(fun);
               new (&tok) Token(tag, fun);
             }
             break;

        default: FIXME;
      }

   tloc.copy(Token_loc(tok, Function_PC(tok_pc)), LOC);
   return true;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
XML_Loading_Archive::read_UCS()
{
   skip_to_tag("UCS");
const UTF8 * utf = find_mandatory_attr("uni");
UCS_string text;
   read_XML_string(text, utf);
   return text;
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_unused_name(int d, Symbol & symbol)
{
   Log(LOG_archive)   err << "      [" << d << "] unused name" << endl;

   if (d == 0)   return;   // Symbol::Symbol has already created the top level

   symbol.push();
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Value()
{
   expect_tag("Value", LOC);

   // read all mandatory attributes even if they are no used in this pass
   //
const Vid vid    = find_Vid_attr("vid",    false, 10);
const int flags  = find_int_attr("flg",    false, 16);
const Vid parent = find_Vid_attr("parent", false, 10);
const int rk     = find_int_attr("rk",     false, 10);

   Log(LOG_archive)   err << "  read_Value() vid=" << vid << endl;

   if (reading_vids)
      {
         parents.push_back(parent);
         return;
      }

   // a hand-edited or corrupted workspace (which )LOAD accepts with a
   // warning only) must not create a malformed value, e.g. one with a
   // negative axis (Blake McBride, Bugs32 #26)
   //
   if (rk < 0 || rk > MAX_RANK)
      {
        MORE_ERROR() << "corrupt workspace: Value vid=" << vid
                     << " has invalid rank " << rk;
        DOMAIN_ERROR;
      }

Shape sh_value;
   loop(r, rk)
      {
        char sh[20];
        SPRINTF(sh, "sh-%d", int(r));
        const UTF8 * sh_r = find_mandatory_attr(sh);
        const ShapeItem sh_item = u8::atoll(sh_r);
        if (sh_item < 0)
           {
             MORE_ERROR() << "corrupt workspace: Value vid=" << vid
                          << " has negative shape item " << sh_item;
             DOMAIN_ERROR;
           }
        sh_value.add_shape_item(sh_item);
      }

   /* if:

      1. we do )COPY or )PCOPY, and
      2. vid is not in vids_COPY list,

      then we push 0 (so that indexing with vid still works) and ignore such
      values in read_Ravel.
    */

bool no_copy = false;   // assume the value is needed
   if (copying)
      {
        // if vid is a sub-value then find its topmost owner
        //
        int parent = vid;
        loop(steps, parents.size() + 1)
            {
              // Assert() alone is a no-op at ASSERT_LEVEL 0, and neither
              // it nor an out-of-range check catches a self-referential
              // chain (a crafted <Value vid="5" parent="5"/>): parent
              // stays a valid index forever, so the loop below would spin
              // unboundedly. Enforce the bound, and cap iterations at
              // parents.size()+1 -- a genuine (acyclic) chain can be at
              // most parents.size() long before reaching -1.
              if (parent < 0 || parent >= int(parents.size()))
                 DOMAIN_ERROR;
              if (parents[parent] == -1)   break;   // topmost owner found
              parent = parents[parent];
              if (steps == ShapeItem(parents.size()))   DOMAIN_ERROR;   // cycle
            }

        no_copy = true;   // assume the value is not needed
        loop(v, vids_COPY.size())
           {
              if (parent == vids_COPY[v])   // vid is in the list: copy
                 {
                   no_copy = false;
                   break;
                 }
           }
      }

   if (no_copy)
      {
        values.push_back(Value_P());
      }
   else
      {
        // Assert() alone is a no-op at ASSERT_LEVEL 0: vid comes from the
        // file and is expected to match the sequential push order below
        // (values.size() at this point). If a crafted/corrupted file
        // violates that, push_back() below silently desynchronizes the
        // vid -> values[] mapping the vid range checks elsewhere in this
        // file all rely on -- enforce it instead of merely asserting.
        if (vid != int(values.size()))   DOMAIN_ERROR;
        // flags is the archive's "flg=" attribute, flat-encoded the same
        // way Value::get_flags() writes it (see save_shape() above): bit
        // 3 (0x08) is member; bit 4 (0x10) is the legacy bool-packed-
        // archive marker, from before the ravel_type redesign -- no
        // longer written, but still readable in old archives.
        if (flags & 0x10)   // legacy: bool-packed archive format
           {
             Value_P val(sh_value, /* constructor allocates */ 0, LOC);
             values.push_back(val);
            }
        else
           {
             Value_P val(sh_value, LOC);
             if (flags & 0x08)   val->set_member();
             values.push_back(val);
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
XML_Loading_Archive::read_Variable(int d, Symbol & symbol)
{
const int vid = find_int_attr("vid", false, 10);

   // Assert() is a no-op at the documented default assert level, so it
   // cannot be relied on to reject an out-of-range vid coming from
   // (possibly hand-edited or corrupted) workspace XML; use an
   // always-enforced check (same rationale as read_Ravel() above). vid
   // == -1 is the legitimate "stale variable" sentinel, handled by the
   // vid == -1 check further below -- not an error here.
   if (vid != -1 && (vid < 0 || vid >= int(values.size())))
      {
        MORE_ERROR() << "corrupt workspace: Variable vid=" << vid
                     << " for " << symbol.get_name()
                     << " out of range (0.." << (int(values.size()) - 1)
                     << ")";
        DOMAIN_ERROR;
      }

   Log(LOG_archive)   err << "      [" << d << "] read_Variable() vid=" << vid
                           << " name=" << symbol.get_name() << endl;

   // some system variables are saved for troubleshooting purposes, but
   // should not be loaded...
   //
   if (symbol.is_readonly())                     return;
   if (symbol.get_name().starts_iwith("⎕NLT"))   return;   // extinct
   if (symbol.get_Id() == ID_Quad_SVE)           return;
   if (symbol.get_Id() == ID_Quad_SYL)           return;
   if (symbol.get_Id() == ID_Quad_PS)            return;

   if (vid == -1)   // stale variable
      {
        Log(LOG_archive)   err << "      " << symbol.get_name()
                                << " looks like a stale variable" << endl;
        return;
      }

   if (!values[vid])   return;   // value filtered out

   while (symbol.value_stack_size() <= d)   symbol.push();
// if (d != 0)   symbol.push();

   // )COPY of a variable over a function of the same name: replace the
   // function (LRM p.423) unless it is on the )SI (then refuse, as for a
   // function, see read_Function()). Symbol::assign() would throw a
   // SYNTAX ERROR that is reported against the suspended function's line
   // (Bugs35 #60).
   //
   if (d == 0 && symbol.value_stack_size() == 1 &&
       (symbol.get_NC() & NC_FUN_OPER))
      {
        if (const char * why = symbol.cant_be_defined())
           {
             err << "NOT COPIED: " << symbol.get_name() << " (" << why
                 << ")" << endl;
             return;
           }
        symbol.expunge();
      }

   try
      {
        symbol.assign(values[vid], true, LOC);
      }
   catch (Error &)
      {
        err << "*** Could not assign value " << *values[vid]
             << "    to variable " << symbol.get_name() << " ***" << endl;
      }
   catch (std::bad_alloc &)
      {
        err << "*** Could not assign value " << *values[vid]
             << "    to variable " << symbol.get_name() << " ***" << endl;
        WS_FULL;
      }
   catch (...)
      { FIXME; }
}
//────────────────────────────────────────────────────────────────────────────
const UTF8 *
XML_Loading_Archive::read_XML_string(UCS_string & ucs, const UTF8 * utf)
{
   /* argument 'utf' is the source for a sequence of characters.
      'utf' comes from find_attr() and is therefore NOT 0-terminated but
      instead terminated by '"'.

      The Unicodes read from 'utf' are stored in result 'ucs'.

      On return, 'utf' points to the terminating '"'.
    */

   // skip leading whitespace. Bounded by file_end: utf comes from
   // find_attr() into the mmap'd (exactly file_length bytes, PROT_READ,
   // NOT NUL-terminated) workspace file, so an unterminated/truncated
   // attribute value would otherwise run this scan past the mapped
   // region -- *x <= ' ' is even true for a NUL byte, so a single
   // trailing NUL does not stop it either.
   while (utf < file_end && *utf <= ' ')   ++utf;
   if (utf >= file_end)
      {
        MORE_ERROR() << "corrupt/truncated workspace: unexpected end of "
                        "file while reading an XML string";
        DOMAIN_ERROR;
      }

   // char mode is controlled by delimiters:
   //
   // ² (UNI_PAD_U2 = start of char mode),
   // ¹ (UNI_PAD_U1 = start of hex mode = implicit end of char mode), and
   // ⁰ (UNI_PAD_U0 = end of char mode).
   //
   // This function processes a sequence of ²strings⁰ and ¹unicodes.
   // It returns at the end of the XML attribute (") or a the next
   // micro-tag (³ ⁴ ⁵ ⁶ ⁷ ⁸ ⁹)

   for (bool char_mode = false; utf < file_end && *utf && (*utf != '"');)
       {
         // get next Unicode and advance utf
         //
         const UTF8 * from = utf;
         int len = 0;   // length of uni
         const Unicode uni = UTF8_string::toUni(utf, len, true, file_end);
         utf += len;     // skip uni

          if (char_mode && uni != '\n' && uni != UNI_PAD_U0)
             {
               ucs << uni;
               continue;
             }

         if (uni == UNI_PAD_U2)   // start of char_mode
            {
              char_mode = true;
              continue;
            }

         if (uni == UNI_PAD_U1)   // start of hex mode
            {
              char_mode = false;
              UTF8 * end = 0;
              const int hex = u8::strtoll(utf, &end, 16);
              ucs << Unicode(hex);
              utf = end;
              continue;
            }

         if (uni == UNI_PAD_U0)   // end of ²char_mode⁰
            {
              char_mode = false;
              continue;
            }

         if (uni == '\n')   // end of char_mode (fix old bug)
            {
              // due to an old bug, the trailing UNI_PAD_U0 may be missing
              // at the end of the line. We therefore reset char_mode at the
              // end of the line so that workspaces save with that fault can
              // be read.
              //
              char_mode = false;
              continue;
            }

         if (uni == ' ' || uni == '\t' || uni == '\r')   // indentation, or
            {                                            // \r of a \r\n
              // GNU APL itself only ever writes ' ' for indentation and
              // '\n' for line endings, but a text editor may convert
              // leading spaces to tabs or the file to CRLF line endings;
              // that must not crash the reader. Treat '\t' the same as
              // the ' ' it replaced, and '\r' the same as the
              // indentation it's adjacent to; the '\n' of a \r\n pair
              // (if any) is handled by the case above.
              continue;
            }

         if (!char_mode)   // outside of ²char_mode⁰
            {
              if (uni == UNI_PAD_U3 ||
                  uni == UNI_PAD_U4 ||
                  uni == UNI_PAD_U5 ||
                  uni == UNI_PAD_U6 ||
                  uni == UNI_PAD_U7 ||
                  uni == UNI_PAD_U8 ||
                  uni == UNI_PAD_U9)    return from;
            }

         FIXME   // not reached
       }

   return utf;
}
//════════════════════════════════════════════════════════════════════════════

