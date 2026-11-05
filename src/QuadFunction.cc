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

#include <vector>

#include "Avec.hh"
#include "Bif_F12_FORMAT.hh"
#include "Bif_F1_EXECUTE.hh"
#include "CDR.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "LineInput.hh"
#include "Macro.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "PrintOperator.hh"
#include "QuadFunction.hh"
#include "UCS_string_vector.hh"
#include "Quad_CC.hh"
#include "Quad_FX.hh"
#include "Quad_FFT.hh"
#include "Quad_GTK.hh"
#include "Quad_JSON.hh"
#include "Quad_MAP.hh"
#include "Quad_MX.hh"
#include "Quad_PLOT.hh"
#include "Quad_PNG.hh"
#include "Quad_RE.hh"
#include "Quad_RVAL.hh"
#include "Quad_SQL.hh"
#include "Quad_TF.hh"
#include "Quad_XML.hh"
#include "StateIndicator.hh"
#include "Tokenizer.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

#include "Workspace.icc"

#ifndef environ
extern char **environ;
#endif

// ⎕-function instances
//
Quad_AF    Quad_AF   ::fun;
Quad_AT    Quad_AT   ::fun;
Quad_DL    Quad_DL   ::fun;
Quad_EA    Quad_EA   ::fun;
Quad_EB    Quad_EB   ::fun;
Quad_ENV   Quad_ENV  ::fun;
Quad_EX    Quad_EX   ::fun;
Quad_INP   Quad_INP  ::fun;
Quad_NA    Quad_NA   ::fun;
Quad_NC    Quad_NC   ::fun;
Quad_NL    Quad_NL   ::fun;
Quad_SI    Quad_SI   ::fun;
Quad_UCS   Quad_UCS  ::fun;
Quad_STOP  Quad_STOP ::fun;   // S∆
Quad_TRACE Quad_TRACE::fun;   // T∆

