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

#include "Sys.hh"

/** @file
*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "Bif_F12_TAKE_DROP.hh"
#include "Error.hh"
#include "Output.hh"
#include "Parser.hh"
#include "StateIndicator.hh"
#include "Symbol.hh"
#include "UserFunction.hh"
#include "UserPreferences.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
// conatructor for a lambda
//════════════════════════════════════════════════════════════════════════════
UserFunction::UserFunction(Fun_signature sig, Lambda_number lambda_num,
                           const UCS_string & text, const Token_string & lambda_body,
                           const vector<Symbol *> & lvars)
  : Function(ID_USER_SYMBOL, TOK_FUN0),
    Executable(sig, lambda_num, text, LOC),
    creator(UNI_LAMBDA),
    error_info("Unspecified"),
    error_line(0),
    header(sig, lambda_num)
{
   set_creation_time(now());

   exec_properties[0] = 0;
   exec_properties[1] = 0;
   exec_properties[2] = 0;
   exec_properties[3] = 0;

   if (header.get_error() != E_NO_ERROR)   // bad header
      {
        error_info = header.get_error_info();
        return;
      }

   if      (header.RO())   tag = TOK_OPER2;
   else if (header.LO())   tag = TOK_OPER1;
   else if (header.A())    tag = TOK_FUN2;
   else if (header.B())    tag = TOK_FUN1;
   else                    tag = TOK_FUN0;

   loop(lv, lvars.size())   header.add_local_var(lvars[lv]);

   // order of local vars is reversed. Fix that.
   //
   header.reverse_local_vars();

   parse_body_line(Function_Line_0, lambda_body,
                   lambda_body.size() ? lambda_body.back().get_PC()   // RETURN
                                      : Function_PC_0,
                   false, LOC);
   setup_lambdas();
   set_END_PCs();
#if cfg_ASSERT_LEVEL_WANTED >= 2
   Assert1(check_PCs(LOC) == 0);
#endif
   line_starts.push_back(Prefix_PC(lambda_body.ssize() - 1));
   line_starts.push_back(Prefix_PC_0);
   error_line = -1;   // no error
   error_info = 0;
}
//────────────────────────────────────────────────────────────────────────────
UserFunction::~UserFunction()
{
   Log(LOG_UserFunction__enter_leave)
      CERR << "Function " << get_name() << " deleted." << endl;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UserFunction::canonical(bool with_lines) const
{
UCS_string ucs;
   loop(t, text.size())
      {
        if (with_lines)   ucs << line_prefix(Function_Line(t));
        ucs << text[t] << UNI_LF;
      }

   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_() const
{
   Log(LOG_UserFunction__enter_leave)
      CERR << "Function " << get_name() << " calls eval_()" << endl;

   if (header.B())   SYNTAX_ERROR;   // not defined niladic

   Workspace::push_SI(this, LOC);
   if (header.Z())   header.Z()->push();

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_AB(cValue_R A, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls eval_AB("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC))
             << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC))
             << ")" << endl;
      }

   if (header.LO())    SYNTAX_ERROR;    // defined as operator
   if (!header.A())    VALENCE_ERROR;   // monadic

   Workspace::push_SI(this, LOC);

   if (header.Z())   header.Z()->push();
   if (header.A())   header.A()->push_value(CLONE(&A, LOC));
   if (header.X())   header.X()->push();
   if (header.B())   header.B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_ALB(cValue_R A, Token & LO, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC)) << ", " << endl;
        print_val_or_fun(CERR, LO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.RO())    SYNTAX_ERROR;    // defined as dyadic operator
   if (!header.A())    VALENCE_ERROR;   // monadic

   Workspace::push_SI(this, LOC);

   if (header.X())         header.X()->push();

   if (header.Z())         header.Z()->push();
   /* ALWAYS */            header.A()->push_value(CLONE(&A, LOC));
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   /* ALWAYS */            header.B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_ALRB(cValue_R A, Token & LO, Token & RO, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC)) << ", " << endl;
        print_val_or_fun(CERR, LO) << ", ";
        print_val_or_fun(CERR, RO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (!header.RO())    SYNTAX_ERROR;   // defined monadic op called dyadically
   if (!header.A())    VALENCE_ERROR;   // monadic

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();

   header                        .A()->push_value(CLONE(&A, LOC));
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (RO.is_function())   header.RO()->push_function(RO.get_function());
   else                    header.RO()->push_value(RO.get_apl_val());
   if (header.X())         header.X()->push();
   header                        .B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::axis_error() const
{
   if (is_lambda())
      MORE_ERROR() << get_name() << "[X]: " << get_name()
                   << " has no axis argument (χ is not used)";
   else
      MORE_ERROR() << get_name() << "[X]: " << get_name()
                   << " has no axis argument ([X] is not in its header)";
   AXIS_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_ALRXB(cValue_R A, Token & LO, Token & RO,
                         cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC)) << ", " << endl;
        print_val_or_fun(CERR, LO) << ", ";
        print_val_or_fun(CERR, RO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&X, LOC)) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (!header.RO())   SYNTAX_ERROR;   // defined monadic op called dyadically
   if (!header.A())    VALENCE_ERROR;   // monadic
   if (!header.X())    axis_error();

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();
   header                        .A()->push_value(CLONE(&A, LOC));
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (RO.is_function())   header.RO()->push_function(RO.get_function());
   else                    header.RO()->push_value(RO.get_apl_val());
   if (header.X())         header.X()->push_value(CLONE(&X, LOC));
   header                        .B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC)) << ", " << endl;
        print_val_or_fun(CERR, LO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&X, LOC)) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.RO())    SYNTAX_ERROR;    // defined as dyadic operator
   if (!header.A())    VALENCE_ERROR;   // monadic
   if (!header.X())    axis_error();

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();
   header                        .A()->push_value(CLONE(&A, LOC));
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (header.X())         header.X()->push_value(CLONE(&X, LOC));
   header                        .B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls eval_AB("
             << Token(TOK_APL_VALUE1, CLONE(&A, LOC)) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.LO())    SYNTAX_ERROR;    // defined as operator
   if (!header.A())    VALENCE_ERROR;   // monadic
   if (!header.X())    axis_error();

   Workspace::push_SI(this, LOC);

   if (header.Z())   header.Z()->push();
   if (header.A())   header.A()->push_value(CLONE(&A, LOC));
   if (header.X())   header.X()->push_value(CLONE(&X, LOC));
   if (header.B())   header.B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_B(cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls eval_B("
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.LO())    SYNTAX_ERROR;   // defined as operator

   Workspace::push_SI(this, LOC);

   if (header.Z())   header.Z()->push();
   if (header.A())   header.A()->push();
   if (header.X())   header.X()->push();
   if (header.B())   header.B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_LB(Token & LO, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "(";
        print_val_or_fun(CERR, LO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.RO())    SYNTAX_ERROR;   // dyadic operator called monadically

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z() ->push();
   if (header.A())         header.A() ->push();
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (header.X())         header.X() ->push();
   header                        .B() ->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_LRB(Token & LO, Token & RO, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "(";
        print_val_or_fun(CERR, LO) << ", ";
        print_val_or_fun(CERR, RO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (!header.RO())    SYNTAX_ERROR;   // not defined as dyadic operator

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();
   if (header.A())         header.A()->push();

   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (RO.is_function())   header.RO()->push_function(RO.get_function());
   else                    header.RO()->push_value(RO.get_apl_val());
   if (header.X())         header.X() ->push();
   header                        .B() ->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_LRXB(Token & LO, Token & RO, cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "(";
        print_val_or_fun(CERR, LO) << ", ";
        print_val_or_fun(CERR, RO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&X, LOC)) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (!header.RO())   SYNTAX_ERROR;   // not defined as dyadic operator
   if (!header.X())    axis_error();

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();
   if (header.A())         header.A()->push();
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (RO.is_function())   header.RO()->push_function(RO.get_function());
   else                    header.RO()->push_value(RO.get_apl_val());
   header.X()->push_value(CLONE(&X, LOC));
   header                        .B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_LXB(Token & LO, cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls " << __FUNCTION__ << "(";
        print_val_or_fun(CERR, LO) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&X, LOC)) << ", "
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (header.RO())    SYNTAX_ERROR;   // dyadic operator called monadically
   if (!header.X())    axis_error();

   Workspace::push_SI(this, LOC);

   if (header.Z())         header.Z()->push();
   if (header.A())         header.A()->push();
   if (LO.is_function())   header.LO()->push_function(LO.get_function());
   else                    header.LO()->push_value(LO.get_apl_val());
   if (header.X())         header.X()->push_value(CLONE(&X, LOC));
   header                        .B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_XB(cValue_R X, cValue_R B) const
{
   Log(LOG_UserFunction__enter_leave)
      {
        CERR << "Function " << get_name() << " calls eval_B("
             << Token(TOK_APL_VALUE1, CLONE(&B, LOC)) << ")" << endl;
      }

   if (!header.X())    axis_error();
   if (header.LO())    SYNTAX_ERROR;   // defined as operator

   Workspace::push_SI(this, LOC);

   if (header.Z())   header.Z()->push();
   if (header.A())   header.A()->push();
   if (header.X())   header.X()->push_value(CLONE(&X, LOC));
   if (header.B())   header.B()->push_value(CLONE(&B, LOC));

   header.eval_common();

   return Token(TOK_SI_PUSHED);
}
//────────────────────────────────────────────────────────────────────────────
Function_Line
UserFunction::get_line(Prefix_PC pc) const
{
   Assert(pc >= Prefix_PC_invalid);
   if (pc < Prefix_PC_0)   pc = Prefix_PC_0;

   // search line_starts backwards until a line with non-greater pc is found.
   //
   for (int l = line_starts.size() - 1; l > 0; --l)
       {
         if (line_starts[l] <= pc)   return Function_Line(l);
       }

   return Function_Line_1;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UserFunction::get_name_and_line(Prefix_PC pc) const
{
UCS_string ret = header.get_name();
   if (ret.size() && ret[0] == UNI_LAMBDA)
      {
        UCS_string name = Workspace::find_lambda_name(this);
        if (name.size())   ret = name;
      }
   ret << UNI_L_BRACK;

   // pc may point to the next token already. If that is the case then
   // we go back one token.
   //
   if (pc > Prefix_PC_0 && body[pc - 1].get_Class() == TC_END)   --pc;

const Function_Line line = get_line(pc);
   return ret << line << UNI_R_BRACK;
}
//────────────────────────────────────────────────────────────────────────────
void 
UserFunction::help(ostream & out) const
{
   CERR << "    Header: " << get_text(0) << endl;

   if (is_lambda())
      {
         UCS_string body(get_text(1), 2);
         CERR << "Lambda: { " << body << " ";
         loop(v, local_var_count())
            {
              const Symbol & sym = *get_local_var(v);
              CERR << ";" << sym.get_name();
            }
         CERR << " }" << endl;
         return;
      }

bool got_lamps = false;
bool toronto = false;
const UCS_string two_lamps(U"⍝⍝");
   for (int l = 1; l < get_text_size(); ++l)
       {
         UCS_string line(get_text(l));
         line.remove_leading_and_trailing_whitespaces();
         if (line.size() < 2)          continue;   // too short

         if (line[0] != UNI_COMMENT)   // not a comment
            {
              toronto = false;
              continue;
            }

         const bool double_lamps = line[1] == UNI_COMMENT;   // ⍝⍝ line
         if (line[1] == UNI_FULLSTOP)                  // ⍝. line
            {
              toronto = true;
            }

         if (double_lamps || toronto)
            {
              got_lamps = true;
              CERR << "    " << line << endl;
            }
       }

   if (!got_lamps)   CERR << "    (no ⍝⍝ or ⍝. comment lines)" << endl;
}
//────────────────────────────────────────────────────────────────────────────
Prefix_PC
UserFunction::line_start(Function_Line line) const
{
   if (line < 0 || size_t(line) >= line_starts.size())
      {
        Q1(line)
        Q1(line_starts.size())
        Assert(0);
      }

   return line_starts[line];
}
//────────────────────────────────────────────────────────────────────────────
Prefix_PC
UserFunction::pc_for_line(Function_Line line) const
{
   if (line <= Function_Line_0 || line >= Function_Line(line_starts.size()))
      return body.end_PC() - 1;

   return line_starts[line];
}
//────────────────────────────────────────────────────────────────────────────
ostream &
UserFunction::print(ostream & out) const
{
   out << header.get_name();
   return out;

/*
   out << "Function header:" << endl;
   if (header.Z())     out << "Result:         " << *header.Z()   << endl;
   if (header.A())     out << "Left Argument:  " << *header.A()   << endl;
   if (header.LO())    out << "Left Op Arg:    " << *header.LO()  << endl;
                       out << "Function:       " << header.get_name() << endl;
   if (header.RO())    out << "Right Op Arg:   " << *header.RO()  << endl;
   if (header.B())     out << "Right Argument: " << *header.B()   << endl;
   return Executable::print(out);
*/
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::print_line_PCs(const char * loc) const
{
   CERR << "At " << loc << ":" << endl;
   for (size_t j = 1; j < line_starts.size(); ++j)
       {
         CERR << "   " << get_name() << "[" << j << "]: PC= ";
         const Prefix_PC PC_from = line_starts[j];

         // Note: line_starts[0] is the end of the function
         const int next = j < (line_starts.size() - 1) ? j + 1 : 0;
         const Prefix_PC PC_to = line_starts[next];
         CERR << PC_from << "..." << (PC_to - 1) << endl;
       }
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::print_properties(ostream & out, int indent) const
{
   header.print_properties(out, indent);
UCS_string ind(indent, UNI_SPACE);
   out << ind << "Body Lines:     " << (line_starts.size() - 1) << "+[0]" << endl
       << ind << "Creator:        " << get_creator()      << endl
       << ind << "Body[" << body.ssize() << "⏩]: ";

   loop(b, body.ssize())
       {
         // maybe print line prefix
         //
         Function_Line line_number = Function_Invalid;
         loop(ls, line_starts.size())
            {
              if (Prefix_PC(b) == line_starts[ls])   // start of a line
                 {
                   line_number = Function_Line(ls);
                   out << endl << ind << "    [" << line_number << "]";

                   loop(lab, header.get_label_count())
                       {
                         const labVal & lv = header.get_label(lab);
                         if (lv.line == line_number)
                            {
                              out << " " << lv.sym->get_name() << ":";
                            }
                       }
                 }
            }

         out << " ⏩" << body[b];
       }
   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
bool
UserFunction::pushes_sym(const Symbol * sym) const
{
   if (sym == header.Z())   return true;
   if (sym == header.A())   return true;
   if (sym == header.LO())   return true;
   if (sym == header.X())   return true;
   if (sym == header.RO())   return true;
   if (sym == header.B())   return true;

   loop(l, local_var_count())
       {
         if (sym == get_local_var(l))   return true;
       }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::set_locked_error_info(Error & error) const
{
UCS_string message_2(error.get_error_line_2());

#define SHORT 0
   // header.A()/header.B() are non-zero whenever the function's HEADER
   // declares a left/right argument name -- regardless of whether this
   // particular call actually supplied one. A monadic call of a function
   // declared with a left argument (e.g. the LRM's own "signal VALENCE
   // ERROR via ⎕ES" idiom, p.353) leaves A declared but unbound
   // (NC_UNUSED_USER_NAME, not NC_VARIABLE); Symbol::get_apl_value()
   // throws VALUE_ERROR for exactly that case, which used to escape from
   // here and silently replace whatever error (e.g. the ⎕ES-simulated
   // VALENCE ERROR) this function was in the middle of reporting.
   // Guarded the same way get_apl_value() itself decides bound-ness.
   //
   if (header.A() && header.A()->get_NC() == NC_VARIABLE)
      {
#if SHORT
        message_2.append(header.A()->get_name());
        message_2.append(UNI_SPACE);
#else
        if (Value_P val_A = header.A()->get_apl_value())
           {
             // line 2 shows input (like B below), not boxed output
             PrintContext pctx(PR_APL_FUN);
             PrintBuffer pb(*val_A, pctx, 0);
             message_2 << UCS_string(pb, 1, DEFAULT_Quad_PW) << UNI_SPACE;
           }
#endif
      }

   message_2 << header.get_name();

   if (header.B() && header.B()->get_NC() == NC_VARIABLE)
      {
#if SHORT
        message_2 << UNI_SPACE << header.B()->get_name();
#else
        if (Value_P val_B = header.B()->get_apl_value())
           {
             PrintContext pctx(PR_APL_FUN);
             PrintBuffer pb(*val_B, pctx, 0);
             message_2 << UNI_SPACE << UCS_string(pb, 1, DEFAULT_Quad_PW);
           }
#endif
      }

int nchars;
   {
     UTF8_string utf(message_2);
     nchars = error.set_error_line_2(utf.c_str());
   }

   // right_caret is an absolute column index into message_2 (like
   // left_caret; see Error::get_error_line_3()), not relative to
   // left_caret -- point it at message_2's own last character (under
   // the last character of "[A] NAME [B]") directly, rather than via
   // the previous "left_caret + message_2.size() - 7", which only
   // worked because the caller (Error::update_error_info()) happens to
   // always set left_caret to 6 before calling this function (making
   // left_caret - 7 == -1): correct today by coincidence, silently
   // wrong for any left_caret other than 6.
   //
   // message_2.size() - 1 can point past the end when a printed
   // argument value made line 2 longer than ⎕EM[2;] actually shows;
   // clamp to the truncated length (Blake McBride, Bugs23 #6b).
const int wanted = message_2.size() - 1;
   error.set_right_caret(wanted > nchars ? nchars : wanted);
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::destroy()
{
   // delete will call ~Executable(), which releases the values owned by body.
   //
   if (is_lambda())   decrement_refcount(LOC);
   else               delete this;
}
//────────────────────────────────────────────────────────────────────────────
bool
UserFunction::optimize_label_vectors()
{
   /* Frequent design patterns are:

      → EXPR / L1, L2 ... Ln
      → EXPR / L1 L2 ... Ln
      → EXPR ⍴ L
               ├── const B ──┤

      In these cases, the right argument of / or ⍴ is a constant, so we can
      resolve the label values of L or L1, ... Ln into a single APL value B.
      That avoids the resolution (symbol lookup, line extraction) at runtime.
    */
   if (DONT_FT_DIRECT_BRANCHES)   return false;

   /* check for: VALUE → ENDL      e.g. → 4
      or:        SYMBOL → ENDL     e.g. → LABEL

      but rule out expressions:   e.g. → 4 + 5

      ⎕FX "FOO" "X←2" "→2" "Y←5"
      ⎕FX "FOO" "X←2" "→0" "'NOT REACHED'"
      ⎕FX "FOO" "X←2" "LABEL: Z←3 ◊ →LABEL" "Y←5"

    */
bool VOID_inserted = false;
   for (Function_PC pc = Function_PC(1); pc < body.ssize() - 3; ++pc)
       {
         if (!body[pc].is_RHO_or_SLASH())        continue;
         if (!is_label_or_value(body[pc - 1]))   continue;

         // at this point we have N⍴... or N/... or N⌿...

         // collect items in the right argument B of /B. ⌿B, or ⍴B.
         //
         vector<Function_PC> items_B;

         items_B.push_back(Function_PC(pc - 1));
         for (Function_PC pos = pc - 2; pos >= 0; --pos)
             {
               const TokenTag tag_pos = body[pos].get_tag();
               const TokenClass class_pos = body[pos].get_Class();
               if (is_label(body[pos]))
                  {
                    items_B.push_back(Function_PC(pos));
                  }
               else if (class_pos == TC_VALUE &&
                        body[pos].get_apl_val()->is_int_scalar())
                  {
                    items_B.push_back(Function_PC(pos));
                  }
               else if (tag_pos == TOK_F12_COMMA ||
                        tag_pos == TOK_F12_COMMA1)
                  {
                    // skip , or ⍪
                  }
               else if (pos == 0 || class_pos == TC_END)
                  {
                    break;   // for (int pos...)
                  }
               else   // something else: do nothing
                  {
                    items_B.clear();
                    break;
                  }
             }

         if (items_B.size() == 0)   continue;   // for (Function_PC pc...

         if (items_B.size() == 1)
            {
              // single item. If it is a value then leave it as is.
              // Otherwise resolve the label symbol.
              //
              const Function_PC pc0 = items_B[0];
              Token_loc & tok0 = body[pc0];
              if (tok0.get_Class() == TC_SYMBOL)
                 {
                   const Symbol * symbol = tok0.get_sym_ptr();
                   const int64_t line = get_label_line(symbol);
                   Value_P value = IntScalar(line, LOC);
                   tok0.copy(Token_loc(Token(TOK_APL_VALUE1, value),
                                       tok0.get_PC()), LOC);
                 }
            }
         else   // vector B
            {
              Value_P B(items_B.size(), LOC);
              loop(b, items_B.size())
                  {
                    const Function_PC pc_b = items_B[b];
                    Token_loc & tok_b = body[pc_b];
                    if (tok_b.get_Class() == TC_SYMBOL)
                       {
                         const Symbol * symbol = tok_b.get_sym_ptr();
                         const int64_t line = get_label_line(symbol);
                         B->next_ravel_Int(line);
                       }
                    else if (tok_b.get_Class() == TC_VALUE)
                       {
                         Cell cache;
                         const int64_t line = tok_b.get_apl_val()->get_cfirst(cache)
                                                   .get_int_value();
                         B->next_ravel_Int(line);
                       }
                  }
              B->check_value(LOC);

              // items_B is decreasing, so back() < front(). B gets the PC
              // of its leftmost item, which is body[items_B.front()] since
              // the body is reversed.
              //
              const Function_PC pc_B = body[items_B.front()].get_PC();
              for (Function_PC pc = items_B.back(); pc <= items_B.front(); ++pc)
                  {
                    body[pc].clear(LOC);
                  }
              body[items_B.back()].copy(Token_loc(Token(TOK_APL_VALUE1, B),
                                                  pc_B), LOC);
              VOID_inserted = true;
           }
       }

   if (VOID_inserted)   remove_TOK_VOID();

   return VOID_inserted;
}
//────────────────────────────────────────────────────────────────────────────
bool
UserFunction::optimize_labels()
{
   if (DONT_FT_LABEL_LITERAL)   return false;

const size_t labels_declared = header.get_label_count();
   if (labels_declared == 0)   return false;   // function has no labels

   for (int pc = Function_PC(-1); pc < body.ssize() - 4; ++pc)
       {
         if (body[pc + 2].get_tag() == TOK_R_ARROW &&   // least likely first
             body[pc + 3].get_tag() == TOK_ENDL    &&
             (pc == -1 || body[pc].get_tag() == TOK_ENDL))
            {
              const Token & tok = body[pc + 1];
              if (tok.get_tag() == TOK_SYMBOL)        // → SYMBOL
                 {
                   const Symbol * symbol = tok.get_sym_ptr();
                   loop(idx, labels_declared)
                       {
                         const labVal & label = header.get_label(idx);
                         if (symbol != label.sym)   continue;

                         const int64_t target_pc = int(line_starts[label.line]);
                         // → SYMBOL becomes GOTO_PC with the PC of →
                         body[pc + 1].copy(Token_loc(Token(TOK_GOTO_PC, target_pc),
                                                     body[pc + 2].get_PC()), LOC);
                         body[pc + 2].copy(body[pc + 3], LOC);   // was TOK_R_ARROW

                         OptmizationStatistics::count(OPTI_FT_LABEL_LITERAL);
                       }
                 }
              else if (tok.get_Class() == TC_VALUE)   // →N
                 {
                   const cValue * val_N = tok.get_apl_val().get();
                   Cell cache;
                   const Cell & cell_N = val_N->get_cfirst(cache);
                   if (val_N->is_int_scalar())
                      {
                        const APL_Integer N = cell_N.get_int_value();
                        if (N >= 0 && size_t(N) < line_starts.size())
                           {
                             const int64_t target_pc = int(line_starts[N]);
                             // → N becomes GOTO_PC with the PC of →
                             body[pc + 1].copy(Token_loc(Token(TOK_GOTO_PC, target_pc),
                                                         body[pc + 2].get_PC()), LOC);
                             body[pc + 2].copy(body[pc + 3], LOC);   // was TOK_R_ARROW

                             OptmizationStatistics::count(OPTI_FT_LABEL_LITERAL);
                           }
                      }
                 }
            }
       }

   return false;   // no TOK_VOID inserted
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::parse_body(const char * loc, bool macro)
{
   header.clear_labels();
   parsed_text.clear();
   line_starts.clear();
   line_starts.push_back(Prefix_PC_0);   // will be set later.

   // The function text is modified for parsing it, but restored afterwards
   // so that e.g. ∇FUN[⎕]∇ shows the text entered by the user.
   //
   // Captured once, here, before any of the three multi-line
   // transformation blocks below run -- NOT re-captured by each block
   // right before its own transform (as it used to be). When more than
   // one block fires (e.g. a function body has both a """ string and a
   // <<<>>> literal), each block's own snapshot overwrote the previous
   // one, so by the end this held only the *post-first-transform*
   // state, not the true original; the final restore below then left
   // an orphaned @N@ marker (from the already-applied, "forgotten"
   // transform) permanently baked into the stored text -- which a
   // later re-parse (UserFunction::set_trace_stop() unconditionally
   // re-parses every function right after Nabla defines it) cannot
   // resolve, since it starts with a fresh, empty Lit_DB. Capturing
   // once up front means every block's failure path (and the final
   // restore) always reaches the *actual* original source, so no
   // half-transformed text is ever kept.
   //
UCS_string_vector original_text = text;

   clear_body();

Lit_DB literals;
   if (UserPreferences::uprefs.new_multi_line_strings)
      {
        bool inside = false;
        for (int li = 1; li < get_text_size(); ++li)
            {
              const UCS_string & line = get_text(li);
              const ShapeItem multi = line.multi_pos();
              if (multi == -1)          continue;   // no """, «««, or <<<
              inside = ! inside;
              if (line[multi] == '<')   continue;   // literal <<<

              // the first (!) multi-line literal starts at line li
              //
              if (Parser::replace_multi_line_strings(text, literals, true))
                 {
                   text = original_text;   // restore function text
                   error_line = li;
                   return;
                 }

              break;   // transform_new_multi_line_strings() has done all (!)
            }
      }

   if (UserPreferences::uprefs.multi_line_literals)
      {
        bool inside = false;
        for (int li = 1; li < get_text_size(); ++li)
            {
              const UCS_string & line = get_text(li);
              const ShapeItem multi =
                    line.multi_pos();
              if (multi == -1)           continue;   // no """, «««, or <<<
              inside = ! inside;
              if (line[multi] != '<')   continue;   // literal <<<

              // the first (!) multi-line literal starts at li
              //
              if (Parser::replace_multi_line_literals(text, literals, true))
                 {
                   text = original_text;   // restore function text
                   error_line = li;
                   return;
                 }

              break;   // transform_new_multi_line_strings() has done all (!)
            }
      }

   if (UserPreferences::uprefs.old_multi_line_strings)
      {
        for (int li = 1; li < get_text_size(); ++li)
            {
              const UCS_string & line = get_text(li);
              if (!(line.double_quote_count(false) & 1))   continue;

              if (Parser::transform_old_multi_line_strings(text))
                 {
                   text = original_text;   // restore function text
                   error_line = li;
                   return;
                 }

              break;   // transform_old_multi_line_strings() does all (!)
            }
      }

Function_PC end_PC = Function_PC_0;   // the PC of the end of the last line
   for (int l = 1; l < get_text_size(); ++l)
      {
        bool stop_line = false;
        loop(s, stop_lines.size())
           {
             if (stop_lines[s] == l)
                {
                  stop_line = true;
                  break;
                }
           }

        bool trace_line = false;
        loop(t, trace_lines.size())
           {
             if (trace_lines[t] == l)
                {
                  trace_line = true;
                  break;
                }
           }

        error_line = l;   // assume error
        line_starts.push_back(body.end_PC());

        if (stop_line)
           {
             // the stop statement precedes the line: PC 0
             body.emplace_back(Token(TOK_STOP_LINE), Function_PC_0);
             const int64_t tr = 0;
             body.emplace_back(Token(TOK_END, tr), Function_PC_0);
           }

        const UCS_string & line = get_text(l);
        try {
              const ErrorCode ec =
                    parse_body_line(Function_Line(l), line, trace_line,
                                    loc, macro, &end_PC);
              if (ec != E_NO_ERROR)   return;   // error_line is already l
            }
        catch(const Error & err)
            {
              return;
            }
      }

   error_line = -1;   // OK
   setup_lambdas();

   Log(LOG_UserFunction__fix)
      {
        CERR << "body.ssize() is " << body.ssize() << endl
             << "line_starts.size() is " << line_starts.size() <<endl; 
      }

   // set line_starts[0] be the end of the function, so that →0 returns
   // from the function
   //
   line_starts[0] = body.end_PC();

   // the RETURN token belongs to the last line (see get_line()) and gets
   // the PC of its end.
   //
   if (header.Z())   body.emplace_back(Token(TOK_RETURN_SYMBOL, header.Z()), end_PC);
   else              body.emplace_back(Token(TOK_RETURN_VOID), end_PC);

   // remember the lines as parsed if a multi-line string or literal has
   // changed them (the lines of the body are then no longer the lines of
   // the original text, see get_body_text()). Then restore the original
   // text (before any multi-line expansion)
   //
   parsed_text.clear();
   if (original_text.size())
      {
        bool same = text.size() == original_text.size();
        for (size_t l = 0; same && l < text.size(); ++l)
            same = text[l] == original_text[l];
        if (!same)   parsed_text = text;
        text = original_text;
      }

   // recompute the →→ ←→ ←← jump PCs. An unbalanced →→ ←→ ←← makes the
   // function bad (reported by fix() like any other bad line, Bugs35 #53)
   //
   {
     Function_Line bad_line = Function_Line_0;
     const char * bad_cause = 0;
     if (compute_if_else_targets(&bad_line, &bad_cause))
        {
          error_line = bad_line;
          error_info = bad_cause;
          return;
        }
   }

   // replace marker tokens with their corresponding literal
   //
   if (literals.size())   // if there were any literals defined
      {
        loop(b, body.size())
            {
              Token_loc & token = body[b];
              if (token.get_tag() == TOK_MARKER)   // from @N@
                 {
                   const ShapeItem key = token.get_int_val();
                   Value_P value = literals.pull(key);
                   Assert(value);
                   body[b].copy(Token_loc(Token(TOK_APL_VALUE1, value),
                                          body[b].get_PC()), LOC);
                 }
            }
      }


   // literals was only a temporary storage between
   //
   Assert(literals.size() == 0);
}
//────────────────────────────────────────────────────────────────────────────
VoidCount
UserFunction::remove_TOK_VOID()
{
   // if line_starts is empty (= not yet initialized )then line_starts need
   // not be updated. Only the TOK_VOID need to be removed, and we pretend
   // that no tokens were removed (so the caller needs not care),
   //
   if (line_starts.size() == 0)
      {
        body.remove_TOK_VOID();
        return NO_VOID_TOKEN_REMOVED;
      }

   // line_starts was initialized.
   // 
size_t src_line    = Function_Line_1;
Function_PC dst_PC = Function_PC_0;

   // trailing empty lines (e.g. a label-only last line like "LABEL:" just
   // before ∇) were recorded by parse_body() with start PC == body.ssize()
   // at the time, i.e. one past every valid src_PC below. The src_PC scan
   // loop below can therefore never reach them, so line_starts[] would be
   // left holding this stale, pre-shrink sentinel for such lines unless we
   // patch it up afterwards (see below).
   //
const Prefix_PC old_body_size = body.end_PC();

   // be careful not to increment src_PC if a line is empty!

   loop(src_PC, body.ssize())
      {
        // take care of empty lines. Empty lines have the same PC as the
        // current line (src_line)
        //
        while ((src_line + 1) < line_starts.size() &&
               Prefix_PC(src_PC) == line_starts[src_line + 1])
           {
             // src_PC has reached the first token of the next line.
             // Increment the current line and adjust line_starts.
             ++src_line;
             line_starts[src_line] = Prefix_PC(dst_PC);
           }

        if (body[src_PC].get_tag() != TOK_VOID)
           {
             if (src_PC != dst_PC)   body[dst_PC].move_from(body[src_PC], LOC);
             ++dst_PC;
           }
        else   // TOK_VOID
           {
             // Do not copy but ignore (skip) body[src_PC]. After the removal
             // of the TOK_VOID have all GOTO_PC targets above src_PC become
             // are too high (by 1). Adjust these tokens.
             //
             loop(bb, body.ssize())
                 {
                   Token_loc & tok = body[bb];
                   if (tok.get_tag() == TOK_GOTO_PC)
                      {
                        const int64_t tok_pc = tok.get_int_val();
                        if (tok_pc >= src_PC)   tok.set_int_val(tok_pc - 1);
                      }
                
                 }
           }
      }

const VoidCount ret = VoidCount(body.ssize() - dst_PC);
   body.resize(dst_PC);

   // patch up trailing empty lines that the src_PC scan above could not
   // reach (see comment at old_body_size above); they still hold their
   // stale pre-shrink start PC and must be moved to the (shrunk) end of
   // the body, same as line_starts[0] below.
   //
   for (size_t l = src_line + 1; l < line_starts.size(); ++l)
       {
         if (line_starts[l] == old_body_size)   line_starts[l] = Prefix_PC(dst_PC);
       }

   line_starts[0] = Prefix_PC(dst_PC - 1);   // convention: the end of body

   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::set_trace_stop(const std::vector<Function_Line> & B, bool stop)
{
   // Sort B, so that stop_lines resp. trace_lines will be sorted.
   //
std::vector<bool> ts_lines;

   // clear all bits in ts_lines
   loop(ts, line_starts.size())   ts_lines.push_back(false);

   // set all bits in B, ignorin invalid ones.
   loop(b, B.size())
      {
        const Function_Line Bb = B[b];
        if (Bb < 1)                          continue;   // not valid
        if (Bb >= int(line_starts.size()))   continue;   // not valid
        ts_lines[Bb] = true;
      }

   if (stop)   // ⎕STOP or S∆
      {
        stop_lines.clear();
        loop(ts, ts_lines.size())
           {
             if (ts_lines[ts])   stop_lines.push_back(Function_Line(ts));
           }
      }
   else        // ⎕TRACE or T∆
      {
        trace_lines.clear();
        loop(ts, ts_lines.size())
           {
             if (ts_lines[ts])   trace_lines.push_back(Function_Line(ts));
           }
      }

   parse_body(LOC, false);
}
//════════════════════════════════════════════════════════════════════════════
UserFunction *
UserFunction::fix(const UCS_string & text, int & err_line,
                  bool keep_existing, const char * loc,
                  const UTF8_string & creator, bool quiet,
                  bool honor_lock)
{
   Log(LOG_UserFunction__fix)
      {
        CERR << "fix pmode=user function:" << endl << text << endl
             <<  "------------------- UserFunction::fix() --" << endl;
      }

UserFunction * ufun = new UserFunction(text, loc, creator,
                                       /* macro = */ false, quiet);
const char * info = ufun->get_error_info();
   err_line = ufun->get_error_line();

const bool bad_function = info || err_line != -1;
   if (bad_function)   // something went wrong
      {
        if (info)
           {
             Log(LOG_UserFunction__fix)   CERR << "Error: " << info << endl;
             MORE_ERROR() << info;
           }
         else info = "Error";

         if (err_line == 0)
           {
             MORE_ERROR() << info << " in function header";
             Log(LOG_UserFunction__fix) CERR << "Bad header line" <<  endl;
           }
         else if (err_line > 0)
           {
             UCS_string & more = MORE_ERROR();
             more << info << " in function line [" << err_line << "] of:\n";
             loop(l, ufun->get_text_size())
                more << "[" << l << "] " << ufun->get_text(l) << "\n";

             Log(LOG_UserFunction__fix)
                CERR << "Bad function line: " << err_line << endl;
           }

        delete ufun;
        return 0;
      }

const Symbol * symbol = Workspace::lookup_symbol(ufun->header.get_name());

   // old_ufun (if any) must not be deleted until ufun has passed every
   // remaining validation step below and is about to be bound to the
   // symbol: deleting it early (as this code used to, right here)
   // leaves the symbol pointing at freed memory on every error path
   // between here and the final set_NC() -- e.g. compute_if_else_targets()
   // failing just below -- confirmed via a real UAF SIGSEGV (redefine an
   // existing function with a malformed →→/←→ body, then call it).
   //
const UserFunction * old_ufun = 0;
   if (cFunction_P old_function = symbol->get_function())
      {
        if (keep_existing)
           {
             Log(LOG_UserFunction__fix)
                {
                  CERR << "not fixing '" << ufun->header.get_name()
                       << "' (function already exists, "
                          "and keep_existing is set)" << endl;
                }
             err_line = 0;
             delete ufun;
             return 0;
           }

        // check that the function is allowed to be defined,
        // E.g. it is not on the )SI stack and is not native
        //
        if (const char * reason = symbol->cant_be_defined())
           {
             Log(LOG_UserFunction__fix)
                {
                  CERR << "not fixing '" << ufun->header.get_name()
                       << "' (function already exists, and " << reason << endl;
                }

             // was logged only under LOG_UserFunction__fix (a debug flag,
             // never user-visible) -- the caller (⎕FX) had nothing to
             // show but its own generic "row 1 is bad"-looking error
             // code, indistinguishable from an actual bad header. See
             // Bugs27 #35.
             MORE_ERROR() << "'" << ufun->header.get_name()
                          << "' was not (re-)defined: " << reason;

             err_line = 0;
             delete ufun;
             return 0;
           }

        if (old_function->is_native())
           {
             MORE_ERROR() << "Attempt to re-define native function '"
                          << old_function->get_name() << "'. ⎕EX it first.";
             delete ufun;   // DEFN_ERROR throws; ufun would otherwise leak
             DEFN_ERROR;
           }

        // Bugs28 #92: cant_be_defined() above tests only is_called()
        // and the name class, not the lock bit -- so monadic ⎕FX
        // (unlike the ∇ editor, which already checks
        // get_exec_properties()[0] in Nabla::open_existing_function())
        // silently replaced a locked function, and the replacement was
        // not itself locked. Thrown as DEFN_ERROR, the same way the
        // is_native() case just above is (not cant_be_defined()'s
        // plain "return 0" a few lines up, which leaves the caller no
        // way to distinguish this from any other silent ⎕FX failure).
        // honor_lock is false only for dyadic ⎕FX: its left argument
        // IS the documented way to change a function's execution
        // properties, including deliberately relocking/unlocking it
        // (testcases/Quad_FX.tc's own "execution properties" regression
        // repeatedly redefines a locked function this way), so it must
        // stay exempt from this check.
        //
        if (honor_lock && old_function->get_exec_properties()[0])
           {
             MORE_ERROR() << "Attempt to re-define locked function '"
                          << old_function->get_name() << "'. ⎕EX it first.";
             delete ufun;   // DEFN_ERROR throws; ufun would otherwise leak
             DEFN_ERROR;
           }

        old_ufun = old_function->get_func_ufun();
        Assert(old_ufun);
      }
   else if (!(symbol->top_of_stack()->get_NC() & NC_DEFINABLE))
      {
        // the name is not free, e.g. a variable. set_NC() below would
        // throw the DEFN_ERROR, but after ufun was created and ufun (and
        // the values in its body) would leak, i.e. show up as stale
        // values in every subsequent )CHECK.
        //
        delete ufun;
        DEFN_ERROR;
      }

   ufun->optimize_labels();
   ufun->optimize_label_vectors();
   if (ufun->compute_if_else_targets())
      {
        delete ufun;
        return 0;
      }

   // only now -- past every remaining error path -- is it safe to free
   // the old function. A lambda (e.g. H←{...} re-fixed by ⎕FX) is
   // reference counted: the body of the function that contains it still
   // refers to it (Bugs35 #47).
   //
   if (old_ufun && old_ufun->is_lambda())
      const_cast<UserFunction *>(old_ufun)->decrement_refcount(LOC);
   else
      delete old_ufun;

   // finally: bind function to symbol
   //
   if (ufun->header.LO())   ufun->header.FUN()->set_NC(NC_OPERATOR, ufun);
   else                     ufun->header.FUN()->set_NC(NC_FUNCTION, ufun);

   Log(LOG_UserFunction__fix)
      {
        CERR << " addr " << voidP(ufun) << endl;
        ufun->print(CERR);
        CERR <<  "------------------- UserFunction::fix() OK --" << endl;
      }

   return ufun;
}
//════════════════════════════════════════════════════════════════════════════
UserFunction *
UserFunction::fix_lambda(Symbol & var, const UCS_string & text)
{
   // NOTE: only called from Archive::read_Function(). extract the local
   //       vars the first line of text,
   //
   /* Example: consider;

      ⎕FX 'λ←Test ⍵ ;C;D ⍝⍝ π times Ʃ⍵' 'λ←○+/⍵'

      ⎕FX currently raises a DEFN ERROR, but will nevertheless
      create function 'Test'. Note that { ... } cannot produce this
      directly due to the ⍝ (which comments out the closing } ).

      Test will be )SAVEd (and later be )LOADed as:

      λ←λ1 ⍵ ;C;D ⍝⍝ π times Ʃ⍵
      λ←○+/⍵

      I.e. ⎕CR 'Test' is a plain string with 2 lines, each ending with ASCII_NL.
    */

   // split text into header_text, lvars_text, comment, and body_text, and
   // compute the signature.
   //
UCS_string header_text, lvars_text, comment, body_text;
UCS_string * dest = &header_text;
int signature = SIG_FUN | SIG_Z;


   loop(t, text.size())
       {
         const Unicode uni = text[t];
         switch(uni)
            {
              case UNI_CHI:            signature |= SIG_X;    break;
              case UNI_OMEGA:          signature |= SIG_B;    break;
              case UNI_ALPHA_UNDERBAR: signature |= SIG_LO;   break;
              case UNI_OMEGA_UNDERBAR: signature |= SIG_RO;   break;
              case UNI_ALPHA:          signature |= SIG_A;    break;
              case UNI_SEMICOLON:      dest = &lvars_text;    break;
              case UNI_NUMBER_SIGN:    dest = &comment;       break;
              case UNI_COMMENT:        dest = &comment;       break;
              case UNI_SPACE:
                   if (dest == &lvars_text)   dest = &comment;
                   break;

              case UNI_LF:   // end of header
                   dest = 0;
                   for (++t; t < text.ssize();)   body_text << text[t++];
                   break;                          // header line done
              default:                                        break;
            }
           
         if (dest)   *dest << uni;
       }

   lvars_text.remove_trailing_whitespaces();
   body_text.remove_trailing_whitespaces();
   body_text << comment;

   // append lvars_text to body (as in e.g. { ... ;C;D }. This will also
   // parse the lvars_text needed below.
   //
   body_text << lvars_text;

Parser parser(PM_FUNCTION, LOC, false);
   parser.set_lambda_body();   // body_text may end with ;C;D
Token_string body;
Function_PC end_PC = Function_PC_0;   // the end of body_text
   // if parsing fails at this point, then something is wrong in )SAVE
   //
   try
      {
        if (const ErrorCode ec = parser.parse(body_text, body, true, &end_PC))
           {
             CERR << "Parsing '" << body_text << "' failed (" << ec << ")."
                  << endl;
             return 0;
           }
      }
   catch (const Error & err)
      {
        CERR << "Parsing '" << body_text << "' failed ("
             << err.get_error_code() << ")." << endl;
        return 0;
      }

Function_PC last_statement = Function_PC(-1);
   for (int b = body.size() - 1; b; --b)
       {
         if (body[b].get_tag() == TOK_DIAMOND)
            {
              last_statement = Function_PC(b);
              break;
            }
       }

   {
     body.insert_2(last_statement);
     Token ret_lambda(TOK_RETURN_SYMBOL, &Workspace::get_v_LAMBDA());
     const int64_t trace = 0;
     Token tok_endl(TOK_ENDL, trace);
     // ret_lambda and tok_endl stand for the end of body_text, which is
     // the PC of the first ; of the trailing local variables (if any)
     // since they are removed from the lambda text.
     //
     Function_PC pc_end = end_PC;
     for (ShapeItem b = body.ssize() - 2; b >= 0; b -= 2)
         {
           if (body[b].get_tag() != TOK_SEMICOL)          break;
           if (body[b + 1].get_Class() != TC_SYMBOL)      break;
           pc_end = body[b].get_PC();
         }
     body[last_statement + 1].copy(Token_loc(ret_lambda, pc_end), LOC);
     body[last_statement + 2].copy(Token_loc(tok_endl, pc_end), LOC);
   }

vector<Symbol *> local_vars;
   while (body.size() >= 2)
      {
        const Token & Tsem = body[body.size() - 2];
        const Token & Tsym = body[body.size() - 1];
        if (Tsem.get_tag() == TOK_SEMICOL && Tsym.get_Class() == TC_SYMBOL)
           {
             local_vars.push_back(Tsym.get_sym_ptr());
             body.resize(body.size() - 2);
           }
        else break;
      }

   return new UserFunction(Fun_signature(signature), LAMBDA_NUM_0,
                                         body_text, body, local_vars);
}
//────────────────────────────────────────────────────────────────────────────
// constructor for a normal (non-lambda) define function
UserFunction::UserFunction(const UCS_string txt, const char * loc,
                           const UTF8_string & _creator, bool macro,
                           bool quiet)
  : Function(ID_USER_SYMBOL, TOK_FUN2),
    Executable(txt, true, PM_FUNCTION, loc),
    creator(_creator),
    error_info("Unspecified"),
    error_line(0),   // assume header is wrong
    header(txt, macro)
{
   quiet_parse = quiet;
   if (header.get_error())
      {
        error_info = header.get_error_info();
        return;
      }

   set_creation_time(now());

   exec_properties[0] = 0;
   exec_properties[1] = 0;
   exec_properties[2] = 0;
   exec_properties[3] = 0;

   line_starts.push_back(Prefix_PC_0);   // will be set later.

   if (header.get_error() != E_NO_ERROR)   // bad header
      {
        error_info = header.get_error_info();
        return;
      }

   // set Function::tag
   //
   if      (header.RO())   tag = TOK_OPER2;
   else if (header.LO())   tag = TOK_OPER1;
   else if (header.A())    tag = TOK_FUN2;
   else if (header.B())    tag = TOK_FUN1;
   else                    tag = TOK_FUN0;

   parse_body(loc, macro);
   if (error_line > 0)
      {
        // keep a more specific error_info set by parse_body(), e.g. an
        // unbalanced →→ ←→ ←← (Bugs35 #53)
        if (!error_info || !strcmp(error_info, "Unspecified"))
           error_info = "Error in function body";
        return;
      }

   if (UserPreferences::uprefs.discard_indentation)   // really ?
      {
        Multi_line_SM sm;
        loop(l, get_text_size())
            {
              UCS_string line = get_text(l);
              const ShapeItem multi_pos = line.multi_pos();
              if (multi_pos != -1)   sm.next(line[multi_pos]);
              if (sm.inside_multi())
                 {
                   line.remove_leading_and_trailing_whitespaces();
                   set_text(l, line);
                 }
            }
      }

   // show blank-separated local names with semicolons (Bugs35 #49)
   //
   if (!macro)   set_text(0, UserFunction_header::canonical_locals(get_text(0)));

   error_line = -1;   // no error
   error_info = 0;
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_fill_AB(cValue_R A, cValue_R B) const
{
   // Identity (LRM Figure 20, "Defined Operations": Z<-R), NOT a genuine
   // eval_AB(A, B): a defined function is arbitrary user code, unlike a
   // primitive, and is not known to be pure/total/side-effect-free --
   // actually running it here (tried once, via a new Workspace::
   // run_pushed_SI() driver, for Bugs30 #11/#12) was reverted after
   // confirming live that a dfn with any observable effect fires that
   // effect as an invisible side channel of internal shape/prototype
   // bookkeeping the user never triggered, e.g.
   // (,{⎕<-'SIDE-EFFECT' ⋄ ⍴⍵}⍤1)0 3⍴0 printed SIDE-EFFECT even though
   // the empty frame means the function was never meant to run on real
   // data at all. This is exactly why APL2's own fill function for
   // defined operations is identity, discarding A and the function,
   // while only *primitive* non-scalar functions get "the function
   // itself" as their fill function (Figure 20) -- primitives are known
   // safe to invoke on synthetic fill data, defined functions are not.
   //
   // This method (and its eval_fill_B() sibling below) is reached only
   // from Each and Outer Product, the two operators APL2's Figure 20
   // actually documents a fill function for -- confirmed still correct
   // there for a defined operand via real APL2 itself (Bugs30 #12: ≡Z,
   // ⍴↑Z and 2⎕TF all matched this identity's output on Blake McBride's
   // own examples). The Rank operator (⍤), not part of APL2 at all and
   // never routed through eval_fill_AB()/eval_fill_B(), has its own
   // separate eval_rank_fill_AB()/eval_rank_fill_B() pair instead
   // (Function.hh, default genuinely evaluates) -- real Rank-operator
   // implementations (Dyalog, APL2 extensions) genuinely evaluate a
   // defined LO there, unlike Each/Outer Product, and Bif_OPER2_RANK.cc
   // drives that safely to completion. See its own comments for why the
   // two operators warrant different treatment despite both being
   // "defined function used as an operand on an empty argument".
   //
Value_P Z(static_cast<Value *>(const_cast<cValue *>(&B)), LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
UserFunction::eval_fill_B(cValue_R B) const
{
   // see eval_fill_AB() just above.
Value_P Z(static_cast<Value *>(const_cast<cValue *>(&B)), LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UserFunction::line_prefix(Function_Line l) const
{
char cc[40];
   if      (text.size() > 100)   SPRINTF(cc, "[%3d] ", l)
   else if (text.size() > 10)    SPRINTF(cc, "[%2d] ", l)
   else                          SPRINTF(cc, "[%d] ",  l)
   return UCS_ASCII_string(cc);
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction::print_body_by_line(const char * where) const
{
   CERR << where << endl;
   loop(line, line_starts.size())
      {
        CERR << "[" << line << "]:";
        Prefix_PC next = body.end_PC();   // assume last line
        if ((line + 1) < int(line_starts.size()))
           next = line_starts[line + 1];
        loop(offset, next - line_starts[line])
            CERR << " " << body[int(line_starts[line]) + offset].get_Class();
        CERR << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
ostream &
UserFunction::print_val_or_fun(ostream & out, const Token & tok)
{
   if (tok.is_function())         out << *tok.get_function();
   else if (tok.is_apl_val())     out << tok;
   else if (tok.is_void())        out << "((VOID))";
   else                           FIXME;

   return out;
}
//════════════════════════════════════════════════════════════════════════════
