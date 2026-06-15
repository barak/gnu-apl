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

#include "Listener.hh"
#include "TcpListener.hh"
#if HAVE_SYS_UN_H
#  include "UnixSocketListener.hh"
#endif
//════════════════════════════════════════════════════════════════════════════

Listener *Listener::create_listener( int port )
{
    Listener *ret;

    if( port >= 0 ) {
        ret = new TcpListener( port );
    }
    else {
#if HAVE_SYS_UN_H
        ret = new UnixSocketListener();
#else
        ret = new TcpListener( 0 );   // Unix sockets unavailable; fall back to TCP
#endif
    }

    return ret;
}
//════════════════════════════════════════════════════════════════════════════
