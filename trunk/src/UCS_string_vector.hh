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

#include "PrintOperator.hh"
#include "UCS_string.hh"

#ifndef __UCS_STRING_VECTOR_HH_DEFINED__
#  define __UCS_STRING_VECTOR_HH_DEFINED__

//────────────────────────────────────────────────────────────────────────────
/// a vector of UCS_strings.
class UCS_string_vector : public std::vector<UCS_string>
{
public:
   /// constructor: empty string vector
   UCS_string_vector()   {}

   /// constructor: from APL character matrix (removes trailing blanks)
   /// @param val        APL character matrix to convert
   /// @param surrogate  true to treat surrogate pairs as single characters
   UCS_string_vector(const Value & val, bool surrogate);

   /// return true iff one of the strings is equal to \b ucs
   /// @param ucs  string to search for
   bool contains(const UCS_string & ucs) const
      {
        loop(s, size())   if (ucs == at(s))   return true;
        return false;
      }

   /// dump this string vector (debug function)
   /// @param out  output stream
   /// @param loc  caller location for diagnostics
   ostream & dump(ostream & out, const char * loc) const
      {
        out << "────────  " << loc << "  ──────── " << endl;
        loop(c, size())   out << "[" << c << "]  '" << at(c) << "'" << endl;
        return out << "═══════════════════════════════" << endl;
      }

   /// overload vector<UCS_string>::size() so that it returns a signed length
   ShapeItem ssize() const
      { return ShapeItem(std::vector<UCS_string>::size()); }

   /// replacement for erase(std::vector::iterator position)
   /// @param pos  index of the element to remove
   void erase(size_t pos)
      { std::vector<UCS_string>::erase(begin() + pos); }

   /// replacement for insert(std::vector::iterator position, value)
   /// @param pos    index at which to insert
   /// @param value  string to insert
   void insert(size_t pos, const UCS_string & value)
      { std::vector<UCS_string>::insert(begin() + pos, value); }

   /// sort the strings in this vector alphabetically
   void sort()
      {
        if (size() < 2)   return;
        Heapsort<UCS_string>::sort(*this, UCS_string::compare_names, 0);
      }

   /// return the length of the longest UCS_string in \b this vector,
   /// starting at \b col
   /// @param col           first column index to consider
   /// @param column_count  total number of columns (stride between rows)
   ShapeItem max_width(size_t col, size_t column_count) const;

   /// print items of \b this vector in a table with \b column_count columns
   /// @param out           output stream
   /// @param column_count  number of columns in the output table
   std::ostream & print_table(std::ostream & out, size_t column_count) const;

   /// compute columns width so that items align nicely (for )VARS, )FNS, etc.)
   /// @param tab_size  column tab spacing
   /// @param result    output: computed column widths
   void compute_column_width(int tab_size, std::vector<int> & result);

private:
   /// prevent the inadvertent use of iterator nonsense
   std::vector<UCS_string>::iterator erase(
        std::vector<UCS_string>::iterator position);

   /// prevent the inadvertent use of iterator nonsense
   std::vector<UCS_string>::iterator insert(
        std::vector<UCS_string>::iterator position,
        const UCS_string & value);
};
//────────────────────────────────────────────────────────────────────────────

#endif // __UCS_STRING_VECTOR_HH_DEFINED__
