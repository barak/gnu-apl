
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

#ifndef __Quad_CR_HH_DEFINED__
#define __Quad_CR_HH_DEFINED__

#include "QuadFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/**
   The system function ⎕CR (Character Representation).
 */
/// The class implementing ⎕CR
class Quad_CR : public QuadFunction
{
public:
   /// Constructor
   Quad_CR();

   /// overloaded Function::eval_B()
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_XB().
   /// ⎕CR[X] B  ←→  X ⎕CR B
   /// @param X axis (format selector) APL value
   /// @param B right argument APL value
   virtual Token eval_XB(Value_P X, Value_P B) const
      { return eval_AB(X, B); }

   /// compute \b a ⎕CR \b B
   /// @param a integer selector choosing the character-representation format
   /// @param B right argument APL value
   /// @param pctx print context controlling formatting options
   static Value_P do_CR(APL_Integer a, const Value * B, PrintContext pctx);

   /// compute a good default type and value for the top-level ⍴ of 10 ⎕CR.
   /// Return true for INT and false for CHAR.
   /// @param value APL value whose default fill element is to be determined
   /// @param default_char receives the default character fill element
   /// @param default_int receives the default integer fill element
   static bool figure_default(const Value * value, Unicode & default_char,
                              APL_Integer & default_int);

   static Quad_CR  fun;          ///< Built-in function.

   /// compute \b 35 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR35(const Value * B);

   /// portable variable encoding of value \b name (varname or varname ⊂)
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param var_name name of the APL variable being encoded
   /// @param value APL value to encode
   static void do_CR10_variable(UCS_string_vector & result,
                                const UCS_string & var_name,
                                const Value * value);

protected:
   /// a mapping between function names and function numbers
   static const FunctionGroup::function_info subfunction_infos[];

   /// overloaded FunctionGroup::get_legend
   /// @param lt the legend type requested (short name, description, etc.)
   virtual const char * get_legend(FunctionGroup::Legend_type lt) const;


   /// overloaded FunctionGroup::print_fun_syntax()
   /// @param out output stream to write to
   /// @param info descriptor of the subfunction whose syntax is to be printed
   virtual void print_fun_syntax(ostream & out,
                                 const function_info & info) const;

   /// overloaded FunctionGroup::print_map_syntax()
   /// @param out output stream to write to
   /// @param info descriptor of the subfunction whose map syntax is to be printed
      virtual void print_map_syntax(ostream & out,
                                 const function_info & info) const;

   /// do eval_B() with extra spaces removed
   /// @param B right argument APL value
   /// @param remove_extra_spaces true to collapse runs of spaces in the output
   static Token do_eval_B(const Value * B, bool remove_extra_spaces);

   /// compute \b 5 ⎕CR \b B or \b 6 ⎕CR \b B
   /// @param A56 selector: 5 or 6
   /// @param B right argument APL value
   static Value_P do_CR5_6(int A56, const Value * B);

   /// compute \b 10 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR10(const Value * B);

   /// 10 ⎕CR symbol_name (variable or function name). Also used for )OUT
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param symbol_name APL character value containing the symbol name
   static void do_CR10(UCS_string_vector & result, const Value * symbol_name);

   /// compute 10 ⎕CR recursively (structured variable)
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param varname name of the variable being encoded
   /// @param value APL value to encode recursively
   static const char * do_CR10_structured(UCS_string_vector & result,
                                          const UCS_string & varname,
                                          const Value * value);

   /// emit one level of \b value
   /// @param result vector of UCS strings that receives the emitted lines
   /// @param level current nesting depth
   /// @param value APL value to emit at this level
   static void do_CR10_level(UCS_string_vector & result, size_t level,
                             const Value & value);

   /// emit a simple Cell
   /// @param cell scalar cell to format as a UCS string
   static UCS_string do_CR10_simple_cell(const Cell & cell);

   /// emit a shaoe
   /// @param _shape APL shape to format as a UCS string
   static UCS_string do_CR10_shape(const Shape &_shape);

   /// compute \b 11 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR11(const Value * B);

