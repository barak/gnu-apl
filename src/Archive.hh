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

#include <stdint.h>

#include <fstream>
#include <string.h>

#include "DerivedFunction.hh"
#include "SystemLimits.hh"
#include "UCS_string.hh"
#include "UTF8_string.hh"
#include "Value.hh"
#include "Workspace.hh"

class Cell;
class Function;
class Executable;
class Prefix;
class Symbol;
class StateIndicator;
class SymbolTable;
class Token;
class Token_loc;
class Value;
class ValueStackItem;

using namespace std;

//════════════════════════════════════════════════════════════════════════════
/// class for )SAVEing and )LOADing APL Workspaces in an XML file format
class XML_Archive
{
protected:
   /// @param of stream for informational output
   /// @param ef stream for error output
   XML_Archive(ostream & of, ostream & ef)
   : err(ef),
     out(of)
   {}

   /** archive syntax version, hopefully stepped up after Archive.hh or
      Archive.cc were changed:

      ASX_OTHER: increment to indicate change in Archive.hh/cc that DOES NOT
                 change the XML file format,

      ASX_MINOR: increment to indicate a backward-compatible change in the XML
                 file format, i.e. older Archive.hh/hh versions can still read
                 it but may not benefit from the change,

      ASX_MAJOR: increment to indicate an incompatible change in the XML file
                 format
    **/
   enum ArchiveSyntax
      {
        ASX_MAJOR =  1,   ///< ++ if incompatible XML file format change
        ASX_MINOR = 15,  ///< ++ if backward compatible XML file format change
        ASX_OTHER =  8,   ///< ++ if no XML file format change (code cleanup)
      };

   /// where to send error messages
   ostream & err;

   /// where to send information messages (such as "SAVED...")
   ostream & out;

   /// the fixed text that starts the checksum comment written after
   /// \</Workspace\> (shared between the writer and the verifier)
   static const char * checksum_prefix()
      { return "<!-- checksum: crc32="; }

   /// normalize \b data (the \<Workspace\>...\</Workspace\> region of a
   /// )SAVEd .xml file) for checksum purposes: strip the leading and
   /// trailing whitespace of every physical line and treat \\n, \\r,
   /// and \\r\\n alike as line boundaries, so that a text editor's
   /// whitespace-/line-ending-only reformatting of the file is not
   /// mistaken for corruption. Literal APL data (ravel/token/name
   /// content) is never affected: it always lives inside an XML
   /// attribute value, is never itself the leading or trailing
   /// whitespace of a physical line (only the writer's own structural
   /// line-wrap indentation ever is), and is otherwise left untouched.
   /// @param data start of the byte range to normalize
   /// @param len number of bytes
   static string normalize_for_checksum(const char * data, size_t len);
};
//────────────────────────────────────────────────────────────────────────────
/// a helper class for saving an APL workspace
class XML_Saving_Archive: public XML_Archive
{
public:
   /// constructor: remember output stream and  workspace
   /// @param of stream for informational output
   /// @param ef stream for error output
   /// @param filename path of the XML workspace file to write
   XML_Saving_Archive(ostream & of, ostream & ef, const char * filename);

   /// destructor
   ~XML_Saving_Archive()   { outf.close(); }

   /// an index for \b values
   //
   // fixed underlying type: this enum's only enumerator is -1, so an
   // unscoped enum with no fixed type would take its legal range from
   // that (i.e. [-1, 0]) even though real (pointer-derived) Vids are
   // stored here -- UB on every load of an in-range-looking but
   // non-sentinel value (Bugs18 #7, confirmed via UBSan on the plain
   // testcase suite).
   //
   enum Vid : int64_t { INVALID_VID = -1 };

   /// a value and its parent (if the parent is nested, -1 if not)
   struct _val_par
      {
         /// default constructor
         _val_par()
         : _val(0),
           _par(INVALID_VID),
           _depth(-1)
         {}

         /// constructor
         /// @param v pointer to the APL value
         /// @param par value ID of the parent, or INVALID_VID for top-level
         _val_par(const cValue * v, Vid par)
         : _val(v),
           _par(par),
           _depth(v->compute_depth())
         {}

         /// the value
         const cValue * _val;

         /// the optional parent, -1 for top-level values
         Vid _par;

         /// the depth of the value
         APL_types::Depth _depth;

         /// assign \b other
         /// @param other source _val_par to copy from
         void operator=(const _val_par & other)
            { _val = other._val;   _par = other._par;   _depth = other._depth; }

