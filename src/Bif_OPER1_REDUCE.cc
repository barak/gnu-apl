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

#include "ArgCheck.hh"
#include "Bif_F12_RHO.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Workspace.hh"

Bif_OPER1_REDUCE    Bif_OPER1_REDUCE ::fun;
Bif_OPER1_REDUCE1   Bif_OPER1_REDUCE1::fun;

//════════════════════════════════════════════════════════════════════════════
Token
Bif_REDUCE::do_reduce(const Shape & shape_Z, const Shape3 & Z3, ShapeItem nwise,
                      cFunction_P LO, Value_P B, ShapeItem bm)
{
Value_P Z(shape_Z, LOC);

   // constants that are valid for the entire reduction
   //
const ShapeItem len_Z   = Z->element_count();
const ShapeItem len_L   = Z3.get_last_shape_item();
const ShapeItem len_BML = bm * len_L;
const ShapeItem len_ZM  = Z3.m();
const ShapeItem len_ZML = len_L * len_ZM;
prim_f2 scalar_LO       = LO->get_scalar_f2();

   loop(z, len_Z)
      {
        // break down z into H, M, and L components
        //
        const ShapeItem z_L =  z % len_L;
        const ShapeItem z_M = (z / len_L) % len_ZM;
        const ShapeItem z_H =  z / len_ZML;

        // compute LO_count (= the number of LO-reductions), and b (= the
        // starting point of the LO-reductions). They depend on z and nwise
        //
        const ShapeItem LO_count = (nwise == -1) ? z_M
                                 : (nwise < -1)  ? - nwise - 1
                                 :                 nwise - 1;

        ShapeItem b = z_L + z_H * len_BML       // start of row in B
                          + LO_count * len_L;   // end of beam in B

        // for reverse (i,e. nwise < -1) reduction direction go to the
        // start of the beam instead of the end of the beam.
        //
        if (nwise < -1)    b += (nwise + 1) * len_L;
        if (nwise != -1)   b += z_M * len_L; // start of beam

        // Z[z] = B[b]. We use Z[z] as accumulator for the reduction
        //
        Cell & accu = Z->get_wravel(z);
        {
          Cell cache;
          // accu is freshly-obtained ravel storage (Z->get_wravel(z),
          // above), not yet a live Cell -- init_other(), called on the
          // live source, is the correct direction, not a non-static
          // member call on accu (Blake McBride, Bugs15 #12, UBSan-
          // confirmed via n-wise reduce/overtaking take).
          //
          B->get_cravel(b, cache).init_other(&accu, *Z, LOC);
        }

        loop(lo, LO_count)
           {
             if (nwise < -1)   b += len_L;   // move forward in B
             else              b -= len_L;   // move backward in B

             // one reduction step (one call of LO)
             //
             Cell cache;
             const Cell & cB = B->get_cravel(b, cache);
             if (scalar_LO && accu.is_simple_cell() && cB.is_simple_cell())
                {
                  const ErrorCode ec = (accu.*scalar_LO)(&accu, &cB);
                  if (ec)   throw_apl_error(ec, LOC);
                }
             else
                {
                  Value_P LO_A = cB.to_value(LOC);
                  Value_P LO_B = accu.to_value(LOC);
                  Token result = LO->eval_AB(*LO_A, *LO_B);
                  accu.release(LOC);

                  if (result.get_tag() == TOK_ERROR)
                    throw_apl_error(result.get_ErrorCode(), LOC);

                  Assert(result.get_Class() == TC_VALUE);
                  Value_P ZZ = result.get_apl_val();

                  // the item of Z is ZZ itself: a nested SCALAR ZZ (e.g.
                  // 2,¨3 is ⊂2 3) must be kept enclosed, not stored as its
                  // own PointerCell like init_from_value() does, which
                  // to_value() above then discloses (Bugs35 #45)
                  //
                  if (ZZ->is_scalar() && !ZZ->is_simple_scalar())
                     {
                       Z->invalidate_depth();
                       new (&accu) PointerCell(ZZ.get(), *Z);
                     }
                  else
                     {
                       accu.init_from_value(ZZ.get(), *Z, LOC);
                     }
                }
           }
      }

   Z->set_default(*B.get(), LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_REDUCE::reduce(Token & tok_LO, Value_P B, uAxis axis)
{
   // if B is a scalar, then Z is B.
   //
   if (B->get_rank() == 0)      return Token(TOK_APL_VALUE1, CLONE_P(B, LOC));

   if (!tok_LO.is_function())
      {
        MORE_ERROR() << "f/ B resp. f⌿ B : Bad left argument f"
                        " (expecting a function)";
        DOMAIN_ERROR;
      }

cFunction_P LO = tok_LO.get_function();
   Assert1(LO);
   if (!LO->has_result())
      {
        MORE_ERROR() << LO->get_name() << "/ B resp. "
                     << LO->get_name() << "⌿ B : Bad function "
                     << LO->get_name() << " (expecting a result)";
        DOMAIN_ERROR;
      }

   if (LO->get_fun_valence() != 2)
      {
        MORE_ERROR() << LO->get_name() << "/ B resp. "
                     << LO->get_name() << "⌿ B : Bad function "
                     << LO->get_name() << " (expecting a dyadic function)";
        VALENCE_ERROR;   // like A F/ B (Bugs37 #67)
      }

   if (axis >= B->get_rank())
      {
        MORE_ERROR() << "f/B resp. f⌿B: axis " << axis
                     << " is not a valid axis of B; ⍴⍴B is "
                     << B->get_rank();
        AXIS_ERROR;
      }

const ShapeItem m_len = B->get_shape_item(axis);
   if (m_len == 0)   // apply the identity function
      {
        /*
           Theory:   A)   f/B₁ B₂ ... Bₙ   ←→    (f/B₁, B₂, ... Bₙ₋₁) f Bₙ
           -------   B)   f/B₁   ←→    B₁

                             A)          B)
            therefore:  B₁   ←→   f/B₁   ←→   B₀ f B₁
                        if there is such a B₀ (i.e. with B₀ f B₁   ←→    B₁).
         */
        return LO->eval_identity_fun(*B, axis);
      }

const Shape shape_Z = B->get_shape().without_axis(axis);
   if (m_len == 1)   return Bif_F12_RHO::do_reshape(shape_Z, *B);

   // non-trivial reduce (len > 1)
   //
const Shape3 B3(B->get_shape(), axis);

   // shape_Z can still be empty here even though m_len (the reduced
   // axis) is not: some OTHER axis of B is 0. This must be checked
   // BEFORE the macro dispatch just below, not after: MAC_Z__LO_
   // REDUCE_X4_B does (μ9 μ12)←⊃μ14[μ13] on an empty μ14 in that case
   // -- INDEX ERROR, reachable from ordinary APL input (e.g.
   // {⍺+⍵}/0 3⍴0), confirmed live, where the primitive-LO path just
   // below (do_reduce()) already handles the same shape correctly.
   // Mirrors Bif_SCAN::scan()'s identical ordering/reasoning for its
   // own macro dispatch.
   //
   if (shape_Z.is_empty())
      {
        Value_P Z(shape_Z, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (LO->may_push_SI())   // user defined LO
      {
        Value_P X4(4, LOC);
        X4->next_ravel_Int(axis + Workspace::get_IO());
        X4->next_ravel_Int(B3.h());
        X4->next_ravel_Int(B3.m());
        X4->next_ravel_Int(B3.l());
        X4->check_value(LOC);
        return Macro::get_macro(Macro::MAC_Z__LO_REDUCE_X4_B)
                                ->eval_LXB(tok_LO, *X4, *B);
      }

const Shape3 Z3(B3.h(), 1, B3.l());
   return do_reduce(shape_Z, Z3, B3.m(), LO, B, B->get_shape_item(axis));
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_REDUCE::reduce_n_wise(Value_P A, Token & tok_LO,
                          Value_P B, uAxis axis) const
{
   if (!tok_LO.is_function())
      {
        MORE_ERROR() << "A f/ B resp. A f⌿ B : Bad function argument f"
                        " (expecting a function)";
        DOMAIN_ERROR;
      }

cFunction_P LO = tok_LO.get_function();
   if (!LO->has_result())
      {
        MORE_ERROR() << "A " << LO->get_name() << get_name() << " B : Bad"
                        " function " << LO->get_name()
                     << " (expecting a result)";
        DOMAIN_ERROR;
      }

   if (LO->get_fun_valence() != 2)
      {
        MORE_ERROR() << "A " << LO->get_name() << get_name() << " B : Bad"
                        " function " << LO->get_name()
                     << " (expecting a dyadic function)";
        VALENCE_ERROR;
      }

   if (A->element_count() != 1)
      {
        MORE_ERROR() << "A f/B resp. A f⌿B: A must be a scalar or 1-element"
                        " vector (the n in n-wise reduce); A has "
                     << A->element_count() << " items";
        LENGTH_ERROR;
      }
const APL_Integer A0 = A->get_near_int(0);

   // the number of items (= M1 in ISO). Was 'const int n_wise = ...': A0 is
   // a full APL_Integer (64 bit), so a huge A (e.g. left argument of A f/B
   // with |A| far beyond B's own axis length) silently truncated down to
   // whatever 32 bits n_wise happened to land on -- occasionally even
   // negative, which then sailed past the "n_wise > B's axis length"
   // DOMAIN ERROR check below (a negative number is never > a small
   // positive one), while do_reduce() further down still received the
   // untruncated, huge, unchecked A0 and used it to compute an out-of-
   // bounds ravel offset into B -- a real SEGV, found via the fuzzer.
   // n_wise must stay the same width as A0 for that check to mean
   // anything, and the negation itself needs its own overflow guard
   // (negating APL_Integer's own most negative value overflows the same
   // way).
const APL_Integer neg_A0 = -A0;
   if (A0 < 0 && Cell::diff_overflow(neg_A0, 0, A0))
      {
        MORE_ERROR() << "A f/B resp. A f⌿B: A = " << A0
                     << " overflows on negation";
        DOMAIN_ERROR;
      }
const APL_Integer n_wise = A0 < 0 ? neg_A0 : A0;

   if (B->is_scalar())
      {
        if (n_wise > 2)
           {
             MORE_ERROR() << "A f/B resp. A f⌿B: B is a scalar; expecting"
                             " ∣A∣≤2; A is " << A0;
             DOMAIN_ERROR;
           }
        if (n_wise == 0)
           {
              // eval_identity_fun() (via NonscalarFunction_default_
              // identity::enclosed_identity(), PrimitiveFunction.hh)
              // documents and relies on B being genuinely non-scalar --
              // it does Z->next_ravel_Pointer(CLONE(&B, LOC).get()),
              // which asserts on a simple scalar. The rank>=1 branch
              // further down honours this by wrapping B in a length-0
              // axis before calling eval_identity_fun(); do the same
              // here instead of passing the bare scalar B (Blake
              // McBride, Bugs25 #7).
              //
              Shape shape_B1;
              shape_B1.add_shape_item(0);
              Value_P val(shape_B1, LOC);
              Cell cache;
              // set_default(), NOT get_cproto(): B is a real (non-empty)
              // scalar here, so get_cproto()/get_cravel(0,...) would
              // return B's actual VALUE (e.g. 0⌽/5 wrongly seeding the
              // identity with 5) -- set_default() derives a type-correct
              // default cell (0 for numeric, space for char, ...) from
              // B via Cell::init_type_at(), ignoring B's value entirely,
              // which is what an identity element actually needs. See
              // Bugs27 #27 (0⌽/⍳5-style residual).
              val->set_default(*B, LOC);
              Token ident = LO->eval_identity_fun(*val, 0);
              Value_P Z(2, LOC);
              Z->next_ravel_Cell(ident.get_apl_val()->get_cfirst(cache));
              Z->next_ravel_Cell(ident.get_apl_val()->get_cfirst(cache));
              Z->check_value(LOC);
              return Token(TOK_APL_VALUE1, Z);
           }

        // n_wise is 1 or 2: return (2 - n_wise) ⍴ B
        //
        // NOTE (investigated, reverted): the comment says "(2-n_wise)⍴B",
        // which for n_wise==1 would be a length-1 VECTOR -- but making
        // that change breaks "reduce with reduction"
        // (testcases/Reduce.tc's "+//10 10⍴1" -- lrm p.209's derived-
        // function-as-reduce-operand case): folding a matrix with (+/)
        // as the dyadic operand combines pairs of scalar CELLS via this
        // exact scalar-B, n_wise==1 code path on every fold step, and a
        // length-1-vector result there makes every fold step nest one
        // level deeper instead of staying scalar, turning a simple
        // result into a runaway-nested one (confirmed live: ≡ went from
        // 1 to 2). Keeping the scalar collapse for n_wise==1 is required
        // for that to terminate flat, so this is intentional, not a bug
        // -- left as the original code despite the comment being
        // literally imprecise about the n_wise==1 case.
        //
        Shape sh;
        if (n_wise == 2)   sh.add_shape_item(0);
        return Bif_F12_RHO::do_reshape(sh, *B);
      }
   else
      {
        if (n_wise > (1 + B->get_shape_item(axis)))
           {
             MORE_ERROR() << "A f/B resp. A f⌿B: expecting ∣A∣≤"
                          << (1 + B->get_shape_item(axis)) << " (1+(⍴B)["
                          << (axis + Workspace::get_IO()) << "]); A is "
                          << A0;
             DOMAIN_ERROR;
           }
      }

   Assert1(LO);

   if (B->get_rank() == 0)      return Token(TOK_APL_VALUE1, CLONE_P(B, LOC));

   if (axis >= B->get_rank())
      {
        MORE_ERROR() << "A f/B resp. A f⌿B: axis " << axis
                     << " is not a valid axis of B; ⍴⍴B is "
                     << B->get_rank();
        AXIS_ERROR;
      }

   if (n_wise == 0)   // apply the identity function
      {
        Shape shape_B1 = B->get_shape().insert_axis(axis, 0);
        shape_B1.increment_shape_item(axis + 1);
        Value_P val(shape_B1, LOC);
        // set_default(), NOT get_cproto(): B is genuinely non-empty
        // here (e.g. 0⌽/⍳5), so get_cproto()/get_cravel(0,...) would
        // return B's actual first ELEMENT (1, under ⎕IO←1) instead of
        // a true identity/default value -- set_default() derives a
        // type-correct default cell (0 for numeric, space for char,
        // ...) from B via Cell::init_type_at(), ignoring B's actual
        // content, matching what eval_identity_fun() already does for
        // a genuinely empty B (e.g. ⌽/3 0⍴0). See Bugs27 #27 residual.
        val->set_default(*B, LOC);

        Token result = LO->eval_identity_fun(*val, axis);
        return result;
      }

   if (n_wise == (1 + B->get_shape_item(axis)))   // empty result
      {
        // the examples in ISO/IEC 13751 (2000) p. 116, as well as lrm,
        // suggest an empty result, while the algorithm on pp. 115/116 seems
        // not to work for n_wise == (1 + B->get_shape_item(axis)).
        // We follow the examples and lrm.
        //
        Shape shape_Z = B->get_shape();
        shape_Z.set_shape_item(axis, 0);
        Value_P Z(shape_Z, LOC);
        Z->set_default(*B, LOC);   // see Bugs27 #27 residual, above
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

Shape shape_Z(B->get_shape());
   shape_Z.set_shape_item(axis, shape_Z.get_shape_item(axis) - n_wise + 1);
   if (shape_Z.is_empty())
      {
        // eval_identity_fun() removes the reduction axis and therefore
        // is only valid when B's axis itself has length 0. If some
        // *other* axis of B is empty instead, the windowed axis length
        // computed above (shape_Z) is still correct and must be kept.
        if (B->get_shape_item(axis) == 0)   return LO->eval_identity_fun(*B, axis);

        Value_P Z(shape_Z, LOC);
        Z->set_default(*B, LOC);
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (n_wise == 1)   return Bif_F12_RHO::do_reshape(shape_Z, *B);

const Shape3 Z3(shape_Z, axis);
const Shape3 B3(B->get_shape(), axis);
   if (LO->may_push_SI())   // user defined LO
      {
        Value_P A1 = IntScalar(A0 < 0 ?  A0 + 1 : 1 - A0, LOC);
        Value_P vsh_Z(LOC, &shape_Z);
        Value_P vsh_Z3(LOC, &Z3);
        Value_P vsh_B3(LOC, &B3);
        Value_P X4(4, LOC);
        X4->next_ravel_Int(axis + Workspace::get_IO());   // X
        X4->next_ravel_Pointer(vsh_Z.get());              // ⍴Z
        X4->next_ravel_Pointer(vsh_Z3.get());             // ⍴Z3
        X4->next_ravel_Pointer(vsh_B3.get());             // ⍴B3
        X4->check_value(LOC);
        if (A0 < 0)   return Macro::get_macro(Macro::MAC_Z__nA_LO_REDUCE_X4_B)
                                              ->eval_ALXB(*A1, tok_LO, *X4, *B);
        else         return Macro::get_macro(Macro::MAC_Z__pA_LO_REDUCE_X4_B)
                                              ->eval_ALXB(*A1, tok_LO, *X4, *B);
      }

   return do_reduce(shape_Z, Z3, A0, LO, B, B->get_shape_item(axis));
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_REDUCE::replicate(cValue_R A, cValue_R B, uAxis axis)
{
   // turn scalar B into ,B
   //
Shape shape_B = B.get_shape();
   if (shape_B.get_rank() == 0)
      {
         shape_B.add_shape_item(1);
         axis = 0;
      }

   // LRM: a scalar or one-item vector A is extended along the axis.
   // GNU APL: any 1-element A (Bugs37 #66 scalar-extension rule).
   //
   if (!A.is_scalar_extensible())
      ArgCheck::require_scalar_or_vector("A/B resp. A⌿B", "A", A);
   if (axis >= shape_B.get_rank())
      {
        MORE_ERROR() << "A/B resp. A⌿B: axis " << axis
                     << " is not a valid axis of B; ⍴⍴B is "
                     << shape_B.get_rank();
        AXIS_ERROR;
      }

const ShapeItem len_B = shape_B.get_shape_item(axis);
ShapeItem len_A = A.element_count();

   // compute len_Z ← +/A
   //
ShapeItem len_Z = 0;
std::vector<ShapeItem> rep_counts;
   rep_counts.reserve(len_B);
   if (len_A == 1)   // single a -> a a ... a (len_B times)
      {
        len_A = len_B;
        APL_Integer rep_A = A.get_near_int(0);
        loop(a, len_A)   rep_counts.push_back(rep_A);
        // -rep_A is UB for rep_A == INT64_MIN, and would overflow anyway.
        if (rep_A == INT64_MIN || Cell::prod_overflow(rep_A, len_B))
           WS_FULL;
        if (rep_A < 0)   len_Z = -rep_A*len_B;   // replicate ↑B
        else             len_Z =  rep_A*len_B;   // replicat B[a]
      }
   else              // normal A
      {
        ShapeItem nonneg_A = 0;   // number of items >= 0 in A
        loop(a, len_A)
           {
             const APL_Integer rep_A = A.get_near_int(a);
             rep_counts.push_back(rep_A);

             // amount by which len_Z grows: rep_A (if >= 0) or -rep_A
             APL_Integer grow;
             if (rep_A < 0)
                {
                  if (rep_A == INT64_MIN)   WS_FULL;   // -rep_A is UB
                  grow = -rep_A;
                }
             else
                {
                  grow = rep_A;
                  ++nonneg_A;
                }

             // Cell::sum_overflow() only detects an overflow that has
             // already happened -- computing len_Z + grow via plain +
             // when it overflows is itself undefined behaviour (signed
             // integer overflow), and this check was getting optimised
             // away as unreachable on that assumption, exactly the
             // pattern the comment at Bif_F12_INTERVAL_INDEX.cc:194-206
             // warns not to rely on. Check first, using the same
             // double-precision pre-check Cell::prod_overflow() already
             // uses for the analogous multiplication case 26 lines
             // above, so the actual (potentially overflowing) + is never
             // evaluated at all when it would overflow.
             //
             if (double(len_Z) + double(grow) > double(LARGE_INT))   WS_FULL;
             len_Z += grow;
           }

        // the B axis shall have an item for every non-negative A
        if (len_B != 1 && nonneg_A != len_B)
           {
             MORE_ERROR() << "A/B resp. A⌿B: expecting ⍴A = 1 or "
                          << len_B << " (the length of B along axis "
                          << (axis + Workspace::get_IO()) << "); ⍴A is "
                          << len_A;
             LENGTH_ERROR;
           }
      }

Shape shape_Z(shape_B);
   shape_Z.set_shape_item(axis, len_Z);

Value_P Z(shape_Z, LOC);

const Shape3 shape_B3(shape_B, axis);

   loop(h, shape_B3.h())
      {
        ShapeItem bm = 0;
        loop(m, rep_counts.size())
           {
             const ShapeItem rep = rep_counts[m];

             if (rep >= 0)   // copy l*rep items
                {
                  loop(r, rep)
                  loop(l, shape_B3.l())
                     {
                       const ShapeItem src = shape_B3.hml(h, bm, l);
                       Cell cache;
                       Z->next_ravel_Cell(B.get_cravel(src, cache));
                     }
                  if (shape_B3.m() > 1)   ++bm;
                }
             else  // init l*-rep items with the fill item
                {
                  if (shape_B3.m() == 0)
                     {
                       // B has no row at all along the replicate axis
                       // (e.g. ¯1 ¯1 ⌿ (0 3⍴7)), so there is no real B
                       // item at row 0 to copy the type from -- use B's
                       // prototype cell instead (confirmed: without this,
                       // shape_B3.hml(h,0,l) reads an out-of-bounds row
                       // and crashes).
                       Cell cache;
                       const Cell & proto = B.get_cproto(cache);
                       loop(r, -rep)
                       loop(l, shape_B3.l())
                          Z->next_ravel_Proto(proto);
                     }
                  else
                     {
                       loop(r, -rep)
                       loop(l, shape_B3.l())
                          {
                            const ShapeItem src = shape_B3.hml(h, 0, l);
                            Cell cache;
                            Z->next_ravel_Proto(B.get_cravel(src, cache));
                          }
                     }

                  // cB is not incremented when fill item is used.
                }
           }
      }

   Z->set_default(B, LOC);
   if (B.is_left_value())   Z->set_left_value();
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE::eval_ALXB(cValue_R A, Token & _LO, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A F/[X]B");
   return reduce_n_wise(CLONE(&A, LOC), _LO, CLONE(&B, LOC), axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A/[X]B");
   return replicate(A, B, axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE::eval_LXB(Token & _LO, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "F/[X]B");
   return reduce(_LO, CLONE(&B, LOC), axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE1::eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A F⌿[X]B");
   return reduce_n_wise(CLONE(&A, LOC), LO, CLONE(&B, LOC), axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE1::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "A⌿[X]B");

   return replicate(A, B, axis);
}
//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_REDUCE1::eval_LXB(Token & LO, cValue_R X, cValue_R B) const
{
const sAxis axis = Value::get_single_axis(&X, B.get_rank(), "F⌿[X]B");
   return reduce(LO, CLONE(&B, LOC), axis);
}
//════════════════════════════════════════════════════════════════════════════
