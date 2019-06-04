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

#include <string.h>

#include "Bif_F12_FORMAT.hh"
#include "Bif_F12_SORT.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_OPER1_COMMUTE.hh"
#include "Bif_OPER1_EACH.hh"
#include "Bif_OPER2_INNER.hh"
#include "Bif_OPER2_OUTER.hh"
#include "Bif_OPER2_POWER.hh"
#include "Bif_OPER2_RANK.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "Bif_OPER1_SCAN.hh"
#include "CharCell.hh"
#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "Output.hh"
#include "PointerCell.hh"
#include "Symbol.hh"
#include "SystemLimits.hh"
#include "SystemVariable.hh"
#include "Tokenizer.hh"
#include "Value.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
inline ostream & operator << (ostream & out, const Unicode_source & src)
   { loop(s, src.rest())   out << src[s];   return out; }
//-----------------------------------------------------------------------------
/** convert \b UCS_string input into a Token_string tos.
*/
ErrorCode
Tokenizer::tokenize(const UCS_string & input, Token_string & tos)
{
   try
      {
        do_tokenize(input, tos);
      }
   catch (Error err)
      {
        const int caret_1 = input.size() - rest_1;
        const int caret_2 = input.size() - rest_2;
        err.set_error_line_2(input, caret_1, caret_2);
        return err.get_error_code();
      }

   return E_NO_ERROR;
}
//-----------------------------------------------------------------------------
/** convert \b UCS_string input into a Token_string tos.
*/
void
Tokenizer::do_tokenize(const UCS_string & input, Token_string & tos)
{
   Log(LOG_tokenize)
      CERR << "tokenize: input[" << input.size() << "] is: «"
           << input << "»" << endl;

Unicode_source src(input);
   while ((rest_1 = rest_2 = src.rest()) != 0)
      {
        Unicode uni = *src;
        if (uni == UNI_COMMENT)             break;   // ⍝ comment
        if (uni == UNI_ASCII_NUMBER_SIGN)   break;   // # comment

        const Token tok = Avec::uni_to_token(uni, LOC);

        Log(LOG_tokenize)
           {
             Unicode_source s1(src, 0, 24);
             CERR << "  tokenize(" <<  src.rest() << " chars) sees [tag "
                  << tok.tag_name() << " «" << uni << "»] " << s1;
             if (src.rest() != s1.rest())   CERR << " ...";
             CERR << endl;
           }

        switch(tok.get_Class())
            {
              case TC_END:   // chars without APL meaning
                   rest_2 = src.rest();
                   {
                     Log(LOG_error_throw)
                     CERR << endl << "throwing "
                          << Error::error_name(E_NO_TOKEN)
                          << " in  Tokenizer" << endl;

                     char cc[20];
                     snprintf(cc, sizeof(cc), "U+%4.4X (", uni);
                     MORE_ERROR() << "Tokenizer: No token for Unicode "
                                  <<  cc << uni << ")\nInput: " << input;
                     Error error(E_NO_TOKEN, LOC);
                     throw error;
                   }
                   break;

              case TC_RETURN:
              case TC_LINE:
              case TC_VALUE:
              case TC_INDEX:
                   CERR << "Offending token: " << tok.get_tag()
                        << " (" << tok << ")" << endl;
                   if (tok.get_tag() == TOK_CHARACTER)
                      CERR << "Unicode: " << UNI(tok.get_char_val()) << endl;
                   rest_2 = src.rest();
                   Error::throw_parse_error(E_NON_APL_CHAR, LOC, loc);
                   break;

              case TC_VOID:
                   // Avec::uni_to_token returns TC_VOID for non-apl characters
                   //
                   rest_2 = src.rest();
                   UERR << "Unknown APL character: " << uni
                        << " (" << UNI(uni) << ")" << endl;
                   Error::throw_parse_error(E_NON_APL_CHAR, LOC, loc);
                   break;

              case TC_SYMBOL:
                   if (Avec::is_quad(uni))
                      {
                         tokenize_quad(src, tos);
                      }
                   else if (uni == UNI_QUOTE_Quad)
                      {
                        ++src;
                        tos.push_back(Token(TOK_Quad_QUOTE,
                                         &Workspace::get_v_Quad_QUOTE()));
                      }
                   else if (uni == UNI_ALPHA)
                      {
                        ++src;
                        tos.push_back(Token(TOK_ALPHA,
                                         &Workspace::get_v_ALPHA()));
                      }
                   else if (uni == UNI_ALPHA_UNDERBAR)
                      {
                        ++src;
                        tos.push_back(Token(TOK_ALPHA_U,
                                            &Workspace::get_v_ALPHA_U()));
                      }
                   else if (uni == UNI_CHI)
                      {
                        ++src;
                        tos.push_back(Token(TOK_CHI,
                                            &Workspace::get_v_CHI()));
                      }
                   else if (uni == UNI_LAMBDA)
                      {
                        // this could be λ like in λ← ...
                        // or λ1 or λ2 or ... as in ... ⍺ λ1 ⍵
                        //
                        if (src.rest() > 1 && Avec::is_digit(src[1]))   // λn
                           {
                             tokenize_symbol(src, tos);
                           }
                        else   // λ
                           {
                             ++src;
                             tos.push_back(Token(TOK_LAMBDA,
                                           &Workspace::get_v_LAMBDA()));
                           }
                      }
                   else if (uni == UNI_OMEGA)
                      {
                        ++src;
                        tos.push_back(Token(TOK_OMEGA,
                                            &Workspace::get_v_OMEGA()));
                      }
                   else if (uni == UNI_OMEGA_UNDERBAR)
                      {
                        ++src;
                        tos.push_back(Token(TOK_OMEGA_U,
                                            &Workspace::get_v_OMEGA_U()));
                      }
                   else
                      {
                        tokenize_symbol(src, tos);
                      }
                   break;

              case TC_FUN0:
              case TC_FUN12:
              case TC_OPER1:
                   tokenize_function(src, tos);
                   break;

              case TC_OPER2:
                   if (tok.get_tag() == TOK_OPER2_INNER && src.rest())
                      {
                        // tok is a dot. This could mean that . is either
                        //
                        // the start of a number:     e.g. +.3
                        // or an operator:            e.g. +.*
                        // or a syntax error:         e.g. Done.
                        //
                        if (src.rest() == 1)   // syntax error
                           Error::throw_parse_error(E_SYNTAX_ERROR, LOC, loc);

                        Unicode uni_1 = src[1];
                        const Token tok_1 = Avec::uni_to_token(uni_1, LOC);
                        if ((tok_1.get_tag() & TC_MASK) == TC_NUMERIC)
                           tokenize_number(src, tos);
                        else
                           tokenize_function(src, tos);
                      }
                   else
                      {
                        tokenize_function(src, tos);
                      }
                   if (tos.size() >= 2 &&
                       tos.back().get_tag() == TOK_OPER2_INNER &&
                       tos[tos.size() - 2].get_tag() == TOK_JOT)
                      {
                        new (&tos.back()) Token(TOK_OPER2_OUTER,
                                                Bif_OPER2_OUTER::fun);
                      }

                   break;

              case TC_R_ARROW:
                   ++src;
                   if (src.rest())   tos.push_back(tok);
                   else              tos.push_back(Token(TOK_ESCAPE));
                   break;

              case TC_ASSIGN:
                   ++src;
                   {
                     const bool sym = tos.size() >= 1 &&
                                tos[tos.size() - 1].get_tag() == TOK_SYMBOL;
                     const bool dia = tos.size() > 1 &&
                                tos[tos.size() - 2].get_tag() == TOK_DIAMOND;
                     const bool col = tos.size() > 1 &&
                                tos[tos.size() - 2].get_tag() == TOK_COLON;

                   tos.push_back(tok);

                   // change token tag of ← if:
                   //
                   //   SYM ←   (at the start of line),        or
                   // ◊ SYM ←   (at the start of statement),   or
                   // : SYM ←   (at the start of statement after label)
                   //
                   if (sym && ((tos.size() == 2) || dia || col))
                      tos.back().ChangeTag(TOK_ASSIGN1);
                   }
                   break;

              case TC_L_PARENT:
              case TC_R_PARENT:
              case TC_L_BRACK:
              case TC_R_BRACK:
              case TC_L_CURLY:
              case TC_R_CURLY:
                   ++src;
                   tos.push_back(tok);
                   break;

              case TC_DIAMOND:
                   ++src;
                   tos.push_back(tok);
                   break;

              case TC_COLON:
                   if (pmode != PM_FUNCTION)
                      {
                        rest_2 = src.rest();
                        if (pmode == PM_EXECUTE)
                           Error::throw_parse_error(E_ILLEGAL_COLON_EXEC,
                                                    LOC, loc);
                        else
                           Error::throw_parse_error(E_ILLEGAL_COLON_STAT,
                                                    LOC, loc);
                      }

                   ++src;
                   tos.push_back(tok);
                   break;

              case TC_NUMERIC:
                   tokenize_number(src, tos);
                   break;

              case TC_SPACE:
              case TC_NEWLINE:
                   ++src;
                   break;

              case TC_QUOTE:
                   if (tok.get_tag() == TOK_QUOTE1)
                      tokenize_string1(src, tos);
                   else
                      tokenize_string2(src, tos);
                   break;

              default:
                   CERR << "Input: " << input << endl
                        << "uni:   " << uni << endl
                        << "Token = " << tok.get_tag() << endl;

                   if (tok.get_Id() != ID_No_ID)
                      {
                        CERR << ", Id = " << Id(tok.get_tag() >> 16);
                      }
                   CERR << endl;
                   Assert(0 && "Should not happen");
            }
      }

   Log(LOG_tokenize)
      CERR << "tokenize() done (no error)" << endl;
}
//-----------------------------------------------------------------------------
void
Tokenizer::tokenize_function(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)   CERR << "tokenize_function(" << src << ")" << endl;