         /// compare function for Heapsort::sort()
         /// @param A left-hand operand
         /// @param B right-hand operand
         static bool greater(const _val_par & A, const _val_par & B,
                             const void *)
            { return A._val > B._val; }

         /// compare function for binary searches.
         /// @param key value pointer to search for
         /// @param B element to compare against
         static int compare(const cValue * const & key,
                            const _val_par & B, const void *)
            {
              /* CAUTION: Cannot use int64_t(key) - int64_t(B._val) here
                 because it would ignore the upper 32_bits of the difference.
               */
              if (key < B._val)   return -2;   // key is smaller
              if (key > B._val)   return +2;   // key is larger
              return 0;                        // same (key found)
            }
      };

   /// return \b true iff )SAVE was successful
   bool saved_OK()
      { return save_success; }

   /// write entire workspace
   XML_Saving_Archive & save();

   /// write derived functions cache
   /// @param fns cache of derived functions to serialise
   void save_Derived(const DerivedFunctionCache & fns);

   /// write user-defined function \b fun
   /// @param fun function to serialise
   void save_Function(const Function & fun);

   /// write either the name and SI-level of user-defined function or the
   /// id of a system function. Return number of attribute= items written
   /// @param fun function whose name/id is written
   int save_Function_name(const Function & fun);

   /// write all function
   void save_functions();

   /// write the Prefix parser of \b si (and its derived functions)
   /// @param si state indicator entry whose parser is written
   void save_Parser(const StateIndicator & si);

   /// write ravel of Value \b v
   /// @param vid index of the value whose ravel is written
   XML_Saving_Archive & save_Ravel(Vid vid);

   /// write Value \b v except its ravel
   /// @param vid index of the value in the values array
   XML_Saving_Archive & save_shape(Vid vid);

   /// write StateIndicator entry \b si
   /// @param si state indicator entry to serialise
   void save_SI_entry(const StateIndicator & si);

   /// write Symbol \b sym
   /// @param sym symbol to serialise
   void save_Symbol(const Symbol & sym);

   /// write SymbolTable \b symtab
   /// @param symtab symbol table to serialise
   void save_symtab(const SymbolTable & symtab);

   /// write Token_loc \b tloc
   /// @param tloc token location record to serialise
   void save_token_loc(const Token_loc & tloc);

   /// write UCS_string \b str
   /// @param str Unicode string to serialise
   void save_UCS(const UCS_string & str);

   /// write all user defined commands
   /// @param cmds list of user-defined commands to serialise
   void save_user_commands(const std::vector<Command::user_command> & cmds);

   /// write ValueStackItem \b vsi
   /// @param vsi value stack item to serialise
   void save_vstack_item(const ValueStackItem & vsi);


protected:
   /// width of one indentation level
   enum { INDENT_LEN = 2 };

   /// enter char mode. maybe print ² and return the number of chars printed
   int enter_char_mode()
      { if (char_mode)   return 0;   // already in char mode
        outf << UNI_PAD_U2;   char_mode = true;   return 1; }

   /// leave char mode. maybe print ⁰ and return the number of chars printed
   int leave_char_mode()
      { if (!char_mode)   return 0;   // not in char mode
        outf << UNI_PAD_U0;   char_mode = false;   return 1; }

   /// return true iff (the definition of) \b fun was already saved.
   /// @param fun function to check for prior serialisation
   bool is_saved(const Function * fun) const;

   /// indent by \b indent levels, return space left on line
   int do_indent();

   /// emit one ravel cell \b cell
   /// @param cell ravel cell to emit
   /// @param space remaining character budget on the current line
   void emit_cell(const Cell & cell, int & space);

   /// emit a token value up to (excluding) the '>' of the end token
   /// @param tok token whose value is emitted
   void emit_token_val(const Token & tok);

   /// emit one unicode character (inside "...")
   /// @param uni Unicode code point to emit
   /// @param space remaining character budget on the current line
   void emit_unicode(Unicode uni, int & space);

   /// return the index of \b val in values
   /// @param val pointer to the APL value to look up
   Vid find_vid(const cValue * val);

   void write_XML_header();

   /// append a "<!-- checksum: crc32=XXXXXXXX -->" comment after the
   /// \</Workspace\> line already written to outf, computed over the
   /// normalize_for_checksum()d \<Workspace\>...\</Workspace\> region.
   /// Re-reads the file just written (simplest way to get at the exact
   /// bytes without threading a tee through every outf << call in
   /// save()).
   void write_checksum();

