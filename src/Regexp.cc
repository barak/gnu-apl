/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2017 Elias Mårtenson

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

#include "Workspace.hh"
#include "Regexp.hh"

//════════════════════════════════════════════════════════════════════════════
static const PCRE2_UCHAR32 *
ucs_to_codepoints(const UCS_string & string)
{
int size = string.size();
PCRE2_UCHAR32 * buf = new PCRE2_UCHAR32[size];
    loop(i, size)   buf[i] = string[i];
    return buf;
}
//════════════════════════════════════════════════════════════════════════════
RegexpMatch::RegexpMatch(pcre2_code * code, const UCS_string & B,
                         PCRE2_SIZE start, uint32_t extra_options)
   : matched_B(B)
{
   match_data = pcre2_match_data_create_from_pattern_32(code, NULL);
   if (match_data == 0)   // allocation failure
      {
        MORE_ERROR() << "pcre2_match_data_create_from_pattern_32() failed";
        DOMAIN_ERROR;
      }

   // Bugs28 #98: the pattern is compiled with PCRE2_UTF (Regexp.cc,
   // pcre2_compile_32()), so pcre2_match_32() re-validates the UTF-32
   // encoding of the *entire* subject (0..B.size(), not just from
   // start) on every single call unless told not to -- an O(B.size())
   // cost paid again for every match of a global (⎕RE['g']) search, on
   // top of the actual (cheap) matching work, making a global match
   // quadratic overall once the match count grows with B ('a' ⎕RE['g']
   // 32000⍴'a' took ~370ms; a single, non-global match of the same
   // subject took ~1ms). Every call after the first (start > 0) is
   // continuing a search over this SAME B that a previous call already
   // validated successfully to get this far, so PCRE2_NO_UTF_CHECK is
   // safe there; the first call (start == 0) still validates B in
   // full, so a B holding a genuinely invalid codepoint (Bugs28 #79/#80:
   // GNU APL's Unicode is not restricted to legal Unicode scalars) is
   // still caught as a real PCRE2 error.
   //
const uint32_t options = (start ? PCRE2_NO_UTF_CHECK : 0) | extra_options;

   // an empty B may have no buffer at all, but PCRE2 rejects a NULL
   // subject even with length 0 ("NULL argument passed", Bugs35 #57)
   //
static const PCRE2_UCHAR32 empty_subject[1] = { 0 };
const PCRE2_UCHAR32 * subject = B.size() ? B.raw<PCRE2_UCHAR32>()
                                         : empty_subject;
   match_result = pcre2_match_32(code, subject, B.size(),
                                 start, options, match_data, NULL);
   if (match_result == 0)
      {
        MORE_ERROR() << "Match buffer too small";
        ovector_count = 0;
        ovector = 0;
        pcre2_match_data_free(match_data);   // ~RegexpMatch() will not run
        DOMAIN_ERROR;
      }
    else if (match_result > 0)
      {
        ovector = pcre2_get_ovector_pointer_32(match_data);
        ovector_count = pcre2_get_ovector_count_32(match_data);
      }
    else if (match_result == PCRE2_ERROR_NOMATCH)
      {
        // PCRE2_ERROR_NOMATCH is an entirely ordinary "the pattern did
        // not match" outcome, not an error -- ⎕RE (without the E flag)
        // returns ⍬ for it without raising anything. Setting )MORE text
        // here left every subsequent (successful!) statement's )MORE
        // claiming a bogus "libpcre error: no match", clobbering any
        // real )MORE text from an actual, earlier error (Blake McBride,
        // Bugs20 #7).
        //
        ovector_count = 0;
        ovector = 0;
      }
    else   // match_result < 0, and not PCRE2_ERROR_NOMATCH: a genuine
           // PCRE runtime error (e.g. match limit exceeded), not "no
           // match" -- returning ⍬ for this the same way as an ordinary
           // non-match left the caller with no way to tell the two apart
           // (Blake McBride, Bugs28 #72). DOMAIN ERROR instead, matching
           // the allocation-failure and buffer-too-small cases above.
      {
        MORE_ERROR() << "libpcre error: "
                     << Regexp::pcre_error(match_result);
        ovector_count = 0;
        ovector = 0;
        pcre2_match_data_free(match_data);   // ~RegexpMatch() will not run
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
RegexpMatch::~RegexpMatch()
{
   pcre2_match_data_free(match_data);
}
//────────────────────────────────────────────────────────────────────────────
bool
RegexpMatch::is_match() const
{
   return match_result > 0;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
RegexpMatch::matched_string() const
{
const PCRE2_SIZE * ovector = get_ovector();
UCS_string result(matched_B, ovector[0], ovector[1] - ovector[0]);
   return result;
}
//────────────────────────────────────────────────────────────────────────────
int
RegexpMatch::num_matches() const
{
   if (match_result < 0)
      {
        MORE_ERROR() << "Attempt to call num_matches without matches";
        FIXME;
      }

    return match_result;
}
//════════════════════════════════════════════════════════════════════════════
Regexp::Regexp(const UCS_string & pattern, int flags)
{
const PCRE2_UCHAR32 * pattern_ucs = ucs_to_codepoints(pattern);

int error_code = 0;
PCRE2_SIZE error_offset = -1;

   code = pcre2_compile_32(pattern_ucs, pattern.size(),
                           PCRE2_UTF | PCRE2_UCP | flags, &error_code,
                           &error_offset, 0);
   delete[] pattern_ucs;

   if (code)   // OK
      {
        compute_group_parents(pattern, flags);
        return;
      }

   MORE_ERROR() << 
"in (say) Z←A ⎕RE B: error when compiling the regular expression A.\n"
"    The cause of the error was: " << pcre_error(error_code) << ".\n"
"    The regular expression A was: '" << pattern << "'\n"
"    The error was detected at " << int(error_offset) << "↓A.\n"
"    the offending rest (" << (int)error_offset << "↓A) of A was: '"
   << UCS_string(pattern, error_offset) << "'";

   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
Regexp::compute_group_parents(const UCS_string & pattern, int flags)
{
   // the structure of the ⎕RE result is the structure of the capturing
   // groups in the pattern (apl.texi "Complex Matches"), not of the spans
   // of one match (Bugs38 #55). group_parents[g] is the innermost capturing
   // group that contains group g (0 = the whole match).
   //
   // The scanner skips escapes, character classes (including POSIX items
   // like [:space:]), comments (also # comments in extended mode, see
   // (?x) and ⎕RE['x']), and the condition of a conditional group like
   // (?(2)...|...), so that no ( or ) in them is taken for a group
   // (Bugs39 #28). Likewise backtracking verbs like (*MARK:name), callouts
   // like (?C"text"), and \Q...\E in a character class; with option n (no
   // auto capture) a plain ( does not capture (Bugs40 #89).
   //
std::vector<int> parents(1, -1);   // group 0: the whole match
std::vector<int> open;             // the groups of the open ( (or -1)
std::vector<bool> open_extended;   // the extended mode outside each open (
std::vector<bool> open_no_auto;    // the no-auto-capture mode outside it
bool extended = (flags & PCRE2_EXTENDED) != 0;
bool no_auto = (flags & PCRE2_NO_AUTO_CAPTURE) != 0;
const ShapeItem len = pattern.size();
   for (ShapeItem p = 0; p < len; ++p)
       {
         const Unicode uni = pattern[p];
         if (uni == UNI_BACKSLASH)
            {
              if (p + 1 < len && pattern[p + 1] == UNI_Q)   // \Q ... \E
                 {
                   for (p += 2; p + 1 < len; ++p)
                       if (pattern[p] == UNI_BACKSLASH &&
                           pattern[p + 1] == UNI_E)   { ++p;   break; }
                 }
              else if (p + 1 < len && pattern[p + 1] == Unicode('c'))
                 p += 2;   // \cX: a control character, e.g. \c(
              else ++p;   // skip the escaped character
              continue;
            }

         if (extended && uni == UNI_NUMBER_SIGN)   // # comment
            {
              while (p < len && pattern[p] != UNI_LF)   ++p;
              continue;
            }

         if (uni == UNI_L_BRACK)   // character class [...]
            {
              ++p;
              if (p < len && pattern[p] == UNI_CIRCUMFLEX)   ++p;
              if (p < len && pattern[p] == UNI_R_BRACK)      ++p;   // []...]
              for (; p < len && pattern[p] != UNI_R_BRACK; ++p)
                  {
                    if (pattern[p] == UNI_BACKSLASH && p + 1 < len &&
                        pattern[p + 1] == UNI_Q)   // \Q ... \E in a class
                       {
                         for (p += 2; p < len; ++p)
                             if (pattern[p] == UNI_BACKSLASH && p + 1 < len &&
                                 pattern[p + 1] == UNI_E)   { ++p;   break; }
                         continue;
                       }
                    if (pattern[p] == UNI_BACKSLASH)   { ++p;   continue; }
                    if (pattern[p] == UNI_L_BRACK && p + 1 < len &&
                        (pattern[p + 1] == UNI_COLON ||   // [:alpha:]
                         pattern[p + 1] == UNI_FULLSTOP ||
                         pattern[p + 1] == UNI_EQUAL))
                       {
                         const Unicode delim = pattern[p + 1];
                         for (ShapeItem q = p + 2; q + 1 < len; ++q)
                             {
                               if (pattern[q] == delim &&
                                   pattern[q + 1] == UNI_R_BRACK)
                                  { p = q + 1;   break; }
                             }
                       }
                  }
              continue;
            }

         if (uni == UNI_L_PARENT)
            {
              const Unicode n1 = p + 1 < len ? pattern[p + 1] : Unicode_0;
              const Unicode n2 = p + 2 < len ? pattern[p + 2] : Unicode_0;
              const Unicode n3 = p + 3 < len ? pattern[p + 3] : Unicode_0;
              if (n1 == UNI_QUESTION && n2 == UNI_NUMBER_SIGN)   // (?# ...)
                 {
                   while (p < len && pattern[p] != UNI_R_PARENT)   ++p;
                   continue;
                 }

              // a backtracking verb or a start-of-pattern option, e.g.
              // (*MARK:name), (*:name), (*UTF): no group, and its name may
              // contain ( or ). The alpha assertions like (*pla:...) are
              // lower case and contain a sub-pattern.
              //
              if (n1 == UNI_ASTERISK &&
                  ((n2 >= 'A' && n2 <= 'Z') || n2 == UNI_COLON))
                 {
                   while (p < len && pattern[p] != UNI_R_PARENT)   ++p;
                   continue;
                 }

              // a callout (?C), (?Cn), or (?C"text") with the delimiters
              // " ' ` ^ % # $ (doubled inside the text) or {...}: no group
              //
              if (n1 == UNI_QUESTION && n2 == Unicode('C'))
                 {
                   ShapeItem q = p + 3;
                   const Unicode d = q < len ? pattern[q] : Unicode_0;
                   if (d == UNI_L_CURLY)   // a doubled }} is a } (Bugs41 #79)
                      {
                        for (++q; q < len; ++q)
                            {
                              if (pattern[q] != UNI_R_CURLY)   continue;
                              if (q + 1 < len && pattern[q + 1] == UNI_R_CURLY)
                                 ++q;
                              else   { ++q;   break; }
                            }
                      }
                   else if (d == UNI_DOUBLE_QUOTE || d == UNI_SINGLE_QUOTE ||
                            d == Unicode('`') || d == UNI_CIRCUMFLEX ||
                            d == Unicode('%') || d == UNI_NUMBER_SIGN ||
                            d == Unicode('$'))
                      {
                        for (++q; q < len; ++q)
                            {
                              if (pattern[q] != d)   continue;
                              if (q + 1 < len && pattern[q + 1] == d)   ++q;
                              else   { ++q;   break; }
                            }
                      }
                   while (q < len && pattern[q] != UNI_R_PARENT)   ++q;
                   p = q;
                   continue;
                 }

              // option settings, e.g. (?x) or (?i-x:...)
              //
              bool opt_extended = extended;
              bool opt_no_auto = no_auto;
              bool is_option = false;
              ShapeItem opt_end = p + 2;
              if (n1 == UNI_QUESTION)
                 {
                   bool negative = false;
                   for (; opt_end < len; ++opt_end)
                       {
                         const Unicode o = pattern[opt_end];
                         if (o == UNI_MINUS)   { negative = true;   continue; }
                         if (o == Unicode('x'))
                            { opt_extended = !negative;   continue; }
                         if (o == Unicode('n'))
                            { opt_no_auto = !negative;   continue; }
                         if (o == UNI_CIRCUMFLEX)   { opt_extended = false;
                                                      opt_no_auto = false;
                                                      continue; }
                         if ((o >= 'a' && o <= 'z') || (o >= 'A' && o <= 'Z'))
                            continue;
                         break;
                       }
                   is_option = opt_end > p + 2 && opt_end < len &&
                               (pattern[opt_end] == UNI_R_PARENT ||
                                pattern[opt_end] == UNI_COLON);
                 }

              if (is_option && pattern[opt_end] == UNI_R_PARENT)   // (?x)
                 {
                   extended = opt_extended;   // until the end of the group
                   no_auto = opt_no_auto;
                   p = opt_end;
                   continue;
                 }

              bool capturing = n1 != UNI_QUESTION && n1 != UNI_ASTERISK &&
                               !no_auto;
              if (n1 == UNI_QUESTION)   // named groups capture
                 {
                   if (n2 == UNI_SINGLE_QUOTE)   capturing = true;
                   if (n2 == UNI_P && n3 == UNI_LESS)   capturing = true;
                   if (n2 == UNI_LESS && n3 != UNI_EQUAL &&
                       n3 != UNI_EXCLAM)   capturing = true;
                 }

              open_extended.push_back(extended);
              open_no_auto.push_back(no_auto);
              if (capturing)
                 {
                   int parent = 0;
                   for (ShapeItem o = ShapeItem(open.size()) - 1; o >= 0; --o)
                       if (open[o] > 0)   { parent = open[o];   break; }
                   open.push_back(parents.size());
                   parents.push_back(parent);
                 }
              else
                 {
                   open.push_back(-1);
                 }

              if (is_option)   // (?x: ...)
                 {
                   extended = opt_extended;
                   no_auto = opt_no_auto;
                   p = opt_end;
                 }
              else if (n1 == UNI_QUESTION && n2 == UNI_L_PARENT)   // (?(
                 {
                   // the condition of a conditional group, e.g. (2), (<n>),
                   // or (R): skip it. An assertion condition (?(?=...)...)
                   // is an ordinary (non-capturing) group.
                   //
                   if (n3 != UNI_QUESTION && n3 != UNI_ASTERISK)
                      {
                        ShapeItem q = p + 3;
                        while (q < len && pattern[q] != UNI_R_PARENT)   ++q;
                        p = q;
                      }
                   else
                      {
                        p += 1;   // the next ++p is the condition's (
                      }
                 }
              continue;
            }

         if (uni == UNI_R_PARENT && open.size())
            {
              open.pop_back();
              extended = open_extended.back();
              open_extended.pop_back();
              no_auto = open_no_auto.back();
              open_no_auto.pop_back();
            }
       }

   // use the result only if it agrees with PCRE2 (e.g. not for (?|...),
   // where several groups share a number)
   //
   if (int(parents.size()) == expression_count() + 1)   group_parents = parents;
}
//────────────────────────────────────────────────────────────────────────────
Regexp::~Regexp()
{
   pcre2_code_free(code);
}
//────────────────────────────────────────────────────────────────────────────
int
Regexp::expression_count() const
{
uint32_t result;
    pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &result);

    return result;
}
//────────────────────────────────────────────────────────────────────────────
const RegexpMatch *
Regexp::match(const UCS_string &match, PCRE2_SIZE size) const
{
   return new RegexpMatch(code, match, size);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Regexp::pcre_error(int ec)
{
union _u_pcre
{
  PCRE2_UCHAR32 pbuff[256];   // proposed max: 120 code units
  Unicode       ubuff[256];
} buffer;
const int len = pcre2_get_error_message_32(ec, buffer.pbuff, 256);
   if (len > 0)   return UCS_string(buffer.ubuff, len);

UCS_string ret(U"Unknown libpcre error ");
   return ret << ec;
}
//════════════════════════════════════════════════════════════════════════════
