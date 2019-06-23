/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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

#ifndef __TOKEN_HH_DEFINED__
#define __TOKEN_HH_DEFINED__

#include <ostream>
#include <vector>

#include "Avec.hh"
#include "Common.hh"
#include "Error_macros.hh"
#include "Id.hh"
#include "TokenEnums.hh"
#include "Value.hh"

class Function;
class IndexExpr;
class Symbol;
class Value;
class Workspace;

/**
    A Token, consisting of a \b tag and a \b value. The \b tag (actually
    already (tag & TV_MASK) identifies the type of the \b value.
 */
/// One atom of an APL function or expression
class Token
{
public:
   /// Construct a VOID token. VOID token are used for two purposes: (1) to
   /// fill positions when e.g. 3 tokens were replaced by 2 tokens during
   /// parsing, and (2) as return values of user defined functions that
   /// do not return values.
   Token()
   : tag(TOK_VOID) { value.int_vals[0] = 0; }

   /// copy constructor
   Token(const Token & other);

   /// copy \b src into \b this token. leaving APL value pointer in
   /// src (if any) and add events as needed
   void copy_1(const Token & src, const char * loc);

   /// move mutable \b src into \b this token. clears APL value pointer in
   /// src (if any) and add events as needed
   void move_1(Token & src, const char * loc);

   /// move const \b src into \b this token. clears APL value pointer in
   /// src (if any) and add events as needed
   void move_2(const Token & src, const char * loc);

   /// Construct a token without a value
   Token(TokenTag tg)
   : tag(tg) { Assert(get_ValueType() == TV_NONE);   value.int_vals[0] = 0; }

   /// Construct a token for a \b Function.
   Token(TokenTag tg, Function * fun)
   : tag(tg) { Assert(get_ValueType() == TV_FUN);   value.function = fun; }

   /// Construct a token for a \b line number
   Token(TokenTag tg, Function_Line line)
   : tag(tg) { Assert(tg == TOK_LINE);   value.fun_line = line; }

   /// Construct a token for an \b error code
   Token(TokenTag tg, ErrorCode ec)
   : tag(tg) { Assert(tg == TOK_ERROR);   value.int_vals[0] = ec; }

   /// Construct a token for a \b Symbol
   Token(TokenTag tg, Symbol * sp)
   : tag(tg) { Assert(get_ValueType() == TV_SYM);  value.sym_ptr = sp; }

   /// Construct a token with tag tg for a UNICODE character. The tag is
   /// defined in Avec.def. This token in temporary in the sense that
   /// get_ValueType() can be anything rather than TV_CHAR. The value
   /// for the token (if any) will be added later (after parsing it).
   Token(TokenTag tg, Unicode uni)
   : tag(tg) { value.char_val = uni; }

   /// Construct a token for a single integer value.
   Token(TokenTag tg, int64_t ival)
   : tag(tg) { value.int_vals[0] = ival; }

   /// Construct a token for a single floating point value.
   Token(TokenTag tg, APL_Float flt)
   : tag(tg) { value.float_vals[0] = flt; }

   /// Construct a token for a single complex value.
   Token(TokenTag tg, APL_Float r, APL_Float i)
   : tag(tg)
     {
       value.float_vals[0] = r;
       value.float_vals[1] = i;
     }

   /// Construct a token for an APL value.
   Token(TokenTag tg, Value_P vp)
   : tag(tg)
   { Assert1(get_ValueType() == TV_VAL);
     Assert(!!vp);   new (&value.apl_val) Value_P(vp); }

   /// Construct a token for an index
   Token(TokenTag tg, IndexExpr & idx);

   /// destructor
   ~Token()
     { extract_apl_val("~Token()");  }

   /// swap this and \b other
   inline void Hswap(Token & other)
      { ::Hswap(tag, other.tag);
        ::Hswap(value.int_vals[0], other.value.int_vals[0]);
        ::Hswap(value.int_vals[1], other.value.int_vals[1]);
      }

   /// return the TokenValueType of this token.
   TokenValueType get_ValueType() const
      { return TokenValueType(tag & TV_MASK); }

