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

#include "Assert.hh"
#include "Avec.hh"
#include "Bif_F12_RHO.hh"
#include "CDR.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "PointerCell.hh"
#include "Quad_CR.hh"
#include "Quad_FX.hh"
#include "Quad_TF.hh"
#include "StateIndicator.hh"
#include "Symbol.hh"
#include "Tokenizer.hh"
#include "Token_string.hh"
#include "UCS_string_vector.hh"
#include "Workspace.hh"

Quad_TF   Quad_TF::fun;

enum { Quad_PP_TF = 17 };

//════════════════════════════════════════════════════════════════════════════
Token
Quad_TF::eval_AB(cValue_R A, cValue_R B) const
{
   // A should be an integer scalar or 1-element_vector with value 1 or 2
   //
   if (A.get_rank() > 0)         RANK_ERROR;
   if (A.element_count() != 1)   LENGTH_ERROR;

const APL_Integer mode = A.get_int_value(0);
const UCS_string symbol_name(B);

Value_P Z;

   if (mode == 1)
      {
        const bool inverse = is_inverse(symbol_name);
        if (inverse)   Z = tf1_inv(symbol_name);
        else           Z = tf1(symbol_name);
      }
   else if (mode == 2)
      {
        const bool inverse = is_inverse(symbol_name);
        if (!inverse)   return tf2(symbol_name);
        Z = Value_P(tf2_inverse(symbol_name), LOC);
      }
   else if (mode == 3)
      {
        Z = tf3(symbol_name);
      }
   else
      {
        DOMAIN_ERROR;
      }

   if (!Z)   DOMAIN_ERROR;

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::is_inverse(const UCS_string & maybe_name)
{
   // a forward ⎕TF contains only a function or variable name.
   // an inverse ⎕TF contains at least one non-symbol character
   //
   loop(n, maybe_name.size())
      {
        if (n == 0 && Avec::is_quad(maybe_name[n]))   continue;
        if (Avec::is_symbol_char(maybe_name[n]))      continue;

        return true;   // inverse ⎕TF
      }

   // all chars in val were symbol characters. Therefore it is a forward ⎕TF
   return false;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_TF::no_UCS(const UCS_string & ucs)
{
UCS_string ret;
const ShapeItem len = ucs.ssize();

   // Bugs31 #6 (Blake McBride): the encoder's own ',(⎕UCS n)' marker
   // starts with a quote that is only meaningful as a marker when it is
   // a GENUINE string terminator -- but the very same 7-character text
   // also occurs, with no encoding involved at all, whenever a doubled
   // '' (APL's standard escape for one literal quote WITHIN a string,
   // e.g. from re-quoting a function's own source text for transfer) is
   // immediately followed by literal source text that happens to read
   // ',(⎕UCS...)'. Both are lexically identical; a blind substring scan
   // (the previous implementation) cannot tell them apart and silently
   // decodes the second, corrupting the record from that point on.
   //
   // Fixed by tracking real string-boundary state exactly the way
   // Tokenizer::tokenize_string1() already does for ordinary APL source:
   // once inside a string, a '' is unconditionally two literal quotes
   // (checked via one-character lookahead, BEFORE ever testing the
   // longer marker pattern) and only a quote that is NOT followed by
   // another quote is a genuine terminator -- the marker pattern is
   // tested only at that point, never at the second half of an escape.
   //
bool in_string = false;

   for (ShapeItem u = 0; u < len;)
       {
         const Unicode uni = ucs[u];

         if (!in_string)
            {
              // Outside any string, a quote always unambiguously OPENS
              // one -- there is nothing to escape out here.
              //
              ret << uni;
              ++u;
              if (uni == '\'')   in_string = true;
              continue;
            }

         // in_string: scanning through a string's own content.
         //
         if (uni != '\'')   { ret << uni;   ++u;   continue; }

         if (u + 1 < len && ucs[u + 1] == '\'')
            {
              // '' -- one literal quote, string stays open. Copied
              // through unchanged (the real tokenizer that eventually
              // re-parses ret will un-escape it itself, exactly as for
              // ordinary source text).
              //
              ret << uni << uni;
              u += 2;
              continue;
            }

         // A quote NOT followed by another quote is a genuine
         // terminator -- and only here can a real encoder marker
         // legitimately begin (its own leading quote IS this
         // terminator; tf2_char_vec() closes the string and opens the
         // marker with the very same character).
         //
         if ( u + 6 < len &&
              ucs[u + 1] == ','  &&
              ucs[u + 2] == '('  &&
              ucs[u + 3] == UNI_Quad_Quad &&
              ucs[u + 4] == 'U'  &&
              ucs[u + 5] == 'C'  &&
              ucs[u + 6] == 'S')
            {
              u += 7;   // skip '(⎕UCS

              for (;;)
                  {
                   while (u < len && ucs[u] == ' ')   ++u;

                   if (u >= len)
                      {
                        MORE_ERROR() << "Truncated (⎕UCS ...) in ⎕TF record";
                        DOMAIN_ERROR;
                      }

                   if (ucs[u] == ')')
                      {
                        // skip ')' + the ',' + one reopening quote that
                        // tf2_char_vec() always emits right after (",'"
                        // mid-run, or ",''" at the end of the vector --
                        // either way the string is still logically open,
                        // so in_string stays true; a second reopening
                        // quote from the ",''" case is simply picked up
                        // as an ordinary terminator by the next round).
                        //
                        u = (u + 3 <= len) ? u + 3 : len;
                        break;
                      }

                   if (ucs[u] < '0' || ucs[u] > '9')
                      {
                        MORE_ERROR() << "Bad character in (⎕UCS ...) of "
                                        "⎕TF record";
                        DOMAIN_ERROR;
                      }

                   // at most #x7FFFFFFF, the largest value that ⎕UCS
                   // itself accepts (UTF-8 as of RFC 2279), so that every
                   // character that ⎕UCS can produce survives 2 ⎕TF. Checked
                   // while the digits are accumulated (in 64 bits) so that a
                   // corrupted or crafted record cannot overflow num into a
                   // garbage character (Blake McBride, Bugs32 #30)
                   //
                   int64_t num = 0;
                   while (u < len && ucs[u] >= '0' && ucs[u] <= '9')
                      {
                        num *= 10;   num += ucs[u++] - '0';
                        if (num > 0x7FFFFFFF)
                           {
                             MORE_ERROR() << "(⎕UCS ...) in ⎕TF record "
                                             "exceeds the range of ⎕UCS";
                             DOMAIN_ERROR;
                           }
                      }

                   ret << Unicode(num);
                  }

              // in_string remains true: the marker only ever appears
              // spliced into what is, logically, still one continuous
              // string.
              //
              continue;
            }

         // genuine terminator, no marker follows: the string just ends.
         //
         in_string = false;
         ret << uni;
         ++u;
       }

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::tf1(const UCS_string & name)
{
const NamedObject * obj = Workspace::lookup_existing_name(name);
   if (obj == 0)   return Str0(LOC);

const Function * function = obj->get_function();

   if (function)
      {
        if (obj->is_user_defined())   return tf1(name, *function);

        // quad function or primitive: return ''
        return Str0(LOC);
      }


   if (const Symbol * symbol = obj->get_symbol())
      {
        if (Value_P value = symbol->get_apl_value())   return tf1(name, value);

        /* not a variable: fall through */
      }

   return Str0(LOC);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_TF::tf2(const UCS_string & name)
{
const NamedObject * obj = Workspace::lookup_existing_name(name);
   if (obj == 0)
      {
        Log(LOG_Quad_TF)   CERR << "bad name in tf2(" << name << ")" << endl;
        return Token(TOK_APL_VALUE1, Str0(LOC));
      }

const Function * function = obj->get_function();

   if (function)
      {
        if (obj->is_user_defined())
           {
             UCS_string ucs = tf2_fun(name, *function);
             Value_P Z(ucs, LOC);
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }

        // quad function or primitive: return ''
        return Token(TOK_APL_VALUE1, Str0(LOC));
      }

const Symbol * symbol = obj->get_symbol();
   if (symbol)
      {
        if (Value_P value = symbol->get_apl_value())   return tf2_var(name, *value);

        /* not a variable: fall through */
      }

   Log(LOG_Quad_TF)   CERR << "error in tf2(" << name << ")" << endl;
   return Token(TOK_APL_VALUE1, Str0(LOC));
}
//════════════════════════════════════════════════════════════════════════════
void
Quad_TF::tf2_char_vec(UCS_string & ucs, const UCS_string & vec,
                      bool fun_line)
{
   if (vec.size() == 0)   // an empty function line
      {
        // must not vanish from the ⎕FX strand: that renumbered the lines
        // after it and re-aimed →N branches (Blake McBride, Bugs32 #9)
        //
        ucs << UNI_SINGLE_QUOTE << UNI_SINGLE_QUOTE;
        return;
      }

   // A vec containing at least one character needing ⎕UCS produces a
   // multi-token catenation, e.g. 'text ',(⎕UCS 8900),' more text' --
   // unlike a plain 'xxx' literal, that is NOT self-delimiting when it
   // is itself one bare element of a larger strand (as tf2_fun_ucs()'s
   // caller uses it, one call per ⎕FX line argument): without an outer
   // (...), the catenation chain is ambiguous about where this strand
   // element ends and the next one begins, confirmed live to make real
   // APL2's )IN reconstruct a function with the wrong number of lines
   // (e.g. a 9-line function coming back with 12, and a stray "←" of
   // its own). A plain literal with no ⎕UCS at all needs no such
   // wrapping, exactly as for tf2_shape()'s self_delimiting case above.
   //
   // in APL code (outside strings and comments), ⋆ ∧ ∼ are written as
   // * ^ ~ (their ATF codes) and )IN maps them back (Bugs37 #7)
   //
const std::vector<bool> in_text = fun_line
                                ? text_mask(vec)
                                : std::vector<bool>(vec.size(), true);

bool any_ucs = false;
   loop(v, vec.size())
       if (Avec::need_UCS(vec[v], in_text[v]))   { any_ucs = true;   break; }

   if (any_ucs)   ucs << UNI_L_PARENT;

bool in_UCS = false;
   ucs << UNI_SINGLE_QUOTE;

   loop(v, vec.size())
       {
         const Unicode uni = vec[v];
         const bool need_UCS = Avec::need_UCS(uni, in_text[v]);
         if (in_UCS != need_UCS)   // mode changed
            {
              if (in_UCS)   ucs << "),'";       // UCS() → 'xxx'
              else          ucs << "',(⎕UCS";   // 'xxx' → UCS()
              in_UCS = need_UCS;
            }

         if (in_UCS)
            {
              ucs << UNI_SPACE << int(uni);
            }
         else
            {
              ucs << uni;
              if (uni == UNI_SINGLE_QUOTE)   ucs << uni;
            }
       }

   if (in_UCS)   ucs << "),''";
   else          ucs << UNI_SINGLE_QUOTE;

   if (any_ucs)   ucs << UNI_R_PARENT;
}
//────────────────────────────────────────────────────────────────────────────
std::vector<bool>
Quad_TF::text_mask(const UCS_string & line)
{
std::vector<bool> mask(line.size(), false);
Unicode end = Invalid_Unicode;   // the end of the current string, if any
   loop(l, line.size())
       {
         const Unicode uni = line[l];
         if (end != Invalid_Unicode)   // in a string
            {
              mask[l] = true;
              if (uni == end)   end = Invalid_Unicode;
              else if (end == UNI_DOUBLE_QUOTE && uni == UNI_BACKSLASH &&
                       l + 1 < line.ssize())   mask[++l] = true;   // \x
              continue;
            }

         if (uni == UNI_COMMENT || uni == UNI_NUMBER_SIGN)   // comment
            {
              while (l < line.ssize())   mask[l++] = true;
              break;
            }

         if      (uni == UNI_SINGLE_QUOTE)   end = UNI_SINGLE_QUOTE;
         else if (uni == UNI_DOUBLE_QUOTE)   end = UNI_DOUBLE_QUOTE;
         else if (uni == UNI_LEFT_DAQ)       end = UNI_RIGHT_DAQ;
         if (end != Invalid_Unicode)   mask[l] = true;
       }

   return mask;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::ATF_principal_chars(const cValue & B)
{
UCS_string_vector lines;
   if (B.get_rank() == 2)   // character matrix
      {
        loop(r, B.get_rows())
           {
             UCS_string line;
             loop(c, B.get_cols())
                {
                  const ShapeItem idx = c + r*B.get_cols();
                  if (!B.is_character_cell(idx))   return Value_P();
                  line << B.get_char_value(idx);
                }
             lines.push_back(line);
           }
      }
   else if (B.is_char_string())   // a single line
      {
        lines.push_back(UCS_string(B));
      }
   else                           // nested vector of lines
      {
        loop(l, B.element_count())
           {
             UCS_string line;
             if (B.is_character_cell(l))
                line << B.get_char_value(l);
             else if (B.is_pointer_cell(l) &&
                      B.get_pointer_value(l)->is_char_string())
                line = UCS_string(*B.get_pointer_value(l));
             else
                return Value_P();   // not a function text
             lines.push_back(line);
           }
      }

Value_P Z(ShapeItem(lines.size()), LOC);
   loop(l, lines.size())
      {
        UCS_string & line = lines[l];
        const std::vector<bool> in_text = text_mask(line);
        loop(u, line.size())
           {
             if (in_text[u])   continue;
             if      (line[u] == UNI_ASTERISK)     line[u] = UNI_STAR_OPERATOR;
             else if (line[u] == UNI_CIRCUMFLEX)   line[u] = UNI_AND;
             else if (line[u] == UNI_TILDE)        line[u] = UNI_TILDE_OPERATOR;
           }
        Value_P ZZ(line, LOC);
        Z->next_ravel_Pointer(ZZ.get());
      }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_TF::tf2_fun_ucs(UCS_string & ucs, const UCS_string & fun_name,
                    const Function & fun)
{
   // Bugs28 #92: none of ⎕TF's function paths tested the lock bit
   // (get_exec_properties()[0]), so 2⎕TF (hence )OUT/)DUMP) disclosed a
   // locked function's body in plain text -- ⎕FX's own DEFN_ERROR on a
   // locked function (once #92's other fix closes that hole) would be
   // pointless if ⎕TF just handed the body back anyway.
   //
   if (fun.get_exec_properties()[0])
      {
        MORE_ERROR() << "⎕TF: '" << fun_name << "' is locked; its "
           "definition cannot be disclosed.";
        DOMAIN_ERROR;
      }

const UCS_string text = fun.canonical(false);

   if (fun.is_native())
      {
        CERR << "Warning: the workspace contains a native function '"
             << fun_name << "', making the" << endl
             << "   .atf output file incompatible with other APL interpreters."
             << endl;

        ucs << UNI_SINGLE_QUOTE << fun_name << "' ⎕FX '" << text
            << UNI_SINGLE_QUOTE;;
        return;
      }

   if (fun.is_lambda())
      {
        // )OUT and ⎕TF are for interchange with OTHER APL systems (via
        // .atf transfer-format files), none of which support lambdas
        // -- so round-tripping a lambda through them was never a real
        // requirement, and 2⎕TF's inverse direction (tf2_inverse())
        // need not (and does not) parse "{...}" lambda syntax at all:
        // that would need a real )SI push (Executable.cc's async
        // {...}-block compiler), which tf2_inverse()'s synchronous,
        // single-shot parser cannot provide. Rather than emit some
        // GNU-APL-only encoding that only round-trips through GNU APL
        // itself, refuse outright. See Bugs27 #52.
        //
        MORE_ERROR() << ")OUT/⎕TF: '" << fun_name << "' is a lambda "
           "(dfn); lambdas cannot be written to a transfer-format (.atf) "
           "file or produced by ⎕TF, since other APL systems (and GNU "
           "APL's own transfer-format reader) have no way to represent "
           "them. Use ⎕FX-style named functions for anything that "
           "needs to survive )OUT/)IN or ⎕TF.";
        DOMAIN_ERROR;
      }

UCS_string_vector lines;
   text.to_vector(lines);

   ucs << "⎕FX";

   loop(l, lines.size())
      {
        ucs << UNI_SPACE;
        tf2_char_vec(ucs, lines[l], true);
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_TF::tf2_inverse(const UCS_string & ravel, bool from_ATF)
{
Token_string tos;
   tos.emplace_back(Token(TOK_L_PARENT, int64_t(0)), Function_PC_invalid);

   Log(LOG_Quad_TF)   CERR << "inverse ⎕TF2: " << ravel << endl;

   try
      {
        UCS_string ucs1 = no_UCS(ravel);
        const Parser parser(PM_EXECUTE, LOC, false);
        parser.parse(ucs1, tos, true);
      }
   catch(Error &)
      {
        Log(LOG_Quad_TF)   CERR << "parse error in tf2_inverse()" << endl;
        return UCS_string();
      }
   catch(std::bad_alloc &)
      {
        Log(LOG_Quad_TF)   CERR << "parse error in tf2_inverse()" << endl;
        return UCS_string();
      }
   catch(...)
      { FIXME; }

   // tos is now ( VAR ← ... ) or: ( ⎕FX val ...
   // make it VAR ← ( ... )    or: leave as is
   //
   {
     if (tos.size() >= 3 && tos[1].get_tag() != TOK_Quad_FX)
        {
          tos[0].copy(tos[1], LOC);   // move VAR
          tos[1].copy(tos[2], LOC);   // move ←
          // marker value 1 (vs. the 0 used by real parentheses)
          // distinguishes this synthetic outermost '(' ... ')' pair,
          // added purely so tf2_reduce() has something to reduce, from
          // real parentheses written by the user. tf2_reduce_parentheses()
          // must NOT enclose the value it wraps: unlike a real
          // parenthesised strand item, this pair is not part of the
          // transfer form at all (see Blake's Bugs23 #1).
          tos[2].copy(Token_loc(Token(TOK_L_PARENT, int64_t(1)), Function_PC_invalid), LOC);
        }
   }
    tos.emplace_back(Token(TOK_R_PARENT, int64_t(0)), Function_PC_invalid);

   // simplify tos as much as possible. We replace A⍴B by reshaped B
   // and glue values together.
   //
   tf2_reduce(tos);

   Log(LOG_Quad_TF)
      {
        CERR << "inverse ⎕TF2: tos[" << tos.size() << "] after reduce() is: ";
        tos.print(CERR, false);
        CERR << endl;
      }

   if (tos.size() < 2)
      {
        Log(LOG_Quad_TF)
           CERR << "short tos in tf2_inverse()" << tos.size() << endl;
        return UCS_string();   // too short for an inverse 2⎕TF
      }

   // it could happen that some system variable ⎕XX which is not known by
   // GNU APL is read. This case is parsed as ⎕ XX ← ... 
   // Issue a warning in that case
   //
   if (tos[0].get_tag() == TOK_Quad_Quad &&
       tos[1].get_Class() == TC_SYMBOL)
      {
        UCS_string new_var_or_fun = tos[1].get_sym_ptr()->get_name();
        CERR << "*** Unknown system variable ⎕" << new_var_or_fun
             << " in 2⎕TF / )IN (assignment ignored)" << endl;
        return new_var_or_fun;
      }

   /* we expect either   VAR ← VALUE
      or:              ( ⎕FX fun-text    )

      Try VAR ← VALUE first.
    */
   if (tos[0].get_Class() == TC_SYMBOL && tos[1].get_Class() == TC_ASSIGN)
      {
        // at this point, we expect SYM ← VALUE. A structured-variable
        // transfer form SYM ← ( 38 ⎕CR VALUE ), as produced by
        // tf2_var()/tf2_ravel() for a B.is_member() value (at any
        // nesting level), was already reduced to a plain SYM ← VALUE by
        // tf2_reduce_CR38() above -- which actually calls 38⎕CR
        // (Quad_CR::do_CR38()) rather than just producing the plain
        // matrix, so is_member() is already correctly set on the value
        // assigned below.
        //
        if (tos.size() != 3)                  return UCS_string();
        if (tos[2].get_Class() != TC_VALUE)   return UCS_string();

        // a read-only system variable (e.g. ⎕TS) ignores the assignment,
        // so the transfer form is invalid (lrm p. 339, Bugs37 #75)
        //
        if (dynamic_cast<const RO_SystemVariable *>(tos[0].get_sym_ptr()))
           return UCS_string();

         // the assignment can fail, e.g. for a system variable with an
         // invalid value like ⎕IO←7. Then the transfer form is invalid
         // and Z is '' (lrm p. 339), so that )IN can continue with the
         // next record (Bugs37 #48).
         //
         // Run the assignment in safe execution mode (like ⎕EA) so that
         // a failure is not displayed, and restore the error of the )SI
         // entry afterwards.
         //
         StateIndicator * si = Workspace::SI_top();
         const int saved_depth = si ? si->get_safe_execution_depth() : 0;
         Error saved_error = si ? StateIndicator::get_error(si)
                                : Error(E_NO_ERROR, LOC);
         if (si)   si->set_safe_execution_depth();

         try
            {
              tos[0].get_sym_ptr()->assign(tos[2].get_apl_val(), true, LOC);
            }
         catch (Error &)
            {
              if (si)
                 {
                   si->restore_safe_execution_depth(saved_depth);
                   StateIndicator::get_error(si) = saved_error;
                 }
              Log(LOG_Quad_TF)
                 CERR << "assignment failed in tf2_inverse()" << endl;
              return UCS_string();
            }
         if (si)   si->restore_safe_execution_depth(saved_depth);
         Log(LOG_Quad_TF)   CERR << "valid inverse 2 ⎕TF" << endl;
         return tos[0].get_sym_ptr()->get_name();   // valid 2⎕TF
      }

   // try dyadic ⎕FX (native function)
   //
   // Then tos[5] is: ( VALUE ⎕FX VALUE )
   //
   if (tos.size() == 5                   &&
       tos[1].get_Class() == TC_VALUE    &&
       tos[2].get_tag()   == TOK_Quad_FX &&
       tos[3].get_Class() == TC_VALUE)
      {
        const cValue * fname   =  tos[1].get_apl_val().get();
        const cValue * so_path =  tos[3].get_apl_val().get();

        const Token tok = Quad_FX::do_eval_AB(*so_path, *fname);
        if (tok.get_Class() == TC_VALUE)   // ⎕FX successful
           {
             Value_P val = tok.get_apl_val();
        Log(LOG_Quad_TF)
           CERR << "valid inverse 2 ⎕TF (native function):" << endl;
             return UCS_string(*val.get());
           }

        Log(LOG_Quad_TF)
           CERR << "invalid inverse 2 ⎕TF (native function):" << endl;
        return UCS_string();
      }

   /* monadic ⎕FX. at this point we should have:

      tos[4] =  ( ⎕FX Value )
    */

   Log(LOG_Quad_TF)
      CERR << "inverse 2 ⎕TF (monadic ⎕FX):" << endl;
   if (tos.size() != 4)                   return UCS_string();
   if (tos[1].get_tag() != TOK_Quad_FX)   return UCS_string();
   if (tos[2].get_Class() != TC_VALUE)    return UCS_string();

Value_P B = tos[2].get_apl_val();
   if (from_ATF)   // * ^ ~ in APL code → ⋆ ∧ ∼ (Bugs37 #7)
      {
        Value_P B1 = ATF_principal_chars(*B);
        if (!!B1)   B = B1;
      }

static const int eprops[] = { 0, 0, 0, 0 };
const Token tok = Quad_FX::do_quad_FX(eprops, *B, UTF8_string("2 ⎕TF"));

   // ⎕FX returns the (integer) row number of the offending line if it
   // has failed (Bugs37 #21)
   //
   if (tok.get_Class() != TC_VALUE || !tok.get_apl_val()->is_char_string())
      {
        Log(LOG_Quad_TF)
           CERR << "Quad_FX::do_quad_FX() failed in tf2_inverse()" << endl;
        return UCS_string();   // error in ⎕FX
      }

   // ⎕FX succeeded (and the token returned is the name of the function)
   //
   return UCS_string(*tok.get_apl_val().get());
}
//════════════════════════════════════════════════════════════════════════════
bool
Quad_TF::tf2_value(int level, UCS_string & ucs, const cValue & value,
                   ShapeItem nesting)
{
   Log(LOG_Quad_TF)
      {
        char cc[100];
        SPRINTF(cc, "tf2_value() initial level %u\n", level);
        CERR << "tf2_value(): ucs before at level " << level << ": "
             << ucs << endl << cc;
        value.print_boxed(CERR, 0);
      }

   // Bugs9 #8 (Blake McBride): this shortcut is only correct for the plain
   // '' case (rank 1, shape 0, not enclosed) -- it used to fire for *any*
   // empty character value (any shape, any nesting level), discarding both
   // the shape (e.g. 2 0⍴'a' came back as '') and the enclosure (e.g.
   // ,⊂⊂'' lost both ⊂). Restrict it accordingly; everything else falls
   // through to tf2_shape() + tf2_all_char_ravel(), which already handle
   // shape and nesting correctly.
   //
   if (value.is_empty()           &&
       nesting == 0               &&
       value.get_rank() == 1      &&
       value.get_shape_item(0) == 0)
      {
        Cell cache;
        const Cell & cell = value.get_cfirst(cache);
        if (cell.is_character_cell())
           {
             // left as an unconditional wrap for now (matching this
             // shortcut's pre-existing behavior) even though an empty
             // string can also be a bare strand element (nesting==0)
             // suffering the same redundant-parens issue tf2_shape() now
             // fixes for the general case -- deliberately out of scope
             // for this change; revisit separately if it matters live.
             //
             ucs << UNI_L_PARENT << UNI_SINGLE_QUOTE
                 << UNI_SINGLE_QUOTE << UNI_R_PARENT;
             return true;
           }
      }

const ShapeItem ec = value.nz_element_count();

   // a rank-1, simple (non-nested), unenclosed value's own bare literal
   // or strand ravel already has this exact shape when re-parsed, so an
   // explicit reshape is redundant (LanguageVariances.md #34b, e.g.
   // 'ABC' or 1 2 3 rather than 3⍴'ABC' / 3⍴1 2 3). Lengths 0 and 1 are
   // the two exceptions: a bare single-item literal like 'b' or 5 parses
   // back as a *scalar* (rank 0), not a 1-element vector -- confirmed
   // live (⍴'b' is empty, ⍴,'b' is 1); a bare *empty* numeric literal
   // doesn't exist at all in APL syntax (unlike '' for characters,
   // already handled separately above) -- dropping the 0⍴ prefix left
   // nothing meaningful to write, corrupting the value on re-evaluation
   // (⍳0 round-tripped as scalar 0 instead of an empty vector, caught by
   // testcases/Quad_TF.tc's own test_2TF round-trip check). So the
   // explicit N⍴ reshape is still required for length 0 or 1 (caught by
   // testcases/ZZ64_Regression.tc's structured-value round trip before
   // landing this).
   //
   // A third exception: a character vector containing any codepoint
   // outside ⎕AV is encoded by tf2_all_char_ravel() below as ⎕UCS N1 N2
   // ... rather than a quoted literal. Plain ⍎ of the resulting text
   // round-trips fine either way, but 2⎕TF's own inverse parser
   // (tf2_inverse() / tf2_reduce_UCS()) only successfully folds the
   // "⎕UCS value" pair back into a clean VAR←VALUE pair when it is
   // wrapped in an explicit N⍴(...); without the wrapper the inverse
   // direction silently fails. Caught by testcases/Quad_TF.tc's Kanji
   // ⎕UCS round-trip case (test_2TF ⎕UCS 256⊥¨...).
   //
bool needs_UCS = false;
   if (!value.NOTCHAR())
      {
        loop(e, ec)
           {
             if (Avec::need_UCS(value.get_char_value(e)))
                { needs_UCS = true;   break; }
           }
      }

const bool omit_reshape = nesting == 0                &&
                          value.get_rank() == 1       &&
                          value.get_shape_item(0) > 1 &&
                          value.is_simple()           &&
                          !needs_UCS;

   // emit e.g. ( shape ⍴
   //
   // self_delimiting: only a character (quoted-string) literal's own
   // syntax marks its boundaries without external grouping -- see
   // tf2_shape()'s own comment.
   //
const bool has_parens = tf2_shape(ucs, value.get_shape(), nesting,
                                  omit_reshape, !value.NOTCHAR());
   if (value.NOTCHAR())   tf2_ravel(level, ucs, ec, value, 0);
   else                   tf2_all_char_ravel(level, ucs, value);
   if (has_parens)   ucs << UNI_R_PARENT;   // close '(' from tf2_shape()

   Log(LOG_Quad_TF)
      {
        CERR << "tf2_value(): ucs after at level " << level
             << ": " << ucs << endl;
      }

   return has_parens;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_TF::tf2_var(const UCS_string & var_name, const cValue & B)
{
   Log(LOG_Quad_TF)   CERR << "tf2_var(" << var_name << ")" << endl;

   // B.is_member() (a structured variable) is, at the shape/ravel level,
   // already exactly the plain N×2 nested matrix that 38⎕CR (Quad_CR.cc,
   // "plain → structure") turns back into a structured value -- so
   // tf2_value() below (which just walks shape/ravel, oblivious to
   // is_member()) already produces a correct textual encoding of that
   // matrix. The only thing missing was re-applying the "structured"
   // property on reconstruction, which is exactly what wrapping the
   // encoded matrix in "38⎕CR(...)" (instead of bare "(...)") below
   // achieves; tf2_inverse() parses the result as one VAR←(...) / VAR←fn
   // expression, so this fits its existing single-expression contract
   // without needing any change there.
   //
const bool structured = B.is_member();

UCS_string ucs_value; /// the right hand side of VAR←VALUE
bool has_parens;
   if (B.is_scalar() && !B.is_simple_scalar())
      {
        Assert(B.is_pointer_cell(0));
        // nesting == 1 here always makes tf2_shape()/tf2_value() emit
        // parentheses (see their own need_parens logic), so has_parens
        // is always true on this branch -- kept as a real variable
        // (not assumed) for symmetry with the branch below.
        has_parens = tf2_value(0, ucs_value, *B.get_pointer_value(0), 1);
      }
   else
      {
        // nesting == 0: tf2_value() may now legitimately return false
        // for a bare, unenclosed, non-reshaped literal/strand (e.g.
        // '1^1' or 1 2 3) -- no outer parentheses were emitted at all,
        // so there is nothing to strip below. See tf2_shape()'s own
        // comment for why unconditional parentheses here were wrong.
        //
        has_parens = tf2_value(0, ucs_value, B, 0);
      }

   if (has_parens)
      {
        Assert(ucs_value.front() == UNI_L_PARENT);
        Assert(ucs_value.back()  == UNI_R_PARENT);
      }

UCS_string ucs(var_name);
   ucs << UNI_LEFT_ARROW;

   if (structured)
      {
        // 38⎕CR (plain array → structured value, Quad_CR.cc) turns the
        // matrix back into a proper structured value on reconstruction;
        // keep ucs_value's outer parentheses this time since they are
        // now needed to bind ucs_value as 38⎕CR's right argument. The
        // extra outer (...) makes "38⎕CR(...)" unambiguously one self-
        // contained expression regardless of what surrounds it (matters
        // when this whole VAR←... text is itself embedded as one strand
        // element of an enclosing structured value, see tf2_ravel()).
        // (has_parens is always true here, see above, so ucs_value does
        // have the outer parentheses this relies on.)
        //
        ucs << "(38⎕CR" << ucs_value << UNI_R_PARENT;
      }
   else if (has_parens)
      {
        // copy ucs_value except the outer parrentheses
        for (ShapeItem v = 1; v < (ucs_value.ssize() - 1); ++v)
            ucs << ucs_value[v];
      }
   else
      {
        // no outer parentheses were emitted at all -- copy verbatim.
        ucs << ucs_value;
      }

   Log(LOG_Quad_TF)   CERR << "success in tf2_var(): " << ucs << endl;

Value_P Z(ucs, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::tf3(const UCS_string & name)
{
const NamedObject * obj = Workspace::lookup_existing_name(name);
   if (obj == 0)   return Str0(LOC);

   if (obj->get_function())
      {
        // 3⎕TF is not defined for functions.
        //
        return Str0(LOC);
      }

const Symbol * symbol = obj->get_symbol();
   if (symbol)
      {
        // NOTE: some system variables (e.g. ⎕SYL) compute a fresh value
        // in get_apl_value(), so the Value_P must stay alive while the
        // value is being used.
        //
        if (const Value_P value = symbol->get_apl_value())
           {
             CDR_string cdr;
             CDR::to_CDR(cdr, value.get());

             return Value_P(cdr, LOC);
           }

        /* not a variable: fall through */
      }

   return Str0(LOC);
}
//════════════════════════════════════════════════════════════════════════════
bool
Quad_TF::tf2_shape(UCS_string & ucs, const Shape & shape, ShapeItem nesting,
                   bool omit_reshape, bool self_delimiting)
{
   // Parentheses are needed here to group an actual ⊂ (enclose) prefix,
   // an actual N⍴ (reshape) prefix, or a bare multi-item literal/strand
   // ravel that is NOT self-delimiting -- without them, a bare numeric
   // strand element (e.g. "5 14", one cell of a larger nested value)
   // would silently merge with an adjacent sibling strand element into
   // one longer flat vector on re-parsing (confirmed live:
   // 2 2⍴5 14 6 16 15 28 18 32 instead of the intended
   // 2 2⍴(5 14) (6 16) (15 28) (18 32)). A bare CHARACTER literal (a
   // quoted string) does not have this problem -- APL's own syntax
   // already delimits it via its quotes, so 'ab' 'cd' unambiguously
   // stays two strand members without any wrapping needed at all.
   // Emitting parentheses unconditionally in every case used to wrap
   // even such a self-delimiting bare string in redundant parentheses,
   // e.g. 2⍴('1^1') ('1∨1') instead of 2⍴'1^1' '1∨1' -- harmless to GNU
   // APL's own (lenient) )IN, but the LRM's own Appendix B ("redundant
   // blanks and parentheses are prohibited") forbids it, and real APL2
   // (confirmed live) accepts the )IN but then fails to ⍎ or ⎕UCS the
   // affected strand element, even though it displays and reshapes
   // normally -- the corruption is silent until the value is actually
   // used. Callers must use the return value to decide whether to emit
   // a matching ')' (tf2_value() does so immediately below; tf2_var()
   // propagates it further, see its own comment).
   //
const bool need_parens = nesting > 0 ||
                          (shape.get_rank() && !omit_reshape) ||
                          (omit_reshape && !self_delimiting);
   if (need_parens)   ucs << UNI_L_PARENT;
   loop(n, nesting)   ucs << UNI_SUBSET;   // ⊂...

   // scalars are ''⍴SCALAR but ''⍴ has no effect and can be omitted;
   // likewise a rank-1 shape whose own bare literal/strand ravel already
   // has that shape (omit_reshape, set by the caller for simple,
   // unenclosed values -- see its own declaration)

   if (shape.get_rank() && !omit_reshape)   // non-scalar
      {
        loop(r, shape.get_rank())
            {
              if (r)   ucs << UNI_SPACE;
              ucs << shape.get_shape_item(r);
            }

        ucs << UNI_RHO;
      }

   return need_parens;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_TF::tf2_ravel(int level, UCS_string & ucs, const ShapeItem len,
                   cValue_R V, ShapeItem idx)
{
   Assert(len > 0);

   loop(e, len)
       {
         if (e)   ucs << UNI_SPACE;
         Cell cache;
         const Cell & cell = V.get_cravel(idx++, cache);

         if (Value_P sub_val = cell.try_pointer_value())
            {
              ShapeItem nesting = 0;
              while (sub_val->is_scalar())
                    {
                      Assert(sub_val->is_pointer_cell(0));
                      ++nesting;
                      sub_val = sub_val->get_pointer_value(0);
                    }

              // Bugs9 #8 (Blake McBride), broader than reported: for len>1
              // the ⊂ around each item is not printed explicitly -- the
              // items are simply space-juxtaposed, and APL's own strand
              // notation ("A B" auto-encloses each strand member) recreates
              // the missing PointerCell when the record is re-evaluated.
              // With len==1 there is no second strand member to trigger
              // that, so the sole element would be emitted bare and one
              // level of enclosure silently vanishes on re-evaluation
              // (reproduced independently of any empty array: enclosing
              // 1 2 3 once and ravelling to a 1-element vector came back
              // as the plain scalar 1, since reshaping a 1-element result
              // to shape 1 without the compensating enclose just truncates
              // to the first element instead of nesting it).
              // Compensate by enclosing the lone element explicitly.
              if (len == 1)   ++nesting;

              if (sub_val->is_member())   // nested structured value
                 {
                   // as in tf2_var(): the encoding below is already a
                   // correct plain-matrix encoding of *sub_val, oblivious
                   // to is_member(); wrap it in 38⎕CR(...) so that
                   // tf2_reduce_CR38() (part of the inverse ⎕TF2 pipeline)
                   // re-applies the "structured" property when this
                   // sub-expression is reduced back to a value, the same
                   // way the top-level 38⎕CR wrap in tf2_var() does for
                   // B itself.
                   //
                   // The extra outer (...) makes "38⎕CR(...)" unambiguous
                   // as its own strand element -- without it, the space
                   // between the preceding strand element and "38" would
                   // let APL's own strand-forming absorb "38" into that
                   // element instead of leaving it as the left argument
                   // of dyadic ⎕CR here.
                   //
                   UCS_string sub_ucs;
                   tf2_value(level + 1, sub_ucs, *sub_val, nesting);
                   ucs << "(38⎕CR" << sub_ucs << UNI_R_PARENT;
                 }
              else
                 {
                   tf2_value(level + 1, ucs, *sub_val, nesting);
                 }
           }
        else if (cell.is_lval_cell())
           {
             DOMAIN_ERROR;
           }
        else if (cell.is_complex_cell())
           {
             PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
             bool scaled = true;
             UCS_string ucs1(cell.get_real_value(), scaled, pctx);
             UCS_string ucs2(cell.get_imag_value(), scaled, pctx);
             ucs << ucs1 << UNI_J << ucs2;
           }
        else if (cell.is_float_cell())
           {
             PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
             bool scaled = true;
             UCS_string ucs1(cell.get_real_value(), scaled, pctx);
             ucs << ucs1;
           }
        else
           {
             PrintContext pctx(PR_APL_FUN);
             const PrintBuffer pb = cell.character_representation(pctx);
             ucs << pb.l1();
           }
        }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_TF::tf2_all_char_ravel(int level, UCS_string & ucs, const cValue & value)
{
const ShapeItem ec = value.nz_element_count();

   // check if ⎕UCS is needed. 
   //
   // ⎕UCS is needed if the string contains a character that is:
   //
   // 1. not in our ⎕AV, or
   // 2. not in IBM's ⎕AV
   //
   bool use_UCS = false;
   loop(e, ec)
       {
         const Unicode uni = value.get_char_value(e);
         if (Avec::need_UCS(uni))   { use_UCS = true;   break; }
       }

   if (use_UCS)
      {
        ucs << "⎕UCS";
        loop(e, ec)
            {
              ucs << UNI_SPACE << int(value.get_char_value(e));
            }
      }
   else
      {
        ucs << UNI_SINGLE_QUOTE;
        loop(e, ec)
            {
              const Unicode uni = value.get_char_value(e);
              ucs << uni;
              if (uni == UNI_SINGLE_QUOTE)   ucs << UNI_SINGLE_QUOTE;
            }
        ucs << UNI_SINGLE_QUOTE;
      }
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::tf1(const UCS_string & var_name, Value_P val)
{
   // the migration form cannot represent a nested array (LRM p.338): ''
   // as for a non-empty nested value below. For an empty value, cell 0 is
   // the prototype, and the per-cell check below never runs, so an empty
   // nested value was written as a numeric one (Bugs34 #13).
   //
   if (val->element_count() == 0 && val->is_pointer_cell(0))
      return Str0(LOC);

const bool is_char_array = val->is_character_cell(0);
UCS_string ucs(is_char_array ? UNI_C : UNI_N);

   ucs << var_name << UNI_SPACE << val->get_rank();               // rank

   loop(r, val->get_rank())
      {
        ucs << UNI_SPACE << val->get_shape_item(r);   // shape
      }

const ShapeItem ec = val->element_count();

   if (is_char_array)
      {
        ucs << UNI_SPACE;

        const RavelType rt = val->get_ravel_type();
        if (rt & RPT_char)   // RPT_UNICODE16 / RPT_UNICODE32 — all cells guaranteed char
           {
             loop(e, ec)   ucs << val->get_char_value(e);
           }
        else
           {
             loop(e, ec)
                {
                  Cell cache;
                  const Cell & cell = val->get_cravel(e, cache);
                  if (!cell.is_character_cell())   return  Str0(LOC);
                  ucs << cell.get_char_value();
                }
           }
      }
   else   // number
      {
        // Bugs9 #6 (Blake McBride): the char branch above always emits a
        // separator after the shape, but this branch relies on each
        // element's own leading space to double as that separator -- so
        // an empty numeric array (ec == 0) emitted none, and its own
        // inverse then rejected the record ("missing space (in shape)"),
        // since the shape reader requires a space after every shape item
        // including the last. Only ec == 0 actually needs the extra
        // separator here (LanguageVariances.md #34c: unconditionally
        // adding it doubled the space before the first element whenever
        // ec > 0 -- harmless for GNU APL's own whitespace-insensitive
        // reader, but a needless deviation from the LRM's single-space
        // form).
        //
        if (ec == 0)   ucs << UNI_SPACE;

        const RavelType rt = val->get_ravel_type();
        if (rt == RPT_CELLS)
           {
             loop(e, ec)
                {
                  ucs << UNI_SPACE;
                  Cell cache;
                  const Cell & cell = val->get_cravel(e, cache);
                  if (cell.is_integer_cell())
                     {
                       const int sign_pos = ucs.size();
                       ucs << cell.get_int_value();
                       if (ucs[sign_pos] == '-')   ucs[sign_pos] = UNI_OVERBAR;
                     }
                  else if (cell.is_numeric() && !(cell.is_complex_cell() &&
                                                  cell.get_imag_value() != 0.0))
                     {
                       // real, or complex with an imaginary part of exactly
                       // 0: 1 ⎕TF must never drop a nonzero imaginary part
                       // (Blake McBride, Bugs32 #13), unlike is_near_real().
                       //
                       PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
                       bool scaled = true;
                       UCS_string ucs1(cell.get_real_value(), scaled, pctx);
                       ucs << ucs1;
                     }
                  else if (cell.is_numeric())
                     {
                       PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
                       bool scaled = true;
                       UCS_string ucs1(cell.get_real_value(), scaled, pctx);
                       ucs << ucs1;
                       ucs << UNI_J;
                       ucs1 = UCS_string(cell.get_imag_value(), scaled, pctx);
                       ucs << ucs1;
                     }
                  else
                     {
                       // 1⎕TF's migration/N-record format has no way to
                       // represent a nested value; the LRM (p.338) says
                       // this shall silently yield '' rather than an
                       // error (LanguageVariances.md #35) -- same
                       // convention already used by the character-array
                       // branch above for the same "can't represent
                       // this" situation.
                       return Str0(LOC);
                     }
                }
           }
        else if (rt & RPT_integer)
           {
             loop(e, ec)
                {
                  ucs << UNI_SPACE;
                  const int sign_pos = ucs.size();
                  ucs << val->get_int_value(e);
                  if (ucs[sign_pos] == '-')   ucs[sign_pos] = UNI_OVERBAR;
                }
           }
        else if (rt == RPT_FLOAT64)
           {
             PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
             loop(e, ec)
                {
                  ucs << UNI_SPACE;
                  bool scaled = true;
                  UCS_string ucs1(val->get_real_value(e), scaled, pctx);
                  ucs << ucs1;
                }
           }
        else   // RPT_COMPLEX — per-element near_real check still needed
           {
             PrintContext pctx(PR_APL_MIN, Quad_PP_TF, MAX_Quad_PW);
             loop(e, ec)
                {
                  ucs << UNI_SPACE;
                  bool scaled = true;
                  UCS_string ucs1(val->get_real_value(e), scaled, pctx);
                  ucs << ucs1;
                  if (val->get_imag_value(e) != 0.0)   // Bugs32 #13
                     {
                       ucs << UNI_J;
                       scaled = true;
                       ucs1 = UCS_string(val->get_imag_value(e), scaled, pctx);
                       ucs << ucs1;
                     }
                }
           }
      }

   return Value_P(ucs, LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::tf1(const UCS_string & fun_name, const Function & fun)
{
   if (fun.is_lambda())
      {
        // same rationale as tf2_fun_ucs() (Bugs27 #52): 1⎕TF wrote out
        // canonical() of the lambda under its own header (λ←λ1 ⍵, say)
        // and, on the way back in, "fixed" that text under the header's
        // own name -- redefining λ1 (the lambda's synthetic internal
        // name) instead of the variable the caller assigned T to,
        // leaving the original name a VALUE ERROR. Refuse outright, for
        // the same reason 2⎕TF already does.
        //
        MORE_ERROR() << "⎕TF: '" << fun_name << "' is a lambda "
           "(dfn); lambdas cannot be written to a transfer-format (.atf) "
           "file or produced by ⎕TF, since other APL systems (and GNU "
           "APL's own transfer-format reader) have no way to represent "
           "them. Use ⎕FX-style named functions for anything that "
           "needs to survive )OUT/)IN or ⎕TF.";
        DOMAIN_ERROR;
      }

   // Bugs28 #92: see tf2_fun_ucs()'s identical check.
   //
   if (fun.get_exec_properties()[0])
      {
        MORE_ERROR() << "⎕TF: '" << fun_name << "' is locked; its "
           "definition cannot be disclosed.";
        DOMAIN_ERROR;
      }

const UCS_string text = fun.canonical(false);
UCS_string_vector lines;
const size_t max_len = text.to_vector(lines);

UCS_string ucs;
   ucs << UNI_F << fun_name
       << UNI_SPACE << 2                    // rank
       << UNI_SPACE << int(lines.size())    // rows
       << UNI_SPACE << int(max_len)         // cols
       << UNI_SPACE;

   loop(l, lines.size())
      {
       ucs << lines[l];
       loop(c, max_len - lines[l].size())   ucs << UNI_SPACE;
      }

   return Value_P(ucs, LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_TF::tf1_inv(const UCS_string & ravel)
{
const int len = ravel.size();

   if (len < 2)
      {
        MORE_ERROR() << "1 ⎕TF record too short";
        return Value_P();
      }

   // mode should be 'F', 'N', or 'C'.
const Unicode mode = ravel[0];
   if (mode != 'C' && mode != 'F' && mode != 'N')
      {
        MORE_ERROR() << "1 ⎕TF record type not F, N, or C";
        return Value_P();
      }

UCS_string name(ravel[1]);
   if (!Avec::is_quad(name[0]) && !Avec::is_first_symbol_char(name[0]))
      {
        MORE_ERROR() << "Bad variable name in 1 ⎕TF record";
        return Value_P();
      }

ShapeItem idx = 2;
   while (idx < len && Avec::is_symbol_char(ravel[idx]))
         name << ravel[idx++];

   if (Avec::is_quad(name[0]) && name.size() == 1)
      {
        MORE_ERROR() << "Bad variable name in 1 ⎕TF record";
        return Value_P();
      }

   if (idx >= len || ravel[idx++] != UNI_SPACE)
      {
        MORE_ERROR() << "missing space (before rank) in 1 ⎕TF record";
        return Value_P();
      }

ShapeItem idx0 = idx;
ShapeItem rank64 = 0;   // Bugs9 #3 (Blake McBride): accumulate wide and bound
                        // as we go -- sRank rank = ...; rank = 10*rank + ...
                        // wraps int16_t silently (e.g. a claimed rank of
                        // 65536 wraps to 0 and sails past the > MAX_RANK
                        // check below), letting a bogus record parse as if
                        // it had a small, valid rank.
   while (idx < len && Avec::is_digit(ravel[idx]))
      {
        rank64 = 10 * rank64 + ravel[idx++] - UNI_0;
        if (rank64 > MAX_RANK)
           {
             MORE_ERROR() << "max. rank exceeded in 1 ⎕TF record";
             return Value_P();
           }
      }
const sRank rank = sRank(rank64);

   if (rank == 0 && idx0 == idx)
      {
        MORE_ERROR() << "missing rank in 1 ⎕TF record";
        return Value_P();
      }

   if (idx >= len || ravel[idx++] != UNI_SPACE)
      {
        MORE_ERROR() << "missing space (after rank) in 1 ⎕TF record";
        return Value_P();
      }

Shape shape;
   loop(r, rank)
      {
        idx0 = idx;
        ShapeItem sh = 0;
        while (idx < len && Avec::is_digit(ravel[idx]))
           {
             // Bugs9 #3: same wrap hazard as rank above, but here the
             // accumulator (ShapeItem = int64_t) can also form an
             // overflowing (UB) signed sum given enough digits, so bound
             // *before* multiplying/adding rather than after.
             if (sh > (LARGE_INT - 9) / 10)
                {
                  MORE_ERROR() << "shape item too large in 1 ⎕TF record";
                  return Value_P();
                }
             sh = 10 * sh + ravel[idx++] - UNI_0;
           }
        if (sh == 0 && idx0 == idx)   // no shape
           {
             MORE_ERROR() << "too few shape items in 1 ⎕TF record";
             return Value_P();
           }

        if (idx >= len || ravel[idx++] != UNI_SPACE)
           {
             MORE_ERROR() << "missing space (in shape) in 1 ⎕TF record";
             return Value_P();
           }
        shape.add_shape_item(sh);
      }

const Symbol * symbol = 0;
NameClass nc = NC_UNUSED_USER_NAME;
const NamedObject * sym_or_fun = Workspace::lookup_existing_name(name); 
   if (sym_or_fun)   // existing name
      {
        symbol = sym_or_fun->get_symbol();
        nc = sym_or_fun->get_NC();
        if (symbol && symbol->is_readonly())
           {
             MORE_ERROR() << "symbol cannot be modified in 1 ⎕TF record";
             return Value_P();
           }
     }

const int data_chars = len - idx;

   if (mode == UNI_F)   // function record
      {
        if (rank != 2)
           {
             MORE_ERROR() << "function text is not a matrix in 1 ⎕TF record";
             return Value_P();
           }

        if (nc != NC_UNUSED_USER_NAME &&
            nc != NC_FUNCTION         &&
            nc != NC_OPERATOR)
           {
             MORE_ERROR() << "symbol is an existing variable in 1 ⎕TF F record";
             return Value_P();
           }

        if (data_chars != shape.get_volume())
           {
             MORE_ERROR() << "item count mismatch in 1 ⎕TF N record";
             return Value_P();
           }

        Value_P new_val(shape, LOC);
        loop(d, data_chars)   new_val->next_ravel_Char(ravel[idx + d]);
        new_val->check_value(LOC);

        // ⎕FX returns the function name if it was established, or else the
        // (integer) row number of the offending line. LRM p. 339: Z is ''
        // if the function was not established. Bugs37 #21.
        //
        const Token t = Quad_FX::do_eval_B(*new_val);
        if (t.get_Class() != TC_VALUE ||
            !t.get_apl_val()->is_char_string())
           {
             return Str0(LOC);
           }
      }
   else if (mode == UNI_C)   // char array
      {
        if (data_chars != shape.get_volume())
           {
             MORE_ERROR() << "item count mismatch in 1 ⎕TF C record";
             return Value_P();
           }
        if (nc != NC_UNUSED_USER_NAME && nc != NC_VARIABLE)
           {
             MORE_ERROR() <<
             "symbol is existing and not a variable in 1 ⎕TF C record";
             return Value_P();
           }

        Value_P new_val(shape, LOC);
        loop(d, data_chars)   new_val->next_ravel_Char(ravel[idx + d]);
        // Bugs9 #7 (Blake McBride): with data_chars == 0 (empty char array)
        // the loop body above never runs, so new_val never gets a character
        // cell and silently keeps the default numeric prototype.
        if (data_chars == 0)   new_val->set_proto_Spc();

        new_val->check_value(LOC);

        if (symbol == 0)   symbol = Workspace::lookup_symbol(name);
        const_cast<Symbol *>(symbol)->assign(new_val, false, LOC);
      }
   else if (mode == UNI_N)   // numeric array
      {
        if (nc != NC_UNUSED_USER_NAME && nc != NC_VARIABLE)
           {
             MORE_ERROR() << "symbol cannot be assigned in 1 ⎕TF N record";
             return Value_P();
           }

        UCS_string data(ravel, idx, len - idx);
        Tokenizer tokenizer(PM_EXECUTE, LOC, false);
        Token_string tos;
        try   { tokenizer.tokenize(data, tos); }
        catch (const Error &)
           {
             MORE_ERROR() << "tokenization failed in 1 ⎕TF N record";
             return Value_P();
           }
        if (size_t(tos.size()) != size_t(shape.get_volume()))
           {
             MORE_ERROR() << "item count mismatch in 1 ⎕TF N record";
             return Value_P();
           }

        // check that all tokens are numeric...
        //
        loop(t, tos.size())
           {
             const TokenTag tag = tos[t].get_tag();
             if (tag == TOK_INTEGER)   continue;
             if (tag == TOK_REAL)      continue;
             if (tag == TOK_COMPLEX)   continue;
             MORE_ERROR() <<  "Non-number in 1 ⎕TF N record";
             return Value_P();
           }

        // at this point, we have a valid inverse 1 ⎕TF.
        //
        Value_P ZZ(shape, LOC);

        loop(t, tos.size())
           {
             const Token & tok = tos[t];
             const TokenTag tag = tok.get_tag();
             if (tag == TOK_INTEGER)
                ZZ->next_ravel_Int(tok.get_int_val());
             else if (tag == TOK_REAL)
                ZZ->next_ravel_Float(tok.get_flt_val());
             else if (tag == TOK_COMPLEX)
                ZZ->next_ravel_Complex(tok.get_cpx_real(),
                                            tok.get_cpx_imag());
             else Assert(0);   // since checked above
           }

        ZZ->check_value(LOC);

        if (!symbol)   symbol = Workspace::lookup_symbol(name);

        const_cast<Symbol *>(symbol)->assign(ZZ, false, LOC);
      }
   else Assert(0);   // since checked above

   return Value_P(name, LOC);
}
//════════════════════════════════════════════════════════════════════════════
void
Quad_TF::tf2_reduce(Token_string & tos)
{
   tf2_reduce_UCS(tos);
   for (bool progress = true; progress;)
       {
         progress = false;

         Log(LOG_Quad_TF)   tos.print(CERR, true);

         if ((progress = tf2_reduce_RHO(tos)))               continue;
         if ((progress = tf2_reduce_CR38(tos)))              continue;
         if ((progress = tf2_reduce_COMMA(tos)))             continue;
         if ((progress = tf2_reduce_ENCLOSE_ENCLOSE(tos)))   continue;
         if ((progress = tf2_reduce_ENCLOSE(tos)))           continue;
         if ((progress =  tf2_reduce_ENCLOSE1(tos)))         continue;
         if ((progress = tf2_reduce_sequence(tos)))          continue;
         if ((progress = tf2_reduce_sequence1(tos)))         continue;
         if ((progress = tf2_reduce_parentheses(tos)))       continue;
         if ((progress = tf2_glue(tos)))                     continue;
       }
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_ENCLOSE(Token_string & tos)
{
ShapeItem skipped = 0;

   // replace  ⊂ B ) by an enclosed B )
   //
   loop(s, tos.size())
      {
        if ((s + 2) >= ShapeItem(tos.size())            ||
            tos[s    ].get_tag()   != TOK_F12_PARTITION ||   // not ⊂
            tos[s + 1].get_Class() != TC_VALUE          ||   // not B
            tos[s + 2].get_tag()   == TOK_F12_RHO)           // followed by ⍴
           {
             if (skipped)   // dont copy to itself
                tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        Value_P B = tos[s + 1].get_apl_val();
        if (B->is_simple_scalar())
           {
             tos[s - skipped].move_from(tos[s + 1], LOC);
           }
        else
           {
             Value_P enc_B(LOC);
             enc_B->next_ravel_Pointer(B.get());
             enc_B->check_value(LOC);
             Token tok(TOK_APL_VALUE1, enc_B);
             tos[s - skipped].copy(Token_loc(tok, Function_PC_invalid), LOC);
             tos[s + 1].clear(LOC);   // B
           }
        s += 1;   skipped += 1;
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_ENCLOSE() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_RHO(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        // we replace (A⍴B) by B reshaped to A.
        //
        if ((s + 3) >= ShapeItem(tos.size())        ||   // too short
            tos[s    ].get_Class() != TC_VALUE      ||   // not A
            tos[s + 1].get_tag()   != TOK_F12_RHO   ||   // not ⍴
            tos[s + 2].get_Class() != TC_VALUE      ||   // not B 
            tos[s + 3].get_tag()   != TOK_R_PARENT)      // not )
           {
             // no match: copy the token (but never to itself)
             //
             if (skipped)   tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        Shape sh;
        {
          const Value & aval = *tos[s].get_apl_val();
          sh = Shape(aval, /* ⎕IO */ 0);
          tos[s].release_apl_val(LOC);
        }
        s += 2;     skipped += 2;    // skip ( A

        Value_P bval = tos[s].get_apl_val();
        if (sh.get_volume() == bval->element_count())   // same volume
           {
             tos[s - skipped].move_from(tos[s], LOC);
             tos[s - skipped].ChangeTag(TOK_APL_VALUE1);
             bval->set_shape(sh);
           }
        else
           {
             Token t = Bif_F12_RHO::do_reshape(sh, *bval);
             tos[s - skipped].copy(Token_loc(t, Function_PC_invalid), LOC);
           }
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_RHO() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_CR38(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        // we replace (38 ⎕CR B) by Quad_CR::do_CR38(B), i.e. actually
        // apply the "plain array -> structured value" conversion, rather
        // than just assigning the plain matrix -- this is what restores
        // is_member() on reconstruction for a structured value (or, when
        // this pattern occurs nested inside a larger matrix literal, a
        // nested structured sub-value) encoded by tf2_var()/tf2_ravel().
        //
        if ((s + 3) >= ShapeItem(tos.size())          ||   // too short
            tos[s    ].get_Class() != TC_VALUE        ||   // not 38
            tos[s + 1].get_Class() != TC_FUN12        ||   // not ⎕CR
            tos[s + 1].get_function() != &Quad_CR::fun ||   // not ⎕CR
            tos[s + 2].get_Class() != TC_VALUE        ||   // not B
            tos[s + 3].get_tag()   != TOK_R_PARENT    ||   // not )
            tos[s].get_apl_val()->get_rank() != 0     ||
            tos[s].get_apl_val()->get_int_value(0) != 38)
           {
             // no match: copy the token (but never to itself)
             //
             if (skipped)   tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        Value_P bval = tos[s + 2].get_apl_val();
        Value_P zval = Quad_CR::do_CR38(*bval);

        tos[s].release_apl_val(LOC);
        s += 2;     skipped += 2;    // skip ( 38 ⎕CR

        Token t(TOK_APL_VALUE1, zval);
        tos[s - skipped].copy(Token_loc(t, Function_PC_invalid), LOC);
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_CR38() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_sequence(Token_string & tos)
{
ShapeItem skipped = 0;

//   tos.print(CERR);

   // replace N - ⎕IO - ⍳ K  by  N N+1 ... N+K-1
   //         0 1 2   3 4 5
   //
   loop(s, tos.size())
      {
        if ((s + 5) >= ShapeItem(tos.size())           ||   // too short
            tos[s    ].get_Class() != TC_VALUE         ||   // not N
            tos[s + 1].get_tag()   != TOK_F12_MINUS    ||   // not -
            tos[s + 2].get_tag()   != TOK_Quad_IO      ||   // not ⎕IO
            tos[s + 3].get_tag()   != TOK_F12_MINUS    ||   // not -
            tos[s + 4].get_tag()   != TOK_F12_INDEX_OF ||   // not ⍳
            tos[s + 5].get_Class() != TC_VALUE         ||   // not K
           !tos[s    ].get_apl_val()->is_int_scalar()  ||   // N not integer
           !tos[s + 5].get_apl_val()->is_int_scalar())      // K not integer
           {
             if (skipped)   // dont copy to itself
                tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        Cell cache_N, cache_K;
        const APL_Integer N = tos[s].get_apl_val()->get_cfirst(cache_N)
                                    .get_int_value();
        const APL_Integer K = tos[s + 5].get_apl_val()->get_cfirst(cache_K)
                                        .get_int_value();

        loop(j, 6)   tos[s + j].clear(LOC);

        Value_P sequence(K, LOC);
        loop(k, K)   sequence->next_ravel_Int(N + k);
        sequence->check_value(LOC);
        Token tok(TOK_APL_VALUE1, sequence);
        tos[s - skipped].copy(Token_loc(tok, Function_PC_invalid), LOC);
        s += 5;   skipped += 5;
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_sequence() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_sequence1(Token_string & tos)
{
ShapeItem skipped = 0;

//   tos.print(CERR);

   // replace N - M × ⎕IO - ⍳ K by M(N N+1 ... N+K-1)
   //         0 1 2 3 4   5 6 7
   //
   loop(s, tos.size())
      {
        if ((s + 7) >= ShapeItem(tos.size())           ||   // too short
            tos[s    ].get_Class() != TC_VALUE         ||   // not N
            tos[s + 1].get_tag()   != TOK_F12_MINUS    ||   // not -
            tos[s + 2].get_Class() != TC_VALUE         ||   // not K
            tos[s + 3].get_tag()   != TOK_F12_TIMES    ||   // not ×
            tos[s + 4].get_tag()   != TOK_Quad_IO      ||   // not ⎕IO
            tos[s + 5].get_tag()   != TOK_F12_MINUS    ||   // not -
            tos[s + 6].get_tag()   != TOK_F12_INDEX_OF ||   // not ⍳
            tos[s + 7].get_Class() != TC_VALUE         ||   // not K
           !tos[s    ].get_apl_val()->is_int_scalar()  ||   // N not integer
           !tos[s + 2].get_apl_val()->is_int_scalar()  ||   // M not integer
           !tos[s + 7].get_apl_val()->is_int_scalar())      // K not integer
           {
             if (skipped)   // dont copy to itself
                tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        Cell cache_N, cache_M, cache_K;
        const APL_Integer N = tos[s].get_apl_val()->get_cfirst(cache_N)
                                    .get_int_value();
        const APL_Integer M = tos[s + 2].get_apl_val()->get_cfirst(cache_M)
                                        .get_int_value();
        const APL_Integer K = tos[s + 7].get_apl_val()->get_cfirst(cache_K)
                                        .get_int_value();

        loop(j, 8)   tos[s + j].clear(LOC);

        Value_P sequence(K, LOC);
        loop(k, K)   sequence->next_ravel_Int(M * (N + k));
        sequence->check_value(LOC);
        Token tok(TOK_APL_VALUE1, sequence);
        tos[s - skipped].copy(Token_loc(tok, Function_PC_invalid), LOC);
        s += 7;   skipped += 7;
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_sequence1() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }
   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_parentheses(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        // we replace ( B ) by B 
        //
        if ((s + 2) < ShapeItem(tos.size())        &&
            tos[s].get_tag()       == TOK_L_PARENT &&
            tos[s + 1].get_Class() == TC_VALUE     &&
            tos[s + 2].get_tag()   == TOK_R_PARENT)   // ( B )
           {
             // capture the marker before dest.move_from() below, which
             // (when skipped==0) overwrites tos[s] == dest in place
             const bool synthetic_outer = tos[s].get_int_val() != 0;
             Token_loc & dest = tos[s - skipped];   // '(' of: '(' B ')'
             dest.move_from(tos[s + 1], LOC);      // move B to '('
             if (dest.get_tag() == TOK_APL_VALUE3 && !synthetic_outer)
                {
                  // final result of strand expression: enclose it
                  //
                  Value_P B = dest.get_apl_val();
                  if (!B->is_simple_scalar())   // unless simple scalar
                     {
                       Value_P Z(LOC);
                       Z->next_ravel_Pointer(B.get());
                       Z->check_value(LOC);
                       dest.copy(Token_loc(Token(TOK_APL_VALUE3, Z),
                                           dest.get_PC()), LOC);
                     }
                }
             s += 2;   skipped += 2;
             continue;
           }

        if (skipped)   // dont copy to itself
           tos[s - skipped].move_from(tos[s], LOC);
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_parentheses() has skipped "
                << skipped << " token" << endl;
        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_glue(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())   // ⍴ must not be first or last
      {
        // the first value (if any) survives
        //
        if (skipped)   // dont copy to itself
           tos[s - skipped].move_from(tos[s], LOC);

        if (tos[s - skipped].get_Class() != TC_VALUE)   continue;

         // subsequent values (if any) are glued to the first value which
         // is now tos[s - skipped]
         //
         while ((s + 1) < ShapeItem(tos.size()) &&
                tos[s + 1].get_Class() == TC_VALUE)
            {
               Token_loc tmp;
               tmp.move_from(tos[s - skipped], LOC);
               Value_P Z = Value::glue(tmp, tos[s + 1], LOC);
               // Value::glue(tos[s - skipped], t, tos[s + 1], LOC);
               tos[s - skipped].copy(Token_loc(Token(TOK_APL_VALUE3, Z), Function_PC_invalid), LOC);
               s ++;   skipped ++;
            }
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_glue() has skipped " << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_COMMA(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        // we replace , B by B reshaped or A , B by AB
        //
        if (s < ShapeItem(tos.size() - 1)     &&
            tos[s].get_tag() == TOK_F12_COMMA &&   // ,
            tos[s + 1].get_Class() == TC_VALUE)    // B 
           {
             const ShapeItem d_1 = s - skipped - 1;
             if (s > 0 && tos[d_1].get_tag() == TOK_R_PARENT)   // value to be
                {
                  // TOK_R_PARENT will become a value: just copy it for now
                  //
                  if (skipped)   tos[s - skipped].move_from(tos[s], LOC);
                  continue;
                }

             if (s > 0 && tos[d_1].get_Class() == TC_VALUE)   // value
                {
                  Value_P A = tos[d_1].get_apl_val();
                  Value_P B = tos[s + 1].get_apl_val();
                  Assert(A->get_rank() <= 1);
                  Assert(B->get_rank() <= 1);
                  Value_P Z(A->element_count() + B->element_count(), LOC);
                  loop(a, A->element_count())
                     {
                       Cell cache;
                       Z->next_ravel_Cell(A->get_cravel(a, cache));
                     }
                  loop(b, B->element_count())
                     {
                       Cell cache;
                       Z->next_ravel_Cell(B->get_cravel(b, cache));
                     }
                  Z->check_value(LOC);

                  tos[s + 1].clear(LOC);
                  Token tok_AB(TOK_APL_VALUE1, Z);
                  tos[d_1].copy(Token_loc(tok_AB, Function_PC_invalid), LOC);
                  s += 1;   skipped += 2;   // skip , (of A , B) but not B
                  continue;   // don't move_from() below
                }

             // monadic , B
             s += 1;   skipped += 1;    // skip ,

             Value_P bval = tos[s].get_apl_val();
             const Shape sh(bval->element_count());
             bval->set_shape(sh);

             tos[s].ChangeTag(TOK_APL_VALUE1);
             tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        if (skipped)   tos[s - skipped].move_from(tos[s], LOC);
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_COMMA() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_TF::tf2_fun(const UCS_string & fun_name, const Function & fun)
{
UCS_string ucs;
   tf2_fun_ucs(ucs, fun_name, fun);

   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_ENCLOSE_ENCLOSE(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        if ((s + 2) >= ShapeItem(tos.size())            ||
            (s && tos[s - 1].get_Class() == TC_VALUE)   ||   // dyadic ⊂
            tos[s    ].get_tag()   != TOK_F12_PARTITION ||   // not ⊂
            tos[s + 1].get_tag()   != TOK_F12_PARTITION ||   // not ⊂
            tos[s + 2].get_Class() != TC_VALUE          ||   // not B 
            skipped)            // remove at most one ⊂ (may enable ⍴)
           {
             if (skipped)   // dont copy to itself
                tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        skipped++;   // ignore ⊂ at tos[s]
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_ENCLOSE_ENCLOSE() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }
   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_TF::tf2_reduce_ENCLOSE1(Token_string & tos)
{
ShapeItem skipped = 0;

   // replace  ⍴ ⊂ B  by ⍴ enclosed B
   //
   loop(s, tos.size())
      {
        if ((s + 2) >= ShapeItem(tos.size())            ||
            tos[s    ].get_tag()   != TOK_F12_RHO       ||   // not ⍴
            tos[s + 1].get_tag()   != TOK_F12_PARTITION ||   // not ⊂
            tos[s + 2].get_Class() != TC_VALUE)              // not B 
           {
             if (skipped)   // dont copy to itself
                tos[s - skipped].move_from(tos[s], LOC);
             continue;
           }

        if (skipped)   // dont copy to itself
           tos[s - skipped].move_from(tos[s], LOC);   // move ⍴

        Value_P B = tos[s + 2].get_apl_val();
        if (B->is_scalar())
           {
             tos[s - skipped + 1].move_from(tos[s + 2], LOC);
           }
        else
           {
             Value_P enc_B(LOC);
             enc_B->next_ravel_Pointer(B.get());
             Token tok(TOK_APL_VALUE1, enc_B);
             tos[s - skipped + 1].copy(Token_loc(tok, Function_PC_invalid), LOC);
             tos[s + 2].clear(LOC);   // B
           }
        s += 2;   ++skipped;
      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_ENCLOSE1() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

   return skipped > 0;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_TF::tf2_reduce_UCS(Token_string & tos)
{
ShapeItem skipped = 0;

   loop(s, tos.size())
      {
        if (s < ShapeItem(tos.size() - 1) && tos[s].get_tag() == TOK_Quad_UCS)
           {
             s += 1;     skipped += 1;    // skip ⎕UCS
             for (; s < ShapeItem(tos.size()) &&
                    tos[s].get_Class() == TC_VALUE; ++s)
                {
                  tf2_toggle_UCS(*tos[s].get_apl_val().get());
                  tos[s - skipped].move_from(tos[s], LOC);
                }
           }

        if (skipped)   tos[s - skipped].move_from(tos[s], LOC);

      }

   if (skipped)
      {
        Log(LOG_Quad_TF)
           CERR << "tf2_reduce_UCS() has skipped "
                << skipped << " token" << endl;

        tos.resize(tos.size() - skipped);
      }

}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
Quad_TF::tf2_toggle_UCS(Value & val)
{
ShapeItem error_count = 0;
const ShapeItem ec = val.nz_element_count();
const RavelType rt = val.get_ravel_type();

   if (rt & RPT_char)   // all character → convert to integer
      {
        loop(e, ec)   val.set_ravel_Int(e, val.get_char_value(e));
      }
   else if (rt & RPT_integer)   // all integer → convert to char
      {
        loop(e, ec)   val.set_ravel_Char(e, Unicode(val.get_int_value(e)));
      }
   else   // RPT_CELLS: may have pointers or mixed types
      {
        loop(e, ec)
           {
             if (val.is_character_cell(e))       // char → integer
                {
                  val.set_ravel_Int(e, val.get_char_value(e));
                }
             else if (val.is_integer_cell(e))   // integer → char
                {
                  val.set_ravel_Char(e, Unicode(val.get_int_value(e)));
                }
             else if (val.is_pointer_cell(e))   // nested
                {
                  error_count += tf2_toggle_UCS(*val.get_pointer_value(e));
                }
             else
                {
                  ++error_count;
                }
           }
      }

   if (val.is_empty())   val.to_type(false);   // unlikely

   return error_count;
}
//════════════════════════════════════════════════════════════════════════════
