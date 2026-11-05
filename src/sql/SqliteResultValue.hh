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

/** @file
*/

#ifndef RESULT_VALUE_HH
#define RESULT_VALUE_HH

#include "Cell.hh"

#include <string>
#include <sqlite3.h>
#include <vector>

//════════════════════════════════════════════════════════════════════════════
/// one Sqlite result column value, in a form that can later append itself
/// to an APL Value regardless of its own concrete SQL type
class ResultValue
{
public:
    /// destructor
    virtual ~ResultValue() {}

    /// append this value as the next ravel element of \b val
    virtual void update(Value & val) const = 0;

    /// return a heap-allocated copy of \b this
   virtual ResultValue * clone() const                         = 0;
};

/// a ResultValue holding a SQLITE_INTEGER column
class IntResultValue : public ResultValue {
public:
    /// constructor
    IntResultValue( APL_Integer value_in ) : value( value_in ) {}

    /// destructor
    virtual ~IntResultValue() {}

    /// see ResultValue::update()
    virtual void update(Value & val) const;

    /// see ResultValue::clone()
   virtual ResultValue * clone() const { return new IntResultValue(value); }

private:
    /// the column value
    APL_Integer value;
};

/// a ResultValue holding a SQLITE_FLOAT column
class DoubleResultValue : public ResultValue {
public:
    /// constructor
    DoubleResultValue( double value_in ) : value( value_in ) {}

    /// destructor
    virtual ~DoubleResultValue() {}

    /// see ResultValue::update()
    virtual void update(Value & val ) const;

    /// see ResultValue::clone()
   virtual ResultValue * clone() const { return new DoubleResultValue(value); }

private:
    /// the column value
    double value;
};

/// a ResultValue holding a SQLITE_NULL column
class NullResultValue : public ResultValue {
public:
    /// constructor
    NullResultValue() {};

    /// destructor
    virtual ~NullResultValue() {}

    /// see ResultValue::update()
    virtual void update(Value & val) const;

    /// see ResultValue::clone()
   virtual ResultValue * clone() const { return new NullResultValue(); }
};

/// a ResultValue holding a SQLITE_TEXT column
class StringResultValue : public ResultValue {
public:
    /// constructor
    StringResultValue( string value_in ) : value( value_in ) {}

    /// destructor
    virtual ~StringResultValue() {}

    /// see ResultValue::update()
    virtual void update(Value & val) const;

    /// see ResultValue::clone()
   virtual ResultValue * clone() const { return new StringResultValue(value); }

private:
    /// the column value
    string value;
};

/// one row of a query result: the ResultValue of every column, in
/// column order
class ResultRow
{
public:
    /// constructor: an empty row
    ResultRow() {}

    /// copy constructor: deep-copies every column value via clone()
    ResultRow(const ResultRow & orig)
       {
         loop(o, orig.get_values().size())
             values.push_back(orig.get_values()[o]->clone());
       }

    /// destructor: deletes every column value
    ~ResultRow()
       {
         loop(v, values.size())   delete values[v];
       }

    /// append one ResultValue per column of the current row of
    /// \b statement (dispatching on each column's sqlite3_column_type())
    void add_values(sqlite3_stmt * statement);

    /// this row's column values, in column order
    const vector<const ResultValue *> & get_values() const { return values; }

private:
    /// this row's column values, in column order
    vector<const ResultValue *> values;
};
//════════════════════════════════════════════════════════════════════════════

#endif
