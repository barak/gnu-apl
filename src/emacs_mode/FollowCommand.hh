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

#ifndef FOLLOW_COMMAND_HH
#define FOLLOW_COMMAND_HH

#include "NetworkCommand.hh"
#include "../Symbol.hh"
#include "TraceData.hh"

//════════════════════════════════════════════════════════════════════════════
/// the "follow" network command: subscribe to future assignment events of
/// a variable and push each new value to the client over \b conn
class FollowCommand : public NetworkCommand {
public:
    /// constructor
    FollowCommand( std::string name_in ) : NetworkCommand( name_in ) {};

    /// see NetworkCommand::run_command()
    virtual void run_command( NetworkConnection &conn, const std::vector<std::string> &args );
};
//════════════════════════════════════════════════════════════════════════════

/// maps a followed Symbol to the TraceData tracking its subscribers
typedef map<const Symbol *, TraceData *> SymbolTraceMap;

/// Symbol event callback: on an assignment event, push the symbol's new
/// value to every client following it (looked up in \b trace_data)
void symbol_assignment( const Symbol &symbol, Symbol_Event ev );

#endif
