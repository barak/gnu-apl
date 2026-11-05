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

#ifndef UNIX_SOCKET_LISTENER_HH
#define UNIX_SOCKET_LISTENER_HH

#include "Listener.hh"

//════════════════════════════════════════════════════════════════════════════
/// a Listener accepting connections on a Unix domain socket
class UnixSocketListener : public Listener {
public:
    /// constructor
    UnixSocketListener() : server_socket( 0 ), initialised( false ), closing( false ) {};

    /// destructor
    virtual ~UnixSocketListener();

    /// see Listener::start()
    virtual std::string start( void );

    /// see Listener::wait_for_connection()
    virtual void wait_for_connection( void );

    /// see Listener::close_connection()
    virtual void close_connection( void );

private:
    /// the listening server socket
    int server_socket;

    /// path of the socket file (unlink()ed on close)
    std::string filename;

    /// true once start() has completed successfully; guards
    /// close_connection()'s cleanup against a partially-initialised state
    bool initialised;

    /// set by close_connection() so wait_for_connection() can tell a
    /// deliberate shutdown from a real accept() error
    bool closing;

    /// write end of a self-pipe used to wake a blocked
    /// wait_for_connection() on close_connection()
    int notification_fd;
};
//════════════════════════════════════════════════════════════════════════════

#endif
