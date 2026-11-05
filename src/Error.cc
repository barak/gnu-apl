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

#include "Common.hh"
#include "Error.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "StateIndicator.hh"
#include "UserFunction.hh"
#include "Workspace.hh"

#include "Workspace.icc"

//════════════════════════════════════════════════════════════════════════════
Error::Error(ErrorCode ec, const char * loc)
   : error_code(ec),
     throw_loc(loc),
     parser_loc(0),
     show_locked(false),
     left_caret(-1),
     right_caret(-1),
     print_loc(0)
{
const bool have_more = Workspace::more_error().size();
   if (have_more && Workspace::more_error().back() != UNI_PLUS)
      {
        SPRINTF(error_message_1, "%s%c", error_name(error_code), UNI_PLUS);
      }
   else
      {
        SPRINTF(error_message_1, "%s", error_name(error_code));
      }

   if (have_more && Command::auto_MORE)
      {
        CERR << Workspace::more_error() << endl;
      }

   *symbol_name = 0;
   *error_message_2 = 0;
}
//════════════════════════════════════════════════════════════════════════════
bool
Error::is_known() const
{
   switch(error_code)
      {
   /// the cases
#define err_def(c, _txt, _maj, _min) case  E_ ## c:   return true;
#include "Error.def"
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Error::is_syntax_or_value_error() const
{
   if (error_major(error_code) == 2)   return true;   // some SYNTAX ERROR
   if (error_major(error_code) == 3)   return true;   // VALUE ERROR
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Error::add_MORE_indicator(bool have_more)
{
   if (have_more)   // )MORE info available
      {
        const size_t len = strlen(error_message_1);
        if (len && error_message_1[len - 1] != UNI_PLUS &&
            len < sizeof(error_message_1) - 1)
           {
             error_message_1[len]     = '+';
             error_message_1[len + 1] = 0;
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Error::get_error_line_3() const
{
   if (error_code == E_NO_ERROR)   return UCS_string();   // no error

   if (left_caret < 0)   return UCS_string();             // no ^ position

UCS_string ret;
   if (left_caret > 0)   ret << UCS_string(left_caret, UNI_SPACE);
   ret << UNI_CIRCUMFLEX;

const int diff = right_caret - left_caret;
   if (diff <= 0)   return ret;
   if (diff > 1)   ret << UCS_string(diff - 1, UNI_SPACE);
   ret << UNI_CIRCUMFLEX;

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
Error::print_EM(ostream & out, const char * loc)
{
   if (print_loc)
      {
        CERR << endl << "*** Error printed twice; first printed at "
             << print_loc << endl
             << "now printed from " << loc << endl;

        return;
      }

   print_loc = loc;

   // an empty line 1 means "no message" (LanguageVariances.md #28,
   // e.g. ⎕ES with an event code that has no defined ⎕ET text) --
   // skip it entirely rather than printing a blank line; ⎕EM itself
   // is unaffected (still a fixed 3-row matrix, see Quad_EM).
   //
const UCS_string line_1 = get_error_line_1();
   if (line_1.size())   out << line_1 << endl;

   out << get_error_line_2() << endl
       << get_error_line_3() << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Error::print(ostream & out, const char * loc) const
{
   if (print_loc)
      {
        CERR << endl << "*** Error printed twice; first printed at "
             << print_loc << endl
             << "now printed at " << loc << endl;

        return;
      }

   Log(LOG_verbose_error)
      {
        out << endl
            << "--------------------------" << endl;

        if (error_code == E_NO_ERROR)
           {
             out << error_name(E_NO_ERROR) << endl
                 << "--------------------------" << endl;

             return;
           }

        if (*error_message_1)
           {
             out << error_message_1 << endl;
           }
        else
           {
             out << error_name(error_code);
             if (Workspace::more_error().back() != UNI_PLUS)   out << UNI_PLUS;
             out << endl;
           }

        if (parser_loc)   out << "   Parser LOC: " << parser_loc  << endl;
        if (print_loc)    out << "   Print LOC:  " << print_loc   << endl;
        out                   << "   loc:        " << loc         << endl;
        loc = print_loc;

        if (*symbol_name)
           out                << "   Symbol:     " << symbol_name << endl;

        out <<                   "   Thrown at:  " << throw_loc   << endl
            <<                   "--------------------------"     << endl
                                                                  << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
/// error_message_1/2 are fixed-size byte buffers; a straight strncpy()
/// truncation has no notion of UTF-8 character boundaries and can chop
/// a multi-byte glyph in half, leaving a dangling lead byte that later
/// decodes as private-use-plane garbage (Blake McBride, Bugs23 #6a).
/// Call this ONLY when \b buf was genuinely truncated (its source was
/// longer than the buffer) -- on an untruncated, complete string this
/// would wrongly strip a valid trailing multi-byte character. Returns
/// the resulting *character* count (not byte count), for caret
/// clamping by the caller.
static int
truncate_utf8_boundary(char * buf)
{
int len = strlen(buf);

   // strip trailing continuation bytes (10xxxxxx): they belong to a
   // multi-byte sequence whose lead byte may or may not still be
   // present in buf, so keep going until we're clear of all of them
   while (len > 0 && (buf[len - 1] & 0xC0) == 0x80)   --len;

   // what (if anything) is left at the end is a multi-byte lead byte
   // whose sequence we just found to be incomplete (all of its
   // continuation bytes were removed above) -- since the caller only
   // reaches here when the source was longer than the buffer, that
   // sequence really was cut short, so drop the lead byte too
   if (len > 0 && (buf[len - 1] & 0x80) != 0)   --len;

   buf[len] = 0;
   return len - UTF8_string::bytes_chars(buf);   // byte length → char count
}
//────────────────────────────────────────────────────────────────────────────
void
Error::set_error_line_1(const char * msg_1)
{
   strncpy(error_message_1, msg_1, sizeof(error_message_1));
   error_message_1[sizeof(error_message_1) - 1] = 0;
}
//────────────────────────────────────────────────────────────────────────────
int
Error::set_error_line_2(const char * msg_2)
{
const size_t cap = sizeof(error_message_2) - 1;
const bool truncated = strlen(msg_2) > cap;
   strncpy(error_message_2, msg_2, sizeof(error_message_2));
   error_message_2[cap] = 0;
   if (truncated)   truncate_utf8_boundary(error_message_2);

   // char count (not byte count) of the (possibly truncated) result,
   // for callers that need to clamp a caret computed from the
   // untruncated source text (Blake McBride, Bugs23 #6b).
   return strlen(error_message_2) - UTF8_string::bytes_chars(error_message_2);
}
//────────────────────────────────────────────────────────────────────────────
void
Error::set_error_line_2(const UCS_string & ucs, int lcaret, int rcaret)
{
   // line 2 and the carets are meant to be collected exactly once, at
   // the single point (Tokenizer.cc lexical error, Token_string bracket
   // mismatch, ...) that actually knows the precise range -- never
   // blindly overwritten by an outer/later layer that only has a bare
   // ErrorCode to work with. Path A's update_error_info() does NOT use
   // this overload (it seeds line 2 via the 1-arg set_error_line_2() and
   // adjusts the carets incrementally via set_left_caret()/
   // set_right_caret()), so this Assert is not in its way.
   Assert(left_caret == -1);

UTF8_string utf(ucs);
const size_t cap = sizeof(error_message_2) - 1;
const bool truncated = utf.size() > cap;
   strncpy(error_message_2, utf.c_str(), sizeof(error_message_2));
   error_message_2[cap] = 0;

   // clamp the carets to line 2's actual (possibly truncated) character
   // count: without this, a caret set from the untruncated ucs can
   // point tens of columns past the end of the text ⎕EM[2;] ever shows
   // (Blake McBride, Bugs23 #6b).
const int nchars = truncated ? truncate_utf8_boundary(error_message_2)
                              : ucs.size();
   left_caret  = min(lcaret, nchars);
   right_caret = min(rcaret, nchars);
}
//────────────────────────────────────────────────────────────────────────────
void
Error::update_error_info(StateIndicator * si)
{
   /*
      construct lines 2 and 3 of the standard 3-line APL error message.
      Line 1 was already constructed in Error::Error(ErrorCode ec...).
      For eample: Let

            ∇FOO
      [1] 1 2 3 ◊ Q←Q++
      [2] ∇

      Then:

            FOO                   ⍝ APL code producing the error
      1 2 3                       ⍝ first (correct) statement
      VALUE ERROR                 ⍝ error line 1
      FOO[1]  Q←Q++               ⍝ error line 2: failed statement
                ^                 ⍝ error line 3: caret line (error range)

       There is no error_message_3 in class Error; the third error line
       is constructed from left_caret and right_caret when needed.

       lrm: "The left caret indicates how far execution of the expression
             progressed before the suspension occurred.
             The right caret indicates the likely point of the error. (On
             occasion, the two carets overlap so that only one is displayed.)"
    */

   // the "      " 6-space literal this used to hardcode here (and
   // below) happens to match Workspace::get_prompt()'s only live value
   // (Workspace.cc's constructor has an alternate, 6-*character* but
   // non-blank "  ∇   " prompt behind a permanently-false "#if 0 &&
   // MINGW_SRC", and a longer, fully commented-out "-----> " one) --
   // derive it from get_prompt() directly instead of duplicating
   // whatever its current value happens to be (user, 2026-08-19).
   //
   // A ⎕EB B where B failed and then the cleanup A failed too: A's error is
   // reported, and )MORE names the earlier failure of B. ⎕EC has registered
   // that failure in the )SI entry of the ⎕EB macro (see QuadFunction.cc),
   // below the frame(s) of A in which this error occurred. A ⎕EC safe-
   // execution start on the way means that the error is caught in A itself,
   // and an entry of B's failure ends with the macro, so that it can neither
   // be lost nor be left over (Blake McBride, Bugs32 #22, #32).
   //
   for (const StateIndicator * s = si; s; s = s->get_parent())
       {
         const UCS_string & B_failure = s->get_EB_B_failure();
         if (B_failure.size())
            {
              UCS_string & more = MORE_ERROR();
              const bool known = more.size() >= B_failure.size() &&
                     UCS_string(more, more.size() - B_failure.size(),
                                B_failure.size()) == B_failure;
              if (!known)   // not yet added (e.g. by an earlier call)
                 {
                   if (more.size())   more << UNI_LF;
                   more << B_failure;
                 }
              add_MORE_indicator(true);
              break;
            }
         if (s->is_safe_execution_start())   break;
       }

   {
     const UTF8_string prompt_utf(Workspace::get_prompt());
     const int nchars = set_error_line_2(prompt_utf.c_str());
     set_right_caret(-1);

     // first char after the prompt, clamped to the (possibly truncated)
     // line 2 (Bugs23 #6b)
     //
     const int after_prompt = Workspace::get_prompt().size();
     set_left_caret(min(after_prompt, nchars));
   }

   // prepare the second error line (= display of the failed statement)
   //
   // Walk up past any macro frame(s) (e.g. Z__LO_RANK_X5_B, pushed for
   // f⍤y/f⍣n/¨/∘. with a user-defined -- or, for a frame-rank ≥1 ⍤,
   // even a *primitive* -- operand) to the nearest real caller before
   // building the displayed statement: a macro's own internal
   // bookkeeping statement (e.g. "μ1[μ10]←⊂μ3 μ8⍴μ6[μ10;]") is a GNU
   // APL implementation detail the user never wrote and means nothing
   // to them, the same principle already applied to hide macro frames
   // from )SI (StateIndicator::list()) and to rename a lambda operand
   // away from its macro parameter name (find_lambda_name()) -- this
   // is the same fix for the *statement line* of the error message
   // itself, which neither of those touches. `si` itself (used below
   // for the actual stored Error, the prefix/error_range, and the
   // safe-execution walk) is intentionally left unchanged: the error
   // genuinely occurred in the macro's frame, only its DISPLAY is
   // redirected. See Bugs27 #56 residual.
   //
   const StateIndicator * display_si = si;
   while (display_si                                    &&
          display_si->get_executable()                  &&
          display_si->get_executable()->get_exec_ufun() &&
          display_si->get_executable()->get_exec_ufun()->is_macro())
      display_si = display_si->get_parent();

   if (display_si                   &&
       display_si->get_executable() &&
       display_si->get_executable()->get_exec_ufun())
      {
        const UserFunction * ufun = display_si->get_executable()
                                              ->get_exec_ufun();
        // ufun->print_line_PCs(LOC);
        // property 1 (nonsuspendable) is inherited from every calling
        // )SI entry too (apl2lrm.txt p.360-361 "or-ing"), not just ufun's
        // own: a function called (directly or transitively) by a
        // nonsuspendable one must behave as nonsuspendable itself.
        //
        if (get_show_locked() || display_si->get_inherited_exec_property(1))
           {
             {
               const UTF8_string prompt_utf(Workspace::get_prompt());
               const int nchars = set_error_line_2(prompt_utf.c_str());
               const int after_prompt = Workspace::get_prompt().size();
               set_left_caret(after_prompt > nchars ? nchars : after_prompt);
             }
             ufun->set_locked_error_info(*this);
             goto out;   // maybe print
           }

        UCS_string ucs(ufun->get_name_and_line(display_si->get_PC()));
        ucs << UNI_SPACE << UNI_SPACE;
        const UTF8_string utf(ucs);
        const int nchars = set_error_line_2(utf.c_str());
        // clamp to the (possibly truncated) line 2 (Blake McBride,
        // Bugs23 #6b): an untruncated ucs.size() can point tens of
        // columns past the end of the text ⎕EM[2;] ever shows.
        const int wanted = ucs.size();
        set_left_caret(wanted > nchars ? nchars : wanted);
      }

   // Function_PC2 positions are only meaningful within the SAME
   // Executable they were read from, so this must use display_si (the
   // same frame the statement text above was built from), not si --
   // si's own prefix positions index into the MACRO's body, and
   // set_error_info() below would re-derive and overwrite line 2/3
   // with the macro's raw internal statement all over again (undoing
   // the display_si substitution above) if left as si. Skipped
   // entirely when there is no non-macro display_si to fall back to.
   // See Bugs27 #56 residual.
   //
   if (display_si)
      {
        // from and to are the (Token_loc-) PCs of the rightmost and the
        // leftmost item of the range being reduced, i.e. positions in the
        // tokens of the line (NOT in the body). stat_PC is a position in
        // the body (inside the failed statement).
        //
        const Prefix & prefix = display_si->get_prefix();
        Function_PC from = prefix.get_phrase_low();
        Function_PC to   = prefix.get_phrase_high();
        const Prefix_PC stat_PC = prefix.get_statement_PC();
        bool fun_of_result = false;

        // a VALUE ERROR whose failed token is known (a symbol without a
        // value, or a function that returned no value): a single caret
        // under that token. The range being reduced does not contain the
        // symbol (it is thrown while the symbol is being resolved), or
        // ends at the statement end (for a function without result).
        //
        const bool failed_token = failed_exec   &&
                    error_code == E_VALUE_ERROR &&
                    failed_exec == display_si->get_executable();
        if (failed_token)
           {
             from = to = failed_PC;
             fun_of_result = failed_is_result;
           }
        failed_exec = 0;

        // Bugs28 #100(g): display_si's own Prefix, at the moment a
        // *primitive* operand of a macro-based operator (e.g. f⍤y with
        // multiple chunks) throws, only reflects "the tokens involved
        // in dispatching the call" -- for a still-running (not itself
        // suspended) caller like a bare immediate-execution statement,
        // that degenerates to a single point (confirmed: both carets
        // landed on the derived function's own opening '(', e.g.
        // (⍳⍤1)2 2⍴'ab', regardless of which operand cell actually
        // failed). display_si != si here specifically means we walked
        // up past at least one macro frame to reach a real caller whose
        // own range we cannot otherwise recover (plan1.txt: a macro
        // with a primitive operand argument is an internal-dispatch
        // shortcoming, not something the ordinary per-token error range
        // was ever designed to pinpoint) -- fall back to the whole
        // *statement* the degenerate point falls in, which at least
        // shows the reader the real, complete user expression instead
        // of a single misleading caret at its very start.
        //
        const bool whole_statement = from == to && display_si != si &&
                                     !failed_token;

        // like IBM APL2, place the right caret under the failed function
        // or operator (rather than under the right end of the phrase).
        // A SYNTAX ERROR has no failed function (the phrase matched no
        // pattern), its right caret is placed by Prefix::adjust_right_caret().
        //
        if (!failed_token && !whole_statement &&
            error_code != E_SYNTAX_ERROR && error_code != E_LEFT_SYNTAX_ERROR)
           from = prefix.get_failed_function();

        const Function_PC2 error_range(from, to);
        display_si->get_executable()->set_error_info(*this, stat_PC,
                                     error_range, fun_of_result,
                                     whole_statement);
      }

   // print error, unless we are in safe execution mode.
   //
out:
   failed_exec = 0;   // used (or not) by this error

   // E_COMMAND_PUSHED (Bugs28 #95) is Bif_F1_EXECUTE.cc's internal
   // signal that a )LOAD/)QLOAD/)CLEAR/)RESET/)SIC inside ⍎ needs to
   // run via Command::do_APL_expression() instead of here -- its own
   // comment says "we throw E_COMMAND_PUSHED but without displaying
   // it", but nothing here ever exempted it: it was stored as if it
   // were a real error (⎕ET/⎕EM reporting an "error" -- with a
   // function line and carets, inside a defined function -- for a
   // successful operation) and printed unconditionally by print_EM()
   // below. Return before either happens; Command.cc's own handling of
   // this code is unaffected (the exception is still thrown/caught the
   // same way either way).
   //
   if (get_error_code() == E_COMMAND_PUSHED)   return;

   StateIndicator::get_error(si) = *this;

   // )SI entries below a ⎕ES entry must not print anything but simply return
   //
   for (const StateIndicator * si1 = si; si1; si1 = si1->get_parent())
       {
         if (si1->get_safe_execution_depth())   return;
       }

   print_EM(UERR, LOC);
}
//────────────────────────────────────────────────────────────────────────────
const char *
Error::error_name(ErrorCode err)
{
   switch(err)
      {
   /// the cases
#define err_def(c, txt, _maj, _min) \
   case E_ ## c:   return txt;

#include "Error.def"
      }

   return "Unknown Error";
}
//════════════════════════════════════════════════════════════════════════════
/// map \b code to DOMAIN ERROR if property 3 (error conversion) is set on
/// \b si or inherited from any of its calling )SI entries (apl2lrm.txt
/// p.360-361 "or-ing", not just the current function's own -- a function
/// called (directly or transitively) by an error-converting one must
/// convert its own errors the same way). Shared by throw_apl_error() and
/// Error::throw_symbol_error(), which is a separate VALUE_ERROR throw
/// site that does not go through throw_apl_error() at all and therefore
/// needs the same check applied to its own base error code.
static ErrorCode
maybe_convert_to_domain_error(ErrorCode code, StateIndicator * si)
{
   if (si && si->get_inherited_exec_property(3))   return E_DOMAIN_ERROR;
   return code;
}
//════════════════════════════════════════════════════════════════════════════
void
throw_apl_error(ErrorCode code, const char * loc)
{
   ADD_EVENT(0, VHE_Error, code, loc);

StateIndicator * si = Workspace::SI_top();   // the current )SI entry

   Log(LOG_error_throw)
      {
        CERR << endl
             << "throwing " << Error::error_name(code)
             << " at " << loc << endl;
      }

   Log(LOG_verbose_error)
      {
        if (!(si && si->get_safe_execution_depth()))   BACKTRACE
      }

   code = maybe_convert_to_domain_error(code, si);

Error error(code, loc);
   if (si)   error.update_error_info(si);

const Error & eref = error;
   throw eref;
}
//════════════════════════════════════════════════════════════════════════════
void
Error::throw_define_error(const UCS_string & fun_name, const UCS_string & cmd,
                          const char * loc)
{
   // this error is thrown if the ∇-editor detects an invalid function header,
   // for example: "∇FOO 2"

   Log(LOG_error_throw)   
      {   
        CERR << "throwing DEFN ERROR at " << loc
             << " (function is " << fun_name << ")" << endl;
      }

   Log(LOG_verbose_error)     BACKTRACE

Error err(E_DEFN_ERROR, loc);
Error & eref = err;
UTF8_string fun_name_utf(fun_name);
   SPRINTF(err.symbol_name, "%s", fun_name_utf.c_str());

UTF8_string cmd_utf(cmd);   // cmd is something like ∇FUN[⎕]∇
   SPRINTF(err.error_message_2, "PROMPT%s", cmd_utf.c_str());
   eref.left_caret = 5 + cmd.size();   // last Unicode of error_message_2
   if (Workspace::SI_top())   *Workspace::get_error() = eref;
   throw eref;
}
//────────────────────────────────────────────────────────────────────────────
void
Error::throw_parse_error(ErrorCode code, const char * par_loc, const char *loc)
{
   Log(LOG_error_throw)
      {
        CERR << "\nthrowing " << Error::error_name(code)
             << " at " << loc << endl;
      }

   Log(LOG_verbose_error)   BACKTRACE

   // set )MORE error (unless the caller has done so)
   if (Workspace::more_error().size() == 0)   // no )MORE info yet
      {
        MORE_ERROR() << Error::error_name(code);
      }

Error error(code, loc);
   error.parser_loc = par_loc;

   // Bugs28 #93: unlike throw_apl_error() (the run-time error path),
   // this never stored the error where ⎕ET/⎕EM look for it
   // (StateIndicator::get_error()), so e.g. immediate execution of a
   // fix-time-only error like "1)" (unbalanced parenthesis) printed
   // the right message but left ⎕ET/⎕EM reporting whatever the
   // *previous* statement's error had been. Store directly rather than
   // calling the full update_error_info(): this throw happens before
   // the failing statement has its own SI entry, so update_error_info()
   // would rebuild error_message_2/3 from the *previous* statement's
   // still-current Executable/prefix state (the exact stale-caret
   // problem described where match_par_bra()'s ErrorCode-return style
   // is used instead, a few callers up) and clobber the message just
   // built above. The caller already prints this error itself.
   //
   if (StateIndicator * si = Workspace::SI_top())
      StateIndicator::get_error(si) = error;

const Error & eref = error;
   throw eref;
}
//────────────────────────────────────────────────────────────────────────────
void
Error::throw_parse_error(ErrorCode code, const UCS_string & line,
                         int start, int end, const char * par_loc,
                         const char * loc)
{
   Log(LOG_error_throw)
      {
        CERR << "\nthrowing " << Error::error_name(code)
             << " at " << loc << endl;
      }

   Log(LOG_verbose_error)   BACKTRACE

   // set )MORE error (unless the caller has done so)
   if (Workspace::more_error().size() == 0)   // no )MORE info yet
      {
        MORE_ERROR() << Error::error_name(code);
      }

Error error(code, loc);
   error.parser_loc = par_loc;

   if (start < 0)                start = 0;
   if (end < start)              end = start;
   if (end > int(line.size()))   end = line.size();

const UCS_string fragment(line, start, end - start);

   // line 2 has the same margin (the prompt) as other errors (Bugs37 #62)
   //
UCS_string margined(Workspace::get_prompt());
const int margin = margined.size();
   margined << fragment;
   error.set_error_line_2(margined, margin, margin + fragment.size());

   // Bugs28 #93: see the other throw_parse_error() overload above --
   // store directly, not via the full update_error_info().
   //
   if (StateIndicator * si = Workspace::SI_top())
      StateIndicator::get_error(si) = error;

const Error & eref = error;
   throw eref;
}
//────────────────────────────────────────────────────────────────────────────
const Executable * Error::failed_exec = 0;
Function_PC Error::failed_PC = Function_PC_invalid;
bool Error::failed_is_result = false;
//────────────────────────────────────────────────────────────────────────────
void
Error::throw_symbol_error(const UCS_string & sym_name, const char * loc)
{
   Log(LOG_error_throw)   
      {   
        CERR << "throwing VALUE ERROR at " << loc;
        if (sym_name.size())   CERR << " (symbol is: '" << sym_name << "')"; 
        CERR << endl;
      }

   // Workspace::SI_top() can return null (empty SI stack, e.g. no
   // statement/function currently suspended) -- the unguarded
   // dereference below crashed in that case; SI_top() is correctly
   // null-checked a few lines further down in this same function for
   // exactly this reason. An empty SI is trivially "not in ⎕ES", so
   // treat it the same as get_safe_execution_depth()==0.
   if (Workspace::more_error().size() == 0 &&   // no )MORE info provided, and
       (!Workspace::SI_top() ||
        Workspace::SI_top()->get_safe_execution_depth() == 0))   // and not in ⎕ES
      {
        char extra[20] = { 0 };
        if (sym_name.size() == 1 && sym_name[0] >= 0x80)
           SPRINTF(extra, " (U+%4.4X)", sym_name[0]);
        MORE_ERROR() << "Offending symbol: " << sym_name << extra;
      }

   Log(LOG_verbose_error)     BACKTRACE

StateIndicator * si = Workspace::SI_top();

Error err(maybe_convert_to_domain_error(E_VALUE_ERROR, si), loc);
UTF8_string sym_name_utf(sym_name);
   SPRINTF(err.symbol_name, "%s", sym_name_utf.c_str());

   if (si)   err.update_error_info(si);   // )SI not empty

const Error & eref = err;
   throw eref;
}
//════════════════════════════════════════════════════════════════════════════

