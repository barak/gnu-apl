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

// this file #includes those header files that are of interest for
// native functions

   /** ABI contract version between the interpreter and ⎕FX-loaded native
       .so libraries. A native library built against this header exports
       get_ABI_version() (see native/template.hh) returning this value;
       NativeFunction's constructor (NativeFunction.cc) rejects a library
       whose get_ABI_version() is missing or does not match the running
       interpreter's own NATIVE_ABI_VERSION, instead of loading a
       binary-incompatible .so and letting it silently corrupt the
       interpreter's heap (e.g. a stale, previously-installed library next
       to a newly built/upgraded interpreter, since native libraries live
       in an unversioned directory and are resolved by bare name).

       Bump this whenever a change could affect the in-memory layout (or
       calling convention) of anything a native function can touch --
       Value/Cell and friends above all -- so that mismatched libraries
       are rejected rather than silently misinterpreted.
    */
   enum { NATIVE_ABI_VERSION = 1 };

   // helpers
   //
#include "Common.hh"

   // Cells are the elements of  the ravel of an APL value
   //
#include "Cell.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "LvalCell.hh"
#include "PointerCell.hh"
#include "RealCell.hh"

   // errors that may be returned or thrown....
   //
#include "Error.hh"

   // Functions are primitive or user defined functions and operators
   //
#include "Function.hh"

   // Token are the result of functions and the parsed items in the body
   // of user defined functions  and operators
   //
#include "Token.hh"

   // Values are the APL values (nested or not) manipulated by Functions.
#include "Value.hh"

   // access to system variables (⎕IO and friends)
#include "Workspace.hh"