const Unicode uni = src.get();
const Token tok = tokenize_function(uni);
   tos.push_back(tok);
}
//-----------------------------------------------------------------------------
Token
Tokenizer::tokenize_function(Unicode uni)
{
const Token tok = Avec::uni_to_token(uni, LOC);

#define sys(t, f) \
   case TOK_ ## t: return Token(tok.get_tag(), Bif_ ## f::fun);   break;

   switch(tok.get_tag())
      {
        sys(F0_ZILDE,      F0_ZILDE)
        sys(F1_EXECUTE,    F1_EXECUTE)

        sys(F2_AND,        F2_AND)
        sys(F2_EQUAL,      F2_EQUAL)
        sys(F2_FIND,       F2_FIND)
        sys(F2_GREATER,    F2_GREATER)
        sys(F2_INDEX,      F2_INDEX)
        sys(F2_LESS,       F2_LESS)
        sys(F2_LEQ,        F2_LEQ)
        sys(F2_MEQ,        F2_MEQ)
        sys(F2_NAND,       F2_NAND)
        sys(F2_NOR,        F2_NOR)
        sys(F2_OR,         F2_OR)
        sys(F2_UNEQ,       F2_UNEQ)

        sys(F12_BINOM,     F12_BINOM)
        sys(F12_CIRCLE,    F12_CIRCLE)
        sys(F12_COMMA,     F12_COMMA)
        sys(F12_COMMA1,    F12_COMMA1)
        sys(F12_DECODE,    F12_DECODE)
        sys(F12_DIVIDE,    F12_DIVIDE)
        sys(F12_DOMINO,    F12_DOMINO)
        sys(F12_DROP,      F12_DROP)
        sys(F12_ELEMENT,   F12_ELEMENT)
        sys(F12_ENCODE,    F12_ENCODE)
        sys(F12_EQUIV,     F12_EQUIV)
        sys(F12_FORMAT,    F12_FORMAT)
        sys(F12_INDEX_OF,  F12_INDEX_OF)
        sys(F2_INTER,      F2_INTER)
        sys(F2_LEFT,       F2_LEFT)
        sys(F12_LOGA,      F12_LOGA)
        sys(F12_MINUS,     F12_MINUS)
        sys(F12_NEQUIV,    F12_NEQUIV)
        sys(F12_PARTITION, F12_PARTITION)
        sys(F12_PICK,      F12_PICK)
        sys(F12_PLUS,      F12_PLUS)
        sys(F12_POWER,     F12_POWER)
        sys(F12_RHO,       F12_RHO)
        sys(F2_RIGHT,      F2_RIGHT)
        sys(F12_RND_DN,    F12_RND_DN)
        sys(F12_RND_UP,    F12_RND_UP)
        sys(F12_ROLL,      F12_ROLL)
        sys(F12_ROTATE,    F12_ROTATE)
        sys(F12_ROTATE1,   F12_ROTATE1)
        sys(F12_SORT_ASC,  F12_SORT_ASC)
        sys(F12_SORT_DES,  F12_SORT_DES)
        sys(F12_STILE,     F12_STILE)
        sys(F12_TAKE,      F12_TAKE)
        sys(F12_TRANSPOSE, F12_TRANSPOSE)
        sys(F12_TIMES,     F12_TIMES)
        sys(F12_UNION,     F12_UNION)
        sys(F12_WITHOUT,   F12_WITHOUT)

        sys(JOT,           JOT)

        sys(OPER1_COMMUTE, OPER1_COMMUTE)
        sys(OPER1_EACH,    OPER1_EACH)
        sys(OPER2_POWER,   OPER2_POWER)
        sys(OPER2_RANK,    OPER2_RANK)
        sys(OPER1_REDUCE,  OPER1_REDUCE)
        sys(OPER1_REDUCE1, OPER1_REDUCE1)
        sys(OPER1_SCAN,    OPER1_SCAN)
        sys(OPER1_SCAN1,   OPER1_SCAN1)

        sys(OPER2_INNER,   OPER2_INNER)

        default: break;
      }

   // CAUTION: cannot print entire token here because Avec::uni_to_token()
   // inits the token tag but not any token pointers!
   //
   CERR << endl << "Token = " << tok.get_tag() << endl;
   Assert(0 && "Missing Function");

#undef sys
   return tok;
}
//-----------------------------------------------------------------------------
void
Tokenizer::tokenize_quad(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)
      CERR << "tokenize_quad(" << src.rest() << " chars)"<< endl;

   src.get();               // discard (possibly alternative) ⎕
