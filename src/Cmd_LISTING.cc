/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

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
    Cmd_LISTING:: implements the workspace-introspection/listing commands
    )FNS, )NMS, )OPS, )OWNERS, )SVARS, )SYMBOL, )SYMBOLS, )VALUES, )VARS.
*/

#include "Cmd_LISTING.hh"
#include "Common.hh"
#include "Svar_DB.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_LISTING::cmd_FNS(ostream & out, const UCS_string & arg)
{
   Workspace::list(out, LIST_FUNS, arg);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_NMS(ostream & out, const UCS_string & arg)
{
   Workspace::list(out, LIST_NAMES, arg);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_OPS(ostream & out, const UCS_string & arg)
{
   Workspace::list(out, LIST_OPERS, arg);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_OWNERS(ostream & out)
{
   Value::list_all(out, true);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_SVARS(ostream & out)
{
   Svar_DB::print(out);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_SYMBOL(ostream & out, const UCS_string & arg)
{
   Workspace::get_symbol_table().list_symbol(out, arg);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_SYMBOLS(ostream & out, const UCS_string & arg)
{
   Workspace::list(out, LIST_NONE, arg);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_VALUES(ostream & out)
{
   Value::list_all(out, false);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_LISTING::cmd_VARS(ostream & out, const UCS_string & arg)
{
   Workspace::list(out, LIST_VARS, arg);
}
//════════════════════════════════════════════════════════════════════════════
