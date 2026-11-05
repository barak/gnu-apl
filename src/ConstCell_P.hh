/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2021-2026  Dr. Jürgen Sauermann

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

#ifndef __CONSTCELL_P_DEFINED__
#define __CONSTCELL_P_DEFINED__

#include <cstdint>
#include "Assert.hh"
#include "Value_P.hh"

typedef uint64_t Cell_offset;

//════════════════════════════════════════════════════════════════════════════
/// a "smart" Cell *, remembering its owner and allowing only a subset
/// of what a normal Cell * is capable of.
class ConstRavel_P
{
public:
   /// constructor from the owner of the Cell
   /// @param _owner the APL value whose ravel is iterated
   /// @param _inc   whether operator++() shall advance the offset
   ConstRavel_P(const cValue & _owner, bool _inc)
   : owner(_owner),
     end(_owner.element_count()),
     offset(0),
     increment(_inc)
   {}

   /// constructor from pointer to the owner of the Cell
   /// @param _owner smart pointer to the APL value whose ravel is iterated
   /// @param _inc   whether operator++() shall advance the offset
   ConstRavel_P(Value_P _owner, bool _inc)
   : owner(*_owner),
     end(_owner->element_count()),
     offset(0),
     increment(_inc)
   {}

   /// return the lengths of the Cells
   const Cell_offset get_length() const
      { return end; }

   /// return the owner of the Cells
   const cValue & get_owner() const
      { return owner; }

   /// return the ravel offset of the current Cell
   Cell_offset operator ()() const
      { return offset; }

   /// return a reference to the current Cell
   const Cell & operator *() const
      { return owner.get_cravel(offset, cache); }

   /// return true iff offset is valid (and then *() != 0). For this to work,
   /// increment needs to be true
   bool operator +() const
      { Assert1(increment);   return offset < end || offset == 0; }

   /// return a pointer to the current Cell (or 0 at the end)
   const Cell * operator ->() const
      { return operator +() ? &owner.get_cravel(offset, cache) : 0; }

   /// move to the next Cell
   void operator ++()
      { if (increment)   ++offset; }

protected:
   /// the owner of the ravel
   const cValue & owner;

   /// position of the first Cell after the ravel
   const Cell_offset end;

   /// the current offset
   Cell_offset offset;

   /// whether operator ++() shall increment \b offset
   const bool increment;   // ++ shall/shall not increment offset

   /// per-iterator materialisation slot for packed ravels: each ConstRavel_P
   /// gets its own cache so that two iterators over the same (packed) owner
   /// (e.g. A⊤B or A≡B with A and B aliased) never clobber one another.
   mutable Cell cache;
};
//════════════════════════════════════════════════════════════════════════════
#endif // __CONSTCELL_P_DEFINED__
