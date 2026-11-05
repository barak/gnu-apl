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

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sys/fcntl.h>   // for O_RDONLY
#include <sys/stat.h>

#include "Avec.hh"
#include "Quad_FIO.hh"
#include "Quad_JSON.hh"
#include "Value.hh"
#include "Workspace.hh"

Quad_JSON  Quad_JSON::fun;

//════════════════════════════════════════════════════════════════════════════
//════════════════════════════════════════════════════════════════════════════
Token
Quad_JSON::convert_file(const cValue & B) const
{
const UCS_string filename_ucs(B);
const UTF8_string filename_utf(filename_ucs);

   errno = 0;
const int fd = open(filename_utf.c_str(), O_RDONLY);
  if (fd == -1)
      {
        MORE_ERROR() << "1 ⎕JSON B: error reading " << B
                     << ": " << strerror(errno);
       DOMAIN_ERROR;
      }

struct stat st;
   if (fstat(fd, &st))
      {
        MORE_ERROR() << "1 ⎕JSON B: error in fstat(" << B
                     << "): " << strerror(errno);
        ::close(fd);
        DOMAIN_ERROR;
      }

   if (!S_ISREG(st.st_mode))
      {
        MORE_ERROR() << "1 ⎕JSON B: " << B << " is not a regular file";
        ::close(fd);
        DOMAIN_ERROR;
      }

UTF8 * buffer = 0;
   try
      {
        buffer = new UTF8[st.st_size];
      }
   catch (const std::bad_alloc &)
      {
        ::close(fd);
        MORE_ERROR() << "1 ⎕JSON B: file " << B << " is too large ("
                     << st.st_size << " bytes)";
        WS_FULL;
      }

const ssize_t bytes_read = read(fd, buffer, st.st_size);
   if (bytes_read != st.st_size)
      {
        ::close(fd);
        delete [] buffer;
        MORE_ERROR() << "1 ⎕JSON B: error in reading " << B
                     << "): " << strerror(errno);
        DOMAIN_ERROR;
      }

const UTF8_string json_string_utf8(buffer, bytes_read);
   delete[] buffer;
   ::close(fd);

UCS_string json_string_ucs(json_string_utf8);
Value_P json_string_value(json_string_ucs, LOC);

   return eval_B(*json_string_value);
}
Token
Quad_JSON::eval_AB(cValue_R A, cValue_R B) const
{
   if (A.get_rank() > 0)   RANK_ERROR;

const int function_number = A.get_int_value(0);
   switch(function_number)
      {
        case 0:   // same as monadic ⎕JSON
             {
               return eval_B(B);
             }

         case 1:   // read and convert a JSON file
              {
                return convert_file(B);
              }

         case 2:   // convert an APL value to a JSON string (unsorted)
              {
                return Token(TOK_APL_VALUE1, APL_to_JSON(B, false));
              }

         case 3:   // convert an APL value to a JSON string (sorted)
              {
                return Token(TOK_APL_VALUE1, APL_to_JSON(B, true));
              }

      }

   MORE_ERROR() << "A ⎕JSON B: Bad function number A=" << function_number;
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_JSON::eval_B(cValue_R B) const
{
   if (B.get_rank() != 1)   RANK_ERROR;

Value_P Z = JSON_to_APL(B);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
size_t
Quad_JSON::number_len(const UCS_string & ucs_B, ShapeItem b)
{
const ShapeItem B0 = b;

   // leading sign
   if (ucs_B[b] == UNI_MINUS)          ++b;
   else if (ucs_B[b] == UNI_OVERBAR)   ++b;

   // Integral part. Strict RFC 8259 (int = zero / (digit1-9 *DIGIT))
   // requires at least one digit here, but GNU APL's own ⎕JSON testsuite
   // (Quad_JSON.tc) already relies on also accepting a leading '.' with
   // no integer digit at all, e.g. ".3" -- so the digit requirement below
   // is checked jointly with the fractional part, not on the integral
   // part alone: reject only a mantissa with *no* digits anywhere
   // (neither int nor frac), which is what Blake McBride's Bugs19 #5
   // examples ("[1,-,2]", "-.") actually are.
   const ShapeItem int_start = b;
   while (uint32_t(ucs_B[b] - UNI_0) < 10)   ++b;
   const bool any_int_digit = (b != int_start);

   // RFC 8259: int = zero / (digit1-9 *DIGIT) -- a leading zero is only
   // allowed as the entire integral part ("0"), never followed by more
   // digits ("01", "0123"); this was silently accepted and the extra
   // digits kept (Blake McBride, Bugs21 #4b).
   if (any_int_digit && (b - int_start) > 1 && ucs_B[int_start] == UNI_0)
      {
        MORE_ERROR() << "⎕JSON B: malformed number (leading zero) at "
                     << B0 << "↓B";
        DOMAIN_ERROR;
      }

   bool any_frac_digit = false;
   if (ucs_B[b] == UNI_FULLSTOP)   // optional fractional part
      {
        ++b;   // skip UNI_FULLSTOP
        const ShapeItem frac_start = b;
       while (uint32_t(ucs_B[b] - UNI_0) < 10)   ++b;
        any_frac_digit = (b != frac_start);
      }

   if (!any_int_digit && !any_frac_digit)
      {
        MORE_ERROR() << "⎕JSON B: malformed number (no digits) at "
                     << B0 << "↓B";
        DOMAIN_ERROR;
      }

   if (ucs_B[b] == UNI_E || ucs_B[b] == UNI_e)   // optional exponent
      {
        ++b;   // skip UNI_E / UNI_e
        if (ucs_B[b] == UNI_MINUS)          ++b;
        else if (ucs_B[b] == UNI_OVERBAR)   ++b;
        // RFC 8259: exp = ("e" / "E") ["-" / "+"] 1*DIGIT -- a '+' sign
        // was not accepted (Blake McBride, Bugs16 #5), even though it is
        // exactly what glibc's "%g" (used by this file's own JSON writer,
        // APL_to_JSON_string()) emits for positive exponents, so e.g.
        // ⎕JSON 2⎕JSON 1E300 could not read back its own output.
        else if (ucs_B[b] == UNI_PLUS)      ++b;

        // RFC 8259 also requires at least one exponent digit (Bugs19 #5)
        const ShapeItem exp_start = b;
        while (uint32_t(ucs_B[b] - UNI_0) < 10)   ++b;
        if (b == exp_start)
           {
             MORE_ERROR() << "⎕JSON B: malformed number (no exponent "
                             "digits) at " << B0 << "↓B";
             DOMAIN_ERROR;
           }
      }

   return b - B0;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_JSON::APL_to_JSON(const cValue & B, bool sorted)
{
UCS_string ucs_Z;
   ucs_Z.reserve(2*B.get_enlist_count());
   APL_to_JSON_string(ucs_Z, B, 0, sorted);
   if (ucs_Z.size() == 0)   DOMAIN_ERROR;

Value_P Z(ucs_Z, LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::APL_to_JSON_string(UCS_string & result, const cValue & B,
                              int level, bool sorted)
{
   if (B.is_scalar())   // number or literal
      {
        Cell cache;
        APL_to_JSON_string(result, B.get_cfirst(cache), level, sorted);
        return;
      }

   if (B.is_char_vector())
      {
        const UCS_string ucs_string(B);
        escape_JSON_string(result, ucs_string);
        return;
      }

   if (B.get_rank() == 1)   // JSON array
      {
        UCS_string array(U"[ ");
        const ShapeItem ec = B.element_count();
        loop(e, ec)
            {
              if (e)   array << ", ";
              if (array.size() > 60)
                 {
                   array.back() = UNI_LF;
                   result << array;
                   array = UCS_string(2*level + 2, UNI_SPACE);
                 }

              Cell cache;
              APL_to_JSON_string(array, B.get_cravel(e, cache), level + 1, sorted);
              if (array.size() == 0)   // error in APL_to_JSON_string()
                 {
                   result.clear();   // indicate error
                   return;
                 }
            }
        array << UNI_SPACE << UNI_R_BRACK;
        result << array;
        return;
      }

   if (B.is_structured())   // JSON object
      {
        std::vector<ShapeItem> member_indices;
        B.used_members(member_indices, sorted);

        // Bugs9 #10 (Blake McBride): the opening brace was only ever
        // emitted inside the loop below (guarded by "if (m)"), so an
        // object with zero members printed the closing UNI_R_CURLY at
        // :259 without a matching opening one -- invalid JSON, and not
        // even parseable by ⎕JSON's own reader.
        if (member_indices.size() == 0)   { result << "{}";   return; }

        loop(m, member_indices.size())
           {
             Cell cache_name, cache_data;
             const Cell & member_name = B.get_cravel(2*member_indices[m], cache_name);
             const Cell & member_data = B.get_cravel(2*member_indices[m] + 1, cache_data);

             result << UCS_string(2*level, UNI_SPACE);   // level indent
             if (m)   result << "  ";
             else     result << "{ ";
             const UCS_string member(*member_name.get_pointer_value());
             escape_JSON_string(result, member);
             result << ": ";
             APL_to_JSON_string(result, member_data, level + 1, sorted);
             if (result.size() == 0)   return;   // error converting member_data
             if (size_t(m) < (member_indices.size() - 1))
                {
                  result << UNI_COMMA << UNI_LF;
                }
             else
                {
                  result << UNI_SPACE;
                }
           }

        result << UNI_R_CURLY;
        return;
      }

   // unexpected rank for a JSON value
   //
   MORE_ERROR() << "⎕JSON B: bad rank " << B.get_rank();
   result.clear();   // indicate error
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::escape_JSON_string(UCS_string & result, const UCS_string & in)
{
   result << UNI_DOUBLE_QUOTE;
   loop(u, in.size())
       {
         switch(const Unicode uni = in[u])
            {
              case UNI_BS:           result << "\\b";   break;
              case UNI_HT:           result << "\\t";   break;
              case UNI_LF:           result << "\\n";   break;
              case UNI_FF:           result << "\\f";   break;
              case UNI_CR:           result << "\\r";   break;
              case UNI_DOUBLE_QUOTE: result << "\\\"";   break;
              case UNI_BACKSLASH:    result << "\\\\";   break;

              default: if (uni < UNI_SPACE)
                          {
                            char cc[10];
                            SPRINTF(cc, "\\u%4.4X", int(uni));
                            result << cc;
                          }
                       else
                          {
                            result << uni;
                          }
            }
       }
   result << UNI_DOUBLE_QUOTE;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::APL_to_JSON_string(UCS_string & result, const Cell & cell,
                              int level, bool sorted)
{
   if (cell.is_integer_cell())
      {
        char cc[40];
        SPRINTF(cc, "%lld", long_long(cell.get_int_value()));
        result << cc;
        return;
      }

   if (cell.is_float_cell())
      {
        // Bugs10 #4 (Blake McBride): %lg without a precision truncates to
        // 6 significant digits, silently changing the value; and JSON has
        // no infinity literal, so a non-finite value must be rejected
        // rather than written as the bare (non-JSON) tokens inf/-inf.
        if (!isfinite(cell.get_real_value()))
           {
             MORE_ERROR() << "2 ⎕JSON B: B contains a non-finite value "
                             "(JSON has no infinity literal)";
             DOMAIN_ERROR;
           }
        char cc[40];
        SPRINTF(cc, "%.17g", cell.get_real_value());
        result << cc;
        return;
      }

   if (cell.is_complex_cell())
      {
        // JSON has no complex type, and GNU APL's own parser rejects the
        // 'J' notation this used to emit, so a complex cell cannot be
        // represented in JSON at all.
        MORE_ERROR() << "2 ⎕JSON B: B contains a complex value "
                        "(JSON has no complex type)";
        DOMAIN_ERROR;
      }

   if (cell.is_character_cell())   // a one-character string, e.g. S.z←'t'
      {
        // a scalar character is how APL stores a one-character string in
        // a strand or a member, so encode it as a JSON string. Bugs37 #20.
        //
        const UCS_string ucs_string(1, cell.get_char_value());
        escape_JSON_string(result, ucs_string);
        return;
      }

   if (!cell.is_pointer_cell())
      {
        MORE_ERROR() << "2 ⎕JSON B: Unexpected/unsupported Celltype "
                     << Cell::get_cell_type_name(cell.get_cell_type());
        DOMAIN_ERROR;
      }

   // at this point the cell should end up as char vector. Determint its depth.
   //
const cValue * Z = cell.get_pointer_value().get();
   if (!(Z->is_pointer_cell(0) && Z->is_scalar()))
      {
        APL_to_JSON_string(result, *Z, level, sorted);
        return;
      }

   Z = Z->get_pointer_value(0).get();
   if (!Z->is_char_vector())
      {
        MORE_ERROR() << "2 ⎕JSON B: B contains a doubly-enclosed value "
                        "that is not a character vector (JSON literals "
                        "are encoded as ⊂'true'/⊂'false'/⊂'null')";
        DOMAIN_ERROR;
      }

const UCS_string lit_ucs(*Z);
   if (lit_ucs.compare(UCS_ASCII_string("null"))  == COMP_EQ ||
       lit_ucs.compare(UCS_ASCII_string("true"))  == COMP_EQ ||
       lit_ucs.compare(UCS_ASCII_string("false")) == COMP_EQ)
      {
        result << lit_ucs;
        return;
      }

   MORE_ERROR() << "2 ⎕JSON B: bad JSON literal ⊂'" << lit_ucs
                << "'. Expecting ⊂'true', ⊂'false', or ⊂'null'";
   result.clear();   // indicate error
}
//────────────────────────────────────────────────────────────────────────────
size_t
Quad_JSON::comma_count(const UCS_string & ucs_B,
                       const std::vector<ShapeItem> & tokens_B,
                       size_t & token0)
{
   // ucs_B is the JSON text string being parsed,
   // tokens_B are the start positions (in ucs_B) of the tokenized ucs_B,
   // token0 is the current token in tokens_B which is either [ (start of a
   // JSON array) or { (start of a JSON object).
   //
const Unicode start = ucs_B[tokens_B[token0]];   // either [ or {/
Unicode end;
   if      (start == UNI_L_BRACK)   end = UNI_R_BRACK;
   else if (start == UNI_L_CURLY)   end = UNI_R_CURLY;
   else FIXME;

UCS_string stack(end);   // a stack of ] amd } to track nested [...] and {...}
size_t commas = 0;       // the numebr of (top-level-) commas
bool expect_comma = false;
bool expect_colon = true;

   for (++token0; token0 < tokens_B.size(); ++token0)
       {
         const Unicode uni = ucs_B[tokens_B[token0]];
         switch(uni)
               {
                 case UNI_L_BRACK:            // [
                      stack << UNI_R_BRACK;   // push ]
                      continue;

                 case UNI_L_CURLY:            //      {
                      stack << UNI_R_CURLY;   // push }
                      continue;

                 case UNI_COMMA:
                      if (stack.size() > 1)   continue;
                      if (!expect_comma)
                         {
                           MORE_ERROR() << "⎕JSON B: Got unexpected ',' at "
                                        <<  tokens_B[token0] << "↓B";
                           DOMAIN_ERROR;
                         }

                      ++commas;
                      expect_comma = false;
                      expect_colon = true;
                      continue;

                 case UNI_COLON:
                      if (stack.size() > 1)   continue;
                      if (start == UNI_L_BRACK)
                         {
                           // ':' is only legal inside a JSON *object*
                           // ({"name": value, ...}); this arm used to
                           // check only expect_colon (which starts true
                           // regardless of [ vs {), so the first ':' at
                           // the top level of a JSON *array* was silently
                           // accepted, e.g. "[1:2]" (Blake McBride, Bugs16
                           // #3) -- comma_count() then returned commas==0
                           // (no top-level ',' was ever seen), so
                           // parse_array() took its "one element" branch,
                           // parsed only the "1", and tripped its own
                           // Assert(token0 == token_from) on the leftover
                           // ":2" instead of reporting a clean error.
                           //
                           MORE_ERROR() << "⎕JSON B: Got unexpected ':' "
                                           "inside a JSON array at "
                                        <<  tokens_B[token0] << "↓B";
                           DOMAIN_ERROR;
                         }
                      if (!expect_comma)
                         {
                           MORE_ERROR() << "⎕JSON B: Got unexpected ':' at "
                                        <<  tokens_B[token0] << "↓B";
                           DOMAIN_ERROR;
                         }

                      expect_comma = false;
                      if (!expect_colon)
                         {
                           MORE_ERROR() << "⎕JSON B: Got ':' "
                                           "instead of ',' at "
                                        <<  tokens_B[token0] << "↓B";
                           DOMAIN_ERROR;
                         }
                      expect_colon = false;
                      continue;

                 case UNI_R_BRACK:
                      //
                      // UNI_R_BRACK must match stack.back()
                      //
                      if (stack.back() != UNI_R_BRACK)
                         {
                           MORE_ERROR() << "Mismatch ]";
                           DOMAIN_ERROR;
                         }
                      stack.pop_back();
                      if (stack.size() == 0)   return commas;
                      expect_comma = true;
                      continue;

                 case UNI_R_CURLY:
                      //
                      // UNI_R_CURLY must match stack.back()
                      //
                      if (stack.back() != UNI_R_CURLY)
                         {
                           MORE_ERROR() << "Mismatch ]";
                           DOMAIN_ERROR;
                         }
                      stack.pop_back();
                      if (stack.size() == 0)   return commas;
                      expect_comma = true;
                      continue;

                 default:
                      if (stack.size() > 1)   continue;   // not of interest
                      if (expect_comma)
                         {
                           MORE_ERROR() << "⎕JSON B: Got '" << uni
                                        << "' when expecting ',' at "
                                        <<  tokens_B[token0] << "↓B";
                           DOMAIN_ERROR;
                         }
                      expect_comma = true;
               }

       }

   MORE_ERROR() << "⎕JSON B: No matching " << end << " for " << start
                << " at end of B";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
Unicode
Quad_JSON::decode_UUUU(const UCS_string & ucs_B, ShapeItem b)
{
   // Returns Invalid_Unicode (NOT Unicode_0) on failure: Unicode_0 is also
   // the legitimate result of decoding U+0000, so returning it for both
   // made a well-formed U+0000 escape indistinguishable from a malformed
   // one, and every caller that checked "== Unicode_0" to detect failure
   // treated the former as the latter (Blake McBride, Bugs15 #10).
   //
   // need 6 chars: \, u, and 4 hex digits.
   //
   if (b < 0 || b + 6 > ucs_B.ssize())   return Invalid_Unicode;
   if (ucs_B[b++] != UNI_BACKSLASH)   return Invalid_Unicode;
   if (ucs_B[b++] != UNI_u)           return Invalid_Unicode;

char cc[5];
   cc[0] = ucs_B[b++];
   cc[1] = ucs_B[b++];
   cc[2] = ucs_B[b++];
   cc[3] = ucs_B[b++];
   cc[4] = 0;
   loop(i, 4)
      {
        if (!isxdigit(uint8_t(cc[i])))   return Invalid_Unicode;
      }

   return Unicode(strtoll(cc, 0, 16));
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_JSON::JSON_to_APL(const cValue & B)
{
const ShapeItem len_B = B.element_count();

UCS_string ucs_B;
   ucs_B.reserve(len_B + 1);
   loop(b, len_B)   ucs_B << B.get_char_value(b);
   ucs_B << Unicode_0;   // 0-terminate ucs_B to avoid too many length checks

   // tokenize ucs_B
   //
std::vector<ShapeItem> tokens_B;
   tokens_B.reserve(10 + len_B/2);

   loop(b, ucs_B.size())
       {
         const Unicode uni = ucs_B[b];
         if (uni <= ' ')   continue;   // skip leading whitespace

         tokens_B.push_back(b);
         switch(uni)
            {
              case UNI_DOUBLE_QUOTE:
                   skip_string(ucs_B, b);
                   continue;

              case UNI_COMMA:
              case UNI_COLON:
              case UNI_L_BRACK:
              case UNI_R_BRACK:
              case UNI_L_CURLY:
              case UNI_R_CURLY: continue;       // the six structural chars

              case UNI_f: b += 4;   continue;   // literal false
              case UNI_n:                       // literal null (Bugs15 #9:
                                                 // this comment and the one
                                                 // below were swapped)
              case UNI_t: b += 3;   continue;   // literal true

              case UNI_MINUS:
              case UNI_OVERBAR:
              case UNI_FULLSTOP:
              case UNI_0 ... UNI_9:
                   b += number_len(ucs_B, b) - 1;
                   continue;

              default:
                   MORE_ERROR() << "⎕JSON B: Got '" << uni <<
                   "' when expecting a JSON token at " << b << "↓B";
                   DOMAIN_ERROR;
            }
       }

Value_P Z(LOC);
size_t token0 = 0;
   parse_value(*Z, ucs_B, tokens_B, token0, 0);
   Z->check_value(LOC);

   if (token0 != tokens_B.size())
      {
        MORE_ERROR() <<
        "⎕JSON B: there were extra tokens in B (tokenized: "
        << int(tokens_B.size()) << ", but processed: " << int(token0) <<
        ").\n    The JSON string must be one serialized value.";
        LENGTH_ERROR;
      }

   if (Z->is_simple_scalar())
      {
        Assert(Z->is_numeric(0));
        return Z;   // number
      }

   Assert(Z->is_scalar());
   Assert(Z->is_pointer_cell(0));

   // a bare top-level literal (true/false/null) must keep BOTH enclosure
   // levels that parse_literal() built (Z ≡ ⊂⊂'true'), matching exactly
   // what the same literal gets as an array/object element -- that double
   // enclosure is APL_to_JSON_string()'s own marker for "this is a JSON
   // literal, not a string" (see its error message: "JSON literals are
   // encoded as ⊂'true'/⊂'false'/⊂'null'"). The generic peel below is
   // correct for every OTHER top-level result (plain strings, arrays,
   // objects), but was also stripping the one level that distinguishes a
   // literal from an ordinary string, so e.g. 2⎕JSON ⎕JSON 'true' wrote
   // "true" back out as a quoted JSON string instead of the bare literal
   // true (Blake McBride, Bugs16 #8).
   //
   if (tokens_B.size())
      {
        const Unicode first = ucs_B[tokens_B[0]];
        if (first == UNI_t || first == UNI_f || first == UNI_n)   return Z;
      }

   return Z->get_pointer_value(0);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_array(Value & Z, const UCS_string & ucs_B,
                       const std::vector<ShapeItem> & tokens_B,
                       size_t & token0, size_t depth)
{
   if (depth > MAX_DEPTH)   LIMIT_ERROR_NESTING;

size_t token_from = token0;
   Assert(ucs_B[tokens_B[token_from]] == UNI_L_BRACK);   // [

const size_t commas = comma_count(ucs_B, tokens_B, token0);
   Assert(ucs_B[tokens_B[token0]] == UNI_R_BRACK);   // always ]

   ++token_from;   // skip [
   if (commas == 0)   // [ ] or [ item ]
      {
        if ((token0 - token_from) == 0)   // [ ]
           {
             // CERR << "empty ARRAY" << std::endl;
             Value_P Zsub = Idx0(LOC);
             Z.next_ravel_Pointer(Zsub.get());
           }
        else
           {
             // CERR << "One element ARRAY" << std::endl;
             Value_P Zsub(1, LOC);
             parse_value(*Zsub, ucs_B, tokens_B, token_from, depth + 1);
             Zsub->check_value(LOC);
             Z.next_ravel_Value(Zsub.get());
           }
        ++token_from;   // skip ]
      }
   else               // [ item , item... ]
      {
        const size_t len = commas + 1;
        // CERR << "ARRAY with " << len << " elements" << endl;

        Value_P Zsub(len, LOC);
        loop(l, len)
            {
              parse_value(*Zsub, ucs_B, tokens_B, token_from, depth + 1);
              const Unicode uni = ucs_B[tokens_B[token_from]];
              if (uni == UNI_COMMA)           ++token_from;
              else if (uni == UNI_R_BRACK)    ++token_from;
              else
                 {
                   MORE_ERROR() << "⎕JSON B: Got '" << uni
                                << "' when expecting ',' or ']' at "
                                <<  int(token_from) << "↓B";
                   DOMAIN_ERROR;
                 }
            }
        Zsub->check_value(LOC);
        Z.next_ravel_Value(Zsub.get());
      }

   ++token0;   // skip final ]
   if (token0 != token_from)
      {
        // reachable from malformed user input, not just internal
        // inconsistency (Blake McBride, Bugs16 #3): was an unconditional
        // Assert(), which dumped a backtrace for what is simply bad JSON.
        //
        MORE_ERROR() << "⎕JSON B: malformed JSON array";
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_literal(Value & Z, const UCS_string & ucs_B,
                         ShapeItem b, const char * expected_literal)
{
const size_t len = strlen(expected_literal);
   loop(l, len)
       {
         if (ucs_B[b + l] != expected_literal[l])
            {
              MORE_ERROR() << "⎕JSON B: misspelled JSON literal "
                           << UCS_string(ucs_B, b, len) << " at " << b
                           << "↓B (expecting " << expected_literal << ")";
              DOMAIN_ERROR;
            }
       }

Value_P Zsubsub(len, LOC);
   loop(l, len)
       Zsubsub->next_ravel_Char(Unicode(expected_literal[l]));
   Zsubsub->check_value(LOC);

Value_P Zsub(LOC);
   Zsub->next_ravel_Pointer(Zsubsub.get());
   Zsub->check_value(LOC);

   Z.next_ravel_Pointer(Zsub.get());
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_number(Value & Z, const UCS_string & ucs_B, ShapeItem b)
{
   enum { MAX_NUMLEN = 90 };
char cc[MAX_NUMLEN + 10];
size_t cc_len = 0;
bool need_fract = false;
bool have_expo = false;
double dval = 0;
   // digit-presence tracking for the "malformed number" check below
   // (Bugs19 #5, Blake McBride) -- declared here, not in a nested scope,
   // so they stay valid across the goto number_too_long jumps below.
bool any_int_digit = false;
bool any_frac_digit = false;

   if (ucs_B[b] == UNI_MINUS || ucs_B[b] == UNI_OVERBAR)
      { ++b;   cc[cc_len++] = '-'; }

   // copy integer part to cc...
   //
   while (ucs_B[b] >= UNI_0 && ucs_B[b] <= UNI_9)
         {
           any_int_digit = true;
           if (cc_len >= MAX_NUMLEN)   goto number_too_long;
           cc[cc_len++] = ucs_B[b++];
         }

   // maybe copy fractional part to cc...
   //
   if (ucs_B[b] == UNI_FULLSTOP)   // fractional part
      {
        need_fract = true;   // force FloatCell
        if (cc_len >= MAX_NUMLEN)   goto number_too_long;
        cc[cc_len++] = '.';
        ++b;
        while (ucs_B[b] >= UNI_0 && ucs_B[b] <= UNI_9)
              {
                any_frac_digit = true;
                if (cc_len >= MAX_NUMLEN)   goto number_too_long;
                cc[cc_len++] = ucs_B[b++];
              }
      }

   // Strict RFC 8259 (int = zero / (digit1-9 *DIGIT)) requires at least
   // one integer digit, but GNU APL's own ⎕JSON testsuite already relies
   // on also accepting a leading '.' with no integer digit, e.g. ".3" --
   // so (matching number_len()'s check above) reject only a mantissa
   // with *no* digits anywhere, neither int nor frac (Bugs19 #5, Blake
   // McBride: "[1,-,2]", "-.").
   if (!any_int_digit && !any_frac_digit)
      {
        MORE_ERROR() << "⎕JSON B: malformed number (no digits) at "
                     << b << "↓B";
        DOMAIN_ERROR;
      }

   // maybe copy exponent part to cc...
   //
   if (ucs_B[b] == UNI_E || ucs_B[b] == UNI_e)   // exponent part
      {
        have_expo = true;
        if (cc_len >= MAX_NUMLEN)   goto number_too_long;
        cc[cc_len++] = 'e';
        ++b;
        if (ucs_B[b] == UNI_MINUS || ucs_B[b] == UNI_OVERBAR)   // negative expo
           {
             need_fract = true;
             if (cc_len >= MAX_NUMLEN)   goto number_too_long;
             ++b;   cc[cc_len++] = '-'; }
        else if (ucs_B[b] == UNI_PLUS)   // '+' exponent (Bugs16 #5): see
           {                             // number_len()'s matching fix
             ++b;
           }
        {
          // RFC 8259: at least one exponent digit is mandatory (Bugs19 #5,
          // Blake McBride) -- must match number_len()'s check above.
          const size_t exp_start = cc_len;
          while (ucs_B[b] >= UNI_0 && ucs_B[b] <= UNI_9)
                {
                  if (cc_len >= MAX_NUMLEN)   goto number_too_long;
                   cc[cc_len++] = ucs_B[b++];
                }
          if (cc_len == exp_start)
             {
               MORE_ERROR() << "⎕JSON B: malformed number (no exponent "
                               "digits) at " << b << "↓B";
               DOMAIN_ERROR;
             }
        }
      }
   cc[cc_len++] = 0;

   // always scan the double value
   //
   dval = strtod(cc, 0);

   // Bugs10 #4 (Blake McBride): a numeric literal that overflows to ±∞
   // is not a valid JSON number; reject it instead of silently accepting
   // it as a FloatCell holding an infinity that 2 ⎕JSON could not emit.
   if (!isfinite(dval))
      {
        MORE_ERROR() << "⎕JSON B: numeric literal '" << cc
                     << "' is out of range";
        DOMAIN_ERROR;
      }

   if (dval > LARGE_INT || dval < SMALL_INT)   need_fract = true;

   if (need_fract)
      {
        Z.next_ravel_Float(dval);
        return;
      }
   else if (have_expo)   // so strtoll() won't work
      {
        if (dval < 0)   Z.next_ravel_Int(dval - 0.5);   // round negative → 0
        else            Z.next_ravel_Int(dval + 0.5);   // round positive → 0
        return;
      }
   else
      {
        const long long int ival = strtoll(cc, 0, 10);
        Z.next_ravel_Int(ival);
        return;
      }

number_too_long:
   MORE_ERROR() << "⎕JSON B: number too long at " << b
                << "↓B (max. length is " << MAX_NUMLEN << ")";
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_object(Value & Z, const UCS_string & ucs_B,
                        const std::vector<ShapeItem> & tokens_B,
                        size_t & token0, size_t depth)
{
   if (depth > MAX_DEPTH)   LIMIT_ERROR_NESTING;

size_t token_from = token0;
   Assert(ucs_B[tokens_B[token_from]] == UNI_L_CURLY);   // {

const size_t commas = comma_count(ucs_B, tokens_B, token0);
   Assert(ucs_B[tokens_B[token0]] == UNI_R_CURLY);   // always }

Value_P assoc_array = EmptyStruct(LOC);
   Z.next_ravel_Pointer(assoc_array.get());

   ++token_from;   // skip {
   if (commas == 0)   // { } or { 'name' : value }
      {
        if ((token0 - token_from) == 0)   // { }
           {
             // CERR << "empty OBJECT" << std::endl;
             ++token_from;   // skip }
           }
        else
           {
             // CERR << "One element OBJECT" << std::endl;
             parse_object_member(*assoc_array, ucs_B, tokens_B, token_from,
                                  depth + 1);
           }
      }
   else               // { 'name' : value , 'name' : value... }
      {
        const size_t items = commas + 1;
        // CERR << "OBJECT with " << items << " elements" << endl;

        loop(it, items)
            {
              parse_object_member(*assoc_array, ucs_B, tokens_B, token_from,
                                   depth + 1);
            }
      }

   ++token0;   // skip final }
   if (token0 != token_from)
      {
        // see the twin check in parse_array() (Blake McBride, Bugs16 #3)
        //
        MORE_ERROR() << "⎕JSON B: malformed JSON object";
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_object_member(Value & Z, const UCS_string & ucs_B,
                               const std::vector<ShapeItem> & tokens_B,
                               size_t & token_from, size_t depth)
{
const size_t B_start =  tokens_B[token_from];
   if (ucs_B[B_start] != UNI_DOUBLE_QUOTE)
      {
        MORE_ERROR() << "⎕JSON B: Got '" << ucs_B[B_start] <<
        "' when expecting (double-quoted) object member name at " <<
        int(B_start) << "↓B";
        DOMAIN_ERROR;
      }
   ++token_from;   // skip "

   // parse "member-name" :
   //
UCS_string member_name;
   {
     const ShapeItem ucs_B_name = B_start + 1;

     const ShapeItem ucs_B_colon = tokens_B[token_from];
     if (ucs_B[ucs_B_colon] != UNI_COLON)
        {
          MORE_ERROR() <<
          "⎕JSON B: Got '" << ucs_B[ucs_B_colon] <<
          "' when expecting ':' after object member name at " <<
          int(B_start) << "↓B";
          DOMAIN_ERROR;
        }
     ++token_from;   // skip :

     // find trailing " of "member-name" :  by searching backwards from ':'
     //
     ShapeItem name_len = ucs_B_colon - ucs_B_name;
     while (name_len && ucs_B[ucs_B_name + name_len - 1] != UNI_DOUBLE_QUOTE)
           --name_len;
     --name_len;   // skip the trailing "
     if (name_len < 0)
        {
          // name_len == 0 is a legitimate empty member name (e.g. the
          // "" in {"":1}, valid JSON); only a negative name_len means no
          // closing " was found at all (confirmed: with the old "< 1"
          // check, {"":1} itself crashed the interpreter via FIXME/exit).
          MORE_ERROR() << "⎕JSON B: missing closing '\"' of object "
                          "member name at " << int(B_start) << "↓B";
          DOMAIN_ERROR;
        }

     // decode escapes (\uXXXX, \n, \", ...) instead of taking the raw
     // source characters: object member names were copied verbatim,
     // unlike string values (parse_string() below), so {"aA":1,
     // "aA":2} was accepted as two distinct keys even though A
     // decodes to 'A' and both keys are really the same name "aA" --
     // defeating the duplicate-key check right below this block.
     member_name = decode_json_string_escapes(
                       UCS_string(ucs_B, ucs_B_name, name_len));
   }

   // check that member_name does not yet exist in Z.
   //
   if (Z.get_member_data(member_name))
      {
        // member name exists already
        //
        MORE_ERROR() << "⎕JSON: duplicate member name '" << member_name
                     << "' at " << int(B_start) << "↓B";
        DOMAIN_ERROR;
      }

   // parse the member value
   {
     Value_P Zsub(LOC);
     parse_value(*Zsub, ucs_B, tokens_B, token_from, depth);
     Zsub->check_value(LOC);

     Cell * member_data = Z.get_new_member(member_name);
     Cell cache;
     Zsub->get_cfirst(cache).init_other(member_data, Z, LOC);
   }

   // check member-seperator (or end of object).
   {
     const ShapeItem ucs_B_sepa = tokens_B[token_from++];   // skip , or }
     const Unicode sepa = ucs_B[ucs_B_sepa];
     if (sepa != UNI_COMMA && sepa != UNI_R_CURLY)
        {
          MORE_ERROR() << "⎕JSON B: Got '" << sepa <<
               "' when expecting ',' or '}' after \"" << member_name
               << "\" : at " << int(B_start) << "↓B";
          DOMAIN_ERROR;
        }
   }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_JSON::decode_json_string_escapes(const UCS_string & raw)
{
   // same escape table as parse_string() below, adapted to walk a plain
   // (already-dequoted) UCS_string instead of a position within the
   // token-indexed source text.
UCS_string result;
   for (ShapeItem bb = 0; bb < raw.ssize();)
       {
         Unicode uni = raw[bb++];
         if (uni == UNI_BACKSLASH && bb < raw.ssize())
            {
              Unicode surr = Unicode_0;

              uni = raw[bb++];   // default: escaped = character itself
              switch(uni)
                 {
                   case UNI_b: uni = UNI_BS;   break;   // \b
                   case UNI_f: uni = UNI_FF;   break;   // \f
                   case UNI_n: uni = UNI_LF;   break;   // \n
                   case UNI_r: uni = UNI_CR;   break;   // \r
                   case UNI_t: uni = UNI_HT;   break;   // \t
                   case UNI_u:                          // \uUUUU
                        uni  = decode_UUUU(raw, bb - 2);
                        surr = decode_UUUU(raw, bb + 4);
                        if (is_high_surrogate(uni) && is_low_surrogate(surr))
                           {
                             uni = Unicode(0x10000 + (surr & 0x03FF)
                                                   + ((uni  & 0x03FF) << 10));
                             bb += 10;
                           }
                        else if (is_high_surrogate(uni))
                           {
                             MORE_ERROR() << "⎕JSON B: No low surrogate "
                                             "in object member name";
                             DOMAIN_ERROR;
                           }
                        else if (uni == Invalid_Unicode)
                           {
                             // used to be indistinguishable from a
                             // well-formed U+0000 (both decode_UUUU()
                             // returned Unicode_0), so this genuinely
                             // malformed escape was silently accepted:
                             // uni (0) was appended to result and bb was
                             // NOT advanced past the 4 hex digits, so they
                             // got re-processed as literal text on the
                             // next iteration (Blake McBride, Bugs15 #10).
                             //
                             MORE_ERROR() << "⎕JSON B: bad escape sequence "
                                             "in object member name";
                             DOMAIN_ERROR;
                           }
                        else if (uni == Unicode_0)   // well-formed U+0000
                           {
                             MORE_ERROR() << "⎕JSON B: NUL characters "
                                             "(\\u0000) are not supported "
                                             "in object member names";
                             DOMAIN_ERROR;
                           }
                        else   // normal (non-surrogate) \uUUUU
                           {
                             bb += 4;
                           }
                        break;   // case UNI_u

                   default: break;
                 }
            }

         result << uni;
       }

   return result;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_string(Value & Z, const UCS_string & ucs_B, ShapeItem b)
{
ShapeItem bb = b + 1;   // NOTE: skip_string() will increment b
const ShapeItem content_len = skip_string(ucs_B, b);

Value_P Zsub(content_len, LOC);
   loop(l, content_len)
       {
         Unicode uni = ucs_B[bb++];
         if (uni == UNI_BACKSLASH)
            {
              Unicode surr = Unicode_0;

              uni = ucs_B[bb++];  // default: escaped = character itself
              switch(uni)
                 {
                   case UNI_b: uni = UNI_BS;   break;   // \b
                   case UNI_f: uni = UNI_FF;   break;   // \f
                   case UNI_n: uni = UNI_LF;   break;   // \n
                   case UNI_r: uni = UNI_CR;   break;   // \r
                   case UNI_t: uni = UNI_HT;   break;   // \t
                   case UNI_u:                          // \uUUUU
                        uni  = decode_UUUU(ucs_B, bb - 2);
                        surr = decode_UUUU(ucs_B, bb + 4);
                        if (is_high_surrogate(uni) && is_low_surrogate(surr))
                           {
                             uni = Unicode(0x10000 + (surr & 0x03FF)
                                                   + ((uni  & 0x03FF) << 10));
                             bb += 10;
                           }
                        else if (is_high_surrogate(uni))
                           {
                             MORE_ERROR() <<
                             "⎕JSON B: No low surrogate at " << bb << "↓B";
                             DOMAIN_ERROR;
                           }
                        else if (uni == Invalid_Unicode)   // decode_UUUU()
                                                            // genuinely failed
                           {
                             FIXME;   // since skip_string() should have failed
                           }
                        else   // normal (non-surrogate) \uUUUU, possibly
                               // Unicode_0 -- skip_string() already
                               // rejected that case before parse_string()
                               // is ever reached
                           {
                             bb += 4;
                           }
                        break;   // case UNI_u

                   default: break;
                 }
            }

         Zsub->next_ravel_Char(uni);
       }

   // Bugs9 #11 (Blake McBride): with content_len == 0 ("" ) the loop above
   // never runs, so Zsub never gets a character cell and silently keeps
   // the default numeric prototype -- a subsequent re-serialisation then
   // (correctly, given the wrong prototype) emits a JSON array instead of
   // an empty string. Same one-line shape as Bugs9 #7.
   if (content_len == 0)   Zsub->set_proto_Spc();

   Zsub->check_value(LOC);
   Z.next_ravel_Pointer(Zsub.get());
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_JSON::parse_value(Value & Z, const UCS_string & ucs_B,
                      const std::vector<ShapeItem> & tokens_B, size_t & token0,
                      size_t depth)
{
   // tokens_B.at(token0) below throws std::out_of_range for an empty (or
   // exhausted, e.g. a trailing-comma "[1,]") token list -- that is not
   // an APL Error, so it isn't caught by the normal error path and instead
   // propagates to Workspace::immediate_execution()'s generic catch(...),
   // which prints a FIXME diagnostic and calls exit(0), silently killing
   // the whole interpreter session. Reject it as an ordinary DOMAIN_ERROR
   // instead (reachable via e.g. "⎕JSON ''" or "1 ⎕JSON 'empty.json'").
   if (token0 >= tokens_B.size())
      {
        MORE_ERROR() << "⎕JSON: unexpected end of input when expecting "
                        "a value";
        DOMAIN_ERROR;
      }

const ShapeItem b = tokens_B[token0];
   switch(ucs_B[b])
      {
        // unexpected token...
        //
        case UNI_COMMA:
             MORE_ERROR() << "⎕JSON: Got ',' when expecting a value";
             DOMAIN_ERROR;

        case UNI_COLON:
             MORE_ERROR() << "⎕JSON: Got ':' when expecting a value";
             DOMAIN_ERROR;

        case UNI_R_BRACK:
             MORE_ERROR() << "⎕JSON: Got ']' when expecting a value";
             DOMAIN_ERROR;

        case UNI_R_CURLY:
             MORE_ERROR() << "⎕JSON: Got '}' when expecting a value";
             DOMAIN_ERROR;

        // single token (we increment token0)...
        //
        case '-':
        case UNI_OVERBAR:
        case UNI_FULLSTOP:
        case '0' ... '9': parse_number(Z, ucs_B, b);       ++token0;   break;
        case UNI_DOUBLE_QUOTE: parse_string(Z, ucs_B, b);  ++token0;   break;
        case UNI_f: parse_literal(Z, ucs_B, b, "false");   ++token0;   break;
        case UNI_n: parse_literal(Z, ucs_B, b, "null");    ++token0;   break;
        case UNI_t: parse_literal(Z, ucs_B, b, "true");    ++token0;   break;

        // multi token (they increment token0)...
        //
        case UNI_L_BRACK: parse_array (Z, ucs_B, tokens_B, token0, depth); break;
        case UNI_L_CURLY: parse_object(Z, ucs_B, tokens_B, token0, depth); break;

        default: FIXME;
      }
}
//────────────────────────────────────────────────────────────────────────────
size_t
Quad_JSON::skip_string(const UCS_string & ucs_B, ShapeItem & b)
{
const ShapeItem B0 = b++;   // the leading "
   Assert(ucs_B[B0] == UNI_DOUBLE_QUOTE);

ShapeItem content_len = 0;
   for (; b < ucs_B.ssize(); ++b)
       {
         const Unicode uni = ucs_B[b];
         if (uni == UNI_DOUBLE_QUOTE)   return content_len;

         // RFC 8259 §7: U+0000..U+001F MUST be escaped inside a string
         // (Blake McBride, Bugs22 #6) -- the escape table below is
         // already validated; this is the unescaped half of the same
         // rule.
         if (uni < UNI_SPACE)
            {
              char cc[20];
              SPRINTF(cc, "U+%4.4X", uni);
              MORE_ERROR() << "⎕JSON B: unescaped control character "
                           << cc << " in string at " << b << "↓B";
              DOMAIN_ERROR;
            }

        ++content_len;
        if (uni == UNI_BACKSLASH)   // skip the escaped part...
           {
             // safe_at(): like ucs_B[idx], but returns a placeholder
             // instead of reading out of bounds -- decode_UUUU() only
             // guarantees indices up to (but not including) its own
             // failure point are in range, and a truncated escape (e.g.
             // a lone trailing '\' or '\u') can fail before all 6 (or,
             // for a high surrogate, 12) fields below are valid.
             auto safe_at = [&ucs_B](ShapeItem idx) -> Unicode
                { return (idx >= 0 && idx < ucs_B.ssize())
                         ? ucs_B[idx] : Unicode('?'); };

             if (safe_at(b + 1) == UNI_u)   // \uUUUU
                {
                  const Unicode u1 = decode_UUUU(ucs_B, b);
                  if (u1 == Invalid_Unicode)   // decode_UUUU() genuinely failed
                     {
                       MORE_ERROR() << "⎕JSON B: bad escape sequence "
                                    << safe_at(b    ) << safe_at(b + 1)
                                    << safe_at(b + 2) << safe_at(b + 3)
                                    << safe_at(b + 4) << safe_at(b + 5)
                                    << " at " << b << "↓B";
                       DOMAIN_ERROR;
                     }
                  if (u1 == Unicode_0)   // well-formed \u0000
                     {
                       // Unicode_0 is used throughout this file as an
                       // internal end-of-buffer sentinel (see
                       // JSON_to_APL()'s "0-terminate ucs_B" comment), so a
                       // real embedded NUL can't safely round-trip; this
                       // used to be indistinguishable from a genuinely
                       // malformed escape and reported as one (Blake
                       // McBride, Bugs15 #10) -- now a distinct, honest
                       // message instead of a wrong one.
                       //
                       MORE_ERROR() << "⎕JSON B: NUL characters (\\u0000) "
                                       "are not supported, at " << b << "↓B";
                       DOMAIN_ERROR;
                     }

                  if (is_high_surrogate(u1))
                     {
                       const Unicode u2 = decode_UUUU(ucs_B, b + 6);
                       if (u2 == Invalid_Unicode)   // genuinely failed
                          {
                            MORE_ERROR() << "⎕JSON B: bad escape sequence "
                                         << safe_at(b +  6) << safe_at(b +  7)
                                         << safe_at(b +  8) << safe_at(b +  9)
                                         << safe_at(b + 10) << safe_at(b + 11)
                                         << " at " << (b + 6) << "↓B";
                            DOMAIN_ERROR;
                          }
                       if (u2 == Unicode_0)   // well-formed low surrogate \u0000
                          {
                            MORE_ERROR() << "⎕JSON B: NUL characters "
                                            "(\\u0000) are not supported, "
                                            "at " << (b + 6) << "↓B";
                            DOMAIN_ERROR;
                          }
                       if (!is_low_surrogate(u2))
                          {
                            MORE_ERROR() << "⎕JSON B: No low surrogate "
                                            "at " << (b + 6) << "↓B";
                            DOMAIN_ERROR;
                          }
                       b += 11;
                     }
                  else if (is_low_surrogate(u1))
                     {
                       // a LONE low surrogate, with no preceding high
                       // surrogate -- the mirror image of the
                       // is_high_surrogate(u1) check above (which
                       // already requires a FOLLOWING low surrogate).
                       // This used to fall through to the "normal
                       // \uUUUU" case below and be silently accepted
                       // as an ordinary (unpaired) code point. See
                       // Bugs27 #59(h).
                       //
                       MORE_ERROR() << "⎕JSON B: lone low surrogate "
                                       "at " << b << "↓B";
                       DOMAIN_ERROR;
                     }
                  else   // u1 is not a surrogate (normal \uUUUU)
                     {
                       b += 5;
                     }
                }
             else                     // 1-character escape
                {
                  // RFC 8259 permits exactly \" \\ \/ \b \f \n \r \t (plus
                  // \uXXXX, handled above); anything else was silently
                  // accepted and decoded as the literal character itself
                  // (Blake McBride, Bugs21 #4a) -- reject it instead, so
                  // this validator matches the care already taken over
                  // \uXXXX just above.
                  //
                  switch (safe_at(b + 1))
                     {
                       case UNI_DOUBLE_QUOTE:
                       case UNI_BACKSLASH:
                       case UNI_SLASH:
                       case Unicode('b'):
                       case Unicode('f'):
                       case Unicode('n'):
                       case Unicode('r'):
                       case Unicode('t'):
                            break;

                       default:
                            MORE_ERROR() << "⎕JSON B: invalid escape sequence \\"
                                         << safe_at(b + 1) << " at " << b << "↓B";
                            DOMAIN_ERROR;
                     }
                  ++b;   // the escaped character
                }
           }
       }

   // end of string reached without seeing "
   //
   MORE_ERROR() <<
   "⎕JSON B: No string end for string starting at " << B0 << "↓B";
   DOMAIN_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
