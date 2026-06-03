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

#ifndef __PARSER_HH_DEFINED__
#define __PARSER_HH_DEFINED__

#include "Token.hh"

class Executable;

/*!
     Functions related to parsing of input lines and defined functions.
 */

//════════════════════════════════════════════════════════════════════════════
/** A small dictionary for literals and their markers. A marker is a substring
    of the form "@N@" in the program text, @b  N is a key in the \b Lit_DB,
    and \b value is the value of the multiline string or literal.
 */
class Lit_DB
{
public:
   /// constructor
   Lit_DB()
   : next_key(0)
   {}

   /// @param out output stream to write to
   /// @param loc caller location for diagnostics
   void dump(ostream & out, const char * loc) const
      {
        out << "Lit_DB at " << loc << ":" << endl;
        loop(i, size())
            {
              out << "@" << items[i].key << "@: V-"
                  << items[i].value->get_shape() << endl;
            }
      }

   /// return the key that will be returned by the next \b push().
   ShapeItem get_next_key() const
      { return next_key; }

   /// return the number of literals in the dictionary
   ShapeItem size() const
      { return items.size(); }

   /// return the value for key, removing it from the dictionary.
   /// @param key dictionary key of the value to retrieve
   Value_P pull(ShapeItem key)
      {
         loop(i, size())
             {
                if (items[i].key == key)   // found
                   {
                     Value_P ret = items[i].value;
                     items[i] = items.back();
                     items.pop_back();
                     return ret;
                   }
             }
         return Value_P();
      }

   /// return the only remaining value, removing it from the dictionary.
   Value_P pull_last()
      {
        Assert(size() == 1);
        Value_P ret = items.back().value;
        items.pop_back();
        return ret;
      }

   /// store a new value in the dictionary, return its key.
   /// @param val APL value to store
   ShapeItem push(Value_P val)
      {
        const ShapeItem ret = next_key++;
        const Item item = { ret, val };
        items.push_back(item);
        return ret;
      }
protected:
   /// one key/value pair in the dictionary
   struct Item
      {
        ShapeItem key;
        Value_P   value;
      };

   /// all key/value pairs in the dictionary
   vector<Item> items;

   /// the key for the next key/value pair
   ShapeItem    next_key;
};

//════════════════════════════════════════════════════════════════════════════
/// A state machine for multiline strings and literals
class Multi_line_SM
{
public:
   // constructor
   Multi_line_SM()   { reset(); }

   /// destructor
   ~Multi_line_SM();

   /// return \b true if the top-level is a multiline literal (as opposed to
   /// a multiline string).
   bool in_literal() const
      { return literal_depth != 0; }

   /// return \b true if inside a multiline string or literal. Including
   /// start, excluding (top-level) end.
   bool inside_multi() const
      { return in_string || literal_depth; }

   /// reset state (after an error)
   void reset()
      {
        string_end = Unicode_0;
        in_string = false;
        literal_depth = 0;

      }

   // process next triple
   //
   /// @param triple next Unicode character to process
   void next(Unicode triple);

protected:
   Unicode string_end;
   bool in_string;
   int literal_depth;
};
//════════════════════════════════════════════════════════════════════════════
/// A parser for APL code
class Parser
{
public:
   /// constructor
   /// @param pm parsing mode for this parser
   /// @param loc caller location for diagnostics
   /// @param mac true if tokenizing macro code
   Parser(ParseMode pm, const char * loc, bool mac)
   : create_loc(loc),
     macro(mac),
     pmode(pm)
   {}

   /// Parse \b UCS_string \b input into token string \b tos.
   /// @param input APL source text to parse
   /// @param tos output token string receiving parsed tokens
   /// @param optimize true to apply optimisation passes after parsing
   ErrorCode parse(const UCS_string & input, Token_string & tos,
                   bool optimize) const;

   /// Parse \b Token_string \b input into token string \b tos.
   /// @param input token string to re-parse
   /// @param tos output token string receiving parsed tokens
   /// @param optimize true to apply optimisation passes after parsing
   ErrorCode parse(const Token_string & input, Token_string & tos,
                   bool optimize) const;

   /// quick (!) decision if tos[pos] is right of ←
   /// @param tos token string being examined
   /// @param pos index into \b tos to test
   static Assign_state get_assign_state(Token_string & tos, ShapeItem pos);

   /// return the \b Multiline_status of all lines in \b text.
   /// @param status output vector receiving per-line multiline status
   /// @param text source lines to analyse
   /// @param fun_header true if the first line is a function header
   /// @param strings true to also track multiline string status
   static ErrorCode get_multiline_status(vector<Multiline_status> & status,
                                         const UCS_string_vector & text,
                                         bool fun_header, bool strings);

   /// compute distances between matching (), [], and {}. The distance is
   /// always positive.
   /// @param tos token string whose brackets are matched
   /// @param backwards true to scan right-to-left
   static ErrorCode match_par_bra(Token_string & tos, bool backwards);

