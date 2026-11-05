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

#include <string.h>

#include "IndexExpr.hh"
#include "Output.hh"
#include "PrintOperator.hh"
#include "Symbol.hh"
#include "Token.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
Token::Token(TokenTag t, IndexExpr & idx)
{
   /* construct a token for an index expressions. There are 3 cases which
      differ by tag and by value type:

      1. t indicates a partial index. Partial indices are used after the
         trailing ] but before the leading [ was parsed (remember that we
         parse right to left). Tag is TOK_PINDEX / TV_INDEX

      2. t indicates a complete token (after parsing [). There are 2 sub-cases:

      2a. idx has rank 1: [ axis ]. IndexExpr is converted to a (single) value,
          possibly 0 to indicate an elided index []. Tag is TOK_AXIS / TV_VAL

      2b. idx is [i1;i2...]: Tag is TOK_INDEX / TV_INDEX.

      In case 1. and 2b. the tag == t; only in case 2a. is the caller's
      t == TOK_INDEX replaced with tok == TOK_AXIS.
    */

   if (t == TOK_PINDEX)   // case 1. (partial index   ...] )
      {
        // this token is a partial index in the prefix parser.
        // use it as is.
        //
        tag = TOK_PINDEX;
        value.index_val = &idx;        // type is TV_INDEX
      }
   else                   // case 2. (complete index [ ... ] )
      {
        Assert(t == TOK_INDEX);
        if (idx.get_rank() < 2)   // case 2a. [idx] or []
           {
             // value.apl_val is a plain Value_P_Base: construct a counted
             // Value_P in it (a plain assignment would not count it).
             //
             tag = TOK_AXIS;
             new (&value.apl_val) Value_P(idx.extract_axis());   // TV_VAL
           }
        else                           // [idx; ...]
           {
             tag = TOK_INDEX;
             value.index_val = &idx;   // type is TV_INDEX
           }
      }
}
//────────────────────────────────────────────────────────────────────────────
ostream &
operator << (ostream & out, TokenTag tag)
{
   if (tag > 0xFFFF)   out << HEX(tag);
   else                out << HEX4(tag);

   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
operator << (ostream & out, TokenClass tc)
{
#define tcc(x) case x: out << #x;   break;
   switch(tc)
      {
        tcc(TC_INVALID)
        tcc(TC_END)
        tcc(TC_RETURN)
        tcc(TC_VALUE)
        tcc(TC_INDEX)
        tcc(TC_PINDEX)
        tcc(TC_SYMBOL)
        tcc(TC_FUN0)
        tcc(TC_FUN12)
        tcc(TC_OPER1)
        tcc(TC_OPER2)
        tcc(TC_R_PARENT)
        tcc(TC_L_PARENT)
        tcc(TC_R_BRACK)
        tcc(TC_L_BRACK)
        tcc(TC_R_ARROW)
        tcc(TC_ASSIGN)
        tcc(TC_LINE)
        tcc(TC_VOID)
        tcc(TC_NUMERIC)
        tcc(TC_SPACE)
        tcc(TC_NEWLINE)
        tcc(TC_COLON)
        tcc(TC_QUOTE)
        tcc(TC_OFF)
        tcc(TC_SI_CHANGE)

        default: break;
      }
#undef tcc

   return out;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
operator << (ostream & out, const Token & token)
{
const TokenTag tag = token.get_tag();
   switch(tag)
      {
        case TOK_CHARACTER: return out << "CHAR«" << token.get_char_val()
                                       << "»";

        case TOK_APL_VALUE1:
        case TOK_APL_VALUE2:
        case TOK_APL_VALUE3:
             if      (tag == TOK_APL_VALUE1)   out << "VALUE1";
             else if (tag == TOK_APL_VALUE2)   out << "VALUE2";
             else if (tag == TOK_APL_VALUE3)   out << "VALUE3";
             else                              out << "VALUE???";
             {
               Value_P value = token.get_apl_val();
               // For a suspended ⍎ frame this token's Value_P has
               // already been moved out (e.g. by the time ]SI walks the
               // body printing every token at the current PC) -- print a
               // placeholder instead of asserting; this is a normal,
               // reachable state (Bugs28 #24), not an internal error.
               if (!value)   { out << "«moved»"; break; }
               const APL_types::Depth depth = value->compute_depth();
               out << "«";
               for (APL_types::Depth d = 0; d < depth; ++d)   out << "≡";

               if (value->get_rank())   out << value->get_shape();

               const PrintContext pctx(PR_APL);
               PrintBuffer pb(*value, pctx, 0);
               bool more = pb.get_row_count() > 1;
               if (pb.get_row_count() > 0)
                  {
                    UCS_string ucs = pb.get_line(0).no_pad();
                    if (ucs.size() > 20)
                       {
                         ucs.resize(20);
                         more = true;
                       }
                    out << ucs;
                  }
               if (more)   out << "...";
               return out << "»";
             }

        case TOK_ERROR:
             return out << Error::error_name( ErrorCode(token.get_int_val()));

        case TOK_BRANCH_INT:
             // ↦ (U+21A6) rather than plain → so debug printouts can
             // tell an int/computed branch target apart from a label
             // one (TOK_BRANCH_LAB, which keeps the ordinary →) at a
             // glance; same Arrows block as → for font coverage.
             return out << "↦" << token.get_int_val();

        case TOK_BRANCH_LAB:
             return out << token.get_Id() << token.get_int_val();

        case  TOK_GOTO_PC:
              return out << "→PC=" << token.get_int_val();

        case TOK_NOBRANCH:
             return out << token.get_Id() << token.get_int_val();

        case TOK_INTEGER:
             return out << "INTEGER (" << token.get_int_val() << ") ";

        case TOK_RETURN_SYMBOL:
             {
               const Symbol * symbol = token.get_sym_ptr();
               Assert1(symbol);
               return out << "RETURN_SYMBOL(" << symbol->get_name() << ")";
             }

        case TOK_DIAMOND:      return out << "◊";
        case TOK_ENDL:         return out << "ENDL";
        case TOK_IF_THEN:      return out << "→→";
        case TOK_IF_ELSE:      return out << "←→";
        case TOK_IF_END:       return out << "←←";

        case TOK_RETURN_EXEC:  return out << "RETURN ⍎";
        case TOK_RETURN_STATS: return out << "RETURN ◊";
        case TOK_RETURN_VOID:  return out << "RETURN ∇FUN";

         default: break;
      }

   if (token.get_Id() > ID_No_ID2)   return out << token.get_Id();

   switch(token.get_Class())
      {
        case TC_END:
             return out << "END";

        case TC_RETURN:
             return out << "RETURN ???";

        case TC_VALUE:    return token.print_value(out);

        case TC_INDEX:
        case TC_PINDEX:
             if (tag == TOK_INDEX)   return out << token.get_index_val();
             if (tag == TOK_AXIS)
                {
                  out << "[";
                  if (const cValue * val = token.get_axes().get())
                     out << *val;
                  return out << "]";
                }
             if (tag == TOK_FAXIS)
                {
                  return out << "[" << token.get_int_val() << "]";
                }
             FIXME;

        case TC_SYMBOL:
             if (tag == TOK_LSYMB)
                {
                  token.get_sym_ptr()->print(out);
                  out << UNI_LEFT_ARROW;
                }
             else if (tag == TOK_LSYMB2)
                {
                  token.get_sym_ptr()->print(out << "'(... ");
                  out << ")←";
                }
             else
                {
                  token.get_sym_ptr()->print(out);
                }
             return out;

        case TC_FUN0:
        case TC_FUN12:
        case TC_OPER1:
        case TC_OPER2:    return token.print_function(out);
        case TC_LINE:     return out << "\n[" << token.get_fun_line() << "] ";
        case TC_NUMERIC:  return out << "Numeric";
        case TC_NEWLINE:  return out << "{LFeed}";

        default: break;
      }

   return out <<  "{-unknown Token " << tag << "-}";
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Token::canonical(PrintStyle style) const
{
UCS_string ucs;
   switch(get_Class())
      {
        case TC_ASSIGN:
             ucs << UNI_LEFT_ARROW;
             break;

        case TC_R_ARROW:
             ucs << UNI_RIGHT_ARROW;
             break;

        case TC_COLON:
             ucs << UNI_COLON;
             break;

        case TC_L_BRACK:
             if (get_tag() == TOK_L_BRACK)        ucs << UNI_L_BRACK;
             else if (get_tag() == TOK_SEMICOL)   ucs << UNI_SEMICOLON;
             else
                FIXME;
             break;

        case TC_R_BRACK:
             ucs << UNI_R_BRACK;
             break;

        case TC_END:
             switch(get_tag())
                {
                  case TOK_IF_THEN:  ucs << UNI_RIGHT_ARROW
                                          << UNI_RIGHT_ARROW;   break;
                  case TOK_IF_ELSE:  ucs << UNI_LEFT_ARROW
                                          << UNI_RIGHT_ARROW;   break;
                  case TOK_IF_END:   ucs << UNI_LEFT_ARROW
                                          << UNI_LEFT_ARROW;    break;
                  default:           ucs << UNI_DIAMOND;
                }
             break;

        case TC_RETURN:                                                  break;
        case TC_LINE:
             ucs << UNI_LF;
             break;

        case TC_VALUE:
             if (get_tag() == TOK_CHARACTER)
                {
                  // a raw character literal 'c' as produced by the Tokenizer
                  // (before it is packed into a value). It has no Value_P.
                  //
                  const Unicode uni = get_char_val();
                  ucs << UNI_SINGLE_QUOTE << uni;
                  if (uni == UNI_SINGLE_QUOTE)   ucs << UNI_SINGLE_QUOTE;
                  ucs << UNI_SINGLE_QUOTE;
                  break;
                }
             if (!is_apl_val())   break;   // no value (yet)
             {
               PrintContext pctx(style, DEFAULT_Quad_PP, DEFAULT_Quad_PW);
               PrintBuffer pbuf(*get_apl_val(), pctx, 0);
               if (pbuf.get_row_count() == 0)   return ucs;
               return pbuf.l1();
             }

        case TC_SYMBOL:
             ucs << get_sym_ptr()->get_name();
             break;

        case TC_LIT_ITEM:
             // a raw scalar literal (TOK_INTEGER/TOK_REAL/TOK_COMPLEX)
             // as produced directly by the Tokenizer, before
             // optimize_short_primitives() packs runs of these into a
             // TC_VALUE token holding an actual Value_P (the case
             // above). A Token_string in this pre-packed state occurs
             // e.g. right after Tokenizer::tokenize(), which is what
             // Token_string::all_brackets_closed() operates on.
             {
               const PrintContext pctx(style, DEFAULT_Quad_PP,
                                       DEFAULT_Quad_PW);
               switch(get_ValueType())
                  {
                    case TV_INT:
                         ucs << get_int_val();
                         break;

                    case TV_FLT:
                         {
                           bool scaled = false;
                           ucs << UCS_string(get_flt_val(), scaled, pctx);
                         }
                         break;

                    case TV_CPX:
                         {
                           bool scaled = false;
                           ucs << UCS_string(get_cpx_real(), scaled, pctx);
                           ucs << UNI_J;
                           ucs << UCS_string(get_cpx_imag(), scaled, pctx);
                         }
                         break;

                    default:
                         FIXME;
                  }
             }
             break;

        case TC_R_PARENT:
        case TC_L_PARENT:
        case TC_R_CURLY:
        case TC_L_CURLY:
             return ID::get_name_UCS(get_Id());

        case TC_FUN0:
        case TC_FUN12:
        case TC_OPER1:
        case TC_OPER2:
             if (get_Id() == ID_No_ID)   return get_function()->get_name();
             return ID::get_name_UCS(get_Id());


        case TC_INDEX:
             if (get_tag() == TOK_AXIS)
                {
                  UCS_string ret;
                  ret << "[";

                  // caution: get_apl_val() may be 0 (for index []).
                  //
                  if (const cValue * axis = get_apl_val().get())
                     {
                      // axis may be a near-int float (e.g. a literal axis
                      // like [1E10] that Parser::optimize_literal_axes()
                      // deferred to the value-axis/IndexExpr path because
                      // it was too large for sAxis) -- get_int_value()
                      // throws DOMAIN_ERROR on anything but a genuine
                      // IntCell, which recurses back into this very error
                      // formatting code and stack-overflows. get_near_int()
                      // accepts near-int floats too, but still throws
                      // DOMAIN_ERROR if the value is too large to fit an
                      // int64_t (e.g. [1E30]). A try/catch here would NOT
                      // help: throw_apl_error() calls
                      // Error::update_error_info() -- which is what got us
                      // into this very function -- *before* it actually
                      // throws, so the "error" never unwinds back to a
                      // catch here; it just recurses through plain calls
                      // until the stack overflows. So check first, with a
                      // predicate that cannot throw, instead of catching
                      // after the fact.
                      Cell cache;
                      const Cell & c = axis->get_cscalar(cache);
                      if (c.is_near_int64_t())
                         ret << ShapeItem(c.get_near_int());
                      else if (c.is_numeric())
                         {
                           // too large for get_near_int() (which would
                           // throw and recurse back in here, see above),
                           // but get_real_value()/get_imag_value() do
                           // not throw for a numeric cell -- render it
                           // the same way TOK_LIT_ITEM/TV_FLT renders an
                           // ordinary float literal, so e.g. [1E19]
                           // still shows its actual value instead of a
                           // meaningless "?" placeholder.
                           const PrintContext pctx(style, DEFAULT_Quad_PP,
                                                   DEFAULT_Quad_PW);
                           bool scaled = false;
                           ret << UCS_string(c.get_real_value(), scaled,
                                             pctx);
                           if (c.is_complex_cell() &&
                               c.get_imag_value() != 0.0)
                              {
                                scaled = false;
                                ret << UNI_J
                                    << UCS_string(c.get_imag_value(),
                                                  scaled, pctx);
                              }
                         }
                      else
                         ret << "?";
                     }
                  ret << "]";
                  return ret;
                }

             if (get_tag() == TOK_FAXIS)   // function axis
                {
                  UCS_string ret;
                  return ret << "[" << get_int_val() << "]";
                }

             if (get_tag() == TOK_MARKER)   // function axis
                {
                  UCS_string ret;
                  return ret << "@" << get_int_val() << "@";
                }

             FIXME;

        case TC_VOID:
             {
               UCS_string ret;
               ret << "-VOID-";
               return ret;
             }

        default:
             CERR << "Token: " << HEX4(tag) << " " << *this
                  << " at " << LOC << endl;
             Q1(get_Class())
             BACKTRACE
             FIXME;
      }

   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
int
Token::error_info(UCS_string & ucs) const
{
   return error_info(ucs, canonical(PR_APL_FUN).remove_pad());
}
//────────────────────────────────────────────────────────────────────────────
int
Token::error_info(UCS_string & ucs, const UCS_string & text)
{
const Unicode c1 = ucs.size() ? ucs.back() : Invalid_Unicode;
const Unicode c2 = text.size() ? text[0] : Invalid_Unicode;

   // conditions when we don't need a space
   //
const bool need_space = text.size() &&
                        ! (Avec::no_space_after(c1) || Avec::no_space_before(c2));

   if (need_space)   ucs << UNI_SPACE;

   ucs << text;
   return need_space ? -text.size() : text.size();
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Token::get_function_axis() const
{
   if (get_tag() == TOK_INDEX)  // axis has semicolons
      {
        MORE_ERROR() << "Invalid function axis rank.";
        AXIS_ERROR;
      }

   if (get_tag() == TOK_AXIS)   // axis is an APL value
      {
        if (Value_P Z = get_apl_val())   return Z;
        MORE_ERROR() << "Invalid function axis []";
        AXIS_ERROR;
      }

   if (get_tag() == TOK_FAXIS)   // axis is an integer
      {
        const sAxis axis = get_int_val();
        if (axis == -1)   // [ ]
           {
             MORE_ERROR() << "Invalid elided function axis [ ]";
             AXIS_ERROR;
           }
        return IntScalar(axis, LOC);
      }

   // Bugs28 #100(d): Q1() is documented (Common.hh) to be for printouts
   // already guarded by a Log() conditional -- this call was bare, so
   // it unconditionally dumped an internal "*this: 'MARKER' at
   // Token.cc:..." debug trace to every session hitting this
   // (user-reachable) error, e.g. +/@1@. MORE_ERROR() below is the
   // actual diagnostic.
   //
   MORE_ERROR() << "Invalid Token type for function axis.";
   AXIS_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Token::print_function(ostream & out) const
{
   switch(tag)
      {
        case TOK_Quad_TC:
        case TOK_Quad_TS:
        case TOK_Quad_UL:
        case TOK_Quad_WA:
        case TOK_Quad_AF:
        case TOK_Quad_CR:
        case TOK_Quad_DL:
        case TOK_Quad_EC:
        case TOK_Quad_EX:
        case TOK_Quad_SVQ:
        case TOK_Quad_SVR:
        case TOK_Quad_SVS:
        case TOK_Quad_UCS:
        case TOK_Quad_AT:
        case TOK_Quad_EA:
        case TOK_Quad_EB:
        case TOK_Quad_SVC:
        case TOK_Quad_TF:
        case TOK_Quad_ES:
        case TOK_Quad_FX:
        case TOK_Quad_NA:
        case TOK_Quad_NL:
        case TOK_Quad_SI:
        case TOK_Quad_SVO:      return print_quad(out);

        case TOK_F1_EXECUTE:    return out << UNI_EXECUTE;

        case TOK_F2_LESS:       return out << UNI_LESS;
        case TOK_F2_FIND:       return out << UNI_EPSILON_UBAR;
        case TOK_F2_EQUAL:      return out << UNI_EQUAL;
        case TOK_F2_GREATER:    return out << UNI_GREATER;
        case TOK_F2_AND:        return out << UNI_AND;
        case TOK_F2_AND_B:      return out << UNI_AND << UNI_AND;
        case TOK_F2_OR:         return out << UNI_OR;
        case TOK_F2_OR_B:       return out << UNI_OR << UNI_OR;
        case TOK_F2_INDEX:      return out << UNI_SQUISH_Quad;
        case TOK_F2_LEQU:       return out << UNI_LESS_OR_EQUAL;
        case TOK_F2_MEQU:       return out << UNI_MORE_OR_EQUAL;
        case TOK_F2_UNEQU:      return out << UNI_NOT_EQUAL;
        case TOK_F2_NOR:        return out << UNI_NOR;
        case TOK_F2_NOR_B:      return out << UNI_NOR << UNI_NOR;
        case TOK_F2_NAND:       return out << UNI_NAND;
        case TOK_F2_NAND_B:     return out << UNI_NAND << UNI_NAND;

        case TOK_F12_BINOM:     return out << UNI_EXCLAM;
        case TOK_F12_CIRCLE:    return out << UNI_CIRCLE;
        case TOK_F12_COMMA:     return out << UNI_COMMA;
        case TOK_F12_COMMA1:    return out << UNI_COMMA_BAR;
        case TOK_F12_DECODE:    return out << UNI_UP_TACK;
        case TOK_F12_DIVIDE:    return out << UNI_DIVIDE;
        case TOK_F12_DOMINO:    return out << UNI_Quad_DIVIDE;
        case TOK_F12_DROP:      return out << UNI_DOWN_ARROW;
        case TOK_F12_ELEMENT:   return out << UNI_ELEMENT;
        case TOK_F12_ENCODE:    return out << UNI_DOWN_TACK;
        case TOK_F12_EQUIV:     return out << UNI_EQUIVALENT;
        case TOK_F12_FORMAT:    return out << UNI_FORMAT;
        case TOK_F12_INDEX_OF:  return out << UNI_IOTA;
        case TOK_F12_LOGA:      return out << UNI_LOGARITHM;
        case TOK_F12_MINUS:     return out << UNI_MINUS;
        case TOK_F12_PARTITION: return out << UNI_SUBSET;
        case TOK_F12_PICK:      return out << UNI_SUPERSET;
        case TOK_F12_PLUS:      return out << UNI_PLUS;
        case TOK_F12_POWER:     return out << UNI_STAR_OPERATOR;
        case TOK_F12_RHO:       return out << UNI_RHO;
        case TOK_F12_RND_DN:    return out << UNI_LEFT_FLOOR;
        case TOK_F12_RND_UP:    return out << UNI_LEFT_CEILING;
        case TOK_F12_ROLL:      return out << UNI_QUESTION;
        case TOK_F12_ROTATE:    return out << UNI_CIRCLE_STILE;
        case TOK_F12_ROTATE1:   return out << UNI_CIRCLE_BAR;
        case TOK_F12_SORT_ASC:  return out << UNI_SORT_ASCENDING;
        case TOK_F12_SORT_DES:  return out << UNI_SORT_DECENDING;
        case TOK_F12_STILE:     return out << UNI_DIVIDES;
        case TOK_F12_TAKE:      return out << UNI_UP_ARROW;
        case TOK_F12_TIMES:     return out << UNI_MULTIPLY;
        case TOK_F12_TRANSPOSE: return out << UNI_TRANSPOSE;
        case TOK_F12_WITHOUT:   return out << UNI_TILDE_OPERATOR;

        case TOK_JOT:           return out << UNI_RING_OPERATOR;

        case TOK_OPER1_EACH:
        case TOK_OPER1_REDUCE:
        case TOK_OPER1_REDUCE1:
        case TOK_OPER1_SCAN:
        case TOK_OPER1_SCAN1:
        case TOK_OPER2_OUTER:
        case TOK_OPER2_INNER:   return out << get_Id();

        case TOK_FUN0:          out << "USER-F0 ";
                                get_function()->print(out);
                                return out;
        case TOK_FUN2:          out << "USER-F2 ";
                                get_function()->print(out);
                                return out;
        case TOK_OPER1:         out << "USER-OP1 ";
                                get_function()->print(out);
                                return out;
        case TOK_OPER2:         out << "USER-OP2 ";
                                get_function()->print(out);
                                return out;

        default:                break;
      }

   // unknown tag.

   out << "{ unknown function Token " << tag;

   if (get_Id() != ID_No_ID)   // tag has an Id
      {
        out << ", Id = " << get_Id();
      }
   return out <<  ", }";
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Token::print_value(ostream & out) const
{
   switch(tag)
      {
        case TOK_VARIABLE:  return value.sym_ptr->print(out);
        case TOK_FUN0:      return out <<  "{ fun/0 }";
        case TOK_CHARACTER: return out << value.char_val;
        case TOK_INTEGER:   return out << value.int_vals[0];
        case TOK_REAL:      return out << value.float_vals[0];

        case TOK_COMPLEX:   return out << value.float_vals[0] << "J"
                                       << value.float_vals[1];

        case TOK_APL_VALUE1:
        case TOK_APL_VALUE3:
             {
               const cValue * v = value.apl_val.get();
               if (v->get_rank() == 0)   out << "''";
               loop(r, v->get_rank())
                   {
                     if (r)   out <<  " ";
                     out << v->get_shape_item(r);
                   }
               out << UNI_RHO;
               loop(e, v->element_count())
                 {
                   if (e == 4)
                      {
                        out << "...";
                        break;
                      }

                   const PrintContext pctx(PR_APL, 2, 80);
                   Cell cache;
                   out << UCS_string(v->get_cravel(e, cache)
                                       .character_representation(pctx), 0, 80)
                       << " ";
                 }
               return out;
             }

        case TOK_Quad_Quad: return out << UNI_Quad_Quad;

        case TOK_Quad_AI:
        case TOK_Quad_AV:
        case TOK_Quad_CT:
        case TOK_Quad_EM:
        case TOK_Quad_ET:
        case TOK_Quad_FC:
        case TOK_Quad_IO:
        case TOK_Quad_LC:
        case TOK_Quad_LX:
        case TOK_Quad_L:
        case TOK_Quad_PP:
        case TOK_Quad_PR:
        case TOK_Quad_PS:
        case TOK_Quad_PW:
        case TOK_Quad_R:
        case TOK_Quad_RL:
        case TOK_Quad_SVE:
        case TOK_Quad_TZ:
        case TOK_Quad_WA:   return print_quad(out);

        default: break;
      }

   out << "{ unknown value Token " << tag;

   if (get_Id() != ID_No_ID)   // tag has an Id
      {
        out << ", Id = " << get_Id();
      }

   return out <<  ", }";
}
//────────────────────────────────────────────────────────────────────────────
void
Token::show_trace(ostream & out, const UCS_string & fun_name, 
                  Function_Line line) const
{
UCS_string fn = fun_name;
   fn << UNI_L_BRACK << line << UNI_R_BRACK << UNI_SPACE;
   out << fn;

   switch(get_tag())
      {
        case TOK_APL_VALUE1:
        case TOK_APL_VALUE2:
        case TOK_APL_VALUE3:
             break;   // continue below

        case TOK_BRANCH_INT:
             out << "↦" << get_int_val() << endl;   // see Token::print()
             return;

        case TOK_BRANCH_LAB:
             out << "→" << get_int_val() << endl;
             return;

        case TOK_NOBRANCH:
             out << "→⍬" << endl;
             return;

        case TOK_ESCAPE:
             out << "→" << endl;
             return;

        case TOK_VOID:
        case TOK_NO_VALUE:
             out << endl;
             return;

        default: Q1(*this)
                 FIXME;
      }

   // print a value

PrintContext pctx = Workspace::get_PrintContext(PR_APL);
const Value & val = *get_apl_val();
   if (val.get_rank() == 0)   // scalar
      {
        pctx.set_style(PR_APL_MIN);
      }
   else if (val.get_rank() == 1)   // vector
      {
        // as in cValue::print() (Bugs34 #39)
        if (val.element_count() == 0)
           {
             out << endl;
             return;
           }

        pctx.set_style(PR_APL_MIN);
      }
   else                  // matrix or higher
      {
        pctx.set_style(PrintStyle(pctx.get_style() | PST_NO_FRACT_0));
      }

   // the trace output displays the value like cValue::print() does, i.e.
   // also with STRICT_IBM_APL2_FORMATTING parentheses (Bugs34 #41)
   //
   if (UserPreferences::uprefs.nested_parentheses)
      pctx.set_style(PrintStyle(pctx.get_style() | PST_CS_PARENS));

PrintBuffer pb(val, pctx, 0);
const UCS_string indent(fn.size(), UNI_SPACE);
   loop(l, pb.get_row_count())
      {
        if (l)   out << indent;
        out << pb.get_line(l).no_pad() << endl;
      }

   if (pb.get_row_count() == 0)   out << endl;
}
//════════════════════════════════════════════════════════════════════════════
UCS_string
Token::tag_name() const
{
   switch(get_tag())
      {
#define TD(tag, _tc, _tv, _id) case tag: return UCS_ASCII_string(#tag);
#include "Token.def"
        case TOK_NONE:   return UCS_ASCII_string("???Bad-Tag???");
      }

char cc[40];
   SPRINTF(cc, "0x%X", get_tag());
const UTF8_string utf(cc);
UCS_string ucs(utf);
   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
int
Token::value_use_count() const
{
   if (!is_apl_val())    return 0;            // token is not a TV_VALUE token
   if (!value.apl_val)   return -98;          // it is, but its Value * is -
   return value.apl_val->get_owner_count();   // non-zero Value *
}
//────────────────────────────────────────────────────────────────────────────
void
Token::ChangeTag(TokenTag new_tag)
{
   Assert((int(tag) & int(TV_MASK)) == (int(new_tag) & int(TV_MASK)));
   // tag is ia const TokenTag, so we cheat a little here.
   const_cast<TokenTag &>(tag) = new_tag;
}
//────────────────────────────────────────────────────────────────────────────
Value *
Token::extract_and_keep(const char * loc)
{
   if (!is_apl_val())   return 0;

Value * ret = value.apl_val.get();
   value.apl_val.clear_pointer(loc);
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
void
Token::extract_apl_val(const char * loc)
{
   if (is_apl_val())   value.apl_val.reset();
}
//────────────────────────────────────────────────────────────────────────────
void
Token::release_apl_val(const char * loc)
{
   if (is_apl_val())   value.apl_val.reset();
}
//────────────────────────────────────────────────────────────────────────────
const char *
Token::class_name(TokenTag tag)
{
#define tcn(x) case x: return #x;

const TokenClass tc = TokenClass(int(tag) & int(TC_MASK));
   switch(tc)
      {
        tcn(TC_ASSIGN)
        tcn(TC_R_ARROW)
        tcn(TC_L_BRACK)
        tcn(TC_R_BRACK)
        tcn(TC_END)
        tcn(TC_FUN0)
        tcn(TC_FUN12)
        tcn(TC_INDEX)
        tcn(TC_OPER1)
        tcn(TC_OPER2)
        tcn(TC_L_PARENT)
        tcn(TC_R_PARENT)
        tcn(TC_RETURN)
        tcn(TC_SYMBOL)
        tcn(TC_VALUE)

        tcn(TC_PINDEX)
        tcn(TC_VOID)

        tcn(TC_OFF)
        tcn(TC_SI_CHANGE)
        tcn(TC_LINE)
        tcn(TC_NUMERIC)
        tcn(TC_SPACE)
        tcn(TC_NEWLINE)
        tcn(TC_COLON)
        tcn(TC_QUOTE)
        tcn(TC_L_CURLY)
        tcn(TC_R_CURLY)

        tcn(TC_INVALID)
        default: ;
      }

   return "*** Obscure token class ***";
}
//────────────────────────────────────────────────────────────────────────────
const char * 
Token::short_class_name(TokenTag tag)
{
const TokenClass tc = TokenClass(int(tag) & int(TC_MASK));
   switch(tc)
      {
        case TC_ASSIGN:    return "←";
        case TC_R_ARROW:   return "→";
        case TC_L_BRACK:   return "[";
        case TC_R_BRACK:   return "]";
        case TC_END:
             if (tag == TOK_DIAMOND)   return "◊";
             if (tag == TOK_ENDL)      return "ENDL";
             if (tag == TOK_IF_THEN)   return "→→";
             if (tag == TOK_IF_ELSE)   return "←→";
             if (tag == TOK_IF_END)    return "←←";
             return "END";

        case TC_FUN0:      return "F0";
        case TC_FUN12:     return "F12";
        case TC_INDEX:     return "IDX";
        case TC_OPER1:     return "OP1";
        case TC_OPER2:     return "OP2";
        case TC_L_PARENT:  return "(";
        case TC_R_PARENT:  return ")";
        case TC_RETURN:    return "RET";
        case TC_SYMBOL:    return "SYM";
        case TC_VALUE:     return "VAL";

        case TC_PINDEX:    return "PIDX";
        case TC_VOID:      return "VOID";

        case TC_OFF:       return "OFF";
        case TC_SI_CHANGE: return "CHANGE";
        case TC_LINE:      return "LINE";
        case TC_NUMERIC:   return "NUMB";
        case TC_SPACE:     return "SPACE";
        case TC_NEWLINE:   return "LF";
        case TC_COLON:     return ":";
        case TC_QUOTE:     return "QUOTE";
        case TC_L_CURLY:   return "{";
        case TC_R_CURLY:   return "}";

        case TC_INVALID:   return "INV";
        default:           break;
      }

   return "???";
}
//────────────────────────────────────────────────────────────────────────────
ostream &
Token::print_quad(ostream & out) const
{
   return out << UNI_Quad_Quad << get_Id();
}
//════════════════════════════════════════════════════════════════════════════
// EOF
