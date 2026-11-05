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

#include "PointerCell.hh"
#include "Quad_RE.hh"
#include "Workspace.hh"

Quad_RE Quad_RE::fun;

#if HAVE_LIBPCRE2_32

# include "Regexp.hh"

//════════════════════════════════════════════════════════════════════════════
Token
Quad_RE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   if (A.get_rank() > 1)   RANK_ERROR;
   if (X.get_rank() > 1)   RANK_ERROR;

   if (!A.is_char_string())
      {
        MORE_ERROR() << "left ⎕RE arguments must be a string value";
        DOMAIN_ERROR;
      }

Flags flags(X.get_UCS_ravel());
Regexp regexp(A.get_UCS_ravel(), flags.get_compflags());

const Shape & shape = B.get_shape();
    // NOTE: a character scalar satisfies is_char_string() (rank <= 1) and
    // is handled correctly by the branch below as a one-element string;
    // there used to be an early rank==0 special case here that returned
    // ⍬ unconditionally, silently bypassing the 'E' (error-on-no-match)
    // flag and making a matching and a non-matching pattern
    // indistinguishable.

    if (B.is_char_string())
       {
         Value_P Z = regex_results(regexp, flags, B.get_UCS_ravel());
         Z->check_value(LOC);
         return Token(TOK_APL_VALUE1, Z);
       }

Value_P Z(shape, LOC);
   for (ShapeItem i = 0 ; i < shape.get_volume() ; i++)
       {
         Cell cache;
         const Cell & cell = B.get_cravel(i, cache);
         Value_P B_sub = cell.to_value(LOC);
         if (!B_sub->is_char_string())
            {
              MORE_ERROR() << "Cell does not contain a string";
              DOMAIN_ERROR;
            }

         Value_P Z_sub = regex_results(regexp, flags, B_sub->get_UCS_ravel());
         Z_sub->check_value(LOC);
         Z->next_ravel_Pointer(Z_sub.get());
       }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}

