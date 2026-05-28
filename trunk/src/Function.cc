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
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "Value.hh"

//============================================================================
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
sAxis
FunctionGroup::value_to_subfun(const Value & A_or_X) const
{
   if (A_or_X.is_int_scalar())   // function number
      {
        // we do not check (here) if the number is valid. The caller will.
        //
        return A_or_X.get_cfirst().get_int_value();   // but possibly invalid
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
void
FunctionGroup::bad_subfun_name_ERROR(const UCS_string sub_name) const
{
   MORE_ERROR() << sub_name << " is not a valid subfunction of " << group_name
                << ".\nSee: " << group_name << " '' "
                   "or: " << group_name << " ⍬ for a list of valid names.";
   SYNTAX_ERROR;
}
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
int
FunctionGroup::compare_function_name(const char * const & name,
                                     const function_info * const & info,
                                     const void *)
{
   return strcmp(name, info->function_name);
}
//----------------------------------------------------------------------------
int
FunctionGroup::compare_function_axis(const uAxis & key,
                                     const function_info * const & info,
                                     const void *)
{
   return key - info->axis;
}
//----------------------------------------------------------------------------
const FunctionGroup::function_info *
FunctionGroup::get_info_by_name(const char * name) const
{
const FunctionGroup::function_info* const * ret =
      Heapsort<const FunctionGroup::function_info *>
      ::search<const char *>(name, sorted_by_name, compare_function_name, 0);

   if (ret == 0)   return reinterpret_cast<const function_info *>(0);
   return *ret;
}
//----------------------------------------------------------------------------
const FunctionGroup::function_info *
FunctionGroup::get_info_by_axis(uAxis axis) const
{
const FunctionGroup::function_info* const * ret =
      Heapsort<const FunctionGroup::function_info *>
      ::search<const uAxis>(axis, sorted_by_axis, compare_function_axis, 0);
                     
   if (ret == 0)   return reinterpret_cast<const function_info *>(0);
   return *reinterpret_cast<const function_info * const *>(ret);
}
//----------------------------------------------------------------------------
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
//============================================================================
Token
Function::eval_() const
{
   return phrase_error("");
}
//----------------------------------------------------------------------------
Token
Function::eval_AB(Value_P A, Value_P B) const
{
   return phrase_error("AB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALB(Value_P A, Token & LO, Value_P B) const
{
   return phrase_error("ALB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const
{
   return phrase_error("ALRB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALRXB(Value_P A, Token & LO, Token & RO,
                     Value_P X, Value_P B) const
{
   return phrase_error("ALRXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const
{
   return phrase_error("ALXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   return phrase_error("AXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_B(Value_P B) const
{
   return phrase_error("B");
}
//----------------------------------------------------------------------------
Token
Function::eval_LB(Token & LO, Value_P B) const
{
   return phrase_error("LB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LRB(Token & LO, Token & RO, Value_P B) const
{
   return phrase_error("LRB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LRXB(Token & LO, Token & RO,
                     Value_P X, Value_P B) const
{
   return phrase_error("LRXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LXB(Token & LO, Value_P X, Value_P B) const
{
   return phrase_error("LXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_XB(Value_P X, Value_P B) const
{
   return phrase_error("XB");
}
//----------------------------------------------------------------------------
Token
Function::eval_fill_AB(Value_P A, Value_P B) const
{
  MORE_ERROR() << "Function " << get_name() 
                     << " has no dyadic fill function";

  DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Function::eval_fill_B(Value_P B) const
{
  MORE_ERROR() << "Function " << get_name() 
                     << " has no monadic fill function";

  DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
Token
Function::eval_identity_fun(Value_P B, sAxis axis) const
{
  MORE_ERROR() << "Function " << get_name() 
                     << " has no identity function";
  DOMAIN_ERROR;
}
//----------------------------------------------------------------------------
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
                  const YMDhmsu created(get_creation_time());
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
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
   VALENCE_ERROR;
}
//============================================================================
ostream &
operator << (ostream & out, const Function & fun)
{
   fun.print(out);
   return out;
}
//============================================================================

