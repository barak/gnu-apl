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

#ifndef NETWORK_CONNECTION_HH
#define NETWORK_CONNECTION_HH

#include <pthread.h>
#include <string>
#include <vector>
#include <map>

#include "NetworkCommand.hh"

//════════════════════════════════════════════════════════════════════════════
/// one accepted emacs-mode client connection: reads ':'-separated command
/// lines and dispatches each to the matching registered NetworkCommand
class NetworkConnection {
public:
    /// constructor: wrap an already-accepted socket
    NetworkConnection( int socket_in );

    /// destructor: closes the socket
    virtual ~NetworkConnection();

    /// read and process_command() lines until the connection ends
    void run( void );

    /// read (and buffer-manage) one newline-terminated line from the socket
    std::string read_line_from_fd( void );

    /// write \b s to the socket
    void write_string_to_fd( const std::string &s );

    /// read lines until END_TAG and return them
    std::vector<std::string> load_block( void );

    /// send \b message followed by END_TAG, as the reply to a command
    void send_reply( const std::string &message );

    /// send \b message as an unsolicited (out-of-band) notification
    void send_notification( const std::string &message );

private:
    /// the accepted socket
    int socket_fd;

    /// read_line_from_fd()'s input buffer
    char buffer[1024];

    /// current read position in \b buffer
    int buffer_pos;

    /// number of valid bytes currently in \b buffer
    int buffer_length;

    /// registered commands, keyed by protocol name
    std::map<std::string, NetworkCommand *> commands;

    /// serializes concurrent access to this connection
    pthread_mutex_t connection_lock;

    /// unescape, split, and dispatch one ':'-separated command line to
    /// the matching entry in \b commands
    int process_command( const std::string &command );

    // show_si()/clear_si_stack()/send_function()/show_function() are
    // declared but not defined anywhere -- vestigial, superseded by
    // SiCommand/SicCommand/DefCommand/FnCommand.
    void show_si( void );
    void clear_si_stack( void );
    void send_function( const std::vector<std::string> &content );
    void show_function( const std::string &name );
};

/// base for a connection-handling error carrying a human-readable message
class ConnectionError {
public:
    /// constructor
    ConnectionError( const std::string &message_in ) : message( message_in ) {}

    /// destructor
    virtual ~ConnectionError() {}

    /// human-readable error description
    std::string get_message( void ) { return message; }

protected:
    /// human-readable error description
    const std::string message;
};

/// thrown when the client end has disconnected
class DisconnectedError : public ConnectionError {
public:
    /// constructor
    DisconnectedError( const std::string &message ) : ConnectionError( message ) {};
};

/// thrown on a malformed or unrecognized command
class ProtocolError : public ConnectionError {
public:
    /// constructor
    ProtocolError( const std::string &message ) : ConnectionError( message ) {};
};
//════════════════════════════════════════════════════════════════════════════

#endif
