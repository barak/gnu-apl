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

#ifndef __Quad_FUNCTION_HH_DEFINED__
#define __Quad_FUNCTION_HH_DEFINED__

#include "PrimitiveFunction.hh"

//────────────────────────────────────────────────────────────────────────────
/** The various Quad functions.  */
/// Base class for all system functions
class QuadFunction : public PrimitiveFunction
{
public:
   /// Constructor.
   /// @param tag token tag identifying this quad function
   QuadFunction(TokenTag tag) : PrimitiveFunction(tag) {}

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value
   /// @param B right argument APL value
   virtual Token eval_AB(Value_P A, Value_P B) const
      { VALENCE_ERROR; }

   /// overloaded Function::eval_B().
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const
      { VALENCE_ERROR; }

   /// overloaded Function::has_alpha()
   virtual bool has_alpha() const   { return true; }

   /// overloaded Function::has_result()
   virtual bool has_result() const   { return true; }
};
//────────────────────────────────────────────────────────────────────────────
/** The system function ⎕AF (Atomic Function) */
/// The class implementing ⎕AF
class Quad_AF : public QuadFunction
{
public:
   /// Constructor.
   Quad_AF() : QuadFunction(TOK_Quad_AF) {}

   /// overloaded Function::eval_B().
   /// @param B right argument APL value
   virtual Token eval_B(Value_P B) const;

   static Quad_AF  fun;          ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/** The system function ⎕AT (Attributes) */
/// The class implementing ⎕AT
class Quad_AT : public QuadFunction
{
public:
   /// Constructor.
   Quad_AT() : QuadFunction(TOK_Quad_AT) {}

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (attribute selector)
   /// @param B right argument APL value (name(s) to query)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   static Quad_AT  fun;          ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/** The system function ⎕DL (Delay).  */
/// The class implementing ⎕DL
class Quad_DL : public QuadFunction
{
public:
   /// Constructor.
   Quad_DL() : QuadFunction(TOK_Quad_DL) {}

