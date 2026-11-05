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

#include <sys/time.h>

#include "Bif_F12_TAKE_DROP.hh"
#include "CDR.hh"
#include "CRC32.hh"
#include "Macro.hh"
#include "PointerCell.hh"
#include "Quad_CR.hh"
#include "Quad_TF.hh"
#include "Symbol.hh"
#include "SystemVariable.hh"
#include "Tokenizer.hh"
#include "UCS_string.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
const FunctionGroup::function_info Quad_CR::subfunction_infos[] =
{
#define crdef(N, name, comm_2)   { N, #name, "", comm_2, -1 },
#include "Quad_CR.def"
};
//════════════════════════════════════════════════════════════════════════════
Quad_CR::Quad_CR()
 : QuadFunction(TOK_Quad_CR)
{
   // note: ⎕CR is instantiated twice, once for the static Quad_CR::fun,
   // and once in Workspace::Workspace() fotr macro support
   //
enum { count = sizeof(subfunction_infos) / sizeof(*subfunction_infos) };
   init_function_group(subfunction_infos, count, "⎕CR");
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_CR::eval_B(cValue_R B) const
{
   if (B.element_count())                    // the normal case
      {
        const bool discard = UserPreferences::uprefs.discard_indentation;
        return do_eval_B(B, discard);
      }

   if (B.is_character_cell(0))   // ⎕CR ''
      {
        return list_functions(CERR);
      }

   if (B.is_integer_cell(0))     // ⎕CR ⍬
      {
        return list_mappings(CERR);
      }
   DOMAIN_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_CR::eval_AB(cValue_R A, cValue_R B) const
{
const sAxis subfunction = value_to_subfun(A);

   if (subfunction == 45)   // filter (= return B)
      {
        do_CR45(B);
        return Token(TOK_APL_VALUE1, CLONE(&B, LOC));
      }

PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
Value_P Z = do_CR(subfunction, B, pctx);
   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_CR::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
const sAxis a = value_to_subfun(A);
FrameStyle frame_style;
PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
#define FRAME(f)   pctx.set_style(PR_ ## f); frame_style = FS_ ## f; break;
   switch(a)
      {
        case  0: FRAME(APL)
        case  1: FRAME(APL_FUN)
        case  2: FRAME(BOXED_CHAR)
        case  3: FRAME(BOXED_GRAPHIC)
        case  4: FRAME(BOXED_GRAPHIC)
        case  7: FRAME(BOXED_THIN)
        case  8: FRAME(BOXED_THIN)
        case  9: FRAME(BOXED_DOUBLE)
        case 20: FRAME(NARS)
        case 21: FRAME(NARS_THIN)
        case 22: FRAME(NARS_DOUBLE)
        case 23: FRAME(NARS)
        case 24: FRAME(NARS_THIN)
        case 25: FRAME(NARS_DOUBLE)
        case 29: // strings are quoted, not framed, so user-defined frame
                 // characters do not fit (Bugs37 #58)
                 MORE_ERROR() << "A ⎕CR[X] B: style 29 (quoted strings) "
                                 "has no user-defined frame characters";
                 DOMAIN_ERROR;
        case 46: FRAME(PLAIN_THIN)
        case 47: FRAME(PLAIN_THICK)
        default: MORE_ERROR() << "A ⎕CR[X] B with invalid subfunction A (="
                              << a << ")";
                 DOMAIN_ERROR;
      }
#undef FRAME

   // valid style
   //
   if (X.get_rank() != 1)
      {
        MORE_ERROR() << "A ⎕CR[X] B with non-vector X";
        RANK_ERROR;
      }

   // empty X means: return the frame chars for the subfunction
   //
UCS_string frame_chars = frame_styles()[frame_style];
   if (X.element_count() == 0)
      {
        Value_P Z(frame_chars, LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (X.element_count() > frame_chars.ssize())
      {
        MORE_ERROR() << "A ⎕CR[X] B with too long X (max. is "
                     << frame_chars.size() << ")";
        LENGTH_ERROR;
      }

   // fill in items present in X
   //
   loop(e, X.element_count())
       {
         Cell cache;
         frame_chars[e] = X.get_cravel(e, cache).get_char_value();
       }

Value_P Z = draw_B_boxed(B, pctx, extra_frame(a), frame_chars);
   return Token(TOK_APL_VALUE1, Z);
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_CR::do_CR(APL_Integer a, cValue_R B, PrintContext pctx)
{
   // some functions a have an inverse (which has its own number, but can
   // also be specified as -a
   //
   if (a < 0)   switch(a)
      {
        case  -5: a = 13;   break;
        case  -6: a = 13;   break;
        case -13: a =  6;   break;
        case -12: a = 11;   break;
        case -11: a = 12;   break;
        case -14: a = 15;   break;
        case -15: a = 14;   break;
        case -16: a = 17;   break;
        case -17: a = 16;   break;
        case -18: a = 19;   break;
        case -19: a = 18;   break;
        case -33: a = 34;   break;
        case -34: a = 33;   break;
        case -35: a = 36;   break;
        case -36: a = 35;   break;
        case -38: a = 39;   break;
        case -39: a = 38;   break;
        case -40: a = 41;   break;
        case -41: a = 40;   break;
        default: MORE_ERROR() << "A ⎕CR B with invalid subfunction A < 0";
                 DOMAIN_ERROR;
      }

FrameStyle frame_style = FS_APL;
   switch(a)
      {
/// a local shortcut for the various frame variants of ⎕CR: the print style
/// x (for everything but the frames) and the frame style fs
#define FRAME(f)   pctx.set_style(PR_ ## f); frame_style = FS_ ## f; break;

        case  0: FRAME(APL)
        case  1: FRAME(APL_FUN)
        case  2: FRAME(BOXED_CHAR)     // ASCII chars
        case  3: FRAME(BOXED_GRAPHIC)
        case  4: FRAME(BOXED_GRAPHIC)  // extra frame
        case  5:                               // byte-vector → HEX
        case  6: return do_CR5_6(a, B);        // byte-vector → hex
        case  7: FRAME(BOXED_THIN)
        case  8: FRAME(BOXED_THIN)     // extra frame
        case  9: FRAME(BOXED_DOUBLE)
        case 10: return do_CR10(B);
        case 11: return do_CR11(B);            // Value → CDR conversion
        case 12: return do_CR12(B);            // CDR → Value conversion
        case 13: return do_CR13(B);            // hex → byte-vector
        case 14: return do_CR14(B);            // Value → CDR → hex conversion
        case 15: return do_CR15(B);            // hex → CDR → Value conversion
        case 16: return do_CR16(B);            // byte vector → base64, RFC 4648
        case 17: return do_CR17(B);            // base64 → byte vector, RFC 4648
        case 18: return do_CR18(B);            // UCS → UTF8 byte vector
        case 19: return do_CR19(B);            // UTF8 byte vector → UCS string
        case 20: FRAME(NARS)
        case 21: FRAME(NARS_THIN)
        case 22: FRAME(NARS_DOUBLE)
        case 23: FRAME(NARS)           // extra frame
        case 24: FRAME(NARS_THIN)      // extra frame
        case 25: FRAME(NARS_DOUBLE)    // extra frame
        case 26: return do_CR26(B);            // Cell types
        case 27:                               // value as int
        case 28: return do_CR27_28(a, B);      // value2 as int
        case 29: FRAME(BOXED_QUOTED)    // extra frame
        case 30: return do_CR30(B);            // conform B (for ⍤ macro)
        case 31:                               // ⎕INP helper
        case 32: return do_CR31_32(a, B);      // ⎕INP helper
        case 33: return do_CR33(B);            // TV to TLV byte vector
        case 34: return do_CR34(B);            // TLV byte vector to TV
        case 35: return do_CR35(B);            // lines to nested strings
        case 36: return do_CR36(B);            // nested strings to lines
        case 37: return do_CR37(B);            // ⎕CR B but extra spaces kept
        case 38: return do_CR38(B);            // plain →  structure
        case 39: return do_CR39(B);            // structure → plain
        case 40: return do_CR40(B);            // boolean → packed
        case 41: return do_CR41(B);            // packed → boolean
        case 42: return do_CR42_43(B, false);  // tokenize B
        case 43: return do_CR42_43(B, true);   // parse B
        case 44: return do_CR44(B);            // decode token tags
        case 46: FRAME(PLAIN_THIN)
        case 47: FRAME(PLAIN_THICK)
        case 48: return do_CR48(B);            // ravel packing type as scalar
        case 49: return do_CR49(B);            // packing threshold as scalar
        case 50:                               // int (elementwise) → HEX
        case 51: return do_CR50_51(a, B);      // int (elementwise) → hex
        case 52: return do_CR52(B);            // byte vector → CRC32
        case 53: return do_CR53(B);            // byte vector → CRC32 hex string

        default: MORE_ERROR() << "A ⎕CR B with invalid subfunction A (="
                              << a << ")";
                 DOMAIN_ERROR;
#undef FRAME
      }

   // common code for ⎕CR variants that only differ by their frames...
   //
const UCS_string & frame_chars = frame_styles()[frame_style];
   return draw_B_boxed(B, pctx, extra_frame(a), frame_chars);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::draw_B_boxed(cValue_R B, PrintContext pctx, bool extra,
                      const UCS_string & frame_chars)
{
   if (extra && !B.is_simple_scalar())
      {
        // we print the extra frame around B by printing ⊂B instead of B
        //
        Value_P Z(LOC);                      // a nested scalar
        Value_P Zsub = B.clone(LOC); 
        Z->next_ravel_Pointer(Zsub.get());   // Z ← ⊂B
        Z->check_value(LOC);
        PrintBuffer pb(*Z, pctx, frame_chars, 0);
        return Value_P(pb, LOC);
      }
   else   // no extra frame
      {
         PrintBuffer pb(B, pctx, frame_chars, 0);
         return Value_P(pb, LOC);
      }
}
//────────────────────────────────────────────────────────────────────────────
const UCS_string_vector &
Quad_CR::frame_styles()
{
   // the frame characters (see PrintBuffer::Frame_char) of each FrameStyle:
   // the 81 line-drawing characters, followed by the decorators and options.
   //
   // The line-drawing character FC_Nn_Ee_Ss_Ww is at position 27n + 9e + 3s
   // + w, where n, e, s, and w are 0 (leg absent), 1 (leg in line style 1,
   // the inner frames), or 2 (leg in line style 2, the outer frame). Below,
   // each line holds the 9 characters for one n and e: s,w = 00 01 02 10 11
   // 12 20 21 22. A · means: no character (no legs, Unicode has no such
   // character, or the line style is not used); it is drawn as a blank.
   //
   // line style 1: ASCII (IBM APL2 DISPLAY workspace)
static const char * LINES_ASCII =
   "·-·|.····"   // N0 E0
   "--·.+····"   // N0 E1
   "·········"   // N0 E2
   "|'·|+····"   // N1 E0
   "'+·++····"   // N1 E1
   "·········"   // N1 E2
   "·········"   // N2 E0
   "·········"   // N2 E1
   "·········";  // N2 E2

   // line style 1: thin
static const char * LINES_THIN =
   "·╴·╷┐····"   // N0 E0
   "╶─·┌┬····"   // N0 E1
   "·········"   // N0 E2
   "╵┘·│┤····"   // N1 E0
   "└┴·├┼····"   // N1 E1
   "·········"   // N1 E2
   "·········"   // N2 E0
   "·········"   // N2 E1
   "·········";  // N2 E2

   // line style 1: thick
static const char * LINES_THICK =
   "·╸·╻┓····"   // N0 E0
   "╺━·┏┳····"   // N0 E1
   "·········"   // N0 E2
   "╹┛·┃┫····"   // N1 E0
   "┗┻·┣╋····"   // N1 E1
   "·········"   // N1 E2
   "·········"   // N2 E0
   "·········"   // N2 E1
   "·········";  // N2 E2

   // line style 1: thick, line style 2: double
static const char * LINES_THICK_DOUBLE =
   "·╸·╻┓···╗"   // N0 E0
   "╺━·┏┳····"   // N0 E1
   "··═···╔·╦"   // N0 E2
   "╹┛·┃┫····"   // N1 E0
   "┗┻·┣╋····"   // N1 E1
   "·········"   // N1 E2
   "··╝···║·╣"   // N2 E0
   "·········"   // N2 E1
   "╚·╩···╠·╬";  // N2 E2

   // the decorators and options: ↓→∼+ϵ⊖⌽( (blank: none).
   // # instead of ↓ and → means: the axis lengths (NARS style).
   //
static const char * NO_DECOR = "        ";
static const char * DECOR    = "↓→∼+ϵ⊖⌽ ";
static const char * NARS     = "##∼+ϵ⊖⌽ ";

   // 0 for the line-drawing characters means: no frames at all
   //
static const char * styles[FS_COUNT][2] =
   {
     //  line-drawing         decorators
     { 0,                  NO_DECOR },   // FS_APL
     { 0,                  NO_DECOR },   // FS_APL_FUN
     { LINES_ASCII,        DECOR    },   // FS_BOXED_CHAR
     { LINES_THICK,        DECOR    },   // FS_BOXED_GRAPHIC
     { LINES_THIN,         DECOR    },   // FS_BOXED_THIN
     { LINES_THICK_DOUBLE, DECOR    },   // FS_BOXED_DOUBLE
     { LINES_THICK,        DECOR    },   // FS_BOXED_QUOTED
     { LINES_THIN,         NO_DECOR },   // FS_PLAIN_THIN
     { LINES_THICK,        NO_DECOR },   // FS_PLAIN_THICK
     { LINES_THICK,        NARS     },   // FS_NARS
     { LINES_THIN,         NARS     },   // FS_NARS_THIN
     { LINES_THICK_DOUBLE, NARS     },   // FS_NARS_DOUBLE
   };

   // initialized on the first call (thread-safe since C++11)
   //
static const UCS_string_vector frame_chars = []()
   {
     UCS_string_vector ret;
     loop(fs, FS_COUNT)
        {
          const char * lines = styles[fs][0];
          UCS_string chars = lines ? UCS_string(UTF8_string(lines))
                                   : UCS_string(PrintBuffer::FC_LINES_COUNT,
                                                UNI_SPACE);
          Assert(chars.size() == PrintBuffer::FC_LINES_COUNT);
          loop(c, chars.size())   // · (no character) → blank
             if (chars[c] == Unicode(0x00B7))   chars[c] = UNI_SPACE;
          chars << UCS_string(UTF8_string(styles[fs][1]));
          Assert(chars.size() == PrintBuffer::FC_COUNT);
          ret.push_back(chars);
        }
     return ret;
   }();

   return frame_chars;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_CR::figure_default(const cValue * value, Unicode & default_char,
                        APL_Integer & default_int)
{
ShapeItem zeroes = 0;
ShapeItem blanks = 0;
   loop(v, value->nz_element_count())
      {
        Cell cache;
        const Cell & cell = value->get_cravel(v, cache);
        if (cell.is_integer_cell())
           {
             if (cell.get_int_value() == 0)   ++zeroes;
           }
        else if (cell.is_character_cell())
           {
             if (cell.get_char_value() == UNI_SPACE)   ++blanks;
           }
      }

   return zeroes >= blanks;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR11(cValue_R B)
{
CDR_string cdr;
   CDR::to_CDR(cdr, &B);

const ShapeItem len = cdr.size();
Value_P Z(len, LOC);
   loop(l, len)
       Z->next_ravel_Char(Unicode(0xFF & cdr[l]));

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR12(cValue_R B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

CDR_string cdr;
   loop(b, B.element_count())
       cdr.push_back(B.get_byte_value(b));

Value_P Z = CDR::from_CDR(cdr, LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_CR::do_CR13(cValue_R B)
{
   // hex → Value conversion. 2 characters per byte in B, therefore
   // last axis of B must have even length.
   //
   if (B.get_cols() & 1)   LENGTH_ERROR;

Shape shape_Z(B.get_shape());
   shape_Z.set_shape_item(B.get_rank() - 1, (B.get_cols() + 1)/ 2);

Value_P Z(shape_Z, LOC);
ShapeItem bI = 0;
   loop(z, Z->element_count())
       {
         const int n1 = nibble(B.get_char_value(bI++));
         const int n2 = nibble(B.get_char_value(bI++));
         if (n1 < 0 || n2 < 0)   DOMAIN_ERROR;
         Z->next_ravel_Char(Unicode(16*n1 + n2));
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR35(cValue_R B)
{
   // B must be a true string (is_char_vector() == 1) that MAY contain
   // \n which then separates different lines. The \n are removed and
   // the result is a (nested) vector containing all lines.

   if (B.get_rank() != 1)   RANK_ERROR;

const ShapeItem len_B = B.element_count();
   if (len_B == 0)
      {
        Value_P Z1 = Str0(LOC);   // Z1←''
        Value_P Z(1, LOC);
        Z->next_ravel_Pointer(Z1.get());
        Z->check_value(LOC);
        return Z;
      }

ShapeItem lf_count = 0;
   loop(b, len_B)
       {
         if (B.get_char_value(b) == UNI_LF)   ++lf_count;
       }

   if (B.get_char_value(len_B - 1) != UNI_LF)   ++lf_count;

Value_P Z(lf_count, LOC);
UCS_string line;

   loop(b, len_B)
       {
         const Unicode uni = B.get_char_value(b);
         if (uni == UNI_LF)
            {
              Value_P Zb(line, LOC);
              Z->next_ravel_Pointer(Zb.get());
              line.clear();
            }
         else
            {
              line << uni;
            }
       }

   if (line.size())   // incomplete last line
      {
        Value_P Zb(line, LOC);
        Z->next_ravel_Pointer(Zb.get());
        line.clear();
      }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::check_duplicate_member(Value & Z, const UCS_string & name)
{
   // member names are the keys of an associative array and must be unique
   // (Bugs37 #41)
   //
   if (Z.get_member_data(name))
      {
        MORE_ERROR() << "38 ⎕CR B: duplicate member name " << name
                     << " in B";
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_CR::members_are_names(const cValue & value)
{
   // return true if structured value is non-empty and all its member names
   // (recursively) are APL identifiers
   //
   if (value.get_rank() != 2 || value.get_cols() != 2)   return false;

bool empty = true;
   loop(r, value.get_rows())
       {
         Cell cache_m;
         const Cell & member_cell = value.get_cravel(2*r, cache_m);
         if (member_cell.is_integer_cell())   continue;   // unused row
         empty = false;

         Value_P name_v = member_cell.try_pointer_value();
         if (!name_v || !name_v->is_char_string())   return false;
         const UCS_string name(*name_v);
         if (name.size() == 0)   return false;
         if (!Avec::is_first_symbol_char(name[0]))   return false;
         loop(n, name.size())
            if (!Avec::is_symbol_char(name[n]))   return false;

         Cell cache_d;
         const Cell & data_cell = value.get_cravel(2*r + 1, cache_d);
         if (Value_P sub = data_cell.try_pointer_value())
            {
              if (sub->is_member() && !members_are_names(*sub))
                 return false;
            }
       }

   return !empty;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::do_CR10_variable(UCS_string_vector & result,
                          const UCS_string & var_name,
                          const cValue * value)
{
   // avoid any disturbances by ⎕FC (Format Control).
   //
   Workspace::push_FC();

   // the NAME.member←value form below requires that all member names are
   // APL identifiers, and it produces no line at all for an empty
   // structure. Otherwise use the 2 ⎕TF form NAME←38⎕CR... Bugs37 #18.
   //
   if (value->is_member() && !members_are_names(*value))
      {
        const Token tok = Quad_TF::tf2_var(var_name, *value);
        if (tok.get_Class() == TC_VALUE &&
            tok.get_apl_val()->is_char_string() &&
            tok.get_apl_val()->element_count())
           {
             result.push_back(UCS_string(*tok.get_apl_val()));
             Workspace::pop_FC();   // restore ⎕FC
             return;   // OK
           }
      }

   if (value->is_member())   // structured variable
      {
        if (const char * error = do_CR10_structured(result, var_name, value))
           {
             CERR << "could not )DUMP structured variable " << var_name
                  << ": " << error
                  << "\n)DUMPing it as regular variable instead..." << endl;
             goto not_structured;
           }

        Workspace::pop_FC();   // restore ⎕FC
        return;   // OK
      }

not_structured:

#define PUSH_TEXT result.push_back(text);   text.clear();
#define TMP_VAR_PREFIX "⍙¯_"
#define TMP_VAR_SUFFIX "_∆¯"
#define TMP_VAR(d)   TMP_VAR_PREFIX << int(d) << TMP_VAR_SUFFIX

UCS_string text;
const ShapeItem ec = value->element_count();

   // frequent special case: string
   //
   if (ec < 60 && is_plain_string(value))
      {
        text << var_name << "←\"";
        loop(e, ec)   text << value->get_char_value(e);
        text << "\"";
        PUSH_TEXT
        Workspace::pop_FC();   // restore ⎕FC
        return;
      }

const APL_types::Depth depth = value->compute_depth();
   // frequent special case: simple scalar or short simple vector
   //
   if (depth <= 1 && value->get_rank() <= 1 && ec < 20)   // short simple vector
      {
        text << var_name << "←";
        if (value->element_count())   // non-empty value
           {
             // a lone item (e.g. "V←5") parses back as a *scalar*, which
             // would silently turn a genuine 1-element vector into a
             // scalar. Force vector shape with a leading , (ravel) in
             // that case.
             //
             if (value->get_rank() == 1 && ec == 1)   text << ",";

             loop(e, value->element_count())
                 {
                   if (e)   text << " ";
                   Cell cache;
                   const Cell & cell = value->get_cravel(e, cache);
                   const UCS_string item_e = do_CR10_simple_cell(cell);
                   text << item_e;
                 }
           }
        else                         // empty vector
           {
             if (value->is_character_cell(0))   text << "''";
             else                                           text << "⍬";
           }
        PUSH_TEXT
        Workspace::pop_FC();   // restore ⎕FC
        return;
      }

   // VAR ← (depth+1)⍴0. The first depth items are used as temporary values
   // for the different depths, while the last item is for saving ⎕IO.
   //
   text << TMP_VAR(depth) "←⎕IO ◊ ⎕IO←0   ⍝ " << var_name << "←";    PUSH_TEXT

   do_CR10_level(result, 0, *value);
   text << "⎕IO←" TMP_VAR(depth) << " ◊ "
        << var_name << "←⍙¯_0_∆¯";                                   PUSH_TEXT
   text << "⊣⎕EX ⊃";
   loop(d, depth + 1)   text << " '" << TMP_VAR(d) << "'";
                                                                     PUSH_TEXT
   Workspace::pop_FC();   // restore ⎕FC
}
//════════════════════════════════════════════════════════════════════════════
const char *
Quad_CR::get_legend(FunctionGroup::Legend_type lt) const
{
   switch(lt)
      {
        default: return "";

        case LET_FUN_PREFIX: return
"   ┌─── Legend:───────────────────────────────────────────────────┐\n"
"   │    b - byte vector (vector of integers between -128 and 255) │\n"
"   │    h - hex string (characters 0-9 or A-F resp. a-f)          │\n"
"   │    i - integer vector                                        │\n"
"   │    l - string of (\\n-terminated) lines                       │\n"
"   │    m - character matrix                                      │\n"
"   │    n - nested vector of strings                              │\n"
"   │    r - base64 string according to RFC 4648                   │\n"
"   │    s - string                                                │\n"
"   │    t - token                                                 │\n"
"   │    v - T,V (integer tag T and byte vector V)                 │\n"
"   └──────────────────────────────────────────────────────────────┘\n"
"\n";

   case LET_MAP_SUFFIX: return
"\n"
"   if N ⎕CR has an inverse M ⎕CR then -N can be used instead of M.\n";
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::print_fun_syntax(ostream & out,
                          const function_info & info) const
{
   out << "    " << info.comment_fun << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::print_map_syntax(ostream & out,
                          const function_info & info) const
{
char NN[10];   SPRINTF(NN, "%2d", int(info.axis));
const char * name = info.function_name;
   out << "      " << NN << " ⎕CR  ←→"
       << UCS_string(24 - strlen(name), UNI_SPACE)
       << "'" << name << "' ⎕CR  ←→  ⎕CR." << name << endl;
}
//────────────────────────────────────────────────────────────────────────────
Token
Quad_CR::do_eval_B(cValue_R B, bool remove_extra_spaces)
{
UCS_string symbol_name(B);
   symbol_name.remove_trailing_whitespaces();

   /*  return an empty character matrix,     if:
    *  1) symbol_name is not known,          or
    *  2) symbol_name is not user defined,   or
    *  3) symbol_name is a function,         or
    *  4) symbol_name is not displayable
    */

   if (symbol_name.size() == 0)   return Token(TOK_APL_VALUE1, Str0_0(LOC));

const Function * function = 0;
   if (symbol_name[0] == UNI_MUE)   // macro
      {
        loop(m, Macro::MAC_COUNT)
            {
              const Macro * macro =
                    Macro::get_macro(Macro::Macro_num(m));
              if (symbol_name == macro->get_name())
                 {
                   function = macro;
                   break;
                 }
            }
      }
   else   // maybe defined function
      {
        const NamedObject * obj = Workspace::lookup_existing_name(symbol_name);
        if (obj && obj->is_user_defined())
           {
             function = obj->get_function();
             if (function && function->get_exec_properties()[0])   function = 0;
           }
      }
   if (function == 0)   return Token(TOK_APL_VALUE1, Str0_0(LOC));

   // show the function...
   //
const UCS_string ucs = function->canonical(false);
UCS_string_vector tlines;
   ucs.to_vector(tlines);
int max_len = 0;
   loop(row, tlines.size())
      {
        if (remove_extra_spaces)
           tlines[row].remove_leading_and_trailing_whitespaces();

        if (max_len < tlines[row].ssize())   max_len = tlines[row].ssize();
      }

Shape shape_Z;
   shape_Z.add_shape_item(tlines.size());
   shape_Z.add_shape_item(max_len);

Value_P Z(shape_Z, LOC);
   loop(row, tlines.size())
      {
        const UCS_string & line = tlines[row];
        loop(col, line.size())
            Z->next_ravel_Char(line[col]);

        loop(col, max_len - line.size())
            Z->next_ravel_Char(UNI_SPACE);
      }

   Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, Z);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR5_6(int A_5_6, cValue_R B)
{
const char * alpha = (A_5_6 == 5) ? "0123456789ABCDEF" : "0123456789abcdef";
Shape shape_Z(B.get_shape());
   if (shape_Z.get_rank() == 0)   // scalar B
      {
        shape_Z.add_shape_item(2);
      }
   else
      {
        shape_Z.set_shape_item(B.get_rank() - 1, B.get_cols()*2);
      }

Value_P Z(shape_Z, LOC);

   loop(b, B.element_count())
       {
         const int val = B.get_byte_value(b) & 0x00FF;
         const int h = alpha[val >> 4];
         const int l = alpha[val & 0x0F];
         Z->next_ravel_Char(Unicode(h));
         Z->next_ravel_Char(Unicode(l));
       }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR10(cValue_R B)
{
   // cannot use PrintBuffer here because the lines in ucs_vec
   // have different lengths

   // collect the APL code that produces B in ucs_vec
   //
UCS_string_vector ucs_vec;
   do_CR10(ucs_vec, &B);

Value_P Z(ucs_vec.size(), LOC);
   loop(line, ucs_vec.size())
      {
         Value_P Z_line(ucs_vec[line], LOC);
         Z->next_ravel_Pointer(Z_line.get());
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::do_CR10(UCS_string_vector & result, const cValue * B)
{
   // B is the name of a variable or function.
   // result shall be the APL code that produces it.
   //
const UCS_string symbol_name(*B);
const Symbol * symbol = Workspace::lookup_existing_symbol(symbol_name);
   if (symbol == 0)
      {
        MORE_ERROR() << "10⎕CR: bad Symbol in do_CR10()";
        DOMAIN_ERROR;
      }

   switch(symbol->get_NC())
      {
        case NC_VARIABLE:
             {
               // keep the Value_P alive: some system variables (e.g. ⎕SYL)
               // compute a fresh value in get_apl_value().
               //
               const Value_P value = symbol->get_apl_value();
               do_CR10_variable(result, symbol_name, value.get());
               return;
             }

        case NC_FUNCTION:
        case NC_OPERATOR:
             {
               const Function & ufun = *symbol->get_function();

               // Bugs28 #92: 1⎕CR/2⎕CR/8⎕CR/9⎕CR/11⎕CR already suppress
               // a locked function's body (get_exec_properties()[0],
               // see the "function = 0" case above) and show only its
               // name -- do_CR10() had no such check at all, disclosing
               // the full body (with a ⍫ closer to mark it locked, but
               // the body was there regardless).
               //
               // the bare name is not an expression that produces the
               // function: DOMAIN ERROR like 2 ⎕TF (Bugs37 #74)
               //
               if (ufun.get_exec_properties()[0])
                  {
                    MORE_ERROR() << "10 ⎕CR: '" << symbol_name
                                 << "' is locked; its definition cannot be"
                                    " disclosed.";
                    DOMAIN_ERROR;
                  }

               const UCS_string text = ufun.canonical(false);
               if (ufun.is_lambda())
                  {
                    UCS_string res = symbol->get_name();
                    res << UNI_LEFT_ARROW << UNI_L_CURLY;
                    int t = 0;
                    while (t < text.ssize())   // skip λ header
                       {
                         const Unicode uni = text[t++];
                         if (uni == UNI_LF)   break;
                       }

                    UCS_string body;
                    while (t < text.ssize())   // copy body
                        {
                          const Unicode uni = text[t++];
                          if (uni == UNI_LF)   break;
                           body << uni;
                        }

                    // text (ufun.canonical()) already has the λ← that
                    // Executable::compute_lambda_body() synthesizes
                    // before the last statement baked in as ordinary
                    // source text (a lambda has no separate "original
                    // source" the way a ∇-function does). Emitting it
                    // unstripped meant the tokenizer synthesized a
                    // SECOND λ← over it on the next )LOAD/⎕FX, growing
                    // by one λ← on every )DUMP/)LOAD cycle. Find the
                    // same insertion point compute_lambda_body() used
                    // (after the last top-level ◊, or the very start if
                    // none) and strip it back out here, mirroring that
                    // function's own (deliberately simple, not string-
                    // or brace-aware) ◊ scan exactly. See Bugs27 #37.
                    //
                    ShapeItem sols = 0;
                    for (ShapeItem j = body.size() - 1; j >= 0; --j)
                        {
                          if (Avec::is_DIAMOND(body[j]))
                             {
                               sols = j + 1;
                               while (sols < body.ssize() &&
                                      body[sols] <= UNI_SPACE)   ++sols;
                               break;
                             }
                        }
                    if (sols + 1 < body.ssize() &&
                        body[sols] == UNI_LAMBDA &&
                        body[sols + 1] == UNI_LEFT_ARROW)
                       {
                         body.erase(sols);   // erase() removes 1 char;
                         body.erase(sols);   // call twice for "λ←"
                       }

                    res << body;
                    res << (UNI_R_CURLY);
                    result.push_back(res);
                  }
               else
                  {
                    UCS_string res(UNI_NABLA);

                    loop(u, text.ssize())
                       {
                         if (text[u] == '\n')
                             {
                               result.push_back(res);
                               res.clear();
                               UCS_string next(text, u+1, text.size()-(u+1));
                               if (!next.is_comment_or_label() &&
                                   u < (text.ssize() - 1))   res << UNI_SPACE;
                             }
                         else
                             {
                               res << text[u];
                             }
                       }
                    res << (ufun.get_exec_properties()[0]
                         ? UNI_DEL_TILDE : UNI_NABLA);
                    result.push_back(res);
                  }
               return;
             }

        default: MORE_ERROR() << "⎕RVAL: bad symbol->get_NC() in do_CR10()";
                 DOMAIN_ERROR;
      }
}
//════════════════════════════════════════════════════════════════════════════
const char *
Quad_CR::do_CR10_structured(UCS_string_vector & result,
                            const UCS_string & var_name,
                            const cValue * value)
{
   if (value->get_rank() != 2)   return "bad rank (structured variable)";
   if (value->get_cols() != 2)   return "bad shape (structured variable)";

   loop(r, value->get_rows())
       {
         Cell cache_m;
         const Cell & member_cell = value->get_cravel(2*r, cache_m);
         Value_P member_name_v = member_cell.try_pointer_value();
         if (!member_name_v)   continue;
         const cValue * member_name = member_name_v.get();

         // unused member entries are integer 0.
         //
         if (!member_name->is_char_string())   continue;

         const UCS_string member_ucs(*member_name);
         UCS_string member_path = var_name;
         member_path << UNI_FULLSTOP << member_ucs;

         Cell cache_d;
         const Cell & data_cell = value->get_cravel(2*r + 1, cache_d);
         if (data_cell.is_simple_cell())
            {
              member_path << UNI_LEFT_ARROW;
              const UCS_string data_value = do_CR10_simple_cell(data_cell);
              member_path << data_value;
              result.push_back(member_path);
            }
         else
            {
              do_CR10_variable(result, member_path,
                               data_cell.get_pointer_value().get());
            }
       }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::do_CR10_level(UCS_string_vector & result, size_t level,
                       const cValue & value)
{
UCS_string text;
UCS_string indent(2*(level + 1), UNI_SPACE);
UCS_string var_level;   var_level << TMP_VAR(level); 
UCS_string ind_var_level = indent;   ind_var_level << var_level;

   text << ind_var_level << "←";

   // important special case: string
   //
   if (value.element_count() < int(60 - 2*level) && is_plain_string(&value))
      {
        text << "\"";
        loop(e, value.element_count())
            text << value.get_char_value(e);
        text << "\"";                                                PUSH_TEXT
        return;
      }

   text << value.nz_element_count()
        << "⍴0   ⍝ L" << int(level) << " zeroes";                         PUSH_TEXT

int nested_count = 0;
int simple_count = 0;
int zero_count  = 0;
   loop(v, value.nz_element_count())
       {
         Cell cache;
         const Cell & cell = value.get_cravel(v, cache);
         if (cell.is_pointer_cell())
            {
              ++nested_count;
              continue;
            }
         if (cell.is_near_zero())
            {
              ++zero_count;
            }
         else
            {
              ++simple_count;
            }
       }

   if (simple_count)
      {
        // if all items of value are numeric, then we can safely use ' '.
        // as item separator. Otherwise we need commas (which is less
        // efficient)
        //
        text << indent << "⍝ L" << int(level) << ": ";
        if (zero_count)   text << zero_count << "+";
        text << simple_count << " simple item(s)...";                PUSH_TEXT
        const ShapeItem ec = value.nz_element_count();
        for (ShapeItem e = 0; e < ec;)
            {
              Cell cache;
              const Cell & cell = value.get_cravel(e, cache);
              if (cell.is_pointer_cell())
                 {
                   ++e;
                   continue;
                 }

              UCS_string U1;   // ((⊃V    (fixed size)
              UCS_string U2;   // indices (variable size)
              UCS_string U3;   // ])←     (fixed size)
              UCS_string U4;   // items   (variable size)

              // code for one item
              //
              U1 << var_level << "[";
              U2 << e;
              U3 << "]←";
              U4 << do_CR10_simple_cell(cell);
              ++e;

              UCS_string item_text;
              item_text << U1 << U2 << U3 << U4;

             const int line_limit = 72 - indent.size() - U1.size() - U3.size();

             // fill U2 and U4 with more items until line_limit is reached...
             //
             bool has_string = false;
             int count = 1;        // from first item above
             int e_from = e - 1;   // dito.
             while (e < ec && (U2.ssize() + U4.ssize()) < line_limit)
                   {
                     const ShapeItem e0 = e++;
                     Cell cache2;
                     const Cell & cell = value.get_cravel(e0, cache2);
                     if (cell.is_pointer_cell())   continue;

                     U2 << " " << e - 1;   // - 1 since e++ above
                     ++count;
                     const UCS_string text_e = do_CR10_simple_cell(cell);
                     if (U4.back() == UNI_SINGLE_QUOTE &&
                         text_e.front() == UNI_SINGLE_QUOTE)
                        {
                          U4.pop_back();   // trailing '
                          U4 << UCS_string(text_e, 1, text_e.size() - 1);
                          has_string = true;
                        }
                     else
                        {
                          U4 << Invalid_Unicode << text_e;   // for now
                        }
                   }

              // now we know if we need commas instead of spaces
              //
              loop(u, U4.size())
                  {
                    if (U4[u] == Invalid_Unicode)
                       U4[u] = has_string ? UNI_COMMA : UNI_SPACE;
                  }

              if (count > 3 && count == (e - e_from))   // consecutive indices
                 {
                   U2.clear();
                   U2 << e_from << "+⍳" << count;
                 }

              text << indent << U1 << U2 << U3 << U4;                PUSH_TEXT
            }
      }

   if (nested_count)
      {
        text << indent << "⍝ L" << int(level) << ": " << nested_count
             << " nested item(s)...";                                PUSH_TEXT
        loop(v, value.nz_element_count())
            {
              if (!value.is_pointer_cell(v))   continue;
              const cValue & sub_val = *value.get_pointer_value(v);
              do_CR10_level(result, level + 1, sub_val);

              text << indent << var_level << "[" << v << "]←⊂"
                   << TMP_VAR(level + 1);                            PUSH_TEXT
            }
      }

   // final reshape
   //
const Shape & shape = value.get_shape();
   if (shape.get_rank() != 1 || shape.get_volume() == 0)
      {
        text << ind_var_level << "←" <<  do_CR10_shape(shape)
             << "⍴" << var_level
             << "   ⍝ L" << int(level) << " final reshape";          PUSH_TEXT
      }
   else
      {
        text << indent << "⍝ no (need for) L" << int(level)
             << " final reshape";                                    PUSH_TEXT
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_CR::do_CR10_simple_cell(const Cell & cell)
{
UCS_string text;
   if (cell.is_character_cell())
      {
        // be careful with non-printable characters and quotes
        //
        const Unicode uni = cell.get_char_value();
        if (uni == UNI_SINGLE_QUOTE)   return text << "''''";

        // Tokenizer::tokenize_string1() treats U+2018/U+2019 ('smart
        // quotes') as equivalent to the ASCII quote when scanning for an
        // escaped '' pair inside a '...' string, but always reconstructs
        // that escape as plain U+0027 — so a literal U+2018/U+2019 can
        // never round-trip through a '...' string at all (unlike U+0027,
        // which the doubling above does handle). Fall back to (⎕UCS n)
        // for those, same as for other characters that don't fit '...'.
        //
        if (uni < ' ' || uni == 127 || Avec::is_single_quote(uni))
           {
             return text << "(⎕UCS " << int(uni) << UNI_R_PARENT;
           }
        return text << "'" << uni << "'";
      }

   if (cell.is_integer_cell())   return text << cell.get_int_value();
   if (cell.is_real_cell())      return do_CR10_double(cell.get_real_value());
   if (cell.is_complex_cell()) return do_CR10_double(cell.get_real_value())
                                    << UNI_J
                                    << do_CR10_double(cell.get_imag_value());
   FIXME;   // cell is not simple
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_CR::do_CR10_double(double num)
{
   // 10 ⎕CR must reconstruct the exact value of num, therefore (unlike
   // UCS_string::operator <<(double), which trims 'near-int' values to
   // 2 significant digits for compact display) we never trade away
   // precision. We do, however, use the shortest of %.15g/%.16g/%.17g
   // that still round-trips exactly, so that 'nice' values like 3.1415
   // are not needlessly spelled out to full (17 digit) double precision.
   //
char cc[40];
   for (int prec = 15; prec <= 17; ++prec)
       {
         SPRINTF(cc, "%.*g", prec, num);
         if (strtod(cc, 0) == num)   break;
       }

UCS_string result;
   loop(c, sizeof(cc))
       {
         const char digit = cc[c];
         if (digit == 0)           break;
         if (digit == 'e')         result << UNI_E;
         else if (digit == '-')    result << UNI_OVERBAR;
         else if (digit == '+')    ;   // APL exponents have no '+' sign
         else                      result << Unicode(digit);
       }

   return result;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_CR::do_CR10_shape(const Shape & shape)
{
   if (shape.get_rank() == 0)   return UCS_string(UNI_ZILDE);   // scalar

UCS_string result;
   loop(r, shape.get_rank())
       {
         if (r)   result << UNI_SPACE;
         result << shape.get_shape_item(r);
       }
   return result;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR14(cValue_R B)
{
const char * hex = "0123456789abcdef";
CDR_string cdr;
   CDR::to_CDR(cdr, &B);

const ShapeItem len = cdr.size();
Value_P Z(2*len, LOC);
   loop(l, len)
       {
         const Unicode uh = Unicode(hex[0x0F & cdr[l] >> 4]);
         const Unicode ul = Unicode(hex[0x0F & cdr[l]]);
         Z->next_ravel_Char(uh);
         Z->next_ravel_Char(ul);
       }
   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR15(cValue_R B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

   // an odd length is a LENGTH ERROR, like for 13 ⎕CR (Bugs37 #73)
   //
   if (B.element_count() & 1)   LENGTH_ERROR;

CDR_string cdr;
const ShapeItem len = B.element_count()/2;
ShapeItem bI = 0;
   loop(b, len)
       {
         const int n1 = nibble(B.get_char_value(bI++));
         const int n2 = nibble(B.get_char_value(bI++));
         if (n1 < 0 || n2 < 0)   DOMAIN_ERROR;
         cdr.push_back(16*n1 + n2);
       }

   Value_P Z = CDR::from_CDR(cdr, LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR16(cValue_R B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

const ShapeItem full_quantums = B.element_count() / 3;
const ShapeItem len_Z = 4 * ((B.element_count() + 2) / 3);
Value_P Z(len_Z, LOC);

const char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

ShapeItem bI = 0;
   loop(b, full_quantums)   // encode full quantums
      {
        /*      -- b1 -- -- b2 -- -- b3 --
             z: 11111122 22223333 33444444
         */
        const int b1 = B.get_char_value(bI++) & 0x00FF;
        const int b2 = B.get_char_value(bI++) & 0x00FF;
        const int b3 = B.get_char_value(bI++) & 0x00FF;

        const int z1 = b1 >> 2;
        const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;
        const int z3 = (b2 & 0x0F) << 2 | (b3 & 0xC0) >> 6;
        const int z4 =  b3 & 0x3F;

        Z->next_ravel_Char(Unicode(alpha[z1]));
        Z->next_ravel_Char(Unicode(alpha[z2]));
        Z->next_ravel_Char(Unicode(alpha[z3]));
        Z->next_ravel_Char(Unicode(alpha[z4]));
      }

   // process final bytes
   //
   switch(B.element_count() - 3*full_quantums)
      {
        case 0: break;   // length of B is 3 * N

        case 1:          //  length of B is 3 * N + 1
                {
                  const int b1 = B.get_char_value(bI++) & 0x00FF;
                  const int b2 = 0;

                  const int z1 = b1 >> 2;
                  const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;

                  Z->next_ravel_Char(Unicode(alpha[z1]));
                  Z->next_ravel_Char(Unicode(alpha[z2]));
                  Z->next_ravel_Char(Unicode('='));
                  Z->next_ravel_Char(Unicode('='));
                }
                break;

        case 2:          // two bytes remaining
                {
                  const int b1 = B.get_char_value(bI++) & 0x00FF;
                  const int b2 = B.get_char_value(bI++) & 0x00FF;
                  const int b3 = 0;

                  const int z1 = b1 >> 2;
                  const int z2 = (b1 & 0x03) << 4 | (b2 & 0xF0) >> 4;
                  const int z3 = (b2 & 0x0F) << 2 | (b3 & 0xC0) >> 6;

                  Z->next_ravel_Char(Unicode(alpha[z1]));
                  Z->next_ravel_Char(Unicode(alpha[z2]));
                  Z->next_ravel_Char(Unicode(alpha[z3]));
                  Z->next_ravel_Char(Unicode('='));
                }
                break;
      }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR17(cValue_R B)
{
   if (B.get_rank() != 1)   RANK_ERROR;

const int cols = B.get_cols();
   if (cols == 0)   return Str0(LOC);  // empty value
   if (cols & 3)    LENGTH_ERROR;      // length not 4*n

   // figure number of missing chars in final quantum. Check cols-1 (the
   // true last character) first: a lone '=' at cols-2 with a non-'=' at
   // cols-1 (e.g. "AA=A") is not padding at all, just a stray '=' in the
   // last quantum's 3rd position -- checking cols-2 first (as before)
   // mistook it for 2 missing bytes and silently derived the wrong
   // length (Blake McBride, Bugs12.md #8). The loop below rejects that
   // case explicitly once missing is (correctly) 0 here.
   //
int missing = 0;
   if (B.get_char_value(cols - 1) == '=')
      {
        missing = 1;
        if (B.get_char_value(cols - 2) == '=')   missing = 2;
      }

const ShapeItem len_Z = 3 * (B.element_count() / 4) - missing;
const ShapeItem quantums = B.element_count() / 4;

Value_P Z(len_Z, LOC);
ShapeItem bI = 0;
   loop(q, quantums)
       {
         const int b1 = sixbit(B.get_char_value(bI++));
         const int b2 = sixbit(B.get_char_value(bI++));
         const int q3 = sixbit(B.get_char_value(bI++));
         const int q4 = sixbit(B.get_char_value(bI++));
         const int b3 = q3 & 0x3F;
         const int b4 = q4 & 0x3F;

         /*    b1       b2       b3       b4
            --zzzzzz --zzzzzz --zzzzzz --zzzzzz
              111111   112222   223333   333333
          */
         const int z1 =  b1 << 2         | (b2 & 0x30) >> 4;
         const int z2 = (b2 & 0x0F) << 4 | b3 >> 2;
         const int z3 = (b3 & 0x03) << 6 | b4;

         if (b1 < 0 || b2 < 0 || q3  == -1 || q4 == -1)   DOMAIN_ERROR;

         // '=' (sixbit() == 64, RFC 4648's pad character) is only valid
         // as one or both of the last quantum's trailing characters
         // (checked against 'missing' above, which was itself derived
         // from those same trailing characters); anywhere else -- b1/b2
         // of any quantum, or q3/q4 of a non-last quantum, or q3/q4 of
         // the last quantum in excess of what 'missing' says -- it is a
         // malformed encoding, not valid padding (Bugs12.md #8).
         //
         if (b1 == 64 || b2 == 64)   DOMAIN_ERROR;
         if (q < (quantums - 1))
            {
              if (q3 == 64 || q4 == 64)   DOMAIN_ERROR;
            }
         else
            {
              if (missing == 0 && (q3 == 64 || q4 == 64))   DOMAIN_ERROR;
              if (missing == 1 && q3 == 64)                 DOMAIN_ERROR;
            }

         if (q < (quantums - 1) || missing == 0)
            {
              Z->next_ravel_Char(Unicode(z1));
              Z->next_ravel_Char(Unicode(z2));
              Z->next_ravel_Char(Unicode(z3));
            }
         else if (missing == 1)   // k6 k2l4 l20000 =
            {
              Z->next_ravel_Char(Unicode(z1));
              Z->next_ravel_Char(Unicode(z2));
            }
         else                     // k6 k20000 = =
            {
              Z->next_ravel_Char(Unicode(z1));
            }
       }

   Z->set_proto_Spc();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR18(cValue_R B)
{
UCS_string ucs(B);
UTF8_string utf(ucs);
const ShapeItem length = utf.size();
Value_P Z(length, LOC);

   loop(l, length)   Z->next_ravel_Int(utf[l] & 0xFF);
   Z->set_proto_Int();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR19(cValue_R B)
{
   if (B.get_rank() > 1)   RANK_ERROR;
const ShapeItem len_B = B.element_count();

   // len_B is user-controlled and can be huge, so the byte buffer is
   // heap-allocated (not ALLOCA()'d on the stack).
   //
std::vector<UTF8> bytes_utf_vec(len_B + 10);
UTF8 * bytes_utf = bytes_utf_vec.data();
   loop(b, len_B)   bytes_utf[b] = B.get_byte_value(b);
   bytes_utf[len_B] = 0;

const UTF8_string utf(bytes_utf, len_B);
const UCS_string ucs(utf);
Value_P Z(ucs, LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR26(cValue_R B)
{
const ShapeItem len = B.element_count();
Value_P Z(B.get_shape(), LOC);
   loop(l, len)
      {
        if (Value_P v = B.try_pointer_value(l))
           {
             Value_P Z_sub = do_CR26(*v);
             Z->next_ravel_Pointer(Z_sub.get());
           }
        else
           {
             Z->next_ravel_Int(B.get_cell_type(l));
           }
      }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR27_28(int A_27_28, cValue_R B)
{
const ShapeItem len = B.element_count();
Value_P Z(B.get_shape(), LOC);

   if (A_27_28 == 28)
      {
        // 28 ⎕CR B: secondary value of each Cell (imaginary part for complex,
        // denominator for rational, 0 for all other types incl. packed int/char).
        loop(z, len)
            {
              Cell cache;
              const Cell & cB = B.get_cravel(z, cache);
              APL_Integer data = 0;
              if (Value_P v = cB.try_pointer_value())
                 {
                   Value_P Z_sub = do_CR27_28(28, *v);
                   Z->next_ravel_Pointer(Z_sub.get());
                   continue;
                 }
              if (cB.get_cell_type() == CT_COMPLEX)
                 memcpy(&data, cB.get_u1(), sizeof(data));
              else if (cB.get_cell_type() == CT_CELLREF)
                 {
                   data = APL_Integer(cB.get_cell_owner());
                 }
#ifdef cfg_RATIONAL_NUMBERS_WANTED
              // get_denominator() (FloatCell's own, real implementation
              // under cfg_RATIONAL_NUMBERS_WANTED -- see FloatCell.hh):
              // a rational value is encoded as a FloatCell variant (a
              // union of a plain double vs. a numerator/denominator
              // pair, distinguished by denominator != 0), not a separate
              // cell type -- there is no third numeric cell type to
              // check for here. The previous code instead memcpy'd
              // FloatCell's raw union bits unconditionally (giving
              // nonsense for an ordinary, non-rational FloatCell, whose
              // union holds a plain double, not a numerator) and gave a
              // bare "data = 1" for EVERY IntCell regardless of value --
              // IntCell never represents a rational at all, so it must
              // always report 0 here like every other non-rational,
              // non-complex type already correctly does (see this
              // function's own doc comment above: "0 for all other
              // types incl. packed int/char"). Reproduced live: Bill
              // Heagy, 28⎕CR of a plain int/nested-int B wrongly showed
              // 1 (or a memcpy'd double reinterpreted as an int) for
              // every numeric item instead of 0, but only in a build
              // configured with RATIONAL_NUMBERS_WANTED=yes.
              //
              else if (cB.get_cell_type() == CT_FLOAT)
                 data = cB.get_denominator();
#endif
              Z->next_ravel_Int(data);
            }
        Z->check_value(LOC);
        return Z;
      }

   // 27 ⎕CR B: primary value of each item.
   // For packed ravels, read directly from packed storage (bypasses Cell fetchers).
   switch (B.get_ravel_type())
      {
        case RPT_INT64:
           { const int64_t * pB = B.cravel_int64();
             loop(z, len)   Z->next_ravel_Int(pB[z]);
             Z->check_value(LOC);
             return Z;
           }

        case RPT_FLOAT64:
           { const double * pB = B.cravel_float64();
             loop(z, len)
                 {
                   APL_Integer bits = 0;
                   memcpy(&bits, pB + z, sizeof(bits));
                   Z->next_ravel_Int(bits);
                 }
             Z->check_value(LOC);
             return Z;
           }

        case RPT_COMPLEX:
           { const double * pB = B.cravel_complex();   // re0 im0 re1 im1 ...
             loop(z, len)
                 {
                   APL_Integer bits = 0;
                   memcpy(&bits, pB + 2*z, sizeof(bits));   // real part
                   Z->next_ravel_Int(bits);
                 }
             Z->check_value(LOC);
             return Z;
           }

        case RPT_BOOL:
           { const uint64_t * pB = B.cravel_bool();
             loop(z, len)
                 Z->next_ravel_Int((pB[z >> 6] >> (z & 63)) & 1);
             Z->check_value(LOC);
             return Z;
           }

        case RPT_UNICODE16:
           { const uint16_t * pB = B.cravel_unicode16();
             loop(z, len)   Z->next_ravel_Int(pB[z]);
             Z->check_value(LOC);
             return Z;
           }

        case RPT_UNICODE32:
           { const Unicode * pB = B.cravel_unicode32();
             loop(z, len)   Z->next_ravel_Int(pB[z]);
             Z->check_value(LOC);
             return Z;
           }

        default:   // RPT_CELLS: use the Cell-by-Cell path
           break;
      }

   loop(z, len)
       {
         Cell cache;
         const Cell & cB = B.get_cravel(z, cache);
         if (Value_P v = cB.try_pointer_value())
            {
              Value_P Z_sub = do_CR27_28(27, *v);
              Z->next_ravel_Pointer(Z_sub.get());
            }
         else
            {
              APL_Integer data = 0;
              if (cB.get_cell_type() == CT_CHAR)
                 data = cB.get_char_value();
              else if (cB.get_cell_type() == CT_CELLREF)
                 data = APL_Integer(cB.get_lval_value());
              else
                 memcpy(&data, cB.get_u0(), sizeof(data));
              Z->next_ravel_Int(data);
            }
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR48(cValue_R B)
{
   return IntScalar(B.get_ravel_type(), LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR49(cValue_R B)
{
   // the *effective* threshold, which ⎕SYL[34;2] and --pack_min can
   // change at runtime -- not the compile-time default
   // Value::PACKED_MINIMUM_LENGHT that it is merely initialised from
   // (Blake McBride, Bugs24 #4).
   return IntScalar(Quad_SYL::pack_min_length, LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::hex_of_int_cell(int A_50_51, const Cell & cB, const char * idx_txt)
{
   // is_near_int() alone is not enough: ComplexCell::is_near_int() checks
   // that the real *and* imaginary parts are each individually integral,
   // which is true (and misleading) for e.g. ¯1J1 -- both -1 and 1 are
   // "near an int", but ¯1J1 is not a real integer. Reject complex cells
   // outright.
   if (cB.is_complex_cell() || !cB.is_near_int())
      {
        MORE_ERROR() << "50/51 ⎕CR B : B" << idx_txt
                     << " is not an integer";
        DOMAIN_ERROR;
      }

const APL_Integer value = cB.get_near_int();
const char * format = (A_50_51 == 50) ? "%0*llX" : "%0*llx";
char cc[24];

   if (value >= 0)
      {
        // no leading zeros -- except for 0 itself, which %llX/%llx
        // already renders as "0" rather than an empty string.
        SPRINTF(cc, (A_50_51 == 50) ? "%llX" : "%llx", ulong_long(value));
      }
   else
      {
        // negative: two's complement, using the fewest whole BYTES (not
        // just nibbles) that both represent the value correctly and keep
        // the top bit set (so it reads as negative); byte-alignment is
        // what guarantees an even digit count, and the digits *above*
        // the value's own minimal width come out as 'F' automatically,
        // since two's-complement sign-extension of a negative number is
        // all-1-bits. E.g. ¯1 → FF (1 byte; -1 alone would need just one
        // nibble, 0xF, but that is not byte-aligned, so it widens to
        // 0xFF), ¯200 → FF38 (2 bytes; -200 does not fit in 1 signed
        // byte).
        //
        int nbytes = 1;
        for (; nbytes < 8; ++nbytes)
           {
             const int64_t half = int64_t(1) << (8*nbytes - 1);
             if (value >= -half && value <= half - 1)   break;
           }

        const uint64_t mask = (nbytes == 8) ? ~0ULL
                                            : (1ULL << (8*nbytes)) - 1;
        const uint64_t bits = uint64_t(value) & mask;
        SPRINTF(cc, format, 2*nbytes, ulong_long(bits));
      }

   return Value_P(UCS_ASCII_string(cc), LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR50_51(int A_50_51, cValue_R B)
{
   // elementwise (rather than 5/6's whole-vector) integer → hex string.
   // A scalar B converts to a plain (non-nested) string; a non-scalar B
   // converts to a same-shaped Z with one nested hex string Z[IDX] per
   // B[IDX] (recursing into nested items of B the same way do_CR26()
   // does, and applying the same scalar-B-is-a-plain-string rule at
   // every recursion level).
   //
   if (B.is_scalar())
      {
        Cell cache;
        return hex_of_int_cell(A_50_51, B.get_cscalar(cache), "");
      }

Value_P Z(B.get_shape(), LOC);

   loop(b, B.element_count())
       {
         Cell cache;
         const Cell & cB = B.get_cravel(b, cache);
         if (Value_P v = cB.try_pointer_value())
            {
              Value_P Z_sub = do_CR50_51(A_50_51, *v);
              Z->next_ravel_Pointer(Z_sub.get());
              continue;
            }

         char idx_txt[24];   // "[" + up to 20 digits/sign (int64) + "]" + NUL
         SPRINTF(idx_txt, "[%lld]", long_long(b));
         Value_P Z_sub = hex_of_int_cell(A_50_51, cB, idx_txt);
         Z->next_ravel_Pointer(Z_sub.get());
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
uint32_t
Quad_CR::crc32_of(cValue_R B)
{
   if (B.get_rank() > 1)   RANK_ERROR;

const ShapeItem len_B = B.element_count();

uint32_t crc = 0xFFFFFFFFU;
   loop(b, len_B)
       // B.get_byte_value(b) throws DOMAIN ERROR if B[b] is not a byte
       // (integer in [-128, 255], same convention as do_CR33()/do_CR34())
       crc = apl_crc::crc32_update(crc, uint8_t(B.get_byte_value(b)));

   return crc ^ 0xFFFFFFFFU;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR52(cValue_R B)
{
   return IntScalar(APL_Integer(crc32_of(B)), LOC);
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR53(cValue_R B)
{
const uint32_t crc = crc32_of(B);

char cc[9];
   SPRINTF(cc, "%8.8X", crc);

Value_P Z(8, LOC);
   loop(z, 8)   Z->next_ravel_Char(Unicode(cc[z]));
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR30(cValue_R B)
{
   // Z is B with all items conformed to the same rank and shape. Primarily
   // an internal function used in macros Z__LO_RANK_X5_B and Z__A_LO_RANK_X7_B
   // but possibly useful elsewhere.

const ShapeItem len_B = B.element_count();
   if (len_B == 0)   return CLONE(&B, LOC);

   // we use 'ShapeItem max_shape[MAX_RANK] max_shape' instead of
   // 'Shape max_shape' to avoid multiple recompute_volume() in class Shape
   // while looping along the B ravel.
   //
ShapeItem max_shape[MAX_RANK];
   loop(r, MAX_RANK)   max_shape[r] = 0;
sRank max_rank = 0;

   loop(b, len_B)
      {
        if (B.is_lval_cell(b))   DOMAIN_ERROR;
        if (!B.is_pointer_cell(b))   continue;   // simple scalar

        // the items are expanded to max_rank by prepending 1s (see below),
        // so their shapes are aligned on the trailing axes: max_shape[t]
        // is the maximum of the t'th axis counted from the END. Bugs37 #13.
        //
        const Shape sh = B.get_pointer_value(b)->get_shape();
        const sRank rk = sh.get_rank();
        if (max_rank < rk)   max_rank = rk;
        loop(t, rk)
           {
             const ShapeItem sh_t = sh.get_shape_item(rk - 1 - t);
             if (max_shape[t] < sh_t) max_shape[t] = sh_t;
           }
      }

Shape conformed;
   loop(r, max_rank)   conformed.add_shape_item(max_shape[max_rank - 1 - r]);
const ShapeItem conformed_len = conformed.get_volume();

Shape shape_Z(B.get_shape() + conformed);
Value_P Z(shape_Z, LOC);

   loop(b, len_B)
      {
        Cell cache;
        const Cell & cB = B.get_cravel(b, cache);
        if (Value_P v = cB.try_pointer_value())
           {
             // v may be shared with other values (variables, constants
             // in defined functions, structured variables), so the rank
             // must be expanded in a real copy (CLONE_P() under NEW_CLONE
             // is not one).
             //
             Value_P B_sub = v;
             if (v->get_rank() < conformed.get_rank())
                {
                  B_sub = v->clone(LOC);
                  Shape sh_sub = B_sub->get_shape();
                  sh_sub.expand_rank(conformed.get_rank());
                  B_sub->set_shape(sh_sub);
                }

             Value_P ZZ = Bif_F12_TAKE::do_take(conformed, *B_sub, false);
             loop(zz, conformed_len)
                 {
                   Cell cache_zz;
                   Z->next_ravel_Cell(ZZ->get_cravel(zz, cache_zz));
                 }
           }
        else if (conformed_len)   // simple scalar, and Z has room for it:
           {                      // conformed_len can be 0 when B mixes
                                   // an empty item with a scalar one (the
                                   // conformed shape is then the empty
                                   // item's), in which case Z has no
                                   // cells at all for this b -- writing
                                   // one unconditionally overflows Z's
                                   // allocation by one cell.
             Z->next_ravel_Cell(cB);
             loop(zz, (conformed_len - 1))   Z->next_ravel_0();
           }
      }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR31_32(int A_31_32, cValue_R B)
{
const ShapeItem len = B.element_count();
   if (len == 0)   LENGTH_ERROR;

Value_P Z(len, LOC);

PrintContext pctx = Workspace::get_PrintContext(PR_APL);
   pctx.set_style(PrintStyle(pctx.get_style() | PST_NO_FRACT_0));

   loop(b, len)
      {
        const cValue & row = *B.get_pointer_value(b);

        if (row.element_count() == 1)   // single item
           {
             Value_P Zrow = CLONE_P(row.get_pointer_value(0), LOC);
             Z->next_ravel_Pointer(Zrow.get());
             continue;
           }

        PrintBuffer pb;
        loop(col, row.element_count())
            {
              const cValue & item = *row.get_pointer_value(col);
              PrintBuffer pb_item(item, pctx, 0);
              pb.pad_height(UNI_SPACE, pb_item.get_row_count());
              if (A_31_32 == 31)   // align bottoms
                 pb_item.pad_height_above(UNI_SPACE, pb.get_row_count());
              else
                 pb_item.pad_height(UNI_SPACE, pb.get_row_count());
              pb.add_column(UNI_SPACE, 0, pb_item);
            }
        if (pb.get_row_count() == 1)
           {
             Value_P Zrow(pb.l1(), LOC);
             Z->next_ravel_Pointer(Zrow.get());
           }
        else
           {
             Value_P Zrow(pb, LOC);
             Z->next_ravel_Pointer(Zrow.get());
           }
      }

   Z->check_value(LOC);
   return Z;
}
//════════════════════════════════════════════════════════════════════════════
Value_P
Quad_CR::do_CR33(cValue_R B)
{
   // convert B = Integer Tag, len bytes Data
   // to      Z = 4-byte Tag, 4-byte Len, len bytes Data
   //
   if (B.get_rank() > 1)   RANK_ERROR;
const ShapeItem len_B = B.element_count();
   if (len_B < 1)   LENGTH_ERROR;
const ShapeItem len_B1 = len_B - 1;
   if (!B.is_integer_cell(0))   DOMAIN_ERROR;
   loop (b,  len_B1)   B.get_byte_value(b + 1);   // DOMAIN ERROR if not byte

Value_P Z(len_B + 7, LOC);
const APL_Integer tag = B.get_int_value(0);
   // the TLV header has 4 bytes, decoded as int32_t by 34 ⎕CR (Bugs37 #50)
   if (tag != int32_t(tag))
      {
        MORE_ERROR() << "33 ⎕CR B: the tag B[1] does not fit into the"
                        " 4-byte (signed) tag of the TLV header";
        DOMAIN_ERROR;
      }
    Z->next_ravel_Char(Unicode(tag >> 24 & 0xFF));
    Z->next_ravel_Char(Unicode(tag >> 16 & 0xFF));
    Z->next_ravel_Char(Unicode(tag >>  8 & 0xFF));
    Z->next_ravel_Char(Unicode(tag       & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >> 24 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >> 16 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1 >>  8 & 0xFF));
    Z->next_ravel_Char(Unicode(len_B1       & 0xFF));
    loop(z, len_B1)
        Z->next_ravel_Char(Unicode(B.get_byte_value(z+1)));

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR34(cValue_R B)
{
   // convert B = 4-byte Tag, 4-byte Len, len bytes Data
   // to      Z = Integer Tag, len bytes Data
   //
   //
   if (B.get_rank() != 1)   RANK_ERROR;
const ShapeItem len_B = B.element_count();
   if (len_B < 8)   LENGTH_ERROR;
   // throw DOMAIN ERROR if one of the vector items is not a byte
   loop(b, len_B)   B.get_byte_value(b);

ShapeItem bI = 0;
int32_t tag = 0;
   loop(bb, 4)   tag = tag << 8 | B.get_byte_value(bI++);

uint32_t len = 0;
   loop(bb, 4)   len = len << 8 | B.get_byte_value(bI++);
   if (len != (len_B - 8))   LENGTH_ERROR;

Value_P Z(len_B - 7, LOC);
   Z->next_ravel_Int(tag);
   loop(z, len_B - 8)   Z->next_ravel_Char(Unicode(B.get_byte_value(bI++)));

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR36(cValue_R B)
{
   if (B.get_rank() != 1)   RANK_ERROR;

   // a simple character item of B is a 1-character line, e.g. 'c' in
   // 36 ⎕CR 'ab' 'c' (Bugs37 #72)
   //
const ShapeItem len_B = B.element_count();
ShapeItem len_Z = B.element_count();
   loop(b, len_B)
       {
         if (B.is_character_cell(b))   { len_Z += 2;   continue; }
         const cValue & Bb = *B.get_pointer_value(b);
         if (Bb.get_rank() > 1)   RANK_ERROR;
         len_Z += 1 + Bb.element_count();
       }

UCS_string UZ;
   UZ.reserve(len_Z);
   loop(b, len_B)
       {
         if (B.is_character_cell(b))
            {
              UZ << B.get_char_value(b) << UNI_LF;
              continue;
            }
         const cValue & Bb = *B.get_pointer_value(b);
         UCS_string Ub(Bb);
         UZ << Ub << UNI_LF;
       }

Value_P Z(UZ, LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR38(cValue_R B)
{
   /*
      return a structured value with:

      i.  integer scalar B:   empty (i.e. with B empty rows)
      ii. 2×N matrix B:       members B[;1] and values B[;2]
   */

   if (B.is_scalar())   // return a structured value with B unused rows
      {
        APL_Integer capacity = B.get_near_int(0);
        if (capacity < 0)   DOMAIN_ERROR;
        if (capacity < 8)   return EmptyStruct(LOC);

        // round capacity up to next power of 2
        //
        APL_Integer p2;
        for (p2 = 8; p2 < capacity && p2 < 0x1000000000000;)   p2 += p2;
        if (capacity < p2)   capacity = p2;

        Shape shape_Z(ShapeItem(capacity), ShapeItem(2));
        Value_P Z(shape_Z, LOC);
        loop(c, capacity)
            {
              Z->next_ravel_0();
              Z->next_ravel_0();
            }

        Z->check_value(LOC);
        Z->set_member();
        return Z;
      }

   // convert unstructured array to structured value...
   //
   if (B.get_rank() != 2)   RANK_ERROR;
   if (B.get_cols() != 2)   LENGTH_ERROR;

const ShapeItem rows_B = B.get_rows();
ShapeItem valid_rows = B.get_member_count();

   // trust B's own row count as the capacity when it already looks like
   // one (Bugs30 #18): tf2_var() encodes a structured value's whole
   // member table, capacity rows and all, so recomputing capacity from
   // valid_rows here instead discarded that and silently grew it back
   // up from the (usually much smaller) valid-member count -- breaking
   // a 2⎕TF/⍎ round trip's ≡ and ⍴ even though )SAVE/)LOAD (which don't
   // go through this function) preserve capacity correctly. Only a
   // hand-built B with no such pre-sized capacity (too small, too few
   // rows for its own valid members, or not a power of 2) still falls
   // back to auto-sizing from valid_rows, as before.
   //
ShapeItem capacity = rows_B;
   if (capacity < 8 || capacity < valid_rows
       || (capacity & (capacity - 1)) != 0)
      {
        for (capacity = 8; capacity < valid_rows ;)   capacity += capacity;
        capacity += capacity;   // one more to use ≤ 50%
      }

const Shape shape_Z(capacity, 2);
Value_P Z(shape_Z, LOC);

   // fill with unused
   loop(c, capacity)
       {
         Z->next_ravel_0();
         Z->next_ravel_0();
       }

   loop(r, rows_B)
      {
        if (B.is_character_cell(2*r))   // valid row (1-character member)
           {
             UCS_string name(B.get_char_value(2*r));
             check_duplicate_member(*Z, name);
             Cell * data = Z->get_new_member(name);
             Cell cache;
             B.get_cravel(2*r + 1, cache).init_other(data, *Z, LOC);
           }
        else if (B.is_pointer_cell(2*r)) // valid row (string member)
           {
             UCS_string name(*B.get_pointer_value(2*r));
             check_duplicate_member(*Z, name);
             Cell * data = Z->get_new_member(name);
             Cell cache;
             B.get_cravel(2*r + 1, cache).init_other(data, *Z, LOC);
           }
      }

   Z->set_member();
   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR39(cValue_R B)
{
   // structured value B to array Z.
   //
   if (B.get_rank() != 2)   RANK_ERROR;
   if (B.get_cols() != 2)   LENGTH_ERROR;

const ShapeItem rows_B = B.get_rows();
const ShapeItem valid_rows = B.get_member_count();

const Shape shape_Z(valid_rows, 2);
Value_P Z(shape_Z, LOC);

   loop(r, rows_B)
       {
         Cell cache_name;
         const Cell & name_cell = B.get_cravel(2*r, cache_name);
         if (name_cell.is_integer_cell())   continue;   // unused row

         Z->next_ravel_Cell(name_cell);

         Cell cache_data;
         const Cell & data_cell = B.get_cravel(2*r + 1, cache_data);
         if (Value_P B_sub = data_cell.try_pointer_value())   // non-leaf or nested leaf
            {
              if (B_sub->is_member())   // non-leaf
                 {
                   Value_P B_struct = do_CR39(*B_sub);
                   Z->next_ravel_Pointer(B_struct.get());
                 }
              else                      // leaf
                 {
                   Z->next_ravel_Pointer(B_sub.get());
                 }
            }
         else                                               // leaf
            {
              Z->next_ravel_Cell(data_cell);
            }
       }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR40(cValue_R B)
{
   // return boolean B as packed boolean Z
   //
const ShapeItem B_len = B.element_count() ;
   if (B_len <= Value::PACKED_MINIMUM_LENGHT)
      {
        MORE_ERROR() << "Only Boolean Arrays with more than "
                     << Value::PACKED_MINIMUM_LENGHT << " items can be packed";
        LENGTH_ERROR;
      }

   // Create Z with a full Cell-sized buffer so that explode() is always safe.
   //
Value_P Z(B.get_shape(), LOC);
uint64_t * bits = reinterpret_cast<uint64_t *>(&Z->get_wfirst());

   // round B_len up to the next multiple of 64
   //
const ShapeItem Z_len = (B_len + 63) >> 6;   // length in units of uint64_t

   // set all bits to 0, then some to 1...
   //
   loop(z, Z_len)   bits[z] = 0;

uint64_t chunk = 0;
uint64_t bit  = 1;
   loop(b, B_len)
       {
         if (!B.is_near_bool(b))   DOMAIN_ERROR;
         if (B.get_near_int(b))   chunk |= bit;
         bit += bit;
         if (bit == 0)   // uint64_t bit complete
            {
              bits[b >> 6] = chunk;
              chunk = 0;
              bit = 1;
            }
       }

    if (chunk)   bits[Z_len - 1] = chunk;   // rest bits

   Z->commit_ravel_Bool(B_len);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR41(cValue_R B)
{
   if (!B.is_packed())
      {
        MORE_ERROR() << "B is not packed in 41 ⎕CR B";
        DOMAIN_ERROR;
      }

Value_P Z(B.get_shape(), LOC);

const ShapeItem B_len = B.element_count();

   loop(b, B_len)
      Z->next_ravel_Int(B.get_int_value(b));

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR42_43(cValue_R B, bool parse)
{
const UCS_string ucs(B);
Token_string tos;

   // Bugs28 #100(h): the DOMAIN_ERROR inside the "if (ec)" branch below
   // used to sit INSIDE the try block that catches parser.parse()'s own
   // exceptions -- so for a B like "((" (where parser.parse() returns a
   // nonzero ErrorCode instead of throwing), that DOMAIN_ERROR's own
   // throw was immediately caught by the very catch clause meant only
   // for exceptions escaping parser.parse() itself, which then built
   // and threw a *second* DOMAIN ERROR describing the first one --
   // shown twice, and with the second one's )MORE text embedding the
   // first's raw numeric ErrorCode instead of its name. Capture the
   // ErrorCode from either source (return value or caught exception)
   // first, then report it exactly once, outside any try/catch.
   //
ErrorCode ec = ErrorCode(0);

   if (parse)   // parse ucs
      {
        Parser parser(PM_EXECUTE, LOC, false);
        try   { ec = parser.parse(ucs, tos, /* optimize */ true); }
        catch (const Error & err)   { ec = err.get_error_code(); }

        if (ec)
           {
             MORE_ERROR() << "the parser returned error code: "
                          << Error::error_name(ec);
             DOMAIN_ERROR;
           }
      }
   else          // tokenize ucs
      {
        Tokenizer tokenizer(PM_FUNCTION, LOC, /* macro */ false);

        try   { tokenizer.tokenize(ucs, tos); }
        catch (const Error & err)
           {
             MORE_ERROR() << "the tokenizer returned error code: "
                          << Error::error_name(err.get_error_code());
             DOMAIN_ERROR;
           }
      }

Value_P Z(tos.size(), LOC);
   loop(t, tos.size())
       {
         const Token & tok = tos[t];
         const TokenTag tag = tok.get_tag();
         Value_P ZZ(2, LOC);
         ZZ->next_ravel_Int(tag);   // the token tag

         switch(tok.get_ValueType())
            {
              case TV_CHAR:
                   {
                     ZZ->next_ravel_Char(tok.get_char_val());
                   }
                   break;

              case TV_INT:
                   {
                     ZZ->next_ravel_Int(tok.get_int_val());
                   }
                   break;

              case TV_FLT:
                   {
                     ZZ->next_ravel_Float(tok.get_flt_val());
                   }
                   break;

              case TV_CPX:
                   {
                     ZZ->next_ravel_Complex(tok.get_cpx_real(),
                                            tok.get_cpx_imag());
                   }
                   break;

              case TV_SYM:
                   {
                     const Symbol * sym = tok.get_sym_ptr();
                     Value_P Z2(sym->get_name(), LOC);
                     ZZ->next_ravel_Pointer(Z2.get());
                   }
                   break;

              case TV_FUN:
                   {
                     cFunction_P fun = tok.get_function();
                     Value_P Z2(fun->get_name(), LOC);
                     ZZ->next_ravel_Pointer(Z2.get());
                   }
                   break;

              case TV_VAL:
                   {
                     Value_P Z2 = tok.get_apl_val();
                     if (!Z2)   // null APL value
                        {
                          ZZ->next_ravel_0();
                        }
                     else
                        {
                          // not next_ravel_Pointer(): with optimize=true,
                          // Parser::parse() folds scalar literals into
                          // TOK_APL_VALUE* tokens holding a *simple*
                          // scalar, and next_ravel_Pointer() asserts
                          // !is_simple_scalar() (a simple scalar is
                          // always stored directly in a cell, never
                          // behind a PointerCell). next_ravel_Value()
                          // already handles both the simple-scalar and
                          // nested-scalar cases correctly.
                          //
                          ZZ->next_ravel_Value(Z2.get());
                        }
                   }
                   break;

              default: ZZ->next_ravel_0();       // only the token tag
            }

         ZZ->check_value(LOC);
         Z->next_ravel_Pointer(ZZ.get());
       }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
Value_P
Quad_CR::do_CR44(cValue_R B)
{
Value_P Z(B.get_shape(), LOC);

   if (B.get_rank() > 1)         RANK_ERROR;
   if (B.element_count() == 0)   LENGTH_ERROR;

   loop(b, B.element_count())
       {
          UCS_string ucs_z;
          Cell cache;
          decode_CR44(ucs_z, B.get_cravel(b, cache));
          Value_P ZZ(ucs_z, LOC);
          Z->next_ravel_Pointer(ZZ.get());
       }

   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::decode_CR44(UCS_string & result, const Cell & cB)
{
   if (cB.is_integer_cell())         // token tag
      {
        const APL_Integer b      = cB.get_int_value();
        const TokenTag tag       = TokenTag(b);
        const TokenClass cls     = TokenClass(int(tag) & int(TC_MASK));
        const TokenValueType typ = TokenValueType(int(tag) & int(TV_MASK));

        const UCS_ASCII_string tag_name(tag);
        const UCS_ASCII_string class_name(cls);
        const UCS_ASCII_string type_name(typ);
        result << tag_name << UNI_L_PARENT << class_name << UNI_COMMA
               << UNI_SPACE << type_name << UNI_R_PARENT;
      }
   else if (Value_P v2 = cB.try_pointer_value())    // token ←→ (tag, value)
      {
        const cValue & B2 = *v2;
        if (B2.get_rank() > 1)   RANK_ERROR;

        if (B2.element_count() != 2)   LENGTH_ERROR;

        Cell cache_val, cache_tag;
        const Cell & cVal        = B2.get_cravel(1, cache_val);
        const Cell & cTag        = B2.get_cfirst(cache_tag);   // the tag
        const TokenTag tag       = TokenTag(cTag.get_int_value());
        const TokenClass cls     = TokenClass(int(tag) & int(TC_MASK));
        const TokenValueType typ = TokenValueType(int(tag) & int(TV_MASK));

        const UCS_ASCII_string tag_name(tag);
        result << (tag_name) << UNI_L_PARENT << UNI_SPACE;

        switch(typ)
            {
               case TV_NONE:  switch(cls)
                                 {
                                   case TC_ASSIGN: result << UNI_LEFT_ARROW;
                                                   break;
                                   default:        result << UNI_MINUS;
                                 }
                              break;

               case TV_CHAR:  result << cVal.get_char_value();
                              break;

               case TV_INT:   result << cVal.get_int_value();
                              break;

               case TV_FLT:   result << cVal.get_real_value();
                              break;

               case TV_CPX:   result << cVal.get_real_value() << UNI_J
                                     << cVal.get_imag_value();
                              break;

               case TV_FUN:   
               case TV_SYM:   if (!cVal.is_pointer_cell())   DOMAIN_ERROR;
                              {
                                const cValue & symbol = *cVal.get_pointer_value();
                                result << UCS_string(symbol);
                              }
                              break;

               case TV_LIN:   result << UNI_L_BRACK << cVal.get_int_value()
                                     << UNI_R_BRACK;
                              break;

               case TV_VAL:   // optional shape
                              value_CR44(result, *cVal.get_pointer_value());
                              break;

               case TV_INDEX: result << "[index]";
                              break;


               default:       DOMAIN_ERROR;
            }

        result << UNI_SPACE << UNI_R_PARENT;
      }
   else
      {
        MORE_ERROR() <<
        "Invalid item in 44 ⎕CR B. Expect integer (tag) or (tag value) (token)";
        DOMAIN_ERROR;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::value_CR44(UCS_string & result, cValue_R value)
{
   // 1. shape prefix (unless scalar or vector)
   //
const uRank rank = value.get_rank();
const ShapeItem ec = value.element_count();
   if (rank > 1 || ec == 1)
      {
        loop(r, rank)
           {
             result << value.get_shape_item(r) << UNI_SPACE;
           }
         result.back() = UNI_RHO;
      }

   loop(e, ec)
       {
         if (result.size() > 60)   // long
            {
              result << " ...";
              break;
            }

          Cell cache;
          const  Cell & cell = value.get_cravel(e, cache);
          if (cell.is_character_cell())   // string or char
             {
               result << UNI_SINGLE_QUOTE << cell.get_char_value()
                      << UNI_SINGLE_QUOTE;
             }
          else if (cell.is_integer_cell())
             {
               result << cell.get_int_value();
             }
          else if (cell.is_float_cell())
             {
               result << cell.get_real_value();
             }
          else if (cell.is_complex_cell())
             {
               const APL_Float cr = cell.get_real_value();
               const APL_Float ci = cell.get_imag_value();
               if (ci == 0.0)
                  {
                    const APL_Float fl = floor(cr);
                    if (fl == cr && fl >= -9.0e18 && fl <= 9.0e18)
                       result << APL_Integer(fl);
                    else
                       result << cr;
                  }
               else
                  {
                    result << cr << UNI_J << ci;
                  }
             }
          else if (cell.is_pointer_cell())
             {
               result << "(...)";
             }
          else if (cell.is_lval_cell())
             {
               result << "(...)←";
             }
          else
             {
               FIXME;
             }

         result << UNI_SPACE;
       }

   result.pop_back();   // trailing blanf

}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::do_CR45(cValue_R B)
{
UCS_string prefix;   prefix << "├───";
   do_CR45_value(prefix, B);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::do_CR45_value(const UCS_string prefix, cValue_R B)
{
ostream & out = CERR;
UCS_string sub_prefix = prefix;   sub_prefix << "────";

   out << prefix << " " << voidP(&B) << endl;
   loop(b, B.nz_element_count())
       {
         if (Value_P v = B.try_pointer_value(b))
            {
              do_CR45_value(sub_prefix, *v);
            }
       }
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_CR::is_plain_string(const cValue * value)
{
   if (value->get_rank() != 1)   return false;   // not a vector
   loop(v, value->nz_element_count())
       {
         Cell cache;
         const Cell & cell = value->get_cravel(v, cache);
         if (!cell.is_character_cell())   return false;

         const Unicode uni = cell.get_char_value();
         if (uni < UNI_SPACE)           return false;   // control character
         if (uni == UNI_SINGLE_QUOTE)   return false;
         if (uni == UNI_DELETE)         return false;

         // the caller emits the string between " " without any escaping,
         // so both the delimiter and the escape introducer of "..."
         // strings (see Tokenizer::tokenize_string2()) must be excluded
         // here; such strings then fall back to the (slower) '...' item
         // by item encoding in do_CR10_level()/do_CR10_variable().
         //
         if (uni == UNI_DOUBLE_QUOTE)   return false;
         if (uni == UNI_BACKSLASH)      return false;
       }

   return true;
}
//────────────────────────────────────────────────────────────────────────────
bool
Quad_CR::use_quote(V_mode mode, const cValue * value, ShapeItem pos)
{
int char_len = 0;
int ascii_len = 0;

   for (; pos < value->element_count(); ++pos)
       {
         // if we are in '' mode then a single ASCII char
         // suffices to remain in that mode
         //
         if (ascii_len > 0 && mode == Vm_QUOT) return true;

         // if we are in a non-'' mode then 3 ASCII chars
         // suffice to enter '' mode
         //
         if (ascii_len >= 3)   return true;

         if (!value->is_character_cell(pos))   break;
         ++char_len;
         const Unicode uni = value->get_char_value(pos);
         if (uni >= ' ' && uni <= 0x7E)   ++ascii_len;
         else                             break;
       }

   // if all chars are ASCII then use '' mode
   //
   return (char_len == ascii_len);
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::close_mode(UCS_string & rhs, V_mode mode)
{
   if      (mode == Vm_QUOT)   rhs << UNI_SINGLE_QUOTE;
   else if (mode == Vm_UCS)    rhs << UNI_R_PARENT;
}
//────────────────────────────────────────────────────────────────────────────
void
Quad_CR::item_separator(UCS_string & line, V_mode from_mode, V_mode to_mode)
{
   if (from_mode == to_mode)   // separator in same mode (if any)
      {
        if      (to_mode == Vm_UCS)    line << UNI_SPACE;
        else if (to_mode == Vm_NUM)    line << UNI_SPACE;
      }
   else                // close old mode and open new one
      {
        close_mode(line, from_mode);
        if (from_mode != Vm_NONE)   line << UNI_COMMA;

        if      (to_mode == Vm_UCS)    line << "(,⎕UCS ";
        else if (to_mode == Vm_QUOT)   line << UNI_SINGLE_QUOTE;
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
Quad_CR::temp_varname()
{
char cc[40];
timeval tv;
   gettimeofday(&tv, 0);
   SPRINTF(cc, "⍙¯%X¯%X", uint32_t(tv.tv_sec), uint32_t(tv.tv_usec));
   usleep(1000);   // to make the timestamp unique;

const UTF8_string varname_utf(cc);
   return UCS_string(varname_utf);
}
//════════════════════════════════════════════════════════════════════════════

