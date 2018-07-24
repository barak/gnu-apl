/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2018  Dr. Jürgen Sauermann

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

#include "CharCell.hh"
#include "Workspace.hh"

//-----------------------------------------------------------------------------
CellType
CharCell::get_cell_subtype() const
{
   if (value.ival < 0)   // negative char (only fits in signed containers)
      {
        if (-value.ival <= 0x80)
           return static_cast<CellType>(CT_CHAR | CTS_S8 | CTS_S16 | CTS_S32 | CTS_S64);

        if (-value.ival <= 0x8000)
           return static_cast<CellType>(CT_CHAR | CTS_S16 | CTS_S32 | CTS_S64);

        return static_cast<CellType>(CT_CHAR | CTS_S32 | CTS_S64);
      }

   // positive char
   //
   if (value.ival <= 0x7F)
      return static_cast<CellType>(CT_CHAR | CTS_X8  | CTS_S8  | CTS_U8  |
                                   CTS_X16 | CTS_S16 | CTS_U16 |
                                   CTS_X32 | CTS_S32 | CTS_U32 |
                                   CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0xFF)
      return static_cast<CellType>(CT_CHAR |                     CTS_U8  |
                                   CTS_X16 | CTS_S16 | CTS_U16 |
                                   CTS_X32 | CTS_S32 | CTS_U32 |
                                   CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0x7FFF)
      return static_cast<CellType>(CT_CHAR | CTS_X16 | CTS_S16 | CTS_U16 |
                                   CTS_X32 | CTS_S32 | CTS_U32 |
                                   CTS_X64 | CTS_S64 | CTS_U64);

   if (value.ival <= 0xFFFF)
      return static_cast<CellType>(CT_CHAR |                     CTS_U16 |
                                   CTS_X32 | CTS_S32 | CTS_U32 |
                                   CTS_X64 | CTS_S64 | CTS_U64);

   return static_cast<CellType>(CT_CHAR | CTS_X32 | CTS_S32 | CTS_U32 |
                                CTS_X64 | CTS_S64 | CTS_U64);
}
//-----------------------------------------------------------------------------
bool
CharCell::greater(const Cell & other) const
{
   // char cells are smaller than all others
   //
   if (other.get_cell_type() != CT_CHAR)   return false;

const Unicode this_val  = get_char_value();
const Unicode other_val = other.get_char_value();

   // if both chars are the same, compare cell address
   //
   if (this_val == other_val)   return this > &other;

   return this_val > other_val;
}
//-----------------------------------------------------------------------------
bool
CharCell::equal(const Cell & other, double qct) const
{
   if (!other.is_character_cell())   return false;
   return value.aval == other.get_char_value();
}
//-----------------------------------------------------------------------------
Comp_result
CharCell::compare(const Cell & other) const
{
   if (other.get_char_value() == value.aval)  return COMP_EQ;
   return (value.aval < other.get_char_value()) ? COMP_LT : COMP_GT;
}
//-----------------------------------------------------------------------------
bool
CharCell::is_example_field() const
{
   if (value.aval == UNI_ASCII_COMMA)       return true;
   if (value.aval == UNI_ASCII_FULLSTOP)    return true;
   return value.aval >= UNI_ASCII_0 && value.aval <= UNI_ASCII_9;
}
//-----------------------------------------------------------------------------
PrintBuffer
CharCell::character_representation(const PrintContext & pctx) const
{
UCS_string ucs;
ColInfo info;
   info.flags |= CT_CHAR;

   if (pctx.get_style() == PR_APL_FUN)
      {
        Unicode uni = get_char_value();
        if (uni == UNI_SINGLE_QUOTE)
           {
             ucs.append(UNI_SINGLE_QUOTE);
             ucs.append(uni);
             ucs.append(uni);
             ucs.append(UNI_SINGLE_QUOTE);
           }
        else
           {
             ucs.append(UNI_SINGLE_QUOTE);
             ucs.append(uni);
             ucs.append(UNI_SINGLE_QUOTE);
           }
      }
   else
      {
       if (pctx.get_style() & PST_QUOTE_CHARS)
          {
            ucs.append(UNI_SINGLE_QUOTE);
            ucs.append(get_char_value());
            ucs.append(UNI_SINGLE_QUOTE);
          }
        else
          {
            ucs.append(get_char_value());
          }

      }

   info.real_len = info.int_len = ucs.size();

   return PrintBuffer(ucs, info);
}
//-----------------------------------------------------------------------------
int
CharCell::get_byte_value() const
{
   if (value.aval < -128)
      {
        char cc[20];
        snprintf(cc, sizeof(cc), "' = U+%4.4X", value.aval & 0x0FFFF);
        MORE_ERROR() << "Unicode character '" << value.aval << cc
                     << " is too small (< U+FF80) for a byte value";
        DOMAIN_ERROR;
      }

   if (value.aval > 255)
      {
        char cc[20];
        snprintf(cc, sizeof(cc), "' = U+%4.4X", value.aval & 0x0FFFF);
        MORE_ERROR() << "Unicode character '" << value.aval << cc
                     << " is too large (> U+00FF) for a byte value";
        DOMAIN_ERROR;
      }

   return value.aval;
}
//-----------------------------------------------------------------------------
int
CharCell::CDR_size() const
{
   // use 1 byte for small chars and 4 bytes for other UNICODE chars
   //
const Unicode uni = get_char_value();
   if (uni <  0)     return 4;
   if (uni >= 256)   return 4;
   return 1;
}
//-----------------------------------------------------------------------------