   /// return the TokenClass of this token.
   TokenClass get_Class() const
      { return TokenClass(tag & TC_MASK); }

   /// return the Id of this token.
   Id get_Id() const
      { return Id(tag >> 16); }

   /// return the tag of this token
   const TokenTag get_tag() const   { return tag; }

   /// return the Unicode value of this token
   Unicode get_char_val() const
      { Assert(get_ValueType() == TV_CHAR);   return value.char_val; }

   /// return the integer value of this token
   int64_t get_int_val() const
      { Assert(get_ValueType() == TV_INT);   return value.int_vals[0]; }

   /// return the second integer value of this token
   int64_t get_int_val2() const
      { return value.int_vals[1]; }

   /// return the error code value of this token
   ErrorCode get_ErrorCode() const
      { Assert1(get_tag() == TOK_ERROR);
        Assert1(get_ValueType() == TV_INT);
        return ErrorCode(value.int_vals[0]); }

   /// set the integer value of this token
   void set_int_val(int64_t val)
      { Assert(get_ValueType() == TV_INT);   value.int_vals[0] = val; }

   /// set the second integer value of this token
   void set_int_val2(int64_t val)
      { value.int_vals[1] = val; }

   /// return the float value of this token
   APL_Float get_flt_val() const
      { Assert(get_ValueType() == TV_FLT);   return value.float_vals[0]; }

   /// return the complex real value of this token
   APL_Float get_cpx_real() const
      { Assert(get_ValueType() == TV_CPX);   return value.float_vals[0]; }

   /// return the complex imag value of this token
   APL_Float get_cpx_imag() const
      { Assert(get_ValueType() == TV_CPX);   return value.float_vals[1]; }

   /// return the Symbol * value of this token
   Symbol * get_sym_ptr() const
      { Assert(get_ValueType() == TV_SYM);   return value.sym_ptr; }

   /// return the Function_Line value of this token
   Function_Line get_fun_line() const
      { Assert(get_ValueType() == TV_LIN);   return value.fun_line; }

   /// return true iff \b this token has no value
   bool is_void() const
      { return (get_ValueType() == TV_NONE); }

   /// return true iff \b this token is an apl value
   bool is_apl_val() const
      { return (get_ValueType() == TV_VAL); }

   /// return the Value_P value of this token. The token could be TOK_NO_VALUE;
   /// in that case VALUE_ERROR is thrown.
   Value_P get_apl_val() const
      { if (is_apl_val())   return value._apl_val();   VALUE_ERROR; }

   /// return the address of the Value_P value of this token.
   Value_P * get_apl_valp() const
      { if (is_apl_val())   return &value._apl_val();   VALUE_ERROR; }

   /// clear this token, properly clearing Value token
   void clear(const char * loc)
      {
         if (is_apl_val())   value.apl_val.reset();
         new (this) Token();
      }

   /// return the axis specification of this token (expect non-zero axes)
   Value_P get_nonzero_axes() const
      { Assert1(!!value.apl_val && (get_tag() == TOK_AXES));
        return value._apl_val(); }

   /// return the axis specification of this token
   Value_P get_axes() const
      { Assert1(get_tag() == TOK_AXES);  return value._apl_val(); }

   /// set the Value_P value of this token
   void set_apl_val(Value_P val)
      { Assert(get_ValueType() == TV_VAL);   value._apl_val() = val; }

   /// return the IndexExpr value of this token
   IndexExpr & get_index_val() const
      { Assert(get_ValueType() == TV_INDEX);   return *value.index_val; }

   /// return true if \b this token is a function (or operator)
   bool is_function() const
      { return (get_ValueType() == TV_FUN); }

   /// return the Function * value of this token
   Function * get_function() const
      { if (!is_function())   SYNTAX_ERROR;   return value.function; }

   /// return value usage counter
   int value_use_count() const;

   /// clear the Value_P value (if any) of this token, updating
   /// its refcount as needed
   void extract_apl_val(const char * loc);

