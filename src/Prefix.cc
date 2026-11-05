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

#include "Avec.hh"
#include "Bif_F1_EXECUTE.hh"
#include "Bif_OPER2_RANK.hh"
#include "Bif_OPER2_POWER.hh"
#include "Common.hh"
#include "DerivedFunction.hh"
#include "Executable.hh"
#include "IndexExpr.hh"
#include "LvalCell.hh"
#include "PointerCell.hh"
#include "Prefix.hh"
#include "StateIndicator.hh"
#include "Symbol.hh"
#include "UserFunction.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

uint64_t Prefix::instance_counter = 0;

//════════════════════════════════════════════════════════════════════════════
Prefix::Prefix(StateIndicator & _si, const Token_string & _body)
   : instance(++instance_counter),
     si(_si),
     put(0),
     saved_MISC(Token(TOK_VOID), Function_PC_invalid),
     lookahead_PC(Prefix_PC_invalid),
     body(_body),
     PC(Prefix_PC_0),
     assign_state(ASS_none),
     action(RA_FIXME),
     escape_pending(false)
{
}
//────────────────────────────────────────────────────────────────────────────
Function_PC
Prefix::get_failed_function() const
{
   // GNU APL used to place the carets under the entire failed phrase
   // (supposedly the old APL68000 behavior. IBM APL2 places the right caret
   // under the failed function or operator. This function fixes the old
   // GNU APL behavior so that it now follows IBM APL2.
   //
   // the scan starts at the rightmost (in APL order) item of the reduced
   // phrase, i.e. B = at(prefix_len - 1) = content[put - prefix_len], and
   // moves left (towards at0()). It must not start further right (at
   // content[0]) since the items below the phrase are right of it and were
   // not yet reduced.
   //
const int B = (prefix_len > 0 && prefix_len <= put) ? put - prefix_len : 0;

   // an operator, e.g. f OP2 g. Otherwise the right caret would be placed
   // under a function argument of the operator rather than under the
   // operator itself.
   //
   for (int s = B; s < put; ++s)   // from right to left (in APL order)
       {
         if (is_operator_class(content[s].get_Class()))
            {
              return content[s].get_PC();
            }
       }

   // a function, →, or ← (the essence of A F B, F B, → B, A ← B, ...)
   //
   for (int s = B; s < put; ++s)   // from right to left (in APL order)
       {
         enum { BITS = 1 << TC_FUN0 | 1 << TC_FUN2 | 1 << TC_R_ARROW |
                       1 << TC_ASSIGN };
         if (BITS & 1 << content[s].get_Class())
            {
              return content[s].get_PC();
            }
       }

   // no function or operator: the rightmost item of the phrase
   //
   return put ? content[B].get_PC() : get_phrase_low();
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::neutralize_lval_values()
{
   loop(s, ssize())
      {
        // must not throw (called while an error is being handled):
        // is_apl_val() first, since get_apl_val() throws VALUE ERROR for
        // a value token without a value (e.g. the result of ⍎'')
        //
        const Token & tok = at(s).get_token();
        if (tok.get_Class() != TC_VALUE || !tok.is_apl_val())   continue;
        Value_P val = tok.get_apl_val();
        if (val && val->is_left_value())   val->neutralize_lval_cells(LOC);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::clean_up()
{
   loop(s, ssize())
      {
        const Token & tok = at(s).get_token();
        if (tok.get_ValueType() == TV_INDEX)   delete &tok.get_index_val();
        at(s).copy(Token_loc(), LOC);   // release the value (if any)
      }

   put = 0;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reset(const char * loc)
{
   clean_up();
   put = 0;
   assign_state = ASS_none;
   clear_MISC(loc);
   lookahead_PC = Prefix_PC_invalid;
   prefix_len = 0;

   // The statement being reset here is abandoned outright -- e.g. by
   // A→B (including the same-line "0→COND" retry idiom, see apl.texi's
   // "Dyadic A→B" section), which never lets the statement reach
   // StateIndicator::statement_result() (the normal end-of-statement
   // path, and otherwise the only place fun_oper_cache is reset). Any
   // DerivedFunction slot (e.g. from f⍨, f¨, f.g) allocated while
   // evaluating the now-discarded statement would otherwise never be
   // freed: a tight A→B retry loop containing such an operator leaks
   // one slot per iteration, growing fun_oper_cache without bound,
   // regardless of the statement's own, genuinely small operator count
   // (reported by David Alden, 2026-08-19).
   //
   si.fun_oper_cache.reset();
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::uses_function(const UserFunction * ufun) const
{
   loop (s, ssize())
      {
        const Token & tok = at(s).get_token();
        if (tok.get_ValueType() == TV_FUN &&
            tok.get_function() == ufun)   return true;
      }

   if (saved_MISC.get_ValueType() == TV_FUN &&
       saved_MISC.get_function() == ufun)   return true;

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::has_quad_LRX() const
{
   /* examples:

       at0 at1 at2 at3  prefix_len
        ÷   R           2            in MISC  ÷ R      has ⎕R
        L   ÷   R       3            in    L  ÷ R      has ⎕L and ⎕R
        L   ÷  [X]  R   4            in    L  ÷ [X] R  has ⎕L and ⎕X and ⎕R
        B  [X]                       in  5 6 [2]
    */

   switch(ssize())
      {
        case 0:  return false;
        case 1:  return false;
        case 2:  if (at1().get_Class() == TC_INDEX)   return true;   // B[X]
                 // fall through
        default: return content[0].get_Class() == TC_VALUE;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::print(ostream & out, int indent) const
{
   loop(i, indent)   out << "    ";
   out << "Token: ";
   loop(s, ssize())   out << " " << at(s).get_token();
   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::destroy_derived_in_FIFO()
{
   loop (s, ssize())
      {
        const Token & tok = at(s).get_token();
        if (tok.get_Class() == TC_VALUE)
           {
             // INVESTIGATED, REVERTED: 'Value_P val = tok.get_apl_val();'
             // (get_apl_val() returns Value_P BY VALUE) takes a second,
             // temporary reference and drops it right away -- tok's own
             // owned Value_P is never touched, so despite the comment
             // above, this doesn't actually clear anything. Confirmed
             // "not a leak today": the Prefix's own storage still holds
             // and eventually releases it. Tried the "real" fix,
             // tok.clear(loc) -- but Token::clear() resets the whole
             // token via placement-new, which also changes its Class to
             // TC_VOID as a side effect. The very next loop below
             // depends on TC_VOID meaning "a function genuinely
             // produced no value" to decide VALUE_ERROR vs
             // SYNTAX_ERROR; reclassifying every real value token to
             // TC_VOID here made that check fire spuriously, turning
             // ordinary SYNTAX ERRORs into wrong VALUE ERRORs (confirmed
             // live: 4 testcases regressed, AllPrimitives/OuterProduct/
             // Quad_ET/Quad_R.tc). This loop being a no-op is load-
             // bearing for that reason, not just harmless -- left as
             // the original code.
             Value_P val = tok.get_apl_val();
          }
        else if (tok.get_Class() == TC_FUN2)
          {
            cFunction_P fun = tok.get_function();
            if (fun && fun->is_derived())
               {
                 Function * fp = const_cast<Function *>(fun);
                 reinterpret_cast<DerivedFunction *>(fp)->destroy_derived(LOC);
               }
          }
      }
}
//────────────────────────────────────────────────────────────────────────────
const Token_loc Prefix::no_loc;
//────────────────────────────────────────────────────────────────────────────
const Token_loc &
Prefix::lookahead_loc() const
{
   if (lookahead_PC >= Prefix_PC_0 && lookahead_PC < body.end_PC())
      return body[lookahead_PC];
   return no_loc;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::syntax_error(const char * loc)
{
   // move the PC back to the beginning of the failed statement
   //
   while (PC > Prefix_PC_0)
      {
        --PC;
        if (body[PC].get_Class() == TC_END)
           {
             ++PC;
             break;
           }
      }

   destroy_derived_in_FIFO();

   // see if error was caused by a function not returning a value.
   // In that case we throw a value error instead of a syntax error.
   //
   loop (s, ssize())
      {
        if (at(s).get_Class() == TC_VOID)
           {
             MORE_ERROR() << "No assignment to result variable Z"
                             " in e.g. ∇Z←... ?";
             // at(s) is the (missing) result of a function call. The
             // error reporting shall find the function from its Token_loc.
             Error::set_failed_function(si.get_executable(),
                                        at(s).get_PC());
             throw_apl_error(E_VALUE_ERROR, loc);
           }
      }

const ErrorCode ec = get_assign_state() == ASS_none ? E_SYNTAX_ERROR
                                                    : E_LEFT_SYNTAX_ERROR;
   set_assign_state(ASS_none);
   throw_apl_error(ec, loc);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::unmark_all_values() const
{
   /*
      Note: the marking of Values and happens centrally (in
      Value::mark_all_dynamic_values()) and likewise for IndexExpr,
      while unmarking happens in the classes like Prefix that hold the
      instances (and therefore have no mark_all_XXX() function.
    */
   loop (s, ssize())
      {
        const Token & tok = at(s).get_token();
        if (tok.get_ValueType() == TV_VAL)
           {
             if (Value_P value = tok.get_apl_val())   value->unmark();
           }
        else if (tok.get_ValueType() == TV_INDEX)
           {
             tok.get_index_val().unmark();
           }
      }

   /* saved_MISC is a single Token_loc slot outside the main stack (used
      while reducing indices/axes); it can also hold a live TOK_INDEX and
      must be swept too (Blake McBride, Bugs8 #1).

      No phrase pushes a Value into misc, thefore no way to unmark it.
    */
const Token & misc = saved_MISC.get_token();
   if (misc.get_ValueType() == TV_INDEX)   misc.get_index_val().unmark();
}
//────────────────────────────────────────────────────────────────────────────
int
Prefix::show_owners(const char * prefix, ostream & out,
                          const Value & value) const
{
int count = 0;

   loop (s, ssize())
      {
        const Token & tok = at(s).get_token();
        if (tok.get_ValueType() != TV_VAL)      continue;

        if (Value::is_or_contains(tok.get_apl_val().get(), &value))
           {
             out << prefix << " Fifo [" << s << "]" << endl;
             ++count;
           }

      }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
Value_P *
Prefix::locate_L(UCS_string & function) const
{
   /*  ⎕L requires at least A f B (so we have at0(), at1() and at2(). However,
       the user could fail on a monadic function (with size() = 2) and then
       query ⎕L. For example:

       at0 at1 at2  size()
        ÷   0       2            in × ÷ 0   (hence no ⎕L)
        2   ÷   0   3            in 2 ÷ 0   (⎕L is 2)
    */

   if (ssize() && at0().get_ValueType() == TV_FUN)
      {
        // e.g. DOMAIN ERROR in × ÷ 0. ssize() is 2 and at0() is ÷
        function = at0().get_function()->get_name();
        return 0;
      }

   if (ssize() > 1 && at1().get_ValueType() == TV_FUN)
      {
        function = at1().get_function()->get_name();
      }
   else if (ssize() == 2 && at1().get_Class() == TC_INDEX)   // B[X]
      {
        function = UCS_ASCII_string("[]");
        return at0().get_apl_valp();
      }

   if (ssize() < 3)   return 0;

   if (at0().get_Class() == TC_VALUE)   return at0().get_apl_valp();
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
Value_P *
Prefix::locate_R(UCS_string & function) const
{
   // ⎕R requires at least f B (so we have at0() and at1()

   if (ssize() < 2)   return 0;

   if (ssize() == 2 && at1().get_Class() == TC_INDEX)   // B[X]
      {
        function = UCS_ASCII_string("[]");   // valid function
        return 0;                      // but no ⎕R.
      }

   // either at0() (for monadic f B) or at1() (for dyadic A f B) must
   // be a function or operator
   //
   if (at0().get_ValueType() != TV_FUN &&
       at1().get_ValueType() != TV_FUN)   return 0;

   // prefix_len is the length of the LAST matched phrase, not a property
   // of the current stack -- it can exceed ssize() (leftover from a
   // dissimilar previous reduction), making the index below negative.
   // The sibling pop_args() (Prefix.hh) already guards the same quantity
   // with Assert1(put >= prefix_len); mirror that here instead of
   // indexing content[] with a negative subscript.
   //
   if (prefix_len > ssize())   return 0;

const Token & ret = content[ssize() - prefix_len].get_token();
   if (ret.get_Class() == TC_VALUE)   return ret.get_apl_valp();
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
Value_P *
Prefix::locate_X(UCS_string & function) const
{
   // ⎕X requires at least X B (so we have at0() and at1()

   if (ssize() < 2)   return 0;

   // either at0() (for monadic f X B) or at1() (for dyadic A f X B) must
   // be a function or operator
   //
   rev_loop(x, ssize())
       {
         if (content[x].get_ValueType() == TV_FUN)
            {
              if (cFunction_P fun = content[x].get_function())
                 {
                   function = fun->get_name();
                   // locate_X() always returns 0 for non-derived functions
                   // because we can't allow ⎕X to modify the function body.
                   if (Value_P * X = fun->locate_X())   return  X;
                 }
            }
         else if (content[x].get_Class() == TC_INDEX)   // maybe found X ?
            {
              return content[x].get_token().get_apl_valp();
            }
       }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────

// one entry of a hash table for all prefixes that can be reduced.
// Used in Prefix.def
//
# define reduce_none 0

# define PH(name, suffix, idx, prio, misc, len, can_shift, phrase_number) \
   { #name, #suffix, &Prefix::reduce_ ## suffix, idx, prio, misc, len,   \
     can_shift, phrase_number }

const Prefix::Phrase Prefix::hash_table[] =
{
// the entire hash table or tree with all prefixes that can be reduced...
#include "Prefix.def"   // the hash table
};

Token
Prefix::reduce_body()
{
   Log(LOG_Reduce_XXX)
      {
       loop(s, si.get_depth())   CERR << "    ";   // indent
        CERR << "Reduce Statement (in APL order): ";
        print_patterns(CERR, 6) << endl;
      }

   Log(LOG_prefix_parser)
      {
        CERR << endl << "changed to Prefix[si=" << si.get_level()
             << "]) ============================================" << endl;
      }

   // ⎕EA or ⎕EB in this statement has delivered an escape (→ in its B),
   // see handle_QUAD_ES_ESC(): like a plain → (reduce_END_GOTO__()) or
   // Z←⍎'→', abandon the statement (including the part left of ⎕EA/⎕EB,
   // which was not yet shifted) and return the escape from this context.
   // It used to be pushed as a TOK_ESCAPE into the statement, which was a
   // SYNTAX ERROR if the result of ⎕EA/⎕EB was used, e.g. Z←'5' ⎕EB '→'
   // (Blake McBride, Bugs32 #21).
   //
   if (escape_pending)
      {
        escape_pending = false;
        prefix_len = ssize();   // the entire stack
        pop_args_push_result(Token(TOK_ESCAPE));
        return pop().get_token();
      }

   if (ssize())   goto again;

   // the main loop of an LALR(1) parser...
   //
grow:    // aka. SHIFT
   // the current stack does not contain a valid phrase.
   // Push one more token onto the stack and continue
   //
   Log(LOG_Shift_XXX)
      {
        loop(s, si.get_depth() + 1)   CERR << "    ";   // indent
        CERR << "PC=" << PC << " Shift[" << ssize() << "]: ";
        print_token_value(CERR, body[PC]) << " into ";
        print_patterns(CERR, 1);
      }

   if (push_next_token())
      {
        Log(LOG_Shift_XXX)   CERR << "pushed )SI.";
        return Token(TOK_SI_PUSHED);
      }

   Log(LOG_Shift_XXX)
      {
        Output::indent(50);
        CERR << " yields: ";
        print_patterns(CERR, 3) << endl;
      }

again:   // aka. REDUCE
   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

   /* search for the longest prefix in the phrase table...

      at this point: N≥0 phrases match the current stack. We call
      find_best_phrase() to find the longest of them; best_phrase=0 if N=0
    */
   find_best_phrase();                  // compute best_phrase
   if (best_phrase == 0)   goto grow;   // no phrase matches the curent stack

   /*
      At this point we have found a valid best_phrase which, at least in
      principle, matches the (top of the) current stack.

      Caveat: if the next token ahead binds stronger than best_phrase->prio
              then we have a SHIFT/REDUCE conflict and must SHIFT rather
              than REDUCE.

      best_phrase->can_shift is a static (per-phrase, generated by
      tools/phrase_gen) property folding together the two conditions that
      together make a SHIFT possible at all for this phrase -- both knowable
      purely from the phrase table, without looking at run-time context:

      1. prio < BS_ANY_BRA: a phrase binding as tightly as (or tighter
         than) ANY[] (e.g. A C, V C, V C ASS B) always wins outright;
         nothing can ever bind tighter, so no SHIFT is ever needed.

         Note: prio < BS_ANY_BRA used to be tested here, but in now
               factored into can_shift().

      2. do_shift()'s switch(at0().get_Class()) only ever returns true
         for a leading token class of TC_VALUE, TC_FUN12, or TC_SYMBOL;
         every other leading class hits its own 'default: return false'.

      can_shift is 0 whenever either fails, i.e. whenever bind_to_next()
      would be called just to compute a foregone "false". Checking it
      here avoids that call for those phrases instead of relying on
      bind_to_next()/do_shift() to (correctly, but expensively) reach the
      same conclusion at run time.
    */
   if (best_phrase->can_shift         &&   // a SHIFT is at all possible, and
       PC < body.end_PC() &&   // more tokens ahead, and
       bind_to_next())                     // bext token binds stronger
      {
        goto grow;                         // then: SHIFT
      }

   // otherwise REDUCE
   //
   Log(LOG_prefix_parser)
      {
        CERR << "   phrase #" <<  best_phrase->phrase_number
             << ": " << best_phrase->phrase_name
             << " matches, calling reduce_" << best_phrase->reduce_name
             << "()" << endl;
      }

   prefix_len = best_phrase->phrase_len;

   // a value immediately followed by an operator that never got its
   // operand(s), e.g. R←2 3\ or →4 / or 1+2 / or R←9⍤ : the statement is
   // malformed. Reducing the phrase left of it would execute the
   // assignment, branch, etc. before the SYNTAX ERROR at the end of the
   // statement (or, for a branch, never report it at all), Bugs35 #75.
   //
   // Only for an assignment or a branch (the side effects): a value can
   // also be the operand of an operator on its right (2 OP +, ⍣2⍤1).
   //
   // The same for a parenthesized group on the right that could not be
   // reduced, e.g. Z←{⍵+7}(⌿5): a group in parentheses is always reduced
   // before anything on its left, so the statement is malformed and
   // nothing (also no function) shall be assigned.
   //
   if (ssize() > prefix_len &&
       (strstr(best_phrase->phrase_name, "ASS") ||
        strstr(best_phrase->phrase_name, "GOTO")))
      {
        const TokenClass next = at(prefix_len).get_Class();
        if (at(prefix_len - 1).get_Class() == TC_VALUE &&
            (next == TC_OPER1 || next == TC_OPER2))
           {
             MORE_ERROR() << "operator without (left) operand";
             SYNTAX_ERROR;
           }

        if (next == TC_L_PARENT)
           {
             MORE_ERROR() << "invalid expression in parentheses";
             SYNTAX_ERROR;
           }

        // a value followed by a function that has no right argument, e.g.
        // R←5 + or R←5 ∘. 1 (∘. 1 was reduced to a derived function before
        // the assignment), Bugs36 F1. Everything right of the value has
        // already been reduced, so the function can no longer get its
        // right argument.
        //
        if (at(prefix_len - 1).get_Class() == TC_VALUE && next == TC_FUN12)
           {
             MORE_ERROR() << "function without right argument";
             SYNTAX_ERROR;
           }
      }

   Log(LOG_Reduce_XXX)
      {
        loop(s, si.get_depth() + 1)   CERR << "    ";   // indent
        CERR << "Match[" << prefix_len << "]: calling: reduce_"
             << best_phrase->reduce_name << "()" << endl;
      }

   action = RA_FIXME;   // to detect missing 'action = ' in a reduce_XXX()
   if (best_phrase->misc)   // MISC phrase: save X and remove it
      {
        // A second MISC phrase (e.g. a second ⍣N/⍤N literal-right-operand
        // combination) matched while the first one's saved_MISC is still
        // pending is a genuinely invalid expression (nothing valid
        // consumes two independent axis-like slots at once) -- this used
        // to be a plain Assert(), reachable from ordinary user input
        // (+⍣1 1⍤1 + 1), which crashed here (or, with Assert() compiled
        // out, silently overwrote/orphaned the pending saved_MISC token
        // instead). Reject with SYNTAX ERROR before popping anything, so
        // there is nothing left to leak.
        //
        // Bugs28 #26 residual: the plain SYNTAX_ERROR macro throws
        // directly (throw_apl_error()), bypassing this class's OWN
        // syntax_error(loc) member function just below -- which sweeps
        // the FIFO for any already-reduced derived function (e.g. the
        // "+⍣1" built from the FIRST, otherwise-valid MISC phrase before
        // this second, invalid one was seen) and destroy_derived()s it.
        // Without that sweep, the first phrase's DerivedFunctionCache
        // slot (and the Value_P it owns, e.g. POWER's literal N) is
        // still sitting on the stack when the error unwinds, and nothing
        // else ever frees it -- confirmed via )CHECK: "ERROR - 1 stale
        // values" (Bif_OPER2_POWER.cc:74, IntScalar(N)). Call the member
        // function instead so this MISC-phrase rejection gets the same
        // cleanup every other syntax error in this file already does.
        //
        if (has_MISC())   syntax_error(LOC);
        saved_MISC.copy(pop(), LOC);
        --prefix_len;
      }

   /*
      detect if the reduce_fun() changes the Prefix instance. Only checking
      for e.g. changes in Workspace::SI_top() does not suffice because a
      different Prefix instance could be located at the same address.
    */
const uint64_t inst = instance;
   (this->*best_phrase->reduce_fun)();

   Log(LOG_Reduce_XXX)
      {
        Output::indent(50);
        CERR << " yields: ";
        print_patterns(CERR, 3) << endl;
      }

   if (inst != Workspace::SI_top()->get_prefix().instance)
      {
        // the reduce_fun() above has changed the )SI stack. As a consequence
        // the 'this' pointer is no longer valid and we must not access members
        // of this Prefix instance.
        //
        return Token(TOK_SI_PUSHED);
      }

   Log(LOG_prefix_parser)
      CERR << "   reduce_" << best_phrase->reduce_name << "() returned: ";

   // handle action
   //
   switch(action)
      {
        case RA_CONTINUE:
             LOG_prefix_parser && CERR << "RA_CONTINUE" << endl;

             // the reduce_fun() has modified this Prefix (in most cases with
             // pop_args_push_result()).
             // Repeat the pattern matching without fetching a new token.
             //
             goto again;

        case RA_PUSH_NEXT:
             LOG_prefix_parser && CERR << "RA_PUSH_NEXT" << endl;

             // the reduce_fun() has decided to SHIFT. Fetch one more token.
             //
             goto grow;

        case RA_SI_PUSHED:
             LOG_prefix_parser && CERR << "RA_SI_PUSHED" << endl;

             // the reduce_fun() has pushed the )SI stack.
             // Continue execution in the new )SI item.
             //
             return Token(TOK_SI_PUSHED);

        case RA_RETURN:
             LOG_prefix_parser && CERR << "RA_RETURN" << endl;

             // the reduce_fun() has decided to leave this Prefix.
             // The result of this Prefix is e.g. the TOK_VOID or TOK_BRANCH_INT
             // that was returned by StateIndicator::jump(). pop() the
             // result from this Prefix and return it to the calling Prefix.
             //
             return pop().get_token();

        case RA_FIXME:
             LOG_prefix_parser && CERR << "RA_FIXME" << endl;

             // not expected to happen.
             //
             FIXME;
      }

   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::do_shift(TokenClass next) const
{
   /* resolve a shift/reduce conflict.

       at0() is (in APL order) the current token (aka. top of stack).
       at1() is (in APL order) the token right of the current token
       next is the class of the token left of at0()
    */

   /** MAY_STRAND is a property of best_phrase that defines if the current ToS
       shall be stranded with the next token ahead (i.e. best_phrase binds
       weaker) or not (i.e. best_phrase binds stronger)
    **/
#define MAY_STRAND (best_phrase->prio < BS_VAL_VAL)
   switch(at0().get_Class())
      {
        case TC_VALUE:
             // DOP B must always SHIFT the DOP.
             //
             if (next == TC_OPER2)   return true;

             // A strand must SHIFT if best_phrase binds weakly
             //
             if (next == TC_VALUE)   return MAY_STRAND;

             // A value-) must strand, a function-) must not.
             //
             if (next == TC_R_PARENT)   // ) B
                {
                  if (is_value_parenthesis(PC))   return MAY_STRAND; // (X+Y) B
                   else                           return false;      // (+/) B
                }
             return false;   // REDUCE

        case TC_FUN12:  return next == TC_OPER2;
        case TC_SYMBOL: return next == TC_OPER2;
        default:        return false;   // REDUCE
      }
#undef MAY_STRAND
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::print_stack(ostream & out, const char * loc) const
{
const int si_depth = si.get_level();

   out << "fifo[si=" << si_depth << " len=" << ssize()
       << " PC=" << PC << "] is now :";

   loop(s, ssize())
      {
        const TokenTag tag = at(s).get_tag();
        out << " " << Token::class_name(tag);
      }

   out << "  at " << loc << endl;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Prefix::print_patterns(ostream & out, int which)
{
const bool on_stack = which & 1;
const bool ahead    = which & 2;
const bool reverse  = which & 4;   // print ahead in APL order

   if (on_stack)   // print token on the stack
      {
        int Nval = 0;
        loop (s, ssize())
             {
               const Token & tok = at(s).get_token();
               if (tok.get_Class() == TC_VALUE)   ++Nval;
             }
        const char * AB[4]  = { "A", "B",  "-", "-" };
        const char * ARB[4] = { "A", "RO", "B", "-" };
        const char * FG[4]  = { "F", "G",  "-", "-" };
        const char ** V = AB;
        const char ** F = FG;

        if      (Nval <= 1)   V = AB + 1;
        else if (Nval >  2)   V = ARB;

        int sepa = 0;
        out << "«";
        loop (s, ssize())
             {
               if (sepa++)   out << " ";

               const Token & tok = at(s).get_token();
               switch(const TokenClass tc = tok.get_Class())
                  {
                    case TC_ASSIGN:    out << "←";          break;
                    case TC_R_ARROW:   out << "→";          break;
                    case TC_L_BRACK:   out << "[";          break;
                    case TC_R_BRACK:   out << "]";          break;
                    case TC_END:       out << "END";        break;
                    case TC_FUN0:      out << "N";          break;
                    case TC_FUN12:     out << *F++;         break;
                    case TC_INDEX:     out << "C";          break;
                    case TC_OPER1:     out << "M";          break;
                    case TC_OPER2:     out << "D";          break;
                    case TC_L_PARENT:  out << "(";          break;
                    case TC_R_PARENT:  out << ")";          break;
                    case TC_RETURN:    out << "RET";        break;
                    case TC_SYMBOL:    out << "SYM";        break;
                    case TC_VALUE:     out << *V++;         break;
                    case TC_VOID:      out << "VOID";       break;
                    case TC_SI_CHANGE: out << "SI_CHANGE";  break;
                    default:           out << "-TC_" << int(tc);
                  }
             }
        out << "»";
      }

   if (on_stack && ahead)   out << " ";   // both

   if (ahead)   // print token ahead
      {
        int sepa = 0;
        if (reverse)
           {
             Prefix_PC end_of_statement = body.end_PC() - 1;
             for (Prefix_PC pc = PC; pc < body.end_PC(); ++pc)
                 {
                   if (body[pc].get_Class() == TC_END)
                      {
                        end_of_statement = pc;
                        break;
                      }
                 }

             for (Prefix_PC pc = end_of_statement - 1; pc >= PC; --pc)
                 {
                   if (sepa++)   out << " ";
                   const TokenTag tag = body[pc].get_tag();
                   out << Token::short_class_name(tag);
                 }

             if (end_of_statement != body.end_PC() - 1)
                {
                  if (sepa++)   out << " ";
                  out << "END";
                }
           }
        else
           {
             for (Prefix_PC pc = PC; pc < body.end_PC(); ++pc)
                 {
                   if (sepa++)   out << " ";
                   const TokenTag tag = body[pc].get_tag();
                   out << Token::short_class_name(tag);
                   if ((int(tag) & int(TC_MASK)) == TC_END)   break;   // end of statement
                 }
           }
      }
   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Prefix::print_token_value(ostream & out, const Token & tok)
{
const TokenClass tc = tok.get_Class();
   if (tc == TC_SYMBOL)
      {
        out << "SYMBOL(";
        const Symbol * sym = tok.get_sym_ptr();
        out << sym->get_name();
        return out << ")";
      }

   if (tc == TC_VALUE)
      {
        const TokenTag tag = tok.get_tag();
        if      (tag == TOK_APL_VALUE1)   out << "VALUE1(";
        else if (tag == TOK_APL_VALUE2)   out << "VALUE2(";
        else if (tag == TOK_APL_VALUE3)   out << "VALUE3(";
        else if (tag == TOK_APL_VALUE4)   out << "VALUE4(";
        else                              out << "VALUE?(";

        if (Value_P value = tok.get_apl_val())   value->print_brief(out);
        else                                     out << "0";
        return out << ")";
      }

   if (tc == TC_END)
      {
        const TokenTag tag = tok.get_tag();
        if (tag == TOK_END)       return out << "◊";
        if (tag == TOK_ENDL)      return out << "ENDL";
        if (tag == TOK_IF_THEN)   return out << "→→";
        if (tag == TOK_IF_ELSE)   return out << "←→";
        if (tag == TOK_IF_END)    return out << "←←";
      }

   return out << (Token::class_name(tok.get_tag()) + 3);
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::try_vector_assignment()
{
   // TOKEN ... TOKEN V ) ← B   (reversed)
   //             ↑
   //             PC
   //
   // collect the TOK_LSYMB2 symbols left of PC, in addition to the last
   // symbol V (already read into the caller's at0() and pushed here
   // first). Fewer than 2 symbols in total means this is not a vector
   // assignment at all -- PC is left unchanged in that case (the loop
   // below never advances it without also pushing a symbol), so the
   // caller can go on to try selective specification instead.
   //
vector<Symbol *> symbols;
   symbols.reserve(10);
   symbols.push_back(at0().get_sym_ptr());   // the last symbol V

   while (PC < body.end_PC())
       {
         const Token & tok = body[PC];
         if (tok.get_ValueType() != TV_SYM)   break;

         ++PC;
         Symbol * sym_var = tok.get_sym_ptr();
         Assert(sym_var);

         // Bugs31 #24 (Blake McBride): a read-only system variable
         // (e.g. ⎕TS) in a non-final vector-assignment position IS
         // syntactically a variable-shaped name (can_be_assigned()'s
         // generic !can_be_assigned() check just below would otherwise
         // treat it exactly like "not a variable at all" and silently
         // break out here without consuming it) -- but silently
         // dropping it, or falling through to whatever unrelated error
         // the resulting symbol/RHS count mismatch happens to produce
         // (e.g. a confusing LENGTH ERROR), never tells the user what
         // actually went wrong. Give the same clear diagnostic a direct
         // ⎕TS←7 already gives via Symbol::resolve_left(), and reject
         // the WHOLE vector assignment outright -- no partial
         // assignment to any of the other names either.
         //
         if (sym_var->is_readonly())
            {
              MORE_ERROR() << "Assignment to read-only system variable "
                           << sym_var->get_name();
              SYNTAX_ERROR;
            }

         if (!sym_var->can_be_assigned())   break;   // not a variable
         symbols.push_back(sym_var);
       }

   if (symbols.size() < 2)   return false;   // not a vector assignment

   // a genuine (C D ... V) ← B vector assignment. The loop above only
   // ever stops at the first non-variable token; it does not by itself
   // confirm that token is the '(' a genuine vector assignment
   // requires. Without this check, something like (∊V W)←C collects
   // [W, V] (2 symbols, so this branch runs) with the leading ∊ simply
   // never examined, so Symbol::vector_assignment() below would
   // silently overwrite both V and W as if ∊ were not there at all,
   // before any error about the leftover, unconsumed ∊ token could fire
   // (Blake McBride, Bugs25 #1 follow-up -- must not mutate anything
   // before the syntax is known to be legal, the same principle
   // reduce_V_RPAR_ASS_B()'s own selective-specification half applies).
   //
   if (PC >= body.end_PC() || body[PC].get_Class() != TC_L_PARENT)
      {
        MORE_ERROR() << "Malformed vector specification";
        // this parse attempt is being abandoned outright (like a
        // completed vector assignment, there is nothing further to
        // build on the left of ←) -- reset before the error is thrown
        // so a later syntax_error(LOC) call on this same (reused, see
        // StateIndicator::current_stack) Prefix does not inherit a
        // stale ASS_var_seen from here.
        set_assign_state(ASS_none);
        LEFT_SYNTAX_ERROR;
      }

Value_P B = at3().get_apl_val();
   Symbol::vector_assignment(symbols, B);

   set_assign_state(ASS_none);

   // clean up stack
   //
const Token result(TOK_APL_VALUE2, at3().get_apl_val());
   pop_args_push_result(result);
   if (PC >= body.end_PC() ||
       body[PC++].get_Class() != TC_L_PARENT)   syntax_error(LOC);

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::value_expected() const
{
   /* on entry: saved_MISC.get_Class() == TC_INDEX token.
                body[PC] is the token left of saved_MISC.

      return true if the token left of saved_MISC must be a value (so
      that saved_MISC is the index of a value) or false if the token
      left of saved_MISC must be a function or operator (so that
      saved_MISC is the axis of a  function or operator).

       See also: "Additional Requirement" at the bottom of page 48 in the
       ISO standard.
    */
   Assert1(saved_MISC.get_Class() == TC_INDEX);
   Assert1(lookahead_PC == (PC - 1));

   // function axes cannot contain semicolons. Therefore, if saved_MISC
   // contains semicolons then its get_ValueType() is TV_INDEX and
   // saved_MISC  MUST be the index of a value. The converse is not
   // true: a get_ValueType() of TC_VALUE only indicates the lack of semicolons,
   // which is valid for both functions and values.
   //
   if (saved_MISC.get_ValueType() == TV_INDEX)   return true;   // value

   // look ahead further until value index vs. function axis can be decided.
   //
   for (Prefix_PC pc = PC; pc < body.end_PC();)
      {
        const Token & tok = body[pc++];
        switch(tok.get_Class())
           {
               case TC_R_BRACK:   // skip over [...] (func axis or value index)
                    //
                    pc = pc + int(tok.get_int_val2());
                    continue;

               case TC_END:     return false;   // ◊ [] : syntax error

               case TC_FUN0:    return true;   // niladic function is a value
               case TC_FUN12:   return false;  // function

               case TC_SYMBOL:
                    {
                      const Symbol * sym = tok.get_sym_ptr();
                      const NameClass nc = sym->get_NC();

                      if (nc == NC_FUNCTION)   return false;
                      if (nc == NC_OPERATOR)   return false;
                      return true;   // value
                    }

               case TC_RETURN:  return false;   // syntax error
               case TC_VALUE:   return true;

               default: continue;
           }
      }

   // this is a syntax error.
   //
   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::adjust_right_caret(Function_PC2 & range,
                           const Token_string & failed_statement)

{
   // called after a SYNTAX ERROR. The right caret may be too far right
   // (in APL order). Try to narrow the range.
   //
   // range.low is the right (in APL order) end of the failed phrase and
   // range.high is its left (in APL order) end, both as indices into
   // failed_statement (which is reversed, i.e. index 0 is its rightmost
   // token). Only the tokens of the phrase are searched, not those of the
   // entire statement.
   //
   // If something upstream (e.g. missing_files(), Missing_Libraries.cc,
   // for a ⎕-function whose library isn't compiled in) already set a
   // specific )MORE message before throwing this SYNTAX ERROR, don't
   // guess: every branch below used to unconditionally overwrite it
   // with a generic pattern-matched one (MORE_ERROR() itself clears
   // any existing text before the caller appends to it -- see
   // Common.hh), even when that guess is wrong for the real cause (a
   // dyadic call to a missing-library ⎕-function, e.g. 1 ⎕FFT 1 2 3 4,
   // LOOKS like "nomadic function without B" to the token-adjacency
   // check below). The caret-narrowing (range.low/range.high) below is
   // still useful even when some unrelated )MORE text is already
   // present (e.g. testcases/Quad_R.tc relies on it), so only the
   // MORE_ERROR() calls themselves are skipped, not the narrowing. See
   // Bugs27 #53.
   //
const bool have_more_error = Workspace::more_error().size();

   for (Function_PC pc = range.low; (pc + 1) < range.high; ++pc)
       {
         // find an obviously impossible pattern 
         //
         const Token & T0 = failed_statement[pc];
         const TokenTag tag0 =  T0.get_tag();
         const TokenClass tc0 = T0.get_Class();

         const Token & T1 = failed_statement[pc + 1];
         const TokenTag tag1 =  T1.get_tag();
         const TokenClass tc1 = T1.get_Class();

         // NOTE: pc == 0 is NOT a right end: it used to be the END token
         // of the statement, but failed_statement no longer contains it,
         // so that pc == 0 is the rightmost token, typically the B of T1
         // (e.g. +⍣1 1⍤1+1 was reported as "+ without right argument B").
         //
         const bool right_end = tc0  == TC_R_BRACK  ||
                                tc0  == TC_R_PARENT ||
                                tag0 == TOK_SEMICOL;
          const bool nomadic1 = needs_B(tc1);

          if (right_end && nomadic1)
             {
               // nomadic function without B, e.g. + )
               //
               const UCS_string name = T1.get_function()->get_name();
               if (!have_more_error)
                  MORE_ERROR() << name << " B: Nomadic function " << name
                               << " without right argument B.";
               range.low = pc;
               range.high = pc + 1;
               return;
             }

         if (is_operator_class(tc0))
            {
              const UCS_string name = T0.get_function()->get_name();
              if (tc1 == TC_L_BRACK   ||
                  tc1  == TC_L_PARENT ||
                  tag1 == TOK_SEMICOL)
                 {
                   // operator f OP without f, e.g. ( ⍣
                   //
                   if (!have_more_error)
                      MORE_ERROR() << "f " << name << " Operator: " << name
                                   << " without left argument f.";
                   range.low = pc;
                   range.high = pc + 1;
                   return;
                 }

              if (tc1 == TC_VALUE && tag0 == TOK_OPER2_POWER)
                 {
                   // operator f OP with value f, e.g. 4 ⍣
                   //
                   if (!have_more_error)
                      MORE_ERROR() << "f " << name << ": Operator " << name
                                   << " with left value f.";
                   range.low = pc;
                   range.high = pc + 1;
                   return;
                 }
             }
       }
}
//────────────────────────────────────────────────────────────────────────────
Fun_signature
Prefix::get_current_signature()
{
   // this function is called after some eval_XXX() was invoked and
   // computes the signature XXX from the current stack.
   //
const Prefix & prefix = Workspace::SI_top()->get_prefix();
const int prefix_len = prefix.prefix_len;

int ret = SIG_NONE;
   Assert(prefix_len > 1);   // at least FUN B
   if (prefix.Class_at(prefix_len - 1) == TC_VALUE)   ret |= SIG_B;
   if (prefix.Class_at(0) == TC_VALUE)                ret |= SIG_A;
   loop(p, prefix_len)
       {
         switch(prefix.Class_at(p))
            {
              case TC_FUN12: ret |= SIG_FUN;   break;
              case TC_OPER1: ret |= SIG_LO;    break;
              case TC_OPER2: ret |= SIG_OP2;   break;
              case TC_INDEX: ret |= SIG_X;     break;
              default:       ;
            }
       }

   return Fun_signature(ret);
}
//════════════════════════════════════════════════════════════════════════════
DerivedFunction *
Prefix::get_fun_oper_slot(const Token * LO, cFunction_P F_or_M_or_D, const Token * RO,
                          Value_P X, const char * loc) const
{
   return si.get_fun_oper_slot(LO, F_or_M_or_D, RO, X, loc);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::handle_ELSE(const Token & maybe_else, int num)
{
   // the int value of TOK_IF_ELSE is the PC of the end of the ELSE clause.
   //
   if (maybe_else.get_tag() != TOK_IF_ELSE)   return;   // not ←→

const Prefix_PC PC_from = PC = Prefix_PC(maybe_else.get_int_val());
   while (body[PC].get_tag() == TOK_IF_END)   ++PC;   // nested else
   if (body[PC].get_tag() == TOK_IF_ELSE)   // another ELSE clause
      PC = Prefix_PC(body[PC].get_int_val());
   Log(LOG_IfElse)
      {
        CERR << "END of THEN (" << num << ") reached, PC is now: " << PC
             << "(" << PC_from << ")" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::set_action(const Token & result)
{
   switch(result.get_Class())
      {
        case TC_VALUE:
        case TC_VOID:
        case TC_END:
        case TC_FUN2:
             set_action(RA_CONTINUE);
             return;

        case TC_RETURN:
             // result was one of TOK_RETURN_EXEC, TOK_RETURN_STATS,
             // TOK_RETURN_VOID, or TOK_RETURN_SYMBOL.
             // The current context is complete and may or may not
             // have produced a value.
             //
             set_action(RA_RETURN);
             return;

        case TC_SI_CHANGE:
             //
             // result was TOK_SI_PUSHED or TOK_ERROR
             if (result.get_tag() == TOK_ERROR)   set_action(RA_RETURN);
             else                                 set_action(RA_SI_PUSHED);
             return;

        default: CERR << "CLASS = " << result.get_Class()
                      << " at " << LOC << endl;
                 FIXME;
      }
}
//────────────────────────────────────────────────────────────────────────────
//
// e reduce functions...
//
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::is_value_bracket() const
{
   Assert1(body[PC - 1].get_Class() == TC_R_BRACK);
const int offset = body[PC - 1].get_int_val2();
   Assert1(body[PC + offset - 1].get_Class() == TC_L_BRACK);   // opening [

const Token & tok1 = body[PC + offset];
   if (tok1.get_Class() == TC_VALUE)    return true;
   if (tok1.get_Class() != TC_SYMBOL)   return false;

Symbol * sym = tok1.get_sym_ptr();
const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
   return sym->resolve_class(is_left_sym) == TC_VALUE;
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::is_value_parenthesis(Prefix_PC pc) const
{
   // we have ) XXX with XXX on the stack and need to know if the evaluation
   // of (... ) will be a value as in e.g. (1 + 1) or a function as in (+/).
   //
   Assert1(body[pc].get_Class() == TC_R_PARENT);

   ++pc;
   if (pc >= body.end_PC())   return true;   // syntax error

TokenClass next = body[pc].get_Class();

   if (next == TC_R_BRACK)   // skip [ ... ]
      {
        const int offset = body[pc].get_int_val2();
        pc = pc + offset;
        Assert1(body[pc].get_Class() == TC_L_BRACK);   // opening [
        ++pc;   // was missing: landed on '[' itself instead of past it,
                // misclassifying e.g. (⌽[X]) as a value instead of a
                // function
        if (pc >= body.end_PC())   return true;   // syntax error
        next = body[pc].get_Class();
      }
   else if (next == TC_INDEX && body[pc].get_tag() == TOK_FAXIS)
      {
        // skip a literal function axis, e.g. (⌽[1]). Parser::
        // optimize_literal_axes() already collapsed the [ ... ] triple
        // above into this single TOK_FAXIS token before Prefix.cc ever
        // runs, so the TC_R_BRACK case above (which expects the
        // original, unoptimized triple) never matches it. Without this,
        // next stayed TC_INDEX, matched none of the checks below, and
        // fell through to the final "return true" -- misclassifying
        // (⌽[1]) as a value parenthesis, deferring reduction of
        // whatever dyadic expression is next in the wrong direction
        // (e.g. (⌽[1])2 3⍴⍳6 silently reduced as (⌽[1])2 first, giving
        // 3 2⍴⍳6 instead of (⌽[1])(2 3⍴⍳6)). Found via the new (F C)
        // grammar phrase below/Bugs26 #5, which is what first made
        // (⌽[1]) reducible at all instead of an immediate SYNTAX ERROR.
        //
        ++pc;
        if (pc >= body.end_PC())   return true;   // syntax error
        next = body[pc].get_Class();
      }

   if (next == TC_SYMBOL)   // resolve symbol if necessary
      {
        // dyadic operator with a variable RO, e.g. Y in (⍴⍤Y) -- mirror
        // the TC_VALUE-followed-by-TC_OPER2 case below, which already
        // handles this for a LITERAL RO like (⍴⍤1). Without this, a
        // variable RO was unconditionally "value", so (⍴⍤Y)2 3⍴⍳6
        // stranded 2 3⍴⍳6 with the value parenthesis instead of passing
        // it to the derived function. See Bugs27 #21.
        //
        if (is_OPER2_left_of_RO(pc))   return false;

        const Symbol * sym = body[pc].get_sym_ptr();
        const NameClass nc = sym->get_NC();

        if (nc == NC_FUNCTION)   return false;
        if (nc == NC_OPERATOR)   return false;
        return true;
      }

   if (next == TC_OPER1)   return false;
   if (next == TC_OPER2)   return false;
   if (next == TC_FUN12)   return false;

   // body is walked right-to-left (Prefix's own evaluation order): pc
   // currently points one past our closing ')', i.e. at the RIGHTMOST
   // token still INSIDE these parens. For a source "))" (e.g. the RO's
   // own "(2-1)" ending right where the outer "(⍴⍤(2-1))" also ends),
   // that rightmost inner token is itself a ')' -- TC_R_PARENT, not
   // TC_L_PARENT. The class this used to check for doesn't occur here
   // at all (class token identities are not swapped by the reversal,
   // only their array order is), so this branch silently never matched
   // real "))" input; a literal RO like (⍴⍤1) only worked via the
   // separate TC_VALUE-preceded-by-TC_OPER2 case elsewhere in this
   // function. See Bugs27 #21.
   //
   if (next == TC_R_PARENT)   // )) XXX
      {
        // pc already points at this inner ')' (no extra ++pc first --
        // that used to skip past it onto an unrelated token, since pc
        // here is the token TO recurse on, not one before it, tripping
        // the Assert1(body[pc].get_Class()==TC_R_PARENT) at the top of
        // this very function on the very first recursive call).
        //
        if (!is_value_parenthesis(pc))   return false;   // (fun)) XXX
        const int offset = body[pc].get_int_val2();
        pc = pc + offset;
        if (pc >= body.end_PC())   return true;   // syntax error
        next = body[pc].get_Class();
        Assert1(next == TC_L_PARENT);   // opening (
        ++pc;
        if (pc >= body.end_PC())   return true;   // syntax error

        //   (val)) XXX
        //  ^
        //  pc
        //
        // result is a value unless (val) is the right function operand
        // of a dyadic operator
        //
        next = body[pc].get_Class();
        if (next == TC_OPER2)   return false;
        if (next == TC_SYMBOL)   // resolve symbol if necessary
           {
             const Symbol * sym = body[pc].get_sym_ptr();
             const Function * fun = sym->get_function();
             return ! (fun && fun->is_operator() &&
                       fun->get_oper_valence() == 2);
           }
        return true;
      }

   // dyadic operator with numeric function argument, for example:  ⍤ 0
   //
   if (next == TC_VALUE && is_OPER2_left_of_RO(pc))   return false;

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
Prefix::is_OPER2_left_of_RO(Prefix_PC pc) const
{
   // body[pc] is a possible right operand RO. Return true if it is the RO
   // of a dyadic operator, possibly with an axis between the operator and
   // RO, e.g. (⌽⍤[1]1). Bugs37 #15.
   //
   ++pc;   // the token left of RO
   if (pc >= body.end_PC())   return false;

   if (body[pc].get_Class() == TC_INDEX &&
       body[pc].get_tag() == TOK_FAXIS)   // literal axis, e.g. [1]
      {
        ++pc;
      }
   else if (body[pc].get_Class() == TC_R_BRACK)   // [ ... ]
      {
        pc = pc + body[pc].get_int_val2();
        if (pc >= body.end_PC())   return false;
        if (body[pc].get_Class() != TC_L_BRACK)   return false;
        ++pc;
      }

   return pc < body.end_PC() && body[pc].get_Class() == TC_OPER2;
}
//────────────────────────────────────────────────────────────────────────────
inline bool
Prefix::push_next_token()
{
   if (has_MISC())   // valid lookahead token
      {
        // there is a stored MISC token from a MISC phrase. Symbol resolution
        // was already performed, so we can push it now and are done.
        //
        push(saved_MISC);
        clear_MISC(LOC);   // saved_MISC ← TOK_VOID
        return false;
      }

again:

   // the first token of a statement is its rightmost token
   //
const bool statement_start = ssize() == 0;
const Prefix_PC old_PC = PC++;
const Token_loc & tloc = body[old_PC];   // incl. its (opaque) PC
const Token & tok = tloc.get_token();
const TokenClass tcl = tok.get_Class();
   Log(LOG_prefix_parser)
      {
        CERR << "    [si=" << si.get_level() << " PC=" << (PC - 1)
             << "] Read token[" << ssize()
             << "] (←" << get_assign_state() << "←) " << tok << " "
             << Token::class_name(tok.get_tag()) << endl;
      }

   lookahead_PC = old_PC;   // the position (in body) of the lookahead token

   if (tok.get_tag() == TOK_GOTO_PC)   // →N
      {
        PC = Prefix_PC(tok.get_int_val());
        Assert1(ssize() == 0);
        goto again;
      }

   if (tcl == TC_SYMBOL)          // resolve symbol if necessary
      {
        if (push_Symbol(tloc))   return true;   // )SI pushed (e.g. niladic)
        if (statement_start)   check_dangling_function();
        return false;
      }

   if (tcl == TC_ASSIGN)     // update assign_state (from right to left)
      {
        if (get_assign_state() != ASS_none)   syntax_error(LOC);
        set_assign_state(ASS_arrow_seen);
      }

   push(tloc);
   if (statement_start)   check_dangling_function();
   return false;   // )SI not pushed
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::check_dangling_function()
{
   // A function (or operator) at the right end of a statement can never
   // get its right argument, so the statement is malformed. Detect that
   // before anything to its left is evaluated: otherwise parenthesized
   // assignments, function calls, or niladic functions left of it were
   // executed before the SYNTAX ERROR (Blake McBride, Bugs37 #34).
   //
   // Note: a function before a ) is not checked since it can be valid
   // (e.g. as an operand).
   //
   // return true if tok is a function or operator (but not a niladic one)
   //
   struct _ { static bool is_fun_or_oper(const Token & tok)
      {
        switch(tok.get_Class())
           {
             case TC_FUN12:
             case TC_OPER1:
             case TC_OPER2: return true;

             case TC_SYMBOL:   // a defined function or operator?
                  {
                    const Symbol * sym = tok.get_sym_ptr();
                    const Function * fun = sym ? sym->get_function() : 0;
                    return fun && (fun->get_oper_valence() ||
                                   fun->get_fun_valence());
                  }

             default: return false;
           }
      } };

   // FOO←{...} assigns a lambda (a function) to FOO. A function right of
   // ← is therefore not checked here.
   //
   if (PC < body.end_PC() && body[PC].get_Class() == TC_ASSIGN)   return;

const Token * fun_tok = 0;   // the dangling function or operator, if any
const Token & tok = at(0).get_token();
   switch(tok.get_Class())
      {
        case TC_FUN12:   // e.g. 5 +
        case TC_OPER1:   // e.g. +/
        case TC_OPER2:   // e.g. +.
             fun_tok = &tok;
             break;

        case TC_INDEX:   // e.g. ,[1] (constant axis) as opposed to X[1]
             if (tok.get_tag() != TOK_MARKER && PC < body.end_PC() &&
                 _::is_fun_or_oper(body[PC].get_token()))
                fun_tok = &body[PC].get_token();
             break;

        case TC_R_BRACK:   // e.g. ,[I] (axis) as opposed to X[I] (index)
             {
               int depth = 1;
               for (Prefix_PC pc = PC; pc < body.end_PC(); ++pc)
                   {
                     const Token & bt = body[pc].get_token();
                     if (bt.get_Class() == TC_END)   return;   // no [
                     if (bt.get_Class() == TC_R_BRACK)   ++depth;
                     else if (bt.get_tag() == TOK_L_BRACK && --depth == 0)
                        {
                          if (pc + 1 < body.end_PC() &&
                              _::is_fun_or_oper(body[pc + 1].get_token()))
                             fun_tok = &body[pc + 1].get_token();
                          break;
                        }
                   }
             }
             break;

        default: return;   // a value, ), etc.
      }

   if (fun_tok == 0)   return;

UCS_string fun_name;
   if (fun_tok->get_Class() == TC_SYMBOL)
      fun_name = fun_tok->get_sym_ptr()->get_name();
   else if (fun_tok->is_function())
      fun_name = fun_tok->get_function()->get_name();

   MORE_ERROR() << "Missing mandatory right argument of function "
                << fun_name << " (at the right end of the statement)";

   // syntax_error() and the line number of the error (StateIndicator::
   // get_line()) expect PC after the end of the failed statement.
   //
   while (PC < body.end_PC())
      {
        const TokenClass tc = body[PC++].get_Class();
        if (tc == TC_END || tc == TC_RETURN)   break;
      }
   syntax_error(LOC);
}
//────────────────────────────────────────────────────────────────────────────
inline void
Prefix::find_best_phrase()
{
const int s_max = min(ssize(), 4);   // available tokens on the stack
   best_phrase = 0;  // no best phrase

   /* compute s_max hash values from the available token classes:
      That is:

      hash₀ = TC₀
      hash₁ = TC₀ <<  5 | TC₁
      hash₂ = TC₀ << 10 | TC₁ << 5  | TC₂
      hash₃ = TC₀ << 15 | TC₁ << 10 | TC₂ << 5 | TC₃
    */
unsigned int hash = Class_at(0);   // hash₀ = TC₀
 {
   const Phrase & phrase_0 = hash_table[hash % PHRASE_MODU];
   if (phrase_0.phrase_hash == hash)   best_phrase = &phrase_0;
 }
 if (s_max == 1)   return;   // no more tokens available
 hash |= Class_at(1) << 5;   // hash₁ = TC₀:TC₁
 {
   const Phrase & phrase_1 = hash_table[hash % PHRASE_MODU];
   if (phrase_1.phrase_hash == hash)   best_phrase = &phrase_1;
 }
 if (s_max == 2)   return;   // no more tokens available
 hash |= Class_at(2) << 10;   // hash₂ = TC₀:TC₁:TC₂
 {
   const Phrase & phrase_2 = hash_table[hash % PHRASE_MODU];
   if (phrase_2.phrase_hash == hash)   best_phrase = &phrase_2;
 }
 if (s_max == 3)   return;   // no more tokens available
 hash |= Class_at(3) << 15;   // hash₃ = TC₀:TC₁:TC₂:TC₃
 {
   const Phrase & phrase_3 = hash_table[hash % PHRASE_MODU];
   if (phrase_3.phrase_hash == hash)   best_phrase = &phrase_3;
 }
}
//────────────────────────────────────────────────────────────────────────────
inline bool
Prefix::bind_to_next()
{
TokenClass next = body[PC].get_Class();   // assume no next
   if (next == TC_SYMBOL)
      {
        const Symbol * symbol = body[PC].get_sym_ptr();
        const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
        next = symbol->resolve_class(is_left_sym);
      }

    if (best_phrase->misc && (at0().get_Class() == TC_R_BRACK))
       {
         // the next symbol is a ] and the matching phrase is a MISC
         // phrase (monadic call of a possibly dyadic function).
         // The ] could belong to:
         //
         // 1. an indexed value,        e.g. A[X] or
         // 2. a function with an axis, e.g. +[2]
         //
         // These cases lead to different reductions:
         //
         // 1.  A[X] × B   should evalate × dyadically, while
         // 2.  +[1] × B   should evalate × monadically,
         //
         // We solve this by computing the indexed value first
         //
         if (is_value_bracket())   // case 1.
            {
              // we call reduce_RBRA____, which pushes a partial index list
              // onto the stack. The following tokens are processed until the
              // entire indexed value A[ ... ] is computed
              prefix_len = 1;
              reduce_RBRA___();
              return true;   // do bind
            }

         // case 2: fall through
       }

//   Q1(next) Q1(at0())

   // shift/reduce conflict. See what to do: bind (= shift) or not.
   //
   if (do_shift(next))   // i.e. shift
      {
        Log(LOG_prefix_parser)  CERR
             << "   phrase #" << best_phrase->phrase_number
             << ": " << best_phrase->phrase_name
             << " matches, but prio " << best_phrase->prio
             << " is too small to call " << best_phrase->reduce_name
             << "()" << endl;
        return true;   // do bind
      }

   return false;    // don't bind
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::check_interrupt_or_attention(bool end_of_line)
{
   if (InterruptContext::attention_is_raised() && end_of_line)
      {
        const bool int_raised = InterruptContext::interrupt_is_raised();
        InterruptContext::clear_attention_raised(LOC);
        InterruptContext::clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::push_END_error()
{
   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

   // provide help on some common cases...
   //
   for (int j = 1; j < ssize() - 1; ++j)
       {
         if ( (at(j)    .get_Class() == TC_ASSIGN) &&
              (at(j + 1).get_Class() == TC_VALUE))
            {
              const TokenClass left = at(j - 1).get_Class();
              if (is_function_class(left))
                 {
                    MORE_ERROR() <<
                    "Cannot assign a value to a function";
                 }
              else if (is_operator_class(left))
                 {
                    MORE_ERROR() <<
                    "Cannot assign a value to an operator";
                 }
            }
       }

   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

UCS_string & more = MORE_ERROR();
   more << "At the left end of statement: invalid phrase remaining:";

   // print token, but no more than 4
   //
   enum { MAX_j = 4 };   // limit for the number of tokens displayed
   loop(j, MAX_j)
       {
         const bool rightmost = (j == ssize() - 1);   // end of phrase
         const Token & tok = at(j).get_token();
         more << " ";
         if (tok.is_function())   // token is a function
            {
              const Function * fun = tok.get_function();
              more << fun->get_name();
              if (rightmost)   // rightmost token of the stack is a function
                 {
                   if (MAX_j < ssize())   more << "...";

                   // frequent error: missing right argument of a non-niladic
                   // function.
                   more << "\nMissing mandatory right argument of function "
                        << fun->get_name() << "?";

                   // Bugs28 #26 residual: this VALENCE_ERROR abandons the
                   // statement the same way syntax_error()'s fallthrough
                   // a few lines down does, but (being a separate, raw
                   // throw_apl_error() site) never got its FIFO-cleanup
                   // sweep -- confirmed leaking one Value_P per hit (via
                   // )CHECK) for e.g. "+⍣1 1⍤1" (a derived function
                   // built earlier in this same statement, still sitting
                   // in the FIFO as the LO of the never-completed outer
                   // combination, whose own construction, unstrand_RO_B(),
                   // is what allocated it).
                   //
                   destroy_derived_in_FIFO();
                   VALENCE_ERROR;
                 }
            }
         else
            {
              // the end of a ⍎ context (TOK_RETURN_EXEC etc.) is shown as
              // ENDL, like the end of a ◊ or ∇ statement (Bugs37 #43)
              //
              const UCS_string name = at(j).get_Class() == TC_RETURN
                                    ? UCS_string(U"ENDL")
                                    : at(j).get_token().tag_name();
              if (name.starts_with("TOK_"))   more << UCS_string(name, 4);
              else
                 more << name;
              if (rightmost && MAX_j < ssize())   more << "...";
            }

         if (rightmost)   break;
       }

   syntax_error(LOC);   // no more tokens
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::push_member_chain(const Token_loc & tl, Symbol * symbol)
{
   /* Bugs28 #55: 'symbol' is dot-adjacent (body[PC] is the '.' just to
      its left in source order, checked by the caller) and not itself a
      function/operator -- i.e. it is the first (rightmost-encountered)
      name of a genuine value-member chain "A.B.C...symbol", not an f.g
      inner product. Called only when something with strong-enough
      binding priority (currently: a pending bracket index, e.g.
      "S.b[1]") is already on the stack -- exactly the condition under
      which an ordinary (non-member) symbol already takes its own
      "grab the value directly" fast path a few lines below in
      push_Symbol() (the ssize() && at0()==TC_INDEX check there). For a
      member symbol that same eager reduction (Prefix::reduce_V_C__(),
      phrase "V C") would otherwise fire on the bare symbol -- calling
      resolve_lv() on a name that was never a real global variable --
      before the '.' ever gets a chance to combine it into a proper
      member reference. Resolve the whole chain right here instead
      (mirroring Prefix::reduce_D_V__()'s member-reference branch, just
      triggered earlier) and push a plain value, or -- when this
      reference will itself be selectively assigned or assigned through
      a further index -- an lvalue-capable cellref array, in place of
      the bare symbol. Either way, the result is an ordinary VALUE
      token, so every existing value-based phrase (A C, A ASS B, A C
      then ASS B, ...) applies to it afterward exactly as it already
      does for any other already-resolved value or symbol.

      A DIRECT, immediately-adjacent chain assignment ("A.B.C←value",
      nothing else pending) never reaches here: do_shift() already
      defers shifting a bare dot-followed symbol past a not-yet-formed
      "V ASS B" phrase (its TC_SYMBOL + next==TC_OPER2 case), so
      "D V ASS B" still forms normally and reduce_D_V__() still handles
      that case exactly as before, unaffected by this addition.
    */
vector<const UCS_string *> members;
Symbol * top_sym = 0;
const Token_loc * top_loc = 0;   // the Token_loc of top_sym in body
   members.push_back(symbol->get_name_ptr());

   // ⎕CR.subfun / ⎕FIO.subfun (not a real member chain -- ⎕CR/⎕FIO are
   // functions, not variables): leave that rare pattern to
   // reduce_D_V__() at its normal (later) reduce time, which already
   // handles it correctly, instead of duplicating it here.
   //
   if (!(PC + 1 < body.end_PC() && body[PC + 1].get_Class() == TC_SYMBOL))
      {
        push(tl);
        return;
      }

   // skip the '.' itself (body[PC]) -- the loop below mirrors
   // reduce_D_V__()'s own while loop, which starts with PC already
   // past the initial "D V" pair for the SAME reason.
   //
   PC = PC + 1;
   while (PC + 1 < body.end_PC())
      {
        if (body[PC].get_Class() == TC_SYMBOL)
           {
             Symbol * sym = body[PC].get_sym_ptr();
             members.push_back(sym->get_name_ptr());
             if (body[PC + 1].get_tag() == TOK_OPER2_INNER)
                {
                  PC = PC + 2;
                }
             else
                {
                  top_sym = sym;
                  top_loc = &body[PC];
                  PC = PC + 1;
                  break;
                }
           }
        else
           {
             MORE_ERROR() << "member access: missing variable name";
             syntax_error(LOC);
           }
      }

   if (top_sym == 0)
      {
        MORE_ERROR() << "member access: no top-level variable name";
        syntax_error(LOC);
      }

// the result replaces the whole chain A.B.C...symbol, so it gets the PC of
// its leftmost token: the top-level name A, not that of symbol
//
const Token_loc & pc_top = *top_loc;

Value_P top_val = top_sym->get_var_value();
   if (!top_val)
      {
        UCS_string & more = MORE_ERROR()
               << "member access: missing top-level variable "
               << top_sym->get_name() << " for member ";
        more.append_members(members, 0);
        more << " not found";
        VALUE_ERROR;
      }

const bool will_selectively_assign = get_assign_state() == ASS_arrow_seen;
   if (will_selectively_assign)
      {
        // See the matching comment in reduce_D_V__(): isolate the
        // Symbol's own stored value in place, then re-fetch, before
        // get_existing_member() takes a pointer into its structure.
        //
        top_sym->top_of_stack()->isolate_deep(LOC);
        top_val = top_sym->get_var_value();
      }

const Cell * member_cell = top_val->get_existing_member(members);
   Assert(member_cell);

   if (member_cell->is_member_anchor() && will_selectively_assign)
      {
        UCS_string & more = MORE_ERROR() <<
                     "member access: cannot use non-leaf member ";
        more.append_members(members, 0);
        more << " in selective specification.\n"
                "      )ERASE or ⎕EX that member first.";
        DOMAIN_ERROR;
      }

   // Token has no safe operator=() (it is a raw-union class relying on
   // placement-new / copy() / move_from(), like the rest of this file
   // already carefully does elsewhere) -- construct and push the result
   // Token_loc directly in each branch below rather than building up a
   // shared, reassigned local Token, which silently corrupts whatever
   // Value_P the union was already holding.
   //
   if (member_cell->is_pointer_cell())
       {
         if (will_selectively_assign)
            {
              set_assign_state(ASS_none);
              Value_P cell_refs = member_cell->get_pointer_value()
                                            ->get_cellrefs(LOC);
              const Token_loc tloc_result(Token(TOK_APL_VALUE2, cell_refs),
                                          pc_top);
              push(tloc_result);
            }
         else
            {
              Value_P Z(CLONE_P(member_cell->get_pointer_value(), LOC),
                                LOC);
              const Token_loc tloc_result(Token(TOK_APL_VALUE1, Z),
                                          pc_top);
              push(tloc_result);
            }
       }
   else if (will_selectively_assign)   // simple leaf member, e.g. scalar S.a
      {
        // Bugs31 #26 (Blake McBride): like the is_pointer_cell() branch
        // above (and like the leaf-member branch of reduce_D_V__(), see
        // Bugs29 #4), a selective specification such as S.a[]←7 needs a
        // proper lvalue (an LvalCell referring to the member's cell), not
        // a disconnected read-only copy -- the copy made S.a[]←7 a RANK
        // ERROR (rank 0, no LvalCell) while A[]←7 with a scalar A worked.
        //
        set_assign_state(ASS_none);
        Value * owner = 0;
        Cell * mut_cell = top_val->get_member(members, owner, true);
        Assert(mut_cell);
        Value_P cell_ref(LOC);
        const LvalCell lval_cell(mut_cell, owner);
        cell_ref->next_ravel_Cell(lval_cell);
        cell_ref->check_value(LOC);
        cell_ref->set_left_value();   // like Value::get_cellrefs() (Bugs32 #17)
        const Token_loc tloc_result(Token(TOK_APL_VALUE2, cell_ref),
                                    pc_top);
        push(tloc_result);
      }
   else                                // simple leaf member, reference only
      {
        Value_P Z(LOC);
        Z->next_ravel_Cell(*member_cell);
        Z->check_value(LOC);
        const Token_loc tloc_result(Token(TOK_APL_VALUE1, Z), pc_top);
        push(tloc_result);
      }
}
//────────────────────────────────────────────────────────────────────────────
inline bool
Prefix::push_Symbol(const Token_loc & tl)
{
   // the symbol is resolved into a new token which is then pushed with the
   // PC of tl (tl itself remains unchanged).
   //
Token resolved(tl.get_token());

   if (tl.get_tag() == TOK_LSYMB2)
      {
        // tl.tok is the last token C of a vector assignment (A B ... C)←.
        // Return C and let reduce_V_RPAR_ASS_B() do the rest
        //
        const Symbol * symbol = tl.get_token().get_sym_ptr();
        symbol->resolve_left(resolved, tl);

        LOG_prefix_parser && CERR << "TOK_LSYMB2 " << symbol->get_name() <<
                "resolved to " << resolved << " at " << LOC  << endl;
        push(Token_loc(resolved, tl));
        return false;
      }

Symbol * const symbol = tl.get_token().get_sym_ptr();
   if (PC < body.end_PC() && body[PC].get_tag() == TOK_OPER2_INNER)
      {
        /* The APL code is .SYM which could be:
 
           1. a normal inner product f.SYM, or
           2. a value member VAL.SYM

           We check the name class of symbol to decide.
         */
        const NameClass nc = symbol->get_NC();
        if (nc != NC_FUNCTION && nc != NC_OPERATOR)   // case 2: value member
           {
             // Bugs28 #55: something with strong-enough binding priority
             // (currently: a pending bracket index) is already on the
             // stack and would otherwise eagerly bind to the bare
             // symbol -- as if it were an ordinary global variable --
             // before the '.' ever gets a chance to combine it into a
             // proper member reference. Resolve the whole chain now
             // instead; see push_member_chain()'s own comment.
             //
             if (ssize() && at0().get_Class() == TC_INDEX)
                {
                  push_member_chain(tl, symbol);
                  return false;
                }

             push(tl);
             return false;   // )SI not pushed
           }
      }

   if (get_assign_state() == ASS_arrow_seen)
      {
        // symbol is the first symbol left of ← (in APL order).
        // allow assignment only to variables or undefined names.
        //
        // There is a common, but hard to understand pitfall: a label with
        // the same name exists. We should then provide some more info.
        //
        const NameClass nc = symbol->get_NC();
        if (!(nc & NC_left))   // error
           {
             const char * sym_nc = "???";
             cFunction_P defined_fun = 0;
             switch(nc)
                {
                   case NC_LABEL:      sym_nc = "label";              break;
                   case NC_OPERATOR:   defined_fun = symbol->get_function();
                                       sym_nc = "defined operator";   break;
                   case NC_FUNCTION:   defined_fun = symbol->get_function();
                                       sym_nc = "defined function";   break;
                   case NC_SYSTEM_FUN: sym_nc = "system function";    break;
                   default: FIXME;
                }

             if (defined_fun && defined_fun->is_lambda())
                {
                  MORE_ERROR() << "Assignment to symbol " << symbol->get_name()
                               << " which (currently) is a named lambda.\n"
                  "    You may want to ⎕EX '" << symbol->get_name() << "' first.";
                }
             else
                {
                  MORE_ERROR() << "Assignment to symbol " << symbol->get_name()
                               << " which (currently) is a " << sym_nc;
                }
             syntax_error(LOC);
           }

        // Bugs28 #60: resolve_left() below is a no-op for the common
        // case (NC_VARIABLE) -- it just validates the name and leaves
        // the symbol on the stack as a bare TC_SYMBOL, relying on
        // Prefix::reduce_V_RPAR_ASS_B() (phrase "V RPAR ASS B") to
        // actually resolve it to an lvalue moments later. That phrase
        // only matches when EXACTLY one ')' separates this symbol from
        // the real "← B" (e.g. "(A)←5"). For a selective specification
        // whose target expression contains its OWN internal grouping
        // parens around a sub-expression -- e.g. "(1↑(2/A))←7", where
        // "(2/A)" is grouped for clarity, not redundantly wrapping the
        // whole target -- TWO (or more) ')' are already on the stack by
        // the time this symbol is reached, "V RPAR ASS B" never matches
        // at all, and the symbol is left bare -- so once "/" (or
        // whatever function needs it) tries to use it as an argument,
        // the phrase matcher sees a SYMBOL where "A F B" needs a VALUE,
        // silently falls back to a shorter, wrong match (misreading
        // "2/" alone as a deferred derived-function build), and the
        // whole statement gets stuck in a bare SYNTAX ERROR.
        //
        // Detect this directly: if at least two ')' are ALREADY on the
        // stack (exactly one is the already-handled "(A)←..."/vector-
        // assignment case, left untouched below) and the very next
        // token to be shifted is not itself a symbol -- ruling out a
        // vector assignment "(T U V)←..." (or a redundantly double-
        // wrapped one) still being collected, and the sibling
        // selective-specification-via-selecting-function form "(F V)←"
        // (already handled separately by the generic "F V" phrase,
        // Prefix::reduce_F_V__()) -- this symbol is definitely embedded
        // inside a larger selective-specification target and needs its
        // lvalue now, regardless of how many more ')' still separate it
        // from the real "← B": those strip away transparently
        // afterward via ordinary "(value)" grouping, and the already-
        // generic Prefix::reduce_A_ASS_B_() ("A ASS B") handles the
        // final assignment into whatever lvalue-cellref array survives
        // -- exactly as it already does for e.g. (⊃E)[]←0.
        //
        if (ssize() >= 2                               &&
            at0().get_Class() == TC_R_PARENT           &&
            at1().get_Class() == TC_R_PARENT           &&
            !(PC < body.end_PC() && body[PC].get_Class() == TC_SYMBOL))
           {
             const Token result = symbol->resolve_lv(LOC);
             set_assign_state(ASS_var_seen);
             push(Token_loc(result, tl));
             return false;
           }

        symbol->resolve_left(resolved, tl);
        set_assign_state(ASS_var_seen);
        push(Token_loc(resolved, tl));
        return false;
      }

   if (ssize()                       &&          // at0() is valid,
       at0().get_Class() == TC_INDEX &&          // at0() is [...;... ], and
       tl.get_tag() == TOK_SYMBOL)   // user defined variable
      {
        // indexed reference, e.g. A[N]. Calling symbol->resolve()
        // would copy the entire variable A and then index it, which
        // is inefficient if the variable is big. We rather call
        // Symbol::get_var_value() directly in order to avoid that
        //
        if (Value_P value = symbol->get_var_value())
           {
             Token tok(TOK_APL_VALUE1, value);
             resolved.move_from(tok, LOC);
           }
       else
          {
            symbol->resolve_right(resolved, tl);
           }
      }
   else
      {
        symbol->resolve_right(resolved, tl);
      }

   Log(LOG_prefix_parser)
      {
        CERR << "TOK_SYMBOL resolved to " << resolved
             << " at " << LOC  << endl
             << "   resolved symbol " << symbol->get_name()
             << " to " << resolved.get_Class() << endl;
      }

   // Quad_Quad::resolve() calls ⍎ which may return TOK_SI_PUSHED.
   //
   push(Token_loc(resolved, tl));
   return resolved.get_tag() == TOK_SI_PUSHED;   // )SI not pushed
}
//────────────────────────────────────────────────────────────────────────────
inline bool
Prefix::MM_is_FM(Prefix_PC pc)
{
   /*
      dis-ambiguate / ⌿ \ or ⍀.

      return true if M M shall actually be F M in reduce_M_M__().
      PC was alredy incremented and points to the token left of M M:

       On entry;
                             ┌─────────────────── PC
                             │      ┌──────────── PC-1   i.e. M1
                             │      │        ┌─── PC-2   i.e. M2
       ┌────────┬───   ───┬──────┬──────┬──────────┬───
       │ TC_END │   ...   │ NEXT │ /⌿\⍀ | TC_OPER1 │  (B...)
       └────────┴───   ───┴──────┴──────┴──────────┴───

       NOTE: if M1 is an operator then its left argument must either be a
             value or a function. Therefore the token in metaclass MISC,
             i.e. ← → ; [ END or ( implies that M1 shall be a function F
             (which, unless M1 / ⌿ \ or ⍀) will rise a syntax error in
             reduce_F_M__().
    */
   for (;;)
       {
         const Token & NEXT = body[pc];
         switch(const TokenClass tc_NEXT = NEXT.get_Class())
            {
                                    // Examples:    ┌─────── NEXT
                                    //              │ ┌───── M1 is / ⌿ \ or ⍀
                                    //              │ │ ┌─── M2 any operator
                                    //              │ │ │
              case TC_ASSIGN:       //            Q ← / ⍨ 1 2 3         (MISC)
              case TC_R_ARROW:      //              → / ⍨ 1 2 3         (MISC)
              case TC_L_BRACK:      //              [ / ⍨ 1 2 3         (MISC)
                                    //              ; / ⍨ 1 2 3         (MISC)
              case TC_L_PARENT:     //              ( / ⍨ 1 2 3         (MISC)
              case TC_END:          //              ◊ / ⍨ 1 2 3         (MISC)
              case TC_FUN0:         //            FOO / ¨ ⊂ 'abc
              case TC_OPER2:        //            FOO / ¨ ⊂ 'abc
              case TC_RETURN:       //                / ⍨ 1 2 3
              case TC_VALUE:        // (1 0 1)(0 1 1) / ¨ ⊂ 'abc
                   return true;     // M1 is F       │ │ │
                                    //               │ │ │
              case TC_FUN12:        //               + / ¨ (1 2)(3 4)(5 6)
              case TC_OPER1:        // M1 is M       / / ¨ (1 2)(3 4)(5 6)
                   return false;    // MM is operator

              case TC_SYMBOL:
                   return NEXT.get_sym_ptr()->M_is_F();

              // most likely an indexed value
              case TC_INDEX:
                   return true;   // MM is function

              case TC_R_BRACK:   // skip over [ ... ]
                   pc = pc + int(body[pc].get_int_val2());
                   continue;

              case TC_R_PARENT:
                   pc = pc + 1;
                   continue;

              default: CERR << "NEXT: " << tc_NEXT << endl;
                       TODO;
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
/// true iff the top of the SI stack is a genuine internal macro (Macro.def).
/// The ⎕EA/⎕EB helper macros use this to gate the private ⎕ES 100 \<magic\>
/// branch/escape/commit/error protocol below -- a capability that must
/// never be reachable from user code -- via Function::is_macro() rather
/// than the macro's (cosmetic, Macro.def-only) APL header name.
static bool
called_from_macro()
{
const UserFunction * ufun = Workspace::SI_top()->get_executable()->get_exec_ufun();
   return ufun && ufun->is_macro();
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::handle_QUAD_ES_COM(const Token & result)
{
   // make sure that ⎕ES was called from a macro (which implies
   // that it has a parent).
   //
   if (!called_from_macro())   DOMAIN_ERROR;

   Workspace::pop_SI(LOC);   // discard ⎕EA/⎕EB context

Cell cache;
const Cell & QES_arg2 = result.get_apl_val()->get_cravel(2, cache);
Prefix & caller = Workspace::SI_top()->get_prefix();
   if (Value_P val = QES_arg2.try_pointer_value())
      {
        caller.replace_SI_PUSHED(Token(TOK_APL_VALUE2, val));
      }
   else
      {
        Value_P scalar(LOC);
        scalar->next_ravel_Cell(QES_arg2);
        scalar->check_value(LOC);
        caller.replace_SI_PUSHED(Token(TOK_APL_VALUE2, scalar));
      }
}
//════════════════════════════════════════════════════════════════════════════
void
Prefix::handle_QUAD_ES_ESC()
{
   // make sure that ⎕ES was called from a macro (implies parent)
   //
   if (!called_from_macro())   DOMAIN_ERROR;

   Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context

Prefix & caller = Workspace::SI_top()->get_prefix();
   caller.replace_SI_PUSHED(Token(TOK_ESCAPE));
   caller.escape_pending = true;   // see reduce_body()
}
//════════════════════════════════════════════════════════════════════════════
void
Prefix::handle_QUAD_ES_BRA(const Token & result)
{
   // make sure that ⎕ES was called from a macro (implies parent)
   //
   if (!called_from_macro())   DOMAIN_ERROR;

   Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context

const cValue & QES_val = *result.get_apl_val();

   // If ⎕EA/⎕EB was called inside a macro-implemented operator (e.g.
   // A ⎕EA¨ B or A ∘.⎕EB B), then the new SI top is that (host) macro.
   // Its lines are internal and B's →N must never jump into them: →1/→2
   // re-ran the macro's loop forever, and →0 returned its half-initialized
   // result (Blake McBride, Bugs32 #2).
   //
const bool host_is_macro = called_from_macro();

   // B's →N must not jump into a lambda host: the jump discarded the
   // lambda's pending λ← tokens, and the lambda then either looped forever
   // (→1) or ended in an internal error that exited the interpreter
   // (Bugs34 #3). Treat it like a macro host (i.e. ⎕EA falls back to A).
   //
const UserFunction * host_ufun =
      Workspace::SI_top()->get_executable()->get_exec_ufun();
const bool host_is_lambda = host_ufun && host_ufun->is_lambda();

   // ⎕EA's BRA payload is 100 $FFFD (⊂,A) RES (4 items, A included, see
   // Macro.def); ⎕EB's is 100 $FFFD RES (3 items, no A -- ⎕EB always
   // executes A unconditionally itself via its own "⍎A", so it has no
   // use for a fallback here). Tell them apart by length rather than
   // touching ⎕EB's independent macro/semantics.
   //
   if (QES_val.element_count() < 4)   // ⎕EB: unchanged historical behaviour
      {
        if (host_is_macro)
           {
             MORE_ERROR() << "A ⎕EB B: B's →N cannot branch out of an "
                             "operator (such as ¨ or ∘.)";
             SYNTAX_ERROR;
           }

        if (host_is_lambda)
           {
             MORE_ERROR() << "A ⎕EB B: B's →N cannot branch out of a "
                             "lambda";
             SYNTAX_ERROR;
           }

        Cell cache;
        const Cell & QES_line = QES_val.get_cravel(2, cache);
        Value_P v_line = IntScalar(QES_line.get_int_value(), LOC);
        const Token jump_result = Workspace::SI_top()->jump(*v_line);

        // in immediate execution, the →N leaves the context (which the
        // caller's pending TOK_SI_PUSHED then reported as an internal
        // "invalid phrase remaining", Bugs34 #35)
        //
        if (jump_result.get_tag() == TOK_BRANCH_INT)
           {
             MORE_ERROR() << "A ⎕EB B: B's →N cannot branch in immediate "
                             "execution";
             SYNTAX_ERROR;
           }
        return;
      }

UCS_string statement_A(*QES_val.get_pointer_value(2));
Cell cache;
const Cell & QES_line = QES_val.get_cravel(3, cache);
const APL_Integer line = QES_line.get_int_value();

   if (host_is_macro)   // same as →N out of range: fall back to A
      {
        execute_EA_fallback(statement_A, E_SYNTAX_ERROR,
                       "B's →N cannot branch out of an operator (such as ¨)");
        return;
      }

   if (host_is_lambda)   // same as →N out of range: fall back to A
      {
        execute_EA_fallback(statement_A, E_SYNTAX_ERROR,
                            "B's →N cannot branch out of a lambda");
        return;
      }

StateIndicator * caller = Workspace::SI_top();
Value_P v_line = IntScalar(line, LOC);

   // StateIndicator::jump() only computes what should happen and
   // returns a Token describing it (see its own comment) -- the actual
   // jump is performed by whoever called it. The two normal (non-⎕EA)
   // call sites in this file (reduce_END_GOTO_B_/reduce_RETC_GOTO_B_)
   // act on that Token themselves; this one used to just discard it,
   // silently dropping the branch instead of propagating it (Blake
   // McBride, LanguageVariances.md #27).
   //
const Token jump_result = caller->jump(*v_line);

   if (jump_result.get_tag() == TOK_VOID)   // a real →N within the caller
      {
        // goto_PC() already updated the caller's PC as a side effect;
        // abandon the statement that called ⎕EA so the caller resumes
        // at the new PC instead.
        //
        caller->get_prefix().reset(LOC);
        return;
      }

   // out of range for the caller too (e.g. →0, or →N nowhere in it):
   // per lrm p.349 Figure 38, that means "flow of execution returns to
   // the invoking expression" -- fall back to A, exactly as if B had
   // failed outright (handle_QUAD_ES_ERR() below).
   //
   execute_EA_fallback(statement_A, E_SYNTAX_ERROR,
                       "B's →N was out of range for both B and its caller");
}
//════════════════════════════════════════════════════════════════════════════
void
Prefix::handle_QUAD_ES_ERR(const Token & result)
{
   // this case can only occur with ⎕EA, but not with ⎕EB.

   // make sure that ⎕ES was called from a macro (implies parent)
   //
   if (!called_from_macro())   DOMAIN_ERROR;

   Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context

const cValue & QES_val = *result.get_apl_val();
UCS_string statement_A(  *QES_val.get_pointer_value(2));
const APL_Integer major = QES_val.get_int_value(3);
const APL_Integer minor = QES_val.get_int_value(4);
const ErrorCode ec      = ErrorCode(major << 16 | minor);

   // Bugs28 #56: element 5 (Macro.def's ⊂μ9) is B's own 3-line,
   // ⎕EM-shaped message -- row 0 is the error name (redundant with
   // major/minor above), row 1 is the failed statement, row 2 is the
   // caret line (spaces and '^'s). Recover row 1 and the caret
   // positions (by finding the '^'s in row 2, the same way
   // Error::get_error_line_3() renders them, run in reverse) so
   // execute_EA_fallback() can set them on the Error it constructs --
   // otherwise ⎕EM shows 2 blank rows below B's error name.
   //
UCS_string line2;
int lcaret = -1, rcaret = -1;
bool have_line2 = false;
   if (QES_val.element_count() > 5)
      {
        if (Value_P msg = QES_val.get_pointer_value(5))
           {
             const ShapeItem cols = msg->get_cols();
             if (msg->get_rows() >= 3 && cols > 0)
                {
                  have_line2 = true;
                  loop(c, cols)
                      line2 << msg->get_char_value(cols + c);   // row 1

                  loop(c, cols)
                      {
                        if (msg->get_char_value(2*cols + c) != UNI_CIRCUMFLEX)
                           continue;
                        if (lcaret < 0)   lcaret = c;
                        else              rcaret = c;
                      }
                  if (lcaret >= 0 && rcaret < 0)   rcaret = lcaret;
                }
           }
      }

   execute_EA_fallback(statement_A, ec, "B failed to execute",
                       have_line2 ? &line2 : 0, lcaret, rcaret);
}
//════════════════════════════════════════════════════════════════════════════
void
Prefix::execute_EA_fallback(UCS_string statement_A,
                            ErrorCode ec_on_failure,
                            const char * why,
                            const UCS_string * b_line2,
                            int b_lcaret, int b_rcaret)
{
StateIndicator * top = Workspace::SI_top();
Prefix & top_prefix = top->get_prefix();
   Assert(top_prefix.at0().get_tag() == TOK_SI_PUSHED);

   // Blake McBride, Bugs31 #7 (EA half): originally "fixed" by catching
   // E_COMMAND_PUSHED here and turning an SI-modifying A (e.g. ')OFF'
   // ⎕EA '1÷0') into a DOMAIN_ERROR instead of letting it run. Reverted
   // (user, 2026-09-29): that was based on a mistaken premise. ⎕EC/⎕EA/
   // ⎕EB protect against EXECUTION ERRORS surfacing in a controllable
   // way, not against a program doing exactly what it was written to
   // do -- ')OFF' ⎕EA INIT ("run INIT; if it fails, end the session
   // instead of continuing in a half-initialized workspace") is a
   // legitimate, intentional pattern, and ⎕EA's whole contract is that
   // A executes exactly as if it had been ⍎'d directly (unlike B, which
   // IS sandboxed via ⎕EC). E_COMMAND_PUSHED is not a hazard to guard
   // against here: it is a normal C++ exception that Bif_F1_EXECUTE.cc
   // throws for ANY SI-modifying command reached via ⍎ at ANY call
   // depth, and it already unwinds safely all the way to Command.cc's
   // main loop (the only place that catches it) before the real command
   // ever runs -- confirmed generic and pre-existing, independent of
   // ⎕EA/⎕EB (e.g. a plain defined function calling ⍎')CLEAR' two levels
   // deep already unwinds and clears correctly, with no special-casing
   // anywhere). Catching it here just short-circuited that already-safe
   // mechanism one frame too early. A is therefore executed with no
   // special-casing at all, exactly like any other statement.
   //
   // plain Token operator= is the compiler-generated default (shallow,
   // refcount-unaware) -- move_from() from a properly copy-constructed
   // temporary is the refcount-safe way to capture a returned Token
   // (same convention used elsewhere in this file, e.g. Quad_EC::eval_B()'s
   // own cmd_result.move_from(tmp, LOC)).
   //
Token tmp = Bif_F1_EXECUTE::execute_statement(statement_A);
Token result_A;
   result_A.move_from(tmp, LOC);

   // Record why B failed onto the caller's (this ⎕EA macro's own SI
   // frame's) error slot, and into )MORE, unconditionally -- timed
   // AFTER execute_statement() returns, not before: execute_statement()
   // always calls ExecuteList::fix() (Executable.cc) first, which
   // unconditionally clears this same slot as its own "clear any errors
   // that may have occurred before" bookkeeping, so setting it any
   // earlier gets silently wiped before A ever runs.
   //
   // Quad_EM/Quad_ET::get_apl_value() (SystemVariable.cc) walk the
   // CURRENT SI frame first, then parents, stopping at the first
   // non-zero error code -- so if A itself folds to a plain value, or
   // to a child frame that never errors, this slot (the nearest
   // ancestor with an error) is what they find. If A's own execution
   // DOES later error, A's own (nearer) frame naturally wins that same
   // search instead -- "if both A and B fail, the visible error is A's"
   // falls out for free, no extra logic needed here. Likewise if A
   // itself fails to even PARSE: execute_statement() throws a real
   // SYNTAX_ERROR exception in that case (ExecuteList::fix() returning
   // 0), which unwinds straight past this function without recording
   // anything here at all, so A's own error is again what becomes
   // visible.
   //
   // The ⎕ET/⎕EM code itself is collapsed to the generic SYNTAX_ERROR
   // (2 2) whenever ec_on_failure's own major code is SYNTAX_ERROR's:
   // B's specific parser diagnostic (e.g. UNBALANCED_R_PARENT, major 2
   // minor 6658 for an unparseable B) is meaningful only within B's own
   // parsing context, not part of the stable, documented ⎕ET code space
   // a caller should rely on. That collapse throws away real
   // information though, so -- and ONLY in that case, i.e. only when
   // something is actually lost -- the specific reason is preserved in
   // )MORE. A plain (non-syntax-class) failure like DOMAIN_ERROR
   // already says everything via its own ⎕ET code, so )MORE is left
   // alone for it -- setting it unconditionally regressed existing
   // Quad_EA.tc/Quad_ES.tc cases where A also fails with its own,
   // unrelated plain error afterward (e.g. '⍳3.3' ⎕EA '⍳4.5'): this
   // function's own B-related )MORE text has no way to know A is about
   // to fail too and would linger, misleadingly, next to A's own
   // (already correct, and DOMAIN_ERROR-class so )MORE-less) error
   // report.
   //
ErrorCode ec_ET = ec_on_failure;
   if (Error::error_major(ec_on_failure) == Error::error_major(E_SYNTAX_ERROR))
      {
        MORE_ERROR() << "A ⎕EA B: " << why << " ("
                     << Error::error_name(ec_on_failure) << ")";
        ec_ET = E_SYNTAX_ERROR;
      }
   // Bugs28 #56 (Bugs27 #36 residual): a bare Error(ec_ET, LOC) only
   // ever carries the error CODE -- ⎕ET (which reads just the code)
   // was already right, but ⎕EM (which formats the statement text and
   // carets an Error also carries) showed 3 blank rows instead. Line 1
   // (the error name) still comes from ec_ET as before -- deliberately
   // so, since ec_ET may already be the generic, collapsed SYNTAX_ERROR
   // rather than B's own specific parser code, and line 1 should match
   // whichever code ⎕ET itself reports. Lines 2/3 (the failed statement
   // and its carets) are B's own, real text, when the caller
   // (handle_QUAD_ES_ERR()) was able to recover it from B's error
   // message before that context was discarded.
   //
   new (&StateIndicator::get_error(top)) Error(ec_ET, LOC);
   if (b_line2)
      StateIndicator::get_error(top).set_error_line_2(*b_line2,
                                                       b_lcaret, b_rcaret);

   if (result_A.get_Class() == TC_VALUE)   // A is a plain (foldable) value
      {
        top_prefix.replace_SI_PUSHED(result_A);
        return;
      }

   if (result_A.get_tag() == TOK_SI_PUSHED)
      {
        // A needed real execution (e.g. it is itself a branch/escape
        // statement, not a foldable value expression): let it run
        // normally, but mark its pushed )SI so that if its own →N/→
        // has nowhere real to go, that is a clean no-value completion
        // (lrm p.349 Figure 38: "flow of execution returns to the
        // invoking expression") instead of Command.cc's ordinarily
        // correct "→N without function" SYNTAX ERROR.
        //
        Workspace::SI_top()->set_void_on_orphan_branch();
        return;
      }

   // execute_statement() only ever returns TC_VALUE or TOK_SI_PUSHED
   // normally (see above for the exceptional, A-fails-to-parse case).
   //
   FIXME;
}
//════════════════════════════════════════════════════════════════════════════
void
Prefix::reduce____()
{
   // this function is a placeholder for invalid phrases and should never be
   // called.
   //
   print_stack(CERR, LOC);
   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_LPAR_B_RPAR_()
{
   Assert1(prefix_len == 3);

   // B is a Function or a Value. Make Values have tag TOK_APL_VALUE1
   //
   if (at1().get_Class() == TC_VALUE && at1().get_tag() != TOK_APL_VALUE1)
      {
        const Token result(TOK_APL_VALUE1, at1().get_apl_val());
        pop_args_push_result(result);
      }
   else
      {
        const Token & result = at1();
        pop_args_push_result(result);
      }

   set_action(RA_CONTINUE);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_LPAR_F_C_RPAR()
{
   Assert1(prefix_len == 4);

   // a parenthesized function bound to its own axis, e.g. (⌽[1]), needs
   // to collapse to a single (derived) function, the same way (F) alone
   // does above via reduce_LPAR_B_RPAR_() -- so it can be used the same
   // way as a bare F elsewhere, in particular as the LO of an operator:
   // (⌽[1])⌿B. Without this phrase, "F C" alone (nothing after C to
   // complete some other, longer phrase, e.g. F C M) never reduces to
   // anything by itself, so the shift-reduce engine ran off the end of
   // the statement with "( F C )" still unreduced on the stack and no
   // matching phrase -- SYNTAX ERROR, even for the plain call (⌽[1])B
   // with no operator at all involved. Found investigating Blake
   // McBride's Bugs26 #5 (same area, unrelated root cause).
   //
cFunction_P F = at1().get_function();
Value_P     C = at2().get_function_axis();

DerivedFunction * derived = get_fun_oper_slot(0, F, 0, C, LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_N___()
{
   Assert1(prefix_len == 1);

const Token result = at0().get_function()->eval_();
   if (push_error(result))   return;

   pop_args_push_result(result);
   set_action(RA_CONTINUE);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_MISC_F_B_()
{
   Assert1(prefix_len == 2);

   if (saved_MISC.get_Class() == TC_INDEX)
      {
        // resolve a potential SHIFT/REDUCE conflict. If [] belongs to
        // an indexed left value A[...] then we must not reduce with
        // monadic F, but shift [] for the dyadyc F to reduce A[] F B
        // later on..
        //
        if (value_expected())   // then SHIFT
           {
             // push [...] and read one more token
             //
             push(saved_MISC);
             clear_MISC(LOC);
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
             return;
           }

        // fall through (REDUCE)
      }

Value_P B_val = at1().get_apl_val();
cFunction_P F_mon = at0().get_function();

   // B_val is a selective-assignment target (an lvalue/cellrefs array,
   // e.g. the resolve_lv() result from reduce_F_V__() below, or another
   // selecting function's own already-propagated result) iff its
   // VF_Flags::left_value flag is set. F_mon must then genuinely
   // select/overtake B_val (Function::get_selectivity()) and have a
   // result -- this is the exact same reasoning as
   // Bif_OPER1_EACH.cc's guard (see its comments for the full
   // rationale), just at the point where GNU APL actually applies any
   // monadic function to any value, selective-spec or not: this phrase
   // is reused for ordinary "F B" calls too, where B_val is never
   // left_value and the check below is a no-op. Confirmed exploitable
   // before this guard: (H ↑ B)←5 for a defined H, e.g.
   // ⎕FX 'Z←H B' 'G←G,⊂B' 'Z←B' -- H silently received and could leak
   // the live LvalCell that ↑'s own selective result carried.
   //
   // F_mon->has_monadic_form(): a primitive with no monadic form at all
   // (e.g. Bif_F12_DROP, Bif_F2_INDEX) must be let through to its
   // natural VALENCE ERROR below instead -- get_signature() cannot tell
   // us that for a primitive (see has_monadic_form()'s doc comment), and
   // phrase_error() itself never touches B_val, so letting it run is
   // always safe. Confirmed regression without this: (↓V)←9 used to
   // report SYNTAX ERROR instead of VALENCE ERROR.
   //
   if (B_val->is_left_value() && F_mon->has_monadic_form() &&
       (!F_mon->has_result() ||
        !(F_mon->get_selectivity() & (SEL_MON | SEL_MON_X))))
      {
        // Bugs31 #29 (Blake McBride): this guard rejects the whole
        // selective specification, so B_val's LvalCells will never be
        // consumed by assign_cellrefs() for their original purpose --
        // same leak Bugs30 #28 fixed for assign_cellrefs()'s own
        // LENGTH_ERROR branches, just one step earlier (this guard runs
        // before assign_cellrefs() is ever reached at all).
        //
        B_val->neutralize_lval_cells(LOC);
        syntax_error(LOC);
      }

const Token result = F_mon->eval_B(*B_val);
   if (result.get_Class() != TC_SI_CHANGE)   // the normal case
      {
        pop_args_push_result(result);
        set_action(result);
        return;
      }
   
    /* the executing of monadic at0().get_function() resulted in a
       change of the current )SI entry. This change could have been in
       in two directions:

       1. a new )SI entry was pushed. This is the most common case
          after a defined function was called. In this case the current
          parser is abandoned until the new )SI entry returns.

       2. ⎕ES (Event Simulate) has simulated an event (with one of
              the ⎕EC results like:
       2a.    0 - Error,
       2b.    1 - non-committed value,
       2c.    2 - committed value,
       2d.    3 - missing value (function w/o a result),
       2e.    4 - branch to function line, or
       2f.    5 - escape from function.

          Normally ⎕ES has pop'ed the )SI stack, so this parser does
          not exist anymore (and we must not use its member.
          in particular si
     */
   if (result.get_tag() == TOK_SI_PUSHED)   // case 1.
      {
        pop_args_push_result(result);
        set_action(result);
        return;
      }

   /* NOTE: the tags TOK_QUAD_ES_COM, TOK_QUAD_ES_ESC, TOK_QUAD_ES_BRA,
            and TOK_QUAD_ES_ERR below can only occur if:

       1. ⎕EA resp. ⎕EB is called; each is implemented as macro
          Z__A_Quad_EA_B resp. Z__A_Quad_EB_B.

       2. The macro calls ⎕ES 100 ¯1...¯4, which brings us here.

       Token result is the return token of Quad_ES::eval_AB() or
       Quad_ES::eval_B() and contains the right argument B (as set in
       the macro).

       We must check that ⎕ES 100 was not called directly, but only via
       ⎕EA or ⎕EB.
    */

   switch(result.get_tag())
       {
         default: break;
         case TOK_QUAD_ES_COM:   handle_QUAD_ES_COM(result);   return;
         case TOK_QUAD_ES_ESC:   handle_QUAD_ES_ESC();         return;
         case TOK_QUAD_ES_BRA:   handle_QUAD_ES_BRA(result);   return;
         case TOK_QUAD_ES_ERR:   handle_QUAD_ES_ERR(result);   return;
      }

   // at this point a normal monadic function (i.e. other than ⎕EA/⎕EB)
   // has returned an error
   //
   if (push_error(result))   return;

   // not reached
   //
   Q1(result.get_tag())
   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_MISC_F_C_B()
{
   Assert1(prefix_len == 3);   // F C B

   // Resolve the shift/reduce conflict (saved_MISC.get_Class()==TC_INDEX,
   // i.e. this F's own axis bracket is directly preceded by ANOTHER,
   // completed-but-not-yet-reduced value index, e.g. Z[1;1] in
   // "Z[1;1] ⌷[¯1] 1") BEFORE calling at1().get_function_axis() below,
   // not after. get_function_axis() validates F's axis value and can
   // itself throw (e.g. AXIS ERROR for ⌷[¯1] applied to a scalar), which
   // used to unwind out of this function with saved_MISC still holding
   // the only live reference to Z[1;1]'s IndexExpr -- nothing outside
   // this function knows to look at saved_MISC on an error unwind, so
   // it (and its index values) leaked. See Bugs27 #59(k). Checking
   // first, before anything that can throw, gets Z[1;1] safely back
   // onto the main stack (where the usual per-reduce_XXX() error
   // handling, e.g. reduce_A_C__()'s own try/catch, already covers it)
   // before F's axis is even touched.
   //
   if (saved_MISC.get_Class() == TC_INDEX)
      {
        if (value_expected())
           {
             // push [...] and read one more token
             //
             push(saved_MISC);
             clear_MISC(LOC);
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
             return;
           }
      }

cMonOP  M = at0().get_function();
Value_P C = at1().get_function_axis();
Value_P B = at2().get_apl_val();

   // ⎕FIO and ⌹ are primarily functions, but have subfunctions that are
   // operators. If saved_MISC is a function, then M was possibly called
   // as an operator and we derive it here if the subfunction was anindeed
   // an operator.
   //
   if (saved_MISC.get_Class() == TC_FUN12)
      {
        // C may have more than one item (e.g. a subfunction name like
        // 'strerror' or 'getcwd' -- ⎕FIO axes can be spelled either way,
        // see Quad_FIO::value_to_subfun()), so we should call
        // get_sole_integer() only after having checked that M is ⎕FIO
        // *and* that C actually has exactly one item -- get_sole_integer()
        // itself throws LENGTH_ERROR for anything else, which used to
        // fire here for every multi-character ⎕FIO axis name as soon as
        // any function/operand preceded the M[C]B phrase (this specific
        // F-C-B reduce phrase is only reached in that case; the plain
        // M[C]B phrase, with nothing to its left, uses a different,
        // unaffected reduce path -- confirmed live: `⎕FIO['strerror'] 0`
        // alone works, but e.g. `⍴⎕FIO['strerror'] 0` raised a spurious
        // LENGTH ERROR before this fix). The subfunction which is an
        // operator is ⎕FIO[49] aka. ⎕FIO.read_text.
        //
        const TokenTag tag_M = at0().get_tag();
        if (tag_M == TOK_Quad_FIO && C->element_count() == 1 &&
            C->get_sole_integer() == 49)
           {
             Token LO(saved_MISC.get_token());
             DerivedFunction * derived = get_fun_oper_slot(&LO, M, 0, C, LOC);
             clear_MISC(LOC);

             const Token_loc pc_M(Token(), at(0));   // only its PC (of M)
             pop_and_discard();   // pop M
             pop_and_discard();   // pop C
                                  // but leave B
             Token tok_Derived(TOK_FUN2, derived);
             Token_loc tl_Derived(tok_Derived, pc_M);
             push(tl_Derived);
             prefix_len = 2;   // only f ⎕FIO resp. f ⌹
             set_action(RA_CONTINUE);   // match again (w/o SHIFT)
             return;
           }
      }

const Token Z = M->eval_XB(*C, *B);
   if (push_error(Z))   return;

   pop_args_push_result(Z);
   set_action(Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_F_B_()
{
   Assert1(prefix_len == 3);

Value_P B_val = at2().get_apl_val();
cFunction_P F_dya = at1().get_function();

   // see the matching guard in reduce_MISC_F_B_() above for the full
   // rationale; this is the dyadic form, e.g. the innermost step of a
   // selective-spec chain such as (2↑V)←9 or the defined-function
   // analogue (2 H2 V)←9.
   //
   if (B_val->is_left_value() && F_dya->has_dyadic_form() &&
       (!F_dya->has_result() ||
        !(F_dya->get_selectivity() & (SEL_DYA | SEL_DYA_X))))
      {
        // Bugs31 #29: see the matching guard in reduce_MISC_F_B_() above.
        //
        B_val->neutralize_lval_cells(LOC);
        syntax_error(LOC);
      }

const Token result = F_dya->eval_AB(*at0().get_apl_val(), *B_val);
   if (push_error(result))   return;

   pop_args_push_result(result);
   set_action(result);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_M_B_()
{
   Assert1(prefix_len == 3);

   if (at1().is_SLASH_or_BACKSLASH())   return reduce_A_F_B_();

   // A M B where M is a monadic operator and A is a VALUE (not
   // function) left operand, e.g. 5 OP1 1 for ∇Z←(LO OP1) B. APL2
   // (lrm "Operators": an operand may be an array or a function) and
   // GNU APL's own macros already rely on this (e.g. Macro.def's
   // μ3 Z__LO_POWER_N_B μ5); the dyadic operator phrase table already
   // routes an analogous value LO (A D B) to the SAME handler as a
   // function LO (A D G) -- phrase_gen.def -- but the monadic table
   // only ever had "F M" (function LO). Mirror reduce_F_M__()'s
   // construction of a Derived_LO_M, but apply it immediately since
   // both operands (A as LO, B as the argument) are already on the
   // stack here, unlike reduce_F_M__() which only builds the derived
   // function for a LATER application. See Bugs27 #50.
   //
const Token &  LO = at0();
cMonOP    M = at1().get_function();
Value_P   B = at2().get_apl_val();

DerivedFunction * derived = get_fun_oper_slot(&LO, M, 0, Value_P(), LOC);

const Token Z = derived->eval_B(*B);
   if (push_error(Z))   return;

   pop_args_push_result(Z);
   set_action(Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_F_C_B()
{
   Assert1(prefix_len == 4);   // A F C B

Value_P     A = at0().get_apl_val();
cFunction_P F = at1().get_function();
Value_P     C = at2().get_function_axis();
Value_P     B = at3().get_apl_val();

   // same selectivity guard as in reduce_A_F_B_(), but for a dyadic
   // function with axis, e.g. (M,[1]M)←0 or (M+[1]V)←0 (Bugs37 #39)
   //
   if (B->is_left_value() &&
       (!F->has_result() || !(F->get_selectivity() & SEL_DYA_X)))
      {
        B->neutralize_lval_cells(LOC);
        syntax_error(LOC);
      }

const Token Z = F->eval_AXB(*A, *C, *B);

   // result could be a value or an error
   //
   if (push_error(Z))   return;   // if result was an error

   // result was a value
   //
   pop_args_push_result(Z);
   set_action(Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_M_C_B()
{
   Assert1(prefix_len == 4);

   if (at1().is_SLASH_or_BACKSLASH())   return reduce_A_F_C_B();

   // A M C B: axis-qualified counterpart of reduce_A_M_B_() above, same
   // fix, same reason -- see Bugs27 #50.
   //
const Token &  LO = at0();
cMonOP    M = at1().get_function();
Value_P   C = at2().get_function_axis();
Value_P   B = at3().get_apl_val();

DerivedFunction * derived = get_fun_oper_slot(&LO, M, 0, C, LOC);

const Token Z = derived->eval_B(*B);
   if (push_error(Z))   return;

   pop_args_push_result(Z);
   set_action(Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_M__()
{
   Assert1(prefix_len == 2);

const Token & LO_F = at0();
cMonOP     M = at1().get_function();

DerivedFunction * derived = get_fun_oper_slot(&LO_F, M, 0, Value_P(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_M__()
{
   Assert1(prefix_len == 2);

   if (at1().is_SLASH_or_BACKSLASH())
      {
        // A slash or A backslash with no B (yet) on the stack, e.g.
        // inside "(5/)". Unlike A M B, there is no eager eval_AB() we
        // can perform here (no B), and unlike a genuine custom
        // operator, value LO + slash/backslash does not have
        // derived-function semantics of its own (see the
        // comment in reduce_A_M_B_() above). Do not reduce; keep shifting
        // so that a later B (if any) is caught by the A M B / A M C B
        // phrases instead, which already redirect to compress/replicate.
        //
        set_action(RA_PUSH_NEXT);
        return;
      }

   // A M (parenthesized, standalone, e.g. (5 OP1) B): same value-LO
   // case as reduce_A_M_B_() above, except the derived function is
   // only being BUILT here (no B on the stack yet), mirroring
   // reduce_F_M__()'s function-LO counterpart. See Bugs27 #50.
   //
const Token &  LO = at0();
cMonOP    M = at1().get_function();

DerivedFunction * derived = get_fun_oper_slot(&LO, M, 0, Value_P(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_M_M__()
{
   Assert1(prefix_len == 2);

   if (at0().is_SLASH_or_BACKSLASH() && MM_is_FM(PC))
      {
        reduce_F_M__();
      }
   else   // the normal case: the left M is an operator. SHIFT
      {
        set_action(RA_PUSH_NEXT);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_M_C_()
{
   Assert1(prefix_len == 3);

const Token & LO_F = at0();
cMonOP     M = at1().get_function();
Value_P C      = at2().get_function_axis();

DerivedFunction * derived = get_fun_oper_slot(&LO_F, M, 0, C, LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_C_M_()
{
   Assert1(prefix_len == 3);

cFunction_P LO_F = at0().get_function();
Value_P        C = at1().get_function_axis();
cMonOP         M = at2().get_function();

DerivedFunction * derived_F_C = get_fun_oper_slot(0, LO_F, 0, C, LOC);

Token tok_F_C(TOK_FUN2, derived_F_C);
DerivedFunction * derived = get_fun_oper_slot(&tok_F_C, M, 0, Value_P(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_C_M_C()
{
   Assert1(prefix_len == 4);

cFunction_P F  = at0().get_function();
Value_P     FX = at1().get_function_axis();
cMonOP      M  = at2().get_function();
Value_P     MX = at3().get_function_axis();

DerivedFunction * derived_F_C = get_fun_oper_slot(0, F, 0, FX, LOC);

Token tok_F_C(TOK_FUN2, derived_F_C);
DerivedFunction * derived = get_fun_oper_slot(&tok_F_C, M, 0, MX, LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_D_V__()
{
   /* This is the end of an A.B.C...V chain.

      at0() is the '.' (aka. D) immediately before V, and
      at1() is that first member V itself -- the reverse of what this
      comment used to claim (at0() is the *leftmost* of the two tokens
      per Prefix::at0()'s own doc comment, and D is shifted before V).
      Confirmed via Bugs28 #14: at1().get_Class() is TC_SYMBOL, not
      TC_OPER2, for both a genuine "A.B" and a mistaken "+⍤X".

      Collect the members (and discard the '.' preceeding them)
      until no more '.' tokens are found.

      Prefix::reduce_D_V__() is called from two places:

      case 1. from reduce_body() with prefix D V and (therefore
               prefix_len == 2), or else

      case 2. from reduce_D_V_ASS_B() and therefore prefix_len == 4.

      case 1 is called via best_phrase->reduce_fun)() with best_phrase D V.

      The processing of member variables is somewhat lengthy and almost the
      same for member reference and for member assignment. We therefore
      handle both cases here (i.e. in reduce_D_V__()) instead of replicating
      almost the same code in in reduce_D_V_ASS_B().
    */

   // D is the token class TC_OPER2, so the phrase "D V" (and "D V ASS B")
   // also matches ⍤ X and ⍣ X -- for those two operators X may be a plain
   // value/symbol, not a member name. A trailing ← after such an X (e.g.
   // +⍤X←) then collides with the ASS_var_seen bookkeeping below, which
   // is only ever valid for a genuine member chain. Bugs28 #14: reject
   // here (there being nothing valid to either operator's right) rather
   // than falling through to the Assert()s, which assume at0() is '.'.
   //
   if (at0().get_tag() != TOK_OPER2_INNER)   SYNTAX_ERROR;

const bool member_assign = prefix_len == 4;   // assume member reference
   if (prefix_len == 2)      // case 1: member reference
      {
        // Bugs31 #27 (Blake McBride): a member chain ending in a
        // ⎕-name, followed by a dyadic-looking trailing primitive and
        // ← (e.g. A.⎕I↓←1, A.⎕Z↓←1, foo.bar.⎕I↓←1), can reach here via
        // case 1 (plain reference) with ASS_var_seen ALREADY set by
        // some earlier, unrelated phrase reduction -- the Bugs28 #14
        // neighbour's own TOK_OPER2_INNER check just above does not
        // catch this variant (at0() genuinely is TOK_OPER2_INNER for a
        // real member chain). Give the same clean SYNTAX ERROR the
        // Bugs28 #14 fix already gives for the sibling "+⍤X←" mis-parse,
        // instead of asserting and dumping the whole interpreter's SI.
        //
        if (get_assign_state() == ASS_var_seen)   SYNTAX_ERROR;
      }
   else if (member_assign)   // case 2: member assignment
      {
        Assert(get_assign_state() == ASS_var_seen);   // by reduce_D_V_ASS_B
      }
   else                      // something unexected
      {
        Assert(0 && "Bad prefix length in Prefix::reduce_D_V__()");
        return;
      }

   /*
      construct members which is a vector of member names in reverse order.

      For e.g. A.B.C.D ← 42 members would be { "D", "C", "B", "A" }

      The prefix parser has so far seen Token '.' 'D' or '.' 'D' '←' 42 and
      we now collect 'C' '.' 'B' '.' 'A' in the while() loop below.

      The top-level variable A may or may not already exist and is created
      if not and if this is a member assignment.

      ⎕CR.subfun and ⎕FIO.subfun are a special case in the while() loop below.
      They parse like a member variable access A.subfun but are not since
      A is not a variable but a function. If this special case is detected
      then e.g. ⎕CR.subfun is replaced with ⎕CR[fun] where fun is the function
      number corresponding for the subfunction name subfun. Dito for ⎕FIO.
    */
vector<const UCS_string *>members;
Symbol * top_sym = 0;
const Token_loc * top_loc = 0;   // the Token_loc of top_sym in body
   members.push_back(at1().get_sym_ptr()->get_name_ptr());
   while (PC + 1 < body.end_PC())   // at least 2 more token
         {
           if (body[PC].get_Class() == TC_SYMBOL)   // the normal case
              {
                Symbol * symbol = body[PC].get_sym_ptr();
                members.push_back(symbol->get_name_ptr());
               if (body[PC + 1].get_tag() == TOK_OPER2_INNER)
                  {
                    PC = PC + 2;
                  }
               else
                  {
                    top_sym = symbol;
                    top_loc = &body[PC];
                    PC = PC + 1;
                    break;
                  }
              }
           else   // body[PC] is not a symbol: the special ⎕CR/⎕FIO case
              {
                // this case is normally optimized away in
                // Parser::replace_static_patterns, but may slip through
                // for non-static patterns.
                //
                if (members.size() == 1                  &&
                    body[PC].get_Class() == TC_FUN12     &&
                    body[PC].get_function()->has_subfuns())
                   {
                     /* at this point we have ⎕CR.subfun B or ⎕FIO.subfun B.
                        The sub-function subfun of ⎕FIO is members[0].
                        subfun may or may not be a valid sub-function name
                        and we raise SYNTAX ERROR if not.

                        If the sub-function name is valid (i.e. axis != -1
                        below) then we replace e.g. ⎕FIO.subfun with the
                        corresponding axis function ⎕FIO[axis];
                      */
                     cFunction_P fun = body[PC].get_function();
                     const sAxis axis = fun->subfun_to_axis(*members[0]);
                     if (axis == -1)   // no sub0function with that name
                        {
                          MORE_ERROR() << "'" << *members[0]
                             << "' is not a sub-function of "
                             << fun->get_name() << ".\nTry " << fun->get_name()
                             << " '' for a list of valid sub-function names.";
                          syntax_error(LOC);
                        }

                     pop_args_push_result(Token(TOK_AXIS,
                                                IntScalar(axis, LOC)));
                     set_action(RA_CONTINUE);   // match again (w/o SHIFT)
                     return;
                   }

                 MORE_ERROR() << "member access: missing variable name";
                 syntax_error(LOC);
              }
         }

   if (top_sym == 0)
      {
        MORE_ERROR() << "member access: no top-level variable name";
        syntax_error(LOC);
      }

   // the result replaces the whole chain A.B.C...V, so it gets the PC of
   // its leftmost token: the top-level name A. The result gets the PC of
   // at0() (the D before V) in pop_args_push_result().
   //
   {
     const Token_loc tl_D(at(0).get_token(), *top_loc);
     at(0).copy(tl_D, LOC);
   }

Value_P top_val = top_sym->get_var_value();
   if (!top_val)   // top_sym is not a variable (-name). Maybe create one.
      {
        if (member_assign)
           {
             // VAR.member ← value.
             //
             // The user assigns a value to the member of a structured
             // variable that does not yet exist. We do the same as for
             // VAR←value for not existing APL variables, i.e. we create
             // t automatically.
             //
             top_val = EmptyStruct(LOC);
             top_sym->assign(top_val, false, LOC);
             set_assign_state(ASS_none);
           }
        else   // member reference
           {
             // reference of the member a not existing variable. Like
             // referencing a normal variable we raise a VALUE ERROR but
             // give the user some more info.
             //
             UCS_string & more = MORE_ERROR()
                    << "member access: missing top-level variable "
                    << top_sym->get_name() << " for member ";
             more.append_members(members, 0);
             more << " not found";
             VALUE_ERROR;   // bail out
           }
      }

   // at this point the structured variable exist, either beforehand or
   // created above

   if (member_assign)   // (direct) member assignment e.g. A.B.C←V
      {
        // Bugs28 #100(u): a ⎕-name in the member chain (e.g. S.⎕IO←1)
        // used to be accepted like any other TC_SYMBOL and silently
        // created a member literally named "⎕IO" -- a ⎕-name is
        // exactly as reserved here as it is for a header/local-variable
        // position (Bugs28 #89/#100(b)'s is_system_var()), just via a
        // different Symbol lookup path, so no existing check catches it.
        //
        loop(m, members.size())
           {
             if (Avec::is_quad((*members[m])[0]))
                {
                  MORE_ERROR() << "member access: '" << *members[m]
                               << "' is a ⎕-name and cannot be used as a"
                                  " member name";
                  DOMAIN_ERROR;
                }
           }

        // Depth-cache invalidation for the whole member chain (see
        // plan.txt's 2026-09-01 analysis): member access does not go
        // through Symbol::resolve_lv(), so it needs its own call to the
        // same isolate_deep() mechanism every other left-value form
        // relies on. Must happen before get_member() below takes a
        // pointer into top_val's structure -- isolate_deep() may clone a
        // shared node in place, which would leave a pointer obtained
        // beforehand dangling into the discarded original.
        //
        // top_val is a local Value_P copy (from get_var_value()), not the
        // Symbol's own stored one, so isolating top_val itself would clone
        // it without ever writing the clone back into top_sym -- the same
        // mistake resolve_lv() avoids by isolating value_stack.back() in
        // place. Do the same here via top_of_stack(), then re-fetch.
        //
        top_sym->top_of_stack()->isolate_deep(LOC);
        top_val = top_sym->get_var_value();

        Value * member_owner = 0;
        Cell * member_cell = top_val->get_member(members, member_owner, true);
        Assert(member_owner);

        if (member_cell->is_member_anchor())
           {
             UCS_string & more = MORE_ERROR() <<
                          "member access: cannot override non-leaf member ";
             more.append_members(members, 0);
             more << "\n      )ERASE or ⎕EX that member first.";
             DOMAIN_ERROR;
           }

        member_cell->release(LOC);   // let it free

        Value_P B = at3().get_apl_val();
        if (B->is_simple_scalar())
           {
             Cell cache;
             B->get_cfirst(cache).init_other(member_cell, *member_owner, LOC);
           }
        else
           {
             new (member_cell)   PointerCell(B.get(), *member_owner);
           }
        pop_args_push_result(Token(TOK_APL_VALUE2, B));
      }
   else                 // member reference (or selective specification)
      {
        // If this is going to be used as the start of a selective
        // specification (the ASS_arrow_seen case below), it needs the
        // same whole-chain depth invalidation as member_assign above, and
        // for the same reason it must happen before get_existing_member()
        // takes a pointer into top_val's structure. Checked here, before
        // that call, rather than down in the ASS_arrow_seen branch below,
        // specifically to preserve that ordering -- isolating only when
        // actually needed also avoids paying isolation cost on a plain
        // (non-assigning) member reference.
        //
        const bool will_selectively_assign =
           (get_assign_state() == ASS_arrow_seen);
        if (will_selectively_assign)
           {
             // See the matching comment above (member_assign branch):
             // isolate the Symbol's own stored value in place, then
             // re-fetch, rather than isolating the local top_val copy.
             top_sym->top_of_stack()->isolate_deep(LOC);
             top_val = top_sym->get_var_value();
           }

        const Cell * member_cell = top_val->get_existing_member(members);
        Assert(member_cell);

        if (member_cell->is_member_anchor() &&
            get_assign_state() == ASS_arrow_seen)
           {
             UCS_string & more = MORE_ERROR() <<
                          "member access: cannot use non-leaf member ";
             more.append_members(members, 0);
             more << " in selective specification.\n"
                     "      )ERASE or ⎕EX that member first.";
             DOMAIN_ERROR;
           }
        else if (member_cell->is_pointer_cell())
            {
              if (get_assign_state() == ASS_arrow_seen)   // selective spec.
                 {
                   set_assign_state(ASS_none);
                   Value_P cell_refs = member_cell->get_pointer_value()
                                                 ->get_cellrefs(LOC);
                   // a BARE member reference (nothing selecting from it
                   // yet, e.g. (S.a)←B) must not be usable as a
                   // selective-specification target in its own right --
                   // Value::assign_cellrefs() rejects it with SYNTAX
                   // ERROR if this marker survives unbroken that far
                   // (Bugs30 #14); a genuinely selective form such as
                   // (⌽S.a)←B narrows cell_refs into a fresh Value first
                   // and is unaffected.
                   cell_refs->set_lval_bare_member_ref();
                   pop_args_push_result(Token(TOK_APL_VALUE2, cell_refs));
                 }
              else
                 {
                   Value_P Z(CLONE_P(member_cell->get_pointer_value(), LOC),
                                     LOC);
                   pop_args_push_result(Token(TOK_APL_VALUE1, Z));
                 }
            }
        else   // simple (non-nested) leaf member, e.g. scalar S.a
           {
             // Unlike the is_pointer_cell() branch above, this used to
             // unconditionally build a disconnected read-only copy of
             // the cell, ignoring ASS_arrow_seen entirely -- so a scalar
             // leaf member could never be used as a selective-
             // specification/assignment target at all, e.g. (S.a)←7
             // with S.a a scalar was SYNTAX ERROR while the same with
             // S.a a vector worked (via the is_pointer_cell() branch's
             // own check). Fixed by Bugs29 #4 (Blake McBride) to build a
             // proper lvalue here too -- but a BARE member reference
             // (nothing selecting from it yet) is still rejected by
             // Value::assign_cellrefs() with SYNTAX ERROR if this
             // marker survives unbroken that far (Bugs30 #14, extending
             // the is_pointer_cell() branch's own identical marker to
             // this leaf case for consistency); a genuinely selective
             // form such as (⌽S.a)←B is unaffected.
             //
             if (get_assign_state() == ASS_arrow_seen)   // selective spec.
                {
                  set_assign_state(ASS_none);
                  Value * owner = 0;
                  Cell * mut_cell = top_val->get_member(members, owner, true);
                  Assert(mut_cell);
                  Value_P left_Z(LOC);
                  const LvalCell left_cell(mut_cell, owner);
                  left_Z->next_ravel_Cell(left_cell);
                  left_Z->check_value(LOC);
                  // a left value like any other (Value::get_cellrefs()):
                  // without it, the selectivity guards and the no-op
                  // rules did not apply to scalar members, e.g. (⍳S.a)←9
                  // ran ⍳ on the LvalCell (Blake McBride, Bugs32 #17)
                  //
                  left_Z->set_left_value();
                  left_Z->set_lval_bare_member_ref();
                  pop_args_push_result(Token(TOK_APL_VALUE2, left_Z));
                }
             else
                {
                  Value_P Z(LOC);
                  Z->next_ravel_Cell(*member_cell);
                  Z->check_value(LOC);
                  pop_args_push_result(Token(TOK_APL_VALUE1, Z));
                }
           }
      }

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_D_B_()
{
   Assert1(prefix_len == 3);

const Token &  LO_F = at0();
cDyaOP   D    = at1().get_function();
Value_P  RO_B = at2().get_apl_val();

   // same as F D G, except for D = ⍤ or ⍣
   //
const Id id_D = D->get_Id();
   if (id_D != ID_OPER2_RANK && id_D != ID_OPER2_POWER)
      {
         reduce_F_D_G_();
         return;
      }

   /* At this point we have:
 
      f ⍤ RO with RO←(y B) that were stranded together in Parser.cc, or else
      f ⍣ RO with RO←(N B) that were stranded together in Parser.cc.

      Only a strand (TOK_APL_VALUE3) can contain B. Any other value, i.e.
      a variable, a parenthesized value, or a function result, is the
      entire y resp. N, even if it has the wrong shape (Blake McBride,
      Bugs35 #38, Bugs28 #58). reduce_A_B__() does not glue A B when A
      is preceded by a dyadic operator, so f ⍤ Y B with variables Y and B
      arrives here as f ⍤ Y and B stays on the stack.

      Unstrand RO into y resp. N and (maybe) B. NOTE that in this context
      B is y resp. N of the strand RO and not the real right argument B of
      (f ⍤ RO) B resp. (f ⍣ RO) B.
     
      There may or may not be a left A on the way. Since f can be nomadic
      we do not know if a left argument A for f⍤ is coming and we have to
      create a derived function instead of calling >eval_ALRB() or eval_LRB()
      of ⍤ directly.
    */
Value_P value_B;   // the original B (before stranding RO and B).
DerivedFunction * derived;
   {
     Value_P value_RO;   // j123 (for ⍤) or N (for ⍣)
     const UCS_string LO_name = LO_F.get_function()->get_name();
     if (at2().get_tag() != TOK_APL_VALUE3)   // not a strand: RO is y or N
        value_RO = RO_B;
     else if (id_D == ID_OPER2_RANK)
        Bif_OPER2_RANK::unstrand_RO_B(LO_name, RO_B, value_RO, value_B);
     else
        Bif_OPER2_POWER::unstrand_RO_B(LO_name, RO_B, value_RO, value_B);

     Token tok_RO(TOK_APL_VALUE1, value_RO);
     derived = get_fun_oper_slot(&LO_F, D, &tok_RO, Value_P(), LOC);
   }

   /* for unstrand_RO_B() there are 2 main cases:
 
      case 1: y and B were stranded into y_B, for example: f ⍤ y B.
              In this case unstrand_RO_B() returns y and a valid B

      case 2: y and B were not stranded into y_B, for example: (f ⍤ y) B or
              f ⍤ y SYM In this case unstrand_RO_B() returns only y and no
              valid B. The right argument B is waiting on the stack (at
              at3() for f⍤B, or at at4() for (f⍤B), or at a5() for ((f⍤B)),
              and so on.
    */
const Token dD(TOK_FUN2, derived);

   if (value_B)   // case 1 (valid B)
      {
        // save locations of ⍤ and B
        //
        const Token_loc pc_D(Token(), at(1));   // only its PC
        const Token_loc pc_B(Token(), at(2));   // only its PC

        pop_and_discard();   // pop LO_F
        pop_and_discard();   // pop D
        pop_and_discard();   // pop y_B

        Token B(TOK_APL_VALUE1, value_B);
        Token_loc tloc_B(B, pc_B);
        Token_loc tloc_dD(dD, pc_D);
        push(tloc_B);    // was y_B
        push(tloc_dD);   // was D
      }
   else    // case 2: only y123, but no B (e.g. (f ⍤ 1 2 3) B
      {
        pop_args_push_result(dD);
      }

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_D_G_()
{
   Assert1(prefix_len == 3);

   // bind F and G to D.
   //
const Token &     LO_F = at0();
cFunction_P D    = at1().get_function();
const Token &     RO_G = at2();

DerivedFunction * derived = get_fun_oper_slot(&LO_F, D, &RO_G, Value_P(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_D_C_B()
{
   Assert1(prefix_len == 4);

const Token & LO_F = at0();
cDyaOP     D = at1().get_function();
Value_P  X_C = at2().get_function_axis();
Value_P RO_B = at3().get_apl_val();

const Token_loc pc_D(Token(), at(1));   // only its PC

   // reduce, unless if another dyadic operator is coming. In that case
   // F is the right operand of the other operator (and not the left operand
   // of D and we simply continue (because right operand binds stronger than
   // left operand).
   //
   if (PC < body.end_PC())   // more token ahead
      {
        const Token & tok = body[PC];
        TokenClass next =  tok.get_Class();
        if (next == TC_SYMBOL)
           {
             Symbol * sym = tok.get_sym_ptr();
             const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
             next = sym->resolve_class(is_left_sym);
           }

        if (next == TC_OPER2)
           {
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
             return;
           }
      }

   if (D != &Bif_OPER2_RANK::fun)   // the normal case
      {
        DerivedFunction * derived =
                       get_fun_oper_slot(&LO_F, D, &at2(), Value_P(), LOC);
        pop_and_discard();   // pop LO_F
        pop_and_discard();   // pop D
        pop_and_discard();   // pop C
        Token tok_derived(TOK_FUN2, derived);
        Token_loc tloc_D(tok_derived, pc_D);
        push(tloc_D);
        set_action(RA_CONTINUE);   // match again (w/o SHIFT)
        return;
      }

   /* At this point we have the special case: (A) f ⍤[X] y_B
      with y_B stranded together in Parser.cc.  Unstrand y_B into y
      and (maybe) B. NOTE that B is y (and not the real B).
     
      There may or may not be a left A on the way. Since f can be nomadic
      we do not know if a left argument A for f⍤ is coming and we have to
      create a derived function instead of calling >eval_ALRB() or eval_LRB()
      of ⍤ directly.

      Note that X_C is the axes for LO_F (for the final disclose of the
      NARS variant of ⍤ with axes.
    */
Value_P value_B;   // the original B (before stranding j and B).
DerivedFunction * derived;
   {
     Value_P y123;
     if (at3().get_tag() != TOK_APL_VALUE3)   // not a strand (see F D B)
        y123 = RO_B;
     else
        Bif_OPER2_RANK::unstrand_RO_B(LO_F.get_function()->get_name(),
                                       RO_B, y123, value_B);
     Token T_y123_orig_RO(TOK_APL_VALUE1, y123);

     derived = get_fun_oper_slot(&LO_F, D, &T_y123_orig_RO, X_C, LOC);
   }

   /* for unstrand_RO_B() there are 2 main cases:
 
      case 1: y and B were stranded into y_B, for example: f ⍤ y B.
              In this case unstrand_RO_B() returns y and a valid B

      case 2: y and B were not stranded into y_B, for example: (f ⍤ y) B or
              f ⍤ y SYM In this case unstrand_RO_B() returns only y and no
              valid B. The right argument B is waiting on the stack (at
              at4() for f⍤B, or at at5() for (f⍤B), or at a6() for ((f⍤B)),
              and so on.
    */
const Token tok_derived(TOK_FUN2, derived);

   if (value_B)   // case 1 (valid B)
      {
        const Token_loc pc_B(Token(), at(3));   // only its PC

        pop_and_discard();   // pop LO_F
        pop_and_discard();   // pop D
        pop_and_discard();   // pop C
        pop_and_discard();   // pop y_B

        Token B(TOK_APL_VALUE1, value_B);
        Token_loc tloc_B(B, pc_B);
        Token_loc tloc_dD(tok_derived, pc_D);
        push(tloc_B);    // was y_B
        push(tloc_dD);   // was D
      }
   else    // case 2: only y123, but no B (e.g. (f ⍤[X] 1 2 3) B
      {
        // replace F, D, C, and B with derived. Note that B is RO and not
        // the real B for derived.
        //
        pop_args_push_result(tok_derived);
      }

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_C__()
{
   Assert1(prefix_len == 2);

Value_P A = at0().get_apl_val();
Value_P Z;

   if (at1().get_tag() == TOK_AXIS)   // [] or [IX]
      {
        Value_P axis = at1().get_apl_val();
        if (!axis)   // A[] — empty index
           {
             // A[] names ONE index position (the elided single-axis
             // index), valid only for a vector A -- same rule the
             // A[]←C assignment path already enforces (Symbol.cc's
             // assign_indexed(), Z->get_rank() != 1 -> RANK_ERROR).
             // A[] used to skip this check and just return the whole
             // A regardless of rank.
             //
             // Not when A[] is itself the tail of a SELECTIVE
             // specification (e.g. (⊃E)[]←0: A is then a matrix of
             // LvalCells mirroring ⊃E's shape, produced via
             // get_cellrefs(), and [] there means "every position",
             // valid at any rank -- unlike a plain M[] read/assign on
             // a named variable). get_assign_state() is NOT a usable
             // signal here: it is already reset back to ASS_none by
             // the disclose's own selective-spec handling before this
             // point is reached, so check the cells themselves
             // instead. See Bugs27 #45.
             //
             if (A->get_rank() != 1 &&
                 !(A->element_count() > 0 && A->is_lval_cell(0)))
                {
                  MORE_ERROR() << "A[]: A[] names one index position"
                                  " (applies to vector A only); ⍴⍴A is "
                               << A->get_rank();
                  RANK_ERROR;
                }
             Z = CLONE(A.get(), LOC);

             // Bugs28 #52: with NEW_CLONE, CLONE() is not a real copy
             // -- it is the SAME Value object as A, just wrapped in a
             // new Value_P handle -- so when A is an lval-cellref
             // array carrying Symbol::resolve_lv()'s
             // set_lval_whole_symbol() marker (the "((⍳0)⊃sym)←B means
             // replace the whole array" shortcut), that marker
             // survived onto Z completely unchanged. But A[] selects
             // EVERY position of A (a real, shape-conforming selective
             // specification, e.g. (V[])←1 2 3 must require ⍴,B to be
             // 1 or ⍴V, same as (V[⍳⍴V])←1 2 3), not "replace with no
             // conformance check at all" the way a genuinely empty
             // selector does -- every other lvalue-producing function
             // returns a fresh Value that naturally starts without the
             // marker (see set_lval_whole_symbol()'s own doc comment);
             // clear it explicitly here since CLONE() doesn't actually
             // build a fresh object.
             //
             Z->set_lval_whole_symbol(0);
           }
        else         Z = A->index(*axis);
      }
   else                               // [I1; I2...]
      {
        // get_index_val() only self-protects with Assert (a no-op at the
        // default ASSERT_LEVEL 0). A user-typed @N@ marker (TOK_MARKER,
        // TC_INDEX, TV_INT -- see Token.def) reaches here too, since its
        // token class is the same as a real index; without this check the
        // literal integer N is reinterpreted as an IndexExpr* below,
        // dereferenced, and delete'd -- an attacker-controlled pointer.
        if (at1().get_ValueType() != TV_INDEX)   SYNTAX_ERROR;

        const IndexExpr * idx =  &at1().get_index_val();
        try
           {
             Z = A->index(*idx);
             Log(LOG_delete)
                CERR << "delete " << voidP(idx) << " at " LOC << endl;
             delete idx;
           }
        catch (const Error & err)
           {
             const Token result(TOK_ERROR, err.get_error_code());
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
             pop_args_push_result(result);
             set_action(result);
             return;
           }
      }

const Token result(TOK_APL_VALUE1, Z);
   pop_args_push_result(result);

   set_action(result);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_V_C__()
{
   Assert1(prefix_len == 2);

Symbol * V = at0().get_sym_ptr();
const Token tok = V->resolve_lv(LOC);   // not Token & !
   replace_at(0, tok);
   set_assign_state(ASS_var_seen);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_V_C_ASS_B()
{
   Assert1(prefix_len == 4);

Symbol * V = at0().get_sym_ptr();
Value_P B = at3().get_apl_val();

   if (at1().get_tag() == TOK_AXIS)   // [] or [x]
      {
        const cValue * v_idx = at1().get_axes().get();

        try
           {
             V->assign_indexed(v_idx, B);
           }
        catch (const Error & err)
           {
             const Token result(TOK_ERROR, err.get_error_code());
             replace_at(1, Token(TOK_VOID));
             replace_at(3, Token(TOK_VOID));
             pop_args_push_result(result);
             set_assign_state(ASS_none);
             set_action(result);
             return;
           }
      }
   else                               // [a;...]
      {
        const IndexExpr * idx = &at1().get_index_val();
        try
           {
             V->assign_indexed(*idx, B);
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
           }
        catch (const Error & err)
           {
             const Token result(TOK_ERROR, err.get_error_code());
             replace_at(1, Token(TOK_VOID));
             replace_at(3, Token(TOK_VOID));
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
             pop_args_push_result(result);
             set_assign_state(ASS_none);
             set_action(result);
             return;
           }
      }

const Token result(TOK_APL_VALUE2, B);
   pop_args_push_result(result);
   set_assign_state(ASS_none);
   set_action(result);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_F_V__()
{
   Assert1(prefix_len == 2);

   // turn V into a (left-) value
   //
Symbol * V = at1().get_sym_ptr();
const Token tok = V->resolve_lv(LOC);   // not Token & !
   replace_at(1, tok);
   set_assign_state(ASS_var_seen);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_ASS_B_()
{
   Assert1(prefix_len == 3);

Value_P A = at0().get_apl_val();
Value_P B = at2().get_apl_val();

   A->assign_cellrefs(B);

const Token result(TOK_APL_VALUE2, B);
   pop_args_push_result(result);

   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_V_ASS_B_()
{
   Assert1(prefix_len == 3);

Value_P B = at2().get_apl_val();

   Assert1(B->get_owner_count() >= 2);   // owners are at least B and at2()
const bool clone = B->get_owner_count() != 2 || at1().get_tag() != TOK_ASSIGN1;
Symbol * V = at0().get_sym_ptr();

   // assign before popping V and ←, so that a failed assignment (e.g.
   // ⎕IO←'a') reports the entire phrase V ← B (with the right caret
   // under ←).
   //
   V->assign(B, clone, LOC);

   pop_and_discard();   // V
   pop_and_discard();   // ←

   // change value to committed value
   {
     Token committed(at0());
     committed.ChangeTag(TOK_APL_VALUE2);
     replace_at(0, committed);
   }

   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_D_V_ASS_B()
{
  Assert1(prefix_len == 4);

  set_assign_state(ASS_var_seen);
  reduce_D_V__();
  set_assign_state(ASS_none);
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_V_ASS_F_()
{
   Assert1(prefix_len == 3);

   // named lambda: V ← { ... }
   //
cFunction_P F = at2().get_function();

   // Bugs28 #90: this phrase (V ASS F, meant only for a named-lambda
   // assignment) can also spuriously match "X←⍴F1" once "F1" (a call to
   // a VOID-returning defined function) reduces away, leaving "X ← ⍴"
   // looking exactly like "V ← {a lambda}" one token later -- except F
   // here is ⍴, an ordinary primitive, not a lambda. The raw SYNTAX_ERROR
   // macro used to fire directly for that case, without this class's own
   // syntax_error() member function ever running its "was this actually
   // caused by a function producing no value?" TC_VOID-in-FIFO check
   // (the same class of raw-macro-bypasses-cleanup gap fixed for #26) --
   // so a clear, correctly-classified VALUE ERROR (matching the bare,
   // non-assigned "⍴F1" case, which already correctly VALUE_ERRORs) was
   // reported instead as a bare, unexplained SYNTAX ERROR.
   //
   if (!F->is_lambda())   syntax_error(LOC);

Symbol * V = at0().get_sym_ptr();

   // Bugs28 #100(aa): ⎕ (and the other distinguished symbols ⍺ ⍶ χ ⍹ ⍵,
   // which -- like ⎕ -- carry the generic TC_SYMBOL class this phrase
   // matches on) were accepted here as if they were an ordinary,
   // user-assignable name, silently giving the lambda literal "⎕" (etc.)
   // as its new name instead of running Quad_Quad::assign() (the code
   // that actually implements ⎕←B) -- so ⎕←{⍵} was a silent no-op with
   // neither the SYNTAX ERROR nor the display every other ⎕←function
   // form (⎕←+/, ⎕←F) already correctly gives. ⍞ and ⎕IO already avoid
   // this (they carry their own dedicated token tags, never reaching
   // this generic "V ASS F" phrase at all); ⎕ and ⍺/⍶/χ/⍹/⍵ do not.
   //
   if (Avec::is_quad(V->get_name()[0])                            ||
       V->get_name()[0] == UNI_ALPHA  || V->get_name()[0] == UNI_ALPHA_UNDERBAR ||
       V->get_name()[0] == UNI_CHI    || V->get_name()[0] == UNI_QUOTE_Quad     ||
       V->get_name()[0] == UNI_OMEGA  || V->get_name()[0] == UNI_OMEGA_UNDERBAR)
      SYNTAX_ERROR;

   if (V->assign_named_lambda(F, LOC))   DEFN_ERROR;

   // Bugs28 #50: the result of V←λ used to be a fabricated character-
   // vector VALUE holding V's own name (Value_P Z(V->get_name(), LOC)),
   // which then leaked into any further reduction as an ordinary data
   // value -- e.g. (f←{⍵}) 5 became the 2-item STRAND 'f' 5 instead of
   // applying the lambda to 5, and 1+(f←{⍵}) became a bogus character
   // arithmetic DOMAIN ERROR.
   //
   // Whether the result should behave as a function (so a further
   // reduction can apply it to an argument) or as a value (so a bare
   // V←λ statement stays silent/printable exactly like an ordinary
   // value assignment -- e.g. UT←{...} alone, extremely common,
   // must NOT become "missing mandatory right argument" VALENCE_ERROR)
   // depends on whether anything already sits to F's right on the
   // stack: since this parser shifts right-to-left, anything already
   // reduced from further right in the source (a potential argument,
   // e.g. the 5 in "(f←{⍵}) 5") is already at3() by the time this V
   // ASS F group matches -- ssize() > prefix_len (3) detects that.
   //
   if (ssize() > prefix_len)
      {
        // leave at2()'s own function token in place (shifted into
        // at0() by the pops below): the result of the assignment IS
        // the function itself, so the next reduction sees exactly
        // what "F B" would see for any other named function.
        //
        pop_and_discard();   // V
        pop_and_discard();   // ←
      }
   else
      {
        // nothing to apply F to -- keep the traditional printable/
        // silent-assignment value result (V's own name).
        //
        Value_P Z(V->get_name(), LOC);
        const Token result(TOK_APL_VALUE2, Z);
        pop_args_push_result(result);
      }

   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_RBRA___()
{
   Assert1(prefix_len == 1);

   // start partial index list. Parse the index as right so that, for example,
   // A[IDX}←B resolves IDX properly. assign_state is restored when the
   // index is complete.
   //
IndexExpr * idx = new IndexExpr(get_assign_state(), LOC);
   Log(LOG_delete)
      CERR << "new    " << voidP(idx) << " at " LOC << endl;

   replace_at(0, Token(TOK_PINDEX, *idx));
   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_LBRA_I__()
{
   // either [ I (true index) or else ; I (elided index). I is a (partial)
   // index and LBRA is the left) end of it. The result is either a scalar
   // axis aka. TOK_AXIS() or a non-scalar index aka. TOK_INDEX().
   //
   Assert1(prefix_len == 2);

IndexExpr & idx = at1().get_index_val();
const bool last_index = (at0().get_tag() == TOK_L_BRACK);

   if (idx.get_rank() == 0 && last_index)   // special case: [ ]
      {
        assign_state = idx.get_assign_state();
        const Token result(TOK_INDEX, idx);
        pop_args_push_result(result);
        set_action(RA_CONTINUE);   // match again (w/o SHIFT)
        Log(LOG_delete)   CERR << "delete " << voidP(&idx)
                               << " at " LOC << endl;
        delete &idx;
        return;
      }

   // add elided index to partial index list
   //
Token result = at1();
   result.get_index_val().add_index(Value_P());

   if (last_index)   // [ seen
      {
        assign_state = idx.get_assign_state();

        if (idx.is_axis())   // [] or [ axis ]
           {
             // Bugs8 #8 (Blake McBride): this branch is currently
             // unreachable (is_axis(), i.e. rank==1, after the
             // add_index() above implies rank was 0 before it, and
             // rank==0 && last_index already returned above) but was a
             // trap if it ever became reachable: it used to convert a
             // separate second copy 'I = at1()' and then still push
             // 'result' (which still carried the IndexExpr just
             // deleted below) -- a use-after-free on the prefix stack.
             // Fixed to convert 'result' itself, mirroring the correct
             // sibling reduce_LBRA_B_I_() below.
             //
             Value_P X = idx.extract_axis();
             Assert1(X);   // not [ ]
             Token tok_axis(TOK_AXIS, X);
             result.move_from(tok_axis, LOC);
             Log(LOG_delete)
                CERR << "delete " << voidP(&idx) << " at " LOC << endl;
             delete &idx;
           }
        else
           {
               Token tok_index(TOK_INDEX, idx);
               result.move_from(tok_index, LOC);
           }
      }
   else
      {
        set_assign_state(ASS_none);
      }

   pop_args_push_result(result);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_LBRA_B_I_()
{
   Assert1(prefix_len == 3);

   // [ B I or ; B I   (normal index)
   //
Token I = at2();
   I.get_index_val().add_index(at1().get_apl_val());

const bool last_index = (at0().get_tag() == TOK_L_BRACK);   // ; vs. [

   if (last_index)   // [ seen
      {
        IndexExpr & idx = I.get_index_val();
        assign_state = idx.get_assign_state();

        if (idx.is_axis())   // [] or [ axis ]
           {
             Value_P X = idx.extract_axis();
             Assert1(X);   // not [ ]
             Token tok_axis(TOK_AXIS, X);
             I.move_from(tok_axis, LOC);
             Log(LOG_delete)
                CERR << "delete " << voidP(&idx) << " at " LOC << endl;
             delete &idx;
           }
        else
           {
             Token tok_index(TOK_INDEX, idx);
             I.move_from(tok_index, LOC);
           }
      }
   else
      {
         set_assign_state(ASS_none);
      }

   pop_args_push_result(I);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_B__()
{
   Assert1(prefix_len == 2);

   /* vector notation. Glue A and B together, unless A is the right operand
      of a dyadic operator.

      Right operand is the only case where vector notation binds weaker
      (since bracket (i.e. B[...]) and specification left (i.e. ←A) ido
      obviously not apply here.

       The only case where A is a right operand is case 1 below.
    */
   // A is the latest token read, so the token left of A is the next unread
   // token body[PC]. Prefix uses only its own PC (a position in body) here,
   // never the PC of a Token_loc (which shall not be a position in body).
   //

   if (body[PC].get_Class() == TC_OPER2)   // case 1.
      {
        /* case 1:

                  ┌────────── PC
                  │   ┌────── A
                  │   │ ┌──── B
         ... LO OPER2 A B ...
         */
        set_action(RA_PUSH_NEXT);
        return;
      }

   // Bugs37 #14: a former "case 2" also pushed for ( LO OPER2 RO ) A B,
   // but there RO is inside the parentheses and A B is an ordinary right
   // argument (vector notation binds tighter than function application):
   // (f⍣N) A B ←→ (f⍣N) (A B).

Value_P Z = Value::glue(at0(), at1(), LOC);
const Token result(TOK_APL_VALUE3, Z);
   pop_args_push_result(result);

   set_action(RA_CONTINUE);
}
//════════════════════════════════════════════════════════════════════════════
/// pattern V ) ← B.
void
Prefix::reduce_V_RPAR_ASS_B()
{
   Assert1(prefix_len == 4);   // V ) ← B

   /* This pattern is the trailing tokens of one of 2 cases:

      1. vector assignment;         (C D ... V) ← B
      2. selective specification:   (... FUN V) ← B

      try_vector_assignment() decides which: it collects the symbols
      left of V and, if there are at least 2 in total, handles case 1
      completely (performing the assignment, or raising whatever error
      it warrants) and returns true. A false return means fewer than 2
      symbols were found, so this must be case 2, handled below.
    */
   if (try_vector_assignment())   return;   // case 1: vector assignment

   // case 2: selective specification.
   //
   // an empty selection (no function at all left of V) normally
   // indicates a selective specification. However, an incorrect vector
   // assignment such as (A 1 C) ← would also leave no function there
   // (because Parser.cc would set is_vector_spec to false around line
   // 820 in Parser.cc, and try_vector_assignment() just above found
   // only 1 symbol -- "1" breaks its scan). We fix this case here.
   //
const TokenClass tc = body[PC].get_Class();
   if (!((1 << tc) & TCG_FUN12_OPER12))   // tc is neither fun nor oper
      {
        // this case is rather rare, so we can afford a little time
        // to verify that we have at least one function in the supposed
        // selective specification
        //
        bool selective_spec = false;
        for (Prefix_PC pc = PC; pc < body.end_PC();)
            {
              const Token tok = body[pc++];
              const TokenClass tc = tok.get_Class();
              if ((1 << tc) & TCG_FUN12_OPER12)
                 {
                   selective_spec = true;
                   break;
                 }
              else if (tc == TC_L_PARENT ||   // most likely end of (...)←
                       tc == TC_END)
                 {
                   break;   // so selective_spec remains false
                 }
              else if (tc == TC_R_BRACK)
                 {
                   // a function axis or value index, e.g. the [1] in
                   // (1↓[1]V)←...: its contents are not a stranded
                   // value sitting between V and its selecting
                   // function, they are part of that function's own
                   // syntax. Skip over the whole [...] group (same
                   // idiom as value_expected() above) rather than
                   // examining its contents.
                   //
                   pc = pc + int(tok.get_int_val2());
                 }
              else if (tc == TC_VALUE)
                 {
                   // a value (e.g. a literal) sits between here and
                   // whatever function turns up further left -- that
                   // function, if any, would end up applying to a
                   // *strand* containing V rather than to V alone
                   // (e.g. (∊2 V)←¯1: ∊ would apply to the stranded
                   // pair 2 V, not to V by itself). Scanning past
                   // this and accepting anyway used to let such a
                   // strand form at all, whose lval cells (V's, real)
                   // ended up mixed with the literal's (not real) --
                   // the actual root cause behind Blake McBride's
                   // Bugs25 #1 crash, several layers further down in
                   // cValue::enlist_left()/get_lval_cellowner(). Not
                   // a legal selective specification; stop here so
                   // selective_spec remains false.
                   //
                   break;
                 }
            }

        // at this point the token left of V ) ← B should form a
        // selective specification. Complain if not.
        //
        if (!selective_spec)
           {
             MORE_ERROR() <<
             "Malformed selective specification";
             // see the matching reset in try_vector_assignment(): this
             // parse attempt is abandoned outright, so ASS_none (not
             // whatever this Prefix's assign_state happened to be
             // already) is the correct state for whatever runs on this
             // (reused) Prefix next.
             set_assign_state(ASS_none);
             LEFT_SYNTAX_ERROR;
           }
      }

   // definitively selective specification: replace variable V by its
   // (left-) value and repeat matching.
   //
Symbol * V = at0().get_sym_ptr();
   Assert1(V);
const Token result = V->resolve_lv(LOC);
   set_assign_state(ASS_var_seen);
   replace_at(0, result);

   // at this point variable name V was resolved to the (left-) value
   // of V. Repeat matching with the updated phrase.
   //
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_END___()
{
   /*
      this phrase occurs only if none of the other reduce_END_XXX phrases
      has matched. This happens rarely if either:

      1. the entire statement was empty (and then ssize() == 1).
         However, empty statements are normaly optimized away (in
         Executable::parse_body_line() around line 277) so that this does
         not happen under normal conditions, but conditionals may become
         an exception in the future. Currently empty conditional clauses
         are rejected at parse time in Executable::parse_body_line() around
         line 193, but this may change.

         or otherwise:

      2. an invalid phrase. In this case there are tokens other than B,
         VOID, or GOTO phrases on the stack and the end of statment is
         reacehd. This is a SYNTAX ERROR and we can throw it here rather
         than shifting the END token and failing then.

         will trigger a SYNTAX ERROR because END X will not match.
    */

   Assert1(prefix_len == 1);

   if (ssize() > 1)   push_END_error();   // case 2

   handle_ELSE(at0(), 1);

   // case 1 (empty statement)
   put = 0;
   set_action(RA_PUSH_NEXT);   // match again (w/o SHIFT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_END_VOID__()
{
   Assert1(prefix_len == 2);

   if (ssize() != 2)   syntax_error(LOC);

const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = end_of_line && (at0().get_int_val() & 1);

   handle_ELSE(at0(), 3);

   put = 0;             // pop END and VOID

Token Void(TOK_VOID);
   si.statement_result(Void, trace);
   set_action(RA_PUSH_NEXT);   // aka. SHIFT
   if (InterruptContext::attention_is_raised() && end_of_line)
      {
        const bool int_raised = InterruptContext::interrupt_is_raised();
        InterruptContext::clear_attention_raised(LOC);
        InterruptContext::clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_END_B__()
{
   Assert1(prefix_len == 2);

   if (ssize() != 2)
      {
        UCS_string & more = MORE_ERROR();
        more << "non-empty stack at end of statement:";
        loop(s, ssize())
           {
             const TokenTag tag = at(s).get_tag();
             more << " " << Token::class_name(tag);
           }
        syntax_error(LOC);
      }

const Token END = pop().get_token();   // pop END
const bool end_of_line = END.get_tag() == TOK_ENDL;
const bool trace = end_of_line && (END.get_int_val() & 1);

Token B = pop().get_token();   // pop B

   if (END.get_tag() == TOK_IF_THEN)   // end of condition B
      {
        // B is the Boolean condition of an if/else conditional.
        // END is the token after the condition (PC of the IF clause).
        //
        // If COND is 1 then continue, else jump to the else clause
        const bool cond = B.get_apl_val()->get_sole_bool();
        if (!cond)   PC = Prefix_PC(END.get_int_val());   // else clause

        Log(LOG_IfElse)
           {
             if (cond)   CERR << "IF(1) : Proceeding with THEN clause";
             else        CERR << "IF(0) : Jump to ELSE/ENDIF clause";
             CERR << " PC is now: " << PC << endl;
           }

         set_action(RA_PUSH_NEXT);
         return;
      }

   // true end of statement. This may be:
   //
   // TOK_IF_ELSE: the end of a THEN clause:         adjust the PC
   // TOK_IF_END:  the end of (or no) ELSE clause:   no op
   // TOK_END(L):  normal end of statement or line:  no op
   //
   if (END.get_tag() == TOK_IF_END)   // ←←
      {
        Log(LOG_IfElse)   CERR << 
            "ENDIF reached, PC is now: " << PC << endl;
        while (body[PC].get_tag() == TOK_IF_END)   ++PC;
        if (body[PC].get_tag() == TOK_IF_ELSE)
           PC = Prefix_PC(body[PC].get_int_val());
        Log(LOG_IfElse)   CERR << 
            "ENDIF reached, fixed PC is now: " << PC << endl;
      }
   else
      {
        handle_ELSE(END, 2);   // maybe ←→
      }

   si.fun_oper_cache.reset();
   si.statement_result(B, trace);

   set_action(RA_PUSH_NEXT);   // aka. SHIFT
   if (InterruptContext::attention_is_raised() && end_of_line)
      {
        const bool int_raised = InterruptContext::interrupt_is_raised();
        InterruptContext::clear_attention_raised(LOC);
        InterruptContext::clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_END_GOTO__()   // Escape ( → )
{
   /*
       Normally:

       1a. reduce_END_GOTO__ happens for APL → aka. ESC
       1b. tokens TOK_BRANCH_INT and TOK_NOBRANCH are local result token
           returned by si.jump(line) in reduce_END_GOTO_B_(), where
           si.jump() also modifies the PC.

       In UserFunction::optimize_labels() we optimize
       the statements →N (with literal integer N) and →LABEL into a single
       token TOK_GOTO_PC whose integer value is the new PC, and:

       2a. a value type of TV_INT of the GOTO token indicates that this
           optimization was performed
       2b. the int value is the new PC
       2c. a PC of -1 means: return from the function
    */
    if (at1().get_tag() == TOK_GOTO_PC)   // optimized →N
       {
         reset(LOC);
         const Prefix_PC new_PC = Prefix_PC(at1().get_int_val());
         if (new_PC == Prefix_PC_invalid)   // →0 etc.
            {
              PC = body.end_PC() - 1;   // RETURN_XXX
              const Token result(TOK_VOID);
              pop_args_push_result(result);
              set_action(RA_CONTINUE);      // return from defined function
            }
         else                             // normal →N
            {
              PC = new_PC;
              set_action(RA_PUSH_NEXT);
            }
         return;
       }

   Assert1(prefix_len == 2);

   if (ssize() != 2)   syntax_error(LOC);

   si.fun_oper_cache.reset();

const bool trace = at0().get_Class() == TC_END && (at0().get_int_val() & 1);
   if (trace)
      {
        Token esc(TOK_ESCAPE);
        si.statement_result(esc, true);
      }

   // the statement is → which could mean TOK_ESCAPE (normal →) or
   //  TOK_STOP_LINE from S∆←line
   //
   if (at1().get_tag() == TOK_STOP_LINE)   // S∆ line
      {
        // property 2 (ignore-attention) is inherited from every calling
        // )SI entry too (apl2lrm.txt p.360-361 "or-ing"), not just this
        // function's own.
        //
        if (si.get_inherited_exec_property(2))
           {
              // the function ignores attention (aka. weak interrupt)
              //
              handle_ELSE(at0(), 5);
              pop_and_discard();   // pop END
              pop_and_discard();   // pop GOTO

              // nothing is left to match: continue with the next statement
              // (RA_CONTINUE matched the empty FIFO and ended in
              // reduce____(), i.e. a FIXME, Bugs34 #1)
              set_action(RA_PUSH_NEXT);
              return;
           }

        COUT << si.function_name() << "[" << si.get_line() << "]" << endl;
        const Token result(TOK_ERROR, E_STOP_LINE);
        pop_args_push_result(result);
        set_action(RA_RETURN);            // return from context;
      }
   else
      {
        const Token result(TOK_ESCAPE);
        pop_args_push_result(result);
        set_action(RA_RETURN);            // return from context;
      }
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_END_GOTO_B_()
{
   // monadic →LABEL. Preserves the at0() token and reduces only GOTO_B.

   Assert1(prefix_len == 3);

   si.fun_oper_cache.reset();

   // at0() is either TOK_ENDL (end of line), or one of TOK_END, TOK_FUN12,
   //       or TOK_OPER1
   //
const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = end_of_line && (at0().get_int_val() & 1);

   // →LAB vs →4/→2+2: si.jump() below always tags a real branch-back
   // result as TOK_BRANCH_INT since it only ever sees the resolved
   // integer value; the label/non-label distinction is determined here
   // instead (Symbol::resolve_right()'s NC_LABEL case tags a label's
   // resolved value TOK_APL_VALUE5 rather than the ordinary
   // TOK_APL_VALUE1, so the provenance travels with the value itself),
   // and applied to the result afterwards.
const bool is_label = at2().get_tag() == TOK_APL_VALUE5;
const cValue * line = at2().get_apl_val().get();

   // produce ⎕TRACE output if enabled and branch is not empty
   //
   if (trace && line->element_count() > 0)
      {
        const ShapeItem line_num = line->get_line_number();
        Token bra(is_label ? TOK_BRANCH_LAB : TOK_BRANCH_INT, line_num);
        si.statement_result(bra, true);   // display trace line
      }

const Token result = si.jump(*line);   // may change the PC

   if (result.get_tag() == TOK_BRANCH_INT)   // branch back into a function
      {
        Log(LOG_prefix_parser)
           {
             CERR << "Leaving context after " << result << endl;
           }

        const Token tagged(is_label ? TOK_BRANCH_LAB : TOK_BRANCH_INT,
                            result.get_int_val());
        pop_args_push_result(tagged);
        set_action(RA_RETURN);            // return from context;
        return;
      }

   if (result.get_tag() == TOK_NOBRANCH)   // branch not taken, e.g. →⍬
      {
        handle_ELSE(at0(), 4);
        reset(LOC);
        set_action(RA_PUSH_NEXT);   // again with modified stack
        return;
      }

   reset(LOC);   // branch taken: terminate current statement

   /* NOTE: the →N cases with N≤0 or N≥↑⍴⎕CR 'FUNCTION' are handled in
      UserFunction::pc_for_line(). pc_for_line() sets the PC to the end
      of the function (which then returns):

      StateIndicator::jump()   above
      └── StateIndicator::jump_to_line()
          └── UserFunction::pc_for_line()
    */
   Assert(result.get_tag() == TOK_VOID);   // branch taken, i.e. →N in ∇-context
   branch_within_function(end_of_line);    // does set_action(RA_PUSH_NEXT)
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_A_GOTO_B_()
{
   // dyadic LABEL → CONDITION.

   Assert1(prefix_len == 3);

   // we want this to be fast, therefore we don't check the shape but
   // rather use ↑A and ↑B,
   //
Cell cache_A, cache_B;
const Cell & A0 = at0().get_apl_val()->get_cfirst(cache_A); // the jump offset
const Cell & B0 = at2().get_apl_val()->get_cfirst(cache_B); // the condition

   if (!A0.is_near_int())
      {
        MORE_ERROR() << "A → B: bad (non-Integer) ↑A ";
        DOMAIN_ERROR;
      }

   if (!B0.is_near_int())
      {
        MORE_ERROR() << "A → B: bad (non-Integer) ↑B ";
        DOMAIN_ERROR;
      }

   if (B0.get_near_int() == 0)  // the branch is not taken
      {
        const Token result(TOK_APL_VALUE2, Idx0(LOC));
        pop_args_push_result(result);
        set_action(RA_CONTINUE);   // match again (w/o SHIFT)
        return;
      }

const APL_Integer jump_offset = A0.get_near_int();

   if (const UserFunction * ufun = si.get_executable()->get_exec_ufun())
      {
        // A → B in a ∇-context (lambda or ∇-function)
        //
        // If A came directly from a label symbol (LAB→B), it is tagged
        // TOK_APL_VALUE5 (Symbol::resolve_right()'s NC_LABEL case), and
        // that label's own value already IS the absolute target line --
        // branch there directly, exactly like monadic →LAB, matching
        // the naive (and, for a GNU extension nobody but us has seen
        // before, entirely reasonable) assumption that "LAB→B jumps to
        // LAB". A literal or computed A (0→cond, (N+1)→cond, ...) keeps
        // its existing, established meaning: an offset relative to the
        // current line -- this is what the fast 1-line-loop idiom
        // (0→MAX_N>N←N+1) still relies on. Labels can only occur in a
        // real defined function's own body (never a lambda's -- lambdas
        // have no labels -- and never reachable here at all via ⍎,
        // which SYNTAX_ERRORs on nonzero A before this point), so no
        // further guarding is needed: reaching here with a
        // TOK_APL_VALUE5 A always means a genuine label of *this*
        // function.
        //
        // PC was already incremented and now points to the next token
        // after the branch. In order to compute the proper RELATIVE
        // line number, we have to use the PC BEFORE the branch.
        //
        const bool A_is_label = at0().get_tag() == TOK_APL_VALUE5;
        const int function_line = A_is_label
                                 ? int(jump_offset)   // absolute: LAB's own line
                                 : ufun->get_line(PC - prefix_len) + jump_offset;

        // Unlike monadic →0/→N, where landing at or past the end of the
        // function is the standard, deliberate "return" idiom, an
        // out-of-range target here is far more likely to be an
        // accident: A→B's relative offset silently points somewhere
        // different every time a line is added or removed between the
        // branch and its intended target, and a label can be misspelled
        // or the wrong one referenced. Letting that silently "return"
        // instead of erroring would hide exactly the kind of mistake
        // that's hardest to catch after the fact -- so, deliberately
        // unlike monadic →, any out-of-range A→B target is a DOMAIN
        // ERROR instead.
        if (!ufun->is_valid_line(Function_Line(function_line)))
           {
             MORE_ERROR() << "A → B: branch target line " << function_line
                          << " is outside " << ufun->get_name()
                          << "'s valid line range 1.."
                          << ufun->get_last_line();
             DOMAIN_ERROR;
           }

        si.jump_to_line(Function_Line(function_line));   // changes the PC
        branch_within_function(true);   // check ^C and set_action(RA_PUSH_NEXT)
        reset(LOC);                     // abort the current statement

        // branch_within_function() MAY have set the PC to the end of the
        // function (token ENDL). However, that skips the return of the
        // ∇-result. Fix it.
        //
        if (PC >= body.end_PC())           // PC at or past end of function
           {
             Assert(body[PC-1].get_tag() == TOK_ENDL);
             PC = body.end_PC() - 1;
           }
      }
   else         // ⍎ (or ⍎¨, ⎕EC, ⎕EA, ⎕EB, which all bottom out in the same
                // ExecuteList-based mechanism) or plain top-level immediate
                // execution (◊)
      {
        if (jump_offset == 0)
           {
             // the supported "retry this statement" idiom (e.g. Branch.tc's
             // "0→0<N←⎕←N-1") -- restart the ⍎'d string itself, cheaply, by
             // resetting this context's own PC. No enclosing function's
             // line table is involved, so this is unaffected by the below.
             //
             goto_PC(Prefix_PC_0);   // calls reset(), so don't pop_args()!
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
             return;
           }

        // nonzero A: this needs an enclosing DEFINED FUNCTION to be
        // relative to (or, for a label, to belong to). get_exec_ufun()
        // on this context (⍎'s own, synthetic ExecuteList body) is
        // always 0, so it can't by itself distinguish "genuinely
        // top-level, no enclosing function at all" from "running inside
        // a defined function, just indirectly via ⍎/⎕EC/⎕EA/⎕EB's own
        // separate body, whose tokens have no line numbers of the
        // enclosing function's own". Workspace::SI_top_fun() answers
        // that properly: it walks up the )SI, past any number of nested
        // ⍎/◊ frames, to the nearest real function (if any) -- exactly
        // the same lookup Command.cc's TOK_BRANCH_INT handler performs
        // when it later APPLIES the jump (via si->goon()), so "compute"
        // here and "apply" there always agree on which function, and
        // which of its lines, A is relative to -- however deep the ⍎
        // nesting. TOK_BRANCH_INT/TOK_BRANCH_LAB always carry an
        // ABSOLUTE line number (see StateIndicator::jump_to_line()), so
        // once function_line below is computed there is nothing left to
        // do here but hand it off the same way monadic →LAB/→N already
        // does from ⍎ context: pop_args_push_result() + RA_RETURN, and
        // let it bubble up to be applied there.
        //
        const bool A_is_label = at0().get_tag() == TOK_APL_VALUE5;
        StateIndicator * fsi = Workspace::SI_top_fun();

        if (fsi == 0)
           {
             MORE_ERROR() << "A → B: nonzero A is not permitted when "
                             "evaluated via ⍎, ⍎¨, ⎕EC, ⎕EA, or ⎕EB with no "
                             "enclosing defined function -- there is no "
                             "line for A to be relative to (or, for a "
                             "label, no function it could belong to).";
             SYNTAX_ERROR;
           }

        const UserFunction * fun = fsi->get_executable()->get_exec_ufun();
        Assert(fun);
        const int64_t function_line = A_is_label
                                 ? jump_offset   // absolute: LAB's own line
                                 : fsi->get_line() + jump_offset;

        // see the matching comment in the ∇-context branch above: an
        // out-of-range A → B target is deliberately a DOMAIN ERROR rather
        // than a silent return, regardless of how many ⍎ levels deep it
        // was evaluated from.
        if (!fun->is_valid_line(Function_Line(function_line)))
           {
             MORE_ERROR() << "A → B: branch target line " << function_line
                          << " is outside " << fun->get_name()
                          << "'s valid line range 1.."
                          << fun->get_last_line();
             DOMAIN_ERROR;
           }

        const Token tagged(A_is_label ? TOK_BRANCH_LAB : TOK_BRANCH_INT,
                            function_line);
        pop_args_push_result(tagged);
        set_action(RA_RETURN);            // return from context;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_RETC___()
{
   Assert1(prefix_len == 1);

   // same diagnostic as at the end of a ◊ or ∇ statement, e.g. VALENCE
   // ERROR for ⍎'+/' like for +/ (Bugs37 #43)
   //
   if (ssize() != 1)   push_END_error();

   // action is RA_RETURN, therefore the entire si.fun_oper_cache will be
   // discarded and no reset() of it is required.

   // end of context reached. There are 4 cases:
   //
   // TOK_RETURN_STATS:  end of ◊ context
   // TOK_RETURN_EXEC:   end of ⍎ context with no result (e.g. ⍎'')
   // TOK_RETURN_VOID:   end of ∇ (no result)
   // TOK_RETURN_SYMBOL: end of ∇ (result in Z)
   //
   // case TOK_RETURN_EXEC (end of ⍎ context) is handled in reduce_RETC_A___()
   //
   switch(at0().get_tag())
      {
        case TOK_RETURN_EXEC:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ⍎ context (no result)" << endl;
             replace_at(0, Token(TOK_VOID));
             set_action(RA_RETURN);            // return from context;
             return;

        case TOK_RETURN_STATS:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ◊ context" << endl;
             replace_at(0, Token(TOK_VOID));
             set_action(RA_RETURN);            // return from context;
             return;

        case TOK_RETURN_VOID:   // user-defined function not returning a value
             Log(LOG_prefix_parser)
                CERR << "- end of ∇ context (function has no result)" << endl;

             {
               const UserFunction * ufun = si.get_executable()->get_exec_ufun();
               if (ufun)   { /* do nothing, needed for -Wall */ }
               Assert1(ufun);
               replace_at(0, Token(TOK_VOID));
             }
             set_action(RA_RETURN);            // return from context;
             return;

        case TOK_RETURN_SYMBOL:   // user-defined function returning a value
             {
               const UserFunction * ufun = si.get_executable()->get_exec_ufun();
               Assert1(ufun);
               Symbol * ufun_Z = ufun->get_sym_Z();
               Value_P Z;
               if (ufun_Z)   Z = ufun_Z->get_var_value();
               if (!Z)
                  {
                    Log(LOG_prefix_parser)
                       CERR << "- end of ∇ context (MISSING function result)."
                            << endl;
                    replace_at(0, Token(TOK_VOID));
                  }
               else
                  {
                    Log(LOG_prefix_parser)
                       CERR << "- end of ∇ context (function result is: "
                            << *Z << ")" << endl;
                    replace_at(0, Token(TOK_APL_VALUE1, Z));
                  }
             }
             set_action(RA_RETURN);            // return from context;
             return;

        default: break;
      }

   // not reached
   //
   Q1(at0().get_tag())   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_RETC_VOID__()
{
   // e.g.      ⎕FX 'qio' '⎕IO' ◊ ⍎'qio'
   //
   // execute of a defined function without result. Im[pssible for lambdas
   // since they always have a result λ←.

   Assert1(prefix_len == 2);

   // a value left of the function, e.g. ⍎'N 0': VALUE ERROR (from
   // syntax_error()) like in reduce_END_VOID__(). Bugs37 #46.
   //
   if (ssize() != 2)   syntax_error(LOC);

   // action is RA_RETURN, therefore the entire si.fun_oper_cache will be
   // discarded and no reset() of it is required.

const Token result(TOK_VOID);   // function result is VOID
   pop_args_push_result(result);
   set_action(RA_RETURN);            // return from context;
}
//────────────────────────────────────────────────────────────────────────────
// Note: reduce_RETC_A___ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_B___ instead.
//
void
Prefix::reduce_RETC_A__()
{
   Assert1(prefix_len == 2);

   if (ssize() != 2)   // there extra tokens
      {
        syntax_error(LOC);
      }

   // action is RA_RETURN, therefore the entire si.fun_oper_cache will be
   // discarded and no reset() of it is required.

   Log(LOG_prefix_parser)   CERR << "- end of ⍎ context.";

Token B = at1();
   pop_args_push_result(B);

   set_action(RA_RETURN);            // return from context;
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_RETC_GOTO__()
{
   Assert1(prefix_len == 2);

   // Note: reduce_RETC_ESC___ can only happen for context ⍎, since
   //       contexts ◊ and ∇ use reduce_END_ESC___ instead

   if (ssize() != 2)   syntax_error(LOC);

   reduce_END_GOTO__();
}
//────────────────────────────────────────────────────────────────────────────
void
Prefix::reduce_RETC_GOTO_B_()
{
   // Note: reduce_RETC_GOTO_B__ can only happen for context ⍎, since
   //       the contexts ◊ and ∇ use reduce_END_GOTO_B__ instead.

   if (ssize() != 3)   syntax_error(LOC);

   // monadic →LABEL.

   Assert1(prefix_len == 3);

   si.fun_oper_cache.reset();

   // at0() is either TOK_ENDL (end of line), or one of TOK_END, TOK_FUN12,
   //       or TOK_OPER1
   //
const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = end_of_line && (at0().get_int_val() & 1);

   // see the matching comment in reduce_END_GOTO_B_()
const bool is_label = at2().get_tag() == TOK_APL_VALUE5;
const cValue * line = at2().get_apl_val().get();

   // produce ⎕TRACE output if enabled and branch is not empty
   //
   if (trace && line->element_count() > 0)
      {
        const ShapeItem line_num = line->get_line_number();
        Token bra(is_label ? TOK_BRANCH_LAB : TOK_BRANCH_INT, line_num);
        si.statement_result(bra, true);   // display trace line
      }

const Token result = si.jump(*line);   // may change the PC

   if (result.get_tag() == TOK_BRANCH_INT)   // branch back into a function
      {
        Log(LOG_prefix_parser)
           {
             CERR << "Leaving context after " << result << endl;
           }

        const Token tagged(is_label ? TOK_BRANCH_LAB : TOK_BRANCH_INT,
                            result.get_int_val());
        pop_args_push_result(tagged);
        set_action(RA_RETURN);            // return from context;
        return;
      }

   if (result.get_tag() == TOK_NOBRANCH)   // branch not taken, e.g. →⍬
      {
        // unlike reduce_END_GOTO_B (which can simply reset(LOC) and then
        //  RA_CONTINUE with the next token), we need a TOK_VOID token that
        //  will be returned to the caller of the current ⍎ context.
        //
        const Token result(TOK_VOID);   // function result is VOID
        pop_args_push_result(result);
        set_action(RA_RETURN);   // again with modified stack
        return;
      }

   reset(LOC);   // branch taken: terminate current statement

   /* NOTE: the →N cases with N≤0 or N≥↑⍴⎕CR 'FUNCTION' are handled in 
      UserFunction::pc_for_line(). pc_for_line() sets the PC to the end
      of the function (which then returns):

      StateIndicator::jump()   above
      └── StateIndicator::jump_to_line()
          └── UserFunction::pc_for_line()
    */
   Assert(result.get_tag() == TOK_VOID);   // branch taken, i.e. →N in ∇-context
   branch_within_function(end_of_line);    // does set_action(RA_PUSH_NEXT)

   pop_args_push_result(result);   // proposed by Claude Code
   set_action(RA_RETURN);          // return from context;
}
//────────────────────────────────────────────────────────────────────────────
// Note: reduce_RETC_A_GOTO_B happens only for context ⍎, since
//       the contexts ◊ and ∇ use reduce_END_GOTO_B__ instead.
//
void
Prefix::reduce_RETC_A_GOTO_B()
{
   Assert1(prefix_len == 4);
   syntax_error(LOC);
}
//════════════════════════════════════════════════════════════════════════════

