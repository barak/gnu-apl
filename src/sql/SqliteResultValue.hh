/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2014  Elias Mårtenson

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

#ifndef RESULT_VALUE_HH
#define RESULT_VALUE_HH

#include "Cell.hh"

#include <string>
#include <sqlite3.h>
#include <vector>

class ResultValue
{
public:
    virtual ~ResultValue() {}
    virtual void update(Cell * cell, Value & cell_owner) const = 0;
   virtual ResultValue * clone() const                         = 0;
};

class IntResultValue : public ResultValue {
public:
    IntResultValue( APL_Integer value_in ) : value( value_in ) {}
    virtual ~IntResultValue() {}
    virtual void update( Cell *cell, Value & cell_owner ) const;
   virtual ResultValue * clone() const { return new IntResultValue(value); }

private:
    APL_Integer value;
};

class DoubleResultValue : public ResultValue {
public:
    DoubleResultValue( double value_in ) : value( value_in ) {}
    virtual ~DoubleResultValue() {}
    virtual void update( Cell *cell, Value & cell_owner ) const;
   virtual ResultValue * clone() const { return new DoubleResultValue(value); }

private:
    double value;
};

class NullResultValue : public ResultValue {
public:
    NullResultValue() {};
    virtual ~NullResultValue() {}
    virtual void update( Cell *cell, Value & cell_owner ) const;
   virtual ResultValue * clone() const { return new NullResultValue(); }
};

class StringResultValue : public ResultValue {
public:
    StringResultValue( string value_in ) : value( value_in ) {}
    virtual ~StringResultValue() {}
    virtual void update( Cell *cell, Value & cell_owner ) const;
   virtual ResultValue * clone() const { return new StringResultValue(value); }

private:
    string value;
};

class ResultRow
{
public:
    ResultRow() {}
    ResultRow(const ResultRow & orig)
       {
         loop(o, orig.get_values().size())
             values.push_back(orig.get_values()[o]->clone());
       }

    ~ResultRow()
       {
         loop(v, values.size())   delete values[v];
       }

    void add_values(sqlite3_stmt * statement);
    const vector<const ResultValue *> & get_values() const { return values; }

private:
    vector<const ResultValue *> values;
};

#endif
