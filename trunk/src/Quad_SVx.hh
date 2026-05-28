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

#ifndef __SHARED_VARIABLES_HH_DEFINED__
#define __SHARED_VARIABLES_HH_DEFINED__

#include "QuadFunction.hh"
#include "SystemVariable.hh"

//----------------------------------------------------------------------------
/** some helper functions to start auxiliary processors */
/// Base class for ⎕SVC, ⎕SVE, ⎕SVO, ⎕SVQ, ⎕SVR, and ⎕SVS
class Quad_SVx
{
public:
   /// start the auxiliary processor \b proc
   /// @param proc auxiliary processor number to start
   static void start_AP(AP_num proc);

protected:
   /// disconnect from auxiliary processor proc if connected.
   /// @param proc auxiliary processor number to disconnect from
   static void disconnect(AP_num proc);

   /// return true iff \b filename is executable by everybody
   /// @param filename path of the file to test
   static bool is_executable(const char * filename);
};
//----------------------------------------------------------------------------
/**
   The system function ⎕SVC (Shared Variable Control).
 */
/// The class implementing ⎕SVC
class Quad_SVC : public QuadFunction, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVC() : QuadFunction(TOK_Quad_SVC) {}

   static Quad_SVC  fun;         ///< Built-in function.

protected:
   /// Overloaded Function::eval_AB().
   /// @param A left-argument APL value (access control vector)
   /// @param B right-argument APL value (shared variable name)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_B().
   /// @param B right-argument APL value (shared variable name)
   virtual Token eval_B(Value_P B) const;
};
//----------------------------------------------------------------------------
/**
   The system variable ⎕SVE (Shared Variable Event).
 */
/// The implementation of ⎕SVE
class Quad_SVE : public NL_SystemVariable, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVE();

protected:
   /// overloaded Symbol::assign()
   /// @param value new APL value to assign to ⎕SVE
   /// @param clone true to store a deep copy of value
   /// @param loc caller location for diagnostics
   virtual void assign(Value_P value, bool clone, const char * loc);

   /// Overloaded Symbol::get_apl_value().
   virtual Value_P get_apl_value() const;

   /// when the current ⎕SVE timer expires (as float)
   static APL_time_us timer_end;
};
//----------------------------------------------------------------------------
/**
   The system function Quad-SVO (Shared Variable Offer).
 */
/// The implementation of ⎕SVO
class Quad_SVO : public QuadFunction, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVO() : QuadFunction(TOK_Quad_SVO) {}

   static Quad_SVO  fun; ///< Built-in function.

protected:
   /// Overloaded Function::eval_AB().
   /// @param A left-argument APL value (processor number)
   /// @param B right-argument APL value (variable name to share)
   virtual Token eval_AB(Value_P A, Value_P B) const;

   /// Overloaded Function::eval_B().
   /// @param B right-argument APL value (variable name to share)
   virtual Token eval_B(Value_P B) const;

   /// share one variable
   /// @param proc target auxiliary processor number
   /// @param vname null-terminated UCS variable name
   /// @param coupling output coupling state after the offer
   static SV_key share_one_variable(AP_num proc, const uint32_t * vname,
                                    SV_Coupling & coupling);
};
//----------------------------------------------------------------------------
/**
   The system function Quad-SVQ (Shared Variable Query).
 */
/// The implementation of ⎕SVQ
class Quad_SVQ : public QuadFunction, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVQ () : QuadFunction(TOK_Quad_SVQ) {}

   static Quad_SVQ fun;         ///< Built-in function.

protected:
   /// Overloaded Function::eval_B().
   /// @param B right-argument APL value (query selector)
   virtual Token eval_B(Value_P B) const;

   /// return processors with matching offers
   static Value_P get_processors();

   /// return variables offered by processor proc
   /// @param proc auxiliary processor number to query
   static Value_P get_variables(AP_num proc);
};
//----------------------------------------------------------------------------
/**
   The system function ⎕SVR (shared Variable Retraction).
 */
/// The implementation of ⎕SVR
class Quad_SVR : public QuadFunction, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVR() : QuadFunction(TOK_Quad_SVR) {}

   static Quad_SVR  fun;         ///< Built-in function.

protected:
   /// Overloaded Function::eval_B().
   /// @param B right-argument APL value (variable name to retract)
   virtual Token eval_B(Value_P B) const;
};
//============================================================================
/**
   The system function ⎕SVS (Shared Variable State).
 */
/// The implementation of ⎕SVS
class Quad_SVS : public QuadFunction, Quad_SVx
{
public:
   /// Constructor.
   Quad_SVS() : QuadFunction(TOK_Quad_SVS) {}

   static Quad_SVS fun;         ///< Built-in function.

protected:
   /// Overloaded Function::eval_B().
   /// @param B right-argument APL value (variable name to query)
   virtual Token eval_B(Value_P B) const;
};
//----------------------------------------------------------------------------

#endif // __SHARED_VARIABLES_HH_DEFINED__
