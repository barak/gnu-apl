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

#include "Bif_F12_COMMA.hh"
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_RHO.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "Common.hh"
#include "FloatCell.hh"
#include "IndexExpr.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "Parser.hh"
#include "UserPreferences.hh"
#include "PointerCell.hh"
#include "StateIndicator.hh"
#include "PrintOperator.hh"
#include "Quad_SQL.hh"
#include "Symbol.hh"
#include "SystemLimits.hh"
#include "SystemVariable.hh"
#include "Token.hh"
#include "Token_string.hh"
#include "Tokenizer.hh"
#include "Tokenizer.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
Multi_line_SM::~Multi_line_SM()
{
   if (in_string)       CERR << "*** Multiline string not closed." << endl;
   if (literal_depth)   CERR << "*** Multiline literal not closed (depth"
                             << literal_depth << ")." << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Multi_line_SM::next(Unicode triple)
{
   Log(LOG_Multi_line)
      {
        CERR << "See " << triple << triple << triple
             << ". State (before): string: " << yes_no(in_string)
             << ", literal: " << literal_depth << endl;
      }
                            
   // multiline strings are flat; the only valid triple is the end of string
   // triple.
   //
   if (in_string)
      {
        if (triple == string_end)
           {
             in_string = false;
             string_end = Unicode_0;
             return;   // OK
           }

        // unexpected triple in string. Issue a warning (but do not return
        // an error
        //
        CERR << endl << "*** WARNING: unexpected triplet "
             << triple << triple << triple << " in mult-line string." << endl
             << "    Expected triplet: " << string_end << string_end
             << string_end << " (literal depth: " << literal_depth
             << ")." << endl
             << "    The triplet was added to the multi-line string."
             << endl << endl;
        return;   // OK
      }

   // multiline literals are recursive, therefore they may copntain multiline
   // strings or literals.
   //
   if (literal_depth)
      {
        if (triple == UNI_GREATER)   // end of literal
           {
             --literal_depth;
             return;   // OK.
           }

        if (triple == UNI_LESS)   // start of a new literal
           {
            
             ++literal_depth;
             return;   // OK.
           }

        if (triple == UNI_DOUBLE_QUOTE)   // start of a """ ... """ string
           {
              in_string = true;
              string_end = UNI_DOUBLE_QUOTE;
             return;   // OK.
           }

        if (triple == UNI_LEFT_DAQ)   // start of a ««« ... »»» string
           {
              in_string = true;
              string_end = UNI_RIGHT_DAQ;
             return;   // OK.
           }
      }

   // at this point no string and no literal is ongoing. The only valid
   // triples are start of string and sstart of literal.
   //
   if (triple == UNI_GREATER)
      {
        CERR << endl << "*** ERROR: unexpected triplet >>> outside "
                        "multiline literal." << endl << endl;
        return;   // OK.
      }

   if (triple == UNI_LESS)   // start of a new literal
      {
       
        ++literal_depth;
        return;   // OK.
      }

   if (triple == UNI_DOUBLE_QUOTE)   // start of a """ ... """ string
      {
         in_string = true;
         string_end = UNI_DOUBLE_QUOTE;
        return;   // OK.
      }

   if (triple == UNI_LEFT_DAQ)   // start of a ««« ... »»» string
      {
         in_string = true;
         string_end = UNI_RIGHT_DAQ;
        return;   // OK.
      }

   CERR << endl << "*** ERROR: unexpected " << triple << triple << triple
                << " outside multiline string or literal." << endl;
   return;   // error
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
Parser::parse(const UCS_string & input, Token_string & tos, bool optimize,
              Function_PC * end_PC) const
{
   // convert input characters into token
   //
Token_string tos1;

   {
     Tokenizer tokenizer(pmode, LOC, macro);
     tokenizer.tokenize(input, tos1);      // throws on lexical failure
     if (end_PC)   *end_PC = Function_PC(tos1.size());
     tos1.all_brackets_closed();           // throws on bracket mismatch
     tos1.check_semicolons(lambda_body);   // throws on ; outside [...]
   }

   // special case: single token (to speed up ⍎)
   //
   if (tos1.size() == 1)
      {
        const ErrorCode err = parse_statement(tos1, optimize,
                                              pmode == PM_EXECUTE);
        if (err == E_NO_ERROR)   tos.push_back(tos1[0]);
        return err;
      }

   return parse(tos1, tos, optimize);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::parse(const Token_string & input, Token_string & tos,
              bool optimize) const
{
   parse_log(1, input);

   // split input line into statements (separated by ◊). Held by value
   // (not Token_string *) so that a thrown APL error -- parse_statement()
   // below throws for many malformed inputs, not just returns an
   // ErrorCode -- unwinds through ordinary RAII instead of leaking every
   // not-yet-freed statement (Blake McBride, Bugs14 #4).
   //
std::vector<Token_string> statements;
std::vector<Function_PC> diamond_PCs;   // the PCs of the ◊ between statements
   {
     // mutate statements.back() in place rather than a reused/reassigned
     // local: Token_string's operator= is private ("prevent accidental
     // copying"), so it cannot be reassigned to start the next statement.
     //
     statements.push_back(Token_string());
     int curly_depth = 0;
     loop(idx, input.size())
        {
          const Token & tok = input[idx];
          switch(tok.get_tag())
             {
               case TOK_L_CURLY:
                    ++curly_depth;
                    statements.back().push_back(input[idx]);
                    break;

               case TOK_R_CURLY:
                    if (curly_depth)   --curly_depth;
                    else
                       {
                         return E_UNBALANCED_R_CURLY;
                       }
                    statements.back().push_back(input[idx]);
                    break;

               case TOK_DIAMOND:
                    if (curly_depth)   // ◊ inside { ... }
                       {
               //        return E_ILLEGAL_DIAMOND;  // single statement { ... }
                         statements.back().push_back(input[idx]);   // multi statement { ... }
                       }
                    else               // normal ◊
                       {
                         diamond_PCs.push_back(input[idx].get_PC());
                         statements.push_back(Token_string());
                       }
                    break;
          default:
               statements.back().push_back(input[idx]);
             }
        }
   }

   loop(s, statements.size())
      {
        Token_string & stat = statements[s];
        if (const ErrorCode err = parse_statement(stat, optimize,
                                                  pmode == PM_EXECUTE))
           return err;

        if (s)   tos.emplace_back(Token(TOK_DIAMOND), diamond_PCs[s - 1]);

        loop(t, stat.size())
           {
             tos.emplace_back();
             tos.back().move_from(stat[t], LOC);
           }
      }

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Assign_state
Parser::get_assign_state(Token_string & tos, ShapeItem pos)
{
   // this function is called by an optimization for static [] or [N] cases.
   // The benefit is somewhat marginal, we therefore leave the more
   // complicated cases (i.e. those involving ←) to the prefix parser
   // and return ASS_unknown instead of a more precise result.
   //
   while (pos < tos.ssize())
       {
         if (tos[pos++].get_Class() == TC_ASSIGN)   return ASS_unknown;
       }

   return ASS_none;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::get_multiline_status(vector<Multiline_status> & status,
                             const UCS_string_vector & text,
                             bool fun_header, bool strings)
{
   status.reserve(text.size());

UCS_string scope;
   if (strings)   scope << UNI_DOUBLE_QUOTE << UNI_LEFT_DAQ << UNI_RIGHT_DAQ;
   else           scope << UNI_LESS << UNI_GREATER;

size_t start = 0;
   if (fun_header)   // define function header
      {
        status.push_back(MLS_Function_header);
        ++start;
      }

Multiline_status current = MLS_APL_text;

   for (size_t l = start; l < text.size(); ++l)
       {
         const UCS_string & line = text[l];
         const int pos = line.multi_pos();
         if (pos == -1 || !scope.contains(line[pos]))   // same status
            {
              status.push_back(current);
              continue;
            }

         // status change. Ideally line[pos] tells the status.
         //
         switch(line[pos])
            {
              case UNI_LESS:
              case UNI_LEFT_DAQ:  current = MLS_Start_of_multi;
                   status.push_back(MLS_Start_of_multi);
                   current = MLS_Inside_multi;
                   continue;

              case UNI_GREATER:
              case UNI_RIGHT_DAQ:
                   status.push_back(MLS_End_of_multi);
                   current = MLS_APL_text;
                   continue;

              default:   // ' or ""
                   if (current <= MLS_APL_text)   // start of multi-line string
                      {
                        status.push_back(MLS_Start_of_multi);
                        current = MLS_Inside_multi;
                      }
                   else                           // end of multi-line string
                      {
                        status.push_back(MLS_End_of_multi);
                        current = MLS_APL_text;
                      }
                   continue;
            }
       }

   // all lines processed. Check final status
   //
   if (current == MLS_Start_of_multi || current == MLS_Inside_multi)
      {
         // multi-line string started but not ended.
         //
         MORE_ERROR() << "No end of multi-line "
                      << (strings ? "string" : "literal") << " found.";
         return fun_header ? E_DEFN_ERROR : E_SYNTAX_ERROR;;
      }

   Log(LOG_Multi_line)
      {
        CERR << "At " << LOC << ": scope: " << scope << endl;
        loop(t, text.size())
            {
              CERR << "[" << t << "]: S=" << status[t]
                   << " '" << text[t] << "'" << endl;
            }
      }

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::match_par_bra(Token_string & tos, bool backwards)
{
std::vector<ShapeItem> stack;
   loop(s, tos.ssize())
       {
         const ShapeItem t = backwards ? (tos.ssize() - 1) - s : s;
         ErrorCode ec;   // anticipated error code, not used in most cases
         TokenClass tc_peer;
         switch(tos[t].get_Class())
           {
             // for [, (, or {, push the position onto stack. Note that
             // TOK_SEMICOL also has class TC_L_BRACK, but we don't want it
             // here.
             //
             case TC_L_BRACK:   // NOTE: also includes TOK_SEMICOL, so we check
                  if (tos[t].get_tag() != TOK_L_BRACK)   continue;   // SEMICOL
                  /* fall through */
             case TC_L_PARENT:
             case TC_L_CURLY:
                  stack.push_back(t);
                  continue;

             case TC_R_BRACK:
                  ec = E_UNBALANCED_R_BRACK;   // for empty stack below
                  tc_peer = TC_L_BRACK;
                  break;

             case TC_R_PARENT:
                  ec = E_UNBALANCED_R_PARENT;    // for empty stack below
                  tc_peer = TC_L_PARENT;
                  break;

             case TC_R_CURLY:
                  ec = E_UNBALANCED_R_CURLY;     // for empty stack below
                  tc_peer = TC_L_CURLY;
                  break;

             default:
                  continue;
           }

          // at this point, a closing ), ], or } was detected
          //
          if (stack.size() == 0)   return ec;

           const ShapeItem t1 = stack.back();
           stack.pop_back();

          if (tos[t1].get_Class() != tc_peer)   return ec;

          const ShapeItem diff = (t > t1) ? t - t1 : t1 - t;
          tos[t].set_int_val2(diff);
          tos[t1].set_int_val2(diff);
       }

   // at this point all [ ( or { should have been matched (and therefore
   // stack shuld be empty. If not:  return syntax error of the outer token
   //
   if (stack.size())
      {
        const TokenClass outer = tos[stack[0]].get_Class();
        if (outer == TC_L_BRACK)    return E_UNBALANCED_L_BRACK;
        if (outer == TC_L_PARENT)   return E_UNBALANCED_L_PARENT;
        if (outer == TC_L_CURLY)    return E_UNBALANCED_L_CURLY;
        FIXME;
      }

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
/// return true iff tos[pos] is (the end of) a function, seeing through
/// any chain of closing parens -- e.g. tos[pos] == ) in (+/)[1] correctly
/// sees the / inside. Deliberately does not special-case TC_SYMBOL (the
/// way plan.txt's /⌿\⍀ rules would), so it stays safe to call this
/// early in Parser.cc, before symbols carry their resolved (TOK_P_SYMB
/// etc.) tag.
static bool
ends_in_function(const Token_string & tos, int pos)
{
   if (tos[pos].is_function())          return true;
   if (tos[pos].get_Class() != TC_R_PARENT)   return false;
   if (pos == 0)                        return false;   // syntax error
   return ends_in_function(tos, pos - 1);
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::optimize_literal_axes(Token_string & tos)
{
   // stupid macOS warns about DO_FT_LITERAL_AXIS && DO_FT_LITERAL_INDEX,
   // so we split it.
   //
   if (!DO_FT_LITERAL_AXIS)    return false;
   if (!DO_FT_LITERAL_INDEX)   return false;

   // replace [ ] or [ N ] by their complete index or axis, as to relieve
   // the Prefix parser.
bool progress = false;

   loop(src, tos.ssize() - 1)
       {
         if (tos[src].get_tag() != TOK_L_BRACK)   continue;

         // the replacement of [ ... ] gets the PC of its leftmost token [
         //
         const Function_PC pc_L = tos[src].get_PC();
         const Assign_state assign_state = get_assign_state(tos, src);
         if (assign_state == ASS_unknown)   continue;

         // case 1: [ ] 
         //
         const Token & T1 = tos[src + 1];
         if (T1.get_tag() == TOK_R_BRACK)   // empty index [ ]
            {
              IndexExpr * idx = new IndexExpr(assign_state, LOC);
              Log(LOG_delete)
                 CERR << "new    " << voidP(idx) << " at " LOC << endl;
              tos[src++].copy(Token_loc(Token(TOK_INDEX, *idx), pc_L), LOC);   // replace [
              tos[src].clear(LOC);                                       // replace ]
              // idx has rank 0 here, so the Token ctor above always took
              // its "case 2a" branch (Token.cc): tag became TOK_AXIS and
              // idx->extract_axis()'s *return value* was stored, not idx
              // itself -- idx is now unreferenced anywhere. Without this
              // delete, every literal "[]" leaked one IndexExpr (~125
              // bytes measured: 900k repeated ⍎'A[]' -> 133 MB RSS vs
              // 20 MB for an axis-free statement).
              delete idx;
              OptmizationStatistics::count(OPTI_FT_LITERAL_AXIS);
              progress = true;
              continue;
            }

         /* case 2: [N]. This case has two subcases:

            2a. f[B] with (system-) function f, or
            2b. V[B] with unknown V (most likely a SYMBOL or a Value

            case 2a. is fairly frequent with f being ⎕FIO or ⎕CR.
          */
         if ((src + 2) < tos.ssize()               &&   // tos[src+2] exists
             tos[src + 2].get_tag() == TOK_R_BRACK &&   // and is ]
             T1.get_Class() == TC_VALUE            &&   // value N
             T1.get_apl_val()->is_int_scalar())         // scalar N
            {
              /* at this point we either have a function axis f[N] or else a
                 vector index V[N]. In some cases (system functions, APL
                 primitives) we can decide that here; if not then we have
                 to defer the decision to the Prefix parser.
               */
              // this parse-time literal-axis optimization is the path
              // actually taken for e.g. ⌽[N] or ,[N] with a literal N in
              // brackets (the common case) -- bypassing it entirely
              // skips the runtime axis-range checks in
              // Value::get_single_axis()/catenate_or_laminate() etc.
              // Narrowing an out-of-range N (e.g. 4294967298, or 65537)
              // directly into sAxis (int16_t) here wrapped it into a
              // small in-range value *before* any of those checks ever
              // ran -- ⌽[4294967298]M silently reversed axis 2 instead
              // of raising AXIS_ERROR, same for a huge ,[N] axis. Only
              // take this shortcut when N actually fits in sAxis; leave
              // out-of-range N for the (checked) Prefix-parser path.
              //
              // N itself may be near-int but too large to even fit an
              // int64_t (e.g. 1E30): get_near_int() throws DOMAIN_ERROR
              // in that case (unlike is_int_scalar()/is_near_int(), which
              // consider it int-like since its fractional part rounds off
              // to nothing) -- so only call get_near_int() once N is known
              // to fit, and treat "doesn't fit" the same as "too large for
              // sAxis": defer to the value axis / Prefix-parser path.
              Cell cache;
              const Cell & c1 = T1.get_apl_val()->get_cscalar(cache);
              // is_near_int64_t() alone is true for a complex value like
              // 1J1 too -- ComplexCell::is_near_int64_t() only checks
              // that *both parts* independently round to near-integers,
              // not that the imaginary part is ≈0 (i.e. that the value
              // is actually a real integer). get_near_int() then throws
              // DOMAIN_ERROR for the non-zero imaginary part, escaping
              // straight out of the parser the same way Bugs28 #32's
              // A⍴B case does (no failed statement/carets, ⎕EM/⎕ET
              // unset, ⎕FX refuses to define an otherwise-valid
              // function) -- exclude complex values here explicitly and
              // defer to the value axis / Prefix-parser path instead.
              //
              const bool fits_int64 = c1.is_near_int64_t() &&
                                      !c1.is_complex_cell();
              const APL_Integer wide_axis = fits_int64 ? c1.get_near_int()
                                                         : 0;
              // tos[src-1].is_function() alone only looks at the single
              // token immediately left of [, so a parenthesized derived
              // function -- (+/)[1], as opposed to the bare +/[1] -- had
              // ) there instead of a function-typed token, failed this
              // check, and fell through to the value-index path below
              // instead. Since indexing a function value isn't otherwise
              // meaningful, the axis was then silently dropped rather
              // than erroring, and (+/)[1]/(+/)[2] on a matrix both
              // silently reduced the same (last) axis regardless of
              // which was asked for -- found investigating Bugs26 #5
              // (same root cause: a naive one-token-back check that
              // doesn't see through a closing paren to the real function
              // underneath). ends_in_function() peels through a chain of
              // closing parens; an earlier version of this fix tried
              // reusing the (now-deleted, and long-dead-code even before
              // that: unreferenced anywhere since before this repo's own
              // history begins) Parser::check_if_value(), which did the
              // same paren-peeling for the unrelated /⌿\⍀ decision -- but
              // its TC_SYMBOL case assumed symbols already carry their
              // resolved tag (TOK_P_SYMB etc.), true only once Prefix.cc
              // runs, not yet at this early Parser.cc pass. Reusing it
              // misread a bare, not-yet-tagged variable reference like
              // the A in Z←A[3] as a function instead of a value,
              // turning plain value-indexing into a bogus SYNTAX ERROR
              // (Bracket_Index.tc regression, caught by the full suite).
              if (src > 0 && ends_in_function(tos, src - 1)   // function axis
                  && fits_int64
                  && wide_axis >= -32000 && wide_axis <= 32000)
                 {
                   const sAxis function_axis = wide_axis;

                   Token tok_axis(TOK_FAXIS, function_axis);
                   tos[src++].clear(LOC);                         // invalidate [
                   tos[src++].copy(Token_loc(tok_axis, pc_L), LOC);
                   tos[src].clear(LOC);                           // invalidate ]
                   OptmizationStatistics::count(OPTI_FT_LITERAL_AXIS);
                   progress = true;
                 }
              else                                         // value axis
                 {
                   IndexExpr * idx = new IndexExpr(assign_state, LOC);
                   idx->add_index(T1.get_apl_val());

                   // T1 is an axis [ N ].
                   tos[src++].clear(LOC);                         // invalidate [
                   tos[src++].copy(Token_loc(Token(TOK_INDEX, *idx), pc_L), LOC);
                   tos[src].clear(LOC);                           // invalidate ]
                   // idx has rank 1 here, so the Token ctor above always
                   // took its "case 2a" branch and never stored idx
                   // itself -- see the "[]" case above for the full
                   // explanation; same leak, same fix.
                   delete idx;
                   OptmizationStatistics::count(OPTI_FT_LITERAL_INDEX);
                   progress = true;
                 }
            }
       }

   return progress;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::optimize_short_primitives(Token_string & tos, bool keep_left_value)
{
   if (DONT_FT_SHORT_PRIMITIVE)   return false;

   // multi-line comment, i.e. ⊣ ««« ... »»»
   // ⊣ «««...»»» (multi-line comment): remove it. Not when the value of
   // the statement is used (⍎, ⎕EC, ...: keep_left_value): ⊣ 7 is then the
   // committed value 0 (Bugs35 #77).
   //
   if (!keep_left_value                &&
       tos.size() == 2                 &&
       tos[0].get_tag() == TOK_F2_LEFT &&   // ⊣
       tos[1].get_Class() == TC_VALUE)      // multi-line string
      {
         tos[0].clear(LOC);
         tos[1].clear(LOC);
         return true;
      }

   // replace APL primitives with short literal results and arguments,
   // such as 4⍴0 with their result. The scope of tos is one statement.
   //
bool progress = false;
   if (tos.ssize() < 4)   return false;   // too short to optimize

// CERR << endl << "tos: ";   tos.print(CERR, 0);

   // create a list of 'terminal literals' from where the optimization
   //  may restart. For example:
   //
   //  (2⍴5) ⍴ 6
   //
   // The optimization starts at "6" (end of statement) and restarts at ")".
   //
vector<ShapeItem> ends;
   ends.push_back(tos.ssize());

   // tos is in forward (aka. APL) order. We move backwards from the end
   //
   rev_loop(pc, tos.ssize())
       {
         if (pc <= 4)   break;   // too few tokens remaining in tos

         // at this point we have at least:
         //
         // in APL order:
         //
         // END A F B   (dyadic function call)
         // END F B     (monadic function call)
         //
         // resp. in reverse order:
         //
         // B F A END   (dyadic function call)
         // B F END     (monadic function call)
         //
         // That is A, F, and B are valid, and END must be checked
         // for the dyadic cases. Below we refer to the dyadic END as Q and
         // to the monadic END as AQ (since its position is A).
         //
         if ((1 << tos[pc].get_Class()) & TCG_R_PAR_BRA)
            {
              ends.push_back(pc);
            }
       }

   loop(e, ends.size())
       {
         // set shortcuts for relevant positions (PCs) in tos. The rev_loop()
         // above has checked that the tokens at src_B, src_F, and src_AQ are
         // valid, while src_Q may or may not be valid (and must be checked).
         //
         const ShapeItem src_Q  =                  ends[e] - 4;
         const ShapeItem src_AQ = src_Q  + 1;   // ends[e] - 3
         const ShapeItem src_F  = src_AQ + 1;   // ends[e] - 2
         const ShapeItem src_B  = src_F  + 1;   // ends[e] - 1

         Token_loc & tok_B = tos[src_B];
         if (tok_B.get_Class() != TC_VALUE)   continue;   // no Value B

         Token_loc & tok_F = tos[src_F];
         if (tok_F.get_Class() != TC_FUN2)    continue;   // no function F
         cFunction_P fun = tok_F.get_function();

         // check if the function call can only be monadic and rule out strand
         // notation cases. For example (by Elias Mårtenson and verified with
         // IBM APL2):
         //
         // X←2 ◊ X 1 ⍴ 10 11
         //       │ │ │ │
         //       │ └─┴─┴──── dyadic ⍴, but a 1 ⍴ 10 11 optimization would be
         //       └────────── incorrect because X MAY bind stronger than ⍴.
         //
         const Token & tok_Q = tos[src_Q];

         const bool is_dyadic =
               tos[src_AQ].get_Class() == TC_VALUE &&        // AQ is a Value,
               src_Q >= 0                          &&        // Q is valid, and
               ! ((1 << tok_Q.get_Class()) & TCG_MAY_GLUE);  // Q is not sticky

         const bool is_monadic =
               ((1 << tos[src_AQ].get_Class()) & TCG_NO_A);   // A not a Value

         Value_P B = tok_B.get_apl_val();
         if (is_dyadic)
            {
              Token_loc & tok_A = tos[src_AQ];
              if (fun == &Bif_F12_RHO::fun)   // dyadic A⍴B
                 {
                    // NOTE: we use Bif_F12_RHO::do_reshape() instead of
                    //       Bif_F12_RHO::eval_AB() as to bypass the A⍴B
                    //       optimization in eval_AB() (which does not
                    //       work well here)
                    //
                    // A is not yet known to be a valid shape (integral,
                    // fits ShapeItem, rank ≤ MAX_RANK) -- the Shape
                    // constructor below, or do_reshape() itself, throws
                    // (e.g. DOMAIN ERROR for non-numeric/non-integral/
                    // complex A) when it isn't. Unlike a normal runtime
                    // error, an exception thrown here escapes straight
                    // out of the *parser*, before this statement is even
                    // fully compiled: no failed statement/carets are
                    // ever shown, ⎕EM/⎕ET stay at their previous value,
                    // and ⎕FX refuses to define the (perfectly
                    // syntactically valid) function outright (Bugs28
                    // #32). A try/catch around this is not enough on its
                    // own either: constructing/throwing the Error (via
                    // Error::update_error_info(), before any catch ever
                    // runs) has its own observable side effects (e.g.
                    // clobbering the *next* statement's ⎕FX-time display
                    // with a stray "DOMAIN ERROR" that ⎕ET/⎕EM never
                    // actually recorded) even once caught. So check
                    // first, with predicates that cannot themselves
                    // throw (mirroring Token.cc's own canonical()
                    // fix for the equivalent axis-formatting hazard),
                    // and only even attempt the throwing constructors
                    // once every element is already known to be safe.
                    // Since this whole block is purely a best-effort
                    // optimization -- the ordinary, unoptimized runtime
                    // path below always exists as a fallback and reports
                    // such errors correctly -- just decline to optimize
                    // when A isn't safely foldable.
                    //
                    Value_P Aval = tok_A.get_apl_val();
                    bool A_foldable = Aval->get_rank() <= 1 &&
                                      Aval->element_count() <= MAX_RANK;
                    for (ShapeItem a = 0; A_foldable && a < Aval->element_count(); ++a)
                        {
                          Cell cache;
                          const Cell & c = Aval->get_cravel(a, cache);
                          if (!c.is_near_int64_t())   A_foldable = false;
                          else if (c.get_near_int() < 0)   A_foldable = false;
                          // A negative axis is invalid (Bif_F12_RHO::
                          // eval_AB()'s runtime path DOMAIN_ERRORs via
                          // require_non_negative_ints() -- Blake McBride,
                          // Bugs31 #5/#28: do_reshape() below performs no
                          // such check itself, so a negative axis here
                          // used to silently fold into a live malformed
                          // value instead, corrupting everything
                          // downstream that consumes it (a thrown
                          // std::length_error two layers away in
                          // Bif_OPER1_REDUCE.cc for #5, an internal
                          // Cell::init_other() assert in the rank
                          // empty-frame machinery for #28). Declining to
                          // fold here is safe and cheap: the ordinary,
                          // unoptimized runtime path below always exists
                          // as a fallback and already raises the correct
                          // DOMAIN ERROR for a negative axis.
                        }

                    if (A_foldable)
                       {
                         const Shape sh_A(*Aval, /* qio */ 0);

                         if (sh_A.fits_into(cfg_SHORT_VALUE_LENGTH_WANTED))
                            {
                              Token tZ = Bif_F12_RHO::fun.do_reshape(sh_A, *B);
                              // the result A⍴B gets the PC of A (leftmost)
                              const Token_loc Z(Token(TOK_APL_VALUE4,   // optimized
                                                      tZ.get_apl_val()),
                                                tok_A.get_PC());
                              tok_A.clear(LOC);   // set A to TOK_VOID
                              tok_F.clear(LOC);   // set F to TOK_VOID
                              tok_B.copy(Z, LOC);
                              OptmizationStatistics::count(OPTI_FT_SHORT_PRIMITIVE);
                              progress = true;
                            }
                       }
                 }
            }
         else if (is_monadic)
            {
              if (fun == &Bif_F12_COMMA::fun)            // monadic ,B
                 {
                   const Shape new_shape(B->element_count());
                   B->set_shape(new_shape);
                   // the result ,B gets the PC of , (leftmost)
                   const Token_loc Z(tok_B.get_token(), tok_F.get_PC());
                   tok_B.copy(Z, LOC);
                   tok_F.clear(LOC);   // set , to TOK_VOID
                   OptmizationStatistics::count(OPTI_FT_SHORT_PRIMITIVE);
                   progress = true;
                 }
              else if (fun == &Bif_F12_COMMA1::fun)      // monadic ⍪B
                 {
                   // 1↑⍴⍪B  ←→  1↑⍴B
                   // 1↓⍴⍪B  ←→  ×/1↓⍴B
                   //
                   const Shape & sh_B = B->get_shape();
                   const ShapeItem rows = B->get_rank() ? B->get_shape_item(0)
                                                        : 1;
                   ShapeItem low_volume = 1;   // ×/1↓⍴B
                   for (ShapeItem r = 1; r < sh_B.get_rank(); ++r)
                       low_volume *= sh_B.get_shape_item(r);
                   const Shape new_shape(rows, low_volume);
                   B->set_shape(new_shape);

                   // the result ⍪B gets the PC of ⍪ (leftmost)
                   const Token_loc Z(tok_B.get_token(), tok_F.get_PC());
                   tok_B.copy(Z, LOC);
                   tok_F.clear(LOC);   // set ⍪ to TOK_VOID
                   OptmizationStatistics::count(OPTI_FT_SHORT_PRIMITIVE);
                   progress = true;
                 }
              else if (fun == &Bif_F12_PARTITION::fun)   // monadic ⊂B
                 {
                   if (!B->is_simple_scalar())   // otherwise ⊂B is B
                      {
                        Value_P Z(LOC);
                        Z->next_ravel_Pointer(B.get());
                        Z->check_value(LOC);
                        // the result ⊂B gets the PC of ⊂ (leftmost)
                        tok_B.copy(Token_loc(Token(TOK_APL_VALUE4, Z),
                                             tok_F.get_PC()), LOC);
                      }
                   else
                      {
                        // Bugs28 #100(t): ⊂B for an already-simple-
                        // scalar B is correctly left unchanged (⊂B ≡
                        // B per the comment above), but its token used
                        // to keep its ORIGINAL tag -- so nothing here
                        // marked "a fold happened at this position" the
                        // way the non-scalar branch above does.
                        // Executable.cc's error-display reconstruction
                        // relies on spotting that TOK_APL_VALUE4 marker
                        // to know it must show the un-optimized
                        // statement (which still has ⊂) instead of the
                        // compacted one (which, once tok_F below is
                        // removed, no longer does) -- without it, a
                        // later error in the same statement displayed
                        // the compacted text with ⊂ silently missing,
                        // e.g. (⊂1)↑M shown as (1)↑M. Retag with the
                        // same, unmodified value purely so this fold is
                        // detectable too, mirroring the branch above.
                        //
                        tok_B.copy(Token_loc(Token(TOK_APL_VALUE4, B),
                                             tok_F.get_PC()), LOC);
                      }
                   tok_F.clear(LOC);   // set F to TOK_VOID
                   OptmizationStatistics::count(OPTI_FT_SHORT_PRIMITIVE);
                   progress = true;
                 }
            }

         // otherwise the arity can not be determined statically (and will be
         // decided at runtime in Prefix.cc)
       }

   return progress;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Parser::parse_multi_literal(UCS_string_vector & text, Lit_DB & literals,
                            bool fun_header)
{
   // 0. replace multi-line strings in text and insert them into 'literals'
   //
   replace_multi_line_strings(text, literals, fun_header);

   // 1. compute the shape
   //
int rank = 0;
int counters[MAX_RANK];
int max_counters[MAX_RANK];
   loop(c, MAX_RANK)
       {
         counters[c] = 0;
         max_counters[c] = 0;
       }

int counter_index = 0;
int min_value_length = 1000;
int max_value_length = 0;
UCS_string min_line;
UCS_string max_line;
vector<Value_P>value_rows;   value_rows.reserve(100);

   loop(l, text.size())
       {
         UCS_string line = text[l];
         line.remove_leading_and_trailing_whitespaces();

         if (line.size())   // data line
            {
              counter_index = 0;
            }
         else               // separator line
            {
              // increment the counter(s)
              //
              counters[counter_index] = 0;
              ++counter_index;
              if (counter_index >= MAX_RANK)
                 {
                   MORE_ERROR() << "too many separator lines (i.e. empty "
                                   "lines between data lines)\n"
                                   "in multi-line literal";
                   RANK_ERROR;
                 }
              rank = max(rank, counter_index);
              ++counters[counter_index];
              continue;   // loop(l, text.size())
            }

         Log(LOG_create_value)
            {
              for (int j = MAX_RANK - 1; j >= 0; --j)
                  {
                    CERR << "." << counters[j];
                  }
              CERR << "  '" << line << "'" << endl;
            }

         Parser parser(PM_EXECUTE, LOC, /* macro */ false);
         Token_string tos;   // parsed line
         if (parser.parse(line, tos, /* optimize */ true))
            {
              MORE_ERROR() << "Error parsing multi-line literal:\n"
                              "    '" << line << "'";
              SYNTAX_ERROR;
            }

         ShapeItem len_Z = 0;
         loop(t, tos.size())
             {
               if (tos[t].get_tag() == TOK_MARKER)
                  {
                    ++len_Z;
                  }
               else if (tos[t].get_Class() == TC_VALUE)
                  {
                    Value_P ZZ = tos[t].get_apl_val();
                    if (ZZ->get_rank() > 1)   RANK_ERROR;
                    // TOK_APL_VALUE1 is a string literal that was not
                    // grouped with its neighbours (e.g. because a <<<
                    // marker is adjacent).  Treat it as one nested
                    // element, not as a spread of its characters.
                    if (tos[t].get_tag() == TOK_APL_VALUE1)   ++len_Z;
                    else                                       len_Z += ZZ->element_count();
                  }
               else
                  {
                    MORE_ERROR() << "Invalid token '"
                                 << tos[t].canonical(PR_APL_FUN)
                                 << "' in multi-line literal.\nOnly "
                                    "constant (!) APL values are permitted.";
                    SYNTAX_ERROR;
                  }
             }

         Value_P Z(len_Z, LOC);
         loop(t, tos.size())
             {
               if (tos[t].get_tag() == TOK_MARKER)
                  {
                    const ShapeItem idx = tos[t].get_int_val();
                    Value_P ZZ = literals.pull(idx);
                    Assert(ZZ);
                    Z->next_ravel_Value(ZZ.get());
                  }
               else if (tos[t].get_Class() == TC_VALUE)
                  {
                    Value_P ZZ = tos[t].get_apl_val();
                    if (tos[t].get_tag() == TOK_APL_VALUE1)
                       {
                         Z->next_ravel_Value(ZZ.get());
                       }
                    else
                       {
                         loop(z, ZZ->element_count())
                             {
                               Cell cache;
                               Z->next_ravel_Cell(ZZ->get_cravel(z, cache));
                             }
                       }
                  }
               else FIXME;
             }
         Z->check_value(LOC);
         Token tok_Z(TOK_APL_VALUE1, Z);
         tos.clear();
         tos.emplace_back(tok_Z, Function_PC_0);   // the whole line
         

         const Token & token = tos[0];
         if (token.get_Class() != TC_VALUE)
            {
              MORE_ERROR() << "Error parsing multi-line literal "
                              "(not a value):\n    '" << line << "'";
              DOMAIN_ERROR;
            }

         Value_P value = token.get_apl_val();
         if (value->get_rank() > 1)   // not a strand
            {

              MORE_ERROR() << "Error parsing multi-line literal "
                              "(not a vector):\n" "    '" << line << "'";
              RANK_ERROR;
            }

         const ShapeItem value_len = value->element_count();
         if (min_value_length > value_len)
            {
              min_line = line;
              min_value_length = value_len;
            }
         if (max_value_length < value_len)
            {
              max_line = line;
              max_value_length = value_len;
            }

         value_rows.push_back(value);

         // update cuonter maxima
         loop(r, MAX_RANK)
             {
               max_counters[r] = max(max_counters[r], counters[r]);
             }

         counter_index = 0;
         ++counters[0];
       }   // loop(l, text.size())

   ++counters[counter_index];

   if (min_value_length != max_value_length)
      {
        MORE_ERROR() <<
             "Error parsing multi-line literal (vector length mismatch):\n"
             "    shortest row (" << min_value_length
          << "): '" << min_line << "'\n"
             "    longest row: (" << max_value_length
          << "):'" << max_line << "'";
        LENGTH_ERROR;
      }

   ++rank;   // since counter_index counts from 0

Shape shape_Z;
   loop(r, rank)
       {
         shape_Z.add_shape_item(max_counters[rank - r - 1] + 1);
       }
   shape_Z.add_shape_item(max_value_length);

   // shape_Z is sized from the *maximum* group/row counters per axis, but
   // the fill loop below only ever writes value_rows.size() rows. If some
   // group has fewer rows than the group that set max_counters[], those
   // extra Shape_Z-implied rows are never next_ravel_Cell()'d, leaving
   // that many trailing Cells uninitialized (garbage vtables) -- reachable
   // via a ragged multi-line literal (e.g. groups of 200/1/1 rows) and a
   // guaranteed SEGV on first use of the result. check_value() below only
   // catches this under cfg_VALUE_CHECK_WANTED, so enforce it here instead.
   if (ShapeItem(value_rows.size()) * max_value_length != shape_Z.get_volume())
      {
        MORE_ERROR() <<
             "Error parsing multi-line literal (ragged group/row counts)";
        LENGTH_ERROR;
      }

Value_P Z(shape_Z, LOC);
   loop(r, value_rows.size())
       {
         const Value & row = *value_rows[r];
         Cell cache;
         loop(c, max_value_length)
             {
               Z->next_ravel_Cell(row.get_cravel(c, cache));
             }
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::replace_multi_line_literals(UCS_string_vector & text,
                                    Lit_DB & literals, bool fun_header)
{
  /* multi-line literals. Convert dequences of lines like

     [k+1] PREFIX <<<
     [k+2] L2 ...
     [k+N] >>>

    into:

    [k+1] PREFIX @N@
    [...] (empty)
    [k+N] (empty)

      and add the value for the marker @N@ to this->literals
   */
const int start = fun_header ? 1 : 0;

   for (;;)
       {
         vector<Multiline_status> status;
         if (get_multiline_status(status, text, fun_header, false))
            {
              return fun_header ? E_DEFN_ERROR : E_SYNTAX_ERROR;
            }

         // 1. modify lines...
         //

         // find the first closing >>> and remember its opening <<<
         //
         int literal_start = -1;
         int literal_end   = -1;
         for (size_t l = start; l < text.size(); ++l)
             {
               if (status[l] == MLS_Start_of_multi)
                  {
                    literal_start = l;
                  }
               else if (status[l] == MLS_End_of_multi)
                  {
                    literal_end = l;
                    break;
                  }
             }

         if (literal_end == -1)
            {
              // no more literls found
              //
              return E_NO_ERROR;
            }

// loop(j, text.size())
//     CERR << "[" << j << "] " << status[j]  << "   " << text[j] << endl;


         // a (leaf-) multi-line literal starts at literal_start li...
         //
         if (literal_start == -1)
            {
              MORE_ERROR() << ">>> closes no open <<<.";
              return fun_header ? E_DEFN_ERROR : E_SYNTAX_ERROR;
            }
         Assert1(status[literal_start] == MLS_Start_of_multi);
         UCS_string & prefix = text[literal_start];
         prefix.resize(prefix.multi_pos());
         prefix << " @" << literals.get_next_key() << "@ ";

         // copy the literal content (lines between <<< and >>> to content
         // and clear them in the function text
         //
         UCS_string_vector content;
         int li = literal_start + 1;
         while(status[li] == MLS_Inside_multi)
               {
                 content.push_back(text[li]);
                 text.erase(li);
                 status.erase(status.begin() + li);
               }
         Assert(status[li] == MLS_End_of_multi);
         const UCS_string & suffix = text[li];
         prefix << UCS_string(suffix, suffix.multi_pos() + 3);
         text.erase(li);
         status.erase(status.begin() + li);

         // create value Z for content. content may contain "false" trailing
         // empty lines, which shall be removed (otherwise
         // parse_multi_literal() would be confused.
         //
         while (content.size() && content.back().size() == 0)
               {
                 content.pop_back();
               }

         Value_P Z = parse_multi_literal(content, literals, fun_header);
         literals.push(Z);
       }

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::replace_multi_line_strings(UCS_string_vector & text,
                                   Lit_DB & literals, bool fun_header)
{
  /* new-style multi-line strings. Convert dequences of lines like

     [k+1] PREFIX """
     [k+2] L2 ...
     [k+N] """

    into:

    [k+1] PREFIX "L2" ... "LN"
    [...] (empty)
    [k+N] (empty)

     In contrast to transform_old_multi_line_strings(), line k+N may only
     consist of spaces and the terminating """.
   */

const ShapeItem start = fun_header ? 1 : 0;

vector<Multiline_status> status;
   if (get_multiline_status(status, text, fun_header, true))
      {
        return fun_header ? E_DEFN_ERROR : E_SYNTAX_ERROR;
      }

   // modify lines...
   //
   for (size_t li = start; li < text.size(); )
       {

         if (status[li] == MLS_APL_text)
            {
              ++li;
              continue;
            }

         // a multi-line string starts at line li...
         //
         const ShapeItem ml_start = li++;
         Assert1(status[ml_start] == MLS_Start_of_multi);
         UCS_string & prefix = text[ml_start];
         prefix.resize(prefix.multi_pos());   // remove the trailing """
         prefix << " @" << literals.get_next_key() << "@ ";

         UCS_string_vector content;
         while (status[li] == MLS_Inside_multi)
               {
                 content.push_back(text[li].do_escape(true));
                 text.erase(li);
                 status.erase(status.begin() + li);
               }

         Assert(status[li] == MLS_End_of_multi);
         const UCS_string suffix(text[li], text[li].multi_pos() + 3);
         prefix << " " << suffix;
         text.erase(li);
         status.erase(status.begin() + li);
         // NOTE: no ++li here: erase() already shifted the line that
         // follows the just-processed block into position li, so li
         // already refers to the next unprocessed line. An extra ++li
         // here used to skip that line -- harmless when it was a blank
         // separator line, but a mis-parse (Assert1 failure below) when
         // a block was immediately followed by another block's opening
         // ⊣«««/""" with no separating line at all.

         // create value Z for content
         //
         Value_P Z(content.size(), LOC);
         if (content.size() == 0)        // nothing
            {
             Value_P Zproto = Str0(LOC);
             new (&Z->get_wproto())   PointerCell(Zproto.get(), *Z);
            }
         loop(z, content.size())
             {
               Value_P Zz(content[z], LOC);
               Z->next_ravel_Pointer(Zz.get());
            }
         Z->check_value(LOC);
         literals.push(Z);
       }

   // remove trailing empty lines...
   //
   while (text.size() && text.back().size() == 0)   text.pop_back();

// CERR << endl;
// loop(l, text.size())   CERR << "[" << l << "]  " << text[l] << endl;

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::transform_old_multi_line_strings(UCS_string_vector & text)
{
  /* old-style multi-line strings. convert sets of lines like

     [k+1] PREFIX "L1
     [k+2] L2 ...
     [k+N] Ln" SUFFIX

    into:

    [k+1] PREFIX "L1" "L2" ... "LN" SUFFIX
    [...] (empty)
    [k+N] (empty)

    The result is nested.
   */


// a heap buffer, not ALLOCA(): text.size() is the (user-controlled, via
// ⎕FX on a tall character matrix) line count of the function being fixed,
// with no cap -- ALLOCA()/alloca() here would be an unbounded stack
// allocation. This is a one-shot ⎕FX path, not a hot loop, so the extra
// heap indirection is immaterial.
//
std::vector<char> status_v(text.size());
char * status = status_v.data();
   status[0] = MLS_Function_header;
Multiline_status current = MLS_APL_text;

   // 1. determine the line status of each line...
   //
   for (size_t l = 1; l < text.size(); ++l)
       {
         const int count = text[l].double_quote_count(false);
         if (count & 1)   // start or end of string
            {
              if (current == MLS_APL_text)   // start of multi-line string
                 {
                    status[l] = MLS_Start_of_multi;
                    current = MLS_Inside_multi;
                 }
              else                       // end of multi-line string
                 {
                    status[l] = MLS_End_of_multi;
                    current = MLS_APL_text;
                 }
            }
         else              // no status change
            {
              status[l] = current;
            }
       }

   if (current == MLS_Start_of_multi || current == MLS_Inside_multi)
      {
         // old multi-line string started but not ended.
         //
         return E_DEFN_ERROR;
      }

   // 2. modify lines...
   //
   for (size_t line_number = 1; line_number < text.size();)
       {
         if (status[line_number] == MLS_APL_text)   { ++line_number;   continue; }
         const int start_line_number = line_number;
         Assert1(status[line_number] == MLS_Start_of_multi);
         UCS_string accu = text[line_number++];
         accu << "\" ";
         while (status[line_number] == MLS_Inside_multi)
               {
                 accu << " \"" << text[line_number].do_escape(true) << "\"";
                 text[line_number++].clear();
               }
         Assert(status[line_number] == MLS_End_of_multi);
         accu << "\"" << text[line_number];
         text[start_line_number] = accu;
         text[line_number++].clear();
       }

   // remove trailing empty lines...
   //
   while (text.size() && text.back().size() == 0)   text.pop_back();

// CERR << endl;
// loop(l, text.size())   CERR << "[" << l << "]  " << text[l] << endl;

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::collect_constants(Token_string & tos)
{
bool progress = false;
   Log(LOG_collect_constants)
      {
        CERR << "collect_constants [" << tos.ssize() << " token] in: ";
        tos.print(CERR, 3);
      }

   // convert several items in vector notation into a single APL value
   //
   loop (t, tos.ssize())
      {
        ShapeItem to;
        switch(tos[t].get_tag())
           {
             case TOK_APL_VALUE1:
             case TOK_APL_VALUE3:
             case TOK_APL_VALUE4:
             case TOK_CHARACTER:
             case TOK_COMPLEX:
             case TOK_INTEGER:
             case TOK_REAL:
                  break;          // continue below

             default: continue;   // nex token
           }

        // at this point, t is the first item. Collect subsequenct items.
        //
        for (to = t + 1; to < tos.ssize(); ++to)
            {
#if 1
              // Note: ISO 13751 gives an example with 1 2 3[2] ↔ 2
              // 
              // In contrast, the binding rules stated in IBM's apl2lrm.pdf
              // would give 3[2] ↔ RANK ERROR.
              // 
              // We follow apl2lrm; to enable ISO behavior write #if 0 above.
              // 

              // if tos[to + 1] is [ then [ binds stronger than
              // vector notation and we stop collecting.
              //
              if ((to + 1) < tos.ssize() &&
                  tos[to + 1].get_tag() == TOK_L_BRACK)   break;
#endif
              const TokenTag tag = tos[to].get_tag();

              if (tag == TOK_CHARACTER)    continue;
              if (tag == TOK_INTEGER)      continue;
              if (tag == TOK_REAL)         continue;
              if (tag == TOK_COMPLEX)      continue;
              if (tag == TOK_APL_VALUE1)   continue;
              if (tag == TOK_APL_VALUE3)   continue;
              if (tag == TOK_APL_VALUE4)   continue;

              // no more values: stop collecting
              //
              break;
            }

        create_value(tos, t, to - t);
        if (to > (t + 1))   progress = true;
        else if (tos[t].get_tag() == TOK_APL_VALUE3)
           {
             // a single item was collected, not an (open) strand: close it,
             // the same way collect_value_groups() closes a parenthesized
             // value, so that TOK_APL_VALUE3 keeps meaning "open strand"
             //
             tos[t].ChangeTag(TOK_APL_VALUE1);
           }
      }

   Log(LOG_collect_constants)
      {
        CERR << "collect_constants [" << tos.ssize() << " token] out: ";
        tos.print(CERR, 3);
      }

   return progress;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::collect_value_groups(Token_string & tos)
{
   Log(LOG_collect_constants)
      {
        CERR << "collect_value_groups [" << tos.ssize() << " token] in: ";
        tos.print(CERR, 3);
      }

int opening = -1;
   loop(t, tos.ssize())
       {
         switch(tos[t].get_tag())
            {
              case TOK_CHARACTER:
              case TOK_INTEGER:
              case TOK_REAL:
              case TOK_COMPLEX:
              case TOK_APL_VALUE1:
              case TOK_APL_VALUE3:
              case TOK_APL_VALUE4:
                   continue;

              case TOK_L_PARENT:
                   // we remember the last opening '(' so that we know where
                   // the group started when we see a ')'. This causes the
                   // deepest ( ... ) to be grouped first.
                   opening = t;
                   continue;

              case TOK_R_PARENT:
                   if (opening == -1)      continue;       // ')' without '('
                   if (opening == t - 1)   SYNTAX_ERROR;   // '(' ')'

                   {
                     // the group (...) gets the PC of its leftmost token (
                     //
                     const Function_PC pc_group = tos[opening].get_PC();
                     tos[opening].clear(LOC);  // invalidate '('
                     tos[t].clear(LOC);        // invalidate ')'
                     create_value(tos, opening + 1, t - opening - 1);

                     // we removed parantheses, so we close the value.
                     //
                     Token_loc & group = tos[opening + 1];
                     if (group.get_Class() == TC_VALUE)
                        group.ChangeTag(TOK_APL_VALUE1);
                     const Token_loc tl_group(group.get_token(), pc_group);
                     group.copy(tl_group, LOC);
                   }

                   Log(LOG_collect_constants)
                      {
                        CERR << "collect_value_groups [" << tos.ssize()
                             << " token] out: ";
                        tos.print(CERR, 3);
                      }
                   return true;

              default: // nothing that can be grouped
                   opening = -1;
                   continue;
            }
       }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
Parser::create_scalar_value(Token_loc & output)
{
   switch(output.get_tag())
      {
        case TOK_CHARACTER:
             {
               Value_P scalar(LOC);

               scalar->next_ravel_Char(output.get_char_val());
               scalar->check_value(LOC);
               Token tok(TOK_APL_VALUE3, scalar);
               output.copy(Token_loc(tok, output.get_PC()), LOC);
             }
             return;

        case TOK_INTEGER:
             {
               Token tok(TOK_APL_VALUE3, IntScalar(output.get_int_val(), LOC));
               output.copy(Token_loc(tok, output.get_PC()), LOC);
             }
             return;

        case TOK_REAL:
             {
               Token tok(TOK_APL_VALUE3,
                         FloatScalar(output.get_flt_val(), LOC));
               output.copy(Token_loc(tok, output.get_PC()), LOC);
             }
             return;

        case TOK_COMPLEX:
             {
               Token tok(TOK_APL_VALUE3,
                         ComplexScalar(output.get_cpx_real(),
                                       output.get_cpx_imag(), LOC));
               output.copy(Token_loc(tok, output.get_PC()), LOC);
             }
             return;

        case TOK_APL_VALUE1:
        case TOK_APL_VALUE3:
             return;

        default: break;
      }

   CERR << "Unexpected token " << output.get_tag() << ": " << output << endl;
   Assert(0 && "Unexpected token");
}
//────────────────────────────────────────────────────────────────────────────
void
Parser::create_value(Token_string & tos, int pos, int count)
{
   Log(LOG_create_value)
      {
        CERR << "create_value(" << __LINE__ << ") tos[" << tos.ssize()
             <<  "]  pos " << pos << " count " << count << " in:";
        tos.print(CERR, 1);
      }

   if (count == 1)   create_scalar_value(tos[pos]);
   else              create_vector_value(tos, pos, count);

   Log(LOG_create_value)
      {
        CERR << "create_value(" << __LINE__ << ") tos[" << tos.ssize()
             <<  "]  pos " << pos << " count " << count << " out:";
        tos.print(CERR, 1);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Parser::create_vector_value(Token_string & tos, int pos, int count)
{
Value_P Z(count, LOC);
const Function_PC pc_Z = tos[pos].get_PC();   // the PC of the leftmost item

   loop(l, count)
       {
         Token_loc & tok = tos[pos + l];

         switch(tok.get_tag())
            {
              case TOK_CHARACTER:
                   Z->next_ravel_Char(tok.get_char_val());
                   tok.clear(LOC);   // invalidate token
                   break;

              case TOK_INTEGER:
                   // NOTE: also with STRICT_IBM_APL2_FORMATTING an integer
                   // stays an integer: storing the integers of a numeric
                   // strand with a non-integer as floats changed the
                   // values (Bugs34 #7), and IBM APL2 displays the integral
                   // numbers of a mixed array with all digits anyway (#22)
                   Z->next_ravel_Int(tok.get_int_val());
                   tok.clear(LOC);   // invalidate token
                   break;

              case TOK_REAL:
                   Z->next_ravel_Float(tok.get_flt_val());
                   tok.clear(LOC);   // invalidate token
                   break;

              case TOK_COMPLEX:
                   Z->next_ravel_Complex(tok.get_cpx_real(),
                                         tok.get_cpx_imag());
                   tok.clear(LOC);   // invalidate token
                   break;

            case TOK_APL_VALUE1:
            case TOK_APL_VALUE3:
            case TOK_APL_VALUE4:
                 Z->next_ravel_Value(tok.get_apl_val().get());
                 tok.clear(LOC);   // invalidate token
                 break;

              default: FIXME;
            }
       }

   Z->check_value(LOC);
Token tok(TOK_APL_VALUE3, Z);

   tos[pos].copy(Token_loc(tok, pc_Z), LOC);

   Log(LOG_create_value)
      {
        CERR << "create_value [" << tos.ssize() << " token] out: ";
        tos.print(CERR, 1);
      }
}
//────────────────────────────────────────────────────────────────────────────
/// walk pos_LO (initially the token immediately left of a ⍤/⍣ token)
/// back to the start of the full LO expression it belongs to, so that
/// fix_RANK_syntax()/fix_POWER_syntax() below parenthesize the WHOLE
/// derived function rather than splitting it apart. Handles, looping
/// since they can combine (e.g. a chain ending in `(F.G)[1]/`):
///
///  - a chain of monadic operators (F M, F M M, ...)
///  - one step of dyadic-operator composition (F.G, e.g. +.× or ∘.f):
///    landing on G (the right operand) backs up over the operator token
///    and G itself to F
///  - a trailing literal axis [N] (e.g. F[1])
///  - a parenthesized (...) or curly {...} group
///
/// Bugs27 #20/#21/#38: the walk-back used to only handle the first of
/// these (Bugs26 #5's fix), silently mis-splitting or outright failing
/// to parse an inner/outer-product or bracket-axis LO.
/// return true if the lambda starting at tos[pos_L] (a {) is a monadic
/// operator, i.e. uses ⍶ but not ⍹ (at its own nesting level)
static bool
is_monadic_operator_lambda(const Token_string & tos, int pos_L)
{
bool LO = false;
bool RO = false;
int level = 0;
   for (int t = pos_L + 1; t < tos.ssize(); ++t)
       {
         const TokenTag tag = tos[t].get_tag();
         if (tag == TOK_L_CURLY)        ++level;
         else if (tag == TOK_R_CURLY)   { if (level-- == 0)   break; }
         else if (level == 0)
            {
              if (tag == TOK_ALPHA_U)   LO = true;
              if (tag == TOK_OMEGA_U)   RO = true;
            }
       }
   return LO && !RO;
}
//────────────────────────────────────────────────────────────────────────────
/// return true if tos[pos] is the name of a (currently defined) monadic
/// operator, e.g. a ∇-defined OPX before its axis in ⌽OPX[1]⍤2
static bool
is_monadic_operator_name(const Token_string & tos, int pos)
{
   if (tos[pos].get_Class() != TC_SYMBOL)   return false;
const Symbol * sym = tos[pos].get_sym_ptr();
   if (!sym)   return false;
const Function * fun = sym->get_function();
   return fun && fun->get_token().get_Class() == TC_OPER1;
}
//────────────────────────────────────────────────────────────────────────────
static int
walk_back_LO_start(const Token_string & tos, int pos_LO)
{
   for (;;)
       {
         const int start = pos_LO;

         while (pos_LO > 0 && tos[pos_LO].get_Class() == TC_OPER1)
            --pos_LO;

         if (pos_LO > 0 && tos[pos_LO - 1].get_Class() == TC_OPER2)
            pos_LO -= 2;   // skip G (right operand) and the operator itself

         if (pos_LO > 0 && tos[pos_LO].get_tag() == TOK_R_BRACK)
            pos_LO = tos.find_opening_bracket(pos_LO) - 1;   // skip [N]

         if (pos_LO < 0)   return 0;

         if (tos[pos_LO].get_Class() == TC_R_PARENT)
            pos_LO = tos.find_opening_parent(pos_LO);
         else if (tos[pos_LO].get_Class() == TC_R_CURLY)
            {
              pos_LO = tos.find_opening_curly(pos_LO);

              // a monadic-operator lambda {⍶ ...} belongs together with its
              // left operand further left (Bugs35 #39)
              //
              if (pos_LO > 0 && is_monadic_operator_lambda(tos, pos_LO))
                 --pos_LO;
            }
         else if (pos_LO > 0 && pos_LO != start &&
                  is_monadic_operator_name(tos, pos_LO))
            {
              // a named monadic operator reached after skipping its axis,
              // e.g. OPX in ⌽OPX[1]⍤2 (Bugs35 #39)
              //
              --pos_LO;
            }

         if (pos_LO == start)   return pos_LO;   // no more progress
       }
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::fix_POWER_syntax(Token_string & tos)
{
   /* Z ← A ⍣ N B  or  Z ← ⍣ N B 

      where N is a near-int scalar
    */

bool progress = false;

   // must not use loop() here since tos.size() varies inside the loop.
   //
   for (int t = 0; t < (int(tos.size()) - 2); ++t)
       {
         if (tos[t].get_tag() != TOK_OPER2_POWER)
            {
              continue;   // next ⍣ (if any)
            }

         int pos_POWER = t;

         // When LO itself carries a trailing bracket axis (e.g.
         // ⌽[1]⍣2), Prefix's parenthesis reduction cannot parse
         // "LO[axis] ⍣ N" as a single derived function -- it needs the
         // same narrower (LO[axis])⍣N grouping produced below,
         // regardless of what shape N happens to have (a literal, a
         // parenthesized expression, or a bare variable), exactly like
         // fix_RANK_syntax()'s matching LO_has_axis exception (Bugs30
         // #20 fixed only the literal-N case; Bugs31 #21, Blake
         // McBride, extends it to every N). So check this FIRST and
         // skip the non-literal-N shortcuts below entirely when it
         // holds -- those shortcuts exist only to leave an ORDINARY
         // (no-axis) LO ⍣ N alone for Prefix::reduce_A_B__() to handle
         // directly, which no longer applies once LO has an axis.
         //
         const bool LO_has_axis = pos_POWER > 0 &&
                                   tos[pos_POWER - 1].get_tag() == TOK_R_BRACK;

         if (!LO_has_axis)
            {
              // if N is already in parentheses: skip (...). In this case
              // Prefix::reduce_A_B__() will take care after the expression
              // in parentheses was evaluated.
              //
              if (tos[t + 1].get_tag() == TOK_L_PARENT)   // f ⍣ ( RO )
                 {
                   t = tos.find_closing_parent(t + 1);   // skip (...)
                   continue;                              // next ⍣ (if any)
                }

              const int len_N = j_length(tos, pos_POWER + 1);
              if (len_N == 0)   continue;   // N is not literal

              const int pos_B = pos_POWER + 1 + len_N;
              if (pos_B >= int(tos.size()))   continue;   // ⍣ N at EOS: no B

              if (tos[pos_B].get_Class() == TC_R_PARENT)   // ( LO ⍣ N )
                 continue;
           }

         // at this point either LO has a bracket axis (any shape of N),
         // or we have a literal N not already parenthesized as
         // (LO ⍣ N). Either way we disambiguate by putting LO in its
         // own parentheses.
         //

         // insert a left parenthesis
         {
           int pos_LO = pos_POWER - 1;
           if (pos_LO < 0)   continue;   // ⍣ at the very start of the
                                          // statement: no LO to parenthesize

           // LO itself can be a derived function, e.g. +/⍣2 or
           // 1 2 3+.×⍣2: walk back past any chain of monadic operators,
           // one level of dyadic-operator composition (F.G), a literal
           // axis [N], and a parenthesized/curly group to the true start
           // of LO, instead of grabbing just the one token immediately
           // left of ⍣ -- otherwise only the trailing piece got
           // parenthesized with ⍣, splitting the derived function apart
           // instead of keeping it whole. (Blake McBride, Bugs26 #5,
           // extended for Bugs27 #20/#38.)
           pos_LO = walk_back_LO_start(tos, pos_LO);

           tos.insert_1(pos_LO - 1);   // - 1 means insert before
           // the inserted ( gets the PC of the first token of LO
           tos[pos_LO].copy(Token_loc(Token(TOK_L_PARENT, int64_t(0)),
                                      tos[pos_LO + 1].get_PC()), LOC);
           ++pos_POWER;   // also shifted: it is a position after pos_LO
         }

         // insert a right parenthesis -- narrower than the (LO ⍣ N)
         // wrap used to be: close right before ⍣ itself, i.e. (LO) ⍣ N,
         // not after N. Semantically identical (⍣ binds to whatever
         // function immediately precedes it either way), but a plain
         // primitive LO combined with a trailing literal axis (e.g.
         // ⌽[1]⍣2) hits a separate, deeper parser bug when the whole
         // "LO⍣N" phrase sits inside one pair of parens -- "(LO)⍣N"
         // (parenthesizing only LO) parses correctly for the exact same
         // cases. See Bugs27 #38.
         //
         {
           tos.insert_1(pos_POWER - 1);   // - 1 means insert before
           // the inserted ) gets the PC of the last token of LO
           tos[pos_POWER].copy(Token_loc(Token(TOK_R_PARENT, int64_t(0)),
                                      tos[pos_POWER - 1].get_PC()), LOC);
         }

         t += 2;   // for the new ( and )
         progress = true;
       }
   return progress;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::fix_RANK_syntax(Token_string & tos, bool & has_power)
{
bool progress = false;
   /* Z ← A ⍤ y B  or  Z ← ⍤ y B

      where y is a near-int-vector of length 1, 2, or 3

      Collecting the items of such an expression can make the expression
      ambiguous. We therefore translate the above expression into:

      This function normalizes y to a 3-item vector as follows:

      A.   literal y1        →   Value_P Y ← y1 y1 y1
      B.   literal y1 j2     →   Value_P Y ← y2 y1 y2
      C.   literal y1 j2 j3  →   Value_P Y ← y1 y2 y3
           
      v is a scalar or vector with 3 ≥ ⍴,v and it defines the splitting points
      of ⍴Z, ⍴A, and ⍴B. (Up to) 3 splitting points V are needed and they are
      defined by V3 ← ⌽3⍴⌽v (see ISO p. 124).

      NOTE that:

      ⌽3⍴⌽ 1     ←→ 1 1 1   ⍝ same splitting point for Z, A, and B
      ⌽3⍴⌽ 1 2   ←→ 2 1 2   ⍝ same splitting point for Z and B
      ⌽3⍴⌽ 1 2 3 ←→ 1 2 3
    */

   // must not use loop() here since tos.size() varies inside the loop.
   //
   for (int t = 0; t < (int(tos.size()) - 2); ++t)
       {
         if (tos[t].get_tag() != TOK_OPER2_RANK)
            {
              if (tos[t].get_tag() == TOK_OPER2_POWER)   has_power = true;
              continue;   // next ⍤ (if any)
            }

         int pos_RANK = t;

         // LO carries a trailing bracket axis, e.g. ⌽[1]⍤y (Bugs28 #59 /
         // Bugs29 #3, extended for Bugs30 #21/#22): Prefix's parenthesis
         // reduction can never parse "LO[axis] ⍤ y" as a single derived
         // function, no matter what y looks like (literal, a bare
         // variable, or itself parenthesized) -- it always needs the
         // same narrower (LO[axis])⍤y grouping produced below. Computed
         // once, up front, so each of the "leave y/RO alone, Prefix will
         // sort it out" shortcuts below can be disabled specifically
         // when LO has an axis, instead of only the one case (a literal,
         // unparenthesized y) r2118 originally added this check for.
         //
         const bool LO_has_axis = pos_RANK > 0 &&
                                   tos[pos_RANK - 1].get_tag() == TOK_R_BRACK;

         // if y is already in parentheses: skip (...). In this case
         // Prefix::reduce_A_B__() will take care after the expression
         // in parentheses was evaluated -- unless LO_has_axis, which
         // Prefix cannot parse even once that parenthesized y group is
         // evaluated, e.g. (⌽[1]⍤(2))B or (⌽[1]⍤(Y))B (Bugs30 #22).
         //
         if (tos[t + 1].get_tag() == TOK_L_PARENT && !LO_has_axis)
            {                                        // f ⍤ ( RO )
              t = tos.find_closing_parent(t + 1);   // skip (...)
              continue;                              // next ⍤ (if any)
           }

         // ⍤ with axis: skip [ ]. ⍤ with axis is a GNU APL extension
         // that provides as axis (as opposed to an operand).
         //
         int pos_RO = pos_RANK + 1;
         if (tos[pos_RO].get_tag() == TOK_L_BRACK)
            {
              pos_RO = tos.find_closing_bracket(pos_RO) + 1;   // skip [...]
            }

         // j_length() is 0 both for a non-literal y (e.g. a bare
         // variable Y) and for a parenthesized RO reached from the
         // LO_has_axis branch just above (it stops at the very first
         // token, the '('). Normally that means "not literal, leave it
         // for Prefix" -- unless LO_has_axis, which needs the
         // (LO[axis])⍤y wrap regardless of whether y is literal
         // (Bugs30 #21).
         //
         const int len_y = j_length(tos, pos_RO);
         if (len_y == 0 && !LO_has_axis)   continue;   // y is not literal

         if (len_y > 3)   // ISO p. 124
            {
              // a parse error, not a runtime one: LENGTH_ERROR (i.e.
              // throw_apl_error()) built the error display from the
              // PREVIOUS statement and printed it twice (Bugs35 #37).
              //
              MORE_ERROR() << "⍤ y B: j too long (⍴j = " << len_y << ")";
              Error::throw_parse_error(E_LENGTH_ERROR, LOC, LOC);
            }

         // The EOS and already-parenthesized "(LO⍤y)" shortcuts below
         // only make sense for a literal y (len_y > 0); a non-literal or
         // parenthesized y reaching here only because LO_has_axis always
         // falls through to the (LO[axis]) wrap further down.
         //
         if (len_y > 0)
            {
              const int pos_B = pos_RO + len_y;

              if (pos_B >= int(tos.size()))   continue;   // ⍤ y at EOS

              if (!LO_has_axis &&
                  tos[pos_B].get_Class() == TC_R_PARENT)   // ( LO ⍤ y )
                 {
                   continue;
                 }
            }

         // at this point we have either a literal y with 1, 2, or 3
         // items, or (LO_has_axis only) a non-literal/parenthesized y.
         // Either way we disambiguate by putting LO[axis] in
         // parentheses on its own.
         //

         // insert a left parenthesis
         {
           int pos_LO = pos_RANK - 1;
           if (pos_LO < 0)   continue;   // ⍤ at the very start of the
                                          // statement: no LO to parenthesize

           // LO itself can be a derived function, e.g. +/⍤1 or
           // 1 2 3+.×⍤1: walk back past any chain of monadic operators,
           // one level of dyadic-operator composition (F.G), a literal
           // axis [N], and a parenthesized/curly group to the true start
           // of LO, instead of grabbing just the one token immediately
           // left of ⍤ -- otherwise only the trailing piece got
           // parenthesized with ⍤, splitting the derived function apart
           // instead of keeping it whole. (Blake McBride, Bugs26 #5,
           // extended for Bugs27 #20/#38.)
           pos_LO = walk_back_LO_start(tos, pos_LO);

           tos.insert_1(pos_LO - 1);   // - 1 means insert before
           // the inserted ( gets the PC of the first token of LO
           tos[pos_LO].copy(Token_loc(Token(TOK_L_PARENT, int64_t(0)),
                                      tos[pos_LO + 1].get_PC()), LOC);
           ++pos_RANK;   // also shifted: it is a position after pos_LO
         }

         // insert a right parenthesis -- narrower than the (LO ⍤ y)
         // wrap used to be: close right before ⍤ itself, i.e. (LO) ⍤ y,
         // not after y. Semantically identical (⍤ binds to whatever
         // function immediately precedes it either way), but a plain
         // primitive LO combined with a trailing literal axis (e.g.
         // ⌽[1]⍤2) hits a separate, deeper parser bug when the whole
         // "LO⍤y" phrase sits inside one pair of parens -- "(LO)⍤y"
         // (parenthesizing only LO) parses correctly for the exact same
         // cases. See Bugs27 #38.
         //
         {
           tos.insert_1(pos_RANK - 1);   // - 1 means insert before
           // the inserted ) gets the PC of the last token of LO
           tos[pos_RANK].copy(Token_loc(Token(TOK_R_PARENT, int64_t(0)),
                                      tos[pos_RANK - 1].get_PC()), LOC);
         }

         t += 2;   // for the new ( and )
         progress = true;
       }

   return progress;
}
//────────────────────────────────────────────────────────────────────────────
int
Parser::j_length(Token_string & tos, int pos)
{
int len = 0;
   for (size_t t = pos; t < tos.size(); ++t)
       {
         const Token & T = tos[t];
         const TokenTag tag = T.get_tag();
         if (tag == TOK_INTEGER)
            {
              ++len;
            }
         else if (tag == TOK_REAL)   // stupid ?
            {
              const APL_Float val = T.get_flt_val();
              // is_near_int64_t(), not is_near_int(): near_int() below
              // throws DOMAIN ERROR for e.g. 1E20 while parsing (Bugs35 #37)
              if (Cell::is_near_int64_t(val))
                 {
                   ++len;
                   if (len <= 3)   // replace float with integer
                      {
                        tos[t].copy(Token_loc(Token(TOK_INTEGER, Cell::near_int(val)),
                                              tos[t].get_PC()), LOC);
                      }
                 }
              else break;   // not literal
            }
         else if (tag == TOK_COMPLEX)   // even more stupid ?
            {
              const APL_Float imag = T.get_cpx_imag();
              if (!(Cell::is_near_zero(imag)))   break;
              const APL_Float real = T.get_cpx_real();
              if (!(Cell::is_near_int64_t(real)))   break;

              ++len;
              if (len <= 3)   // replace float with integer
                 {
                   tos[t].copy(Token_loc(Token(TOK_INTEGER, Cell::near_int(real)),
                                              tos[t].get_PC()), LOC);
                 }
            }
         else break;
       }
   return len;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::map_function_groups(Token_string & tos)
{
bool progress = false;
   for (size_t t = 0; (t + 2) < tos.size(); ++t)
       {
         const TokenTag & tag1 = tos[t + 1].get_tag();
         if (tag1 == TOK_OPER2_INNER)   // possibly ⎕xxx.subfun
            {
              const TokenTag & tag0 = tos[t].get_tag();
              const TokenTag & tag2 = tos[t + 2].get_tag();
              if (tag2 == TOK_SYMBOL &&
                  ( tag0 == TOK_F12_DOMINO ||
                    tag0 == TOK_Quad_CR    ||
                    tag0 == TOK_Quad_FFT   ||
                    tag0 == TOK_Quad_FIO   ||
                    tag0 == TOK_Quad_MX    ||
                    tag0 == TOK_Quad_RVAL))   // functions with a plain axis
                 {
                   const Function * topfun = tos[t].get_function();

                   // ⎕MX and ⎕FFT are compiled out (as plain, non-group
                   // Functions) when GSL resp. FFTW are unavailable; their
                   // stub constructors never call init_function_group(), so
                   // group_name stays 0. bad_subfun_name_ERROR() below would
                   // then dereference that null pointer. Treat "no
                   // subfunctions at all" the same as "not a group",
                   // i.e. fall through to ordinary SYNTAX ERROR handling
                   // instead of attempting the member-syntax rewrite
                   // (Blake McBride, Bugs20 #2).
                   //
                   if (!topfun->has_subfuns())   continue;

                   const Symbol * subfun_symbol = tos[t + 2].get_sym_ptr();
                   const UCS_string sub_name = subfun_symbol->get_name();
                   const sAxis axis = topfun->subfun_to_axis(sub_name);
                   if (axis < 0)   topfun->bad_subfun_name_ERROR(sub_name);

                   // valid axis. replace: topfun . subfun with topfun [subfun]
                   //
                   Value_P axis_val = IntScalar(axis, LOC);
                   tos[t + 1].copy(Token_loc(Token(TOK_AXIS, axis_val),   // . subfun
                                             tos[t + 1].get_PC()), LOC);
                   tos[t + 2].clear(LOC);
                   t += 2;
                   progress = true;
                 }
             else if (tag0 == TOK_Quad_SQL)   // ⎕SQL may have a complex axis
                 {
                   const Symbol * subfun_symbol = tos[t + 2].get_sym_ptr();
                   const UCS_string sub_name = subfun_symbol->get_name();
                   const Function * topfun = tos[t].get_function();
                   const sAxis axis = topfun->subfun_to_axis(sub_name);
                   if (axis < 0)   topfun->bad_subfun_name_ERROR(sub_name);

                   // valid axis. replace: ⎕SQL.subfun with either
                   // ⎕SQL[subfun] or with ⎕SQL[subfun;DB]
                   //
                   if (axis != 3 && axis != 4)   // ⎕SQL[subfun]
                      {
                        Value_P axis_val = IntScalar(axis, LOC);
                        tos[t + 1].copy(Token_loc(Token(TOK_AXIS, axis_val),   // . subfun
                                             tos[t + 1].get_PC()), LOC);
                        tos[t + 2].clear(LOC);
                        t += 2;
                        progress = true;
                      }
                   else
                      {
                        /* map:      T0     T1  T2      T3  T4
                              from:  ⎕SQL   .   query   DB  B
                                to:  ⎕SQL3  [   DB      ]   B
                           and:
                              from:  ⎕SQL   .   update  DB  B
                                to:  ⎕SQL4  [   DB      ]   B
                         */
                        const bool SQL_query = axis == 3;   // else: update
                        if ((t + 4) >= tos.size())   // too few token
                           {
                             MORE_ERROR() << "*** Too few arguments for ⎕SQL."
                                          << (SQL_query ? "query" : "update");
                             SYNTAX_ERROR;
                           }

                        // T0 keeps its PC, the [ gets the PC of the .
                        // and the ] gets the PC of DB.
                        //
                        tos[t + 0].copy(Token_loc(SQL_query
                                                  ? Quad_SQL_3::fun.get_token()
                                                  : Quad_SQL_4::fun.get_token(),
                                                  tos[t + 0].get_PC()), LOC);
                        tos[t + 1].copy(Token_loc(Token(TOK_L_BRACK, int64_t(0)),
                                                  tos[t + 1].get_PC()), LOC);   // T1
                        const Function_PC pc_DB = tos[t + 3].get_PC();
                        tos[t + 2].move_from(tos[t + 3], LOC);          // T2
                        tos[t + 3].copy(Token_loc(Token(TOK_R_BRACK, int64_t(0)),
                                                  pc_DB), LOC);   // T3
                        t += 3;
                      }
                 }
      
            }
       }
   return progress;
}
//────────────────────────────────────────────────────────────────────────────
/// in tos, (re-) mark the symbols left of ← as left symbols
void
Parser::mark_lsymb(Token_string & tos)
{
   loop(ass, tos.ssize())
      {
        if (tos[ass].get_Class() != TC_ASSIGN)   continue;

        // found ← in VAR VAR)← move backwards.
        // Before that we handle the special case of a vector specification,
        // i.e. (SYM SYM ... SYM) ← value
        //
        if (ass >= 3 && tos[ass - 1].get_Class() == TC_R_PARENT &&
                        tos[ass - 2].get_Class() == TC_SYMBOL   &&
                        tos[ass - 3].get_Class() == TC_SYMBOL)
           {
             // first make sure that this is really a vector specification.
             //
             const int syms_to = ass - 2;
             int syms_from = ass - 3;
             bool is_vector_spec = true;
             bool is_selective_spec = false;
             for (int a1 = ass - 4; a1 >= 0; --a1)
                 {
                   const TokenClass tc = tos[a1].get_Class();
                   if (tc == TC_L_PARENT)   // end of (...)←
                      {
                        syms_from = a1 + 1;
                        break;
                      }
                   if (tc == TC_SYMBOL)     continue; 
                   if ((1 << tc) & TCG_FUN12_OPER12)
                      {
                        is_selective_spec = true;
                        is_vector_spec = false;
                        break;
                      }

                   // something else
                   //
                   is_vector_spec = false;
                   break;
                 }

             if (is_selective_spec == is_vector_spec)   // none or both
                {
                  MORE_ERROR() <<
                      "Left of )← seems to be neither a vector "
                      "specification nor a selective specification";

                  // throw with the statement in line 2, since the error
                  // would otherwise show the statement of an earlier
                  // error (Bugs37 #42). tos is (still) in source order.
                  //
                  int lpar = (ass - 1) - tos[ass - 1].get_int_val2();
                  if (lpar < 0 || tos[lpar].get_Class() != TC_L_PARENT)
                     lpar = ass - 1;
                  Error error(E_LEFT_SYNTAX_ERROR, LOC);
                  tos.build_error_line_2(error, lpar, ass);
                  if (StateIndicator * si = Workspace::SI_top())
                     StateIndicator::get_error(si) = error;
                  throw error;
                }

             // if this is a vector specification, then mark all symbols
             // inside ( ... ) as TOK_LSYMB2.
             //
             if (is_vector_spec)
                {
                  for (int a1 = syms_from; a1 <= syms_to; ++a1)
                      tos[a1].ChangeTag(TOK_LSYMB2);
                }

             continue;
           }

        int bracks = 0;
        for (int prev = ass - 1; prev >= 0; --prev)
            {
              switch(tos[prev].get_Class())
                 {
                   case TC_R_BRACK: ++bracks;     break;
                   case TC_L_BRACK: --bracks;     break;
                   case TC_SYMBOL:  if (bracks)   break;   // inside [ ... ]
                                    if (tos[prev].get_tag() == TOK_SYMBOL)
                                       {
                                         tos[prev].ChangeTag(TOK_LSYMB);
                                       }
                                    // it could be that a system variable
                                    // is assigned, e.g. ⎕SVE←0. In that case
                                    // (actually always) we simply stop here.
                                    //

                                    /* fall-through */

                   case TC_END:     prev = 0;   // stop prev loop
                                    break;

                   default: break;
                 }
            }
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Parser::optimize_static_patterns(Token_string & tos)
{
   /* Replace:

       ∧∧, ∨∨, ⍲⍲, and ⍱⍱    with their bitwise token variant,
       ⎕FIO.function_name    with ⎕FIO[X] (via subfun_to_axis(function_name))
    */

bool TOK_VOID_inserted = false;
   loop(t, tos.ssize() - 1)   // -1 since we need tos[t + 1]
       {
         if (tos[t].get_tag() == TOK_OPER2_INNER    &&   // some f.g
             t                                      &&   // f exists
             tos[t-1].get_Class() == TC_FUN2        &&   // f is function
             tos[t-1].get_function()                &&   // f is valid
             tos[t-1].get_function()->has_subfuns() &&   // f is ⎕FIO or ⎕CR
             tos[t+1].get_tag() == TOK_SYMBOL)           // g is (sub-) name
            {
              const Symbol * symbol = tos[t+1].get_sym_ptr();   // subfun name
              cFunction_P fun = tos[t-1].get_function();
              const sAxis axis = fun->subfun_to_axis(symbol->get_name());
              if (axis != -1)   // subfunction is valid
                 {
                   Value_P function_axis = IntScalar(axis, LOC);
                   Token tok_axis(TOK_AXIS, function_axis);
                   tos[t].copy(Token_loc(tok_axis, tos[t].get_PC()), LOC);   // replace . with [X]
                   ++t;
                   tos[t++].clear(LOC);               // invalidate g
                   TOK_VOID_inserted = true;
                 }
              continue;
            }

         if (tos[t].get_tag() != TOK_F12_ENCODE)   continue;

         switch(tos[t+1].get_tag())
            {
              case TOK_F2_AND:
                   tos[t].copy(Token_loc(Token(TOK_F2_AND_B, &Bif_F2_AND_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              case TOK_F2_OR:
                   tos[t].copy(Token_loc(Token(TOK_F2_OR_B, &Bif_F2_OR_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              case TOK_F2_NAND:
                   tos[t].copy(Token_loc(Token(TOK_F2_NAND_B, &Bif_F2_NAND_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              case TOK_F2_NOR:
                   tos[t].copy(Token_loc(Token(TOK_F2_NOR_B, &Bif_F2_NOR_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              case TOK_F2_EQUAL:
                   tos[t].copy(Token_loc(Token(TOK_F2_EQUAL_B, &Bif_F2_EQUAL_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              case TOK_F2_UNEQU:
                   tos[t].copy(Token_loc(Token(TOK_F2_UNEQ_B, &Bif_F2_UNEQ_B::fun),
                                         tos[t].get_PC()), LOC);
                   tos[++t].clear(LOC);
                   TOK_VOID_inserted = true;
                   continue;

              default: break;
            }
       }

   if (TOK_VOID_inserted)   tos.remove_TOK_VOID();
}
//────────────────────────────────────────────────────────────────────────────
void
Parser::parse_log(int N, const Token_string & tos)
{
   Log(LOG_parse)
      {
        CERR << "parse " << N << " [" << tos.ssize() << "]: " << endl;
        tos.print(CERR, 3);
      }
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
Parser::parse_statement(Token_string & tos, bool optimize,
                        bool keep_left_value)
{
   // Unbalanced-paren/bracket/curly pre-check, before any step below
   // that could itself throw on the same imbalance. remove_nongrouping_
   // parantheses() (next) calls Token_string::find_closing_parent(),
   // which throws a raw SYNTAX_ERROR directly (Token_string.cc) rather
   // than returning an ErrorCode -- for a genuinely malformed statement
   // being parsed for the first time, that throw happens before this
   // statement has an SI entry, so Error::update_error_info() ends up
   // copying the caret/text of the PREVIOUS immediate-execution error
   // instead (reported twice: once under the old statement, once under
   // the new one, and the correct "Unbalanced left parenthesis" message
   // from match_par_bra() below -- the real, ErrorCode-returning check
   // -- never even runs). Running match_par_bra() here first, before
   // any mutating step, reports the SAME imbalance cleanly instead.
   // (match_par_bra() also runs again, unchanged, at the end of this
   // function on the fully-optimized tos: its int_val2 side effect
   // (paren/bracket distances) must reflect the FINAL token positions,
   // not these pre-optimization ones, so this early call's side effect
   // is deliberately allowed to be overwritten by the later one.)
   // See Bugs27 #49.
   //
   if (const ErrorCode ec = match_par_bra(tos, false))
      {
        loop(t, tos.ssize())   tos[t].clear(LOC);
        return ec;
      }

   // Empty-parentheses pre-check, for the same reason as the unbalanced-
   // paren pre-check above: collect_value_groups() (called indirectly via
   // remove_nongrouping_parantheses() below) throws a raw SYNTAX_ERROR
   // directly when it sees "( )" with nothing in between, which happens
   // before this statement has an SI entry, losing the statement text and
   // carets and getting reported under the previous statement instead
   // (Blake McBride, Bugs28 #61). Catching it here, in the same
   // ErrorCode-returning style as match_par_bra(), reports it cleanly.
   //
   loop(t, tos.ssize() - 1)
       {
         if (tos[t].get_tag() != TOK_L_PARENT)     continue;
         if (tos[t + 1].get_tag() != TOK_R_PARENT)   continue;
         loop(t1, tos.ssize())   tos[t1].clear(LOC);
         return E_SYNTAX_ERROR;
       }

   bool has_power_op = false;
   if (fix_RANK_syntax(tos, has_power_op))   parse_log(2, tos);

   if (has_power_op && fix_POWER_syntax(tos))   parse_log(3, tos);

   // 1. convert (X) into X and ((X...)) into (X...)
   //
   {
     const bool progress = remove_nongrouping_parantheses(tos);
     if (progress)
        {
          tos.remove_TOK_VOID();
          parse_log(4, tos);
        }
   }

   {
     const bool progress = map_function_groups(tos);
     if (progress)
        {
          tos.remove_TOK_VOID();
          parse_log(5, tos);
        }
   }

   // 2. convert groups like '(' val val...')' into single APL values
   // A group is defined as 2 or more values enclosed in '(' and ')'.
   // For example: (1 2 3) or (4 5 (6 7) 8 9)
   //
   for (int progress = true; progress;)
      {
        progress = collect_value_groups(tos);
        if (progress)
           {
             tos.remove_TOK_VOID();
             parse_log(6, tos);
           }
      }

   // 3. convert vectors like 1 2 3 or '1' 2 '3' into single APL values
   //
   {
     const bool progress = collect_constants(tos);
     if (progress)
        {
          tos.remove_TOK_VOID();
          parse_log(7, tos);
        }
   }

   /* parse_statement() is normally called with optimize == true; However,
      Executable::reparse() calls it with optimize == false in order to
      restore the body before it was optimized.
     
      To disable optimizations you must set the enable arg(s) in
      Performance.def to 0 (as opposed to setting optimize to false.
     */

   // 4. maybe optimize literal axes and short primitives
   //
   if (optimize)
      {
        const bool axes = DO_FT_LITERAL_AXIS && optimize_literal_axes(tos);
        if (axes)   tos.remove_TOK_VOID();

        for (bool progress = true; progress;)
            {
              progress = DO_FT_SHORT_PRIMITIVE &&
                         optimize_short_primitives(tos, keep_left_value);
              if (progress)   tos.remove_TOK_VOID();
            }
      }
   parse_log(8, tos);

   // 5. special case: single APL value (to speed up ⍎)
   //
   if (tos.ssize() == 1)
      {
        // was "parse 5: ..." -- collides with the real step 5
        // (map_function_groups, logged via parse_log(5, tos) above);
        // debug-trace-only mislabeling, no functional effect, but
        // makes -l LOG_parse output look like step 5 ran twice.
        Log(LOG_parse)   CERR << "parse (single value): " << tos[0] << endl;
        return E_NO_ERROR;
      }
   parse_log(9, tos);

   // 6. mark symbol left of ← as LSYMB
   //
   mark_lsymb(tos);
   parse_log(10, tos);

   // 7. replace bitwise functions ⊤∧, ⊤∨, ⊤⍲, and ⊤⍱ by their bitwise variant
   //
   if (optimize)
      {
        optimize_static_patterns(tos);
        parse_log(11, tos);
      }

   // 8. update the distances between ( and ), [ and ], or { and }. After
   //    that, tos.ssize() must not be changed anymore.
   //
   if (const ErrorCode ec = match_par_bra(tos, false))
       {
         loop(t, tos.ssize())   tos[t].clear(LOC);
         return ec;
       }

   return E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
bool
Parser::remove_nongrouping_parantheses(Token_string & tos)
{
   //
   // 1. replace ((X...)) by: (X...)
   // 2. replace (X)      by: X for a single token X,
   //
bool ret = false;        // total progress
bool progress = false;   // per iteration progress
   do {
         progress = false;
         loop(t, tos.ssize() - 2)
             {
               if (tos[t].get_Class() != TC_L_PARENT)   continue;

               const int closing = tos.find_closing_parent(t);

               // check for case 1.
               //
               if (tos[t + 1].get_Class() == TC_L_PARENT)
                  {
                    // tos[t] is (( ...
                    //
                    const int closing_1 = tos.find_closing_parent(t + 1);
                    if (closing == (closing_1 + 1))
                       {
                         // tos[closing_1] is )) ...
                         // We have case 1. but not for example ((...)(...))
                         // remove redundant tos[t] and tos[closing] because
                         // ((...)) are not "not separating"
                         //
                         ret = progress = true;

                         // the remaining (...) gets the PC of the outer (
                         //
                         const Token_loc tl_L(tos[t + 1].get_token(),
                                              tos[t].get_PC());
                         tos[t + 1].copy(tl_L, LOC);
                         tos[t].clear(LOC);
                         tos[closing].clear(LOC);
                         continue;
                       }
                  }

               // check for case 2.
               //
               if (closing != (t + 2))   continue;


               // case 2. We have tos[t] = ( X ) ...
               //
               // (X) : "not grouping" if X is a scalar. 
               // If X is non-scalar, enclose it
               //
               ret = progress = true;

               // the remaining X gets the PC of the ( (leftmost)
               //
               {
                 const Token_loc tl_X(tos[t + 1].get_token(), tos[t].get_PC());
                 tos[t + 2].copy(tl_X, LOC);
               }

               // we "remember" the nongrouping parantheses to disambiguate
               // e,g, SYM/xxx from (SYM)/xxx
               //
               if (tos[t + 2].get_tag() == TOK_SYMBOL)
                  tos[t + 2].ChangeTag(TOK_P_SYMB);
               tos[t + 1].clear(LOC);
               tos[t].clear(LOC);
               ++t;   // skip tos[t + 1]
             }
       }
   while(progress);
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