   /// clear the Value_P (if any) without updating its refcount. Return 
   /// the old Value * that was overridden
   Value * extract_and_keep(const char * loc);

   /// change the tag (within the same TokenValueType)
   void ChangeTag(TokenTag new_tag);

   /// helper function to print a function.
   ostream & print_function(ostream & out) const;

   /// helper function to print an APL value
   ostream & print_value(ostream & out) const;

   /// show trace output for this token
   void show_trace(ostream & out, const UCS_string & fun_name,
                   Function_Line line) const;

   /// the Quad_CR representation of the token.
   UCS_string canonical(PrintStyle style) const;

   /// the tag in readable form (TOK_...)
   UCS_string tag_name() const;

   /// print the token to \b out in the format used by print_error_info().
   /// return the number of characters printed.
   int error_info(UCS_string & out) const;

   /// copy src to \b this token, updating ref counts for APL values
   void copy_N(const Token & src);

   /// return a brief token class name for debugging purposes
   static const char * short_class_name(TokenClass cls);

   /// the optional value of the token.
   union sval
      {
        Unicode         char_val;        ///< the Unicode for CTV_CHARTV_
        APL_Integer     int_vals[2];     ///< the integer for TV_INT
        APL_Float_Base  float_vals[2];   ///< the doubles for TV_FLT and TV_CPX
        Symbol        * sym_ptr;         ///< the symbol for TV_SYM
        Function_Line   fun_line;        ///< the function line for TV_LIN
        IndexExpr     * index_val;       ///< the index for TV_INDEX
        Function      * function;        ///< the function for TV_FUN
        Value_P_Base    apl_val;         ///< the APL value for TV_VAL

        /// a shortcut for accessing apl_val
        Value_P & _apl_val() const
           { return reinterpret_cast<Value_P &>
                    (const_cast<Value_P_Base &>(apl_val)); }
      };

   /// the name of \b tc
   static const char * class_name(TokenClass tc);

protected:
   /// The tag indicating the type of \b this token
   TokenTag tag;

   /// The value of \b this token
   sval value;

   /// helper function to print Quad-function (system function or variable).
   ostream & print_quad(ostream & out) const;
};
//-----------------------------------------------------------------------------
/// A sequence of Token
class Token_string : public  std::vector<Token>
{
public:
   /// construct an empty string
   Token_string()   {}

   /// construct a string of \b len Token, starting at \b data.
   Token_string(const Token * data, ShapeItem len)
      { loop(l, len)   push_back(data[l]); }

   /// construct a string of \b len Token from another token string
   Token_string(const Token_string & other, uint32_t pos, uint32_t len)
      { loop(l, len)   push_back(other[pos++]); }

   /// reversde the token order from \b from to \b to (including)
   void reverse_from_to(ShapeItem from, ShapeItem to);

   /// print this token string
   void print(ostream & out, bool details) const;

private:
   /// prevent accidental copying
   Token_string & operator =(const Token_string & other);
};
//-----------------------------------------------------------------------------
/** a token with its location information. For token copied from a function
    body: low = high = PC. For token from a reduction low is the low location
    of the first token and high is the high of the last token of the token
    range that led to e.g a result token.
    A Token and its position (in a Token_string)
 */
/// A Token and its location information (position in a Token_string)
struct Token_loc
{
   /// constructor: invalid Token_loc
   Token_loc()
   : pc(Function_PC_invalid)
   {}

   /// constructor: invalid Token with valid loc
   Token_loc(Function_PC _pc)
   : pc(_pc)
   {}

   /// constructor: valid Token with valid loc
   Token_loc(const Token & t, Function_PC _pc)
   : tok(t),
     pc(_pc)
   {}

   /// copy this Token_loc to \b other
   void copy(const Token_loc & other, const char * loc)
      {
        pc = other.pc;
        tok.copy_1(other.tok, loc);
      }

   /// the token
   Token tok;

   /// the PC of the leftmost (highest PC) token
   Function_PC pc;
};
//-----------------------------------------------------------------------------

#endif // __TOKEN_HH_DEFINED__
