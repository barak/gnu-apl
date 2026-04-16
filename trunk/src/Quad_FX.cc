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

#include <stdlib.h>

#include "IntCell.hh"
#include "NativeFunction.hh"
#include "Quad_FX.hh"
#include "Security.hh"
#include "Symbol.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

Quad_FX Quad_FX::fun;

//============================================================================
Token
Quad_FX::do_eval_B(const Value * B)
{
   // monadic ⎕FX is simply dyadic A ⎕FX with default execution properties A
   //
static const int default_eprops[] = { 0, 0, 0, 0 };
   return do_quad_FX(default_eprops, B, UTF8_string("⎕FX"));
}
//----------------------------------------------------------------------------
Token
Quad_FX::do_eval_AB(const Value * A, const Value * B)
{
   CHECK_SECURITY(disable_native_functions);

   if (A->get_rank() > 1)         RANK_ERROR;

   // dyadic ⎕FX supports the following formats:
   //
   // 1.   "libname.so" ⎕FX "APL-name"   (native function)
   //
   //                                             exec properties  creator
   //                                             ------------------------
   // 2a.  N            ⎕FX "APL-text"            N  N  N  N       "⎕FX"
   // 2b.  N "creator"  ⎕FX "APL-text"            N  N  N  N       "creator"
   // 2c.  N1 N2 N3 N4           ⎕FX "APL-text"   N1 N2 N3 N4      "⎕FX"
   // 2d.  N1 N2 N3 N4 "creator" ⎕FX "APL-text"   N1 N2 N3 N4      "creator"
   //

   if (A->is_char_string())
      return Token(TOK_APL_VALUE1, do_native_FX(A, -1, B));

int eprops[4];
UTF8_string creator("⎕FX");

   switch(A->element_count())
      {
        case 2:   // format 2b.
             {
               const Value & C = *A->get_cravel(1).get_pointer_value().get();
               UCS_string creator_ucs(C);
               creator = UTF8_string(creator_ucs);
             }
             /* no break */
        case 1:   // format 2a.
             eprops[0] = A->get_cfirst().get_int_value();
             if (eprops[0] < 0)   DOMAIN_ERROR;
             if (eprops[0] > 1)   DOMAIN_ERROR;
             eprops[3] = eprops[2] = eprops[1] = eprops[0];
             break;

        case 5:   // format 2d.
             {
               const Value & C = *A->get_cravel(4).get_pointer_value().get();
               UCS_string creator_ucs(C);
               creator = UTF8_string(creator_ucs);
             }
             /* no break */
        case 4:   // format 2c.
             loop(e, 4)
                {
                  eprops[e] = A->get_cravel(e).get_int_value();
                  if (eprops[e] < 0)   DOMAIN_ERROR;
                  if (eprops[e] > 1)   DOMAIN_ERROR;
                }
             break;

        default: LENGTH_ERROR;
      }

   return do_quad_FX(eprops, B, creator);
}
//----------------------------------------------------------------------------
Token
Quad_FX::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   CHECK_SECURITY(disable_native_functions);

   if (!X->is_scalar_or_len1_vector())   AXIS_ERROR;
   if (A->get_rank() > 1)                RANK_ERROR;
   if (!A->is_char_string())             DOMAIN_ERROR;

