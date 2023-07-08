/*
    This file is a port of the LApack 'dgelsy' and 'zgelsy' functions
    (and if the functions that they call) from liblapack to C++.

    liblapack has the following license/copyright notice,
    see http://www.netlib.org/lapack/LICENSE:

    Copyright (c) 1992-2011 The University of Tennessee and The University
                            of Tennessee Research Foundation.  All rights
                            reserved.
    Copyright (c) 2000-2011 The University of California Berkeley. All
                            rights reserved.
    Copyright (c) 2006-2011 The University of Colorado Denver.  All rights
                            reserved.


    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer. 
  
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer listed
      in this license in the documentation and/or other materials
      provided with the distribution.
  
    - Neither the name of the copyright holders nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.
  
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT  
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT  
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2023  Dr. Jürgen Sauermann

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

#ifndef assert
# define assert(x)
#endif

typedef int Ccol;   // C/C++ column:   0..N (excluding N)
typedef int Crow;   // C/C++ row:      0..N (excluding N)
typedef int Fcol;   // FORTRAN column: 1..N (including N)
typedef int Frow;   // FORTRAN row:    1..N (including N)

#define ALL_ROWS(M)   for (Crow row = 0; row < (M); ++row)
#define ALL_COLS(N)   for (Ccol col = 0; col < (N); ++col)
#define REV_COLS(N)   for (Ccol k = (N); k--;)


/** A class that implements Dgelsy() and Zgelsy(), bundled with the helper
    functions that the implementation needs. Class LA_pack uses the following
    sub-classes:

   DD:   a single real number
   ZZ:   a single complex number
   Vector<T> a vector of T (where T is DD or ZZ)
   Matrix<T> a matrix of T (where T is DD or ZZ)

   Vector<T>  and Matrix<T> are actually views of an underlying T* so that
   sub-vectors and matrices can be accessed without copying them.

 */
class LA_pack
{
public:
  /// a real number
  typedef APL_Float DD;
  
  /// a complex number
  class ZZ
     {
     public:
       /// constructor: 0.0J0.0   (real 0.0)
       ZZ() : _r(0.0), _i(0.0) {}
     
       /// constructor: reJ0.0   (real re)
       ZZ(APL_Float re) : _r(re), _i(0) {}
     
       /// constructor: reJim   (complex r re + I×im)
       ZZ(APL_Float re, APL_Float im) : _r(re), _i(im) {}
     
       /// return the real part of \b this ZZ
       APL_Float real() const
          { return _r; }
     
       /// return the imag part of \b this ZZ
       APL_Float imag() const
          { return _i; }
     
       /// set the real part of \b this ZZ
       void set_real(APL_Float value)
          { _r = value; }
     
       /// set the imag part of \b this ZZ
       void set_imag(APL_Float value)
          { _i = value; }
     
       /// add complex \b this and complex \b zz
       ZZ operator +(const ZZ & zz) const
          { return ZZ(_r + zz._r, _i + zz._i); }

       /// add complex \b zz to \b this
       void operator += (const ZZ & zz)
          { _r += zz._r; _i += zz._i; }

       /// negative of \b this ZZ
       ZZ operator -() const
          { return ZZ(-_r, -_i); }

       /// subtract complex \b z2 from complex \b this
       ZZ operator -(const ZZ & zz) const
          { return ZZ(_r - zz._r, _i - zz._i); }

       /// subtract complex \b zz from \b this
       void operator -= (const ZZ & zz)
          { _r -= zz._r; _i -= zz._i; }

       /// multiply complex \b z1 with complex \b zz
       ZZ operator *(const ZZ & zz) const
          { return ZZ(_r*zz._r - _i*zz._i, _i*zz._r + _r*zz._i); }

       /// multiply *this with real \b dd
       void operator *= (ZZ & zz)
          { *this = *this * zz; }

       /// divide complex \b this by complex \b zz
       ZZ operator /(const ZZ & zz) const
          { 
            const APL_Float denominator = square(zz);
            return ZZ((_r*zz._r + _i*zz._i) / denominator,
                      (_i*zz._r - _r*zz._i) / denominator);
          }

       /// divide *this by complex \b zz
       void operator /= (const ZZ & zz)
          { *this = *this / zz; }

       /// divide complex \b this by real \b z2
       ZZ operator /(const APL_Float & dd) const
          { return ZZ(_r/dd, _i/dd); }

     protected:
       /// the real part
       APL_Float _r;
     
       /// the imaginary part
       APL_Float _i;
     };
  
  /// the maximum of \b x and \b y
  static DD max(DD x, DD y)
     { return  x < y ? y : x; }
  
   /// set dd to 0.0
   static void clear(DD & dd)
      { dd = 0.0; }

   /// set zz to 0.0j0.0
   static void clear(ZZ & zz)
      { zz = ZZ(); }

