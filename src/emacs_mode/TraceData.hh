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

#ifndef TRACE_DATA_HH
#define TRACE_DATA_HH

#include "emacs.hh"
#include "NetworkConnection.hh"
#include "pthread.h"
#include "../Symbol.hh"

#include <set>
#include <map>

//════════════════════════════════════════════════════════════════════════════
/// per-listener state for one FollowCommand subscription: how the
/// followed value should be formatted for this listener
class TraceDataEntry {
public:
    /// constructor
    /// @param cr_level_in a `⎕CR`-style representation code (see
    ///   Quad_CR::do_CR()), or negative for the default plain format
    TraceDataEntry( int cr_level_in ) : cr_level( cr_level_in ) {}

    /// the representation code this entry was constructed with
    int get_cr_level( void ) { return cr_level; }

private:
    /// the representation code this entry was constructed with
    int cr_level;
};

/// tracks every client currently following one Symbol's assignments, and
/// pushes each new value to them
class TraceData {
public:
    /// constructor: track assignments of \b symbol_in
    TraceData( Symbol *symbol_in );

    /// destructor
    virtual ~TraceData() {};

    /// start pushing updates for the tracked symbol to \b connection,
    /// formatted per \b cr_level (see TraceDataEntry)
    void add_listener( NetworkConnection *connection, int cr_level = -1 );

    /// stop pushing updates to \b connection
    void remove_listener( NetworkConnection *connection );

    /// on an assignment event, push the tracked symbol's current value to
    /// every active listener
    void send_update( Symbol_Event ev );

    /// write \b value to \b out, formatted per \b cr_level (see
    /// TraceDataEntry)
    static void display_value_for_trace(ostream & out, Value_P value,
                                        int cr_level );

private:
    /// the tracked symbol
    Symbol *symbol;

    /// every connection currently following \b symbol, and how to
    /// format updates for it
    map<NetworkConnection *, TraceDataEntry> active_listeners;
};
//════════════════════════════════════════════════════════════════════════════

#endif
