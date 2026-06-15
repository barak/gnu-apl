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
class AddrWrapper {
public:
    AddrWrapper(struct addrinfo *addr_in) : addr(addr_in) {}
    virtual ~AddrWrapper() { freeaddrinfo( addr ); }

private:
    struct addrinfo *addr;
};

class InitProtocolError {
public:
    InitProtocolError( const std::string &message_in ) : message( message_in ) {}
    virtual ~InitProtocolError() {}
    std::string get_message( void ) { return message; }

protected:
    std::string message;
};
//════════════════════════════════════════════════════════════════════════════

void start_listener( int port );
void *connection_loop( void *arg );
void register_listener( Listener *listener );
void unregister_listener( Listener *listener );
void close_listeners( void );

#endif
