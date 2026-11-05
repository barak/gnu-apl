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
#include "StateIndicator.hh"
#include "Token_string.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Token_string::all_brackets_closed() const
{
   /// an opener still waiting for its closer: its token index (for the
   /// caret) and the closing tag it expects
   struct Open_bracket
      {
        int       pos;        ///< index of the opening token in *this
        TokenTag  closer;     ///< the matching closing tag
      };

vector<Open_bracket> expected;

   // throw for the outermost of the openers expected[from...] (which have
   // no closer): with the statement in line 2 (which the ErrorCode returned
   // by Parser::match_par_bra() does not have).
   //
const auto throw_unclosed = [&](size_t from)
   {
     const Open_bracket & outer = expected[from];
     const ErrorCode ec =
           outer.closer == TOK_R_BRACK ? E_UNBALANCED_L_BRACK
         : outer.closer == TOK_R_CURLY ? E_UNBALANCED_L_CURLY
                                       : E_UNBALANCED_L_PARENT;

     if (Workspace::more_error().size() == 0)
        MORE_ERROR() << Error::error_name(ec);
     Error error(ec, LOC);
     build_error_line_2(error, outer.pos, outer.pos);

     if (StateIndicator * si = Workspace::SI_top())
        StateIndicator::get_error(si) = error;

     throw error;
   };

   loop(s, size())
      {
        const TokenTag tag = at(s).get_tag();
        ErrorCode ec;   // the closing tag has no matching opener: unbalanced
        switch(tag)      // *right* bracket, regardless of which case below
           {
             default: continue;   // not (, ), [, ], {, ot }.

             case TOK_DIAMOND:
                  // a ◊ ends the statement (or the lambda statement):
                  // a ( or [ opened in it is unclosed (Bugs37 #68).
                  // Otherwise A[1◊2] or (1◊2) look balanced here.
                  //
                  if (expected.size() &&
                      expected.back().closer != TOK_R_CURLY)
                     {
                       size_t from = expected.size() - 1;
                       while (from > 0 &&
                              expected[from - 1].closer != TOK_R_CURLY)
                             --from;
                       throw_unclosed(from);
                     }
                  continue;

             case TOK_L_BRACK:
                  expected.push_back({int(s), TOK_R_BRACK});    continue;
             case TOK_L_CURLY:
                  expected.push_back({int(s), TOK_R_CURLY});    continue;
             case TOK_L_PARENT:
                  expected.push_back({int(s), TOK_R_PARENT});   continue;

             case TOK_R_BRACK:  ec = E_UNBALANCED_R_BRACK;    break;
             case TOK_R_CURLY:  ec = E_UNBALANCED_R_CURLY;    break;
             case TOK_R_PARENT: ec = E_UNBALANCED_R_PARENT;   break;
           }

        if (expected.size() && tag == expected.back().closer)
           {
             expected.pop_back();   // level done
             continue;
           }

        // error: either no opener at all (highlight just the offending
        // closer), or the wrong opener (highlight opener..closer, same
        // fashion as a Prefix.cc/Path A error -- see build_error_line_2()).
        //
        const int lo = expected.size() ? expected.back().pos : int(s);

        if (Workspace::more_error().size() == 0)
           MORE_ERROR() << Error::error_name(ec);
        Error error(ec, LOC);
        build_error_line_2(error, lo, int(s));

        // Bugs28 #93: see Error::throw_parse_error()'s identical fix --
        // without this, ⎕ET/⎕EM after a fix-time-only error like "1)"
        // kept reporting whatever the *previous* statement's error was.
        // Store directly (not via the full update_error_info()), which
        // would instead rebuild error_message_2/3 from the *previous*
        // statement's still-current Executable/prefix state and
        // clobber the message build_error_line_2() just built above.
        //
        if (StateIndicator * si = Workspace::SI_top())
           StateIndicator::get_error(si) = error;

        throw error;
      }

   // an opener without its closer: report the outermost one (like
   // Parser::match_par_bra()), but with the statement in line 2 (which
   // the ErrorCode returned by match_par_bra() does not have).
   //
   if (expected.size())   throw_unclosed(0);
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::check_semicolons(bool lambda_locals) const
{
   // a ; is only valid:
   //
   // 1. between [ and ] (an index or axis), or
   // 2. between { and } (the local variables of a lambda; the lambda body
   //    itself is checked again when the lambda is created), or
   // 3. at the end of a lambda body: ;A;B... (lambda_locals)
   //
   // Otherwise the input is a SYNTAX ERROR before anything in it was
   // executed (Bugs37 #17).
   //
vector<TokenTag> openers;   // TOK_L_BRACK, TOK_L_PARENT, or TOK_L_CURLY
   loop(s, size())
      {
        const TokenTag tag = at(s).get_tag();
        switch(tag)
           {
             case TOK_L_BRACK:
             case TOK_L_PARENT:
             case TOK_L_CURLY:  openers.push_back(tag);
                                continue;

             case TOK_R_BRACK:
             case TOK_R_PARENT:
             case TOK_R_CURLY:  if (openers.size())   openers.pop_back();
                                continue;   // balance: all_brackets_closed()

             case TOK_SEMICOL:  break;
             default:           continue;
           }

        // at(s) is a ;
        //
        if (openers.size() && openers.back() == TOK_L_BRACK)   continue;
        if (openers.size() && openers.back() == TOK_L_CURLY)   continue;

        if (lambda_locals && openers.size() == 0)   // maybe 3.
           {
             bool locals = true;
             for (ShapeItem l = s; l < ssize(); l += 2)
                 {
                   if (l + 1 >= ssize()                        ||
                       at(l).get_tag() != TOK_SEMICOL          ||
                       at(l + 1).get_Class() != TC_SYMBOL)
                      {
                        locals = false;
                        break;
                      }
                 }
             if (locals)   return;
           }

        MORE_ERROR() << "; outside of [...] (only an index or an axis"
                        " can contain ;)";
        Error error(E_SYNTAX_ERROR, LOC);
        build_error_line_2(error, int(s), int(s));

        // see all_brackets_closed() above
        //
        if (StateIndicator * si = Workspace::SI_top())
           StateIndicator::get_error(si) = error;

        throw error;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::build_error_line_2(Error & error, int lo, int hi) const
{
UCS_string message_2;
int left_caret = -1;
int right_caret = -1;

   loop(q, size())
      {
        // avoid duplicate ∘ in ∘.f (like Executable::set_error_info()),
        // Bugs37 #61
        //
        if (at(q).get_tag() == TOK_JOT && q + 1 < ssize() &&
            at(q + 1).get_tag() == TOK_OPER2_OUTER)
           {
             if (q == lo)   ++lo;   // caret on the ∘. instead
             if (q == hi)   ++hi;
             continue;
           }

        int alen;   // chars added by this token itself (a leading
                    // space, if any, belongs to the gap before it)
        if (message_2.size() == 0)   // Token::error_info() assumes a non-empty ucs
                      // (it checks ucs.back() to decide on a leading
                      // space) -- nothing precedes the first token, so
                      // append its canonical text directly.
           {
             const UCS_string canon = at(q).canonical(PR_APL_FUN)
                                            .remove_pad();
             message_2 << canon;
             alen = canon.size();
           }
        else
           {
             const int len = at(q).error_info(message_2);
             alen = len < 0 ? -len : len;
           }

        if (q == lo)   left_caret  = message_2.size() - alen;
        if (q == hi)   right_caret = message_2.size() - 1;   // last char
      }

   // the same margin (the prompt) as other errors in line 2 (Bugs37 #62)
   //
UCS_string margined(Workspace::get_prompt());
const int margin = margined.size();
   margined << message_2;
   error.set_error_line_2(margined,
                          left_caret  < 0 ? left_caret  : left_caret  + margin,
                          right_caret < 0 ? right_caret : right_caret + margin);
}
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
         else if (at(p).get_tag() == TOK_L_BRACK)   ++others;
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
         else if (at(p).get_Class() == TC_L_PARENT)   ++others;
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
   emplace_back();   // TOK_VOID
   for (int from = size() - 2; from > pos; --from)
       {
         at(from + 1).move_from(at(from), LOC);   // shift towards end
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::insert_2(int pos)
{
   emplace_back();   // TOK_VOID
   emplace_back();   // TOK_VOID
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
         at(pos++).copy(src[s], LOC);
       }
   return pos;
}
//────────────────────────────────────────────────────────────────────────────
void
Token_string::reverse_from_to(ShapeItem from, ShapeItem to)
{
Token_loc * t1 = &at(from);
Token_loc * t2 = &at(to);
   Assert(0 <= from);
   Assert(from <= to);
   Assert(to <= ShapeItem(size()));

   while (t1 < t2)   t1++->swap(*t2--);
}
//════════════════════════════════════════════════════════════════════════════

