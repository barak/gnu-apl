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

#include "apl-sqlite.hh"
#include "SqliteResultValue.hh"

#include "Value.hh"
#include "IntCell.hh"
#include "FloatCell.hh"
#include "CharCell.hh"
#include "PointerCell.hh"

void
IntResultValue::update(Value & Z) const
{
    Z.next_ravel_Int(value);
}

void
DoubleResultValue::update(Value & Z) const
{
    Z.next_ravel_Float(value);
}

void StringResultValue::update(Value & Z) const
{
    if (value.size() == 0)
       {
         Z.next_ravel_Pointer(Str0(LOC).get());
       }
    else
       {
         Z.next_ravel_Pointer(make_string_cell(value, LOC).get());
       }
}

void
NullResultValue::update(Value & Z) const
{
    Z.next_ravel_Pointer(Idx0(LOC).get());
}

void
ResultRow::add_values( sqlite3_stmt *statement )
{
const int n = sqlite3_column_count( statement );
    loop(i, n)
       {
          ResultValue * value;
          int type = sqlite3_column_type( statement, i );
          switch( type ) {
        case SQLITE_INTEGER:
             value = new IntResultValue(sqlite3_column_int64(statement, i));
             break;

        case SQLITE_FLOAT:
             value = new DoubleResultValue(sqlite3_column_double(statement, i));
             break;

        case SQLITE_TEXT:
             value = new StringResultValue(reinterpret_cast<const char *>
                      (sqlite3_column_text(statement, i)));
             break;

        case SQLITE_BLOB:
             value = new NullResultValue();
             break;

        case SQLITE_NULL:
             value = new NullResultValue();
             break;

        default:
            CERR << "Unsupported column type, column="
                 << i << ", type+" << type << endl;
            value = new NullResultValue();
        }

        values.push_back(value);
    }
}