//════════════════════════════════════════════════════════════════════════════
Token
Quad_AF::eval_B(cValue_R B) const
{
const ShapeItem ec = B.element_count();
Value_P Z(B.get_shape(), LOC);

   const RavelType rt = B.get_ravel_type();
   if (rt & RPT_char)   // all character — Unicode to AV index
      {
        loop(v, ec)
           {
             const Unicode uni = B.get_char_value(v);
             int32_t pos = Avec::find_av_pos(uni);
             if (pos < 0)   Z->next_ravel_Int(Avec::MAX_AV);
             else           Z->next_ravel_Int(pos);
           }
      }
   else if (rt & RPT_integer)   // all integer — AV index to char
      {
        loop(v, ec)
           {
             const APL_Integer idx = B.get_near_int(v);
             // LRM p.268: "Integers in R must be nonnegative and less
             // than 2*31" (LanguageVariances.md #37) -- indexed_at()
             // itself silently maps anything outside ⎕AV's actual 256
             // entries to the same "not in ⎕AV" placeholder (◌) it
             // uses for legitimate mid-range gaps, so a negative or
             // ≥2*31 index needs to be caught here instead.
             if (idx < 0 || idx >= (APL_Integer(1) << 31))   DOMAIN_ERROR;
             Z->next_ravel_Char(Quad_AV::indexed_at(idx));
           }
      }
   else
      {
        loop(v, ec)
           {
             if (B.is_character_cell(v))   // Unicode to AV index
                {
                  const Unicode uni = B.get_char_value(v);
                  int32_t pos = Avec::find_av_pos(uni);
                  if (pos < 0)   Z->next_ravel_Int(Avec::MAX_AV);
                  else           Z->next_ravel_Int(pos);
                  continue;
                }
             if (B.is_integer_cell(v))
                {
                  const APL_Integer idx = B.get_near_int(v);
                  if (idx < 0 || idx >= (APL_Integer(1) << 31))   DOMAIN_ERROR;
                  Z->next_ravel_Char(Quad_AV::indexed_at(idx));
                  continue;
                }
             DOMAIN_ERROR;
           }
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_AT::eval_AB(cValue_R A, cValue_R B) const
{
   // A is an integer scalar 1, 2, 3, or 4 (the mode)
   // B is a matrix of symbol names

   if (A.get_rank() > 0)   RANK_ERROR;

const APL_Integer mode = A.get_near_int(0);
   if (mode < 1)   DOMAIN_ERROR;
   if (mode > 4)   DOMAIN_ERROR;

const ShapeItem cols = B.get_cols();
const ShapeItem rows = B.get_rows();
   if (rows == 0)   LENGTH_ERROR;

               // mode:  1  2  3  4
const int mode_vec[] = { 3, 7, 4, 2 };
const int mode_len = mode_vec[mode - 1];
Shape shape_Z(rows);
   shape_Z.add_shape_item(mode_len);

Value_P Z(shape_Z, LOC);

   loop(r, rows)
      {
        // get the symbol name by stripping trailing spaces
        //
        const ShapeItem b = r*cols;   // start of the symbol name
        UCS_string symbol_name;
        loop(c, cols)
           {
            const Unicode uni = B.get_char_value(b + c);
            if (uni == UNI_SPACE)   break;
            symbol_name << uni;
           }

        const NamedObject * obj = Workspace::lookup_existing_name(symbol_name);
        if (obj == 0)   Error::throw_symbol_error(symbol_name, LOC);

        if (const Function * function = obj->get_function())
           {
             // defined or system function.
             function->get_attributes(mode, *Z);
             continue;
           }

        if (const Symbol * symbol = obj->get_symbol())
           {
             // defined or sys var
             symbol->get_attributes(mode, *Z);
             continue;
           }

        // neither function nor variable (e.g. unused name)
        VALUE_ERROR;

#if 0 // not reached ??
        if (Avec::is_quad(symbol_name[0]))   // system function or variable
           {
             int l;
             const Token t(Workspace::get_quad(symbol_name, l));
             if (t.get_Class() == TC_SYMBOL)   // system variable
                {
                  // Assert() if symbol_name is not a function
                  t.get_sym_ptr();
                }
             else
                {
                  // throws SYNTAX_ERROR if t is not a function
                  t.get_function();
                }
           }
        else                                   // user defined
           {
             Symbol * symbol = Workspace::lookup_existing_symbol(symbol_name);
             if (symbol == 0)   VALUE_ERROR;

             symbol->get_attributes(mode, &Z->get_cravel(r*mode_len));
           }
#endif
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_DL::eval_B(cValue_R B) const
{
const APL_time_us start = now();

   // B should be an integer or real scalar
   //
   if (B.get_rank() > 0)                 RANK_ERROR;
   if (!B.is_real_cell(0))   DOMAIN_ERROR;

const APL_time_us end = start + 1000000 * B.get_real_value(0);
   if (end < start)                           DOMAIN_ERROR;
   if (end > start + 31*24*60*60*1000000LL)   DOMAIN_ERROR;   // > 1 month

bool need_LF = false;
   while (now() < end)
       {
         usleep(20000);
         if (InterruptContext::attention_is_raised())   need_LF = true;   // first ^C
         if (InterruptContext::interrupt_is_raised())                     // second ^C
            {
              need_LF = true;
              break;
            }
       }

   // interrupt or attention may have displayed ^C, start a new line if so.
   if (need_LF)   CERR << endl;

   // we do not clear_attention_raised(LOC) or clear_interrupt_raised(LOC);
   // so that the user can continue with →''

   // return the time elapsed.
   //
   return Token(TOK_APL_VALUE1, FloatScalar(0.000001*(now() - start), LOC));
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_EA::eval_AB(cValue_R A, cValue_R B) const
{
   if (!A.is_char_string())
      {
        if (A.get_rank() > 1)   RANK_ERROR;
        else                     DOMAIN_ERROR;
      }

   if (!B.is_char_string())
      {
        if (B.get_rank() > 1)   RANK_ERROR;
        else                     DOMAIN_ERROR;
      }

   return Macro::get_macro(Macro::MAC_Z__A_Quad_EA_B)->eval_AB(A, B);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_EB::eval_AB(cValue_R A, cValue_R B) const
{
   if (!A.is_char_string())
      {
        if (A.get_rank() > 1)   RANK_ERROR;
        else                     DOMAIN_ERROR;
      }

   if (!B.is_char_string())
      {
        if (B.get_rank() > 1)   RANK_ERROR;
        else                     DOMAIN_ERROR;
      }

   return Macro::get_macro(Macro::MAC_Z__A_Quad_EB_B)->eval_AB(A, B);
}
//════════════════════════════════════════════════════════════════════════════
/// append \b more_info (the raw text of Workspace::more_error(), which may
/// contain embedded UNI_LF line breaks, e.g. Quad_FIO.cc's multi-line
/// "Too few arguments..." messages) to \b pb as one \b pb row per actual
/// line (Bugs28 #100(e)): appending it as a single row instead left the
/// LFs embedded in that one (very wide) row, so GNU APL's own ⎕PW-based
/// output wrapping later cut it at column boundaries instead of the
/// author's intended line breaks -- garbling multi-line )MORE text into
/// a single mid-word-wrapped mess.
static void
append_more_error(PrintBuffer & pb, const UCS_string & more_info)
{
UCS_string_vector lines;
   more_info.to_vector(lines);
   loop(l, lines.size())   pb.append_ucs(lines[size_t(l)]);
}
//────────────────────────────────────────────────────────────────────────────
/// build the ⎕EC/⎕EA result Z for an SI-modifying command ()LOAD,
/// )QLOAD, )CLEAR, )RESET, )SIC) that Bif_F1_EXECUTE::execute_command()
/// refused to push (E_COMMAND_PUSHED) -- shared by Quad_EC::eoc() (a
/// pushed ⍎ hit one) and Quad_EC::eval_B() (Bugs28 #100(z): B is
/// directly a )command hitting one).
static Value_P
build_EC_command_pushed_refusal(const UCS_string & more_info)
{
UCS_string line_1((UTF8_string(Error::error_name(E_DOMAIN_ERROR))));
   if (more_info.size())   line_1 << UNI_PLUS;

PrintBuffer pb;
   pb.append_ucs(line_1);
UCS_string line_2;
   line_2 << "the SI-modifying command '"
          << Workspace::get_pushed_Command()
          << "' cannot be executed from within ⎕EC/⎕EA";
   pb.append_ucs(line_2);
   pb.append_ucs(UCS_string());
   if (more_info.size())   append_more_error(pb, more_info);

Value_P Z2(2, LOC);
    Z2->next_ravel_Int(Error::error_major(E_DOMAIN_ERROR));
    Z2->next_ravel_Int(Error::error_minor(E_DOMAIN_ERROR));
   Z2->check_value(LOC);

Value_P Z3(pb, LOC);

Value_P Z(3, LOC);
   Z->next_ravel_0();
   Z->next_ravel_Pointer(Z2.get());
   Z->next_ravel_Pointer(Z3.get());
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
/// return the )SI entry of the macro implementing A ⎕EB B if ⎕EC was called
/// by it (to execute B), or 0 if not
static StateIndicator *
EB_macro_of_EC(bool B_pushed)
{
   // If B_pushed, then ⎕EC runs with its own ⍎ frame for B (the safe-
   // execution start) on top of the SI and its parent is ⎕EC's caller
   // (Quad_EC::eoc()); otherwise ⎕EC's caller is on top (Quad_EC::eval_B()
   // before pushing B). Look no further: a B that itself calls ⎕EC has B's
   // ⍎ frame as that caller, and an older, suspended ⎕EB further down the
   // SI has nothing to do with this ⎕EC.
   //
StateIndicator * top = Workspace::SI_top();
   if (!top)   return 0;

StateIndicator * si = B_pushed ? top->get_parent() : top;
   if (!si)   return 0;

const UserFunction * ufun = si->get_executable()->get_exec_ufun();
   if (ufun && ufun == Macro::get_macro(Macro::MAC_Z__A_Quad_EB_B))   return si;
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
/// the )MORE text "A ⎕EB B: B failed with ..." for B's ⎕EM-like character
/// matrix EM (as returned by ⎕EC): row 1 the message, row 2 the statement
/// (or the reason for a refused command, then without carets in row 3),
/// and B's own )MORE info (if any) in the rows after row 3. Also returns
/// the message (without a trailing +) in msg.
static UCS_string
EB_B_failure_text(const cValue & EM, UCS_string & msg)
{
UCS_string_vector lines;
   if (EM.get_rank() == 2)
      {
        const ShapeItem rows = EM.get_rows();
        const ShapeItem cols = EM.get_cols();
        loop(r, rows)
           {
             UCS_string line;
             loop(c, cols)   line << EM.get_char_value(r*cols + c);
             line.map_pad();   // internal pad characters → blanks
             line.remove_trailing_whitespaces();
             lines.push_back(line);
           }
      }

   msg = lines.size() ? lines[0] : UCS_string();
   if (msg.size() && msg.back() == UNI_PLUS)   msg.pop_back();

UCS_string line_2 = lines.size() > 1 ? lines[1] : UCS_string();
   line_2.remove_leading_and_trailing_whitespaces();
const bool carets = lines.size() > 2 && lines[2].size();

UCS_string text;
   text << "A ⎕EB B: B failed with " << msg;
   if (line_2.size())   text << (carets ? " in: " : ": ") << line_2;
   for (size_t r = 3; r < lines.size(); ++r)   text << UNI_LF << lines[r];
   return text;
}
//────────────────────────────────────────────────────────────────────────────
/// if ⎕EC was called by the macro implementing A ⎕EB B, then remember the
/// failure of B (whose ⎕EM-like matrix is EM) in the )SI entry of the macro
static void
register_EB_B_failure(const cValue & EM, bool B_pushed)
{
   if (StateIndicator * eb = EB_macro_of_EC(B_pushed))
      {
        UCS_string msg;
        eb->set_EB_B_failure(EB_B_failure_text(EM, msg));
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_EC::eoc(Token & result)
{
   // set Token result to an APL value Z = (Z1 Z2 Z3) where:
   //
   // Z1 is an integer scalar 0-5 (return code):
   //
   //    0: Error
   //    1: Value
   //    2: Committed Value
   //    3: Void
   //    4: Branch
   //    5: Escape
   //
   // Z2 is a two-element integer vector with the value that ⎕ET would have,
   // ⎕ET is not set while in ⎕EC.
   //
   // Z3 is a value that depends on Z1:
   //
   // 0:      ⎕EM
   // 1 or 2: the value
   // 3 or 5: 0⍴0
   // 4:      the branch line

Value_P Z(3, LOC);
   if (result.get_tag() == TOK_ERROR)
      {
        // Capture any )MORE info produced while evaluating the ⎕EC
        // argument BEFORE clearing it below. Previously this detail was
        // simply discarded: a caller using ⎕EC to catch an error had no
        // way to retrieve the same )MORE text an interactive session
        // would show, since more_error() is unconditionally cleared here
        // so that ⎕EC-internal errors don't leak into an outer session's
        // stale )MORE state. Now it is folded into Z3 as an extra line
        // (added only when non-empty, so existing 3-line "like ⎕EM"
        // consumers of Z3 are unaffected), matching the interactive
        // convention of a trailing '+' on the primary error line when
        // )MORE has content.
        //
        const UCS_string more_info = Workspace::more_error();
        Workspace::more_error().clear();
        StateIndicator * si = Workspace::SI_top();
        si->clear_safe_execution();

        const ErrorCode ec = ErrorCode(result.get_int_val());

        // Bugs28 #95: E_COMMAND_PUSHED is Bif_F1_EXECUTE.cc's internal
        // signal that ⍎ hit a )LOAD/)QLOAD/)CLEAR/)RESET/)SIC, meant to
        // unwind all the way to Command::do_APL_expression() so THAT
        // command can actually run there -- but ⎕EC/⎕EA's safe
        // execution isolates the caller from exactly that kind of
        // disruption (a )CLEAR from inside ⎕EC would destroy the very
        // workspace ⎕EC is running in), so the pushed command can
        // never actually reach Command.cc from here and was instead
        // silently dropped, while still being reported as if it were a
        // real (if oddly-worded) error. Report it as a real, honestly-
        // worded refusal instead -- Error::update_error_info() no
        // longer stores E_COMMAND_PUSHED into the SI at all (see its
        // own Bugs28 #95 fix), so err/line_1/2/3 below cannot be used
        // for this code; deal with it up front instead.
        //
        if (ec == E_COMMAND_PUSHED)
           {
             Value_P Z_refused = build_EC_command_pushed_refusal(more_info);
             register_EB_B_failure(*Z_refused->get_pointer_value(2), true);
             Token tok_Z(TOK_APL_VALUE1, Z_refused);
             result.move_from(tok_Z, LOC);
             return;
           }

        const Error & err = StateIndicator::get_error(si);

        // err.get_error_line_1() carries the ACTUAL message (e.g. a
        // ⎕ES'd string, or A's override text for A ⎕ES B) rather than
        // just the generic name for ec -- reliable now that Command.cc's
        // safe-execution SI unwind propagates the innermost frame's
        // error up to si before eoc() ever runs (Bugs27 #57). Still
        // gated on the error codes matching as a defensive fallback, in
        // case some path reaches here without going through that unwind.
        //
        UCS_string line_1 = (err.get_error_code() == ec)
                           ? err.get_error_line_1()
                           : UCS_string(UTF8_string(Error::error_name(ec)));

        // Bugs28 #100(e): line_1, when taken from err (the common case),
        // may already carry a trailing '+' of its own -- Error's ctor
        // (Error.cc) adds one at throw time whenever )MORE info exists
        // then, which is exactly whenever more_info here is non-empty.
        // Appending a second one unconditionally gave e.g. "DOMAIN
        // ERROR++".
        //
        if (more_info.size() &&
            (line_1.size() == 0 || line_1.back() != UNI_PLUS))
           line_1 << UNI_PLUS;

        PrintBuffer pb;
        pb.append_ucs(line_1);
        pb.append_ucs(err.get_error_line_2());
        pb.append_ucs(err.get_error_line_3());
        if (more_info.size())   append_more_error(pb, more_info);

        Value_P Z2(2, LOC);
            Z2->next_ravel_Int(Error::error_major(ec));
            Z2->next_ravel_Int(Error::error_minor(ec));
        Z2->check_value(LOC);

        Value_P Z3(pb, LOC);   // 3 line message like ⎕EM

        Z->next_ravel_0();
        Z->next_ravel_Pointer(Z2.get());
        Z->next_ravel_Pointer(Z3.get());

        Z->check_value(LOC);
        register_EB_B_failure(*Z3, true);

        Token tok_Z(TOK_APL_VALUE1, Z);
        result.move_from(tok_Z, LOC);
        return;
      }

   // all other cases have Z2 = 0 0
   //
Value_P Z2(2, LOC);
   Z2->next_ravel_0();
   Z2->next_ravel_0();
   Z2->check_value(LOC);

   switch(result.get_tag())
      {
        case TOK_APL_VALUE1:
        case TOK_APL_VALUE3:
        case TOK_APL_VALUE4:
        case TOK_APL_VALUE5:
             Z->next_ravel_1();
             Z->next_ravel_Pointer(Z2.get());
             Z->next_ravel_Value(result.get_apl_val().get());
             break;

        case TOK_APL_VALUE2:
             Z->next_ravel_Int(2);
             Z->next_ravel_Pointer(Z2.get());
             Z->next_ravel_Value(result.get_apl_val().get());
             break;

        case TOK_NO_VALUE:
        case TOK_VOID:
             Z->next_ravel_Int(3);
             Z->next_ravel_Pointer(Z2.get());
             Z->next_ravel_Pointer(Idx0_0(LOC).get());
             break;

        case TOK_BRANCH_INT:
        case TOK_BRANCH_LAB:
             Z->next_ravel_Int(4);
             Z->next_ravel_Pointer(Z2.get());
             Z->next_ravel_Int(result.get_int_val());
             break;

        case TOK_ESCAPE:
             Z->next_ravel_Int(5);
             Z->next_ravel_Pointer(Z2.get());
             Z->next_ravel_Pointer(Idx0_0(LOC).get());
             break;

        default: CERR << "unexpected result tag " << result.get_tag()
                      << " in Quad_EC::eoc()" << endl;
                 Assert(0);
      }

   Z->check_value(LOC);
Token tok_Z(TOK_APL_VALUE1, Z);
   result.move_from(tok_Z, LOC);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_EC::eval_B(cValue_R B) const
{
const UCS_string statement_B(B);

   // Mark the CURRENT frame as safe execution before even attempting to
   // parse B, and unconditionally restore it right after (regardless of
   // outcome), rather than only marking a NEW frame after a successful
   // ExecuteList::fix() below. Error::update_error_info() (called from
   // inside throw_apl_error(), i.e. before any catch below even runs)
   // only suppresses printing an error to the console when it finds a
   // safe-execution frame walking up from where the error occurred --
   // otherwise it prints immediately. A B that fails to even PARSE
   // (not just fails at runtime) throws from deep inside
   // ExecuteList::fix() itself, before this function had ever pushed a
   // new (and thus protected) frame for B's own execution -- so it use
   // to print straight to the console, bypassing the catch two lines
   // below entirely and violating ⎕EC/⎕EA/⎕EB's whole contract that a
   // failed B is caught and returned as a value, never surfaced
   // directly. A B that fails at RUNTIME instead (parses fine, then
   // errors while executing) was never affected: by then a real,
   // already-protected child frame exists (see the unchanged
   // set_safe_execution_depth() call on the pushed frame near the end
   // of this function). See Bugs27 #36.
   //
StateIndicator * const top = Workspace::SI_top();
const int top_depth_before = top->get_safe_execution_depth();
   top->set_safe_execution_depth();

   // Bugs28 #100(z): unlike ⍎, ⎕EC/⎕EA always ran B through
   // ExecuteList::fix() as ordinary APL code, even when B is a
   // )command -- ⎕EC ')CLEAR' mis-parsed ")CLEAR" and gave a bogus
   // "Unbalanced right parenthesis" instead of either running the
   // command (e.g. )VARS) or giving the same honest "cannot be
   // executed from within ⎕EC/⎕EA" refusal a )command reached via ⍎
   // already gets (Bugs28 #95). Bif_F1_EXECUTE::execute_statement()'s
   // own check (first char is ')' or ']') is mirrored here so the same
   // command text is recognised the same way for both ⍎ and ⎕EC/⎕EA.
   //
   {
     UCS_string trimmed(statement_B);
     trimmed.remove_leading_and_trailing_whitespaces();
     if (trimmed.size() &&
         (trimmed[0] == UNI_R_PARENT || trimmed[0] == UNI_R_BRACK))
        {
          Token cmd_result;
          try
             {
               // plain Token operator= is the compiler-generated
               // default (shallow, refcount-unaware) -- move_from()
               // from a properly copy-constructed temporary is the
               // refcount-safe way to capture a returned Token, the
               // same convention used everywhere else in this file
               // (e.g. result.move_from(tok_Z, LOC) in eoc() above).
               //
               Token tmp = Bif_F1_EXECUTE::execute_command(trimmed);
               cmd_result.move_from(tmp, LOC);
             }
          catch (Error & err)
             {
               top->restore_safe_execution_depth(top_depth_before);
               if (err.get_error_code() == E_COMMAND_PUSHED)
                  {
                    const UCS_string more_info = Workspace::more_error();
                    Workspace::more_error().clear();
                    Value_P Z_refused =
                            build_EC_command_pushed_refusal(more_info);
                    register_EB_B_failure(*Z_refused->get_pointer_value(2),
                                          false);
                    return Token(TOK_APL_VALUE1, Z_refused);
                  }

               const ErrorCode ec = err.get_error_code();
               PrintBuffer pb;
               pb.append_ucs(UTF8_string(Error::error_name(ec)));
               pb.append_ucs(err.get_error_line_2());
               pb.append_ucs(err.get_error_line_3());

               Value_P Z2(2, LOC);
                   Z2->next_ravel_Int(Error::error_major(ec));
                   Z2->next_ravel_Int(Error::error_minor(ec));
                   Z2->check_value(LOC);

               Value_P Z3(pb, LOC);
               Value_P Z(3, LOC);
               Z->next_ravel_0();
               Z->next_ravel_Pointer(Z2.get());
               Z->next_ravel_Pointer(Z3.get());
               Z->check_value(LOC);
               register_EB_B_failure(*Z3, false);
               return Token(TOK_APL_VALUE1, Z);
             }

          top->restore_safe_execution_depth(top_depth_before);

          if (cmd_result.get_tag() == TOK_SI_PUSHED)
             {
               // trimmed was a *user-defined* command, i.e. a real APL
               // function that itself suspended -- the new frame is
               // already pushed; just protect it like the ordinary
               // "fun was parsed" success path below does, and let it
               // run/suspend normally (its own eventual completion or
               // error reaches Quad_EC::eoc() as usual).
               //
               Workspace::SI_top()->set_safe_execution_depth();
               return cmd_result;
             }

          // otherwise execute_command() returns TOK_APL_VALUE1 (a
          // vector of output-line strings); wrap it exactly as eoc()
          // wraps an ordinary successful result (case 1).
          Value_P Z2(2, LOC);
              Z2->next_ravel_0();
              Z2->next_ravel_0();
              Z2->check_value(LOC);
          Value_P Z(3, LOC);
          Z->next_ravel_1();
          Z->next_ravel_Pointer(Z2.get());
          Z->next_ravel_Value(cmd_result.get_apl_val().get());
          Z->check_value(LOC);
          return Token(TOK_APL_VALUE1, Z);
        }
   }

ExecuteList * fun = 0;
Error fix_error(E_SYNTAX_ERROR, LOC);   // fallback if nothing better is caught
   try {
         fun = ExecuteList::fix(statement_B, LOC);
         if (fun == 0)   // fix() caught the real error and stashed it away
            {
              if (const Error * werr = Workspace::get_error())
                 fix_error = *werr;
            }
       }
     catch (Error & err)      { fix_error = err; }
     catch (std::bad_alloc &) { WS_FULL; }
     catch (...)              { FIXME; }

   // restore top's own depth to what it was before, either way, and
   // before doing anything else with it: the (unchanged) success path
   // below computes the newly pushed frame's OWN depth from top's, so
   // top must be back to its true value first, or that computation
   // would double-count the temporary bump above. Must be the exact
   // saved value, not clear_safe_execution()'s "reset to parent's
   // depth" -- top may itself already have been a safe-execution frame
   // (a nested ⎕EC), whose own protection clear_safe_execution() would
   // incorrectly strip here (Bugs28 #27).
   //
   top->restore_safe_execution_depth(top_depth_before);

   if (fun == 0)
      {
        // B could not be parsed/compiled. Report the actual error caught
        // above (rather than always hard-coding E_SYNTAX_ERROR, which
        // discards the real reason, e.g. WS_FULL) and use the same
        // 3-line ⎕EM-shaped matrix that Quad_EC::eoc() builds for
        // run-time errors below (a bare, unshaped character vector here
        // would make Z3 inconsistent in rank between fix-time and
        // run-time errors).
        //
        const ErrorCode ec = fix_error.get_error_code();

        PrintBuffer pb;
        pb.append_ucs(UTF8_string(Error::error_name(ec)));

        // the fix() error has no statement line: show B itself, like the
        // run-time errors do (Blake McBride, Bugs32 #24)
        //
        UCS_string line_2(fix_error.get_error_line_2());
        UCS_string line_3(fix_error.get_error_line_3());
        UCS_string blank_2(line_2);
        blank_2.map_pad();
        blank_2.remove_leading_and_trailing_whitespaces();
        if (blank_2.size() == 0)
           {
             line_2 = Workspace::get_prompt();
             line_2 << statement_B;
             line_3 = UCS_string(Workspace::get_prompt().size(), UNI_SPACE);
             line_3 << UNI_CIRCUMFLEX;
           }
        pb.append_ucs(line_2);
        pb.append_ucs(line_3);

        Value_P Z2(2, LOC);
            Z2->next_ravel_Int(Error::error_major(ec));
            Z2->next_ravel_Int(Error::error_minor(ec));
            Z2->check_value(LOC);

        Value_P Z3(pb, LOC);   // 3 line message like ⎕EM
        Value_P Z(3, LOC);
        Z->next_ravel_0();              // return code = error
        Z->next_ravel_Pointer(Z2.get());   // ⎕ET value
        Z->next_ravel_Pointer(Z3.get());   // ⎕EM
        register_EB_B_failure(*Z3, false);

        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   Assert(fun);

   Log(LOG_UserFunction__execute)   fun->print(CERR);

   Workspace::push_SI(fun, LOC);
   Workspace::SI_top()->set_safe_execution_depth();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_EC::eval_fill_B(cValue_R B) const
{
Value_P Z2(2, LOC);                             // Z2←0 0 0
   Z2->next_ravel_0();
   Z2->next_ravel_0();
   Z2->check_value(LOC);

Value_P Zsub(3, LOC);                           // Zsub ← 3 (⊂Z2) (⊂⍬)...
   Zsub->next_ravel_Int(3);                     // Zsub[1] ← 3
   Zsub->next_ravel_Pointer(Z2.get());          // Zsub[2] ← ⊂ 0 0 0
   Zsub->next_ravel_Pointer(Idx0(LOC).get());   // Zsub[3] ← ⊂⍬
   Zsub->check_value(LOC);

Value_P Z(ShapeItem(0), LOC);                   // Z ← 0 ⍴ ⊂Zsub
  new (&Z->get_wproto())   PointerCell(Zsub.get(), *Z);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_ENV::eval_B(cValue_R B) const
{
   if (!B.is_char_string())   DOMAIN_ERROR;

const ShapeItem ec_B = B.element_count();

std::vector<const char *> evars;

   for (char **e = environ; *e; ++e)
       {
         const char * env = *e;

         // check if env starts with B.
         //
         bool match = true;
         loop(b, ec_B)
            {
              if (B.get_char_value(b) != Unicode(env[b]))
                 {
                   match = false;
                   break;
                 }
            }

         if (match)   evars.push_back(env);
       }

const Shape sh_Z(evars.size(), 2);
Value_P Z(sh_Z, LOC);

   loop(e, evars.size())
      {
        const char * env = evars[e];
        UCS_string ucs;
        while (*env)
           {
             if (*env != '=')   ucs << Unicode(*env++);
             else               break;
           }
        ++env;   // skip '='

        Value_P varname(ucs, LOC);

        ucs.clear();
        while (*env)   ucs << Unicode(*env++);

        Value_P varval(ucs, LOC);

        Z->next_ravel_Pointer(varname.get());
        Z->next_ravel_Pointer(varval.get());
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_ES::eval_AB(cValue_R A, cValue_R B) const
{
   // The macro Z__A_Quad_EB_B (Macro.def) re-signals the error of B as
   // μ9 ⎕ES μ8, where μ9 is the ⎕EM-like character matrix that ⎕EC returned
   // for B: row 1 the message, row 2 the statement (or the reason for a
   // refused command), row 3 the carets, and B's own )MORE info (if any)
   // in the rows after them. Report B's own message (e.g. that of
   // ⎕ES 'MY SPECIAL ERROR' in B), and name B's failure in )MORE. Doing this
   // here, i.e. when the error is reported, rather than when B failed means
   // that nothing executed in between (the cleanup A) can lose it, and that
   // nothing is left over if A branches away instead (Blake McBride, Bugs32
   // #22, #23, #24, #32).
   //
   if (A.get_rank() == 2 &&
       Workspace::SI_top()->get_executable()->get_exec_ufun() ==
       Macro::get_macro(Macro::MAC_Z__A_Quad_EB_B))
      {
        UCS_string msg;
        const UCS_string text = EB_B_failure_text(A, msg);

        Error error(E_NO_ERROR, LOC);
        // the )MORE info and the + (which event_simulate() takes from A
        // as given) must be in place before event_simulate() reports it
        //
        MORE_ERROR() << text;
        msg << UNI_PLUS;
        const Token ret = event_simulate(&msg, CLONE(&B, LOC), error);
        if (error.get_error_code() == E_NO_ERROR)   return ret;

        throw error;
      }

const UCS_string ucs(A);
Error error(E_NO_ERROR, LOC);
const Token ret = event_simulate(&ucs, CLONE(&B, LOC), error);
   if (error.get_error_code() == E_NO_ERROR)   return ret;

   throw error;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_ES::eval_B(cValue_R B) const
{
Error error(E_NO_ERROR, LOC);
const Token ret = event_simulate(0, CLONE(&B, LOC), error);
   if (error.get_error_code() == E_NO_ERROR)   return ret;

   // note: unlike the (undocumented, presumably historic) short-circuit
   // that used to be here, a simulated error must always be thrown, even
   // inside a safe execution context (e.g. ⎕EC 'foo'). Error::update_error_info()
   // (called by event_simulate() above) already suppresses printing the
   // error while a safe execution is in progress; throwing is what
   // notifies the enclosing ⎕EC that an event occurred at all (it turns
   // the throw into the caught, structured (rc ⎕ET ⎕EM) result). Silently
   // swallowing it here instead makes ⎕ES B a no-op inside ⎕EC, i.e. the
   // simulated error is lost and execution wrongly continues.
   //
   throw error;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_ES::event_simulate(const UCS_string * A, Value_P B, Error & error)
{
   // B is empty: no action
   //
   if (B->element_count() == 0)   return Token();

const ErrorCode ec = get_error_code(B);
   if (ec == E_QUAD_ES_BRA)   return Token(TOK_QUAD_ES_BRA, CLONE_P(B, LOC));
   if (ec == E_QUAD_ES_COM)   return Token(TOK_QUAD_ES_COM, CLONE_P(B, LOC));
   if (ec == E_QUAD_ES_ERR)   return Token(TOK_QUAD_ES_ERR, CLONE_P(B, LOC));
   if (ec == E_QUAD_ES_ESC)   return Token(TOK_QUAD_ES_ESC, CLONE_P(B, LOC));

   // copy the throw location out before the placement-new below
   // reconstructs error in place -- reading error.get_throw_loc() as a
   // constructor argument for the object it is itself about to
   // reinitialize triggers a (believed false-positive, per Blake
   // McBride, Bugs25 minor observations) -Wmaybe-uninitialized on some
   // compilers.
   //
const char * const throw_loc = error.get_throw_loc();
   new (&error)   Error(ec, throw_loc);

   if (error.get_error_code() == E_NO_ERROR)   // B = 0 0: reset ⎕ET and ⎕EM.
      {
        Workspace::clear_error(LOC);
        return Token();
      }

   if (error.get_error_code() == E_ASSERTION_FAILED)   // B = 0 ASSERTION_FAILED
      {
        Assert(0 && "simulated ASSERTION_FAILED in ⎕ES");
      }

   // at this point we shall throw the error. Add some error details.
   //
   // set up error message 1
   //
   if (A)                                 // A ⎕ES B
      {
        UCS_string msg1_ucs(*A);
        UTF8_string msg1_utf(msg1_ucs);
        error.set_error_line_1(msg1_utf.c_str());
      }
   else if (error.get_error_code() == E_USER_DEFINED_ERROR &&
            B->is_char_string())   // ⎕ES with character B
      {
        // the numeric pair 0 1 (major/minor) also happens to equal
        // E_USER_DEFINED_ERROR's own code, so get_error_code() can reach
        // here with a purely numeric B too (Blake McBride, Bugs28 #71,
        // e.g. plain ⎕ES 0 1) -- B->is_char_string() tells those apart;
        // treating a numeric B as text here crashed into an internal
        // cell-type assertion, leaking its text into )MORE.
        //
        UCS_string msg1_ucs(*B.get());
        UTF8_string msg1_utf(msg1_ucs);
        error.set_error_line_1(msg1_utf.c_str());
      }
   else if (error.is_known())             //  ⎕ES B with known major/minor B
      {
        /* error_message_1 already OK */ ;
      }
   else                                   //  ⎕ES B with unknown major/minor B
      {
        // LRM p.284: "An event simulation is generated ... but no
        // message is reported" for an event code with no defined
        // ⎕ET text (LanguageVariances.md #28). ⎕ET/⎕EM[2],[3] (the
        // failed statement + caret) still get set normally below;
        // only the message line itself is suppressed.
        error.clear_error_line_1();
      }

   error.set_show_locked(true);

   Assert(Workspace::SI_top());

   // ⎕ES simulates the event "as though the function were primitive"
   // (lrm p.282), which is why the code below hunts for the nearest real
   // (user-visible) calling context to blame instead of the frame that
   // literally called ⎕ES. But GNU APL also implements several
   // primitives/operators (⍎¨, +.× with primitive operands, n-wise
   // reduce, ...) via hidden internal macros (UserFunctions with
   // is_macro() true, see Macro.def) that themselves call ⎕ES to signal
   // LRM-documented error conditions. Those macro frames are pure
   // implementation detail -- never meant to be user-visible -- so pop
   // any of them off the top of the SI stack first; this makes the
   // unchanged logic below see the nearest real user function or the
   // top-level statement, exactly as if the macro's own primitive-like
   // effect (not its internal APL source) had failed directly.
   //
bool macro_popped = false;
   while (const UserFunction * top_ufun =
          Workspace::SI_top()->get_executable()->get_exec_ufun())
      {
        if (!top_ufun->is_macro())   break;
        Workspace::pop_SI(LOC);
        macro_popped = true;
      }

   // If ⎕ES was executed by a macro, then the macro implements a primitive
   // (e.g. ⎕EB, F¨, ⍎¨) that was called from the statement now on top of
   // the SI. That statement did NOT execute ⎕ES itself, so the "as though
   // the function were primitive or locked" rule below must not be applied
   // to it (again): report the error like any other error of a primitive,
   // e.g. "T[1]  '5' ⎕EB '3÷0'" rather than "T" (as if T were locked).
   //
   if (macro_popped)
      {
        error.set_show_locked(false);
        error.update_error_info(Workspace::SI_top());
        return Token();
      }

   // lrm p.282: "When ⎕ES is executed from within a defined function or
   // operator and R is not empty, the event action is generated as though
   // the function were primitive or locked [...]. Suspension occurs at the
   // calling point, not within the defined operation."
   //
   // The defined function (the "issuer") is the nearest non-macro defined
   // function at or below the top of the SI, skipping ⍎ frames and the
   // macro frames of primitives (e.g. ⍎¨, ⎕EA's A) on the way: IBM APL2
   // treats a ⍎'⎕ES ...' in TEST as executed within TEST (displays the
   // calling statement TEST, and )SI shows only *). Remove the issuer and
   // everything above it, and report the error at its calling statement,
   // e.g. "W[1]  TEST" or (in immediate execution) "      TEST".
   //
   // Nothing is removed if a frame on the way is where ⎕EC's safe
   // execution started -- popping it would remove the very frame that
   // Command.cc's safe-execution unwind later looks at, silently defeating
   // ⎕EC/⎕EA for this error. ⎕ES without an enclosing defined function
   // (e.g. ⍎'⎕ES 5 4' in immediate execution) is reported where it is.
   //
UCS_string B_failure;   // of an A ⎕EB B on the way (if any)
   {
     const StateIndicator * issuer = Workspace::SI_top();
     while (issuer)
        {
          if (issuer->is_safe_execution_start())   { issuer = 0;   break; }

          // ⎕ES in immediate execution has no issuer: the frames below
          // belong to earlier (suspended) statements, whose suspension
          // ⎕ES removed while reporting at their statement (Bugs34 #20)
          //
          if (issuer->get_parse_mode() == PM_STATEMENT_LIST)
             { issuer = 0;   break; }

          const UserFunction * ufun = issuer->get_executable()->get_exec_ufun();
          if (ufun && !ufun->is_macro())   break;   // found
          issuer = issuer->get_parent();
        }

     if (issuer && issuer->get_parent())
        {
          // the frames popped below may contain an A ⎕EB B macro whose B
          // has failed (and ⎕ES is its cleanup A): keep that failure for
          // )MORE, which update_error_info() looks up in the (then
          // popped) frames (Bugs34 #34)
          //
          for (const StateIndicator * s = Workspace::SI_top();
               s && s != issuer; s = s->get_parent())
              {
                if (s->get_EB_B_failure().size())
                   {
                     B_failure = s->get_EB_B_failure();
                     break;
                   }
                if (s->is_safe_execution_start())   break;
              }

          while (Workspace::SI_top() != issuer)   Workspace::pop_SI(LOC);
          Workspace::pop_SI(LOC);   // the issuer itself
          error.set_show_locked(false);
        }
   }

   if (B_failure.size())   // see above (before update_error_info() copies
      {                     // error into the SI entry)
        UCS_string & more = MORE_ERROR();
        if (more.size())   more << UNI_LF;
        more << B_failure;
        error.add_MORE_indicator(true);
      }

   error.update_error_info(Workspace::SI_top());
   return Token();
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Quad_ES::get_error_code(Value_P B)
{
   // B shall be one of:                  → Error code
   //
   // 1. empty, or                        → No error
   // 2. a 2 element integer vector, or   → B[0]/N[1]
   // 3. a simple character vector.       → user defined
   //
   // Otherwise, throw an error

   // We extend the standard error codes by a markers E_QUAD_ES_* (10-13) which
   // are only used by the Macros Z__A_Quad_EA_B and Z__A_Quad_EB_B in order to
   // return branches and the like as a result.

   if (B->get_rank() > 1)   RANK_ERROR;

   if (B->element_count() == 0)   return E_NO_ERROR;
   if (B->is_char_string())       return E_USER_DEFINED_ERROR;

   // every remaining case (an ordinary 2-element error code, or one of
   // the internal ⎕EA/⎕EB marker codes below) needs at least major and
   // minor, i.e. 2 elements. get_near_int() does not bounds-check the
   // ravel, so checking this first (rather than after computing err)
   // avoids reading past the end of B for e.g. ⎕ES 5 (a lone element).
   //
   if (B->element_count() < 2)   LENGTH_ERROR;

   // major (B[0]) and minor (B[1]) are packed into a single 32-bit
   // ErrorCode as (major<<16)|minor, so each must be a valid unsigned
   // 16-bit value -- unlike the case handled above (99 99, in range but
   // with no defined ⎕ET text, which LRM p.284 says is fine and reports
   // no message), a major or minor outside 0..65535 cannot even be
   // represented and silently wrapped/truncated instead (Blake McBride,
   // Bugs28 #71): e.g. ⎕ES 1E9 1E9 gave ⎕ET ¯1126 51712, and ⎕ES ¯5 ¯4
   // gave ⎕ET ¯1 65532.
   //
   {
     const APL_Integer major = B->get_near_int(0);
     const APL_Integer minor = B->get_near_int(1);
     if (major < 0 || major > 0xFFFF || minor < 0 || minor > 0xFFFF)
        {
          MORE_ERROR() << "⎕ES B: B[0] (major) and B[1] (minor) must each"
                          " be in range 0..65535; B[0] is " << major
                       << ", B[1] is " << minor;
          DOMAIN_ERROR;
        }
   }

const APL_Integer err = (B->get_near_int(0) << 16)
                      | (B->get_near_int(1));

   if ((err >> 16) == (E_QUAD_ES_BRA >> 16))   // one of the ⎕EA or ⎕EB events
      {
        const ShapeItem len_B = B->element_count();
        if (err == E_QUAD_ES_COM && len_B == 3)   return E_QUAD_ES_COM;
        // len_B==6 (Bugs28 #56): the ⎕EA macro's error payload gained
        // a 6th, enclosed element carrying B's real 3-line ⎕EM-shaped
        // message (Macro.def's Z__A_Quad_EA_B), alongside the original
        // 5 (100, $FFFF, ⊂A, major, minor).
        //
        if (err == E_QUAD_ES_ERR && len_B == 6)   return E_QUAD_ES_ERR;
        // len_B==3 is ⎕EB's BRA payload (100 $FFFD RES, no fallback);
        // len_B==4 is ⎕EA's (100 $FFFD (⊂,A) RES, A included so
        // handle_QUAD_ES_BRA() can fall back to it -- Macro.def).
        if (err == E_QUAD_ES_BRA && (len_B == 3 || len_B == 4))
           return E_QUAD_ES_BRA;
        if (err == E_QUAD_ES_ESC && len_B == 2)   return E_QUAD_ES_ESC;
        DOMAIN_ERROR;
      }

   if (B->element_count() != 2)   LENGTH_ERROR;

   return ErrorCode(err);
}
//════════════════════════════════════════════════════════════════════════════
int
Quad_EX::expunge(const UCS_string & name)
{
   if (name.size() == 0)   return 0;

   if (!name.contains(UNI_FULLSTOP))   // unless member access
      {
        Symbol * symbol = Workspace::lookup_existing_symbol(name);
        // ISO/APL2: ⎕EX of a name that was never defined already
        // satisfies ⎕EX's own postcondition (the name is unbound and
        // available) -- so it is success (1), not failure (0). Only
        // for a genuine user name though: lookup_existing_symbol()
        // also returns 0 for a system name like ⎕NC (not resolved the
        // same way as a user symbol), but that name is never
        // "available" for (re)definition the way a plain user name
        // is, and testcases/Quad_EX.tc already pins ⎕EX '⎕NC' at 0
        // (failure). See Bugs27 #58.
        // Bugs35 #56: but an invalid name (e.g. 1x, like ⎕NC's ¯1) or ⍞
        // (a system variable, like ⎕) cannot be disassociated: 0.
        //
        if (symbol == 0)
           {
             if (Avec::is_quad(name[0]) || name[0] == UNI_QUOTE_Quad)
                return 0;
             if (!Avec::is_first_symbol_char(name[0]))   return 0;
             loop(n, name.size())
                 if (!Avec::is_symbol_char(name[n]))   return 0;
             return 1;
           }

        // Bugs37 #69: a label is a local constant (LRM p. 340) that
        // cannot be disassociated: 0, and the label stays.
        //
        if (symbol->get_NC() == NC_LABEL)   return 0;

        return symbol->expunge();
      }

   // expunge a member...
   //
int ret = 0;   // assume ⎕EX failure

   // build vector of member names in reverse order
   //
vector<const UCS_string *>members;
   {
     int dot = name.size();
     for (int from = dot - 1; from >= 0; --from)
         {
           if (name[from] != UNI_FULLSTOP)   continue;

           members.push_back(new UCS_string(name, from + 1, dot - from - 1));
           dot = from;
//         CERR << "MEMBER '" << *members.back() << "'" << endl;
         }
     members.push_back(new const UCS_string(name, 0, dot));
   }

//      CERR << "VAR '"     << *members.back() << "'" << endl;

Symbol * symbol = Workspace::lookup_existing_symbol(*members.back());
   if (symbol == 0)     goto cleanup;

   {
     Value_P toplevel_val = symbol->get_var_value();
     if (!toplevel_val)   goto cleanup;

     // Bugs28 #53: get_existing_member() throws VALUE_ERROR/DOMAIN_
     // ERROR/RANK_ERROR/LENGTH_ERROR for every "no such member"
     // condition (correct for an actual member *reference*, e.g. plain
     // S.zz, but wrong for ⎕EX/)ERASE, whose job is only to check
     // whether there is something there to remove) -- and a plain
     // try/catch around it is not enough, since constructing/throwing
     // an Error has an observable console side effect
     // (Error::update_error_info()) even when caught immediately,
     // which used to leak into the current statement's own output.
     // try_existing_member() mirrors the same traversal without ever
     // throwing. An undefined member of an otherwise-existing
     // structured variable simply has nothing to expunge -- unlike a
     // bare undefined top-level name (handled above, always success:
     // it already satisfies ⎕EX's own postcondition), so this is
     // failure (0), not success.
     //
     if (const Cell * ccell = toplevel_val->try_existing_member(members))
        {
          Cell * cell = const_cast<Cell *>(ccell);
          cell->release(LOC);   IntCell::z0(cell--);   // member value
          cell->release(LOC);   IntCell::z0(cell);     // member name
          ret = 1;   // ⎕EX success
        }
   }

cleanup:
   loop(m, members.size())   delete members[m];
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
/// return one name per logical entry of B, for ⎕EX/⎕NC's B (a list of
/// symbol names). Bugs28 #48: the traditional form (B a simple
/// character vector -- one name -- or a character matrix -- one name
/// per row, via UCS_string_vector(B,false)) does not cover the more
/// natural nested-vector-of-names form (e.g. ⎕EX 'X' 'Y'): each of
/// UCS_string_vector's own get_char_value() calls on a PointerCell
/// item there returned garbage (misread pointer bytes as a Unicode
/// code point), flattening the whole nested B into one bogus name --
/// var_count stayed 1 regardless of how many names B actually held.
/// Detect nesting (any top-level item is itself an enclosed value) and
/// handle it separately: one name per top-level item, each disclosed
/// via get_UCS_ravel(), matching ISO 13751's "one result per name".
static UCS_string_vector
quad_EX_NC_names(cValue_R B)
{
UCS_string_vector names;

bool nested = false;
   loop(b, B.element_count())
       {
         if (B.try_pointer_value(b))   { nested = true;   break; }
       }

   if (!nested)   return UCS_string_vector(B, false);

   loop(b, B.element_count())
       {
         if (Value_P item = B.try_pointer_value(b))
            {
              // fully disclose, not just one level: e.g. ⎕EX ⊂⊂'ABCDE'
              // nests twice (unlike ⊂⊂'X' for a single-CHARACTER 'X'
              // -- ⊂ of an already-simple scalar is idempotent, but
              // 'ABCDE' is a vector, so its first ⊂ genuinely nests
              // it, and the second ⊂ nests the now-nested scalar
              // AGAIN) -- keep disclosing single-item pointer scalars
              // until reaching the actual character content.
              //
              while (item->is_scalar())
                  {
                    Value_P inner = item->try_pointer_value(0);
                    if (!inner)   break;
                    item = inner;
                  }
              names.push_back(item->get_UCS_ravel());
            }
         else if (B.is_character_cell(b))
            names.push_back(UCS_string(1, B.get_char_value(b)));
         else
            names.push_back(UCS_string());   // invalid: empty name
       }

   return names;
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_EX::eval_B(cValue_R B) const
{
   if (B.get_rank() > 2)   RANK_ERROR;

bool nested = false;
   loop(b, B.element_count())
       if (B.try_pointer_value(b))   { nested = true;   break; }

   // the per-CELL character validation below assumes flat character
   // cells (the traditional B: a name, or a matrix of names) and does
   // not apply to a nested B (Bugs28 #48: e.g. ⎕EX 'X' 'Y', one
   // enclosed name per item) -- each disclosed name is validated
   // structurally by expunge() itself instead.
   //
   // we don't throw a DOMAIN ERROR if B is bad, but provide info for
   // the user if she asks for it with )MORE.
   if (!nested)   loop(b, B.element_count())
       {
         if (!B.is_character_cell(b))
            {
              MORE_ERROR() << "⎕EX B: non-character in list B "
                              "(of symbol names)";
              break;
            }

        const Unicode uni = B.get_char_value(b);
        if (!( Avec::is_symbol_char(uni) ||
               Avec::is_white(uni)       ||
               (uni == UNI_FULLSTOP)))
           {
              MORE_ERROR() << "⎕EX B: invalid character "
                           << uni << " in list B (of symbol names)";
              break;
           }
       }

const UCS_string_vector vars = quad_EX_NC_names(B);
const ShapeItem var_count = vars.size();

Shape sh_Z;
   // ISO 13751: ⍴Z ←→ ↑⍴B for a matrix B (Bugs28 #88) -- a matrix B
   // (one name per row) always needs a vector Z sized to its row
   // count, even when that count is 0 or 1 ("var_count > 1" alone left
   // a 1-row (1 1⍴'A') or 0-row (0 3⍴'A') matrix B with a scalar Z
   // instead, and a scalar Z whose single cell is never written -- the
   // loop below runs var_count times -- is an incomplete value). The
   // "var_count > 1" disjunct is still needed for B a nested vector of
   // several names (⎕EX 'X' 'Y'), where a plain (non-nested) B is a
   // single name and rightly keeps a scalar Z regardless of length.
   //
   if (B.get_rank() == 2 || var_count > 1)   sh_Z.add_shape_item(var_count);
Value_P Z(sh_Z, LOC);

   loop(z, var_count)
      {
        const UCS_string & var = vars[z];
        const int erased = expunge(var);

        Z->next_ravel_Int(erased);
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
UCS_string Quad_INP::esc1;
UCS_string Quad_INP::esc2;
UCS_string Quad_INP::end_marker;
UCS_string_vector Quad_INP::raw_lines;
UCS_string_vector Quad_INP::prefixes;
UCS_string_vector Quad_INP::escapes;
UCS_string_vector Quad_INP::suffixes;
bool Quad_INP::Quad_INP_running = false;

Token
Quad_INP::eval_AB(cValue_R A, cValue_R B) const
{
   if (Quad_INP_running)
      {
        MORE_ERROR() << "⎕INP called recursively";
        SYNTAX_ERROR;
      }

   // make sure that B is a non-empty string
   //
   if (B.get_rank() > 1)         RANK_ERROR;
   if (B.element_count() == 0)   LENGTH_ERROR;

   // temporary strings if get_esc() should fail
   //
UCS_string e1;
UCS_string e2;
   get_esc(CLONE(&A, LOC), e1, e2);

   Quad_INP_running = true;

   end_marker = B.get_UCS_ravel();
   esc1 = e1;
   esc2 = e2;

   prefixes.clear();
   escapes.clear();
   suffixes.clear();

   read_strings();    // read lines from file or stdin
   split_strings();   // split lines into prefixes, escapes, and suffixes

const ShapeItem line_count = raw_lines.size();
Value_P BB(line_count, LOC);
   loop(l, line_count)
       {
         if (escapes[l].size() == 0)   // prefix only
            {
              Value_P c1(prefixes[l], LOC);
              Value_P row(1, LOC);
              row->next_ravel_Pointer(c1.get());
              row->check_value(LOC);
              BB->next_ravel_Pointer(row.get());
            }
         else if (suffixes[l].size() == 0)   // prefix and escape
            {
              Value_P c1(prefixes[l], LOC);
              Value_P c2(escapes [l], LOC);
              Value_P row(2, LOC);
              row->next_ravel_Pointer(c1.get());
              row->next_ravel_Pointer(c2.get());
              row->check_value(LOC);
              BB->next_ravel_Pointer(row.get());
            }
         else                                // prefix, escape, and suffix
            {
              Value_P c1(prefixes[l], LOC);
              Value_P c2(escapes [l], LOC);
              Value_P c3(suffixes[l], LOC);
              Value_P row(3, LOC);
              row->next_ravel_Pointer(c1.get());
              row->next_ravel_Pointer(c2.get());
              row->next_ravel_Pointer(c3.get());
              row->check_value(LOC);
              BB->next_ravel_Pointer(row.get());
            }
       }
   BB->check_value(LOC);

Token ret = Macro::get_macro(Macro::MAC_Z__Quad_INP_B)->eval_B(*BB);
   Assert1(ret.get_tag() == TOK_SI_PUSHED);

// loop(l, line_count)   BB->release(l, LOC);

   Quad_INP_running = false;
   return Token(TOK_SI_PUSHED);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_INP::eval_B(cValue_R B) const
{
   if (Quad_INP_running)
      {
        MORE_ERROR() << "⎕INP called recursively";
        SYNTAX_ERROR;
      }

   // make sure that B is a non-empty string
   //
   if (B.get_rank() > 1)         RANK_ERROR;
   if (B.element_count() == 0)   LENGTH_ERROR;

   end_marker = B.get_UCS_ravel();

   Quad_INP_running = true;

   read_strings();    // read lines from file or stdin

Value_P Z(raw_lines.size(), LOC);
   loop(l, raw_lines.size())
       {
         Value_P ZZ(raw_lines[l], LOC);
         Z->next_ravel_Pointer(ZZ.get());
       }

   Quad_INP_running = false;
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_INP::eval_XB(cValue_R X, cValue_R B) const
{
   if (X.element_count() != 1)
      {
        if (X.get_rank() > 1)   RANK_ERROR;
        else                     LENGTH_ERROR;
      }

APL_Integer x = X.get_near_int(0);
   if (x == 0)   return eval_B(B);
   if (x > 1)    DOMAIN_ERROR;

   // B is the end of document marker for a 'HERE document', similar
   // to ENDCAT in cat << ENDCAT.
   //
   // make sure that B is a non-empty string and extract it.
   //
   if (B.get_rank() > 1)         RANK_ERROR;
   if (B.element_count() == 0)   LENGTH_ERROR;

UCS_string end_marker(B.get_UCS_ravel());

UCS_string_vector lines;
Parser parser(PM_EXECUTE, LOC, false);

   for (;;)
      {
         bool eof = false;
         UCS_string line;
         UCS_string prompt;
         InputMux::get_line(LIM_Quad_INP, prompt, line, eof,
                            LineHistory::quad_INP_history);
         if (eof)   break;
         const int end_pos = line.substr_pos(end_marker);
         if (end_pos != -1)  break;

         Token_string tos;
         ErrorCode ec = parser.parse(line, tos, true);
         if (ec != E_NO_ERROR)
            {
              throw_apl_error(ec, LOC);
            }

         if ((tos.size() & 1) == 0)   LENGTH_ERROR;

         // expect APL values at even positions and , at odd positions
         //
         loop(t, tos.size())
            {
              Token_loc & tok = tos[t];
              if (t & 1)   // , or ⍪
                 {
                   if (tok.get_tag() != TOK_F12_COMMA &&
                       tok.get_tag() != TOK_F12_COMMA1)   DOMAIN_ERROR;
                 }
              else         // value
                 {
                   if (tok.get_Class() != TC_VALUE)   DOMAIN_ERROR;
                 }

            }
         lines.push_back(line);
      }

Value_P Z(lines.size(), LOC);
   loop(z, lines.size())
      {
         Token_string tos;
         try   { parser.parse(lines[z], tos, true); }
         catch (const Error &)   { /* already validated above; ignore */ }
         const ShapeItem val_count = (tos.size() + 1)/2;
         Value_P ZZ(val_count, LOC);
         loop(v, val_count)
            {
              Value_P val = tos[2*v].get_apl_val();
              ZZ->next_ravel_Value(val.get());
            }

         ZZ->check_value(LOC);
         Z->next_ravel_Pointer(ZZ.get());
      }

   if (lines.size() == 0)   // empty result
      {
        Value_P ZZ(UCS_string(), LOC);
        Z->set_ravel_Pointer(0, ZZ.get());
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_INP::get_esc(Value_P A, UCS_string & esc1, UCS_string & esc2)
{

   // A is either one string (esc1 == esc2) or two (nested) strings
   // for esc1 and esc2 respectively.
   //
   if (A->compute_depth() == 2)   // two (nested) strings
      {
        if (A->get_rank() != 1)        RANK_ERROR;
        if (A->element_count() != 2)   LENGTH_ERROR;

        loop(e, 2)
           {
             Cell cache;
             const Cell & cell = A->get_cravel(e, cache);
             if (Value_P v = cell.try_pointer_value())   // char vector
                {
                  if (e)   esc2 = v->get_UCS_ravel();
                  else     esc1 = v->get_UCS_ravel();
                }
             else                          // char scalar
                {
                  if (e)   esc2 = cell.get_pointer_value()->get_UCS_ravel();
                  else     esc1 = cell.get_pointer_value()->get_UCS_ravel();
                }
           }

        if (esc1.size() == 0)   LENGTH_ERROR;
      }
   else                       // one string 
      {
        esc1 = A->get_UCS_ravel();
        if (esc1.size() == 0)   LENGTH_ERROR;
        esc2 = esc1;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_INP::read_strings()
{
   // read lines until an end-maker is detected
   //
   raw_lines.clear();
   for (;;)
      {
        bool eof = false;
        UCS_string prompt;
        UCS_string line;
        InputMux::get_line(LIM_Quad_INP, prompt, line, eof,
                                 LineHistory::quad_INP_history);

        const int end = line.substr_pos(end_marker);
        if (end != -1)   // end marker found
           {
             line.resize(end);
             if (line.size())   raw_lines.push_back(line);
             break;
           }

        if (eof && !line.size())   break;
        raw_lines.push_back(line);
        if (eof)   break;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_INP::split_strings()
{
UCS_string empty;
   loop(r, raw_lines.size())
      {
        UCS_string & line = raw_lines[r];
        const ShapeItem epos = line.substr_pos(esc1);
        if (esc1.size() == 0 || epos == -1)   // no escape in this line
           {
             prefixes.push_back(line);
             escapes.push_back(empty);
             suffixes.push_back(empty);
             continue;
           }

        // at this point line did contain an exec string
        //
        UCS_string pref = line;
        pref.resize(epos);
        prefixes.push_back(pref);

        line = line.drop(epos + esc1.size());   // skip prefix and esc1
        if (esc2.size())   // end defined
           {
             const ShapeItem eend = line.substr_pos(esc2);
             if (eend == -1)   // no exec end in this line
                {
                  escapes.push_back(line);
                  suffixes.push_back(empty);
                  continue;
                }
             else              // found an exec end in this line
                {
                  UCS_string exec = line;
                  exec.resize(eend);
                  escapes.push_back(exec);
                  line = line.drop(eend + esc2.size());   // skip exec and esc2
                  suffixes.push_back(line);
                  continue;
                }
           }
        else               // no end defined
           {
             escapes.push_back(line);
             suffixes.push_back(empty);
           }
      }

   Assert(prefixes.size() >= raw_lines.size());
   Assert(prefixes.size() == escapes.size());
   Assert(prefixes.size() == suffixes.size());
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_NC::eval_B(cValue_R B) const
{
   if (B.get_rank() > 2)   RANK_ERROR;

   // Bugs28 #48: see quad_EX_NC_names()'s comment -- ⎕NC of a nested
   // vector of names (e.g. ⎕NC 'X' 'Y') has the same bug as ⎕EX.
   //
const UCS_string_vector vars = quad_EX_NC_names(B);
const ShapeItem var_count = vars.size();

Shape sh_Z;
   // same fix, and for the same reason, as Quad_EX::eval_B() above
   // (Bugs28 #88): a matrix B always needs a vector Z sized to its row
   // count, even for 0 or 1 rows.
   //
   if (B.get_rank() == 2 || var_count > 1)   sh_Z.add_shape_item(var_count);
Value_P Z(sh_Z, LOC);

   loop(v, var_count)
       {
         const int nc = get_NC(vars[v]);
         if (nc == NC_INVALID)   Z->next_ravel_Int(-1);
         else                    Z->next_ravel_Int(nc & NC_case_mask);
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
/// the character for ⎕UCS B with an integer B in [¯128, #x7FFFFFFF]: B
/// itself, except that a negative B is a signed byte (e.g. from a ⎕FIO byte
/// vector) and stands for the byte B+256. A negative Unicode (e.g.
/// #xFFFFFFFF for ¯1) would neither fit into 31 bits nor be encodable in
/// UTF-8.
static inline Unicode
byte_or_Unicode(APL_Integer bint)
{
   return Unicode(bint < 0 ? bint + 256 : bint);
}
//────────────────────────────────────────────────────────────────────────────
APL_Integer
Quad_NC::get_NC(const UCS_string ucs)
{
   if (ucs.size() == 0)   return NC_INVALID;   // invalid name

   // Bugs28 #54: ⎕NC had no member (.) handling at all -- it looked
   // up the WHOLE string "S.a" as one symbol name (never found, since
   // '.' is not a valid symbol character), falling through to the
   // "user-defined name" path below and reporting NC_INVALID for
   // every member reference, existing or not. doc/apl.texi says the
   // usual rules for normal variables apply to members: an existing
   // member is a variable (2, matching ⎕NC 'S'), a nonexistent member
   // of an otherwise-existing structured variable is simply unused
   // (0, same as any other undefined name), and reuses
   // try_existing_member() (Bugs28 #53) to check without throwing.
   //
   if (ucs.contains(UNI_FULLSTOP))
      {
        vector<const UCS_string *>members;
        {
          int dot = ucs.size();
          for (int from = dot - 1; from >= 0; --from)
              {
                if (ucs[from] != UNI_FULLSTOP)   continue;
                members.push_back(new UCS_string(ucs, from + 1, dot - from - 1));
                dot = from;
              }
          members.push_back(new UCS_string(ucs, 0, dot));
        }

        int ret = 0;   // assume unused (or malformed base name)
        if (Symbol * base_sym =
                  Workspace::lookup_existing_symbol(*members.back()))
           {
             if (Value_P top = base_sym->get_var_value())
                {
                  if (top->try_existing_member(members))   ret = NC_VARIABLE;
                }
           }

        loop(m, members.size())   delete members[m];
        return ret;
      }

const Unicode uni = ucs[0];
   if (uni == UNI_QUOTE_Quad)   // ⍞
      {
        if (ucs.size() == 1)   return NC_SYSTEM_VAR;
        else                   return NC_INVALID;
      }

   if (uni == UNI_Quad_Quad && ucs.size() == 1)   return NC_SYSTEM_VAR;   // ⎕

   // system name ?
   {
     const Symbol * sys = 0;

     // ⍺, ⍶, ⍵, and ⍹ are local variables of lambdas and may change their
     // NC at runtime. We will therefore consult their symbol later on.
     //
     if      (uni == UNI_ALPHA)           sys = &Workspace::get_v_ALPHA();
     else if (uni == UNI_LAMBDA)          sys = &Workspace::get_v_LAMBDA();
     else if (uni == UNI_CHI)             sys = &Workspace::get_v_CHI();
     else if (uni == UNI_OMEGA)           sys = &Workspace::get_v_OMEGA();
     else if (uni == UNI_ALPHA_UNDERBAR)  sys = &Workspace::get_v_ALPHA_U();
     else if (uni == UNI_OMEGA_UNDERBAR)  sys = &Workspace::get_v_OMEGA_U();

     // the caller may ask for e.g. ⍺123 but we accept ⍺ and friends only if
     // their lengths is 1.
     //
     if (sys && ucs.size() != 1)   return NC_INVALID;

     if (Avec::is_quad(uni))   // distinguished name
        {
          int len = 0;   // set by Workspace::get_quad()
          const Token tok = Workspace::get_quad(ucs, len);

          if (tok.is_function())   // system function (⎕EA, ⎕FX, ...): these
             {                     // are QuadFunction objects, not Symbols,
                                    // so they never reach the sys/TC_SYMBOL
                                    // path below -- ISO 13751 11.5.2 and
                                    // NC_SYSTEM_FUN (NamedObject.hh) agree
                                    // this class is 6, not "invalid".
               // same prefix-tolerance issue as the TC_SYMBOL case below
               // (e.g. ⎕FFTxyz would otherwise match the valid ⎕FFT).
               //
               if (tok.get_function()->get_name().size() != ucs.size())
                  return NC_INVALID;
               return NC_SYSTEM_FUN;
             }

          if (tok.get_Class() != TC_SYMBOL)   return NC_INVALID;

          // NOTE: Workspace::get_quad() tolerates prefixes (e.g ⎕FFTxyz
          // would be accepted since ⎕FFT is valid. This is OK for toenization,
          // but not for ⎕NC. We therefore need to check the length as well.
          //
          sys = tok.get_sym_ptr();
          if (sys->get_name().size() != ucs.size())   return NC_INVALID;
        }
    
     if (sys)   // system variable (⎕xx, ⍺, ⍶, ⍵, ⍹, λ, or χ
        {
          const NameClass nc = sys->get_NC();
          if (nc == NC_UNUSED_USER_NAME)   return NC_SYSTEM_FUN;
          return nc + 3;
        }
     }

   // user-defined name
   //
const Symbol * user_sym = Workspace::lookup_existing_symbol(ucs);

   if (!user_sym)
      {
        // user_sym not found. Distinguish between invalid and unused names
        //
        if (!Avec::is_first_symbol_char(ucs[0]))   return NC_INVALID;
        loop (u, ucs.size())
           {
             if (!Avec::is_symbol_char(ucs[u]))   return NC_INVALID;
           }
        return 0;   // unused
      }

   switch(const NameClass nc = user_sym->get_NC())
      {
        case NC_INVALID:      return NC_INVALID;
        case NC_SYSTEM_VAR:   return NC_VARIABLE;
        default:              return nc & NC_case_mask;
      }
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_NL::do_quad_NL(Value_P A, Value_P B)
{
   if (A && !A->is_char_string())   DOMAIN_ERROR;
   if (B->get_rank() > 1)            RANK_ERROR;
   if (B->element_count() == 0)   // nothing requested
      {
        return Token(TOK_APL_VALUE1, Str0_0(LOC));
      }

UCS_string first_chars;
   if (A)   first_chars = UCS_string(*A);

   // 1. create a bitmap of ⎕NC values requested in B
   //
int requested_NCs = 0;
   {
     loop(b, B->element_count())
        {
          const APL_Integer bb = B->get_near_int(b);
          if (bb < 1)   DOMAIN_ERROR;
          if (bb > 6)   DOMAIN_ERROR;
          requested_NCs |= 1 << bb;
        }
   }

   // 2, build a name table, starting with user defined names
   //
UCS_string_vector names;
   {
     std::vector<const Symbol *> symbols = Workspace::get_all_symbols();

     loop(s, symbols.size())
        {
          const Symbol * symbol = symbols[s];
          if (symbol->is_erased())   continue;

          NameClass nc = symbol->get_NC();
          if (nc == NC_SYSTEM_VAR)   nc = NC_VARIABLE;

          // nc is a NameClass (one-hot selector in the high byte, ordinal
          // in the low byte -- NamedObject.hh), not a small ordinal;
          // shifting by it directly is UB (e.g. NC_VARIABLE == 2050) and
          // only happened to work on x86, which masks the shift count to
          // 5 bits, and because every NameClass is a multiple of 32
          // (Blake McBride, Bugs12.md #9). Mask down to the ordinal first.
          //
          if (!(requested_NCs & 1 << (nc & NC_case_mask)))   continue;

          if (first_chars.size())
             {
               const Unicode first_char = symbol->get_name()[0];
               if (!first_chars.contains(first_char))   continue;
             }

          names.push_back(symbol->get_name());
        }
   }

   // 3, append ⎕-vars and ⎕-functions to name table (unless prevented by A)
   //
const bool vars = requested_NCs & 1 << 5;
const bool funs = requested_NCs & 1 << 6;

   if (first_chars.size() == 0 ||                  // all
       first_chars.contains(UNI_Quad_Quad))   // ⎕-variables and -functions
      {
// not unbound names, e.g. ⍺ ⍵ λ outside a lambda, which ⎕NC reports as 6
// (Bugs35 #72). (No NameClass is 0, so the former != 0 test was void.)
//
#define ro_sv_def(x, _str, _txt)                                    \
   { const Symbol * symbol = &Workspace::get_v_ ## x();             \
     if (vars && symbol->get_NC() != NC_UNUSED_USER_NAME)           \
        names.push_back(symbol->get_name()); }

#define rw_sv_def(x, _str, _txt)                                    \
   { const Symbol * symbol = &Workspace::get_v_ ## x();             \
     if (vars && symbol->get_NC() != NC_UNUSED_USER_NAME)           \
        names.push_back(symbol->get_name()); }

#define sf_def(_x, str, _txt)                               \
   { if (funs)   names.push_back(UCS_ASCII_string("⎕" str)); }
#include "SystemVariable.def"
      }


   // 4. compute length of longest name
   //
ShapeItem longest = 0;
   loop(n, names.size())
      {
        if (longest < names[n].ssize())   longest = names[n].size();
      }

const Shape shZ(names.size(), longest);
Value_P Z(shZ, LOC);

   // 5. construct result. The number of symbols is small and ⎕NL is
   // (or should) not be a perfomance // critical function, so we can
   // use a simple (find smallest and print) // approach.
   //
   for (int count = names.size(); count; --count)
      {
        // find smallest.
        //
        uint32_t smallest = count - 1;
        loop(t, count - 1)
           {
             if (names[smallest].compare(names[t]) > 0)   smallest = t;
           }
        // copy name to result, padded with spaces.
        //
        loop(l, longest)
           {
             const UCS_string & ucs = names[smallest];
             Z->next_ravel_Char(l < ucs.ssize() ? ucs[l] : UNI_SPACE);
           }

        // remove smalles from table
        //
        names[smallest] = names[count - 1];
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_SI::eval_AB(cValue_R A, cValue_R B) const
{
   if (A.element_count() != 1)   // not scalar-like
      {
        if (A.get_rank() > 1)   RANK_ERROR;
        else                     LENGTH_ERROR;
      }
APL_Integer a = A.get_near_int(0);
const ShapeItem len = Workspace::SI_entry_count();
   if (a >= len)   DOMAIN_ERROR;
   if (a < -len)   DOMAIN_ERROR;
   if (a < 0)   a += len;   // negative a counts backwards from end

const StateIndicator * si = 0;
   for (si = Workspace::SI_top(); si; si = si->get_parent())
       {
         if (si->get_level() == a)   break;   // found
       }

   Assert(si);

   if (B.element_count() != 1)   // not scalar-like
      {
        if (B.get_rank() > 1)   RANK_ERROR;
        else                     LENGTH_ERROR;
      }

const Prefix_PC PC = si->get_PC() - 1;
const Executable * exec = si->get_executable();
const ParseMode pm = exec->get_parse_mode();
const UCS_string & fun_name = exec->get_name();
const Function_Line fun_line = exec->get_line(PC);

Value_P Z;

const APL_Integer b = B.get_near_int(0);
   switch(b)
      {
        case 1:  Z = Value_P(fun_name, LOC);
                 break;

        case 2:  Z = Value_P(LOC);
                Z->next_ravel_Int(fun_line);
                break;

        case 3:  {
                   UCS_string fun_and_line(fun_name);
                   fun_and_line << UNI_L_BRACK << fun_line << UNI_R_BRACK;
                   Z = Value_P(fun_and_line, LOC); 
                 }
                 break;

        case 4: if (StateIndicator::get_error(si).get_error_code())
                   {
                     const UCS_string text(UTF8_string(
                           StateIndicator::get_error(si).get_error_line_2()));
                     Z = Value_P(text, LOC);
                   }
                else
                    {
                      const UCS_string text = exec->statement_text(PC);
                      Z = Value_P(text, LOC);
                    }
                break;

        case 5: Z = Value_P(LOC);
                Z->next_ravel_Int(int(PC));
                break;

        case 6: Z = Value_P(LOC);
                Z->next_ravel_Int(pm);
                break;

        default: DOMAIN_ERROR;
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_SI::eval_B(cValue_R B) const
{
   if (B.element_count() != 1)   // not scalar-like
      {
        if (B.get_rank() > 1)   RANK_ERROR;
        else                     LENGTH_ERROR;
      }

const APL_Integer b = B.get_near_int(0);
const ShapeItem len = Workspace::SI_entry_count();

   if (b < 1)   DOMAIN_ERROR;
   if (b > 4)   DOMAIN_ERROR;

   // at this point we should not fail...
   //
Value_P Z(len, LOC);

   // collect the SI chain newest (SI_top()) to oldest, the same direction
   // as every other SI walk in this codebase, then process it in reverse
   // so that ⎕SI still moves from oldest SI entry towards SI_top()...
   //
std::vector<const StateIndicator *> sis;
   sis.reserve(len);
   for (const StateIndicator * si = Workspace::SI_top(); si;
        si = si->get_parent())
       sis.push_back(si);

   for (ShapeItem s = ShapeItem(sis.size()) - 1; s >= 0; --s)
       {
         const StateIndicator * si = sis[s];

         const Prefix_PC PC = si->get_PC() - 1;
         const Executable * exec = si->get_executable();
         const ParseMode pm = exec->get_parse_mode();
         const UCS_string & fun_name = exec->get_name();
         const Function_Line fun_line = exec->get_line(PC);

         switch (b)
           {
             case 1:  {
                        Value_P name(fun_name, LOC);
                        Z->next_ravel_Pointer(name.get());
                      }
                      break;

             case 2:  Z->next_ravel_Int(fun_line);
                      break;

             case 3:  {
                        UCS_string fun_and_line(fun_name);
                        fun_and_line << UNI_L_BRACK << fun_line << UNI_R_BRACK;
                        Value_P name_and_line(fun_and_line, LOC);
                        Z->next_ravel_Pointer(name_and_line.get());
                      }
                      break;

             case 4:  if (StateIndicator::get_error(si).get_error_code())
                         {
                           const Error & e = StateIndicator::get_error(si);
                           const UCS_string ucs = e.get_error_line_2();
                           Value_P ZZ(ucs, LOC);
                           Z->next_ravel_Pointer(ZZ.get());
                         }
                      else   // no error in context si
                         {
                           const UCS_string text = exec->statement_text(PC);
                           Value_P text_val(text, LOC);
                           Z->next_ravel_Pointer(text_val.get());
                         }
                      break;

             case 5:  Z->next_ravel_Int(int(PC));   break;
             case 6:  Z->next_ravel_Int(pm);   break;
           }
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_UCS::eval_B(cValue_R B) const
{
Value_P Z(B.get_shape(), LOC);
const ShapeItem ec = B.element_count();

   if (ec == 0)   // prototype
      {
        if (B.is_character_cell(0))   // char to integer Unicode
           Z->set_proto_Int();
        else
           Z->set_proto_Spc();
      }

   const RavelType rt = B.get_ravel_type();
   if (rt & RPT_char)   // all character — char to Unicode integer
      {
        loop(v, ec)   Z->next_ravel_Int(B.get_char_value(v));
      }
   else if (rt & RPT_integer)   // all integer — integer to char
      {
        loop(v, ec)
           {
             const APL_Integer bint = B.get_near_int(v);
             if (bint < -0x80)        DOMAIN_ERROR;
             if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
             Z->next_ravel_Char(byte_or_Unicode(bint));
           }
      }
   else if (rt == RPT_FLOAT64)   // all float — near-int to char
      {
        loop(v, ec)
           {
             const APL_Integer bint = B.get_near_int(v);
             if (bint < -0x80)        DOMAIN_ERROR;
             if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
             Z->next_ravel_Char(byte_or_Unicode(bint));
           }
      }
   else if (rt == RPT_COMPLEX)   // all complex — check imag, near-int to char
      {
        loop(v, ec)
           {
             if (!Cell::is_near_zero(B.get_imag_value(v)))   DOMAIN_ERROR;
             const APL_Integer bint = B.get_near_int(v);
             if (bint < -0x80)        DOMAIN_ERROR;
             if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
             Z->next_ravel_Char(byte_or_Unicode(bint));
           }
      }
   else   // RPT_CELLS: mixed types
      {
        loop(v, ec)
           {
             Cell cache;
             const Cell & cell_B = B.get_cravel(v, cache);
             if (cell_B.is_character_cell())   // char to Unicode
                {
                  const Unicode uni = cell_B.get_char_value();
                  Z->next_ravel_Int(uni);
                  continue;
                }
             if (cell_B.is_integer_cell())
                {
                  const APL_Integer bint = cell_B.get_near_int();
                  if (bint < -0x80)        DOMAIN_ERROR;
                  if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
                  Z->next_ravel_Char(byte_or_Unicode(bint));
                  continue;
                }
             if (cell_B.is_float_cell())
                {
                  const APL_Integer bint = cell_B.get_near_int();
                  if (bint < -0x80)        DOMAIN_ERROR;
                  if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
                  Z->next_ravel_Char(byte_or_Unicode(bint));
                  continue;
                }
             if (cell_B.is_complex_cell())
                {
                  if (!Cell::is_near_zero(cell_B.get_imag_value()))   DOMAIN_ERROR;
                  const APL_Integer bint = cell_B.get_near_int();
                  if (bint < -0x80)        DOMAIN_ERROR;
                  if (bint > 0x7FFFFFFF)   DOMAIN_ERROR;
                  Z->next_ravel_Char(byte_or_Unicode(bint));
                  continue;
                }
             MORE_ERROR() << "⎕UCS got unexpected Cell type "
                          << cell_B.get_classname()
                          << ". Expecting int or character";
             DOMAIN_ERROR;
           }
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
void
Stop_Trace::assign(UserFunction * ufun, const cValue & new_value, bool stop)
{
std::vector<Function_Line> lines;
   lines.reserve(new_value.element_count());

   loop(l, new_value.element_count())
      {
        APL_Integer line = new_value.get_near_int(l);
        if (line < 1)   continue;
        lines.push_back(Function_Line(line));
      }

   ufun->set_trace_stop(lines, stop);
}
//────────────────────────────────────────────────────────────────────────────
const UserFunction *
Stop_Trace::locate_fun(const cValue & fun_name)
{
   if (!fun_name.is_char_string())   return 0;

UCS_string fun_name_ucs(fun_name);
   if (fun_name_ucs.size() == 0)   return 0;

// callers (Quad_STOP/Quad_TRACE eval_AB/eval_B) raise DOMAIN_ERROR once
// locate_fun() has failed for every candidate value -- report via
// )MORE instead of printing straight to CERR ahead of that error, which
// used to leak diagnostic text (e.g. "symbol NONEX not found") before
// the DOMAIN ERROR was ever raised. See Bugs27 #59(i).
//
Symbol * fun_symbol = Workspace::lookup_existing_symbol(fun_name_ucs);
   if (fun_symbol == 0)
      {
        MORE_ERROR() << "symbol " << fun_name_ucs << " not found";
        return 0;
      }

cFunction_P fun = fun_symbol->get_function();
   if (fun == 0)
      {
        MORE_ERROR() << "symbol " << fun_name_ucs << " is not a function";
        return 0;
      }

const UserFunction * ufun = fun->get_func_ufun();
   if (ufun == 0)
      {
        MORE_ERROR() << "symbol " << fun_name_ucs
                     << " is not a defined function";
        return 0;
      }

   return ufun;
}
//════════════════════════════════════════════════════════════════════════════
Token
Stop_Trace::reference(const std::vector<Function_Line> & lines, bool assigned)
{
Value_P Z(lines.size(), LOC);

   loop(z, lines.size())   Z->next_ravel_Int(lines[z]);

   Z->check_value(LOC);
   if (assigned)   return Token(TOK_APL_VALUE2, Z);
   else            return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_STOP::eval_AB(cValue_R A, cValue_R B) const
{
   // Note: Quad_STOP::eval_AB can be called directly or via S∆. If
   //
   // 1. called via S∆   then A is the function and B are the lines.
   // 2. called directly then B is the function and A are the lines.
   //
   if (const UserFunction * ufun = locate_fun(A))   // case 1.
      {
        assign(const_cast<UserFunction *>(ufun), B, true);
        return reference(ufun->get_stop_lines(), true);
      }

   // case 2.
   //
   if (const UserFunction * ufun = locate_fun(B))   // case 2.
      {
        assign(const_cast<UserFunction *>(ufun), A, true);
        return reference(ufun->get_stop_lines(), true);
     }

   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_STOP::eval_B(cValue_R B) const
{
   if (const UserFunction * ufun = locate_fun(B))
      return reference(ufun->get_stop_lines(), false);

   DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
Token
Quad_TRACE::eval_AB(cValue_R A, cValue_R B) const
{
   // Note: Quad_TRACE::eval_AB can be called directly or via S∆. If
   //
   // 1. called via S∆   then A is the function and B are the lines.
   // 2. called directly then B is the function and A are the lines.
   //
   if (const UserFunction * ufun = locate_fun(A))   // case 1.
      {
        assign(const_cast<UserFunction *>(ufun), B, false);
        return reference(ufun->get_trace_lines(), true);
      }

   if (const UserFunction * ufun = locate_fun(B))   // case 2.
      {
        assign(const_cast<UserFunction *>(ufun), A, false);
        return reference(ufun->get_trace_lines(), true);
      }

   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_TRACE::eval_B(cValue_R B) const
{
   if (const UserFunction * ufun = locate_fun(B))
      return reference(ufun->get_trace_lines(), false);

   DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════

