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

#include "Avec.hh"
#include "Common.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "Output.hh"
#include "Performance.hh"
#include "UTF8_string.hh"

//════════════════════════════════════════════════════════════════════════════
bool
DiffOut::different(const UTF8 * apl, const UTF8 * ref, size_t & pos)
{
   // apl/ref are always NUL-terminated C strings here (built by the test
   // harness from complete in-memory strings, unlike Archive.cc's mmap'd
   // file content), so toUni() without an end bound was already safe in
   // practice -- a NUL byte is never a valid UTF-8 continuation byte, so
   // decoding always stops at or before the terminator regardless. Still,
   // passing the bound costs one strlen() up front and matches the
   // pattern used everywhere else UTF-8 gets decoded (Bugs15 #21's
   // documented-but-not-fixed gap).
   //
const UTF8 * const apl_end = apl + strlen(charP(apl));
const UTF8 * const ref_end = ref + strlen(charP(ref));

   for (pos = 0;; ++pos)   // compare position in ref
       {
         int len_apl;   // length of the UTF8 encoding
         int len_ref;   // length of the UTF8 encoding

         const Unicode a = UTF8_string::toUni(apl, len_apl, true, apl_end);
         const Unicode r = UTF8_string::toUni(ref, len_ref, true, ref_end);
         apl += len_apl;   // skip a in apl
         ref += len_ref;   // skip r in ref

         if (a == r)   // same char
            {
              if (a == 0)   return false;   // end of string: not different
              continue;
            }

         // different chars: try special matches (⁰, ¹, ², ³, ⁴, ⁵, ⁶, or ⁿ)...

         if (r == UNI_DIFF_DIGITS)   // ⁰: match one or more digits
            {
              if (!Avec::is_digit(a))   return true;   // no match

              // match: skip trailing digits
              while (Avec::is_digit(Unicode(*apl)))   ++apl;
              continue;
            }

         if (r == UNI_DIFF_SPACES)   // ¹: match zero or more spaces
            {
              if (a != UNI_SPACE)   return true;   // different

              // skip trailing digits
              while (*apl == ' ')   ++apl;
              continue;
            }

         if (r == UNI_DIFF_REAL)   // ²: match floating point number
            {
              if (!Avec::is_digit(a) &&
                  a != UNI_OVERBAR   &&
                  a != UNI_E   &&
                  a != UNI_J   &&
                  a != UNI_FULLSTOP)   return true;   // different

              // skip trailing digits
              //
              for (;;)
                  {
                    if (Avec::is_digit(Unicode(*apl)) ||
                        *apl == UNI_E           ||
                        *apl == UNI_J           ||
                        *apl == UNI_FULLSTOP)   ++apl;
                   else
                      {
                         int len;
                         if (UTF8_string::toUni(apl, len, true, apl_end)
                             == UNI_OVERBAR)
                            {
                              apl += len;
                              continue;
                            }
                         else break;
                      }
                  }

              continue;
            }

         if (r == UNI_DIFF_ANY)   // ³: match anything
            {
              return false;   // not different
            }

         if (r == UNI_DIFF_OVERBAR)   // ⁴: match optional ¯
            {
              if (a != UNI_OVERBAR)   apl -= len_apl;   // restore apl
              continue;
            }

         if (r == UNI_DIFF_SIGN)   // ⁵: match + or -
            {
              if (a != '+' && a != '-')   return true;   // different
              continue;
            }

         if (r == UNI_DIFF_CR28_29)   // ⁶: match 28 ⎕CR 42
            {
#ifdef cfg_RATIONAL_NUMBERS_WANTED
              if (a != '1')   return true;   // different
#else
              if (a != '0')   return true;   // different
#endif
              continue;
            }
         if (r == UNI_DIFF_MULT)   // ⁿ: optional unit multiplier
            {
              // ⁿ shall match an optional unit (milli, micro, or nano)
              // multiplier i.e. m, n, u, or μ
              //
              if (a == 'm')       continue;
              if (a == 'n')       continue;
              if (a == 'u')       continue;
              if (a == UNI_MUE)   continue;

              // no unit multiplier matches
              apl -= len_apl;
              continue;
            }

         return true;   // different (r does not match a)
       }

   // not reached
   //
   return true;
}
//────────────────────────────────────────────────────────────────────────────
DiffOut::int_type
DiffOut::overflow(DiffOut::int_type c)
{
   // process one output character c from APL. Characters are
   // accumulated in aplout and only written to cout once per line
   // (rather than via "cout << char(c)" here, for every character) --
   // Output::init() sets ios::unitbuf on cout whenever stdout is not a
   // tty, so every "cout << ..." there flushes, i.e. costs a write()
   // syscall; for a long line (no embedded '\n') that was one syscall
   // per character, ~100x slower than necessary (Blake McBride, Bugs6
   // #6). aplout was already used for exactly this line-at-a-time
   // buffering on the (separate) testcase-comparison path below; this
   // just also routes the plain "echo it to cout" path through it.
   //
PERFORMANCE_START(cout_perf)
   Output::set_color_mode(errout ? Output::COLM_UERROR
                                 : Output::COLM_OUTPUT);

   // overflow(EOF) is how ostream::flush()/sync() ask for "flush, nothing
   // to insert" -- unlike CinOut_streambuf/ErrOut_streambuf (Output.cc),
   // c==EOF was never routed to this class's own flush point (that only
   // happens below, on c=='\n', i.e. a real end-of-line -- a live
   // cross-check run confirmed EOF was never observed to reach it), so
   // there is no flush-timing behavior to preserve here: just don't treat
   // it as a real character (it used to be silently appended to aplout as
   // a bogus byte, and would also have miscounted as a UTF continuation
   // byte in the output_column tracking below).
   if (traits_type::eq_int_type(c, traits_type::eof()))
      {
        PERFORMANCE_END(fs_COUT_B, cout_perf, 1)
        return traits_type::not_eof(c);
      }

const char ch = traits_type::to_char_type(c);
   Output::set_column(ch);   // update the output column

   if (ch != '\n')   // not end of line: accumulate only
      {
        aplout += ch;
        PERFORMANCE_END(fs_COUT_B, cout_perf, 1)
        return traits_type::not_eof(c);
      }

   // end of line: emit the accumulated line (plus '\r' first if
   // expand_LF) to cout in one shot.
   //
   if (expand_LF)   cout << "\r";
   cout.write(aplout.c_str(), aplout.size());
   cout << '\n';

   if (!InputFile::is_validating())
      {
        aplout.clear();
        PERFORMANCE_END(fs_COUT_B, cout_perf, 1)
        return 0;
      }

   // complete APL output line received. Compare it with the reference (= the
   // next input line) and write the comparison result into the current
   // .tc.log file.
   //
ofstream & report = IO_Files::get_current_testreport();
   Assert(report.is_open());

const char * apl = aplout.c_str();   // the APL output
UTF8_string ref;                     // the expected output (= the reference)
bool eof = false;
size_t diff_pos = 0;                 // the first mismatch
   IO_Files::read_file_line(ref, eof);
   if (eof)                // nothing in current_testfile
      {
        report << "extra: " << apl << endl;
      }
   else if (different(utf8P(apl), utf8P(ref.c_str()), diff_pos))
      {
        IO_Files::diff_error();
        report << "apl: ⋅⋅⋅" << apl << "⋅⋅⋅" << endl
            << "ref: ⋅⋅⋅" << ref.c_str() << "⋅⋅⋅" << endl
            << " ∆ : ⋅⋅⋅";
        loop(p, diff_pos)   report << " ";
        report << "^" << endl;
      }
   else                    // same
      {
        report << "== " << apl << endl;
      }

   aplout.clear();
   PERFORMANCE_END(fs_COUT_B, cout_perf, 1)
   cout << flush;
   return 0;
}
//════════════════════════════════════════════════════════════════════════════
