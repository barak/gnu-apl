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

#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>   // for ⎕WA
#include <sys/time.h>

// FreeBSD 8.x lacks utmpx.h
#ifdef HAVE_UTMPX_H
# include <utmpx.h>           // for ⎕UL
#endif

#include "Bif_F12_FORMAT.hh"
#include "CDR.hh"
#include "CharCell.hh"
#include "Command.hh"
#include "Common.hh"
#include "FloatCell.hh"
#include "IndexExpr.hh"
#include "IntCell.hh"
#include "LineInput.hh"
#include "Output.hh"
#include "Parallel.hh"
#include "PointerCell.hh"
#include "Prefix.hh"
#include "ProcessorID.hh"
#include "StateIndicator.hh"
#include "SystemVariable.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Value.hh"
#include "Workspace.hh"

UCS_string Quad_QUOTE::prompt;

ShapeItem Quad_SYL::si_depth_limit = 0;
ShapeItem Quad_SYL::value_count_limit = 0;
ShapeItem Quad_SYL::ravel_count_limit = 0;
ShapeItem Quad_SYL::print_length_limit = 0;

Unicode Quad_AV::qav[MAX_AV];

//=============================================================================
void
SystemVariable::assign(Value_P value, bool clone, const char * loc)
{
   CERR << "SystemVariable::assign() not (yet) implemented for "
        << get_Id() << endl;
   FIXME;
}
//-----------------------------------------------------------------------------
void
SystemVariable::assign_indexed(Value_P X, Value_P value)
{
   CERR << "SystemVariable::assign_indexed() not (yet) implemented for "
        << get_Id() << endl;
   FIXME;
}
//-----------------------------------------------------------------------------
ostream &
SystemVariable::print(ostream & out) const
{
   return out << get_Id();
}
//-----------------------------------------------------------------------------
void
SystemVariable::get_attributes(int mode, Cell * dest) const
{
   switch(mode)
      {
        case 1: // valences (always 1 0 0 for variables)
                new (dest + 0) IntCell(1);
                new (dest + 1) IntCell(0);
                new (dest + 2) IntCell(0);
                break;

        case 2: // creation time (always 7⍴0 for variables)
                new (dest + 0) IntCell(0);
                new (dest + 1) IntCell(0);
                new (dest + 2) IntCell(0);
                new (dest + 3) IntCell(0);
                new (dest + 4) IntCell(0);
                new (dest + 5) IntCell(0);
                new (dest + 6) IntCell(0);
                break;

        case 3: // execution properties (always 4⍴0 for variables)
                new (dest + 0) IntCell(0);
                new (dest + 1) IntCell(0);
                new (dest + 2) IntCell(0);
                new (dest + 3) IntCell(0);
                break;

        case 4: {
                  Value_P val = get_apl_value();
                  const CDR_type cdr_type = val->get_CDR_type();
                  const int brutto = val->total_size_brutto(cdr_type);
                  const int data = val->data_size(cdr_type);

                  new (dest + 0) IntCell(brutto);
                  new (dest + 1) IntCell(data);
                }
                break;

        default:  Assert(0 && "bad mode");
      }

}
//=============================================================================
Quad_AV::Quad_AV()
   : RO_SystemVariable(ID_Quad_AV)
{
   Assert1(MAX_AV == 256);
Value_P AV(MAX_AV, LOC);
   loop(cti, MAX_AV)
       {
         const int av_pos = Avec::get_av_pos(CHT_Index(cti));
         const Unicode uni = Avec::unicode(CHT_Index(cti));

         qav[av_pos] = uni;   // remember unicode for indexed_at()
         new (&AV->get_ravel(av_pos))  CharCell(uni);
       }
   AV->check_value(LOC);

   Symbol::assign(AV, false, LOC);
}
//-----------------------------------------------------------------------------
Unicode
Quad_AV::indexed_at(uint32_t pos)
{
   if (pos < MAX_AV)   return qav[pos];
   return UNI_AV_MAX;
}
//=============================================================================
Quad_AI::Quad_AI()
   : RO_SystemVariable(ID_Quad_AI),
     session_start(now()),
     user_wait(0)
{
   Symbol::assign(get_apl_value(), false, LOC);
}
//-----------------------------------------------------------------------------
Value_P
Quad_AI::get_apl_value() const
{
const int total_ms = (now() - session_start)/1000;
const int user_ms  = user_wait/1000;

Value_P ret(5, LOC);
   new (ret->next_ravel())   IntCell(ProcessorID::get_own_ID());
   new (ret->next_ravel())   IntCell(total_ms - user_ms);
   new (ret->next_ravel())   IntCell(total_ms);
   new (ret->next_ravel())   IntCell(user_ms);
   new (ret->next_ravel())   IntCell(Command::get_APL_expression_count());

   ret->check_value(LOC);
   return ret;
}
//=============================================================================
Quad_ARG::Quad_ARG()
   : RO_SystemVariable(ID_Quad_ARG)
{
}
//-----------------------------------------------------------------------------
Value_P
Quad_ARG::get_apl_value() const
{
const int argc = uprefs.expanded_argv.size();

Value_P Z(argc, LOC);
   loop(a, argc)
      {
        const char * arg = uprefs.expanded_argv[a];
        UTF8_string utf(arg);
        UCS_string ucs(utf);
        Value_P sub(ucs, LOC);
        new (Z->next_ravel())   PointerCell(sub.get(), Z.getref());
      }

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_CT::Quad_CT()
   : SystemVariable(ID_Quad_CT)
{
   Symbol::assign(FloatScalar(DEFAULT_Quad_CT, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_CT::assign(Value_P value, bool clone, const char * loc)
{
   if (!value->is_scalar_or_len1_vector())
      {
        if (value->get_rank() > 1)   RANK_ERROR;
        else                         LENGTH_ERROR;
      }

const Cell & cell = value->get_ravel(0);
   if (!cell.is_numeric())             DOMAIN_ERROR;
   if (cell.get_imag_value() != 0.0)   DOMAIN_ERROR;

APL_Float val = cell.get_real_value();
   if (val < 0.0)     DOMAIN_ERROR;

   // APL2 "discourages" the use of ⎕CT > 1E¯9.
   // We round down to MAX_Quad_CT (= 1E¯9) instead.
   //
   if (val > MAX_Quad_CT)
      {
        Symbol::assign(FloatScalar(MAX_Quad_CT, LOC), false, LOC);
      }
   else
      {
        Symbol::assign(value, clone, LOC);
      }
}
//=============================================================================
Value_P
Quad_EM::get_apl_value() const
{
const Error * err = 0;

   for (StateIndicator * si = Workspace::SI_top(); si; si = si->get_parent())
       {
         if (StateIndicator::get_error(si).get_error_code() != E_NO_ERROR)
            {
              err = &StateIndicator::get_error(si);
              break;
            }
         if (si->get_parse_mode() == PM_FUNCTION)   break;
       }

   if (err == 0)   // no SI entry with non-zero error code
      {
        // return 3 0⍴' '
        //
        Shape sh(ShapeItem(3), ShapeItem(0));
        Value_P Z(sh, LOC);
        Z->set_proto_Spc();
        Z->check_value(LOC);
        return Z;
      }

PrintBuffer pb;
   pb.append_ucs(UCS_string(UTF8_string(err->get_error_line_1())));
   pb.append_ucs(UCS_string(UTF8_string(err->get_error_line_2())));
   pb.append_ucs(err->get_error_line_3());
   pb.pad_to_spaces();   // replace pad char by spaces
Value_P Z(pb, LOC);
   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Value_P
Quad_ET::get_apl_value() const
{
Value_P Z(2, LOC);
ErrorCode ec = E_NO_ERROR;

   for (StateIndicator * si = Workspace::SI_top(); si; si = si->get_parent())
       {
         ec = StateIndicator::get_error(si).get_error_code();
         if (ec != E_NO_ERROR)
            {
              new (Z->next_ravel()) IntCell(Error::error_major(ec));
              new (Z->next_ravel()) IntCell(Error::error_minor(ec));
              goto done;
            }

         if (si->get_parse_mode() == PM_FUNCTION)   break;
       }

   new (Z->next_ravel()) IntCell(Error::error_major(E_NO_ERROR));
   new (Z->next_ravel()) IntCell(Error::error_minor(E_NO_ERROR));

done:
   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_FC::Quad_FC() : SystemVariable(ID_Quad_FC)
{
Value_P QFC(6, LOC);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_FULLSTOP);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_COMMA);
   new (QFC->next_ravel()) CharCell(UNI_STAR_OPERATOR);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_0);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_UNDERSCORE);
   new (QFC->next_ravel()) CharCell(UNI_OVERBAR);
   QFC->check_value(LOC);

   Symbol::assign(QFC, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_FC::push()
{
   Symbol::push();

Value_P QFC(6, LOC);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_FULLSTOP);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_COMMA);
   new (QFC->next_ravel()) CharCell(UNI_STAR_OPERATOR);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_0);
   new (QFC->next_ravel()) CharCell(UNI_ASCII_UNDERSCORE);
   new (QFC->next_ravel()) CharCell(UNI_OVERBAR);
   QFC->check_value(LOC);

   Symbol::assign(QFC, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_FC::assign(Value_P value, bool clone, const char * loc)
{
   if (!value->is_scalar_or_vector())   RANK_ERROR;

ShapeItem value_len = value->element_count();
   if (value_len > 6)   value_len = 6;

   loop(c, value_len)
       if (!value->get_ravel(c).is_character_cell())   DOMAIN_ERROR;

   // new value is correct. 
   //
Unicode fc[6] = { UNI_ASCII_FULLSTOP, UNI_ASCII_COMMA,      UNI_STAR_OPERATOR,
                  UNI_ASCII_0,        UNI_ASCII_UNDERSCORE, UNI_OVERBAR };
                  
   loop(c, 6)   if (c < value_len)
         fc[c] = value->get_ravel(c).get_char_value();
                  
   // 0123456789,. are forbidden for ⎕FC[4 + ⎕IO]
   //
   if (Bif_F12_FORMAT::is_control_char(fc[4]))
      fc[4] = UNI_ASCII_SPACE;

UCS_string ucs(fc, 6);
Value_P new_val(ucs, LOC);
   Symbol::assign(new_val, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_FC::assign_indexed(IndexExpr & IX, Value_P value)
{
   if (IX.value_count() != 1)   INDEX_ERROR;

   // at this point we have a one dimensional index. It it were non-empty,
   // then assign_indexed(Value) would have been called instead. Therefore
   // IX must be an elided index (like in ⎕FC[] ← value)
   //
   Assert1(!IX.values[0]);
   assign(value, true, LOC);   // ⎕FC[]←value
}
//-----------------------------------------------------------------------------
void
Quad_FC::assign_indexed(Value_P X, Value_P value)
{
   // we don't do scalar extension but require indices to match the value.
   //
ShapeItem ec = X->element_count();
   if (ec != value->element_count())   INDEX_ERROR;

   // ignore extra values.
   //
   if (ec > 6)   ec = 6;

const APL_Integer qio = Workspace::get_IO();
Unicode fc[6];
   {
     Value_P old = get_apl_value();
     loop(e, 6)   fc[e] = old->get_ravel(e).get_char_value();
   }

   loop(e, ec)
      {
        const APL_Integer idx = X->get_ravel(e).get_near_int() - qio;
        if (idx < 0)   continue;
        if (idx > 5)   continue;

        fc[idx] = value->get_ravel(e).get_char_value();
      }

   // 0123456789,. are forbidden for ⎕FC[4 + ⎕IO]
   //
   if (Bif_F12_FORMAT::is_control_char(fc[4]))
      fc[4] = UNI_ASCII_SPACE;

UCS_string ucs(fc, 6);
Value_P new_val(ucs, LOC);
   Symbol::assign(new_val, false, LOC);
}
//=============================================================================
Quad_IO::Quad_IO()
   : SystemVariable(ID_Quad_IO)
{
   Symbol::assign(IntScalar(1, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_IO::assign(Value_P value, bool clone, const char * loc)
{
   if (!value->is_scalar_or_len1_vector())
      {
        if (value->get_rank() > 1)   RANK_ERROR;
        else                         LENGTH_ERROR;
      }

   if (value->get_ravel(0).get_near_bool())
      Symbol::assign(IntScalar(1, LOC), false, LOC);
   else
      Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//=============================================================================
Quad_L::Quad_L()
 : NL_SystemVariable(ID_Quad_L)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_L::assign(Value_P value, bool clone, const char * loc)
{
StateIndicator * si = Workspace::SI_top_fun();
   if (si == 0)   return;

   // ignore assignments if error was SYNTAX ERROR or VALUE ERROR
   //
   if (StateIndicator::get_error(si).is_syntax_or_value_error())   return;

   si->set_L(value);
}
//-----------------------------------------------------------------------------
Value_P
Quad_L::get_apl_value() const
{
StateIndicator * si = Workspace::SI_top_error();
   if (si)
      {
        Value_P ret = si->get_L();
        if (!!ret)   return  ret;
      }

   VALUE_ERROR;
}
//=============================================================================
Quad_LC::Quad_LC()
   : RO_SystemVariable(ID_Quad_LC)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
Value_P
Quad_LC::get_apl_value() const
{

   // count how many function elements Quad-LC has...
   //
int len = 0;
   for (StateIndicator * si = Workspace::SI_top();
        si; si = si->get_parent())
       {
         if (si->get_parse_mode() == PM_FUNCTION)   ++len;
       }

Value_P Z(len, LOC);

   for (StateIndicator * si = Workspace::SI_top();
        si; si = si->get_parent())
       {
         if (si->get_parse_mode() == PM_FUNCTION)
            new (Z->next_ravel())   IntCell(si->get_line());
       }

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_LX::Quad_LX()
   : NL_SystemVariable(ID_Quad_LX)
{
   Symbol::assign(Str0(LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_LX::assign(Value_P value, bool clone, const char * loc)
{
   if (value->get_rank() > 1)      RANK_ERROR;
   if (!value->is_char_string())   DOMAIN_ERROR;

   Symbol::assign(value, clone, LOC);
}
//=============================================================================
Quad_PP::Quad_PP()
   : SystemVariable(ID_Quad_PP)
{
Value_P value(LOC);

   new (&value->get_ravel(0)) IntCell(DEFAULT_Quad_PP);

   value->check_value(LOC);
   Symbol::assign(value, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_PP::assign(Value_P value, bool clone, const char * loc)
{
APL_Integer pp = value->get_sole_integer();
   if (pp < MIN_Quad_PP)   DOMAIN_ERROR;
   if (pp > MAX_Quad_PP)   pp = MAX_Quad_PP;

   Symbol::assign(IntScalar(pp, LOC), false, LOC);
}
//=============================================================================
Quad_PR::Quad_PR()
   : SystemVariable(ID_Quad_PR)
{
   Symbol::assign(CharScalar(UNI_ASCII_SPACE, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_PR::assign(Value_P value, bool clone, const char * loc)
{
UCS_string ucs = value->get_UCS_ravel();

   if (ucs.size() > 1)   LENGTH_ERROR;

   Symbol::assign(value, clone, LOC);
}
//=============================================================================
Quad_PS::Quad_PS()
   : SystemVariable(ID_Quad_PS),
     print_quotients(false),
     style(Command::boxing_format)
{
Value_P val(2, LOC);
   new (val->next_ravel())   IntCell(print_quotients);
   new (val->next_ravel())   IntCell(style);
   val->check_value(LOC);
   Symbol::assign(val, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_PS::assign(Value_P B, bool clone, const char * loc)
{
APL_Integer B_quot  = 0;
APL_Integer B_style = 0;

   if (B->get_rank() > 1)        RANK_ERROR;

   if (B->element_count() < 1)   LENGTH_ERROR;
   if (B->element_count() > 2)   LENGTH_ERROR;

   if (!B->get_ravel(0).is_near_bool())
      {
        MORE_ERROR() << "Bad quot in ⎕PS←quot style: quot is not near bool";
        DOMAIN_ERROR;
      }

   if (B->element_count() == 1)
      {
        // for compatibility with old workspaces
        //
        B_style = B->get_ravel(0).get_near_int();
      }
   else
      {
        B_quot  = B->get_ravel(0).get_near_bool();
        B_style = B->get_ravel(1).get_near_int();
      }

   switch(B_style) // boxing format
      {
        case -29:
        case -25: case -24: case -23:
        case -22: case -21: case -20:
        case -9: case  -8: case  -7:
        case -4: case  -3: case  -2:
        case  0:
        case  2: case   3: case   4:
        case  7: case   8: case   9:
        case 20: case  21: case  22:
        case 23: case  24: case  25:
        case 29: break;   // OK

        default: MORE_ERROR() <<
                 "Invalid style in ⎕PS←quot style or ⎕PS←style:\n"
                 "style is not ± 0, 2-4, 7-9, 20-25, or 29";
                  DOMAIN_ERROR;
      }

   // values in B are valid
   //
   Command::boxing_format = B_style;
   print_quotients = B_quot;
   style = B_style;

Value_P B2(2, LOC);
   new (B2->next_ravel())   IntCell(B_quot);
   new (B2->next_ravel())   IntCell(B_style);
   B2->check_value(LOC);

   Symbol::assign(B2, false, LOC);
   return;
}
//-----------------------------------------------------------------------------
void
Quad_PS::assign_indexed(Value_P X, Value_P B)
{
   if (!(X->is_int_scalar() || X->is_int_vector()))   INDEX_ERROR;
   if (!(B->is_int_scalar() || B->is_int_vector()))   DOMAIN_ERROR;
   if (X->element_count() != B->element_count())      LENGTH_ERROR;

const ShapeItem ec = X->element_count();
const APL_Integer qio = Workspace::get_IO();

APL_Integer Z_quot = print_quotients;
APL_Integer Z_style = style;
   loop(e, ec)
      {
        const APL_Integer x = X->get_ravel(e).get_near_int() - qio;
        const APL_Integer b = B->get_ravel(e).get_near_int();

        if (x == 0)   // display quotients
           {
             if (b < 0)   DOMAIN_ERROR;
             if (b > 1)   DOMAIN_ERROR;
             Z_quot = b;
           }
        else if (x == 1)   switch(b) // boxing format
           {
             case -29:
             case -25: case -24: case -23:
             case -22: case -21: case -20:
             case -9: case  -8: case  -7:
             case -4: case  -3: case  -2:
             case  0:
             case  2: case   3: case   4:
             case  7: case   8: case   9:
             case 20: case  21: case  22:
             case 23: case  24: case  25:
             case 29: Z_style = b;
                  Command::boxing_format = b;
                  break;
 
             default:
                  MORE_ERROR() <<
                       "Invalid print style (try 0, 2-4, 7-9, 20-25, or 29)";
                  DOMAIN_ERROR;
           }
        else
           {
             INDEX_ERROR;
           }
      }

Value_P Z(2, LOC);
   new (Z->next_ravel())   IntCell(Z_quot);
   new (Z->next_ravel())   IntCell(Z_style);
   Z->check_value(LOC);
   Symbol::assign(Z, false, LOC);
}
//=============================================================================
Quad_PW::Quad_PW()
   : SystemVariable(ID_Quad_PW)
{
   Symbol::assign(IntScalar(uprefs.initial_pw, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_PW::assign(Value_P value, bool clone, const char * loc)
{
const APL_Integer pw = value->get_sole_integer();

   // min. ⎕PW is 30. Ignore smaller values.
   if (pw < MIN_Quad_PW)   return;

   // max val is system specific. Ignore larger values.
   if (pw > MAX_Quad_PW)   return;

   Symbol::assign(IntScalar(pw, LOC), false, LOC);
}
//=============================================================================
Quad_Quad::Quad_Quad()
 : SystemVariable(ID_Quad_Quad)
{
}
//-----------------------------------------------------------------------------
void
Quad_Quad::assign(Value_P value, bool clone, const char * loc)
{
   // write pending LF from  ⍞ (if any)
   Quad_QUOTE::done(true, LOC);

   value->print(COUT);
}
//-----------------------------------------------------------------------------
void
Quad_Quad::resolve(Token & token, bool left)
{
   if (left)   return;   // ⎕←xxx

   // write pending LF from  ⍞ (if any)
   Quad_QUOTE::done(true, LOC);

   COUT << UNI_Quad_Quad << ":" << endl;

bool eof = false;
UCS_string line;
   InputMux::get_line(LIM_Quad_Quad, Workspace::get_prompt(), line, eof,
                      LineHistory::quad_quad_history);

   line.remove_leading_and_trailing_whitespaces();

   token.move_2(Bif_F1_EXECUTE::execute_statement(line), LOC);
}
//=============================================================================
Quad_QUOTE::Quad_QUOTE()
 : SystemVariable(ID_QUOTE_Quad)
{
   // we assign a dummy value so that ⍞ is not undefined.
   //
Value_P dummy(UCS_string(LOC), LOC);
   dummy->set_complete();
   Symbol::assign(dummy, false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_QUOTE::done(bool with_LF, const char * loc)
{
   Log(LOG_cork)
      CERR << "Quad_QUOTE::done(" << with_LF << ") called from " << loc
           << " , buffer = [" << prompt << "]" << endl;

   if (prompt.size())
      {
        if (with_LF)   COUT << endl;
        prompt.clear();
      }
}
//-----------------------------------------------------------------------------
void
Quad_QUOTE::assign(Value_P value, bool clone, const char * loc)
{
   Log(LOG_cork)
      CERR << "Quad_QUOTE::assign() called, buffer = ["
           << prompt << "]" << endl;

PrintContext pctx(PR_QUOTE_Quad);
PrintBuffer pb(*value, pctx, 0);
   if (pb.get_height() > 1)   // multi line output: flush and restart corking
      {
        loop(y, pb.get_height())
           {
             done(true, LOC);
             prompt = pb.get_line(y).no_pad();
             COUT << prompt;
           }
      }
   else if (pb.get_height() > 0)
      {
        COUT << pb.l1().no_pad() << flush;
        prompt.append(pb.l1());
      }
   else   // empty output
      {
        // nothing to do
      }

   cout << flush;

   Log(LOG_cork)
      CERR << "Quad_QUOTE::assign() done, buffer = ["
           << prompt << "]" << endl;
}
//-----------------------------------------------------------------------------
Value_P
Quad_QUOTE::get_apl_value() const
{
   Log(LOG_cork)
      CERR << "Quad_QUOTE::get_apl_value() called, buffer = ["
           << prompt << "]" << endl;

   // get_quad_cr_line() may call done(), so we save the current prompt.
   //
const UCS_string old_prompt = prompt.no_pad();

bool eof = false;
UCS_string line;
   InputMux::get_line(LIM_Quote_Quad, old_prompt, line, eof,
                      LineHistory::quote_quad_history);
   done(false, LOC);   // if get_quad_cr_line() has not called it

   if (interrupt_is_raised())   INTERRUPT

const UCS_string qpr = Workspace::get_PR();

   if (qpr.size() > 0)   // valid prompt replacement char
      {
        loop(i, line.size())
           {
             if (i >= old_prompt.size())   break;
             if (old_prompt[i] != line[i])   break;
             line[i] = qpr[0];
           }
      }

Value_P Z(line, LOC);
   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_R::Quad_R()
 : NL_SystemVariable(ID_Quad_R)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_R::assign(Value_P value, bool clone, const char * loc)
{
StateIndicator * si = Workspace::SI_top_fun();
   if (si == 0)   return;

   // ignore assignments if error was SYNTAX ERROR or VALUE ERROR
   //
   if (StateIndicator::get_error(si).is_syntax_or_value_error())   return;

   si->set_R(value);
}
//-----------------------------------------------------------------------------
Value_P
Quad_R::get_apl_value() const
{
StateIndicator * si = Workspace::SI_top_error();
   if (si)
      {
        Value_P ret = si->get_R();
        if (!!ret)   return  ret;
      }

   VALUE_ERROR;
}
//=============================================================================
void
Quad_SYL::assign(Value_P value, bool clone, const char * loc)
{
   // Quad_SYL is mostly read-only, so we only allow assign_indexed() with
   // certain values.
   //
   if (!!value)   SYNTAX_ERROR;

   // this assign is called from the constructor in order to trigger the
   // creation of a symbol for Quad_SYL.
   //
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_SYL::assign_indexed(IndexExpr & IDX, Value_P value)
{
   //  must be an array index of the form [something; 2]
   //
   if (IDX.value_count() != 2)   INDEX_ERROR;

   // IDX is in reverse order: ⎕SYL[X1;X2]
   //
Value_P X2 = IDX.extract_value(0);

const APL_Integer qio = Workspace::get_IO();

   if (!X2)                                          INDEX_ERROR;
   if (X2->element_count() != 1)                     INDEX_ERROR;
   if (!X2->get_ravel(0).is_near_int())              INDEX_ERROR;
   if (X2->get_ravel(0).get_near_int() != qio + 1)   INDEX_ERROR;

Value_P X1 = IDX.extract_value(1);

   assign_indexed(X1, value);
}
//-----------------------------------------------------------------------------
void
Quad_SYL::assign_indexed(Value_P X, Value_P B)
{
   if (!(X->is_int_scalar() || X->is_int_vector()))   INDEX_ERROR;
   if (!(B->is_int_scalar() || B->is_int_vector()))   DOMAIN_ERROR;
   if (X->element_count() != B->element_count())      LENGTH_ERROR;

const ShapeItem ec = X->element_count();
const APL_Integer qio = Workspace::get_IO();

   loop(e, ec)
      {
        const APL_Integer x = X->get_ravel(e).get_near_int() - qio;
        const APL_Integer b = B->get_ravel(e).get_near_int();

        if (x == SYL_SI_DEPTH_LIMIT)   // SI depth limit
           {
             const int depth = Workspace::SI_entry_count();
             if (b && (b < (depth + 4)))   DOMAIN_ERROR;    // limit too low
             si_depth_limit = b;
           }
        else if (x == SYL_VALUE_COUNT_LIMIT)   // value count limit
           {
             if (b && (b < APL_Integer(Value::value_count + 10)))    // too low
                DOMAIN_ERROR;
             value_count_limit = b;
           }
        else if (x == SYL_RAVEL_BYTES_LIMIT)   // ravel bytes limit
           {
             const int cells = b / sizeof(Cell);
             if (cells && (cells < APL_Integer(Value::total_ravel_count + 1000)))
                DOMAIN_ERROR;

             ravel_count_limit = cells;
           }
        else if (x == SYL_CURRENT_CORES)   // number of cores
           {
#if PARALLEL_ENABLED
             if (Parallel::set_core_count(CoreCount(b), false))
                DOMAIN_ERROR;
#else
             DOMAIN_ERROR;
#endif
           }
        else if (x == SYL_PRINT_LIMIT)   // print length limit
           {
             if (b < 1000)   DOMAIN_ERROR;
             print_length_limit = b;
           }
        else if (x == SYL_WA_MARGIN)   // ⎕WA safety margin
           {
             if (b < 1000000)   DOMAIN_ERROR;
             Quad_WA::WA_margin = b;
           }
        else if (x == SYL_WA_SCALE)   // ⎕WA memory scale (45% - 200% %
           {
             if (b <  45)   DOMAIN_ERROR;
             if (b > 200)   DOMAIN_ERROR;
             Quad_WA::WA_scale = b;
           }
        else
           {
             INDEX_ERROR;
           }
      }
}
//-----------------------------------------------------------------------------
Value_P
Quad_SYL::get_apl_value() const
{
const Shape sh(SYL_MAX, 2);
Value_P Z(sh, LOC);

#define syl2(n, e, v) syl1(n, e, v)
#define syl3(n, e, v) syl1(n, e, v)
#define syl1(n, _e, v) \
  new (Z->next_ravel()) \
      PointerCell(Value_P(UCS_string(UTF8_string(n)), LOC).get(), Z.getref()); \
  new (Z->next_ravel()) IntCell(v);
#include "SystemLimits.def"

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_TC::Quad_TC()
   : RO_SystemVariable(ID_Quad_TC)
{
Value_P QCT(3, LOC);
   new (QCT->next_ravel()) CharCell(UNI_ASCII_BS);
   new (QCT->next_ravel()) CharCell(UNI_ASCII_CR);
   new (QCT->next_ravel()) CharCell(UNI_ASCII_LF);
   QCT->check_value(LOC);

   Symbol::assign(QCT, false, LOC);
}
//=============================================================================
Quad_TS::Quad_TS()
   : RO_SystemVariable(ID_Quad_TS)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
Value_P
Quad_TS::get_apl_value() const
{
const APL_time_us offset_us = 1000000 * Workspace::get_v_Quad_TZ().get_offset();
const YMDhmsu time(now() + offset_us);

Value_P Z(7, LOC);
   new (Z->next_ravel()) IntCell(time.year);
   new (Z->next_ravel()) IntCell(time.month);
   new (Z->next_ravel()) IntCell(time.day);
   new (Z->next_ravel()) IntCell(time.hour);
   new (Z->next_ravel()) IntCell(time.minute);
   new (Z->next_ravel()) IntCell(time.second);
   new (Z->next_ravel()) IntCell(time.micro / 1000);

   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_TZ::Quad_TZ()
   : SystemVariable(ID_Quad_TZ)
{
   offset_seconds = compute_offset();

   // ⎕TZ is the offset in hours between GMT and local time
   //
   if (offset_seconds % 3600 == 0)   // full hour
      Symbol::assign(IntScalar(offset_seconds/3600, LOC), false, LOC);
   else
      Symbol::assign(FloatScalar(offset_seconds/3600, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
int
Quad_TZ::compute_offset()
{
   // the GNU/Linux timezone variable conflicts with function timezone() on
   // other systems. timezone.tz_minuteswest in gettimeofday() does not
   // sound reliable either.
   // 
   // We compute localtime() and gmtime() and take the difference (hoping
   // that localtime() and gmtime() are more portable.
   // 

   // choose a reference point in time.
   //
const time_t ref = now()/1000000;

tm * local = localtime(&ref);
const int local_minutes = local->tm_min + 60*local->tm_hour;
const int local_days = local->tm_mday + 31*local->tm_mon + 12*31*local->tm_year;

tm * gmean = gmtime(&ref);
const int gm_minutes    = gmean->tm_min + 60*gmean->tm_hour;
const int gm_days = gmean->tm_mday + 31*gmean->tm_mon + 12*31*gmean->tm_year;
int diff_minutes = local_minutes - gm_minutes;

   if (local_days < gm_days)        diff_minutes -= 24*60;   // local < gmean
   else if (local_days > gm_days)   diff_minutes += 24*60;   // local > gmean

   return 60*diff_minutes;
}
//-----------------------------------------------------------------------------
ostream &
Quad_TZ::print_timestamp(ostream & out, APL_time_us when) const
{
const APL_time_us offset = get_offset();
const YMDhmsu time(when + 1000000*offset);
const char * tz_sign = (offset < 0) ? "" : "+";

ostringstream os;
   os << setfill('0') << time.year  << "-"
      << setw(2)      << time.month << "-"
      << setw(2)      << time.day   << "  "
      << setw(2)      << time.hour  << ":"
      << setw(2)      << time.minute << ":"
      << setw(2)      << time.second << " (GMT"
      << tz_sign      << offset/3600 << ")";

   return out << os.str();
}
//-----------------------------------------------------------------------------
void
Quad_TZ::assign(Value_P value, bool clone, const char * loc)
{
   if (!value->is_scalar())   RANK_ERROR;

   // ignore values outside [-12 ... 14], DOMAIN ERROR for bad types.

Cell & cell = value->get_ravel(0);
   if (cell.is_integer_cell())
      {
        const APL_Integer ival = cell.get_near_int();
        if (ival < -12)   return;
        if (ival > 14)    return;
        offset_seconds = ival*3600;
        Symbol::assign(value, clone, LOC);
        return;
      }

   if (cell.is_float_cell())
      {
        const double hours = cell.get_real_value();
        if (hours < -12.1)   return;
        if (hours > 14.1)    return;
        offset_seconds = int(0.5 + hours*3600);
        Symbol::assign(value, clone, LOC);
        return;
      }

   DOMAIN_ERROR;
}
//=============================================================================
Quad_UL::Quad_UL()
   : RO_SystemVariable(ID_Quad_UL)
{
   Symbol::assign(get_apl_value(), false, LOC);
}
//-----------------------------------------------------------------------------
Value_P
Quad_UL::get_apl_value() const
{
int user_count = 0;

#ifdef USER_PROCESS
   setutxent();   // rewind utmp file

   for (;;)
       {
         utmpx * entry = getutxent();
         if (entry == 0)   break;   // end of  user accounting database

         if (entry->ut_type == USER_PROCESS)   ++user_count;
       }

   endutxent();   // close utmp file
#else
   user_count = 1;
#endif

Value_P Z(LOC);
   new (Z->next_ravel())   IntCell(user_count);
   Z->check_value(LOC);
   return Z;
}
//=============================================================================
Quad_X::Quad_X()
 : NL_SystemVariable(ID_Quad_X)
{
   Symbol::assign(IntScalar(0, LOC), false, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_X::assign(Value_P value, bool clone, const char * loc)
{
StateIndicator * si = Workspace::SI_top_fun();
   if (si == 0)   return;

   // ignore assignments if error was SYNTAX ERROR or VALUE ERROR
   //
   if (StateIndicator::get_error(si).is_syntax_or_value_error())   return;

   si->set_X(value);
}
//-----------------------------------------------------------------------------
Value_P
Quad_X::get_apl_value() const
{
StateIndicator * si = Workspace::SI_top_error();
   if (si)
      {
        Value_P ret = si->get_X();
        if (!!ret)   return ret;
      }

   VALUE_ERROR;
}
//=============================================================================