UCS_string ucs(UNI_Quad_Quad);
   Assert(ucs[0]);

   if (src.rest() > 0)   ucs.append(src[0]);
   if (src.rest() > 1)   ucs.append(src[1]);
   if (src.rest() > 2)   ucs.append(src[2]);
   if (src.rest() > 3)   ucs.append(src[3]);
   if (src.rest() > 4)   ucs.append(src[4]);

int len = 0;
const Token t = Workspace::get_quad(ucs, len);
   src.skip(len - 1);
   tos.push_back(t);
}
//-----------------------------------------------------------------------------
/** tokenize a single quoted string.
 ** If the string is a single character, then we
 **  return a TOK_CHARACTER. Otherwise we return TOK_APL_VALUE1.
 **/
void
Tokenizer::tokenize_string1(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)   CERR << "tokenize_string1(" << src << ")" << endl;

const Unicode uni = src.get();
   Assert(Avec::is_single_quote(uni));

UCS_string string_value;
bool got_end = false;

   while (src.rest())
       {
         const Unicode uni = src.get();

         if (Avec::is_single_quote(uni))
            {
              // a single ' is the end of the string, while a double '
              // (i.e. '') is a single '. 
              //
              if ((src.rest() == 0) || !Avec::is_single_quote(*src))
                 {
                   got_end = true;
                   break;
                 }

              string_value.append(UNI_SINGLE_QUOTE);
              ++src;      // skip the second '
            }
         else if (uni == UNI_ASCII_CR)
            {
              continue;
            }
         else if (uni == UNI_ASCII_LF)
            {
              rest_2 = src.rest();
              Error::throw_parse_error(E_NO_STRING_END, LOC, loc);
            }
         else
            {
              string_value.append(uni);
            }
       }

   if (!got_end)   Error::throw_parse_error(E_NO_STRING_END, LOC, loc);

   if (string_value.size() == 1)   // scalar
      {
        tos.push_back(Token(TOK_CHARACTER, string_value[0]));
      }
   else
      {
        tos.push_back(Token(TOK_APL_VALUE1, Value_P(string_value, LOC)));
      }
}
//-----------------------------------------------------------------------------
/** tokenize a double quoted string.
 ** If the string is a single character, then we
 **  return a TOK_CHARACTER. Otherwise we return TOK_APL_VALUE1.
 **/
