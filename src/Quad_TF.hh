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

#ifndef __QUAD_TF_HH_DEFINED__
#define __QUAD_TF_HH_DEFINED__

#include "QuadFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/** The system function Quad-TF (Transfer Form).  */
/// The class implementing ⎕TF
class Quad_TF : public QuadFunction
{
public:
   /// Constructor.
   Quad_TF() : QuadFunction(TOK_Quad_TF) {}

   /// Overloaded Function::eval_AB().
   /// @param A left argument APL value (transfer format number)
   /// @param B right argument APL value (symbol name or data)
   virtual Token eval_AB(cValue_R A, cValue_R B) const;

   /// return true if val contains an 1⎕TF or 2⎕TF record
   /// @param maybe_name string to test for transfer-format content
   static bool is_inverse(const UCS_string & maybe_name);

   /// undo ⎕UCS() created by tf2_char_vec
   /// @param ucs encoded string possibly containing ⎕UCS notation
   static UCS_string no_UCS(const UCS_string & ucs);

   /// return B in transfer format 1 (old APL format)
   /// @param symbol_name name of the symbol to transfer
   static Value_P tf1(const UCS_string & symbol_name);

   /// return B in transfer format 2 (new APL2 format)
   /// @param symbol_name name of the symbol to transfer
   static Token tf2(const UCS_string & symbol_name);

   /// store simple character vector \b vec in \b ucs, either as
   /// 'xxx' or as (UCS nn nnn ...)
   /// @param ucs output string being built
   /// @param vec character vector to encode
   /// @param fun_line true if \b vec is a line of a defined function
   static void tf2_char_vec(UCS_string & ucs, const UCS_string & vec,
                            bool fun_line);

   /// return a mask that is true for the characters of function line
   /// \b line that are in a string ('...', "...", or «...») or comment
   /// @param line a line of a defined function
   static std::vector<bool> text_mask(const UCS_string & line);

   /// return the lines in \b B (the argument of ⎕FX in an ATF record) with
   /// the ASCII * ^ ~ in APL code (i.e. not in strings or comments)
   /// replaced by the principal APL characters ⋆ ∧ ∼
   /// @param B nested vector of lines or character matrix
   static Value_P ATF_principal_chars(const cValue & B);

   /// store B in transfer format 2 (new APL format) into \b ucs
   /// @param ucs output string being built
   /// @param fun_name name of the function
   /// @param fun the function object to serialize
   static void tf2_fun_ucs(UCS_string & ucs, const UCS_string & fun_name,
                           const Function & fun);

   /// try inverse ⎕TF2 of ucs, set \b new_var_or_fun if successful
   /// @param ravel transfer-format-2 encoded string to decode
   /// @param from_ATF true if \b ravel comes from an ATF file ()IN)
   static UCS_string tf2_inverse(const UCS_string & ravel,
                                 bool from_ATF = false);

   /// append ravel of \b value in tf2_format to \b ucs.
   /// Return true iff the value should be enclosed in parentheses when grouped.
   /// @param level current nesting depth for formatting
   /// @param ucs output string being built
   /// @param value APL value whose ravel to format
   /// @param nesting current nesting level in the output
   static bool tf2_value(int level, UCS_string & ucs,
                                    const cValue & value, ShapeItem nesting);

   /// return B in transfer format 2 (new APL format) for a variable
   /// @param var_name name of the variable
   /// @param val APL value to encode
   static Token tf2_var(const UCS_string & var_name, const cValue & val);

   /// return B in transfer format 3 (APL2 CDR format)
   /// @param symbol_name name of the symbol to transfer
   static Value_P tf3(const UCS_string & symbol_name);

   static Quad_TF  fun;          ///< Built-in function.

protected:
   /// append \b shape in tf2_format to \b ucs.
   /// @param ucs output string being built
   /// @param shape APL array shape to encode
   /// @param nesting current nesting level in the output
   /// @param omit_reshape true to skip the "N⍴" prefix (rank-1, simple,
   ///        unenclosed values: their bare literal/strand ravel already
   ///        has the right shape on its own, so an explicit reshape is
   ///        redundant, per LanguageVariances.md #34b)
   /// @param self_delimiting true if the bare ravel's own syntax already
   ///        marks its boundaries without external grouping (a quoted
   ///        character-vector literal); false for e.g. a bare numeric
   ///        vector, which would silently merge with an adjacent strand
   ///        sibling if not parenthesized. Only consulted when
   ///        omit_reshape is true and nesting is 0.
   /// @return true if an opening '(' was emitted (nesting > 0, an N⍴
   ///        prefix was written, or the bare ravel is not
   ///        self-delimiting) -- the caller must then emit the matching
   ///        ')' itself; false if no parentheses were needed at all (a
   ///        bare, unenclosed, non-reshaped, self-delimiting literal),
   ///        in which case none must be emitted, since transfer-file
   ///        records may not contain redundant parentheses (LRM
   ///        Appendix B).
   static bool tf2_shape(UCS_string & ucs, const Shape & shape,
                         ShapeItem nesting, bool omit_reshape = false,
                         bool self_delimiting = false);

