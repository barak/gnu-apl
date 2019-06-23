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

#ifndef __ARRAY_ITERATOR_HH_DEFINED__
#define __ARRAY_ITERATOR_HH_DEFINED__

#include "Common.hh"
#include "Value.hh"
#include "SystemLimits.hh"

//-----------------------------------------------------------------------------
/// An iterator counting 0, 1, 2, ... ⍴,shape
class ArrayIterator
{
public:
   /// constructor
   ArrayIterator(const Shape & shape)
   : ref_B(shape),
     current_offset(0),
     rank(shape.get_rank()),
     done(false)
      {
        ShapeItem _weight = 1;
        loop(r, rank)
            {
              const ShapeItem sB = shape.get_transposed_shape_item(r);
              _twc & twc_r = twc[r];

              twc_r.current = 0;
              twc_r.to      = sB;
              twc_r.weight  = _weight;

              if (0 == sB)   done = true;   // empty array
              _weight *= sB;
            }
      }

   /// the work-horse of the iterator
   void operator++()
      {
        ++current_offset;
        loop(r, rank)
            {
             _twc & twc_r = twc[r];
             ++twc_r.current;

              if (twc_r.current < twc_r.to)   return;

              // end of the axis reached: reset this axis
              // and increment next axis via loop()
              //
              twc_r.current = 0;
           }
        done = true;
      }

   /// return true iff this iterator has more items to come.
   bool more() const   { return !done; }

   /// Get the current offset for axis r (! mirrored !) .
   ShapeItem get_offset(Rank r) const
      { return twc[rank - r - 1].current; }

   /// return the current offset
   ShapeItem operator()() const
      { return current_offset; }

   /// return the offsets as a Shape
   Shape get_offsets() const
      {
        Shape ret;
        loop(r, rank)   ret.add_shape_item(twc[rank - r - 1].current);
        return ret;
      }

   /// multiply the current offsets with w and return their sum
   ShapeItem multiply(const Shape & w) const
      { Assert1(rank == w.get_rank());
        ShapeItem ret = 0;
        loop(r, rank)   ret += twc[rank - r - 1].current * w.get_shape_item(r);
        return ret; }

protected:
   /// shape of the array over which we iterate
   const Shape & ref_B;

   /// _twc means to / weight / current
   struct _twc twc[MAX_RANK];

   /// the current offset from ↑B
   ShapeItem current_offset;

   /// the number of axes
   const uRank rank;

   /// true iff this interator has reached its final item
   bool done;
};
//-----------------------------------------------------------------------------
/** An iterator counting 0, 1, 2, ... ⍴,shape but with permuted axes
    The permutation is given as a \b Shape. If perm = 0, 1, 2, ... then
    PermutedArrayIterator is the same as ArrayIterator.
 **/
/// an iterator for arrays with permnuted axes
class PermutedArrayIterator : public ArrayIterator
{
public:
   /// constructor
   PermutedArrayIterator(const Shape & shape, const Shape & perm)
   : ArrayIterator(shape),
     permutation(perm),
     weight(shape.reverse_scan())
   { Assert(rank == perm.get_rank()); }

   /// the work-horse of the iterator
   void operator ++()
      {
         if (done)   return;

         loop(upr, rank)   // upr: un-permuted rank
             {
               const Axis r = permutation.get_shape_item(rank - upr - 1);
               _twc & twc_r = twc[rank - r - 1];
               ++twc_r.current;
               current_offset += twc_r.weight;

               if (twc_r.current < twc_r.to)   return;

               // end of the axis reached: reset this axis
               // and increment the next axis via loop()
               //
               current_offset -= twc_r.weight * twc_r.current;
               twc_r.current = 0;
             }

         done = true;
      }

protected:
   /// The permutation of the indices.
   const Shape permutation;

   /// the weights of the indices
   const Shape weight;
};
//=============================================================================

#endif // __ARRAY_ITERATOR_HH_DEFINED__