void
Tokenizer::tokenize_string2(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)   CERR << "tokenize_string2(" << src << ")" << endl;

   // skip the leading "
   {
     const Unicode uni = src.get();
     if (uni)   { /* do nothing, needed for -Wall */ }

     Assert1(uni == UNI_ASCII_DOUBLE_QUOTE);
   }

UCS_string string_value;
bool got_end = false;

   while (src.rest())
       {
         const Unicode uni = src.get();

         if (uni == UNI_ASCII_DOUBLE_QUOTE)     // terminating "
            {
              got_end = true;
              break;
            }
         else if (uni == UNI_ASCII_CR)          // ignore CR
            {
              continue;
            }
         else if (uni == UNI_ASCII_LF)          // end of line before "
            {
              rest_2 = src.rest();
              Error::throw_parse_error(E_NO_STRING_END, LOC, loc);
            }
         else if (uni == UNI_ASCII_BACKSLASH)   // backslash
            {
              const Unicode uni1 = src.get();
              switch(uni1)
                 {
                   case '0':  string_value.append(UNI_ASCII_NUL);         break;
                   case 'a':  string_value.append(UNI_ASCII_BEL);         break;
                   case 'b':  string_value.append(UNI_ASCII_BS);          break;
                   case 't':  string_value.append(UNI_ASCII_HT);          break;
                   case 'n':  string_value.append(UNI_ASCII_LF);          break;
                   case 'v':  string_value.append(UNI_ASCII_VT);          break;
                   case 'f':  string_value.append(UNI_ASCII_FF);          break;
                   case 'r':  string_value.append(UNI_ASCII_CR);          break;
                   case '[':  string_value.append(UNI_ASCII_ESC);         break;
                   case '"':  string_value.append(UNI_ASCII_DOUBLE_QUOTE);break;
                   case '\\': string_value.append(UNI_ASCII_BACKSLASH);   break;
                   default:   string_value.append(uni);
                              string_value.append(uni1);
                 }
            }
         else
            {
              string_value.append(uni);
            }
       }

   if (!got_end)   Error::throw_parse_error(E_NO_STRING_END, LOC, loc);

   else
      {
        tos.push_back(Token(TOK_APL_VALUE1, Value_P(string_value, LOC)));
      }
}
//-----------------------------------------------------------------------------
void
Tokenizer::tokenize_number(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)   CERR << "tokenize_number(" << src << ")" << endl;

   // numbers:
   // real
   // real 'J' real
   // real 'D' real   // magnitude + angle in degrees
   // real 'R' real   // magnitude + angle in radian

