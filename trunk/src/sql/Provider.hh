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

#ifndef PROVIDER_HH
#define PROVIDER_HH

#include "apl-sqlite.hh"
#include "Connection.hh"

//════════════════════════════════════════════════════════════════════════════
/// An abstract provider for a database service
class Provider
{
public:
   /// destructor
   virtual  ~Provider() {}

   /// return the name of \b this provider (SQLite, postgreSQL, etc.)
   virtual const char * get_provider_name() const = 0;

   /// return the type of \b this provider (sqlite, postgresql, etc.)
   virtual const char * get_provider_type() const = 0;

   /// connect to the database
   virtual Connection * open_database(const Value & B) = 0;

   /// library version_string
   virtual const char * version_string() const = 0;

   /// library version_number
   virtual int version_number() const   = 0;
};
//════════════════════════════════════════════════════════════════════════════

#endif
