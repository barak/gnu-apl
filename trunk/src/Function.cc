/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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
#include <sys/mman.h>

#include "Error.hh"
#include "Function.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "Parser.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "Value.hh"

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
Token
Function::eval_() const
{
   return phrase_error("");
}
//----------------------------------------------------------------------------
Token
Function::eval_B(Value_P B) const
{
   return phrase_error("B");
}
//----------------------------------------------------------------------------
Token
Function::eval_AB(Value_P A, Value_P B) const
{
   return phrase_error("AB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LB(Token & LO, Value_P B) const
{
   return phrase_error("LB");
}
//----------------------------------------------------------------------------
Token
Function::eval_XB(Value_P X, Value_P B) const
{
   return phrase_error("XB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALB(Value_P A, Token & LO, Value_P B) const
{
   return phrase_error("ALB");
}
//----------------------------------------------------------------------------
Token
Function::eval_AXB(Value_P A, Value_P X, Value_P B) const
{
   return phrase_error("AXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LRB(Token & LO, Token & RO, Value_P B) const
{
   return phrase_error("LRB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALRB(Value_P A, Token & LO, Token & RO, Value_P B) const
{
   return phrase_error("ALRB");
}
//----------------------------------------------------------------------------
Token
Function::eval_ALXB(Value_P A, Token & LO, Value_P X, Value_P B) const
{
   return phrase_error("ALXB");
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
Function::eval_LXB(Token & LO, Value_P X, Value_P B) const
{
   return phrase_error("LXB");
}
//----------------------------------------------------------------------------
Token
Function::eval_LRXB(Token & LO, Token & RO,
                     Value_P X, Value_P B) const
{
   return phrase_error("LRXB");
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
ostream &
operator << (ostream & out, const Function & fun)
{
   fun.print(out);
   return out;
}
//----------------------------------------------------------------------------

