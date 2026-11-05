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

#include <stdio.h>
#include <math.h>

#include "Value.hh"
#include "ErrorCode.hh"
#include "PointerCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "Workspace.hh"

#include "Cell.icc"
//════════════════════════════════════════════════════════════════════════════

const IntCell IntCell::boolean_FALSE(0);
const IntCell IntCell::boolean_TRUE(1);

/*
   general comment: for operations between an IntCell and a "higher" numeric
   Cell type (i.e. FloatCell or ComplexCell), we try to call the corresponding
   function (or its opposite) of the higher type.

   For example:
   INT ≠ FLOAT  --> FLOAT ≠ INT
   INT ≤ FLOAT  --> FLOAT ≥ INT   (≤ is the opposite of ≥)

 */
//────────────────────────────────────────────────────────────────────────────
// monadic built-in functions...
//────────────────────────────────────────────────────────────────────────────

//────────────────────────────────────────────────────────────────────────────
/// overloaded Cell::bif_near_int64_t()
ErrorCode
IntCell::bif_near_int64_t(Cell * Z) const
{
   return IntCell::bif_near_int64_t_i(Z, value.ival);
}

//────────────────────────────────────────────────────────────────────────────
/// overloaded Cell::bif_within_quad_CT()
ErrorCode
IntCell::bif_within_quad_CT(Cell * Z) const
{
   return IntCell::bif_within_quad_CT_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
bool
IntCell::equal(const Cell & other, double qct) const
{
   if (other.is_integer_cell())
      {
        // ISO 13751 tolerant equality (p. 19) has no exception for
        // integers: two IntCells used to compare exactly (==) while the
        // very same two numbers, stored as FloatCells, compared
        // tolerantly via tolerantly_equal() -- e.g. 1000000000000000 =
        // 1000000000000001 gave 0, but 1E15 = 1000000000000001 (the
        // same values) gave 1 (Bugs28 #81). Keep the exact fast path
        // (also sidesteps any int64-to-double precision loss above
        // 2^53 when the two values are identical) and fall back to the
        // same tolerant comparison used for every other numeric type.
        //
        if (value.ival == other.get_int_value())   return true;

        // Bugs30 #3 (Blake McBride): tolerantly_equal()'s own A==B fast
        // path operates on the *already double-converted* values, which
        // can (and does, above 2⋆53) coincide for two int64s that are
        // NOT equal -- e.g. 9007199254740993 and 9007199254740992 both
        // round to the same double, so tolerantly_equal() wrongly
        // reported them equal even at qct==0, contradicting the exact
        // int64 comparison above (and IntCell::compare()'s own exact
        // result: a>b was 1 while a=b was also 1). At qct==0, ISO p.19
        // tolerant equality collapses to exact equality by definition;
        // the two int64s are already known unequal (the fast path above
        // just failed), so there is nothing further to check -- return
        // false without ever converting to double. A non-zero qct still
        // goes through the normal (inherently approximate, same as for
        // any other numeric type) tolerant comparison.
        //
        if (qct == 0.0)   return false;
        return tolerantly_equal(APL_Float(value.ival),
                                 APL_Float(other.get_int_value()), qct);
      }
   if (other.is_numeric())         return other.equal(*this, qct);
   return false;
}
//────────────────────────────────────────────────────────────────────────────
// dyadic built-in functions...
//
// where possible a function with non-int A is delegated to the corresponding
// member function of A. For numeric cells that is the FloatCell or ComplexCell
// function and otherwise the default function (that returns E_DOMAIN_ERROR.
//
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_add(Cell * Z, const Cell * A) const
{
   if (A->is_integer_cell())
      return IntCell::bif_add_ii(Z, A->get_int_value(), value.ival);
   if (A->is_real_cell())
      return FloatCell::bif_add_ff(Z, A->get_real_value(), APL_Float(value.ival));
   if (A->is_complex_cell())
      return ComplexCell::bif_add_cc(Z, A->get_complex_value(),
                                       APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_ceiling(Cell * Z) const
{
   return IntCell::bif_ceiling_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_conjugate(Cell * Z) const
{
   return IntCell::bif_conjugate_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_direction(Cell * Z) const
{
   return IntCell::bif_direction_i(Z, value.ival);
}
ErrorCode
IntCell::bif_divide(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   if (A->is_integer_cell())
      {
#ifdef cfg_RATIONAL_NUMBERS_WANTED
        APL_Integer a = A->get_int_value();
        APL_Integer b = value.ival;
        if (b == 0)
           {
             if (a == 0)   return IntCell::z1(Z);
             return E_DOMAIN_ERROR;
           }

        // a = -a and b = -b below would overflow (UB) for a or b ==
        // INT64_MIN, since -INT64_MIN is not representable as
        // APL_Integer (the same trap fixed elsewhere for bif_divide_ii/
        // bif_power_ii/bif_negative_i); reject up front instead of
        // risking UB or a wrong GCD from a still-negative operand.
        //
        if (a == APL_Integer(0x8000000000000000ULL))   return E_DOMAIN_ERROR;
        if (b == APL_Integer(0x8000000000000000ULL))   return E_DOMAIN_ERROR;

        if (b < 0)   { a = -a;   b = -b; }
        const APL_Integer g = FloatCell::gcd(b, a);
        if (b == g)   return IntCell::zI(Z, a/g);
        return FloatCell::zR(Z, a/g, b/g);
#else
        return IntCell::bif_divide_ii(Z, A->get_int_value(), value.ival);
#endif
      }
   if (A->is_real_cell())
      return FloatCell::bif_divide_ff(Z, A->get_real_value(),
                                          APL_Float(value.ival));
   if (A->is_complex_cell())
      return ComplexCell::bif_divide_cc(Z, A->get_complex_value(),
                                          APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_exponential(Cell * Z) const
{
   return IntCell::bif_exponential_i(Z, value.ival);
}
extern const APL_Integer int_factorials[] =
{
   /*  0! */  0x0000000000000001LL,
   /*  1! */  0x0000000000000001LL,
   /*  2! */  0x0000000000000002LL,
   /*  3! */  0x0000000000000006LL,
   /*  4! */  0x0000000000000018LL,
   /*  5! */  0x0000000000000078LL,
   /*  6! */  0x00000000000002d0LL,
   /*  7! */  0x00000000000013b0LL,
   /*  8! */  0x0000000000009d80LL,
   /*  9! */  0x0000000000058980LL,
   /* 10! */  0x0000000000375f00LL,
   /* 11! */  0x0000000002611500LL,
   /* 12! */  0x000000001c8cfc00LL,
   /* 13! */  0x000000017328cc00LL,
   /* 14! */  0x000000144c3b2800LL,
   /* 15! */  0x0000013077775800LL,
   /* 16! */  0x0000130777758000LL,
   /* 17! */  0x0001437eeecd8000LL,
   /* 18! */  0x0016beecca730000LL,
   /* 19! */  0x01b02b9306890000LL,
   /* 20! */  0x21c3677c82b40000LL
};

extern const APL_Float float_factorials[] =
{
   /*   0! */   1.0000000000000000E0  ,
   /*   1! */   1.0000000000000000E0  ,
   /*   2! */   2.0000000000000000E0  ,
   /*   3! */   6.0000000000000000E0  ,
   /*   4! */   2.4000000000000000E1  ,
   /*   5! */   1.2000000000000000E2  ,
   /*   6! */   7.2000000000000000E2  ,
   /*   7! */   5.0400000000000000E3  ,
   /*   8! */   4.0320000000000000E4  ,
   /*   9! */   3.6288000000000000E5  ,
   /*  10! */   3.6288000000000000E6  ,
   /*  11! */   3.9916800000000000E7  ,
   /*  12! */   4.7900160000000000E8  ,
   /*  13! */   6.2270208000000000E9  ,
   /*  14! */   8.7178291200000000E10 ,
   /*  15! */   1.3076743680000000E12 ,
   /*  16! */   2.0922789888000000E13 ,
   /*  17! */   3.5568742809600000E14 ,
   /*  18! */   6.4023737057280000E15 ,
   /*  19! */   1.2164510040883200E17 ,
   /*  20! */   2.4329020081766400E18 ,
   /*  21! */   5.1090942171709440E19 ,
   /*  22! */   1.1240007277776077E21 ,
   /*  23! */   2.5852016738884978E22 ,
   /*  24! */   6.2044840173323941E23 ,
   /*  25! */   1.5511210043330986E25 ,
   /*  26! */   4.0329146112660565E26 ,
   /*  27! */   1.0888869450418352E28 ,
   /*  28! */   3.0488834461171387E29 ,
   /*  29! */   8.8417619937397019E30 ,
   /*  30! */   2.6525285981219107E32 ,
   /*  31! */   8.2228386541779224E33 ,
   /*  32! */   2.6313083693369352E35 ,
   /*  33! */   8.6833176188118859E36 ,
   /*  34! */   2.9523279903960416E38 ,
   /*  35! */   1.0333147966386145E40 ,
   /*  36! */   3.7199332678990125E41 ,
   /*  37! */   1.3763753091226346E43 ,
   /*  38! */   5.2302261746660112E44 ,
   /*  39! */   2.0397882081197444E46 ,
   /*  40! */   8.1591528324789768E47 ,
   /*  41! */   3.3452526613163808E49 ,
   /*  42! */   1.4050061177528800E51 ,
   /*  43! */   6.0415263063373834E52 ,
   /*  44! */   2.6582715747884489E54 ,
   /*  45! */   1.1962222086548019E56 ,
   /*  46! */   5.5026221598120892E57 ,
   /*  47! */   2.5862324151116818E59 ,
   /*  48! */   1.2413915592536073E61 ,
   /*  49! */   6.0828186403426752E62 ,
   /*  50! */   3.0414093201713376E64 ,
   /*  51! */   1.5511187532873822E66 ,
   /*  52! */   8.0658175170943877E67 ,
   /*  53! */   4.2748832840600255E69 ,
   /*  54! */   2.3084369733924138E71 ,
   /*  55! */   1.2696403353658276E73 ,
   /*  56! */   7.1099858780486348E74 ,
   /*  57! */   4.0526919504877214E76 ,
   /*  58! */   2.3505613312828785E78 ,
   /*  59! */   1.3868311854568984E80 ,
   /*  60! */   8.3209871127413899E81 ,
   /*  61! */   5.0758021387722484E83 ,
   /*  62! */   3.1469973260387939E85 ,
   /*  63! */   1.9826083154044401E87 ,
   /*  64! */   1.2688693218588417E89 ,
   /*  65! */   8.2476505920824715E90 ,
   /*  66! */   5.4434493907744307E92 ,
   /*  67! */   3.6471110918188683E94 ,
   /*  68! */   2.4800355424368305E96 ,
   /*  69! */   1.7112245242814130E98 ,
   /*  70! */   1.1978571669969892E100,
   /*  71! */   8.5047858856786230E101,
   /*  72! */   6.1234458376886085E103,
   /*  73! */   4.4701154615126844E105,
   /*  74! */   3.3078854415193862E107,
   /*  75! */   2.4809140811395400E109,
   /*  76! */   1.8854947016660504E111,
   /*  77! */   1.4518309202828587E113,
   /*  78! */   1.1324281178206297E115,
   /*  79! */   8.9461821307829757E116,
   /*  80! */   7.1569457046263806E118,
   /*  81! */   5.7971260207473678E120,
   /*  82! */   4.7536433370128420E122,
   /*  83! */   3.9455239697206588E124,
   /*  84! */   3.3142401345653532E126,
   /*  85! */   2.8171041143805501E128,
   /*  86! */   2.4227095383672734E130,
   /*  87! */   2.1077572983795279E132,
   /*  88! */   1.8548264225739844E134,
   /*  89! */   1.6507955160908460E136,
   /*  90! */   1.4857159644817615E138,
   /*  91! */   1.3520015276784029E140,
   /*  92! */   1.2438414054641308E142,
   /*  93! */   1.1567725070816416E144,
   /*  94! */   1.0873661566567431E146,
   /*  95! */   1.0329978488239059E148,
   /*  96! */   9.9167793487094965E149,
   /*  97! */   9.6192759682482120E151,
   /*  98! */   9.4268904488832480E153,
   /*  99! */   9.3326215443944153E155,
   /* 100! */   9.3326215443944151E157,
   /* 101! */   9.4259477598383599E159,
   /* 102! */   9.6144667150351271E161,
   /* 103! */   9.9029007164861805E163,
   /* 104! */   1.0299016745145628E166,
   /* 105! */   1.0813967582402910E168,
   /* 106! */   1.1462805637347084E170,
   /* 107! */   1.2265202031961380E172,
   /* 108! */   1.3246418194518290E174,
   /* 109! */   1.4438595832024937E176,
   /* 110! */   1.5882455415227430E178,
   /* 111! */   1.7629525510902446E180,
   /* 112! */   1.9745068572210740E182,
   /* 113! */   2.2311927486598138E184,
   /* 114! */   2.5435597334721877E186,
   /* 115! */   2.9250936934930160E188,
   /* 116! */   3.3931086844518981E190,
   /* 117! */   3.9699371608087211E192,
   /* 118! */   4.6845258497542909E194,
   /* 119! */   5.5745857612076058E196,
   /* 120! */   6.6895029134491271E198,
   /* 121! */   8.0942985252734441E200,
   /* 122! */   9.8750442008336011E202,
   /* 123! */   1.2146304367025329E205,
   /* 124! */   1.5061417415111409E207,
   /* 125! */   1.8826771768889261E209,
   /* 126! */   2.3721732428800469E211,
   /* 127! */   3.0126600184576594E213,
   /* 128! */   3.8562048236258041E215,
   /* 129! */   4.9745042224772875E217,
   /* 130! */   6.4668554892204741E219,
   /* 131! */   8.4715806908788206E221,
   /* 132! */   1.1182486511960043E224,
   /* 133! */   1.4872707060906857E226,
   /* 134! */   1.9929427461615188E228,
   /* 135! */   2.6904727073180504E230,
   /* 136! */   3.6590428819525489E232,
   /* 137! */   5.0128887482749920E234,
   /* 138! */   6.9177864726194886E236,
   /* 139! */   9.6157231969410894E238,
   /* 140! */   1.3462012475717526E241,
   /* 141! */   1.8981437590761709E243,
   /* 142! */   2.6953641378881629E245,
   /* 143! */   3.8543707171800731E247,
   /* 144! */   5.5502938327393044E249,
   /* 145! */   8.0479260574719917E251,
   /* 146! */   1.1749972043909107E254,
   /* 147! */   1.7272458904546389E256,
   /* 148! */   2.5563239178728654E258,
   /* 149! */   3.8089226376305698E260,
   /* 150! */   5.7133839564458547E262,
   /* 151! */   8.6272097742332400E264,
   /* 152! */   1.3113358856834524E267,
   /* 153! */   2.0063439050956823E269,
   /* 154! */   3.0897696138473508E271,
   /* 155! */   4.7891429014633941E273,
   /* 156! */   7.4710629262828942E275,
   /* 157! */   1.1729568794264145E278,
   /* 158! */   1.8532718694937350E280,
   /* 159! */   2.9467022724950384E282,
   /* 160! */   4.7147236359920616E284,
   /* 161! */   7.5907050539472190E286,
   /* 162! */   1.2296942187394494E289,
   /* 163! */   2.0044015765453026E291,
   /* 164! */   3.2872185855342959E293,
   /* 165! */   5.4239106661315887E295,
   /* 166! */   9.0036917057784375E297,
   /* 167! */   1.5036165148649991E300,
   /* 168! */   2.5260757449731984E302,
   /* 169! */   4.2690680090047051E304,
   /* 170! */   7.2574156153079990E306
};
enum
{
   int_fact_max   = sizeof(int_factorials)   / sizeof(APL_Integer),
   float_fact_max = sizeof(float_factorials) / sizeof(APL_Float)
};

ErrorCode
IntCell::bif_factorial(Cell * Z) const
{
   return IntCell::bif_factorial_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_floor(Cell * Z) const
{
   return IntCell::bif_floor_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_magnitude(Cell * Z) const
{
   return IntCell::bif_magnitude_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_multiply(Cell * Z, const Cell * A) const
{
   if (A->is_integer_cell())
      return IntCell::bif_multiply_ii(Z, A->get_int_value(), value.ival);
   if (A->is_real_cell())
      return FloatCell::bif_multiply_ff(Z, A->get_real_value(),
                                            APL_Float(value.ival));
   if (A->is_complex_cell())
      return ComplexCell::bif_multiply_cc(Z, A->get_complex_value(),
                                            APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_power(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;
   if (A->is_integer_cell())
      return IntCell::bif_power_ii(Z, A->get_int_value(), value.ival);
   if (A->is_real_cell())
      return FloatCell::bif_power_fi(Z, A->get_real_value(), value.ival);
   return ComplexCell::bif_power_ci(Z, A->get_complex_value(), value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_nat_log(Cell * Z) const
{
   return IntCell::bif_nat_log_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_negative(Cell * Z) const
{
   return IntCell::bif_negative_i(Z, value.ival);
}
ErrorCode
IntCell::bif_pi_times(Cell * Z) const
{
   return IntCell::bif_pi_times_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_pi_times_inverse(Cell * Z) const
{
   return IntCell::bif_pi_times_inverse_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_reciprocal(Cell * Z) const
{
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   switch(value.ival)
      {
        case  0: return E_DOMAIN_ERROR;
        case  1: return IntCell::z1(Z);
        case -1: return IntCell::z_1(Z);
      }
   if (value.ival < 0)   return FloatCell::zR(Z, -1, -value.ival);
   else                  return FloatCell::zR(Z,  1,  value.ival);
#endif
   return IntCell::bif_reciprocal_i(Z, value.ival);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_roll(Cell * Z) const
{
   return IntCell::bif_roll_i(Z, value.ival);
}
//════════════════════════════════════════════════════════════════════════════
//════════════════════════════════════════════════════════════════════════════
ErrorCode
IntCell::bif_subtract(Cell * Z, const Cell * A) const
{
   if (A->is_integer_cell())
      return IntCell::bif_subtract_ii(Z, A->get_int_value(), value.ival);
   if (A->is_real_cell())
      return FloatCell::bif_subtract_ff(Z, A->get_real_value(),
                                            APL_Float(value.ival));
   if (A->is_complex_cell())
      return ComplexCell::bif_subtract_cc(Z, A->get_complex_value(),
                                            APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_maximum(Cell * Z, const Cell * A) const
{
   if (A->is_integer_cell())
      return IntCell::bif_maximum_ii(Z, A->get_int_value(), value.ival);

   // a complex A with an imaginary part of exactly 0 is real: compare it
   // exactly at ⎕CT←0 (Blake McBride, Bugs32 #14) like a FloatCell A
   //
   if (A->is_real_cell() ||
       (A->is_complex_cell() && A->get_imag_value() == 0.0))
      {
        if (Workspace::get_CT() == 0.0)
           {
             // Bugs31 #8 (Blake McBride): see FloatCell::bif_maximum()
             // -- A->get_real_value() below converts this IntCell's
             // exact int64 through double first, losing precision
             // beyond 2⋆53. Compare exactly and, if this IntCell wins,
             // return its ORIGINAL exact value.
             //
             const APL_Float f = A->get_real_value();
             if (Cell::compare_int_float(value.ival, f) >= 0)
                return IntCell::zI(Z, value.ival);
             return FloatCell::zF(Z, f);
           }
        return FloatCell::bif_maximum_ff(Z, A->get_real_value(),
                                             APL_Float(value.ival));
      }
   if (A->is_complex_cell())
      return ComplexCell::bif_maximum_cc(Z, A->get_complex_value(),
                                           APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_minimum(Cell * Z, const Cell * A) const
{
   if (A->is_integer_cell())
      return IntCell::bif_minimum_ii(Z, A->get_int_value(), value.ival);

   // a complex A with an imaginary part of exactly 0 is real: compare it
   // exactly at ⎕CT←0 (Blake McBride, Bugs32 #14) like a FloatCell A
   //
   if (A->is_real_cell() ||
       (A->is_complex_cell() && A->get_imag_value() == 0.0))
      {
        if (Workspace::get_CT() == 0.0)
           {
             // see IntCell::bif_maximum() above (Bugs31 #8)
             //
             const APL_Float f = A->get_real_value();
             if (Cell::compare_int_float(value.ival, f) <= 0)
                return IntCell::zI(Z, value.ival);
             return FloatCell::zF(Z, f);
           }
        return FloatCell::bif_minimum_ff(Z, A->get_real_value(),
                                             APL_Float(value.ival));
      }
   if (A->is_complex_cell())
      return ComplexCell::bif_minimum_cc(Z, A->get_complex_value(),
                                           APL_Complex(value.ival, 0));
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_residue(Cell * Z, const Cell * A) const
{
   if (!A->is_numeric())   return E_DOMAIN_ERROR;

   if (A->get_imag_value() != 0.0)
      return ComplexCell::bif_residue_cc(Z, A->get_complex_value(),
                                           APL_Complex(value.ival, 0));

const APL_Float a = A->get_real_value();

   // an exact IntCell A always takes the exact int64 '%' path below,
   // however large -- checked before the near-INT64-limit float
   // fallback rather than after it, since converting an A that is
   // already an exact IntCell to double is precisely where that
   // fallback loses the most precision (a double's 53-bit mantissa
   // cannot even distinguish adjacent int64 values up here), turning
   // what should be the best-case input into the worst case (Blake
   // McBride, Bugs22 #1).
   if (A->is_integer_cell())
      return IntCell::bif_residue_ii(Z, A->get_int_value(), value.ival);

   // A FloatCell whose value happens to be integral (e.g. a literal like
   // 10.0, or any computed float) should get the same exact treatment
   // as a genuine IntCell A: without this, e.g. 10.0|123456789012345678
   // fell through to the float-quotient path below, where converting
   // the int64 B to double first (quot = value.ival / a) already loses
   // precision, and then the |quot| > 4.5E15 "tiny A" overflow guard
   // (Bugs23 #4, meant for a genuinely near-zero A) fires on this
   // unrelated large-but-finite quotient too, silently returning 0. See
   // Bugs27 #25.
   //
   // is_near_int64_t(), not is_near_int(): the latter also returns true
   // for an A beyond int64 range (e.g. 1E200 -- nothing left to round
   // off at that magnitude), which near_int() below would then reject
   // with its own DOMAIN_ERROR (confirmed via Residue.tc's A∘.|A over
   // ±1E100/±1E200: this raised exactly that DOMAIN_ERROR before the
   // fix). is_near_int64_t() is the one that actually agrees with what
   // near_int() can convert; a genuinely huge A already falls through
   // correctly to the float path below.
   //
   // a == nearbyint(a), not just is_near_int64_t(a): the latter allows
   // a to be within INTEGER_TOLERANCE (a fixed absolute epsilon) of an
   // integer, not only EXACTLY one -- for a value deliberately offset by
   // a tiny amount to probe ⎕CT (e.g. 12.000000000001, 1E¯12 away from
   // 12), rounding it down to the exact integer 12 here discarded
   // exactly the precision a tight ⎕CT comparison needed, changing
   // 12.000000000001|12 from the correct 12 (12 < the modulus, so no
   // reduction) to 0 (as if the modulus genuinely were 12). Confirmed
   // via Quad_CT.tc's ∣ CT_OP regression. Requiring exact equality
   // keeps the fix scoped to values like 10.0 that really are integers,
   // not merely close to one.
   //
   if (Cell::is_near_int64_t(a) && a == nearbyint(a))
      return IntCell::bif_residue_ii(Z, APL_Integer(a), value.ival);

   // for a genuinely non-integer A near INT64 limit, use float arithmetic
   if (a > (BIG_INT64_F - 1E10) || a < (1E10 - BIG_INT64_F))
      return FloatCell::bif_residue_ff(Z, a, APL_Float(value.ival));

   // non-integer float A: use CT tolerance check and near-zero guard
   // (mirrors the original IntCell::bif_residue for float A)
   if (a == 0.0)   return IntCell::zI(Z, value.ival);
   if (value.ival == 0)   return IntCell::z0(Z);

const APL_Float quot = value.ival / a;
   // same overflow guard as FloatCell::bif_residue_ff(): when |a| is
   // small enough that B÷a overflows, quot (and qf below) become
   // +-inf, the ⎕CT tests below compare inf against inf and never
   // match, and z = B - a*inf silently comes out +-inf instead of 0
   // (Blake McBride, Bugs23 #4 -- e.g. 1E¯308|2 gave ¯∞).
   if (!isfinite(quot))        return IntCell::z0(Z);
   if (fabs(quot) > 4.5E15)    return IntCell::z0(Z);
const APL_Float qf = floor(quot);
const double qct = Workspace::get_CT();
   if (qct != 0)
      {
        // a nearby integer of 0 is never a match here either -- same
        // near-zero-quotient bug and fix as Cell::integral_within(),
        // whose two branches this duplicates by hand (Blake McBride,
        // Bugs22 #1).
        const APL_Float qc = ceil(quot);
        if (qc != 0.0 && quot > qc - qct)   return IntCell::z0(Z);
        if (qf != 0.0 && quot < qf + qct)   return IntCell::z0(Z);
      }

APL_Float z = value.ival - a * qf;
   if (Cell::is_near_zero(z))   return IntCell::z0(Z);
   if (a < 0.0 && z > 0.0)   z = z + a;
   else if (a > 0.0 && z < 0.0)   z = z + a;
   return FloatCell::zF(Z, z);
}
//════════════════════════════════════════════════════════════════════════════
ErrorCode
IntCell::bif_add_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_subtract(Z, this);
}
ErrorCode
IntCell::bif_multiply_inverse(Cell * Z, const Cell * A) const
{
   return A->bif_divide(Z, this);
}
//────────────────────────────────────────────────────────────────────────────
CellType
IntCell::get_cell_subtype() const
{
   if (value.ival < 0)   // negative integer (only fits in signed containers)
      {
        if (-value.ival <= 0x80)
           return CellType(CT_INT | CTS_S8 | CTS_S16 | CTS_S32 | CTS_S64);

        if (-value.ival <= 0x8000)
           return CellType(CT_INT | CTS_S16 | CTS_S32 | CTS_S64);

        if (-value.ival <= 0x80000000)
           return CellType(CT_INT | CTS_S32 | CTS_S64);

        return CellType(CT_INT | CTS_S64);
      }

   // positive integer
   //
   if (value.ival == 0)   // 0: bit (fits in all containers)
      return CellType(CT_INT | CTS_BIT | CTS_X8  | CTS_S8  | CTS_U8  |
                                         CTS_X16 | CTS_S16 | CTS_U16 |
                                         CTS_X32 | CTS_S32 | CTS_U32 |
                                         CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival == 1)   // 1: bit (fits in all containers)
      return CellType(CT_INT | CTS_BIT | CTS_X8  | CTS_S8  | CTS_U8  |
                                         CTS_X16 | CTS_S16 | CTS_U16 |
                                         CTS_X32 | CTS_S32 | CTS_U32 |
                                         CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0x7F)
      return CellType(CT_INT | CTS_X8  | CTS_S8  | CTS_U8  |
                               CTS_X16 | CTS_S16 | CTS_U16 |
                               CTS_X32 | CTS_S32 | CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0xFF)
      return CellType(CT_INT |                     CTS_U8  |
                               CTS_X16 | CTS_S16 | CTS_U16 |
                               CTS_X32 | CTS_S32 | CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0x7FFF)
      return CellType(CT_INT | CTS_X16 | CTS_S16 | CTS_U16 |
                               CTS_X32 | CTS_S32 | CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0xFFFF)
      return CellType(CT_INT |                     CTS_U16 |
                               CTS_X32 | CTS_S32 | CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0x7FFFFFFF)
      return CellType(CT_INT | CTS_X32 | CTS_S32 | CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0xFFFFFFFF)
      return CellType(CT_INT |                     CTS_U32 |
                               CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0x7FFFFFFFFFFFFFFFLL)   // note: this is always the case
      return CellType(CT_INT | CTS_X64 | CTS_S64 | CTS_U64);

   return CellType(CT_INT | CTS_U64);
}
//────────────────────────────────────────────────────────────────────────────
// throw/nothrow boundary. Functions above MUST NOT (directly or indirectly)
// throw while funcions below MAY throw.
//────────────────────────────────────────────────────────────────────────────

#include "Error.hh"

//────────────────────────────────────────────────────────────────────────────
bool
IntCell::greater(const Cell & other) const
{
const APL_Integer this_val  = get_int_value();

   switch(other.get_cell_type())
      {
        case CT_INT:
             {
               const APL_Integer other_val = other.get_int_value();
               if (this_val == other_val)   return this > &other;
               return this_val > other_val;
             }

        case CT_FLOAT:
             {
               const APL_Float other_val = other.get_real_value();
               if (this_val == other_val)   return this > &other;
               return this_val > other_val;
             }

        case CT_COMPLEX: break;
        case CT_CHAR:    return true;    // int is always greater than char
        case CT_POINTER: return false;   // int is always smaller than nested
        case CT_CELLREF: DOMAIN_ERROR;
        default:         Assert(0 && "Bad celltype");
      }

const Comp_result comp = compare(other);
   if (comp == COMP_EQ)   return this > &other;
   return (comp == COMP_GT);
}
//────────────────────────────────────────────────────────────────────────────
Comp_result
IntCell::compare(const Cell & other) const
{
   if (other.is_integer_cell())
      {
       if (value.ival == other.get_int_value())   return COMP_EQ;
       return (value.ival < other.get_int_value()) ? COMP_LT : COMP_GT;
      }

   if (other.is_numeric())   // float or complex
      {
        return Comp_result(-other.compare(*this));
      }

   if (other.is_character_cell())   return COMP_GT;   // numeric > char
   if (other.is_pointer_cell())     return COMP_LT;   // numeric < nested
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
PrintBuffer
IntCell::character_representation(const PrintContext & pctx) const
{
   if (pctx.get_scaled())
      {
        FloatCell fc(get_int_value());
        return fc.character_representation(pctx);
      }

char cc[40];
   SPRINTF(cc, "%lld", long_long(value.ival));

UCS_string ucs;

   loop(c, sizeof(cc))
       {
         const char q = cc[c];
         if (q == 0)   break;
         if (q == '-')   ucs << UNI_OVERBAR;
         else            ucs << Unicode(q);
       }

ColInfo info;
   info.flags |= CT_INT;
   info.real_len = ucs.size();
   info.int_len = ucs.size();

   return PrintBuffer(ucs, info);
}
//────────────────────────────────────────────────────────────────────────────
int
IntCell::get_byte_value() const
{
   if (value.ival < -128)
      {
        MORE_ERROR() << "integer " << value.ival
                     << " is too small (< -128) for a byte value";
        DOMAIN_ERROR;
      }

   if (value.ival > 255)
      {
        MORE_ERROR() << "integer " << value.ival
                     << " is too large (> 255) for a byte value";
        DOMAIN_ERROR;
      }

   return value.ival;
}
//────────────────────────────────────────────────────────────────────────────
int
IntCell::CDR_size() const
{
   // use 4 byte for small integers and 8 bytes for others (converted to float).
   //
   // was: only val==0 or val==0xFFFFFFFF (4294967295 as a signed 64-bit
   // APL_Integer, not -1) got 4 bytes; every other integer -- including
   // ordinary small ones like 1, 2, or -5 -- fell through to 8, which
   // contradicts the comment's own stated intent and the sibling
   // range check already used for the same "fits in CDR's 32-bit slot"
   // decision elsewhere (cValue.cc's get_CDR_type()). Fixed to test the
   // actual signed 32-bit range.
   //
const APL_Integer val = get_int_value();

   if (val >= -0x80000000LL && val <= 0x7FFFFFFFLL)   return 4;
   return 8;
}
//════════════════════════════════════════════════════════════════════════════
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_add_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   // a + b directly would be signed overflow (UB) exactly when
   // sum_overflow() below is about to say so (Blake McBride,
   // Bugs12.md #11a). Compute via uint64_t, whose overflow is
   // well-defined wraparound, then reinterpret -- same bit pattern
   // two's-complement a + b would have produced anyway.
   //
const APL_Integer z = APL_Integer(uint64_t(a) + uint64_t(b));
   if (Cell::sum_overflow(z, a, b))
      return FloatCell::zF(Z, APL_Float(a) + APL_Float(b));
   return IntCell::zI(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_subtract_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   // see bif_add_ii() above.
   //
const APL_Integer z = APL_Integer(uint64_t(a) - uint64_t(b));
   if (Cell::diff_overflow(z, a, b))
      return FloatCell::zF(Z, APL_Float(a) - APL_Float(b));
   return IntCell::zI(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_multiply_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   // exact overflow detection (Bugs28 #82), like bif_add_ii()/
   // bif_subtract_ii() above: the previous double-precision pre-check
   // (comparing APL_Float(a)*APL_Float(b) against the conservative
   // LARGE_INT/SMALL_INT margin) demoted the top ~0.4% of the int64_t
   // range to float even when the exact product fit, e.g.
   // 9223372036854775807×1 gave 9.2233720368547758E18 instead of the
   // exact integer.
   //
APL_Integer prod;
   if (__builtin_mul_overflow(a, b, &prod))
      return FloatCell::zF(Z, APL_Float(a) * APL_Float(b));
   return IntCell::zI(Z, prod);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_divide_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   if (b == 0)
      {
        if (a == 0)   return IntCell::z1(Z);
        return E_DOMAIN_ERROR;
      }

   // a / b overflows (hardware #DE / SIGFPE) for the single combination
   // b == -1 && a == INT64_MIN, since -INT64_MIN is not representable
   // as APL_Integer. Confirmed directly: ¯9223372036854775808 ÷ ¯1
   // crashes the interpreter. Handle b == -1 up front (negation, with
   // the INT64_MIN sub-case promoted to float) instead of dividing.
   if (b == -1)
      {
        if (a == APL_Integer(0x8000000000000000ULL))   // INT64_MIN
           return FloatCell::zF(Z, -APL_Float(a));
        return IntCell::zI(Z, -a);
      }

const APL_Integer i_quot = a / b;
   if (a != i_quot * b)   return FloatCell::zF(Z, a / APL_Float(b));
   return IntCell::zI(Z, i_quot);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_power_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
const bool invert_Z = b < 0;
   if (invert_Z)   b = -b;

   if (b == 0)   return IntCell::z1(Z);   // A⋆0 = 1

   if (b == 1)
      {
        if (invert_Z)
           {
             switch(a)
                {
                  case  0: return E_DOMAIN_ERROR;
                  case  1: return IntCell::z1(Z);
                  case -1: return IntCell::z_1(Z);
                  default: return FloatCell::zF(Z, 1.0 / a);
                }
           }
        return IntCell::zI(Z, a);
      }

   // b >= 2
   //
   if (a == 0)
      {
        if (invert_Z)   return E_DOMAIN_ERROR;
        return IntCell::z0(Z);
      }
   if (a == 1)    return IntCell::z1(Z);
   if (a == -1)   return (b & 1) ? IntCell::z_1(Z) : IntCell::z1(Z);
   if (a == APL_Integer(0x8000000000000000ULL))   // INT64_MIN
      {
        // -a below would overflow right back to INT64_MIN (UB, and stays
        // negative), which later makes a_2_n wrap to 0 and SIGFPEs on the
        // division at line ~822. pow() handles a negative base directly
        // (correct sign for odd/even b), so go straight to the float path.
        APL_Float z = pow(APL_Float(a), APL_Float(b));
        if (invert_Z)   z = 1.0 / z;
        return FloatCell::zF(Z, z);
      }

const bool negate_Z = (a < 0) && (b & 1);
   if (a < 0)   a = -a;

bool overflow = false;
APL_Integer a_2_n = a;
APL_Integer zi = 1;
   for (APL_Integer b1 = b; b1; b1 >>= 1)
      {
        if (b1 & 1)
           {
             if (uint64_t(zi) >= 0x7FFFFFFFFFFFFFFFULL / a_2_n)
                {
                  overflow = true;
                  break;
                }
             zi *= a_2_n;
             if (b1 == 1)   break;
           }

        // a_2_n *= a_2_n below must not overflow int64: the real bound is
        // ⌊√INT64_MAX⌋ = 3037000499, not the far tighter 1E8 this used to
        // check -- 1E8 falsely declared overflow for e.g. 10*16 (needs
        // a_2_n up to 1E16, which fits easily), silently downgrading an
        // exact IntCell result to an imprecise FloatCell one. The actual
        // consumer, zi *= a_2_n a few lines above, already has its own
        // overflow check. See Bugs27 #43.
        if (a_2_n >= 3037000500LL)
           {
             overflow = true;
             break;
           }
        a_2_n *= a_2_n;
      }

   if (!overflow)
      {
        if (negate_Z)    zi = -zi;
        if (!invert_Z)   return IntCell::zI(Z, zi);
        if (zi != 0)     return FloatCell::zF(Z, 1.0 / zi);
        return E_DOMAIN_ERROR;
      }

   // overflow: use float. Pass the ORIGINAL (negative, when invert_Z)
   // exponent to pow() directly rather than computing pow(a,b) and then
   // inverting: pow(2.0, 1024.0) alone already overflows to inf (b was
   // negated to a positive 1024 above), and 1.0/inf silently becomes 0
   // -- even though the true result (2⋆¯1024 = 1/2⋆1024) is a perfectly
   // representable tiny double. pow(a, -b) computes that directly, with
   // no intermediate overflow. See Bugs27 #23.
   //
APL_Float z = pow(APL_Float(a), invert_Z ? -APL_Float(b) : APL_Float(b));
   if (negate_Z)   z = -z;
   if (!isfinite(z))   return E_DOMAIN_ERROR;   // 2⋆4096 gave ∞
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_maximum_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   return IntCell::zI(Z, a >= b ? a : b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_minimum_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   return IntCell::zI(Z, a <= b ? a : b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_residue_ii(Cell * Z, APL_Integer a, APL_Integer b)
{
   if (a == 0)   return IntCell::zI(Z, b);
   if (b == 0)   return IntCell::z0(Z);

   // b % a overflows (hardware #DE / SIGFPE) for a == -1 && b ==
   // INT64_MIN, the same INT64_MIN/-1 overflow as bif_divide_ii above.
   // Confirmed directly: ¯1 | ¯9223372036854775808 crashes the
   // interpreter. Residue by +-1 is always 0, so this is a cheap
   // early-out rather than a special divide.
   if (a == 1 || a == -1)   return IntCell::z0(Z);

APL_Integer rest = b % a;
   if (a < 0)
      {
        if (rest > 0)   rest += a;
      }
   else
      {
        if (rest < 0)   rest += a;
      }
   return IntCell::zI(Z, rest);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_ceiling_i(Cell * Z, APL_Integer b)
{
   return IntCell::zI(Z, b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_floor_i(Cell * Z, APL_Integer b)
{
   return IntCell::zI(Z, b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_conjugate_i(Cell * Z, APL_Integer b)
{
   return IntCell::zI(Z, b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_direction_i(Cell * Z, APL_Integer b)
{
APL_Integer result = 0;
   if      (b > 0)   result =  1;
   else if (b < 0)   result = -1;
   return IntCell::zI(Z, result);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_exponential_i(Cell * Z, APL_Integer b)
{
const APL_Float z = exp(APL_Float(b));
   if (!isfinite(z))   return E_DOMAIN_ERROR;   // ⋆1000 gave ∞
   return FloatCell::zF(Z, z);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_factorial_i(Cell * Z, APL_Integer b)
{
   if (b < 0)                    return E_DOMAIN_ERROR;
   if (b < int_fact_max)       return IntCell::zI(Z, int_factorials[b]);
   if (b < float_fact_max)     return FloatCell::zF(Z, float_factorials[b]);
   return E_DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_magnitude_i(Cell * Z, APL_Integer b)
{
   if (b >= 0)   return IntCell::zI(Z, b);
   if (uint64_t(b) == 0x8000000000000000ULL)
      return FloatCell::zF(Z, -APL_Float(b));
   return IntCell::zI(Z, -b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_nat_log_i(Cell * Z, APL_Integer b)
{
   if (b == 0)   return E_DOMAIN_ERROR;
   if (b > 0)    return FloatCell::zF(Z, log(APL_Float(b)));
   return ComplexCell::zC(Z, log(APL_Complex(b, 0)));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_negative_i(Cell * Z, APL_Integer b)
{
   if (uint64_t(b) == 0x8000000000000000ULL)
      return FloatCell::zF(Z, -APL_Float(b));
   return IntCell::zI(Z, -b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_pi_times_i(Cell * Z, APL_Integer b)
{
   return FloatCell::zF(Z, M_PI * b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_pi_times_inverse_i(Cell * Z, APL_Integer b)
{
   return FloatCell::zF(Z, b / M_PI);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_reciprocal_i(Cell * Z, APL_Integer b)
{
   switch(b)
      {
        case  0: return E_DOMAIN_ERROR;
        case  1: return IntCell::z1(Z);
        case -1: return IntCell::z_1(Z);
      }
   return FloatCell::zF(Z, 1.0 / b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_roll_i(Cell * Z, APL_Integer b)
{
   if (b <= 0)   return E_DOMAIN_ERROR;
const uint64_t rnd = Workspace::get_RL(b);
   return IntCell::zI(Z, Workspace::get_IO() + APL_Integer(rnd % b));
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_near_int64_t_i(Cell * Z, APL_Integer b)
{
   return IntCell::zI(Z, b);
}
//────────────────────────────────────────────────────────────────────────────
ErrorCode
IntCell::bif_within_quad_CT_i(Cell * Z, APL_Integer b)
{
   return IntCell::zI(Z, b);
}