   /// decrement \b space by length of \b str and return \b str
   /// @param space character budget counter to decrement
   /// @param str string whose length is subtracted from space
   static const char * decr(int & space, const char * str);

   /// return true if \b uni prints nicely and is allowed in XML "..." strings
   /// @param uni Unicode code point to test
   static bool xml_allowed(Unicode uni);

   /// true iff ² is pending
   bool char_mode;

   /// current indentation level
   int indent;

   /// output XML file
   ofstream outf;

   /// path of outf, kept for write_checksum()'s re-read
   const char * filename;

   /// the temporary path outf actually writes to (filename + ".tmp",
   /// renamed onto filename only once save() has fully succeeded) --
   /// write_checksum() must re-read *this*, not filename, since filename
   /// may still be the previous (or nonexistent) workspace file while
   /// outf is open (Bugs28 #29).
   string tmp_path;

   /// \b true iff )SAVE was successful
   bool save_success;

   /// functions saved so far
   vector<const Function *> saved_Functions;

   /// an array of values and the Vid of its parent (if value is a
   ///sub-value of a nested parent). The top-level of an APL value has
   /// has no parents (i.e. INVALID_VID).
   /// all values in the workspace
   vector<_val_par> val_pars;
};
//────────────────────────────────────────────────────────────────────────────
/// a helper class for loading an APL workspace
class XML_Loading_Archive: public XML_Archive
{
public:
   /// constructor: remember file name and workspace
   /// @param of stream for informational output
   /// @param ef stream for error output
   /// @param _filename path of the XML workspace file to read
   /// @param dump_fd file descriptor for optional hex dump (-1 if unused)
   XML_Loading_Archive(ostream & of, ostream & ef, const char * _filename,
                       int & dump_fd);

   /// destructor (unmap()s file).
   ~XML_Loading_Archive();

   /// return true iff constructor could open the file
   bool is_open() const   { return file_start != 0; }

   /// return true iff the file was opened (is_open()) *and* looks like a
   /// GNU APL .xml workspace (or a )DUMP .apl file, i.e. dump_fd was set).
   /// false means the constructor already printed a diagnostic (e.g. "file
   /// X does not have the format of a GNU APL .xml or .apl file") and the
   /// caller should stop rather than attempt to read_Workspace()/
   /// read_vids() on content that isn't XML at all.
   bool is_valid_format() const   { return valid_format; }

   /// set copying and maybe set protection
   /// @param prot true if protection (⎕PCOPY) is requested
   /// @param allowed names of objects to copy; empty means copy all
   void set_protection(bool prot, const UCS_string_vector & allowed)
      { copying = true;   protection = prot;   allowed_objects = allowed;
        have_allowed_objects = allowed_objects.size() > 0; }

   /// undo copy_snapshot_symbol()'s effect for every Symbol touched so
   /// far by the current )COPY, in response to a )COPY that failed
   /// part-way through; clears the snapshot list afterwards. Called by
   /// Workspace::copy_WS() when read_Workspace() throws.
   void copy_restore_snapshots();

   /// check compatibility information in the workspace and maybe warn the
   /// user
   void check_compatibility();

   /// return true iff the file ends the way )SAVE always writes it
   /// (XML_Saving_Archive::operator<<(Workspace&): 4 NUL bytes followed
   /// by a newline, "so that string functions can be used if the file
   /// should be mmap()ed"). Every ordinary (i.e. not specially bounded)
   /// u8::strstr()/strtoll()/strtod()/sscanf() call below relies on this
   /// holding -- a corrupted or truncated file may not have it, so every
   /// public entry point that starts scanning the mmap'd file (currently
   /// read_Workspace() and check_checksum(), the two ways to end up
   /// calling get_checksum_status()) must check it first, before this
   /// (or any other) precondition it protects.
   bool has_trailing_NUL() const
      { return file_length >= 5 &&
               file_end[-1] == '\n' &&
               file_end[-2] == 0 && file_end[-3] == 0 &&
               file_end[-4] == 0 && file_end[-5] == 0; }

   /// if a "<!-- checksum: crc32=XXXXXXXX -->" comment is present after
   /// \</Workspace\>, recompute it over the normalize_for_checksum()d
   /// \<Workspace\>...\</Workspace\> region and warn (but still accept the
   /// workspace) if it does not match. Silently does nothing if no such
   /// comment is present (e.g. an older file, or one not written by
   /// GNU APL).
   void verify_checksum();