   /// compute \b 12 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR12(const Value * B);

   /// compute \b 13 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR13(const Value * B);

   /// compute \b 14 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR14(const Value * B);

   /// compute \b 15 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR15(const Value * B);

   /// compute \b 16 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR16(const Value * B);

   /// compute \b 17 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR17(const Value * B);

   /// compute \b 18 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR18(const Value * B);

   /// compute \b 19 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR19(const Value * B);

   /// compute \b 26 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR26(const Value * B);

   /// compute \b 27 ⎕CR \b B or \b 28 ⎕CR \b B
   /// @param A_27_28 selector: 27 or 28
   /// @param B right argument APL value
   static Value_P do_CR27_28(int A_27_28, const Value * B);

   /// compute \b 30 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR30(const Value * B);

   /// compute \b 31 ⎕CR \b B or \b 32 ⎕CR \b B
   /// @param A_31_32 selector: 31 or 32
   /// @param B right argument APL value
   static Value_P do_CR31_32(int A_31_32, const Value * B);

   /// compute \b 33 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR33(const Value * B);

   /// compute \b 34 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR34(const Value * B);

   /// compute \b 36 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR36(const Value * B);

   /// compute \b 37 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR37(const Value * B)
      { return do_eval_B(B, false).get_apl_val(); }

   /// compute \b 38 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR38(const Value * B);

   /// compute \b 39 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR39(const Value * B);

   /// compute \b 40 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR40(const Value * B);

   /// compute \b 41 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR41(const Value * B);

   /// compute \b 42 ⎕CR \b B (tokenize) or 43 ⎕CR \b B (parse); return tags
   /// @param B right argument APL value
   /// @param parse true to parse tokens (43 ⎕CR), false to only tokenize (42 ⎕CR)
   static Value_P do_CR42_43(const Value * B, bool parse);

   /// compute \b 44 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR44(const Value * B);

   /// decode token or token tag \b cB into \b result
   /// @param result UCS string that receives the decoded representation
   /// @param cB cell containing the token or token tag to decode
   static void decode_CR44(UCS_string & result, const Cell & cB);

   /// append \b value to result
   /// @param result UCS string to append to
   /// @param value APL value whose representation is appended
   static void value_CR44(UCS_string & result, const Value & value);

   /// print value addresses in B
   /// @param B right argument APL value
   static void do_CR45(const Value * B);

   /// @param prefix indentation prefix string for this nesting level
   /// @param B APL value whose internal addresses are to be printed
   static void do_CR45_value(const UCS_string prefix, const Value * B);

   /// the state of the current output line
   enum V_mode
      {
        Vm_NONE,   ///< initial state
        Vm_NUM,    ///< in numeric vector.          e.g. 1 2 3
        Vm_QUOT,   ///< in quoted character vector, e.g. 'abc'
        Vm_UCS     ///< in ⎕UCS(),                  e.g. ⎕UCS(97 98 99)
      };

   /// return true if Value is a plain string that can be double-quoted
   /// without problems.
   /// @param value APL value to inspect
   static bool is_plain_string(const Value * value);

   /// decide if 'xxx' or ⎕UCS(xxx) shall be used
   /// @param mode current output mode (quoted, UCS, or numeric)
   /// @param value APL value being encoded
   /// @param pos index of the current item within the value ravel
   static bool use_quote(V_mode mode, const Value * value, ShapeItem pos);

   /// close current mode (ending ' for '' or ) for ⎕UCS())
   /// @param line UCS string being built; closing delimiter is appended to it
   /// @param mode current output mode to close
   static void close_mode(UCS_string & line, V_mode mode);

   /// print item separator
   /// @param line UCS string being built; separator is appended to it
   /// @param from_mode current output mode before the item separator
   /// @param to_mode output mode for the next item
   static void item_separator(UCS_string & line,
                              V_mode from_mode, V_mode to_mode);

   /// return an (almost) unique variable name
   static UCS_string temp_varname();
};
//════════════════════════════════════════════════════════════════════════════

#endif //__Quad_CR_HH_DEFINED__

