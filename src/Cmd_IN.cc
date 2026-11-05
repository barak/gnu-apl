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

#include "Command.hh"
#include "Function.hh"
#include "Quad_TF.hh"
#include "Symbol.hh"
#include "Sys.hh"
#include "Tokenizer.hh"
#include "Token_string.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"
#include "u8_string.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_IN::cmd_IN(ostream & out, UCS_string_vector & args, bool protection)
{
   // Command is:
   //
   // IN filename [objects...]

UCS_string fname = args.front();
   args.front() = args.back();
   args.pop_back();

const LibRef_name lib_name(LIB0, fname);
const UTF8_string filename = LibPaths::get_filename(lib_name, true, ".atf", 0);

FileReader in(filename.c_str());
   if (!in)   // open failed: try filename.atf unless already .atf
      {
        CERR << ")IN " << fname << " failed: " << strerror(errno) << endl;

        MORE_ERROR() << "command )IN: could not open file "
                     << fname << " for reading: "
                     << strerror(errno);
        return;
      }

UTF8 buffer[80];
int idx = 0;

transfer_context tctx(protection);

   for (;;)
      {
        const int cc = in.fgetc();
        if (cc == EOF)   break;
        if (idx == 0 && cc == 0x0A)   // optional LF
           {
             // CERR << "CRLF" << endl;
             continue;
           }
        if (idx < 80)
           {
              if (idx < 72)   buffer[idx++] = cc;
              else            buffer[idx++] = 0;
             continue;
           }

        if (cc == 0x0D || cc == 0x15)   // ASCII or EBCDIC
           {
             tctx.is_ebcdic = (cc == 0x15);
             tctx.process_record(buffer, args);

             idx = 0;
             ++tctx.recnum;
             continue;
           }

        CERR << "BAD record charset (neither ASCII nor EBCDIC)" << endl;
        break;
      }

   // )PIN: LRM p.447 "If a name conflict does occur, the object from the
   // transfer file is listed in a NOT COPIED: message" (Bugs35 #64)
   //
   if (tctx.not_copied.size())
      {
        out << "NOT COPIED:";
        loop(n, tctx.not_copied.size())   out << " " << tctx.not_copied[n];
        out << endl;
      }
}
//════════════════════════════════════════════════════════════════════════════
bool
Cmd_IN::transfer_context::is_protected(const UCS_string & name) const
{
   if (!protection || name.size() == 0)   return false;

bool exists = true;   // system variables always exist
   if (!Avec::is_quad(name[0]))
      {
        const Symbol * sym = Workspace::lookup_existing_symbol(name);
        exists = sym && sym->get_NC() != NC_UNUSED_USER_NAME;
      }
   if (!exists)   return false;

   if (!not_copied.contains(name))   not_copied.push_back(name);
   return true;
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_IN::transfer_context::process_record(const UTF8 * record,
                                          const UCS_string_vector & objects)
{
const char rec_type = record[0];   // '*', ' ', or 'X'
const char sub_type = record[1];

   if (rec_type == '*')   // comment or similar
      {
        // parsing the *(...) timestamp record is a side effect, not a log
        // message, and must run unconditionally: it used to be nested
        // inside Log(LOG_command_IN), which expands to 'if (x)'
        // (Logging.hh:60), and LOG_command_IN is off in every release
        // build (tools/check_src requires every Logging.def facility to
        // be shipped off) -- so )IN never restored a function's/variable's
        // creation timestamp written by )OUT, even though )OUT wrote the
        // *(YYYY MM DD hh mm ss uuuuuu) record correctly (Blake McBride,
        // Bugs16 #4).
        //
        if (sub_type == '(')   // timestamp
           {
             YMDhmsu t(now());   // fallback if sscanf() != 7
             if (7 == u8::sscanf(record + 1,
                             "(%d %d %d %d %d %d %d)",
                             &t.year, &t.month, &t.day,
                             &t.hour, &t.minute, &t.second,
                             &t.micro))
                {
                  timestamp = t.get();
                }
           }

        Log(LOG_command_IN)
           {
             const char * stype = " *** bad sub-record of *";
             switch(sub_type)
                {
                  case ' ': stype = " comment";     break;
                  case '(': stype = " timestamp";   break;
                  case 'I': stype = " imbed";       break;
                }

             CERR << "record #" << setw(3) << recnum << ": '" << rec_type
                  << "'" << stype << endl;
           }
      }
   else if (rec_type == ' ' || rec_type == 'X')   // object
      {
        if (new_record)
           {
             Log(LOG_command_IN)
                {
                  const char * stype = " *** bad sub-record of X";

//                          " -------------------------------------";
                  switch(sub_type)
                     {
                       case 'A': stype = " 2 ⎕TF array ";           break;
                       case 'C': stype = " 1 ⎕TF char array ";      break;
                       case 'F': stype = " 2 ⎕TF function ";        break;
                       case 'N': stype = " 1 ⎕TF numeric array ";   break;
                     }

                  CERR << "record #" << setw(3) << recnum
                       << ": " << stype << endl;
                }

             item_type = sub_type;
           }

        add(record + 1, 71);

        new_record = (rec_type == 'X');   // 'X' marks the final record
        if (new_record)
           {
             if      (item_type == 'A')   array_2TF(objects);
             else if (item_type == 'C')   chars_1TF(objects);
             else if (item_type == 'N')   numeric_1TF(objects);
             else if (item_type == 'F')   function_2TF(objects);
             else                         CERR << "????: " << data << endl;
             data.clear();
           }
      }
   else
      {
        CERR << "record #" << setw(3) << recnum << ": '" << rec_type << "'"
             << "*** bad record type '" << rec_type << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
uint32_t
Cmd_IN::transfer_context::get_nrs(UCS_string & name, Shape & shape) const
{
int idx = 1;

   // data + 1 is: NAME RK SHAPE RAVEL...
   //
   while (idx < data.ssize() && data[idx] != UNI_SPACE)   name << data[idx++];
   ++idx;   // skip space after the name

int rank = 0;
   while (idx < data.ssize() &&
          data[idx] >= UNI_0 &&
          data[idx] <= UNI_9)
      {
        rank *= 10;
        rank += data[idx++] - UNI_0;
      }
   ++idx;   // skip space after the rank

   loop (r, rank)
      {
        ShapeItem s = 0;
        while (idx < data.ssize() &&
               data[idx] >= UNI_0 &&
               data[idx] <= UNI_9)
           {
             s *= 10;
             s += data[idx++] - UNI_0;
           }
        shape.add_shape_item(s);
        ++idx;   // skip space after shape[r]
      }

   return idx;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_IN::transfer_context::numeric_1TF(const UCS_string_vector & objects) const
{
UCS_string var_name;
Shape shape;
int idx = get_nrs(var_name, shape);

   if (objects.size() && !objects.contains(var_name))   return;
   if (var_name.size() == 0)   return;   // malformed record: empty object name
   if (is_protected(var_name))   return;   // )PIN

Symbol * sym = 0;
   if (Avec::is_quad(var_name.front()))   // system variable.
      {
        // a record for a system function (⎕FIO, ⎕FX, ...) or for an
        // unknown ⎕-name failed an Assert() (Bugs34 #24): skip it
        //
        int len = 0;
        const Token t = Workspace::get_quad(var_name, len);
        if (len != var_name.ssize() || t.get_ValueType() != TV_SYM)
           {
             CERR << ")IN: " << var_name
                  << " is not a system variable (record skipped)" << endl;
             return;
           }
        sym = t.get_sym_ptr();
      }
   else                            // user defined variable
      {
        sym = Workspace::lookup_symbol(var_name);
        Assert(sym);
      }

   Log(LOG_command_IN)
      {
        CERR << endl << var_name << " rank " << shape.get_rank() << " IS '";
        loop(j, data.size() - idx)   CERR << data[idx + j];
        CERR << "'" << endl;
      }

Token_string tos;
   {
     UCS_string data1(data, idx);
     Tokenizer tokenizer(PM_EXECUTE, LOC, false);
     try   { tokenizer.tokenize(data1, tos); }
     catch (const Error &)   { return; }
   }
 
   if (tos.ssize() != shape.get_volume())   return;

Value_P Z(shape, LOC);
   Z->set_proto_Int();   // prototype

const ShapeItem ec = Z->element_count();
   loop(e, ec)
      {
        const Token & tok = tos[e];
        const TokenTag tag = tok.get_tag();
        if      (tag == TOK_INTEGER)  Z->next_ravel_Int(tok.get_int_val());
        else if (tag == TOK_REAL)     Z->next_ravel_Float(tok.get_flt_val());
        else if (tag == TOK_COMPLEX)  Z->next_ravel_Complex(tok.get_cpx_real(),
                                                            tok.get_cpx_imag());
        else return;   // e.g. a name or string token: malformed record,
                        // reject it the same way the other malformed
                        // records in this file already are (above).
      }
   Z->check_value(LOC);

   Assert(sym);
   sym->assign(Z, false, LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_IN::transfer_context::chars_1TF(const UCS_string_vector & objects) const
{
UCS_string var_name;
Shape shape;
int idx = get_nrs(var_name, shape);

   if (objects.size() && !objects.contains(var_name))   return;
   if (var_name.size() == 0)   return;   // malformed record: empty object name
   if (is_protected(var_name))   return;   // )PIN

Symbol * sym = 0;
   if (Avec::is_quad(var_name.front()))   // system variable.
      {
        // a record for a system function (⎕FIO, ⎕FX, ...) or for an
        // unknown ⎕-name failed an Assert() (Bugs34 #24): skip it
        //
        int len = 0;
        const Token t = Workspace::get_quad(var_name, len);
        if (len != var_name.ssize() || t.get_ValueType() != TV_SYM)
           {
             CERR << ")IN: " << var_name
                  << " is not a system variable (record skipped)" << endl;
             return;
           }
        sym = t.get_sym_ptr();
      }
   else                            // user defined variable
      {
        sym = Workspace::lookup_symbol(var_name);
        Assert(sym);
      }

   Log(LOG_command_IN)
      {
        CERR << endl << var_name << " shape " << shape << " IS: '";
        loop(j, data.size() - idx)   CERR << data[idx + j];
        CERR << "'" << endl;
      }

Value_P Z(shape, LOC);
const ShapeItem ec = Z->element_count();
   Z->set_proto_Spc();   // prototype

ShapeItem padded = 0;
   loop(e, ec)
      {
        Unicode uni = UNI_SPACE;
        if (e < (data.ssize() - idx))   uni = data[e + idx];
        else                            ++padded;
        Z->next_ravel_Char(uni);
      }

   if (padded)
      {
        CERR << "WARNING: ATF Record for " << var_name << " is broken ("
             << padded << " spaces added)" << endl;
      }

   Z->check_value(LOC);

   Assert(sym);
   sym->assign(Z, false, LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_IN::transfer_context::array_2TF(const UCS_string_vector & objects) const
{
   // an Array in 2 ⎕TF format
   //
UCS_string data1(data, 1);
UCS_string var_or_fun;

   // data1 is: VARNAME←data...
   //
   {
     UCS_string var_name;
     loop(d, data1.size())
        {
          const Unicode uni = data1[d];
          if (uni == UNI_LEFT_ARROW)   break;
          var_name << uni;
        }

     if (objects.size() && !objects.contains(var_name))   return;
     if (is_protected(var_name))   return;   // )PIN
   }

   var_or_fun = Quad_TF::tf2_inverse(data1);

   if (var_or_fun.size() == 0)
      {
        CERR << "ERROR: inverse 2 ⎕TF failed for '" << data1 << "'" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_IN::transfer_context::function_2TF(const UCS_string_vector & objects)const
{
int idx = 1;
UCS_string fun_name;

   /// chars 1...' ' are the function name
   while ((idx < data.ssize()) && (data[idx] != UNI_SPACE))
        fun_name << data[idx++];
   ++idx;

   if (objects.size() && !objects.contains(fun_name))   return;
   if (is_protected(fun_name))   return;   // )PIN

UCS_string statement;
   while (idx < data.ssize())   statement << data[idx++];
   statement << UNI_LF;

UCS_string fun_name1 = Quad_TF::tf2_inverse(statement, true);
   if (fun_name1.size() == 0)   // tf2_inverse() failed
      {
        CERR << "inverse 2 ⎕TF failed for the following APL statement: "
             << endl << "    " << statement << endl;
        return;
      }

Symbol * sym1 = Workspace::lookup_existing_symbol(fun_name1);
   Assert(sym1);
   {
     Function * fun1 = const_cast<Function *>(sym1->get_function());
     Assert(fun1);
     fun1->set_creation_time(timestamp);
   }

   Log(LOG_command_IN)
      {
       const YMDhmsu ymdhmsu(timestamp);
       CERR << "FUNCTION '" << fun_name1 <<  "'" << endl
            << "   created: " << ymdhmsu.day << "." << ymdhmsu.month
            << "." << ymdhmsu.year << "  " << ymdhmsu.hour
            << ":" << ymdhmsu.minute << ":" << ymdhmsu.second
            << "." << ymdhmsu.micro << " (" << timestamp << ")" << endl;
      }
}
//════════════════════════════════════════════════════════════════════════════
void
Cmd_IN::transfer_context::add(const UTF8 * str, int len)
{

#if 0
   // helper function to print the uni_to_cp_map table when given the inverse
   // cp_to_uni_map. Before that the IBM ⎕AV is printed
   //
   Avec::print_inverse_IBM_quad_AV();
   DOMAIN_ERROR;
#endif

   // the ATF codes of * ^ ~ are kept as the ASCII characters, so that
   // strings come back unchanged. In APL code (i.e. outside strings and
   // comments of a function line) they become ⋆ ∧ ∼ later on, see
   // Quad_TF::ATF_principal_chars() (Bugs37 #7).
   //
const Unicode * cp_to_uni_map = Avec::IBM_quad_AV();
   loop(l, len)   data << Unicode(cp_to_uni_map[str[l]]);
}
//════════════════════════════════════════════════════════════════════════════
