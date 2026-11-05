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

#ifndef NETWORK_HH
#define NETWORK_HH

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "emacs.hh"
#include <errno.h>

#include <sys/types.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H

#if HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>   // getaddrinfo(), freeaddrinfo() on Windows
#endif // HAVE_WINSOCK2_H

#if HAVE_SYS_UN_H
# include <sys/un.h>
#endif // HAVE_SYS_UN_H

#if HAVE_NETDB_H
# include <netdb.h>   // gethostbyname() etc.
#endif

class Listener;

//════════════════════════════════════════════════════════════════════════════
/// owns a getaddrinfo() result and freeaddrinfo()s it on destruction
class AddrWrapper {
public:
    /// constructor: take ownership of \b addr_in
    AddrWrapper(struct addrinfo *addr_in) : addr(addr_in) {}

    /// destructor: freeaddrinfo()s the wrapped result
    virtual ~AddrWrapper() { freeaddrinfo( addr ); }

private:
    /// the wrapped result
    struct addrinfo *addr;
};

/// thrown when starting the network listener fails
class InitProtocolError {
public:
    /// constructor
    InitProtocolError( const std::string &message_in ) : message( message_in ) {}

    /// destructor
    virtual ~InitProtocolError() {}

    /// human-readable error description
    std::string get_message( void ) { return message; }

protected:
    /// human-readable error description
    std::string message;
};
//════════════════════════════════════════════════════════════════════════════

/// create a Listener for \b port (or a Unix socket if \b port is negative)
/// and start a thread running its accept loop
void start_listener( int port );

/// thread entry point: run one accepted NetworkConnection until it
/// disconnects or errors, then delete it
/// @param arg the NetworkConnection*, owned by this call
void *connection_loop( void *arg );

/// add \b listener to the set closed by close_listeners()
void register_listener( Listener *listener );

/// remove \b listener from the set closed by close_listeners()
void unregister_listener( Listener *listener );

/// close_connection() every currently registered Listener
void close_listeners( void );

#endif
