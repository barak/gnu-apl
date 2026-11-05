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

    See also file LApack.hh for additional copyright notices.
*/

/*
   The mathematical background for the below is nicely described in:

   "The Householder transformation in numerical linear algebra"
   by: John Kerl February 3, 2008
 */

/** @file
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "Bif_F12_DOMINO.hh"
#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Value.hh"
#include "Workspace.hh"

using namespace std;

#include "LApack.hh"

inline void next_Cell(Value & value, const LA_pack::DD & dd)
   { value.next_ravel_Float(real(dd)); }

inline void next_Cell(Value & value, const LA_pack::ZZ & zz)
   { value.next_ravel_Complex(real(zz), imag(zz)); }

/*
   Notation: in the literature the conjujate transpose of a vector v resp.
   a matrix M is usually (and also in LApack) denoted by v* resp. M*.

   Since Unicode has no superscript-* and to avoid confusion as to
   what * in FORTRAN comments shall mean, we normally use the following
   conventions in this file:

     v° resp. M° to denote the conjugate transposes (v* resp. M* in FORTRAN),
     A∘B for the inner product of A and B (aka. matrix multiplication), and
     A×B for the scalar (component-wise) multiplication of A and B

   ══════════════════════════════════════════════════════════════════════
   THE BIG PICTURE, IN PLAIN ENGLISH
   ══════════════════════════════════════════════════════════════════════

   What A⌹B actually wants: A⌹B (and ⌹B, which is just A⌹B with A = the
   identity matrix) solves the system of linear equations B∘X = A for the
   unknown matrix X (its columns are solved one at a time). If B has more
   rows than columns (more equations than unknowns) there is usually no
   X that satisfies B∘X = A exactly, so instead we compute the X that
   makes B∘X as CLOSE to A as possible (the "least squares" solution,
   the same thing linear regression computes). If B's columns are not
   independent (e.g. one column is a multiple of another, or a matrix
   is otherwise "redundant"/rank-deficient), there are either no such X
   or infinitely many, and A⌹B must detect that and fail with a
   DOMAIN ERROR rather than silently returning garbage.

   Why not just compute inv(B) and multiply? Because B may not be square,
   and because for a badly-conditioned (nearly-redundant) B, explicitly
   inverting it amplifies rounding errors badly. LApack instead avoids
   ever forming an inverse and works with ORTHOGONAL/UNITARY transforms
   (rotations and mirror-reflections) that never change lengths or
   distort the error, only rearrange the numbers. That is the "complete
   orthogonal factorization" strategy explained below, in 3 stages:

   Stage 1 - triangularize B (laqp2, using larfg/larf as its building
   blocks): repeatedly apply a Householder reflector (think: a mirror
   placed just right so that everything below the current diagonal
   element of the current column collapses to 0, without changing the
   column's length) to turn B into an upper triangular matrix R, i.e.
   a matrix that is 0 everywhere below the diagonal. This is the "QR
   factorization" B = Q∘R, with Q the accumulated product of all those
   mirror-reflections (an orthogonal/unitary matrix) and R upper
   triangular. Before each reflection, the remaining column with the
   largest length ("column pivoting") is swapped to the front - this
   both improves numerical stability and, crucially, makes redundant
   (linearly dependent) columns end up at the back of R with tiny
   diagonal entries, which is exactly what makes rank detection (stage
   2) possible without a full, expensive SVD.

   Stage 2 - figure out the rank (estimate_rank, using laic1_MIN/MAX):
   walk down the diagonal of R that stage 1 produced, and after each
   step update a running ESTIMATE of the smallest and largest singular
   values that R would have if we stopped right there (a singular value
   is, loosely, "how much can this matrix stretch or squash a vector" -
   a matrix that is truly redundant/rank deficient has a singular value
   of (numerically near) 0). As soon as the smallest estimate becomes
   too small compared to the largest one (relative to the requested
   tolerance ⎕CT, i.e. RCOND), the columns processed so far are all the
   "independent" information B has - the remaining columns are
   redundant, and the rank is however many columns were processed
   before that happened. This is much cheaper than a real SVD because
   it only ever needs O(N) extra work, not O(N³).

   Stage 3 - solve the triangular system (trsm) and undo the QR
   transforms and the pivoting (unm2r to undo Q, then permute columns
   back into their original order): once B is known to be upper
   triangular (and full rank up to that point), solving R∘X = (Q°∘A) is
   simple back-substitution: start with the last unknown (which only
   involves the last equation) and substitute upward. The Q° (or Q^T
   for real numbers) is applied to the right-hand side A first (with
   unm2r) so it becomes RHS of a plain triangular system; then trsm
   solves it row by row from the bottom up.

   The functions below implement stages 1-3 more or less one-to-one, so
   if you are trying to match this code against the FORTRAN, laqp2 is
   stage 1, estimate_rank/laic1_MIN/laic1_MAX are stage 2, and
   unm2r+trsm are stage 3. larfg/larf/gemv/gerc/ila_lc are small helper
   routines (building and applying a single Householder reflector, and
   the two basic matrix products that reflector application boils down
   to) shared by several of the bigger functions.

   Call trees:

   divide_matrix<T>(Z, A, B)
   │
   └─── gelsy<T>(A, B, rcond)
        │
        └─── scaled_gelsy(A, B, rcond)
             │
             ├─── laqp2<T>(A, pivot, tau)
             │    │
             │    ├─── larfg<T>('Left', X)
             │    └─── larf<T>(v, tau, C)
             │
             ├─── estimate_rank(A, rcond)
             │
             ├─── trsm<T>('Left', 'Upper', 'No transp.' N, A, tau, B)
             │
             └─── unm2r<T>('Left', 'Conj. transp.', N, A, tau, B)
                  │
                  ├─── larf<T>('Left', v, 1, tau, C)
                  ├─── ...
                  └─── larf<T>('Left', v, N, tau, C)

   factorize_matrix(Z, B)
   │
   ├─── laqp2<T>(A, pivot, tau)
   │    │
   │    ├─── larfg<T>('Left', X)
   │    └─── larf<T>(v, len_v, tau, C)
   │
   ├─── grab_Q()
   └─── grab_R ()

   The FORTRAN code of LApack uses a naming convention where the first
   character of a function name indicates the data type (I for integer,
   D for real double, Z for complex double, and some more that are not
   of interest here. For example:

           Complex A/B │ Real A/B │ file names
           ────────────┼──────────┼───────────────────
            ZUNMQR     │ DORMQR   │ zunmqr.f, dormqr.f
            ZUNM2R     │ DORM2R   │ zunm2r.f, dorm2r.f
           ────────────┴──────────┴───────────────────
  
   Other notes/conventions:

   * The FORTRAN function names are UPPERCASE, their filename lowercase.f,
   * The C/C++ functions are lowercase (static members of class LApack),
   * All complex FORTRAN functions start with Z, all real FORTRAN functions
     with D (omitted above and in C/C++).
 */
//════════════════════════════════════════════════════════════════════════════
/* numerical limits
     
   DLAMCH determines double precision machine parameters.
   We (only) #define those that we need. Precision is not crucial
   as long as rounding is done in the right direction.
 */

/// aka. \b eps: relative machine precision. About 1 LSB of 1.0
#define dlamch_E 1.11022e-16

/// base (=2 for binary) * dlamch_E
#define dlamch_P 2.22045e-16

/// aka. \b sfmin: smallest (positive) number with a valid reciprocal
#define dlamch_S 2.22507e-308

static const APL_Float small_number = dlamch_S / dlamch_P;    // 1.00208E¯292
static const APL_Float big_number   = 1.0 / small_number;     // 9.97923E291
static const APL_Float pos_safe_min =  dlamch_S / dlamch_E;   // 2.00417E¯292
static const APL_Float neg_safe_min =  -dlamch_S / dlamch_E;  // -2.00417E¯292
static const APL_Float tol3z        = sqrt(dlamch_E);         // 1.05367E¯8

/// the smallest ⎕CT (aka. RCOND, see divide_matrix() below) that
/// estimate_rank()'s incremental condition estimation can meaningfully
/// use: below machine precision, R's diagonal entries that SHOULD be
/// exactly 0 for a genuinely singular B are indistinguishable from
/// ordinary floating-point rounding noise, so the rank-deficiency test
/// (smax*rcond > smin) can never fire no matter how singular B truly
/// is -- see divide_matrix()'s rcond check.
static const APL_Float min_rcond = dlamch_E;                  // ≈1.11022E¯16, machine epsilon
#undef dlamch_S
#undef dlamch_P

//════════════════════════════════════════════════════════════════════════════

# include "LAdebug.icc"   // print_matrix() etc.