   /// outcome of get_checksum_status()
   enum ChecksumStatus
      {
        CS_NO_WORKSPACE,   ///< no \<Workspace\>...\</Workspace\> found
        CS_NO_CHECKSUM,    ///< no checksum comment (e.g. an older file)
        CS_OK,             ///< checksum present and matching
        CS_MISMATCH,       ///< checksum present but not matching
      };

   /// compute the checksum status of this (already mmap()ed, not yet
   /// parsed) file; \b stored_crc and \b computed_crc are only set for
   /// CS_OK and CS_MISMATCH.
   ChecksumStatus get_checksum_status(uint32_t & stored_crc,
                                      uint32_t & computed_crc) const;

   /// like verify_checksum(), but for )CHECK_WS: report the outcome
   /// (including CS_OK and CS_NO_CHECKSUM, unlike verify_checksum())
   /// on \b out, without loading the workspace.
   void check_checksum(ostream & out);

   /// move to next tag, return true if EOF
   /// @param loc caller location for diagnostics
   bool next_tag(const char * loc);

   /// read vids of top-level variables
   void read_vids();

   /// read an entire workspace, throw DOMAIN_ERROR on failure
   /// @param silent suppress progress messages when true
   void read_Workspace(bool silent);

   /// reset archive to the start position
   void reset();

   /// skip to tag \b tag, return EOF if end of file
   /// @param tag XML tag name to search for
   bool skip_to_tag(const char * tag);

protected:
   /// a value ID in a )SAVEd workspace
   //
   // fixed underlying type -- see XML_Saving_Archive::Vid above; the same
   // hazard applies on the )LOAD side (Bugs18 #7).
   //
   enum Vid : int64_t { NO_VID = int64_t(-1) };   ///< no (invalid) value ID

   /// the address of a function in a )SAVEd workspace
   enum Fid : int64_t { NO_FID = int64_t(-1) };   ///< no (invalid) function ID

   /// a value ID and the ID of its parent
   struct _vid_pvid
     {
       Vid vid;    ///< value ID
       Vid pvid;   ///< parent's value ID
     };

   /// one mapping from an fid in the old (SAVEed) workspace to the function
   /// address in the new (LOADing) workspace
   struct fun_map
      {
        Fid old_fid;           ///< the fid in the )SAVEed workspace
        cFunction_P new_fun;   ///< address in the )LOADing workspace
        const char * loc;      ///< where allocated
      };

   /// properties of a derived function
   struct _derived_todo
      {
        Function * cache;       ///< the new()-address of the function
        cFunction_P * symptr;   ///< the address of a function * to be set
        Fid fid;                ///< the address of \b this derived function
        Fid LO_fid;             ///< the address of \b LO of this function
        Fid OPER_fid;           ///< the address of \b OPER of this function
        Fid RO_fid;             ///< the address of \b RO of this function
        Vid AXIS_vid;           ///< the AXIS of \b this function
        const char * loc;       ///< where \b this was initialized
      };

   /// the pre-)COPY state of a Symbol, taken right before )COPY's
   /// top-level (d==0) entry for it is applied, so that a )COPY that
   /// fails part-way through (a later <Symbol> throwing) can restore
   /// every Symbol it already touched instead of leaving the workspace
   /// a half-copied mix of old and new bindings. NC_LABEL/NC_SHARED_VAR
   /// are restored only to NC_UNUSED_USER_NAME (a known, accepted gap:
   /// still safe, just not faithful for those two rare top-level cases).
   struct _copy_snapshot
      {
        Symbol * symbol;        ///< the Symbol being overwritten
        int size;                ///< its value_stack_size() before )COPY
        NameClass nc;            ///< its top_of_stack()->get_NC() before )COPY
        Value_P value;           ///< valid iff nc == NC_VARIABLE
        cFunction_P function;    ///< valid iff nc == NC_FUNCTION/NC_OPERATOR
      };

   /// return true iff there is more data in the file
   bool more() const   { return data < file_end; }

   /// return Fid value of attribute \b att_name
   /// @param att_name XML attribute name to look up
   /// @param optional true if the attribute may be absent
   /// @param base numeric base for integer parsing (e.g. 10 or 16)
   Fid find_Fid_attr(const char * att_name, bool optional, int base)
      { return Fid(find_int_attr(att_name, optional, base)); }

   /// find mandatory attribute \b att_name. Return a pointer to its value
   ///  if found and throw DOMAIN ERROR if not.
   /// @param att_name XML attribute name that must be present
   const UTF8 * find_mandatory_attr(const char * att_name)
      { return find_attr(att_name, false); }