APL_Float   real_flt = 0.0;   // always valid
APL_Integer real_int = 0;     // valid if need_float is false
bool        real_need_float = false;
const bool real_valid = tokenize_real(src, real_need_float, real_flt, real_int);
   if (!real_need_float)   real_flt = real_int;
   if (!real_valid)
      {
        rest_2 = src.rest();
        Error::throw_parse_error(E_BAD_NUMBER, LOC, loc);
      }

   if (src.rest() && (*src == UNI_ASCII_J || *src == UNI_ASCII_j))
      {
        ++src;   // skip 'J'

        APL_Float   imag_flt = 0.0;   // always valid
        APL_Integer imag_int = 0;     // valid if imag_need_float is false
        bool imag_need_float = false;
        const bool imag_valid = tokenize_real(src, imag_need_float,
                                              imag_flt, imag_int);
        if (!imag_need_float)   imag_flt = imag_int;

        if (!imag_valid)
           {
             --src;   // undo skip 'J'
             if (real_need_float)
                {
                  tos.push_back(Token(TOK_REAL,    real_flt));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: real " << real_flt << endl;
                }
             else
                {
                  tos.push_back(Token(TOK_INTEGER, real_int));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: integer " << real_int << endl;
                }
             goto done;;
           }

        tos.push_back(Token(TOK_COMPLEX, real_flt, imag_flt));
        Log(LOG_tokenize)   CERR << "  tokenize_number: complex "
                                 << real_flt << "J" << imag_flt << endl;
      }
   else if (src.rest() && (*src == UNI_ASCII_D || *src == UNI_ASCII_d))
      {
        ++src;   // skip 'D'

        APL_Float   degrees_flt = 0;  // always valid
        APL_Integer degrees_int = 0;  // valid if imag_floating is true
        bool imag_need_float = false;
        const bool imag_valid = tokenize_real(src, imag_need_float,
                                              degrees_flt, degrees_int);
        if (!imag_need_float)   degrees_flt = degrees_int;

        if (!imag_valid)
           {
             --src;   // undo skip 'D'
             if (real_need_float)
                {
                  tos.push_back(Token(TOK_REAL,    real_flt));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: real " << real_flt << endl;
                }
             else
                {
                  tos.push_back(Token(TOK_INTEGER, real_int));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: integer " << real_int << endl;
                }
             goto done;;
           }

        // real_flt is the magnitude and the angle is in degrees.
        //
        APL_Float real = real_flt * cos(M_PI*degrees_flt / 180.0);
        APL_Float imag = real_flt * sin(M_PI*degrees_flt / 180.0);
        tos.push_back(Token(TOK_COMPLEX, real, imag));
        Log(LOG_tokenize)   CERR << "  tokenize_number: complex " << real
                                 << "J" << imag << endl;
      }
   else if (src.rest() && (*src == UNI_ASCII_R || *src == UNI_ASCII_r))
      {
        ++src;   // skip 'R'

        APL_Float radian_flt;     // always valid
        APL_Integer radian_int;   // valid if imag_floating is true
        bool radian_need_float = false;
        const bool imag_valid = tokenize_real(src, radian_need_float,
                                              radian_flt, radian_int);
        if (!radian_need_float)   radian_flt = radian_int;

        if (!imag_valid)
           {
             --src;   // undo skip 'R'
             if (real_need_float)
                {
                  tos.push_back(Token(TOK_REAL,    real_flt));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: real " << real_flt << endl;
                }
             else
                {
                  tos.push_back(Token(TOK_INTEGER, real_int));
                  Log(LOG_tokenize)
                     CERR << "  tokenize_number: integer " << real_int << endl;
                }
             goto done;;
           }

        // real_flt is the magnitude and the angle is in radian.
        //
        APL_Float real = real_flt * cos(radian_flt);
        APL_Float imag = real_flt * sin(radian_flt);
        tos.push_back(Token(TOK_COMPLEX, real, imag));
        Log(LOG_tokenize)   CERR << "  tokenize_number: complex " << real
                                 << "J" << imag << endl;
      }
   else 
      {
        if (real_need_float)
           {
             tos.push_back(Token(TOK_REAL,    real_flt));
             Log(LOG_tokenize)
                CERR << "  tokenize_number: real " << real_flt << endl;
           }
        else
           {
             tos.push_back(Token(TOK_INTEGER, real_int));
             Log(LOG_tokenize)
                CERR << "  tokenize_number: integer " << real_int << endl;
           }
      }

