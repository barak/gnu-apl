/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2023  Dr. Jürgen Sauermann

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

/// prefix search mode (hash table vs. search tree
// #define PREFIX_HASH

uint64_t Prefix::instance_counter = 0;

//----------------------------------------------------------------------------
Prefix::Prefix(StateIndicator & _si, const Token_string & _body)
   : instance(++instance_counter),
     si(_si),
     put(0),
     saved_MISC(Token(TOK_VOID), Function_PC_invalid),
     body(_body),
     PC(Function_PC_0),
     assign_state(ASS_none),
     PC_range_high(Function_PC_invalid),
     action(RA_FIXME)
{
}
//----------------------------------------------------------------------------
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
             tok.get_index_val().~IndexExpr();
           }
      }

   put = 0;
}
//----------------------------------------------------------------------------
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
        else if (tok.get_Class() == TC_FUN2)
          {
            Function_P fun = tok.get_function();
            if (fun && fun->is_derived())
               {
                 Function * fp = const_cast<Function *>(fun);
                 reinterpret_cast<DerivedFunction *>(fp)->destroy_derived(LOC);
               }
          }
      }

   // see if error was caused by a function not returning a value.
   // In that case we throw a value error instead of a syntax error.
   //
   loop (s, size())
      {
        if (at(s).tok.get_Class() == TC_VOID)
           {
             MORE_ERROR() << "No assignment to result variable Z"
                             " in e.g. ∇Z←... ?";
             throw_apl_error(E_VALUE_ERROR, loc);
           }
      }

   throw_apl_error(get_assign_state() == ASS_none ? E_SYNTAX_ERROR
                                                  : E_LEFT_SYNTAX_ERROR, loc);
}
//----------------------------------------------------------------------------
bool
Prefix::uses_function(const UserFunction * ufun) const
{
   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() == TV_FUN &&
            tok.get_function() == ufun)   return true;
      }

   if (saved_MISC.tok.get_ValueType() == TV_FUN &&
            saved_MISC.tok.get_function() == ufun)   return true;

   return false;
}
//----------------------------------------------------------------------------
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

   switch(size())
      {
        case 0:  return false;
        case 1:  return false;
        case 2:  if (at1().get_Class() == TC_INDEX)   return true;   // B[X]
                 // fall through
        default: if (at(size() - 1).tok.get_Class() == TC_VALUE)   /// ... ⎕R
                    return true;
      }

   return false;
}
//----------------------------------------------------------------------------
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
        const NameClass nc = sym->get_NC();

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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
int
Prefix::vector_ass_count() const
{
int count = 0;

   // TOKEN ... TOKEN VAR ) ←   (reversed)
   //             ↑
   //             PC
   //
   // return the number of TOKEN (not including VAR)
   //
   for (Function_PC pc = PC; pc < Function_PC(body.size()); ++pc)
       {
         if (body[pc].get_tag() != TOK_LSYMB2)   break;
         ++count;
       }

   return count;
}
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
int
Prefix::show_owners(const char * prefix, ostream & out,
                          const Value & value) const
{
int count = 0;

   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() != TV_VAL)      continue;

        if (Value::is_or_contains(tok.get_apl_val().get(), &value))
           {
             out << prefix << " Fifo [" << s << "]" << endl;
             ++count;
           }

      }

   return count;
}
//----------------------------------------------------------------------------
Function_PC
Prefix::get_range_high() const
{
   // if the stack is empty then return the last address (if any) or otherwise
   // the address of the next token.
   //
   if (size() == 0)     // stack is empty
      {
        if (PC_range_high == Function_PC_invalid)   return PC;
        return PC_range_high;
      }

   // stack non-empty: return address of highest item or the last address
   //
Function_PC high = PC_range_high;
   if (high == Function_PC_invalid)   high = at(0).pc;

   if (best_phrase && best_phrase->misc)   --high;
   return high;
}
//----------------------------------------------------------------------------
Function_PC
Prefix::get_range_low() const
{
   // if the stack is not empty then return the PC of the lowest element
   //
   if (size() > 0)     return at(size() - 1).pc;


   // the stack is empty. Return the last address (if any) or otherwise
   // the address of the next token.
   //
   if (PC_range_high == Function_PC_invalid)   return PC;   // no last address
   return PC_range_high;
}
//----------------------------------------------------------------------------
bool
Prefix::value_expected()
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
   Assert1(saved_MISC.tok.get_Class() == TC_INDEX);
   Assert1(saved_MISC.pc == (PC - 1));

   // function axes cannot contain semicolons. Therefore, if saved_MISC
   // contains semicolons then its get_ValueType() is TV_INDEX and
   // saved_MISC  MUST be the index of a value. The converse is not
   // true: a get_ValueType() of TC_VALUE only indicates the lack of semicolons,
   // which is valid for both functions and values.
   //
   if (saved_MISC.tok.get_ValueType() == TV_INDEX)   return true;   // value

   // look ahead further until value index vs. function axis can be decided.
   //
   for (size_t pc = PC; pc < body.size();)
      {
        const Token & tok = body[pc++];
        switch(tok.get_Class())
           {
               case TC_R_BRACK:   // skip over [...] (func axis or value index)
                    //
                    pc += tok.get_int_val2();
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
//----------------------------------------------------------------------------
void
Prefix::unmark_all_values() const
{
   loop (s, size())
      {
        const Token & tok = at(s).tok;
        if (tok.get_ValueType() != TV_VAL)      continue;

        Value_P value = tok.get_apl_val();
        if (+value)   value->unmark();
      }
}
//----------------------------------------------------------------------------
bool
Prefix::push_Symbol(Token_loc & tl)
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
        // tl.tok is the last token C of a vector assignment // (A B ... C)←.
        // We return C and let the caller do the rest
        //
        sym->resolve_left(tl.tok);
        Log(LOG_prefix_parser)
           CERR << "TOK_LSYMB2 " << sym->get_name()
                << "resolved to " << tl.tok
                << " at " << LOC  << endl;
      }
   else   // not TOK_LSYMB2
      {
        const NameClass nc = sym->get_NC();
        if (size_t(PC + 1) < body.size() &&
            body[PC + 1].get_tag() == TOK_OPER2_INNER)
           {
             // APL code is .SYM which could be a normal inner product
             // product f.SYM or a value member Val.g . We check the
             // name class to decide.
             //
             if (nc != NC_FUNCTION &&
                 nc != NC_OPERATOR)   // not inner product
                {
                  PC = PC_range_high + 1;   // resolved: restore PC
                  push(tl);
                  return false;   // )SI not pushed
                }
           }

        const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
        if (is_left_sym)
          {
            // allow assignment only to variables or undefined names.
            // There is a common, but hard to understand pitfall: a label with
            // the same name exists. We should then provide some more info.
            //
            if (!(nc & NC_left))
               {
                 UCS_string & more = MORE_ERROR();
                 more << "Assignment to symbol " << sym->get_name()
                      << " which (currently) is ";
                 switch(nc)
                    {
                       case NC_LABEL:
                            more << "a label";              break;
                       case NC_OPERATOR:
                            more << "a defined operator";   break;
                       case NC_FUNCTION:
                            more << "a defined function";   break;
                       case NC_SYSTEM_FUN:
                            more << "a system function";   break;

                       default: FIXME;
                    }
                 syntax_error(LOC);
               }
          }

        bool resolved = false;
        if (size() > 0 && at(0).tok.get_Class() == TC_INDEX &&
            tl.tok.get_tag() == TOK_SYMBOL)   // user defined variable
           {
             // indexed reference, e.g. A[N]. Calling sym->resolve()
             // would copy the entire variable and then index it, which
             // is inefficient if the variable is big. We rather call
             // Symbol::get_var_value() directly in order to avoid that
             //
             Value_P val = sym->get_var_value();
             if (+val && !is_left_sym)
                {
                  Token tok(TOK_APL_VALUE1, val);
                  tl.tok.move(tok, LOC);
                  resolved = true;
                }
           }
        if (!resolved)
           {
             if (is_left_sym)   sym->resolve_left(tl.tok);
             else               sym->resolve_right(tl.tok);
           }

        if (is_left_sym)   set_assign_state(ASS_var_seen);
        Log(LOG_prefix_parser)
           {
             const char * what = is_left_sym ? "TOK_LSYMB"
                                             : "TOK_SYMBOL";
             CERR << what << " resolved to " << tl.tok
                  << " at " << LOC  << endl;
           }
      }

   PC = PC_range_high + 1;   // resolve() succeeded: restore PC

   Log(LOG_prefix_parser)
      {
        CERR << "   resolved symbol " << sym->get_name()
             << " to " << tl.tok.get_Class() << endl;
      }

   // Quad_Quad::resolve() calls ⍎ which may return TOK_SI_PUSHED.
   //
   push(tl);
   return (tl.tok.get_tag() == TOK_SI_PUSHED);   // )SI not pushed
}
//----------------------------------------------------------------------------
void
Prefix::push_END_error()
{
   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

   // provide help on some common cases...
   //
   for (int j = 1; j < (size() - 1); ++j)
       {
         if ( (at(j)    .tok.get_Class() == TC_ASSIGN) &&
              (at(j + 1).tok.get_Class() == TC_VALUE))
            {
              const TokenClass left = at(j - 1).tok.get_Class();
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
   syntax_error(LOC);   // no more token
}
//----------------------------------------------------------------------------
bool
Prefix::push_next_token()
{
   if (saved_MISC.tok.get_tag() != TOK_VOID)   // valid lookahead token
      {
        // there is a stored MISC token from a MISC phrase. Symbol resolution
        // was already performed, so we can push it now and are done.
        //
        push(saved_MISC);
        saved_MISC.tok.clear(LOC);   // saved_MISC = TOK_VOID
        return false;
      }

   // TC_END is the end of the statement (i.e. the leftmost token in APL).
   // The caller wants one more Token, but we may not have any. In that
   /// case: prepare the )MORE info and throw a SYNTAX_ERROR.
   //
   if (size() > 0 && at0().get_Class() == TC_END)   push_END_error();

Token_loc tloc = lookahead();   // possibly changed by Symbol::resolve()
const TokenClass tcl = tloc.tok.get_Class();
   Log(LOG_prefix_parser)
      {
        CERR << "    [si=" << si.get_level() << " PC=" << (PC - 1)
             << "] Read token[" << size()
             << "] (←" << get_assign_state() << "←) " << tloc.tok << " "
             << Token::class_name(tcl) << endl;
      }

   PC_range_high = tloc.pc;   // expand the PC range of the stack

   if (tcl == TC_SYMBOL)   // resolve symbol if necessary
      return push_Symbol(tloc);   // true iff )SI pushed
   else if (tcl == TC_ASSIGN)   // update assign_state (from right to left)
      {
        if (get_assign_state() != ASS_none)   syntax_error(LOC);
        set_assign_state(ASS_arrow_seen);
      }

   push(tloc);
   return false;   // )SI not pushed
}
//----------------------------------------------------------------------------
#ifdef PREFIX_HASH
void
Prefix::find_best_phrase()
{
const int hash_0 = at0().get_Class();

  if (size() >= 3)   // at2() and maybe at3() is valid
     {
       const int hash_01  = hash_0  | at1().get_Class() <<  5;
       const int hash_012 = hash_01 | at2().get_Class() << 10;

       if (size() >= 4)   // at3() is valid
          {
            const int hash_0123 = hash_012 | at3().get_Class() << 15;
            best_phrase = hash_table + hash_0123 % PHRASE_MODU;
            if (best_phrase->phrase_hash == hash_0123)   return;   // found
          }

       best_phrase = hash_table + hash_012 % PHRASE_MODU;
       if (best_phrase->phrase_hash == hash_012)   return;         // found

       best_phrase = hash_table + hash_01 % PHRASE_MODU;
       if (best_phrase->phrase_hash == hash_01)   return;          // found

       best_phrase = hash_table + hash_0 % PHRASE_MODU;
       if (best_phrase->phrase_hash == hash_0)   return;           // found
     }
  else               // at0() and maybe at1() is valid
     {
       if (size() == 2)   // at1() is valid
          {
            const int hash_01 = hash_0 | at1().get_Class() << 5;
            best_phrase = hash_table + hash_01 % PHRASE_MODU;
            if (best_phrase->phrase_hash == hash_01)   return;     // found
          }

       // no len 2 phrase match
       best_phrase = hash_table + hash_0 % PHRASE_MODU;
       if (best_phrase->phrase_hash == hash_0)   return;           // found
     }

   best_phrase = 0;   // not found
}
#endif

#ifdef PREFIX_TREE
void
Prefix::find_best_phrase()
{
   best_phrase = 0;   // assume nbothing found

// CERR << "\nfind_best_phrase(): size=" << size() << endl;
// print_stack(CERR, LOC);

int idx = 0;
   loop(s, size())
       {
// CERR << "s=" << s << " idx=" << HEX2(idx) << endl;

         const TokenClass tc = at(s).tok.get_Class();
// CERR << " TokenClass tc=" << HEX2(tc) << " aka. " << Token::class_name(tc) << endl;

         idx = hash_table[idx].sub_nodes[tc];
         const Phrase & phrase = hash_table[idx];
// CERR << " phrase now #" << HEX2(idx) << ": " << phrase.phrase_name << endl;
// CERR << " reduce name: " << phrase.reduce_name << endl;

         {
// CERR << "  ├── best_phrase before=" << voidP(best_phrase) << endl;
         if (phrase.reduce_fun)   best_phrase = &phrase;
// CERR << "  └── best_phrase after=" << voidP(best_phrase) << endl;
         }

// if (idx)   CERR << "  now idx=" << HEX2(idx) << " : " << hash_table[idx].reduce_name << endl;
// else       CERR << "  no child" << endl;
         if (idx == 0)
            {
// CERR << "end of tree" << endl;
              return;
            }
       }
// CERR << "end of prefix" << endl;
}
#endif
//----------------------------------------------------------------------------

// one entry of a hash table for all prefixes that can be reduced.
// Used in Prefix.def
//
# define reduce_none 0

#ifdef PREFIX_HASH
# define PH(name, suffix, idx, prio, misc, len)                         \
   { #name, #suffix, &Prefix::reduce_ ## suffix, idx, prio, misc, len }
#endif   // PREFIX_HASH

#ifdef PREFIX_TREE

# define PH(name, suffix, idx, prio, misc, len,                               \
           t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,tA,tB,tC,tD,tE,tF,tG,tH)             \
  { #name, #suffix, &Prefix::reduce_ ## suffix, idx, prio, misc, len,         \
    { 0x##t0, 0x##t1, 0x##t2, 0x##t3, 0x##t4, 0x##t5, 0x##t6, 0x##t7, 0x##t8, \
      0x##t9, 0x##tA, 0x##tB, 0x##tC, 0x##tD, 0x##tE, 0x##tF, 0x##tG, 0x##tH }}

// dito, but without reduce function.
#define P0(name, suffix, idx, prio, misc, len,                          \
           t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,tA,tB,tC,tD,tE,tF,tG,tH)       \
   { #name, #suffix, 0, idx, prio, misc, len,                           \
    { 0x##t0, 0x##t1, 0x##t2, 0x##t3, 0x##t4, 0x##t5, 0x##t6, 0x##t7, 0x##t8, \
      0x##t9, 0x##tA, 0x##tB, 0x##tC, 0x##tD, 0x##tE, 0x##tF, 0x##tG, 0x##tH }}
#endif   // PREFIX_TREE

const Prefix::Phrase Prefix::hash_table[] =
{
// the entire hash table or tree with all prefixes that can be reduced...
#include "Prefix.def"   // the hash table
};

Token
Prefix::reduce_statements()
{
   Log(LOG_prefix_parser)
      {
        CERR << endl << "changed to Prefix[si=" << si.get_level()
             << "]) ============================================" << endl;
      }

   if (size() > 0)   goto again;

   // the main loop of an LALR(1) parser...
   //
grow:    // aka. SHIFT
   // the current stack does not contain a valid phrase.
   // Push one more token onto the stack and continue
   //
   if (push_next_token())   return Token(TOK_SI_PUSHED);

again:   // aka. REDUCE
   Log(LOG_prefix_parser)   print_stack(CERR, LOC);

   // search longest prefixes in phrase table...
   //
   find_best_phrase();   // set best_phrase
   if (best_phrase == 0)   goto grow;

   // found a reducible prefix. See if the next token class binds stronger
   // than best_phrase->prio
   //
   if (check_next_binding())   goto grow;

   Log(LOG_prefix_parser)  CERR
      << "   phrase #" <<  (best_phrase - hash_table)
      << ": " << best_phrase->phrase_name
      << " matches, prio " << best_phrase->prio
      << ", calling reduce_" << best_phrase->reduce_name
      << "()" << endl;

   action = RA_FIXME;   // to detect missing action = in a reduce_XXX()
   prefix_len = best_phrase->phrase_len;
   if (best_phrase->misc)   // MISC phrase: save X and remove it
      {
        Assert(saved_MISC.tok.get_tag() == TOK_VOID);
        saved_MISC.copy(pop(), LOC);
        --prefix_len;
      }

const uint64_t inst = instance;
   (this->*best_phrase->reduce_fun)();

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
        case RA_CONTINUE: LOG_prefix_parser && CERR << "RA_CONTINUE" << endl;
             goto again;

        case RA_PUSH_NEXT: LOG_prefix_parser && CERR << "RA_PUSH_NEXT" << endl;
             goto grow;

        case RA_SI_PUSHED: LOG_prefix_parser && CERR << "RA_SI_PUSHED" << endl;
             return Token(TOK_SI_PUSHED);

        case RA_RETURN: LOG_prefix_parser && CERR << "RA_RETURN" << endl;
             return pop().tok;

        case RA_FIXME: LOG_prefix_parser && CERR << "RA_FIXME" << endl;
             FIXME;
      }

   FIXME;
}
//----------------------------------------------------------------------------
bool
Prefix::check_next_binding()
{
TokenClass next = TC_INVALID;   // assume no bext
    if (PC < Function_PC(body.size()))
       {
         const Token & tok = body[PC];
         next = tok.get_Class();
         if (next == TC_SYMBOL)
            {
              const Symbol * sym = tok.get_sym_ptr();
              const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
              next = sym->resolve_class(is_left_sym);
            }
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
              // onto the stack. The following token are processed until the
              // entire indexed value A[ ... ] is computed
              prefix_len = 1;
              reduce_RBRA___();
              return true;
            }
       }

//   Q1(next) Q1(at0())

   // shift/reduce conflict. See what to do.
   // if we should shift.
   //
   if (do_shift(next))   // i.e. shift
      {
        Log(LOG_prefix_parser)  CERR
             << "   phrase #" << (best_phrase - hash_table)
             << ": " << best_phrase->phrase_name
             << " matches, but prio " << best_phrase->prio
             << " is too small to call " << best_phrase->reduce_name
             << "()" << endl;
        return true;
      }

   return false;
}
//----------------------------------------------------------------------------
bool
Prefix::do_shift(TokenClass next) const
{
   switch(at0().get_Class())
      {
        case TC_VALUE:
             if (next == TC_OPER2)           // DOP B
                {
                  return true;
                }
             else if (next == TC_VALUE)      // A B
                {
                  return best_phrase->prio < BS_VAL_VAL;
                }
             else if (next == TC_R_PARENT)   // ) B
                {
                  if (is_value_parenthesis(PC))     // e.g. (X+Y) B
                     {
                       return best_phrase->prio < BS_VAL_VAL;
                     }
                   else                      // e.g. (+/) B
                     {
                       return false;
                     }
                }
             return false;   // TC_VALUE

        case TC_FUN12:
             if (next == TC_OPER2)
                {
                  return true;
                }
             return false;   // TC_FUN12

        case TC_SYMBOL:
             if (next == TC_OPER2)
                {
                  return true;
                }
             return false;   // TC_SYMBOL

        default: return false;
      }
}
//----------------------------------------------------------------------------
bool
Prefix::replace_AB(Value_P old_value, Value_P new_value)
{
   Assert(+old_value);
   Assert(+new_value);

   loop(s, size())
     {
       Token & tok = at(s).tok;
       if (tok.get_Class() != TC_VALUE)   continue;
       if (tok.get_apl_val().get() == old_value.get())   // found
          {
            new (&tok) Token(tok.get_tag(), new_value);
            return true;
          }
     }
   return false;
}
//----------------------------------------------------------------------------
Value_P *
Prefix::locate_L(UCS_string & function)
{
   /*  ⎕L requires at least A f B (so we have at0(), at1() and at2(). However,
       the user could fail on a monadic function (with size() = 2) and then
       query ⎕L. For example:

       at0 at1 at2  size()
        ÷   0       2            in × ÷ 0   (hence no ⎕L)
        2   ÷   0   3            in 2 ÷ 0   (⎕L is 2)
    */

   if (size() > 0 && at0().get_ValueType() == TV_FUN)
      {
        // e.g. DOMAIN ERROR in × ÷ 0. size() is 2 and at0() is ÷
        function = at0().get_function()->get_name();
        return 0;
      }

   if (size() > 1 && at1().get_ValueType() == TV_FUN)
      {
        function = at1().get_function()->get_name();
      }
   else if (size() == 2 && at1().get_Class() == TC_INDEX)   // B[X]
      {
        function = UCS_string("[]");
        return at0().get_apl_valp();
      }

   if (size() < 3)   return 0;

   if (at0().get_Class() == TC_VALUE)   return at0().get_apl_valp();
   return 0;
}
//----------------------------------------------------------------------------
Value_P *
Prefix::locate_X(UCS_string & function)
{
   // ⎕X requires at least X B (so we have at0() and at1()

   if (size() < 2)   return 0;

   // either at0() (for monadic f X B) or at1() (for dyadic A f X B) must
   // be a function or operator
   //
   for (int x = size() - 1; x >= 0; --x)
       {
         if (content[x].tok.get_ValueType() == TV_FUN)
            {
              if (Function_P fun = content[x].tok.get_function())
                 {
                   function = fun->get_name();
                   // locate_X() always returns 0 for non-derived functions
                   // because we can't allow ⎕X to modify the function body.
                   if (Value_P * X = fun->locate_X())   return  X;
                 }
            }
         else if (content[x].tok.get_Class() == TC_INDEX)   // maybe found X ?
            {
              return content[x].tok.get_apl_valp();
            }
       }

   return 0;
}
//----------------------------------------------------------------------------
Value_P *
Prefix::locate_R(UCS_string & function)
{
   // ⎕R requires at least f B (so we have at0() and at1()

   if (size() < 2)   return 0;

   if (size() == 2 && at1().get_Class() == TC_INDEX)   // B[X]
      {
        function = UCS_string("[]");   // valid function
        return 0;                      // but no ⎕R.
      }

   // either at0() (for monadic f B) or at1() (for dyadic A f B) must
   // be a function or operator
   //
   if (at0().get_ValueType() != TV_FUN &&
       at1().get_ValueType() != TV_FUN)   return 0;

Token * ret = &content[size() - prefix_len].tok;
   if (ret->get_Class() == TC_VALUE)   return ret->get_apl_valp();
   return 0;
}
//----------------------------------------------------------------------------
void
Prefix::print(ostream & out, int indent) const
{
   loop(i, indent)   out << "    ";
   out << "Token: ";
   loop(s, size())   out << " " << at(s).tok;
   out << endl;
}
//============================================================================
//
// phrase reduce functions...
//
//----------------------------------------------------------------------------
void
Prefix::reduce____()
{
   // this function is a placeholder for invalid phrases and should never be
   // called.
   //
   FIXME;
}
//----------------------------------------------------------------------------
void
Prefix::reduce_LPAR_B_RPAR_()
{
   Assert1(prefix_len == 3);

Token result = at1();   // B or F
   if (result.get_tag() == TOK_APL_VALUE3)   result.ChangeTag(TOK_APL_VALUE1);

   pop_args_push_result(result);
   set_action(RA_CONTINUE);
}
//----------------------------------------------------------------------------
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
   at3().move(at2(), LOC);   // move C left
   at2().move(at1(), LOC);   // move F left
   pop_and_discard();          // discard old C
   pop_and_discard();          // discard old RPAR
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_N___()
{
   Assert1(prefix_len == 1);

Token result = at0().get_function()->eval_();
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        set_action(RA_RETURN);            // return from context;
        return;
      }

   pop_args_push_result(result);
   set_action(RA_CONTINUE);
}
//----------------------------------------------------------------------------
void
Prefix::reduce_MISC_F_B_()
{
   Assert1(prefix_len == 2);

   if (saved_MISC.tok.get_Class() == TC_INDEX)
      {
        if (value_expected())
           {
             // push [...] and read one more token
             //
             push(saved_MISC);
             saved_MISC.tok.clear(LOC);
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
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

             const Cell & QES_arg2 = result.get_apl_val()->get_cravel(2);
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
                  scalar->next_ravel_Cell(QES_arg2);
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

             const Cell & QES_arg2 = result.get_apl_val()->get_cravel(2);
             const APL_Integer line = QES_arg2.get_int_value();

             Token & si_pushed = Workspace::SI_top()->get_prefix().at0();
             Assert(si_pushed.get_tag() == TOK_SI_PUSHED);

             Value_P v_line = IntScalar(line, LOC);
             Workspace::SI_top()->jump(v_line.get());
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

             const Cell * QES_arg = &result.get_apl_val()->get_cfirst();
             UCS_string statement_A(  *QES_arg[2].get_pointer_value());
             const APL_Integer major = QES_arg[3].get_int_value();
             const APL_Integer minor = QES_arg[4].get_int_value();
             const ErrorCode ec      = ErrorCode(major << 16 | minor);

             Token result_A = Bif_F1_EXECUTE::execute_statement(statement_A);
             if (result_A.get_Class() == TC_VALUE)   // ⍎ literal
                {
                  Workspace::SI_top()->get_prefix().at0().move(result_A, LOC);
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
             set_action(RA_RETURN);            // return from context;
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
//----------------------------------------------------------------------------
void
Prefix::reduce_MISC_F_C_B()
{
   Assert1(prefix_len == 3);

   if (saved_MISC.tok.get_Class() == TC_INDEX)
      {
        if (value_expected())
           {
             // push [...] and read one more token
             //
             push(saved_MISC);
             saved_MISC.tok.clear(LOC);
             set_action(RA_PUSH_NEXT);   // aka. SHIFT
             return;
           }
      }

   if (at1().get_ValueType() != TV_VAL)   // [i1;i2...] instead of [axis]
      {
        // the user has mistakenly used a bracket index instead of a
        // function axis. We must delete the current IndexExpr before
        // throwing the syntax error.
        //
        IndexExpr * idx = &at1().get_index_val();
        Log(LOG_delete)   CERR << "delete " << voidP(idx) << " at " LOC
                               << endl;
        delete idx;
        at1().clear(LOC);
        SYNTAX_ERROR;
      }

   if (!at1().get_apl_val())   SYNTAX_ERROR;

   if (at0().get_tag() == TOK_Quad_FIO &&
       saved_MISC.tok.get_Class() == TC_FUN12)
      {
        DerivedFunction * derived =
                          Workspace::SI_top()->fun_oper_cache.get(LOC);
        new (derived)   DerivedFunction(saved_MISC.tok,
                                        at0().get_function(),
                                        at1().get_apl_val(),  LOC);
        saved_MISC.tok.clear(LOC);
        prefix_len = 2;   // only f ⎕FIO
        pop_args_push_result(Token(TOK_FUN2, derived));
        set_action(RA_CONTINUE);   // match again (w/o SHIFT)
        return;
      }

Token result = at0().get_function()->eval_XB(at1().get_apl_val(),
                                             at2().get_apl_val());
   if (result.get_tag() == TOK_ERROR)
      {
        Token_loc tl(result, get_range_low());
        push(tl);
        set_action(RA_RETURN);            // return from context;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//----------------------------------------------------------------------------
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
        set_action(RA_RETURN);            // return from context;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_M_B_()
{
   if (is_SLASH_or_BACKSLASH(at1().get_tag()))   return reduce_A_F_B_();
   syntax_error(LOC);
}
//----------------------------------------------------------------------------
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
        set_action(RA_RETURN);            // return from context;
        return;
      }

   pop_args_push_result(result);
   set_action(result);
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_M_C_B()
{
   if (is_SLASH_or_BACKSLASH(at1().get_tag()))   return reduce_A_F_C_B();
   syntax_error(LOC);
}
//----------------------------------------------------------------------------
void
Prefix::reduce_F_M__()
{
   Assert1(prefix_len == 2);

DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
bool
Prefix::MM_is_FM(Function_PC pc)
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
             i.e. ← → ; [ END or ( imply that M1 shall be a function F
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
                   return false;

              case TC_SYMBOL:
                   return NEXT.get_sym_ptr()->M_is_F();

              // TC_INDEX should not happen here since body[PC]... have
              // not yet been parsed
              case TC_INDEX: FIXME

              case TC_R_BRACK:   // skip over [ ... ]
                   pc = Function_PC(int(pc) + body[pc].get_int_val2());
                   continue;

              case TC_R_PARENT:
                   pc = Function_PC(pc + 1);
                   continue;

              default: CERR << "NEXT: " << tc_NEXT << endl;
                       TODO;
            }
       }
}
//----------------------------------------------------------------------------
void
Prefix::reduce_M_M__()
{
   if (is_SLASH_or_BACKSLASH(at0().get_tag()) && MM_is_FM(PC))
      {
        reduce_F_M__();
      }
   else   // the normal case: the left M is an operator. SHIFT
      {
        set_action(RA_PUSH_NEXT);
      }
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_M__()
{
   Assert1(prefix_len == 2);

DerivedFunction * derived = Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), LOC);

   pop_args_push_result(Token(TOK_FUN1, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_F_M_C_()
{
   Assert1(prefix_len == 3);

   if (at2().get_tag() != TOK_AXIS)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXIS was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(),
                                 at1().get_function(),
                                 at2().get_axes(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_F_C_M_()
{
   Assert1(prefix_len == 3);

   if (at1().get_tag() != TOK_AXIS)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXIS was expected
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
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_F_C_M_C()
{
   Assert1(prefix_len == 4);

   if (at1().get_tag() != TOK_AXIS)   // e.g. F[;2] instead of F[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXIS was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

   if (at3().get_tag() != TOK_AXIS)   // e.g. M[;2] instead of M[2]
      {
        // the user has provided a TOK_INDEX where TOK_AXIS was expected
        MORE_ERROR() << "illegal ; in axis";
        AXIS_ERROR;
      }

DerivedFunction * F_C = Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (F_C) DerivedFunction(at0().get_function(), at1().get_axes(), LOC);

Token tok_F_C(TOK_FUN2, F_C);
DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(tok_F_C, at2().get_function(),
                                          at3().get_axes(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_D_V__()
{
   /* end of an A.B.C...V chain. at0() is the final member V and at1() is
      the '.' (aka. D) before V. Collect the members (and discard the '.'
      preceeding them) until no more '.' token are found.

      Prefix::reduce_D_V__() is called from two places:

      1. from reduce_statements() with prefix_len == 2, or else
      2. from reduce_D_V_ASS_B()  with prefix_len == 4.

      The processing of member variables is somewhat lengthy and almost the
      same for member reference and for member assignment. We therefore
      handle both cases here (i.e. in reduce_D_V__()) instead of replicating
      almost the same code in in reduce_D_V_ASS_B().
    */

const bool member_assign = prefix_len == 4;   // assume member reference
   if (prefix_len == 2)        // case 1: member reference
      {
      }
   else if (member_assign)     // case 2: member assignment
      {
        Assert(get_assign_state() == ASS_var_seen);   // by reduce_D_V_ASS_B
      }
   else                        // something unexected
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
   members.push_back(at1().get_sym_ptr()->get_name_ptr());
   while (size_t(PC) < (body.size() - 1))   // at least 2 more token
         {
           if (body[PC].get_Class() == TC_SYMBOL)   // the normal case
              {
                Symbol * symbol = body[PC].get_sym_ptr();
                members.push_back(symbol->get_name_ptr());
               if (body[PC + 1].get_tag() == TOK_OPER2_INNER)
                  {
                    PC = Function_PC(PC + 2);
                  }
               else
                  {
                    top_sym = symbol;
                    PC = Function_PC(PC + 1);
                    break;
                  }
              }
           else   // body[PC] is not a symbol: the special ⎕CR/⎕FIO case
              {
                if (members.size() == 1 &&
                    body[PC].get_Class() == TC_FUN12 &&
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
                     Function_P fun = body[PC].get_function();
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

Value_P top_val = top_sym->get_var_value();
   if (!top_val)   // top_sym is not a variable (-name). Maybe create one.
      {
        if (member_assign)
           {
             // VAR.member ← value. The user assigns a value to the member of
             // a structured variable that does not yet exist. We do the same
             // as for VAR←value for not existing APL variables, i.e. we
             // create it automatically.
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
             VALUE_ERROR;
           }
      }

Value * member_owner = 0;
Cell * member_cell = top_val->get_member(members, member_owner,
                                         member_assign, true);
   Assert(member_owner);

   if (member_assign)   // (direct) member assignment
      {
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
             member_cell->init(B->get_cfirst(), *member_owner, LOC);
           }
        else
           {
             new (member_cell)   PointerCell(B.get(), *member_owner);
           }
        pop_args_push_result(Token(TOK_APL_VALUE2, B));
      }
   else                 // member reference (or selective specification)
      {
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
                   Value_P cell_refs = member_cell->get_pointer_value()
                                                 ->get_cellrefs(LOC);
                   pop_args_push_result(Token(TOK_APL_VALUE2, cell_refs));
                 }
              else
                 {
                   Value_P Z(CLONE_P(member_cell->get_pointer_value(),LOC), LOC);
                   pop_args_push_result(Token(TOK_APL_VALUE1, Z));
                 }
            }
        else
           {
             Value_P Z(LOC);
             Z->next_ravel_Cell(*member_cell);
             Z->check_value(LOC);
             pop_args_push_result(Token(TOK_APL_VALUE1, Z));
           }
      }

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_F_D_G_()
{
DerivedFunction * derived =
   Workspace::SI_top()->fun_oper_cache.get(LOC);
   new (derived) DerivedFunction(at0(), at1().get_function(), at2(), LOC);

   pop_args_push_result(Token(TOK_FUN2, derived));
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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
               const bool is_left_sym = get_assign_state() == ASS_arrow_seen;
               next = sym->resolve_class(is_left_sym);
             }

          if (next == TC_OPER2)
             {
               set_action(RA_PUSH_NEXT);   // aka. SHIFT
               return;
             }
        }
   // we have f ⍤ [X] y_B with y_B glued beforehand. Unglue it.
   //
Value_P y123;
Value_P B;
   Bif_OPER2_RANK::split_y123_B(at3().get_apl_val(), y123, B);
Token new_y123(TOK_APL_VALUE1, y123);

   if (at2().get_tag() != TOK_AXIS)   // e.g. D[;2] instead of D[;2]
      {
        // the user has provided a TOK_INDEX where TOK_AXIS was expected
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

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_C__()
{
Value_P A = at0().get_apl_val();
Value_P Z;

   if (at1().get_tag() == TOK_AXIS)
      {
        Z = A->index(at1().get_apl_val().get());
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
//----------------------------------------------------------------------------
void
Prefix::reduce_V_C__()
{
Symbol * V = at0().get_sym_ptr();
Token tok = V->resolve_lv(LOC);
   at0().move(tok, LOC);
   set_assign_state(ASS_var_seen);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_V_C_ASS_B()
{
Symbol * V = at0().get_sym_ptr();
Value_P B = at3().get_apl_val();

   if (at1().get_tag() == TOK_AXIS)   // [] or [x]
      {
        const Value * v_idx = at1().get_axes().get();

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
//----------------------------------------------------------------------------
void
Prefix::reduce_F_V__()
{
   // turn V into a (left-) value
   //
Symbol * V = at1().get_sym_ptr();
Token tok = V->resolve_lv(LOC);
   at1().move(tok, LOC);
   set_assign_state(ASS_var_seen);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_ASS_B_()
{
Value_P A = at0().get_apl_val();
Value_P B = at2().get_apl_val();

   A->assign_cellrefs(B);

Token result = Token(TOK_APL_VALUE2, B);
   pop_args_push_result(result);

   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_D_V_ASS_B()
{
  set_assign_state(ASS_var_seen);
  reduce_D_V__();
  set_assign_state(ASS_none);
}
//----------------------------------------------------------------------------
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
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)

   V->assign(B, clone, LOC);
}
//----------------------------------------------------------------------------
void
Prefix::reduce_V_ASS_F_()
{
   // named lambda: V ← { ... }
   //
Function_P F = at2().get_function();
   if (!F->is_lambda())   SYNTAX_ERROR;

Symbol * V = at0().get_sym_ptr();
   if (V->assign_named_lambda(F, LOC))   DEFN_ERROR;

Value_P Z(V->get_name(), LOC);
Token result = Token(TOK_APL_VALUE2, Z);
   pop_args_push_result(result);

   set_assign_state(ASS_none);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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
        Token result = Token(TOK_INDEX, idx);
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

        if (idx.is_axis())
           {
             Token tok_axis(TOK_AXIS, idx.values[0]);
             result.move(tok_axis, LOC);
           }
        else
           {
               Token tok_index(TOK_INDEX, idx);
               result.move(tok_index, LOC);
           }
      }
   else
      {
        set_assign_state(ASS_none);
      }

   pop_args_push_result(result);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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
             Assert1(+X);   // not [ ]
             Token tok_axis(TOK_AXIS, X);
             I.move(tok_axis, LOC);
             Log(LOG_delete)
                CERR << "delete " << voidP(&idx) << " at " LOC << endl;
             delete &idx;
           }
        else
           {
             Token tok_index(TOK_INDEX, idx);
             I.move(tok_index, LOC);
           }
      }
   else
      {
         set_assign_state(ASS_none);
      }

   pop_args_push_result(I);
   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_B__()
{
   Assert1(prefix_len == 2);

Token result;
   Value::glue(result, at0(), at1(), LOC);
   pop_args_push_result(result);

   set_action(result);
}
//----------------------------------------------------------------------------
/// pattern V ) ← B
void
Prefix::reduce_V_RPAR_ASS_B()
{
   Assert1(prefix_len == 4);

const int count = vector_ass_count();

   if (count == 0)
      {
        // count == 0 normally indicates a selective specification. However,
        // an incorrect vector assignment such as (A 1 C) ← would also lead
        // to count == 0 (because Parser.cc would set is_vector_spec to
        // false around line 820 in Parser.cc). We fix this case here.
        //
        TokenClass tc = body[PC].get_Class();
        if (tc != TC_FUN12 && tc != TC_OPER1 && tc != TC_OPER2)
           {
             // this case is rather rare, so we can afford a little time
             // to verify that we have at least one function in the supposed
             // selective specification
             bool selective_spec = false;
             for (int pc = PC + 1;;)
                 {
                   tc = body[pc++].get_Class();
                   if (tc == TC_FUN12 || tc == TC_OPER1 || tc == TC_OPER2)
                      {
                        selective_spec = true;
                        break;
                      }
                   else if (tc == TC_L_PARENT)   // most likely end of (...)←
                      {
                        break;
                      }
                 }

             if (!selective_spec)
                {
                  MORE_ERROR() <<
                  "Malformed selective specification or vector specification";
                  LEFT_SYNTAX_ERROR;
                }
           }
      }

   // vector assignment also matches selective specification, but count
   // distinguishes them, e.g.
   //
   // (T U V) ← value        count = 2      vector assignment
   //   (U V) ← value        count = 1      vector assignment
   // (2 ↑ V) ← value        count = 0      selective specification
   //
   if (count < 1)
      {
        // selective specification. Convert variable V into a (left-) value
        //
        Symbol * V = at0().get_sym_ptr();
        Assert1(V);
        Token result = V->resolve_lv(LOC);
        set_assign_state(ASS_var_seen);
        at0().move(result, LOC);
        set_action(RA_CONTINUE);   // match again (w/o SHIFT)
        return;
      }

   // vector assignment.
   //
std::vector<Symbol *> symbols;
   symbols.push_back(at0().get_sym_ptr());
   loop(c, count)
      {
        const Token tok = lookahead().tok;
        Assert1(tok.get_tag() == TOK_LSYMB2);   // by vector_ass_count()
        Symbol * V = tok.get_sym_ptr();
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

   set_action(RA_CONTINUE);   // match again (w/o SHIFT)
}
//----------------------------------------------------------------------------
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
   set_action(RA_PUSH_NEXT);   // aka. SHIFT
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//----------------------------------------------------------------------------
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

   set_action(RA_PUSH_NEXT);   // aka. SHIFT
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//----------------------------------------------------------------------------
void
Prefix::reduce_A_GOTO_B_()
{
   // dyadic LABEL → CONDITION.

   Assert1(prefix_len == 3);

   // we want this to be fast, therefore we don't check the shape but
   // rather use ↑A and ↑B,
   //
const Cell & A0 = at0().get_apl_val()->get_cfirst();   // the jump offset
const Cell & B0 = at2().get_apl_val()->get_cfirst();   // the condition

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
     
        // PC was already incremented and now points to the next token
        // after the branch. In order to compute the proper line number,
        // we have to use the PC BEFORE the branch.
        //
        const int function_line = ufun->get_line(PC - prefix_len) + jump_offset;
        si.jump_to_line(Function_Line(function_line));   // changes the PC
        branch_within_function(true);   // check ^C and set_action(RA_PUSH_NEXT)
        reset(LOC);                     // abort the current statement

        // branch_within_function() MAY have set the PC to the end of the
        // function (token ENDL). However, that skips the return of the
        // ∇-result. Fix it.
        //
        if (ulong(PC + 1) == body.size())           // PC at end of function
           {
             Assert(body[PC].get_tag() == TOK_ENDL);
             Assert(body[PC-1].get_Class() == TC_RETURN);
             --PC;
           }
      }
   else         // ⍎ or ◊ context
      {
        // A → B in ⍎ or ◊ context. Unlike ∇-contexts, the line number is
        // always 0 and →0 does NOT return and non-zero jump_offsets are
        // not permitted.
        //
        if (jump_offset)   // function_line is 0 in ⍎ or ◊ contexts
           {
             MORE_ERROR() << "A → B with non-zero A is only permitted "
                             "inside a defined function";
             DOMAIN_ERROR;
           }

        // jump to same line (but outside a defined function)
        //
        set_PC(Function_PC_0);   // calls reset(), so don't pop_args() !
        set_action(RA_PUSH_NEXT);   // aka. SHIFT
      }
}
//----------------------------------------------------------------------------
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
const bool trace = at0().get_Class() == TC_END && (at0().get_int_val() & 1);

const Value * line = at2().get_apl_val().get();

   // produce ⎕TRACE output if enabled and branch is not empty
   //
   if (trace && line->element_count() > 0)
      {
        const ShapeItem line_num = line->get_line_number();
        Token bra(TOK_BRANCH, line_num);
        si.statement_result(bra, true);   // display trace line
      }

const Token result = si.jump(line);   // may change the PC

   if (result.get_tag() == TOK_BRANCH)   // branch back into a function
      {
        Log(LOG_prefix_parser)
           {
             CERR << "Leaving context after " << result << endl;
           }

        pop_args_push_result(result);
        set_action(RA_RETURN);            // return from context;
        return;
      }


   if (result.get_tag() == TOK_NOBRANCH)   // branch not taken, e.g. →⍬
      {
        const Token_loc tl_END = pop();   // END
        pop_and_discard();                // →
        pop_and_discard();                // B
        const Token result(TOK_APL_VALUE2, Idx0(LOC));
        const Token_loc tl_result(result, tl_END.pc + 1);
        push(tl_result);
        push(tl_END);

        // not a backward branch, therefore no need to check INT or ATT,
        // or to ⎕STOP or ⎕TRACE.
        //
        set_action(RA_CONTINUE);   // aka. SHIFT
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
//----------------------------------------------------------------------------
void
Prefix::check_interrupt_or_attention(bool end_of_line)
{
   if (attention_is_raised() && end_of_line)
      {
        const bool int_raised = interrupt_is_raised();
        clear_attention_raised(LOC);
        clear_interrupt_raised(LOC);
        if (int_raised)   INTERRUPT
        else              ATTENTION
      }
}
//----------------------------------------------------------------------------
void
Prefix::reduce_END_GOTO__()   // Escape ( → )
{
   Assert1(prefix_len == 2);

   if (size() != 2)   syntax_error(LOC);

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
        const UserFunction * ufun = si.get_executable()->get_exec_ufun();
        if (ufun && ufun->get_exec_properties()[2])
           {
              // the function ignores attention (aka. weak interrupt)
              //
              pop_and_discard();   // pop END
              pop_and_discard();   // pop GOTO
              set_action(RA_CONTINUE);   // match again (w/o SHIFT)
              return;
           }

        COUT << si.function_name() << "[" << si.get_line() << "]" << endl;
        Token result(TOK_ERROR, E_STOP_LINE);
        pop_args_push_result(result);
        set_action(RA_RETURN);            // return from context;
      }
   else
      {
        Token result(TOK_ESCAPE);
        pop_args_push_result(result);
        set_action(RA_RETURN);            // return from context;
      }
}
//----------------------------------------------------------------------------
void
Prefix::reduce_RETC_VOID__()
{
   // e.g.      ⎕FX 'qio' '⎕IO' ◊ ⍎'qio'
   //
   // execute of a defined function without result. Im[pssible for lambdas
   // since they always have a result λ←.

   Assert1(prefix_len == 2);

Token result = Token(TOK_VOID);   // function result is VOID
   pop_args_push_result(result);
   set_action(RA_RETURN);            // return from context;
}
//----------------------------------------------------------------------------
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
   // case TOK_RETURN_EXEC (end of ⍎ context) is handled in reduce_RETC_A___()
   //
   switch(at0().get_tag())
      {
        case TOK_RETURN_EXEC:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ⍎ context (no result)" << endl;
             at0().clear(LOC);
             set_action(RA_RETURN);            // return from context;
             return;

        case TOK_RETURN_STATS:   // immediate execution context
             Log(LOG_prefix_parser)
                CERR << "- end of ◊ context" << endl;
             at0().clear(LOC);
             set_action(RA_RETURN);            // return from context;
             return;

        case TOK_RETURN_VOID:   // user-defined function not returning a value
             Log(LOG_prefix_parser)
                CERR << "- end of ∇ context (function has no result)" << endl;

             {
               const UserFunction * ufun = si.get_executable()->get_exec_ufun();
               if (ufun)   { /* do nothing, needed for -Wall */ }
               Assert1(ufun);
               at0().clear(LOC);
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
                    at0().clear(LOC);
                  }
               else
                  {
                    Log(LOG_prefix_parser)
                       CERR << "- end of ∇ context (function result is: "
                            << *Z << ")" << endl;
                    new (&at0()) Token(TOK_APL_VALUE1, Z);
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
//----------------------------------------------------------------------------
// Note: reduce_RETC_A___ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_B___ instead.
//
void
Prefix::reduce_RETC_A__()
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

   set_action(RA_RETURN);            // return from context;
}
//----------------------------------------------------------------------------
// Note: reduce_RETC_GOTO_B__ happens only for context ⍎, since
//       the contexts ◊ and ∇ use reduce_END_GOTO_B__ instead.
//
void
Prefix::reduce_RETC_A_GOTO_B()
{
   Assert1(prefix_len == 4);
   syntax_error(LOC);
}
//----------------------------------------------------------------------------
// Note: reduce_RETC_GOTO_B__ happens only for context ⍎, since
//       the contexts ◊ and ∇ use reduce_END_GOTO_B__ instead.
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

   set_action(RA_RETURN);            // return from context;
}
//----------------------------------------------------------------------------
// Note: reduce_RETC_ESC___ happens only for context ⍎,
//       since contexts ◊ and ∇ use reduce_END_ESC___ instead.
//
void
Prefix::reduce_RETC_GOTO__()
{
   if (size() != 2)   syntax_error(LOC);

   reduce_END_GOTO__();
}
//----------------------------------------------------------------------------

