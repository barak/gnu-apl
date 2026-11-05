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

#include "Bif_F12_ENCODE_DECODE.hh"
#include "ComplexCell.hh"
#include "ConstCell_P.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "ScalarFunction.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F12_ENCODE    Bif_F12_ENCODE   ::fun;    // ⊤
Bif_F12_DECODE    Bif_F12_DECODE   ::fun;    // ⊥

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_ENCODE::eval_AB(cValue_R A, cValue_R B) const
{
   // A⊤B: every number in B is represented in a number system with
   // radix A. ⍴Z ←→ (⍴A, ⍴B) and Z[...;...b...] = A⊤B[...b...]

   if (A.is_scalar())   return Bif_F12_STILE::fun.eval_AB(A, B);

const ShapeItem ec_A = A.element_count();
const ShapeItem ec_B = B.element_count();
const Shape shape_Z = A.get_shape() + B.get_shape();
Value_P Z(shape_Z, LOC);

   if (ec_A == 0 || ec_B == 0)   // empty A or B
      {
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

const ShapeItem aL = A.get_shape_item(0);    // first (LSB) dimension of A
const ShapeItem aH = ec_A/aL;                 // remaining (MSB) dimensions

const double qct = Workspace::get_CT();

ConstRavel_P iA(A, true);
   loop(a, aH)
      {
        // find largest Celltype in A, starting with CT_INT and maybe
        // "increasing" it to CT_FLOAT or CT_COMPLEX as needed.
        //
        CellType ct_a = CT_INT;
        loop(h, aL)
            {
              const CellType ct = A.get_cell_type(a + h*aH);
              if (ct == CT_INT)            ;
              else if (ct == CT_FLOAT)     { if (ct_a == CT_INT)  ct_a = ct; }
              else if (ct == CT_COMPLEX)   ct_a = CT_COMPLEX;
              else
                 {
                   MORE_ERROR() << "A⊤B: A["
                                << (a + h*aH + Workspace::get_IO())
                                << "] is not numeric";
                   DOMAIN_ERROR;
                 }
            }

        for (ConstRavel_P iB(B, true); +iB; ++iB)
            {
              CellType ct = ct_a;
              const CellType ct_b = B.get_cell_type(iB());
              if (ct_b == CT_INT)            ;
              else if (ct_b == CT_FLOAT)     { if (ct == CT_INT)  ct = ct_b; }
              else if (ct_b == CT_COMPLEX)   ct = CT_COMPLEX;
              else
                 {
                   MORE_ERROR() << "A⊤B: B[" << (iB() + Workspace::get_IO())
                                << "] is not numeric";
                   DOMAIN_ERROR;
                 }

              if (ct == CT_INT)          // A and B are both integer
                 encode_Int(*Z, aL, aH, iA, iB);
              else if (ct == CT_FLOAT)   // A and B are not APL_Complex
                 encode_Flt(*Z, aL, aH, iA, iB, qct);
              else                       // A or B contain APL_Complex
                 encode_Cpx(*Z, aL, aH, iA, iB, qct);
            }
         ++iA;
       }

   Z->set_default(B, LOC);

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_ENCODE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   // A ⊤[X] B  ←→ (X⍴A)⊤B   for X > 0, or
   //              (Q⍴A)⊤B   for X = 0 and Q computed from B
   //
const APL_Integer A0 = A.get_sole_integer();   // may throw RANK_ERROR or LENGTH_ERROR

   /// radix 0 means that an item of of B overflows entirely into its leading element
   /// (which is the item itself).
   if (A0 == 0)   return Token(TOK_APL_VALUE1, CLONE(&B, LOC));

APL_Integer X0 = X.get_sole_integer();   // may throw RANK_ERROR or LENGTH_ERROR
   if (X0 < 0)
      {
        MORE_ERROR() << "A⊤[X]B: X = " << X0 << " is negative";
        DOMAIN_ERROR;
      }

   if (X0 == 0)   X0 = get_X0(A0, B);   // compute X0 from B

Value_P new_A(X0, LOC);
   loop(x, X0)   new_A->next_ravel_Int(A0);
   new_A->check_value(LOC);
   return eval_AB(*new_A, B);
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_ENCODE::encode_Cpx(Value & Z, ShapeItem aL, ShapeItem aH,
                           const ConstRavel_P & iA,
                           const ConstRavel_P & iB, double qct)
{
const ShapeItem dZ = aH*iB.get_length();
Cell * cZ = &Z.get_wravel(iB()
          + iA()*iB.get_length())   // = the current Z
          + aL*dZ;                  // + the size of Z

   // work downwards from the higher indices (see encode_Int())...
   //
ShapeItem idxA = iA() + iA.get_length();      // the end of A (+1)

APL_Complex bc = iB->get_complex_value();   // the value being decoded
   loop(a, aL)
       {
         idxA -= aH;
         cZ -= dZ;

        Cell cache;
        const Cell & cellA = iA.get_owner().get_cravel(idxA, cache);
        const ComplexCell cC(bc);
        // latent (A is validated as numeric before this loop runs, so
        // bif_residue() cannot actually fail here), propagated while
        // the discarded-ErrorCode pattern is being cleaned up
        // (Blake McBride, Bugs22 #3 appendix).
        const ErrorCode ec = cC.bif_residue(cZ, &cellA);
        if (ec != E_NO_ERROR)   throw_apl_error(ec, LOC);

        if (cellA.is_near_zero())
           {
             bc = APL_Complex(0, 0);
           }
         else
           {
             bc -= cZ->get_complex_value();
             bc /= cellA.get_complex_value();
           }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_ENCODE::encode_Flt(Value & Z, ShapeItem aL, ShapeItem aH,
                           const ConstRavel_P & iA,
                           const ConstRavel_P & iB, double qct)
{
const ShapeItem dZ = aH*iB.get_length();
Cell * cZ = &Z.get_wravel(iB()
          + iA()*iB.get_length())   // = the current Z
          + aL*dZ;                  // + the size of Z

ShapeItem idxA = iA() + iA.get_length();      // the end of A (+1)

   // work downwards from the higher indices (see encode_Int())...
   //
APL_Float bf = iB->get_real_value();   // the value being decoded
   loop(a, aL)
       {
         idxA -= aH;
         cZ -= dZ;

        Cell cache;
        const Cell & cellA = iA.get_owner().get_cravel(idxA, cache);
        const FloatCell cC(bf);
        // latent (A is validated as numeric before this loop runs, so
        // bif_residue() cannot actually fail here), propagated while
        // the discarded-ErrorCode pattern is being cleaned up
        // (Blake McBride, Bugs22 #3 appendix).
        const ErrorCode ec = cC.bif_residue(cZ, &cellA);
        if (ec != E_NO_ERROR)   throw_apl_error(ec, LOC);

        if (cellA.is_near_zero())
           {
             bf = 0.0;
           }
         else
           {
             bf -= cZ->get_real_value();
             bf /= cellA.get_real_value();
           }
       }
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_ENCODE::encode_Int(Value & Z, ShapeItem aL, ShapeItem aH,
                           const ConstRavel_P & iA, const ConstRavel_P & iB)
{
const ShapeItem dZ = aH*iB.get_length();
Cell * cZ = &Z.get_wravel(iB()
          + iA()*iB.get_length())   // = the current Z (near bottom)
          + aL*dZ;                  // + the size of Z (above top)

ShapeItem idxA = iA() + iA.get_length();      // the end of A (+aH)

   /* unfortunately the less significant weights of the number system base A
      are located at higher indices of A. We therefore work downwards from
      the higher indices of A resp. Z...

            ←     ← ... ← .. ← cA   // pre-decremented by aH
                           ↓
        ┌────┬────┬─────┬────┐
        │ a0 │ a1 │ ... │ aN │ // number base (MSB...LSB)
        └────┴────┴─────┴────┘
           ↓    ↓    ↓     ↓
          bi ← bi ← ... ← bi   // divided by ai
           ↓    ↓          ↓
        ┌────┬────┬─────┬────┐
        │ z0 │ z1 │ ... │ zN │ // remainders
        └────┴────┴─────┴────┘
           ↑    ↑    ↑     ↑
            ←     ← ... ← .. ← cZ   // pre-decremented by dZ
    */

APL_Integer bi = iB->get_int_value();   // the value being decoded
   loop(a, aL)
       {
         idxA -= aH;
         cZ -= dZ;

        Cell cache;
        const Cell & cellA = iA.get_owner().get_cravel(idxA, cache);
        const IntCell cC(bi);
        // latent (A is validated as numeric before this loop runs, so
        // bif_residue() cannot actually fail here), propagated while
        // the discarded-ErrorCode pattern is being cleaned up
        // (Blake McBride, Bugs22 #3 appendix).
        const ErrorCode ec = cC.bif_residue(cZ, &cellA);
        if (ec != E_NO_ERROR)   throw_apl_error(ec, LOC);

        if (cellA.get_int_value() == 0)
           {
             bi = 0;
           }
         else if (cellA.get_int_value() == -1)
           {
             // X|¯1 (the residue computed above) is always 0, so bi is
             // unchanged by the subtraction step here -- no precision
             // hazard in this branch (see the else branch below for that).
             //
             // bi /= -1 traps (hardware #DE / SIGFPE) for the single
             // combination bi == INT64_MIN, since -INT64_MIN is not
             // representable as APL_Integer (confirmed directly:
             // (,¯1) ⊤ ¯9223372036854775808 crashes the interpreter).
             // Negate via unsigned wraparound instead of dividing --
             // well-defined, and reproduces two's-complement -x for every
             // representable x, INT64_MIN included (same on-the-wire result
             // dividing by -1 would give: bi unchanged, the standard
             // fixed-width wraparound also used elsewhere in encode/decode).
             bi = APL_Integer(0 - uint64_t(bi));
           }
         else
           {
             // bi - residue can need up to ~65 bits (cZ has the sign of
             // the radix a, so for a large |bi| and a large negative a
             // the difference can approach 2×INT64_MAX), which used to
             // be handled by widening to __int128 before dividing.
             // __int128 is a GCC/Clang extension unavailable on 32-bit
             // targets though (reported: compile failure on 32-bit
             // Debian), so avoid the wide intermediate entirely instead:
             // IntCell::bif_residue_ii() defines cZ with the same sign
             // as a (the standard floored-division remainder
             // convention), which means (bi - cZ)/a is exactly
             // ⌊bi ÷ a⌋ under that same convention -- computable
             // directly from bi and a via ordinary 64-bit truncating /
             // and % plus the textbook trunc-to-floor correction, with
             // no wide type needed on any platform. a==0 was excluded
             // above and a==-1 is excluded by this branch, so neither
             // bi/a nor bi%a can hit the INT64_MIN/-1 trap.
             const APL_Integer a = cellA.get_int_value();
             const APL_Integer trunc_q = bi / a;
             const APL_Integer trunc_r = bi % a;
             bi = trunc_q - ((trunc_r != 0 && ((trunc_r < 0) != (a < 0)))
                              ? 1 : 0);
           }
       }
}
//────────────────────────────────────────────────────────────────────────────
int
Bif_F12_ENCODE::get_X0(APL_Integer A0, const cValue & B)
{
   // X0 == 0.   // compute X0 from B
   //
int64_t min_B = 0x7FFFFFFFFFFFFFFF;   // smallest item in B
int64_t max_B = 0x8000000000000000;   // largest item in B
const RavelType rt = B.get_ravel_type();
   if (rt == RPT_CELLS)
      { loop(b, B.element_count())
           {
             Cell cache;
             const Cell & cell = B.get_cravel(b, cache);
             if (cell.is_integer_cell())
                {
                  const APL_Integer value = cell.get_int_value();
                  if (min_B > value)   min_B = value;
                  if (max_B < value)   max_B = value;
                }
             else if (cell.is_float_cell())
                {
                  const APL_Integer value = cell.get_near_int();
                  if (min_B > value)   min_B = value;
                  if (max_B < value)   max_B = value;
                }
             else if (cell.is_complex_cell())
                {
                  const FloatCell real_cell(cell.get_real_value());
                  const FloatCell imag_cell(cell.get_imag_value());
                  if (!(real_cell.is_near_int64_t() && imag_cell.is_near_int64_t()))
                     {
                       MORE_ERROR() << "A⊤[X]B: complex number " << cell
                                    << " in B is not near int.";
                       DOMAIN_ERROR;
                     }
                  const APL_Integer real_value = real_cell.get_near_int();
                  if (min_B > real_value)   min_B = real_value;
                  if (max_B < real_value)   max_B = real_value;
                  const APL_Integer imag_value = imag_cell.get_near_int();
                  if (min_B > imag_value)   min_B = imag_value;
                  if (max_B < imag_value)   max_B = imag_value;
                }
             else
                { MORE_ERROR() << "A⊤[X]B: invalid Cell type in B";
                  DOMAIN_ERROR; }
           }
      }
   else if (rt & RPT_integer)   // RPT_INT64 or RPT_BOOL
      { loop(b, B.element_count())
           {
             const APL_Integer value = B.get_int_value(b);
             if (min_B > value)   min_B = value;
             if (max_B < value)   max_B = value;
           }
      }
   else if (rt == RPT_FLOAT64)
      { loop(b, B.element_count())
           {
             const APL_Integer value = B.get_near_int(b);
             if (min_B > value)   min_B = value;
             if (max_B < value)   max_B = value;
           }
      }
   else
      { MORE_ERROR() << "A⊤[X]B: invalid Cell type in B"; DOMAIN_ERROR; }

const uint64_t abs_A0 = A0 < 0 ? -A0 : A0;

   // abs_A0 == 1: every subsequent min_V/max_V *= abs_A0 below would stay
   // 1 forever (radix 1 can't represent anything but 0), and the
   // min_N/max_N >= log_A0 guard -- log_A0 == 2^63/1 -- would never trip
   // either, so the loops never terminate. Reject up front instead.
   if (abs_A0 <= 1)
      {
        MORE_ERROR() << "A⊤[X]B: A = " << A0
                     << " cannot represent B (expecting ∣A∣>1) when X=0";
        DOMAIN_ERROR;
      }

const uint64_t log_A0 = 0x8000000000000000 / abs_A0;

uint64_t abs_min_B = min_B;
   if (min_B < 0 && abs_min_B != 0x8000000000000000)   abs_min_B = - min_B;

uint32_t min_N = 0;   // number of digits for abs_min_B
   // the loops below multiply by the signed A0, not abs_A0: for a
   // negative radix the unsigned accumulator wraps huge on the very
   // first multiply and the loop exits after N == 1 -- too few digits.
   for (uint64_t min_V = 1; min_V < abs_min_B; min_V *= abs_A0)
       {
         ++min_N;
         if (min_N >= log_A0)
            {
              MORE_ERROR() << "A⊤[X]B: the smallest item of B needs too"
                              " many digits for radix A = " << A0
                           << " (integer overflow) when X=0";
              DOMAIN_ERROR;
            }
       }

uint64_t abs_max_B = max_B;
   if (max_B < 0 && abs_max_B != 0x8000000000000000)   abs_max_B = - max_B;
uint32_t max_N = 0;   // number of digits for abs_min_B
   for (uint64_t max_V = 1; max_V < abs_max_B; max_V *= abs_A0)
       {
         ++max_N;
         if (max_N >= log_A0)
            {
              MORE_ERROR() << "A⊤[X]B: the largest item of B needs too"
                              " many digits for radix A = " << A0
                           << " (integer overflow) when X=0";
              DOMAIN_ERROR;
            }
       }

const int N = max_N > min_N ? max_N : min_N;

   // if any B is negative, then add a sign digit
   return (min_B < 0) ? N + 1 : N;
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_DECODE::eval_AB(cValue_R A, cValue_R B) const
{
   // ρZ  is: (¯1↓ρA),1↓ρB
   // ρρZ is: (0⌈¯1+ρρA) + (0⌈¯1+ρρB)
   //
const Shape shape_A1 = A.get_shape().without_last_axis();
const Shape shape_B1 = B.get_shape().without_first_axis();

const ShapeItem l_len_A = A.get_rank() ? A.get_last_shape_item() : 1;
const ShapeItem h_len_B = B.get_rank() ? B.get_shape_item(0)     : 1;

const ShapeItem h_len_A = shape_A1.get_volume();
const ShapeItem l_len_B = shape_B1.get_volume();

const Shape shape_Z = shape_A1 + shape_B1;

   if (shape_Z.get_volume() == 0)   // empty result: shape mismatches don't
      {                             // matter since there is nothing to fill
        Value_P Z(shape_Z, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if ((l_len_A != 1) &&       // cannot scalar-extend A, and
       (h_len_B != 1) &&       // cannot scalar-extend B, and
       (l_len_A != h_len_B))   // the lengths of A and B differ
      {
        MORE_ERROR() << "A⊥B: expecting (¯1↑⍴A) = (1↑⍴B) (or one of them"
                        " 1); ¯1↑⍴A is " << l_len_A << ", 1↑⍴B is "
                     << h_len_B;
        LENGTH_ERROR;
      }

Value_P Z(shape_Z, LOC);

ShapeItem idxA = 0;

   loop(h, h_len_A)
       {
         // A[idxA..idxA+l_len_A-1] are used. See if they are complex.
         //
         bool complex_A = false;
         bool integer_A = true;
         loop(aa, l_len_A)
             {
                if (!A.is_near_real(idxA + aa))
                   {
                     complex_A = true;
                     integer_A = false;
                     break;
                   }

                // is_near_int64_t() (Cell-level), not is_near_int(): a
                // radix >= LARGE_INT (e.g. 1E30) is_near_int()==true (by
                // design -- see Cell::is_near_int()'s doc comment), which
                // would route it into decode_int() below; decode_int()'s
                // own overflow-to-decode_real() fallback only ever fires
                // *after* a successful weight multiply, but
                // get_near_int() on a value this size throws DOMAIN_ERROR
                // immediately, before that fallback gets a chance.
                // Classify as non-integer up front instead so it goes
                // straight to decode_real()/decode_complex().
                Cell cache;
                if (!A.get_cravel(idxA + aa, cache).is_near_int64_t())
                   integer_A = false;
             }

         loop(l, l_len_B)
             {
                // B[l], B[l + l_len_B], ... are used. See if they are complex
                //
                bool complex_B = false;
                bool integer_B = true;
                loop(bb, h_len_B)
                    {
                      if (!B.is_near_real(l + bb*l_len_B))
                         {
                           complex_B = true;
                           integer_B = false;
                           break;
                         }

                      // see the integer_A classification above.
                      Cell cache;
                      if (!B.get_cravel(l + bb*l_len_B, cache).is_near_int64_t())
                         integer_B = false;
                    }

               if (integer_A && integer_B)
                  {
                    const bool overflow = decode_int(*Z, l_len_A, A, idxA,
                                                     h_len_B, B, l, l_len_B);
                     if (!overflow)   continue;

                     // otherwise: compute as float
                  }

               if (complex_A || complex_B)
                  decode_complex(*Z, l_len_A, A, idxA,
                                             h_len_B, B, l, l_len_B);
               else
                  decode_real(*Z, l_len_A, A, idxA,
                                          h_len_B, B, l, l_len_B);
             }
         idxA += l_len_A;
       }

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_DECODE::decode_complex(Value & Z, ShapeItem len_A,
                       cValue_R VA, ShapeItem idxA,
                       ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                       ShapeItem dB)
{
const ShapeItem dec_A = len_A == 1 ? 0 : 1;
const ShapeItem dec_B = len_B == 1 ? 0 : dB;
const ShapeItem len = dec_A ? len_A : len_B;

   idxA += dec_A*len_A;    // let idxA point past the lowest weight item in A
   idxB += dec_B*len_B;    // let idxB point past the lowest weight item in B

APL_Complex accu(0.0, 0.0);
APL_Complex weight(1.0, 0.0);
   loop(l, len)
      {
        idxA -= dec_A;
        idxB -= dec_B;
        accu += weight*VB.get_complex_value(idxB);
        weight *= VA.get_complex_value(idxA);
      }

   // Bugs28 #42: a large-radix decode (e.g. 1E300⊥1 1 1, whose Horner
   // accumulation multiplies by 1E300 repeatedly) can overflow to inf
   // -- unlike every scalar-function _ff/_cc helper, nothing here
   // checked isfinite(), so the non-finite value silently entered the
   // workspace instead of raising the same DOMAIN ERROR that e.g.
   // 1E300×1E300 already gets.
   //
   if (!isfinite(accu.real()) || !isfinite(accu.imag()))   DOMAIN_ERROR;
   Z.next_ravel_Number(accu);
}
//────────────────────────────────────────────────────────────────────────────
bool
Bif_F12_DECODE::decode_int(Value & Z, ShapeItem len_A,
                       cValue_R VA, ShapeItem idxA,
                       ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                       ShapeItem dB)
{
   // decode_int() can easily produce an integer overflow. We keep track
   // of that by also computing the final result as double and return
   // true if an integer overflow occurs. The caller shall then call
   // decode_real() instead.
   //
const ShapeItem dec_A = len_A == 1 ? 0 : 1;
const ShapeItem dec_B = len_B == 1 ? 0 : dB;
const ShapeItem len = dec_A ? len_A : len_B;

   idxA += dec_A*len_A;    // let idxA point past the lowest weight item in A
   idxB += dec_B*len_B;    // let idxB point past the lowest weight item in B

APL_Integer value = 0;
APL_Integer weight = 1;

   // exact overflow detection (Bugs28 #82): this used to track a
   // parallel double-precision shadow (value_f/weight_f) purely to
   // decide, via the conservative LARGE_INT/SMALL_INT margin, whether
   // the exact int64_t computation above it could be trusted -- so a
   // decode landing in the top ~0.4% of the int64_t range (e.g.
   // 2⊥63⍴1, i.e. 2^63-1) was declared "overflowed" and recomputed in
   // float even though the exact result fit perfectly well.
   // __builtin_{mul,add}_overflow report overflow of the exact int64_t
   // computation directly, so the float shadow is no longer needed.
   //
   loop(l, len)
      {
        idxA -= dec_A;
        idxB -= dec_B;

        const APL_Integer vB = VB.get_near_int(idxB);
        APL_Integer term;
        if (__builtin_mul_overflow(weight, vB, &term))      return true;
        if (__builtin_add_overflow(value, term, &value))    return true;

        // weight is only needed for a further iteration -- e.g.
        // 2⊥63⍴1 (2^63-1, the exact int64_t maximum) legitimately
        // multiplies weight up to 2^63 on the last iteration, which
        // overflows int64_t even though it is never actually used
        // (skipping this update, like the old float-shadow code's
        // weight_f check did implicitly by only checking weight_f
        // *before* using it on the next iteration, avoids flagging
        // that harmless, unused overflow).
        //
        if (l + 1 < len)
           {
             if (__builtin_mul_overflow(weight, VA.get_near_int(idxA),
                                         &weight))
                return true;
           }
      }

   Z.next_ravel_Int(value);

   return false;   // no overflow
}
//────────────────────────────────────────────────────────────────────────────
void
Bif_F12_DECODE::decode_real(Value & Z, ShapeItem len_A,
                       cValue_R VA, ShapeItem idxA,
                       ShapeItem len_B, cValue_R VB, ShapeItem idxB,
                       ShapeItem dB)
{
const ShapeItem dec_A = len_A == 1 ? 0 : 1;
const ShapeItem dec_B = len_B == 1 ? 0 : dB;
const ShapeItem len = dec_A ? len_A : len_B;

   idxA += dec_A*len_A;    // let idxA point past the lowest weight item in A
   idxB += dec_B*len_B;    // let idxB point past the lowest weight item in B

APL_Float accu   = 0.0;
APL_Float weight = 1.0;
   loop(l, len)
      {
        idxA -= dec_A;
        idxB -= dec_B;
        accu += weight * VB.get_real_value(idxB);
        weight *= VA.get_real_value(idxA);
      }

   // see the matching isfinite() check + comment in decode_complex()
   // above (Bugs28 #42).
   //
   if (!isfinite(accu))   DOMAIN_ERROR;
   Z.next_ravel_Number(accu);
}
//════════════════════════════════════════════════════════════════════════════