   /// substitute literal axes like [ VAL ] with their reduction (i.e.
   /// TOK_AXIS or TOK_INDEX, depending on VAL. Return \b true if so.
   /// @param tos token string to optimise in place
   static bool optimize_literal_axes(Token_string & tos);

   /// substitute some primitives with short arguments and short result,
   /// like 4⍴0 in place.
   /// @param tos token string to optimise in place
   static bool optimize_short_primitives(Token_string & tos);

   /// parse a multi-line literal into a single value
   /// @param text lines of APL source containing the literal
   /// @param literals dictionary accumulating parsed literal values
   /// @param fun_header true if parsing a function header context
   static Value_P parse_multi_literal(UCS_string_vector & text,
                                      Lit_DB & literals, bool fun_header);

   /// replace multi-line literals in \b text with marker symbols and store the
   /// marker symbols in \b literals.
   /// @param text source lines modified in place
   /// @param literals dictionary receiving replaced literal values
   /// @param fun_header true if the first line is a function header
   static ErrorCode replace_multi_line_literals(UCS_string_vector & text,
                                                Lit_DB & literals,
                                                bool fun_header);

   /// replace multi-line strings in \b text with marker symbols and store the
   /// marker symbols in \b literals.
   /// @param text source lines modified in place
   /// @param literals dictionary receiving replaced string values
   /// @param fun_header true if the first line is a function header
   static ErrorCode replace_multi_line_strings(UCS_string_vector & text,
                                               Lit_DB & literals,
                                               bool fun_header);

   /// transform a function body that contains an (old-style) multi-line
   /// string into a standard function body
   /// @param text source lines of the function body, modified in place
   static ErrorCode transform_old_multi_line_strings(UCS_string_vector & text);

protected:

   /// check if tos[pos] is the end of a value or of a function
   /// @param tos token string being examined
   /// @param pos index of the token to classify
   static bool check_if_value(const Token_string & tos, int pos);

   /// Collect consecutive smaller APL values or value token into vectors
   /// @param tos token string modified in place
   static bool collect_constants(Token_string & tos);

   /// Replace (V1, V2,...) with a single (nested) /// value.
   /// @param tos token string modified in place
   static bool collect_value_groups(Token_string & tos);

   /// Create one scalar APL value from a token.
   /// @param tok token to convert into a scalar APL value
   static void create_scalar_value(Token & tok);

   /// Create one APL value from \b len values.
   /// @param tos token string modified in place
   /// @param pos starting index within \b tos
   /// @param len number of consecutive value tokens to combine
   static void create_value(Token_string & tos, int pos, int len);

   /// Create one vector APL value from \b value_count single values.
   /// @param tos token string modified in place
   /// @param pos starting index of the first element token
   /// @param len number of element tokens to combine into a vector
   static void create_vector_value(Token_string & tos, int pos, int len);

   /// fix the syntax of the POWER (⍣) operator.
   /// @param tos token string modified in place
   static bool fix_POWER_syntax(Token_string & tos);

   /// fix the syntax of the RANK (⍤) operator.
   /// @param tos token string modified in place
   static bool fix_RANK_syntax(Token_string & tos);

   /// return the number of INT or near-INT tokens, starting at \b pos (and
   /// at most 4). Replace near-int floats and complex /// literals with ints.
   /// @param tos token string being inspected
   /// @param pos starting index within \b tos
   static int j_length(Token_string & tos, int pos);

   /// replace subfunction names like ⎕XXX.subfun with a numeric
   /// axis like XXX[num]
   /// @param tos token string modified in place
   static bool map_function_groups(Token_string & tos);

   /// mark next symbol left of ← on the same level as LSYMB
   /// @param tos token string to scan and mark in place
   static void mark_lsymb(Token_string & tos);

   /** Replace:
       ∧∧, ∨∨, ⍲⍲, and ⍱⍱    with their bitwise variant,
       ⎕FIO.function_name    with ⎕FIO[X] (via subfun_to_axis(function_name))
    */
   /// @param tos token string modified in place
   static void optimize_static_patterns(Token_string & tos);

   /// print a detailed log message
   /// @param N pass number or stage index for the log label
   /// @param tos token string to dump
   static void parse_log(int N, const Token_string & tos);

   /// Parse token string \b tos (a statement without diamonds).
   /// @param tos token string for a single statement, modified in place
   /// @param optimize true to apply optimisation passes
   static ErrorCode parse_statement(Token_string & tos, bool optimize);

   /// Replace (X) with X and ((..) with (..) in \b tos (where X is a
   // single token).
   /// @param tos token string modified in place
   static bool remove_nongrouping_parantheses(Token_string & tos);

   /// where this parser was constructed
   const char * create_loc;

   /// tokenize macro code
   const bool macro;

   /// the parsing mode of this parser
   const ParseMode pmode;
};

#endif // __PARSER_HH_DEFINED__
