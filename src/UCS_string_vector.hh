/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2016  Dr. Jürgen Sauermann

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

#include "UCS_string.hh"

#ifndef __UCS_STRING_VECTOR_HH_DEFINED__
#define __UCS_STRING_VECTOR_HH_DEFINED__

//-----------------------------------------------------------------------------
/// a vector of UCS_strings.
class UCS_string_vector
{
public:
   /// constructor: empty vector
   UCS_string_vector()
   {}

   /// constructor: from APL character matrix (removes trailing blanks)
   UCS_string_vector(const Value & val, bool surrogate);

   /// constructor: from another string vector
   UCS_string_vector(const UCS_string_vector & other)
      { loop(o, other.size())   append(other[o]); }

   /// destructor
   ~UCS_string_vector()
     { shrink(0); }

   /// allocate space for \b res strings
   void reserve(ShapeItem res)
      { strings.reserve(res); }

   /// assignment from another string vector
   UCS_string_vector & operator=(const UCS_string_vector & other)
      {
        shrink(0);
        loop(o, other.size())   append(other[o]);
        return *this;
      }

   /// return true iff one of the strings is equal to \b ucs
   bool contains(const UCS_string & ucs) const
      {
        loop(s, size())   if (ucs == *strings[s])   return true;
        return false;
      }

   /// return the number of strings in this vector
   ShapeItem size() const
      { return strings.size(); }

   /// return the \b idx'th sting
   const UCS_string & operator [](ShapeItem idx) const
      { return *strings[idx]; }

   /// return the \b idx'th sting
   UCS_string & operator [](ShapeItem idx)
      { return *strings[idx]; }

   /// return the last string
   const UCS_string & last() const
      { return *strings.last(); }

   /// return the last string
   UCS_string & last()
      { return *strings.last(); }

   /// append string \b ucs
   void append(const UCS_string & ucs)
      { strings.append(new UCS_string(ucs)); }

   /// insert string \b ucs before the \b pos'th string
   void insert_before(ShapeItem pos, const UCS_string & ucs)
      { strings.insert_before(pos, new UCS_string(ucs)); }

   /// remove the idx'th string
   void erase(ShapeItem idx)
      { delete strings[idx];   strings[idx] = 0;   strings.erase(idx); }

   /// forget last string
    void pop()
       {
         Assert(size() > 0);
         UCS_string * ucs = strings.last();
         delete ucs;
         strings.last() = 0;
         strings.pop();
       }

   /// reduce number of strings to \b new_size
   void shrink(ShapeItem new_size)
      { while (size() > new_size)   pop(); }

   /// resize to \b new_size strings
   void resize(ShapeItem new_size)
      {
        if (new_size == strings.size())     return;
        if (new_size <  strings.size())     { shrink(new_size);   return; }
        while (new_size > strings.size())   strings.append(new UCS_string());
      }

   /// sort strings
   void sort()
      {
        if (strings.size() < 2)   return;
        Heapsort<const UCS_string *>::sort(const_cast<const UCS_string **>
                                                      (&strings[0]),
                                           strings.size(), 0,
                                           UCS_string::compare_names);
      }

   /// compute columns widths so that items align nicely
   Simple_string<int> compute_column_width(int tab_size);

protected:
   /// the strings in this string vector
   Simple_string<UCS_string *> strings;

private:
   void * operator new(size_t size);
   void * operator new[](size_t size);
};
//-----------------------------------------------------------------------------

#endif // __UCS_STRING_VECTOR_HH_DEFINED__