   /// find optional attribute \b att_name. Return a pointer to its value
   /// if found and 0 if not.
   /// @param att_name XML attribute name to look up
   const UTF8 * find_optional_attr(const char * att_name)
      { return find_attr(att_name, true); }

   /// return Vid value of attribute \b att_name
   /// @param att_name XML attribute name to look up
   /// @param optional true if the attribute may be absent
   /// @param base numeric base for integer parsing (e.g. 10 or 16)
   Vid find_Vid_attr(const char * att_name, bool optional, int base)
      { return Vid(find_int_attr(att_name, optional, base)); }

   /// check that is_tag(prefix) is true and print error info if not
   /// @param prefix expected XML tag name prefix
   /// @param loc caller location for diagnostics
   void expect_tag(const char * prefix, const char * loc) const;

   /// return true iff \b tagname starts with prefix
   /// @param prefix XML tag name prefix to test
   bool is_tag(const char * prefix) const;

   /// print current tag
   void print_tag() const;

   /// add fid and function to find_fun_map. Either fid must be new, or else
   /// an existing fid must have its new_fun == 0 (forward declaration).
   /// @param fid function ID from the saved workspace
   /// @param new_fun resolved function pointer in the loading workspace
   /// @param loc caller location for diagnostics
   void add_fid_function(Fid fid, cFunction_P new_fun, const char * loc);

   /// remember symbol's current top-of-stack state before a )COPY
   /// overwrites it (no-op for )LOAD, i.e. unless copying is set).
   /// @param symbol the Symbol about to be overwritten by )COPY
   void copy_snapshot_symbol(Symbol * symbol);

   /// find attribute \b att_name and return: a pointer to its value if found,
   /// 0 if optional is true, and throw DOMAIN ERROR if optional is false.
   /// @param att_name XML attribute name to look up
   /// @param optional true if the attribute may be absent
   const UTF8 * find_attr(const char * att_name, bool optional);

   /// scan forward from \b from for the next occurrence of \b stop,
   /// bounded by file_end; throw DOMAIN ERROR if not found (an attribute
   /// value with no closing quote would otherwise scan past the mmap'd
   /// file into unmapped memory).
   /// @param from position to start scanning from
   /// @param stop character to scan for
   const UTF8 * scan_for(const UTF8 * from, char stop);

   /// return floating point value of attribute \b att_name
   /// @param att_name XML attribute name to look up
   APL_Float find_float_attr(const char * att_name);

   /// return fun_map for fid, or 0 if not found.
   /// @param fid function ID from the saved workspace
   fun_map * find_fun_map(Fid fid);

   /// return function for fid, or 0 if not found.
   /// @param fid function ID from the saved workspace
   cFunction_P find_function(Fid fid);

   /// return integer value of attribute \b att_name
   /// @param att_name XML attribute name to look up
   /// @param optional true if the attribute may be absent
   /// @param base numeric base for integer parsing (e.g. 10 or 16)
   int64_t find_int_attr(const char * att_name, bool optional, int base);

   /// find a lambda in the current SI entry
   /// @param lambda Unicode name of the lambda to locate
   cFunction_P find_lambda(const UCS_string & lambda);

   /// set \b current_char to next (UTF-8 encoded) char, return true if EOF
   bool get_uni();

   /// instantiate the derived functions in \b derived_todos
   /// @param allocate true to allocate new derived functions, false to link them
   void instantiate_derived_functions(bool allocate);

   /// read one or more Cell(s) from input into the ravel of Z;
   /// return the  UTF8 * after the cells.
   /// @param Z target APL value whose ravel receives the cells
   /// @param input pointer to the encoded cell data in the file buffer
   const UTF8 * read_Cells(Value & Z, const UTF8 * input);

   /// read next Command element
   void read_Command();

   /// read all user-defined commands
   void read_Commands();

   /// read next derived function
   /// @param si state indicator entry that owns the derived function
   /// @param lev SI nesting level of the derived function
   void read_Derived(StateIndicator & si, int lev);

   /// read next Function element
   void read_Function();

   /// read next Function element
   /// @param d depth (SI level) of the symbol entry
   /// @param symbol symbol to receive the function binding
   void read_Function(int d, Symbol & symbol);

   /// read a system function with attribute id_prefix-id or a user defined
   /// functions with attributes 'ufun_prefix-ufun' and 'level_prefix-prefix'
   cFunction_P read_Function_name();

