/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2019  Dr. Jürgen Sauermann

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

#ifndef __QUAD_WA_HH_DEFINED__
#define __QUAD_WA_HH_DEFINED__

#include "SystemVariable.hh"

//-----------------------------------------------------------------------------
/**
   System variable Quad-WA (Workspace Available).
 */
/// The class implementing ⎕WA
class Quad_WA : public RO_SystemVariable
{
public:
   /// Constructor.
   Quad_WA();

   /// initialize total_memory
   static void init(bool log_startup);

   /// the estimated (!) the amount of free memory
   static uint64_t total_memory;

   /// a safety margin causing WS FULL before complete memory starvation
   static int64_t WA_margin;

   /// a percentage between 45% and 100% to compensate malloc() losses
   static int WA_scale;

   /// the memory limit at startup
   static rlim_t initial_rlimit;

   /// the sbrk() at startup
   static unsigned long long initial_sbrk;

protected:
   /// overloaded Symbol::get_apl_value().
   virtual Value_P get_apl_value() const;
   /// estimate (!) the amount of free memory
   static uint64_t get_free_memory();

};
//-----------------------------------------------------------------------------

#endif // __QUAD_WA_HH_DEFINED__