  /// return the absolute value (=length) of real dd
  static DD abs(DD dd)
     { return dd < 0.0 ? -dd : dd; }
  
  /// return the absolute value (= length) of complex zz
  static DD abs(ZZ zz)
     { return sqrt(square(zz)); }
  
  /// swap x and y
  template<typename T>
  static void exchange(T & x, T & y)
     {
       const T tmp = x;
       x = y;
       y = tmp;
     }
  
  //----------------------------------------------------------------------------
  /// A vector of real or complex numbers. Actually a vector view of some data
  template<typename T>
  class Vector
     {
     public:

         /// constructor: vector of length _len, with values _data.
         /// Unlike for std::vector<T>, \b _data must outlive \b this!
         Vector(T * _data, ShapeItem _len)
           : data(_data),
             len (_len)
         {}
      
         /// the first \b new_len elements of \b this
         Vector sub_len(ShapeItem new_len) const
            { assert(new_len <= len);   return Vector(data, new_len); }
      
         /// the rest of \b this Vector, starting at \b off. I.e. \b off↓this.
         Vector sub_off(ShapeItem off) const
            { assert(off <= len);   return Vector(data + off, len - off); }
      
         /// \b new_len elements of \b this Vector, starting at \b off.
         /// I.e. \b new_len↑off↓rhis
         Vector sub_off_len(ShapeItem off, ShapeItem new_len) const
            { assert((off + new_len) <= len);
              return Vector(data + off, new_len); }
      
         /// return the number of elements in \b this Vector
         const ShapeItem get_length() const { return len; }
      
         /// return the norm² of \b this Vector
         APL_Float norm_2() const
            {
              APL_Float ret = 0.0;
              const T * dj = data;
              loop(j, len)
                 {
                   const APL_Float re = get_real(*dj);   // most likely ≠ 0
                   ret += re * re;
      
                   const APL_Float im = get_imag(*dj);   // most likely = 0
                   if (im != 0.0)   ret += im * im;
                   ++dj;
                 }
              return ret;
            }
      
         /// the norm of \b this vector
         APL_Float norm() const
            { return sqrt(norm_2()); }
 
         /// multiply \b this vector by \b factor
         void scale(T factor)
            {
              T * dj = data;
              loop(j, len)   *dj++ *= factor;
            }
      
         /// set all elemens of \b this to 0
         void clear()
            {
              T * dj = data;
              loop(j, len)   *dj++.clear();
            }
      
         /// return true if \b this is 0
         bool is_null(ShapeItem count) const
            {
              const T * dj = data;
              loop(j, count)
                 {
                   if (get_imag(*dj) != 0.0)     return false;
                   if (get_real(*dj++) != 0.0)   return false;
                 }
              return true;
            }
       
         /// return \b this[i]
         T & at(ShapeItem i)
            { assert(i < len);  return *(data + i); }
      
         /// return \b this[i]
         const T & at(ShapeItem i) const
            { assert(i < len);  return *(data + i); }
      
     protected:
         /// the elements of \b this vector
         T * const data;
      
         ///  the length of this vector
         const ShapeItem len;
     };

  /// return the square of the real dd
  static DD square(DD dd)
     { return dd * dd; }
  
  /// return the square of the absolute value of complex zz
  static DD square(ZZ zz)
     { return square(zz.real()) + square(zz.imag()); }
  
  /// return the square of the length of Vector \b vec
  template<typename  T>
  static DD square(const Vector<T> vec)
     { return vec.norm_2(); }

  // A² + B² = C²
  template<typename T1, typename T2>
  static APL_Float hypotenuse(const T1 & kath_A, const T2 & kath_B)
     { return sqrt(square(kath_A) + square(kath_B)); }
 
  //----------------------------------------------------------------------------
  /// A double or complex matrix. Actually a matrix view of some data
  template<typename T>
  class Matrix
     {
     public:
        /// constructor: _rows by _cols matrix values from _data
        /// _CAUTION: data must outlive \b this!
        Matrix(T * _data, ShapeItem _rows, ShapeItem _cols, ShapeItem _dx)
          : data(_data),
            rows(_rows),
            cols(_cols),
            dx(_dx)
        {}
     
        /// constructor: sub-matrix of \b other with \b new_col_count columns
        /// CAUTION: other must outlive \b this!
        Matrix(const Matrix & other, ShapeItem new_col_count)
          : data(other.data),
            rows(other.rows),
            cols(new_col_count),
            dx(other.dx)
        { assert(new_col_count <= other.cols); }
     
        /// return the sub-matrix starting at row and col.
        /// I.e. \b row col↓this
        Matrix sub_matrix(Crow row, Crow col)
           {
             assert(col <= cols && row <= rows);
             return Matrix(&at(row, col), rows - row, cols - col, dx);
           }
     
