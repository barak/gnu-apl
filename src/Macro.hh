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

#ifndef __MACRO_HH_DEFINED__
#define __MACRO_HH_DEFINED__

#include "UserFunction.hh"

//════════════════════════════════════════════════════════════════════════════
/// a system function or operator implemented as an internal defined function
class Macro : public UserFunction
{
public:
   /// the unique number of a macro
   enum Macro_num
      {
#define mac_def(name, _txt) MAC_ ## name,   ///< a Macro_num
#include "Macro.def"
        MAC_COUNT,
        MAC_NONE = -1
      };

   /// A reference to one of a macro's positional argument/local-var
   /// symbols μ1 .. μMARG_COUNT -- 18 being the highest slot number used
   /// by any macro (in Z__pA_LO_REDUCE_X4_B and Z__nA_LO_REDUCE_X4_B: Z
   /// A1 LO [slot 4/RO unused by these two] X4 B rho_B3 B3 rho_Z rho_Z3 T
   /// H M L a N I I_max). A macro's own identity is Macro_num, not
   /// Macro_arg_num -- dispatch is always by Macro_num, and a macro's own
   /// APL header name (Macro.def-local text, e.g. "LO_EACH_B") has no
   /// functional significance since nothing ever calls a macro by name.
   /// Macro_arg_num used to also carry negative values MUE_M1 ..
   /// MUE_M29, identifying a macro by its former "μ¯N" self-name; since
   /// no macro body has contained "μ¯N" text since header names became
   /// plain Macro.def-local text, that range is gone and Macro_arg_num
   /// is positive-only. 0 is deliberately unused (not a valid slot) so
   /// it stays available as an "unset" sentinel distinct from every real
   /// Macro_arg_num value.
   ///
   enum Macro_arg_num
      {
        // 0 intentionally unused (see comment above)

        MUE_1   =   1,   ///< argument/local-var slot 1
        MUE_2   =   2,   ///< argument/local-var slot 2
        MUE_3   =   3,   ///< argument/local-var slot 3
        MUE_4   =   4,   ///< argument/local-var slot 4
        MUE_5   =   5,   ///< argument/local-var slot 5
        MUE_6   =   6,   ///< argument/local-var slot 6
        MUE_7   =   7,   ///< argument/local-var slot 7
        MUE_8   =   8,   ///< argument/local-var slot 8
        MUE_9   =   9,   ///< argument/local-var slot 9
        MUE_10  =  10,   ///< argument/local-var slot 10
        MUE_11  =  11,   ///< argument/local-var slot 11
        MUE_12  =  12,   ///< argument/local-var slot 12
        MUE_13  =  13,   ///< argument/local-var slot 13
        MUE_14  =  14,   ///< argument/local-var slot 14
        MUE_15  =  15,   ///< argument/local-var slot 15
        MUE_16  =  16,   ///< argument/local-var slot 16
        MUE_17  =  17,   ///< argument/local-var slot 17
        MUE_18  =  18,   ///< argument/local-var slot 18

        MARG_COUNT = 18,   ///< the number of argument/local-var slots

        // aliases for the 6 argument slots whose position and meaning
        // are consistent across (most) macros: Z (the result-
        // communication symbol -- a named slot like B/A, not really an
        // "argument", but treated the same way rather than folded
        // anonymously into the generic locals below), A (the left/count
        // argument, present in the dyadic-flavoured macros), LO and RO
        // (the operator's own operand functions), X (the axis/extra
        // parameter X4/X5/X7/N, present in REDUCE/SCAN/RANK/POWER-with-
        // count), B (the fundamental right operand, present in every
        // macro). Chosen, per Jürgen Sauermann, so that in every macro's
        // header the μ-slots actually used appear in increasing numeric
        // order left to right, e.g. Z←A (LO FUN RO)[X] B or Z←A (LO FUN
        // N) B -- RO and X never both appear in the same macro (they
        // occupy the same "third operand" header position, just spelled
        // differently depending on the macro), so their relative order
        // (4 vs 5) is arbitrary and does not matter. Everything from
        // slot 7 on is just LV_1 .. LV_12 (12 slots, MARG_COUNT - 6).
        //
        MUE_Z   = MUE_1,   ///< the result-communication symbol
        MUE_A   = MUE_2,   ///< the left/count operand
        MUE_LO  = MUE_3,   ///< the left operand function
        MUE_RO  = MUE_4,   ///< the right operand function
        MUE_X   = MUE_5,   ///< the axis/extra parameter
        MUE_B   = MUE_6,   ///< the right operand

