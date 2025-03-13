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
ostream & operator << (ostream &, const AP_num3 &);         // Value.cc
ostream & operator << (ostream &, const Cell &);            // Cell.cc
ostream & operator << (ostream &, const DynamicObject &);   // DynamicObject.cc
ostream & operator << (ostream &, const Format_sub &);      // Bif_F12_FORMAT.cc
ostream & operator << (ostream &, const Function &);        // Function.cc
ostream & operator << (ostream &, const Function_PC2 &);    // Common.cc
ostream & operator << (ostream &,       Id id);             // Id.cc
ostream & operator << (ostream &, const IndexExpr &);       // IndexExpr.cc
ostream & operator << (ostream &, const LineLabel &);       // Nabla.cc
ostream & operator << (ostream &, const PrintBuffer &);     // PrintBuffer.cc
ostream & operator << (ostream &, const Shape &);           // Shape.cc
ostream & operator << (ostream &, const Symbol &);          // Symbol.cc
ostream & operator << (ostream &, const Token &);           // Token.cc
ostream & operator << (ostream &, const Token_string &);    // Token.cc
ostream & operator << (ostream &,       TokenTag);          // Token.cc
ostream & operator << (ostream &,       TokenClass);        // Token.cc
ostream & operator << (ostream &, const UCS_string &);      //  UCS_string.cc
ostream & operator << (ostream &,       Unicode);           // UCS_string.cc
ostream & operator << (ostream &, const UTF8_string &);     // UTF8_string.cc
ostream & operator << (ostream &, const Value &);           // Value.cc

#endif // __PRINTOPERATOR_HH_DEFINED
