/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#ifndef __SYMBOLTABLE_HH_DEFINED__
#define __SYMBOLTABLE_HH_DEFINED__

#include <stdint.h>

#include "UCS_string.hh"

//----------------------------------------------------------------------------
/// common part of user-defined names and distinguished names
template <typename T, size_t SYMBOL_COUNT>
class SymbolTableBase
{
public:
   enum { max_symbol_count = SYMBOL_COUNT };

   /// Construct an empty \b SymbolTable.
   SymbolTableBase()
     { memset(symbol_table, 0, sizeof(symbol_table)); }

   ~SymbolTableBase()
      {
        loop(s, SYMBOL_COUNT)
           for (T * sym = symbol_table[s]; sym;)
           {
              T * del = sym;
              sym = sym->next;
              delete del;
           }
      }

   /// return a \b Symbol with name \b name in \b this \b SymbolTable.
   /// @param name symbol name to look up
   T * lookup_existing_symbol(const UCS_string & name)
      {
        const uint32_t hash = compute_hash(name);
        for (T * sym = symbol_table[hash]; sym; sym = sym->next)
            {
              if (!sym->equal(name))   continue;   // name mismatch
              if (!sym->is_erased())      return sym;
            }

        return 0;
      }

   /// return a \b Symbol with name \b name in \b this \b SymbolTable.
   /// @param name symbol name to look up
   const T * lookup_existing_symbol(const UCS_string & name) const
      {
        const uint32_t hash = compute_hash(name);
        for (const T * sym = symbol_table[hash]; sym; sym = sym->next)
            {
              if (!sym->equal(name))   continue;   // name mismatch
              if (!sym->is_erased())      return sym;
            }

        return 0;
      }

   /// return the name to which \b lambda ia assigned (empty if not found)
   /// @param lambda user-defined lambda function to search for
   UCS_string find_lambda_name(const UserFunction * lambda);

   /// add \b sym to the symbol table. The caller has checked
   /// that new_name does not yet exist in the symbol table
   /// @param sym symbol to insert into the hash table
   void add_symbol(T * sym)
       {
         const uint32_t hash = compute_hash(sym->get_name());
         if (symbol_table[hash] == 0)   // unused slot
            {
              symbol_table[hash] = sym;
              sym->next = 0;
              return;
            }

         for (T * t = symbol_table[hash]; ; t = t->next)
             {
               if (t->next == 0)   // append new_symbol at the end
                  {
                    t->next = sym;
                    sym->next = 0;
                    return;
                  }
             }
       }

   /// compute a 16-bit hash for \b name
   /// @param name symbol name to hash
   static uint32_t compute_hash(const UCS_string & name)
      {
        uint32_t hash = name.FNV_hash();
        hash = (((hash >> 16) ^ hash) & 0x0000FFFF) % SYMBOL_COUNT;

        Log(LOG_SYMBOL_lookup_symbol)
           {
              CERR << "name[len=" << name.size() << "] " << name
                   << " has hash " << HEX(hash)
                   << " in table of size " << SYMBOL_COUNT << endl;
           }

        return  hash;
     }

protected:
   /// Hash table for all symbols.
   T * symbol_table[SYMBOL_COUNT];
};

class Symbol;

//----------------------------------------------------------------------------
/// The table containing all user defined symbols.
class SymbolTable : public SymbolTableBase<Symbol, SYMBOL_HASH_TABLE_SIZE>
{
public:
   /// Return or create a \b Symbol with name \b ucs in \b this \b SymbolTable.
   /// @param ucs symbol name to look up or create
   Symbol * lookup_symbol(const UCS_string & ucs);

   /// return the name to which \b lambda ia assigned (empty if not found)
   /// @param lambda user-defined lambda function to search for
   UCS_string find_lambda_name(const UserFunction * lambda);

   /// List all symbols in \b this \b SymbolTable (for )VARS, )FNS etc.)
   /// @param out output stream to write the listing to
   /// @param which category of names to include in the listing
   /// @param from_to optional name range filter (start–end)
   void list(ostream & out, ListCategory which, UCS_string from_to) const;

   /// clear the marked flag of all symbols
   void unmark_all_values() const;

   /// print variables owning value
   /// @param out output stream to write ownership information to
   /// @param value APL value whose owners are reported
   int show_owners(ostream & out, const Value & value) const;

   /// clear this symbol table (remove all user-defined symbols)
   /// @param out output stream for diagnostic messages
   void clear(ostream & out);

   /// clear one slot (hash) in this symbol table
   /// @param out output stream for diagnostic messages
   /// @param hash slot index to clear
   void clear_slot(ostream & out, int hash);

