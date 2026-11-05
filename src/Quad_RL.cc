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
*/

#include <stdlib.h>

#include "Quad_RL.hh"
#include "Value.hh"
#include "IntCell.hh"

uint64_t Quad_RL::state = 0;

//════════════════════════════════════════════════════════════════════════════
Quad_RL::Quad_RL()
   : SystemVariable(ID_Quad_RL)
{
Value_P Z(LOC);

const unsigned int seed = reset_seed();

   Z->next_ravel_Int(seed);
   Z->check_value(LOC);

   Symbol::assign(Z, false, LOC);
}
//────────────────────────────────────────────────────────────────────────────
uint64_t
Quad_RL::get_random()
{
   Assert(value_stack.size());   // by Quad_RL::assign()
   if (value_stack.back().get_NC() != NC_VARIABLE)   VALUE_ERROR;  // localized

   state *= Knuth_a;
   state += Knuth_c;

   // ⎕RL is documented as a non-negative value (apl2lrm.txt p.322 gives
   // ¯2+2*31 as APL2's own upper bound, sized for APL2's 32-bit
   // integers). GNU APL keeps the full 64-bit LCG state internally for
   // generator quality/period, but exposes/stores only its low 63 bits
   // as ⎕RL's own value -- "positive values only" scaled up to GNU
   // APL's native 64-bit integers rather than clamped down to APL2's
   // smaller ones (user's explicit call, LanguageVariances.md #31).
   //
   value_stack.back().get_val_wptr()->
      set_ravel_Int(0, state & 0x7FFFFFFFFFFFFFFFULL);
   return state;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RL::assign(Value_P B, bool /* clone */, const char * loc)
{
   if (!B->is_scalar_or_len1_vector())
      {
        if (B->get_rank() > 1)   RANK_ERROR;
        else                     LENGTH_ERROR;
      }

Cell cache;
const Cell & cell = B->get_cscalar(cache);
const APL_Integer val = cell.get_near_int();

   // ⎕RL must be non-negative (see get_random() above); 0 itself is a
   // perfectly safe seed for this specific LCG -- confirmed via the
   // Hull-Dobell full-period theorem (a≡1 mod 4, c odd, m=2^64): a
   // full-period generator has a single cycle covering every state, so
   // it provably has no fixed point and no seed (including 0) can ever
   // produce a degenerate/constant sequence. Rejected immediately at
   // assignment (user's call), not lazily on the next roll/deal, with
   // plain DOMAIN ERROR rather than the IBM-specific ⎕RL ERROR class
   // (user's call).
   //
   if (val < 0)
      {
        MORE_ERROR() << "⎕RL must be a non-negative integer; GNU APL's "
                        "valid range is 0 to 9223372036854775807 "
                        "(2*63-1) -- wider than APL2's documented "
                        "¯2+2*31, scaled up to GNU APL's native 64-bit "
                        "integers (see LanguageVariances.md #31)";
        DOMAIN_ERROR;
      }

   state = val;
   Symbol::assign(IntScalar(state, loc), false, LOC);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_RL::pop()
{
   Symbol::pop();
   state = value_stack.back().get_val_cptr()->get_near_int(0);
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
void
Quad_RL::push()
{
   // clone the current state
   //
   Symbol::push_value(IntScalar(state, LOC));
}
//════════════════════════════════════════════════════════════════════════════
