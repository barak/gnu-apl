/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#include <sys/time.h>

#include "Bif_F12_TAKE_DROP.hh"
#include "CDR.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Quad_CR.hh"
#include "Symbol.hh"
#include "Tokenizer.hh"
#include "UCS_string.hh"
#include "Workspace.hh"

//----------------------------------------------------------------------------
const FunctionGroup::function_info Quad_CR::subfunction_infos[] =
{
#define crdef(N, name, comm_2)   { N, #name, "", comm_2, -1 },
#include "Quad_CR.def"
};
//----------------------------------------------------------------------------
Quad_CR::Quad_CR()
 : QuadFunction(TOK_Quad_CR)
{
   // note: ⎕CR is instantiated twice, once for the static Quad_CR::fun,
   // and once in Workspace::Workspace() fotr macro support
   //
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕CR");
}
//----------------------------------------------------------------------------
const char *
Quad_CR::get_legend(FunctionGroup::Legend_type lt) const
{
   switch(lt)
      {
        default: return "";

        case LET_FUN_PREFIX: return
"   ┌─── Legend:───────────────────────────────────────────────────┐\n"
"   │    b - byte vector (vector of integers between -128 and 255) │\n"
"   │    h - hex string (characters 0-9 or A-F resp. a-f)          │\n"
"   │    i - integer vector                                        │\n"
"   │    l - string of (\\n-terminated) lines                       │\n"
"   │    m - character matrix                                      │\n"
"   │    n - nested vector of strings                              │\n"
"   │    r - base64 string according to RFC 4648                   │\n"
"   │    s - string                                                │\n"
"   │    t - token                                                 │\n"
"   │    v - T,V (integer tag T and byte vector V)                 │\n"
"   └──────────────────────────────────────────────────────────────┘\n"
"\n";

   case LET_MAP_SUFFIX: return
"\n"
"   if N ⎕CR has an inverse M ⎕CR then -N can be used instead of M.\n";
      }
}
//----------------------------------------------------------------------------
void
Quad_CR::print_fun_syntax(ostream & out,
                          const function_info & info) const
{
   out << "    " << info.comment_fun << endl;
}
//----------------------------------------------------------------------------
void
Quad_CR::print_map_syntax(ostream & out,
                          const function_info & info) const
{
char NN[10];   SPRINTF(NN, "%2d", int(info.axis));
const UTF8_literal name = info.function_name;
   out << "      " << NN << " ⎕CR  ←→"
       << UCS_string(24 - name.get_char_count(), UNI_SPACE)
       << "'" << name << "' ⎕CR  ←→  ⎕CR." << name << endl;
}
//----------------------------------------------------------------------------
Token
Quad_CR::eval_B(Value_P B) const
{
   if (B->element_count())                    return do_eval_B(B.get(), true);
   if (B->get_cfirst().is_character_cell())   return list_functions(CERR);
   if (B->get_cfirst().is_integer_cell())     return list_mappings(CERR);
   DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Quad_CR::do_eval_B(const Value * B, bool remove_extra_spaces)
{
UCS_string symbol_name(*B);
   symbol_name.remove_trailing_whitespaces();

   /*  return an empty character matrix,     if:
    *  1) symbol_name is not known,          or
    *  2) symbol_name is not user defined,   or
    *  3) symbol_name is a function,         or
    *  4) symbol_name is not displayable
    */

   if (symbol_name.size() == 0)   return Token(TOK_APL_VALUE1, Str0_0(LOC));

const Function * function = 0;
   if (symbol_name[0] == UNI_MUE)   // macro
      {
        loop(m, Macro::MAC_COUNT)
            {
              const Macro * macro =
                    Macro::get_macro(Macro::Macro_num(m));
              if (symbol_name == macro->get_name())
                 {
                   function = macro;
                   break;
                 }
            }
      }
   else   // maybe user defined function
      {
        const NamedObject * obj = Workspace::lookup_existing_name(symbol_name);
        if (obj && obj->is_user_defined())
           {
             function = obj->get_function();
             if (function && function->get_exec_properties()[0])   function = 0;
           }
      }
   if (function == 0)   return Token(TOK_APL_VALUE1, Str0_0(LOC));

   // show the function...
   //
const UCS_string ucs = function->canonical(false);
UCS_string_vector tlines;
   ucs.to_vector(tlines);
int max_len = 0;
   loop(row, tlines.size())
      {
        if (remove_extra_spaces)
           tlines[row].remove_leading_and_trailing_whitespaces();

        if (max_len < tlines[row].ssize())   max_len = tlines[row].ssize();
      }

Shape shape_Z;
   shape_Z.add_shape_item(tlines.size());
   shape_Z.add_shape_item(max_len);

Value_P Z(shape_Z, LOC);
   loop(row, tlines.size())
      {
        const UCS_string & line = tlines[row];
        loop(col, line.size())
            Z->next_ravel_Char(line[col]);

        loop(col, max_len - line.size())
            Z->next_ravel_Char(UNI_SPACE);
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Token
Quad_CR::eval_AB(Value_P A, Value_P B) const
{
const sAxis subfunction = value_to_subfun(*A);

   if (subfunction == 45)   // filter (= return B)
      {
        do_CR45(B.get());
        return Token(TOK_APL_VALUE1, B);
      }

PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
Value_P Z = do_CR(subfunction, B.get(), pctx);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR(APL_Integer a, const Value * B, PrintContext pctx)
{
   // some functions a have an inverse (which has its own number, but can
   // also be specified as -a
   //
   if (a < 0)   switch(a)
      {
        case  -5: a = 13;   break;
        case  -6: a = 13;   break;
        case -13: a =  6;   break;
        case -12: a = 11;   break;
        case -11: a = 12;   break;
        case -14: a = 15;   break;
        case -15: a = 14;   break;
        case -16: a = 17;   break;
        case -17: a = 16;   break;
        case -18: a = 19;   break;
        case -19: a = 18;   break;
        case -33: a = 34;   break;
        case -34: a = 33;   break;
        case -35: a = 36;   break;
        case -36: a = 35;   break;
        case -38: a = 39;   break;
        case -39: a = 38;   break;
        case -40: a = 41;   break;
        case -41: a = 40;   break;
        default: MORE_ERROR() << "A ⎕CR B with invalid A < 0";
                 DOMAIN_ERROR;
      }

// extra_frame draws an extra frame (even if B is not nested).
bool extra_frame = false;
   switch(a)
      {
        case  4:
        case  8:
        case 23:
        case 24:
        case 25:
        case 29:
        case 46:
        case 47: extra_frame = true;

      }

   switch(a)
      {

/// a local shortcut for the various frame variants of ⎕CR
#define FRAME(x)   pctx.set_style(x);   break;

        case  0: FRAME(PR_APL)
        case  1: FRAME(PR_APL_FUN)
        case  2: FRAME(PR_BOXED_CHAR)           // fraed with ASCII chars
        case  3: FRAME(PR_BOXED_GRAPHIC)
        case  4: FRAME(PR_BOXED_GRAPHIC)       // with extra frame
        case  5:                               // byte-vector → HEX
        case  6: return do_CR5_6(a, B);        // byte-vector → hex
        case  7: FRAME(PR_BOXED_GRAPHIC1)
        case  8: FRAME(PR_BOXED_GRAPHIC1)       // with extra frame
        case  9: FRAME(PR_BOXED_GRAPHIC2)
        case 10: return do_CR10(B);
        case 11: return do_CR11(B);            // Value → CDR conversion
        case 12: return do_CR12(B);            // CDR → Value conversion
        case 13: return do_CR13(B);            // hex → byte-vector
        case 14: return do_CR14(B);            // Value → CDR → hex conversion
        case 15: return do_CR15(B);            // hex → CDR → Value conversion
        case 16: return do_CR16(B);            // byte vector → base64, RFC 4648
        case 17: return do_CR17(B);            // base64 → byte vector, RFC 4648
        case 18: return do_CR18(B);            // UCS → UTF8 byte vector
        case 19: return do_CR19(B);            // UTF8 byte vector → UCS string
        case 20: FRAME(PR_NARS)
        case 21: FRAME(PR_NARS1)
        case 22: FRAME(PR_NARS2)
        case 23: FRAME(PR_NARS)                // with extra frame
        case 24: FRAME(PR_NARS1)               // with extra frame
        case 25: FRAME(PR_NARS2)               // with extra frame
        case 26: return do_CR26(B);            // Cell types
        case 27:                               // value as int
        case 28: return do_CR27_28(a, B);      // value2 as int
        case 29: FRAME(PR_BOXED_GRAPHIC3)      // with extra frame
        case 30: return do_CR30(B);            // conform B (for ⍤ macro)
        case 31:                               // ⎕INP helper
        case 32: return do_CR31_32(a, B);      // ⎕INP helper
        case 33: return do_CR33(B);            // TV to TLV byte vector
        case 34: return do_CR34(B);            // TLV byte vector to TV
        case 35: return do_CR35(B);            // lines to nested strings
        case 36: return do_CR36(B);            // nested strings to lines
        case 37: return do_CR37(B);            // ⎕CR B but extra spaces kept
        case 38: return do_CR38(B);            // plain →  structure
        case 39: return do_CR39(B);            // structure → plain
        case 40: return do_CR40(B);            // boolean → packed
        case 41: return do_CR41(B);            // packed → boolean
        case 42: return do_CR42_43(B, false);  // tokenize B
        case 43: return do_CR42_43(B, true);   // parse B
        case 44: return do_CR44(B);            // decode token tags
        case 46: FRAME(PR_BOXED_GRAPHIC4)
        case 47: FRAME(PR_BOXED_GRAPHIC5)

        default: MORE_ERROR() << "A ⎕CR B with invalid A (=" << a << ")";
                 DOMAIN_ERROR;
#undef FRAME
      }

   // common code for ⎕CR variants that only differ by print style...
   //
   if (extra_frame && !B->is_simple_scalar())
      {
        Value_P Z(LOC);                          // a nested scalar
        Value * Zsub = const_cast<Value *>(B);   // will die at } below
        Z->next_ravel_Pointer(Zsub);             // Z ← ⊂ B
        Z->check_value(LOC);
        PrintBuffer pb(*Z, pctx, 0);
        return Value_P(pb, LOC);
      }
   else   // no frame
      {
         PrintBuffer pb(*B, pctx, 0);
         return Value_P(pb, LOC);
      }
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR5_6(int A_5_6, const Value * B)
{
const char * alpha = (A_5_6 == 5) ? "0123456789ABCDEF" : "0123456789abcdef";
Shape shape_Z(B->get_shape());
   if (shape_Z.get_rank() == 0)   // scalar B
      {
        shape_Z.add_shape_item(2);
      }
   else
      {
        shape_Z.set_shape_item(B->get_rank() - 1, B->get_cols()*2);
      }

Value_P Z(shape_Z, LOC);

const Cell * cB = &B->get_cfirst();
   loop(b, B->element_count())
       {
         const int val = cB++->get_byte_value() & 0x00FF;
         const int h = alpha[val >> 4];
         const int l = alpha[val & 0x0F];
         Z->next_ravel_Char(Unicode(h));
         Z->next_ravel_Char(Unicode(l));
       }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR10(const Value * B)
{
   // cannot use PrintBuffer here because the lines in ucs_vec
   // have different lengths

   // collect the APL code that produces B in ucs_vec
   //
UCS_string_vector ucs_vec;
   do_CR10(ucs_vec, B);

Value_P Z(ucs_vec.size(), LOC);
   loop(line, ucs_vec.size())
      {
         Value_P Z_line(ucs_vec[line], LOC);
         Z->next_ravel_Pointer(Z_line.get());
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
void
Quad_CR::do_CR10(UCS_string_vector & result, const Value * B)
{
   // B is the name of a variable or function.
   // result shall be the APL code that produces it.
   //
const UCS_string symbol_name(*B);
const Symbol * symbol = Workspace::lookup_existing_symbol(symbol_name);
   if (symbol == 0)
      {
        MORE_ERROR() << "10⎕CR: bad Symbol in do_CR10()";
        DOMAIN_ERROR;
      }

   switch(symbol->get_NC())
      {
        case NC_VARIABLE:
             {
               const Value * value = symbol->get_apl_value().get();
               do_CR10_variable(result, symbol_name, value);
               return;
             }

        case NC_FUNCTION:
        case NC_OPERATOR:
             {
               const Function & ufun = *symbol->get_function();
               const UCS_string text = ufun.canonical(false);
               if (ufun.is_lambda())
                  {
                    UCS_string res = symbol->get_name();
                    res.append(UNI_LEFT_ARROW);
                    res.append(UNI_L_CURLY);
                    int t = 0;
                    while (t < text.ssize())   // skip λ header
                       {
                         const Unicode uni = text[t++];
                         if (uni == UNI_LF)   break;
                       }


                    while (t < text.ssize())   // copy body
                        {
                          const Unicode uni = text[t++];
                          if (uni == UNI_LF)   break;
                           res.append(uni);
                        }
                    res.append(UNI_R_CURLY);

                    result.push_back(res);
                  }
               else
                  {
                    UCS_string res(UTF8_string("∇"));

                    loop(u, text.ssize())
                       {
                         if (text[u] == '\n')
                             {
                               result.push_back(res);
                               res.clear();
                               UCS_string next(text, u+1, text.size()-(u+1));
                               if (!next.is_comment_or_label() &&
                                   u < (text.ssize() - 1))
                                  res.append(UNI_SPACE);
                             }
                         else
                             {
                               res.append(text[u]);
                             }
                       }
                    res.append(ufun.get_exec_properties()[0]
                               ? UNI_DEL_TILDE : UNI_NABLA);
                    result.push_back(res);
                  }
               return;
             }

        default: MORE_ERROR() << "⎕RVAL: bad symbol->get_NC() in do_CR10()";
                 DOMAIN_ERROR;
      }
}
//----------------------------------------------------------------------------
void
Quad_CR::do_CR10_variable(UCS_string_vector & result,
                          const UCS_string & var_name,
                          const Value * value)
{
   // avoid any disturbances by ⎕FC (Format Control).
   //
   Workspace::push_FC();

   if (value->is_member())   // structured variable
      {
        if (const char * error = do_CR10_structured(result, var_name, value))
           {
             CERR << "could not )DUMP structured variable " << var_name
                  << ": " << error
                  << "\n)DUMPing it as regular variable instead..." << endl;
             goto not_structured;
           }

        Workspace::pop_FC();   // restore ⎕FC
        return;   // OK
      }

not_structured:

#define PUSH_TEXT result.push_back(text);   text.clear();
#define TMP_VAR_PREFIX "⍙¯_"
#define TMP_VAR_SUFFIX "_∆¯"
#define TMP_VAR(d)   TMP_VAR_PREFIX << int(d) << TMP_VAR_SUFFIX

UCS_string text;
const ShapeItem ec = value->element_count();

   // frequent special case: string
   //
   if (ec < 60 && is_plain_string(value))
      {
        text << var_name << "←\"";
        loop(e, ec)   text << value->get_cravel(e).get_char_value();
        text << "\"";
        PUSH_TEXT
        Workspace::pop_FC();   // restore ⎕FC
        return;
      }

const APL_types::Depth depth = value->compute_depth();
   // frequent special case: simple scalar or short simple vector
   //
   if (depth <= 1 && value->get_rank() <= 1 && ec < 20)   // short simple vector
      {
        text << var_name << "←";
        if (value->element_count())   // non-empty value
           {
             loop(e, value->element_count())
                 {
                   if (e)   text << " ";
                   const Cell & cell = value->get_cravel(e);
                   const UCS_string item_e = do_CR10_simple_cell(cell);
                   text << item_e;
                 }
           }
        else                         // empty vector
           {
             if (value->get_cfirst().is_character_cell())   text << "''";
             else                                           text << "⍬";
           }
        PUSH_TEXT
        Workspace::pop_FC();   // restore ⎕FC
        return;
      }

   // VAR ← (depth+1)⍴0. The first depth items are used as temporary values
   // for the different depths, while the last item is for saving ⎕IO.
   //
   text << TMP_VAR(depth) "←⎕IO ◊ ⎕IO←0   ⍝ " << var_name << "←";    PUSH_TEXT

   do_CR10_level(result, 0, *value);
   text << "⎕IO←" TMP_VAR(depth) << " ◊ "
        << var_name << "←⍙¯_0_∆¯";                                   PUSH_TEXT
   text << "⊣⎕EX ⊃";
   loop(d, depth + 1)   text << " '" << TMP_VAR(d) << "'";
                                                                     PUSH_TEXT
   Workspace::pop_FC();   // restore ⎕FC
}
//----------------------------------------------------------------------------
void
Quad_CR::do_CR10_level(UCS_string_vector & result, size_t level,
                       const Value & value)
{
UCS_string text;
UCS_string indent(2*(level + 1), UNI_SPACE);
UCS_string var_level;   var_level << TMP_VAR(level); 
UCS_string ind_var_level = indent;   ind_var_level << var_level;

   text << ind_var_level << "←";

   // important special case: string
   //
   if (value.element_count() < int(60 - 2*level) && is_plain_string(&value))
      {
        text << "\"";
        loop(e, value.element_count())
            text << value.get_cravel(e).get_char_value();
        text << "\"";                                                PUSH_TEXT
        return;
      }

   text << value.nz_element_count()
        << "⍴0   ⍝ L" << level << " zeroes";                         PUSH_TEXT

size_t nested_count = 0;
size_t simple_count = 0;
size_t zero_count  = 0;
   loop(v, value.nz_element_count())
       {
         const Cell & cell = value.get_cravel(v);
         if (cell.is_pointer_cell())
            {
              ++nested_count;
              continue;
            }
         if (cell.is_near_zero())
            {
              ++zero_count;
            }
         else
            {
              ++simple_count;
            }
       }

   if (simple_count)
      {
        // if all items of value are numeric, then we can safely use ' '.
        // as item separator. Otherwise we need commas (which is less
        // efficient)
        //
        text << indent << "⍝ L" << level << ": ";
        if (zero_count)   text << zero_count << "+";
        text << simple_count << " simple item(s)...";                PUSH_TEXT
        const ShapeItem ec = value.nz_element_count();
        for (ShapeItem e = 0; e < ec;)
            {
              const Cell & cell = value.get_cravel(e);
              if (cell.is_pointer_cell())
                 {
                   ++e;
                   continue;
                 }

              UCS_string U1;   // ((⊃V    (fixed size)
              UCS_string U2;   // indices (variable size)
              UCS_string U3;   // ])←     (fixed size)
              UCS_string U4;   // items   (variable size)

              // code for one item
              //
              U1 << var_level << "[";
              U2 << e;
              U3 << "]←";
              U4 << do_CR10_simple_cell(cell);
              ++e;

              UCS_string item_text;
              item_text << U1 << U2 << U3 << U4;

             const int line_limit = 72 - indent.size() - U1.size() - U3.size();

             // fill U2 and U4 with more items until line_limit is reached...
             //
             bool has_string = false;
             int count = 1;        // from first item above
             int e_from = e - 1;   // dito.
             while (e < ec && (U2.ssize() + U4.ssize()) < line_limit)
                   {
                     const Cell & cell = value.get_cravel(e++);
                     if (cell.is_pointer_cell())   continue;

                     U2 << " " << e - 1;   // - 1 since e++ above
                     ++count;
                     const UCS_string text_e = do_CR10_simple_cell(cell);
                     if (U4.back() == UNI_SINGLE_QUOTE &&
                         text_e.front() == UNI_SINGLE_QUOTE)
                        {
                          U4.pop_back();   // trailing '
                          U4 << UCS_string(text_e, 1, text_e.size() - 1);
                          has_string = true;
                        }
                     else
                        {
                          U4 << Invalid_Unicode << text_e;   // for now
                        }
                   }

              // now we know if we need commas instead of spaces
              //
              loop(u, U4.size())
                  {
                    if (U4[u] == Invalid_Unicode)
                       U4[u] = has_string ? UNI_COMMA : UNI_SPACE;
                  }

              if (count > 3 && count == (e - e_from))   // consecutive indices
                 {
                   U2.clear();
                   U2 << e_from << "+⍳" << count;
                 }

              text << indent << U1 << U2 << U3 << U4;                PUSH_TEXT
            }
      }

   if (nested_count)
      {
        text << indent << "⍝ L" << level << ": " << nested_count
             << " nested item(s)...";                                PUSH_TEXT
        loop(v, value.nz_element_count())
            {
              const Cell & cell = value.get_cravel(v);
              if (!cell.is_pointer_cell())   continue;
              const Value & sub_val = *cell.get_pointer_value();
              do_CR10_level(result, level + 1, sub_val);

              text << indent << var_level << "[" << v << "]←⊂"
                   << TMP_VAR(level + 1);                            PUSH_TEXT
            }
      }

   // final reshape
   //
const Shape & shape = value.get_shape();
   if (shape.get_rank() != 1 || shape.get_volume() == 0)
      {
        text << ind_var_level << "←" <<  do_CR10_shape(shape)
             << "⍴" << var_level
             << "   ⍝ L" << level << " final reshape";               PUSH_TEXT
      }
   else
      {
        text << indent << "⍝ no (need for) L" << level
             << " final reshape";                                    PUSH_TEXT
      }
}
//----------------------------------------------------------------------------
UCS_string
Quad_CR::do_CR10_simple_cell(const Cell & cell)
{
UCS_string text;
   if (cell.is_character_cell())
      {
        // be careful with non-printable characters and quotes
        //
        const Unicode uni = cell.get_char_value();
        if (uni < ' ' || uni == 127)
           {
             text << "(⎕UCS " << int(uni) << ")";
           }
        else if (uni == UNI_SINGLE_QUOTE)
           {
             text << "''''";
           }
        else
           {
             text << "'" << uni << "'";
           }
        return text;
      }

   if (cell.is_integer_cell())
      {
        return text << cell.get_int_value();
      }
   if (cell.is_real_cell())
      {
        PrintContext pctx(PR_APL);
        PrintBuffer pbuf = cell.character_representation(pctx) ;
        return text << pbuf.l1();
      }
   if (cell.is_complex_cell())
      {
        text.append_complex(cell.get_real_value(), cell.get_imag_value());
        return text;
      }
   FIXME;
}
//----------------------------------------------------------------------------
UCS_string
Quad_CR::do_CR10_shape(const Shape & shape)
{
   if (shape.get_rank() == 0)   return UCS_string(UNI_ZILDE);   // scalar

UCS_string result;
   loop(r, shape.get_rank())
       {
         if (r)   result.push_back(UNI_SPACE);
         result.append_number(shape.get_shape_item(r));
       }
   return result;
}
//----------------------------------------------------------------------------
const char *
Quad_CR::do_CR10_structured(UCS_string_vector & result,
                            const UCS_string & var_name,
                            const Value * value)
{
   if (value->get_rank() != 2)   return "bad rank (structured variable)";
   if (value->get_cols() != 2)   return "bad shape (structured variable)";

   loop(r, value->get_rows())
       {
         const Cell & member_cell = value->get_cravel(2*r);
         if (!member_cell.is_pointer_cell())   continue;
         const Value * member_name = member_cell.get_pointer_value().get();

         // unused member entries are integer 0.
         //
         if (!member_name->is_char_string())   continue;

         const UCS_string member_ucs(*member_name);
         UCS_string member_path = var_name;
         member_path.push_back(UNI_FULLSTOP);
         member_path.append(member_ucs);

         const Cell & data_cell = value->get_cravel(2*r + 1);
         if (data_cell.is_simple_cell())
            {
              member_path.push_back(UNI_LEFT_ARROW);
              const UCS_string data_value = do_CR10_simple_cell(data_cell);
              member_path.append(data_value);
              result.push_back(member_path);
            }
         else
            {
              do_CR10_variable(result, member_path,
                               data_cell.get_pointer_value().get());
            }
       }

   return 0;
}
//----------------------------------------------------------------------------
bool
Quad_CR::figure_default(const Value * value, Unicode & default_char,
                        APL_Integer & default_int)
{
ShapeItem zeroes = 0;
ShapeItem blanks = 0;
   loop(v, value->nz_element_count())
      {
        const Cell & cell = value->get_cfirst();
        if (cell.is_integer_cell())
           {
             if (cell.get_int_value() == 0)   ++zeroes;
           }
        else if (cell.is_character_cell())
           {
             if (cell.get_char_value() == UNI_SPACE)   ++blanks;
           }
      }

   return zeroes >= blanks;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR11(const Value * B)
{
CDR_string cdr;
   CDR::to_CDR(cdr, B);

const ShapeItem len = cdr.size();
Value_P Z(len, LOC);
   loop(l, len)
       Z->next_ravel_Char(Unicode(0xFF & cdr[l]));

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR12(const Value * B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

CDR_string cdr;
   loop(b, B->element_count())
       cdr.push_back(B->get_cravel(b).get_byte_value());

Value_P Z = CDR::from_CDR(cdr, LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR13(const Value * B)
{
   // hex → Value conversion. 2 characters per byte in B, therefore
   // last axis of B must have even length.
   //
   if (B->get_cols() & 1)   LENGTH_ERROR;

Shape shape_Z(B->get_shape());
   shape_Z.set_shape_item(B->get_rank() - 1, (B->get_cols() + 1)/ 2);

Value_P Z(shape_Z, LOC);
const Cell * cB = &B->get_cfirst();
   loop(z, Z->element_count())
       {
         const int n1 = nibble(cB++->get_char_value());
         const int n2 = nibble(cB++->get_char_value());
         if (n1 < 0 || n2 < 0)   DOMAIN_ERROR;
         Z->next_ravel_Char(Unicode(16*n1 + n2));
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR14(const Value * B)
{
const char * hex = "0123456789abcdef";
CDR_string cdr;
   CDR::to_CDR(cdr, B);

const ShapeItem len = cdr.size();
Value_P Z(2*len, LOC);
   loop(l, len)
       {
         const Unicode uh = Unicode(hex[0x0F & cdr[l] >> 4]);
         const Unicode ul = Unicode(hex[0x0F & cdr[l]]);
         Z->next_ravel_Char(uh);
         Z->next_ravel_Char(ul);
       }
   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR15(const Value * B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

CDR_string cdr;
const ShapeItem len = B->element_count()/2;
const Cell * cB = &B->get_cfirst();
   loop(b, len)
       {
         const int n1 = nibble(cB++->get_char_value());
         const int n2 = nibble(cB++->get_char_value());
         if (n1 < 0 || n2 < 0)   DOMAIN_ERROR;
         cdr.push_back(16*n1 + n2);
       }

   Value_P Z = CDR::from_CDR(cdr, LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR16(const Value * B)
{
   if (B->get_rank() > 1)   RANK_ERROR;

const ShapeItem full_quantums = B->element_count() / 3;
const ShapeItem len_Z = 4 * ((B->element_count() + 2) / 3);
Value_P Z(len_Z, LOC);

const char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

const Cell * cB = &B->get_cfirst();
   loop(b, full_quantums)   // encode full quantums
      {
        /*      -- b1 -- -- b2 -- -- b3 --
             z: 11111122 22223333 33444444
         */
        const int b1 = cB++->get_char_value() & 0x00FF;
        const int b2 = cB++->get_char_value() & 0x00FF;
        const int b3 = cB++->get_char_value() & 0x00FF;

        const int z1 = b1 >> 2;
        const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;
        const int z3 = (b2 & 0x0F) << 2 | (b3 & 0xC0) >> 6;
        const int z4 =  b3 & 0x3F;

        Z->next_ravel_Char(Unicode(alpha[z1]));
        Z->next_ravel_Char(Unicode(alpha[z2]));
        Z->next_ravel_Char(Unicode(alpha[z3]));
        Z->next_ravel_Char(Unicode(alpha[z4]));
      }

   // process final bytes
   //
   switch(B->element_count() - 3*full_quantums)
      {
        case 0: break;   // length of B is 3 * N

        case 1:          //  length of B is 3 * N + 1
                {
                  const int b1 = cB++->get_char_value() & 0x00FF;
                  const int b2 = 0;

                  const int z1 = b1 >> 2;
                  const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;

                  Z->next_ravel_Char(Unicode(alpha[z1]));
                  Z->next_ravel_Char(Unicode(alpha[z2]));
                  Z->next_ravel_Char(Unicode('='));
                  Z->next_ravel_Char(Unicode('='));
                }
                break;

        case 2:          // two bytes remaining
                {
                  const int b1 = cB++->get_char_value() & 0x00FF;
                  const int b2 = cB++->get_char_value() & 0x00FF;
                  const int b3 = 0;

                  const int z1 = b1 >> 2;
                  const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;
                  const int z3 = (b2 & 0x0F) << 2 | (b3 & 0xC0) >> 6;

                  Z->next_ravel_Char(Unicode(alpha[z1]));
                  Z->next_ravel_Char(Unicode(alpha[z2]));
                  Z->next_ravel_Char(Unicode(alpha[z3]));
                  Z->next_ravel_Char(Unicode('='));
                }
                break;
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR17(const Value * B)
{
   if (B->get_rank() != 1)   RANK_ERROR;

const int cols = B->get_cols();
   if (cols == 0)   return Str0(LOC);  // empty value
   if (cols & 3)    LENGTH_ERROR;      // length not 4*n

   // figure number of missing chars in final quantum
   //
int missing = 0;
   if      (B->get_cravel(cols - 2).get_char_value() == '=')   missing = 2;
   else if (B->get_cravel(cols - 1).get_char_value() == '=')   missing = 1;

const ShapeItem len_Z = 3 * (B->element_count() / 4) - missing;
const ShapeItem quantums = B->element_count() / 4;

Value_P Z(len_Z, LOC);
const Cell * cB = &B->get_cfirst();
   loop(q, quantums)
       {
         const int b1 = sixbit(cB++->get_char_value());
         const int b2 = sixbit(cB++->get_char_value());
         const int q3 = sixbit(cB++->get_char_value());
         const int q4 = sixbit(cB++->get_char_value());
         const int b3 = q3 & 0x3F;
         const int b4 = q4 & 0x3F;

         /*    b1       b2       b3       b4
            --zzzzzz --zzzzzz --zzzzzz --zzzzzz
              111111   112222   223333   333333
          */
         const int z1 =  b1 << 2         | (b2 & 0x30) >> 4;
         const int z2 = (b2 & 0x0F) << 4 | b3 >> 2;
         const int z3 = (b3 & 0x03) << 6 | b4;

         if (b1 < 0 || b2 < 0 || q3  == -1 || q4 == -1)   DOMAIN_ERROR;

         if (q < (quantums - 1) || missing == 0)
            {
              Z->next_ravel_Char(Unicode(z1));
              Z->next_ravel_Char(Unicode(z2));
              Z->next_ravel_Char(Unicode(z3));
            }
         else if (missing == 1)   // k6 k2l4 l20000 =
            {
              Z->next_ravel_Char(Unicode(z1));
              Z->next_ravel_Char(Unicode(z2));
            }
         else                     // k6 k20000 = =
            {
              Z->next_ravel_Char(Unicode(z1));
            }
       }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR18(const Value * B)
{
UCS_string ucs(*B);
UTF8_string utf(ucs);
const ShapeItem length = utf.size();
Value_P Z(length, LOC);

   loop(l, length)   Z->next_ravel_Int(utf[l] & 0xFF);
   Z->set_proto_Int();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR19(const Value * B)
{
   if (B->get_rank() > 1)   RANK_ERROR;
const ShapeItem len_B = B->element_count();

UTF8 * bytes_utf = ALLOCA(UTF8, len_B + 10);
   loop(b, len_B)   bytes_utf[b] = B->get_cravel(b).get_byte_value();
   bytes_utf[len_B] = 0;

const UTF8_string utf(bytes_utf, len_B);
const UCS_string ucs(utf);
Value_P Z(ucs, LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR26(const Value * B)
{
const ShapeItem len = B->element_count();
Value_P Z(B->get_shape(), LOC);
   loop(l, len)
      {
        const Cell & cB = B->get_cravel(l);
        if (cB.is_pointer_cell())
           {
             Value_P B_sub = cB.get_pointer_value();
             Value_P Z_sub = do_CR26(B_sub.get());
             Z->next_ravel_Pointer(Z_sub.get());
           }
        else
           {
             Z->next_ravel_Int(cB.get_cell_type());
           }
      }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR27_28(int A_27_28, const Value * B)
{
const ShapeItem len = B->element_count();
Value_P Z(B->get_shape(), LOC);
   loop(z, len)
       {
         const Cell & cB = B->get_cravel(z);
         if (cB.is_pointer_cell())
            {
              Value_P B_sub = cB.get_pointer_value();
              Value_P Z_sub = do_CR27_28(A_27_28, B_sub.get());
              Z->next_ravel_Pointer(Z_sub.get());
            }
         else
            {
              APL_Integer data = 0;
              if (A_27_28 == 27)   // 27 ⎕CR B: primary value
                 {
                   if (cB.get_cell_type() == CT_CHAR)
                      data = cB.get_char_value();
                   else if (cB.get_cell_type() == CT_CELLREF)
                      data = APL_Integer(cB.get_lval_value());
                   else
                      memcpy(&data, cB.get_u0(), sizeof(data));
                 }
              else               // 28 ⎕CR B: additional value
                 {
                   if (cB.get_cell_type() == CT_COMPLEX)
                      {
                        memcpy(&data, cB.get_u1(), sizeof(data));
                      }
                   else if (cB.get_cell_type() == CT_CELLREF)
                      {
                        const LvalCell & cB_lval =
                                       reinterpret_cast<const LvalCell &>(cB);
                        data = APL_Integer(cB_lval.get_cell_owner());
                      }
#ifdef cfg_RATIONAL_NUMBERS_WANTED
                   else if (cB.get_cell_type() == CT_FLOAT)
                      {
                        memcpy(&data, cB.get_u1(), sizeof(data));
                      }
                   else if (cB.get_cell_type() == CT_INT)
                      {
                        data = 1;
                      }
#endif
                 }

              Z->next_ravel_Int(data);
            }
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR30(const Value * B)
{
   // Z is B with all items conformed to the same rank and shape. Primarily
   // an internal function used in macros Z__LO_RANK_X5_B and Z__A_LO_RANK_X7_B
   // but possibly useful elsewhere.

const ShapeItem len_B = B->element_count();
   if (len_B == 0)   return CLONE(B, LOC);

   // we use 'ShapeItem max_shape[MAX_RANK] max_shape' instead of
   // 'Shape max_shape' to avoid multiple recompute_volume() in class Shape
   // while looping along the B ravel.
   //
ShapeItem max_shape[MAX_RANK];
   loop(r, MAX_RANK)   max_shape[r] = 0;
sRank max_rank = 0;

   loop(b, len_B)
      {
        const Cell & cB = B->get_cravel(b);
        if (cB.is_lval_cell())   DOMAIN_ERROR;
        if (!cB.is_pointer_cell())   continue;   // simple scalar

        const Shape sh = cB.get_pointer_value()->get_shape();
        const sRank rk = sh.get_rank();
        if (max_rank < rk)   max_rank = rk;
        loop(s, rk)
           {
             const ShapeItem sh_s = sh.get_shape_item(s);
             if (max_shape[s] < sh_s) max_shape[s] = sh_s;
           }
      }

Shape conformed;
   loop(r, max_rank)   conformed.add_shape_item(max_shape[r]);
const ShapeItem conformed_len = conformed.get_volume();

Shape shape_Z(B->get_shape() + conformed);
Value_P Z(shape_Z, LOC);

   loop(b, len_B)
      {
        const Cell & cB = B->get_cravel(b);
        if (cB.is_pointer_cell())
           {
             Value_P B_sub = CLONE_P(cB.get_pointer_value(), LOC);
             Shape sh_sub = B_sub->get_shape();
             sh_sub.expand_rank(conformed.get_rank());
             B_sub->set_shape(sh_sub);

             Value_P ZZ = Bif_F12_TAKE::do_take(conformed, *B_sub, false);
             loop(zz, conformed_len)   Z->next_ravel_Cell(ZZ->get_cravel(zz));
           }
        else   // simple scalar
           {
             Z->next_ravel_Cell(cB);
             loop(zz, (conformed_len - 1))   Z->next_ravel_0();
           }
      }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR31_32(int A_31_32, const Value * B)
{
const ShapeItem len = B->element_count();
   if (len == 0)   LENGTH_ERROR;

Value_P Z(len, LOC);

PrintContext pctx = Workspace::get_PrintContext(PR_APL);
   pctx.set_style(PrintStyle(pctx.get_style() | PST_NO_FRACT_0));

   loop(b, len)
      {
        Value_P row = B->get_cravel(b).get_pointer_value();

        if (row->element_count() == 1)   // single item
           {
             Value_P Zrow = CLONE_P(row->get_cfirst().get_pointer_value(), LOC);
             Z->next_ravel_Pointer(Zrow.get());
             continue;
           }

        PrintBuffer pb;
        loop(col, row->element_count())
            {
              Value_P item = row->get_cravel(col).get_pointer_value();
              PrintBuffer pb_item(*item, pctx, 0);
              pb.pad_height(UNI_SPACE, pb_item.get_row_count());
              if (A_31_32 == 31)   // align bottoms
                 pb_item.pad_height_above(UNI_SPACE, pb.get_row_count());
              else
                 pb_item.pad_height(UNI_SPACE, pb.get_row_count());
              pb.add_column(UNI_SPACE, 0, pb_item);
            }
        if (pb.get_row_count() == 1)
           {
             Value_P Zrow(pb.l1(), LOC);
             Z->next_ravel_Pointer(Zrow.get());
           }
        else
           {
             Value_P Zrow(pb, LOC);
             Z->next_ravel_Pointer(Zrow.get());
           }
      }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
bool
Quad_CR::is_plain_string(const Value * value)
{
   if (value->get_rank() != 1)   return false;   // not a vector
   loop(v, value->nz_element_count())
       {
         const Cell & cell = value->get_cravel(v);
         if (!cell.is_character_cell())   return false;

         const Unicode uni = cell.get_char_value();
         if (uni < UNI_SPACE)           return false;   // control character
         if (uni == UNI_SINGLE_QUOTE)   return false;
         if (uni == UNI_DELETE)         return false;
       }

   return true;
}
//----------------------------------------------------------------------------
bool
Quad_CR::use_quote(V_mode mode, const Value * value, ShapeItem pos)
{
int char_len = 0;
int ascii_len = 0;

   for (; pos < value->element_count(); ++pos)
       {
         // if we are in '' mode then a single ASCII char
         // suffices to remain in that mode
         //
         if (ascii_len > 0 && mode == Vm_QUOT) return true;

         // if we are in a non-'' mode then 3 ASCII chars
         // suffice to enter '' mode
         //
         if (ascii_len >= 3)   return true;

         if (!value->get_cravel(pos).is_character_cell())   break;
         ++char_len;
         const Unicode uni = value->get_cravel(pos).get_char_value();
         if (uni >= ' ' && uni <= 0x7E)   ++ascii_len;
         else                             break;
       }

   // if all chars are ASCII then use '' mode
   //
   return (char_len == ascii_len);
}
//----------------------------------------------------------------------------
void
Quad_CR::close_mode(UCS_string & rhs, V_mode mode)
{
   if      (mode == Vm_QUOT)   rhs.append_UTF8("'");
   else if (mode == Vm_UCS)    rhs.append_UTF8(")");
}
//----------------------------------------------------------------------------
void
Quad_CR::item_separator(UCS_string & line, V_mode from_mode, V_mode to_mode)
{
   if (from_mode == to_mode)   // separator in same mode (if any)
      {
        if      (to_mode == Vm_UCS)    line.append_UTF8(" ");
        else if (to_mode == Vm_NUM)    line.append_UTF8(" ");
      }
   else                // close old mode and open new one
      {
        close_mode(line, from_mode);
        if (from_mode != Vm_NONE)   line.append_UTF8(",");

        if      (to_mode == Vm_UCS)    line.append_UTF8("(,⎕UCS ");
        else if (to_mode == Vm_QUOT)   line.append_UTF8("'");
      }
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR33(const Value * B)
{
   // convert B = Integer Tag, len bytes Data
   // to      Z = 4-byte Tag, 4-byte Len, len bytes Data
   //
   if (B->get_rank() > 1)   RANK_ERROR;
const ShapeItem len_B = B->element_count();
   if (len_B < 1)   LENGTH_ERROR;
const ShapeItem len_B1 = len_B - 1;
const Cell * cB = &B->get_cfirst();
   if (!cB++->is_integer_cell())   DOMAIN_ERROR;
   loop (b,  len_B1)   cB++->get_byte_value();   // DOMAIN ERROR if not byte

Value_P Z(len_B + 7, LOC);
const APL_Integer tag = B->get_cfirst().get_int_value();
    Z->next_ravel_Char(Unicode(tag >> 24 & 0xFF));
    Z->next_ravel_Char(Unicode(tag >> 16 & 0xFF));
    Z->next_ravel_Char(Unicode(tag >>  8 & 0xFF));
    Z->next_ravel_Char(Unicode(tag       & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >> 24 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >> 16 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >>  8 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1       & 0xFF));
    loop(z, len_B1)
        Z->next_ravel_Char(Unicode(B->get_cravel(z+1).get_byte_value()));

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR34(const Value * B)
{
   // convert B = 4-byte Tag, 4-byte Len, len bytes Data
   // to      Z = Integer Tag, len bytes Data
   //
   //
   if (B->get_rank() != 1)   RANK_ERROR;
const ShapeItem len_B = B->element_count();
   if (len_B < 8)   LENGTH_ERROR;
const Cell * cB = &B->get_cfirst();

   // throwe DOMAIN ERROR if one of the vector items is not a byte
   loop(b, len_B)
       {
         cB++->get_byte_value();
       }

   cB = &B->get_cfirst();

int32_t tag = 0;
   loop(bb, 4)   tag = tag << 8 | cB++->get_byte_value();

uint32_t len = 0;
   loop(bb, 4)   len = len << 8 | cB++->get_byte_value();
   if (len != (len_B - 8))   LENGTH_ERROR;

Value_P Z(len_B - 7, LOC);
   Z->next_ravel_Int(tag);
   loop(z, len_B - 8)   Z->next_ravel_Char(Unicode(cB++->get_byte_value()));

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR35(const Value * B)
{
   // B must be a true string (is_char_vector() == 1) that MAY contain
   // \n which then separates different lines. The \n are removed and
   // the result is a (nested) vector containing all lines.

   if (B->get_rank() != 1)   RANK_ERROR;

const ShapeItem len_B = B->element_count();
   if (len_B == 0)
      {
        Value_P Z1 = Str0(LOC);   // Z1←''
        Value_P Z(1, LOC);
        Z->next_ravel_Pointer(Z1.get());
        Z->check_value(LOC);
        return Z;
      }

ShapeItem lf_count = 0;
   loop(b, len_B)
       {
         if (B->get_cravel(b).get_char_value() == UNI_LF)   ++lf_count;
       }

   if (B->get_cravel(len_B - 1).get_char_value() != UNI_LF)   ++lf_count;

Value_P Z(lf_count, LOC);
UCS_string line;

   loop(b, len_B)
       {
         const Unicode uni = B->get_cravel(b).get_char_value();
         if (uni == UNI_LF)
            {
              Value_P Zb(line, LOC);
              Z->next_ravel_Pointer(Zb.get());
              line.clear();
            }
         else
            {
              line.append(uni);
            }
       }

   if (line.size())   // incomplete last line
      {
        Value_P Zb(line, LOC);
        Z->next_ravel_Pointer(Zb.get());
        line.clear();
      }

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR36(const Value * B)
{
   if (B->get_rank() != 1)   RANK_ERROR;

const ShapeItem len_B = B->element_count();
ShapeItem len_Z = B->element_count();
   loop(b, len_B)
       {
         const Value & Bb = *B->get_cravel(b).get_pointer_value().get();
         if (Bb.get_rank() > 1)   RANK_ERROR;
         len_Z += 1 + Bb.element_count();
       }

UCS_string UZ;
   UZ.reserve(len_Z);
   loop(b, len_B)
       {
         const Value & Bb = *B->get_cravel(b).get_pointer_value().get();
         UCS_string Ub(Bb);
         UZ.append(Ub);
         UZ.append(UNI_LF);
       }

Value_P Z(UZ, LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR38(const Value * B)
{
   /*
      return a structured value with:

      i.  integer scalar B:   empty (i.e. with B empty rows)
      ii. 2×N matrix B:       members B[;1] and values B[;2]
   */

   if (B->is_scalar())   // return a structured value with B unused rows
      {
        APL_Integer capacity = B->get_cfirst().get_near_int();
        if (capacity < 0)   DOMAIN_ERROR;
        if (capacity < 8)   return EmptyStruct(LOC);

        // round capacity up to next power of 2
        //
        APL_Integer p2;
        for (p2 = 8; p2 < capacity && p2 < 0x1000000000000;)   p2 += p2;
        if (capacity < p2)   capacity = p2;

        Shape shape_Z(ShapeItem(capacity), ShapeItem(2));
        Value_P Z(shape_Z, LOC);
        loop(c, capacity)
            {
              Z->next_ravel_0();
              Z->next_ravel_0();
            }

        Z->check_value(LOC);
        Z->set_member();
        return Z;
      }

   // convert unstructured array to structured value...
   //
   if (B->get_rank() != 2)   RANK_ERROR;
   if (B->get_cols() != 2)   LENGTH_ERROR;

const ShapeItem rows_B = B->get_rows();
ShapeItem valid_rows = B->get_member_count();

ShapeItem capacity;
   for (capacity = 8; capacity < valid_rows ;)   capacity += capacity;
   capacity += capacity;   // one more to use ≤ 50%

const Shape shape_Z(capacity, 2);
Value_P Z(shape_Z, LOC);

   // fill with unused
   loop(c, capacity)
       {
         Z->next_ravel_0();
         Z->next_ravel_0();
       }

   loop(r, rows_B)
      {
        const Cell & member_name = B->get_cravel(2*r);
        if (member_name.is_character_cell())   // valid row (1-character member)
           {
             UCS_string name(member_name.get_char_value());
             Cell * data = Z->get_new_member(name);
             data->init(B->get_cravel(2*r + 1), *Z, LOC);
           }
        else if (member_name.is_pointer_cell()) // valid row (string member)
           {
             UCS_string name(*member_name.get_pointer_value());
             Cell * data = Z->get_new_member(name);
             data->init(B->get_cravel(2*r + 1), *Z, LOC);
           }
      }

   Z->set_member();
   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR39(const Value * B)
{
   // structured value B to array Z.
   //
   if (B->get_rank() != 2)   RANK_ERROR;
   if (B->get_cols() != 2)   LENGTH_ERROR;

const ShapeItem rows_B = B->get_rows();
const ShapeItem valid_rows = B->get_member_count();

const Shape shape_Z(valid_rows, 2);
Value_P Z(shape_Z, LOC);

   loop(r, rows_B)
       {
         const Cell & name_cell = B->get_cravel(2*r);
         if (name_cell.is_integer_cell())   continue;   // unused row

         Z->next_ravel_Cell(name_cell);

         const Cell & data_cell = B->get_cravel(2*r + 1);
         if (data_cell.is_pointer_cell())   // non-leaf or nested leaf
            {
              Value_P B_sub = data_cell.get_pointer_value();
              if (B_sub->is_member())   // non-leaf
                 {
                   Value_P B_struct = do_CR39(B_sub.get());
                   Z->next_ravel_Pointer(B_struct.get());
                 }
              else                      // leaf
                 {
                   Z->next_ravel_Pointer(B_sub.get());
                 }
            }
         else                                               // leaf
            {
              Z->next_ravel_Cell(data_cell);
            }
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR40(const Value * B)
{
   // return boolean B as packed boolean Z
   //
const ShapeItem B_len = B->element_count() ;
   if (B_len <= Value::PACKED_MINIMUM_LENGHT)
      {
        MORE_ERROR() << "Only Boolean Arrays with more than "
                     << Value::PACKED_MINIMUM_LENGHT << " items can be packed";
        LENGTH_ERROR;
      }

   // round B_len up to the next multiple of 64
   //
const ShapeItem Z_len = (B_len + 63) >> 6;   // length in units of uint64_t
uint64_t * bits = new uint64_t[Z_len];
   if (bits == 0)   WS_FULL;

   // set all bits to 0, then some to 1...
   //
   loop(z, Z_len)   bits[z] = 0;

uint64_t chunk = 0;
uint64_t bit  = 1;
   loop(b, B_len)
       {
         const Cell & cell_B = B->get_cravel(b);
         if (!cell_B.is_near_bool())   DOMAIN_ERROR;
         if (cell_B.get_near_int())   chunk |= bit;
         bit += bit;
         if (bit == 0)   // uint64_t bit complete
            {
              bits[b >> 6] = chunk;
              chunk = 0;
              bit = 1;
            }
       }

    if (chunk)   bits[Z_len - 1] = chunk;   // rest bits

Value_P Z(B->get_shape(), bits, LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR41(const Value * B)
{
   if (!(B->get_flags() & VF_packed))
      {
        MORE_ERROR() << "B is not packed in 41 ⎕CR B";
        DOMAIN_ERROR;
      }

Value_P Z(B->get_shape(), LOC);

const ShapeItem B_len = B->element_count();
const uint8_t * bits = reinterpret_cast<const uint8_t *>(&B->get_cfirst());

   loop(b, B_len)   Z->next_ravel_Int((bits[b >> 3] & 1ULL << (b & 7)) ? 1 : 0);

   return Idx0(LOC);
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR42_43(const Value * B, bool parse)
{
const UCS_string ucs(*B);
Token_string tos;

   if (parse)   // parse ucs
      {
        Parser parser(PM_EXECUTE, LOC, false);
        if (const ErrorCode ec = parser.parse(ucs, tos, /* optimize */ true))
           {
             MORE_ERROR() << "the parser returned error code: " << ec;
             DOMAIN_ERROR;
           }
      }
   else          // tokenize ucs
      {
        Tokenizer tokenizer(PM_FUNCTION, LOC, /* macro */ false);

        if (const ErrorCode ec = tokenizer.tokenize(ucs, tos))
           {
             MORE_ERROR() << "the tokenizer returned error code: " << ec;
             DOMAIN_ERROR;
           }
      }

Value_P Z(tos.size(), LOC);
   loop(t, tos.size())
       {
         const Token & tok = tos[t];
         const TokenTag tag = tok.get_tag();
         Value_P ZZ(2, LOC);
         ZZ->next_ravel_Int(tag);   // the token tag

         switch(tok.get_ValueType())
            {
              case TV_CHAR:
                   {
                     ZZ->next_ravel_Char(tok.get_char_val());
                   }
                   break;

              case TV_INT:
                   {
                     ZZ->next_ravel_Int(tok.get_int_val());
                   }
                   break;

              case TV_FLT:
                   {
                     ZZ->next_ravel_Float(tok.get_flt_val());
                   }
                   break;

              case TV_CPX:
                   {
                     ZZ->next_ravel_Complex(tok.get_cpx_real(),
                                            tok.get_cpx_imag());
                   }
                   break;

              case TV_SYM:
                   {
                     const Symbol * sym = tok.get_sym_ptr();
                     Value_P Z2(sym->get_name(), LOC);
                     ZZ->next_ravel_Pointer(Z2.get());
                   }
                   break;

              case TV_FUN:
                   {
                     cFunction_P fun = tok.get_function();
                     Value_P Z2(fun->get_name(), LOC);
                     ZZ->next_ravel_Pointer(Z2.get());
                   }
                   break;

              case TV_VAL:
                   {
                     Value_P Z2 = tok.get_apl_val();
                     if (!Z2)   // null APL value
                        {
                          ZZ->next_ravel_0();
                        }
                     else
                        {
                          ZZ->next_ravel_Pointer(Z2.get());
                        }
                   }
                   break;

              default: ZZ->next_ravel_0();       // only the token tag
            }

         ZZ->check_value(LOC);
         Z->next_ravel_Pointer(ZZ.get());
       }

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_CR::do_CR44(const Value * B)
{
Value_P Z(B->get_shape(), LOC);

   if (B->get_rank() > 1)         RANK_ERROR;
   if (B->element_count() == 0)   LENGTH_ERROR;

   loop(b, B->element_count())
       {
          UCS_string ucs_z;
          decode_CR44(ucs_z, B->get_cravel(b));
          Value_P ZZ(ucs_z, LOC);
          Z->next_ravel_Pointer(ZZ.get());
       }

   return Z;
}
//----------------------------------------------------------------------------
void
Quad_CR::decode_CR44(UCS_string & result, const Cell & cB)
{
   if (cB.is_integer_cell())         // token tag
      {
        const APL_Integer b      = cB.get_int_value();
        const TokenTag tag       = TokenTag(b);
        const TokenClass cls     = TokenClass(tag & TC_MASK);
        const TokenValueType typ = TokenValueType(tag & TV_MASK);

        const UCS_ASCII_string tag_name(tag);
        const UCS_ASCII_string class_name(cls);
        const UCS_ASCII_string type_name(typ);
        result.append(tag_name);
        result.append_ASCII("(");
        result.append(class_name);
        result.append_ASCII(", ");
        result.append(type_name);
        result.append_ASCII(")");
      }
   else if (cB.is_pointer_cell())    // token ←→ (tag, value)
      {
        Value_P B2 = cB.get_pointer_value();
        if (B2->get_rank() > 1)   RANK_ERROR;

        if (B2->element_count() != 2)   LENGTH_ERROR;

        const Cell & cVal        = B2->get_cravel(1);
        const Cell & cTag        = B2->get_cravel(0);   // the tag
        const TokenTag tag       = TokenTag(cTag.get_int_value());
        const TokenClass cls     = TokenClass(tag & TC_MASK);
        const TokenValueType typ = TokenValueType(tag & TV_MASK);

        const UCS_ASCII_string tag_name(tag);
        result.append(tag_name);
        result.append_ASCII("( ");

        switch(typ)
            {
               case TV_NONE:  switch(cls)
                                 {
                                   case TC_ASSIGN: result.append_ASCII("←");
                                                   break;
                                   default:        result.append_ASCII("-");
                                 }
                              break;

               case TV_CHAR:  result.push_back(cVal.get_char_value());
                              break;

               case TV_INT:   result.append_number(cVal.get_int_value());
                              break;

               case TV_FLT:   result.append_double(cVal.get_real_value());
                              break;

               case TV_CPX:   result.append_double(cVal.get_real_value());
                              result.push_back(UNI_J);
                              result.append_double(cVal.get_imag_value());
                              break;

               case TV_FUN:   
               case TV_SYM:   if (!cVal.is_pointer_cell())   DOMAIN_ERROR;
                              {
                                Value_P symbol = cVal.get_pointer_value();
                                const UCS_string sym_name(*symbol);
                                result.append(sym_name);
                              }
                              break;

               case TV_LIN:   result.append_ASCII("[");
                              result.append_number(cVal.get_int_value());
                              result.append_ASCII("]");
                              break;

               case TV_VAL:   // optional shape
                              value_CR44(result, *cVal.get_pointer_value());
                              break;

               case TV_INDEX: result.append_ASCII("[index]");
                              break;


               default:       DOMAIN_ERROR;
            }

        result.append_ASCII(" )");
      }
   else
      {
        MORE_ERROR() <<
        "Invalid item in 44 ⎕CR B. Expect integer (tag) or (tag value) (token)";
        DOMAIN_ERROR;
      }
}
//----------------------------------------------------------------------------
void
Quad_CR::value_CR44(UCS_string & result, const Value & value)
{
   // 1. shape prefix (unless scalar or vector)
   //
const uRank rank = value.get_rank();
const ShapeItem ec = value.element_count();
   if (rank > 1 || ec == 1)
      {
        loop(r, rank)
           {
             result.append_number(value.get_shape_item(r));
             result.push_back(UNI_SPACE);
             
           }
         result.back() = UNI_RHO;
      }

   loop(e, ec)
       {
         if (result.size() > 60)   // long
            {
              result.append_ASCII(" ...");
              break;
            }

          const  Cell & cell = value.get_cravel(e);
          if (cell.is_character_cell())   // string or char
             {
               result.push_back(UNI_SINGLE_QUOTE);
               result.push_back(cell.get_char_value());
               result.push_back(UNI_SINGLE_QUOTE);
             }
          else if (cell.is_integer_cell())
             {
               result.append_number(cell.get_int_value());
             }
          else if (cell.is_float_cell())
             {
               result.append_double(cell.get_real_value());
             }
          else if (cell.is_complex_cell())
             {
               result.append_double(cell.get_real_value());
               result.push_back(UNI_J);
               result.append_double(cell.get_imag_value());
             }
          else if (cell.is_pointer_cell())
             {
               result.append_ASCII("(...)");
             }
          else if (cell.is_lval_cell())
             {
               result.append_ASCII("(...)←");
             }
          else
             {
               FIXME;
             }

         result.push_back(UNI_SPACE);
       }

   result.pop_back();   // trailing blanf

}
//----------------------------------------------------------------------------
void
Quad_CR::do_CR45(const Value * B)
{
UCS_string prefix;   prefix << "├───";
   do_CR45_value(prefix, B);
}
//----------------------------------------------------------------------------
void
Quad_CR::do_CR45_value(const UCS_string prefix, const Value * B)
{
ostream & out = CERR;
UCS_string sub_prefix = prefix;   sub_prefix << "────";

   out << prefix << " " << voidP(B) << endl;
   loop(b, B->nz_element_count())
       {
         const Cell & cB = B->get_cravel(b);
         if (cB.is_pointer_cell())
            {
              do_CR45_value(sub_prefix, cB.get_pointer_value().get());
            }
       }
}
//----------------------------------------------------------------------------
UCS_string
Quad_CR::temp_varname()
{
char cc[40];
timeval tv;
   gettimeofday(&tv, 0);
   SPRINTF(cc, "⍙¯%X¯%X", uint32_t(tv.tv_sec), uint32_t(tv.tv_usec));
   usleep(1000);   // to make the timestamp unique;

const UTF8_string varname_utf(cc);
   return UCS_string(varname_utf);
}
//----------------------------------------------------------------------------

