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
    Cmd_SI:: implements the SI-stack commands )SI, )SIC, )SINL, )SIS.
*/

#include "Cmd_SI.hh"
#include "Common.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_SI::cmd_SI(ostream & out, bool dbg)
{
   if (dbg)   Workspace::list_SI(out, SIM_SI_dbg);
   else       Workspace::list_SI(out, SIM_SI);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SI::cmd_SIC(ostream & out)
{
   Workspace::clear_SI(out);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SI::cmd_SINL(ostream & out)
{
   Workspace::list_SI(out, SIM_SINL);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_SI::cmd_SIS(ostream & out, bool dbg)
{
   if (dbg)   Workspace::list_SI(out, SIM_SIS_dbg);
   else       Workspace::list_SI(out, SIM_SIS);
}
//════════════════════════════════════════════════════════════════════════════