   /// erase symbols from \b this SymbolTable
   /// @param out output stream for diagnostic messages
   /// @param symbols list of symbol names to erase
   void erase_symbols(ostream & out, const UCS_string_vector & symbols);

   /// List details of single symbol in buf.
   /// @param out output stream to write the symbol details to
   /// @param buf name of the symbol to describe
   ostream & list_symbol(ostream & out, const UCS_string & buf) const;

   /// write all symbols in )OUT format to file \b out
   /// @param out file to write serialised symbols to
   /// @param seq running sequence number for )OUT records
   void write_all_symbols(FILE * out, uint64_t & seq) const;

   /// return all symbols  (including erased symbols)
   std::vector<const Symbol *> get_all_symbols() const;

   /// dump symbols to out
   /// @param out output stream to write the dump to
   /// @param fcount running count of functions found, updated in place
   /// @param vcount running count of variables found, updated in place
   void dump(ostream & out, int & fcount, int & vcount) const;

protected:
   /// erase one symbol, return \b true on error, \b false on success
   /// @param sym name of the symbol to erase
   bool erase_one_symbol(const UCS_string & sym);
};
//============================================================================
class QuadFunction;
class SystemVariable;

/// a distinguished name (i.e. ⎕xx)
class SystemName
{
public:
   /// constructor: system variable or function
   /// @param var_name distinguished name string (e.g. ⎕IO)
   /// @param var_id internal Id enum for this name
   /// @param fun pointer to the system function, or 0 if none
   /// @param var pointer to the system variable, or 0 if none
   SystemName(const UCS_string & var_name, Id var_id,
              QuadFunction * fun,  SystemVariable * var)
   : name(var_name),
     id(var_id),
     function(fun),
      sysvar(var)
   {}

   /// return the distinguished name
   UCS_string get_name() const
      { return name; }

   /// system variables are never erased
   bool is_erased() const   { return false; }

   /// Compare the name of \b this \b Symbol with \b ucs
   /// @param ucs name to compare against
   bool equal(const UCS_string & ucs) const
      { return (name.compare(ucs) == COMP_EQ); }

   /// return the variable (if any) that this name represents
   SystemVariable * get_variable() const
      { return sysvar; }

   /// return the function (if any) that this name represents
   QuadFunction * get_function() const
      { return function; }

   /// return the ID of this name
   Id get_id() const
      { return id; }

   /// The next name with the same hash value as \b this \b SystemName
   SystemName * next;

protected:
   /// the name (including ⎕). Eg. ⎕IO
   const UCS_string name;

   /// the Id of the variable or function
   const Id id;

   /// the function if the name refers to a system function, or 0 if not
   QuadFunction * function;

   /// the variable if the name refers to a system variable, or 0 if not
   SystemVariable * sysvar;
};
//----------------------------------------------------------------------------
/// The table containing all system defined symbols (aka. distinguished names)
class SystemSymTab : public SymbolTableBase<SystemName, 256 - 1>
{
public:
   // constructor
   SystemSymTab()
   : max_name_len(0)
   {}

   /// clear this symbol table (remove all user-defined symbols)
   /// @param out output stream for diagnostic messages
   void clear(ostream & out);

   /// clear one slot (hash) in this symbol table
   /// @param out output stream for diagnostic messages
   /// @param hash slot index to clear
   void clear_slot(ostream & out, int hash);

   /// add \b function to the symbol table
   /// @param name distinguished name string for the function
   /// @param id internal Id enum for this function
   /// @param function pointer to the QuadFunction to register
   void add_function(const UCS_string & name, Id id, QuadFunction * function)
      { add_fun_or_var(name, id, function, 0); }

   /// add \b variable to the symbol table
   /// @param name distinguished name string for the variable
   /// @param id internal Id enum for this variable
   /// @param variable pointer to the SystemVariable to register
   void add_variable(const UCS_string & name, Id id,
                     SystemVariable * variable)
      { add_fun_or_var(name, id, 0, variable); }

   /// don't add ⍺ and friends
   /// @param name distinguished name string (ignored)
   /// @param id internal Id enum (ignored)
   /// @param variable user Symbol pointer (ignored)
   void add_variable(const UCS_string & name, Id id, Symbol * variable)
      { }

protected:
   /// add a function or variable
   /// @param name distinguished name string to register
   /// @param id internal Id enum for the name
   /// @param function pointer to the QuadFunction, or 0 if registering a variable
   /// @param variable pointer to the SystemVariable, or 0 if registering a function
   void add_fun_or_var(const UCS_string & name, Id id,
                       QuadFunction * function, SystemVariable * variable);

   /// the length of the longest name
   int max_name_len;
};
//============================================================================

#endif // __SYMBOLTABLE_HH_DEFINED__
