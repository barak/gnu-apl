/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

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

#include <stdlib.h>

#include <iostream>

#include "Common.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Output.hh"
#include "Value.hh"
#include "Workspace.hh"

using namespace std;

// the implementation of gelsy<T>
#include "LApack.hh"

//-----------------------------------------------------------------------------
void
divide_matrix(Cell * cZ, bool need_complex,
              ShapeItem rows, ShapeItem cols_A, const Cell * cA,
              ShapeItem cols_B, const Cell * cB)
{
   // the following has been checked by the caller:
   //
   // rows_B >= cols_B and
   // rows_A == rows_B  (aka. rows below)
   //
   loop(c, cols_A)
       {
         if (need_complex)
            {
              APL_Float * ad = reinterpret_cast<APL_Float *>(
                               malloc(rows * cols_A * 2*sizeof(APL_Float)));
              if (ad == 0)   WS_FULL
              ZZ * const a = reinterpret_cast<ZZ *>(ad);
              loop(r, rows)
                  {
                    new (a + r) ZZ(cA[r*cols_A + c].get_real_value(),
                                   cA[r*cols_A + c].get_imag_value());
                  }

              APL_Float * bd = reinterpret_cast<APL_Float *>(
                               malloc(rows * cols_B * 2*sizeof(APL_Float)));
              if (bd == 0)   { free(a);   WS_FULL }

              ZZ * const b = reinterpret_cast<ZZ *>(bd);
              ZZ * bb = b;
              loop(rr, cols_B)
              loop(cc, rows)
                 {
                   new (bb++) ZZ(cB[rr + cc*cols_B].get_real_value(),
                                 cB[rr + cc*cols_B].get_imag_value());
                 }

              {
                const APL_Float rcond = Workspace::get_CT();

                Matrix<ZZ> B(b, rows, cols_B, /* LDB */ rows);
                Matrix<ZZ> A(a, rows, 1,      /* LDA */ rows);
                const ShapeItem rk = gelsy<ZZ>(B, A, rcond);
                free(bd);
                if (rk != cols_B)
                   {
                     free(ad);
                     DOMAIN_ERROR;
                   }
              }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //
              loop(r, cols_B)
                  new (cZ + r*cols_A + c) ComplexCell(a[r].real(), a[r].imag());
              free(ad);
            }
         else   // real
            {
              APL_Float * a = reinterpret_cast<APL_Float *>(
                              malloc(rows * cols_A * sizeof(APL_Float)));
              if (a == 0)   WS_FULL
              loop(r, rows)
                 {
                   a[r] = cA[r*cols_A + c].get_real_value();
                 }

              APL_Float * b = reinterpret_cast<APL_Float *>(
                              malloc(rows * cols_B * sizeof(APL_Float)));
              if (b == 0)   { free(a);   WS_FULL }

              APL_Float * bb = b;
              loop(rr, cols_B)
              loop(cc, rows)
                 {
                   *bb++ = cB[rr + cc*cols_B].get_real_value();
                 }

              {
                const APL_Float rcond = Workspace::get_CT();

                Matrix<DD> B(b, rows, cols_B, /* LDB */ rows);
                Matrix<DD> A(a, rows, 1,      /* LDA */ rows);
                const ShapeItem rk = gelsy<DD>(B, A, rcond);
                free(b);
                if (rk != cols_B)
                   {
                     free(a);
                     DOMAIN_ERROR;
                   }
              }

              // cols_A = rows_Z. We have computed the result for col c of A
              // which is row c of Z.
              //

              loop(r, cols_B)
                  new (cZ + r*cols_A + c)   FloatCell(a[r]);
              free(a);
            }
       }
}
//-----------------------------------------------------------------------------