   static Quad_DL  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value (delay in seconds)
   virtual Token eval_B(Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕EA (Execute Alternate)
 */
/// The class implementing ⎕EA
class Quad_EA : public QuadFunction
{
public:
   /// Constructor.
   Quad_EA() : QuadFunction(TOK_Quad_EA) {}

   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }

   static Quad_EA  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (alternate expression string)
   /// @param B right argument APL value (primary expression string)
   virtual Token eval_AB(Value_P A, Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕EB (Execute Both)
 */
/// The class implementing ⎕EB
class Quad_EB : public QuadFunction
{
public:
   /// Constructor.
   Quad_EB() : QuadFunction(TOK_Quad_EB) {}

   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }

   static Quad_EB  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (expression string)
   /// @param B right argument APL value (expression string)
   virtual Token eval_AB(Value_P A, Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕EC (Execute Controlled)
 */
/// The class implementing ⎕EC
class Quad_EC : public QuadFunction
{
public:
   /// Constructor.
   Quad_EC() : QuadFunction(TOK_Quad_EC) {}

   /// overladed Function::may_push_SI()
   virtual bool may_push_SI() const   { return true; }

   /// end of context handler for ⎕EC
   /// @param token result token produced by the controlled expression
   static void eoc(Token & token);

   static Quad_EC  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value (expression string)
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_fill_B().
   /// @param B right argument APL value (expression string)
   virtual Token eval_fill_B(Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕ENV (ENvironment Variables)
 */
/// The class implementing ⎕ENV
class Quad_ENV : public QuadFunction
{
public:
   /// Constructor.
   Quad_ENV() : QuadFunction(TOK_Quad_ENV) {}

   static Quad_ENV  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value (variable name pattern)
   virtual Token eval_B(Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕ES (Event Simulate).
 */
/// The class implementing ⎕ES
class Quad_ES : public QuadFunction
{
public:
   /// Constructor.
   Quad_ES() : QuadFunction(TOK_Quad_ES) {}

   static Quad_ES  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (event message string)
   /// @param B right argument APL value (event code)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B().
   /// @param B right argument APL value (event code)
   virtual Token eval_B(Value_P B) const;

   /// common inplementation for eval_AB() and eval_B()
   /// @param A optional event message string (null means no message)
   /// @param B right argument APL value (event code)
   /// @param error error object to populate with the simulated event
   static Token event_simulate(const UCS_string * A, Value_P B, Error & error);

   /// compute error code for B
   /// @param B right argument APL value encoding the error code
   static ErrorCode get_error_code(Value_P B);
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕EX (Expunge).
 */
/// The class implementing ⎕EX
class Quad_EX : public QuadFunction
{
public:
   /// Constructor.
   Quad_EX() : QuadFunction(TOK_Quad_EX) {}

   /// disassociate name from value, return 0 on failure or 1 on success.
   /// @param name Unicode name of the symbol to expunge
   static int expunge(const UCS_string & name);

   static Quad_EX  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_B().
   /// @param B right argument APL value (name(s) to expunge)
   virtual Token eval_B(Value_P B) const;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕INP (input from script, aka. HERE document)
 */
/// The class implementing ⎕INP
class Quad_INP : public QuadFunction
{
public:
   /// constructor.
   Quad_INP()
   : QuadFunction(TOK_Quad_INP)
     {
       Quad_INP_running = false;
     }

   static Quad_INP  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (escape delimiters)
   /// @param B right argument APL value (end-marker string)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_B().
   /// @param B right argument APL value (end-marker string)
   virtual Token eval_B(Value_P B) const;

   /// overloaded Function::eval_XB().
   /// @param X axis specifier APL value
   /// @param B right argument APL value (end-marker string)
   virtual Token eval_XB(Value_P X, Value_P B) const;

   /// extract the esc1 and esc2 strings from \b A
   /// @param A left argument APL value carrying the escape delimiters
   /// @param esc1 output: opening escape delimiter string
   /// @param esc2 output: closing escape delimiter string
   static void get_esc(Value_P A, UCS_string & esc1, UCS_string & esc2);

   /// read \b raw_lines from stdin or file, stop at end_marker
   static void read_strings();

   /// split \b raw_lines into \b prefixes, \b escapes, and \b suffixes
   static void split_strings();

   /// bool to prevent recursive ⎕INP calls
   static bool Quad_INP_running;

   /// the end merker for the entire ⎕INP input
   static UCS_string end_marker;

   /// the end merker for APL escapes (dyadic ⎕INP only)
   /// the start merker for APL escapes (dyadic ⎕INP only)
   static UCS_string esc1;

   /// the end merker for APL escapes (dyadic ⎕INP only)
   static UCS_string esc2;

   /// the line parts to exe executed
   static UCS_string_vector escapes;

   /// the line parts left of the escapes
   static UCS_string_vector prefixes;

   /// the raw lines read from stdin
   static UCS_string_vector raw_lines;

   /// the line parts right of the escapes
   static UCS_string_vector suffixes;
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕NA (Name Association).
 */
/// The class implementing ⎕NA
class Quad_NA : public QuadFunction
{
public:
   /// Constructor.
   Quad_NA() : QuadFunction(TOK_Quad_NA) {}

   static Quad_NA  fun;          ///< Built-in function.

protected:
   /// overloaded Function::eval_AB()
   /// @param A left argument APL value (calling-convention descriptor)
   /// @param B right argument APL value (function/library specification)
   virtual Token eval_AB(Value_P A, Value_P B) const
      { TODO; }

   /// overloaded Function::eval_B()
   /// @param B right argument APL value (name association query)
   virtual Token eval_B(Value_P B) const
      { TODO; }
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕NC (Name class).
 */
/// The class implementing ⎕NC
class Quad_NC : public QuadFunction
{
public:
   /// Constructor.
   Quad_NC() : QuadFunction(TOK_Quad_NC) {}

   /// overloaded Function::eval_B().
   /// @param B right argument APL value (name(s) to classify)
   virtual Token eval_B(Value_P B) const;

   /// return the ⎕NC for variable name \b var
   /// @param var Unicode name to classify
   static APL_Integer get_NC(const UCS_string var);

   static Quad_NC  fun;          ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕NL (Name List).
 */
/// The class implementing ⎕NL
class Quad_NL : public QuadFunction
{
public:
   /// Constructor.
   Quad_NL() : QuadFunction(TOK_Quad_NL) {}

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (name prefix filter)
   /// @param B right argument APL value (name-class filter)
   virtual Token eval_AB(Value_P A, Value_P B) const
      { return do_quad_NL(A, B); }

   /// overloaded Function::eval_B().
   /// @param B right argument APL value (name-class filter)
   virtual Token eval_B(Value_P B) const
      { return do_quad_NL(Value_P(), B); }

   static Quad_NL  fun;          ///< Built-in function.

protected:
   /// return A ⎕NL B
   /// @param A optional left argument APL value (name prefix filter)
   /// @param B right argument APL value (name-class filter)
   static Token do_quad_NL(Value_P A, Value_P B);
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕SI (State Indicator)
 */
/// The class implementing ⎕SI
class Quad_SI : public QuadFunction
{
public:
   /// Constructor.
   Quad_SI() : QuadFunction(TOK_Quad_SI) {}

   /// overloaded Function::eval_AB().
   /// @param A left argument APL value (SI attribute selector)
   /// @param B right argument APL value (SI level or name)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// overloaded Function::eval_AB().
   /// @param B right argument APL value (SI attribute selector)
   virtual Token eval_B(Value_P B) const;

   static Quad_SI  fun;          ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/**
   The system function ⎕UCS (Universal Character Set)
 */
/// The class implementing ⎕UCS
class Quad_UCS : public QuadFunction
{
public:
   /// Constructor.
   Quad_UCS() : QuadFunction(TOK_Quad_UCS) {}

   /// overloaded Function::eval_B().
   /// @param B right argument APL value (characters or code points to convert)
   virtual Token eval_B(Value_P B) const;

   static Quad_UCS  fun;          ///< Built-in function.

protected:
};
//────────────────────────────────────────────────────────────────────────────
/// Base class for ⎕STOP and ⎕TRACE
class Stop_Trace : public QuadFunction
{
protected:
   /// constructor
   /// @param tag token tag identifying this Stop/Trace function
   Stop_Trace(TokenTag tag)
   : QuadFunction (tag)
   {}

   /// return assign lines in new_value to stop or trace vector in ufun
   /// @param ufun user-defined function whose stop/trace lines are updated
   /// @param new_value APL value containing the new line numbers
   /// @param stop true for ⎕STOP, false for ⎕TRACE
   static void assign(UserFunction * ufun, const Value & new_value, bool stop);

   /// find UserFunction named \b fun_name
   /// @param fun_name APL value containing the function name to locate
   static const UserFunction * locate_fun(const Value & fun_name);

   /// return integers in lines
   /// @param lines vector of function line numbers
   /// @param assigned true if called in an assignment context
   static Token reference(const std::vector<Function_Line> & lines,
                          bool assigned);
};
//────────────────────────────────────────────────────────────────────────────
/// The class implementing ⎕STOP
class Quad_STOP : public Stop_Trace
{
public:
   /// Constructor
   Quad_STOP()
   : Stop_Trace(TOK_Quad_STOP)
   {}

   /// Overloaded Function::eval_AB()
   /// @param A left argument APL value (line numbers to set as stop points)
   /// @param B right argument APL value (function name)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_B()
   /// @param B right argument APL value (function name)
   virtual Token eval_B(Value_P B) const;

   static Quad_STOP  fun;          ///< Built-in function.
};
//────────────────────────────────────────────────────────────────────────────
/// The class implementing ⎕TRACE
class Quad_TRACE : public Stop_Trace
{
public:
   /// Constructor
   Quad_TRACE()
   : Stop_Trace(TOK_Quad_TRACE)
   {}

   /// Overloaded Function::eval_AB()
   /// @param A left argument APL value (line numbers to set as trace points)
   /// @param B right argument APL value (function name)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_B()
   /// @param B right argument APL value (function name)
   virtual Token eval_B(Value_P B) const;

   static Quad_TRACE  fun;          ///< Built-in function.
};
//────────────────────────────────────────────────────────────────────────────

#endif // __Quad_FUNCTION_HH_DEFINED__
