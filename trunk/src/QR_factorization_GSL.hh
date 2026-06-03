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

//════════════════════════════════════════════════════════════════════════════

#if apl_GSL
#  include <gsl/gsl_linalg.h>
#else
class gsl_matrix;
#endif

class Value;
class Cell;

class GSL
{
public:
   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param B input APL matrix value
   /// @param need_complex true if complex arithmetic is required
   static void LQ_factorize(Value & Z, int M, int N, Value_P B,
                            bool need_complex);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param cB pointer to first cell of the input ravel (real matrix)
   static void LU_factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param cB pointer to first cell of the input ravel (complex matrix)
   static void LU_factorize_ZZ_matrix(Value & Z, int M, int N, const Cell * cB);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param cB pointer to first cell of the input ravel (real matrix)
   static void QL_factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param cB pointer to first cell of the input ravel (real matrix)
   static void QR_factorize_DD_matrix(Value & Z, int M, int N, const Cell * cB);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param cB pointer to first cell of the input ravel (complex matrix)
   static void QR_factorize_ZZ_matrix(Value & Z, int M, int N, const Cell * cB);

   /// @param Z output value receiving the factorization result
   /// @param M number of rows in the input matrix
   /// @param N number of columns in the input matrix
   /// @param B input APL matrix value
   static void RQ_factorize(Value & Z, int M, int N, Value_P B);

protected:
  /// @param reason human-readable error description from GSL
  /// @param file source file where the GSL error occurred
  /// @param line line number where the GSL error occurred
  /// @param gsl_errno GSL error code
  static void GSL_error_handler(const char * reason, const char * file,
                                int line, int gsl_errno);

  /// invert the M×M matrix Q in place
  /// @param Q GSL matrix to invert in place
  /// @param M dimension of the square matrix
  static int invert_matrix(gsl_matrix * Q, size_t M);

  static void set_GSL_error_handler();
};
//════════════════════════════════════════════════════════════════════════════
