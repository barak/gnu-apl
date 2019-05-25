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

#ifndef __SIMPLE_STRING_HH_DEFINED__
#define __SIMPLE_STRING_HH_DEFINED__

#include <iostream>

#include <string.h>

#ifdef AUXILIARY_PROCESSOR
# define __ASSERT_HH_DEFINED__
# include <assert.h>
# define Assert(x) assert(x)
#else
# include "Assert.hh"
# include "Common.hh"
#endif

using namespace std;

//-----------------------------------------------------------------------------
/// a simple string
template <typename T, bool has_destructor>
class Simple_string
{
public:
   /// constructor: empty string
   Simple_string()
   : items_allocated(0),
     items_valid(0),
     items(0)
      {}

   /// constructor: the first \b len items of \b data
   Simple_string(const T * data, ShapeItem len)
      {
        allocate(len);
        loop(l, items_valid)
           {
             (items + l)->~T();
             new (items + l) T(data[l]);
           }
      }

   /// constructor: \b len times \b data
   Simple_string(ShapeItem len, const T & data)
      {
        allocate(len);
        loop(l, items_valid)
           {
             (items + l)->~T();
             new (items + l) T(data);
           }
      }

   /// constructor: copy other string
   Simple_string(const Simple_string & other)
      {
        allocate(other.items_valid);
        loop(l, items_valid)
           {
             (items + l)->~T();
             new (items + l) T(other.items[l]);
           }
      }

   /// constructor: copy other string, starting at pos, max. len items
   Simple_string(const Simple_string & other, ShapeItem pos, ShapeItem len)
      {
        Assert((pos + len) <= other.items_valid);
        allocate(len);
        loop(l, items_valid)
           {
             (items + l)->~T();
             new (items + l) T(other.items[l + pos]);
           }
      }

   /// destructor
   ~Simple_string()
      { deallocate(); }

   /// copy \b other
   void operator =(const Simple_string & other)
      {
        deallocate();
        new (this) Simple_string(other);
      }

   /// return the number of characters in \b this string
   ShapeItem size() const
      { return items_valid; }

   /// return the idx'th character
   const T & operator[](ShapeItem idx) const
      { return at(idx); }

   /// return the idx'th character
   T & operator[](ShapeItem idx)
      { return at(idx); }

   /// return a reference to the last item (size() MUST be checked beforehand)
   const T & last() const
      { return at(items_valid - 1); }

   /// return a reference to the last item (size() MUST be checked beforehand)
   T & last()
      { return at(items_valid - 1); }

   /// append character \b t to \b this string
   void append(const T & t, const char * loc = 0)
      {
        extend(items_valid + 1);
        (items + items_valid)->~T();
        new (items + items_valid++) T(t);
      }

   /// append string \b other to \b this string
   void append(const Simple_string & other)
      {
        extend(items_valid + other.items_valid);
        loop(o, other.items_valid)   append(other[o]);
      }

   /// insert character \b t before position \b pos
   void insert_before(ShapeItem pos, const T & t)
      {
        Assert(pos <= items_valid);
        if (pos == items_valid)
           {
             append(t);
             return;
           }

        extend(items_valid + 1);

        memmove(items + pos + 1, items + pos , (items_valid - pos) * sizeof(T));
        new (items + pos)   T(t);
        ++items_valid;
      }

   /// forget (and maybe destruct) the last item
    void pop()
      {
        Assert(items_valid > 0);
        --items_valid;
        if (has_destructor)
           {
             T * t = items + items_valid;
             t->~T();
             new (t) T();
           }
      }

   /// decrease size to \b new_size
   void shrink(ShapeItem new_size)
      {
        Assert((items_valid - new_size) >= 0);
        if (has_destructor)   while ((items_valid - new_size) > 0)   pop();
        else                  items_valid = new_size;
      }

   /// erase \b one item, at \b pos
   void erase(ShapeItem pos)
      {
        Assert(pos < items_valid);

        const ShapeItem rest = items_valid - (pos + 1);
        T * t = items + pos;
        t->~T();                               // destruct erased item

        void * vp0 = t;                        // erased item
        void * vp1 = t + 1;                    // next higher item
        memmove(vp0, vp1, rest * sizeof(T));   // copy remaining items down

        vp0 = items + items_valid - 1;
        new (items + items_valid - 1) T();
        --items_valid;   // not pop() !!!
      }

   /// extend allocated size
   void reserve(ShapeItem new_alloc_size)
      {
        extend(new_alloc_size);
      }

   /// exchange this and other (without copying the data)
   void swap(Simple_string & other)
      {
        const ShapeItem ia = items_allocated;
        items_allocated = other.items_allocated;
        other.items_allocated = ia;

        const ShapeItem iv = items_valid;
        items_valid = other.items_valid;
        other.items_valid = iv;

        T * const it = items;
        items = other.items;
        other.items = it;
      }

   /// deallocate memory
   void deallocate()
      { 
        delete [] items;
        items = 0;
        items_valid = 0;
        items_allocated = 0;
      }

protected:
   /// allocation tuning
   enum
      {
        ADD_ALLOC = 4,    ///< extra chars added when extending the string
        MIN_ALLOC = 16,   ///< min. size allocated
      };

   /// allocate memory
   void allocate(ShapeItem min_size)
      {
        Assert1(min_size >= 0);
        items_valid = min_size;
        items_allocated = items_valid + ADD_ALLOC;   // and a few more
        if (items_allocated < MIN_ALLOC)   items_allocated = MIN_ALLOC;
        items = new T[items_allocated];
      }

   /// increase the allocated size to at least \b new_size items
   void extend(ShapeItem new_size)
      {
        if ((items_allocated - new_size) >= 0)   return;   // large enough

        T * old_items = items;
        items_allocated = new_size + ADD_ALLOC;
        T * new_items = new T[items_allocated];
        memcpy(static_cast<void *>(new_items), old_items,
               items_valid * sizeof(T));
        memset(static_cast<void *>(old_items), 0, items_valid * sizeof(T));
        delete [] old_items;
        items = new_items;
      }

   /// the number of characters allocated
   ShapeItem items_allocated;

   /// the number of characters valid (always <= items_allocated)
   ShapeItem items_valid;

   /// the items
   T * items;

   /// return the idx'th character
   const T & at(ShapeItem idx) const
      {
        Assert(items);
        if (idx < 0)                    Assert(0 && "Bad index");
        if ((items_valid - idx) <= 0)   Assert(0 && "Bad index");
        return items[idx];
      }

   /// return the idx'th character
   T & at(ShapeItem idx)
      {
        Assert(items);
        if (idx < 0)                    Assert(0 && "Bad index");
        if ((items_valid - idx) <= 0)   Assert(0 && "Bad index");
        return items[idx];
      }
};
//-----------------------------------------------------------------------------

#endif // __SIMPLE_STRING_HH_DEFINED__

