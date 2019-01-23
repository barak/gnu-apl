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

#include "ArrayIterator.hh"
#include "Common.hh"
#include "Value.hh"

//-----------------------------------------------------------------------------
ArrayIteratorBase::ArrayIteratorBase(const Shape & shape)
   : max_vals(shape),
     total(0),
     is_done(false)
{
   Assert(!shape.is_empty());

   // reset values to 0 0 ... 0
   loop(r, max_vals.get_rank())   values.add_shape_item(0);

   Assert(values.get_rank() == max_vals.get_rank());
}
//-----------------------------------------------------------------------------
void ArrayIterator::operator ++()
{
   if (is_done)   return;

   ++total;

   // max_vals[0] is the highest dimension in APL which increments ar
   // the slowest rate. max_vals[ramk-1] is the fastest.
   //
   for (Rank r = max_vals.get_rank() - 1; !is_done; --r)
       {
         if (r < 0)
            {
              is_done = true;
              return;
            }

         values.increment_shape_item(r);
         if (values.get_shape_item(r) < max_vals.get_shape_item(r))   return;

         values.set_shape_item(r, 0);   // wrap around
       }

   Assert(0 && "Not reached");
}
//-----------------------------------------------------------------------------
void PermutedArrayIterator::operator ++()
{
   if(is_done)   return;

   for (Rank up = max_vals.get_rank() - 1; !is_done; --up)
       {
         if (up < 0)
            {
              is_done = true;
              return;
            }

         const Rank pp = permutation.get_shape_item(up);

         total += weight.get_shape_item(pp);

         values.increment_shape_item(pp);
         if (values.get_shape_item(pp) < max_vals.get_shape_item(pp))   return;

         total -= weight.get_shape_item(pp) * max_vals.get_shape_item(pp);
         values.set_shape_item(pp, 0);   // wrap around
       }

   Assert(0 && "Not reached");
}
//=============================================================================
