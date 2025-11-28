/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2025  Dr. Jürgen Sauermann

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

#include "Common.hh"
#include "ComplexCell.hh"
#include "Quad_RVAL.hh"
#include "Value.hh"

size_t N;
vector<int> Quad_RVAL::desired_ranks;
Shape       Quad_RVAL::desired_shape;
vector<int> Quad_RVAL::desired_types;
int         Quad_RVAL::desired_maxdepth;
char        Quad_RVAL::state[256];
size_t      Quad_RVAL::N = 256;

const FunctionGroup::function_info Quad_RVAL::subfunction_infos[] =
{
#define rvaldef(N, fun, comm_2) { N, #fun, "", comm_2, -1 },
  rvaldef(0, state, "get (B=⍬) or set (B≠⍬) the random number generator state" )
  rvaldef(1, rank,  "set the rank of subsequently returned random values"      )
  rvaldef(2, shape, "set he shape of subsequently returned random values"      )
  rvaldef(3, type,  "set the data type of subsequently returned random values" )
  rvaldef(4, depth, "set the depth of subsequently returned random values"     )
};

Quad_RVAL  Quad_RVAL::fun;

// ⎕RVAL depends on glibc, so we use it only in development mode

#if HAVE_LIBC

//----------------------------------------------------------------------------
Quad_RVAL::Quad_RVAL()
   : QuadFunction(TOK_Quad_RVAL)
{
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕RVAL");

   N = 8;
   desired_maxdepth = 4;
   memset(state, 0, sizeof(state));
   initstate(1, state, N);

   while (desired_shape.get_rank() < MAX_RANK)
         desired_shape.add_shape_item(1);

   desired_ranks.push_back(0);

   desired_types.push_back(0);   // no chars
   desired_types.push_back(1);   // ints
   desired_types.push_back(0);   // no real
   desired_types.push_back(0);   // no complex
   desired_types.push_back(0);   // no nested
}
//----------------------------------------------------------------------------
Token
Quad_RVAL::eval_B(Value_P B) const
{
   if (B->is_str0())    return list_functions(CERR);
   if (B->is_zilde())   return list_mappings(CERR);
   if (B->is_empty())   DOMAIN_ERROR;
   if (B->get_rank() > 1)
      {
        MORE_ERROR() << "⎕RVAL B: expecting 1 < ⍴⍴B.";
        RANK_ERROR;
      }

   return Token(TOK_APL_VALUE1, do_eval_B(*B, 0));
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::do_eval_B(const Value & B, int depth) const
{
ShapeItem len_B = B.element_count();

   if (len_B > 4)
      {
        MORE_ERROR() << "monadic ⎕RVAL B expects at most 4 properties B←"
                        "(rank, (shape), (type), and maxdepth)";
        LENGTH_ERROR;
      }

   if (B.is_scalar())   len_B = 1;   // pretend that B is a vector

   // save properties so that we can restore them
   //
vector<int> old_desired_ranks    = desired_ranks;
Shape       old_desired_shape    = desired_shape;
vector<int> old_desired_types    = desired_types;
int         old_desired_maxdepth = desired_maxdepth;
bool need_restore = false;

   try {
         need_restore = true;   // since something was changed

         if (len_B >= 2)   // rank: scalar or enclosed vector (distribution)
            {
              const Cell & cell = B.get_cfirst();
              if (cell.is_pointer_cell())
                 {
                   do_eval_AB(1, *cell.get_pointer_value());
                 }
              else   // rank as scalar
                 {
                   Value_P rank = IntScalar(cell.get_int_value(), LOC);
                   do_eval_AB(1, *rank);
                 }
            }

         if (len_B >= 2)   // shape: scalar or enclosed vector
            {
              const Cell & cell = B.get_cravel(1);
              if (cell.is_pointer_cell())
                 {
                   do_eval_AB(2, *cell.get_pointer_value());
                 }
              else   // shape as scalar: Z← (rank⍴ec_B)⍴random_data
                 {
                   Value_P rank = IntScalar(cell.get_int_value(), LOC);
                   do_eval_AB(2, *rank);
                 }
            }

         if (len_B >= 3)   // type: always enclosed vector (distribution)
            do_eval_AB(3, *B.get_cravel(2).get_pointer_value());

         if (len_B >= 4)   // maxdepth: scalar or 1-element vector
            {
              const Cell & cell = B.get_cravel(3);
              if (cell.is_pointer_cell())   // maxdepth as 1-element vector
                 {
                   do_eval_AB(4, *cell.get_pointer_value());
                 }
              else   // maxdepth as scalar
                 {
                   Value_P rank = IntScalar(cell.get_int_value(), LOC);
                   do_eval_AB(4, *rank);
                 }
            }
       }
    catch (...)
      {
        desired_ranks = old_desired_ranks;
        desired_shape = old_desired_shape;
        desired_types = old_desired_types;
        desired_maxdepth = old_desired_maxdepth;
        throw;
      }

   Log(LOG_Quad_RVAL)
      {
        CERR << "⎕RVAL B:" << endl << "desired_ranks:   ";
        loop(r, desired_ranks.size())   CERR << " " << desired_ranks[r];
        CERR << endl << "desired_shape:    " << desired_shape << endl;
        CERR << "desired_types:   ";
        loop(t, desired_types.size())   CERR << " " << desired_types[t];
        CERR << endl << "desired_maxdepth: " << desired_maxdepth << endl;
      }

const sRank rank = choose_integer(desired_ranks);
Shape shape;
   for (sRank r = MAX_RANK - rank; r < MAX_RANK; ++r)
       {
         vector<int> vsh_r;
         vsh_r.push_back(desired_shape.get_shape_item(r));
         const int sh_r = choose_integer(vsh_r);
         shape.add_shape_item(sh_r);
       }

Value_P Z(shape, LOC);

const ShapeItem ec = Z->element_count();

   loop(z, ec)
      {
         int type_z;  do    { type_z = choose_integer(desired_types); }
                      while (depth == desired_maxdepth && type_z == 4);
         switch(type_z)
            {
               case 0:   random_character(*Z);          continue;
               case 1:   random_integer(*Z);            continue;
               case 2:   random_float(*Z);              continue;
               case 3:   random_complex(*Z);            continue;
               case 4:   random_nested(*Z, B, depth);   continue;
               default:  FIXME;
            }
      }

   if (need_restore)
      {
        desired_ranks    = old_desired_ranks;
        desired_shape    = old_desired_shape;
        desired_types    = old_desired_types;
        desired_maxdepth = old_desired_maxdepth;
      }

   if (ec == 0)   Z->set_proto_Int();

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Token
Quad_RVAL::eval_AB(Value_P A, Value_P B) const
{
const sAxis subfunction = value_to_subfun(*A);

Value_P Z = do_eval_AB(subfunction, *B);
   return Token(TOK_APL_VALUE1, Z);
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::do_eval_AB(int subfunction, const Value & B)
{
   switch(subfunction)
      {
        case 0: return generator_state(B);
        case 1: return result_rank(B);
        case 2: return result_shape(B);
        case 3: return result_type(B);
        case 4: return result_maxdepth(B);
      }

   fun.bad_subfun_number_ERROR(subfunction);
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::generator_state(const Value & B)
{
   // expect an empty, 8, 16, 32, 64, 128, or 256 byte integer vector
   //
   if (B.get_rank() != 1)   RANK_ERROR;

const ShapeItem new_N = B.element_count();
   if (new_N !=   0 && new_N !=   8 && new_N !=  32 &&
       new_N !=  64 && new_N != 128 && new_N != 256)
      {
        MORE_ERROR() << "bad new_N (aka. ⍴B) in generator_state()";
        LENGTH_ERROR;
      }

   // always return the previous state
   //
Value_P Z(N, LOC);
   loop(n, N)   Z->next_ravel_Int(state[n] & 0xFF);
   Z->check_value(LOC);

   if (new_N)   // set generator state
      {
        // make sure that all values are bytes
        //
        loop(b, N)
            {
              const APL_Integer byte = B.get_cravel(b).get_int_value();
              if ((byte < -256) || (byte >  255))
                 {
                   MORE_ERROR() << "Bad right argument B in 0 ⎕RVAL B,"
                                   "expecting bytes (integers -256...255)";
                   DOMAIN_ERROR;
                 }
            }
        N = new_N;
        loop(b, N)
            {
               state[b] = B.get_cravel(b).get_int_value();
            }
         setstate(state);
      }

   return Z;
}
//----------------------------------------------------------------------------
void Quad_RVAL::print_fun_syntax(ostream & out,
                                 const function_info & info) const
{
   out << "    ⎕RVAL[" << info.axis << "] B   ⍝ " << info.comment_fun << endl;
}
//----------------------------------------------------------------------------
void Quad_RVAL::print_map_syntax(ostream & out,
                                 const function_info & info) const
{
const UTF8_literal name = info.function_name;
const UCS_string blanks(max_function_name_length - name.get_char_count(),
                        UNI_SPACE);

   out << "   ⎕RVAL[" << info.axis << "]  ←→  ⎕RVAL['" << name << "']"
       << blanks << "  ←→  ⎕RVAL." << name << endl;
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::result_rank(const Value & B)
{
   // B is a single rank or a distribution of ranks 0, 1, ...
   if (B.get_rank() > 1)   RANK_ERROR;

   // result Z is the current rank
   //
Value_P Z(desired_ranks.size(), LOC);
   loop(z, desired_ranks.size())   Z->next_ravel_Int(desired_ranks[z]);
   Z->check_value(LOC);

   if (B.is_scalar())   // single rank (fixed or equal distribution)
      {
        ShapeItem rk = B.get_cfirst().get_int_value();

        if ((rk < -MAX_RANK) || (rk > MAX_RANK))
           {
             MORE_ERROR() << "a scalar right argument B of 1 ⎕RVAL B should "
                             "be an integer from ¯" << MAX_RANK << " to "
                          << MAX_RANK;
             DOMAIN_ERROR;
           }

         desired_ranks.clear();
         desired_ranks.push_back(rk);
      }
   else if (B.element_count())   // distribution of ranks
      {
        vector<int>new_ranks;
        loop(b, B.element_count())
            {
              const int rank_b = B.get_cravel(b).get_int_value();
              if (rank_b < 0)
                 {
                   MORE_ERROR() << "a vector right argument B of 1 ⎕RVAL B "
                              "should be a distribution (integers ≥ 0)";
                   DOMAIN_ERROR;
                 }
              new_ranks.push_back(rank_b);
            }

        desired_ranks = new_ranks;
      }

   Log(LOG_Quad_RVAL)
      {
        CERR << "set desired_ranks to";
        loop(j, desired_ranks.size())   CERR << " " << desired_ranks[j];
        CERR << endl;
      }

   return Z;   // previous rank
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::result_shape(const Value & B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

const ShapeItem len_B = B.element_count();
   if (len_B > MAX_RANK)   LENGTH_ERROR;   // to many shape items

   // result Z is the current shape limits
   //
Value_P Z(MAX_RANK, LOC);
   loop(r, MAX_RANK)
       Z->next_ravel_Int(desired_shape.get_shape_item(r));
   Z->check_value(LOC);

   if (len_B)   // len_B > 0: set the current shape
      {
        Shape new_shape;

        if (B.is_scalar())   // scalar-extend B
           {
              const APL_Integer len = B.get_cfirst().get_int_value();
              loop(b, MAX_RANK)   new_shape.add_shape_item(len);
           }
        else                 // vector B: prepend 1s as needed
           {
             // fill leading dimensions with 1
             //
             loop(b, MAX_RANK - len_B)   new_shape.add_shape_item(1);

             // fill lower dimensions with B
              loop(b, len_B)
                  new_shape.add_shape_item(B.get_cravel(b).get_int_value());
           }

        desired_shape = new_shape;
      }

   Log(LOG_Quad_RVAL)
      {
        CERR << "set desired_shape to";
        loop(j, desired_shape.get_rank())
            {
              CERR << " " << desired_shape.get_shape_item(j);
            }
        CERR << endl;
      }

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::result_type(const Value & B)
{
   // B is a distribution of cell types char (0), int (1), real (2),
   // complex (3), // or nested (4).

   if (!B.is_vector())
      {
        MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
        "should be a vector of up to 5 integers (the\nrelative "
        "probabilities for types CHAR INT REAL COMPLEX or NESTED respectively)";
          RANK_ERROR;
      }

const ShapeItem len_B = B.element_count();
   if (len_B > 5)
      {
        MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
        "should be a vector of up to 5 integers (the\nrelative "
        "probabilities for types CHAR INT REAL COMPLEX or NESTED respectively)";
          LENGTH_ERROR;
      }

   // result Z is the current types
   //
Value_P Z(desired_types.size(), LOC);
   loop(z, desired_types.size())   Z->next_ravel_Int(desired_types[z]);
   Z->check_value(LOC);

   if (len_B)   // len_B > 0: distribution of depths
      {
        vector<int>new_types;
        bool B_has_simple = false;
        loop(b, B.element_count())
            {
              const int type_b = B.get_cravel(b).get_int_value();
              if (type_b < 0)
                 {
                   MORE_ERROR() << "the right argument B of 3 ⎕RVAL B "
                   "should be a distribution (vector of integers ≥ 0)";
                   DOMAIN_ERROR;
                 }
              if (type_b && b < 4)   B_has_simple = true;
              new_types.push_back(type_b);
            }

        // if all simple types had a probability of 0 then random_nested()
        // would loop forever, attempting to choose one. Do not allow that.
        //
        if (!B_has_simple)
           {
             MORE_ERROR() << "the right argument B of 3 ⎕RVAL B should contain "
                   "at least one simple type with a non-zero probability";
             DOMAIN_ERROR;
           }

        while (new_types.size() < 5)    new_types.push_back(0);   // make 5=⍴Z
        desired_types = new_types;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_types to";
             const char * types[] = { "char", "int", "float",
                                      "complex", "nested" };
             loop(j, desired_types.size())
                 {
                   CERR << " " << types[j] << "=" << desired_types[j];
                 }
             CERR << endl;
           }
      }

   return Z;
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::result_maxdepth(const Value & B)
{
   if (B.get_rank() > 1)        RANK_ERROR;
   if (B.element_count() > 1)   LENGTH_ERROR;

   // result Z is the current max. depth
   //
Value_P Z = IntScalar(desired_maxdepth, LOC);

   if (B.element_count())   // set the desired maxdepth
      {
        const APL_Integer mxd = B.get_cfirst().get_int_value();
        if (mxd < 0)
           {
             MORE_ERROR() << "bad max.depth";
             DOMAIN_ERROR;
           }
        desired_maxdepth = mxd;

        Log(LOG_Quad_RVAL)
           {
             CERR << "set desired_maxdepth to" << desired_maxdepth << endl;
           }
      }

   return Z;   // previous desired_maxdepth
}
//----------------------------------------------------------------------------
int
Quad_RVAL::choose_integer(const vector<int> & dist)
{
const int dist_len = dist.size();
   Assert(dist_len > 0);

   /* a distribution with a single value N shall mean:

       N > 0: fixed value N
       N < 0: equal distribution 0..N-1
    */
    if (dist_len == 1)   // fixed value or equal distribution 0, 1, ... n
      {
        const int desired = dist[0];
        if (desired >= 0)   return desired;   // fixed value
        const int rand = rand17();
        return rand % (1 - desired);          // = 0... desired
      }

   // dist is a distribution...

   // 1. compute the sum of the weights.
   //    The weights should be reasonably small (sum ≤ 0xFFFF).
   //
int sum = 0;
   for (size_t d = 0; d < dist.size(); ++d)   sum += dist[d];

   // 2. pick a random number 0...sum
   //
const int rand = (rand17() & 0xFFFF) % sum;

   // 3. return the index of rand in +\dist
   //
   sum = 0;
   for (size_t d = 0; d < dist.size(); ++d)
       {
          sum += dist[d];
          if (rand < sum)   return d;
       }

   FIXME;   // not reached
}
//----------------------------------------------------------------------------
void
Quad_RVAL::random_character(Value & Z)
{
const int32_t rnd = rand17();
   Z.next_ravel_Char(Unicode((rnd & 0x1FFF) + ((rnd & 0x2000) >> 1)));
}
//----------------------------------------------------------------------------
void
Quad_RVAL::random_integer(Value & Z)
{
const int64_t rnd = rand17()
                  ^ (rand17() << 16)
                  ^ (rand17() << 32)
                  ^ (rand17() << 48);
   Z.next_ravel_Int(rnd);
}
//----------------------------------------------------------------------------
double
Quad_RVAL::random_ieee()
{
union { double f;
        char bytes[8];
      } u;
   do {
        const int64_t rand64 = rand17()
                             ^ (rand17() << 16)
                             ^ (rand17() << 32)
                             ^ (rand17() << 48);
        
        enum { BIAS     = 1023,          // IEEE
               EXPO     = 0 + BIAS,      // 2^0
               EXPO_LSB = EXPO & 0x0F,   // 0..15
               EXPO_MSB = EXPO >> 4      // 0..15
             };

        u.bytes[7] = EXPO_MSB;                // SIGN + and exponent MSBs
        u.bytes[6] = (EXPO_LSB << 4)          // exponent LSBs
                   | (rand64 >> 56 & 0x0F);   // 52 bit fraction MSBs
        u.bytes[5] = rand64 >> 40;            // 52 bit fraction
        u.bytes[4] = rand64 >> 32;            // 52 bit fraction
        u.bytes[3] = rand64 >> 24;            // 52 bit fraction
        u.bytes[2] = rand64 >> 16;            // 52 bit fraction
        u.bytes[1] = rand64 >>  8;            // 52 bit fraction
        u.bytes[0] = rand64 >>  0;            // 52 bit fraction LSBs
      } while (!isnormal(u.f));

   // at this point: 1.0 < u.f < 2.0
   //
   return u.f - 1.0;
}
//----------------------------------------------------------------------------
void
Quad_RVAL::random_float(Value & Z)
{
   Z.next_ravel_Float(random_ieee());
}
//----------------------------------------------------------------------------
void
Quad_RVAL::random_complex(Value & Z)
{
   Z.next_ravel_Complex(random_ieee(), random_ieee());
}
//----------------------------------------------------------------------------
void
Quad_RVAL::random_nested(Value & Z, const Value & B, int depth) const
{
Value_P Zsub;

   do Zsub = do_eval_B(B, depth + 1);
   while (Zsub->is_simple_scalar());

   Z.next_ravel_Pointer(Zsub.get());
}
//----------------------------------------------------------------------------
uint64_t
Quad_RVAL::rand17()
{
const int32_t rnd = random();

   // the lower bits are less random, so we xor the upper 16 bits into
   // the lower 16 bits and return them.
   return (rnd ^ (rnd >> 16)) & 0x1FFFF;
}
//----------------------------------------------------------------------------

#else    // not HAVE_LIBC

//----------------------------------------------------------------------------
Quad_RVAL::Quad_RVAL()
   : QuadFunction(TOK_Quad_RVAL)
{
   N = 8;
}
//----------------------------------------------------------------------------
Value_P
Quad_RVAL::do_eval_B(const Value & B, int depth) const
{
    MORE_ERROR() <<
"⎕RVAL is only available on platforms that have glibc.\n"
"Your platform is lacking initstate_r() (and probably others).";

   SYNTAX_ERROR;
   return Value_P();
}
//----------------------------------------------------------------------------
Token
Quad_RVAL::eval_AB(Value_P A, Value_P B) const
{
    MORE_ERROR() <<
"⎕RVAL is only available on platforms that have glibc.\n"
"Your platform is lacking initstate_r() (and probably others).";

   SYNTAX_ERROR;
   return Token();
}
//----------------------------------------------------------------------------
Token
Quad_RVAL::eval_XB(Value_P A, Value_P B) const
{
    MORE_ERROR() <<
"⎕RVAL is only available on platforms that have glibc.\n"
"Your platform is lacking initstate_r() (and probably others).";

   SYNTAX_ERROR;
   return Token();
}
//----------------------------------------------------------------------------
void
Quad_RVAL::print_fun_syntax(ostream & out,
                            const FunctionGroup::function_info & info) const
{
}
//----------------------------------------------------------------------------
void
Quad_RVAL::print_map_syntax(ostream & out,
                            const FunctionGroup::function_info & info) const
{
}
//----------------------------------------------------------------------------
#endif   // HAVE_LIBC
