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

#ifndef __HEAPSORT_HH_DEFINED__
#define __HEAPSORT_HH_DEFINED__

#include <cstdint>
#include <vector>

/// heapsort an array of items of type \b T
template<typename T>
class Heapsort
{
public:
   /// a function to compare two items of array a. The function shall return
   /// true if \b item_a is larger than \b item_b.
   /// \b comp_arg is some additional argument guiding the comparison
   typedef bool (*greater_fun)(const T & item_a, const T & item_b,
                 const void * comp_arg);

   /// sort array \b a according to greater_fun \b gf
   static void sort(vector<T> & array, const void * comp_arg, greater_fun gf)
      {
        /* Turn array into a heap. An array a is a if for every odd element
           a[i+i+1] in a[] the heap property "Hodd" and for every even element
           a[i+i+2] in a[] the heap property "Heven" holds, where:

           (Hodd)   a[i] > a[i+i+1]
           (Heven)  a[i] > a[i+i+2]

           The elements a[i] in the right half of a[] have no "children"
           a[i+i+1] or a[i+i+2] in a[] and therefore the respective heap
           property "Hodd" or "Heven" trivially holds.

           To turn a[] into a head it therefore suffices to start at the
           middle of a[] and move left towards the root p=0, establishing
           the heap property for each element visited.

                p=0    p=1                  p=heapsize/2             p=heapsize
                ├──────┼────────────────────┼────────────────────────┤
           a[]: │ root │  inner tree nodes  │      tree leaves       │
                └──────┴────────────────────┴────────────────────────┘
                │  └ largest element        │  the  heap property is │
                │    on return              │  trivially satisfied   │
                │                                                    │
                │← ← ← ← ← ← ← ← ← ← ← ← HEAP → → → → → → → → → → → →│
        */

        int64_t heapsize = array.size();
        for (int64_t parent = heapsize/2 - 1; parent >= 0; --parent)
            make_heap(array, heapsize, parent, comp_arg, gf);

        /* At this point a[] is a heap (and therefore a[0] is the largest
           element). In the following loop:

           The left heapsize elements of a[] are a heap and the remaining
           right arguments of a[] are sorted. Every iteration of the loop
           exchanges the root of the heap (which is the largest element of
           the heap) and makes it the smallest element of the sorted part,

           That is, the size of the heap is decremented while the size of
           the sorted part is incremented.

                │← ← ← ← ← ← ← HEAP  → → → → → → →│                  │
                ╔══════╤═══════════════════╤══════╦══════════════════╗
           a[]: ║ root │ > ... >     ... > │ last ║      sorted      ║
                ╚══════╧═══════════════════╧══════╩══════════════════╝
                  ↓↓↓↓                       ↓↓↓↓
                ╔══════╤═══════════════════╦══════╤══════════════════╗
           a[]: ║ last │ ...           ... ║ root │ <    sorted      ║
                ╚══════╧═══════════════════╩══════╧══════════════════╝
                │← ← ← ← ←  HEAP  → → → → →│                         │

           Every such swap destroyes the heap property of the root and must
           be re-established with make_heap() aka. "siftDown".
        */
        for (--heapsize; heapsize > 0; heapsize--)
            {
              // The root a[0] is the largest element in a[0] ... a[k].
              // Exchange a[0] and a[k], decrease the heap size,
              // and re-establish the heap property of the new a[0].
              //
              swap(array[heapsize], array[0]);

              // re-establish the heap property of the new root a[0]
              //
              make_heap(array, heapsize, /* root */ 0, comp_arg, gf);
            }
      }

   /** binary search for \b key (of type KEY) in \b array of type T). The
       key is typically a member of T. If K is, say, integer and T is
       sorted ascendingly then compare() might be:

      int compare(const int & key, const T & t, const void *)
         { return key - t.key; }   // return > 0 if key is above t.
    **/
   template<typename KEY>
   static const T * search(const KEY & key,
                           const vector<T> & array,
                           int (*compare)(const KEY & key,
                                          const T & item,
                                          const void * comp_ctx),
                           const void * ctx)
      {
        for (int64_t l = 0, u = array.size(); l < u;)
            {
              const int64_t m = (l + u) / 2;   // middle of the range
              const int comp = (*compare)(key, array[m], ctx);
              if      (comp > 0)   l = m + 1;   // search in the upper half
              else if (comp < 0)   u = m;       // search in the lower half
              else                 return &array[m];   // key found
            }

        return 0;   // not found
      }

   /// initialize the (initially empty) vector \b sorted with pointers to the
   /// items of \b array in such a way that *sorted[N] < *sorted[N+1] according
   /// to the comparison function \b gf.
   static void sorted_pointers(vector<const T *> & sorted,
                               const vector<T> & array,
                               greater_fun gf, const void * comp_arg = 0)
      {
        Assert(sorted.size() == 0);   // fresh vector
        sorted.reserve(array.size());
        loop(a, array.size())   sorted.push_back(&array[a]);
        sort(sorted, comp_arg, gf);
      }

   /// initialize the (initially empty) vector \b sorted with pointers to the
   /// items of \b array in such a way that *sorted[N] < *sorted[N+1] according
   /// to the comparison function \b gf.
   static void sorted_pointers(vector<T *> & sorted,
                                      vector<T> & array,
                                      greater_fun gf,
                                      const void * comp_arg = 0)
      {
        Assert(sorted.size() == 0);   // fresh vector
        sorted.reserve(array.size());
        loop(a, array.size())   sorted.push_back(&array[a]);
        sort(sorted, comp_arg, gf);
      }

   /// initialize the (initially empty) vector \b sorted with indices of the
   /// array in such a way that arrat[sorted[N]] < array[sorted[N+1]] according
   /// to the comparison function \b gf.
   static void sorted_indices(vector<ShapeItem> & sorted,
                              const vector<T> & array,
                              greater_fun gf, const void * comp_arg = 0)
      {
        Assert(sorted.size() == 0);   // fresh vector
        sorted.reserve(array.size());
        loop(a, array.size())   sorted.push_back(a);
        sort(sorted, comp_arg, gf);
      }

protected:
   /// establish the heap property of the subtree with root a[parent]
   static void make_heap(vector<T> & array, int64_t heapsize, int64_t parent,
                         const void * comp_arg, greater_fun gf)
      {
        for (;;)
           {
             const int64_t left = 2*parent + 1;   // left  child of parent.
             const int64_t right = left + 1;      // right child of parent.
             int64_t max = parent;                // assume parent is the max.

             // set max to either left, or to right, or leave it as is so that
             // a[max] is largest of a[i], a[left], and a[right]
             //
             if (left < heapsize)                    // array[left] exists
                {
                  if ((*gf)(array[left],
                            array[max], comp_arg))   // and left is > a[max]
                     max = left;                     // then: max ← left

                  if ( (right < heapsize) &&         // array[right] exists,
                       (*gf)(array[right],
                             array[max], comp_arg))   // and right is > a[max]
                     max = right;                     // then max ← right
                }
             // otherwise: left >= heapsize and then also right >= heapsize

             // if max is still the parent, then neither left nor right
             // were larger then max and we are done.
             //
             if (max == parent)   return; // parent was the max: done

             // left or right was the max. exchange and continue
             swap(array[max], array[parent]);
             parent = max;
           }
      }

   /// echange \b a and \b b
   //
   static void swap(T & a, T & b)
      {
        const T t = a;   a = b;   b = t;
      }
};
//===========================================================================

#endif // __HEAPSORT_HH_DEFINED__
// EOF