done:
   // ISO 13751 requires a space between two numeric scalar literals (page 42),
   // but not between a numeric scalar literal and an identifier.
   //
   // IBM APL2 requires a space in both cases. For example, 10Q10 is tokenized
   // as  10Q 10
   //
   // We follow ISO. The second numeric literal cannot start with 0-9 because
   // that would have been eaten by the first literal. Therefor the only cases
   // remaining to be checked are a numeric scalar literal followed by ¯
   // or by . (page 42)
   //
   if (src.rest())
      {
        if (*src == UNI_OVERBAR || *src == '.')
           Error::throw_parse_error(E_BAD_NUMBER, LOC, loc);
      }
}
//-----------------------------------------------------------------------------

enum { MAX_TOKENIZE_DIGITS_1 = 20,                       // incl. rounding digit
       MAX_TOKENIZE_DIGITS = MAX_TOKENIZE_DIGITS_1 - 1   // excl. rounding digit
     };

#define exp_0_9(x) x ## 0L, x ## 1L, x ## 2L, x ## 3L, x ## 4L,  \
                           x ## 5L, x ## 6L, x ## 7L, x ## 8L, x ## 9L, 

static const long double expo_tab[310] = 
{
   exp_0_9(1E)   exp_0_9(1E1)  exp_0_9(1E2)  exp_0_9(1E3)  exp_0_9(1E4)
   exp_0_9(1E5)  exp_0_9(1E6)  exp_0_9(1E7)  exp_0_9(1E8)  exp_0_9(1E9)
   exp_0_9(1E10) exp_0_9(1E11) exp_0_9(1E12) exp_0_9(1E13) exp_0_9(1E14)
   exp_0_9(1E15) exp_0_9(1E16) exp_0_9(1E17) exp_0_9(1E18) exp_0_9(1E19)
   exp_0_9(1E20) exp_0_9(1E21) exp_0_9(1E22) exp_0_9(1E23) exp_0_9(1E24)
   exp_0_9(1E25) exp_0_9(1E26) exp_0_9(1E27) exp_0_9(1E28) exp_0_9(1E29)
   exp_0_9(1E30)
};

static const long double nexpo_tab[310] = 
{
   exp_0_9(1E-)   exp_0_9(1E-1)  exp_0_9(1E-2)  exp_0_9(1E-3)  exp_0_9(1E-4)
   exp_0_9(1E-5)  exp_0_9(1E-6)  exp_0_9(1E-7)  exp_0_9(1E-8)  exp_0_9(1E-9)
   exp_0_9(1E-10) exp_0_9(1E-11) exp_0_9(1E-12) exp_0_9(1E-13) exp_0_9(1E-14)
   exp_0_9(1E-15) exp_0_9(1E-16) exp_0_9(1E-17) exp_0_9(1E-18) exp_0_9(1E-19)
   exp_0_9(1E-20) exp_0_9(1E-21) exp_0_9(1E-22) exp_0_9(1E-23) exp_0_9(1E-24)
   exp_0_9(1E-25) exp_0_9(1E-26) exp_0_9(1E-27) exp_0_9(1E-28) exp_0_9(1E-29)
   exp_0_9(1E-30)
};

