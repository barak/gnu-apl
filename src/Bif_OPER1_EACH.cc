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

#include "Bif_F12_DOMINO.hh"
#include "Bif_F12_TAKE_DROP.hh"
#include "Bif_F1_EXECUTE.hh"
#include "Bif_OPER1_EACH.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "UserFunction.hh"
#include "Workspace.hh"

Bif_OPER1_EACH Bif_OPER1_EACH::fun;

//════════════════════════════════════════════════════════════════════════════
Token
Bif_OPER1_EACH::eval_ALB(cValue_R A, Token & _LO, cValue_R B) const
{
   // dyadic EACH: call _LO for corresponding items of A and B

   if (!A.same_shape(B))
      {
        // if the shapes differ then either A or B must be a 1-element
        // value of any rank (scalar extension as for the scalar functions,
        // see Value::is_scalar_extensible(), for every LO, Bugs37 #66).
        //
        if (A.get_rank() != B.get_rank())
           {
             if      (A.is_scalar_extensible())    ;   // OK
             else if (B.is_scalar_extensible())    ;   // OK
             else if (A.get_rank() != B.get_rank())
                {
                  MORE_ERROR() << "A f¨B: expecting ⍴⍴A = ⍴⍴B (or A or B with"
                                  " 1 element); ⍴⍴A is "
                               << A.get_rank() << ", ⍴⍴B is " << B.get_rank();
                  RANK_ERROR;
                }
             else
                {
                  MORE_ERROR() << "A f¨B: expecting ⍴A = ⍴B (or A or B with"
                                  " 1 element); ⍴A is "
                               << A.get_shape() << ", ⍴B is " << B.get_shape();
                  LENGTH_ERROR;
                }
           }
      }

cFunction_P LO = _LO.get_function();
   Assert1(LO);

   // for the ambiguous operators /. ⌿, \, and ⍀ is_operator() returns true,
   // which is incorrect in this context. We use get_signature() instead.
   //
   if ((LO->get_signature() & SIG_DYA) != SIG_DYA)   VALENCE_ERROR;

   // B is a selective-assignment target (an lvalue/cellrefs array) iff
   // its VF_Flags::left_value flag is set (Value::get_cellrefs() sets it
   // on every value it builds). Do NOT infer this from a cell's type/
   // position instead (e.g. B.is_lval_cell(0)): that is only reliable
   // for a value fresh out of get_cellrefs(), whose ravel is uniformly
   // LvalCells, and silently wrong for one built by other means -- e.g.
   // a nested each's own Z, which mixes plain Cells (next_ravel_Cell(),
   // for a scalar sub-result) and PointerCells (next_ravel_Pointer(),
   // for a non-scalar one) positionally, so cell 0 need not represent
   // the rest of the array. LO must then genuinely select/overtake B
   // (Function::get_selectivity(), SEL_DYA or SEL_DYA_X -- Each does
   // not itself distinguish whether LO carries a bracket axis, so both
   // bits qualify here) and have a result: a function that merely
   // computes a new value from B's content -- whether primitive (e.g.
   // scalar +, whose own evaluation cannot even interpret a raw
   // LvalCell) or defined (which could inspect, store, or leak the live
   // LvalCell into unrelated storage via an ordinary side effect, later
   // dangling once the array it points into is freed) -- must never
   // reach the primitive fast path below or any eval_AB call with an
   // unresolved LvalCell argument. Neither is_defined() nor
   // may_push_SI() is the right test here: a primitive can be just as
   // non-selecting as a defined function (+), and every defined
   // function is non-selecting regardless of what it happens to do
   // (get_selectivity() is never overridden for one).
   //
   if (!B.is_empty() && B.is_left_value() && LO->has_dyadic_form() &&
       (!LO->has_result() || !(LO->get_selectivity() & (SEL_DYA | SEL_DYA_X))))
      {
        // Bugs31 #29 (Blake McBride): this guard rejects the whole
        // selective specification, so B's LvalCells will never be
        // consumed by assign_cellrefs() for their original purpose --
        // same leak Bugs30 #28 fixed for assign_cellrefs()'s own
        // LENGTH_ERROR branches, just one step earlier (this guard runs
        // before assign_cellrefs() is ever reached at all).
        //
        const_cast<Value &>(static_cast<const Value &>(B))
           .neutralize_lval_cells(LOC);
        SYNTAX_ERROR;
      }

   if (A.is_empty() || B.is_empty())
      {
        if (!LO->has_result())   return Token(TOK_VOID);

        Value_P first_A = Bif_F12_TAKE::first(A);
        Value_P first_B = Bif_F12_TAKE::first(B);
        Shape shape_Z;   // will be ⍴A or ⍴B and therefore empty

        // is_scalar_extensible(), not is_scalar(): the general
        // (non-empty) conformance test a few lines above already accepts
        // a 1-element vector partner for scalar-extension, but this
        // empty-arg branch required a strict scalar -- ⍬+¨,5 gave
        // DOMAIN_ERROR (B=,5 is a 1-element vector, not scalar) though
        // the same B would extend fine against any non-empty A.
        if (A.is_empty())          shape_Z = A.get_shape();
        else if (!A.is_scalar_extensible())
           {
             MORE_ERROR() << "A f¨B: B is empty and A has more than 1"
                             " element; ⍴A is " << A.get_shape();
             // a conformance error, not a DOMAIN ERROR (Bugs34 #33)
             if (A.get_rank() == B.get_rank())   LENGTH_ERROR;
             RANK_ERROR;
           }

        if (A.is_empty() && B.is_empty() &&
            A.get_shape() != B.get_shape())   // both empty (Bugs35 #41)
           {
             MORE_ERROR() << "A f¨B: ⍴A is " << A.get_shape()
                          << " but ⍴B is " << B.get_shape();
             if (A.get_rank() == B.get_rank())   LENGTH_ERROR;
             RANK_ERROR;
           }

        if (B.is_empty())          shape_Z = B.get_shape();
        else if (!B.is_scalar_extensible())
           {
             MORE_ERROR() << "A f¨B: A is empty and B has more than 1"
                             " element; ⍴B is " << B.get_shape();
             // a conformance error, not a DOMAIN ERROR (Bugs34 #33)
             if (A.get_rank() == B.get_rank())   LENGTH_ERROR;
             RANK_ERROR;
           }

        // evaluate the fill function (lrm p. 245)
        //
        Value_P Z1;
        if (LO->is_defined() || LO->may_push_SI())
           {
             // the fill function of a defined function (or of a derived
             // function whose operand is defined, e.g. F⍤1 for a
             // ⎕FX-defined F) is the identity function, i.e. its right
             // argument -- calling eval_AB()/eval_B() on such an LO
             // pushes a real SI frame and returns TOK_SI_PUSHED, not a
             // value, so falling into the generic "else" branch below
             // returned that SI-pushed token as if it were Z1 (Bugs28
             // #35: (F⍤1)¨⍬ gave the scalar F 1 instead of an empty Z)
             //
             Z1 = first_B;
           }
        else if (LO->is_scalar_function())
           {
             // NOTE: even though eval_ALB is dyadic, we call the monadic
             //       fill function because it (and not the dyadic fill
             //       function handles the empty argument case).
             //
             Token tok_Z1 = LO->eval_fill_B(*(B.is_empty() ? first_B : first_A));
             if (tok_Z1.get_Class() != TC_VALUE)   return tok_Z1;
             Z1 = tok_Z1.get_apl_val();
           }
        else
           {
             // the fill function is the function itself
             //
             Token tok_Z1 = LO->eval_AB(*first_A, *first_B);
             if (tok_Z1.get_Class() != TC_VALUE)   return tok_Z1;
             Z1 = tok_Z1.get_apl_val();
           }

        Value_P Z(shape_Z, LOC);

        // Z1 is the prototype of the empty Z
        //
        if (Z1->is_simple_scalar())     // then ⊂Z1 is Z1
           {
             Cell cache;
             Z->set_ravel_Cell(0, Z1->get_cscalar(cache));
           }
        else   // need to enclose Z1
           {
             Z->set_ravel_Pointer(0, Z1.get());
           }

        Z->check_value(LOC);
        Z->to_type(true);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (LO->may_push_SI())   // user defined LO
      {
         // the shape of Z is chosen as for a primitive LO (see shape_Z
         // below): ⍴B if A is a scalar, ⍴A if B is a scalar, and otherwise
         // the shape of the argument that is not extended. If both have
         // 1 element (but neither is a scalar), then A is extended and Z
         // has the shape of B (Bugs37 #66).
         //
         const bool extend_A = A.is_scalar_extensible() && !B.is_scalar();
         const bool extend_B = B.is_scalar_extensible() &&
                               (B.is_scalar() || !A.is_scalar_extensible());

         Macro * macro = 0;
         if (LO->has_result())
            {
              if (extend_A)
                 {
                   macro = Macro::get_macro(extend_B
                                          ? Macro::MAC_Z__sA_LO_EACH_sB
                                          : Macro::MAC_Z__sA_LO_EACH_vB);
                }
             else
                {
                  macro = Macro::get_macro(extend_B
                                         ? Macro::MAC_Z__vA_LO_EACH_sB
                                         : Macro::MAC_Z__vA_LO_EACH_vB);
                }
            }
         else   // LO has no result, so we can ignore the shape of the result
            {
              if (extend_A && extend_B)
                 {
                   macro = Macro::get_macro(Macro::MAC_sA_LO_EACH_sB);
                 }
              else if (extend_B)
                 {
                   macro = Macro::get_macro(Macro::MAC_vA_LO_EACH_sB);
                 }
              else if (extend_A)
                 {
                   macro = Macro::get_macro(Macro::MAC_sA_LO_EACH_vB);
                 }
               else
                 {
                   macro = Macro::get_macro(Macro::MAC_vA_LO_EACH_vB);
                 }
            }

        if (extend_A && !A.is_scalar())        // 1-element non-scalar A
           {
             Value_P A1(LOC);
             Cell cache;
             A.get_cfirst(cache).init_other(&A1->get_wscalar(), *A1, LOC);
             A1->check_value(LOC);

             return macro->eval_ALB(*A1, _LO, B);
           }
        else if (extend_B && !B.is_scalar())   // 1-element non-scalar B
           {
             Value_P B1(LOC);
             Cell cache;
             B.get_cfirst(cache).init_other(&B1->get_wscalar(), *B1, LOC);
             B1->check_value(LOC);

             return macro->eval_ALB(A, _LO, *B1);
           }
        else
           {
             return macro->eval_ALB(A, _LO, B);
           }
      }


   // use the same scheme as ScalarFunction::do_scalar_AB() to determine
   // the shape of the result. In order to detect conformity errors we compute
   // shape_Z even if no result is returned.
   //
const int inc_A = A.get_increment();
const int inc_B = B.get_increment();

const Shape * shape_Z = 0;
   if      (A.is_scalar())      shape_Z = &B.get_shape();
   else if (B.is_scalar())      shape_Z = &A.get_shape();
   else if (inc_A == 0)          shape_Z = &B.get_shape();
   else if (inc_B == 0)          shape_Z = &A.get_shape();
   else if (A.same_shape(B))   shape_Z = &B.get_shape();
   else   // error
      {
        if (!A.same_rank(B))
           {
             MORE_ERROR() << "A f¨B: expecting ⍴⍴A = ⍴⍴B (or A or B a"
                             " scalar); ⍴⍴A is " << A.get_rank()
                          << ", ⍴⍴B is " << B.get_rank();
             RANK_ERROR;
           }
        else
           {
             MORE_ERROR() << "A f¨B: expecting ⍴A = ⍴B (or A or B a"
                             " scalar); ⍴A is " << A.get_shape()
                          << ", ⍴B is " << B.get_shape();
             LENGTH_ERROR;
           }
      }

const ShapeItem len_Z = shape_Z->get_volume();
Value_P Z;
   if (LO->has_result())   Z = Value_P(*shape_Z, LOC);

   loop(z, len_Z)
      {
        Cell cache_A, cache_B;
        const Cell & cA = A.get_cravel(inc_A * z, cache_A);
        const Cell & cB = B.get_cravel(inc_B * z, cache_B);
        const bool left_val = cB.is_lval_cell();
        Value_P LO_A = cA.to_value(LOC);     // left argument of LO
        Value_P LO_B = cB.to_value(LOC);     // right argument of LO;
        if (left_val)
           {
             // Z (this each's own result) becomes B for an outer
             // wrapping selector in a nested selective-spec chain, e.g.
             // (H¨(⌽¨V))←N: mark it now, not just values fresh out of
             // get_cellrefs(), or an outer defined LO would see
             // B.is_left_value() == false (Z was built cell-by-cell
             // here, never through get_cellrefs()) and slip straight
             // past the guard above -- confirmed exploitable before
             // this line was added.
             //
             if (Z)   Z->set_left_value();

             Cell * dest = cB.get_lval_value();
             // target can be 0! (Value.cc:502's own comment) -- e.g.
             // a selective assignment through an over-take of an
             // empty vector, (1 2+¨1↑⍬)←1 2: dereferencing it
             // unconditionally segfaults instead of a clean error.
             if (dest == 0)   INDEX_ERROR;
             if (dest->is_pointer_cell())
                {
                  Value_P sub = dest->get_pointer_value();
                  LO_B = sub->get_cellrefs(LOC);
                }
           }

        Token result = LO->eval_AB(*LO_A, *LO_B);

        // if LO was a primitive function, then result may be a value.
        // if LO was a user defined function then result may be TOK_SI_PUSHED.
        // in both cases result could be TOK_ERROR.
        //
        if (result.get_Class() == TC_VALUE)
           {
             Value_P vZ = result.get_apl_val();

             if (!Z)
                ;   // LO without result: nothing to store
             else if (vZ->is_simple_scalar() || (left_val && vZ->is_scalar()))
                {
                  Cell cache;
                  Z->next_ravel_Cell(vZ->get_cfirst(cache));
                }
             else
                Z->next_ravel_Pointer(vZ.get());

             continue;   // next z
           }

        if (result.get_tag() == TOK_VOID)   // LO without result
           {
             // LO claims a result (Z was allocated) but returned none,
             // e.g. the help listing of ⎕RVAL ⍬: a VALUE ERROR like
             // without ¨, not an uninitialized item of Z (Bugs35 #46)
             if (Z)
                {
                  MORE_ERROR() << LO->get_name() << "¨: " << LO->get_name()
                               << " returned no result";
                  VALUE_ERROR;
                }
             continue;   // next z
           }

        if (result.get_tag() == TOK_ERROR)   return result;

        Q1(result);   FIXME;
      }

   if (!Z)   return Token(TOK_VOID);   // LO without result

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Bif_OPER1_EACH::do_eval_LB(Token & _LO, cValue_R B)
{
   // monadic EACH: call _LO for every item of B

cFunction_P LO = _LO.get_function();
   Assert1(LO);

   if (LO->is_operator() &&
       !_LO.is_SLASH_or_BACKSLASH())     SYNTAX_ERROR;
   if (!(LO->get_signature() & SIG_B))   VALENCE_ERROR;

   // see the dyadic branch (eval_ALB) for the full explanation of both
   // the VF_Flags::left_value test (B.is_left_value(), not a cell-
   // type/position guess) and why LO must genuinely select/overtake B
   // (SEL_MON or SEL_MON_X -- Each does not itself distinguish whether
   // LO carries a bracket axis, so both bits qualify here) and have a
   // result, or it must never be handed a selective-assignment target
   // (an lvalue array) -- whether it is a primitive that cannot
   // interpret a raw LvalCell (e.g. scalar functions) or a defined
   // function that could smuggle the live LvalCell out into unrelated
   // storage.
   //
   if (!B.is_empty() && B.is_left_value() && LO->has_monadic_form() &&
       (!LO->has_result() || !(LO->get_selectivity() & (SEL_MON | SEL_MON_X))))
      {
        // Bugs31 #29: see the matching guard in eval_ALB() above.
        //
        const_cast<Value &>(static_cast<const Value &>(B))
           .neutralize_lval_cells(LOC);
        SYNTAX_ERROR;
      }

   if (B.is_empty())
      {
        if (!LO->has_result())   return Token(TOK_VOID);    // no-op

        // B is itself an (empty) selective-assignment target (Blake
        // McBride, Bugs31 #4, e.g. (∊¨0↑V)←9): there is nothing to select
        // regardless of what LO would compute for a hypothetical single
        // element, so skip the "fill function" machinery below entirely
        // and return the same empty-lvalue no-op placeholder
        // Bif_F12_ELEMENT::do_eval_B()'s own dedicated empty-B-lvalue
        // branch uses. Every cell of a value whose is_left_value() flag
        // is set must be an LvalCell or PointerCell
        // (Value::check_lval_consistency()) -- an earlier version of this
        // fix set the flag but left the prototype as a plain copy of the
        // (irrelevant, LO-computed) fill value, which check_lval_
        // consistency() then correctly rejected as malformed. Like the
        // non-Each control (∊0↑V)←9, this makes the whole selective
        // specification a no-op on A, while the *expression's own value*
        // is still the plain, unselected right argument (an ordinary APL2
        // assignment property, confirmed live for the control:
        // X←(∊0↑V)←9 gives X≡9) -- that is handled generically by
        // Value::assign_cellrefs()'s dest_count==0 early return, not
        // anything specific to Each, so nothing more is needed here.
        //
        if (B.is_left_value())
           {
             Value_P Z(B.get_shape(), LOC);
             new (&Z->get_wproto()) LvalCell(0, 0);
             Z->set_left_value();
             Z->check_value(LOC);
             return Token(TOK_APL_VALUE1, Z);
           }

        Value_P first_B = Bif_F12_TAKE::first(B);

        // evaluate the fill function (lrm p. 245)
        //
        Value_P Z1;
        if (LO != &Quad_EC::fun && (LO->is_defined() || LO->may_push_SI()))
           {
             // the fill function of a defined function (or of a derived
             // function whose operand is defined, e.g. F⍤1 for a
             // ⎕FX-defined F) is the identity function, i.e. its right
             // argument -- see the dyadic branch above for the full
             // explanation (Bugs28 #35). ⎕EC itself is excluded: it also
             // may_push_SI(), but has its own dedicated fill value
             // fabricated below rather than being its own identity.
             //
             Z1 = first_B;
           }
        else if (LO->is_scalar_function() || (LO == &Bif_F12_DOMINO::fun))
           {
             Token tok_Z1 = LO->eval_fill_B(*first_B);
             if (tok_Z1.get_Class() != TC_VALUE)   return tok_Z1;
             Z1 = tok_Z1.get_apl_val();
           }
        else if (LO == &Quad_EC::fun)
           {
             // fake Z1←⎕EC ''  ←→  3 (0 0) (0 0⍴0)
             //
             if (!B.is_character_cell(0))
                {
                  MORE_ERROR() << "⎕EC¨B: B's prototype is not a character"
                                  " (needed to fabricate ⎕EC's fill value)";
                  DOMAIN_ERROR;
                }
             Z1 = Value_P(3, LOC);
             Z1->next_ravel_Number(3);
             {
               Value_P sub1(2, LOC);
               sub1->next_ravel_0();
               sub1->next_ravel_0();
               sub1->check_value(LOC);
               Z1->next_ravel_Pointer(sub1.get());
             }
             Value_P sub2 = Idx0_0(LOC);
             Z1->next_ravel_Pointer(sub2.get());
           }
        else
           {
             // the fill function is the function itself
             //
             Token tok_Z1 = LO->eval_B(*first_B);
             if (tok_Z1.get_Class() != TC_VALUE)   return tok_Z1;
             Z1 = tok_Z1.get_apl_val();
           }

        Value_P Z(B.get_shape(), LOC);

        // Z1 is the prototype of the empty Z
        //
        if (Z1->is_simple_scalar())     // then ⊂Z1 is Z1
           {
             Cell cache;
             Z->set_ravel_Cell(0, Z1->get_cscalar(cache));
           }
        else                            // need to encose Z1
           Z->set_ravel_Pointer(0, Z1.get());

        // B.is_left_value() already returned above via its own dedicated
        // placeholder, so B here is never a selective-assignment target.
        //
        Z->check_value(LOC);
        Z->to_type(true);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (LO->may_push_SI())   // user defined LO
      {
        if (!LO->has_result())
           return Macro::get_macro(Macro::MAC_LO_EACH_B)->eval_LB(_LO, B);

        if (LO == &Bif_F1_EXECUTE::fun)
           return Macro::get_macro(Macro::MAC_Z__EXEC_EACH_B)->eval_B(B);

        return Macro::get_macro(Macro::MAC_Z__LO_EACH_B)->eval_LB(_LO, B);
      }

const ShapeItem len_Z = B.element_count();
Value_P Z;
   if (LO->has_result())   Z = Value_P(B.get_shape(), LOC);

   loop (z, len_Z)
      {
        if (LO->get_fun_valence() == 0)
           {
             // we allow niladic functions N so that one can simply loop
             // over them with N ¨ 1 2 3 4
             //
             Token result = LO->eval_();

             if (result.get_Class() == TC_VALUE)
                {
                  Value_P vZ = result.get_apl_val();
                  if (!Z)
                     ;   // LO without result: nothing to store
                  else if (vZ->is_simple_scalar())
                     {
                       Cell cache;
                       Z->next_ravel_Cell(vZ->get_cfirst(cache));
                     }
                  else
                     Z->next_ravel_Pointer(vZ.get());

                  continue;   // next z
           }

             if (result.get_tag() == TOK_VOID)
                {
                  // LO claims a result (Z was allocated) but returned
                  // none: see eval_ALB() (Bugs35 #46)
                  if (Z)
                     {
                       MORE_ERROR() << LO->get_name() << "¨: "
                                    << LO->get_name() << " returned no result";
                       VALUE_ERROR;
                     }
                  continue;   // next z
                }

             if (result.get_tag() == TOK_ERROR)   return result;

             Q1(result);   FIXME;
           }
        else
           {
             Cell cache;
             const Cell & cB = B.get_cravel(z, cache);
             const bool is_left_val = cB.is_lval_cell();
             Value_P LO_B = cB.to_value(LOC);      // right argument of LO

             if (is_left_val)
                {
                  // Z (this each's own result) becomes B for an outer
                  // wrapping selector in a nested selective-spec chain
                  // -- see the dyadic branch (eval_ALB) for the full
                  // explanation and a confirmed-exploitable repro.
                  //
                  if (Z)   Z->set_left_value();

                  Cell * dest = cB.get_lval_value();
                  // target can be 0! (Value.cc:502's own comment) -- e.g.
                  // a selective assignment through an over-take of an
                  // empty vector, (,¨1↑⍬)←,1: dereferencing it
                  // unconditionally segfaults instead of a clean error.
                  if (dest == 0)   INDEX_ERROR;
                  if (dest->is_pointer_cell())
                     {
                       Value_P sub = dest->get_pointer_value();
                       LO_B = sub->get_cellrefs(LOC);
                     }
                }

             Token result = LO->eval_B(*LO_B);
             if (result.get_Class() == TC_VALUE)
                {
                  Value * vZ = result.get_apl_val().get();

                  if (!Z)
                     ;   // LO without result: nothing to store
                  else if (vZ->is_simple_scalar() ||
                           (is_left_val && vZ->is_scalar()))
                     {
                       Cell cache;
                       Z->next_ravel_Cell(vZ->get_cfirst(cache));
                     }
                  else
                     Z->next_ravel_Pointer(vZ);

                  continue;   // next z
                }

             if (result.get_tag() == TOK_VOID)
                {
                  // LO claims a result (Z was allocated) but returned
                  // none: see eval_ALB() (Bugs35 #46)
                  if (Z)
                     {
                       MORE_ERROR() << LO->get_name() << "¨: "
                                    << LO->get_name() << " returned no result";
                       VALUE_ERROR;
                     }
                  continue;   // next z
                }

             if (result.get_tag() == TOK_ERROR)   return result;

             Q1(result);   Q1(*LO) FIXME;
           }
      }

   if (!Z)   return Token(TOK_VOID);   // LO without result

   Z->set_default(B, LOC);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════

