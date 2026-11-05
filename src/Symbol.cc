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

#include <string.h>
#include <strings.h>

#include "ArgCheck.hh"
#include "CDR.hh"
#include "Command.hh"
#include "Function.hh"
#include "IndexExpr.hh"
#include "IndexIterator.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "ProcessorID.hh"
#include "QuadFunction.hh"
#include "Quad_TF.hh"
#include "StateIndicator.hh"
#include "Svar_signals.hh"
#include "Symbol.hh"
#include "SystemVariable.hh"
#include "UserFunction.hh"
#include "Value.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
Symbol::Symbol(Id id)
   : NamedObject(id),
     next(0),
     monitor_callback(0),
     name(ID::get_name_UCS(id))
{
   push();
}
//────────────────────────────────────────────────────────────────────────────
Symbol::Symbol(const UCS_string & ucs, Id id)
   : NamedObject(id),
     next(0),
     monitor_callback(0),
     name(ucs)
{
   push();
}
//────────────────────────────────────────────────────────────────────────────
bool
Symbol::can_be_assigned() const
{
   // Bugs31 #24 (Blake McBride): NC_left alone doesn't exclude a
   // read-only system variable like ⎕TS -- it is still classified as a
   // (conceptually) variable-shaped name, but Symbol::assign() is a
   // silent no-op for it (RO_SystemVariable::assign()). A direct
   // assignment ⎕TS←7 never reaches assign() at all (it goes through
   // resolve_left(), which correctly SYNTAX_ERRORs first), but
   // Prefix::try_vector_assignment()/Symbol::vector_assignment() call
   // assign() directly, so a read-only variable in a non-final vector-
   // assignment position (e.g. (⎕TS A B)←7 8 9) used to pass this check,
   // silently dropping its own assignment while A and B still got
   // theirs. Excluding it here instead makes try_vector_assignment()'s
   // existing loop treat it exactly like any other non-variable token
   // (stop collecting, leave it unconsumed), which correctly surfaces as
   // "Malformed vector specification" / SYNTAX ERROR with no partial
   // assignment at all -- consistent with plain ⎕TS←7.
   //
   return (value_stack.back().get_NC() & NC_left) && !is_readonly();
}
//────────────────────────────────────────────────────────────────────────────
const char *
Symbol::cant_be_defined() const
{
// if (value_stack.size() > 1)         return "symbol was localized";
   if (Workspace::is_called(name))
      {
        static char cc[100];
        const UTF8_string utf(get_name());
        SPRINTF(cc, "function %s is called (used on the )SI stack). "
                    "Try )SIC first.", utf.c_str());
        return cc;
      }

   // Bugs28 #16: also refuse if the current function is bound as the
   // LO/RO operand of some other, suspended defined operator -- that
   // frame holds a raw Function* to it for as long as it stays
   // suspended, invisible to is_called() above since no source text
   // anywhere names this function.
   //
   if (value_stack.back().get_NC() & NC_FUN_OPER)
      {
        const UserFunction * ufun =
                     value_stack.back().get_function()->get_func_ufun();
        if (ufun && Workspace::is_bound_as_operand(ufun))
           {
             static char cc[100];
             const UTF8_string utf(get_name());
             SPRINTF(cc, "function %s is bound as the operand of a "
                         "suspended defined operator. Try )SIC first.",
                     utf.c_str());
             return cc;
           }
      }

   if (value_stack.back().get_NC() & NC_DEFINABLE)   return 0;   // OK

   return "bad name class";
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::dump(ostream & out) const
{
const ValueStackItem & vs = value_stack[0];
   if (vs.get_NC() == NC_VARIABLE)
      {
        UCS_string_vector CR10;
        const cValue * value = vs.get_val_cptr();
        Quad_CR::do_CR10_variable(CR10, get_name(), value);

        if (value->is_member())
           out << "⍝ structured variable " << get_name() << endl;

        loop(l, CR10.size())
           {
             if (l || value->is_member())   out << "  ";
             out << CR10[l] << endl;
           }

        if (value->is_member())
           out << "⍝ end of structured variable " << get_name() << endl;
      }
   else if (vs.get_NC() & NC_FUN_OPER)
      {
        cFunction_P fun = vs.get_function();
        if (fun == 0)
           {
             out << "⍝ function " << get_name() << " has function pointer 0!"
                 << endl << endl;
             return;
           }

        const UserFunction * ufun = fun->get_func_ufun();
        if (ufun == 0)
           {
             out << "⍝ function " << get_name() << " has ufun1 pointer 0!"
                 << endl << endl;
             return;
           }

        const UCS_string text = ufun->canonical(false);
        if (ufun->is_lambda())
           {
             out << get_name();
             out << "←{";
             int t = 0;
             while (t < text.ssize())   // skip λ header
                {
                  const Unicode uni = text[t++];
                  if (uni == UNI_LF)   break;
                }

             // copy body. This used to first skip a λ← right after the
             // header (lines removed above), which only strips the
             // synthesized λ← for a SINGLE-statement lambda -- for a
             // multi-statement one (e.g. {⍺+⍵ ⋄ ⍵}), compute_lambda_
             // body() (Executable.cc) inserts λ← after the LAST ◊, not
             // right after the header, so it went uncaught here and
             // was copied into the dump verbatim; reloading it then
             // synthesized a SECOND λ← over it, growing by one on every
             // )DUMP/)LOAD cycle. Find the same insertion point
             // compute_lambda_body() used (mirroring its own, simple,
             // not string/brace-aware ◊ scan) and strip it wherever it
             // actually is. See Bugs27 #37.
             //
             UCS_string body;
             while (t < text.ssize())
                {
                   const Unicode uni = text[t++];
                   if (uni == UNI_LF)   break;
                   body << uni;
                }

             ShapeItem sols = 0;
             for (ShapeItem j = body.size() - 1; j >= 0; --j)
                 {
                   if (Avec::is_DIAMOND(body[j]))
                      {
                        sols = j + 1;
                        while (sols < body.ssize() &&
                               body[sols] <= UNI_SPACE)   ++sols;
                        break;
                      }
                 }
             if (sols + 1 < body.ssize() &&
                 body[sols] == UNI_LAMBDA &&
                 body[sols + 1] == UNI_LEFT_ARROW)
                {
                  body.erase(sols);
                  body.erase(sols);
                }

             out << body;

             // append local variables
             //
             loop(l, ufun->local_var_count())
                 {
                   out << ";" << ufun->get_local_var(l)->get_name();
                 }
             out << UNI_R_CURLY << endl;
           }
        else
           {
             UCS_string_vector lines;
             text.to_vector(lines);
             out << "∇";
             loop(l, lines.size())
                {
                  UCS_string & line = lines[l];
                  line.remove_leading_and_trailing_whitespaces();
                  if (l)   out << " ";
                  out << line << endl;
                }

             if (ufun->get_exec_properties()[0])   out << "⍫";
             else                                  out << "∇";
             out << endl;
           }
      }
   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Symbol::get_apl_value() const
{
   Assert(value_stack.size() > 0);
   if (value_stack.back().get_NC() != NC_VARIABLE)
      Error::throw_symbol_error(get_name(), LOC);

   return Value_P(static_cast<Value *>(const_cast<cValue *>(value_stack.back().get_val_cptr())), LOC);
}
//────────────────────────────────────────────────────────────────────────────
const Cell *
Symbol::get_first_cell(Cell & cache) const
{
   Assert(value_stack.size() > 0);
   if (value_stack.back().get_NC() != NC_VARIABLE)   return 0;
   return &value_stack.back().get_val_cptr()->get_cfirst(cache);
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Symbol::get_function() const
{
   Assert(value_stack.size() > 0);
   if (value_stack.back().get_NC() & NC_FUN_OPER)
      return value_stack.back().get_function();
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Symbol::get_function(unsigned int si) const
{
   Assert(value_stack.size() > 0);
   if (value_stack[si].get_NC() & NC_FUN_OPER)
      return value_stack[si].get_function();
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
int
Symbol::get_SI_level(cFunction_P fun) const
{
   loop(v, value_stack.size())
       {
         const ShapeItem from_tos = value_stack.size() - v - 1;
         const ValueStackItem & item = value_stack[from_tos];
         if (item.get_function() == fun)
            return Workspace::SI_top()->nth_push(this, from_tos);
       }

   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
int
Symbol::get_SI_level(const cValue & val) const
{
   loop(v, value_stack.size())
       {
         const ShapeItem from_tos = value_stack.size() - v - 1;
         const ValueStackItem & vs = value_stack[from_tos];
         if (vs.get_val_cptr() == &val)
            return Workspace::SI_top()->nth_push(this, from_tos);
       }

   FIXME;
}
//────────────────────────────────────────────────────────────────────────────
SV_key
Symbol::get_SV_key() const
{
   Assert(value_stack.size() > 0);

   if (value_stack.back().get_NC() != NC_SYSTEM_VAR)   return SV_key(0);

   return value_stack.back().get_key();
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Symbol::list(ostream & out) const
{
   out << "   ";
   loop(s, name.size())   out << name[s];

   for (int s = name.size(); s < 32; ++s)   out << " ";

   if (is_erased())   out << "   ERASED";
   Assert(value_stack.size());
   switch(value_stack.back().get_NC())
      {
        case NC_INVALID:          out << "   INVALID NC";   break;
        case NC_UNUSED_USER_NAME: out << "   Unused";       break;
        case NC_LABEL:            out << "   Label";        break;
        case NC_VARIABLE:         out << "   Variable";     break;
        case NC_FUNCTION:         out << "   Function";     break;
        case NC_OPERATOR:         out << "   Operator";     break;
        default:                  out << "   !!! Error !!!";
      }

   return out << endl;
}
//────────────────────────────────────────────────────────────────────────────
bool
Symbol::M_is_F() const
{
   // this is a symbol left of an ambiguous operator (i.e. / ⌿ \ or ⍀).
   // return true if the operator shall be downgraded to a function and
   // false if it should remain an operator.

const NameClass nc = get_NC();
   if (nc & NC_values)      return true;
   if (nc & NC_functions)   return false;
   if (nc == NC_OPERATOR)
      {
        Assert(get_function());
        return get_function()->get_oper_valence() == 2;
      }
   if (nc & NC_unknown)   // user error
      {
        MORE_ERROR() << "unassigned symbol " << get_name()
                     << "when resolving the function argument "
                         "of a monadic operator";
        SYNTAX_ERROR;
      }

   CERR << "TODO: Nameclass " << nc << endl;
   FIXME;
   return false;
}
//────────────────────────────────────────────────────────────────────────────
TokenClass
Symbol::resolve_class(bool left) const
{
   Assert1(value_stack.size());

   switch(value_stack.back().get_NC())
      {
        case NC_LABEL:
        case NC_VARIABLE:
        case NC_SYSTEM_VAR:
             return left ? TC_SYMBOL : TC_VALUE;

        case NC_FUNCTION:
             {
               const int valence = value_stack.back().get_function()
                                                    ->get_fun_valence();
               if (valence == 2)   return TC_FUN2;
               if (valence == 1)   return TC_FUN1;
               return TC_FUN0;
             }

        case NC_OPERATOR:
             {
               const int valence = value_stack.back().get_function()
                                 ->get_oper_valence();
               return (valence == 2) ? TC_OPER2 : TC_OPER1;
             }

        default: return TC_SYMBOL;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::resolve_shared_variable(Token & tok) const
{
   // wait for shared variable to be ready
   //
const bool ws_to_ws = Svar_DB::is_ws_to_ws(get_SV_key());

   for (int w = 0; ; ++w)
       {
         if (Svar_DB::may_use(get_SV_key(), w))   // ready for reading
            {
              if (w)
                 {
                   Log(LOG_shared_variables)   cerr << " - OK." << endl;
                 }
              break;
            }

         if (w == 0)
            {
              Log(LOG_shared_variables)
                 {
                   CERR << "apl" << ProcessorID::get_id().proc
                                    << ": Shared variable ";
                   for (const uint32_t * varname =
                                         Svar_DB::get_svar_name(get_SV_key());
                        varname && *varname; ++varname)
                       CERR << Unicode(*varname);
                   CERR << " is blocked on use. Waiting ...";
                 }
            }
         else if (w%25 == 0)
            {
              Log(LOG_shared_variables)   cerr << ".";
            }

         usleep(10000);   // wait 10 ms
       }

const TCP_socket tcp = Svar_DB::get_DB_tcp();

   if (ws_to_ws)
      {
        // variable shared between workspaces. It is stored on APserver
        //
        READ_WSWS_VAR_c(tcp, get_SV_key());

        char * del = 0;
        char buffer[MAX_SIGNAL_CLASS_SIZE + 40000];
        const char * err_loc = 0;
        const Signal_base * response =
              Signal_base::recv_TCP(tcp, buffer, sizeof(buffer),
                                    del, 0, &err_loc);

        if (response == 0)
           {
             if (del)   delete[] del;
             CERR << "no response to signal READ_WSWS_VAR" << endl;
             VALUE_ERROR;
           }

        const string & data = response->get__WSWS_VALUE_IS__cdr_value();
        if (data.size() == 0)
           {
             delete response;
             if (del)   delete[] del;
             CERR << "no data in signal WSWS_VALUE_IS" << endl;
             VALUE_ERROR;
           }

        CDR_string cdr;
        loop(d, data.size())   cdr.push_back(data[d]);
        delete response;
        if (del)   delete[] del;

        Value_P value = CDR::from_CDR(cdr, LOC);
        if (!value)     VALUE_ERROR;

        value->check_value(LOC);
        new (&tok) Token(TOK_APL_VALUE1, value);
        return;
      }

GET_VALUE_c request(tcp, get_SV_key());

char * del = 0;
char buffer[MAX_SIGNAL_CLASS_SIZE + 40000];
const char * err_loc = 0;
const Signal_base * response =
      Signal_base::recv_TCP(tcp, buffer, sizeof(buffer), del, 0, &err_loc);

   if (response == 0)
      {
        if (del)   delete[] del;
        CERR << "TIMEOUT on signal GET_VALUE" << endl;
        VALUE_ERROR;
      }

const ErrorCode err(ErrorCode(response->get__VALUE_IS__error()));
   if (err)
      {
        Log(LOG_shared_variables)
           {
             cerr << Error::error_name(err) << " referencing "
                  << get_name() << ", detected at "
                  << response->get__VALUE_IS__error_loc() << endl;
           }

        string eloc = response->get__VALUE_IS__error_loc();
        delete response;
        if (del)   delete[] del;
        throw_apl_error(err, eloc.c_str());
      }

const string & data = response->get__VALUE_IS__cdr_value();
CDR_string cdr;
   loop(d, data.size())   cdr.push_back(data[d]);
   delete response;
   if (del)   delete[] del;

Value_P value = CDR::from_CDR(cdr, LOC);
   if (!value)     VALUE_ERROR;

   // update shared var state (AFTER sending request to peer)
   //
   Svar_DB::set_state(get_SV_key(), true, LOC);

   value->check_value(LOC);
   new (&tok) Token(TOK_APL_VALUE1, value);
}
//════════════════════════════════════════════════════════════════════════════
int
Symbol::show_owners(ostream & out, const Value & value) const
{
int count = 0;

   loop(v, value_stack.size())
       {
         const ValueStackItem & item = value_stack[v];
         switch(item.get_NC())
            {
              case NC_VARIABLE:
                   if (Value::is_or_contains(item.get_val_cptr(), &value))
                      {
                         out << "    Variable[vs=" << v << "] "
                             << get_name() << endl;
                         ++count;
                      }
                   break;

              case NC_FUNCTION:
              case NC_OPERATOR:
                   {
                     cFunction_P fun = item.get_function();
                     if (fun->is_native())   break;

                     // ufun can be 0 e.g. if this is a function argument
                     // (LO/RO) of a called defined operator and that
                     // argument is a primitive function -- a primitive
                     // simply owns no values, same as
                     // unmark_all_values() above already treats this
                     // case (Bugs28 #97: ]OWNERS used to Assert(ufun ||
                     // fun->is_native()) here instead and crash the
                     // interpreter, e.g. while ÷OP1 0 is suspended with
                     // LO←÷ still localised).
                     //
                     const Executable * ufun = fun->get_func_ufun();
                     if (ufun)
                        {
                          char cc[100];
                          SPRINTF(cc, "    VS[%lld] ", long_long(v));
                          count += ufun->show_owners(cc, out, value);
                        }
                   }
                   break;

              default: break;
            }
       }

   return count;
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::unmark_all_values() const
{
   loop(v, value_stack.size())
       {
         const ValueStackItem & item = value_stack[v];
         switch(item.get_NC())
            {
              case NC_VARIABLE:
                   item.get_val_cptr()->unmark();
                   break;

              case NC_FUNCTION:
              case NC_OPERATOR:
                   if (item.get_function()->is_native())   break;
                   {
                     // ufun can be 0 for example if F is a function argument
                     // of a defined operator and F is a primitive function
                     //
                     if (const UserFunction * ufun =
                               item.get_function()->get_func_ufun())
                        {
                          ufun->unmark_all_values();
                        }
                   }
                   break;

              default: break;
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::assign_lambda_var(Value_P value, bool clone, const char * loc)
{
   // Bugs37 #70: outside a lambda the assignment was silently ignored.
   // A suspended lambda on the SI still localizes the name (debugging).
   //
   if (value_stack_size() <= 1)
      {
        MORE_ERROR() << get_name() << " can only be assigned inside a"
                        " lambda (or while a lambda is suspended)";
        SYNTAX_ERROR;
      }

   Symbol::assign(value, clone, loc);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::assign(Value_P new_value, bool clone, const char * loc)
{
   Assert(new_value);
   Assert(value_stack.size());

   if (!new_value->is_complete())
      {
        CERR << "Incomplete value at " LOC << endl;
        new_value->print_properties(CERR, 0, false);
        VH_entry::print_history(CERR, *new_value, LOC);
        Assert(0);
      }

ValueStackItem & vs = value_stack.back();

   switch(vs.get_NC())
      {
        case NC_UNUSED_USER_NAME:
             if (clone)  new_value = CLONE_P(new_value, loc);

             vs.set_apl_value(new_value);
             if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
             return;

        case NC_LABEL:
             MORE_ERROR() << "attempt to assign a value to label "
                          << get_name() << " (line " << vs.get_label() << ")";
             SYNTAX_ERROR;   // was missing: fell through into NC_VARIABLE,
                              // performing the forbidden assignment and
                              // silently turning the label into a variable

        case NC_VARIABLE:
             if (vs.get_val_cptr() == new_value.get())   return;   // X←X

             if (clone)  new_value = CLONE_P(new_value, loc);

             vs.set_apl_value(new_value);
             if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
             return;

        case NC_SYSTEM_VAR:
             assign_shared_variable(new_value, loc);
             if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
             return;

        default: SYNTAX_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::assign_indexed(const IndexExpr & IX, Value_P B)   // A[IX;...] ← B
{
   if (IX.is_axis() && IX.values[0])   // one-dimensional index, not elided
      {
         assign_indexed(IX.values[0].get(), B);
        return;
      }

   // see comments in Value::index() above.

   value_stack.back().isolate(LOC);
Value_P Z = get_apl_value();

   // The rows of a structured variable are its (hashed) member table.
   // Writing into that table directly (e.g. S[1;1]←'q' or S[;]←0) would
   // destroy it; the only write forms are S.m←B and S['m']←B.
   //
   if (Z->is_member())
      {
        MORE_ERROR() << "A[B;C]←D: member access: the member table of "
                        "structured variable " << get_name()
                     << " cannot be assigned.\n      Use "
                     << get_name() << ".member←D or " << get_name()
                     << "['member']←D instead.";
        DOMAIN_ERROR;
      }

   if (Z->get_rank() != IX.get_rank())   // ISO p. 159
      {
        MORE_ERROR() << "A[B]←C: expecting " << Z->get_rank()
                     << " semicolon-separated indices (⍴⍴A); got "
                     << IX.get_rank();
        RANK_ERROR;
      }

   // B must either be a scalar (and is then scalar extended to the size
   // of the updated area, or else have the shape of the concatenated index
   // items. For example:
   //
   //  X:   X1    ; X2    ; X3
   //  ⍴B:  b1 b2   b3 b4   b5 b6
   //
   // ISO 13751 §10.2.15: a one-element B is scalar-extended just like a
   // true scalar B, not shape-matched against the selected area -- the
   // broadcast-capable assignment loop below (incr_B = ec_B==1 ? 0 : 1)
   // already handles this correctly for ANY B with one element, but the
   // shape-validation block skipped only for B->is_scalar() (rank 0),
   // wrongly rejecting e.g. M[1;]←,7 (a 1-element VECTOR) with a RANK
   // ERROR before ever reaching that loop. See Bugs27 #46.
   //
   if (!B->is_scalar() && B->element_count() != 1)
      {
        // Bugs28 #100(s) investigated and reverted: squeezing B's own
        // length-1 axes before comparing against IX1 looks, in
        // isolation, like it wrongly tolerates a mis-shaped B (e.g.
        // M[1;1 2]←2 1⍴7 8) -- but ZZZ0_Standard_10x.tc's own
        // ISO-standard-sourced worked example (Y[2 1;;1]←N12121, where
        // N12121 has shape 1 2 1 2 1) relies on exactly this squeeze to
        // match a 2x2 selected area, so it is genuine, tested,
        // standard-mandated leniency, not a bug. Left as originally
        // designed (Bugs27 #46).
        //
        // remove dimensions with len 1 from the shapes of X and B...
        // if we see an empty Xn then we return.
        //
        Shape B1;   // shape B with axes of length 1 removed
        loop(b, B->get_rank())
           {
             const ShapeItem sh_b = B->get_shape_item(b);
             if (sh_b != 1)   B1.add_shape_item(sh_b);
           }

        Shape IX1;
        for (ShapeItem ix = IX.get_rank() - 1; ix >= 0; --ix)
            {
              const cValue * ix_val = IX.values[ix].get();
              if (ix_val)   // non-elided index
                 {
                   // Bugs28 #100(f): an empty index means the selected
                   // area itself is empty (0 elements), regardless of
                   // the other axes. A silent return here (unconditional
                   // no-op) wrongly accepted ANY B, including a
                   // non-conforming, non-empty one, instead of LENGTH
                   // ERROR (the same check M[1;1 2]←1 2 3 already gets
                   // via B1 != IX1 below) -- but B with 0 elements of
                   // its own (e.g. Z[1;⍬]←⍬) is genuinely conforming and
                   // must remain a no-op.
                   //
                   if (ix_val->element_count() == 0)
                      {
                        if (B->element_count() != 0)
                           {
                             MORE_ERROR() << "A[B]←C: an index is empty, so"
                                             " the selected area is empty,"
                                             " but C has "
                                          << B->element_count()
                                          << " element(s) (expecting 0)";
                             LENGTH_ERROR;
                           }
                        return;
                      }
                   loop(xx, ix_val->get_rank())
                      {
                        const ShapeItem sxx = ix_val->get_shape_item(xx);
                        if (sxx != 1)   IX1.add_shape_item(sxx);
                      }
                 }
               else         // elided index: add corresponding B dimenssion
                 {
                   if (ix >= Z->get_rank())   RANK_ERROR;
                   const ShapeItem sbx =
                         Z->get_shape_item(Z->get_rank() - ix - 1);
                   // Bugs28 #100(f): same fix as the non-elided branch
                   // above, for an elided axis whose own (whole) length
                   // happens to be 0.
                   //
                   if (sbx == 0)
                      {
                        if (B->element_count() != 0)
                           {
                             MORE_ERROR() << "A[B]←C: an elided index's axis"
                                             " has length 0, so the selected"
                                             " area is empty, but C has "
                                          << B->element_count()
                                          << " element(s) (expecting 0)";
                             LENGTH_ERROR;
                           }
                        return;
                      }
                   if (sbx != 1)   IX1.add_shape_item(sbx);
                 }
            }

        if (B1 != IX1)
           {
             if (B1.get_rank() != IX1.get_rank())
                {
                  MORE_ERROR() << "A[B]←C: expecting ⍴⍴C = " << IX1.get_rank()
                               << " (⍴⍴ of the selected area, after removing"
                                  " length-1 axes); ⍴⍴C is " << B1.get_rank();
                  RANK_ERROR;
                }
             else
                {
                  MORE_ERROR() << "A[B]←C: expecting ⍴C = " << IX1
                               << " (⍴ of the selected area, after removing"
                                  " length-1 axes); ⍴C is " << B1;
                  LENGTH_ERROR;
                }
           }
      }

   // per-axis index bounds check, same as for A[B] (see cValue::index()).
   // A valid multi-index (checked here) always produces an in-range flat
   // offset below, so the offset itself no longer needs checking.
   //
   IX.check_index_range(Z->get_shape());

MultiIndexIterator mult(Z->get_shape(), IX);

const ShapeItem ec_B = B->element_count();
const int incr_B = (ec_B == 1) ? 0 : 1;
ShapeItem idxB = 0;

   while (mult.has_more())
      {
        const ShapeItem offset_Z = mult++;
        Assert(offset_Z >= 0 && offset_Z < Z->element_count());
        Cell cache;
        const Cell & cB = B->get_cravel(idxB, cache);
        Z->assign_cell(offset_Z, cB, LOC);
        idxB += incr_B;
     }

   if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::assign_indexed(const cValue * X, Value_P B)   // A[X] ← B
{
   // this function is only called for A[X}←B when X is one-dimensional,
   // i.e. an index with no semicolons. If X contains semicolons, then
   // Symbol::assign_indexed(IndexExpr IX, ...) is called instead.
   // 
const APL_Integer qio = Workspace::get_IO();

   value_stack.back().isolate(LOC);
Value_P Z = get_apl_value();  // the current APL value of this Symbol

   if (Z->is_member())
      {
        // X is 0 for the elided-index form Z[]←B, which (unlike for an
        // ordinary array, see #1's ⎕FC[]←B/⎕PS[]←B/⎕SYL[]←B fix) has no
        // sensible meaning for a member-indexed structured value -- there
        // is no member name to assign through. Z[] as a plain *reference*
        // already RANK_ERRORs (Symbol::resolve()); match that here rather
        // than dereferencing the null X.
        if (X == 0)   RANK_ERROR;

        // Z is indexed with a member name, e.g. Z['member'] ← 42
        const UCS_string name(*X);
        Cell * data = Z->get_member_data(name);
        if (data)   // existing member
           {
             if (data->is_pointer_cell() &&
                 data->get_pointer_value()->is_member())
                {
                  MORE_ERROR()
                     << "A[B]←C: member access: cannot override non-leaf"
                        " member " << name << " of variable " << get_name()
                     << ".\n      )ERASE or ⎕EX that member first.";
                  DOMAIN_ERROR;
                }
             data->release(LOC);   // release old content
           }
        else        // new member
           {
             data = Z->get_new_member(name);
           }
        data->init_from_value(B.get(), *Z, LOC);
        return;
      }

const ShapeItem max_idx = Z->element_count();

   // A[]←B (fully elided index, no semicolons at all -- unlike A[;]/
   // A[;;] etc., which go through the sibling assign_indexed(IndexExpr&)
   // overload instead): "select everything", valid for ANY rank of A,
   // not just a vector -- the Z->get_rank()!=1 check just below is only
   // for a genuine single (non-elided) flat index X, which does require
   // a vector. A scalar (or one-element) B is broadcast to every cell of
   // A as usual, but a genuinely multi-item B must match ⍴A exactly --
   // A[] selects the WHOLE array, so (unlike an ordinary selective
   // specification's "same shape when ones are ignored" leniency) there
   // is no partial/squeezed match here, real APL2: A←4⍴4 ⋄ A[]←2 2⍴9
   // requires ⍴A = 2 2, not just 4 elements in some other shape.
   //
   if (!X)
      {
        // but not for a scalar A, like the reference S[] (bracket index
        // of a rank-0 array, ISO 13751) and ⎕PP[]←B (Bugs34 #45)
        //
        if (Z->is_scalar())
           {
             MORE_ERROR() << "A[]←B: A is a scalar";
             RANK_ERROR;
           }

        const ShapeItem ec_B = B->element_count();
        if (ec_B == 1)   // scalar (or one-element) B: broadcast
           {
             // re-fetch every iteration (via incr_B == 0), not once
             // outside the loop: matches the established pattern
             // elsewhere in this function (and the non-scalar branch
             // just below) for a cache-backed get_cravel()/get_cfirst()
             // result, rather than assuming it is safe to reuse the
             // same Cell& across many assign_cell() calls.
             //
             Cell cache;
             loop(a, max_idx)   Z->assign_cell(a, B->get_cfirst(cache), LOC);
             if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
             return;
           }

        if (B->get_shape() != Z->get_shape())
           {
             if (B->get_rank() != Z->get_rank())
                {
                  MORE_ERROR() << "A[]←C: expecting ⍴⍴C = " << Z->get_rank()
                               << " (⍴⍴A); ⍴⍴C is " << B->get_rank();
                  RANK_ERROR;
                }
             MORE_ERROR() << "A[]←C: expecting ⍴C = " << Z->get_shape()
                          << " (⍴A); ⍴C is " << B->get_shape();
             LENGTH_ERROR;
           }

        Cell cache;
        loop(a, max_idx)   Z->assign_cell(a, B->get_cravel(a, cache), LOC);
        if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
        return;
      }

   if (X              &&     // X exists,      and
       X->is_scalar() &&     // X is a scalar, and
       B->is_scalar() &&     // B is a scalar, and
       Z->get_rank() == 1)   // Z is a vector
      {
        const APL_Integer idx = X->get_near_int(0) - qio;
        if (idx >= 0 && idx < max_idx)   // idx is a valid index of Z
           {
             Cell cache;
             Z->assign_cell(idx, B->get_cfirst(cache), LOC);
             return;
           }
      }

   if (Z->get_rank() != 1)
      {
        MORE_ERROR() << "A[B]←C: A is not a vector (single-index A[B]←C"
                        " only applies to vector A); ⍴⍴A is "
                     << Z->get_rank();
        RANK_ERROR;
      }

const ShapeItem ec_B = B->element_count();
const ShapeItem ec_X = X->element_count();
const int incr_B = (ec_B == 1) ? 0 : 1;   // maybe scalar extend B
ShapeItem idxX = 0;
ShapeItem idxB = 0;

   // Bugs28 #100(s): only the ELEMENT COUNT of B was checked here
   // (against 1, for scalar extension, or ec_X); B's own shape was
   // never required to actually be a vector, so e.g. A[1 2]←2 1⍴1 2
   // (a 2 1 matrix, 2 elements) silently succeeded instead of RANK
   // ERROR -- unlike the sibling multi-axis Symbol::assign_indexed
   // (IndexExpr, above), which already requires (after this same fix)
   // C's shape to exactly match the selected area's, not just its
   // element count.
   //
   // C must have the shape of the index B (= the shape of A[B]), not
   // always that of a vector: V[2 2⍴I]←2 2⍴C is valid and V[2 2⍴I]←4⍴C
   // is not (Bugs34 #6).
   //
   if (ec_B != 1)
      {
        if (ec_B != ec_X)
           {
             MORE_ERROR() << "A[B]←C: expecting ⍴C to be 1 or ⍴B ("
                          << X->get_shape() << "); ⍴C is "
                          << B->get_shape();
             LENGTH_ERROR;
           }
        if (B->get_shape() != X->get_shape())
           {
             MORE_ERROR() << "A[B]←C: expecting ⍴C to be ⍴B ("
                          << X->get_shape() << "); ⍴C is "
                          << B->get_shape();
             RANK_ERROR;
           }
      }

   loop(x, ec_X)
      {
        const ShapeItem this_idxX = idxX;
        const ShapeItem idx = X->get_near_int(idxX++) - qio;
        if (idx < 0 || idx >= max_idx)
           {
             MORE_ERROR() << "A[B]←C: B[" << (this_idxX + qio) << "] = "
                          << (idx + qio)
                          << " is not a valid index for A (expecting "
                          << qio << "≤index<" << (max_idx + qio) << ")"
                          << ArgCheck::index_io0_note(idx, max_idx);
             INDEX_ERROR;
           }
        Cell cache;
        const Cell & cB = B->get_cravel(idxB, cache);
        Z->assign_cell(idx, cB, LOC);

        idxB += incr_B;
      }

   if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
}
//════════════════════════════════════════════════════════════════════════════
bool
Symbol::assign_named_lambda(cFunction_P lambda, const char * loc)
{
ValueStackItem & vs = value_stack.back();
   switch(vs.get_NC())
      {
        case NC_FUNCTION:
        case NC_OPERATOR:
             {
               // a lambda with the same name already exists. Remove it.
               //
               cFunction_P old_fun = vs.get_function();
               Assert(old_fun);
               if (!old_fun->is_lambda())   SYNTAX_ERROR;
               const UserFunction * old_ufun = old_fun->get_func_ufun();
               Assert(old_ufun);
               for (StateIndicator * si = Workspace::SI_top();
                    si; si = si->get_parent())
                   {
                     if (si->uses_function(old_ufun))
                        {
                          MORE_ERROR() << "function " << get_name()
                                       << " is suspended or used";
                          return true;
                        }
                   }

               const_cast<UserFunction *>(vs.get_function()->get_func_ufun())
                         ->decrement_refcount(LOC);
             }

             /* fall through */

        case NC_UNUSED_USER_NAME:
             {
               const UserFunction * ufun = lambda->get_func_ufun();
               Assert(ufun);
               UTF8_string creator;
               creator << ufun->get_name() << "←λ";
               
 
               vs.set_function(lambda);
               const_cast<UserFunction *>(ufun)
                         ->increment_refcount(LOC, creator.c_str());
               if (monitor_callback)   monitor_callback(*this, SEV_ASSIGNED);
               return false;
             }

        default: SYNTAX_ERROR;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
operator <<(ostream & out, const Symbol & sym)
{
   return sym.print(out);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::assign_shared_variable(Value_P new_value, const char * loc)
{
   // put new_value into a CDR string
   //
CDR_string cdr;
   CDR::to_CDR(cdr, new_value.get());
   if (cdr.size() > MAX_SVAR_SIZE)   LIMIT_ERROR_SVAR;

const std::string data(charP(cdr.get_items()), cdr.size());

   // wait for shared variable to be ready
   //
const bool ws_to_ws = Svar_DB::is_ws_to_ws(get_SV_key());

   for (int w = 0; ; ++w)
       {
         if (Svar_DB::may_set(get_SV_key(), w))   // ready for writing
            {
              if (w)
                 {
                   Log(LOG_shared_variables)
                      CERR << " - OK." << endl;
                 }
              break;
            }

         if (w == 0)
            {
              Log(LOG_shared_variables)
                 {
                   CERR << "Shared variable ";
                   for (const uint32_t * varname =
                                         Svar_DB::get_svar_name(get_SV_key());
                        varname && *varname; ++varname)
                       CERR << Unicode(*varname);
                   CERR << " is blocked on set. Waiting ...";
                 }
            }
         else if (w%25 == 0)
            {
              Log(LOG_shared_variables)   CERR << ".";
            }

         usleep(10000);   // wait 10 ms
       }

const TCP_socket tcp = Svar_DB::get_DB_tcp();

   if (ws_to_ws)
      {
        // variable shared between workspaces. It is stored on APserver
        //

        // update shared var state (BEFORE sending request to peer)
        //
        ASSIGN_WSWS_VAR_c(tcp, get_SV_key(), data);
        return;
      }

   // update shared var state (BEFORE sending request to peer)
   //
   Svar_DB::set_state(get_SV_key(), false, loc);

   ASSIGN_VALUE_c request(tcp, get_SV_key(), data);

char * del = 0;
char buffer[2*MAX_SIGNAL_CLASS_SIZE];
const char * err_loc = 0;
const Signal_base * response =
      Signal_base::recv_TCP(tcp, buffer, sizeof(buffer), del, 0, &err_loc);

   if (response == 0)
      {
        cerr << "TIMEOUT on signal ASSIGN_VALUE" << endl;
        if (del)   delete[] del;
        VALUE_ERROR;
      }

const ErrorCode ec = ErrorCode(response->get__SVAR_ASSIGNED__error());
   if (ec)
      {
        Log(LOG_shared_variables)
           {
             Error e(ec, response->get__SVAR_ASSIGNED__error_loc().c_str());
             cerr << Error::error_name(ec) << " assigning "
                  << get_name() << ", detected at "
                  << response->get__SVAR_ASSIGNED__error_loc()
                  << endl;
           }

        delete response;
        if (del)   delete[] del;
        throw_apl_error(ec, loc);
      }

   delete response;
   if (del)   delete[] del;
}
//════════════════════════════════════════════════════════════════════════════
void
Symbol::clear_vs()
{
   while (value_stack.size() > 1)
      {
        pop();
      }

ValueStackItem & tos = value_stack[0];

   switch(tos.get_NC())
      {
        case NC_LABEL:
             // should not happen since stack height == 1"
             FIXME;
             break;

        case NC_VARIABLE:
             tos.reset_apl_value();   // delete the value owned by this var
             tos.set_NC(NC_UNUSED_USER_NAME);
             break;

        case NC_UNUSED_USER_NAME:
             break;

        case NC_FUNCTION:
        case NC_OPERATOR:
             const_cast<Function *>(tos.get_function())->destroy();
             tos.set_NC(NC_UNUSED_USER_NAME);
             break;

        default: break;
      }
}
//────────────────────────────────────────────────────────────────────────────
int
Symbol::expunge()
{
   if (value_stack.size() == 0)   return 1;   // empty stack

ValueStackItem & vs = value_stack.back();

   if (vs.get_NC() == NC_VARIABLE)
      {
        vs.reset_apl_value();   // delete the value owned by this var
      }
   else if (vs.get_NC() &  NC_FUN_OPER)
      {
        if (vs.get_function()->is_native())
           {
             // do not delete native functions
           }
        else if (vs.get_function()->is_lambda())
           {
             const UserFunction * ufun = vs.get_function()->get_func_ufun();
             Assert(ufun);

             // same Bugs27 #58 refusal as the non-lambda branch below:
             // a lambda genuinely suspended on the )SI must not be
             // expunged out from under its own suspended frame either
             // (Bugs28 #17 -- this branch was missing the guard, so
             // ⎕EX of a suspended lambda silently "succeeded" while
             // leaving a dangling Function* on the )SI).
             //
             const Executable * exec = ufun;
             if (Workspace::oldest_exec(exec))            return 0;
             if (Workspace::is_bound_as_operand(ufun))    return 0;

             const_cast<UserFunction *>(ufun)->decrement_refcount(LOC);
           }
        else
           {
             const UserFunction * ufun = vs.get_function()->get_func_ufun();
             const Executable * exec = ufun;
             StateIndicator * oexec = Workspace::oldest_exec(exec);
             if (oexec)
                {
                  // ufun is genuinely still on the SI stack (case 1:
                  // ufun IS the executable of some SI level, i.e. it is
                  // pendant/suspended there right now) -- refuse, the
                  // same way )ERASE already does (SymbolTable.cc's
                  // erase_one_symbol(), via Workspace::is_called()).
                  // ⎕EX (this function's other caller, QuadFunction.cc's
                  // Quad_EX::expunge()) used to have no such check at
                  // all, silently removing the name binding regardless
                  // (⎕EX 'G4' returning 1/success for a suspended G4,
                  // while )ERASE G4 correctly refused). Note this is
                  // deliberately narrower than Workspace::is_called():
                  // that helper's "case 3" (ufun merely referenced
                  // somewhere in an ancestor frame's already-compiled
                  // body, not actually executing) is far broader and,
                  // tried first here, wrongly refused ⎕EX for a
                  // function that had already returned but was still
                  // textually mentioned in the calling function's own
                  // source (testcases/Quad_CR10.tc: ⊣⎕EX 'Fun' after
                  // Fun already returned, called from a line above that
                  // also, textually, calls Fun). See Bugs27 #58.
                  //
                  return 0;
                }

             // Bugs28 #16: ufun may not itself be executing, yet still be
             // bound as the LO/RO operand of some OTHER, suspended
             // defined operator -- that frame holds a raw Function* to
             // ufun for as long as it stays suspended, invisible to the
             // oldest_exec() check above (which only ever compares
             // against si->get_executable(), i.e. the function actually
             // running/pending at each SI level, never a value merely
             // bound to one of its local symbols).
             //
             if (Workspace::is_bound_as_operand(ufun))   return 0;
             // Not deleted right away: the tokens of the immediate-
             // execution statement currently in flight (e.g. every
             // application form of (⎕EX'G')G ... covered by Bugs28 #15)
             // may still hold a raw Function* to this same ufun -- the
             // check above only catches ufun genuinely suspended on the
             // )SI, not merely referenced by a not-yet-fully-reduced
             // statement. Queue it on the existing (already correctly
             // wired into the stale-value checker's mark/sweep via
             // unmark_all_values(), and into )CHECK's own reporting via
             // show_owners()) expunged-functions mechanism instead of a
             // brand-new one -- actually deleted later, once the )SI is
             // provably clear, by )CHECK (Workspace::cleanup_expunged(),
             // Cmd_DIAG.cc). This mechanism already existed but was
             // never actually wired to a real delete site.
             //
             Workspace::add_expunged_function(ufun);
           }
        vs.set_NC(NC_UNUSED_USER_NAME);
      }

   vs.clear();
   call_monitor_callback(SEV_ERASED);
   return 1;
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::get_attributes(int mode, Value & Z) const
{
const ValueStackItem & vs = value_stack.back();
int has_result = 0;   // no result

   switch(vs.get_NC())
      {
        case NC_LABEL:
        case NC_VARIABLE:
             has_result = 1;
             break;

        case NC_FUNCTION:
        case NC_OPERATOR:
             vs.get_function()->get_attributes(mode, Z);
             return;

        default: break;
      }

   switch(mode)
      {
        case 1: // valences
                Z.next_ravel_Int(has_result);
                Z.next_ravel_0();
                Z.next_ravel_0();
                break;

        case 2: // creation time (mode_len == 7, see QuadFunction.cc's
                // mode_vec)
                loop(j, 7)   Z.next_ravel_0();
                break;

        case 3: // execution properties (mode_len == 4)
                loop(j, 4)   Z.next_ravel_0();
                break;

        case 4: {
                  const cValue * val = get_val_cptr();
                  const CDR_type cdr_type = val->get_CDR_type();
                  const ShapeItem brutto = val->total_CDR_size_brutto(cdr_type);
                  const ShapeItem data = val->CDR_data_size(cdr_type);

                  Z.next_ravel_Int(brutto);
                  Z.next_ravel_Int(data);
                }
                break;

        default:  Assert(0 && "bad mode");
      }
}
//────────────────────────────────────────────────────────────────────────────
int
Symbol::get_exec_ufun_depth(const UserFunction * ufun)
{
const Function * fun = ufun;
const int sym_stack_size = value_stack_size();

   loop(s, sym_stack_size)
      {
        const ValueStackItem & vsi = value_stack[s];
        if (vsi.get_NC() != NC_FUNCTION &&
            vsi.get_NC() != NC_OPERATOR)   continue;
        if (fun != vsi.get_function())       continue;

       // found at level s
       //
       return s;
      }

   // not found: return -1
   return -1;
}
//────────────────────────────────────────────────────────────────────────────
cFunction_P
Symbol::get_function()
{
const ValueStackItem & vs = value_stack.back();

   if (vs.get_NC() & NC_FUN_OPER)   return vs.get_function();

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Symbol::get_var_value()
{
   if (value_stack.size() && get_NC() == NC_VARIABLE)
      {
        return Value_P(value_stack.back().get_val_wptr(), LOC);
      }

   return Value_P();
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::pop()
{
   if (value_stack.size() == 0)
      {
        CERR << "Symbol is: '" << get_name() << "' at " << LOC << endl;
        FIXME;
      }

const ValueStackItem & vs = value_stack.back();

   if (get_NC() == NC_VARIABLE)
      {
        Log(LOG_SYMBOL_push_pop)
           {
             const cValue * ret = get_val_cptr();
             CERR << "-pop-value " << name
                  << " flags " << ret->get_flags() << " ";
             if (value_stack.size() == 0)   CERR << " (last)";
             CERR << " addr " << voidP(ret) << endl;
           }

        value_stack.pop_back();
        if (monitor_callback)   monitor_callback(*this, SEV_POPED);
      }
   else
      {
        Log(LOG_SYMBOL_push_pop)
           {
             CERR << "-pop " << name
                  << " name_class " << vs.get_NC() << " ";
             if (value_stack.size() == 0)   CERR << " (last)";
             CERR << endl;
           }

        value_stack.pop_back();
        if (monitor_callback)   monitor_callback(*this, SEV_POPED);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::pop_local()
{
   // a function FIXED into a localized name (e.g. by ⎕FX inside the
   // function that localizes it: its header names this symbol) is owned by
   // the local stack entry: free it like expunge() does when the function
   // returns, or it leaks on every call (Bugs35 #47). NOT a function merely
   // bound to the name, and not a lambda (reference counted). Not in pop()
   // itself: e.g. )LOAD pops a binding only to push the same function again.
   //
   if (value_stack.size() > 1)
      {
        const ValueStackItem & vs = value_stack.back();
        if ((vs.get_NC() & NC_FUN_OPER) && vs.get_function() &&
            !vs.get_function()->is_native())
           {
             const UserFunction * ufun = vs.get_function()->get_func_ufun();
             if (ufun && !ufun->is_lambda() &&
                 ufun->get_header().FUN() == this)
                Workspace::add_expunged_function(ufun);
           }
      }

   pop();
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Symbol::print(ostream & out) const
{
   return out << name;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Symbol::print_verbose(ostream & out) const
{
   out << "Symbol ";
   print(out) << " at " << voidP(this) << endl;

   loop(v, value_stack.size())
       {
         out << "[" << v << "] ";
         const ValueStackItem & item = value_stack[v];
         switch(item.get_NC())
            {
              case NC_INVALID:
                   out << "---INVALID---" << endl;
                   break;

              case NC_UNUSED_USER_NAME:
                   out << "Unused user defined name" << endl;
                   break;

              case NC_LABEL:
                   out << "Label line " << item.get_label() << endl;
                   break;

              case NC_VARIABLE:
                   {
                      const cValue * val = item.get_val_cptr();
                      out << "Variable at " << voidP(val) << endl;
                      val->print_properties(out, 8, false);
                      out << endl;
                   }
                   break;

              case NC_FUNCTION:
              case NC_OPERATOR:
                   {
                     cFunction_P fun = item.get_function();
                     Assert(fun);

                     fun->print_properties(out, 4);
                     out << "    ⎕NC:            " << item.get_NC() << endl
                         << "    addr:           " << voidP(fun)    << endl;

                     out << endl;
                   }
                   break;

              default: break;
            }
       }

   return out;
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::push()
{
   Log(LOG_SYMBOL_push_pop)
      {
        CERR << "+push " << name;
        if (value_stack.size() == 0)   CERR << " (initial)";
        CERR << endl;
      }

   value_stack.push_back(ValueStackItem());
   if (monitor_callback)   monitor_callback(*this, SEV_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::push_function(cFunction_P function)
{
   Log(LOG_SYMBOL_push_pop)
      {
        CERR << "+push_function " << name << " " << voidP(function);
        if (value_stack.size() == 0)   CERR << " (initial)";
        CERR << endl;
      }

ValueStackItem vs;
   vs.set_function(function);
   value_stack.push_back(vs);
   if (monitor_callback)   monitor_callback(*this, SEV_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::push_label(Function_Line label)
{
   Log(LOG_SYMBOL_push_pop)
      {
        CERR << "+push_label " << name;
        if (value_stack.size() == 0)   CERR << " (initial)";
        CERR << endl;
      }

   value_stack.push_back(ValueStackItem(label));
   if (monitor_callback)   monitor_callback(*this, SEV_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::push_value(Value_P value)
{
ValueStackItem vs;
   value_stack.push_back(vs);
   if (monitor_callback)   monitor_callback(*this, SEV_PUSHED);
   assign(value, true, LOC);

   Log(LOG_SYMBOL_push_pop)
      {
        CERR << "+push-value " << name << " flags ";
        print_flags(CERR, get_var_value()->get_flags()) << " ";
        if (value_stack.size() == 0)   CERR << " (initial)";
        CERR << " addr " << voidP(get_var_value().get()) << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::resolve_left(Token & tok, const Token_loc & tl) const
{
   Log(LOG_SYMBOL_resolve)
      CERR << "resolve_left() symbol " << get_name() << endl; 

   Assert1(value_stack.size());

const ValueStackItem & vs = value_stack.back();
   switch(vs.get_NC())
      {
        case NC_UNUSED_USER_NAME:
             return;   // leave symbol as is

        case NC_LABEL:
             SYNTAX_ERROR;   // assignment to (read-only) label

        case NC_VARIABLE:
             return;   // leave symbol as is

        case NC_FUNCTION:
        case NC_OPERATOR:
             if (vs.get_function()->is_lambda())
                {
                  // lambda re-assign, e.g. SYM←{ ... }
                  //
                  return;
                }
             else
                {
                  Token tok_fun = vs.get_function()->get_token();
                  tok.move_from(tok_fun, LOC);
                  return;
                }

        case NC_SYSTEM_VAR:
             return;   // leave symbol as is

        default:
             CERR << "Symbol is '" << get_name() << "' at " << LOC << endl;
             SYNTAX_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
Token
Symbol::resolve_lv(const char * loc)
{
   Log(LOG_SYMBOL_resolve)
      CERR << "resolve_lv() symbol " << get_name() << endl; 

   Assert(value_stack.size());

   if (value_stack.back().get_NC() == NC_VARIABLE)
      {
        value_stack.back().isolate_deep(loc);
        Value_P Z = get_var_value();
        Value_P refs = Z->get_cellrefs(loc);
        refs->set_lval_whole_symbol(this);
        return Token(TOK_APL_VALUE1, refs);
      }

   MORE_ERROR() << "Symbol '" << get_name()
                << "' has changed type from variable to name class "
                << value_stack.back().get_NC()
                << "\nwhile executing an assignment\n";
   throw_apl_error(E_LEFT_SYNTAX_ERROR, loc);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::resolve_right(Token & tok, const Token_loc & tl) const
{
   Log(LOG_SYMBOL_resolve)
      CERR << "resolve_right() symbol " << get_name() << endl; 

   Assert1(value_stack.size());

const ValueStackItem & vs = value_stack.back();
   switch(vs.get_NC())
      {
        case NC_UNUSED_USER_NAME:
             if (const StateIndicator * si = Workspace::SI_top())
                Error::set_failed_token(si->get_executable(), tl.get_PC());
             Error::throw_symbol_error(get_name(), LOC);

        case NC_LABEL:
             {
               // TOK_APL_VALUE5 rather than the ordinary TOK_APL_VALUE1:
               // carries "this integer came directly from a label" forward
               // with the value itself, so any later consumer (⌹, →, ...)
               // can tell a label-derived branch target apart from a
               // literal/computed one without having to re-derive its
               // provenance from source position bookkeeping.
               Value_P value = IntScalar(vs.get_label(), LOC);
               Token t(TOK_APL_VALUE5, value);
               tok.move_from(t, LOC);
             }
             return;

        case NC_VARIABLE:
             // if we resolve a variable. the value is considered grouped.
             {
               Token tok_val(TOK_APL_VALUE1, CLONE_P(get_apl_value(), LOC));
               tok.move_from(tok_val, LOC);
             }
             return;

        case NC_FUNCTION:
        case NC_OPERATOR:
             {
               Token tok_function(vs.get_function()->get_token());
               tok.move_from(tok_function, LOC);
             }
             return;

        case NC_SYSTEM_VAR:
             resolve_shared_variable(tok);
             return;

        default:
             CERR << "Symbol is '" << get_name() << "' at " << LOC << endl;
             SYNTAX_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::set_NC(NameClass nc)
{
ValueStackItem & vs = value_stack.back();

   // the name class can only be set for unused names.
   //
   if (vs.get_NC() == NC_UNUSED_USER_NAME)
      {
        vs.set_NC(nc);
        return;
      }

   DEFN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::set_NC(NameClass nc, cFunction_P fun)
{
ValueStackItem & vs = value_stack.back();

const bool can_set = (vs.get_NC() == NC_FUNCTION) ||
                     (vs.get_NC() == NC_OPERATOR) ||
                     (vs.get_NC() == NC_UNUSED_USER_NAME);

   Assert(nc == NC_FUNCTION || nc == NC_OPERATOR || nc == NC_UNUSED_USER_NAME);

   if (!can_set)   DEFN_ERROR;

   if (fun)   vs.set_function(fun);
   else       vs.clear_function();
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::set_SV_key(SV_key key)
{
   value_stack.back().set_key(key);
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::share_var(SV_key key)
{
const ValueStackItem & vs = value_stack.back();

   if (vs.get_NC() == NC_UNUSED_USER_NAME)   // new shared variable
      {
        set_SV_key(key);
        return;
      }

   if (vs.get_NC() == NC_VARIABLE)           // existing variable
      {
        // remember old value
        //
        Value_P old_value = get_apl_value();

        // change name class and store AP number
        //
        set_SV_key(key);

        // assign old value to shared variable
        assign(old_value, true, LOC);

        return;
      }

   DEFN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
SV_Coupling
Symbol::unshare_var()
{
   if (value_stack.size() == 0)   return NO_COUPLING;

ValueStackItem & vs = value_stack.back();
   if (vs.get_NC() != NC_SYSTEM_VAR)   return NO_COUPLING;

const SV_key key = get_SV_key();
const SV_Coupling old_coupling = Svar_DB::get_coupling(key);

   Svar_DB::retract_var(key);

   set_SV_key(0);
   vs.set_NC(NC_UNUSED_USER_NAME);

   return old_coupling;
}
//════════════════════════════════════════════════════════════════════════════
void
Symbol::write_OUT(FILE * out, uint64_t & seq) const
{
   // )OUT records have a fixed format:
   // We use buffers that are a little larger than that.
   //
enum { LAST_REC_LEN =  1,   //    1 byte last record indicator (' ' or 'X')
       CONTENT_REST = 71,   // + 71 byte content  
                            // ────
       CONTENT_LEN  = 72,   // = 72 byte content
       SEQ_LEN      =  8,   // +  8 byte sequence nunber
       CRLF_LEN     =  2,   // +  2 byte CR/LF
                            // ────
       RECORD_SIZE  = 82,   // = 82 byte
       SLACK        =  8,   // +  8 byte slack
                            // ────
       BUFSIZE      = 90    // = 90 byte buffer size
     };
UCS_string data;

   switch(value_stack[0].get_NC())
      {
        case NC_VARIABLE:
             {
               data << UNI_A;

               // a system variable like ⎕TS computes its value in
               // get_apl_value(); value_stack[0] is only a placeholder
               // for it (Bugs37 #75). A user variable is written with its
               // global value.
               //
               const Value_P sys_val = get_Id() == ID_USER_SYMBOL
                                     ? Value_P() : get_apl_value();
               const cValue & val = !!sys_val ? *sys_val
                                  : *value_stack[0].get_val_cptr();
               Token tok = Quad_TF::tf2_var(get_name(), val);
               const UCS_string ucs(*tok.get_apl_val());
               data << ucs;
             }
             break;

        case NC_FUNCTION:
        case NC_OPERATOR:
             {
               // defined functions and operators are preceeded by
               // a timestamp record
               //
               cFunction_P fun = value_stack[0].get_function();

               // lambdas cannot be represented in transfer-format (see
               // Quad_TF::tf2_fun_ucs(), Bugs27 #52): )OUT enumerates
               // every object in the workspace, so letting that DOMAIN
               // ERROR propagate here aborts the whole )OUT half-way
               // through, leaving a truncated .atf with everything
               // written before this symbol but nothing after it (Bugs28
               // #75). Skip this one symbol with a CERR warning instead,
               // the way )SAVE warns about (but does not fail on) a
               // dirty SI, and keep writing the rest of the workspace.
               //
               if (fun->is_lambda())
                  {
                    CERR << "Warning: )OUT: '" << get_name() << "' is a "
                            "lambda (dfn) and cannot be written to a "
                            "transfer-format (.atf) file -- skipped."
                         << endl;
                    return;
                  }

               // same for a locked function (its text is not available,
               // see Quad_TF::tf2_fun_ucs(), Bugs28 #92). Bugs37 #19.
               //
               if (fun->get_exec_properties()[0])
                  {
                    CERR << "Warning: )OUT: '" << get_name() << "' is "
                            "locked and cannot be written to a "
                            "transfer-format (.atf) file -- skipped."
                         << endl;
                    return;
                  }

               const YMDhmsu ymdhmsu(fun->get_creation_time());
               char ts_buf[BUFSIZE];
               char * const seq_pos = ts_buf + CONTENT_LEN;
               SPRINTF(ts_buf, "*(%d %d %d %d %d %d %d)",
                       ymdhmsu.year, ymdhmsu.month, ymdhmsu.day,
                       ymdhmsu.hour, ymdhmsu.minute, ymdhmsu.second,
                       ymdhmsu.micro/1000);

               for (char * cp = ts_buf + strlen(ts_buf); cp < seq_pos; )
                   {
                     *cp++ = ' ';
                   }
                if (seq >= 100000000ULL)   // SEQ_LEN == 8 decimal digits
                   {
                     MORE_ERROR() << ")OUT: transfer-file sequence number "
                        "exceeded the fixed " << int(SEQ_LEN) << "-digit "
                        "sequence field (more than 10⁸ records written to "
                        "this transfer file)";
                     DOMAIN_ERROR;
                   }
                snprintf(ts_buf + CONTENT_LEN, BUFSIZE - CONTENT_LEN,
                         "%8.8lld\r\n", long_long(seq++));
               NULL_TERMINATE(ts_buf)
               if (fwrite(ts_buf, 1, RECORD_SIZE, out) != RECORD_SIZE)
                  CERR << "fwrite error during )SAVE" << endl;

               // write function record(s)
               //
               data << UNI_F << (get_name()) << UNI_SPACE;
               Quad_TF::tf2_fun_ucs(data, get_name(), *fun);
             }
             break;

        default: return;
      }

char buffer[BUFSIZE];
   for (ShapeItem u = 0; u < data.ssize() ;)
      {
        const ShapeItem rest = data.size() - u;
        if (rest <= CONTENT_REST)   buffer[0] = 'X';   // last record
        else                        buffer[0] = ' ';   // more records
        loop(uu, CONTENT_REST)
           {
             unsigned char cc = ' ';
             if (u < data.ssize()) cc = Avec::unicode_to_cp(data[u++]);
             buffer[1 + uu] = cc;
           }

        if (seq >= 100000000ULL)   // SEQ_LEN == 8 decimal digits
           {
             MORE_ERROR() << ")OUT: transfer-file sequence number "
                "exceeded the fixed " << int(SEQ_LEN) << "-digit sequence "
                "field (more than 10⁸ records written to this transfer "
                "file)";
             DOMAIN_ERROR;
           }
        snprintf(buffer + CONTENT_LEN, BUFSIZE - CONTENT_LEN,
                 "%8.8lld\r\n", long_long(seq++));
        NULL_TERMINATE(buffer)
        fwrite(buffer, 1, RECORD_SIZE, out);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Symbol::vector_assignment(std::vector<Symbol *> & symbols, Value_P values)
{
   if (values->get_rank() > 1)   RANK_ERROR;
   if (!values->is_scalar() &&
       size_t(values->element_count()) != symbols.size())   LENGTH_ERROR;

   // The assignment is atomic: if one of the names rejects its value
   // (e.g. ⎕IO←99 in (X ⎕IO Y)←1 99 2) or anything else throws (e.g. WS
   // FULL), then the names assigned before it get their old values back
   // (Blake McBride, Bugs32 #19). Old values were valid, so restoring them
   // cannot fail; a name without an old value becomes undefined again.
   //
const int incr = values->is_scalar() ? 0 : 1;
std::vector<Value_P> old_values;
ShapeItem idxV = 0;
   try
      {
        loop(s, symbols.size())
           {
             Symbol * sym = symbols[symbols.size() - s - 1];
             const NameClass nc = sym->get_NC();
             if (nc == NC_VARIABLE || nc == NC_SYSTEM_VAR)
                old_values.push_back(sym->get_apl_value());
             else
                old_values.push_back(Value_P());   // no old value

             Cell cache;
             const Cell & cV = values->get_cravel(idxV, cache);
             if (Value_P v = cV.try_pointer_value())
                {
                  sym->assign(v, true, LOC);
                }
             else
                {
                  Value_P val(LOC);
                  val->next_ravel_Cell(cV);
                  val->check_value(LOC);
                  sym->assign(val, true, LOC);
                }

             idxV += incr;   // scalar extend values
           }
      }
   catch (...)
      {
        // restore the names assigned so far (the last one in old_values
        // threw, i.e. was not assigned), in reverse order
        //
        for (ShapeItem s = ShapeItem(old_values.size()) - 2; s >= 0; --s)
            {
              Symbol * sym = symbols[symbols.size() - s - 1];
              if (old_values[s])   sym->assign(old_values[s], true, LOC);
              else                 sym->expunge();
            }
        throw;
      }
}
//════════════════════════════════════════════════════════════════════════════
