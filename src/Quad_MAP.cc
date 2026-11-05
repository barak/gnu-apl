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

#include "Quad_MAP.hh"
#include "Workspace.hh"

Quad_MAP  Quad_MAP::fun;

//════════════════════════════════════════════════════════════════════════════
/*
    8 ⎕CR A←5 2⍴'eEwWaAzZ92' ◊ 8 ⎕CR B←'Halloween' ◊ A ⎕MAP B
 */
//════════════════════════════════════════════════════════════════════════════
Token
Quad_MAP::eval_AB(cValue_R A, cValue_R B) const
{
bool recursive = false;

   /* a nested scalar A indicates recursive ⎕MAP. For example

      Map←6 2⍴'aAbBcCdDeEfF'   ⍝ lowercase to uppercase for a-f
      Map ⎕MAP B      ⍝ flat (top-level only) mapping of B
      (⊂Map) ⎕Map B   ⍝ recursive mapping of B

      if A is a nested  scalar, then setrecursive and proceed with ⊃A.
    */
Value_P A_hold_;
const cValue * pA = &A;
   if (pA->get_rank() == 0 && pA->is_pointer_cell(0))
      {
         recursive = true;
         A_hold_ = pA->get_pointer_value(0);
         pA = A_hold_.get();
      }

ShapeItem map_len = pA->get_rows();               // the number of mappings
   if      (pA->get_rank() == 0)   RANK_ERROR;
   else if (pA->get_rank() == 1)
      {
        // A is a vector, accepted to avoid the need for (N 2⍴A)
        // A[1 3 5...] are the keys while A[2 4 6...] are the mapped values
        //
        map_len = pA->element_count();
        if (map_len & 1)
           {
             MORE_ERROR() << "Odd length of A in A ⎕MAP B";
             LENGTH_ERROR;
           }
        map_len = map_len >> 1;
      }
   else if (pA->get_rank() > 2)   RANK_ERROR;
   else if (pA->get_cols() != 2)
      {
        // rank 2: every key/value access below hard-codes a 2-column
        // layout (get_cravel(2*item) etc.); a matrix with a different
        // column count desyncs map_len (= rows) from the real element
        // count, reading past the ravel (confirmed: (3 1⍴'abc')⎕MAP'a'
        // hits an out-of-bounds get_cravel()).
        MORE_ERROR() << "A in A ⎕MAP B must have exactly 2 columns "
                        "(has " << pA->get_cols() << ")";
        LENGTH_ERROR;
      }

   if (map_len == 0)
           {
             MORE_ERROR() << "A with length 0 in A ⎕MAP B";
             LENGTH_ERROR;
           }

vector<ShapeItem> indices;
   try         { indices.reserve(map_len); }
   catch (std::bad_alloc &) { WS_FULL; }
   catch (...)              { FIXME; }

   loop(m, map_len)   indices.push_back(m);

const ravel_comp_len ctx = { pA, 1};
   Heapsort<ShapeItem>::sort(indices, &Quad_MAP::greater_map, &ctx);

   // complain about duplicated keys
   //
const double qct = Workspace::get_CT();
   for (ShapeItem m = 1; m < map_len; ++m)
       {
          Cell cache1, cache2;
          const Cell & cm1 = pA->get_cravel(2*indices[m - 1], cache1);
          const Cell & cm  = pA->get_cravel(2*indices[m    ], cache2);
          if (cm1.equal(cm, qct))
             {
               const int qio = Workspace::get_IO();
               MORE_ERROR() << "Duplicate keys (e.g. A["
                            << (qio + indices[m - 1]) << "] and A["
                            << (qio + indices[m]) << "]) in 'A ⎕MAP B'";
               DOMAIN_ERROR;
             }
       }

Value_P Z = do_map(*pA, indices, B, recursive);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
int
compare_MAP(const Cell & key, const ShapeItem & item, const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);

Cell cache;
   return key.compare(rcl->value->get_cravel(2*item, cache));
}
//════════════════════════════════════════════════════════════════════════════

Value_P
Quad_MAP::do_map(const cValue & A, const vector<ShapeItem> & ordered_indices_A,
                 cValue_R B, bool recursive)
{
Value_P Z(B.get_shape(), LOC);         // the result, ⍴Z ←→ ⍴B

const ravel_comp_len ctx = { &A, 1};

const ShapeItem len_B = B.element_count();
   if (len_B == 0)   // empty value
      {
         Cell cache_B;
         const Cell & cell_B = B.get_cravel(0, cache_B);
         if (const ShapeItem * map =
                   Heapsort<ShapeItem>::search<const Cell &>
                                              (cell_B,
                                               ordered_indices_A,
                                               compare_MAP,
                                               &ctx))
            {
              Cell cache_A;
              const Cell & cell_A = A.get_cravel(*map*2 + 1, cache_A);
              if (cell_A.is_pointer_cell())
                 {
                   Cell & cell_Z0 = Z->get_wproto();
                   cell_A.init_other(&cell_Z0, *Z, LOC);
                   cell_Z0.get_pointer_value()->to_type(false);
                 }
              else if (cell_A.is_character_cell())
                 Z->set_proto_Spc();
              else if (cell_A.is_numeric())
                 Z->set_proto_Int();
              else
                 DOMAIN_ERROR;
            }
         else   // not mapped, simple
            {
              Z->set_default(cell_B, LOC);
            }
      }

   loop(b, len_B)
       {
         Cell cache_B;
         const Cell & cell_B = B.get_cravel(b, cache_B);
         if (const ShapeItem * map = Heapsort<ShapeItem>::search<const Cell &>
                   (cell_B, ordered_indices_A, compare_MAP, &ctx))
            {
             Cell cache_A;
             Z->next_ravel_Cell(A.get_cravel(*map*2 + 1, cache_A));
            }
         else   // cell_B shall not be mapped
            {
              if (recursive && cell_B.is_pointer_cell())   // nested: recursive
                 {
                   Value_P sub_B = cell_B.get_pointer_value();
                   Value_P sub_Z = do_map(A, ordered_indices_A,
                                          *sub_B, true);
                   Z->next_ravel_Pointer(sub_Z.get());
                 }
              else   // not mapped, simple
                 {
                  Z->next_ravel_Cell(cell_B);
                 }
            }
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_MAP::greater_map(const ShapeItem & a, const ShapeItem & b,
                      const void * ctx)
{
const ravel_comp_len * rcl = reinterpret_cast<const ravel_comp_len *>(ctx);

Cell cache_a, cache_b;
   if (const Comp_result cr = rcl->value->get_cravel(2*a, cache_a)
                                  .compare(rcl->value->get_cravel(2*b, cache_b)))
      return cr == COMP_GT;

   return a > b;
}
//════════════════════════════════════════════════════════════════════════════
