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
#include <fcntl.h>
#include <sys/stat.h>

#include "Error.hh"
#include "Function.hh"
#include "Heapsort.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "Parser.hh"
#include "Prefix.hh"
#include "Workspace.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "Value.hh"

//════════════════════════════════════════════════════════════════════════════
void
FunctionGroup::init_function_group(const FunctionGroup::function_info * unsorted,
                                   size_t count, const char * grp_name)
{
   // only called from true FunctionGroups (⎕CR, ⎕FIO, ...)
   Assert(unsorted);

   group_name = grp_name;
   subfun_count = count;

   {
   loop(c, count)
      {
        const FunctionGroup::function_info * info = unsorted + c;
        sorted_by_name.push_back(info);
        sorted_by_axis.push_back(info);
        max_function_name_length = max(max_function_name_length,
                                       strlen(info->function_name));
      }

   Heapsort<const FunctionGroup::function_info *>::
            sort(sorted_by_name, greater_function_name, 0);
   Heapsort<const FunctionGroup::function_info *>::
            sort(sorted_by_axis, greater_function_axis, 0);
   }

   // check that sorted_by_name is sorted ascendingly
   //
   loop(c, subfun_count - 1)
       {
         const function_info * info_0 = sorted_by_name[c];
         const function_info * info_1 = sorted_by_name[c + 1];

         // CERR << "NAME " << c << ": " << info_0->function_name << endl;
         Assert(strcmp(info_0->function_name, info_1->function_name) < 0);
       }

   // check that sorted_by_axis is sorted ascendingly
   //
   loop(c, subfun_count - 1)
       {
         const function_info * info_0 = sorted_by_axis[c];
         const function_info * info_1 = sorted_by_axis[c + 1];
         // CERR << "AXIS " << c << ": " << int(info_0->axis) << endl;

         Assert(info_0->axis < info_1->axis);
       }

   // check that all names can be found
   //
   loop(sub, subfun_count)
       {
         const function_info & info = unsorted[sub];

         {
           const char * key = info.function_name;
           const function_info * found = get_info_by_name(key);
           Assert_fatal(found);
           Assert_fatal(!strcmp(key, found->function_name));
         }

         {
           const sAxis key = info.axis;
           const function_info * found = get_info_by_axis(key);
           Assert_fatal(found);
           Assert_fatal(found->axis == key);
         }
       }
}
//────────────────────────────────────────────────────────────────────────────
sAxis
FunctionGroup::subfun_to_axis(const UCS_string & subfun_name) const
{
   if (subfun_count)
      {
        const UTF8_string name_utf(subfun_name);
        if (const function_info * info = get_info_by_name(name_utf.c_str()))
           {
             return info->axis;   // found
           }
      }

   return -1;   // not found (or no real FunctionGroup)
}
//────────────────────────────────────────────────────────────────────────────
sAxis
FunctionGroup::value_to_subfun(const cValue & A_or_X) const
{
   if (A_or_X.is_int_scalar())   // function number
      {
        // we do not check (here) if the number is valid. The caller will.
        // But a number that does not fit into sAxis would be truncated
        // to some other (possibly valid) number (Bugs37 #47).
        //
        // near-int, like is_int_scalar() above (Bugs37 #56)
        const APL_Integer number = A_or_X.get_near_int(0);
        if (number != sAxis(number))
           {
             MORE_ERROR() << "invalid subfunction number " << number;
             DOMAIN_ERROR;
           }
        return number;   // but possibly invalid
      }

   if (A_or_X.is_char_string())   // function name
      {
        const UCS_string name(A_or_X);
        const sAxis axis = subfun_to_axis(name);
        if (axis >= 0)   return axis;   // valid axis

        const Fun_signature signature = Prefix::get_current_signature();
        const char * AX = signature & SIG_X ? "X" : "A";
        MORE_ERROR() << get_signature_string(signature)
         << ": invalid subfunction name " << AX
         << " (= '" << name << "').";

        DOMAIN_ERROR;
      }

   if (A_or_X.get_rank() > 1)   RANK_ERROR;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
FunctionGroup::list_functions(ostream & out) const
{
   out << "\n"
       << group_name << " is a function group. It is comprised of "
                        "the following (sub-)functions:\n"
          "\n"
       << get_legend(LET_FUN_PREFIX)
       << "    " << group_name << " ''   ⍝ display this list\n"
       << "    " << group_name << " ⍬    ⍝ display syntax alternatives for "
       << group_name << "\n\n";


   loop(c, subfun_count)
      {
        const function_info & info = *sorted_by_axis[c];
        print_fun_syntax(out, info);
      }

   out << get_legend(LET_FUN_SUFFIX);

   out << "\nThe functions of " << group_name
       << " can be called with one of several syntax alternatives.\n"
          "The syntax alternatives for " << group_name
       << " can be displayed with:\n\n";

   COUT << "      " << group_name << " ⍬   ⍝ display the "
        << " syntax alternatives for " << group_name << "\n\n";

   return Token();
}
//────────────────────────────────────────────────────────────────────────────
Token
FunctionGroup::list_mappings(ostream & out) const
{
   out << "\n"
          "The syntax alternatives for the functions of "
       << group_name << " are:\n"
         "\n";

   out << get_legend(LET_MAP_PREFIX);

   loop(c, subfun_count)
      {
        const function_info & info = *sorted_by_name[c];
        print_map_syntax(out, info);
      }
   out << get_legend(LET_MAP_SUFFIX);

   out  << "\nTo display a brief description of the functions:\n\n";
   COUT << "      " << group_name << " ⍬   ⍝ display function descriptions\n\n";

   return Token();
}
//────────────────────────────────────────────────────────────────────────────
void
FunctionGroup::bad_subfun_name_ERROR(const UCS_string sub_name) const
{
   MORE_ERROR() << sub_name << " is not a valid subfunction of " << group_name
                << ".\nSee: " << group_name << " '' "
                   "or: " << group_name << " ⍬ for a list of valid names.";

   // this is thrown while the *new* statement is still being parsed,
   // before it has an Executable/StateIndicator of its own -- the
   // SYNTAX_ERROR macro (-> throw_apl_error() -> update_error_info())
   // would instead describe whatever statement Workspace::SI_top()
   // currently happens to point to, which after a prior suspended
   // top-level error is a leftover, unrelated statement (Blake
   // McBride, Bugs24 #2: wrong statement/carets shown, then printed
   // again via the stale "already printed" latch it carries along).
   // throw_parse_error() correctly does not consult SI_top() at all.
   Error::throw_parse_error(E_SYNTAX_ERROR, LOC, LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
FunctionGroup::bad_subfun_number_ERROR(int number) const
{
const Fun_signature signature = Prefix::get_current_signature();
const char * AX = signature & SIG_X ? "X" : "A";

   MORE_ERROR() << get_signature_string(signature)
                << ": invalid subfunction number " << AX << " (= " << number
                << ").\nSee: " << group_name << " '' or: " << group_name
                << " ⍬ for a list of valid numbers.";
   DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
int
FunctionGroup::compare_function_name(const char * const & name,
                                     const function_info * const & info,
                                     const void *)
{
   return strcmp(name, info->function_name);
}
//────────────────────────────────────────────────────────────────────────────
int
FunctionGroup::compare_function_axis(const uAxis & key,
                                     const function_info * const & info,
                                     const void *)
{
   return key - info->axis;
}
//────────────────────────────────────────────────────────────────────────────
const FunctionGroup::function_info *
FunctionGroup::get_info_by_name(const char * name) const
{
const FunctionGroup::function_info* const * ret =
      Heapsort<const FunctionGroup::function_info *>
      ::search<const char *>(name, sorted_by_name, compare_function_name, 0);

   if (ret == 0)   return reinterpret_cast<const function_info *>(0);
   return *ret;
}
//────────────────────────────────────────────────────────────────────────────
const FunctionGroup::function_info *
FunctionGroup::get_info_by_axis(uAxis axis) const
{
const FunctionGroup::function_info* const * ret =
      Heapsort<const FunctionGroup::function_info *>
      ::search<const uAxis>(axis, sorted_by_axis, compare_function_axis, 0);
                     
   if (ret == 0)   return reinterpret_cast<const function_info *>(0);
   return *reinterpret_cast<const function_info * const *>(ret);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
FunctionGroup::get_signature_string(Fun_signature sig) const
{
UCS_string ret;

   if (sig & SIG_Z)        ret << "Z←";
   if (sig & SIG_A)        ret << "A ";
   if (sig & SIG_LORO)   // operator
      {
                           ret << "(LO " << group_name;
        if (sig & SIG_X)   ret << "[X]";
        if (sig & SIG_RO)  ret << "RO";
         ret << ")";
      }
   else                   // plain function
      {
                           ret << group_name;
        if (sig & SIG_X)   ret << "[X]";
      }
   if (sig & SIG_B)        ret << " B";
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
Token
Function::eval_() const
{
   return phrase_error("");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_AB(cValue_R A, cValue_R B) const
{
   return phrase_error("AB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_ALB(cValue_R A, Token & LO, cValue_R B) const
{
   return phrase_error("ALB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_ALRB(cValue_R A, Token & LO, Token & RO, cValue_R B) const
{
   return phrase_error("ALRB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_ALRXB(cValue_R A, Token & LO, Token & RO,
                     cValue_R X, cValue_R B) const
{
   return phrase_error("ALRXB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const
{
   return phrase_error("ALXB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   return phrase_error("AXB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_B(cValue_R B) const
{
   return phrase_error("B");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_LB(Token & LO, cValue_R B) const
{
   return phrase_error("LB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_LRB(Token & LO, Token & RO, cValue_R B) const
{
   return phrase_error("LRB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_LRXB(Token & LO, Token & RO,
                     cValue_R X, cValue_R B) const
{
   return phrase_error("LRXB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_LXB(Token & LO, cValue_R X, cValue_R B) const
{
   return phrase_error("LXB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_XB(cValue_R X, cValue_R B) const
{
   return phrase_error("XB");
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_fill_AB(cValue_R A, cValue_R B) const
{
  MORE_ERROR() << "Function " << get_name() 
                     << " has no dyadic fill function";

  DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_fill_B(cValue_R B) const
{
  MORE_ERROR() << "Function " << get_name()
                     << " has no monadic fill function";

  DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_rank_fill_AB(cValue_R A, cValue_R B) const
{
   return eval_AB(A, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_rank_fill_B(cValue_R B) const
{
   return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::eval_identity_fun(cValue_R B, sAxis axis) const
{
  MORE_ERROR() << "Function " << get_name()
                     << " has no identity function, needed to reduce along"
                        " axis " << axis << " of B (⍴B is " << B.get_shape()
                     << "), whose length there is 0";

  // Neither the LRM ("an attempt to reduce an empty argument with a
  // defined function generates a DOMAIN ERROR") nor ISO 13751's Table 5
  // ("all others -- Signal domain-error") get the error class right
  // here: DOMAIN_ERROR is for bad ravel content, not a bad shape (this
  // codebase's own convention: LENGTH_ERROR when some other length at
  // the same rank would work, RANK_ERROR when no rank would). Reducing
  // this same B along this same axis works fine for any length other
  // than 0 -- e.g. LO/1 2 3 or LO/,5 -- so this is squarely a length
  // problem, not a content problem.
  //
  LENGTH_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Function::get_attributes(int mode, Value & Z) const
{
   switch(mode)
      {
        case 1: // valences
                Z.next_ravel_Int(has_result() ? 1 : 0);
                Z.next_ravel_Int(get_fun_valence());
                Z.next_ravel_Int(get_oper_valence());
                return;

        case 2: // creation time (7⍴0 for system functions)
                {
                  // in local time (⎕TZ) like ⎕TS and the ∇ display, and
                  // 7⍴0 rather than the epoch for a system function
                  // (Bugs35 #69)
                  //
                  if (get_creation_time() == 0)
                     {
                       loop(z, 7)   Z.next_ravel_Int(0);
                       return;
                     }
                  const APL_time_us offset =
                        Workspace::get_v_Quad_TZ().get_offset();
                  const YMDhmsu created(get_creation_time() + 1000000*offset);
                  Z.next_ravel_Int(created.year);
                  Z.next_ravel_Int(created.month);
                  Z.next_ravel_Int(created.day);
                  Z.next_ravel_Int(created.hour);
                  Z.next_ravel_Int(created.minute);
                  Z.next_ravel_Int(created.second);
                  Z.next_ravel_Int(created.micro/1000);
                }
                return;

        case 3: // execution properties
                Z.next_ravel_Int(get_exec_properties()[0]);
                Z.next_ravel_Int(get_exec_properties()[1]);
                Z.next_ravel_Int(get_exec_properties()[2]);
                Z.next_ravel_Int(get_exec_properties()[3]);
                return;

        case 4: // 4 ⎕DR for functions is always 0 0
                Z.next_ravel_0();
                Z.next_ravel_0();
                return;
      }

   Assert(0 && "Not reached");
}
//────────────────────────────────────────────────────────────────────────────
Fun_signature
Function::get_signature() const
{
int sig = SIG_FUN;
   if (has_result())   sig |= SIG_Z;
   if (has_axis())     sig |= SIG_X;

   if (get_oper_valence() == 2)   sig |= SIG_RO;
   if (get_oper_valence() >= 1)   sig |= SIG_LO;

   if (get_fun_valence() == 2)    sig |= SIG_A;
   if (get_fun_valence() >= 1)    sig |= SIG_B;

   return Fun_signature(sig);
}
//────────────────────────────────────────────────────────────────────────────
Token
Function::phrase_error(const char * pattern) const
{
   Log(LOG_verbose_error)   CERR << get_name() << "::" << __FUNCTION__
        << "() called (overloaded variant not yet implemented?)" << endl;

int signature = 0;   // the signature for pattern

   for (; *pattern; ++pattern)
       {
         switch(*pattern)
            {
              case 'A': signature |= SIG_A;    break;
              case 'L': signature |= SIG_LO;   break;
              case 'R': signature |= SIG_RO;   break;
              case 'X': signature |= SIG_X;    break;
              case 'B': signature |= SIG_B;    break;
              default:  FIXME;
            }
       }

UCS_string & more = MORE_ERROR();
   more << "Invalid phrase '";
   if (signature & SIG_A)   more << "A";
   if (signature & SIG_LO)   more << " (L ";
   more << get_name();
   if (signature & SIG_X)   more << "[X]";
   if (signature & SIG_RO)   more << " R)";
   if (signature & SIG_B)   more << " B";

   more << "'. The phrase may be valid in general, but not for function "
        << get_name();

   // apl2lrm.txt "Conditions for Axis Specification" (p.45): axis brackets
   // are always syntactically legal, but if the function/operator is not
   // one that supports axis specification (i.e. this phrase's own missing
   // signature bit is SIG_X, not e.g. a genuine arity mismatch), the LRM
   // requires AXIS ERROR rather than VALENCE ERROR.
   //
   if (signature & SIG_X)   AXIS_ERROR;
   VALENCE_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
ostream &
operator << (ostream & out, const Function & fun)
{
   fun.print(out);
   return out;
}
//════════════════════════════════════════════════════════════════════════════

