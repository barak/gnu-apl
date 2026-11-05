
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
#include "UCS_string_vector.hh"

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
   virtual Token eval_B(cValue_R B) const;

   /// overloaded Function::eval_AB()
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// overloaded Function::eval_XB().
   /// ⎕CR[X] B  ←→  X ⎕CR B
   /// @param X axis (format selector) APL value
   /// @param B right argument APL value
   virtual Token eval_XB(cValue_R X, cValue_R B) const
      { return eval_AB(X, B); }

   /// overloaded Function::eval_AXB()
   /// @param A left argument APL value
   /// @param X axis argument APL value
   /// @param B right argument APL value
   virtual Token eval_AXB(cValue_R A, cValue_R X, cValue_R B) const;

   /// compute \b a ⎕CR \b B
   /// @param a integer selector choosing the character-representation format
   /// @param B right argument APL value
   /// @param pctx print context controlling formatting options
   static Value_P do_CR(APL_Integer a, cValue_R B, PrintContext pctx);

   /// the frame styles of the boxing variants of ⎕CR (indices into
   /// frame_styles())
   enum FrameStyle
      {
        FS_APL           = 0,    ///< 0 ⎕CR: no frames
        FS_APL_FUN       = 1,    ///< 1 ⎕CR: no frames
        FS_BOXED_CHAR    = 2,    ///< 2 ⎕CR: ASCII frames
        FS_BOXED_GRAPHIC = 3,    ///< 3, 4 ⎕CR: thick frames
        FS_BOXED_THIN    = 4,    ///< 7, 8 ⎕CR: thin frames
        FS_BOXED_DOUBLE  = 5,    ///< 9 ⎕CR: thick frames in a double frame
        FS_BOXED_QUOTED  = 6,    ///< 29 ⎕CR: thick frames, quoted strings
        FS_PLAIN_THIN    = 7,    ///< 46 ⎕CR: thin frames, no decorators
        FS_PLAIN_THICK   = 8,    ///< 47 ⎕CR: thick frames, no decorators
        FS_NARS          = 9,    ///< 20, 23 ⎕CR: thick frames, axis lengths
        FS_NARS_THIN     = 10,   ///< 21, 24 ⎕CR: thin frames, axis lengths
        FS_NARS_DOUBLE   = 11,   ///< 22, 25 ⎕CR: like 20 in a double frame
        FS_COUNT         = 12    ///< the number of frame styles
      };

   /// return the frame characters (see PrintBuffer::Frame_char) of all
   /// frame styles, indexed by FrameStyle
   static const UCS_string_vector & frame_styles();

   /// compute a good default type and value for the top-level ⍴ of 10 ⎕CR.
   /// Return true for INT and false for CHAR.
   /// @param value APL value whose default fill element is to be determined
   /// @param default_char receives the default character fill element
   /// @param default_int receives the default integer fill element
   static bool figure_default(const cValue * value, Unicode & default_char,
                              APL_Integer & default_int);

   static Quad_CR  fun;          ///< Built-in function.

   /// compute \b 35 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR35(cValue_R B);

   /// throw a DOMAIN ERROR if \b Z already has a member \b name
   /// @param Z the structured value being built
   /// @param name the member name
   static void check_duplicate_member(Value & Z, const UCS_string & name);

   /// return true if the structured \b value is not empty and all its
   /// member names (recursively) are APL identifiers
   /// @param value the structured value
   static bool members_are_names(const cValue & value);

   /// portable variable encoding of value \b name (varname or varname ⊂)
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param var_name name of the APL variable being encoded
   /// @param value APL value to encode
   static void do_CR10_variable(UCS_string_vector & result,
                                const UCS_string & var_name,
                                const cValue * value);

   /// compute \b 38 ⎕CR \b B (plain array -> structured value); public
   /// so that Quad_TF::tf2_inverse() can reconstruct a structured
   /// variable's "38⎕CR(...)" transfer form (see Quad_TF.cc).
   /// @param B right argument APL value
   static Value_P do_CR38(cValue_R B);