        /// return the sub-matrix of size new_len_y: new_len_x.
        /// I.e. \b new_rows new_rows↑this
        /// starting at row 0 and column 0
        Matrix sub_len(ShapeItem new_rows, ShapeItem new_cols)
           {
             assert(new_rows <= rows);
             assert(new_cols <= cols);
             return Matrix(data, new_rows, new_cols, dx);
           }
     
        /// return the number of rows of \b this Matrox
        const ShapeItem get_row_count() const
           { return rows; }
     
        /// return the number of columns of \b this Matrox
        const ShapeItem get_column_count() const
           { return cols; }
     
        /// return column \b col of \b this Matrix
        Vector<T> get_column(Ccol col) 
           {
             return Vector<T>(data + col*dx, rows);
           }
     
        /// return column \b col of \b this Matrix
        const Vector<T> get_column(Ccol col) const
           {
             return Vector<T>(data + col*dx, rows);
           }
     
        /// return \b this[i, j]
        T & at(Crow i, Ccol j)
           { assert(i < rows);   assert(j < cols); return *(data + i + j*dx); }
     
        /// return \b this[i, j]
        const T & at(ShapeItem i, ShapeItem j) const
           { assert(i < rows);   assert(j < cols);
             return *(data + i + j*dx); }
     
        /// return \b this[i, i]
        T & diag(ShapeItem i) const
           { assert(i < rows);   assert(i < cols);
             return *(data + i*(1 + dx)); }
     
        /// swap columns c1 and c2
        void exchange_columns(ShapeItem c1, ShapeItem c2)
           {
             assert(c1 < cols);
             assert(c2 < cols);
             T * p1 = data + c1*dx;
             T * p2 = data + c2*dx;
             loop(r, rows)   exchange(*p1++, *p2++);
           }
     
        /// return the distance between two adjacent columns in \b data
        ShapeItem get_dx() const   { return dx; }
     
        /// return the length of the largest element
        APL_Float max_norm() const
           {
             const ShapeItem len = rows * cols;
             if (is_ZZ(*data))   // complex max. norm
                {
                  APL_Float ret2 = 0;
                  loop(l, len)
                     {
                       const APL_Float a2 = square(data[l]);
                       if (ret2 < a2)   ret2 = a2;
                     }
                  return sqrt(ret2);
                }
             else                // real max norm
                {
                  APL_Float ret = 0;
                  loop(l, len)
                     {
                       const APL_Float a = get_real(data[l]);
                       if (ret < a)         ret = a;
                       else if (ret < -a)   ret = -a;
                     }
                  return ret;
                }
           }
     
        /// multiply \b this by \b factor
        void scale(T factor)
           {
             T * dj = data;
             const ShapeItem len = rows * cols;
             loop(j, len)   *dj++ *= factor;
           }
     
     protected:
        /// the elements of \b this matrix
        T * const data;
     
        /// the number of rows of \b this matrix
        const Crow rows;
     
        /// the number of columns of \b this matrix
        const Ccol cols;
     
        /// return the distance between two adjacent columns in \b data.
        /// this distance is usually called LDx (Leading Dimension of x)
        /// in FORTRAN, e.g. LDA for a FORTRAN matrix A with LDA rows.
        const ShapeItem dx;
     };
  //============================================================================
  /// return the offset of the (absolute) largest element in vec
  static Ccol
  max_pos(const APL_Float * vec, ShapeItem len)
     {
       Ccol imax = 0;                 // start with index 0
       APL_Float vec_max = vec[0];   // and its value
       for (Ccol j = 1; j < len; ++j)
           {
             const APL_Float vec_j = abs(vec[j]);
             if (vec_max < vec_j)   { vec_max = vec_j;   imax = j; }
           }
     
        return imax;
     }
  //============================================================================
  // (static) functions ported from LAPACK (FORTRAN) .f files
  public:
     /// LAPACK dgelse (for real matrices A and B)
     static int Dgelsy(Matrix<DD> & A, Matrix<DD> &B, APL_Float rcond)
        { return gelsy<DD>(A, B, rcond); }
  
     /// LAPACK dgelse (for complex matrices A and B)
     static int Zgelsy(Matrix<ZZ> & A, Matrix<ZZ> &B, APL_Float rcond)
        { return gelsy<ZZ>(A, B, rcond); }
  
  protected:
   /// return the real part of dd (= dd)
   static DD get_real(const DD & dd)
      { return dd; }

   /// return the real part of zz
   static DD get_real(const ZZ & zz)
      { return zz.real(); }

   /// return the imag part of dd (= dd)
   static DD get_imag(const DD & dd)
      { return 0.0; }

   /// return the imag part of zz
   static DD get_imag(const ZZ & zz)
      { return zz.imag(); }

