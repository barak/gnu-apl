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

#include <stdint.h>

#include <fstream>
#include <string.h>
#include <vector>
#include <vector>

#include <vector>

#include "SystemLimits.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"
#include "Workspace.hh"

#include "Value.hh"

class Cell;
class Function;
class Executable;
class Prefix;
class Symbol;
class StateIndicator;
class SymbolTable;
class Token;
struct Token_loc;
class Value;
class ValueStackItem;

using namespace std;

//-----------------------------------------------------------------------------
/// a helper class for saving an APL workspace
class XML_Saving_Archive
{
public:
   /// constructor: remember output stream and  workspace
   XML_Saving_Archive(ofstream & of)
   : indent(0),
     out(of),
     value_count(0),
     char_mode(false)
   {}

   /// destructor
   ~XML_Saving_Archive()   { out.close();  delete [] values; }

   /// an index for \b values
   enum Vid { INVALID_VID = -1 };

   /// write user-defined function \b fun
   void save_Function(const Function & fun);

   /// write either the name and SI-level of user-defined function or the
   /// id of a system function. Return number of attribute= items written
   int save_Function_name(const char * ufun_prefix, const char * level_prefix,
                          const char * id_prefix, const Function & fun);

   /// write Prefix \b pfp
   void save_Parser(const Prefix & pfp);

   /// write Symbol \b sym
   void save_Symbol(const Symbol & sym);

   /// write all user defined commands
   void save_user_commands(const std::vector<Command::user_command> & cmds);

   /// write SymbolTable \b symtab
   void save_symtab(const SymbolTable & symtab);

   /// write StateIndicator entry \b si
   void save_SI_entry(const StateIndicator & si);

   /// write Token_loc \b tloc
   void save_token_loc(const Token_loc & tloc);

   /// write ValueStackItem \b vsi
   void save_vstack_item(const ValueStackItem & vsi);

   /// write UCS_string \b str
   void save_UCS(const UCS_string & str);

   /// write Value \b v except its ravel
   XML_Saving_Archive & save_shape(Vid vid);

   /// write ravel of Value \b v
   XML_Saving_Archive & save_Ravel(Vid vid);

   /// write entire workspace
   XML_Saving_Archive & save();

   /// a value and its parent (if the parent is nested)
   struct _val_par
      {
         /// default constructor
         _val_par()
         : _val(0),
           _par(INVALID_VID)
         {}

         /// constructor
         _val_par(const Value * v, Vid par)
         : _val(v),
           _par(par)
         {}

         /// the value
         const Value * _val;

         /// the optional parent
         Vid _par;

         /// assign \b other
         void operator=(const _val_par & other)
            { new (this) _val_par(other._val, other._par); }

         /// compare function for Heapsort::sort()
         static bool compare_val_par(const _val_par & A, const _val_par & B,
                                     const void *);

         /// compare function for bsearch()
         static int compare_val_par1(const void * key, const void * B);
      };

protected:
   /// width of one indentation level
   enum { INDENT_LEN = 2 };

   /// indent by \b indent levels, return space left on line
   int do_indent();

   /// return the index of \b val in values
   Vid find_vid(const Value * val);

   /// emit one unicode character (inside "...")
   void emit_unicode(Unicode uni, int & space);

   /// emit one ravel cell \b cell
   void emit_cell(const Cell & cell, int & space);

   /// emit a token value up to (excluding) the '>' of the end token
   void emit_token_val(const Token & tok);

   /// enter char mode. maybe print ² and return the number of chars printed
   int enter_char_mode()
      { if (char_mode)   return 0;   // already in char mode
        out << UNI_PAD_U2;   char_mode = true;   return 1; }

   /// leave char mode. maybe print ⁰ and return the number of chars printed
   int leave_char_mode()
      { if (!char_mode)   return 0;   // not in char mode
        out << UNI_PAD_U0;   char_mode = false;   return 1; }

   /// decrement \b space by length of \b str and return \b str
   static const char * decr(int & space, const char * str);

   /// return true if \b uni prints nicely and is allowed in XML "..." strings
   static bool xml_allowed(Unicode uni);

   /// current indentation level
   int indent;

   /// output XML file
   std::ofstream & out;

   /// a value and the Vid of its parent (if value is a sub-value of a 
   /// nested parent)
   /// all values in the workspace
   _val_par * values;

   /// the number of (non-stale) values
   ShapeItem value_count;

   /// true iff ² is pending
   bool char_mode;
};
//-----------------------------------------------------------------------------
inline void Hswap(XML_Saving_Archive::_val_par & vp1,
                  XML_Saving_Archive::_val_par & vp2)
{
const XML_Saving_Archive::_val_par tmp = vp1;   vp1 = vp2;   vp2 = tmp;
}
//-----------------------------------------------------------------------------
/// a helper class for loading an APL workspace
class XML_Loading_Archive
{
public:
   /// constructor: remember file name and workspace
   XML_Loading_Archive(const char * _filename, int & dump_fd);

   /// destructor (unmap()s file).
   ~XML_Loading_Archive();

   /// return true iff constructor could open the file
   bool is_open() const   { return fd != -1; }

   /// read an entire workspace, throw DOMAIN_ERROR on failure
   void read_Workspace(bool silent);

