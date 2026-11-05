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

#include "Bif_F0_ZILDE.hh"
#include "Bif_F12_COMMA.hh"
#include "Bif_F12_ELEMENT.hh"
#include "Bif_F12_ENCODE_DECODE.hh"
#include "Bif_F12_EQUIV.hh"
#include "Bif_F12_FORMAT.hh"
#include "Bif_F12_INDEX_OF.hh"
#include "Bif_F12_INTERVAL_INDEX.hh"
#include "Bif_F12_PARTITION_PICK.hh"
#include "Bif_F12_RHO.hh"
#include "Bif_F12_ROTATE.hh"
#include "Bif_F12_SORT.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_F12_TRANSPOSE.hh"
#include "Bif_F12_UNION_INTER.hh"
#include "Bif_F1_EXECUTE.hh"
#include "Bif_F2_INDEX.hh"
#include "Bif_F2_LEFT_RIGHT.hh"
#include "Bif_OPER1_COMMUTE.hh"
#include "Bif_F12_DOMINO.hh"
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
#include "Token_string.hh"
#include "Value.hh"
#include "StateIndicator.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
inline ostream & operator << (ostream & out, const Unicode_source & src)
   { loop(s, src.rest_len())   out << src[s];   return out; }
//════════════════════════════════════════════════════════════════════════════
/** convert \b UCS_string input into a Token_string tos.
*/
//════════════════════════════════════════════════════════════════════════════
void
Tokenizer::tokenize(const UCS_string & input, Token_string & tos) const
{
size_t rest_2 = 0;
   do_tokenize(input, tos, rest_2);
   // any lexical failure throws (see throw_parse_error() call sites in
   // do_tokenize()/tokenize_string1()/tokenize_string2()/tokenize_number(),
   // each of which sets error_message_2/left_caret/right_caret to a
   // precise per-lexeme range before throwing) -- there is no ErrorCode
   // to reduce that Error to here. Reducing it used to be exactly the
   // "double indirection" bug documented in devel_doc/Carets.txt: the
   // rich Error this function used to build was thrown away in favour of
   // a bare code, and every caller up the chain had no choice but to
   // reconstruct an empty one. Let it propagate instead.
}
//────────────────────────────────────────────────────────────────────────────
Token
Tokenizer::tokenize_function(Unicode uni)
{
const Token tok = Avec::uni_to_token(uni, LOC);

#define sys(t, f) \
   case TOK_ ## t: return Token(tok.get_tag(), &Bif_ ## f::fun);   break;

   switch(tok.get_tag())
      {
        case TOK_F0_ZILDE:
             // new ⍬ style: tokenize ⍬ as constant
             //
             return Token(TOK_APL_VALUE1, Idx0(LOC));

             // old ⍬ style: tokenize ⍬ as niladic function
             //
             // return Token(tok.get_tag(), &Bif_F0_ZILDE::fun);  break;

        sys(F1_EXECUTE,    F1_EXECUTE)

        sys(F2_AND,             F2_AND)
        sys(F2_EQUAL,           F2_EQUAL)
        sys(F2_FIND,            F2_FIND)
        sys(F2_GREATER,         F2_GREATER)
        sys(F2_INDEX,           F2_INDEX)
        sys(F2_LESS,            F2_LESS)
        sys(F2_LEQU,            F2_LEQU)
        sys(F2_MEQU,            F2_MEQU)
        sys(F2_NAND,            F2_NAND)
        sys(F2_NOR,             F2_NOR)
        sys(F2_OR,              F2_OR)
        sys(F2_UNEQU,           F2_UNEQU)

        sys(F12_BINOM,          F12_BINOM)
        sys(F12_CIRCLE,         F12_CIRCLE)
        sys(F12_COMMA,          F12_COMMA)
        sys(F12_COMMA1,         F12_COMMA1)
        sys(F12_DECODE,         F12_DECODE)
        sys(F12_DIVIDE,         F12_DIVIDE)
        sys(F12_DOMINO,         F12_DOMINO)
        sys(F12_DROP,           F12_DROP)
        sys(F12_ELEMENT,        F12_ELEMENT)
        sys(F12_ENCODE,         F12_ENCODE)
        sys(F12_EQUIV,          F12_EQUIV)
        sys(F12_FORMAT,         F12_FORMAT)
        sys(F12_INDEX_OF,       F12_INDEX_OF)
        sys(F12_INTERVAL_INDEX, F12_INTERVAL_INDEX)
        sys(F2_INTER,           F2_INTER)
        sys(F2_LEFT,            F2_LEFT)
        sys(F12_LOGA,           F12_LOGA)
        sys(F12_MINUS,          F12_MINUS)
        sys(F12_NEQUIV,         F12_NEQUIV)
        sys(F12_PARTITION,      F12_PARTITION)
        sys(F12_PICK,           F12_PICK)
        sys(F12_PLUS,           F12_PLUS)
        sys(F12_POWER,          F12_POWER)
        sys(F12_RHO,            F12_RHO)
        sys(F2_RIGHT,           F2_RIGHT)
        sys(F12_RND_DN,         F12_RND_DN)
        sys(F12_RND_UP,         F12_RND_UP)
        sys(F12_ROLL,           F12_ROLL)
        sys(F12_ROTATE,         F12_ROTATE)
        sys(F12_ROTATE1,        F12_ROTATE1)
        sys(F12_SORT_ASC,       F12_SORT_ASC)
        sys(F12_SORT_DES,       F12_SORT_DES)
        sys(F12_STILE,          F12_STILE)
        sys(F12_TAKE,           F12_TAKE)
        sys(F12_TRANSPOSE,      F12_TRANSPOSE)
        sys(F12_TIMES,          F12_TIMES)
        sys(F12_UNION,          F12_UNION)
        sys(F12_WITHOUT,        F12_WITHOUT)

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
//════════════════════════════════════════════════════════════════════════════

Tokenizer::Int_or_Double
Tokenizer::tokenize_real(Unicode_source & src)
{
   // hexadecimal ?
   //
   // also for a $ at the end of the line, which then consumes the $ and
   // fails like a $ followed by a non-hex character: otherwise nothing
   // was consumed and "Bad number" was reported without the statement
   // and carets (Blake McBride, Bugs32 #31)
   //
   if (src.rest_len() >= 1 && *src == UNI_DOLLAR_SIGN)
      return tokenize_hex(src);

enum { MAX_TOKENIZE_DIGITS = 19 };   // == atrlen("9223372036854775807")

   // hard cap on how many digit characters we accumulate per part. This
   // is far more than any double could ever use (usable magnitude and
   // precision top out around MAX_TOKENIZE_DIGITS), so it does not
   // affect any real numeric literal -- it only stops a pathological
   // literal with millions of digits from growing the ALLOCA() buffer
   // below without bound: a plain digit run had no length cap before
   // reaching that ALLOCA(), so it sized a stack allocation proportional
   // to the (attacker/paste-controlled) literal length (confirmed stack
   // overflow via inspection).
   //
enum { MAX_ACCUM_DIGITS = 1024 };

UTF8_string int_digits;     // the digits left of . (if any)
UTF8_string fract_digits;   // the digits right of . (if any)
UTF8_string expo_digits;    // the digits  right of E (if any)
bool need_float = false;
bool mant_negative = false;   // mantissa is negative
bool expo_negative = false;   // exponent is negative
bool skipped_0 = false;     // some leading 0 in the integer part were skipped
bool dot_seen = false;      // the decimal . was seen


   /* 1. split src into integer, fractional, and exponent parts,
         thereby removing (skipping):

      1a. a leading sign of the integer part,         (sets mant_negative)
      1b. leading zeros of the integer part,
      1c. the . between the integer and fractional parts,
      1d. trailing zeros of the fractional part,
      1e. the E between the fractional and exponent parts, and/or
      1f. a sign of the exponent part                 (sets expo_negative)
    */
   if (src.has_more() && *src == UNI_OVERBAR)   // 1a.
      {
        mant_negative = true;
        ++src;
      }

   // 1b. discard leading zeros in the integer part
   //
   while (src.skip_if(UNI_0))   { skipped_0 = true; }

   // integer part
   //
size_t skipped_int_digits = 0;
   while (src.has_more() && Avec::is_digit(*src))
      {
        const Unicode digit = src.get();
        if (int_digits.size() < MAX_ACCUM_DIGITS)   int_digits += digit;
        else                                        ++skipped_int_digits;
      }

   // integer-part digits beyond MAX_ACCUM_DIGITS were dropped entirely
   // above -- since APL numeric literals are positional, that silently
   // divides the value by 10 per dropped digit (a magnitude error, not
   // just a precision loss: "123..." (1030 digits) parsed as if the
   // last 6 digits had never been typed, not as a rounded approximation
   // of the actual 1030-digit value). Pad back with zeros instead: this
   // still loses precision beyond MAX_ACCUM_DIGITS significant digits
   // (unavoidable, and irrelevant at this magnitude), but keeps the
   // decimal point -- and therefore the magnitude -- where it belongs.
   loop(z, skipped_int_digits)   int_digits += UNI_0;

   // fractional part...
   //
   if (src.skip_if(UNI_FULLSTOP))   // fract part present
      {
        dot_seen = true;
        while (src.has_more() && Avec::is_digit(*src))
           {
             const Unicode digit = src.get();
             if (fract_digits.size() < MAX_ACCUM_DIGITS)   fract_digits += digit;
           }

        while (fract_digits.size() && fract_digits.back() == UNI_0)   // 1d.
           {
             fract_digits.pop_back();
             skipped_0 = true;
           }
      }

   // APL syntax requires at least one (integer or fractional) digit
   //
   if (int_digits.size()   == 0  &&   // empty integer part, and
       fract_digits.size() == 0)      // empty fractional part
      {
        /* the mantisssa is empty. There are two possibilities:

           1. input was correct (e.g. 0. ), but the 0 was skipped above.;
           2. syntax error by the user ( . without digits)
         */
        if (skipped_0)                  // case 1
           {
             // a zero mantissa can still have an exponent (0E0, 0.0E¯5,
             // as displayed for a 0 in a scaled column): consume it too.
             // Returning before it left E0 behind as a name, so that
             // ⍎'0E0' was a VALUE ERROR (Bugs34 #8).
             //
             if (src.rest_len() >= 2 && (*src == UNI_E || *src == UNI_e))
                {
                  int skip = 0;   // length of E or E¯ before the digits
                  if (Avec::is_digit(src[1]))   skip = 1;
                  else if (src[1] == UNI_OVERBAR && src.rest_len() >= 3 &&
                           Avec::is_digit(src[2]))   skip = 2;
                  if (skip)
                     {
                       loop(s, skip)   ++src;
                       while (src.has_more() && Avec::is_digit(*src))  ++src;
                     }
                }

             // an explicit decimal point (0., .0, 0.00, ...) is the
             // user's own choice of storage class, kept even though the
             // value happens to be 0 -- see the dot_seen note below.
             if (dot_seen)   return Int_or_Double(APL_Float(0.0));
             return Int_or_Double(APL_Integer(0));
           }

        MORE_ERROR() << "expecting 0. or .0 but not . without digits.";
        return Int_or_Double();                     // case 2: syntax error
      }

   // exponent part (but could also be a name starting with E or e)
   //
   if (src.rest_len() >= 2 &&              // at least E and a digit or ¯
       (*src == UNI_E || *src == UNI_e))   // and maybe exponent
      {
        expo_negative = (src[1] == UNI_OVERBAR);
        if (expo_negative       &&
            src.rest_len() >= 3 &&
            Avec::is_digit(src[2]))                    // E¯nnn
           {
             need_float = true;
             ++src;                        // skip e/E
             ++src;                        // skip ¯
             while (src.has_more() && Avec::is_digit(*src))
                {
                  const Unicode digit = src.get();
                  if (expo_digits.size() < MAX_ACCUM_DIGITS)   expo_digits += digit;
                }
           }
        else if (Avec::is_digit(src[1]))               // Ennn
           {
             need_float = true;
             ++src;                        // skip e/E
             while (src.has_more() && Avec::is_digit(*src))
                {
                  const Unicode digit = src.get();
                  if (expo_digits.size() < MAX_ACCUM_DIGITS)   expo_digits += digit;
                }
           }
      }

   // second dot (which is a syntax error)?
   //
   if (dot_seen && src.has_more() && *src == UNI_FULLSTOP)
      {
        MORE_ERROR() << "Two . in a number.";
        return Int_or_Double();
      }

   // set expo to the optional Ennn (if any) in iii.fff.Ennn)
   //
   Log(LOG_tokenize)
      {
        Q1(int_digits.size());
        Q1(int_digits);
        Q1(fract_digits);
        Q1(expo_digits);
      }

   // resolve very big mantissas (including any carry from rounding up
   // int_digits[]) *before* constructing the C string below, so that the
   // buffer is filled from the final digits rather than the pre-rounding
   // ones.
   //
   if (int_digits.size() > MAX_TOKENIZE_DIGITS)   // very big mantissa
      {
        need_float = true;
        fract_digits.clear();   // ignore fract digits
      }
   else if (int_digits.size() == MAX_TOKENIZE_DIGITS &&
            int_digits[0] == UNI_9 &&
            int_digits[1] >= UNI_2)   // 9200000000000000000 or more
      {
        // the max. APL integer is 0x7FFFFFFFFFFFFFFF aka. 9223372036854775807
        // with 19 deciman digits. int_digits has 19  and starts with 92. The
        // 2 in 92 somewhat simplifies rounding up of aby fractional digits.
        // It thus may or may not fit into an APL integer.
        // This happens rarely so we can afford some extra effort as to figure
        // precisely whether int_digits is real or integer).
        //
        if (fract_digits.size() && fract_digits[0] >= UNI_5)   // round up
           {
             for (int d = int_digits.size();;)
                 {
                   if (d == 0)   // all digits were 9 (10^19 overflow → float)
                      {
                        // by this point every digit has already been
                        // rewritten '9'->'0' (see the loop body below) --
                        // int_digits is now e.g. "0000000000000000000"
                        // instead of the correctly-rounded-up
                        // "10000000000000000000". Prepend the carried-out
                        // leading 1 so the buffer built below actually
                        // represents the rounded value instead of 0.
                        // Reproduced: 9999999999999999999.5 gave 0 (want
                        // 1E19).
                        int_digits.insert(int_digits.begin(), UNI_1);
                        need_float = true;
                        break;
                      }
                   const UTF8 digit = int_digits[--d];
                   if (digit == UNI_9)   // propagate carry
                      {
                        int_digits[d] = UNI_0;       // 9 → 0 and continue
                      }
                   else
                      {
                        int_digits[d] = digit + 1;   // ++N and stop
                        break;
                      }
                 }
           }
        fract_digits.clear();

        const UTF8_string max_int("9223372036854775807");
        loop(j, MAX_TOKENIZE_DIGITS)
            {
              if (int_digits[j] < max_int[j])   break;   // small (int is OK).
              if (int_digits[j] > max_int[j])            // float needed
                 {
                   need_float = true;
                   // float precisionm is lower than integer precision, we can
                   // therefore discard any fractional digits.
                   fract_digits.clear();   // ignore fract digits
                   break;
                 }
            }
      }

   // an explicit decimal point is the user's own choice between integer
   // and real storage (LanguageVariances.md #1/V1) -- keep it real even
   // when 1d. above stripped every fractional digit away as trailing
   // zeros (e.g. 456789.0): demoting such a literal to integer storage
   // gains precision (an int64 mantissa is wider than a double's) but
   // silently overrides the user's own explicit choice and defeats
   // small-⎕PP scaled display for values the user deliberately wrote as
   // real. A value that ends up demoted anyway -- e.g. via later
   // arithmetic on a real result -- is a separate, implicit decision
   // and not affected by this: this only concerns how a *literal* is
   // tokenized.
   //
   if (dot_seen)   need_float = true;

   // construct a C string according to the APL string
   //
char * buffer = ALLOCA(char, int_digits.size() +
                             fract_digits.size() +
                             expo_digits.size() + 20);
char * b = buffer;
   if (mant_negative)   *b++ = '-';
   loop(i, int_digits.size())   *b++ = int_digits[i];

   if (fract_digits.size())
      {
         need_float = true;
         *b++ = '.';
         loop(f, fract_digits.size())   *b++ = fract_digits[f];
      }

   if (expo_digits.size())
      {
         need_float = true;
         *b++ = 'e';
         if (expo_negative)   *b++ = '-';
         loop(e, expo_digits.size())   *b++ = expo_digits[e];
      }
   *b = 0;

   if (need_float)
      {
        const APL_Float dval = APL_Float(strtod(buffer, 0));
        if (!isfinite(dval))   // overflow: ±HUGE_VAL
           {
             // underflow (to 0, or to a subnormal) is benign and not
             // reported -- glibc's strtod() sets errno==ERANGE for those
             // too, not just for true overflow, so the (better than
             // errno) test here is isfinite() on the actual result.
             // Overflow is not benign, since every arithmetic primitive
             // refuses to produce (or consume) a non-finite value --
             // letting the tokenizer hand out ∞/¯∞ as an ordinary literal
             // only postpones the same DOMAIN ERROR to some later, more
             // confusing point.
             MORE_ERROR() << "Numeric literal '" << buffer
                          << "' is out of range for a real number.";
             return Int_or_Double();
           }
        return Int_or_Double(dval);
      }
   else
      {
        // keep the errno of the user's last ⎕FIO call (⎕FIO[1], Bugs37 #54)
        const int saved_errno = errno;
        errno = 0;
        const long long  result = strtoll(buffer, 0, 10);   // may set errno
        const bool failed = errno != 0;
        errno = saved_errno;
        if (failed)   // strtoll() failed (int_digits too large)
           {
             const APL_Float dval = APL_Float(strtod(buffer, 0));
             if (!isfinite(dval))
                {
                  MORE_ERROR() << "Numeric literal '" << buffer
                               << "' is out of range for a real number.";
                  return Int_or_Double();
                }
             return Int_or_Double(dval);
           }
        else
           return Int_or_Double(APL_Integer(result));
      }
}
//────────────────────────────────────────────────────────────────────────────
/** convert \b UCS_string input into a Token_string tos. The tokenization
    stops at the end of input (= end of line), or when a comment (⍝ or #)
    is detected.
*/
void
Tokenizer::do_tokenize(const UCS_string & input, Token_string & tos,
                       size_t & rest_2) const
{
   Log(LOG_tokenize)
      CERR << "tokenize: input[" << input.size() << "] is: «"
           << input << "»" << endl;

Unicode_source src(input);

   for(;;)
      {
        rest_2 = src.rest_len();
        if (rest_2 == 0)   break;   // end of input

        Unicode uni = *src;
        if (uni == UNI_COMMENT)       break;   // ⍝ comment: discared the rest
        if (uni == UNI_NUMBER_SIGN)   break;   // # comment: discared the rest

        const Token tok = Avec::uni_to_token(uni, LOC);   // may normalize uni

        Log(LOG_tokenize)
           {
             Unicode_source s1(src, 0, 24);
             CERR << "  tokenize(" <<  src.rest_len() << " chars) sees [tag "
                  << tok.tag_name() << " «" << uni << "»] " << s1;
             if (src.rest_len() != s1.rest_len())   CERR << " ...";
             CERR << endl;
           }

        switch(tok.get_Class())
            {
              case TC_END:   // chars without APL meaning
                   if (tok.get_tag() == TOK_DIAMOND)
                       {
                         ++src;
                         tos.emplace_back(tok, Function_PC(tos.size()));
                         break;
                       }

                   // UNI_MUE (μ) is Avec.def Token END: outside macro mode
                   // it has no dispatch of its own and falls through to the
                   // E_NO_TOKEN error below like any other TC_END char (it
                   // is NOT independently usable as an ordinary identifier
                   // despite carrying FLG_SYMBOL -- that flag only governs
                   // is_symbol_char()'s CONTINUATION checks inside
                   // tokenize_symbol(), once already scanning a symbol; it
                   // is never consulted for what starts one). In macro
                   // mode, a leading μ starts one of the positional
                   // "μ¯N"/"μN" names written directly into Macro.def's
                   // own source text, so dispatch into the same symbol
                   // scanner used for any other identifier.
                   //
                   if (macro && uni == UNI_MUE)
                      {
                        tokenize_symbol(src, tos);
                        break;
                      }

                   rest_2 = src.rest_len();
                   {
                     Log(LOG_error_throw)
                        {
                          CERR << endl << "throwing "
                               << Error::error_name(E_NO_TOKEN)
                               << " in  Tokenizer" << endl;
                        }

                     char cc[20];
                     SPRINTF(cc, "U+%4.4X (", uni);
                     MORE_ERROR() << "Tokenizer: No token for Unicode "
                                  <<  cc << uni << ")\nInput was: " << input;
                     const int char_start = src.get_pos();
                     Error::throw_parse_error(E_NO_TOKEN, input, char_start,
                                              char_start + 1, LOC, loc);
                   }
                   break;

              case TC_RETURN:
              case TC_LINE:
              case TC_VALUE:
              case TC_INDEX:
                   if (uni == UNI_AT_SIGN)   // marker
                      {
                        const int marker_start = src.get_pos();
                        ++src;
                        int64_t idx = 0;
                        while (src.has_more() && Avec::is_digit(*src))
                              {
                                const int64_t digit = src.get() - '0';
                                // detect overflow *before* it happens
                                // (signed overflow is UB): an arbitrarily
                                // long digit run (e.g. @999...9@) would
                                // otherwise overflow idx.
                                if (idx > (INT64_MAX - digit) / 10)
                                   {
                                     MORE_ERROR() << "marker index too large.";
                                     Error::throw_parse_error(E_SYNTAX_ERROR,
                                                              input,
                                                              marker_start,
                                                              src.get_pos(),
                                                              LOC, loc);
                                   }
                                idx = 10 * idx + digit;
                              }
                        if (!src.has_more() || src.get() != UNI_AT_SIGN)
                           {
                             // this may occur if a user types e.g. @123
                             MORE_ERROR() << "No second '@' in marker.";
                             Error::throw_parse_error(E_SYNTAX_ERROR, input,
                                                      marker_start,
                                                      src.get_pos(), LOC, loc);
                           }
                        tos.emplace_back(Token(TOK_MARKER, idx), Function_PC(tos.size()));
                        break;
                      }

                   // this and the TC_VOID case below used to print
                   // unconditionally, unlike every sibling diagnostic in
                   // this switch (see the TC_END case above, which is
                   // properly Log()-gated) -- so e.g. AllPrimitives.tc's
                   // fuzz row for the execute primitive (rval.def's
                   // "Z<-EXEC B", which deliberately generates B as an
                   // unrestricted random character array to stress-test
                   // execute's error handling) leaked this to the error
                   // stream on every draw containing a character outside
                   // the interpreter's own character set, even though the
                   // resulting E_NON_APL_CHAR error is perfectly normal
                   // and gets caught correctly by the caller.
                   //
                   Log(LOG_error_throw)
                      {
                        CERR << "Offending token: " << tok.get_tag()
                             << " (" << tok << ")" << endl;
                        if (tok.get_tag() == TOK_CHARACTER)
                           CERR << "Unicode: " << UNI(tok.get_char_val())
                                << endl;
                      }
                   rest_2 = src.rest_len();
                   Error::throw_parse_error(E_NON_APL_CHAR, input,
                                            src.get_pos(), src.get_pos() + 1,
                                            LOC, loc);
                   break;

              case TC_VOID:
                   // Avec::uni_to_token returns TC_VOID for non-apl characters
                   //
                   rest_2 = src.rest_len();
                   Log(LOG_error_throw)
                      UERR << "Unknown APL character: " << uni
                           << " (" << UNI(uni) << ")" << endl;
                   Error::throw_parse_error(E_NON_APL_CHAR, input,
                                            src.get_pos(), src.get_pos() + 1,
                                            LOC, loc);
                   break;

              case TC_SYMBOL:
                   if (Avec::is_quad(uni))
                      {
                         const int start = src.get_pos();
                         if (tokenize_quad(src, tos))   // unknown name
                            Error::throw_parse_error(E_SYNTAX_ERROR, input,
                                                     start, src.get_pos(),
                                                     LOC, loc);
                      }
                   else if (uni == UNI_QUOTE_Quad)
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_Quad_QUOTE,
                                         &Workspace::get_v_Quad_QUOTE()), Function_PC(tos.size()));
                      }
                   else if (uni == UNI_ALPHA)   // ⍺
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_ALPHA,
                                         &Workspace::get_v_ALPHA()), Function_PC(tos.size()));
                      }
                   else if (uni == UNI_ALPHA_UNDERBAR)   // ⍶
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_ALPHA_U,
                                            &Workspace::get_v_ALPHA_U()), Function_PC(tos.size()));
                      }
                   else if (uni == UNI_CHI)   // χ
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_CHI,
                                            &Workspace::get_v_CHI()), Function_PC(tos.size()));
                      }
                   else if (uni == UNI_LAMBDA)   // λ
                      {
                        // this could be λ like in λ← ...
                        // or λ1 or λ2 or ... as in ... ⍺ λ1 ⍵
                        //
                        if (src.rest_len() > 1 &&
                            Avec::is_digit(src[1]))   // λn
                           {
                             tokenize_symbol(src, tos);
                           }
                        else   // λ
                           {
                             ++src;
                             tos.emplace_back(Token(TOK_LAMBDA,
                                           &Workspace::get_v_LAMBDA()), Function_PC(tos.size()));
                           }
                      }
                   else if (uni == UNI_OMEGA)   // ⍵
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_OMEGA,
                                            &Workspace::get_v_OMEGA()), Function_PC(tos.size()));
                      }
                   else if (uni == UNI_OMEGA_UNDERBAR)   // ⍹
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_OMEGA_U,
                                            &Workspace::get_v_OMEGA_U()), Function_PC(tos.size()));
                      }
                   else   // A-Z, a-z
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
                   if (tok.get_tag() == TOK_OPER2_INNER && src.rest_len())
                      {
                        /* tok is a dot. This could mean that . is either
                          
                           case 1:   the start of a number:     e.g. +.3
                           case 2:   or an operator:            e.g. +.*
                           case 3:   or a syntax error:         e.g. Done.
                         */
                        const TokenClass prev = tos.size()
                                              ? tos.back().get_Class()
                                              : TC_INVALID;
                        if (src.rest_len() == 1 &&   // . at the end and
                            (prev == TC_FUN12   ||   // after a function, e.g.
                             prev == TC_SYMBOL  ||   // 5∘. or +. or G2. or
                             prev == TC_R_CURLY ||   // {⍵}. or (+/).
                             prev == TC_R_PARENT))
                           {
                             // case 2: an operator without its right
                             // operand. Let the parser report that (with
                             // the entire statement in line 2), Bugs36 F3,
                             // also for a defined function or lambda,
                             // Bugs37 #63.
                             //
                             tokenize_function(src, tos);
                           }
                        else if (src.rest_len() == 1)   // case 3: syntax error
                           {
                             // e.g. 1 2 3 . : the entire statement in line
                             // 2 and the caret under the . (Bugs37 #63)
                             //
                             MORE_ERROR() << "no digit before or after '.'.";
                             Error error(E_SYNTAX_ERROR, LOC);
                             UCS_string line_2(Workspace::get_prompt());
                             const int dot = line_2.size() + src.get_pos();
                             line_2 << input;
                             error.set_error_line_2(line_2, dot, dot);
                             if (StateIndicator * si = Workspace::SI_top())
                                StateIndicator::get_error(si) = error;
                             throw error;
                           }
                        else
                           {
                             Unicode uni_1 = src[1];
                             const Token tok_1 = Avec::uni_to_token(uni_1,
                                                                    LOC);
                             if ((int(tok_1.get_tag()) & int(TC_MASK)) ==
                                 TC_NUMERIC)
                                tokenize_number(src, tos, rest_2);
                             else
                                tokenize_function(src, tos);
                           }
                      }
                   else
                      {
                        tokenize_function(src, tos);
                      }
                   if (tos.size() >= 2 &&
                       tos.back().get_tag() == TOK_OPER2_INNER &&
                       tos[tos.size() - 2].get_tag() == TOK_JOT)
                      {
                        // ∘ . : the . becomes ∘. (and keeps its PC)
                        const Token_loc outer(Token(TOK_OPER2_OUTER,
                                                    &Bif_OPER2_OUTER::fun),
                                              tos.back().get_PC());
                        tos.back().copy(outer, LOC);
                      }

                   break;

              case TC_R_ARROW:   // →
                   ++src;        // skip →
                   if (src.rest_len() == 0)                      // single →
                      {
                        tos.emplace_back(Token(TOK_ESCAPE), Function_PC(tos.size()));
                      }
                   else if (rest_2 &&
                            *src == UNI_RIGHT_ARROW)   // double → (→→)
                      {
                        ++src;
                        tos.emplace_back(Token(TOK_IF_THEN, int64_t(0)), Function_PC(tos.size()));
                      }
                   else
                      {
                        tos.emplace_back(tok, Function_PC(tos.size()));
                      }
                   break;

              case TC_ASSIGN:    // ←
                   ++src;        // skip ←
                   if (rest_2 > 1 && *src == UNI_LEFT_ARROW)         // ←←
                      {
                        ++src;   // skip second ←
                        tos.emplace_back(Token(TOK_IF_END, int64_t(0)), Function_PC(tos.size()));
                      }
                   else if (rest_2 > 1 && *src == UNI_RIGHT_ARROW)   // →→
                      {
                        ++src;   // skip second →
                        tos.emplace_back(Token(TOK_IF_ELSE, int64_t(0)), Function_PC(tos.size()));
                      }
                   else
                      {
                        const bool sym = tos.size() >= 1 &&
                                   tos[tos.size() - 1].get_tag() == TOK_SYMBOL;
                        const bool dia = tos.size() > 1 &&
                                   tos[tos.size() - 2].get_tag() == TOK_DIAMOND;
                        const bool col = tos.size() > 1 &&
                                   tos[tos.size() - 2].get_tag() == TOK_COLON;
                     
                      /* change tos.get_tag() from  TOK_R_ARROW to TOK_ASSIGN1
                         in the following 3 cases:

                         1.      SYM ←   (at the start of line),        or
                         2.    ◊ SYM ←   (at the start of statement),   or
                         3.    : SYM ←   (at the start of statement after label)

                         or else leave it as is (.
                       */
                      if (sym && ((tos.size() == 1) ||   // case 1.
                          dia                       ||   // case 2.
                          col))                          // case 3.
                         {
                           tos.emplace_back(TOK_ASSIGN1, Function_PC(tos.size()));
                         }
                      else
                         {
                           tos.emplace_back(tok, Function_PC(tos.size()));
                         }
                      }
                   break;

              case TC_L_PARENT:
              case TC_R_PARENT:
              case TC_L_BRACK:
              case TC_R_BRACK:
              case TC_L_CURLY:
              case TC_R_CURLY:
                   ++src;
                   tos.emplace_back(tok, Function_PC(tos.size()));
                   break;

              case TC_COLON:
                   if (pmode != PM_FUNCTION)
                      {
                        rest_2 = src.rest_len();
                        if (pmode == PM_EXECUTE)
                           Error::throw_parse_error(E_ILLEGAL_COLON_EXEC,
                                                    input, src.get_pos(),
                                                    src.get_pos() + 1,
                                                    LOC, loc);
                        else
                           Error::throw_parse_error(E_ILLEGAL_COLON_STAT,
                                                    input, src.get_pos(),
                                                    src.get_pos() + 1,
                                                    LOC, loc);
                      }

                   ++src;
                   tos.emplace_back(tok, Function_PC(tos.size()));
                   break;

              case TC_NUMERIC:
                   tokenize_number(src, tos, rest_2);
                   break;

              case TC_SPACE:
              case TC_NEWLINE:
                   ++src;
                   break;

              case TC_QUOTE:   // ' or " or « or »
                   if (tok.get_tag() == TOK_QUOTE1)
                      tokenize_string1(src, tos, rest_2);
                   else
                      tokenize_string2(src, tos, rest_2);
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
      {
        CERR << "tokenize() done (no error)." << endl
             << "   └── tos[" << tos.size() << "] is:";
        loop(t, tos.size())   CERR << " " << tos[t];
        CERR << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Tokenizer::tokenize_function(Unicode_source & src, Token_string & tos) const
{
   Log(LOG_tokenize)   CERR << "tokenize_function(" << src << ")" << endl;

const Unicode uni = src.get();
const Token tok = tokenize_function(uni);
   tos.emplace_back(tok, Function_PC(tos.size()));
}
//────────────────────────────────────────────────────────────────────────────
bool
Tokenizer::tokenize_quad(Unicode_source & src, Token_string & tos) const
{
   Log(LOG_tokenize)
      CERR << "tokenize_quad(" << src.rest_len() << " chars)"<< endl;

   src.get();               // discard (possibly alternative) ⎕
UCS_string ucs(UNI_Quad_Quad);
   Assert(ucs[0]);

   // ⎕ and all name characters after it (see Workspace::get_quad()),
   // except digits: no system name contains one, and ⎕UCS20 is ⎕UCS 20
   //
   for (int j = 0; j < src.rest_len() && Avec::is_symbol_char(src[j]) &&
                   !Avec::is_digit(src[j]); ++j)
       ucs << src[j];

int len = 0;
const Token t = Workspace::get_quad(ucs, len);
   if (len != ucs.ssize())   // unknown system name
      {
        MORE_ERROR() << "unknown system name " << ucs;
        src.skip(ucs.size() - 1);
        return true;
      }

   src.skip(len - 1);
   tos.emplace_back(t, Function_PC(tos.size()));
   return false;
}
//════════════════════════════════════════════════════════════════════════════
/** tokenize a single quoted string.
 ** If the string is a single character, then we
 **  return a TOK_CHARACTER. Otherwise we return TOK_APL_VALUE1.
 **/
void
Tokenizer::tokenize_string1(Unicode_source & src, Token_string & tos,
                            size_t & rest_2) const
{
   Log(LOG_tokenize)   CERR << "tokenize_string1(" << src << ")" << endl;

const int start_pos = src.get_pos();
const Unicode uni = src.get();
   Assert(Avec::is_single_quote(uni));

UCS_string string_value;
bool got_end = false;

   while (src.has_more())
       {
         const Unicode uni = src.get();

         if (Avec::is_single_quote(uni))
            {
              // a single ' is the end of the string, while a double '
              // (i.e. '') is a single '. 
              //
              if ((src.rest_len() == 0) || !Avec::is_single_quote(*src))
                 {
                   got_end = true;
                   break;
                 }

              string_value << UNI_SINGLE_QUOTE;
              ++src;      // skip the second '
            }
         else if (uni == UNI_CR &&   // CR of a CRLF line end: ignore it
                  (src.rest_len() == 0 || *src == UNI_LF))
            {
              // but keep a CR inside the literal (Bugs35 #71)
              continue;
            }
         else if (uni == UNI_LF)
            {
              rest_2 = src.rest_len();
              Error::throw_parse_error(E_NO_STRING_END, src.data(),
                                       start_pos, src.get_pos(), LOC, loc);
            }
         else
            {
              string_value << uni;
            }
       }

   // an unterminated '...' string is always an error. Old-style multi-line
   // strings ("..." only) were already joined into single lines by
   // Parser::transform_old_multi_line_strings() before tokenization
   // (Bugs28 #100(i), Bugs35 #55).
   //
   if (!got_end)
      {
        const int pos = start_pos + Workspace::get_IO();
        MORE_ERROR() << "Standard APL string (i.e. '...'): start at "
                        "column (prompt+" << pos << "), but no end.";
        Error::throw_parse_error(E_NO_STRING_END, src.data(), start_pos,
                                 src.get_pos(), LOC, loc);
      }

   if (string_value.size() == 1)   // scalar
      {
        tos.emplace_back(Token(TOK_CHARACTER, string_value[0]), Function_PC(tos.size()));
      }
   else
      {
        tos.emplace_back(Token(TOK_APL_VALUE1, Value_P(string_value, LOC)), Function_PC(tos.size()));
      }
}
//────────────────────────────────────────────────────────────────────────────
/** tokenize a double quoted string, i.e.  "..." or «...».
 ** Unlike '...' strings (which may be character scalars or character vectors,
    are "..." and «...» strings always character vectors.
 **
 ** for special cases ««« and »»» do nothing.
 **/
void
Tokenizer::tokenize_string2(Unicode_source & src, Token_string & tos,
                            size_t & rest_2) const
{
   Log(LOG_tokenize)   CERR << "tokenize_string2(" << src << ")" << endl;

const int start_pos = src.get_pos();

   // remember the leading ", «. or »
   //
const Unicode first = src.get();
Unicode last = Invalid_Unicode;   // no last
   if (first == UNI_DOUBLE_QUOTE)
      {
        last = UNI_DOUBLE_QUOTE;
      }
   else if (first == UNI_LEFT_DAQ)
      {
        last = UNI_RIGHT_DAQ;
      }
   else if (first == UNI_RIGHT_DAQ)
      {
        if (src.rest_len() >= 2     &&
            src[0] == UNI_RIGHT_DAQ &&
            src[1] == UNI_RIGHT_DAQ)   // special case: »»»
           {
             ++src;   ++src;   // discard second and third »
             return;
           }
      }
   else    // internal error
      {
        FIXME;
      }

UCS_string string_value;
bool got_end = false;

   while (src.has_more())
       {
         const Unicode uni = src.get();

         if (uni == last)
            {
              got_end = true;
              break;
            }

         if (uni == UNI_CR &&        // ignore the CR of a CRLF line end,
             (src.rest_len() == 0 || *src == UNI_LF))   // (Bugs35 #71)
            {
              continue;   // while (src.has_more())
            }

         if (uni == UNI_LF)          // end of line before " or »
            {
              rest_2 = src.rest_len();
              Error::throw_parse_error(E_NO_STRING_END, src.data(),
                                       start_pos, src.get_pos(), LOC, loc);
            }
         else if (uni == UNI_BACKSLASH)   // backslash
            {
              // A backslash as the very last character before the source
              // is exhausted (no closing quote at all) must not fall
              // through to src.get(): that only self-protects with
              // Assert(), a no-op at the default (production) assert
              // level, so an untrusted trailing backslash would read
              // one past the end of the underlying UCS_string.
              if (!src.has_more())
                 Error::throw_parse_error(E_NO_STRING_END, src.data(),
                                          start_pos, src.get_pos(), LOC, loc);

              const Unicode uni1 = src.get();
              switch(uni1)
                 {
                   case UNI_0:
                   case UNI_L_BRACK:
                   case UNI_DOUBLE_QUOTE:
                   case UNI_LEFT_DAQ:
                   case UNI_RIGHT_DAQ:
                   case UNI_BACKSLASH: string_value << uni1;      break;

                   case UNI_a:  string_value << UNI_BEL;            break;
                   case UNI_b:  string_value << UNI_BS;             break;
                   case UNI_t:  string_value << UNI_HT;             break;
                   case UNI_n:  string_value << UNI_LF;             break;
                   case UNI_v:  string_value << UNI_VT;             break;
                   case UNI_f:  string_value << UNI_FF;             break;
                   case UNI_r:  string_value << UNI_CR;             break;
                   default:   string_value << uni << uni1;
                 }
            }
         else if (uni == UNI_LEFT_DAQ)   // « another
            {
              MORE_ERROR() << "Invalid start of « string inside a « string";
              Error::throw_parse_error(E_NESTED_DAQ_STRING, src.data(),
                                       start_pos, src.get_pos(), LOC, loc);
            }
         else
            {
              string_value << uni;
            }
       }

   // an unterminated "..." or «...» is always an error: old-style multi-line
   // strings were already joined by Parser::transform_old_multi_line_strings()
   // (Bugs35 #55).
   //
   if (!got_end)
      {
        const int pos = start_pos + Workspace::get_IO();
        if (first == UNI_DOUBLE_QUOTE)
           {
             MORE_ERROR() << "Double quoted string (i.e. \"...\"): start at "
                             "column (prompt+" << pos << "), but no end.";
             Error::throw_parse_error(E_NO_STRING_END, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }
        if (first == UNI_LEFT_DAQ)
           {
             MORE_ERROR() << "DAQ string (i.e. «...»): start « at column "
                             "(prompt+" << pos << "), but no ending ».";
             Error::throw_parse_error(E_NO_STRING_END, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        MORE_ERROR() << "DAQ string (i.e. «...»): end » at column (prompt+"
                     << pos << "), but no starting «.";
        Error::throw_parse_error(E_NO_STRING_START, src.data(), start_pos,
                                 src.get_pos(), LOC, loc);
      }

   tos.emplace_back(Token(TOK_APL_VALUE1, Value_P(string_value, LOC)),
                    Function_PC(tos.size()));
}
//────────────────────────────────────────────────────────────────────────────
void
Tokenizer::tokenize_number(Unicode_source & src, Token_string & tos,
                           size_t & rest_2) const
{
   Log(LOG_tokenize)   CERR << "tokenize_number(" << src << ")" << endl;

const int start_pos = src.get_pos();

   // numbers:
   // real
   // real 'J' real
   // real 'D' real   // magnitude + angle in degrees
   // real 'R' real   // magnitude + angle in radian

const Int_or_Double real_val = tokenize_real(src);
   if (!real_val.is_valid)   // tokenize_real() sets )MORE info
      {
        rest_2 = src.rest_len();
        Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                 src.get_pos(), LOC, loc);
      }

   if (src.skip_if(UNI_J) || src.skip_if(UNI_j))   // e.g. 3J4
      {
        // a complex number in real + imag format
        //
        if (!(src.has_more() && Avec::is_number(*src)))
           {
             MORE_ERROR() << "Missing imaginary part I"
                             " in complex number RjI.";
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        const Int_or_Double imag_val = tokenize_real(src);
        if (!imag_val.is_valid)   // malformed imaginary part.
           {
             // tokenize_real() has already consumed at least one digit of
             // the imaginary part (guaranteed by the Avec::is_number()
             // check above) before failing, so there is no way to
             // un-consume just that and fall back to treating real_val as
             // a standalone number without silently dropping input;
             // tokenize_real() has already set )MORE text explaining why.
             //
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        tos.emplace_back(Token(TOK_COMPLEX, real_val.get_double(),
                                         imag_val.get_double()), Function_PC(tos.size()));
        Log(LOG_tokenize)
           CERR << "  tokenize_number: complex " << real_val.get_double()
                << "J" << imag_val.get_double() << endl;
      }
   else if (src.skip_if(UNI_D) || src.skip_if(UNI_d))   // e.g. 1D90 → 0J1
      {
        // a complex number in magnitude + degrees format
        //
        if (!(src.has_more() && Avec::is_number(*src)))
           {
             MORE_ERROR() << "Missing angle D in complex number MdD.";
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        const Int_or_Double degrees = tokenize_real(src);
        if (!degrees.is_valid)   // malformed angle: see the 'J' branch
                                  // above for why this cannot recover.
           {
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        // real_flt is the magnitude and the angle is in degrees.
        //
        const APL_Float real = real_val.get_double()
                             * cos(M_PI*degrees.get_double() / 180.0);
        const APL_Float imag = real_val.get_double()
                             * sin(M_PI*degrees.get_double() / 180.0);
        tos.emplace_back(Token(TOK_COMPLEX, real, imag), Function_PC(tos.size()));
        Log(LOG_tokenize)   CERR << "  tokenize_number: complex " << real
                                 << "J" << imag << endl;
      }
   else if (src.skip_if(UNI_R) || src.skip_if(UNI_r))   // 1r3.141592654 → ¯1J0
      {
        // a complex number in magnitude + radians format
        //
        if (!(src.has_more() && Avec::is_number(*src)))
           {
             MORE_ERROR() << "Missing angle R in complex number MrR.";
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        const Int_or_Double radian = tokenize_real(src);
        if (!radian.is_valid)   // malformed angle: see the 'J' branch
                                 // above for why this cannot recover.
           {
             Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                      src.get_pos(), LOC, loc);
           }

        // real_flt is the magnitude and the angle is in radian.
        //
        APL_Float real = real_val.get_double() * cos(radian.get_double());
        APL_Float imag = real_val.get_double() * sin(radian.get_double());
        tos.emplace_back(Token(TOK_COMPLEX, real, imag), Function_PC(tos.size()));
        Log(LOG_tokenize)   CERR << "  tokenize_number: complex " << real
                                 << "J" << imag << endl;
      }
   else   // neither XjY not XdY nor XrY
      {
        if (real_val.is_double)
           {
             tos.emplace_back(Token(TOK_REAL, real_val.value.APL_flt), Function_PC(tos.size()));
             Log(LOG_tokenize)
                CERR << "  tokenize_number: real "
                     << real_val.value.APL_flt << endl;
           }
        else
           {
             tos.emplace_back(Token(TOK_INTEGER, real_val.value.APL_int), Function_PC(tos.size()));
             Log(LOG_tokenize)
                CERR << "  tokenize_number: integer "
                     << real_val.value.APL_int << endl;
           }
      }

   /* ISO 13751 does:

      A. require a space between two numeric-scalar-literals (page 42), but
      B. requires no space between a numeric scalar literal and an identifier:

      Numeric Literal

      →──┬──→numeric-scalar-literal─┬──*─→
         ↑                          ↓
         └────────┬──blank──┬───────┘ ␄
                  │         │
␆ ␅               └────←────┘

      As a consequence, e.g. 10Q10 should parse as 10 Q10 in ISO.

      IBM APL2 requires a space in both cases.
      As a consequence, e.g. 10Q10 raises a SYNTAX ERROR inm IBM APL2.
     
      We follow ISO (and parse e.g. 10Q10 as 10 Q10).

      This leaves the case of two adjacent numeric literals. In this case
      the second of (two adjacent) numeric literal can not start with 0-9
      because that digit would have been eaten by the first 
      literal. Thereforethe only cases remaining to be checked are a
      numeric scalar literal followed by ¯ or by . (page 42).

      NOTE: The second case below (e.g. 10.10.10) will not come here since
            it would have reised an E_BAD_NUMBER  in tokenize_real() above.
    */
   if (src.has_more())
      {
        if (*src == UNI_OVERBAR ||   // e.g 10¯10
            *src == '.')             // e.g. 10.10.10
           Error::throw_parse_error(E_BAD_NUMBER, src.data(), start_pos,
                                    src.get_pos(), LOC, loc);
      }
}
//────────────────────────────────────────────────────────────────────────────
Tokenizer::Int_or_Double
Tokenizer::tokenize_hex(Unicode_source & src)
{
   src.get();   // skip $
   if (!src.has_more())   return Int_or_Double();   // $ at end of line
   if (!Avec::is_hex_digit(*src))   return Int_or_Double();   // no hex after $

   // accumulate in an unsigned type: shifting a signed APL_Integer once
   // it already occupies the sign bit (e.g. digit 16 of a literal such
   // as $8000000000000000, used deliberately to construct INT64_MIN) is
   // UB for a signed type pre-C++20. uint64_t shifting/overflow is
   // always well-defined, and the final cast back to APL_Integer gives
   // the same wrapped 2's-complement bit pattern that callers rely on.
   //
uint64_t hex_val = 0;
   while (src.has_more())
         {
           int digit;
           if      (*src <  UNI_0)   break;
           else if (*src <= UNI_9)   digit = src.get() - UNI_0;
           else if (*src <  UNI_A)   break;
           else if (*src <= UNI_F)   digit = 10 + src.get() - UNI_A;
           else if (*src <  UNI_a)   break;
           else if (*src <= UNI_f)   digit = 10 + src.get() - UNI_a;
           else                            break;

           // more than 64 bits: the high digits were silently dropped,
           // e.g. $10000000000000000 was 0 (Bugs34 #31)
           if (hex_val >> 60)
              {
                MORE_ERROR() << "hex literal with more than 64 bits";
                return Int_or_Double();   // bad number
              }
           hex_val = hex_val << 4 | uint64_t(digit);
         }

   return Int_or_Double(APL_Integer(hex_val));
}
//────────────────────────────────────────────────────────────────────────────
void
Tokenizer::tokenize_symbol(Unicode_source & src, Token_string & tos) const
{
   Log(LOG_tokenize)   CERR << "tokenize_symbol() : " << src.rest_len() << endl;

UCS_string symbol;
   symbol << src.get();

   while (src.has_more())
       {
         const Unicode uni = *src;
         if (!Avec::is_symbol_char(uni))   break;
         symbol << uni;
         ++src;
       }

   if (symbol.size() > 2 && symbol[1] == UNI_DELTA  &&
       (symbol[0] == UNI_S || symbol[0] == UNI_T))
      {
        // S∆ or T∆

        while (src.has_more() && *src <= UNI_SPACE)   src.get();   // spaces
        UCS_string symbol1(symbol, 2);   // without S∆/T∆
        Value_P AB(symbol1, LOC);
        cFunction_P ST = 0;
        if (symbol[0] == UNI_S) ST = &Quad_STOP::fun;
        else                    ST = &Quad_TRACE::fun;

        const bool assigned = (src.rest_len() && *src == UNI_LEFT_ARROW);
        if (assigned)   // dyadic: AB ∆fun
           {
             src.get();                                // skip ←
             Log(LOG_tokenize)
                CERR << "Stop/Trace assigned: " << symbol1 << endl;
             tos.emplace_back(Token(TOK_APL_VALUE1, AB), Function_PC(tos.size()));   // left argument of ST
             tos.emplace_back(Token(TOK_FUN2, ST), Function_PC(tos.size()));
           }
        else
           {
             Log(LOG_tokenize)
                CERR << "Stop/Trace referenced: " << symbol1 << endl;
             tos.emplace_back(Token(TOK_FUN2, ST), Function_PC(tos.size()));
             tos.emplace_back(Token(TOK_APL_VALUE1, AB), Function_PC(tos.size()));   // right arg of ST
           }

        return;
      }

Symbol * sym = Workspace::lookup_symbol(symbol);
   Assert(sym);
   tos.emplace_back(Token(TOK_SYMBOL, sym), Function_PC(tos.size()));
}
//════════════════════════════════════════════════════════════════════════════