   /// read next Label element
   /// @param d depth (SI level) of the symbol entry
   /// @param symbol symbol to receive the label value
   void read_Label(int d, Symbol & symbol);

   /// read a lambda
   /// @param lambda_name UTF-8 encoded name of the lambda to read
   Executable * read_lambda(const UTF8 * lambda_name);

   /// read parsers in SI entry
   /// @param si state indicator entry to populate with parser data
   /// @param lev SI nesting level being restored
   void read_Parser(StateIndicator & si, int lev);

   /// read next Ravel element
   void read_Ravel();

   /// read next Shared-Variable element
   /// @param d depth (SI level) of the symbol entry
   /// @param symbol symbol to receive the shared-variable binding
   void read_Shared_Variable(int d, Symbol & symbol);

   /// read next StateIndicator entry
   /// read the Error of an SI entry
   /// @param si the SI entry (being read)
   void read_SI_Error(StateIndicator & si);

   /// @param level nesting depth of the SI entry to read
   void read_SI_entry(int level);

   /// read ⍎ Executable
   const Executable * read_SI_Execute();

   /// read ◊ Executable
   const Executable * read_SI_Statement();

   /// read a user defined Executable
   const Executable * read_SI_UserFunction();

   /// read next StateIndicator element
   void read_StateIndicator();

   /// read next Symbol element
   void read_Symbol();

   /// read next Symbol element
   void read_SymbolTable();

   /// read a token
   /// @param tloc token location record to populate
   bool read_Token(Token_loc & tloc);

   /// read an UCS string
   UCS_string read_UCS();

   /// read next unused-name element
   /// @param d depth (SI level) of the symbol entry
   /// @param symbol symbol to update
   void read_unused_name(int d, Symbol & symbol);

   /// read next Value element
   void read_Value();

   /// read next Variable element
   /// @param d depth (SI level) of the symbol entry
   /// @param symbol symbol to receive the variable binding
   void read_Variable(int d, Symbol & symbol);

   /// input is a UTF8-encoded sequence of bytes, terminated with ".
   /// UTF8-decode the sequence, thereby removing the escape tagging
   /// with ⁰ (aka. UNI_PAD_U0), ¹ (UNI_PAD_U1), ²(UNI_PAD_U2), and \n.
   /// return the  UTF8 * pointing to the terminating ".
   /// @param ucs output string that receives the decoded Unicode characters
   /// @param input pointer to the UTF-8 encoded attribute value in the file buffer
   const UTF8 * read_XML_string(UCS_string & ucs, const UTF8 * input);

   /// the names of objects (empty if all)
   UCS_string_vector allowed_objects;

   /// the attributes of the current tag
   const UTF8 * attributes;

   /// true for )COPY and )PCOPY, false for )LOAD
   bool copying;

   /// pre-)COPY snapshots of every Symbol touched so far by the current
   /// )COPY, for copy_restore_snapshots() to undo on failure.
   vector<_copy_snapshot> copy_snapshots;

   /// the current char
   Unicode current_char;

   /// the next char
   const UTF8 * data;

      /// derived functions that need to be instantiated
      vector<_derived_todo> derived_todos;

   /// the end of attributes
   const UTF8 * end_attr;

   /// all mappings from fids to functions
   std::vector<fun_map> fid_to_function;

   /// the end of the workspace file
   const UTF8 * file_end;

   /// true if file contains a <\/Workspace> tag at the end
   bool file_is_complete;

   /// the length of the workspace file
   ssize_t file_length;

   /// the start of the workspace file
   const UTF8 * file_start;

   /// the file name from which this archive was read
   const char * filename;

   /// true for selective copy (COPY with symbol names)
   bool have_allowed_objects;

   /// the current line
   int line_no;

   /// the start of the current line
   const UTF8 * line_start;

   /// parents[vid] is the parent of vid, or NO_VID if vid is a top-level value
   std::vector<Vid> parents;

   /// true for )PCOPY, false for )COPY and )LOAD
   bool protection;

   /// true if reading vids (preparation for )COPY or )PCOPY)
   bool reading_vids;

   /// the name of the current tag, e.g. "Symbol" or "/Symbol"
   const UTF8 * tag_name;

   /// false if the constructor determined that the file does not have
   /// the format of a GNU APL .xml or .apl file (see is_valid_format())
   bool valid_format;

   /// all values in the workspace
   std::vector<Value_P> values;

   /// the vids to be copied (empty if all)
   std::vector<Vid> vids_COPY;
};
//════════════════════════════════════════════════════════════════════════════