   /// set copying and maybe set protection
   void set_protection(bool prot, const UCS_string_vector & allowed)
      { copying = true;   protection = prot;   allowed_objects = allowed;
        have_allowed_objects = allowed_objects.size() > 0; }

   /// reset archive to the start position
   void reset();

   /// skip to tag \b tag, return EOF if end of file
   bool skip_to_tag(const char * tag);

   /// read vids of top-level variables
   void read_vids();

protected:
   /// move to next tag, return true if EOF
   bool next_tag(const char * loc);

   /// read next Value element, return true on success
   void read_Value();

   /// read cell(s) starting at \b cell from string starting at \b first
   /// to number of cells read
   void read_Cells(Cell * & cell, Value & cell_owner, const UTF8 * & first);

   /// read characters tagged with INI_PAD_U1 and INI_PAD_U2, until end of line
   void read_chars(UCS_string & ucs, const UTF8 * & first);

   /// read next Ravel element
   void read_Ravel();

   /// read next unused-name element
   void read_unused_name(int d, Symbol & symbol);

   /// read next Variable element
   void read_Variable(int d, Symbol & symbol);

   /// read next Function element
   void read_Function(int d, Symbol & symbol);

   /// read next Label element
   void read_Label(int d, Symbol & symbol);

   /// read next Shared-Variable element
   void read_Shared_Variable(int d, Symbol & symbol);

   /// read next Symbol element
   void read_SymbolTable();

   /// read next Symbol element
   void read_Symbol();

   /// read all user-defined commands
   void read_Commands();

   /// read next Command element
   void read_Command();

   /// read next StateIndicator element
   void read_StateIndicator();

   /// read next StateIndicator entry
   void read_SI_entry(int level);

   /// read parsers in SI entry
   void read_Parser(StateIndicator & si, int lev);

   /// read ⍎ Executable
   Executable * read_Execute();

   /// read ◊ Executable
   Executable * read_Statement();

   /// read a user defined Executable
   Executable * read_UserFunction();

   /// read a lambda
   Executable * read_lambda(const UTF8 * lambda_name);

   /// read an UCS string
   UCS_string read_UCS();

   /// read a token
   bool read_Token(Token_loc & tloc);

   /// read a system function with attribute id_prefix-id or a user defined
   /// functions with attributes 'ufun_prefix-ufun' and 'level_prefix-prefix'
   Function * read_Function_name(const char * ufun_name, const char * si_level,
                                 const char * sysfun_id);

   /// find a lambda in the current SI entry
   Function * find_lambda(const UCS_string & lambda);

   /// return true iff there is more data in the file
   bool more() const   { return data < file_end; }

   /// show some characters starting at the current position
   void where(ostream & out);

   /// show attributes of current tag
   void where_att(ostream & out);

   /// set \b current_char to next (UTF-8 encoded) char, return true if EOF
   bool get_uni();

   /// return true iff \b tagname starts with prefix
   bool is_tag(const char * prefix) const;

   /// check that is_tag(prefix) is true and print error info if not
   void expect_tag(const char * prefix, const char * loc) const;

   /// print current tag
   void print_tag(ostream & out) const;

   /// find attribute \b att_name and return a pointer to its value if found)
   const UTF8 * find_attr(const char * att_name, bool optional);

   /// return integer value of attribute \b att_name
   int64_t find_int_attr(const char * att_name, bool optional, int base);

   /// return floating point value of attribute \b att_name
   APL_Float find_float_attr(const char * att_name);

   /// the file descriptor for the mmap()ed workspace.xml file
   int fd;

   /// the start of the file (for mmap())
   void * map_start;

   /// the length of the file (for mmap())
   size_t map_length;

   /// the start of the file
   const char * file_start;

   /// the start of the current line
   const UTF8 * line_start;

   /// the current line
   int line_no;

   /// the current char
   Unicode current_char;

   /// the next char
   const UTF8 * data;

   /// the name of the current tag, e.g. "Symbol" or "/Symbol"
   const UTF8 * tag_name;

   /// the attributes of the current tag
   const UTF8 * attributes;

   /// the end of attributes
   const UTF8 * end_attr;

   /// the end of the file
   const UTF8 * file_end;

   /// all values in the workspace
   std::vector<Value_P> values;

   /// true for )COPY and )PCOPY, false for )LOAD
   bool copying;

   /// true for )PCOPY, false for )COPY and )LOAD
   bool protection;

   /// true if reading vids (preparation for )COPY or )PCOPY)
   bool reading_vids;

   /// the vids to be copied (empty if all)
   std::vector<int> vids_COPY;

   /// the names of objects (empty if all)
   UCS_string_vector allowed_objects;

   /// true for selective copy (COPY with symbol names)
   bool have_allowed_objects;

   /// a value ID and the ID of its parent
   struct _vid_pvid
     {
       int vid;    ///< value ID
       int pvid;   ///< parent's value ID
     };

   /// parents[vid] os the parent of vid, or -1 if vid is a top-level value
   std::vector<int> parents;

   /// the file name from which this archive was read
   const char * filename;

   /// true if file contains a <\/Workspace> tag at the end
   bool file_is_complete;
};
//-----------------------------------------------------------------------------