   /// append ravel \b cells in tf2_format to \b ucs.
   /// @param level current nesting depth for formatting
   /// @param ucs output string being built
   /// @param len number of cells to encode
   /// @param cells pointer to the first cell of the ravel
   static void tf2_ravel(int level, UCS_string & ucs, const ShapeItem len,
                           cValue_R V, ShapeItem idx);

   /// append the ravel of a simple character array (of any rank)
   /// @param level current nesting depth for formatting
   /// @param ucs output string being built
   /// @param value APL value whose character ravel to encode
   static void tf2_all_char_ravel(int level, UCS_string & ucs,
                                  const cValue & value);

   /// return B in transfer format 1 (old APL format) for variable B
   /// @param var_name name of the variable
   /// @param B APL value to encode
   static Value_P tf1(const UCS_string & var_name, Value_P B);

   /// return B in transfer format 1 (old APL format) for function B
   /// @param fun_name name of the function
   /// @param B the function object to serialize
   static Value_P tf1(const UCS_string & fun_name, const Function & B);

   /// return inverse transfer format 1 (old APL format) for a variable
   /// @param ravel transfer-format-1 encoded string to decode
   static Value_P tf1_inv(const UCS_string &  ravel);

   /// simplify tos by removing UCS nnn etc.
   /// @param tos token string to reduce in place
   static void tf2_reduce(Token_string & tos);

   /// replace pattern ⊂ B  in \b tos with the single token (⊂B);
   /// return true iff done so.
   /// @param tos token string to reduce in place
   static bool tf2_reduce_ENCLOSE(Token_string & tos);

   /// replace pattern A ⍴ B in \b tos with the single token (A⍴B);
   /// return true iff done so.
   /// @param tos token string to reduce in place
   static bool tf2_reduce_RHO(Token_string & tos);

   /// replace pattern 38 ⎕CR B in \b tos with the single token
   /// Quad_CR::do_CR38(B) (plain array -> structured value), as emitted
   /// by tf2_var()/tf2_ravel() for a structured value (or sub-value);
   /// return true iff done so.
   /// @param tos token string to reduce in place
   static bool tf2_reduce_CR38(Token_string & tos);

   /// replace pattern N - ⎕IO - ⍳ K  in \b tos with N N+1 ... N+K-1;
   /// return true iff done so.
   /// @param tos token string to reduce in place
   static bool tf2_reduce_sequence(Token_string & tos);

   /// replace pattern N - M × ⎕IO - ⍳ K  in \b tos with M × (N N+1 ... N+K-1);
   /// return true iff done so.
   static bool tf2_reduce_sequence1(Token_string & tos);

   /// replace pattern ( B ) in tos with B; return true iff done so.
   static bool tf2_reduce_parentheses(Token_string & tos);

   /// replace pattern value1 value2... in tos with the single token
   //  (value1 value2...); return true iff done so.
   static bool tf2_glue(Token_string & tos);

   /// replace pattern , B in tos with single token (,B);
   /// return true iff done so.
   static bool tf2_reduce_COMMA(Token_string & tos);

   /// return B in transfer format 2 (new APL format) for a function
   static UCS_string tf2_fun(const UCS_string & fun_name, const Function & fun);

   /// replace pattern ⊂ ⊂ B in tos with the single token (⊂⊂B)
   static bool tf2_reduce_ENCLOSE_ENCLOSE(Token_string & tos);

   /// replace pattern ⊂ B in tos with the single token (⊂B)
   static bool tf2_reduce_ENCLOSE1(Token_string & tos);

   /// replace ⎕UCS n... with the corresponding Unicodes,
   static void tf2_reduce_UCS(Token_string & tos);

   /// in-place exchange of IntCells and CharCells
   static ShapeItem tf2_toggle_UCS(Value & val);
};
//════════════════════════════════════════════════════════════════════════════
#endif // __QUAD_TF_HH_DEFINED__
