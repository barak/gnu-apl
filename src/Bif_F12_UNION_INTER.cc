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

#include "ArgCheck.hh"
#include "Bif_F12_UNION_INTER.hh"
#include "Heapsort.hh"
#include "Value.hh"
#include "Workspace.hh"

// primitive function instances
//
Bif_F12_UNION     Bif_F12_UNION    ::fun;    // ∪
Bif_F2_INTER      Bif_F2_INTER     ::fun;    // ∩

//════════════════════════════════════════════════════════════════════════════
Token
Bif_F12_UNION::eval_AB(cValue_R A, cValue_R B) const
{
   /*
      NOTE: Neither IBM APL2 nor ISO define dyadic A ∪ B.
            Dyalog APL defines it as:

            A ∪ B ←→ A , (B ∼ A)

            However, that definition suffers from asymmetry: duplicated items
            in A remain duplicated while duplicated items in B are removed.

            We therefore define dyadic A ∪ B as:

            A ∪ B ←→ ∪ (A , B)

            which is simpler, symmetrical, and closer to the mathematical
            definition of a set.
    */
   ArgCheck::require_scalar_or_vector("A∪B", "A", A);
   ArgCheck::require_scalar_or_vector("A∪B", "B", B);

const ShapeItem len_A = A.element_count();
const ShapeItem len_B = B.element_count();

   // Z←A, B
   //
Value_P Z(len_A + len_B, LOC);

   loop(a, len_A)
       { Cell cache; Z->next_ravel_Cell(A.get_cravel(a, cache)); }
   loop(b, len_B)
       { Cell cache; Z->next_ravel_Cell(B.get_cravel(b, cache)); }
   Z->set_default(A, LOC);   // prototype from A, not B (Bugs28 #65,
                              // mirror image of Bugs27 #31)
   Z->check_value(LOC);
   return eval_B(*Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_F12_UNION::eval_B(cValue_R B) const
{
   // ∪B : Unique. The items of B without duplicates

   ArgCheck::require_scalar_or_vector("∪B", "B", B);

const ShapeItem len_B = B.element_count();
   if (len_B <= 1)   return Token(TOK_APL_VALUE1, CLONE(&B, LOC));

   // 1. create a vector with all cells of B and sort it.
   //
vector<const Cell *> cells_B;
vector<uint8_t> stable_B_mem;
   // matching Bif_F2_INTER::eval_AB()'s guard just below in this file:
   // a bare reserve()/resize() on a very large B raises a C++
   // std::bad_alloc instead of the APL WS FULL its sibling produces
   // (Bugs18 #16).
   //
   try {
         cells_B.reserve(len_B);
         stable_B_mem.resize(len_B * sizeof(Cell));
       } catch (std::bad_alloc &) { WS_FULL; }
         catch (...)              { FIXME; }

   {
     Cell * p = reinterpret_cast<Cell *>(stable_B_mem.data());
     loop(b, len_B)   cells_B.push_back(&B.get_cravel(b, p[b]));
   }
   Heapsort<const Cell *>::sort(cells_B, Cell::compare_stable, 0);

   // 2. remove duplicates
   //
const double qct = Workspace::get_CT();
vector<const Cell *> unique;
   unique.reserve(len_B);
   for (ShapeItem b = 0; b < len_B; )
       {
         const Cell * u = cells_B[b++];
         unique.push_back(u);
         while (b < len_B && u->equal(*cells_B[b], qct))   ++b;   // duplicate
       }

   // 3. restore original order and create the result
   //
   Heapsort<const Cell *>::sort(unique, Cell::compare_ptr, 0);

const ShapeItem len_Z = unique.size();
Value_P Z(len_Z, LOC);
   loop(u, len_Z)   Z->next_ravel_Cell(*unique[u]);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_F2_INTER::eval_AB(cValue_R A, cValue_R B) const
{
   ArgCheck::require_scalar_or_vector("A∩B", "A", A);
   ArgCheck::require_scalar_or_vector("A∩B", "B", B);

const ShapeItem len_A = A.element_count();
const ShapeItem len_B = B.element_count();

const double qct = Workspace::get_CT();

   if (len_A*len_B > 60*60)
      {
        // large A or B: sort A and B to speed up searches
        //
        vector<const Cell *> cells_A;
        vector<const Cell *> cells_B;
        vector<const Cell *> cells_Z;
        vector<uint8_t> stable_A_mem, stable_B_mem;
        try {
              cells_A.reserve(len_A);
              cells_B.reserve(len_B);
              cells_Z.reserve(len_A + len_B);   // worst case
              stable_A_mem.resize(len_A * sizeof(Cell));
              stable_B_mem.resize(len_B * sizeof(Cell));
            } catch (std::bad_alloc &) { WS_FULL; }
              catch (...)              { FIXME; }

        {
          Cell * p = reinterpret_cast<Cell *>(stable_A_mem.data());
          loop(a, len_A)   cells_A.push_back(&A.get_cravel(a, p[a]));
        }

        {
          Cell * p = reinterpret_cast<Cell *>(stable_B_mem.data());
          loop(b, len_B)   cells_B.push_back(&B.get_cravel(b, p[b]));
        }

        Heapsort<const Cell *>::sort(cells_A, Cell::compare_stable, 0);
        Heapsort<const Cell *>::sort(cells_B, Cell::compare_stable, 0);

        ShapeItem idx_B = 0;
        loop(idx_A, len_A)
            {
              const Cell * ref = cells_A[idx_A];
              while (idx_B < len_B)
                  {
                    if (ref->equal(*cells_B[idx_B], qct))
                       {
                         cells_Z.push_back(ref);   // A is in B
                         break;   // for idx_B → next idx_A
                       }

                    // B is much (by ⎕CT) smaller or greater than A
                    //
                    if (ref->greater(*cells_B[idx_B]))    ++idx_B;
                    else                                  break;
                 }
            }

        // sort cells_Z by position so that the original order in A is
        //  reconstructed
        //
        Heapsort<const Cell *>::sort(cells_Z, Cell::compare_ptr, 0);
        Value_P Z(cells_Z.size(), LOC);
        loop(z, cells_Z.size())   Z->next_ravel_Cell(*cells_Z[z]);

        Z->set_default(A, LOC);   // prototype from A, not B (Bugs27 #31)
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
    else
      {
        // small A and B: use quadratic time algorithm.
        // Collect matching A indices rather than Cell pointers: a Cell
        // reference from get_cravel(idx, cache) is only valid until its
        // caller-owned cache is reused or goes out of scope.
        //
        vector<ShapeItem> indices_Z;
        indices_Z.reserve(len_A);

        Cell a_cache;
        for (ShapeItem a = 0; a < len_A; ++a)
            {
              const Cell & ca = A.get_cravel(a, a_cache);
              Cell b_cache;
              loop(b, len_B)
                  {
                    if (ca.equal(B.get_cravel(b, b_cache), qct))
                       {
                         indices_Z.push_back(a);
                         break;
                       }
                  }
            }

        Value_P Z(indices_Z.size(), LOC);
        Cell iz_cache;
        loop(z, indices_Z.size())
            Z->next_ravel_Cell(A.get_cravel(indices_Z[z], iz_cache));

        Z->set_default(A, LOC);   // prototype from A, not B (Bugs27 #31)
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }
}
//════════════════════════════════════════════════════════════════════════════