bool
Tokenizer::tokenize_real(Unicode_source & src, bool & need_float,
                         APL_Float & flt_val, APL_Integer & int_val)
{
   int_val = 0;
   need_float = false;

UTF8_string int_digits;
UTF8_string fract_digits;
UTF8_string expo_digits;
bool negative = false;
bool expo_negative = false;
bool skipped_0 = false;
bool dot_seen = false;

   // hexadecimal ?
   //
   if (src.rest() > 1 && *src == UNI_ASCII_DOLLAR_SIGN)
      {
        src.get();   // skip $
        if (!Avec::is_hex_digit(*src))   return false;   // no hex after $
        while (src.rest())
           {
             int digit;
             if      (*src <  UNI_ASCII_0)   break;
             else if (*src <= UNI_ASCII_9)   digit = src.get() - '0';
             else if (*src <  UNI_ASCII_A)   break;
             else if (*src <= UNI_ASCII_F)   digit = 10 + src.get() - 'A';
             else if (*src <  UNI_ASCII_a)   break;
             else if (*src <= UNI_ASCII_f)   digit = 10 + src.get() - 'a';
             else                            break;
             int_val = int_val << 4 | digit;
           }
        flt_val = int_val;
        return true;   // OK
      }

   // 1. split src into integer, fractional, and exponent parts, removing:
   // 1a. a leading sign of the integer part
   // 1b. leading zeros of the integer part
   // 1c. the . between the integer and fractional parts
   // 1d. trailing zeros of the fractional part
   // 1e. the E between the fractional and exponent parts
   // 1f. a sign of the exponent part
   //
   if (src.rest() && *src == UNI_OVERBAR)   // 1a.
      {
        negative = true;
        ++src;
      }

   // 1b. skip leading zeros in integer part
   //
   while (src.rest() && *src == '0')   { ++src;   skipped_0 = true; }

   // integer part
   //
   while (src.rest() && Avec::is_digit(*src))   int_digits += src.get();

   // fractional part
   //
   if (src.rest() && *src == UNI_ASCII_FULLSTOP)   // fract part present
      {
        ++src;   // 1c. skip '.'
        dot_seen = true;
        while (src.rest() && Avec::is_digit(*src))
           {
             fract_digits += src.get();
           }

        while (fract_digits.size() && fract_digits.back() == '0')   // 1d.
           fract_digits.pop_back();
      }

   // require at least one integer or fractional digit
   //
   if (int_digits.size() == 0    &&
       fract_digits.size() == 0  &&
       !skipped_0)   return false;   // error

   // exponent part (but could also be a name starting with E or e)
   //
   if (src.rest() >= 2 &&   // at least E and a digit or ¯
       (*src == UNI_ASCII_E || *src == UNI_ASCII_e))   // maybe exponent
      {
        expo_negative = (src[1] == UNI_OVERBAR);
        if (expo_negative   &&
            src.rest() >= 3 &&
            Avec::is_digit(src[2]))                    // E¯nnn
           {
             need_float = true;
             ++src;                        // skip e/E
             ++src;                        // skip ¯
             while (src.rest() && Avec::is_digit(*src))
                  expo_digits += src.get();
           }
        else if (Avec::is_digit(src[1]))               // Ennn
           {
             need_float = true;
             ++src;                        // skip e/E
             while (src.rest() && Avec::is_digit(*src))
                  expo_digits += src.get();
           }
      }

   // second dot ?
   if (dot_seen && src.rest() && *src == UNI_ASCII_FULLSTOP)
      return false;   // error

int expo = 0;
   loop(d, expo_digits.size())   expo = 10*expo + (expo_digits[d] - '0');
   if (expo_negative)   expo = - expo;
   expo += int_digits.size();

UTF8_string digits = int_digits;
   digits.append_UTF8(fract_digits);

   // at this point, digits is the fractional part ff... of 0.ff... ×10⋆expo
   // discard leading fractional 0s and adjust expo accordingly
   //
   while (digits.size() && digits[0] == '0')
      {
        digits.erase(0);   // discard '0'
        --expo;
      }

   // at this point, digits is the fractional part ff... of 0.ff... ×10⋆expo
   // from 0.1000... to 0.9999... Discard digits beyond integer precision.
   //
   // The largest 64-bit integer is 0x7FFFFFFFFFFFFFFF = 9223372036854775807
   // which has 19 decimal digits. Discard all after first 20 digits and
   // round according to the last digit.
   //
   if (digits.size() > MAX_TOKENIZE_DIGITS_1)
      digits.resize(MAX_TOKENIZE_DIGITS_1);

   if (digits.size() == MAX_TOKENIZE_DIGITS_1)
      {
        if (digits.round_0_1())   ++expo;
      }

   // special cases: all digits 0 or very small number
   //
   if (digits.size() == 0)   return true;   // OK: all digits were 0
   if (expo <= -307)         return true;   // OK: very small number

   Assert(digits.size() <= MAX_TOKENIZE_DIGITS);

   if (expo > MAX_TOKENIZE_DIGITS)   need_float = true;   /// large integer

   // special case: integer between 9223372036854775807 and 9999999999999999999
   //
   if (expo == MAX_TOKENIZE_DIGITS && digits[0] == '9')
      {
        // a uint64_t compare might overflow, so we compare the digit string
        const char * maxint = "9223372036854775807";
        loop(j, digits.size())
            {
               if (digits[j] < maxint[j])    break;
               if (digits[j] == maxint[j])   continue;
               need_float = true;
               break;
            }
      }

   if (int(digits.size()) > expo)   need_float = true;

   if (need_float)
      {
        if (digits.size() > 17)   digits.resize(17);

       int64_t v = 0;
        loop(j, digits.size())
          {
            v = 10*v + (digits[j] - '0');
            --expo;
          }

        if (expo > 0)
           {
             if (negative)   flt_val = - v * expo_tab[expo];
             else            flt_val =   v * expo_tab[expo];
             return true;   // OK
           }
        else if (expo < 0)
           {
             if (negative)   flt_val = - v * nexpo_tab[-expo];
             else            flt_val =   v * nexpo_tab[-expo];
             return true;   // OK
           }

        if (negative)   flt_val = - v;
        else            flt_val =   v;
        return true;   // OK
      }
   else
      {
        int_val = 0;
        loop(j, digits.size())   int_val = 10*int_val + (digits[j] - '0');
        if (negative)   int_val = - int_val;
        flt_val = int_val;
        return true;   // OK
      }
}
//-----------------------------------------------------------------------------
void
Tokenizer::tokenize_symbol(Unicode_source & src, Token_string & tos)
{
   Log(LOG_tokenize)   CERR << "tokenize_symbol() : " << src.rest() << endl;

UCS_string symbol;
   if (macro)
      {
        symbol.append(UNI_MUE);
        symbol.append(UNI_ASCII_MINUS);
      }
   symbol.append(src.get());

   while (src.rest())
       {
         const Unicode uni = *src;
         if (!Avec::is_symbol_char(uni))   break;
         symbol.append(uni);
         ++src;
       }

   if (symbol.size() > 2 && symbol[1] == UNI_DELTA  &&
       (symbol[0] == UNI_ASCII_S || symbol[0] == UNI_ASCII_T))
      {
        // S∆ or T∆

        while (src.rest() && *src <= UNI_ASCII_SPACE)   src.get();   // spaces
        UCS_string symbol1(symbol, 2, symbol.size() - 2);   // without S∆/T∆
        Value_P AB(symbol1, LOC);
        Function * ST = 0;
        if (symbol[0] == UNI_ASCII_S) ST = Quad_STOP::fun;
        else                          ST = Quad_TRACE::fun;

        const bool assigned = (src.rest() && *src == UNI_LEFT_ARROW);
        if (assigned)   // dyadic: AB ∆fun
           {
             src.get();                                // skip ←
             Log(LOG_tokenize)
                CERR << "Stop/Trace assigned: " << symbol1 << endl;
             tos.push_back(Token(TOK_APL_VALUE1, AB));   // left argument of ST
             tos.push_back(Token(TOK_FUN2, ST));
           }
        else
           {
             Log(LOG_tokenize)
                CERR << "Stop/Trace referenved: " << symbol1 << endl;
             tos.push_back(Token(TOK_FUN2, ST));
             tos.push_back(Token(TOK_APL_VALUE1, AB));   // right argument of ST
           }

        return;
      }

Symbol * sym = Workspace::lookup_symbol(symbol);
   Assert(sym);
   tos.push_back(Token(TOK_SYMBOL, sym));
}
//-----------------------------------------------------------------------------

