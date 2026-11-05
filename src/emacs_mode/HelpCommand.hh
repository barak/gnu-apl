/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2018  Elias Mårtenson, Alexey Veretennikov

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

#ifndef HELP_COMMAND_HH
#define HELP_COMMAND_HH

#include "NetworkCommand.hh"
#include "emacs.hh"
#include <vector>

//════════════════════════════════════════════════════════════════════════════
/// the "help" network command: return the entries from Help.def as
/// S-expressions. Takes either one argument (the APL symbol to describe)
/// or none (return every entry from Help.def). If no entry is found,
/// "nil" is returned so it is always safe to "read" on the Emacs Lisp side.
class HelpCommand : public NetworkCommand {
public:
    /// one Help.def entry, as loaded via the help_def() macro
    struct HelpEntry
    {
        /// default constructor (leaves fields uninitialized/empty)
        HelpEntry() {}

        /// constructor
        HelpEntry(int ar,
                  const char* prim,
                  const char* arg_name,
                  const char* title,
                  const char* descr) :
            arity(ar),
            symbol(prim),
            name(arg_name),
            short_desc(title),
            long_desc(descr) {}

        /// 0 (niladic), 1 (monadic), or 2 (dyadic)
        int arity;

        /// the APL glyph/name this entry describes
        std::string symbol;

        /// the argument name(s) shown in the entry's syntax line
        std::string name;

        /// one-line summary
        std::string short_desc;

        /// full description
        std::string long_desc;
    };

    /// all loaded HelpEntry records
    typedef std::vector<HelpEntry> HelpEntries;
public:
    /// constructor: loads help_entries from Help.def
    HelpCommand( std::string name_in );

    /// see NetworkCommand::run_command()
    virtual void run_command( NetworkConnection &conn, const std::vector<std::string> &args );
private: // variables
    /// every entry loaded from Help.def
    HelpEntries help_entries;
};
//════════════════════════════════════════════════════════════════════════════

#endif
