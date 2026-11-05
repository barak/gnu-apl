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


#include "Executable.hh"
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_RHO.hh"
#include "Parser.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "UCS_string.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

#include "Workspace.icc"

//════════════════════════════════════════════════════════════════════════════
Executable::Executable(const UCS_string & ucs,  bool multi_line,
                       ParseMode pm, const char * loc)
   : alloc_loc(loc),
     pmode(pm),
     quiet_parse(false),
     refcount(0)
{
// { cerr << "Executable " << (void *)this << " created at " << loc << endl; }

   // initialize text, stripping carriage returns and line feeds.
   //
   if (multi_line)
      {
        UCS_string line;

        loop(t, ucs.size())
           {
             const Unicode uni = ucs[t];
             // a CR is ignored if it ends a line (CRLF), but kept e.g. in
             // a string literal (Bugs35 #71)
             //
             if (uni == UNI_CR &&
                 (t + 1 == ucs.ssize() || ucs[t + 1] == UNI_LF))   continue;

             switch (uni)
                {
                  case UNI_LF: text.push_back(line);   line.clear();   break;
                  default:     line << uni;
               }
           }

        if (line.size())   text.push_back(line);
      }
   else
      {
        text.push_back(ucs);
      }
}
//────────────────────────────────────────────────────────────────────────────
Executable::Executable(Fun_signature sig, int lambda_num,
                       const UCS_string & lambda_text, const char * loc)
   : alloc_loc(loc),
     pmode(PM_FUNCTION),
     quiet_parse(false),
     refcount(0)
{
UCS_string header = UserFunction_header::lambda_header(sig, lambda_num);

   // remove local vars from lambda_text and add them to the header
   //
ShapeItem last_semi = -1;
   rev_loop(t,lambda_text.size())
       {
         const Unicode cc = lambda_text[t];
         if (cc == UNI_SPACE)            continue;
         if (Avec::is_symbol_char(cc))   continue;
         if (Avec::is_quad(cc))          continue;
         if (cc == UNI_SEMICOLON)   last_semi = t;
         else                             break;
       }

   if (last_semi != -1)
      {
        for (ShapeItem t = last_semi; t < lambda_text.ssize(); ++t)
            header << lambda_text[t];
        text.push_back(header);

        UCS_string new_text(lambda_text);
        new_text.resize(last_semi);
        new_text.remove_trailing_whitespaces();
        text.push_back(new_text);
      }
   else
      {
        text.push_back(header);
        text.push_back(lambda_text);
     }
}
//════════════════════════════════════════════════════════════════════════════
Executable::~Executable()
{
   Log(LOG_UserFunction__fix)
      {
        CERR << "deleting Executable " << voidP(this)
             << " (body size=" << body.size() << ")" << endl;
      }

   clear_body();
}
//────────────────────────────────────────────────────────────────────────────
Token
Executable::execute_body() const
{
StateIndicator & si = *Workspace::SI_top();

   try                       { return si.run();                             }
   catch (const Error & err)
      {
        // The error has ended the current statement of si: it can only be
        // restarted, not resumed, so a selective specification in progress
        // in it can never complete. Neutralize the LvalCells (references
        // into the target variable) in the values of its stack, rather than
        // keeping them alive in the suspended statement until )SIC (Blake
        // McBride, Bugs32 #25). Not earlier (e.g. when the error is thrown):
        // an error caught inside a primitive does not end the statement.
        //
        // ⎕ES may have popped si already (e.g. a LENGTH ERROR of A{⍺}¨B,
        // raised by ⎕ES in a macro frame, Bugs34 #4): then si is deleted
        // and must not be touched.
        //
        for (StateIndicator * s = Workspace::SI_top(); s; s = s->get_parent())
            {
              if (s == &si)
                 {
                   si.get_prefix().neutralize_lval_values();
                   break;
                 }
            }
        return Token(TOK_ERROR, err.get_error_code());
      }

   catch (std::bad_alloc &)
      {
        // Bugs8 #2 (Blake McBride): a bare Token(TOK_ERROR, ...) here (with
        // no Error ever stored into the SI) left ⎕ET/⎕EM holding whatever
        // the *previous* error was, so Command::process_line() printed
        // nothing and the caller (interactively or inside a function) saw
        // no result and no error at all. Build and attach a real Error
        // instead, exactly like throw_apl_error() does.
        //
        Error error(E_WS_FULL, LOC);
        if (StateIndicator * si = Workspace::SI_top())
           error.update_error_info(si);
        MORE_ERROR() << "C++ allocation failed (std::bad_alloc)";
        return Token(TOK_ERROR, error.get_error_code());
      }
   catch (...)              { FIXME; }

   // not reached
   FIXME;
}
//════════════════════════════════════════════════════════════════════════════
Prefix_PC
Executable::get_statement_end(Prefix_PC pc) const
{
   // bounded scan: without the pc < body.size() check, a body that (for
   // whatever reason) has no TC_END/TOK_RETURN_EXEC at or after pc would
   // make this loop walk off the end of body[] indefinitely.
   //
   while (pc < body.end_PC()   &&
          body[pc].get_Class() != TC_END  &&
          body[pc].get_tag() != TOK_RETURN_EXEC)   ++pc;
   Assert(pc < body.end_PC());
   return pc;
}
//────────────────────────────────────────────────────────────────────────────
Prefix_PC
Executable::get_statement_start(Prefix_PC pc) const
{
   // this function is used in error reporting; do_Assert() itself guards
   // against re-entrant failures (see Assert.cc), so Assert() here is safe.
   //
   // given pc, return the start of the statement to which pc belongs

   Assert(pc < body.end_PC());

   // if we are at the end of the statement, move back.
   //
   if (int(pc) < 2)   return Prefix_PC_0;
   if (body[pc].get_Class() == TC_RETURN)   --pc;
   if (body[pc].get_Class() == TC_END)      --pc;

   for (; pc > Prefix_PC_0; --pc)
      {
        if (body[pc-1].get_Class() == TC_END)          return pc;
        if (body[pc-1].get_tag() == TOK_RETURN_EXEC)   return pc;
      }

   return Prefix_PC_0;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::print_text(ostream & out) const
{
   loop(l, text.size())   out << text[l] << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::print_token(ostream & out, int details) const
{
   out << endl
       <<  "Function body " << &body
       << " [" << body.size() << " token]:" << endl;

   body.print(out, details);
}
//────────────────────────────────────────────────────────────────────────────
bool
Executable::reparse(Token_string & original, Prefix_PC stat_PC,
                    const Token_string & stat) const
{
   // the line that contains the failed statement. ⍎ and ◊ executables have
   // only line 0.
   //
int line = 0;
   if (get_parse_mode() == PM_FUNCTION)   line = get_line(stat_PC);

   // clamp line: a function with N body lines has text.ssize() == N+1,
   // while the line of the trailing RETURN region can be N+1. The lines
   // of the body differ from the lines of text after a multi-line string
   // or literal (see get_body_text()).
   //
   if (line >= text.ssize())   line = text.ssize() - 1;
   if (parsed_text.size() && line >= parsed_text.ssize())
      line = parsed_text.ssize() - 1;
   if (line < 0)   return false;

const UCS_string & line_text = get_body_text(line);

   // parse the entire line again, without optimizations. The PC of every
   // token is the position of its (leftmost) token in the token string of
   // the tokenizer, i.e. the same as the PCs of the (optimized) body. Line
   // 2 shows the canonical form of the parsed tokens (APL input format, see
   // set_error_info()), not the text as entered.
   //
const Parser parser(get_parse_mode(), LOC, false);
Token_string orig;
   try
      {
        if (parser.parse(line_text, orig, false))   return false;
      }
   catch (const Error &)
      {
        // parser.parse() throws (rather than returning a code) for lexical
        // failures -- but we are already building the display for a
        // DIFFERENT, already-pending error; letting a fresh exception
        // escape here would silently replace the error being reported.
        //
        return false;
      }

   // the failed statement starts (in APL order) at its leftmost token, i.e.
   // at the smallest PC in stat, and extends to the right up to (excluding)
   // the first ◊ or COND token that is not inside a lambda, or to a ; of
   // the local variables of a lambda (which were moved to its header), or
   // to the end of the line.
   //
Function_PC min_pc = Function_PC_invalid;
   loop(s, stat.size())
      {
        const Function_PC pc = stat[s].get_PC();
        if (pc >= 0 && (min_pc < 0 || pc < min_pc))   min_pc = pc;
      }
   if (min_pc < 0)   return false;

ShapeItem from = 0;   // the leftmost token of the statement in orig
   while (from < orig.ssize() && orig[from].get_PC() < min_pc)   ++from;

   // the label of the line (if any) belongs to its first statement. It is
   // not in the body (see parse_body_line()), but shall be shown in line 2.
   //
   if (from == 2 && get_parse_mode() == PM_FUNCTION &&
       orig[0].get_tag() == TOK_SYMBOL && orig[1].get_tag() == TOK_COLON)
      from = 0;

ShapeItem to = from;   // the token after the rightmost token in orig
int curly = 0;         // nesting level of { }
int bracket = 0;       // nesting level of [ ]
   for (; to < orig.ssize(); ++to)
       {
         const TokenTag tag = orig[to].get_tag();
         if      (tag == TOK_L_CURLY)   ++curly;
         else if (tag == TOK_R_CURLY)   --curly;
         else if (orig[to].get_Class() == TC_L_BRACK &&
                  tag != TOK_SEMICOL)   ++bracket;
         else if (orig[to].get_Class() == TC_R_BRACK)   --bracket;
         else if (curly == 0 && (tag == TOK_DIAMOND || orig[to].is_COND()))
              break;
         else if (curly == 0 && bracket == 0 && tag == TOK_SEMICOL)
              break;
       }

   for (ShapeItem j = to - 1; j >= from; --j)   // reversed
       original.push_back(orig[j]);

   return original.size() != 0;
}
//────────────────────────────────────────────────────────────────────────────
/// the index (in \b tos, reversed as in body) of the token that contains
/// the (Token_loc-) \b pc, i.e. the token with the largest PC <= pc. If
/// several tokens have that PC (e.g. a ( inserted by the parser) then the
/// leftmost of them, since the PC of an item is that of its leftmost token.
static Function_PC
token_at(const Token_string & tos, Function_PC pc)
{
const Function_PC leftmost = Function_PC(tos.size() - 1);
   if (pc < 0)   return leftmost;

Function_PC best = leftmost;
Function_PC best_pc = Function_PC_invalid;
   for (Function_PC t = leftmost; t >= 0; --t)   // from left to right
      {
        const Function_PC tpc = tos[t].get_PC();
        if (tpc > pc)   break;   // the PCs increase from left to right
        if (tpc > best_pc)   { best = t;   best_pc = tpc; }
      }

   return best;
}
//────────────────────────────────────────────────────────────────────────────
/// the index (in \b tos, reversed as in body) of the function that is
/// located at or right of tos[idx] (but not inside parentheses or brackets
/// of its left argument), i.e. the function whose result starts at tos[idx]
static Function_PC
function_at(const Token_string & tos, Function_PC idx)
{
int depth = 0;
   for (Function_PC p = idx; p >= 0 && p < tos.ssize(); --p)
       {
         const Token & tok = tos[p];
         switch(tok.get_Class())
            {
              case TC_L_PARENT:
              case TC_L_BRACK:   ++depth;   continue;   // left of A's inside
              case TC_R_PARENT:
              case TC_R_BRACK:   --depth;   continue;
              case TC_END:
              case TC_RETURN:    return idx;   // statement end: not found
              default:           break;
            }

         if (depth > 0)   continue;   // inside of A

         if (tok.get_Class() == TC_FUN0 || tok.get_Class() == TC_FUN12)
            return p;

         if (tok.get_Class() == TC_SYMBOL)
            {
              const Symbol * sym = tok.get_sym_ptr();
              const NameClass nc = sym ? sym->get_NC() : NC_INVALID;
              if (nc == NC_FUNCTION || nc == NC_OPERATOR)   return p;
            }
       }

   return idx;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::set_error_info(Error & error, Prefix_PC stat_PC,
                           Function_PC2 pcs, bool fun_of_result,
                           bool whole_statement) const
{
   /* called rarely, so we have time.
   
      error:   the error whose info shall be set
      stat_PC: a position in body (!) inside the failed statement.
      pcs:     the PCs of the Token_locs of the rightmost (pcs.low) and the
               leftmost (pcs.high) item of the failed range. Unlike stat_PC
               they are positions in the token string that the tokenizer has
               produced for the line of the failed statement (and are
               therefore the same in the optimized body and in the
               unoptimized reparse() of the line).
    */
   if (error.get_error_code() == E_VALUE_ERROR)
      {
        // for value errors we point to the failed symbol itself.
        //
        pcs.low = pcs.high;
      }
   else if (error.get_error_code() == E_ATTENTION ||
            error.get_error_code() == E_INTERRUPT)
      {
        /* E_ATTENTION is typically raised at the end of the line, i.e. when
           attention_raised was checked and not when attention_raised was
           raised. We use the range recorded when ^C was hit instead.

           E_INTERRUPT may be raised when interrupt_raised was checked (and
           not when interrupt_raised was raised). Note that E_INTERRUPT may
           also be raised synchronously, e.g. when a system limit was hit.

           The range was recorded in the SI top when ^C was hit, which may
           be another executable (e.g. the macro of LO⍣RO, whose error is
           reported for the calling statement): then it is not a range of
           this body (Bugs34 #37).
         */
        const bool attention = error.get_error_code() == E_ATTENTION;
        const Function_PC2 & cc_range =
                     attention ? InterruptContext::get_attention_range()
                               : InterruptContext::get_interrupt_range();
        const Prefix_PC cc_stat =
                     attention ? InterruptContext::get_attention_stat_PC()
                               : InterruptContext::get_interrupt_stat_PC();
        if (cc_stat != Prefix_PC_invalid &&
            InterruptContext::get_range_executable() == this)
           {
             pcs = cc_range;
             stat_PC = cc_stat;
           }
        else
           {
             // the statement of this executable (which need not be the SI
             // top, see above)
             const StateIndicator * si = Workspace::SI_top();
             while (si->get_parent() && si->get_executable() != this)
                   si = si->get_parent();
             const Prefix & prefix = si->get_prefix();
             stat_PC = prefix.get_statement_PC();
             if (stat_PC < Prefix_PC_0 || stat_PC >= body.end_PC())
                stat_PC = Prefix_PC_0;
             pcs.low = pcs.high = body.size() ? body[stat_PC].get_PC()
                                              : Function_PC_invalid;
           }
      }

   Log(LOG_prefix__location_info)
      {
        Q1(body.size())
        Q1(stat_PC)
        Q1(pcs.low)
        Q1(pcs.high)
      }

   if (body.size() == 0)   return;

   // find the failed statement in body. A RETURN token belongs to the
   // statement before it.
   //
   if (stat_PC < Prefix_PC_0)      stat_PC = Prefix_PC_0;
   if (stat_PC >= body.end_PC())   stat_PC = body.end_PC() - 1;

   if (stat_PC > Prefix_PC_0                  &&
       body[stat_PC].get_Class() == TC_RETURN &&
       body[stat_PC].get_tag() != TOK_RETURN_EXEC)
      stat_PC = stat_PC - 1;

const Prefix_PC start = get_statement_start(stat_PC);
const Prefix_PC end   = get_statement_end(stat_PC);

   // the tokens of the failed statement (reversed, without its END)
   //
   if (start == end)   return;   // empty statement
const Token_string stat_after(body, size_t(start), end - start);

   // display the statement as written in the original text (i.e. parse its
   // line again, without optimizations). Fall back to the (possibly
   // optimized) statement in body only if that fails.
   //
Token_string stat_before;
const bool reparsed = reparse(stat_before, stat_PC, stat_after);
const Token_string & failed_statement = reparsed ? stat_before : stat_after;

   // map the PCs to the tokens of failed_statement (whose index 0 is the
   // rightmost token). The END token of the statement is not in
   // failed_statement, but has the PC of its leftmost token.
   //
Function_PC left_pc  = pcs.high;
Function_PC right_pc = pcs.low;
   if (whole_statement)   // from the leftmost to the rightmost item
      {
        left_pc  = stat_after[stat_after.size() - 1].get_PC();
        right_pc = stat_after[0].get_PC();
      }
   if (left_pc == Function_PC_invalid)    left_pc = right_pc;
   if (right_pc == Function_PC_invalid)   right_pc = left_pc;

   // the failed phrase lies within the failed statement, i.e. (APL order):
   // left end of statement ≤ left end of phrase ≤ right end of phrase
   //                       ≤ right end of statement
   //
   if (left_pc != Function_PC_invalid)
      {
        const Function_PC stat_left  = stat_after[stat_after.size() - 1].get_PC();
        const Function_PC stat_right = stat_after[0].get_PC();
        Assert(stat_left <= left_pc && left_pc <= right_pc &&
               right_pc <= stat_right);
      }

const Function_PC leftmost = Function_PC(failed_statement.size() - 1);
Function_PC2 range(Function_PC_0, leftmost);
   if (left_pc != Function_PC_invalid)
      {
        range.high = token_at(failed_statement, left_pc);
        range.low  = token_at(failed_statement, right_pc);
        if (fun_of_result)
           {
             // the missing result of a function: mark the function
             range.low = range.high = function_at(failed_statement,
                                                  range.high);
           }
        if (range.low > range.high)   range.low = range.high;
      }

   set_error_info(error, failed_statement, range);
}
//────────────────────────────────────────────────────────────────────────────
/// return true if \b tok is (the right end of) a value, i.e. if a value
/// token left of it would be part of the same strand
static bool
ends_value(const Token & tok)
{
   switch(tok.get_Class())
      {
        case TC_VALUE:
        case TC_SYMBOL:
        case TC_R_PARENT:
        case TC_R_BRACK:   return true;
        default:           return false;
      }
}
//────────────────────────────────────────────────────────────────────────────
/// return true if \b tok is (the left end of) a value or an index, i.e. if a
/// value token right of it would be part of the same strand or indexed
static bool
starts_value(const Token & tok)
{
   switch(tok.get_Class())
      {
        case TC_VALUE:
        case TC_SYMBOL:
        case TC_L_PARENT:   return true;
        case TC_L_BRACK:    return tok.get_tag() == TOK_L_BRACK;
        default:            return false;
      }
}
//────────────────────────────────────────────────────────────────────────────
/// the canonical text (APL input format) of item \b q of the (reversed)
/// \b statement. The parser has folded a parenthesized group, e.g. (1 2),
/// into a single (TOK_APL_VALUE1) value. Such a value of 2 or more items is
/// put in parentheses if it would otherwise merge with a neighbour, e.g.
/// A (1 2) or (1 2 3)[2]. A string ('AB') is a single item and needs no
/// parentheses.
static UCS_string
canonical_item(const Token_string & statement, ShapeItem q)
{
const Token & tok = statement[q].get_token();
const UCS_string text = tok.canonical(PR_APL_FUN).remove_pad();

   // the : of a label (see reparse()), followed by a blank like in ⎕CR
   //
   if (tok.get_tag() == TOK_COLON)   return UCS_string(UTF8_string(": "));

   if (tok.get_tag() != TOK_APL_VALUE1 || !tok.is_apl_val())   return text;

const cValue & value = *tok.get_apl_val();
   if (value.get_rank() != 1 || value.element_count() < 2 ||
       value.is_char_vector())   return text;

   // statement is reversed: q + 1 is the left and q - 1 the right neighbour
   //
const bool merge_left  = q + 1 < statement.ssize() &&
                         ends_value(statement[q + 1].get_token());
const bool merge_right = q > 0 && starts_value(statement[q - 1].get_token());

   // the right operand of ⍣ or ⍤ (possibly after the axis of ⍤): without
   // the parentheses, e.g. -⍣3 5, the parser would split it into N resp. y
   // and B (Bugs35 #38)
   //
bool after_RO = false;
   for (ShapeItem l = q + 1; l < statement.ssize(); ++l)
       {
         const TokenTag tag = statement[l].get_token().get_tag();
         if (tag == TOK_FAXIS)   continue;   // a literal axis, e.g. ⍤[1]
         after_RO = tag == TOK_OPER2_POWER || tag == TOK_OPER2_RANK;
         break;
       }

   if (!(merge_left || merge_right || after_RO))   return text;

UCS_string ret;
   ret << UNI_L_PARENT << text << UNI_R_PARENT;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::set_error_info(Error & error,
                           const Token_string & failed_statement, 
                           Function_PC2 range) const
{
   if (error.get_error_code() == E_SYNTAX_ERROR)
      {
        Prefix::adjust_right_caret(range, failed_statement);
        error.add_MORE_indicator(Workspace::more_error().size());
      }

   Log(LOG_prefix__location_info)
      {
        Q1(range.low)
        Q1(range.high)
        Q1(failed_statement.size())
      }

   // Line 2: statement
   //

int len_left = 0;      // characters before the left caret
int len_between = 0;   // distance between the left and the right caret

   // every statement is stored right to left. In order to print it
   // left to right we have to move backwards from the end.
   //
UCS_string message_2(error.get_error_line_2());
   rev_loop(q, failed_statement.size())
       {
         // avoid duplicate ∘ in ∘.f
         //
         if (failed_statement[q].get_tag() == TOK_JOT && q > 0 &&
             failed_statement[q - 1].get_tag() == TOK_OPER2_OUTER)   continue;

         // Note: Token::error_info returns -len if it inserts a space.
         // Such an inserted space counts for the previous token.
         // The previous token is q + 1 since we count down.
         //
         int len = Token::error_info(message_2,
                                     canonical_item(failed_statement, q));

         if (len < 0)   // space inserted, len is negative
            {
              if ((q + 1) > range.high)       len_left++;
              else if ((q + 1) > range.low)   len_between++;
              len = -len;
            }

         if (q > range.high)       len_left += len;
         else if (q > range.low)   len_between += len;
       }

int nchars;
   {
     const UTF8_string utf(message_2);
     nchars = error.set_error_line_2(utf.c_str());
   }

   // Line 3: carets
   //
   // Both carets are offsets into message_2 as just (re)built above; if
   // that text was longer than ⎕EM[2;] actually shows (truncated to
   // nchars), clamp both to the truncated length instead of pointing
   // tens of columns past the end of the printed text (Blake McBride,
   // Bugs23 #6b).
const int wanted_left = error.get_left_caret() + len_left;
   error.set_left_caret(wanted_left > nchars ? nchars : wanted_left);

   // Always set right_caret; do NOT special-case range.high == range.low
   // here. When range.high == range.low, every token in the rev_loop
   // above satisfies either "q > range.high" or "q <= range.low" (the
   // two conditions become the same test), so the "else if (q >
   // range.low)" branch that accumulates len_between is never taken and
   // len_between is provably 0 -- giving right_caret == left_caret.
   // That is exactly the input Error::get_error_line_3() needs to decide
   // (via its own "diff <= 0" check) that only one caret should be
   // printed. Leaving right_caret unset (-1) here used to make that the
   // SAME decision by a different route (right_caret - left_caret then
   // being trivially negative) -- two independent "one caret vs two"
   // checks that happened to agree rather than one. See devel_doc/
   // Carets.txt, "Known gaps" #2.
   //
const int wanted_right = error.get_left_caret() + len_between;
   error.set_right_caret(wanted_right > nchars ? nchars : wanted_right);
}
//────────────────────────────────────────────────────────────────────────────
int
Executable::show_owners(const char * prefix, ostream & out,
                        const Value & value) const
{
int count = 0;

   loop (b, body.size())
      {
        const Token & tok = body[b];
        if (tok.get_ValueType() != TV_VAL)      continue;

        if (Value::is_or_contains(tok.get_apl_val().get(), &value))
           {
             out << prefix << get_name() << "[" << b << "]" << endl;
             ++count;
           }
      }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Executable::statement_text(Prefix_PC pc) const
{
   if (pc >= body.end_PC())
      {
        Assert(pc == body.end_PC());
        pc = body.end_PC() - 1;
      }

   if (pc > Prefix_PC_0 && body[pc].get_Class() == TC_RETURN)   --pc;
   if (pc > Prefix_PC_0 && body[pc].get_Class() == TC_END)      --pc;

const Function_Line line = get_line(pc);
const UCS_string & line_txt = get_body_text(line);

   // count statement ends before pc
   //
int stats_before = 0;
   for (Prefix_PC p = line_start(line); p < pc; ++p)
       {
         // a TOK_STOP_LINE is immediately followd by a TC_END, but the
         // TC_END has no ◊ in line_txt. We therefore discount stats_before.
         //
         if (body[p].get_tag() == TOK_STOP_LINE)          --stats_before;

         // TOK_IF_THEN/IF_ELSE/IF_END are TC_END too (conditional-branch
         // bookkeeping tokens synthesized for →→/←→/←← syntax) but have
         // no ◊ of their own in line_txt -- counting them here would
         // overcount relative to the ◊ scan below and desync tidx.
         if (body[p].get_Class() == TC_END &&
             body[p].get_tag() != TOK_IF_THEN &&
             body[p].get_tag() != TOK_IF_ELSE &&
             body[p].get_tag() != TOK_IF_END)             ++stats_before;
         else if (body[p].get_tag() == TOK_RETURN_EXEC)   ++stats_before;
       }

int tidx = 0;
   while(tidx < line_txt.ssize())
      {
        if (stats_before == 0)   break;
        if ( Avec::is_DIAMOND(line_txt[tidx++])   // ◊ xxx
           )   --stats_before;
      }

   // skip leading spaces
   //
   while (tidx < line_txt.ssize() && line_txt[tidx] == ' ')   ++tidx;

UCS_string ret;
   while (tidx < line_txt.ssize())
      {
        if (Avec::is_DIAMOND(line_txt[tidx]))    break;
        ret << line_txt[tidx++];
      }

   // skip trailing spaces
   //
   while (ret.size() && ret.back() <= ' ')   ret.pop_back();

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::unmark_all_values() const
{
   loop (b, body.size())
      {
        const Token & tok = body[b];
        if (tok.get_ValueType() == TV_VAL)
           {
             if (Value_P value = tok.get_apl_val())   value->unmark();
           }

        if (tok.get_ValueType() == TV_FUN)
           {
             cFunction_P fun = tok.get_function();
             const UserFunction * ufun = fun->get_func_ufun();
             if (ufun && ufun->is_lambda())
                {
                  ufun->unmark_all_values();
                }
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::clear_body()
{
   loop(b, body.size())
      {
        body[b].release_apl_val(LOC);

        if (body[b].is_function())
           {
             cFunction_P fun = body[b].get_function();

             // fun->get_func_ufun() below is virtual, but by the time
             // clear_body() runs from ~Workspace() during )OFF, the
             // statically-allocated primitive/system function singletons
             // (Bif_F12_PLUS::fun etc.) have already been destroyed --
             // they are constructed after, hence destroyed before, the
             // static Workspace. A user-defined function body can still
             // hold Function* tokens pointing at those dead singletons,
             // so a virtual call through fun would be UB (Blake McBride,
             // Bugs21 #3). is_user_defined() only reads the plain,
             // non-virtual Id set at construction time, so it is safe to
             // call here even if fun's own vtable is gone; only
             // UserFunction (which includes lambdas) is ever constructed
             // with ID_USER_SYMBOL, so primitives are correctly skipped.
             //
             if (fun->is_user_defined())
                {
                  const UserFunction * ufun = fun->get_func_ufun();
                  if (ufun && ufun->is_lambda())
                     const_cast<UserFunction *>(ufun)->decrement_refcount(LOC);
                }
             body[b].clear(LOC);
           }
      }

   body.clear();
}
//────────────────────────────────────────────────────────────────────────────
int
Executable::check_PCs(const char * loc) const
{
int errors = 0;
Function_PC left_PC = Function_PC_invalid;   // the PC of the statement END
bool leftmost = false;                       // the next token is leftmost
Function_PC prev = Function_PC_invalid;      // the PC of the token left of b

   // the body is a sequence of reversed statements, i.e. right-to-left,
   // each followed by a TC_END token (or TC_RETURN at the end of the body).
   // The END token is the left end of its statement and has the PC of the
   // leftmost token of the statement (or, for an empty statement, the PC of
   // the ◊ or end of line). We scan the body backwards, i.e. from the END of
   // a statement and then from its leftmost token to its rightmost token.
   // The PCs must therefore not decrease.
   //
   for (ShapeItem b = body.ssize() - 1; b >= 0; --b)
       {
         const Token_loc & tl = body[b];
         const Function_PC pc = tl.get_PC();
         const TokenClass tc = tl.get_Class();
         const char * problem = 0;
         if (pc < 0)                                  problem = "invalid PC";
         else if (tc == TC_END || tc == TC_RETURN)
            {
              left_PC = pc;   // the left end of a (reversed) statement
              leftmost = true;
              prev = Function_PC_invalid;
            }
         else if (leftmost && pc != left_PC)
              problem = "PC of the leftmost token differs from its END";
         else if (prev != Function_PC_invalid && pc < prev)
              problem = "PC smaller than the PC of the token left of it";

         if (tc != TC_END && tc != TC_RETURN)
            {
              leftmost = false;
              if (pc >= 0)   prev = pc;
            }
         if (problem == 0)   continue;

         if (errors++ == 0)
            CERR << "*** check_PCs() of " << get_name() << " ("
                 << loc << "):" << endl;
         CERR << "    body[" << b << "] " << tl.get_token() << " pc="
              << pc << ": " << problem << endl;
       }

   return errors;
}
//────────────────────────────────────────────────────────────────────────────
bool
Executable::compute_if_else_targets(Function_Line * bad_line,
                                    const char ** bad_cause)
{
#if cfg_ASSERT_LEVEL_WANTED >= 2
   Assert1(check_PCs(LOC) == 0);
#endif

   /*
      This function recursively matches corresponding TOK_IF_XXX tokens and
      sets their int values to the pc of their companion. In a single sided
      IF (i.e. without ELSE):

      the intval of the TOK_IF_THEN token is the PC after the TOK_IF_END.

      In a double sided IF/ELSE:

      the intval of the TOK_IF_THEN token is the PC after the TOK_IF_ELSE.
      the intval of the TOK_IF_ELSE token is the PC after the TOK_IF_END.

      In Prefix::reduce_END_B__() these intvals are used to quickly jump over
      the then resp. else clauses. All TOK_IF_XXX (as well as ◊ and \n)
      belong to token class TC_END and are essentially ◊ tokens with this
      additional jump information.
    */
   Log(LOG_IfElse)
      {
         CERR << "initial body at " << LOC ":" << endl;
         print_token(CERR, 3);
      }

const char * cause = "";
Function_Line cause_line = Function_Line_0;
Function_PC cause_low  = Function_PC_invalid;
Function_PC cause_high = Function_PC_invalid;

vector<conditional> conditionals;

   loop(b, body.size())
       {
         const Function_PC pc = Function_PC(b);
         switch(body[pc].get_tag())
            {
              case TOK_IF_THEN:   // →→ (always allowed).
                   Log(LOG_IfElse)
                      CERR << "SEE →→ (then)  at [" << pc << "]" << endl;
          
                   {
                     const conditional cond(pc);
                     conditionals.push_back(cond);
                   }
                   Log(LOG_IfElse)
                      {
                        if (pc > Function_PC_0 && body[pc-1].get_Class() == TC_END)
                           {
                             CERR << "\n*** WARNING: TC_END before TOK_IF_THEN"
                                  << endl;
                           }
                      }
                   continue;   // loop(b, body.size())
          
              case TOK_IF_ELSE:   // ←→
                   Log(LOG_IfElse)
                      {
                        CERR << "SEE ←→ (else)  at [PC=" << pc << "]" << endl;
                      }
          
                   if (conditionals.size() == 0)
                      {
                        cause = "←→ without →→ (aka. ELSE without IF)";
                        cause_line = get_line(Prefix_PC(pc));
                        cause_low = pc;
                        goto error;
                      }

                   if (conditionals.back().if_ELSE >= Function_PC_0)
                      {
                        cause = "duplicate ←→ (aka. duplicate ELSE)";
                        cause_line = get_line(Prefix_PC(pc));
                        cause_low = conditionals.back().if_ELSE;
                        cause_high = pc;
                        goto error;
                      }

                   conditionals.back().if_ELSE = pc;
                   Log(LOG_IfElse)
                      {
                         if (pc > Function_PC_0 && body[pc-1].get_Class() == TC_END)
                            {
                              CERR << "\n*** WARNING: TC_END before TOK_IF_ELSE"
                                   << endl;
                            }
                      }
                   continue;   // loop(b, body.size())
          
              case TOK_IF_END:    // ←←
                   Log(LOG_IfElse)
                      {
                        CERR << "SEE ←← (endif) at [" << pc << "]" << endl;
                      }
          
                   if (conditionals.size() == 0)
                      {
                        cause = "←← without →→ (aka. ENDIF without IF)";
                        cause_line = get_line(Prefix_PC(pc));
                        cause_low = pc;
                        goto error;
                      }
          
                   {
                     const conditional & cond = conditionals.back();
                    
                     Log(LOG_IfElse)
                       CERR << "cond[" << conditionals.size() << "]:"
                               "\n ├── then:  " << cond.if_THEN
                            << "\n ├── else:  " << cond.if_ELSE
                            << "\n └── endif: " << pc
                            << endl;
          
                     if (cond.if_ELSE == (cond.if_THEN + 1))   // empty THEN
                        {
                          cause = "empty →→ ... ←→ (aka. empty THEN)";
                          cause_line = get_line(Prefix_PC(pc));
                          cause_low = pc - 1;
                          cause_high = pc;
                          goto error;
                        }
          
                     if (pc == (cond.if_ELSE + 1))   // empty ELSE
                        {
                          cause = "empty ←→ ... ←← (aka. empty ELSE)";
                          cause_line = get_line(Prefix_PC(pc));
                          cause_low = pc - 1;
                          cause_low = pc - 1;
                          cause_high = pc;
                          goto error;
                        }
          
                     if (cond.if_ELSE < Function_PC_0)   // simple if ... endif
                        {
                          body[cond.if_THEN].set_int_val(pc + 1);
                        }
                     else                    // if ... else ... endif
                        {
                          body[cond.if_THEN].set_int_val(cond.if_ELSE + 1);
                          body[cond.if_ELSE].set_int_val(pc + 1);
                        }

                     Log(LOG_IfElse)
                        {
                          conditionals.back().print(CERR, conditionals.size(),
                                                    body);
                        }
                     conditionals.pop_back();
                   }

                   Log(LOG_IfElse)
                      {
                         if (pc > Function_PC_0 && body[pc-1].get_Class() == TC_END)
                            {
                              CERR << "\n*** WARNING: TC_END before TOK_IF_END"
                                   << endl;
                            }
                       }
                   continue;   // loop(b, body.size())
          
              default:
                   continue;   // loop(b, body.size())
            }
       }

   // at this point, all conditionals shoud have been processed.
   // If not, complain.
   //
   if (conditionals.size())
      {
        cause = "→→ without ←← (aka. IF without ENDIF)";
        cause_line = get_line(Prefix_PC(conditionals.back().if_THEN));
        cause_low = conditionals.back().if_THEN;
        goto error;
      }

   Log(LOG_IfElse)
      {
        CERR << "final body[" << body.size() << "]:" << endl;
        print_token(CERR, 3);
      }
   return false;   // OK.

error:
   // the caller reports the error (as a SYNTAX ERROR, or as the bad line
   // of ⎕FX), not this function on CERR (Bugs35 #53)
   //
   MORE_ERROR() << cause;
   if (bad_line)    *bad_line = Function_Line(cause_line);
   if (bad_cause)   *bad_cause = cause;

   Log(LOG_IfElse)
      {
        CERR <<  cause << ". First observed on line [" << cause_line << "]";
        if (cause_low  != Function_PC_invalid)   CERR << ", PC=" << cause_low;
        if (cause_high != Function_PC_invalid)   CERR << "-" << cause_high;
        CERR << "." << endl;
        CERR << "incorrect body at " <<  LOC "]:" << endl;
        print_token(CERR, 3);
      }
   return true;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::decrement_refcount(const char * loc)
{
UserFunction * ufun = get_exec_ufun();
   Assert1(ufun);
   Assert(ufun->is_lambda());

   if (refcount <= 0)
      {
        CERR << "*** Warning: refcount of " << get_name() << " is " << refcount
             << ":" << endl;
        print_text(CERR);
        FIXME;
      }

   --refcount;

// CERR << "*** decrement_refcount() of " << get_name()
//      << " to " << refcount << " at " << loc << endl;


   if (refcount <= 0)
      {
//      CERR << "*** lambda died" << endl;
//      clear_body();
        // Not deleted right away: the tokens of the immediate-execution
        // statement currently in flight may still hold a raw Function*
        // to this same lambda (e.g. (⎕EX'L')L 1, Bugs28 #15 -- the same
        // use-after-free class as Symbol::expunge()'s non-lambda case,
        // just reached via refcount dropping to 0 here instead of a
        // direct delete). Queue it on the same pre-existing expunged-
        // functions mechanism Symbol::expunge() now uses -- see that
        // call site's comment for why it, not a fresh delete, is safe.
        //
        Workspace::add_expunged_function(ufun);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::increment_refcount(const char * loc, const char * creator)
{
const UserFunction * ufun = get_exec_ufun();
   Assert1(ufun);
   if (!ufun->is_lambda())
      {
        MORE_ERROR() << "Attempt (of " << creator <<
                        ") to create or edit a lambda. Use { ... } instead.";
        DEFN_ERROR;
      }

   ++refcount;

// CERR << "*** increment_refcount() of " << get_name()
//      << " to " << refcount << " at " << loc << endl;A
}
//────────────────────────────────────────────────────────────────────────────
VoidCount
Executable::remove_TOK_VOID()
{
   return body.remove_TOK_VOID();
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::conditional::print(ostream & out, int level,
                               const Token_string & body) const
{
int endif_PC;
   out << "COND[" << level << "] IF ... THEN@PC=" << if_THEN;
   Assert1(body[if_THEN].get_tag() == TOK_IF_THEN);
   if (if_ELSE < Function_PC_0)   // single sided if/endif
      {
        endif_PC = body[if_THEN].get_int_val() - 1;
      }
   else               // double sided if/else/endif
      {
        out << " ELSE@PC=" << if_ELSE;
        Assert1(body[if_ELSE].get_tag() == TOK_IF_ELSE);
        endif_PC = body[if_ELSE].get_int_val() - 1;
      }

   Assert1(body[endif_PC].get_tag() == TOK_IF_END);
   out << " ENDIF@PC=" << endif_PC << endl;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Executable::extract_lambda_text(Fun_signature signature, int skip) const
{
   // Skip over 'skip' top -level lambdas and return the next one. 

   // this->text is the entire text of this Executable. Extract the text of
   // the skip'th top-level lambda in this->text (and return it).
   //
UCS_string lambda_text;

// level is used to skip over nested lambdas (= lambdas in a lambda body)
int level = 0;   // { } nesting level

// copying is false for the first 'skip' lambdas, and true after that.
bool copying = false;

ShapeItem tidx = 0;    // the current line in text[]
ShapeItem tcol = 0;    // the current column in text[tidx];

   // skip over the first skip lambdas and copy the next one to lambda_text
   //
   for (;;)
       {
         if (tidx >= text.ssize())
            {
              // this should never happens since we have 'skip' lambdas.
              Q1(copying)   Q1(tidx)   Q1(skip)   FIXME;
            }

         const UCS_string & line = text[tidx];

         if (tcol >= line.ssize())   // end of line: wrap to next line
            {
              ++tidx;     // next line
              tcol = 0;   // first column
              continue;
            }

         const Unicode uni = line[tcol++];
         if (uni == UNI_SINGLE_QUOTE ||   // standard APL 'string'
             uni == UNI_DOUBLE_QUOTE ||   // GNU APL "string"
             uni == UNI_LEFT_DAQ)         // GNU APL «string»
            {
              const ShapeItem end = line.string_end_pos(tcol - 1, false);
              if (end == -1)
                 {
                   MORE_ERROR() << "No (APL-) string end in function line "
                                << tidx;
                   DOMAIN_ERROR;
                 }

              if (copying)
                 {
                   lambda_text << uni;
                   while (tcol < end + 1)   lambda_text << line[tcol++];
                 }
              tcol = end + 1;
              continue;
            }

         // at this point uni is outside strings
         //
         if (uni == UNI_COMMENT || uni == UNI_NUMBER_SIGN)
            {
              ++tidx;
              tcol = 0;
              continue;
            }

         if (copying)   lambda_text << uni;

         switch(uni)
            {
              case UNI_L_CURLY:            // start of a new { ... }
                   if (level++ == 0 && skip == 0)   copying = true;
                   continue;

              case UNI_R_CURLY:            // end of { ... }
                   if (--level)   continue;      // but not of a top-level }
                   --skip;                       // one less to skip
                   if (!copying)  continue;

                   // this } is the 
                   lambda_text.pop_back();       // the top-level }
                   goto out;                     // copying done.

              default: continue;
            }
       }

out:

   // if thew lambda has no results, then we are done.
   //
   if (!(signature & SIG_Z))   return lambda_text;

   // Otherwise: insert λ ←. The λ ← shall be inserted before the last
   // statement (= after the last ◊), or at the beginning if there is only
   // one statement
   //
ShapeItem sols = 0;   // start-of-last-statement; assume no ◊.
   rev_loop(j, lambda_text.size())
      {
        if (Avec::is_DIAMOND(lambda_text[j]))
           {
             sols = j + 1;
             // currently unreachable (SIG_Z implies non-whitespace
             // follows the last ◊), but bound defensively in case a
             // future change to lambda parsing makes it reachable.
             while (sols < lambda_text.ssize() &&
                    lambda_text[sols] <= UNI_SPACE)   ++sols;
             break;
           }
      }

UCS_string ret;
   loop(j, lambda_text.size())
       {
         if (j == sols)   ret << UNI_LAMBDA << UNI_LEFT_ARROW;   // insert λ←
         ret << lambda_text[j];
       }
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
/// throw DEFN_ERROR for a lambda rejected by compute_lambda_body() below
/// (Bugs28 #94, Bugs27 #49 residual). The raw DEFN_ERROR macro
/// (throw_apl_error()) calls Error::update_error_info(), which rebuilds
/// the error's line 2/3 from whatever SI frame is current -- at this
/// point (still inside StatementList::fix(), before the lambda's own
/// statement has an SI entry of its own) that is the *previous*
/// statement's frame, stale and unrelated (the same update_error_info()
/// limitation Bugs28 #93 fixed for parse errors). update_error_info()
/// also prints immediately, and the caller
/// (Command::do_APL_expression()) prints the same error again in its
/// own catch block, doubling it. Store directly and throw without ever
/// calling update_error_info(), leaving line 2/3 empty (the same as a
/// fresh interpreter with no previous statement to leak from) instead
/// of wrong, and printed exactly once.
static void
throw_lambda_defn_error()
{
Error error(E_DEFN_ERROR, LOC);
   if (StateIndicator * si = Workspace::SI_top())
      StateIndicator::get_error(si) = error;
   throw error;
}
//────────────────────────────────────────────────────────────────────────────
Fun_signature
Executable::compute_lambda_body(Token_string & lambda_body,
                                ShapeItem b, ShapeItem bend,
                                Function_PC pc_L, Function_PC pc_R)
{
   /* The PCs of the lambda body refer to the tokens of the lambda text
      (see extract_lambda_text()), which is the text between { and } with
      λ← inserted before the last statement. A token with PC p in this
      body gets PC p - pc_L - 1 in the lambda body, plus 2 if it is
      located after the inserted λ←.
    */
int signature = SIG_FUN;
int level = 0;

   /* find the insertion point for λ←. For a single statement lambda the
      insertion point is the first body token lambda_body[b].
      For a multi-statement lambda it is the last ◊ before lambda_body[bend].
    */
ShapeItem insertion_point = b;
   for (ShapeItem j = b; j < bend; ++j)
       {
         if (body[j].get_tag() == TOK_END)
            {
              // TOK_END indicates a multi-statement lambda. Move the
              // insertion_point to the (beginning of) the next statement.
              insertion_point = j + 1;
            }
       }

   // at_stmt_start tracks whether the next token is statement-initial (true at
   // the very first token, and after every TOK_END).  A statement-initial
   // TOK_R_ARROW is a branch-as-statement (→N) which is illegal in a lambda.
   // A mid-statement TOK_R_ARROW is the supported dyadic form A→B.
   bool at_stmt_start = true;

   for (; b < bend; ++b)
       {
         Token_loc t;
         t.move_from(body[b], LOC);
         body[b].clear(LOC);   // invalidate in main body

         // drop empty statements, e.g. {⋄} or {A⋄⋄B}: their TOK_END has
         // no valid PC in the lambda body (Bugs37 #45).
         //
         if (t.get_tag() == TOK_END && at_stmt_start)   continue;

         // figure the signature by looking for ⍺, ⍶, ⍵, ⍹, and χ
         // and complain about ◊ and →
         switch(t.get_tag())
            {
              case TOK_ALPHA:   if (!level)   signature |= SIG_A;   // no break
              case TOK_OMEGA:   if (!level)   signature |= SIG_B;         break;
              case TOK_CHI:     if (!level)   signature |= SIG_X;         break;
              case TOK_OMEGA_U: if (!level)   signature |= SIG_RO;  // no break
              case TOK_ALPHA_U: if (!level)   signature |= SIG_LO;        break;

              // TOK_DIAMOND is dead code here: parse_body_line() converts
              // diamonds inside {} to TOK_END before compute_lambda_body()
              // runs.  Keep the case as documentation.
              case TOK_DIAMOND:
              // TOK_BRANCH_INT is the resolved branch token.  Always illegal.
              case TOK_BRANCH_INT:
                   MORE_ERROR() << "→ is not allowed in λ expression ";
                   throw_lambda_defn_error();

              // TOK_R_ARROW is what the tokenizer produces for →N when
              // parse_body_line() split statements at a diamond inside {}.
              // When statement-initial it is a branch (→N) and must be
              // rejected.  When mid-statement it is the supported dyadic
              // form A→B (conditional branch with a value).
              case TOK_R_ARROW:
                   if (at_stmt_start)
                      {
                        MORE_ERROR() << "→ is not allowed in λ expression ";
                        throw_lambda_defn_error();
                      }
                   break;

              // TOK_END marks a statement boundary inside a multi-statement
              // lambda; the next token is statement-initial.
              case TOK_END:
                   at_stmt_start = true;
                   break;

              case TOK_ESCAPE:
              case TOK_IF_THEN:
              case TOK_IF_ELSE:
              case TOK_IF_END:
                   MORE_ERROR() << "conditionals (i.e. ←←, ←→, or →→)"
                                   " are not allowed in λ expressions";
                   throw_lambda_defn_error();

              case TOK_L_CURLY: ++level;   break;
              case TOK_R_CURLY: --level;   break;
              default: break;
            }
         at_stmt_start = (t.get_tag() == TOK_END);

         const Function_PC pc_t = Function_PC(t.get_PC() - pc_L - 1);
         if (b == insertion_point)
            {
              // insert  λ ← tokens
              //
              Symbol * sym_Z = &Workspace::get_v_LAMBDA();
              lambda_body.emplace_back(Token(TOK_LAMBDA, sym_Z), pc_t);
              lambda_body.emplace_back(Token(TOK_ASSIGN1),
                                       Function_PC(pc_t + 1));

              signature |= SIG_Z;
            }

         lambda_body.emplace_back(t.get_token(),
                                  Function_PC(signature & SIG_Z ? pc_t + 2
                                                                : pc_t));
       }

   // the end of the lambda text (the PC of the closing } in lambda text)
   //
const Function_PC pc_end = Function_PC(pc_R - pc_L - 1 +
                                       (signature & SIG_Z ? 2 : 0));

   if ((signature & SIG_B) == 0 &&   // niladic
       (signature & (SIG_A | SIG_LO | SIG_RO | SIG_X)))
      {
        MORE_ERROR() <<
           "niladic lambda with axis. left argument, or function argument(s)";
        throw_lambda_defn_error();
      }

   // operators with axis are fine, monadic (like ∇Z←(F OPX)[X] B) and
   // dyadic (like ∇Z←(F OPX G)[X] B): lambdas behave like defined
   // functions (Bugs35 #44, D15)

   // if the lambda has at least one token, then it supposedly returns λ.
   // Otherwise the lambda is empty (and result-less)
   //
   if (signature & SIG_Z)   // result
      {
        const int64_t trace = 0;
        lambda_body.emplace_back(Token(TOK_ENDL, trace), pc_end);

        Token ret_lambda(TOK_RETURN_SYMBOL, &Workspace::get_v_LAMBDA());
        lambda_body.emplace_back(ret_lambda, pc_end);
      }
   else
      {
        Token ret_void(TOK_RETURN_VOID);
        lambda_body.emplace_back(ret_void, pc_end);
      }

   return Fun_signature(signature);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Executable::parse_body_line(Function_Line line, const UCS_string & ucs_line,
                            bool trace, const char * loc, bool macro,
                            Function_PC * line_end_PC)
{
   Log(LOG_UserFunction__set_line)
      CERR << "[" << line << "]" << ucs_line << endl;

Token_string in;
const Parser parser(get_parse_mode(), loc, macro);
Function_PC end_PC = Function_PC_0;
   if (const ErrorCode ec = parser.parse(ucs_line, in, true, &end_PC))
      {
        if (ec == E_NO_TOKEN)
           {
             Error error(ec, LOC);
             throw error;
           }

        if (ec != E_NO_ERROR)   Error::throw_parse_error(ec, LOC, LOC);
      }

   if (line_end_PC)   *line_end_PC = end_PC;
   return parse_body_line(line, in, end_PC, trace, loc);
} 
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Executable::parse_body_line(Function_Line line, const Token_string & input,
                            Function_PC end_PC, bool trace, const char * loc)
{
ShapeItem idx = 0;
const ShapeItem end = input.size();

   Log(LOG_UserFunction__set_line)
      {
        CERR << "[input line " << line << "]: ";
        input.print(CERR, false);
      }

   // handle label (if any)
   //
   if (get_parse_mode() == PM_FUNCTION &&   // defined function
       end >= 2                        &&   // at least 2 tokens
       input[1].get_tag() == TOK_COLON &&   // second token is :
       input[0].get_tag() == TOK_SYMBOL)    // first token is (label-) name
      {
        Token tok_sym = input[0];   // get the label symbol
        idx = 2;                    // skip label :

        UserFunction * ufun = get_exec_ufun();
        Assert(ufun);

        // ISO 13751 / APL2 LRM: header names (Z, A, LO, FUN, RO, X, B, and
        // locals) and labels must all be distinct (Blake McBride, Bugs28
        // #70). Without this check a label duplicating a header name
        // silently shadowed it (e.g. 'B:' makes B the label's line number
        // instead of the argument), and a label duplicating an earlier
        // label just silently rebound it to the new line -- both accepted
        // at fix/edit time instead of DEFN ERROR.
        //
        // throw_parse_error(), not DEFN_ERROR: the latter reported the
        // error on the session while ⎕FX returned its row anyway (Bugs35
        // #54). The function name is a header name too.
        //
        Symbol * label_sym = tok_sym.get_sym_ptr();
        if (ufun->pushes_sym(label_sym) ||
            label_sym == ufun->get_header().FUN())
           {
             MORE_ERROR() << "Label " << label_sym->get_name()
                          << " duplicates a header name";
             Error::throw_parse_error(E_DEFN_ERROR, LOC, LOC);
           }
        if (ufun->is_label(label_sym))
           {
             MORE_ERROR() << "Label " << label_sym->get_name()
                          << " is already declared";
             Error::throw_parse_error(E_DEFN_ERROR, LOC, LOC);
           }

        ufun->add_label(label_sym, line);
      }

   /*
       Convert (APL order) :

       ┌───┐ ┌───┐ ┌───┐     ┌───┐ ┌─────┐ 
       │ A │ │ B │ │ C │ ... │ Z │ │ END │ 
       └───┘ └───┘ └───┘     └───┘ └─────┘ 

       To (reverse order) :

       ┌───┐     ┌───┐ ┌───┐ ┌───┐ ┌─────┐ 
       │ Z │ ... │ C │ │ B │ │ A │ │ END │ 
       └───┘     └───┘ └───┘ └───┘ └─────┘ 

       The END token may be missing.
    */

Token_string output;   // in reverse order
   while (idx < end)   // loop over one line
      {
        // 1. determine the number of tokens (excluding ◊) in the statement
        //    that starts at idx. The END token may be missing and in that case
        //    TOK_END is appended.
        //
        ShapeItem stat_len = 0;   // statement length, not counting ◊
        int diamond_len    = 0;   // ◊ length (if any)
        TokenTag tag_end   = TOK_END;
        int64_t tr         = trace ? 1 : 0;
        loop(t, end - idx)   // loop over one statement, starting at input[idx]
            {
              const Token & tok = input[idx + t];
              const TokenTag tag = tok.get_tag();
              if (tag == TOK_DIAMOND)   // ◊ is allowed (and discarded)
                 {
                   diamond_len = 1;
                   break;
                 }
              else if (tok.is_COND())   // TOK_IF_xxx
                 {
                   tr = -2;
                   diamond_len = 1;
                   tag_end = tok.get_tag();
                   break;
                 }
              else if (tok.get_Class() >= TC_MAX_PERM &&   // bad class,
                       tag != TOK_L_CURLY             &&   // but allow {
                       tag != TOK_R_CURLY)                 // and allow }
                 {
                   // e.g. the : of a second label. Explain it in )MORE,
                   // unless )MORE already has some other information.
                   //
                   if (!quiet_parse && Workspace::more_error().size() == 0)
                      {
                        std::ostringstream out;
                        out << "Line " << line << ": offending token "
                            << tok << " (tag " << tag << ") in statement: ";
                        input.print(out, /* details */ 0);
                        MORE_ERROR() << UCS_string(UTF8_string(out.str().c_str()));
                      }

                   return E_SYNTAX_ERROR;
                 }

              // none of the above: count and continue;
              ++stat_len;
            }

        // 2. copy input (in APL order) to  output (in reverse order)
        //
        for (ShapeItem j = idx + stat_len - 1; j >= idx; --j)
            {
              output.push_back(input[j]);
            }
        Assert(stat_len + diamond_len);

        // 3. maybe add a TC_END token. In the body the END token follows
        //    the leftmost token of the (reversed) statement, i.e. it is the
        //    left end of the statement (in APL order). It therefore gets the
        //    PC of the leftmost token of the statement (its neighbour in the
        //    body). An empty statement has no tokens and its END gets the
        //    PC of the ◊ or COND token that ends it, or else the PC of the
        //    end of the line.
        //
        const Function_PC pc_end = diamond_len ? input[idx + stat_len].get_PC()
                                               : end_PC;
        if (stat_len == 0)   // empty statement
           {
             if (tag_end != TOK_END)
                {
                  // the COND token replaces the END token of the previous
                  // statement (if any) and is then the left end of it.
                  //
                  if (output.size() && output.back().get_tag() == TOK_END)
                     {
                       const Token_loc prev_end(Token(tag_end, tr),
                                                output.back());
                       output.back().copy(prev_end, LOC);
                     }
                  else
                     {
                       output.emplace_back(Token(tag_end, tr), pc_end);
                     }
                }
              ++idx;
           }
        else                 // non-empty statement
           {
             // output.back() is the leftmost token of the statement
             //
             const Token_loc stat_end(Token(tag_end, tr), output.back());
             if (tr < 0 || diamond_len || get_parse_mode() != PM_EXECUTE)
                {
                  output.push_back(stat_end);
                }

             idx += stat_len + diamond_len;   // skip statement and (if any) ◊
           }
      }   // end of loop over statements of one line

   // replace the trailing TOK_END (if any) with TOK_ENDL
   //
   if (output.size() && output.back().get_tag() == TOK_END)
      {
        output.back().ChangeTag(TOK_ENDL);
       }

   Log(LOG_UserFunction__set_line)
      {
        CERR << "[final line " << line << "]: ";
        output.print(CERR, false);
      }

   loop(t, output.size())   body.push_back(output[t]);
   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::setup_lambdas()
{
   // quick check if this body contains any lambdas. Parser::match_par_bra()
   // should have checked for unbalanced { } so we simply look for } (which
   // is the first to occur since the body is reversed.
   //
   if (!body_has_curly())   return;   // no { ... } in this body

   // undo the token reversion of the body so that the body token and the
   // function text run in the same direction.
   //
   reverse_each_statement(body);

Lambda_number lambda_num = LAMBDA_NUM_0;
   loop(b, body.size())
       {
         if (body[b].get_tag() != TOK_L_CURLY)   continue;   // not {

         ShapeItem end = -1;
         int curly_level = 1;   // since body[b] is {
         for (ShapeItem b1 = b + 1; b1 < ShapeItem(body.size()); ++b1)
             {
               switch(body[b1].get_tag())
                  {
                    case TOK_L_CURLY:   // opening {
                         ++curly_level;
                         continue;

                    case TOK_R_CURLY:   // closing }
                         if (0 == --curly_level)
                            {
                              end = b1;
                              b1 = body.size();
                            }
                         continue;

                    case TOK_MARKER:   // multi-line string or literal
                         MORE_ERROR() << "Invalid multi-line string or "
                                         "literal in (single-line) lambda.";
                         DEFN_ERROR;
                         continue;

                    default:
                         continue;
                  }
             }
         Assert(end != -1);
         lambda_num = Lambda_number(lambda_num + 1);
         b = setup_one_lambda(b, end, lambda_num);
       }

   // redo the token reversion of the body so that the body token and the
   // text run in the same direction.
   //
   reverse_each_statement(body);

   // remove the TOK_VOIDs left where the lambda texts were BEFORE the
   // offsets of matching ( ) and [ ] are computed, or the offsets of the
   // pairs that enclose a lambda are too large (Bugs35 #74)
   //
   remove_TOK_VOID();
   Parser::match_par_bra(body, true);
   set_END_PCs();
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::set_END_PCs()
{
   for (ShapeItem b = 1; b < body.ssize(); ++b)
       {
         const TokenClass tc = body[b].get_Class();
         if (tc != TC_END && tc != TC_RETURN)   continue;

         const TokenClass prev = body[b - 1].get_Class();
         if (prev == TC_END || prev == TC_RETURN)   continue;   // empty

         const Token_loc tl(body[b].get_token(), body[b - 1]);
         body[b].copy(tl, LOC);
       }
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
Executable::setup_one_lambda(ShapeItem b, ShapeItem bend,
                             Lambda_number lambda_num)
{
   // called with this->body in forward (APL-) order, i.e  { ... }
   //
   Assert(body[b].get_tag()    == TOK_L_CURLY);
   Assert(body[bend].get_tag() == TOK_R_CURLY);

const Function_PC pc_L = body[b].get_PC();      // the PC of {
const Function_PC pc_R = body[bend].get_PC();   // the PC of }
   body[b++].clear(LOC);    // invalidate the leading  {
   body[bend].clear(LOC);   // invalidate the trailing }

Token_string lambda_body;
const Fun_signature signature = compute_lambda_body(lambda_body, b, bend,
                                                    pc_L, pc_R);

   /* strip off the trailing local variables.

      The function body ends with ENDL RETURN.
      To strip off a local variable we have to:

      body[-4]        body[-3]        body[-2]        body[-1] 
      ─────────────────────────────────────────────────────────────
      ';'             SYMBOL          ENDL            RETURN
                                      │               │
      ┌─────────────── ← ─────────────┘               │
      │               ┌─────────────── ← ─────────────┘
      │               │
      ENDL            RETURN
    */
vector<Symbol *> local_vars;   // collector for local variables
   while (lambda_body.size() >= 4)
      {
        const size_t semi = lambda_body.size() - 4;   // maybe ';' SYMBOL ?
        if (lambda_body[semi]  .get_tag() != TOK_SEMICOL)   break;   // no
        if (lambda_body[semi+1].get_Class() != TC_SYMBOL)   break;   // no
        local_vars.push_back(lambda_body[semi + 1].get_sym_ptr());

        // the local variables are moved from the lambda text to the
        // header. ENDL is the left end of the last statement (and keeps
        // its PC), while RETURN (an empty statement) gets the PC of the ;
        //
        const Function_PC pc_semi = lambda_body[semi].get_PC();
        const Token_loc endl(lambda_body[semi + 2].get_token(),
                             lambda_body[semi + 2]);
        const Token_loc ret(lambda_body[semi + 3].get_token(), pc_semi);
        lambda_body[semi]    .copy(endl, LOC);
        lambda_body[semi + 1].copy(ret, LOC);
        lambda_body.resize(semi + 2);   // leave ENDL and RETURN_SYMBOL
      }

const UCS_string lambda_text = extract_lambda_text(signature, lambda_num - 1);

   reverse_each_statement(lambda_body);
   reverse_all_tokens(lambda_body);

UserFunction * ufun = new UserFunction(signature, lambda_num,
                                       lambda_text, lambda_body, local_vars);

   ufun->increment_refcount(LOC, "λ" );

   // put a token for the lambda at the place where the { was.
   // That replaces, for example, (in forward notation):
   //
   // A←{ ... }   by:
   // A←UFUN      UFUN being a user-defined function with body { ... }
   //
Token tok_ufun = ufun->get_token();
   body[bend].copy(Token_loc(tok_ufun, pc_L), LOC);   // the PC of {

   return bend;
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::reverse_all_tokens(Token_string & tos)
{
   // on an empty tos both initialisers below are UB (&tos[0] is out of
   // range, and tos.size() - 1 underflows) before the t1 < t2 guard can
   // help (Blake McBride, Bugs17 #6, related finding).
   if (tos.size() == 0)   return;

   for (Token_loc * t1 = &tos[0], * t2 = &tos[tos.size() - 1]; t1 < t2;)
       t1++->swap(*t2--);
}
//────────────────────────────────────────────────────────────────────────────
void
Executable::reverse_each_statement(Token_string & tos)
{
const ShapeItem last = tos.size();
   for (ShapeItem from = 0, to = 1; to < tos.ssize(); ++to)
       {
         if (tos[to].get_Class() == TC_END)
            {
              // found ia TC_END token within tos. Reverse the tokens
              // from...to to - 1, but leave the TC_END token as is.
              //
              if (from != to)   tos.reverse_from_to(from, to - 1);
              from = to + 1;   // continue after the TC_END token.
            }
         else if (to == (last - 1))
            {
              // reached the end of tos. from...to is the last statement
              // in tos. revert then entire statement (no TC_END to preserve).
              //
              tos.reverse_from_to(from, to);
              break;
            }
       }
}
//════════════════════════════════════════════════════════════════════════════
ExecuteList *
ExecuteList::fix(const UCS_string & data, const char * loc)
{
   // clear any errors that may have occurred before
   //
   if (Error * err = Workspace::get_error())   err->clear_error_code();

ExecuteList * fun = new ExecuteList(data, loc);

   Log(LOG_UserFunction__fix)
      {
        CERR << "fix pmode=execute list:" << endl << data
             << " addr " << voidP(fun) << endl
             << "------------------- ExecuteList::fix() --" << endl;
      }

   if (Error * err = Workspace::get_error())
      {
        if (err->get_error_code())
           {
             Log(LOG_UserFunction__fix)
                {
                   CERR << "fix pmode=execute list failed with error "
                        << Error::error_name(err->get_error_code()) << endl;
                }
     
             err->set_parser_loc(0);
             delete fun;
             return 0;
           }
      }

Function_PC end_PC = Function_PC_0;   // the PC of the end of data
   try
      {
        fun->parse_body_line(Function_Line_0, data, false, loc, false,
                             &end_PC);

        // inside the try: a lambda rejected at fix time (e.g. {→0}) throws
        // DEFN ERROR; fun (and the values in its body) leaked, and the
        // error was not reported (Bugs35 #73)
        fun->setup_lambdas();
      }
   catch (const Error & err)
      {
        if (Error * werr = Workspace::get_error())   *werr = err;

        delete fun;
        return 0;
      }

   Log(LOG_UserFunction__fix)
      {
        CERR << "fun->body.size() is " << fun->body.size() << endl;
      }

   // for ⍎ we do not append TOK_END, but only TOK_RETURN_EXEC.
   //
   fun->body.append_statement_end(Token(TOK_RETURN_EXEC), end_PC);
   if (fun->compute_if_else_targets())   // unbalanced →→ ←→ ←←
      {
        // a SYNTAX ERROR like any other parse error (Bugs35 #53)
        delete fun;
        try   { Error::throw_parse_error(E_SYNTAX_ERROR, LOC, LOC); }
        catch (const Error & err)
           {
             if (Error * werr = Workspace::get_error())   *werr = err;
           }
        return 0;
      }

   Log(LOG_UserFunction__fix)   fun->print(CERR);
   return fun;
}
//════════════════════════════════════════════════════════════════════════════
StatementList *
StatementList::fix(const UCS_string & data, Value_P literal, const char * loc)
{
StatementList * fun = new StatementList(data, loc);

   Log(LOG_UserFunction__fix)
      {
        CERR << "fix pmode=statement list:" << endl << data << endl
             << " addr " << voidP(fun) << endl
             << "------------------- StatementList::fix() --" << endl;
      }

   if (Error * err = Workspace::get_error())   err->set_parser_loc(0);

Function_PC end_PC = Function_PC_0;   // the PC of the end of data
   try
      {
        fun->parse_body_line(Function_Line_0, data, false, loc, false,
                             &end_PC);
        fun->setup_lambdas();
      }
   catch (Error & e)
      {
         if (e.get_error_code() == E_UNBALANCED_L_CURLY)
            {
              int curly_level = 0;
              loop(d, data.size())
                  {
                    const Unicode uni = data[d];
                    if (uni == UNI_L_CURLY)        // opening {
                       {
                         ++curly_level;
                       }
                    else if (uni == UNI_R_CURLY)   // closing }
                       {
                         --curly_level;
                       }
                    else if (uni == UNI_COMMENT)
                      {
                        MORE_ERROR() << "Invalid comment (⍝) in { ... }";
                      }
                    else if (uni == UNI_NUMBER_SIGN)
                      {
                        MORE_ERROR() << "Invalid comment (#) in { ... }";
                      }
                  }
              if (curly_level < 0)          // too many }
                 MORE_ERROR() << "Unbalanced (too many) }";
              else if (curly_level > 0)     // too many {
                 MORE_ERROR() << "Unbalanced (too many) {";
            }

        Log(LOG_UserFunction__fix)
           CERR << "parse_body_line(line 0) failed" << endl;
        delete fun;
        throw e;
      }

   Log(LOG_UserFunction__fix)
      {
        CERR << "fun->body.size() is " << fun->body.size() << endl;
      }

   if (literal)
      {
        bool found_marker = false;
        loop(b, fun->body.size())
            {
              if (fun->body[b].get_tag() == TOK_MARKER)
                 {
                   found_marker = true;
                   fun->body[b].copy(Token_loc(Token(TOK_APL_VALUE1, literal),
                                                fun->body[b].get_PC()), LOC);
                   break;
                 }
            }
         Assert(found_marker);
       }

   // for ◊ we do not append TOK_END, but only TOK_RETURN_STATS.
   //
   fun->body.append_statement_end(Token(TOK_RETURN_STATS), end_PC);
   if (fun->compute_if_else_targets())   // unbalanced →→ ←→ ←←
      {
        // a SYNTAX ERROR like any other parse error (Bugs35 #53)
        delete fun;
        Error::throw_parse_error(E_SYNTAX_ERROR, LOC, LOC);
      }

   Log(LOG_UserFunction__fix)   fun->print(CERR);
   return fun;
}
//════════════════════════════════════════════════════════════════════════════