   /// set the real part of real dd
   static void set_real(DD & dd, APL_Float value)
      { dd = value; }

   /// set the real part of complex zz
   static void set_real(ZZ & zz, APL_Float value)
      { zz.set_real(value); }

   /// set the imag part of real dd
   static void set_imag(DD & dd, APL_Float value)
      { }

   /// set the real part of complex zz
   static void set_imag(ZZ & zz, APL_Float value)
      { zz.set_imag(value); }

   /// dd is 0.0
   static bool is_zero(const DD & dd)
      { return dd == 0.0; }

   /// dd is not 0.0
   static bool is_nonzero(const DD & dd)
      { return dd != 0.0; }

   /// zz is 0.0
   static bool is_zero(const ZZ & zz)
      { return zz.real() == 0.0 && zz.imag() == 0.0; }

   /// never
   static bool is_ZZ(const DD & dd)
      { return false; }

   /// always
   static bool is_ZZ(const ZZ & zz)
      { return true; }

   /// dd is not 0.0
   static bool is_nonzero(const ZZ & zz)
      { return zz.real() != 0.0 || zz.imag() != 0.0; }

      /// conjugate real dd in place (noop)
      static void conjugate(DD & dd)
         { }

      /// conjugate complex zz in place
      static void conjugate(ZZ & zz)
         { zz.set_imag(-zz.imag()); }

      /// return real dd conjugated
      static DD conjugated(const DD & dd)
         { return dd; }

      /// return complex zz conjugated
      static ZZ conjugated(const ZZ & zz)
         { return ZZ(zz.real(), - zz.imag()); }

      /// return 1.0 / real dd
      static DD reciprocal(const DD & dd)
         { return 1.0 / dd; }

      /// return 1.0 / complex zz
      static ZZ reciprocal(const ZZ & zz)
        {
          const APL_Float denom = zz.real() * zz.real() + zz.imag() * zz.imag();
          return ZZ(zz.real()/denom, - zz.imag()/denom);
        }

     /// dgelsy and zgelsy
     template<typename T>
     static int gelsy(Matrix<T> & A, Matrix<T> &B, APL_Float rcond);
  
     /// dgelsy and zgelsy with nicely scaled A and B
     template<typename T>
     static int scaled_gelsy(Matrix<T> & A, Matrix<T> & B, double rcond);
  
     /// estimate the rank of matrix A
     template<typename T>
     static int estimate_rank(const Matrix<T> & A, APL_Float rcond);
  
     /// LApack function geqp3
     template<typename T>
     static void geqp3(Matrix<T> & A, Ccol * pivot, T * tau);
  
     /// LApack function unm2r
     template<typename T>
     static void unm2r(Crow K, Matrix<T> & A, const T * tau, Matrix<T> & C);
  
     /** LApack function laic1 (estimate largest singular value).
         apply one step of incremental condition estimation.
  
         SEST: largest Singular value ESTimate (in/out)
      **/
     template<typename T>
     static void laic1_MAX(APL_Float & SEST, T alpha, T GAM, T & SIN, T & COS);
  
     /** LApack function laic1 (estimate smallest singular value).
         apply one step of incremental condition estimation.
  
         SEST: smallest Singular value ESTimate (in/out)
      **/
     template<typename T>
     static void laic1_MIN(APL_Float & SEST, T alpha, T GAM, T & SIN, T & COS);
  
     /// LApack function larfg. It generates an elementary reflector
     /// (aka. a Householder matrix)
     template<typename T>
     static T larfg(Ccol N, Vector<T> &x);
  
     /// LApack function trsm. Solves op(A) * X = alpha * B
     template<typename T>
     static void trsm(const Matrix<T> & A, Matrix<T> & B);
  
    /// LApack function ila_lc. Return the last column of C with a nonzero
    /// item in the first M rows.
     template<typename T>
     static Fcol ila_lc(Crow M, const Matrix<T> & A);
  
     /// LApack function gemv. y := alpha*A*x + beta*y
     ///                    or y := alpha*A**T*x + beta*y
     template<typename T>
     static inline void gemv(int M, int N, const Matrix<T> & A,
                             const Vector<T> &x, Vector<T> &y);
  
     /// LApack function gerc: A := alpha * x * y* * H + A,
     template<typename T>
     static void gerc(int M, int N, T ALPHA, const Vector<T> &x,
                      const Vector<T> &y, Matrix<T> & A);
  
     // LApack function larf: applies an elementary reflector to a general
     /// rectangular matrix
     template<typename T>
     static void larf(Vector<T> & v, T tau, Matrix<T> & c);
  
     /// LApack function laqp2: computes a QR factorization with column pivoting
     /// of the matrix block
     template<typename T>
     static void laqp2(Matrix<T> & A, Ccol * pivot,
                       T * tau, APL_Float * work);
};
//============================================================================
