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

#ifndef LISTENER_HH
#define LISTENER_HH

#include <string>

#include "network.hh"

/// signature of a pthread_create() thread entry point
typedef void *ThreadFunction( void * );

//════════════════════════════════════════════════════════════════════════════
/// abstract base for something that accepts incoming emacs-mode
/// connections (one subclass per transport, e.g. TcpListener,
/// UnixSocketListener)
class Listener {
public:
    /// constructor: registers \b this with register_listener()
    Listener() { register_listener( this ); }

    /// destructor: unregisters \b this with unregister_listener()
    virtual ~Listener() { unregister_listener( this ); }

    /// bind/listen and return a human-readable description of where
    virtual std::string start( void ) = 0;

    /// accept loop: block until connections arrive, spawning a
    /// connection_loop() thread for each one, until closed
    virtual void wait_for_connection( void ) = 0;

    /// stop accepting and release any resources held by start()
    virtual void close_connection( void ) = 0;

    /// create a TcpListener for \b port, or a UnixSocketListener if
    /// \b port is negative and Unix sockets are available
    static Listener *create_listener( int port );

    /// remember the thread running wait_for_connection() for \b this
    virtual void set_thread( pthread_t thread_id_in ) { thread_id = thread_id_in; }

    /// the thread running wait_for_connection() for \b this
    virtual pthread_t get_thread( void ) { return thread_id; }

protected:
    /// the thread running wait_for_connection() for \b this
    pthread_t thread_id;
};

/// calls close_connection() on the wrapped Listener on destruction
class ListenerWrapper {
public:
    /// constructor: wrap \b listener_in
    ListenerWrapper( Listener *listener_in ) : listener( listener_in ) { }

    /// destructor: close_connection()s the wrapped Listener
    virtual ~ListenerWrapper() {
        listener->close_connection();
    }

private:
    /// the wrapped Listener
    Listener *listener;
};
//════════════════════════════════════════════════════════════════════════════

#endif