//────────────────────────────────────────────────────────────────────────────
Quad_RE::Flags::Flags(const UCS_string & flags_string)
   : flags(0),
     error_on_no_match(false),
     global(false),
     result_type(RST_string)
{
int ofcnt = 0;
   loop (f, flags_string.size())
      {
        const Unicode uni = flags_string[f];
        switch(uni)
           {
             case UNI_SUBSET:     result_type = RST_partition;  ++ofcnt;  break;
             case UNI_DOWN_ARROW: result_type = RST_pos_len;    ++ofcnt;  break;
             case UNI_SLASH:      result_type = RST_reduce;     ++ofcnt;  break;
             case UNI_g:          global = true;                         break;
             case UNI_E:          error_on_no_match = true;              break;
             case UNI_i:          flags |= PCRE2_CASELESS;               break;
             case UNI_m:          flags |= PCRE2_MULTILINE;              break;
             case UNI_s:          flags |= PCRE2_DOTALL;                 break;
             case UNI_x:          flags |= PCRE2_EXTENDED;               break;
             default:
                  MORE_ERROR() << "Unknown ⎕RE flag: '" << UCS_string(1, uni)
                               << "'. Valid flags are: Eimsx⊂↓/";
                DOMAIN_ERROR;
           }
     }

   if (ofcnt > 1)
      {
        MORE_ERROR() << "Multiple ⎕RE output flags: '" << flags_string
                     << "'. The ⎕RE output flags are: ⊂↓/";
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
static Value_P
deep_value(int idx, const PCRE2_SIZE * ovector, int count, const int * parents,
           const int * child_count, const UCS_string * B, int real_count)
{
   if (child_count[idx] == 0)   // simple RE (no sub-REs)
      {
        // Bugs10 #7 (Blake McBride): group idx either did not participate
        // in the match (ovector[2*idx] == PCRE2_UNSET, but only readable
        // when idx < real_count -- see below), or was never reached at
        // all (idx >= real_count, e.g. the losing side of a trailing
        // alternation), in which case PCRE2 leaves its ovector slot
        // uninitialized and it must not be read. Either way, emit a
        // placeholder instead of silently omitting the group, which
        // shifted every later group's number down by one.
        if (idx >= real_count || ovector[2*idx] == PCRE2_UNSET)
           {
             if (B)   // string form: empty string
                {
                  Value_P Z(UCS_string(), LOC);
                  Z->check_value(LOC);
                  return Z;
                }
             else     // pos+len form: ¯1 0, i.e. "no match"
                {
                  Value_P Z(2, LOC);
                  Z->next_ravel_Int(-1);
                  Z->next_ravel_Int(0);
                  Z->check_value(LOC);
                  return Z;
                }
           }

        const PCRE2_SIZE start = ovector[2*idx];
        const PCRE2_SIZE end   = ovector[2*idx + 1];
        if (B)   // string
           {
             const UCS_string item(*B, start, end - start);
             Value_P Z(item, LOC);
             Z->check_value(LOC);
             return Z;
           }
        else     // pos+len
           {
             Value_P Z(2, LOC);
             Z->next_ravel_Int(start);
             Z->next_ravel_Int(end - start);
             Z->check_value(LOC);
             return Z;
           }
      }

ShapeItem ini = B ? 1 : 2;
Value_P Z(ini + child_count[idx], LOC);
   if (B)
      {
        const PCRE2_SIZE start = ovector[2*idx];
        const PCRE2_SIZE end = ovector[2*idx + 1];
        const UCS_string item(*B, start, end - start);
        Value_P sub_value(item, LOC);
        sub_value->check_value(LOC);
        Z->next_ravel_Pointer(sub_value.get());
      }
   else
      {
        Z->next_ravel_Int(ovector[2*idx]);
        Z->next_ravel_Int(ovector[2*idx + 1] - ovector[2*idx]);
      }

   loop(ch, count)
       {
         if (parents[ch] != idx)   continue;   // ch is not a child of idx
         Value_P CH = deep_value(ch, ovector, count, parents, child_count,
                                 B, real_count);
         Z->next_ravel_Pointer(CH.get());
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RE::regex_results(const Regexp & A, const Flags & X, const UCS_string & B)
{
ShapeItem B_offset = 0;   // updated by XXX_result() functions

   switch(X.get_result_type())
      {
        case RST_reduce:
        case RST_partition: return partition_result(A, X, B);

        case RST_string:    if (X.get_global())   break;   // continue below
                                return string_result(A, X, B, B_offset);

        case RST_pos_len:   if (X.get_global())   break;   // continue below
                                return index_result (A, X, B, B_offset);

        default:           FIXME;
      }

   /* At this point the result type is RST_string or RST_pos_len, and the global
      flag is set (which means that all matches shall be returned).

      We have not yet called the corresponding string_result() or index_result()
      function (which creates the result), and therefore do not know how long
      Z will become.

      Bugs28 #98: this used to start with a ⍴Z of
      cfg_SHORT_VALUE_LENGTH_WANTED, double ⍴Z (by allocating a new Value_P
      and copying every existing cell across, cell by cell) whenever it
      filled up, and finally shrink ⍴Z to the true number of matches --
      quadratic in the number of matches once B is large enough to need
      several doublings (each doubling re-copies everything seen so far,
      and the *result* of a global match over a repetitive subject grows
      with B itself, so match count and B's length scale together:
      'a' ⎕RE['g'] 2000⍴'a' took ~1ms, 8000⍴'a' ~15ms, 32000⍴'a' ~370ms --
      each doubling of B costing four times the time, not two). Collect
      the matches in a plain std::vector (amortized O(1) push_back, no
      per-match copying of already-collected matches) and build Z once,
      at the real final size, instead.
   */

std::vector<Value_P> matches;

   for (;;)
       {
         Value_P ZZ;
         switch(X.get_result_type())
            {
              case RST_string:
                   ZZ = string_result(A, X, B, B_offset);
                   break;

              case RST_pos_len:
                   ZZ = index_result(A, X, B, B_offset);
                   break;

              default:           FIXME;
            }

         if (B_offset == -1)   break;   // no more matches

         matches.push_back(ZZ);

         // a zero-width match at the end of B moves B_offset beyond B;
         // searching again would be PCRE2_ERROR_BADOFFSET (Bugs35 #57)
         if (B_offset > B.ssize())   break;
       }

Value_P Z(matches.size(), LOC);
   loop(m, matches.size())   Z->next_ravel_Pointer(matches[m].get());
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RE::partition_result(const Regexp & A, const Flags & X,
                          const UCS_string & B)
{
const PCRE2_SIZE len = B.size();
Value_P Z(len, LOC);

PCRE2_SIZE B_offset = 0;
ShapeItem match_id_partition = 1;
ShapeItem match_id_compress  = 1;
bool any_match = false;

ShapeItem & match_id = X.get_result_type() == RST_partition
                     ? match_id_partition : match_id_compress;

   // compress needs a numeric vector like  1 1 1 0 0 0 0 0 1 1 1...
   // partition needs a numeric vector like 1 1 1 2 2 2 2 2 3 3 3...
   // where the ranges of equal numbers belong to the same partition.
   //
   // We call RegexpMatch() multiple times and create a partition (-range)
   // for every match.
   //
PCRE2_SIZE last_end = 0;
   for (; B_offset < len; ++match_id_partition)
       {
         RegexpMatch rem(A.get_code(), B, B_offset);
         if (!rem.is_match())   break;
         any_match = true;

         const PCRE2_SIZE * ovector = rem.get_ovector();
         const PCRE2_SIZE start = ovector[0];
         const PCRE2_SIZE end   = ovector[1];

         // zeros (if any) between the previous and the current partition
         loop(z, start - last_end)   Z->next_ravel_0();
         last_end = end;

         loop (b, end - start)  Z->next_ravel_Int(match_id);

         if (!X.get_global())   break;

         // a zero-width match does not advance end/start; force progress
         // to avoid an infinite loop (standard PCRE2 NOTEMPTY_ATSTART idiom).
         //
         B_offset = (end == start) ? end + 1 : end;
       }

   // this function drives its own (single-call) global search internally,
   // unlike string_result()/index_result() which are called repeatedly by
   // an external driver loop -- so there is no separate "first_call" to
   // track; "no match anywhere in B at all" is the equivalent condition,
   // and E previously had no effect at all here (Blake McBride, Bugs16
   // #7): 'xyz' ⎕RE['E⊂']'abc' and 'xyz' ⎕RE['E/']'abc' silently returned
   // an all-zero result instead of raising, unlike plain 'E' (string
   // result) on the same non-matching B.
   //
   if (!any_match && X.get_error_on_no_match())
      {
        MORE_ERROR() << "No match";
        DOMAIN_ERROR;
      }

   while (Z->more())   Z->next_ravel_0();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RE::string_result(const Regexp & A, const Flags & X,
                       const UCS_string & B, ShapeItem & B_offset)
{
RegexpMatch rem(A.get_code(), B, B_offset);
   if (!rem.is_match())
      {
        // Bugs10 #11 (Blake McBride): B_offset is still its initial 0 iff
        // this is the very first call for this ⎕RE invocation -- the g
        // early-return below is also the loop terminator that
        // Quad_RE::regex_results() uses to end a global search once at
        // least one match was found, but it must not pre-empt the E
        // (error-on-no-match) check on the first call, when there were
        // genuinely no matches at all.
        const bool first_call = (B_offset == 0);
        B_offset = -1;   // indicates an error / end of matches
        if (X.get_global() && !first_call)   return Value_P();
        // Str0(), not Idx0(): this function returns the matching
        // sub-string(s) of B, a character value, so its no-match empty
        // must be a character empty too, or its prototype (and thus
        // fills from ↑/↓ and overtake) behaves as numeric (Blake
        // McBride, Bugs23 #8).
        if (!X.get_error_on_no_match())      return Str0(LOC);
        MORE_ERROR() << "No match";
        DOMAIN_ERROR;
      }

   // rem is a match
   //
const PCRE2_SIZE * ovector = rem.get_ovector();
   B_offset = ovector[1];
   if (0 && rem.num_matches() == 1)   // simple match
      {
        // single string
        //
        Value_P Z(2, LOC);
        const PCRE2_SIZE start = ovector[0];
        const PCRE2_SIZE end   = ovector[1];
        Z->next_ravel_Int(start);
        Z->next_ravel_Int(end - start);
        Z->check_value(LOC);
        return Z;
      }

   // a zero-width match does not advance ovector[1]; force progress to
   // avoid an infinite loop (standard PCRE2 NOTEMPTY_ATSTART idiom).
   //
   if (ovector[1] == ovector[0])   ++B_offset;

// num_matches() (== pcre2_match's own return value, "the number of
// pairs that have been set") tells us how much of ovector is safe to
// read; get_ovector_count() is the ovector's *allocated capacity*
// (sized to the pattern's total capture-group count via
// pcre2_match_data_create_from_pattern, constant for a given pattern
// regardless of which alternative matched). Reading ovector for indices
// at or beyond real_count is undefined -- PCRE2 leaves those slots
// uninitialized -- but the group numbers up to total_count are still
// real capture groups of the pattern and must appear in the result
// (Bugs10 #7, Blake McBride): 'a|b' matched against 'b' has group 1
// (the alternative that didn't match) reported in ovector as [-1,-1]
// (real_count includes it), while 'a|b' matched against 'a' never even
// allocates group 2 in real_count at all (total_count still does).
const uint32_t real_count  = rem.num_matches();
const uint32_t total_count = rem.get_ovector_count();
vector<int> parents(total_count, -1);   // no parents
vector<int> ccount(total_count, 0);     // 0 children

   for (int o = real_count - 1; o >= 0; --o)
       {
         const PCRE2_SIZE ostart = ovector[2*o];
         for (int p = o - 1; p >= 0; --p)
             {
               if (ovector[2*p] <= ostart && ostart < ovector[2*p + 1])
                  {
                    parents[o] = p;
                    ++ccount[p];
                    break;
                  }
             }

         // a non-participating group (ovector[2*o] == PCRE2_UNSET) has
         // no enclosing span for the search above to find, so parents[o]
         // stays -1 -- attach it to the whole match instead of dropping
         // it from the result.
         if (parents[o] == -1 && o > 0)
            {
              parents[o] = 0;
              ++ccount[0];
            }
       }

   // groups beyond real_count were never reached at all during matching
   // (e.g. the losing side of a trailing alternation); their ovector
   // slots are uninitialized, so attach them to the whole match directly
   // without reading ovector.
   for (uint32_t o = real_count; o < total_count; ++o)
       {
         parents[o] = 0;
         ++ccount[0];
       }

   return deep_value(0, ovector, total_count,
                     parents.data(), ccount.data(), &B, real_count);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_RE::index_result(const Regexp & A, const Flags & X,
                      const UCS_string & B, ShapeItem & B_offset)
{
RegexpMatch rem(A.get_code(), B, B_offset);
   if (!rem.is_match())
      {
        // same first_call treatment as string_result() (Bugs10 #11): the
        // g early-return below ends a global search once at least one
        // match was found, but must not pre-empt the E (error-on-no-
        // match) check on the first call, when there were genuinely no
        // matches at all. Previously missing here (Blake McBride, Bugs16
        // #7), so 'gE' silently returned empty while plain 'E' (no g)
        // correctly raised on the very same B.
        //
        const bool first_call = (B_offset == 0);
        B_offset = -1;
        if (X.get_global() && !first_call)   return Value_P();
        if (!X.get_error_on_no_match())      return Idx0(LOC);
        MORE_ERROR() << "No match";
        DOMAIN_ERROR;
      }

   // rem is a match
   //
const PCRE2_SIZE * ovector = rem.get_ovector();
   B_offset = ovector[1];

   // a zero-width match does not advance ovector[1]; force progress to
   // avoid an infinite loop (standard PCRE2 NOTEMPTY_ATSTART idiom).
   //
   if (ovector[1] == ovector[0])   ++B_offset;

// num_matches() (== pcre2_match's own return value, "the number of
// pairs that have been set") tells us how much of ovector is safe to
// read; get_ovector_count() is the ovector's *allocated capacity*
// (sized to the pattern's total capture-group count via
// pcre2_match_data_create_from_pattern, constant for a given pattern
// regardless of which alternative matched). Reading ovector for indices
// at or beyond real_count is undefined -- PCRE2 leaves those slots
// uninitialized -- but the group numbers up to total_count are still
// real capture groups of the pattern and must appear in the result
// (Bugs10 #7, Blake McBride): 'a|b' matched against 'b' has group 1
// (the alternative that didn't match) reported in ovector as [-1,-1]
// (real_count includes it), while 'a|b' matched against 'a' never even
// allocates group 2 in real_count at all (total_count still does).
const uint32_t real_count  = rem.num_matches();
const uint32_t total_count = rem.get_ovector_count();
vector<int> parents(total_count, -1);   // no parent
vector<int> ccount(total_count,   0);   // 0 children

   for (int o = real_count - 1; o >= 0; --o)
       {
         const PCRE2_SIZE ostart = ovector[2*o];
         for (int p = o - 1; p >= 0; --p)
             {
               if (ovector[2*p] <= ostart && ostart < ovector[2*p + 1])
                  {
                    parents[o] = p;
                    ++ccount[p];
                    break;
                  }
             }

         // a non-participating group (ovector[2*o] == PCRE2_UNSET) has
         // no enclosing span for the search above to find, so parents[o]
         // stays -1 -- attach it to the whole match instead of dropping
         // it from the result.
         if (parents[o] == -1 && o > 0)
            {
              parents[o] = 0;
              ++ccount[0];
            }
       }

   // groups beyond real_count were never reached at all during matching
   // (e.g. the losing side of a trailing alternation); their ovector
   // slots are uninitialized, so attach them to the whole match directly
   // without reading ovector.
   for (uint32_t o = real_count; o < total_count; ++o)
       {
         parents[o] = 0;
         ++ccount[0];
       }

   return deep_value(0, ovector, total_count,
                     parents.data(), ccount.data(), 0, real_count);
}
#else // ! HAVE_LIBPCRE2_32

Token
Quad_RE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const char * libs[] = { "libpcre.so",   0 };
const char * hdrs[] = { "pcre2.h",      0 };
const char * pkgs[] = { "libpcre3-dev", 0 };

   return missing_files("⎕RE", libs, hdrs, pkgs);
}
//════════════════════════════════════════════════════════════════════════════

#endif   // HAVE_LIBPCRE2_32
