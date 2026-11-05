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

#include "CDR.hh"
#include "CharCell.hh"
#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "IntCell.hh"
#include "PointerCell.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
Value_P
CDR::from_CDR(const CDR_string & cdr, const char * loc, int nest_level)
{
   // a CDR_NEST32 sub-record's offset can point anywhere in the buffer,
   // including back at (or into) an enclosing record, e.g. a nested
   // sub-item whose offset resolves to itself: from_CDR() would then
   // recurse into an identical copy of the same bytes forever, exhausting
   // the stack (confirmed via a real SIGSEGV/stack-overflow crash with a
   // self-referential offset). Cap the nesting depth generously above any
   // real-world use.
   //
enum { MAX_CDR_NEST_LEVEL = 100 };
   if (nest_level > MAX_CDR_NEST_LEVEL)
      {
        MORE_ERROR() << "CDR nested nesting level exceeds "
                     << int(MAX_CDR_NEST_LEVEL);
        LENGTH_ERROR;
      }

   // read header...
   //
/*
struct tf3_header
    {
       uint32_t ptr;      //  0: byte reversed
       uint32_t nb;       //  4: byte reversed
       uint32_t nelm;     //  8: byte reversed
       uint8_t  type;     // 12:
       uint8_t  rank;     // 13:
       uint8_t  fill[2];  // 14:
       uint32_t dim[1];   // 16: byte reversed
    }
*/

   if (cdr.size() < 20)
      {
        MORE_ERROR() << "CDR strings must have a header of at least 20 bytes";
        LENGTH_ERROR;
      }

const uint8_t * data = cdr.get_items();

const uint32_t nelm = get_4_be(data + 8);
const CDR_type vtype = CDR_type(data[12]);
const sRank rank = data[13];

   // Guard: rank must not push the shape-dimensions past the buffer end.
   if ((size_t)(16 + 4*rank) > cdr.size())
      {
        MORE_ERROR() << "CDR rank " << rank << " exceeds buffer";
        LENGTH_ERROR;
      }

Shape shape;
   loop(r, rank)
      {
        const ShapeItem sh = get_4_be(data + 16 + 4*r);
        shape.add_shape_item(sh);
      }

   // Guard: nelm must match the shape volume (otherwise next_ravel() would
   // either run out of cells or leave cells uninitialized), and the ravel
   // data (whose per-element size depends on vtype) must actually fit in
   // the buffer (otherwise we would read past cdr's end).
   //
   if (ShapeItem(nelm) != shape.get_volume())
      {
        MORE_ERROR() << "CDR nelm " << nelm << " does not match shape "
                        "volume " << shape.get_volume();
        LENGTH_ERROR;
      }

const size_t header_len = size_t(16 + 4*rank);
size_t elem_size = 0;   // 0 means: checked separately below (or unsupported)
   switch (vtype)
      {
        case CDR_INT32:    elem_size = 4;    break;
        case CDR_FLT64:    elem_size = 8;    break;
        case CDR_CPLX128:  elem_size = 16;   break;
        case CDR_CHAR8:    elem_size = 1;    break;
        case CDR_CHAR32:   elem_size = 4;    break;
        case CDR_NEST32:   elem_size = 4;    break;   // offset table
        default:           break;
      }

   if (vtype == CDR_BOOL1)
      {
        const size_t bit_bytes = (size_t(nelm) + 7) / 8;
        if (header_len + bit_bytes > cdr.size())
           {
             MORE_ERROR() << "CDR BOOL1 data (" << bit_bytes
                          << " bytes) exceeds buffer";
             LENGTH_ERROR;
           }
      }
   else if (vtype == CDR_PROG64)   // fixed-size body: first + increment
      {
        if (header_len + 8 > cdr.size())
           {
             MORE_ERROR() << "CDR PROG64 data (8 bytes) exceeds buffer";
             LENGTH_ERROR;
           }
      }
   else if (elem_size
         && (header_len + elem_size*size_t(nelm) > cdr.size()))
      {
        MORE_ERROR() << "CDR nelm " << nelm << " × " << elem_size
                     << " bytes exceeds buffer";
        LENGTH_ERROR;
      }

Value_P Z(shape, loc);

   // an empty CDR record has no ravel data, so none of the per-type fill
   // loops below ever runs and Z would keep its default (integer)
   // prototype regardless of vtype -- set it from vtype instead, so
   // e.g. a CHAR8/CHAR32 record with nelm == 0 decodes back to '' rather
   // than ⍬ (Blake McBride, Bugs24 #1). For CDR_NEST32 with nelm == 0
   // the nested prototype isn't recoverable from the header alone;
   // set_proto_Int() just preserves prior (already wrong) behaviour there.
   if (nelm == 0)
      {
        if (vtype == CDR_CHAR8 || vtype == CDR_CHAR32)   Z->set_proto_Spc();
        else                                             Z->set_proto_Int();
      }

const uint8_t * ravel = data + 16 + 4*rank;

   if (vtype == CDR_BOOL1)        // packed bit vector
      {
        loop(n, nelm)
           {
             const int bit = ravel[n >> 3] & (0x80 >> (n & 7));
             Z->next_ravel_Int(bit ? 1 : 0);
           }
      }
   else if (vtype == CDR_INT32)   // 4 byte ints vector
      {
        loop(n, nelm)
           {
             APL_Integer d = get_4_le(ravel + 4*n);
             if (d & 0x80000000)   d |= 0xFFFFFFFF00000000ULL;
             Z->next_ravel_Int(d);
           }
      }
   else if (vtype == CDR_FLT64)   // 8 byte float vector
      {
        loop(n, nelm)
           {
             const APL_Float v =
                  get_float_le(ravel + 8*n);
             Z->next_ravel_Float(v);
           }
      }
  else if (vtype == CDR_CPLX128)   // 16 byte complex vector
      {
        loop(n, nelm)
           {
             const APL_Float vr =
                  get_float_le(ravel + 16*n);
             const APL_Float vi =
                  get_float_le(ravel + 16*n + 8);
             Z->next_ravel_Complex(vr, vi);
           }
      }
   else if (vtype == CDR_CHAR8)   // 1 byte char vector
      {
        // Bugs9 #4 (Blake McBride): sign-extending a *character* byte is
        // meaningless (chars 128-255, e.g. Latin-1 e-acute, came back as
        // negative Unicode code points) and disagreed with both the writer
        // (CDR::fill() type 4 stores the plain unsigned byte) and the
        // nested-scalar CHAR8 reader just below (no sign extension).
        loop(n, nelm)   Z->next_ravel_Char(Unicode(ravel[n]));
      }
   else if (vtype == CDR_CHAR32)   // 4 byte UNICODE vector
      {
        loop(n, nelm)
           {
             const uint32_t d = get_4_le(ravel+4*n);
             Z->next_ravel_Char(Unicode(d));
           }
      }
   else if (vtype == CDR_PROG64)   // arithmetic progression: first, incr
      {
        APL_Integer first = get_4_le(ravel);
        if (first & 0x80000000)   first |= 0xFFFFFFFF00000000ULL;
        APL_Integer incr = get_4_le(ravel + 4);
        if (incr & 0x80000000)    incr  |= 0xFFFFFFFF00000000ULL;

        loop(n, nelm)   Z->next_ravel_Int(first + n*incr);
      }
   else if (vtype == CDR_NEST32)   // packed vector with 4 bytes offsets.
      {
        loop(n, nelm)
            {
              APL_Integer offset =
                         get_4_le(ravel + 4*n);
              if ((size_t)(offset + 14) > cdr.size())
                 {
                   MORE_ERROR() << "CDR nested offset " << offset
                                << " exceeds buffer";
                   LENGTH_ERROR;
                 }
              const uint8_t * sub_data = data + offset;
              const uint32_t sub_vtype = sub_data[12];
              const sRank sub_rank = sub_data[13];
              const uint8_t * sub_ravel = sub_data + 16 + 4*sub_rank;

              // if the sub-item is a non-nested scalar then we append it to
              // ret directly; otherwise we append a pointer to it,
              //
              if (sub_rank == 0)   // scalar
                 {
                   // The offset+14 guard above validates only sub_data's
                   // first 14 bytes, but sub_ravel starts at sub_data+16
                   // -- every scalar branch below reads at or past that,
                   // up to 16 bytes for a complex value (up to 17 bytes
                   // past a buffer whose last valid offset is exactly
                   // offset+14, confirmed by inspection). Validate the
                   // full per-type size before dereferencing sub_ravel.
                   size_t sub_size = 0;
                   switch (sub_vtype)
                      {
                        case 0: case 4: sub_size = 17; break;   // bit, char8
                        case 1: case 5: sub_size = 20; break;   // int, char32
                        case 2:         sub_size = 24; break;   // float
                        case 3:         sub_size = 32; break;   // complex
                      }
                   if (sub_size && size_t(offset) + sub_size > cdr.size())
                      {
                        MORE_ERROR() << "CDR nested scalar (type "
                                     << sub_vtype << ") at offset " << offset
                                     << " exceeds buffer";
                        LENGTH_ERROR;
                      }

                   if (sub_vtype == 0)        // bit
                      {
                        Z->next_ravel_Int((*sub_ravel & 0x80) ? 1 : 0);
                        continue;   // next n
                      }

                   if (sub_vtype == 1)        // 4 byte int
                      {
                        APL_Integer d = get_4_le(sub_ravel);
                        if (d & 0x80000000)   d |= 0xFFFFFFFF00000000ULL;
                        Z->next_ravel_Int(d);
                        continue;   // next n
                      }

                   if (sub_vtype == 2)        // 8 byte real
                      {
                        Z->next_ravel_Float(
                                        get_float_le(sub_ravel));
                        continue;   // next n
                      }

                   if (sub_vtype == 3)        // 16 byte complex
                      {
                        Z->next_ravel_Complex(
                                        get_float_le(sub_ravel),
                                        get_float_le(sub_ravel + 8));
                        continue;   // next n
                      }

                   if (sub_vtype == 4)        // 1 byte ASCII char
                      {
                        Z->next_ravel_Char(Unicode(*sub_ravel));
                        continue;   // next n
                      }

                   if (sub_vtype == 5)        // 4 byte Unicode
                      {
                        // Bugs9 #5 (Blake McBride): GNU APL's CDR is
                        // deliberately mixed-endian -- header fields use
                        // get_4_be(), but ravel data (including this nested
                        // scalar, matching the writer at CDR.cc and the
                        // sibling int branch just above) is little-endian
                        // (byte-wise, so on any host: Barak Pearlmutter). Using get_4_be() here read written bytes
                        // in the wrong order (e.g. U+012C came back as
                        // 0x2C010000).
                        Z->next_ravel_Char(Unicode(
                               get_4_le(sub_ravel)));
                        continue;   // next n
                      }

                   // sub_vtype 6 (progression vector) can't be scalar
                 }

              // at this point the sub-item is not a scalar
              //
              if (sub_vtype == 6)        // arithmetic progression vector
                 {
                   // an APV's body is { first, increment } (2 4-byte ints,
                   // 8 bytes total, matching CDR::fill()'s type 6 writer
                   // above and the count coming from the header's own
                   // nelm/shape, per apl2lrm.txt Appendix B's
                   // "first|incr×⎕IO|⍳count" transfer-form definition).
                   //
                   // sub_rank (an unvalidated attacker byte, 0-255) drives
                   // the shape-item loop below, and the 8-byte body follows
                   // the shape; the offset+14 guard above only validates 14
                   // bytes, but this reads up to offset+16+4*sub_rank+8 --
                   // validate that full extent first (mirroring the
                   // scalar-branch guard above).
                   if (size_t(offset) + 16 + 4*size_t(sub_rank) + 8
                       > cdr.size())
                      {
                        MORE_ERROR() << "CDR nested APV (sub_rank "
                                     << sub_rank << ") at offset " << offset
                                     << " exceeds buffer";
                        LENGTH_ERROR;
                      }

                   Shape sh;
                   loop(r, sub_rank)
                      sh.add_shape_item(get_4_be(sub_data + 16 + 4*r));

                   const uint8_t * sub_body = sub_data + 16 + 4*sub_rank;
                   APL_Integer first =
                        get_4_le(sub_body);
                   if (first & 0x80000000)   first |= 0xFFFFFFFF00000000ULL;
                   APL_Integer incr =
                        get_4_le(sub_body + 4);
                   if (incr & 0x80000000)    incr  |= 0xFFFFFFFF00000000ULL;

                   Value_P sub_val(sh, LOC);
                   loop(v, sh.get_volume())
                       {
                         sub_val->next_ravel_Int(first + v*incr);
                       }

                   sub_val->check_value(LOC);
                   Z->next_ravel_Pointer(sub_val.get());
                   continue;   // next n
                 }

              const uint32_t sub_cdr_len = get_4_be(sub_data + 4);

              // sub_cdr_len is attacker-controlled and was, until this
              // check, never validated against the bytes actually
              // remaining: CDR_string's constructor copies sub_cdr_len
              // bytes one at a time starting at sub_data, so an
              // oversized sub_cdr_len read far past the buffer (crash +
              // information leak, confirmed via a real SIGSEGV). 20 is
              // the same minimum header size from_CDR() itself requires
              // a few lines below (once sub_cdr is actually parsed).
              if (sub_cdr_len < 20 ||
                  size_t(offset) + sub_cdr_len > cdr.size())
                 {
                   MORE_ERROR() << "CDR nested sub-record length "
                                << sub_cdr_len << " at offset " << offset
                                << " exceeds buffer";
                   LENGTH_ERROR;
                 }

              CDR_string sub_cdr(sub_data, sub_cdr_len);
              Value_P sub_val = from_CDR(sub_cdr, LOC, nest_level + 1);
              Assert(sub_val);
              Z->next_ravel_Pointer(sub_val.get());
            }
      }
   else
      {
        // Assert() alone is a no-op at the default ASSERT_LEVEL 0: Z was
        // constructed above (some shape) but never filled via any
        // next_ravel_*() call in this branch, so falling through to
        // check_value()/return Z below would hand the caller a Value
        // whose polymorphic Cells are uninitialized garbage (reachable
        // via e.g. a crafted 12 ⎕CR with CDR type 6 = CDR_PROG64, which
        // has no top-level handler here -- only as a CDR_NEST32 sub-item
        // type, a different code path entirely).
        CERR << "Unsupported CDR type " << vtype << endl;
        MORE_ERROR() << "Unsupported/unhandled CDR type " << vtype;
        DOMAIN_ERROR;
      }

   Z->check_value(LOC);
   return Z;
}
//────────────────────────────────────────────────────────────────────────────
void
CDR::to_CDR(CDR_string & result, const cValue * value)
{
const CDR_type type = value->get_CDR_type();
const ShapeItem len = value->total_CDR_size_brutto(type);
   result.reserve(len + 1);
   result.clear();

   fill(result, type, len, *value);
}
//────────────────────────────────────────────────────────────────────────────
void
CDR::fill(CDR_string & result, int type, ShapeItem len, const cValue & val)
{
   Assert((len & 0x0F) == 0);

/*
    struct tf3_header
    {
       uint32_t ptr;      // byte reversed
       uint32_t nb;       // byte reversed
       uint32_t nelm;     // byte reversed
       uint8_t  type;
       uint8_t  rank;
       uint8_t  fill[2];
       uint32_t dim[1];   // byte reversed
    };
*/

   // ptr
   //
const uint32_t ptr = 0x00002020;
   result.push_back(Unicode(ptr >> 24 & 0xFF));
   result.push_back(Unicode(ptr >> 16 & 0xFF));
   result.push_back(Unicode(ptr >>  8 & 0xFF));
   result.push_back(Unicode(ptr       & 0xFF));

   // nb (header + data + sub-values + padding)
   //
   result.push_back(Unicode(len >> 24 & 0xFF));
   result.push_back(Unicode(len >> 16 & 0xFF));
   result.push_back(Unicode(len >>  8 & 0xFF));
   result.push_back(Unicode(len       & 0xFF));

   // nelm
   //
const uint32_t nelm = val.element_count();
   result.push_back(Unicode(nelm >> 24 & 0xFF));
   result.push_back(Unicode(nelm >> 16 & 0xFF));
   result.push_back(Unicode(nelm >>  8 & 0xFF));
   result.push_back(Unicode(nelm       & 0xFF));

   // type, rank, 0, 0
   //
   result.push_back(Unicode(type));
   result.push_back(Unicode(val.get_rank()));
   result.push_back(Unicode_0);
   result.push_back(Unicode_0);

   // shape
   //
   loop(r, val.get_rank())
      {
        const uint32_t sh = val.get_shape_item(r);
        result.push_back(Unicode(sh >> 24 & 0xFF));
        result.push_back(Unicode(sh >> 16 & 0xFF));
        result.push_back(Unicode(sh >>  8 & 0xFF));
        result.push_back(Unicode(sh       & 0xFF));
      }

   // body
   //
   if (type == 0)        // packed bit vector
      {
        int accu = 0;

        loop(e, nelm)
           {
             const int bit = e%8;
             const APL_Integer i = val.get_int_value(e);
             Assert(i == 0 || i == 1);
             if (i)   accu |= 0x80 >> bit;
             if (bit == 7)
                {
                  result.push_back(Unicode(accu));
                  accu = 0;
                }
           }

        if (nelm % 8)   // partly filled
           {
             result.push_back(Unicode(accu));
           }

      }
   else if (type == 1)   // 4 byte ints vector
      {
        loop(e, nelm)
           {
             uint64_t i = val.get_int_value(e);
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             Assert(i == 0 || i == 0xFFFFFFFF);
           }
      }
   else if (type == 2)   // 8 byte float vector
      {
        loop(e, nelm)
           {
             const APL_Float v = val.get_real_value(e);
             const APL_Float * dv = &v;
             uint64_t i = *reinterpret_cast<const uint64_t *>(dv);
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
           }
      }
   else if (type == 3)   // 16 byte complex vector
      {
        loop(e, nelm)
           {
             APL_Float v = val.get_real_value(e);
             const APL_Float * dv = &v;
             uint64_t i = *reinterpret_cast<const uint64_t *>(dv);
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;

             v = val.get_imag_value(e);
             dv = &v;
             i = *reinterpret_cast<const uint64_t *>(dv);
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
           }
      }
   else if (type == 4)   // 1 byte char vector
      {
        loop(e, nelm)
           {
             const Unicode uni = val.get_char_value(e);
             Assert(uni >= 0);
             Assert(uni < 256);
             result.push_back(uni);
           }
      }
   else if (type == 5)   // 4 byte UNICODE vector
      {
        loop(e, nelm)
           {
             uint32_t i = val.get_char_value(e);
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
             result.push_back(Unicode(i & 0xFF));   i >>= 8;
           }
      }
   else if (type == 7)   // packed vector with 4 bytes offsets.
      {
        // the first offset is after header + offset-vector and then increases
        // by the size of each sub-value. I.e.
        //
        //   +------------------+
        //   | TOP-LEVEL-HEADER |                        16 bytes + shape
        //   +------------------+
        //   | OFFSET-1         | --------+               4 bytes
        //   +------------------+         |
        //   | OFFSET-2         | --------|--+            4 bytes
        //   +------------------+         |  |
        //   | ...              |         |  |              ...
        //   +------------------+         |  |
        //   | OFFSET-nelm      | --------|--|--+         4 bytes
        //   +------------------+         |  |  |
        //   | SUB-HEADER-1     | <-------+  |  |        16 bytes + shape
        //   | VALUE-1          |            |  |
        //   +------------------+            |  |
        //   | SUB-HEADER-2     | <----------+  |        16 bytes + shape
        //   | VALUE-2          |               |
        //   +------------------+               |
        //   | ...              |               |           ...
        //   +------------------+               |
        //   | SUB-HEADER-nelm  | <-------------+        16 bytes + shape
        //   | VALUE-nelm       |
        //   +------------------+
        //

        uint32_t offset = 16 + 4*val.get_rank() + 4*nelm;
        while (offset & 0x0F)   ++offset;
        loop(e, nelm)
           {
             result.push_back(Unicode(offset       & 0xFF));
             result.push_back(Unicode(offset >>  8 & 0xFF));
             result.push_back(Unicode(offset >> 16 & 0xFF));
             result.push_back(Unicode(offset >> 24 & 0xFF));

             if (val.is_simple_cell(e))
                {
                  // a non-pointer sub value: 16 byte header,
                  // and 1-16 data bytes, padded up to 16 bytes,
                  //
                  offset += 32;
                }
             else if (val.is_pointer_cell(e))
                {
                  const cValue & sub_val = *val.get_pointer_value(e);
                  const CDR_type sub_type = sub_val.get_CDR_type();
                  offset += sub_val.total_CDR_size_brutto(sub_type);
                }
              else
                DOMAIN_ERROR;
           }

        // pad top level to 16 bytes. end is aligned to 16 byte so we
        // case use it as reference.
        //
        while (result.size() & 0x0F)   result.push_back(Unicode_0);

        // append sub-values.
        //
        loop(e, nelm)
           {
             Cell cache;
             const Cell & cell = val.get_cravel(e, cache);
             if (cell.is_simple_cell())
                {
                  Value_P sub_val(LOC);
                  sub_val->set_ravel_Cell(0, cell);

                  const CDR_type sub_type = sub_val->get_CDR_type();
                  const ShapeItem sub_len = sub_val->total_CDR_size_brutto(sub_type);
                  fill(result, sub_type, sub_len, *sub_val);
                }
              else
                {
                  Value_P sub_val = cell.get_pointer_value();

                  const CDR_type sub_type = sub_val->get_CDR_type();
                  const ShapeItem sub_len = sub_val->total_CDR_size_brutto(sub_type);
                  fill(result, sub_type, sub_len, *sub_val);
                }
           }

        // since we padded the top level to 16 and sub-values have a padded
        // length as well, we should be properly padded here.
        //
      }
   else Assert(0 && "Bad/unsupported CDR type");

   // final padding
   //
   while (result.size() & 15)   result.push_back(Unicode_0);
}
//════════════════════════════════════════════════════════════════════════════

