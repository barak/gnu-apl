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

#include "Ravel.hh"
#include "Value.hh"
#include "ScalarFunction.hh"
#include "Workspace.hh"

static_assert(sizeof(IntRavel)   == sizeof(Ravel),
              "IntRavel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(FloatRavel) == sizeof(Ravel),
              "FloatRavel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(Char16Ravel) == sizeof(Ravel),
              "Char16Ravel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(MemberNameRavel) == sizeof(Ravel),
              "MemberNameRavel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(Char32Ravel) == sizeof(Ravel),
              "Char32Ravel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(BoolRavel) == sizeof(Ravel),
              "BoolRavel must not add data members (vtable upgrade via placement-new)");
static_assert(sizeof(ComplexRavel) == sizeof(Ravel),
              "ComplexRavel must not add data members (vtable upgrade via placement-new)");

//────────────────────────────────────────────────────────────────────────────
/// true if the int64 ravels of A and B can be compared exactly (see
/// IntCell::ints_compare_exactly())
static bool
ints_compare_exactly(cValue_R A, cValue_R B)
{
const double qct = Workspace::get_CT();
   return IntCell::ints_compare_exactly(A.cravel_int64(), A.element_count(),
                                        qct)
       && IntCell::ints_compare_exactly(B.cravel_int64(), B.element_count(),
                                        qct);
}
//────────────────────────────────────────────────────────────────────────────
bool
Ravel::apply_fast_dyadic(const ScalarFunction & sf,
                          cValue_R A, int inc_A,
                          cValue_R B, int inc_B,
                          Value & Z, ShapeItem len_Z) const
{
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;

   switch (A.get_ravel_type())
      {
        case RPT_FLOAT64:
           if (B.get_ravel_type() != RPT_FLOAT64)   break;
           if (const vv_f2f_t fn = sf.get_vv_f2f())
              {
                double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
                fn(pZ, A.cravel_float64(), inc_A,
                       B.cravel_float64(), inc_B, len_Z);
                Z.commit_ravel_Float64(len_Z);
                return true;
              }
           if (const vv_f2b_t fn = sf.get_vv_f2b())
              {
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_float64(), inc_A,
                       B.cravel_float64(), inc_B, len_Z, Workspace::get_CT());
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_INT64:
           // IntRavel vtable is only installed for heap ravels (N > cfg_SHORT_VALUE_LENGTH_WANTED).
           // Short ravels with RPT_INT64 still dispatch here via base Ravel.
           if (B.get_ravel_type() != RPT_INT64)   break;
           if (const vv_i2i_t fn = sf.get_vv_i2i())
              {
                int64_t * pZ = reinterpret_cast<int64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_int64(), inc_A,
                       B.cravel_int64(), inc_B, len_Z);
                Z.commit_ravel_Int64(len_Z);
                return true;
              }
           if (const vv_i2b_t fn = sf.get_vv_i2b())
              {
                // the comparisons are exact: only if tolerant comparison
                // gives the same (Bugs35 #34)
                if (!ints_compare_exactly(A, B))   break;
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_int64(), inc_A,
                       B.cravel_int64(), inc_B, len_Z);
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_UNICODE16:
           if (B.get_ravel_type() != RPT_UNICODE16)   break;
           if (const vv_c16_2b_t fn = sf.get_vv_c16_2b())
              {
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_unicode16(), inc_A,
                       B.cravel_unicode16(), inc_B, len_Z);
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_UNICODE32:
           if (B.get_ravel_type() != RPT_UNICODE32)   break;
           if (const vv_c32_2b_t fn = sf.get_vv_c32_2b())
              {
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_unicode32(), inc_A,
                       B.cravel_unicode32(), inc_B, len_Z);
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_BOOL:
           // BoolRavel vtable only installed for heap ravels.
           // Short bool ravels dispatch here via base Ravel.
           if (B.get_ravel_type() != RPT_BOOL)   break;
           if (const vv_b2b_t fn = sf.get_vv_b2b())
              {
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, A.cravel_bool(), inc_A,
                       B.cravel_bool(), inc_B, len_Z);
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_COMPLEX:
           // ComplexRavel vtable only installed for heap ravels.
           if (B.get_ravel_type() != RPT_COMPLEX)   break;
           if (const vv_z2z_t fn = sf.get_vv_z2z())
              {
                double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
                fn(pZ, A.cravel_complex(), inc_A,
                       B.cravel_complex(), inc_B, len_Z);
                Z.commit_ravel_Complex(len_Z);
                return true;
              }
           break;

        default:
           break;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Ravel::apply_fast_monadic(const ScalarFunction & sf,
                           cValue_R B,
                           Value & Z, ShapeItem len_Z) const
{
   if (B.get_pointer_cell_count())
      return false;

   switch (B.get_ravel_type())
      {
        case RPT_FLOAT64:
           if (const v_f2f_t fn = sf.get_v_f2f())
              {
                double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
                fn(pZ, B.cravel_float64(), len_Z);
                Z.commit_ravel_Float64(len_Z);
                return true;
              }
           break;

        case RPT_INT64:
           // IntRavel vtable is only installed for heap ravels (N > cfg_SHORT_VALUE_LENGTH_WANTED).
           // Short ravels with RPT_INT64 still dispatch here via base Ravel.
           if (const v_i2i_t fn = sf.get_v_i2i())
              {
                int64_t * pZ = reinterpret_cast<int64_t *>(&Z.get_wfirst());
                fn(pZ, B.cravel_int64(), len_Z);
                Z.commit_ravel_Int64(len_Z);
                return true;
              }
           break;

        case RPT_BOOL:
           // Short bool ravels dispatch here via base Ravel.
           if (const v_b2b_t fn = sf.get_v_b2b())
              {
                uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
                fn(pZ, B.cravel_bool(), len_Z);
                Z.commit_ravel_Bool(len_Z);
                return true;
              }
           break;

        case RPT_COMPLEX:
           // Short complex ravels dispatch here via base Ravel.
           if (const v_z2z_t fn = sf.get_v_z2z())
              {
                double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
                fn(pZ, B.cravel_complex(), len_Z);
                Z.commit_ravel_Complex(len_Z);
                return true;
              }
           break;

        default:
           break;
      }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
IntRavel::apply_fast_dyadic(const ScalarFunction & sf,
                             cValue_R A, int inc_A,
                             cValue_R B, int inc_B,
                             Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_INT64);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_INT64)   return false;
   if (const vv_i2i_t fn = sf.get_vv_i2i())
      {
        int64_t * pZ = reinterpret_cast<int64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_int64(), inc_A,
               B.cravel_int64(), inc_B, len_Z);
        Z.commit_ravel_Int64(len_Z);
        return true;
      }
   if (const vv_i2b_t fn = sf.get_vv_i2b())
      {
        // the comparisons are exact: only if tolerant comparison gives the
        // same (Bugs35 #34)
        if (!ints_compare_exactly(A, B))   return false;
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_int64(), inc_A,
               B.cravel_int64(), inc_B, len_Z);
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
IntRavel::apply_fast_monadic(const ScalarFunction & sf,
                              cValue_R B,
                              Value & Z, ShapeItem len_Z) const
{
   Assert(B.get_ravel_type() == RPT_INT64);
   if (B.get_pointer_cell_count())
      return false;
   if (const v_i2i_t fn = sf.get_v_i2i())
      {
        int64_t * pZ = reinterpret_cast<int64_t *>(&Z.get_wfirst());
        fn(pZ, B.cravel_int64(), len_Z);
        Z.commit_ravel_Int64(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatRavel::apply_fast_dyadic(const ScalarFunction & sf,
                               cValue_R A, int inc_A,
                               cValue_R B, int inc_B,
                               Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_FLOAT64);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_FLOAT64)   return false;
   if (const vv_f2f_t fn = sf.get_vv_f2f())
      {
        double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
        fn(pZ, A.cravel_float64(), inc_A,
               B.cravel_float64(), inc_B, len_Z);
        Z.commit_ravel_Float64(len_Z);
        return true;
      }
   if (const vv_f2b_t fn = sf.get_vv_f2b())
      {
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_float64(), inc_A,
               B.cravel_float64(), inc_B, len_Z, Workspace::get_CT());
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
FloatRavel::apply_fast_monadic(const ScalarFunction & sf,
                                cValue_R B,
                                Value & Z, ShapeItem len_Z) const
{
   Assert(B.get_ravel_type() == RPT_FLOAT64);
   if (B.get_pointer_cell_count())
      return false;
   if (const v_f2f_t fn = sf.get_v_f2f())
      {
        double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
        fn(pZ, B.cravel_float64(), len_Z);
        Z.commit_ravel_Float64(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Char16Ravel::apply_fast_dyadic(const ScalarFunction & sf,
                                cValue_R A, int inc_A,
                                cValue_R B, int inc_B,
                                Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_UNICODE16);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_UNICODE16)   return false;
   if (const vv_c16_2b_t fn = sf.get_vv_c16_2b())
      {
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_unicode16(), inc_A,
               B.cravel_unicode16(), inc_B, len_Z);
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Char16Ravel::apply_fast_monadic(const ScalarFunction & sf,
                                 cValue_R B,
                                 Value & Z, ShapeItem len_Z) const
{
   return false;   // no monadic scalar function produces a char16 result
}
//────────────────────────────────────────────────────────────────────────────
bool
Char32Ravel::apply_fast_dyadic(const ScalarFunction & sf,
                                cValue_R A, int inc_A,
                                cValue_R B, int inc_B,
                                Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_UNICODE32);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_UNICODE32)   return false;
   if (const vv_c32_2b_t fn = sf.get_vv_c32_2b())
      {
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_unicode32(), inc_A,
               B.cravel_unicode32(), inc_B, len_Z);
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
Char32Ravel::apply_fast_monadic(const ScalarFunction & sf,
                                 cValue_R B,
                                 Value & Z, ShapeItem len_Z) const
{
   return false;   // no monadic scalar function produces a char32 result
}
//────────────────────────────────────────────────────────────────────────────
bool
BoolRavel::apply_fast_dyadic(const ScalarFunction & sf,
                              cValue_R A, int inc_A,
                              cValue_R B, int inc_B,
                              Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_BOOL);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_BOOL)   return false;
   if (const vv_b2b_t fn = sf.get_vv_b2b())
      {
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, A.cravel_bool(), inc_A,
               B.cravel_bool(), inc_B, len_Z);
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
BoolRavel::apply_fast_monadic(const ScalarFunction & sf,
                               cValue_R B,
                               Value & Z, ShapeItem len_Z) const
{
   Assert(B.get_ravel_type() == RPT_BOOL);
   if (B.get_pointer_cell_count())
      return false;
   if (const v_b2b_t fn = sf.get_v_b2b())
      {
        uint64_t * pZ = reinterpret_cast<uint64_t *>(&Z.get_wfirst());
        fn(pZ, B.cravel_bool(), len_Z);
        Z.commit_ravel_Bool(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexRavel::apply_fast_dyadic(const ScalarFunction & sf,
                                 cValue_R A, int inc_A,
                                 cValue_R B, int inc_B,
                                 Value & Z, ShapeItem len_Z) const
{
   Assert(A.get_ravel_type() == RPT_COMPLEX);
   if (A.get_pointer_cell_count() || B.get_pointer_cell_count())
      return false;
   if (B.get_ravel_type() != RPT_COMPLEX)   return false;
   if (const vv_z2z_t fn = sf.get_vv_z2z())
      {
        double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
        fn(pZ, A.cravel_complex(), inc_A,
               B.cravel_complex(), inc_B, len_Z);
        Z.commit_ravel_Complex(len_Z);
        return true;
      }
   return false;
}
//────────────────────────────────────────────────────────────────────────────
bool
ComplexRavel::apply_fast_monadic(const ScalarFunction & sf,
                                  cValue_R B,
                                  Value & Z, ShapeItem len_Z) const
{
   Assert(B.get_ravel_type() == RPT_COMPLEX);
   if (B.get_pointer_cell_count())
      return false;
   if (const v_z2z_t fn = sf.get_v_z2z())
      {
        double * pZ = reinterpret_cast<double *>(&Z.get_wfirst());
        fn(pZ, B.cravel_complex(), len_Z);
        Z.commit_ravel_Complex(len_Z);
        return true;
      }
   return false;
}
