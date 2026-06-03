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

#ifndef __VALUE_P_HH_DEFINED__
#define __VALUE_P_HH_DEFINED__

#ifndef __COMMON_HH_DEFINED__
#  error This file shall NOT be #included directly, but by #including Comon.hh
#endif

class CDR_string;
class Cell;
class Value;
class Shape;
class UTF8_string;
class UCS_string;
class PrintBuffer;

//────────────────────────────────────────────────────────────────────────────
/// A Value smart * and access functions (except constructors)
class Value_P_Base
{
   friend class PointerCell;

public:
   /// return a const pointer to the Value
   const Value * get() const
      { return value_p; }

   /// return true if the pointer is invalid
   bool operator!() const
      { return value_p == 0; }

   /// return a const reference to the Value
   const Value & operator*() const
      { return *value_p; }

   /// return true if the pointer is valid
   bool operator+() const
      { return value_p != 0; }

   /// return a const pointer to the Value (overloaded *)
   const Value * operator->()  const
      { return value_p; }

   /// return a pointer to the Value
   Value * get()
      { return value_p; }

   /// init the pointer (without adding an event)
   inline void init_pointer()
      {
         value_p = 0;
      }

   /// return a reference to the Value
   Value & operator*()
      { return *value_p; }

   /// return a pointer to the Value (overloaded ->)
   Value * operator->()
      { return value_p; }

   /// reset and add value event
   /// @param loc caller location for diagnostics
   inline void clear(const char * loc);

   /// clear the pointer (and possibly add an event)
   /// @param loc caller location for diagnostics
   inline void clear_pointer(const char * loc);

   /// clone value if more than one Value_P points to it
   /// @param loc caller location for diagnostics
   inline void isolate(const char * loc);

   /// isolate this value and its sub-values
   /// @param loc caller location for diagnostics
   inline void isolate_deep(const char * loc);

   /// move the Value * from \b other to \b this.
   /// @param other source Value_P_Base to move from
   /// @param loc   caller location for diagnostics
   inline void move(Value_P_Base & other, const char * loc);

   /// decrement owner-count and reset pointer to 0
   inline void reset();

protected:
   /// pointer to the value
   Value * value_p;
};
//────────────────────────────────────────────────────────────────────────────
/// A Value smart * (constructors)
class Value_P : public Value_P_Base
{
public:
   /// Constructor: 0 pointer
   Value_P()
      { value_p = 0; }

   /// a new scalar value with un-initialized ravel
   /// @param loc caller location for diagnostics
   inline Value_P(const char * loc);

   /// a new scalar value with the value of cell
   /// @param cell the Cell whose value initialises the scalar ravel
   /// @param loc  caller location for diagnostics
   inline Value_P(const Cell & cell, const char * loc);

   /// a new true vector (rank 1 value) of length len and un-initialized ravel
   /// @param len number of elements in the vector
   /// @param loc caller location for diagnostics
   inline Value_P(ShapeItem len, const char * loc);

   /// a new matrix (rank 2 value) with an un-initialized ravel
   /// @param rows number of rows
   /// @param cols number of columns
   /// @param loc  caller location for diagnostics
   inline Value_P(ShapeItem rows, ShapeItem cols, const char * loc);

   /// a new value with shape sh and un-initialized ravel
   /// @param sh  shape of the new value
   /// @param loc caller location for diagnostics
   inline Value_P(const Shape & sh, const char * loc);

   /// constructor: a packed array with shape \b sh
   /// @param sh   shape of the packed array
   /// @param bits pointer to the packed bit data
   /// @param loc  caller location for diagnostics
   inline Value_P(const Shape & sh, uint64_t * bits, const char * loc);

   /// a new vector value from a UCS string
   /// @param ucs the Unicode character string to convert
   /// @param loc caller location for diagnostics
   inline Value_P(const UCS_string & ucs, const char * loc);

   /// a new vector value from a UTF8 string
   /// @param utf the UTF-8 encoded string to convert
   /// @param loc caller location for diagnostics
   inline Value_P(const UTF8_string & utf, const char * loc);

   /// a new vector value from a CDR record
   /// @param cdr the CDR-encoded data record to convert
   /// @param loc caller location for diagnostics
   inline Value_P(const CDR_string & cdr, const char * loc);

   /// a new character matrix value from a PrintBuffer record
   /// @param pb  the PrintBuffer supplying the character matrix data
   /// @param loc caller location for diagnostics
   inline Value_P(const PrintBuffer & pb, const char * loc);

   /// a new vector value from a shape
   /// @param loc caller location for diagnostics
   /// @param sh  pointer to the shape of the new value
   inline Value_P(const char * loc, const Shape * sh);

   /// Constructor: from Value *
   /// @param val raw pointer to the Value to wrap
   /// @param loc caller location for diagnostics
   inline Value_P(Value * val, const char * loc);

   /// Constructor: from other Value_P
   /// @param other the Value_P to copy from
   /// @param loc   caller location for diagnostics
   inline Value_P(const Value_P & other, const char * loc);

   /// Constructor: from other Value_P
   /// @param other the Value_P to copy from
   inline Value_P(const Value_P & other);

   /// Destructor
   inline ~Value_P();

   /// copy operator
   /// @param other the Value_P to copy from
   void operator =(const Value_P & other);
};
//────────────────────────────────────────────────────────────────────────────

#endif // __SHARED_VALUE_POINTER_HH_DEFINED__