const sAxis axis = Value::get_single_axis(X.get(), 10);
   return Token(TOK_APL_VALUE1, do_native_FX(A.get(), axis, B.get()));
}
//----------------------------------------------------------------------------
Token
Quad_FX::do_quad_FX(const int * exec_props, const Value * B,
                    const UTF8_string & creator)
{
   if (B->get_rank() > 2)   RANK_ERROR;
   if (B->get_rank() < 1)   RANK_ERROR;

UCS_string text;

   // ⎕FX accepts two kinds of arguments B:
   //
   // 1. A vector whose elements are the (nested) lines of the function, or
   // 2. A character matrix whose rows are the lines of the function.
   //
   // we convert each format into text, which is a UCS string with
   // lines separated by ASCII_LF.
   //
const bool keep_indent = !UserPreferences::uprefs.discard_indentation;
   if (B->compute_depth() >= 2)   // case 1: vector of simple character vectors
      {
        const ShapeItem rows = B->element_count();
        loop(row, rows)
           {
             const Cell & cell = B->get_cravel(row);
             if (cell.is_character_cell())   /// a line with a single char.
                {
                  // rare special case: single char. This can only occur if
                  // the user is using single APL quotes like 'a' (as opposed
                  // to double quotes like "a".
                  //
                  const Unicode uni = cell.get_char_value();
                  if (uni > UNI_SPACE || keep_indent)   text << uni;
                  text << UNI_LF;
                  continue;
                }

             // row has more than 1 character, so it must be nested
             if (!cell.is_pointer_cell())
                {
                  MORE_ERROR() << "⎕FX: Function line " << row
                               << " is not a string";
                  DOMAIN_ERROR;
                }

             Value_P line = cell.get_pointer_value();
             Assert(+line);

             Log(LOG_quad_FX)
                {
                  CERR << "[" << setw(2) << row << "] " << *line << endl;
                }

             if (line->is_char_vector())
                {
                  // this is the normal case. line is an APL string.
                  //
                  const ShapeItem line_len = line->element_count();
                  bool skipping = false;
                  loop(l, line_len)
                     {
                       const Cell & cell = line->get_cravel(l);
                       if (!cell.is_character_cell())
                          {
                            MORE_ERROR() << "non-char in line at " LOC;
                            DOMAIN_ERROR;
                          }

                       const Unicode uni = cell.get_char_value();
                       if (l == 0 || skipping)
                          skipping = (uni <= UNI_SPACE && !keep_indent);
                       if (!skipping)   text << uni;
                     }
                }
             else if (line->is_scalar())
                {
                  // this should not happen because line is a nested simple
                  // character scalar.
                  //
                  MORE_ERROR() << "Internal error: nested character scalar "
                               << *line << " at " LOC;
                  FIXME;
                }
             else
                {
                  line->print_boxed(CERR, 0);
                  MORE_ERROR() << "bad line at " LOC;
                  DOMAIN_ERROR;
                }

             text << UNI_LF;
           }
      }
   else                      // case 2: simple character matrix
      {
        const ShapeItem rows = B->get_rows();
        const ShapeItem cols = B->get_cols();
        const Cell * cB = &B->get_cfirst();

        loop(row, rows)
           {
             bool skipping = false;
             loop(col, cols)
                 {
                   const Unicode uni = cB++->get_char_value();
                   if (col == 0 || skipping)
                      skipping = (uni <= UNI_SPACE && !keep_indent);
                   if (!skipping)   text << uni;
                 }
             text << UNI_LF;
           }
      }

   return do_quad_FX(exec_props, text, creator);
}
//----------------------------------------------------------------------------
Token
Quad_FX::do_quad_FX(const int * exec_props, const UCS_string & text,
                    const UTF8_string & creator)
{
int error_line = 0;
UserFunction * fun = UserFunction::fix(text, error_line, false, LOC, creator);

   if (fun == 0)   // UserFunction::fix() dailed
      {
        Value_P Z = IntScalar(error_line + Workspace::get_IO(), LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (text[0] == UNI_LAMBDA)
      {
        const char * creator = "⎕FX";
        fun->increment_refcount(LOC, creator);
      }

   fun->set_exec_properties(exec_props);

const UCS_string fun_name = fun->get_name();
Value_P Z(fun_name, LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Value_P
Quad_FX::do_native_FX(const Value * A, sAxis axis, const Value * B)
{
   if (UserPreferences::uprefs.safe_mode)   DOMAIN_ERROR;

const UCS_string so_name       = A->get_UCS_ravel();
const UCS_string function_name = B->get_UCS_ravel();

   if (so_name.size() == 0)         LENGTH_ERROR;
   if (function_name.size() == 0)   LENGTH_ERROR;

NativeFunction * fun = NativeFunction::fix(so_name, function_name);
   if (fun == 0)  return IntScalar(0, LOC);

   return CLONE(B, LOC);
}
//============================================================================
