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

#ifndef __APL_TYPES_HH_DEFINED__
#define __APL_TYPES_HH_DEFINED__

#ifndef __COMMON_HH_DEFINED__
# error This file shall not be #included directly, but by #including Common.hh
#endif

#include <complex>
#include <memory>

#include <math.h>
#include <stdint.h>

#include "Common.hh"
#include "Unicode.hh"

using namespace std;

#define APL_Float_is_class 0

//////////////////////////////////////////////////////////////
// A. typedefs                                              //
//////////////////////////////////////////////////////////////

/// The (signed) rank or axis of an APL value.
typedef int16_t sRank;
typedef sRank sAxis;

/// The (unsigned) rank or axis of an APL value.
typedef uint32_t uRank;
typedef uRank uAxis;

/// A bitmap for axes (in fun[X] arguments) normalized to ⎕IO←0.
typedef uint16_t AxesBitmap;

/// The length of one dimension (axis) of an APL shape.
typedef int64_t ShapeItem;

/// for ulong(x) instead of  static_cast<unsigned long>(x)
typedef unsigned long ulong;

/// for long_long(x) instead of  static_cast<long long>(x)
typedef long long long_long;

/// for ulong_long(x) instead of  static_cast<unsigned long long>(x)
typedef unsigned long long ulong_long;   // dito.

/// The SI level, 0 = global (oldest), the caller at SI level N calls the
/// functions at SI level N+1
typedef int SI_level;

/// One APL character value.
typedef Unicode APL_Char;

/// One APL integer value.
typedef int64_t APL_Integer;

class Function;
typedef const Function * cFunction_P;

inline void
Hswap(APL_Integer & i1, APL_Integer & i2)
{ const APL_Integer tmp = i1;   i1 = i2;   i2 = tmp; }

/// One (real) APL floating point value.
#if APL_Float_is_class // APL_Float is a class

#include "APL_Float_as_class.hh"

inline void release_APL_Float(APL_Float * x)   { x->~APL_Float(); }

#else                  // APL_Float is a POD (double)

typedef double APL_Float_Base;
typedef APL_Float_Base APL_Float;

#define complex_exponent(x) exp(x)
#define complex_power(x, y) pow((x), (y))
#define complex_sqrt(x)     sqrt(x)
#define release_APL_Float(x)

#endif // APL_Float is class vs. POD

//----------------------------------------------------------------------------
/// One APL complex value.
typedef complex<APL_Float> APL_Complex;
//----------------------------------------------------------------------------
/// APL time = microseconds since Jan. 1. 1970 00:00:00 UTC
typedef int64_t APL_time_us;

class Symbol;
class Value;
class Cell;

//////////////////////////////////////////////////////////////
// B. enums            i                                    //
//////////////////////////////////////////////////////////////

#include "APL_enums.hh"

//////////////////////////////////////////////////////////////
// C. structs                                               //
//////////////////////////////////////////////////////////////

/// three AP numbers that uniquely identify a processor
struct AP_num3
{
   /// constructor: processor, parent, and grand-parent
   AP_num3(AP_num pro = NO_AP, AP_num par = AP_NULL, AP_num gra = AP_NULL)
   : proc(pro),
     parent(par),
     grand(gra)
   {}

   /// copy \b other to \b this
   void operator =(const AP_num3 & other)
      {
        proc   = other.proc;
        parent = other.parent;
        grand  = other.grand;
      }

   /// true if \b this AP_num3 is equal to \b other
   bool operator==(const AP_num3 & other) const
      { return proc   == other.proc   &&
               parent == other.parent &&
               grand  == other.grand; }

   AP_num proc;     ///< the processor
   AP_num parent;   ///< the parent of the processor
   AP_num grand;    ///< the parent of the parent
};
//----------------------------------------------------------------------------
/// two Function_PCs that indicate a token range in the body of a defined
//  function body
struct Function_PC2
{
   /// a PC range
   Function_PC2(Function_PC l, Function_PC h)
   : low(l),
     high(h)
   {}

   Function_PC low;    ///< low PC  (including)
   Function_PC high;   ///< high PC (including)
};
//----------------------------------------------------------------------------
/** A single label. A label is a local variable (sym) with an integer function
    line (in which \b sym: was specified as the first 2 tokens in the line)
 */
///  A label in a defined function
struct labVal
{
   Symbol      * sym;    ///< The symbol for the label variable.
   Function_Line line;   ///< The line number of the label.
};
//----------------------------------------------------------------------------
/// The end and the state of an abstract iterator along one axis
/// (to / weight / current)
struct _twc
{
   /// the end index (exclusive) for each axis. It can be > dimension
   /// length for over-Take from the start.
   ShapeItem to;

   /// weight of the dimension
   ShapeItem weight;

   /// the current index
   ShapeItem current;
};
//----------------------------------------------------------------------------
/// The range and the state of an abstract iterator along one axis
/// (from / to / weight / current)
struct _ftwc : public _twc
{
   /// the start index (inclusive) for each axis. It can be < 0 for
   /// over-Take from the end.
   ShapeItem from;
};
//----------------------------------------------------------------------------
/// the ravel (of an APL value) and a comparison lenght (= number of
/// consecutive cells to be compared)
struct ravel_comp_len
{
   /// the ravel (first Cell of some Value)
   const Cell * ravel;

   /// the number of consecutive Cells to be compared
   ShapeItem comp_len;
};
//----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////
// D. Namespace APL_types
//////////////////////////////////////////////////////////////

/*
  instead of putting everything into a namespace (which makes the code
  rather unreadable, we put only those types into a namesapce that were
  observed to conflict with other libraries.
 */

namespace APL_types
{
  typedef int32_t Depth;
}

#endif // __APL_TYPES_HH_DEFINED__
