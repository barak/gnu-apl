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
#include "Token_string.hh"

//────────────────────────────────────────────────────────────────────────────
int
Token_string::find_closing_bracket(int pos) const
{
   Assert(at(pos).get_tag() == TOK_L_BRACK);

int others = 0;

   for (ShapeItem p = pos + 1; p < ssize(); ++p)
       {
         Log(LOG_find_closing)
            CERR << "find_closing_bracket() sees " << at(p) << endl;

         if (at(p).get_tag() == TOK_R_BRACK)
            {
             if (others == 0)   return p;
             --others;
            }
         else if (at(p).get_tag() == TOK_R_BRACK)   ++others;
       }

   MORE_ERROR() << "No closing ] for opening ]";
   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
int
Token_string::find_closing_parent(int pos) const
{
   Assert1(at(pos).get_Class() == TC_L_PARENT);

int others = 0;

   for (ShapeItem p = pos + 1; p < ssize(); ++p)
       {
         Log(LOG_find_closing)
            CERR << "find_closing_bracket() sees " << at(p) << endl;

         if (at(p).get_Class() == TC_R_PARENT)
            {
             if (others == 0)   return p;
             --others;
            }
         else if (at(p).get_Class() == TC_R_PARENT)   ++others;
       }

   MORE_ERROR() << "No closing ) for opening (";
   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
int
Token_string::find_opening_bracket(int pos) const
{
   Assert(at(pos).get_tag() == TOK_R_BRACK);

int others = 0;

   for (int p = pos - 1; p >= 0; --p)
       {
         Log(LOG_find_closing)
            CERR << "find_opening_bracket() sees " << at(p) << endl;

         if (at(p).get_tag() == TOK_L_BRACK)
            {
             if (others == 0)   return p;
             --others;
            }
         else if (at(p).get_tag() == TOK_R_BRACK)   ++others;
       }

   MORE_ERROR() << "No opening [ for closing ]";
   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
int
Token_string::find_opening_curly(int pos) const
{
   Assert(at(pos).get_Class() == TC_R_CURLY);

int others = 0;

   for (int p = pos - 1; p >= 0; --p)
       {
         Log(LOG_find_closing)
            CERR << "find_opening_bracket() sees " << at(p) << endl;

         if (at(p).get_Class() == TC_L_CURLY)
            {
             if (others == 0)   return p;
             --others;
            }
         else if (at(p).get_Class() == TC_R_CURLY)   ++others;
       }

   MORE_ERROR() << "No opening { for closing }";
   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
int
Token_string::find_opening_parent(int pos) const
{
   Assert(at(pos).get_Class() == TC_R_PARENT);

int others = 0;

   for (int p = pos - 1; p >= 0; --p)
       {
         Log(LOG_find_closing)
            CERR << "find_opening_bracket() sees " << at(p) << endl;

         if (at(p).get_Class() == TC_L_PARENT)
            {
             if (others == 0)   return p;
             --others;
            }
         else if (at(p).get_Class() == TC_R_PARENT)   ++others;
       }
   MORE_ERROR() << "No opening ( for closing )";
   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
operator << (ostream & out, const Token_string & tos)
{
   out << "[" << tos.size() << " token]: ";
   loop(t, tos.size())   CERR << "⏩" << tos[t] << "  ";
   out << endl;
   return out << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::print(ostream & out, int details) const
{
const bool PC  = details & 1;
const bool VAL = details & 2;
   
   loop(pc, size())
       {
         const Token & tok = at(pc);
         if (PC)   out << "    [PC=" << setw(2) << pc << "] ";
         out << "⏩" << tok;
         if (VAL)
            {
              switch(tok.get_ValueType())
                 {
                   case TV_INT: out << ":" << tok.get_int_val();   break;
                   case TV_FLT: out << ":" << tok.get_flt_val();   break;
                   default:                                        break;
                 }
            }
         if (PC)   out << endl;
         else      out << "  ";
       }

   out << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::insert_1(int pos)
{
   push_back(Token(TOK_VOID));
   for (int from = size() - 2; from > pos; --from)
       {
         at(from + 1).move_from(at(from), LOC);   // shift towards end
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::insert_2(int pos)
{
   push_back(Token(TOK_VOID));
   push_back(Token(TOK_VOID));
   for (int from = size() - 3; from > pos; --from)
       {
         at(from + 2).move_from(at(from), LOC);   // shift towards end
       }
}
//────────────────────────────────────────────────────────────────────────────
VoidCount
Token_string::remove_TOK_VOID()
{
ShapeItem dst = 0;

   loop(src, size())
       {
         if (at(src).get_tag() == TOK_VOID)   continue;   // ignore (skip)
         if (src != dst)   at(dst).move_from(at(src), LOC);
         ++dst;
       }

const VoidCount ret = VoidCount(size() - dst);
   resize(dst);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
ShapeItem
Token_string::replace_segment(const Token_string & src, ShapeItem pos)
{
   loop(s, src.size())
       {
         at(pos).clear(LOC);
         new (&at(pos++)) Token(src[s], LOC);
       }
   return pos;
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::reverse_from_to(ShapeItem from, ShapeItem to)
{
Token * t1 = &at(from);
Token * t2 = &at(to);
   Assert(0 <= from);
   Assert(from <= to);
   Assert(to <= ShapeItem(size()));

   while (t1 < t2)   t1++->swap_token(*t2--);
}
//────────────────────────────────────────────────────────────────────────────

