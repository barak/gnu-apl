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

#ifndef __PRINTOPERATOR_HH_DEFINED
#define __PRINTOPERATOR_HH_DEFINED

#include <iostream>

#include "Common.hh"
#include "Id.hh"
#include "TokenEnums.hh"
#include "Unicode.hh"

struct AP_num3;
class  Cell;
class  DynamicObject;
struct Format_sub;
class  Function;
struct Function_PC2;
class  IndexExpr;
struct LineLabel;
class  PrintBuffer;
class  Shape;
class  Symbol;
class  Token;
class  Token_string;
class  UCS_string;
class  UTF8_string;
class  Value;

// the implementations of the operators below are spread over the
// various files in which their second argument is implemented.
                                                            // Implementation:
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const AP_num3 & x);         // Value.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Cell & x);            // Cell.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const DynamicObject & x);   // DynamicObject.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Format_sub & x);      // Bif_F12_FORMAT.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Function & x);        // Function.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Function_PC2 & x);    // Common.cc
/// @param out output stream
/// @param id value to print
ostream & operator << (ostream & out,       Id id);               // Id.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const IndexExpr & x);       // IndexExpr.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const LineLabel & x);       // Nabla.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const PrintBuffer & x);     // PrintBuffer.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Shape & x);           // Shape.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Symbol & x);          // Symbol.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Token & x);           // Token.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Token_string & x);    // Token.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out,       TokenTag x);          // Token.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out,       TokenClass x);        // Token.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const UCS_string & x);      //  UCS_string.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out,       Unicode x);           // UCS_string.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const UTF8_string & x);     // UTF8_string.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const UTF8 * x);            // UTF8_string.cc
/// @param out output stream
/// @param x value to print
ostream & operator << (ostream & out, const Value & x);           // Value.cc

#endif // __PRINTOPERATOR_HH_DEFINED
