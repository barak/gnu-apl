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

#ifndef __SCALAR_OPS_HH_DEFINED__
#define __SCALAR_OPS_HH_DEFINED__

#include "APL_types.hh"   // for ShapeItem

/// Dyadic worker: packed float64 × float64 → float64.
/// incA/incB are 0 (scalar extension) or 1 (full vector).
typedef void (*vv_f2f_t)(double * pZ,
                          const double * pA, int incA,
                          const double * pB, int incB,
                          ShapeItem N);

/// Monadic worker: packed float64 → float64.
typedef void (*v_f2f_t)(double * pZ, const double * pB, ShapeItem N);

/// Dyadic worker: packed int64 × int64 → int64.
/// incA/incB are 0 (scalar extension) or 1 (full vector).
typedef void (*vv_i2i_t)(int64_t * pZ,
                          const int64_t * pA, int incA,
                          const int64_t * pB, int incB,
                          ShapeItem N);

/// Monadic worker: packed int64 → int64.
typedef void (*v_i2i_t)(int64_t * pZ, const int64_t * pB, ShapeItem N);

/// Dyadic worker: packed int64 × int64 → bit-packed bool ravel.
/// incA/incB are 0 (scalar extension) or 1 (full vector).
typedef void (*vv_i2b_t)(uint64_t * pZ,
                           const int64_t * pA, int incA,
                           const int64_t * pB, int incB,
                           ShapeItem N);

/// Dyadic worker: packed float64 × float64 → bit-packed bool ravel.
/// ct is ⎕CT, read once by the caller to avoid pulling in Workspace.hh here.
typedef void (*vv_f2b_t)(uint64_t * pZ,
                           const double * pA, int incA,
                           const double * pB, int incB,
                           ShapeItem N, double ct);

/// Dyadic worker: packed unicode16 × unicode16 → bit-packed bool ravel.
/// incA/incB are 0 (scalar extension) or 1 (full vector).
typedef void (*vv_c16_2b_t)(uint64_t * pZ,
                              const uint16_t * pA, int incA,
                              const uint16_t * pB, int incB,
                              ShapeItem N);

/// Dyadic worker: packed unicode32 × unicode32 → bit-packed bool ravel.
/// incA/incB are 0 (scalar extension) or 1 (full vector).
typedef void (*vv_c32_2b_t)(uint64_t * pZ,
                              const Unicode * pA, int incA,
                              const Unicode * pB, int incB,
                              ShapeItem N);

/// Dyadic worker: packed bit × packed bit → packed bit.
/// inc=0: scalar (single bit at bit 0 of word 0); inc=1: N-bit vector.
typedef void (*vv_b2b_t)(uint64_t * pZ,
                          const uint64_t * pA, int incA,
                          const uint64_t * pB, int incB,
                          ShapeItem N);

/// Monadic worker: packed bit → packed bit (e.g. logical NOT).
typedef void (*v_b2b_t)(uint64_t * pZ, const uint64_t * pB, ShapeItem N);

/// Dyadic worker: packed complex (re,im double pairs) × complex → complex.
/// inc=0: scalar (two doubles at pA[0..1]); inc=1: N-element array.
typedef void (*vv_z2z_t)(double * pZ,
                          const double * pA, int incA,
                          const double * pB, int incB,
                          ShapeItem N);

/// Monadic worker: packed complex → complex.
typedef void (*v_z2z_t)(double * pZ, const double * pB, ShapeItem N);

/// All dyadic packed fast-path workers collected for one dispatch call.
struct ScalarFastPaths_AB
{
   vv_f2f_t     f2f  = nullptr;
   vv_i2i_t     i2i  = nullptr;
   vv_i2b_t     i2b  = nullptr;
   vv_f2b_t     f2b  = nullptr;
   vv_c16_2b_t  c16b = nullptr;
   vv_c32_2b_t  c32b = nullptr;
};

/// All monadic packed fast-path workers collected for one dispatch call.
struct ScalarFastPaths_B
{
   v_f2f_t  v_f = nullptr;
   v_i2i_t  v_i = nullptr;
};

#endif // __SCALAR_OPS_HH_DEFINED__
