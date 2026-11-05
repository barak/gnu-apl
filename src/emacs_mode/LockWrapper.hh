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

#ifndef LOCK_WRAPPER_HH
#define LOCK_WRAPPER_HH

#include <pthread.h>

//════════════════════════════════════════════════════════════════════════════
/// RAII lock guard: locks \b lock_in on construction, unlocks it on
/// destruction
class LockWrapper {
public:
    /// constructor: locks \b lock_in
    LockWrapper( pthread_mutex_t *lock_in );

    /// destructor: unlocks the wrapped mutex
    virtual ~LockWrapper();

private:
    /// the wrapped mutex
    pthread_mutex_t *lock;
};
//════════════════════════════════════════════════════════════════════════════

#endif
