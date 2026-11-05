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

#ifndef __ID_HH_DEFINED__
#define __ID_HH_DEFINED__

#include <iostream>

#include "IdEnums.hh"
#include "TokenEnums.hh"
#include "UTF8_string.hh"
#include "UCS_string.hh"

class Function;
class Symbol;
class UCS_string;

//════════════════════════════════════════════════════════════════════════════
/**
 An Identifier for each internal object (primitives, Quad-symbols, and more).
 The ID can be derived in different ways:

 1. from a (user-defined) name,       e.g.  ID_APL_VALUE or ID_CHARACTER
 2. from a distinguished var name,    e.g.  ID_Quad_AI or ID_Quad_AV
 3. from a distinguished fun name,    e.g.  ID_Quad_AT or ID_Quad_EM
 4. from a special token (apl symbol) e.g.  ID_L_PARENT1 or ID_ASSIGN

  This is controlled by 5 corresponding macros: pp() qv() qf() resp. st() which are
  abbreviations of their purpose:

  pp (pretty print) : generic.                           e.g. FUN VAR
  qf                : Quad function,                     e.g. ⎕AF, ⎕AT
  qv                : Quad variable                      e.g. ⎕AI, ⎕ARG
  sf                  system function (aka. primitive)   e.g. F2_AND, F2_AND_B
  st                  system token                       e.g. BRANCH, COLON
 */
/// Identifier of an internal object of the APL interpreter
class ID
{
public:
   /// constructor
   /// @param _id   the identifier enum value
   /// @param name  printable name string for the identifier
   ID(Id _id, const char * name)
   : id(_id),
     name_utf(utf8P(name))
     {}

   /// search helper: compare \b id1 with id2
   /// @param key the Id value to search for
   /// @param id  the ID entry to compare against
   static int compare_id(const Id & key, const ID & id, const void *)
      { return key - id.id; }

   /// If \b tag is the tag of primitive function, primitive operator, or
   /// quad function, then return a pointer to it. Otherwise return 0.
   /// @param tag token tag whose upper 16 bits encode the Id
   static cFunction_P get_system_function(TokenTag tag)
      { return get_system_function(Id(tag >> 16)); }

   /// sort helper: compare IDs. Return \b true if id1 > id2
   /// @param id1 first ID to compare
   /// @param id2 second ID to compare
   static bool greater_id(const ID & id1, const ID & id2, const void *)
      { return id1.id > id2.id; }

   /// return the printable name for id as UTF8 *
   /// @param id the identifier to look up
   static const UTF8 * get_name(Id id);

   /// return the printable name for id as UCS_string
   /// @param id the identifier to look up
   static UCS_string get_name_UCS(Id id);

   /// If \b id is the ID of primitive function, primitive operator, or
   /// quad function, then return a pointer to it. Otherwise return 0.
   /// @param id the identifier to look up
   static cFunction_P get_system_function(Id id);

   /// If \b id is the ID of a quad variable, then return a pointer to its
   /// symbol. Otherwise return 0.
   /// @param id the identifier to look up
   static Symbol * get_system_variable(Id id);

   /// return the TokenTag for \b id
   /// @param id the identifier to convert
   static TokenTag get_token_tag(Id id);

protected:
   /// the ID (defined in IdEnums.hh)
   Id id;
     
   /// how \b id is being printed
   const UTF8 * name_utf;

   /// a sorted vector of all IDs. Created on demand
   static vector<ID> all_IDs;
};
//════════════════════════════════════════════════════════════════════════════

#include "TokenEnums.hh"

#endif // __ID_HH_DEFINED__

