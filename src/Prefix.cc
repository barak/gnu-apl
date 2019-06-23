/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#include "Bif_OPER2_RANK.hh"
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

//-----------------------------------------------------------------------------
Prefix::Prefix(StateIndicator & _si, const Token_string & _body)
   : instance(++instance_counter),
     si(_si),
     put(0),
     saved_lookahead(Token(TOK_VOID), Function_PC_invalid),
     body(_body),
     PC(Function_PC_0),
     assign_state(ASS_none),
     lookahead_high(Function_PC_invalid),
     action(RA_FIXME)
{
}
//-----------------------------------------------------------------------------
void
Prefix::clean_up()
{
   loop(s, size())
      {
        Token tok = at(s).tok;
        if (tok.get_Class() == TC_VALUE)
           {
             tok.extract_apl_val(LOC);
           }
        else if (tok.get_ValueType() == TV_INDEX)
           {
             tok.get_index_val().extract_all();
           }
      }

   put = 0;
}
//-----------------------------------------------------------------------------
void
Prefix::syntax_error(const char * loc)
{
   // move the PC back to the beginning of the failed statement
   //
   while (PC > 0)
      {
        --PC;
        if (body[PC].get_Class() == TC_END)
           {
             ++PC;
             break;
           }
      }

   // clear values in FIFO
   //
   loop (s, size())
      {
        Token & tok = at(s).tok;
        if (tok.get_Class() == TC_VALUE)
           {
             Value_P val = tok.get_apl_val();
          }
      }

   // see if error was caused by a function not returning a value.
   // in that case we throw a value error instead of a syntax error.
   //
   loop (s, size())
      {
        if (at(s).tok.get_Class() == TC_VOID)
           throw_apl_error(E_VALUE_ERROR, loc);
      }

   throw_apl_error(get_assign_state() == ASS_none ? E_SYNTAX_ERROR
                                                  : E_LEFT_SYNTAX_ERROR, loc);
}
//-----------------------------------------------------------------------------
bool
Prefix::uses_function(const UserFunction * ufun) const
{
   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() == TV_FUN &&
            tok.get_function() == ufun)   return true;
      }

   if (saved_lookahead.tok.get_ValueType() == TV_FUN &&
            saved_lookahead.tok.get_function() == ufun)   return true;

   return false;
}
//-----------------------------------------------------------------------------
bool
Prefix::is_value_parenthesis(int pc) const
{
   // we have ) XXX with XXX on the stack and need to know if the evaluation
   // of (... ) will be a value as in e.g. (1 + 1) or a function as in (+/).
   //
   Assert1(body[pc].get_Class() == TC_R_PARENT);

   ++pc;
   if (pc >= int(body.size()))   return true;   // syntax error

TokenClass next = body[pc].get_Class();

   if (next == TC_R_BRACK)   // skip [ ... ]
      {
        const int offset = body[pc].get_int_val2();
        pc += offset;
        Assert1(body[pc].get_Class() == TC_L_BRACK);   // opening [
        if (pc >= Function_PC(body.size()))   return true;   // syntax error
        next = body[pc].get_Class();
      }

   if (next == TC_SYMBOL)   // resolve symbol if necessary
      {
        const Symbol * sym = body[pc].get_sym_ptr();
        const NameClass nc = sym->get_nc();

        if (nc == NC_FUNCTION)   return false;
        if (nc == NC_OPERATOR)   return false;
        return true;
      }

   if (next == TC_OPER1)   return false;
   if (next == TC_OPER2)   return false;
   if (next == TC_FUN12)   return false;

   if (next == TC_L_PARENT)   // )) XXX
      {
        ++pc;
        if (!is_value_parenthesis(pc))   return false;   // (fun)) XXX
        const int offset = body[pc].get_int_val2();
        pc += offset;
        if (pc >= Function_PC(body.size()))   return true;   // syntax error
        next = body[pc].get_Class();
        Assert1(next == TC_L_PARENT);   // opening (
        ++pc;
        if (pc >= Function_PC(body.size()))   return true;   // syntax error

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
   if (next == TC_VALUE                  &&
       pc < Function_PC(body.size() - 1) &&
       body[pc+1].get_Class() == TC_OPER2)   return false;

   return true;
}
//-----------------------------------------------------------------------------
bool
Prefix::is_fun_or_oper(int pc) const
{
   // this function is called when / ⌿ \ or ⍀ shall be resolved. pc points
   // to the token left of / ⌿ \ or ⍀.
   //
TokenTag tag_LO = body[pc].get_tag();

   if (tag_LO == TOK_R_BRACK)
      {
        // e.g. fun[...]/ or value[...]/ Skip over [...]
        //
        pc += body[pc].get_int_val2();
        Assert1(body[pc++].get_Class() == TC_L_BRACK);   // opening [
        tag_LO = body[pc].get_tag();
      }

   if (tag_LO == TOK_R_PARENT)   return !is_value_parenthesis(pc);
   if (body[pc].get_Class() == TC_OPER2)   return false;   // make / a function

   if ((tag_LO & TV_MASK) == TV_FUN)   return true;

   if (tag_LO == TOK_SYMBOL)
      {
        Symbol * sym = body[pc].get_sym_ptr();
        if (sym == 0)   return false;
        return sym->get_function() != 0;
      }

   return false;   // not a function or operator
}
//-----------------------------------------------------------------------------
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
const bool left_sym = get_assign_state() == ASS_arrow_seen;
   return sym->resolve_class(left_sym) == TC_VALUE;
}
//-----------------------------------------------------------------------------
int
Prefix::vector_ass_count() const
{
int count = 0;

   for (Function_PC pc = PC; pc < Function_PC(body.size()); ++pc)
       {
         if (body[pc].get_tag() != TOK_LSYMB2)   break;
         ++count;
       }

   return count;
}
//-----------------------------------------------------------------------------
void
Prefix::print_stack(ostream & out, const char * loc) const
{
const int si_depth = si.get_level();

   out << "fifo[si=" << si_depth << " len=" << size()
       << " PC=" << PC << "] is now :";

   loop(s, size())
      {
        const TokenClass tc = at(s).tok.get_Class();
        out << " " << Token::class_name(tc);
      }

   out << "  at " << loc << endl;
}
//-----------------------------------------------------------------------------
int
Prefix::show_owners(const char * prefix, ostream & out,
                          const Value & value) const
{
int count = 0;

   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() != TV_VAL)      continue;

        if (Value::is_or_contains(tok.get_apl_val().get(), value))
           {
             out << prefix << " Fifo [" << s << "]" << endl;
             ++count;
           }

      }

   return count;
}
//-----------------------------------------------------------------------------
Function_PC
Prefix::get_range_high() const
{
   // if the stack is empty then return the last address (if any) or otherwise
   // the address of the next token.
   //
   if (size() == 0)     // stack is empty
      {
        if (lookahead_high == Function_PC_invalid)   return PC;
        return lookahead_high;
      }

   // stack non-empty: return address of highest item or the last address
   //
Function_PC high = lookahead_high;
   if (high == Function_PC_invalid)   high = at(0).pc;

   if (best && best->misc)   --high;
   return high;
}
//-----------------------------------------------------------------------------
Function_PC
Prefix::get_range_low() const
{
   // if the stack is not empty then return the PC of the lowest element
   //
   if (size() > 0)     return at(size() - 1).pc;


   // the stack is empty. Return the last address (if any) or otherwise
   // the address of the next token.
   //
   if (lookahead_high == Function_PC_invalid)   return PC;   // no last address
   return lookahead_high;
}
//-----------------------------------------------------------------------------
bool
Prefix::value_expected()
{
   // return true iff the current saved_lookahead token (which has been tested
   // to be a TC_INDEX token) is the index of a value and false it it is
   // a function axis

   // if it contains semicolons then get_ValueType() is TV_INDEX and
   // it MUST be a value.
   //
   if (saved_lookahead.tok.get_ValueType() == TV_INDEX)   return true;

   for (int pc = PC; pc < int(body.size());)
      {
        const Token & tok = body[pc++];
        switch(tok.get_Class())
           {
               case TC_R_BRACK:   // skip over [...] (func axis or value index)
                    //
                    pc += tok.get_int_val2();
                    continue;

               case TC_END:     return false;   // syntax error

               case TC_FUN0:    return true;   // niladic function is a value
               case TC_FUN12:   return false;  // function

               case TC_SYMBOL:
                    {
                      const Symbol * sym = tok.get_sym_ptr();
                      const NameClass nc = sym->get_nc();

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
//-----------------------------------------------------------------------------
void
Prefix::unmark_all_values() const
{
   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() != TV_VAL)      continue;

        Value_P value = tok.get_apl_val();
        if (!!value)   value->unmark();
      }
}
//-----------------------------------------------------------------------------

// a hash table with all prefixes that can be reduced...
#define PH(name, idx, prio, misc, len, fun) \
   { #name, idx, prio, misc, len, &Prefix::reduce_ ## fun, #fun },

#include "Prefix.def"   // the hash table

Token
Prefix::reduce_statements()
{
   Log(LOG_prefix_parser)
      {
        CERR << endl << "changed to Prefix[si=" << si.get_level()
             << "]) ============================================" << endl;
      }

   if (size() > 0)   goto again;

grow:
   // the current stack does not contain a valid phrase.
   // Push one more token onto the stack and continue
   //
   {
     if (saved_lookahead.tok.get_tag() != TOK_VOID)
        {
          // there is a MISC token from a MISC phrase. Use it.
          //
          push(saved_lookahead);
          saved_lookahead.tok.clear(LOC);
          goto again;   // success
        }

     // if END was reached, then there are no more token in current-statement
     //
     if (size() > 0 && at0().get_Class() == TC_END)
        {
          Log(LOG_prefix_parser)   print_stack(CERR, LOC);

          // provide help on some common cases...
          //
          for (int j = 1; j < (size() - 1); ++j)
              {
                if ( (at(j).tok.get_Class() == TC_ASSIGN)    &&
                     (at(j + 1).tok.get_Class() == TC_VALUE))
                   {
                     if (at(j - 1).tok.get_Class() == TC_FUN0 ||
                         at(j - 1).tok.get_Class() == TC_FUN12)
                        {
                           MORE_ERROR() <<
                           "Cannot assign a value to a function";
                        }
                     else if (at(j - 1).tok.get_Class() == TC_OPER1 ||
                             at(j - 1).tok.get_Class() == TC_OPER1)
                        {
                           MORE_ERROR() <<
                           "Cannot assign a value to an operator";
                        }
                   }
              }
          syntax_error(LOC);   // no more token
        }

     Token_loc tl = lookahead();
     Log(LOG_prefix_parser)
        {
          CERR << "    [si=" << si.get_level() << " PC=" << (PC - 1)
               << "] Read token[" << size()
               << "] (←" << get_assign_state() << "←) " << tl.tok << " "
               << Token::class_name(tl.tok.get_Class()) << endl;
        }

     lookahead_high = tl.pc;
     TokenClass tcl = tl.tok.get_Class();

     if (tcl == TC_SYMBOL)   // resolve symbol if necessary
        {
          // reset the PC back to the previous token, so that a failed
          // resolve() (aka VALUE ERROR) will re-fetch the token
          //
          // But not if symbol is ⎕LC because that would make the first
          // element of ⎕LC too low!
          //
          if (tl.tok.get_tag() != TOK_Quad_LC)   --PC;

          Symbol * sym = tl.tok.get_sym_ptr();
          if (tl.tok.get_tag() == TOK_LSYMB2)
             {
               // this is the last token C of a vector assignment
               // (A B ... C)←. We return C and let the caller do the rest
               //
               sym->resolve(tl.tok, true);
               Log(LOG_prefix_parser)
                  CERR << "TOK_LSYMB2 " << sym->get_name()
                       << "resolved to " << tl.tok << endl;
             }
          else
             {
               const bool left_sym = get_assign_state() == ASS_arrow_seen;
               bool resolved = false;
               if (size() > 0 && at(0).tok.get_Class() == TC_INDEX && 
                   tl.tok.get_tag() == TOK_SYMBOL)   // user defined variable
                  {
                    // indexed reference, e.g. A[N]. Calling sym->resolve()
                    // would copy the entire variable and then index it, which
                    // is inefficient if the variable is big. We rather call
                    // Symbol::get_value() directly in order to avoid that
                    //
                    Value_P val = sym->get_value();
                    if (!!val && !left_sym)
                       {
                         Token tok(TOK_APL_VALUE1, val);
                         tl.tok.move_1(tok, LOC);
                         resolved = true;
                       }
                  }
               if (!resolved)   sym->resolve(tl.tok, left_sym);

               if (left_sym)   set_assign_state(ASS_var_seen);
               Log(LOG_prefix_parser)
                  {
                    if (left_sym)   CERR << "TOK_LSYMB ";
                    else            CERR << "TOK_SYMBOL ";
                    CERR << "resolved to " << tl.tok << endl;
                  }
             }
          PC = lookahead_high + 1;   // resolve() succeeded: restore PC

          Log(LOG_prefix_parser)
             {
               CERR << "   resolved symbol " << sym->get_name()
                    << " to " << tl.tok.get_Class() << endl;
             }

          if (tl.tok.get_tag() == TOK_SI_PUSHED)
            {
              // Quad_Quad::resolve() calls ⍎ which returns TOK_SI_PUSHED.
              //
              push(tl);
              return Token(TOK_SI_PUSHED);
            }
        }
     else if (tcl == TC_ASSIGN)   // resolve symbol if necessary
        {
          if (get_assign_state() != ASS_none)   syntax_error(LOC);
          set_assign_state(ASS_arrow_seen);
        }

     push(tl);
   }

again:
   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

   // search prefixes in phrase table...
   //
   {
     const int hash_0 = at0().get_Class();

     if (size() >= 3)
        {
          const int hash_01  = hash_0  | at1().get_Class() <<  5;
          const int hash_012 = hash_01 | at2().get_Class() << 10;

          if (size() >= 4)
             {
               const int hash_0123 = hash_012 | at3().get_Class() << 15;
               best = hash_table + hash_0123 % PHRASE_MODU;
               if (best->phrase_hash == hash_0123)   goto found_prefix;
             }

          best = hash_table + hash_012 % PHRASE_MODU;
          if (best->phrase_hash == hash_012)   goto found_prefix;

          best = hash_table + hash_01 % PHRASE_MODU;
          if (best->phrase_hash == hash_01)   goto found_prefix;

          best = hash_table + hash_0 % PHRASE_MODU;
          if (best->phrase_hash != hash_0)   goto grow;   // no matching phrase
        }
     else   // 0 < size() < 3
        {
          if (size() >= 2)
             {
               const int hash_01 = hash_0 | at1().get_Class() << 5;
               best = hash_table + hash_01 % PHRASE_MODU;
               if (best->phrase_hash == hash_01)   goto found_prefix;
             }

          best = hash_table + hash_0 % PHRASE_MODU;
          if (best->phrase_hash != hash_0)   goto grow;
        }
   }

found_prefix:

   // found a reducible prefix. See if the next token class binds stronger
   // than best->prio
   //
   {
     TokenClass next = TC_INVALID;
     if (PC < Function_PC(body.size()))
        {
          const Token & tok = body[PC];

          next = tok.get_Class();
          if (next == TC_SYMBOL)
             {
               Symbol * sym = tok.get_sym_ptr();
               const bool left_sym = get_assign_state() == ASS_arrow_seen;
               next = sym->resolve_class(left_sym);
            }
        }

     if (best->misc && (at0().get_Class() == TC_R_BRACK))
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
               // onto the stack. The following token are processed until the
               // entire indexed value A[ ... ] is computed
               prefix_len = 1;
               reduce_RBRA___();
               goto grow;
             }
        }

//   Q(next) Q(at0())

     // we could reduce, but we could also shift. Compute more, which is true
     // if we should shift.
     //
     const bool shift = dont_reduce(next);
     if (shift)
        {
           Log(LOG_prefix_parser)  CERR
               << "   phrase #" << (best - hash_table)
               << ": " << best->phrase_name
               << " matches, but prio " << best->prio
               << " is too small to call " << best->reduce_name
               << "()" << endl;
          goto grow;
        }
   }

   Log(LOG_prefix_parser)  CERR
      << "   phrase #" <<  (best - hash_table)
      << ": " << best->phrase_name
      << " matches, prio " << best->prio
      << ", calling reduce_" << best->reduce_name
      << "()" << endl;

   action = RA_FIXME;
   prefix_len = best->phrase_len;
   if (best->misc)   // MISC phrase: save X and remove it
      {
        Assert(saved_lookahead.tok.get_tag() == TOK_VOID);
        saved_lookahead.copy(pop(), LOC);
        --prefix_len;
      }

const uint64_t inst = instance;
   (this->*best->reduce_fun)();

   if (inst != Workspace::SI_top()->get_prefix().instance)
      {
        // the reduce_fun() above has changed the )SI stack. As a consequence
        // the 'this' pointer is no longer valid and we must not access members
        // of this Prefix instance.
        //
        return Token(TOK_SI_PUSHED);
      }

   Log(LOG_prefix_parser)
      CERR << "   reduce_" << best->reduce_name << "() returned: ";

   // handle action (with decreasing likelihood)
   //
   if (action == RA_CONTINUE)
      {
        Log(LOG_prefix_parser)   CERR << "RA_CONTINUE" << endl;
        goto again;
      }

   if (action == RA_PUSH_NEXT)
      {
        Log(LOG_prefix_parser)   CERR << "RA_PUSH_NEXT" << endl;
        goto grow;
      }

   if (action == RA_SI_PUSHED)
      {
        Log(LOG_prefix_parser)   CERR << "RA_SI_PUSHED" << endl;
        return Token(TOK_SI_PUSHED);
      }

   if (action == RA_RETURN)
      {
        Log(LOG_prefix_parser)   CERR << "RA_RETURN" << endl;
        return pop().tok;
      }

   if (action == RA_FIXME)
      {
        Log(LOG_prefix_parser)   CERR << "RA_FIXME" << endl;
        FIXME;
      }

   FIXME;
}
//-----------------------------------------------------------------------------
bool
Prefix::dont_reduce(TokenClass next) const
{
   if (at0().get_Class() == TC_VALUE)
      {
        if (next == TC_OPER2)           // DOP B
           {
             return true;
           }
        else if (next == TC_VALUE)      // A B
           {
             return best->prio < BS_VAL_VAL;
           }
        else if (next == TC_R_PARENT)   // ) B
           {
             if (is_value_parenthesis(PC))     // e.g. (X+Y) B
                {
                  return best->prio < BS_VAL_VAL;
                }
              else                      // e.g. (+/) B
                {
                  return false;
                }
           }
      }
   else if (at0().get_Class() == TC_FUN12)
      {
        if (next == TC_OPER2)
           {
             return true;
           }
      }

   return false;
}
//-----------------------------------------------------------------------------
bool
Prefix::replace_AB(Value_P old_value, Value_P new_value)
{
   Assert(!!old_value);
   Assert(!!new_value);

   loop(s, size())
     {
       Token & tok = at(s).tok;
       if (tok.get_Class() != TC_VALUE)   continue;
       if (tok.get_apl_val() == old_value)   // found
          {
            new (&tok) Token(tok.get_tag(), new_value);
            return true;
          }
     }
   return false;
}
//-----------------------------------------------------------------------------
Token * Prefix::locate_L()
{
   // expect at least A f B (so we have at0(), at1() and at2()

   if (prefix_len < 3)   return 0;

   if (at1().get_Class() != TC_FUN12 &&
       at1().get_Class() != TC_OPER1 &&
       at1().get_Class() != TC_OPER2)   return 0;

   if (at0().get_Class() == TC_VALUE)   return &at0();
   return 0;
}
//-----------------------------------------------------------------------------
Value_P *
Prefix::locate_X()
{
   // expect at least B X (so we have at0() and at1() and at2()

   if (prefix_len < 2)   return 0;

   // either at0() (for monadic f X B) or at1() (for dyadic A f X B) must
   // be a function or operator
   //
   for (int x = put - 1; x >= put - prefix_len; --x)
       {
         if (content[x].tok.get_ValueType() == TV_FUN)
            {
              Function * fun = content[x].tok.get_function();
              if (fun)
                 {
                   Value_P * X = fun->locate_X();
                   if (X)   return  X;   // only for derived function
                 }
            }
         else if (content[x].tok.get_Class() == TC_INDEX)   // maybe found X ?
            {
              return content[x].tok.get_apl_valp();
            }
       }

   return 0;
}
//-----------------------------------------------------------------------------
Token * Prefix::locate_R()
{
   // expect at least f B (so we have at0(), at1() and at2()

   if (prefix_len < 2)   return 0;

   // either at0() (for monadic f B) or at1() (for dyadic A f B) must
   // be a function or operator
   //
   if (at0().get_Class() != TC_FUN12 &&
       at0().get_Class() != TC_OPER1 &&
       at0().get_Class() != TC_OPER2 &&
       at1().get_Class() != TC_FUN12 &&
       at1().get_Class() != TC_OPER1 &&
       at1().get_Class() != TC_OPER2)   return 0;

Token * ret = &content[put - prefix_len].tok;
   if (ret->get_Class() == TC_VALUE)   return ret;
   return 0;
}
//-----------------------------------------------------------------------------
void
Prefix::print(ostream & out, int indent) const
{
   loop(i, indent)   out << "    ";
   out << "Token: ";
   loop(s, size())   out << " " << at(s).tok;
   out << endl;
}
//=============================================================================
//
// phrase reduce functions...
//
//-----------------------------------------------------------------------------
void
Prefix::reduce____()
{
   // this function is a placeholder for invalid phrases and should never be
   // called.
   //
   FIXME;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_LPAR_B_RPAR_()
{
   Assert1(prefix_len == 3);

Token result = at1();   // B or F
   if (result.get_tag() == TOK_APL_VALUE3)   result.ChangeTag(TOK_APL_VALUE1);

   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_LPAR_F_C_RPAR()
{
   Assert1(prefix_len == 4);

   // C should be an axis and not a [;;] index
   //
   if (at2().get_ValueType() != TV_VAL)   SYNTAX_ERROR;
   if (!at2().get_apl_val())              SYNTAX_ERROR;

   //     at: 0 1 2 3
   // before: ( F C )
   // after:  F C
   //
   at3().move_1(at2(), LOC);
   at2().move_1(at1(), LOC);
   pop_and_discard();    // pop old RPAR
   pop_and_discard();    // pop old C
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_N___()
{
   Assert1(prefix_len == 1);

Token result = at0().get_function()->eval_();
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        action = RA_RETURN;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_MISC_F_B_()
{
   Assert1(prefix_len == 2);

   if (saved_lookahead.tok.get_Class() == TC_INDEX)
      {
        if (value_expected())
           {
             // push [...] and read one more token
             //
             push(saved_lookahead);
             saved_lookahead.tok.clear(LOC);
             action = RA_PUSH_NEXT;
             return;
           }
      }

Token result = at0().get_function()->eval_B(at1().get_apl_val());
   if (result.get_Class() == TC_SI_LEAVE)
      {
        if (result.get_tag() == TOK_SI_PUSHED)   goto done;

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

        if (result.get_tag() == TOK_QUAD_ES_COM)
           {
             // make sure that ⎕ES was called from a macro (implies parent)
             //
             if (Workspace::SI_top()->function_name()[0] != UNI_MUE)
                DOMAIN_ERROR;

             Workspace::pop_SI(LOC);   // discard ⎕EA/⎕EB context

             const Cell & QES_arg2 = result.get_apl_val()->get_ravel(2);
             Token & si_pushed = Workspace::SI_top()->get_prefix().at0();
             Assert(si_pushed.get_tag() == TOK_SI_PUSHED);
             if (QES_arg2.is_pointer_cell())
                {
                  Value_P val = QES_arg2.get_pointer_value();
                  new (&si_pushed)  Token(TOK_APL_VALUE2, val);
                }
             else
                {
                  Value_P scalar(LOC);
                  scalar->next_ravel()->init(QES_arg2, scalar.getref(),LOC);
                  scalar->check_value(LOC);
                  new (&si_pushed)  Token(TOK_APL_VALUE2, scalar);
                }
             return;
           }

        if (result.get_tag() == TOK_QUAD_ES_ESC)
           {
             // make sure that ⎕ES was called from a macro (implies parent)
             //
             if (Workspace::SI_top()->function_name()[0] != UNI_MUE)
                DOMAIN_ERROR;

             Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context

             Token & si_pushed = Workspace::SI_top()->get_prefix().at0();
             Assert(si_pushed.get_tag() == TOK_SI_PUSHED);
             new (&si_pushed)  Token(TOK_ESCAPE);
             return;
           }

        if (result.get_tag() == TOK_QUAD_ES_BRA)
           {
             // make sure that ⎕ES was called from a macro (implies parent)
             //
             if (Workspace::SI_top()->function_name()[0] != UNI_MUE)
                DOMAIN_ERROR;

             Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context

             const Cell & QES_arg2 = result.get_apl_val()->get_ravel(2);
             const APL_Integer line = QES_arg2.get_int_value();

             Token & si_pushed = Workspace::SI_top()->get_prefix().at0();
             Assert(si_pushed.get_tag() == TOK_SI_PUSHED);

             Workspace::SI_top()->jump(IntScalar(line, LOC));
             return;
           }

        if (result.get_tag() == TOK_QUAD_ES_ERR)
           {
             // this case can only occur with ⎕EA, but not with ⎕EB.

             // make sure that ⎕ES was called from a macro (implies parent)
             //
             if (Workspace::SI_top()->function_name()[0] != UNI_MUE)
                DOMAIN_ERROR;

             Workspace::pop_SI(LOC);   // discard the ⎕EA/⎕EB context
             StateIndicator * top = Workspace::SI_top();

             Token & si_pushed = top->get_prefix().at0();
             Assert(si_pushed.get_tag() == TOK_SI_PUSHED);

             const Cell * QES_arg = &result.get_apl_val()->get_ravel(0);
             UCS_string statement_A(   QES_arg[2].get_pointer_value().getref());
             const APL_Integer major = QES_arg[3].get_int_value();
             const APL_Integer minor = QES_arg[4].get_int_value();
             const ErrorCode ec = ErrorCode(major << 16 | minor);

             Token result_A = Bif_F1_EXECUTE::execute_statement(statement_A);
             if (result_A.get_Class() == TC_VALUE)   // ⍎ literal
                {
                  Workspace::SI_top()->get_prefix().at0().move_1(result_A, LOC);
                  return;
                }
             new (&StateIndicator::get_error(top)) Error(ec, LOC);
             return;
           }

        // at this point a normal monadic function (i.e. other than ⎕EA/⎕EB)
        // has returned an error
        //
        if (result.get_tag() == TOK_ERROR)
           {
             Token_loc tl(result, get_range_low());
             push(tl);
             action = RA_RETURN;
             return;
           }

        // not reached
        Q1(result.get_tag())
        FIXME;
      }

done:
   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_MISC_F_C_B()
{
   Assert1(prefix_len == 3);

   if (saved_lookahead.tok.get_Class() == TC_INDEX)
      {
        if (value_expected())
           {
             // push [...] and read one more token
             //
             push(saved_lookahead);
             saved_lookahead.tok.clear(LOC);
             action = RA_PUSH_NEXT;
             return;
           }
      }

   if (at1().get_ValueType() != TV_VAL)   // [i1;i2...] instead of [axis]
      {
        IndexExpr * idx = &at1().get_index_val();
        Log(LOG_delete)   CERR << "delete " << voidP(idx) << " at " LOC << endl;
        delete idx;
         at1().clear(LOC);
         SYNTAX_ERROR;
      }
   if (!at1().get_apl_val())              SYNTAX_ERROR;

   if (at0().get_tag() == TOK_Quad_FIO &&
       saved_lookahead.tok.get_Class() == TC_FUN12)
      {
        DerivedFunction * derived =
                          Workspace::SI_top()->fun_oper_cache.get(LOC);
        new (derived)   DerivedFunction(saved_lookahead.tok,
                                        at0().get_function(),
                                        at1().get_apl_val(),  LOC);
        saved_lookahead.tok.clear(LOC);
        prefix_len = 2;   // only f ⎕FIO
        pop_args_push_result(Token(TOK_FUN2, derived));
        action = RA_CONTINUE;
        return;
      }

Token result = at0().get_function()->eval_XB(at1().get_apl_val(),
                                             at2().get_apl_val());
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        action = RA_RETURN;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_F_B_()
{
   Assert1(prefix_len == 3);

Token result = at1().get_function()->eval_AB(at0().get_apl_val(),
                                             at2().get_apl_val());
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        action = RA_RETURN;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_M_B_()
{
const TokenTag tag = at1().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_A_F_B_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_F_C_B()
{
   Assert1(prefix_len == 4);

   if (at2().get_ValueType() != TV_VAL)   SYNTAX_ERROR;
   if (!at2().get_apl_val())              SYNTAX_ERROR;

Token result = at1().get_function()->eval_AXB(at0().get_apl_val(),
                                              at2().get_apl_val(),
                                              at3().get_apl_val());
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        action = RA_RETURN;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_M_C_B()
{
const TokenTag tag = at1().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_A_F_C_B();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_M__()
{
   Assert1(prefix_len == 2);

DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_M__()
{
   if (is_fun_or_oper(PC))
      {
         action = RA_PUSH_NEXT;
        return;
      }

const TokenTag tag = at0().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_M__();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_M_C_()
{
   Assert1(prefix_len == 3);

   if (at2().get_tag() != TOK_AXES)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXES was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(),
                                 at1().get_function(),
                                 at2().get_axes(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_M_C_()
{
   if (is_fun_or_oper(PC))
      {
         action = RA_PUSH_NEXT;
        return;
      }

const TokenTag tag = at0().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_M_C_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_C_M_()
{
   Assert1(prefix_len == 3);

   if (at1().get_tag() != TOK_AXES)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXES was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

DerivedFunction * F_C =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (F_C) DerivedFunction(at0().get_function(),
                             at1().get_axes(), LOC);

Token tok_F_C(TOK_FUN2, F_C);
DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(tok_F_C, at2().get_function(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_C_M_()
{
   if (is_fun_or_oper(PC))
      {
         action = RA_PUSH_NEXT;
        return;
      }

const TokenTag tag = at0().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_C_M_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_C_M_C()
{
   Assert1(prefix_len == 4);

   if (at1().get_tag() != TOK_AXES)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXES was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

   if (at3().get_tag() != TOK_AXES)   // e.g. M[;2] instead of M[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXES was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

DerivedFunction * F_C =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (F_C) DerivedFunction(at0().get_function(),
                             at1().get_axes(), LOC);

Token tok_F_C(TOK_FUN2, F_C);
DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(tok_F_C, at2().get_function(),
                                          at3().get_axes(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_C_M_C()
{
   if (is_fun_or_oper(PC))
      {
         action = RA_PUSH_NEXT;
        return;
      }

const TokenTag tag = at0().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_C_M_C();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_D_B_()
{
   // same as G2, except for ⍤
   //
   if (at1().get_function()->get_Id() != ID_OPER2_RANK)
      {
         reduce_F_D_G_();
         return;
      }

   // we have f ⍤ y_B with y_B glued beforehand. Unglue it.
   //
Value_P y123;
Value_P B;
   Bif_OPER2_RANK::split_y123_B(at2().get_apl_val(), y123, B);
Token new_y123(TOK_APL_VALUE1, y123);

DerivedFunction * derived = Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), new_y123, LOC);

Token result = Token(TOK_FUN2, derived);

   if (!B)   // only y123, no B (e.g. (f ⍤[X] 1 2 3)
      {
        pop_args_push_result(result);
      }
   else      // a new B split off from the original B
      {
        // save locations of ⍤ and B
        //
        Function_PC pc_D = at(1).pc;
        Function_PC pc_B = at(2).pc;

        pop_and_discard();   // pop F
        pop_and_discard();   // pop C
        pop_and_discard();   // pop B (old)

        Token new_B(TOK_APL_VALUE1, B);
        Token_loc tl_B(new_B, pc_B);
        Token_loc tl_derived(result, pc_D);
        push(tl_B);
        push(tl_derived);
      }

   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_D_G_()
{
DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), at2(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_D_M_()
{
const TokenTag tag = at2().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_D_G_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_D_G_()
{
const TokenTag tag = at0().get_tag();
   if (tag == TOK_OPER1_REDUCE  || tag == TOK_OPER1_SCAN ||
       tag == TOK_OPER1_REDUCE1 || tag == TOK_OPER1_SCAN1)
      return reduce_F_D_G_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_M_D_M_()
{
const TokenTag tag0 = at0().get_tag();
const TokenTag tag2 = at2().get_tag();
   if ((tag0 == TOK_OPER1_REDUCE  || tag0 == TOK_OPER1_SCAN ||
        tag0 == TOK_OPER1_REDUCE1 || tag0 == TOK_OPER1_SCAN1) && 
       (tag2 == TOK_OPER1_REDUCE  || tag2 == TOK_OPER1_SCAN ||
        tag2 == TOK_OPER1_REDUCE1 || tag2 == TOK_OPER1_SCAN1))
      return reduce_F_D_G_();

   syntax_error(LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_D_C_B()
{
   // reduce, except if another dyadic operator is coming. In that case
   // F belongs to the other operator and we simply continue.
   //
   if (PC < Function_PC(body.size()))
        {
          const Token & tok = body[PC];
          TokenClass next =  tok.get_Class();
          if (next == TC_SYMBOL)
             {
               Symbol * sym = tok.get_sym_ptr();
               const bool left_sym = get_assign_state() == ASS_arrow_seen;
               next = sym->resolve_class(left_sym);
             }

          if (next == TC_OPER2)
             {
               action = RA_PUSH_NEXT;
               return;
             }
        }
   // we have f ⍤ [X] y_B with y_B glued beforehand. Unglue it.
   //
Value_P y123;
Value_P B;
   Bif_OPER2_RANK::split_y123_B(at3().get_apl_val(), y123, B);
Token new_y123(TOK_APL_VALUE1, y123);

   if (at2().get_tag() != TOK_AXES)   // e.g. D[;2] instead of D[;2]
      {
        // the user has provided a TOK_INDEX where TOK_AXES was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

Value_P v_idx = at2().get_axes();
DerivedFunction * derived = Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(),
                                 v_idx, new_y123, LOC);

Token result = Token(TOK_FUN2, derived);

   if (!B)   // only y123, no B (e.g. (f ⍤[X] 1 2 3)
      {
        pop_args_push_result(result);
      }
   else      // a new B split off from the original B
      {
        // save locations of ⍤ and B
        //
        Function_PC pc_D = at(1).pc;
        Function_PC pc_B = at(3).pc;

        pop_and_discard();   // pop F
        pop_and_discard();   // pop D
        pop_and_discard();   // pop C
        pop_and_discard();   // pop B (old)

        Token new_B(TOK_APL_VALUE1, B);
        Token_loc tl_B(new_B, pc_B);
        Token_loc tl_derived(result, pc_D);
        push(tl_B);   
        push(tl_derived);   
      }

   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_C__()
{
Value_P A = at0().get_apl_val();
Value_P Z;

   if (at1().get_tag() == TOK_AXES)
      {
        Z = A->index(at1().get_apl_val());
      }
   else
      {
        IndexExpr * idx =  &at1().get_index_val();
        try
           {
             Z = A->index(*idx);
             Log(LOG_delete)
                CERR << "delete " << voidP(idx) << " at " LOC << endl;
             delete idx;
           }
        catch (Error err)
           {
             Token result = Token(TOK_ERROR, err.get_error_code());
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
             pop_args_push_result(result);
             set_action(result);
             return;
           }
      }

Token result = Token(TOK_APL_VALUE1, Z);
   pop_args_push_result(result);

   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_V_C__()
{
Symbol * V = at0().get_sym_ptr();
Token tok = V->resolve_lv(LOC);
   at0().move_1(tok, LOC);
   set_assign_state(ASS_var_seen);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_V_C_ASS_B()
{
Symbol * V = at0().get_sym_ptr();
Value_P B = at3().get_apl_val();

   if (at1().get_tag() == TOK_AXES)   // [] or [x]
      {
        Value_P v_idx = at1().get_axes();

        try
           {
             V->assign_indexed(v_idx, B);
           }
        catch (Error err)
           {
             Token result = Token(TOK_ERROR, err.get_error_code());
             at1().clear(LOC);
             at3().clear(LOC);
             pop_args_push_result(result);
             set_assign_state(ASS_none);
             set_action(result);
             return;
           }
      }
   else                               // [a;...]
      {
        IndexExpr * idx = &at1().get_index_val();
        try
           {
             V->assign_indexed(*idx, B);
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
           }
        catch (Error err)
           {
             Token result = Token(TOK_ERROR, err.get_error_code());
             at1().clear(LOC);
             at3().clear(LOC);
             Log(LOG_delete)   CERR << "delete " << voidP(idx)
                                    << " at " LOC << endl;
             delete idx;
             pop_args_push_result(result);
             set_assign_state(ASS_none);
             set_action(result);
             return;
           }
      }

Token result = Token(TOK_APL_VALUE2, B);
   pop_args_push_result(result);
   set_assign_state(ASS_none);
   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_F_V__()
{
   // turn V into a (left-) value
   //
Symbol * V = at1().get_sym_ptr();
Token tok = V->resolve_lv(LOC);
   at1().move_1(tok, LOC);
   set_assign_state(ASS_var_seen);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_ASS_B_()
{
Value_P A = at0().get_apl_val();
Value_P B = at2().get_apl_val();

   A->assign_cellrefs(B);

Token result = Token(TOK_APL_VALUE2, B);
   pop_args_push_result(result);

   set_assign_state(ASS_none);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_V_ASS_B_()
{
Value_P B = at2().get_apl_val();

   Assert1(B->get_owner_count() >= 2);   // owners are at least B and at2()
const bool clone = B->get_owner_count() != 2 || at1().get_tag() != TOK_ASSIGN1;
Symbol * V = at0().get_sym_ptr();
   pop_and_discard();   // V
   pop_and_discard();   // ←

   at0().ChangeTag(TOK_APL_VALUE2);   // change value to committed value

   set_assign_state(ASS_none);
   action = RA_CONTINUE;

   V->assign(B, clone, LOC);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_V_ASS_F_()
{
   // named lambda: V ← { ... }
   //
Function * F = at2().get_function();
   if (!F->is_lambda())   SYNTAX_ERROR;

Symbol * V = at0().get_sym_ptr();
   if (V->assign_named_lambda(F, LOC))   DEFN_ERROR;

Value_P Z(V->get_name(), LOC);
Token result = Token(TOK_APL_VALUE2, Z);
   pop_args_push_result(result);

   set_assign_state(ASS_none);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
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

   new (&at0()) Token(TOK_PINDEX, *idx);
   set_assign_state(ASS_none);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_LBRA_I__()
{
   // [ I or ; I   (elided index)
   //
   Assert1(prefix_len == 2);

IndexExpr & idx = at1().get_index_val();
const bool last_index = (at0().get_tag() == TOK_L_BRACK);

   if (idx.value_count() == 0 && last_index)   // special case: [ ]
      {
        assign_state = idx.get_assign_state();
        Token result = Token(TOK_INDEX, idx);
        pop_args_push_result(result);
        action = RA_CONTINUE;
        Log(LOG_delete)   CERR << "delete " << voidP(&idx)
                               << " at " LOC << endl;
        delete &idx;
        return;
      }

   // add elided index to partial index list
   //
Token result = at1();
   result.get_index_val().add(Value_P());

   if (last_index)   // [ seen
      {
        assign_state = idx.get_assign_state();

        if (idx.is_axis()) result.move_2(Token(TOK_AXES, idx.values[0]), LOC);
        else               result.move_2(Token(TOK_INDEX, idx), LOC);
      }
   else
      {
        set_assign_state(ASS_none);
      }

   pop_args_push_result(result);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_LBRA_B_I_()
{
   Assert1(prefix_len == 3);

   // [ B I or ; B I   (normal index)
   //
Token I = at2();
   I.get_index_val().add(at1().get_apl_val());

const bool last_index = (at0().get_tag() == TOK_L_BRACK);   // ; vs. [

   if (last_index)   // [ seen
      {
        IndexExpr & idx = I.get_index_val();
        assign_state = idx.get_assign_state();

        if (idx.is_axis())
           {
             Value_P X = idx.extract_value(0);
             Assert1(!!X);
             I.move_2(Token(TOK_AXES, X), LOC);
             Log(LOG_delete)
                CERR << "delete " << voidP(&idx) << " at " LOC << endl;
             delete &idx;
           }
        else
           {
             I.move_2(Token(TOK_INDEX, idx), LOC);
           }
      }
   else
      {
         set_assign_state(ASS_none);
      }

   pop_args_push_result(I);
   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_A_B__()
{
   Assert1(prefix_len == 2);

Token result;
   Value::glue(result, at0(), at1(), LOC);
   pop_args_push_result(result);

   set_action(result);
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_V_RPAR_ASS_B()
{
   Assert1(prefix_len == 4);

const int count = vector_ass_count();

   // vector assignment also matches selective specification, but count
   // distinguishes them, e.g.
   //
   // (T U V) ← value        count = 2	    vector assignment
   //   (U V) ← value        count = 1	    vector assignment
   // (2 ↑ V) ← value        count = 0	    selective specification
   //
   if (count < 1)
      {
        // selective specification. Convert variable V into a (left-) value
        //
        Symbol * V = at0().get_sym_ptr();
        Token result = V->resolve_lv(LOC);
        set_assign_state(ASS_var_seen);
        at0().move_1(result, LOC);
        action = RA_CONTINUE;
        return;
      }

   // vector assignment.
   //
std::vector<Symbol *> symbols;
   symbols.push_back(at0().get_sym_ptr());
   loop(c, count)
      {
        Token_loc tl = lookahead();
        Assert1(tl.tok.get_tag() == TOK_LSYMB2);   // by vector_ass_count()
        Symbol * V = tl.tok.get_sym_ptr();
        Assert(V);
        symbols.push_back(V);
      }

Value_P B = at3().get_apl_val();
   Symbol::vector_assignment(symbols, B);

   set_assign_state(ASS_none);

   // clean up stack
   //
Token result(TOK_APL_VALUE2, at3().get_apl_val());
   pop_args_push_result(result);
Token_loc tl = lookahead();
   if (tl.tok.get_Class() != TC_L_PARENT)   syntax_error(LOC);

   action = RA_CONTINUE;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_END_VOID__()
{
   Assert1(prefix_len == 2);

   if (size() != 2)   syntax_error(LOC);

const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = (at0().get_int_val() & 1) != 0;

   pop_and_discard();   // pop END
   pop_and_discard();   // pop VOID

Token Void(TOK_VOID);
   si.statement_result(Void, trace);
   action = RA_PUSH_NEXT;
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_END_B__()
{
   Assert1(prefix_len == 2);

   if (size() != 2)   syntax_error(LOC);

const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = (at0().get_int_val() & 1) != 0;

   pop_and_discard();   // pop END
Token B = pop().tok;    // pop B
   si.fun_oper_cache.reset();
   si.statement_result(B, trace);

   action = RA_PUSH_NEXT;
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_END_GOTO_B_()
{
   Assert1(prefix_len == 3);

   if (size() != 3)   syntax_error(LOC);

   si.fun_oper_cache.reset();

   // at0() is either TOK_END or TOK_ENDL.
   //
const bool end_of_line = at0().get_tag() == TOK_ENDL;
const bool trace = at0().get_Class() == TC_END &&
                  (at0().get_int_val() & 1) != 0;

Value_P line = at2().get_apl_val();
   if (trace && line->element_count() > 0)
      {
        const int64_t line_num = line->get_line_number();
        Token bra(TOK_BRANCH, line_num);
        si.statement_result(bra, true);
      }

const Token result = si.jump(line);

   if (result.get_tag() == TOK_BRANCH)   // branch back into a function
      {
        Log(LOG_prefix_parser)
           {
             CERR << "Leaving context after " << result << endl;
           }

        pop_args_push_result(result);
        action = RA_RETURN;
        return;
      }

   // StateIndicator::jump may have called set_PC() which resets the prefix.
   // we do not call pop_args_push_result(result) (which would fail due
   // to the now incorrect prefix_len), but discard the entire statement.
   //
   reset(LOC);

   if (result.get_tag() == TOK_NOBRANCH)   // branch not taken, e.g. →⍬
      {
        Token bra(TOK_NOBRANCH);
        si.statement_result(bra, trace);

        action = RA_PUSH_NEXT;
        if (attention_is_raised() && end_of_line)
           {
             const bool int_raised = interrupt_is_raised();
             clear_attention_raised(LOC);
             clear_interrupt_raised(LOC);
             if (int_raised)   INTERRUPT
             else              ATTENTION
           }

        return;
      }

   if (result.get_tag() == TOK_VOID)   // branch taken, e.g. →N
      {
        action = RA_PUSH_NEXT;
        if (attention_is_raised() && end_of_line)
           {
             const bool int_raised = interrupt_is_raised();
             clear_attention_raised(LOC);
             clear_interrupt_raised(LOC);
             if (int_raised)   INTERRUPT
             else              ATTENTION
           }

        return;
      }

   // branch within function
   //
const Function_PC new_pc = si.get_PC();
   Log(LOG_prefix_parser)
      {
        CERR << "Staying in context after →PC(" << new_pc << ")" << endl;
        print_stack(CERR, LOC);
      }

   Assert1(size() == 0);
   action = RA_PUSH_NEXT;
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_END_GOTO__()
{
   Assert1(prefix_len == 2);

   if (size() != 2)   syntax_error(LOC);

   si.fun_oper_cache.reset();

const bool trace = at0().get_Class() == TC_END &&
                  (at0().get_int_val() & 1) != 0;
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
        const UserFunction * ufun = si.get_executable()->get_ufun();
        if (ufun && ufun->get_exec_properties()[2])
           {
              // the function ignores attention (aka. weak interrupt)
              //
              pop_and_discard();   // pop END
              pop_and_discard();   // pop GOTO
              action = RA_CONTINUE;
              return;
           }

        COUT << si.function_name() << "[" << si.get_line() << "]" << endl;
        Token result(TOK_ERROR, E_STOP_LINE);
        pop_args_push_result(result);
        action = RA_RETURN;
      }
   else
      {
        Token result(TOK_ESCAPE);
        pop_args_push_result(result);
        action = RA_RETURN;
      }
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_RETC_VOID__()
{
   Assert1(prefix_len == 2);

Token result = Token(TOK_VOID);
   pop_args_push_result(result);
   action = RA_RETURN;
}
//-----------------------------------------------------------------------------
void
Prefix::reduce_RETC___()
{
   Assert1(prefix_len == 1);

   if (size() != 1)   syntax_error(LOC);

   // end of context reached. There are 4 cases:
   //
   // TOK_RETURN_STATS:  end of ◊ context
   // TOK_RETURN_EXEC:   end of ⍎ context with no result (e.g. ⍎'')
   // TOK_RETURN_VOID:   end of ∇ (no result)
   // TOK_RETURN_SYMBOL: end of ∇ (result in Z)
   //
   // case TOK_RETURN_EXEC (end of ⍎ context) is handled in reduce_RETC_B___()
   //
   switch(at0().get_tag())
      {
        case TOK_RETURN_EXEC:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ⍎ context (no result)" << endl;
             at0().clear(LOC);
             action = RA_RETURN;
             return;

        case TOK_RETURN_STATS:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ◊ context" << endl;
             at0().clear(LOC);
             action = RA_RETURN;
             return;

        case TOK_RETURN_VOID:   // user-defined function not returning a value
             Log(LOG_prefix_parser)
                CERR << "- end of ∇ context (function has no result)" << endl;

             {
               const UserFunction * ufun = si.get_executable()->get_ufun();
               if (ufun)   { /* do nothing, needed for -Wall */ }
               Assert1(ufun);
               at0().clear(LOC);
             }
             action = RA_RETURN;
             return;

        case TOK_RETURN_SYMBOL:   // user-defined function returning a value
             {
               const UserFunction * ufun = si.get_executable()->get_ufun();
               Assert1(ufun);
               Symbol * ufun_Z = ufun->get_sym_Z();
               Value_P Z;
               if (ufun_Z)   Z = ufun_Z->get_value();
               if (!!Z)
                  {
                    Log(LOG_prefix_parser)
                       CERR << "- end of ∇ context (function result is: "
                            << *Z << ")" << endl;
                    new (&at0()) Token(TOK_APL_VALUE1, Z);
                  }
               else
                  {
                    Log(LOG_prefix_parser)
                       CERR << "- end of ∇ context (MISSING function result)."
                            << endl;
                    at0().clear(LOC);
                  }
             }
             action = RA_RETURN;
             return;

        default: break;
      }

   // not reached
   //
   Q1(at0().get_tag())   FIXME;
}
//-----------------------------------------------------------------------------
// Note: reduce_RETC_B___ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_B___ instead.
//
void
Prefix::reduce_RETC_B__()
{
   Assert1(prefix_len == 2);

   if (size() != 2)
      {
        syntax_error(LOC);
      }

   Log(LOG_prefix_parser)
      CERR << "- end of ⍎ context.";

Token B = at1();
   pop_args_push_result(B);

   action = RA_RETURN;
}
//-----------------------------------------------------------------------------
// Note: reduce_RETC_GOTO_B__ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_GOTO_B__ instead.
//
void
Prefix::reduce_RETC_GOTO_B_()
{
   if (size() != 3)   syntax_error(LOC);

   reduce_END_GOTO_B_();

   if (action == RA_PUSH_NEXT)
      {
        // reduce_END_GOTO_B_() has detected a non-taken branch (e.g. →'')
        // and wants to continue with the next statement.
        // We are in ⍎ mode, so there is no next statement and we
        // RA_RETURN a TOK_VOID instead of RA_PUSH_NEXT.
        //
        Token tok(TOK_VOID);
        Token_loc tl(tok, Function_PC_0);
        push(tl);
      }

   action = RA_RETURN;
}
//-----------------------------------------------------------------------------
// Note: reduce_RETC_ESC___ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_ESC___ instead.
//
void
Prefix::reduce_RETC_GOTO__()
{
   if (size() != 2)   syntax_error(LOC);

   reduce_END_GOTO__();
}
//-----------------------------------------------------------------------------

