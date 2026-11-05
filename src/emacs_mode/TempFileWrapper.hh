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

#ifndef TEMP_FILE_WRAPPER_HH
#define TEMP_FILE_WRAPPER_HH

#include <vector>
#include <string>

//════════════════════════════════════════════════════════════════════════════
/// owns an open file descriptor and close()s it on destruction
class FileWrapper {
public:
    /// constructor: take ownership of \b fd_in (must not be -1)
    FileWrapper( int fd_in );

    /// destructor: closes the wrapped descriptor
    ~FileWrapper();

private:
    /// the wrapped descriptor
    int fd;
};

/// a uniquely-named temp file created via mkstemp(), removed on
/// destruction
class TempFileWrapper {
public:
    /// constructor: create a temp file named \b prefix + "XXXXXX"
    TempFileWrapper( const std::string &prefix );

    /// destructor: close()s the file (if not already closed) and
    /// unlink()s it
    ~TempFileWrapper();

    /// the temp file's actual (mkstemp()-generated) path
    const std::string &get_name() { return name; }

    /// the open file descriptor
    int get_fd() { return fd; }

    /// close the file descriptor (idempotent; the destructor still
    /// unlinks the file afterwards)
    void close();

private:
    /// the temp file's actual path
    std::string name;

    /// the open file descriptor
    int fd;

    /// true once close() has been called
    bool closed;
};
//════════════════════════════════════════════════════════════════════════════

#endif
