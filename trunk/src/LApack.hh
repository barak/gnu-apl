/*
    This file is a port of the 'dgelsy' and 'zgelsy' functions (and the
    functions that they call) from liblapack to C++.

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

#ifndef __GELSY_HH_DEFINED__
#define __GELSY_HH_DEFINED__

#ifndef assert
# define assert(x)
#endif

using namespace std;

class LA_pack
{
public:
  /// a real number
  typedef APL_Float DD;
  
  /// a complex number
  class ZZ
  {
  public:
     /// 0J0
     ZZ()
     : _r(0),
       _i(0)
     {}
  
     /// rJ0
     ZZ(APL_Float r)
     : _r(r),
       _i(0)
     {}
  
     /// rJi
     ZZ(APL_Float r, APL_Float i)
     : _r(r),
       _i(i)
     {}
  
     /// return the real part of \b this
     APL_Float real() const       { return _r; }
  
     /// set the real part of \b this
     void set_real(APL_Float x)   { _r = x; }
  
     /// return the imag part of \b this
     APL_Float imag() const    { return _i; }
  
     /// set the imag part of \b this
     void set_imag(APL_Float x)   { _i = x; }
  
     /// conjugate \b this
     void conjugate()   { _i = - _i; }
  
     /// return true if \b this is real and not equal to \b dd
     bool operator != (APL_Float dd) const   { return _r != dd || _i != 0.0; }
  
     /// return true if \b this is real and equal to \b dd
     bool operator == (APL_Float dd) const   { return _r == dd && _i == 0.0; }
  
     /// add \b r zz to \b this
     ZZ operator +(const ZZ & zz) const
        { return ZZ(_r + zz._r, _i + zz._i); }
  
     /// subtract \b zz from \b this
     ZZ operator -(const ZZ & zz) const
        { return ZZ(_r - zz._r, _i - zz._i); }
  
     /// negate \b this
     ZZ operator -() const
        { return ZZ(-_r, -_i); }
  
     /// multiply \b zz and \b this
     ZZ operator *(const ZZ & zz) const
        { return ZZ(_r * zz._r - _i * zz._i,
                    _i * zz._r + _r * zz._i); }
  
     /// divide \b this by \b d
     ZZ operator /(APL_Float d) const
        { return ZZ(_r/d, _i/d); }
  
     /// divide \b this by \b zz
     ZZ operator /(const ZZ & zz) const
        { 
          const APL_Float denom = zz._r * zz._r + zz._i * zz._i;
           return ZZ((_r * zz._r + _i * zz._i) / denom,
                     (_i * zz._r - _r * zz._i) / denom);
        }
  
  protected:
     /// the real part
    APL_Float _r;
  
     /// the imaginary part
    APL_Float _i;
  };
  
  /// the maximum of \b x and \b y
  #define MAX(x, y) ((x) >= (y) ? (x) : (y))
  
  /// the minimum of \b x and \b y
  #define MIN(x, y) ((x) <= (y) ? (x) : (y))
  
  /** a single double or complex number. This class contains wrappers to
      double or complex numbers so that they can be used in templates.
   */
  /// A single double or complex number
  class DZ
  {
  public:
     /// return true if the template argument (DD or ZZ) is complex (i.e. ZZ)
     static bool is_ZZ(DD x)   { return false; }
     /// return true if the template argument (DD or ZZ) is complex (i.e. ZZ)
     static bool is_ZZ(ZZ z)   { return true;  }
  
     /// return the real part of \b x
     static APL_Float get_real(DD x)   { return x;   }
  
     /// return the imaginary part of \b x
     static APL_Float get_imag(DD x)   { return 0.0; }
  
     /// return the real part of \b x
     static APL_Float get_real(ZZ z)   { return z.real(); }
  
     /// return the imaginary part of \b x
     static APL_Float get_imag(ZZ z)   { return z.imag(); }
  
     /// set the real part of \b x
     static void set_real(DD & y, DD x)   { y = x; }
  
     /// set the imaginary part of \b x
     static void set_imag(DD & y, DD x)   { assert(x == 0.0); }
  
     /// set the real part of \b x
     static void set_real(ZZ & y, DD x)   { y.set_real(x); }
  
     /// set the imaginary part of \b x
     static void set_imag(ZZ & y, DD x)   { y.set_imag(x); }
  
     /// conjugate \b x
     static void conjugate(DD x)     { }
  
     /// conjugate \b z
     static void conjugate(ZZ & z)   { z.conjugate(); }
  
     /// return \b x conjugated
     static DD CONJ(DD x)           { return x; }
  
     /// return \b z conjugated
     static ZZ CONJ(const ZZ & z)   { return ZZ(z.real(), -z.imag()); }
  
     /// return the inverse of \b dd
     static DD inv(DD dd)           { return 1.0 / dd; }
  
     /// return the inverse of \b zz
     static inline ZZ inv(const ZZ & zz)
        {
          const APL_Float denom = zz.real() * zz.real() + zz.imag() * zz.imag();
          return ZZ(zz.real()/denom, - zz.imag()/denom);
        }
  };
  
  /// return the real part of \b x
  template<typename T>
  static APL_Float REAL(T x)
  {
     return DZ::get_real(x);
  }
  
  /// return the imaginary part of \b x
  template<typename T>
  static APL_Float IMAG(T x)
  {
    return DZ::get_imag(x);
  }
  
  /// set the real part of \b x
  template<typename T>
  static void  SREAL(T & y, APL_Float x)
  {
    DZ::set_real(y, x);
  }
  
  /// set the imaginary part of \b x
  template<typename T>
  static void SIMAG(T & y, APL_Float x)
  {
    DZ::set_imag(y, x);
  }
  
  /// set the real and imaginary parts of \b y to (xr, xi). I.e. y = xrJi×xi
  template<typename T>
  static void Sri(T & y, APL_Float xr, APL_Float xi)
  {
     SREAL<T>(y, xr);
     SIMAG<T>(y, xi);
  }
  
  /// set the real part of \b y and clear its imaginary part
  template<typename T> static void Sri(T & y, APL_Float xr)
  {
     SREAL<T>(y, xr);
     SIMAG<T>(y, 0);
  }
  
  /// return the absolute value of a
  static inline DD ABS(DD a) { return a < 0.0 ? DD(-a) : a; }
  
  /// return the square of the absolute value of a
  static inline DD ABS_2(DD a)
     { return a*a; }
  
  /// return the absolute value of z
  static inline DD ABS(ZZ z)
     { return sqrt(z.real()*z.real() + z.imag()*z.imag()); }
  
  /// return the square of the absolute value of z
  static inline DD ABS_2(ZZ z)
     { return z.real()*z.real() + z.imag()*z.imag(); }
  
  /// return abs(a) with the sign of b
  static inline APL_Float SIGN(APL_Float a, APL_Float b)
  {
     if (b < 0.0)  return -ABS(a);
     else          return  ABS(a);
  }
  
  /// swap x and y
  template<typename T>
  static inline void exchange(T & x, T & y)
  {
  const T tmp = x;
    x = y;
    y = tmp;
  }
  
  //----------------------------------------------------------------------------
  /// A vector of real numbers or a vector of complex numbers
  template<typename T>
  class Vector
  {
  public:
     /// constructor: vector of length _len, with values _data. Unlike for
     /// std::vector<T>, \b _data must outlive \b this!
     Vector(T * _data, ShapeItem _len)
       : data(_data),
         len (_len)
     {}
  
     /// the first \b new_len elements of \b this
     Vector sub_len(ShapeItem new_len) const
        { assert(new_len <= len);   return Vector(data, new_len); }
  
     /// the rest of \b this, starting at \b off
     Vector sub_off(ShapeItem off) const
        { assert(off <= len);   return Vector(data + off, len - off); }
  
     /// \b new_len elements of \b this, starting at \b off
     Vector sub_off_len(ShapeItem off, ShapeItem new_len) const
        { assert((off + new_len) <= len);
          return Vector(data + off, new_len); }
  
     /// return the number of elements
     const ShapeItem get_length() const { return len; }
  
     /// return true if \b this is a vector of complex numbers (and not a vector
     /// of real numbers
     bool is_ZZ() const   { return DZ::is_ZZ(*data); }
  
     /// the norm of \b this
     APL_Float norm() const
        {
  //     if (len < 1)   return 0.0;
  //     APL_Float scale = 0.0;
  //     APL_Float ssq = 1.0;
  
          APL_Float ret = 0.0;
          const T * dj = data;
          loop(j, len)
             {
               const APL_Float re = REAL(*dj);
               if (re != 0.0)
                  {
  /***
                    const APL_Float temp = ABS(re);
                    if (scale < temp)
                       {
                         const APL_Float scale_temp = scale/temp;
                         ssq = 1.0 + ssq*scale_temp*scale_temp;
                         scale = temp;
                       }
                    else
                       {
                         const APL_Float temp_scale = temp/scale;
                         ssq += temp_scale*temp_scale;
                       }
  ***/
                    ret += re*re;
                  }
  
               const APL_Float im = IMAG(*dj);
               if (im != 0.0)
                  {
  /***
                    const APL_Float temp = ABS(im);
                    if (scale < temp)
                       {
                         const APL_Float scale_temp = scale/temp;
                         ssq = 1.0 + ssq*scale_temp*scale_temp;
                         scale = temp;
                       }
                    else
                       {
                         const APL_Float temp_scale = temp/scale;
                         ssq += temp_scale*temp_scale;
                       }
  ***/
                    ret += im*im;
                  }
               ++dj;
             }
          return sqrt(ret);
  //      return scale*sqrt(ssq);
        }
  
     /// multiply \b this by \b factor
     void scale(T factor)
        {
          T * dj = data;
          loop(j, len)
              {
                *dj = *dj * factor;
                ++dj;
              }
        }
  
     /// set all elemens of \b this to 0
     void clear()
        {
          T * dj = data;
          loop(j, len)   *dj++ = 0.0;
        }
  
     /// return true if \b this is 0
     bool is_zero(ShapeItem count) const
        {
          const T * dj = data;
          loop(j, count)
             {
               if (IMAG(*dj) != 0.0)     return false;
               if (REAL(*dj++) != 0.0)   return false;
             }
          return true;
        }
   
     /// conjugate \b this
     void conjugate()
        {
          if (!is_ZZ())   return;
          T * dj = data;
          loop(j, len)   DZ::conjugate(*dj++);
        }
  
     /// add \b other to \b this
     void add(const Vector & other)
        {
          assert(len == other.len);
          T * dj = data;
          T * sj = other.data;
          loop(j, len)   *dj++ += *sj++;
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
  //----------------------------------------------------------------------------
  /// A double or complex matrix
  template<typename T>
  class Matrix
  {
  public:
     /// constructor: _rows by _cols matrix values from _data
     /// _data must outlive \b this!
     Matrix(T * _data, ShapeItem _rows, ShapeItem _cols, ShapeItem _dx)
       : data(_data),
         rows(_rows),
         cols(_cols),
         dx(_dx)
     {}
  
     /// constructor: sub-matrix of \b other with \b new_col_count columns
     /// other must outlive \b this!
     Matrix(const Matrix & other, ShapeItem new_col_count)
       : data(other.data),
         rows(other.rows),
         cols(new_col_count),
         dx(other.dx)
     { assert(new_col_count <= other.cols); }
  
     /// return the sub-matrix starting at row y and column x
     Matrix sub_yx(ShapeItem row, ShapeItem col)
        {
          assert(col <= cols && row <= rows);
          return Matrix(&at(row, col), rows - row, cols - col, dx);
        }
  
     /// return the sub-matrix of size new_len_y: new_len_x
     /// starting at row 0 and column 0
     Matrix sub_len(ShapeItem new_rows, ShapeItem new_cols)
        {
          assert(new_rows <= rows);
          assert(new_cols <= cols);
          return Matrix(data, new_rows, new_cols, dx);
        }
  
     /// return true if \b this is a matrix of complex numbers (and not a matrix
     /// of real numbers
     bool is_ZZ() const   { return DZ::is_ZZ(*data); }
  
     /// return the number of rows
     const ShapeItem get_row_count() const
        { return rows; }
  
     /// return the number of columns
     const ShapeItem get_column_count() const
        { return cols; }
  
     /// return column \b c
     Vector<T> get_column(ShapeItem c) 
        {
          return Vector<T>(data + c*dx, rows);
        }
  
     /// return column \b c
     const Vector<T> get_column(ShapeItem c) const
        {
          return Vector<T>(data + c*dx, rows);
        }
  
     /// return \b this[i, j]
     T & at(ShapeItem i, ShapeItem j)
        { assert(i < rows);   assert(j < cols); return *(data + i + j*dx); }
  
     /// return \b this[i, j]
     const T & at(ShapeItem i, ShapeItem j) const
        { assert(i < rows);   assert(j < cols); return *(data + i + j*dx); }
  
     /// return \b this[i, i]
     T & diag(ShapeItem i) const
        { assert(i < rows);   assert(i < cols); return *(data + i*(1 + dx)); }
  
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
          if (is_ZZ())   // complex max. norm
             {
               APL_Float ret2 = 0;
               loop(l, len)
                  {
                    const APL_Float a2 = ABS_2(data[l]);
                    if (ret2 < a2)   ret2 = a2;
                  }
               return sqrt(ret2);
             }
          else           // real max norm
             {
               APL_Float ret = 0;
               loop(l, len)
                  {
                    const APL_Float a = DZ::get_real(data[l]);
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
          const ShapeItem len = rows*cols;
          loop(j, len)
              {
                 *dj = *dj * factor;
                 ++dj;
              }
        }
  
  protected:
     /// the elements of \b this matrix
     T * const data;
  
     /// the number of rows of \b this matrix
     const ShapeItem rows;
  
     /// the number of columns of \b this matrix
     const ShapeItem cols;
  
     /// return the distance between two adjacent columns in \b data
     const ShapeItem dx;
  };
  //============================================================================
  /// return the offset of the (absolute) largest element in vec
  static inline ShapeItem
  max_pos(const APL_Float * vec, ShapeItem len)
  {
  ShapeItem imax = 0;
  APL_Float dmax = ABS(vec[0]); 
     for (ShapeItem j = 1; j < len; ++j)
         {
           const APL_Float dj = ABS(vec[j]);
           if (dmax < dj )   { dmax = dj;   imax = j; }
         }
  
     return imax;
  }
  //============================================================================
  /// (static( functions ported from LAPACK (Fortran) files
  public:
     /// LAPACK dgelse (for real matrices A and B)
     static int Dgelsy(Matrix<DD> & A, Matrix<DD> &B, APL_Float rcond)
        { return gelsy<DD>(A, B, rcond); }
  
     /// LAPACK dgelse (for complex matrices A and B)
     static int Zgelsy(Matrix<ZZ> & A, Matrix<ZZ> &B, APL_Float rcond)
        { return gelsy<ZZ>(A, B, rcond); }
  
  protected:
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
     static void geqp3(Matrix<T> & A, ShapeItem * pivot, T * tau);
  
     /// LApack function unm2r
     template<typename T>
     static void unm2r(ShapeItem K, Matrix<T> & A, const T * tau, Matrix<T> & c);
  
     /** LApack function laic1 (estimate largest singular value).
         apply one step of incremental condition estimation.
  
         SEST: largest estimated singular value of \b this matrix (in/out)
      **/
     template<typename T>
     static void laic1_max(APL_Float & SEST, T alpha, T GAMMA, T & SIN, T & COS);
  
     /** LApack function laic1 (estimate smallest singular value).
         apply one step of incremental condition estimation.
  
         SEST: smallest estimated singular value of \b this matrix (in/out)
      **/
     template<typename T>
     static void laic1_min(APL_Float & SEST, T alpha, T GAMMA, T & SIN, T & COS);
  
     /** LApack function larfg. It generates an elementary reflector
         (aka. Householder matrix)
  
        Consider the linear transformation of a point x:
  
        x → x - 2(x,v)v = x - 2v(v* x)  (v* ←→ complex conjugate of v = -⍉v)
  
        The Householder matrix P is then
  
        P = I - 2 v v*   (where I is the identity matrix.
      */
     template<typename T>
     static T larfg(ShapeItem N, T & ALPHA, Vector<T> &x);
  
     /// LApack function trsm. Solves op(A) * X = alpha * B
     template<typename T>
     static void trsm(int M, int N, const Matrix<T> & A, Matrix<T> & B);
  
    /// LApack function ila_lc
     template<typename T>
     static int ila_lc(ShapeItem M, ShapeItem N, const Matrix<T> & A);
  
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
     static void laqp2(Matrix<T> & A, ShapeItem * pivot,
                       T * tau, APL_Float * vn1);
 };
//============================================================================

#endif // __GELSY_HH_DEFINED__
