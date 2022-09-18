/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#include "Shape.hh"
#include "Value.hh"

//----------------------------------------------------------------------------
Shape::Shape(const Value & A, int qio_A)
   : rho_rho(0),
     volume(1)
{
   // check that A is a shape value, like the left argument of A⍴B
   //
   if (A.get_rank() > 1)   RANK_ERROR;
const ShapeItem len_A = A.element_count();
   if (len_A > MAX_RANK)   LIMIT_ERROR_RANK;   // of A

   loop(a, len_A)
      {
        add_shape_item(A.get_cravel(a).get_near_int() - qio_A);
      }
}
//----------------------------------------------------------------------------
Shape Shape::abs() const
{
Shape ret;
   loop(r, rho_rho)
      {
        const ShapeItem len = rho[r];
        if (len < 0)   ret.add_shape_item(- len);
        else           ret.add_shape_item(  len);
      }

   return ret;
}
//----------------------------------------------------------------------------
bool
Shape::operator ==(const Shape & other) const
{
   if (rho_rho != other.rho_rho)   return false;
 
   loop(r, rho_rho)
   if (rho[r] != other.rho[r])   return false;

   return true;
}
//----------------------------------------------------------------------------
void
Shape::expand(const Shape & B)
{
   // increase rank as necessary
   //
   expand_rank(B.get_rank());

   // increase axes as necessary
   //
   volume = 1;
   loop(r, rho_rho)
      {
        if (rho[r] < B.rho[r])   rho[r] = B.rho[r];
        volume *= rho[r];
      }
}
//----------------------------------------------------------------------------
Shape
Shape::insert_axis(sAxis axis, ShapeItem len) const
{
   if (get_rank() >= MAX_RANK)   LIMIT_ERROR_RANK;

Shape ret;

   if (axis <= 0)   // insert before first axis
      {
        ret.add_shape_item(len);
        loop(a, get_rank())          ret.add_shape_item(get_shape_item(a));
      }
   else if (uAxis(axis) >= get_rank())   // insert after last axis
      {
        loop(a, get_rank())          ret.add_shape_item(get_shape_item(a));
        ret.add_shape_item(len);
      }
   else
      {
        loop(a, axis)                ret.add_shape_item(get_shape_item(a));
        ret.add_shape_item(len);
        loop(a, get_rank() - axis)   ret.add_shape_item(get_shape_item(a + axis));
      }

   return ret;
}
//----------------------------------------------------------------------------
ShapeItem
Shape::ravel_pos(const Shape & idx) const
{
ShapeItem p = 0;
ShapeItem w = 1;

   for (sRank r = get_rank(); r-- > 0;)
      {
        p += w*idx.get_shape_item(r);
        w *= get_shape_item(r);
      }

   return p;
}
//----------------------------------------------------------------------------
Shape
Shape::offset_to_index(ShapeItem offset, int quad_io) const
{
Shape ret;
   ret.rho_rho = rho_rho;
   for (int r = rho_rho; r > 0;)
       {
         --r;
         if (rho[r] == 0)
            {
              Assert(offset == 0);
              ret.rho[r] = 0;
              continue;
            }

         ret.rho[r] = quad_io + offset % rho[r];
         offset /= rho[r];
       }

   return ret;
}
//----------------------------------------------------------------------------
void
Shape::check_same(const Shape & B, ErrorCode rank_err, ErrorCode len_err,
                  const char * loc) const
{
   if (get_rank() != B.get_rank())   throw_apl_error(rank_err, loc);

   loop(r, get_rank())
      {
        if (get_shape_item(r) == B.get_shape_item(r))   continue;
        throw_apl_error(len_err, loc);
      }
}
//----------------------------------------------------------------------------
bool
Shape::is_permutation() const
{
ShapeItem axes = 0;   // a bitmap of axes

   loop(r, get_rank())
       {
         const ShapeItem ax = get_shape_item(r);
         if (ax < 0)                return false;
         if (ax >= get_rank())      return false;
         if (axes & 1 << ax)        return false;
          axes |= 1 << ax;
       }

   return true;
}
//----------------------------------------------------------------------------
ostream &
operator <<(ostream & out, const Shape & shape)
{
   out << "⊏";
   loop(r, shape.get_rank())
       {
         if (r)   out << ":";
         out << shape.get_shape_item(r);
       }
   out << "⊐";
   return out;
}
//----------------------------------------------------------------------------