//════════════════════════════════════════════════════════════════════════════
template<typename T>
//════════════════════════════════════════════════════════════════════════════
LA_pack::PTVVy<T>::PTVVy(Ccol N, bool with_pivot)
   : N(N)
{
   tau      = std::allocator<T>{}.allocate(N);
   y        = std::allocator<T>{}.allocate(N);
   work_min = std::allocator<T>{}.allocate(N);
   work_max = std::allocator<T>{}.allocate(N);

   if (with_pivot)   // ⌹B or A⌹B but not ⌹[X]B
      {
        pivot = std::allocator<Ccol>{}.allocate(N);
        vn1   = std::allocator<APL_Float>{}.allocate(N);
        vn2   = std::allocator<APL_Float>{}.allocate(N);

        // init the pivot
        loop(n, N)   pivot[n] = n;
      }
   else              // only ⌹[X]B
      {
        pivot = 0;   // as to figure with_pivot later on
        vn1   = 0;
        vn2   = 0;
      }
}
//────────────────────────────────────────────────────────────────────────────
template<typename T>
LA_pack::PTVVy<T>::~PTVVy()
{
   std::allocator<T>{}.deallocate(tau,      N);
   std::allocator<T>{}.deallocate(y,        N);
   std::allocator<T>{}.deallocate(work_min, N);
   std::allocator<T>{}.deallocate(work_max, N);
   if (pivot)
      {
        std::allocator<Ccol>{}.deallocate(pivot, N);
        std::allocator<APL_Float>{}.deallocate(vn1, N);
        std::allocator<APL_Float>{}.deallocate(vn2, N);
      }
}
//────────────────────────────────────────────────────────────────────────────
template<typename T>
int LA_pack::PTVVy<T>::print_pivot(ostream & out,
                                    Ccol N, const char * loc) const
{
   if (!pivot)   return 0;

   out << "PIVOT:";
   loop(n, N)   out << " " << pivot[n];
   out << "   at " << loc << endl;
   return N;
}
//────────────────────────────────────────────────────────────────────────────
template<typename T>
int LA_pack::PTVVy<T>::print_tau(ostream & out, Ccol N,
                                  const char * loc) const
{
   out << "TAU:";
   loop(n, N)   { out << " ";   print_item(tau[n]); }
   out << endl;
   return N;
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
/* In plain English: laqp2() left behind, in A, a compressed recipe for
   the sequence of Householder mirror-reflections that turned B into R
   (the reflectors themselves, one per column, plus their tau scalars).
   unm2r() replays that recipe, applying each reflector in turn to some
   OTHER matrix C (here: the right-hand side A of A⌹B). This is how Q°
   (conceptually a big M×M matrix) gets applied to the right-hand side
   without ever actually building Q - each reflector application is
   cheap (larf(), see below) and we just chain K of them. Concretely,
   this turns "solve B∘X=A" into "solve R∘X=(Q°∘A)", the right-hand
   side of a system that is now upper triangular and therefore solvable
   by simple back-substitution (see trsm() further down).

   LApack function UNM2R. Overwrite the general complex m-by-n matrix C:

   C ← Q ∘ C         if SIDE = 'L' and TRANS = 'N', or   (case 1)
   C ← Q° ∘ H ∘ C    if SIDE = 'L' and TRANS = 'C', or   (case 2)   <---
   C ← C ∘ Q         if SIDE = 'R' and TRANS = 'N', or   (case 3)
   C ← C ∘ Q° ∘ ×    if SIDE = 'R' and TRANS = 'C',      (case 4)

   where Q is a unitary matrix defined as the product of k
   elementary reflectors:

   Q = H(1) ∘ H(2) ∘...∘ H(k)

   The reflectors are in the lower triangle of A, the diagonal in tau

   NOTE: in LApack, the real version is DORM2R, while
                    the complex version is ZUNM2R.

         We use the name unm2r<T> for both.
 */
template<typename T>
void LA_pack::unm2r(Crow K, const fMatrix<T> & A, const PTVVy<T> & ptvvy,
                    fMatrix<T> & C)
{
   // only case 2 (SIDE == "L" and TRANS = 'C') is needed and implemented.
   // thus NOTRAN is false (and then tau[row] (Q°) is conjugated).
   //
   ALL_DIAS(K)
       {
         /* for row=dia of A; apply H(dia) to SUB = C(dia:m, 1:n)

          ┬        ╔════════════════╗   0   ╔═════════╗
          │        ║ \              ║   ↓   ║         ║
          │        ║  \    A        ║   ↓   ║    C    ║
          │        ║   \            ║   ↓   ║         ║ 
          M   ┬    ╟────⍺           ╫─ dia ─╫─────────╢  ⍺ := 1.0 before
          │   │    ║    │           ║   ↓   ║         ║  restored after
          │ M-dia  ║    │v          ║   K   ║   SUB   ║  v: reflector data,
          │   │    ║    ↓           ║       ║         ║  column dia only
          ┴   ┴    ╚════════════════╝       ╚═════════╝
          */
         const T tau = conjugated(ptvvy.tau[dia]);   // must copy tau[dia] !

         // strictly speaking is A not const. However, we modify it
         // but then restore again. Therefore the const_cast<T &> is OK.
         //
         T & ALPHA = const_cast<T &>(A.diag(dia));
         const T alpha = ALPHA;   // remember A(dia, dia)
             ALPHA = T(1.0);
             fMatrix<T> SUB = C.sub_matrix(dia, 0);
             larf<T>(&A.diag(dia), tau, SUB, ptvvy.y);
         ALPHA = alpha;           // restore A(dia, dia);
       }
}

Value_P LA_pack::invert_DD_UTM(Crow M, Ccol N,
                               fMatrix<DD> & UTM, fMatrix<DD> & AUG)
   { return invert_UTM<DD>(M, N, UTM, AUG); }

Value_P LA_pack::invert_ZZ_UTM(Crow M, Ccol N,
                               fMatrix<ZZ> & UTM, fMatrix<ZZ> & AUG)
   { return invert_UTM<ZZ>(M, N, UTM, AUG); }

//────────────────────────────────────────────────────────────────────────────
/// In plain English: plain Gauss-Jordan elimination, specialized for an
/// upper triangular input QUTM (this is used to invert R itself when the
/// APL programmer explicitly asks ⌹B for the inverse of the R factor,
/// not for the A⌹B/⌹B solve path above, which never needs R⁻¹
/// explicitly - see grab_R() below). QAUG starts as the identity matrix;
/// every row operation that is applied to QUTM (to eventually turn it
/// into the identity matrix too) is mirrored onto QAUG, so that QAUG
/// ends up holding QUTM's inverse. First each row is divided by its own
/// diagonal element (making the diagonal all 1s); then, from the
/// rightmost column backward, each column's above-diagonal entries are
/// eliminated by subtracting an appropriate multiple of the row that
/// has its 1 in that column - by the time this is done, QUTM has become
/// the identity matrix (not that its final value is used - only QAUG,
/// the accumulated effect of the same operations, matters) and QAUG is
/// R⁻¹.
template<typename T>
void LA_pack::invert_QUTM(Ccol N, fMatrix<T> & QUTM, fMatrix<T> & QAUG)
{
print_matrix("  QUTM before invert_QUTM()", QUTM);

   // start with empty qaug. 
   //
   ALL_ROWS(N)
   ALL_COLS(N)   QAUG.at(row, col) = T(0.0);

   // divide every row of UTM by its diagonal element, so that the diagonal
   // elements of UTM become 1.0. Also initialize the diagonal of AUG.
   //
   ALL_ROWS(N)
       {
         const T diag = QUTM.diag(row);
         if (diag == 0.0)
            {
              MORE_ERROR() << "⌹[X]B: 0 on the main diagonal of R";
              DOMAIN_ERROR;
            }

         // divide all items right of UTM[row;row] as well as the entire
         // AUG[row;] by diag. Conceptually the entire diagonal of UTM is
         // set to 1.0. However, we do not use the diagonal of UTM after
         // this loop and therefore don't bother to set it to 1.0.
         //
         for (Ccol col = row + 1; col < N; ++col)   QUTM.AT(row, col) /= diag;

         // divide diagonal items of UTM | AUG by diag_y
         //
         QAUG.diag(row) = 1.0 / diag;
       }

   /* at this point every item right of UTM's diagonal has been divided
      by that row's own (former) diagonal value, so UTM's diagonal is
      CONCEPTUALLY 1.0 (u₍ⱼⱼ₎ below) - but, per the comment above, it was
      never actually overwritten with 1.0 (it still holds the original
      diag value) since nothing reads it again. AUG is zero except on
      its diagonal:

       ┌────UTM────┐         ┌────AUG────┐
       │ (u) u u u │         │ a         │
       │   (u) u u │         │   a    0  │ aⱼⱼ = 1.0 ÷ uⱼⱼ
       │     (u) u │         │     a     │ uⱼⱼ = conceptually 1.0 (not
       │  0    (u) │         │  0    a   │            actually written)
       │        (u)│         │         a │
       └───────────┘         └───────────┘
    */

   // subtract a multiple of the diagonal element from the items above it
   // so that the item becomes 0.0
   //
   REV_COLS(N)   // for every column k
   ALL_ROWS(k)   // for every row above row k
       {
         /* subtract (factor1×k) from row. factor1 is the element of
            row that is above the diagonal item of k. This makes
            at(row, k) == 0.0 and updates the columns right of it
            in both UTM and AUG:

                   0 ←   ← k ← N-1 ──────────── outer loop (row k=N-1:0)
                           ↓
                   ┌────UTM────┐   0    ┌────UTM────┐
                   │ 1 u u u 0 │   ↓    │ 1 u u u 0 │
                   │   1 u ⍺ 0 │← row → │   1 u 0 0 │  ⍺ ← 0
                   │     1 0 0 │   ↓    │     1 0 0 │
                   │  0    1 0 │   k    │  0    1 0 │
                   │         1 │   │    │         1 │
                   └───────────┘   │    └───────────┘
                           ↑       │            ↑
                          col→     │           col→
                                   │
                                   └─────────── inner loop (row=0:k)

            In this double loop, every item ⍺ above the diagonal is
            visited and set to 0, updating UTM and AUG simultaneously.
          */

         const T alpha = QUTM.AT(row, k);
         if (LA_pack::is_zero(alpha))   continue;   // already 0.0

         for (Ccol col = row; col < N; ++col)
             {
               // make QUTM[k;col] zero by subtracting QUTM[y;x] × alpha
               //
               QUTM.at(row, col) -= QUTM.AT(k, col) * alpha;
               QAUG.at(row, col) -= QAUG.AT(k, col) * alpha;
             }
       }
   print_matrix("QAUG after invert_T_UTM()", QAUG);
}
//────────────────────────────────────────────────────────────────────────────
/** In plain English: this is unm2r() again, but instead of applying the
   recipe of reflectors to some other matrix, it applies it to the
   identity matrix, which is precisely how you turn "a recipe for Q" into
   "the actual matrix Q". GNU APL only needs this to hand Q back to the
   APL programmer when they explicitly ask for the QR factorization
   itself (⌹[1]B / ⌹[2]B and friends, see grab_Q() below); the A⌹B /
   ⌹B solve path never needs Q explicitly (it only ever needs Q°∘A,
   which unm2r() computes directly and more cheaply).

   LApack function UNG2R.

   A is a matrix whose upper triangle matrix (including its diagonal) is
   not used (the result of laqp2()). The lower triangle of A contains
   elementary reflecors, and tau[] the diagonal (as returned by larfg()).

   Only K=N is needed and therefore FORTRAN argument K skipped and set to N.
   On exit, A is Q = H(1)∘H(2)∘...∘H(K).

   In FORTRAN: real DORG2R and complex ZUNG2R

   ung2r() is essentially unm2r() applied to the unit matrix
 **/
template<typename T>
void LA_pack::ung2r(fMatrix<T> & A, PTVVy<T> & ptvvy)
{
const Ccol N = A.get_column_count();
const Crow M = A.get_row_count();

// Assert(M >= N);

   // Initialise columns k+1:n to columns of the unit matrix.
   // JSA: We have K=N, therefore nothing to do here.

   /* apply reflector loop along the diagonal of A. Something like:

          0 ← ← ← k ← ← ← N
          ↑  ╔════╪═A═════╗
          ↑  ║ \  │       ║
          ↑  ║  \ │       ║
          ↑  ║   \│       ║
          k  ╟────⍺┌──────╢   ⍺: Aₖₖ = A.diag(k) := 1.0
          ↑  ║     │┌─────╢
          ↑  ║     ││┌────╢
          ↑  ║     │││┌───╢
          ↑  ║     ││││ S ║
          N ─╫─    ││││ U ║
             ║     ││││ B ║
          M  ╚═════╧╧╧╧═══╝
    */
   REV_COLS(N)   // i in FORTRAN comments is k in C++.
      {
        /* Apply reflector H(col) to A(col:m,col:n)
           (aka. (-col, col)↑A) from the left

                   0 ← ← ← k ← ← ← N
                   ↑  ╔═A══╪═══════╗
                   ↑  ║ \  │       ║
                   ↑  ║  \ │       ║
                   ↑  ║   \│       ║
            ┬      k  ╟────⍺┌──────╢   ⍺: Akk = A.diag(k) := 1.0
            │     S_X ║    ▒│      ║   Z: reflector H(k)
            │      ↑  ║    ▒│      ║
          len_X    ↑  ║    X│ SUB  ║
            │      N  ║    ▒│      ║
            │         ║    ▒│      ║
            ┴      M  ╚═════╧══════╝
         */

        const Crow s_X   = k + 1;   // start of X = the row below diag(k)

        // apply reflector H(k) to A...
        //
        A.diag(k) = T(1.0);          // set ⍺ to 1.0
        fMatrix<T> SUB = A.sub_matrix(k, s_X);
        larf<T>(&A.diag(k), ptvvy.tau[k], SUB, ptvvy.y);

        /*
           Update the entire column k of A as follows:

           (a) items above the diagonal:   set to 0.0,
           (b) items on the diagonal:      set to 1.0 - tau[k];
           (c) items below the diagonal:   multiply by -tau[k]
         */

        ALL_ROWS(M)   // for the entire column k
           {
             T & Ar = A.at(row, k);
             if      (row < k)   Ar = T(0.0);                // case (a)
             else if (row > k)   Ar *=      -ptvvy.tau[k];   // case (c)
             else                Ar = T(1.0)-ptvvy.tau[k];   // case (b)
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
sRank LA_pack::divide_DD_matrix(Value & Z, Crow M,
                                Ccol cols_A, cValue_R VA,
                                Ccol cols_B, cValue_R VB)
{
   return divide_matrix<DD>(Z, M, cols_A, VA, cols_B, VB);
}
//────────────────────────────────────────────────────────────────────────────
sRank LA_pack::divide_ZZ_matrix(Value & Z, Crow M,
                                Ccol cols_A, cValue_R VA,
                                Ccol cols_B, cValue_R VB)
{
   return divide_matrix<ZZ>(Z, M, cols_A, VA, cols_B, VB);
}
//────────────────────────────────────────────────────────────────────────────
// DEAD CODE: factorize_DD_matrix()/factorize_ZZ_matrix() are not called
// from anywhere (Bif_F12_DOMINO.cc's ⌹[X]B no longer selects the
// LApack-based QR factorization ALGO_QR_LAPACK; it uses the libgsl-based
// factorizations, or the independent Helzer algorithm, instead). Kept
// disabled (rather than deleted) in case a future ⌹[X]B option wants to
// reconnect this LApack-based QR-factorize-only implementation; see the
// "In plain English" comment on factorize_matrix() further below for
// what it does. grab_Q()/grab_R(), which factorize_matrix() calls, are
// disabled together with it, a few hundred lines further down.
#if 0
// instantiate factorize_matrix<DD>()
void LA_pack::factorize_DD_matrix(Value & Z, Crow M, Ccol N,
                                  cValue_R VB, APL_Float rcond)
{
   factorize_matrix<DD>(Z, M, N, VB, rcond);
}
//────────────────────────────────────────────────────────────────────────────
// instantiate factorize_matrix<ZZ>()
void LA_pack::factorize_ZZ_matrix(Value & Z, Crow M, Ccol N,
                                  cValue_R VB, APL_Float rcond)
{
   factorize_matrix<ZZ>(Z, M, N, VB, rcond);
}
#endif // 0 - factorize_DD_matrix/factorize_ZZ_matrix are dead code
//────────────────────────────────────────────────────────────────────────────
/// In plain English: this is the top-level entry point for A⌹B (and,
/// via a synthetic identity matrix A, for ⌹B too). Its job is mostly
/// bookkeeping/layout conversion: APL stores matrices row-major, LApack
/// (being FORTRAN) expects them column-major, so this function copies
/// one column of VA and the whole of VB into scratch buffers in the
/// layout gelsy() expects, calls gelsy() to solve B∘X = (that one
/// column of A), copies the resulting X back into the result buffer in
/// row-major order, and repeats for every column of VA. Note that this
/// means B is re-copied and completely re-factorized by gelsy() for
/// EVERY column of A - simple, and fine for the modest matrix sizes
/// A⌹B is normally used for, but not the most efficient possible
/// approach (a single QR factorization of B, reused for every column
/// of A, would do less repeated work).
///
/// the implementation of Z←A⌹B, where
/// Z[;j] is the solution of A[;j] = B +.× Z[j]   (if rows == cols_B),
/// or else: A[;j] - B +.× Z[j] is minimal         (if rows > cols_B)
template<typename T>
sRank
LA_pack::divide_matrix(Value & Z, Crow rows,
                       Ccol cols_A, cValue_R VA,
                       Ccol cols_B, cValue_R VB)
{
   // the following has been checked by the caller:
   //
   // rows_B >= cols_B and
   // rows_A == rows_B  (aka. rows)
   //

   // degenerate case: B has no columns (and, per the precondition above,
   // no rows either). Z has shape (cols_B, cols_A) and is therefore
   // trivially empty regardless of cols_A -- skip gelsy() entirely since
   // it asserts on empty (M==0 or N==0) input and there is nothing to solve.
   if (cols_B == 0 || rows == 0)   return cols_B;

const T t0(0.0);
const APL_Float rcond = Workspace::get_CT();

   // Rank-revealing is gelsy()'s job (below, via rcond): it compares
   // singular values, which is the textbook test. An element-magnitude
   // ratio pre-check was tried here (r2085) but is not a valid proxy for
   // conditioning -- a matrix can have an enormous spread of element
   // magnitudes and still be perfectly well-conditioned (Blake McBride,
   // Bugs20 #1) -- so it was removed again.
   //
   // ⎕CT here is not "comparison tolerance" in its usual sense -- it is
   // reused (by design, not by accident) as gelsy()'s RCOND parameter,
   // the standard LAPACK rank-revealing-QR control value (real LAPACK's
   // DGELSY takes the same RCOND, unvalidated, from its caller). RCOND
   // values below machine precision are not valid input to the
   // incremental condition estimate below: R's diagonal can carry
   // rounding noise on the order of machine epsilon even for an exactly
   // singular B (a mathematically-zero pivot does not come out as
   // literal 0.0 after floating-point Householder elimination), so an
   // RCOND that small can never let the rank-deficiency test tell that
   // noise apart from a real singular value -- estimate_rank() then
   // reports full rank regardless of B, and the caller gets a finite
   // but meaningless result instead of the DOMAIN ERROR a singular B
   // should always produce. Reject up front rather than silently
   // substitute a usable value: an invalid RCOND must never be allowed
   // to reach the rank prediction unchallenged, and (per this
   // interpreter's convention) a clear error beats a silently-adjusted
   // value the user has no way to notice.
   if (rcond < min_rcond)
      {
        MORE_ERROR() << "A⌹B: here ⎕CT is used as RCOND (not as an "
                        "ordinary comparison tolerance); its valid range "
                        "for A⌹B/⌹B is ⎕CT≥" << min_rcond << " (≈ "
                        "machine precision). The given ⎕CT=" << rcond
                     << " is below that: floating-point rounding noise "
                        "in the matrix factorization could then no "
                        "longer be told apart from genuine singularity, "
                        "making the rank estimate meaningless. See "
                        "\"The Impact of ⎕CT for ⌹\" in the GNU APL "
                        "manual for details";
        DOMAIN_ERROR;
      }

const size_t items_A      = rows;
const size_t items_B      = rows * cols_B;
const size_t items_result = cols_A * cols_B;

const size_t bytes_A      = items_A      * sizeof(t0);
const size_t bytes_B      = items_B      * sizeof(t0);
const size_t bytes_result = items_result * sizeof(t0);

   // allocate storage for A, B, and result buffer
   //
if (bytes_A > SIZE_MAX - bytes_B)              WS_FULL;
if (bytes_result > SIZE_MAX - bytes_A - bytes_B)   WS_FULL;
T * work_AB = std::allocator<T>{}.allocate(items_A + items_B + items_result);

T * work_A = work_AB;
T * work_B = work_A + items_A;
T * result = work_B + items_B;

   ALL_COLS(cols_A)   // APL columns
      {
        // initialize one column of A...
        //
        // a[] is  one column A[;col] on entry and one column Z[; col] on exit.
        // Therefore a must not be changed.
        //
        T * const a = work_A;
        fMatrix<T> A(a, rows, 1,      /* LDA */ rows);   // rows × 1

        ALL_ROWS(rows)   // APL rows
           {
             const ShapeItem idx_A = row * cols_A + col;
             set_real(a[row], VA.get_real_value(idx_A));
             set_imag(a[row], VA.get_imag_value(idx_A));
           }

        // initialize entire b[] in FORTRAN (aka. column major) order,
        // which is ⍉ APL (aka. row major) order. I.e. use ⍉B.
        //
        T * b = work_B;
        fMatrix<T> B(b, rows, cols_B, /* LDB */ rows);   // rows × cols_B

        ALL_COLS(cols_B)   // APL columns
        ALL_ROWS(rows)     // APL rows
           {
             const ShapeItem idx_B = col + row*cols_B;
             set_real(*b,   VB.get_real_value(idx_B));
             set_imag(*b++, VB.get_imag_value(idx_B));
           }

        const sRank rank = gelsy<T>(A, B, rcond);
        if (rank < cols_B)
           {
             std::allocator<T>{}.deallocate(work_AB, items_A + items_B + items_result);
             return rank;
           }

        // cols_A = rows_Z. Store result for column col in row-major layout.
        //
        ALL_ROWS(cols_B)
           result[row * cols_A + col] = a[row];
      }

   // write Z in row-major order from the result buffer
   ALL_ROWS(cols_B)
   ALL_COLS(cols_A)
      next_Cell(Z, result[row * cols_A + col]);

   std::allocator<T>{}.deallocate(work_AB, items_A + items_B + items_result);
   return cols_B;
}
//════════════════════════════════════════════════════════════════════════════
// DEAD CODE: grab_Q() is only called from factorize_matrix() further
// below, which is itself dead code (see the comment on
// factorize_DD_matrix()/factorize_ZZ_matrix() above). Disabled together
// with factorize_matrix() rather than deleted, for the same reason.
//
// In plain English: it hands the actual orthogonal matrix Q back to the
// caller as an APL value: C on entry holds laqp2()'s compressed
// reflector recipe (see laqp2()'s comment above), and ung2r() (further
// up in this file) turns that recipe into the literal Q matrix in
// place; this function then just copies that into a fresh APL value Z1
// and appends it to the result Z. If B was wide (more columns than
// rows), Q needs padding with identity-matrix rows first so that Q ends
// up genuinely square/orthogonal.
#if 0
template<typename T>
void LA_pack::grab_Q (Value & Z, fMatrix<T> & C, PTVVy<T> & ptvvy)
{

const Crow M = C.get_row_count();
const Ccol N = C.get_column_count();

   if (M < N)   // C is under-determined. Pad it to the unity matrix
      {
        /*   ├────N────┤    ├────N────┤    
           ┬ ╔════C════╗    ╔════C════╗ ┬
           M ║   src   ║ →  ║   dst   ║ │
           ┴ ╚═════════╝    ║     1 0 ║ N  ┬
                            ║     0 1 ║ │ N-M
                            ╚═════════╝ ┴  ┴
                                ├─N-M─┤
         */

        print_matrix("C before expansion in grab_Q()", C);
        C.set_rows(N);   // expand M→N
        T * data = &C.diag(0);

        // this is an inplace copy so we copy downwards from the end
        // to avoid that source items overwritten before they were used.
        REV_ROWS(N)   // j
        REV_COLS(N)   // k
           {
             T & dest =                data[k + N * j];
             if (k < M)         dest = data[k + M * j];   // valid source
             else if (j == k)   dest = T(1.0);            // ID diag
             else               dest = T(0.0);            // ID other
           }
      }

print_matrix("C before ung2r() in grab_Q()", C);
   ung2r<T>(C, ptvvy);
print_matrix("C after ung2r() in grab_Q()", C);

const Ccol min_MN = min(M, N);
const Shape shape_Z1(M, min_MN);   // the orthogonal matrix Q
Value_P Z1(shape_Z1, LOC);

   ALL_ROWS(M)        // APL rows    of Z1
   ALL_COLS(min_MN)   // APL columns of Z1
      {
        next_Cell(*Z1, C.at(row, col));
      }

   Z1->check_value(LOC);
   Z.next_ravel_Pointer(Z1.get());
}
//────────────────────────────────────────────────────────────────────────────
// grab_R() is the R-factor counterpart of grab_Q() above, and dead code
// for the same reason (see the comment on grab_Q()). Still disabled
// together with it (one #if 0 spans both functions).
//
// In plain English: HR on entry is laqp2()'s output (upper triangle = R,
// lower triangle = reflector recipe); this copies just the upper
// triangle (R itself) into a fresh APL value Z2, forcing the
// below-diagonal entries to a clean 0 in the copy along the way (they
// are only conceptually 0 - HR's actual stored values there are the
// reflector data, not real R entries). It then calls invert_UTM() below
// to additionally compute R⁻¹, since APL's ⌹[2]B-style factorization
// results traditionally hand back both a factor and its inverse.
template<typename T>
void LA_pack::grab_R(fMatrix<T> & HR, Value & Z, fMatrix<T> & AUG)
{
print_matrix("HR in grab_R()", HR);

const Crow M = HR.get_row_count();
const Ccol N = HR.get_column_count();

const Crow min_MN = min(M, N);
const Shape shape_Z2(min_MN, N);   // the upper triangular N×N matrix R
Value_P Z2(shape_Z2, LOC);

   // copy the upper triangle of HR to UTM (aka. R)
   //
   ALL_ROWS(min_MN)   // APL rows
   ALL_COLS(N)   // APL columns
      {
        if (row <= col)   // on or above diagonal: HR is valid
           {
             next_Cell(*Z2, HR.at(row, col));
           }
        else
           {
             const T Zero(0.0);
             HR.at(row, col) = Zero;
             next_Cell(*Z2, Zero);
           }
      }

print_matrix("HR in grab_R() (2)", HR);

   // invert UTM into AUG (aka. Ri). Destroys UTM
   //
Value_P Z3 = invert_UTM<T>(M, N, HR, AUG);

   Z2->check_value(LOC);
   Z3->check_value(LOC);

   Z.next_ravel_Pointer(Z2.get());
   Z.next_ravel_Pointer(Z3.get());
   Z.check_value(LOC);
}
#endif // 0 - grab_Q/grab_R are dead code
//────────────────────────────────────────────────────────────────────────────
/** In plain English: a small wrapper that decides how many rows/columns
    of UTM are actually meaningful (an under-specified M<N triangular
    matrix - more unknowns than equations - only has its first M rows
    worth inverting; a "normal" M>=N one has N) and hands that off to
    invert_QUTM() above to do the actual elimination, then packages the
    result as an APL value. This is also called directly from
    Bif_F12_DOMINO.cc's QR_Helzer() to invert the R factor produced by
    the OTHER (non-LApack, Gary Helzer's algorithm) QR factorization
    that GNU APL's ⌹[0]B uses - it is not exclusively part of the
    LApack/gelsy code path.

    invert an upper-triangular matrix. Input is an upper triangular M×N matrix
    utm (whose items below the diagonal are ignored but pretended to be 0).
    Result is the inverse N×M matrix aug.
 **/
template<typename T>
Value_P LA_pack::invert_UTM(Crow M, Ccol N,
                            fMatrix<T> & UTM, fMatrix<T> & AUG)
{
   if (M < N)   // UTM under-specified
      {
        print_matrix("(under-specified) UTM before invert_UTM()", UTM);
        invert_QUTM<T>(M, UTM, AUG);
      }
   else   // M >= N
      {
        print_matrix("  UTM before invert_UTM()", UTM);
        invert_QUTM<T>(N, UTM, AUG);
      }

   // create the result value
   //
const Cdia min_MN = min(M, N);
const Shape shape_Z3(min_MN, min_MN);   // INV has the transposed shape!
Value_P Z3(shape_Z3, LOC);
const bool cplx = is_complex(UTM.diag(0));
   ALL_ROWS(min_MN)   // APL row
   ALL_COLS(min_MN)   // APL col
       {
         if (col < N && col >= row)   // item is on or above diagonal 
            {
              const T & item = AUG.AT(row, col);
              if (!(isfinite(real(item)) && isfinite(imag(item))))
                 {
                   UCS_string & more = MORE_ERROR();
                   more << "non-finite AUG[" << row << ";" << col << "]:";
                   if (!(isfinite(real(item))))
                      more << " real " << real(item);
                   else
                      more << " imag " << imag(item);
                   more << " in " << __FUNCTION__;

                   DOMAIN_ERROR;
                 }
              next_Cell(*Z3, item);
            }
         else if (cplx)   Z3->next_ravel_Complex(0.0, 0.0);
         else             Z3->next_ravel_Float(0.0);
       }

   Z3->check_value(LOC);
   return Z3;
}
//────────────────────────────────────────────────────────────────────────────
/// In plain English: "just give me the QR factorization of B, without
/// solving anything" - the QR-factorization-only counterpart of
/// divide_matrix()/gelsy() above, skipping the parts of gelsy() that
/// exist purely to solve B∘X=A for some A (rank estimation, applying
/// reflectors to a right-hand side, back-substitution). It calls
/// laqp2() once (same as scaled_gelsy() does) and then just hands back
/// Q, R, and R⁻¹ via grab_Q()/grab_R() above.
///
/// NOTE: as of this writing, this function (and the DGELSY/ZGELSY-based
/// "ALGO_QR_LAPACK" it implements) is not reachable from any APL-level
/// primitive - Bif_F12_DOMINO.cc's eval_XB() only ever selects the
/// Helzer algorithm (QR_Helzer(), a different, independent
/// implementation - see Bif_F12_DOMINO.cc/hh) or the libgsl-based
/// factorizations for ⌹[X]B. It is therefore DEAD CODE, disabled below
/// (along with grab_Q()/grab_R(), which only it calls, further up) with
/// #if 0 rather than deleted, in case a future ⌹[X]B option wants to
/// expose it again.
///
/// the implementation of Z←⌹[2]B, where Z is (Q T T⁻¹) and B = T∘Q.
//
#if 0
template<typename T>
sRank LA_pack::factorize_matrix(Value & Z, Crow M, Ccol N,
                               cValue_R VB, APL_Float rcond)
{
   // work sizes...
   //
const ShapeItem max_MN = max(M, N);
const size_t items_B  = M * N;
const size_t items_HR = M * N;
const size_t items_R  = max_MN * max_MN;
const size_t items_Ri = N * M;
const size_t bytes_B  = items_B  * sizeof(T);   // size of work_B
const size_t bytes_HR = items_HR * sizeof(T);   // size of work_HR
const size_t bytes_R  = items_R  * sizeof(T);   // size of work_R
const size_t bytes_Ri = items_Ri * sizeof(T);   // size of work_Ri

   // work memories
   //
if (bytes_HR > SIZE_MAX - bytes_B
    || bytes_R  > SIZE_MAX - (bytes_B + bytes_HR)
    || bytes_Ri > SIZE_MAX - (bytes_B + bytes_HR + bytes_R))   WS_FULL;
const size_t total_items_fac = items_B + items_HR + items_R + items_Ri;
T * work_B = std::allocator<T>{}.allocate(total_items_fac);
T * work_HR = work_B  + items_B;
T * work_R  = work_HR + items_HR;
T * work_Ri = work_R  + items_R;

fMatrix<T>  B(work_B,  M, N, /* LDB */ M);
fMatrix<T> HR(work_HR, M, N, /* LDB */ M);
fMatrix<T>  R(work_R,  M, N, /* LDB */ M);
fMatrix<T> Ri(work_Ri, N, M, /* LDB */ M);

T * bb = &B.diag(0);
T * hr = &HR.diag(0);

   // initialize bb[] and hr[] in FORTRAN (.e. in column major) order,
   // which is ⍉ APL (i.e. row major) order. I.e. use ⍉B to init bb and hr.
   //
   ALL_COLS(N)   // FORTRAN columns (left to right)
   ALL_ROWS(M)   // FORTRAN rows (adjacent, top to bottom)
      {
        const ShapeItem APL_offset = col + N*row;
        set_real(*bb,   VB.get_real_value(APL_offset));
        set_imag(*bb,   VB.get_imag_value(APL_offset));
        *hr++ = *bb++;
      }

   // at this point: work_B is the items of APL N in FORTRAN order,
   // work_HR is a copy of B, subject to in-place modification by LApack,
   // and C allocated but not initialized,
   //
PTVVy<T> ptvvy(max(M, N), false);
   LA_DEBUG && ptvvy.print_pivot(CERR, N, LOC);

#if LA_DEBUG
DebugMatrix HR_before("HR_before laqp2()", HR);
   laqp2<T>(HR, ptvvy);
   print_QR("HR after laqp2() at " LOC, HR_before, HR, ptvvy);
#else
   laqp2<T>(HR, ptvvy);
#endif

   // zero-fill tau entries for virtual reflectors when M < N
   //
   for (Cdia k = M; k < N; ++k)   ptvvy.tau[k] = T(0.0);

   // compute Q ←→ Z[1]. At this point R was not yet initiaized, so we can
   // use it a work area.
   //
   R = HR;   // copy items of HR into R (no allocate)

   grab_Q<T>(Z, R, ptvvy);

   // extract the upper triangular matrix R of HR into Z[2] and
   // store its inverse R⁻¹ into Z[3].  Return R⁻¹ in Ri (destroys R).
   grab_R<T>(HR, Z, Ri);   // HR → R, R⁻¹

   std::allocator<T>{}.deallocate(work_B, total_items_fac);
   return N;
}
#endif // 0 - factorize_matrix is dead code
//════════════════════════════════════════════════════════════════════════════
/**
    In plain English: this is a thin wrapper around scaled_gelsy() (the
    function that does the actual 3-stage algorithm described at the top
    of this file) whose only job is to protect against a badly-scaled
    input: if the values in B (or A) are all extremely large or all
    extremely small, intermediate computations (like squaring a value to
    compute a length) could overflow or underflow even though the final
    mathematical answer would have been perfectly representable. So
    before calling scaled_gelsy(), this function checks the largest
    entry of B and of A and, if it is unreasonably large or small,
    multiplies every entry by a fixed safe factor (small_number or
    big_number, chosen so the result cannot overflow/underflow) to bring
    it back into a safe range; after scaled_gelsy() returns, the result
    is scaled back by the inverse factor(s) so the caller never sees any
    of this. (NOTE: unlike DGELSY/ZGELSY, which compute the exact ratio
    needed to bring the norm precisely to a target value, this uses one
    of two fixed multipliers - simpler, and sufficiently accurate for
    the matrix sizes GNU APL deals with, but not bit-for-bit identical
    to what DLASCL/ZLASCL would compute for extreme inputs.)

    LApack functions DGELSY and ZGELSY aka. xGELSY:

    xGELSY computes the minimum-norm solution to a complex linear least
    squares problem:

    minimize ║ B × X - A ║

    using a complete orthogonal factorization of B.
    B is an M-by-N matrix which may be rank-deficient.

    * if M < N (or if too many lines of A are linearly dependent) then
         A is under-determined and a DOMAIN ERRO is returned.
    * if M > N then A is overdetermined and the nearest X is returnd.
    * otherwise M = N and an exact solution X is returned if it exists.

    Several right hand side vectors b and solution vectors x can be
    handled in a single call; they are stored as the columns of the
    M-by-NRHS right hand side matrix A and the N-by-NRHS solution
    matrix X.

    The routine first computes a QR factorization with column pivoting:

    B × P = Q × ⎡ R₁₁ R₁₂ ⎤
                ⎣  0  R₂₂ ⎦

    with R₁₁ defined as the largest leading submatrix whose estimated
    condition number is less than 1/RCOND. The order of R11, i.e. RANK,
    is the effective rank of B.

    Then, R22 is considered to be negligible, and R12 is annihilated
    by unitary transformations from the right, arriving at the
    complete orthogonal factorization:

    B ∘ P = Q ∘ ⎡ T₁₁ 0 ⎤ ∘ Z
                ⎣  0  0 ⎦

    The minimum-norm solution is then

    X = P × Z° × H [ inv(T11) × Q1 * × H × A ]
                     [        0         ]

    where q₁ consists of the first rank columns of q.

     This routine is basically identical to the original xGELSX except
     three differences:

       • The permutation of matrix A (the right hand side) is faster and
         more simple.

       • The call to the subroutine xGEQPF has been substituted by the
         the call to the subroutine xGEQP3. This subroutine is a Blas-3
         version of the QR factorization with column pivoting.

       • fMatrix A (the right hand side) is updated with Blas-3.

    If A.get_dx() is 0 then only the QR factorization of B shall be
    computed and returned.
 */
template<typename T>
sRank LA_pack::gelsy(fMatrix<T> & A, fMatrix<T> & B, APL_Float rcond)
{
const Crow M = B.get_row_count();
const Ccol N = B.get_column_count();

   // APL is responsible for handling the empty cases
   //
   Assert(M && N && A.get_column_count() && N <= M);

   // For a better precision, scale B and A so that their max. norm lies
   // between small_number and big_number. Then call scaled_gelsy() and
   // scale the result back by the same factors.
   //
APL_Float un_scale_B = 1.0;
   {
     T * const B00 = &B.diag(0);
     const size_t MN = M * N;
     const APL_Float norm_B = max_item(B00, MN);

     if (norm_B == 0.0)              return 0;   // B is the 0-matrix
     if (norm_B < small_number)      scale(B00, MN, un_scale_B = big_number);
     else if (norm_B > big_number)   scale(B00, MN, un_scale_B = small_number);
   }

APL_Float un_scale_A = 1.0;
     {
       T * const A00 = &A.diag(0);
       const size_t MN = A.get_row_count() * A.get_column_count();
       const APL_Float norm_A = max_item(A00, MN);

       if (norm_A < small_number)   scale(A00, MN, un_scale_A = big_number);
       if (norm_A > big_number)     scale(A00, MN, un_scale_A = small_number);
     }

   // do the work.
   //
   {
     const sRank RANK = scaled_gelsy(B, A, rcond);
     if (RANK < N)   return RANK;   // this is an error
   }

   // Undo scaling.
   //
   if (un_scale_A != 1.0 || un_scale_B != 1.0)   // unlikely
      {
        fMatrix<T> A1 = A.take(N, A.get_column_count());
        if (un_scale_B != 1.0)
           {
             scale(&A1.diag(0), N*A.get_column_count(), un_scale_B);
           }

        if (un_scale_A != 1.0)
           {
             scale(&A1.diag(0), N * A.get_column_count(), 1.0/un_scale_A);
           }

        if (un_scale_B != 1.0)
           {
             fMatrix<T> B1 = B.take(N, N);
             scale(&B1.diag(0), N*N, 1.0/un_scale_B);
           }
      }
 
   return N;   // success
}
//────────────────────────────────────────────────────────────────────────────
/// scaled_gelsy() computes gelsy() with B and A scaled nicely.
/*
   In plain English: this is where the actual work happens - stages 1
   to 3 from the big-picture overview near the top of this file, in
   order:

   1. laqp2() below triangularizes B (with column pivoting) into R,
      recording the Householder reflectors that did it.
   2. estimate_rank() walks R's diagonal and figures out how many of
      B's columns are actually independent (given the requested rcond).
      If that is fewer than all of them, B is (numerically) singular
      and we give up right here (the caller turns this into a
      DOMAIN ERROR).
   3. unm2r() replays the same reflectors from step 1 onto A (the
      right-hand side), turning "solve B∘X=A" into "solve R∘X=(Q°∘A)".
      trsm() then solves that (now upper triangular) system by ordinary
      back-substitution. Finally the small loop at the very end undoes
      the column pivoting from step 1 (columns of R got shuffled around
      for numerical-stability reasons in step 1; the corresponding rows
      of the solution X must be shuffled back to match the caller's
      original column order of B).

   B is the matrix to be inverted, and
   A is one or more column vectors for which we want to solve B ∘ X[;i] = A[;i]

   On return: result X stored in B and items of A overwritten.
 */
template<typename T>
sRank LA_pack::scaled_gelsy(fMatrix<T> & B, fMatrix<T> & A, double rcond)
{

   // print_matrix<T>("B", B);
   // print_matrix<T>("A", A);

   // this gelsy is optimized for (and restricted to) the following conditions:
   //
   // 0 < N <= M  →  min_MN = N and max_MN = M
   // 0 < NRHS
   //
const Ccol NRHS = A.get_column_count();   // right hand side (of B ∘ X = A)
const Ccol N    = B.get_column_count();
PTVVy<T> ptvvy(max(NRHS, N), true);
   LA_DEBUG && ptvvy.print_pivot(CERR, N, LOC);

/* Compute QR factorization of B with column pivoting: B ∘ P = Q ∘ R

   The routine first computes a QR factorization with column pivoting:

                                 ⎡ R11 R12 ⎤
       B ∘ P = Q ∘ R    with R = ⎢         ⎥, and
                                 ⎣  0  R22 ⎦

   R11 defined as the largest leading submatrix whose estimated
   condition number is less than 1/RCOND. The order of R11, i.e. RANK,
   is the effective rank of B.

   Then, R22 is considered to be negligible, and R12 is annihilated
   by orthogonal transformations from the right, arriving at the
   complete orthogonal factorization:

                                     ⎡ T11 0 ⎤
       B ∘ P = Q ∘ T ∘ Z    with T = ⎢       ⎥
                                     ⎣  0  0 ⎦

   The minimum-norm solution is then

       X = P ∘ Z° ∘ T [ inv(T11) ∘ Q1° ∘ T ∘ A ]
                   [        0         ]
   where Q1 consists of the first RANK columns of Q.
*/

#if LA_DEBUG
DebugMatrix B_before("B_before laqp2()", B);
   laqp2<T>(B, ptvvy);
   print_QR("Q∘R after laqp2() at " LOC, B_before, B, ptvvy);
#else
   laqp2<T>(B, ptvvy);
#endif

   // Determine the RANK of B using incremental condition estimation
   //
   {
     const int RANK_B = estimate_rank(B, rcond, ptvvy);
     if (RANK_B < N)   return RANK_B;
   }

   if (A.get_dx() == 0)   return N;   // QR factorization only

   // from here on, RANK == N (B has N linear independent rows)
   // We leave RANK in the comments but use N in the code.

   //                         ⎡ R11 R12 ⎤
   // Logically partition R = ⎢         ⎥,
   //                         ⎣  0  R22 ⎦
   //
   // where R11 = R[1:RANK, 1:RANK]
   // [R11, R12] = [ T11, 0 ] × A
   //

   /* according to e.g. ZUNM2R:

      B is COMPLEX*16 array, dimension (LDA,K)
      The i-th column must contain the vector which defines the
      elementary reflector H(i), for i = 1,2,...,k, as returned by
      ZGEQRF in the first k columns of its array argument B.

      B is modified by the routine but restored on exit.
      A[1:M, 1:NRHS] := Q° ∘ H ∘ A[1:M, 1:NRHS]
    */
   unm2r<T>(N, B, ptvvy, A);   // called with SIDE = 'Left'

   // A(1:RANK, 1:NRHS) := reciprocal(T11) * A(1:RANK,1:NRHS)
   //
   // solve B ∘ X[; 1:NRHS] = A[; 1:NRHS]; store the result X in A[; 1:NRHS].
   //
   trsm<T>(B, A, NRHS);

   // tmp[N]: B column of A, permuted by pivot. FORTRAN uses a separate tmp[N]. 
   // However, tau[N] is no longer needed, so we can reuse it here. The loop
   // below undoes the pivoting of columns performed in laqp2() / laqp2().
   //
// ptvvy.print_pivot(CERR, N, LOC);

   ALL_COLS(NRHS)   // for every column col of A
      {
        ALL_ROWS(N)
           ptvvy.tau[ptvvy.pivot[row]] = A.at(row, col);   // tau ← A[;col]
        ALL_ROWS(N)
           A.at(row, col) = ptvvy.tau[row];          // A[;col]←tau
      }

   return N;
}
//────────────────────────────────────────────────────────────────────────────
/// function estimating the rank of \b A. (simplified part of GELSY)
template<typename T>
sRank LA_pack::estimate_rank(const fMatrix<T> & A, APL_Float rcond,
                             PTVVy<T> & ptvvy)
{
   /* In plain English: A here is R, the upper triangular matrix that
      laqp2() (with column pivoting!) produced from B. "Rank" answers
      the question "how many of B's columns actually carry independent
      information, given that we're willing to ignore differences
      smaller than rcond (aka ⎕CT)?" The textbook way to answer that
      is a full SVD (see the GESVD description below) and count how
      many singular values are bigger than rcond*(largest singular
      value) - but a full SVD is expensive (cubic in N) and massive
      overkill just to get a yes/no answer for each successive column.

      Instead, this function walks down R's diagonal one column at a
      time and, after each column, cheaply updates a RUNNING ESTIMATE
      of what the smallest and largest singular values of the matrix
      would be if we had stopped right there (that update is
      laic1_MIN/laic1_MAX below - "laic1" stands for "linear algebra,
      incremental condition estimation, step 1"). The moment the
      estimated smallest singular value becomes too small relative to
      the estimated largest one (smax*rcond > smin), we know that
      adding the next column did not add any real new information -
      i.e. columns from here on are (numerically) redundant - and we
      stop and report the rank found so far. Because column pivoting
      in laqp2() always moves the "most independent-looking" remaining
      column to the front, redundant columns are guaranteed to end up
      at the back, which is exactly what makes this early-stopping
      approach work.

      Determine RANK using incremental condition estimation...
     
      most likely this is pretty much an inlined GESVD with not used cases
      removed. As to GESVD:

      GESVD computes the singular value decomposition (SVD) of an
      M-by-N matrix A, optionally computing the left and/or right singular
      vectors. The SVD is written

      A = U × SIGMA × conjugate-transpose(V)  (i.e. A = U × SIGMA × V°)

      where SIGMA is an M-by-N matrix which is zero except for its
      min(m,n) diagonal elements, U is an M-by-M unitary matrix, and
      V is an N-by-N unitary matrix.  The diagonal elements of SIGMA
      are the singular values of A; they are real and non-negative, and
      are returned in descending order.  The first min(m,n) columns of
      U and V are the left and right singular vectors of A.

      Note that the routine returns V° ∘ H, not V.
    */
const Ccol N = A.get_column_count();

   // let s be some singular value of A. smax is an upper bound that approaches
   // s from below and smin is a lower bound thaty approaches from above. If
   // some smax*rcond > smin below then matrix A is rank-deficient.
   //
APL_Float smax = abs(A.diag(0));
APL_Float smin = smax;
   if (smax == 0.0)   return 0;

T * work_min = ptvvy.work_min;
T * work_max = ptvvy.work_max;

   /*
      store minima in work_min[]
      store maxima in work_max[]

      work_min and work_max will grow in the 'dia' loop, so only work_min[0]
      and work_max[0] need to be initialized beforehand.

      The last item in work_minx resp. work_max is always COS_min resp. COS_max
     (from laic1_MIN/MAX()), while the items before are the the products
     of the prior SIN_min/SON_max (also from laic1_MIN/MAX().
    */
   work_min[0] = T(1.0);   // cos 90°
   work_max[0] = T(1.0);   // cos 90°

   for (Cdia dia = 1; dia < N; ++dia)   // loop over the diagonal of A
       {
         T SIN_min(0.0);
         T SIN_max(0.0);
         T COS_min(0.0);
         T COS_max(0.0);

         // accumulate the rows of A above (but excluding) the diagonal,
         // scaled with the diagonal item A.diag(row). 
         //
         T alpha_min = 0.0;   // accumulator for work_min
         T alpha_max = 0.0;   // accumulator for work_max
         ALL_ROWS(dia)   // all rows above the diagonal
            {
              const T Ard = A.at(row, dia);   // row 'row' above the diagonal
              alpha_min += conjugated(work_min[row]) * Ard;
              alpha_max += conjugated(work_max[row]) * Ard;
            }

         const T gamma(A.diag(dia));
         laic1_MIN<T>(smin, alpha_min, gamma, SIN_min, COS_min);
         laic1_MAX<T>(smax, alpha_max, gamma, SIN_max, COS_max);

         if (smax*rcond > smin)   // done (error). The rank of A is < N.
            {
              return dia;
            }

         ALL_ROWS(dia)
            {
              work_min[row] *= SIN_min;
              work_max[row] *= SIN_max;
            }

         work_min[dia] = COS_min;   // for the next iteration
         work_max[dia] = COS_max;   // for the next iteration
       }

   return N;   // OK
}
//────────────────────────────────────────────────────────────────────────────
/*  In plain English: this is Stage 1 of the big picture at the top of
    this file - it IS the QR factorization (with column pivoting) of A,
    done in place: on return, A's upper triangle (on and above the
    diagonal) holds R, while A's lower triangle (below the diagonal,
    conceptually "should be" all 0 in a plain QR factorization) has been
    overwritten with a compressed description of the Householder
    reflectors that were used to get there - each column's below-
    diagonal part is that column's reflector vector v, and ptvvy.tau[]
    holds the accompanying tau scalar. Together (v, tau) is everything
    unm2r()/ung2r() (further up in this file) need later to either apply
    those same reflectors to some other matrix, or turn them into the
    literal orthogonal matrix Q.

    Concretely, for every column (from left to right):
      1. (if pivoting is enabled) find the not-yet-processed column
         with the largest remaining length and swap it into place -
         this is what makes the rank estimate in estimate_rank()
         meaningful, and also improves numerical stability in general.
      2. build a Householder reflector (larfg(), see below) that would
         turn everything below the diagonal, in the column just swapped
         into place, into 0.
      3. apply that reflector (larf()) to the rest of the matrix (all
         columns to the right of the current one) - this is what
         actually eliminates the entries below the diagonal, while also
         updating those later columns consistently.
      4. cheaply update the remembered lengths ("column norms") of the
         remaining columns, so step 1 doesn't need to rescan the whole
         matrix on the next iteration.

    LApack function laqp2. Compute a QR factorization with or without
 *    column pivoting of matrix A:

    A ∘ P = Q ∘ R
    A     = Q ∘ R

    P is the permutation of columns produced by the pivoting.
 */
template<typename T>
void LA_pack::laqp2(fMatrix<T> & A, PTVVy<T> & ptvvy)
{
   print_matrix("laqp2() input", A);

const Crow M = A.get_row_count();
const Ccol N = A.get_column_count();

   // Initialize the pivot and the partial column norms. Moved from geqp3()
   // which no longer exists.
   //
   if (ptvvy.pivot)   // A⌹B or ⌹B
      {
        Assert(M >= N);
        ALL_COLS(N)
           {
             ptvvy.pivot[col] = col;
             ptvvy.vn1[col] = ptvvy.vn2[col] = norm_2(&A.at(0, col), M);
           }
      }

const Cdia D = min(M, N);
   ALL_DIAS(D)
      {
        if (ptvvy.pivot)
           {
             // pvt_0 is the column at or right of col that has the largest vn1.
             // If col already has the largest norm: OK, otherwise exchange
             // columns col and pvt_0 of a and their column norms
             //
             const int pvt_0 = dia + max_pos(ptvvy.vn1 + dia, N - dia);
             if (pvt_0 != dia)   // unless dia already is the pivot column
                {
                  A.exchange_columns(pvt_0, dia);
                  exchange(ptvvy.pivot[pvt_0], ptvvy.pivot[dia]);
                  ptvvy.vn1[pvt_0] = ptvvy.vn1[dia];
                  ptvvy.vn2[pvt_0] = ptvvy.vn2[dia];
                }
           }

       /* Generate the elementary reflector H(dia) that annihilates the
           column vector X= A(s_X:M, dia), where s_X = dia+1.

               0 → → → dia → → N
               ↓    ╔═══│═A════╗
               ↓    ║ \ │      ║
               ↓    ║  \│      ║
              dia ──╫───⍺      ║   ⍺: ALPHA aka. X[-1] of reflector X
           ┬  s_X ──╫   ▒\     ║      len_X is the reflector length
           │   ↓    ║   ▒ \    ║      (=number fo items below ALPHA)
           │   ↓    ║   ▒  \   ║ 
         len   ↓    ║   X   \  ║ 
           │   ↓    ║   ▒    \ ║
           │   N  ──╫───▒──────╫── N
           │        ║   ▒      ║ 
           ┴   M    ╚═══╪══════╝
                       dia     
         */
        T & tau = ptvvy.tau[dia];
        const Crow s_X   = dia + 1;   // start of X = row below diag(dia)
        const Crow len_X = M - s_X;   // length of reflector X

        // NOTE: always call larfg(), even when len_X == 0 (i.e. dia is the
        // last row of A). For real T this is a no-op (tau becomes 0, as if
        // we had skipped the call), matching dlarfg.f's unconditional
        // N<=1 shortcut. For complex T however, zlarfg.f does NOT take a
        // shortcut for a 1-element (alpha-only) "vector" unless alpha is
        // already real: a complex alpha with a nonzero imaginary part must
        // still be rotated onto the real axis (a 1x1 Householder "reflector"
        // that is a pure phase rotation). Skipping that call unconditionally
        // (as this code used to do for len_X == 0) leaves such a diagonal
        // element non-real; larfg() below reproduces both cases correctly.
        //
        // &A.diag(dia)+1 is used instead of &A.at(s_X, dia) because the
        // latter is out of bounds (one row past the last row of A) exactly
        // when len_X == 0; the two pointers are identical whenever s_X is
        // in bounds (both matrices are column-major with column dia), so
        // this is a pure bug fix for the len_X == 0 case, no behaviour
        // change otherwise.
        //
        tau = larfg<T>(&A.diag(dia) + 1, len_X);

        /* Apply H(dia)° × H to A(offset+i:m, i+1:n) from the left.

           0→ → → dia → → N
           ↓  ╔════╪═A═════╗
           ↓  ║ \  │       ║
           ↓  ║  \ │       ║
           ↓  ║   \│       ║ 
          dia ║    ⍺┌──────╢   ⍺: Acc = A.diag(dia) := 1.0
        ┬ s_X ║     │      ║ 
        │  ↓  ║     │ SUB  ║ 
    len_X  ↓  ║     │      ║ 
        │  N  ║     │      ║ 
        ╩  M  ╚═════╪══════╝
                  dia+1 == s_X
         */
        if (s_X < D)
           {
             T & ALPHA = A.diag(dia);
             const T alpha = ALPHA;           // remember ⍺ = A(dia, dia)
                ALPHA = T(1.0);
                fMatrix<T> SUB = A.sub_matrix(dia, s_X);
                larf<T>(&A.diag(dia),conjugated(tau), SUB, ptvvy.y);
             ALPHA = alpha;                   // restore ⍺ = A(dia, dia)
           }

        if (!ptvvy.pivot)   continue;

        // Update the partial column norms vn1 and vn2.
        //
        for (Ccol c = dia + 1; c < N; ++c)
            {
              if (ptvvy.vn1[c] == 0.0)   continue;

              const APL_Float abs_A = abs(A.at(dia, c)) / ptvvy.vn1[c];
              const APL_Float temp = max(1.0 - square(abs_A), 0.0);
              const APL_Float quot = ptvvy.vn1[c] / ptvvy.vn2[c];
              if (temp * quot * quot <= tol3z)   // temp and/or quot too small
                 {
                   ptvvy.vn1[c] = 0.0;   // fallback if  s_X is an invalid row
                   if (s_X < M)          // s_X is a valid row
                      {
                        // vn1[c] = length of the column vector in A that
                        // starts at row s_X and columns c.
                        //
                        ptvvy.vn1[c] = norm_2(&A.at(s_X, c), M - s_X);
                      }
                   ptvvy.vn2[c] = ptvvy.vn1[c];
                 }
              else                                 // "normal" quot
                 {
                   ptvvy.vn1[c] *= sqrt(temp);
                 }
            }
      }
}
//────────────────────────────────────────────────────────────────────────────
/** In plain English: imagine growing a square matrix one row/column at a
    time (that is exactly what estimate_rank() does, walking down R's
    diagonal) and, after every growth step, wanting an up-to-date guess
    of "by how much can this matrix stretch a vector, at most?" (its
    largest singular value) - WITHOUT recomputing that from scratch
    every time. laic1_MAX() is that one incremental update step: given
    the previous estimate SEST (for the matrix one size smaller), and
    ALPHA/GAMMA (numbers summarizing how the new row/column relates to
    everything before it - see estimate_rank()'s call site for exactly
    what they are), it computes an updated SEST for the grown matrix,
    plus a SIN/COS pair describing the direction (as a vector rotation)
    in which that maximal stretch now happens - which is only needed so
    that the *next* call to laic1_MAX() can keep updating incrementally
    without redoing older work.

    This trick (and its GAMMA-vs-SEST-vs-ALPHA comparisons, which look
    arbitrary at first glance) comes from the mathematics of how a
    matrix's singular values change when you append one more row and
    column to it - see the LApack Working Notes / "incremental condition
    estimation" literature if you want the derivation; this code only
    needs to reproduce the formulas correctly, not re-derive them.

    LApack function laic1 (estimate largest singular value).
    apply one step of incremental condition estimation.

    SEST: a lower bound for the largest estimated singular value of some matrix

    In the end (i.e. after repeating laic1_MAX() along the diagonal of some
    matrix A) is SEST a lower bound for the largest singular value of A.

    See also: laic1_MIN() below.

    laic1_MAX() is FORTRAN laic1 with JOB = 1, while
    laic1_MIN() is FORTRAN laic1 with JOB = 2.
 **/
template<typename T>
void LA_pack::laic1_MAX(APL_Float & SEST, T ALPHA, T GAMMA, T & SIN, T & COS)
{
const APL_Float abs_ALPHA = abs(ALPHA);
const APL_Float abs_GAMMA = abs(GAMMA);
const APL_Float abs_SEST  = abs(SEST);

   //    Estimating largest singular value ...

   //    special cases
   //
   if (SEST == 0.0)
      {
        const APL_Float smax = max(abs_GAMMA, abs_ALPHA);
        if (smax == 0.0)
           {
             SIN = T(0.0);   // 0°
             COS = T(1.0);   // 0°
             SEST = 0.0;
           }
        else
           {
             SIN = ALPHA / smax;
             COS = GAMMA / smax;
             SEST = smax * normalize(SIN, COS);
           }
        return;
      }

   if (abs_GAMMA <= dlamch_E * abs_SEST)   // if /tmp overflow
      {
        SIN = T(1.0);   // 90⁰
        COS = T(0.0);   // 90⁰
        const APL_Float abs_max = max(abs_SEST, abs_ALPHA);
        const APL_Float s1 = abs_SEST / abs_max;
        const APL_Float s2 = abs_ALPHA    / abs_max;
        SEST = abs_max * hypotenuse(s1, s2);
        return;
      }

   if (abs_ALPHA <= dlamch_E * abs_SEST)   // small abs_ALPHA
      {
        if (abs_GAMMA <= abs_SEST)
           {
             SIN = T(1.0);   // 90⁰
             COS = T(0.0);   // 90⁰
             SEST = abs_SEST;
           }
        else
           {
             SIN = T(0.0);   // 0⁰
             COS = T(1.0);   // 0⁰
             SEST = abs_GAMMA;
           }
        return;
      }

   if (abs_SEST <= dlamch_E * abs_ALPHA ||   // small abs_SEST
       abs_SEST <= dlamch_E * abs_GAMMA)     // small abs_SEST
      {
        if (abs_GAMMA <= abs_ALPHA)
           {
             const APL_Float quot = abs_GAMMA / abs_ALPHA;
             const APL_Float scale = hypotenuse(1.0, quot);

             SEST = abs_ALPHA * scale;
             SIN = (ALPHA / abs_ALPHA) / scale;
             COS = (GAMMA / abs_ALPHA) / scale;
           }
        else
           {
             const APL_Float quot = abs_ALPHA / abs_GAMMA;
             const APL_Float scale = hypotenuse(1.0, quot);
             SEST = abs_GAMMA * scale;
             SIN = (ALPHA / abs_GAMMA) / scale;
             COS = (GAMMA / abs_GAMMA) / scale;
           }
        return;
      }

   // the normal case
   //
const APL_Float zeta1 = abs_ALPHA / abs_SEST;
const APL_Float zeta2 = abs_GAMMA / abs_SEST;
const APL_Float b = 0.5*(1.0 - square(zeta1) - square(zeta2));
const APL_Float t = b > 0.0 ? square(zeta1) / (b + hypotenuse(b, zeta1))
                            : hypotenuse(b, zeta1) - b;

   SIN = -(ALPHA / abs_SEST) / t;
   COS = -(GAMMA / abs_SEST) / (1.0 + t);
   normalize(SIN, COS);
   SEST = sqrt(t + 1.0) * abs_SEST;
}
//────────────────────────────────────────────────────────────────────────────
/** In plain English: the mirror image of laic1_MAX() above (read that
    one first) - same incremental-growth idea, but tracking the SMALLEST
    singular value instead of the largest. This is the more important
    of the two for rank detection: estimate_rank() compares
    smax*rcond > smin, i.e. it is THIS function's output that decides
    whether the matrix so far still looks "genuinely full rank" or has
    become (numerically) singular.

    Every special case below mirrors one in laic1_MAX() (compare the
    two side by side if something looks odd) except that the roles of
    ALPHA and GAMMA are not simply swapped - the formulas for "smallest"
    are genuinely different from "largest" (that is expected: finding
    the smallest singular value of a growing matrix is a harder,
    differently-shaped problem than finding the largest one), so do not
    "simplify" this function by copy-pasting from laic1_MAX() without
    checking each condition against dlaic1.f/zlaic1.f (devel_doc/FORTRAN)
    line by line - that copy-paste mistake (comparing |gamma| against
    |alpha| instead of against the running estimate |sest| in one of the
    branches below) is exactly the bug that was fixed here; it broke
    rank/singularity detection for matrices whose near-zero singular
    value fell in a specific, narrow relative-magnitude window.

    LApack function laic1 (estimate smallest singular value).
    Results are: SEST, SIN, and COS, updated in place (i.e. IN/OUT
    parameters in FORTRAN). Apply one step of incremental condition
    estimation.
 */

/*
     SEST: smallest estimated singular value of \b this matrix

     LAIC1 applies one step of incremental condition estimation in
     its simplest version:

     Let x, twonorm(x) = 1, be an approximate singular vector of an j-by-j
     lower triangular matrix L, such that

          twonorm(L × x) = sest

     Then LAIC1 computes sestpr, s, c such that the vector

                 [ s × x ]


     x^ = [  c  ]

     is an approximate singular vector of

     [ L 0 ]

     L^ = [ w° × H gamma ]


     in the sense that twonorm(L^ × x^) = sestpr.

     Depending on JOB (_MIN resp, _MAX), an estimate for the largest resp.
     smallest singular value is computed (in FORTRAN). To make the code more
     readable, GNU APL has split the FORTRAN laic1(JOB) into two separate
     functions laic1_MIN() and laic1_MAX().

    Note that [s c]° × H and sestpr° × 2 is an eigenpair of the system

                                          ⎡ conjg(alpha) ⎤
    diag(sest*sest, 0) + [alpha  gamma] × ⎢              ⎥
                                          ⎣ conjg(gamma) ⎦

    where  alpha =  x° × H × w.

    In the end (i.e. after repeating laic1_MIN() along the diagonal of some
    matrix A) is SEST an upper bound for the smallest singular value of A.
 **/
template<typename T>
void LA_pack::laic1_MIN(APL_Float & SEST, T ALPHA, T GAMMA, T & SIN, T & COS)
{
   // Estimating the smallest singular value...
   //
const APL_Float abs_ALPHA = abs(ALPHA);
const APL_Float abs_GAMMA = abs(GAMMA);
const APL_Float abs_SEST  = abs(SEST);

   // special cases
   //
   if (SEST == 0.0)
      {
        SIN = 1.0;   // 90°
        COS = 0.0;   // 90°
        if (abs_GAMMA > 0.0 || abs_ALPHA > 0.0)
           {
             SIN = -conjugated(GAMMA);
             COS =  conjugated(ALPHA);
           }

        const APL_Float abs_max = max(abs(SIN), abs(COS));
        SIN /= abs_max;
        COS /= abs_max;
        normalize(SIN, COS);
        return;
      }

   if (abs_GAMMA <= dlamch_E * abs_SEST)
      {
        SIN = T(0.0);   // 0°
        COS = T(1.0);   // 0°
        SEST = abs_GAMMA;
        return;
      }

   if (abs_ALPHA <= dlamch_E * abs_SEST)
      {
        if (abs_GAMMA <= abs_SEST)
          {
            SIN = T(0.0);   // 0°
            COS = T(1.0);   // 0°
            SEST = abs_GAMMA;
          }
        else
          {
            SIN = T(1.0);
            COS = T(0.0);
            SEST = abs_SEST;
          }
        return;
      }

   if (abs_SEST <= dlamch_E * abs_ALPHA ||   // small abs_SEST
       abs_SEST <= dlamch_E * abs_GAMMA)     // small abs_SEST
      {
        const T conj_gamma = conjugated(GAMMA);
        const T conj_alpha = conjugated(ALPHA);

        if (abs_GAMMA <= abs_ALPHA)
           {
             const APL_Float quot = abs_GAMMA / abs_ALPHA;
             const APL_Float scale = hypotenuse(1.0, quot);
             const APL_Float tmp_scale = quot / scale;

             SIN = - (conj_gamma / abs_ALPHA) / scale;
             COS =   (conj_alpha / abs_ALPHA) / scale;
             SEST = abs_SEST * tmp_scale;
           }
        else
           {
             const APL_Float tmp = abs_ALPHA / abs_GAMMA;
             const APL_Float scale = hypotenuse(1.0, tmp);

             SIN = - (conj_gamma / abs_GAMMA) / scale;
             COS =   (conj_alpha / abs_GAMMA) / scale;
             SEST = abs_SEST / scale;
           }
        return;
      }

   // normal case
   //
const APL_Float zeta1 = abs_ALPHA / abs_SEST;
const APL_Float zeta2 = abs_GAMMA / abs_SEST;
const APL_Float zeta = zeta1 + zeta2;

const APL_Float norma_1 = 1.0 + zeta1 * zeta;
const APL_Float norma_2 =       zeta2 * zeta;
const APL_Float norma = max(norma_1, norma_2);

const APL_Float test = 1.0 + 2.0*(zeta1 - zeta2)*(zeta1 + zeta2);
   if (test >= 0.0 )
      {
        // root is close to zero, compute directly
        //
        const APL_Float zeta2_2 = square(zeta2);
        const APL_Float b = 0.5*(square(zeta1) + zeta2_2 + 1.0);
        const APL_Float t = zeta2_2 / (b + sqrt(abs(b*b - zeta2_2)));
        SIN =   (ALPHA / abs_SEST) / (1.0 - t);
        COS = - (GAMMA / abs_SEST) / t;
        SEST *= sqrt(t + square(2.0 * dlamch_E) * norma);
      }
   else
      {
        // root is closer to ONE, shift by that amount
        //
        const APL_Float b = 0.5 * (square(zeta2) + square(zeta1) - 1.0);
        APL_Float t;
        if (b >= 0.0)   t = -square(zeta1) / (b + hypotenuse(b, zeta1));
        else            t = b - hypotenuse(b, zeta1);

        SIN = - (ALPHA / abs_SEST) / t;
        COS = - (GAMMA / abs_SEST) / (1.0 + t);
        SEST *= sqrt(1.0 + t + square(2.0*dlamch_E) * norma);
      }

   normalize(SIN, COS);
}
//────────────────────────────────────────────────────────────────────────────
/** In plain English: given a column vector (ALPHA, X) (ALPHA is its
    first element, X the rest), this computes the description of a
    "mirror" H - a Householder reflector - that, when applied to that
    vector, leaves it pointing exactly along the first axis: H applied
    to (ALPHA, X) gives (BETA, 0, 0, ..., 0) for some single number
    BETA (of the same length as the original vector, since a mirror
    reflection never changes lengths). That is precisely what is needed
    to zero out everything below the diagonal of one column during QR
    factorization (laqp2() above).

    H itself is never built as an explicit matrix (that would be
    wasteful); instead, this function only returns the "recipe" for H:
    the scalar tau, plus the vector v which is written directly into X
    in place (overwriting it - the original X is not needed anymore
    once H is known). Together, H = I - tau × v∘v° (see larf() below
    for how that recipe is turned into an actual transformation of some
    other matrix, without ever forming H).

    LApack function larfg. It generates one elementary reflector,
    i.e. one Householder matrix H.

   LARFG generates one elementary reflector H of order N, such that:

             ⎛alpha⎞   ⎛beta⎞
      H°∘H × ⎜     ⎟ = ⎜    ⎟
             ⎝  x  ⎠   ⎝  0 ⎠

   and:

      H°∘H ∘ H = I.

   where alpha and beta are scalars, with beta real,
   and x is an (n-1)-element vector.

   IOW: applying H to a column vector (alpha, X₀, ... Xₙ₋₁)
        yields a column vector (beta, 0, ... 0).

   Reflector H is represented in the form

                    ⎛ 1.0 ⎞
      H = I - tau × ⎜     ⎟ × ( 1.0, v° ) × H
                    ⎝  v  ⎠

   where tau is a scalar of type T and v is a (n-1)-element vector of type T.
   Note that H is not hermitian (not H⁻¹ = H°). If the elements of x are
   all zero and alpha is real, then tau = 0 (and therefore H is the unit
   matrix I). Otherwise 1 <= real(tau) <= 2 and abs(tau-1) <= 1 . 

   Note also: The caller of larfg() i.e. laqp2() sets ALPHA = X[-1]. We
   set ALPHA inside larfg() rather than setting it before calling larfg()
   and passing it as parameter to larfg.
 */
/** Safely compute sqrt(a² + b² + c²) for arbitrary finite a, b, c
    (all assumed already non-negative, as every caller here passes
    |something|), without ever squaring any of them directly -- unlike
    a naive sqrt(a*a + b*b + c*c), this stays correct up to the full
    representable double range instead of overflowing once any of
    a, b, c exceeds sqrt(DBL_MAX) (~1.34E154). Same scaled-hypot
    technique as the fixed norm_2() above (scale by the largest
    magnitude, sum squares of ratios ≤1, multiply the single sqrt by
    scale once at the end -- never scale²).
 */
static APL_Float scaled_hypot3(APL_Float a, APL_Float b, APL_Float c)
{
const APL_Float scale = max(a, max(b, c));
   if (scale == 0.0)   return 0.0;
const APL_Float ra = a/scale;
const APL_Float rb = b/scale;
const APL_Float rc = c/scale;
   return scale * sqrt(ra*ra + rb*rb + rc*rc);
}
//────────────────────────────────────────────────────────────────────────────
template<typename T>
T LA_pack::larfg(T * X, Crow len_X)
{
   // NOTE: unlike earlier versions of this function, there is no shortcut
   // returning T(0.0) here for len_X == 0. For real T that shortcut was
   // harmless (dlarfg.f always returns TAU=0 when N<=1), but for complex T
   // it was wrong: zlarfg.f only returns TAU=0 for N<=1 when ALPHA is
   // already real (imag == 0); otherwise it still rotates ALPHA onto the
   // real axis. The general code below (see the "ALPHA_i == 0.0 &&
   // norm2_X == 0.0" check) already reproduces both cases correctly when
   // len_X == 0 (norm2_X is then trivially 0), so no special case is
   // needed here.
   //
   // ALPHA: the geometrical length of vector X (i.e. NOT len_X!)
   // BETA:  the geometrical length of vector ALPHA,X
   //
T & ALPHA = X[-1];   // ⍺ is the diagonal element of A above X

APL_Float ALPHA_r = get_real(ALPHA);
APL_Float ALPHA_i = get_imag(ALPHA);
const APL_Float XNORM = norm_2(X, len_X);   // ║X║ (NOT squared, see norm_2())

   if (ALPHA_i == 0.0 &&                 // ALPHA is real, and
       XNORM == 0.0)   return T(0.0);    // all x are 0, then H = I.

   // BETA = length of ║ALPHA, X║ = sqrt(ALPHA_r² + ALPHA_i² + XNORM²).
   // Naive sqrt(square(ALPHA) + norm2_X) overflowed to +Inf (then, a few
   // lines later, tau = (BETA_r-ALPHA_r)/BETA_r = Inf/Inf = NaN) once any
   // of ALPHA_r/ALPHA_i/XNORM exceeded ~1.34E154 -- Blake McBride's H17e,
   // e.g. ⌹2 2⍴1E160 1 1E160 1 (genuinely singular) silently returned
   // -nan- instead of DOMAIN ERROR. scaled_hypot3() (above) is immune.
APL_Float BETA_r = scaled_hypot3(fabs(ALPHA_r), fabs(ALPHA_i), XNORM);
   if (ALPHA_r > 0.0)   BETA_r = -BETA_r;           // opposite sign of ALPHA

   // ALPHA_r/ALPHA_i/XNORM can all underflow to exactly 0.0 for a
   // denormal-scale ALPHA/X (e.g. 1E-200J1E-200), making BETA_r itself
   // exactly 0.0 -- which the "scale small BETA" loop below can never
   // move away from 0 (0.0 * anything is still 0.0), so its
   // "BETA_r > neg_safe_min && BETA_r < pos_safe_min" exit condition
   // holds forever. Reproduced: ⌹ 2 2⍴1J1 0 0 1E¯200J1E¯200 hangs at
   // 100% CPU. Only ALPHA_i==0.0 exactly short-circuits above; guard
   // the general BETA_r==0.0 case here too (H = I is the correct, same
   // degenerate answer as that earlier check).
   if (BETA_r == 0.0)   return T(0.0);

   // scale small BETA (and, with it, X) so that it can be used safely
   //
int kcnt = 0;
   if (BETA_r > neg_safe_min && BETA_r < pos_safe_min)
      {
        const APL_Float inv_safe_min = 1.0 / pos_safe_min;   // 4.98959E291
        while (BETA_r > neg_safe_min && BETA_r < pos_safe_min)
           {
             ++kcnt;
             scale(X, len_X, inv_safe_min);    // scale X
             BETA_r  *= inv_safe_min;          // scale BETA_r
             ALPHA_r *= inv_safe_min;          // scale real(ALPHA)
             ALPHA_i *= inv_safe_min;          // scale imag(ALPHA)
           }

        set_real(ALPHA, ALPHA_r);   // update ALPHA
        set_imag(ALPHA, ALPHA_i);   // update ALPHA
        BETA_r = scaled_hypot3(fabs(ALPHA_r), fabs(ALPHA_i), norm_2(X, len_X));
        if (ALPHA_r > 0.0)   BETA_r = -BETA_r;           // opposite sign of ⍺
      }

T           tau( (  BETA_r - ALPHA_r) / BETA_r);
   set_imag(tau, ( /*0.0*/ - ALPHA_i) / BETA_r);   // BETA_i is 0

   scale(X, len_X, T(1.0) / (ALPHA - BETA_r));   // scale X
   loop(k, kcnt)   BETA_r *= pos_safe_min;       // un-scale small beta

   ALPHA = T(BETA_r);

   return tau;
}
//────────────────────────────────────────────────────────────────────────────
/// In plain English: this is stage 3's "solve the triangular system"
/// step - ordinary back-substitution. A is upper triangular (it is R,
/// or rather the top N×N part of it) and B holds the right-hand side
/// (already transformed by unm2r() to account for Q). Because A is
/// upper triangular, the LAST unknown only involves the last equation
/// (a₍ₙₙ₎xₙ = bₙ, i.e. xₙ = bₙ/a₍ₙₙ₎ - no other unknowns are involved),
/// so it can be solved immediately; once xₙ is known, it can be moved
/// to the right-hand side of every equation above it (subtracting
/// a₍ᵢₙ₎×xₙ from bᵢ), which turns the SECOND-TO-LAST equation into one
/// that again only involves a single unknown, and so on upward. That
/// is exactly what the loop below does, one column of B (one right-hand
/// side) and one row of A at a time, from the bottom row up.
///
/// LApack function trsm. Solves op(A) * X = alpha * B
//  The result is stored in the first NRHS columns of B
//  Only the special case needed for A⌹B is implemented.

template<typename T>
void LA_pack::trsm(const fMatrix<T> & A, fMatrix<T> & B, Ccol NRHS)
{
   /* only: SIDE   = 'Left'              → lside == true
            UPLO   = 'Upper'             → upper = true
            TRANSA = 'No transpose'      → A is not conjugate transposed
            DIAG   = 'Non-unit' and      → nounit = true
            ALPHA  = 1.0                 → no scaling
            op     = A                   → neither A°∘T nor A°∘H
     
      is implemented! is upper triangular, therefore the computation
      of B starts at the last equation xₙ = bₙ  and moves upwards by
      repeatedly subtracting the xₙ column on the right side from the
      b column on the left side. In every step:

       a₁₁ x₁ + a₁₂ x₂ + ... + a₁ₙ xₙ₋₁ + a₁ₙ xₙ = b₁
          0   + a₂₂ x₂ + ... + a₂ₙ xₙ₋₁ + a₂ₙ xₙ = b₂
                        ...         
          0   +    0   + ... + aₙₙ xₙ₋₁ + aₙₙ xₙ = bₙ

       becomes:

       a₁₁ x₁ + a₁₂ x₂ + ... + a₁ₙ xₙ₋₁       = b₁   - a₁ₙ     (bₙ ÷ aₙₙ)
          0   + a₂₂ x₂ + ... + a₂ₙ xₙ₋₁       = b₂   - a₂ₙ     (bₙ ÷ aₙₙ)
                                             ...
          0   +    0   + ... + a₍ₙ₋₁₎ₙ xₙ₋₁   = bₙ₋₁ - a₍ₙ₋₁₎ₙ (bₙ ÷ aₙₙ)

       and the scalar result xₙ = bₙ ÷ aₙₙ in each step is stored in B[col;n].
    */
   ALL_COLS(NRHS)
   REV_COLS(A.get_column_count())
      {
        T & B_kj = B.at(k, col);   // result bₙ
        if (is_nonzero(B_kj))
           {
             B_kj /= A.diag(k);
             ALL_ROWS(k)   B.at(row, col) -= B_kj * A.at(row, k);
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
/// In plain English: a small speed optimization for larf(), nothing
/// mathematically deep. C's columns from left to right are: some
/// columns with data, then (usually) a run of all-0 columns at the
/// right edge (because earlier reflectors already zeroed them out).
/// There's no point asking gemv()/gerc() to do arithmetic on columns
/// that are entirely 0, so this just finds where that all-0 tail
/// starts, scanning from the right (since it's normally short, this is
/// fast) and returns the count of columns to actually process.
///
/// LApack function ila_lc
template<typename T>
Ccol LA_pack::ila_lc(Crow M, const fMatrix<T> & C)
{
   /* return the smallest col so that C(1:M, col:N) is the null matrix:
 
       ├──────── N ────────┤
       ╔═════════C═══╤═════╗ ┬
       ║             │     ║ │
       ║           ≠0│  0  ║ M
       ║             │     ║ │
       ║             └─────╢ ┴
       ║           ↑  ↑    ║
       ╚═══════════│══│════╝
                   │  │
                   │  └── ila_lc(M, N, C): first column of a trailing 0-block
                   └───── !column.is_null(M)   (last non-0 column of C)
    */
const Ccol N = C.get_column_count();

   REV_COLS(N)   // backards from the last column
   ALL_ROWS(M)   // down from the first row
      {
        if (is_nonzero(C.at(row, k)))   return k + 1;
      }

   /* at this point all columns in the first M rows of C are 0.

       ├──────── N ────────┤
       ╔═════════C═════════╗ ┬
       ║                   ║ │
       ║         0         ║ M
       ║                   ║ │
       ╟───────────────────╢ ┴
       ║ c c c c c c c c c ║
       ╚═══════════════════╝
    */
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
/* In plain English: this computes v°∘C, i.e. one number per column of
   C - the dot product of the reflector vector v with that column. It
   is the first of the two cheap operations larf() uses instead of
   building H explicitly (see larf() above). "gemv" is BLAS-speak for
   "GEneral Matrix-Vector multiply".

   LApack function gemv. Multiply every (conjugated) partial column vector
   M↑(+C)[;col] of C with vector x. Set the row vector y to the sums
   of the products.

      ├──────N──────┤            
    ┬ ╔════╤═╤═+C╤══╤═══╗   ┌───┐
    │ ║    │ │   │  │   ║   │   │
    │ ║    │ │   │  │   ║   │   │
    M ║    │∑│   │  │   ║ × │ x │
    │ ║    │ │   │  │ 0 ║   │   │
    │ ║    │ │   │  │   ║   │   │
    ┴ ╟────┴─┴───┴──┘   ║   └───┘
      ║     ↓           ║     0  
      ╚═════↓═══════════╝        
            ↓    
      ┌─────────────┐            
      │      y      │ 0
      └─────────────┘            

   NOTE: in order to avoid useless sums and products with 0, the caller has
         computed a submatrix M N↑C of C with non-zero items in C and x.
         Therefore, in general, M N≤⍴C but N↓y is 0.


   gemv() is simply one step of some matrix multiplication +/ A[row;]×B[;col].
   x is the row of some other matrix which is multiplied with every column
   of C, and the products are then cumulated to produce Y.
 */
template<typename T>
inline void LA_pack::gemv(const fMatrix<T> & C, Crow M, Ccol N,
                          const T * x, T * y)
{
   ALL_COLS(N)
      {
        T sum(0.0);   // column sum
        ALL_ROWS(M)   sum += conjugated(C.at(row, col)) * x[row];
        y[col] = sum;
      }
}
//────────────────────────────────────────────────────────────────────────────
/* In plain English: this subtracts tau × v × y° from (a submatrix of) C,
   where y is the vector gemv() just computed - i.e. it finishes the
   H∘C = C - tau×v∘(v°∘C) computation that larf() needs (see larf()
   above), by doing the "outer product and subtract" half of it. This
   is a classic "rank-1 update" (it changes C by something that, as a
   standalone matrix, only has rank 1): every entry C[m,n] gets
   ALPHA×x[m]×conj(y[n]) added to it. "gerc" is BLAS-speak for "GEneral
   Rank-1 update, Conjugated" (the plain, non-conjugated version used
   for real numbers is called "ger" in BLAS).

   LApack function gerc. Let SUB←M N↑C.

   C[m; n] ← C[m; n] + ALPHA × x[m] × y[n] for SUB ←→ M N↑C


            ├────N────┤            
            ┌─────────┐            
            │   +y    │ 0          
            └─────────┘            
                 ×                
               ALPHA
                 ×                
            ╔══════C══╤═══╗   ┌───┐ ┬
            ║         │   ║   │   │ │
            ║         │   ║   │   │ │
        C ← ║   SUB   │   ║ × │ x │ M
            ║         │   ║   │   │ │
            ║         │   ║   │   │ │
            ╟─────────┘   ║   └───┘ ┴
            ║             ║     0  
            ╚═════════════╝        
 */
template<typename T>
void LA_pack::gerc(fMatrix<T> & C, Crow M, Ccol N,
                   T ALPHA, const T * x, const T * y)
{
   if (!(M && N && is_nonzero(ALPHA)))   return;

   ALL_COLS(N)
      {
        if (is_zero(y[col]))   continue;

        const T Ay = conjugated(y[col]) * ALPHA;       // alpha * y°
        ALL_ROWS(M)   C.at(row, col) += Ay * x[row];   //
      }
}
//────────────────────────────────────────────────────────────────────────────
/** In plain English: this applies the mirror H that larfg() built (as
    a tau/v pair, not as an explicit matrix) to some matrix C, computing
    H∘C, again without ever forming H explicitly (forming an M×M matrix
    just to throw it away after one multiplication would be wasteful,
    especially since H = I - tau×v∘v° is 1 minus a simple, cheap-to-use
    correction).

    Instead, the identity H∘C = C - tau×v∘(v°∘C) is used directly:
    v°∘C (a matrix-vector-like product, computed by gemv() below) gives
    one number per column of C, and then subtracting tau×v times that
    from C (a rank-1 update, computed by gerc() below) finishes the
    job. Both of those are cheap O(rows×cols) operations, compared to
    the O(rows²×cols) it would cost to first build H and then multiply.
    ila_lc() (further below) is a small additional optimization: it
    first finds how many columns of C on the right are already all-0
    (nothing to do there) so gemv()/gerc() don't waste time on them.

    LApack function larf: applies the elementary reflector H to the
    rectangular matrix C.

   LARF applies an elementary reflector H to an M-by-N matrix C,
   from either the left or the right. The Householder matrix H
   is represented by the scalar tau and the vector v in the form:

       H = I - tau × v ∘ v°

   That is:

             ⎧ 1.0 - tau × v²   if i=j
       Hᵢⱼ = ⎨
             ⎩  tau × vᵢ × vⱼ   if i≠j

   where tau is a scalar and v is a vector. Instead of creating the entire
   matrix H beforehand, LApack generally uses the vector v instead and
   computes Hᵢⱼ when needed).

   NOTE: If tau = 0, then H is the unit matrix I and applying H is a no-op.

   C is overwritten in place by the matrix H ∘ C.
 */
template<typename T>
void LA_pack::larf(const T * v, T tau, fMatrix<T> & C, T * y)
{
   if (is_zero(tau))   return;   // H is I.

const Crow M = C.get_row_count();

   // NOTE: only SIDE == "Left" in FORTRAN is needed (and implemented here).

   /*
                  ├──────N──────┤
          ┬ ┌─┐   ╔══════C═╤════╗ ┬
          │ │v│   ║        │0 0 ║ │
      len_v │v│   ║       c│0 0 ║ │
          │ │v│   ║        │0 0 ║ M   v≠0, c≠0
          ┴ │v│ → ╟────────┘0 0 ║ │
        ↑   │0│   ║       │     ║ │
        ↑   │0│   ║       │     ║ │
        M   └─┘   ╚═══════│═════╝ ┴
                      lastC ← N
    */

   // compute len_v = the length of v without trailing zeroes of v.
   // Decreasing M to len_v speeds up the computation by not adding up
   // products v×Cᵢⱼ that are obviously 0.
   //
Crow len_v = M;   // significant length of v, len_v ≤ M
   while (len_v && is_zero(v[len_v - 1]))   --len_v;

   if (len_v == 0)   return;   // no-op if v is the null vector

   // in the quadratic sub-matrix C(1:len_v, 1:len_v): compute the
   // last column lastC that has a nonzero item.
   //
const Ccol lastC = ila_lc<T>(len_v, C);

   // (len_v lastC)↑C is the submatrix SUB of concern in gemv() and gerc()
   //
   gemv<T>(C, len_v, lastC,       v, y);   // set y[]
   gerc<T>(C, len_v, lastC, -tau, v, y);   // use y[]
}
//════════════════════════════════════════════════════════════════════════════
