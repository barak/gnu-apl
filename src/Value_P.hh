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

#ifndef __VALUE_P_HH_DEFINED__
#define __VALUE_P_HH_DEFINED__

#ifndef __COMMON_HH_DEFINED__
# error This file shall NOT be #included directly, but by #including Comon.hh
#endif

class CDR_string;
class Cell;
class Value;
class Shape;
class UTF8_string;
class UCS_string;
class PrintBuffer;

#define ptr_clear(p, l) p.reset()

//-----------------------------------------------------------------------------
/// A Value smart * and access functions (except constructors)
class Value_P_Base
{
   friend class PointerCell;

public:
   /// decrement owner-count and reset pointer to 0
   inline void reset();

   /// reset and add value event
   inline void clear(const char * loc);

   /// return true if the pointer is invalid
   bool operator!() const
      { return value_p == 0; }

   /// return true if this Value_P points to the same Value as \b other
   bool operator ==(const Value_P_Base & other) const
      { return value_p == other.value_p; }

   /// return true if this Value_P points to a different Value than \b other
   bool operator !=(const Value_P_Base & other) const
      { return value_p != other.value_p; }

   /// return a const pointer to the Value (overloaded ->)
   const Value * operator->()  const
      { return value_p; }

   /// return a pointer to the Value (overloaded ->)
   Value * operator->()
      { return value_p; }

   /// return a const reference to the Value
   const Value & operator*() const
      { return *value_p; }

   /// return a const pointer to the Value
   const Value * get() const
      { return value_p; }

   /// return a pointer to the Value
   Value * get()
      { return value_p; }

   /// return a const reference of the Value
   const Value & getref() const
      { return *value_p; }

   /// return a reference of the Value
   Value & getref()
      { return *value_p; }

   /// clear the pointer (and possibly add an event)
   inline void clear_pointer(const char * loc);

   /// decrement the owner count of \b val. The function bidy requires Value.hh
   /// and is therefore implemented in Value.icc.
   static inline void decrement_owner_count(Value * & val, const char * loc);

   /// increment the owner count of \b val. The function body requires Value.hh
   /// and is therefore implemented in Value.icc.
   static inline void increment_owner_count(Value * val, const char * loc);

protected:
   /// pointer to the value
   Value * value_p;
};
//-----------------------------------------------------------------------------
/// A Value smart * (constructors)
class Value_P : public Value_P_Base
{
public:
   /// Constructor: 0 pointer
   Value_P()
   { value_p = 0; }

   /// a new scalar value with un-initialized ravel
   inline Value_P(const char * loc);

   /// a new scalar value with the value of Cell
   inline Value_P(const Cell & cell, const char * loc);

   /// a new true vector (rank 1 value) of length len and un-initialized ravel
   inline Value_P(ShapeItem len, const char * loc);

   /// a new value with shape sh and un-initialized ravel
   inline Value_P(const Shape & sh, const char * loc);

   /// a new vector value from a UCS string
   inline Value_P(const UCS_string & ucs, const char * loc);

   /// a new vector value from a UTF8 string
   inline Value_P(const UTF8_string & utf, const char * loc);

   /// a new vector value from a CDR record
   inline Value_P(const CDR_string & cdr, const char * loc);

   /// a new character matrix value from a PrintBuffer record
   inline Value_P(const PrintBuffer & pb, const char * loc);

   /// a new vector value from a shape
   inline Value_P(const char * loc, const Shape * sh);

   /// Constructor: from Value *
   inline Value_P(Value * val, const char * loc);

   /// Constructor: from other Value_P
   inline Value_P(const Value_P & other, const char * loc);

   /// Constructor: from other Value_P
   inline Value_P(const Value_P & other);

   /// copy operator
   void operator =(const Value_P & other);

   /// Destructor
   inline ~Value_P();
};
//-----------------------------------------------------------------------------

#endif // __SHARED_VALUE_POINTER_HH_DEFINED__
