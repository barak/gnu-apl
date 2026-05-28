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

#include <vector>
#include "Avec.hh"
#include "UCS_string_vector.hh"
#include "Value.hh"
#include "Workspace.hh"


//----------------------------------------------------------------------------
UCS_string_vector::UCS_string_vector(const Value & val, bool surrogate)
{
  // val is a simple text matrix with var_count rows and name_len columns.
  // Each row of val is one or two variable names.
  //
const ShapeItem var_count = val.get_rows();
const ShapeItem name_len = val.get_cols();

   loop(v, var_count)
      {
        ShapeItem start = v * name_len;   // the v'th variable
        const ShapeItem end = start + name_len;
        UCS_string name;   // name of the (left) variable
        loop(n, name_len)
           {
             const Unicode uni = val.get_cravel(start++).get_char_value();

             if (n == 0)   // first char of the variable name
                {
                  if ( Avec::is_quad(uni))   // leading ⎕
                     {
                       name << uni;
                       continue;
                     }
                  else if (uni == UNI_ALPHA  || uni == UNI_ALPHA_UNDERBAR ||
                           uni == UNI_OMEGA  || uni == UNI_OMEGA_UNDERBAR ||
                           uni == UNI_LAMBDA || uni == UNI_CHI            ||
                           uni == UNI_QUOTE_Quad)
                     {
                       name << uni;
                       continue;
                     }
                }

             if (Avec::is_symbol_char(uni)      // valid symbol char
                || uni == UNI_FULLSTOP)   // .member dot
                {
                  name << uni;
                  continue;
                }

             // end of (first) name reached. At this point we expect either
             // spaces until 'end' or some spaces and another name.
             //
             if (uni != UNI_SPACE)
                {
                  name.clear();
                  break;
                }

             // we have reached the end of the first name. At this point
             // there could be:
             //
             // 1. spaces until 'end' (= one name), or
             // 2. a second name (alias)

             // skip spaces from start and subsequent spaces
             //
             while (start < end &&
                    val.get_cravel(start).get_char_value() == UNI_SPACE)
                   ++start;


             if (start == end)   break;   // only spaces (no second name)

             // at this point we maybe have the start of a second name, which
             // is an error (if last is false) or not. In both cases the first
             // name can be ignored.
             //
             name.clear();
             if (!surrogate)   break;   // error

             // 'last' is true thus to_varnames() was called from ⎕SVO and
             // the line may contains two variable names.
             // Return the second (i.e. surrogate name)
             //
             surrogate = false;
             while (start < end)
                {
                  const Unicode uni = val.get_cravel(start++).get_char_value();
                  if (Avec::is_symbol_char(uni))   // valid symbol char
                     {
                       name << uni;
                     }
                  else if (uni == UNI_SPACE)
                     {
                       break;
                     }
                  else
                     {
                       name.clear();   // error
                       break;
                     }
                }
             break;
           }
        push_back(name);
      }
}
//----------------------------------------------------------------------------
ShapeItem
UCS_string_vector::max_width(size_t col, size_t column_count) const
{
   /*  interpret this UCS_string_vector as a 2-dimensional array of names.
       Assume further that this hypothetical array has column_count columns.
       Then return the length of the longest name in column col.
    */
ShapeItem ret = 0;

   for (ShapeItem s = col; s < ssize(); s += column_count)
       {
          ret = max(ret, at(s).ssize());
       }

   return ret;
}
//----------------------------------------------------------------------------
std::ostream & 
UCS_string_vector::print_table(std::ostream & out, size_t column_count) const
{
   // return the length of the longest UCS_string in \b this vector,
   /// starting at \b col

const ShapeItem column_count1 = column_count - 1;
std::vector<ShapeItem> column_widths_v(column_count);
ShapeItem * column_widths = column_widths_v.data();
   loop(col, column_count)   column_widths[col] = max_width(col, column_count);

const UCS_string frame(U"╔╤╗╚╧╝═║│");

   // top row
   //
   out << frame[0];                                  // ╔
   loop(col, column_count)
       {
         const ShapeItem width = column_widths[col];
         loop(w, width)   out << frame[6];           // ═
         if (col < column_count1)   out << frame[1];   // ╤
         else                       out << frame[2];   // ╗
       }
   out << std::endl;

   // data rows
   //
   for (ShapeItem d = 0; d < ssize();)
       {
         out << frame[7];                                  // ║
         loop(col, column_count)
             {
               const ShapeItem width = column_widths[col];
               if (d < ssize())   // valid data available
                  {
                    const UCS_string & data = (*this)[d++];
                    loop(j, data.size())             out << data[j];
                    loop(l, (width - data.size()))   out << UNI_SPACE;
                   }
               else              // no more data: fill with blanks
                   {
                    loop(j, width)                   out << UNI_SPACE;
                   }
               if (col < column_count1)   out << frame[8];   // │
               else                       out << frame[7];   // ║
             }
         out << std::endl;
       }

   // bottom row
   //
   out << frame[3];                                  // ╚
   loop(col, column_count)
       {
         const ShapeItem width = column_widths[col];
         loop(w, width)   out << frame[6];           // ═
         if (col < column_count1)   out << frame[4];   // ╧
         else                       out << frame[5];   // ╝
       }
   out << std::endl;

   return out;
}
//----------------------------------------------------------------------------
void
UCS_string_vector::compute_column_width(int tab_size,
                                        std::vector<int> & result)
{
const int quad_PW = Workspace::get_PW();

   result.clear();

   if (size() < 2)
      {
        if (size())   result.push_back(front().size());
        else          result.push_back(quad_PW);
        return;
      }

   // compute block counts (one block having tab_size characters)
   //
const int max_blocks = (quad_PW + 1) / tab_size;
std::vector<int> name_blocks;
   loop(n, size())   name_blocks.push_back(1 + (1 + at(n).size()) / tab_size);

   // compute max number of column blocks based on first line blocks
   //
int max_col = -1;
   {
     int blocks = 0;
     loop(n, size())
         {
           if ((blocks + name_blocks[n]) < max_blocks)   // name_blocks[n] fits
              blocks += name_blocks[n];
           else                                          // max_blocks exceeded
             {
               max_col = n - 1;
               break;
             }
         }

     if (max_col == -1)   // all blocks fit
        {
          result.clear();
          loop(n, size())   result.push_back(name_blocks[n]);
          return;
        }
   }

   // decrease max_col until all names fit...
   //
   for (;max_col > 1; --max_col)
      {
        result.clear();
        int free_blocks = max_blocks;
        loop(n, size())   // try to fit blocks into result
            {
              const int col_n = n % max_col;
              const int bn = name_blocks[n];
              if (n < max_col)   // first row: append column
                 {
                   free_blocks -= bn;
                   if (free_blocks < 0)   break;
                   result.push_back(bn);
                 }
              else if (bn > result[col_n])
                 {
                   free_blocks -= bn - result[col_n];
                   if (free_blocks < 0)   break;
                   result[col_n] = bn;
                 }
            }

        if (free_blocks >= 0)   return;   // success
      }

   // single colums
   //
   result.clear();
int max_nb = 0;
   loop(n, size())   if (max_nb < name_blocks[n])   max_nb = name_blocks[n];
   result.push_back(max_nb);
}
//----------------------------------------------------------------------------