        LV_1    = MUE_7,    ///< local var 1
        LV_2    = MUE_8,    ///< local var 2
        LV_3    = MUE_9,    ///< local var 3
        LV_4    = MUE_10,   ///< local var 4
        LV_5    = MUE_11,   ///< local var 5
        LV_6    = MUE_12,   ///< local var 6
        LV_7    = MUE_13,   ///< local var 7
        LV_8    = MUE_14,   ///< local var 8
        LV_9    = MUE_15,   ///< local var 9
        LV_10   = MUE_16,   ///< local var 10
        LV_11   = MUE_17,   ///< local var 11
        LV_12   = MUE_18,   ///< local var 12
      };

   /// true once mue_symbols is fully populated (set at the end of
   /// init()). SymbolTable::lookup_symbol() consults this before taking
   /// its μN fast path: init() itself looks up those very same names
   /// (via Workspace::lookup_symbol(), i.e. through that same function)
   /// while filling mue_symbols in, and must not have its own lookups
   /// short-circuited back to the not-yet-assigned array slot it is in
   /// the middle of filling -- false (the default until init() finishes)
   /// keeps those particular lookups going through the ordinary
   /// (real-symbol-table) path, exactly as before this feature existed.
   static bool mue_ready;

   /// initialize mue_symbols. Must run BEFORE the macros themselves are
   /// ⎕FX-equivalently defined (static_Objects.cc's mac_def expansion,
   /// which constructs the 29 static Macro objects below): each Macro
   /// constructor parses its own APL source text right there and then,
   /// which already does its own (unrelated, μ-prefixed-name) symbol
   /// lookups, so mue_symbols needs to already be in place before that
   /// point if anything defined while ⎕FX-ing a macro is ever going to
   /// be able to reference it.
   static void init();

   /// Symbol for every Macro_arg_num value. Indexed as: [0] = 0 (no
   /// symbol, Macro_arg_num 0 is unused); [1 .. MARG_COUNT] = the
   /// argument/local-var symbols μ1 .. μ18 (for MUE_1 .. MUE_18, i.e.
   /// index = Macro_arg_num).
   static Symbol * mue_symbols[MARG_COUNT + 1];

   /// constructor
   /// @param num unique macro identifier
   /// @param text UTF-8 encoded APL source text of the macro body
   Macro(Macro_num num, const UTF8_string & text);

   /// overloaded UserFunction::get_macnum()
   virtual int get_macnum() const
      { return macro_number; }

   /// overloaded Function::is_macro()
   virtual bool is_macro() const   { return true; }

   /// return the macro with \b macro_number num
   /// @param num macro identifier to look up
   static Macro * get_macro(Macro_num num);

   /// return true if \b sym is the name symbol (header FUN()) of a macro.
   /// Such symbols must survive )CLEAR because the macros themselves do.
   static bool is_macro_name(const Symbol * sym);

   /// unmark all values used in the bodies of all macros
   static void unmark_all_macros();

#define mac_def(name, _txt) static Macro name;   ///< a macro
#include "Macro.def"

   /// a vector of all macros
   static Macro * all_macros[MAC_COUNT];

protected:
   /// A (compile-time) unique number for this macro
   const Macro_num macro_number;

private:
   /// destructor (not supposed to be called)
   ~Macro();
};
//════════════════════════════════════════════════════════════════════════════

#endif // __MACRO_HH_DEFINED__