protected:
   /// return \b true if the frame defined by fun has an extra (outer) frame.
   /// \b true for fun ∈ 4 8 23 24 25 29 46 47.
   /// @param fun the frame sub-function
   static bool extra_frame(int fun)
      {
        enum { HAS_EXTRA = 1ULL <<  4 | 1ULL << 8  |
                           1ULL << 23 | 1ULL << 24 | 1ULL << 25 |
                           1ULL << 29 | 1ULL << 46 |
                           1ULL << 47 };
         return (1ULL << fun & HAS_EXTRA) != 0;
      }

   /// draw B in one of the boxed styles, possibly with an outer frame
   /// @param B the value to be printed
   /// @param pctx line styles etc.
   /// @param extra draw an extra frame around B
   static Value_P draw_B_boxed(cValue_R B, PrintContext pctx, bool extra,
                                const UCS_string & frame_chars);

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
   static Token do_eval_B(cValue_R B, bool remove_extra_spaces);

   /// compute \b 5 ⎕CR \b B or \b 6 ⎕CR \b B
   /// @param A56 selector: 5 or 6
   /// @param B right argument APL value
   static Value_P do_CR5_6(int A56, cValue_R B);

   /// compute \b 10 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR10(cValue_R B);

   /// 10 ⎕CR symbol_name (variable or function name). Also used for )OUT
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param symbol_name APL character value containing the symbol name
   static void do_CR10(UCS_string_vector & result, const cValue * symbol_name);

   /// compute 10 ⎕CR recursively (structured variable)
   /// @param result vector of UCS strings that receives the encoded lines
   /// @param varname name of the variable being encoded
   /// @param value APL value to encode recursively
   static const char * do_CR10_structured(UCS_string_vector & result,
                                          const UCS_string & varname,
                                          const cValue * value);

   /// emit one level of \b value
   /// @param result vector of UCS strings that receives the emitted lines
   /// @param level current nesting depth
   /// @param value APL value to emit at this level
   static void do_CR10_level(UCS_string_vector & result, size_t level,
                             const cValue & value);

   /// emit a simple Cell
   /// @param cell scalar cell to format as a UCS string
   static UCS_string do_CR10_simple_cell(const Cell & cell);

   /// format a Float value with full (round-trip) precision, i.e. as
   /// needed to reconstruct the exact value via 10 ⎕CR. Unlike
   /// UCS_string::operator <<(double), which trims values that are
   /// close to an integer to 2 significant digits for compact display,
   /// this function never loses precision.
   /// @param num the value to format
   static UCS_string do_CR10_double(double num);

   /// emit a shaoe
   /// @param _shape APL shape to format as a UCS string
   static UCS_string do_CR10_shape(const Shape &_shape);

   /// compute \b 11 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR11(cValue_R B);

   /// compute \b 12 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR12(cValue_R B);

   /// compute \b 13 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR13(cValue_R B);

   /// compute \b 14 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR14(cValue_R B);

   /// compute \b 15 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR15(cValue_R B);

   /// compute \b 16 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR16(cValue_R B);

   /// compute \b 17 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR17(cValue_R B);

   /// compute \b 18 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR18(cValue_R B);

   /// compute \b 19 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR19(cValue_R B);

   /// compute \b 26 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR26(cValue_R B);

   /// compute \b 27 ⎕CR \b B or \b 28 ⎕CR \b B
   /// @param A_27_28 selector: 27 or 28
   /// @param B right argument APL value
   static Value_P do_CR27_28(int A_27_28, cValue_R B);

   /// compute \b 30 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR30(cValue_R B);

   /// compute \b 31 ⎕CR \b B or \b 32 ⎕CR \b B
   /// @param A_31_32 selector: 31 or 32
   /// @param B right argument APL value
   static Value_P do_CR31_32(int A_31_32, cValue_R B);

   /// compute \b 33 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR33(cValue_R B);

   /// compute \b 34 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR34(cValue_R B);

   /// compute \b 36 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR36(cValue_R B);

   /// compute \b 37 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR37(cValue_R B)
      { return do_eval_B(B, false).get_apl_val(); }

   /// compute \b 39 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR39(cValue_R B);

   /// compute \b 40 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR40(cValue_R B);

   /// compute \b 41 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR41(cValue_R B);

   /// compute \b 42 ⎕CR \b B (tokenize) or 43 ⎕CR \b B (parse); return tags
   /// @param B right argument APL value
   /// @param parse true to parse tokens (43 ⎕CR), false to only tokenize (42 ⎕CR)
   static Value_P do_CR42_43(cValue_R B, bool parse);

   /// compute \b 44 ⎕CR \b B
   /// @param B right argument APL value
   static Value_P do_CR44(cValue_R B);

   /// decode token or token tag \b cB into \b result
   /// @param result UCS string that receives the decoded representation
   /// @param cB cell containing the token or token tag to decode
   static void decode_CR44(UCS_string & result, const Cell & cB);

   /// append \b value to result
   /// @param result UCS string to append to
   /// @param value APL value whose representation is appended
   static void value_CR44(UCS_string & result, cValue_R value);

   /// print value addresses in B
   /// @param B right argument APL value
   static void do_CR45(cValue_R B);

   /// return the ravel packing type of B as an integer scalar (RPT_XXX)
   /// @param B right argument APL value
   static Value_P do_CR48(cValue_R B);

   /// return the configured packing threshold (PACKED_MINIMUM_LENGHT) as scalar
   /// @param B right argument APL value (ignored)
   static Value_P do_CR49(cValue_R B);

   /// compute \b 50 ⎕CR \b B or \b 51 ⎕CR \b B: elementwise integer
   /// B[IDX] → (nested, unless B is scalar) hex string Z[IDX]; no leading
   /// zeros for B[IDX] ≥ 0, minimal-byte two's complement for B[IDX] < 0
   /// @param A5051 selector: 50 (uppercase) or 51 (lowercase)
   /// @param B right argument APL value (integers, may be nested)
   static Value_P do_CR50_51(int A5051, cValue_R B);

   /// hex string (per do_CR50_51()'s rules) for one integer-valued Cell
   /// @param A5051 selector: 50 (uppercase) or 51 (lowercase)
   /// @param cB the cell to convert (must be an integer or near-integer)
   /// @param idx_txt "[N]" for error messages, or "" if B itself is cB
   static Value_P hex_of_int_cell(int A5051, const Cell & cB,
                                  const char * idx_txt);

   /// compute \b 52 ⎕CR \b B: the CRC32 of byte vector B (elements
   /// validated via cValue::get_byte_value(), i.e. integers in
   /// [-128, 255], same convention as do_CR33()/do_CR34()), returned
   /// as an integer scalar
   /// @param B right argument APL value (a byte vector)
   static Value_P do_CR52(cValue_R B);

   /// compute \b 53 ⎕CR \b B: like do_CR52(), but returned as an
   /// 8-character vector of uppercase hex digits
   /// @param B right argument APL value (a byte vector)
   static Value_P do_CR53(cValue_R B);

   /// compute the CRC32 of byte vector B (shared by do_CR52()/do_CR53())
   /// @param B right argument APL value (a byte vector)
   static uint32_t crc32_of(cValue_R B);

   /// @param prefix indentation prefix string for this nesting level
   /// @param B APL value whose internal addresses are to be printed
   static void do_CR45_value(const UCS_string prefix, cValue_R B);

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
   static bool is_plain_string(const cValue * value);

   /// decide if 'xxx' or ⎕UCS(xxx) shall be used
   /// @param mode current output mode (quoted, UCS, or numeric)
   /// @param value APL value being encoded
   /// @param pos index of the current item within the value ravel
   static bool use_quote(V_mode mode, const cValue * value, ShapeItem pos);

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

