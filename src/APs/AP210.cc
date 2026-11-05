/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2026  Dr. Jürgen Sauermann

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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <vector>

#include "CDR_string.hh"
#include "APmain.hh"
#include "Svar_DB.hh"
#include "Svar_signals.hh"

using namespace std;

bool debug_log = false;;

static int var_count_210 = 0;

enum { ALL_RW = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH };

/// look up the Coupled_var with the given \b key in the global coupled_vars
/// (defined below, after coupled_vars itself is declared).
Coupled_var & find_coupled_var(SV_key key);

/// data related to one AP210 instance
struct SVAR_context
{
   /// constructor
   SVAR_context(FILE * f, int c, Coupled_var & vC, Coupled_var & vD, bool wr)
   : file(f),
     key_C(vC.key),
     key_D(vD.key),
     write(wr),
     encoding(c),
     rec_num(0),
     rec_size(128),
     filepos(0),
     state_D(SVS_NOT_SHARED)
  {}

   ~SVAR_context()
      { assert(file == 0); }

   /// the (shared) CTL variable for the file. Looked up by key on every
   /// call rather than cached as a reference/pointer: SVAR_context can
   /// outlive a reallocation of the global coupled_vars vector (⎕SVO
   /// while a file is open push_back()s it, ⎕SVR erase()s from it), which
   /// would otherwise leave var_C/var_D dangling (Blake McBride, Bugs15
   /// #4). The linear lookup is fine here -- coupled_vars is small and
   /// these calls are not on a hot path.
   Coupled_var & var_C() const   { return find_coupled_var(key_C); }

   /// the (shared) DAT variable for the file, see var_C() above.
   Coupled_var & var_D() const   { return find_coupled_var(key_D); }

   /// the (open) file to be read or written
   FILE * file;

   /// the key (in Svar_DB / coupled_vars) of the CTL variable for the file
   SV_key key_C;

   /// the key (in Svar_DB / coupled_vars) of the DAT variable for the file
   SV_key key_D;

   /// true if file is to be written (as opposed to being read).
   const bool write;

   /// file encoding (as per IBM APL2 User's Guide, p. 345) 
   int encoding;

   /// the record size of the file
   int rec_num;

   /// the (next) record number in the file
   int rec_size;

   /// the file position of the file
   int filepos;

   /// state changes of DAT variable
   Svar_state state_D;
};
//────────────────────────────────────────────────────────────────────────────
Coupled_var &
find_coupled_var(SV_key key)
{
   loop(c, coupled_vars.size())
      {
        if (coupled_vars[c].key == key)   return coupled_vars[c];
      }

   // a Coupled_var whose key no longer exists in coupled_vars means the
   // variable was ⎕SVR'd (retracted) while a file was still open on it --
   // not expected via normal APL use (retracting the CTL/DAT variable of
   // an open AP210 file), but fail loudly rather than dereference garbage.
   //
   assert(0 && "find_coupled_var(): key not found");
   __builtin_unreachable();
}

//════════════════════════════════════════════════════════════════════════════
const char * prog_name()
{
   return "AP210";
}
//════════════════════════════════════════════════════════════════════════════
int
set_ACK(Coupled_var & var, int exco)
{
const uint8_t _cdr[] =
   {
     0x00, 0x00, 0x20, 0x20, // ptr
     0x00, 0x00, 0x00, 0x20, // nb: total len
     0x00, 0x00, 0x00, 0x01, // nelm: element count
     0x01, 0x00, 0x00, 0x00, // type, rank, pad, pad
     uint8_t(exco), uint8_t(exco>>8), uint8_t(exco>>16), uint8_t(exco>>24),
     0x00, 0x00, 0x00, 0x00, // pad
     0x00, 0x00, 0x00, 0x00, // pad
     0x00, 0x00, 0x00, 0x00, // pad
   };

const CDR_string * cdr = new CDR_string(_cdr, sizeof(_cdr));
   delete var.data;
   var.data = cdr;

   return exco;
}
//════════════════════════════════════════════════════════════════════════════
int
write_fixed(FILE * file, int code, Coupled_var & var_D,
            int & filepos, int rn, int rs)
{
   if (code == 'A')        // APL2 format (CDR)
      {
      }
   else if (code == 'B')   // boolean fixed size
      {
      }
   else if (code == 'C')   // char fixed size
      {
      }
   else if (code == 'D')   // char (var size
      {
      }
   else if (code == 'T')   // translate
      {
      }
   else assert(0 && "Bad code at" LOC);

   return 0;
}
//════════════════════════════════════════════════════════════════════════════
int
seek_variable(FILE * file, int code, int rn)
{
   rewind(file);

   // skip first rn records

   if (code == 'A')        // APL2 format (CDR)
      {
        loop(r, rn)
            {
              CDR_header header;
              const int hlen = fread(&header, 1, sizeof(CDR_header), file);
              if (hlen != sizeof(header))   return -44;   // delimiter not found

              const int nb = header.get_nb();
              const int rest = nb - sizeof(CDR_header);
              if (fseek(file, rest, SEEK_CUR))   return -44;   // no delimiter
            }

        return 0;
      }
   else if (code == 'B')   // boolean fixed size
      {
      }
   else if (code == 'C')   // char fixed size
      {
      }
   else if (code == 'D')   // char (var size
      {
      }
   else if (code == 'T')   // translate
      {
      }
   else assert(0 && "Bad code at" LOC);

   return 0;
}
//════════════════════════════════════════════════════════════════════════════
int
read_variable(FILE * file, int code, Coupled_var & var_D,
              int & filepos, int rn, int rs, bool skip_NL)
{
   if (filepos != rn)
      if (int result = seek_variable(file, code, rn))   return result;
   filepos = rn;

   if (code == 'A')        // APL2 format (CDR)
      {
        CDR_header header;
        const int hlen = fread(&header, 1, sizeof(header), file);
        if (hlen != sizeof(CDR_header))   return -44;   // delimiter not found

        const uint32_t nb = header.get_nb();
        if (nb < sizeof(CDR_header))   return -44;   // header claims to be
                                                       // shorter than itself

        const size_t rest = nb - sizeof(CDR_header);
        uint8_t * buffer = new uint8_t[nb];
        Assert(buffer);
        memcpy(buffer, &header, sizeof(CDR_header));
        const size_t rlen = fread(&buffer[sizeof(CDR_header)], 1, rest, file);
        if (rlen != rest)
           {
             delete[] buffer;
             return -44;   // delimiter not found
           }

        delete var_D.data;
        var_D.data = new CDR_string(buffer, nb);
        delete[] buffer;
        return 0;
      }
   else if (code == 'D')   // char (var size)
      {
      }
   else if (code == 'T')   // translate
      {
      }

   return -31;   // invalid code, i.e. var_C domain error
}
//════════════════════════════════════════════════════════════════════════════
int
write_variable(FILE * file, int code, Coupled_var & var_D,
               int & filepos, int rn, int rs)
{
   // if rn is larger than filepos then append at the end
   //
   if (rn > filepos)   rn = filepos;

   if (rn < filepos)
      {
        const int result = seek_variable(file, code, rn);
        if (result)   return result;
      }
   filepos = rn;

   assert(var_D.data);
const CDR_string & cdr = *var_D.data;

   if (code == 'A')        // APL2 format (CDR)
      {
        const int len = cdr.header().get_nb();
        const uint8_t * data = cdr.get_items();
        const int written = fwrite(data, len, 1, file);
        if (written != 1)   return -48;   // write error or partial write

        ++filepos;
        return 0;
      }
   else if (code == 'D')   // char (var size)
      {
      }
   else if (code == 'T')   // translate
      {
      }
   return -31;   // invalid code, i.e. var_C domain error
}
//════════════════════════════════════════════════════════════════════════════
void
sub_command(SVAR_context & ctx)
{
int op = -1;
int result;

   assert(ctx.var_C().data);

const CDR_string & cdr = *ctx.var_C().data;

   if (cdr.check())
      {
        get_CERR() << "Bad CDR record:" << endl;
        cdr.debug(get_CERR(), LOC);
        set_ACK(ctx.var_C(), -47);
        return;
      }

   if (cdr.get_rank() > 1)   // not a scalar or vector
      {
        get_CERR() << "Bad CDR rank (" << cdr.get_rank() << endl;
        set_ACK(ctx.var_C(), -32);   // C rank error
        return;
      }

const int nelm = cdr.header().get_nelm();
   if (nelm == 0)       // '' or ⍳0: close file
      {
        // close file
        fclose(ctx.file);
        ctx.file = 0;

        // return to command mode
        ctx.var_C().context = ctx.var_D().context = 0;
        set_ACK(ctx.var_C(), 0);

        delete &ctx;
        return;
      }

   if (nelm  > 3)
      {
        set_ACK(ctx.var_C(), -33);   // C length error
        return;
      }

   if (cdr.get_type() == CDR_BOOL1)
      {
        const uint8_t byte = *cdr.ravel();
        op                            = byte & 0x80 ? 1 : 0;
        if (nelm >= 2)   ctx.rec_num  = byte & 0x40 ? 1 : 0;
        if (nelm >= 3)   ctx.rec_size = byte & 0x20 ? 1 : 0;
      }
   else if (cdr.get_type() == CDR_INT32)
      {
        // CDR ravel items are little endian on any host
        const uint8_t * ravel = cdr.ravel();
        op                        = CDR_header::get_le32(ravel);
        if (nelm >= 2)   ctx.rec_num  = CDR_header::get_le32(ravel + 4);
        if (nelm >= 3)   ctx.rec_size = CDR_header::get_le32(ravel + 8);
      }
   else   // neither int nor bool:
      {
        set_ACK(ctx.var_C(), -31);    // C domain error 
        return;
      }

   switch(op)
      {
        case 0:  // read fixed-len record
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 return;
                 break;

        case 1:  // write fixed-len record
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 return;
                 break;

        case 2:  // read direct
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 return;
                 break;

        case 3:  // write direct
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 return;
                 break;

        case 4:  // read variable (APL object or line)
                 if (ctx.write)
                    {
                      get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                      set_ACK(ctx.var_C(), -47);    // -47 INVALID SUBCOMMAND
                      return;
                    }

                 result = read_variable(ctx.file, ctx.encoding, ctx.var_D(),
                                        ctx.filepos, ctx.rec_num,
                                        ctx.rec_size, false);
                 set_ACK(ctx.var_C(), result);
                 ctx.state_D = SVS_OFF_HAS_SET;
                 if (result)   return;
                 ctx.rec_num = ctx.filepos;
                 break;

        case 5:  // write variable (APL object or line)
                 if (!ctx.write)
                    {
                      get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                      set_ACK(ctx.var_C(), -47);    // -47 INVALID SUBCOMMAND
                      return;
                    }

                 result = write_variable(ctx.file, ctx.encoding, ctx.var_D(),
                                        ctx.filepos, ctx.rec_num, ctx.rec_size);
                 set_ACK(ctx.var_C(), result);
                 ctx.state_D = SVS_IDLE;
                 if (result)   return;
                 ctx.rec_num = ctx.filepos;
                 break;

        case 6:  // read variable (APL object or line) without CR/LF
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 return;
                       break;

        default: set_ACK(ctx.var_C(), -47);    // -47 := INVALID SUBCOMMAND
                 get_CERR() << " INVALID SUBCOMMAND at " << LOC << endl;
                 return;
      }
}
//════════════════════════════════════════════════════════════════════════════
FILE *
open_pipe(const char * filename, const char * mode)
{
   // if filename does not exist, create a named pipe with that name
   //
struct stat st;
   if (stat(filename, &st) == 0)   // file exists
      {
        FILE * pipe = 0;
        if (S_ISFIFO(st.st_mode))
           {
             pipe = fopen(filename, mode);
             if (pipe == 0 && debug_log)
                get_CERR() << "Could not open pipe " << filename << ": "
                           << strerror(errno) << endl;
           }

        return pipe;   // file exists but is not a pipe
      }

   // stat failed: create pipe
   //
#if MINGW_SRC
   get_CERR() << "TODO: pipe on windows" << endl;
#else
   if (mknod(filename, S_IFIFO | ALL_RW, 0) == 0)   // mknod successful
      return fopen(filename, mode);
#endif // MINGW_SRC

   return 0;
}
//════════════════════════════════════════════════════════════════════════════
void
read_pipe(const char * filename, int code,
          Coupled_var & var_C, Coupled_var & var_D)
{
   if (code == 0)   code = 'A';    // default if no code is provided
   if (!strchr("ABCDT", code))
      {
       set_ACK(var_C, -31);        // -31: CTL DOMAIN ERROR
       return;
      }

FILE * f = open_pipe(filename, "r");
   if (f == 0)
      {
       set_ACK(var_C, 2);          //  2 := FILE NOT FOUND
       return;
      }

SVAR_context * ctx = new SVAR_context(f, code, var_C, var_D, false);
   var_C.context = var_D.context = ctx;
   set_ACK(var_C, 0);
   set_ACK(var_D, 0);
}
//════════════════════════════════════════════════════════════════════════════
void
write_pipe(const char * filename, int code, Coupled_var & var_C, Coupled_var & var_D)
{
   if (code == 0)   code = 'A';    // default if no code is provided
   if (!strchr("ABCDT", code))
      {
       set_ACK(var_C, -31);        // -31: CTL DOMAIN ERROR
       return;
      }

FILE * f = open_pipe(filename, "a+");
   if (f == 0)
      {
       set_ACK(var_C, 2);          // 2 := FILE NOT FOUND
       return;
      }

SVAR_context * ctx = new SVAR_context(f, code, var_C, var_D, true);
   var_C.context = var_D.context = ctx;
   set_ACK(var_C, 0);
   set_ACK(var_D, 0);
}
//════════════════════════════════════════════════════════════════════════════
void
read_file(const char * filename, int code, Coupled_var & var_C,
          Coupled_var & var_D)
{
   if (code == 0)   code = 'A';    // default if no code is provided
   if (!strchr("ABCDT", code))
      {
       set_ACK(var_C, -31);        // -31: CTL DOMAIN ERROR
       return;
      }

FILE * f = fopen(filename, "r");
   if (f == 0)
      {
        if (debug_log)
           get_CERR() << "Could not open file " << filename << " for reading: "
                      << strerror(errno) << endl;

       set_ACK(var_C, 2);          //  2 := FILE NOT FOUND
       return;
      }

   fseek(f, 0, SEEK_END);
   set_ACK(var_D, ftell(f));

SVAR_context * ctx = new SVAR_context(f, code, var_C, var_D, false);
   var_C.context = var_D.context = ctx;
   fseek(f, 0, SEEK_SET);
   set_ACK(var_C, 0);
}
//════════════════════════════════════════════════════════════════════════════
void
write_file(const char * filename, int code, Coupled_var & var_C, Coupled_var & var_D)
{
   if (code == 0)   code = 'A';    // default if no code is provided
   if (!strchr("ABCDT", code))
      {
        set_ACK(var_C, -31);        // -31: CTL DOMAIN ERROR
        return;
      }

FILE * f = fopen(filename, "w");
   if (f == 0)
      {
        if (debug_log)
           get_CERR() << "Could not open file " << filename << " for writing: "
                      << strerror(errno) << endl;

       set_ACK(var_C, 2);          //  2 := FILE NOT FOUND
       return;
      }

   fseek(f, 0, SEEK_END);
SVAR_context * ctx = new SVAR_context(f, code, var_C, var_D, true);
   var_C.context = var_D.context = ctx;
   set_ACK(var_C, 0);
   set_ACK(var_D, ftell(f));
}
//════════════════════════════════════════════════════════════════════════════
void
delete_file(const char * filename, Coupled_var & var_C)
{
const int ret = unlink(filename) ? errno : 0;
   set_ACK(var_C, ret);
}
//════════════════════════════════════════════════════════════════════════════
void
rename_file(const char * from, const char * to, Coupled_var & var_C)
{
const int ret = rename(from, to) ? errno : 0;
   set_ACK(var_C, ret);
}
//════════════════════════════════════════════════════════════════════════════
bool
parse_arg(const char * & cmd, const char * cmd_end, char * arg,
         int arglen, int minlen)
{
int arg_chars = 0;
bool error = false;
bool quoted = false;

   while (!error)
      {
         if (cmd >= cmd_end || *cmd == 0)   // end of cmd string reached
            {
              error = quoted;   // error iff quote is missing
              break;
            }

         const char cc = *cmd++;
         if (cc == '"')              // start or end of quoted arg
            {
              quoted = !quoted;
              continue;
            }

         if (cc == ',' && !quoted)   // end of arg reached
            break;

         // argument character: copy it.
         //
         if (arg_chars < arglen)   arg[arg_chars++] = cc;
         else                      error = true;
      }

   arg[arg_chars] = 0;
   if (arg_chars < minlen)   error = true;
   return error;
}
//════════════════════════════════════════════════════════════════════════════
void
handle_cmd(const char * cmd, int len, Coupled_var & var_C, Coupled_var & var_D)
{
   // cmd is a string containing 1, 2, or 3 arguments separated by commas.
   // parse the arguments.
   //
   // Blake McBride, Bugs15 #13: this used to keep going after a
   // parse_arg() failure -- the error ACK it set was simply overwritten
   // by the next set_ACK() call (including the one at the very end of
   // this function on an unrecognized arg1), so a rejected command still
   // executed, with truncated/partial arguments, and reported success.
   // Now returns immediately instead. arg1 is zero-initialized so arg1[1]
   // is never read uninitialized when parse_arg() writes zero characters
   // into it (arg_chars == 0, only arg[0] gets explicitly set to 0).
   //
char arg1[3] = { 0, 0, 0 };
char arg2[FILENAME_MAX+2];
char arg3[FILENAME_MAX+2];
const char * const cmd_end = cmd + len;

   if (parse_arg(cmd, cmd_end, arg1, sizeof(arg1) - 1, 2))
      { set_ACK(var_C, 401);   return; }
   if (parse_arg(cmd, cmd_end, arg2, sizeof(arg2) - 1, 1))
      { set_ACK(var_C, 402);   return; }
   if (parse_arg(cmd, cmd_end, arg3, sizeof(arg3) - 1, 0))
      { set_ACK(var_C, 403);   return; }

const char c0 = arg1[0];
const char c1 = arg1[1];
   if      (c0 == 'I' && c1 == 'R')   read_file  (arg2, *arg3, var_C, var_D);
   else if (c0 == 'I' && c1 == 'W')   write_file (arg2, *arg3, var_C, var_D);
   else if (c0 == 'P' && c1 == 'R')   read_pipe  (arg2, *arg3, var_C, var_D);
   else if (c0 == 'P' && c1 == 'W')   write_pipe (arg2, *arg3, var_C, var_D);
   else if (c0 == 'D' && c1 == 'L')   delete_file(arg2,        var_C);
   else if (c0 == 'R' && c1 == 'N')   rename_file(arg2,  arg3, var_C);
   else    set_ACK(var_C, 1);   // 1 := INVALID COMMAND
}
//════════════════════════════════════════════════════════════════════════════
bool
is_valid_varname(const uint32_t * varname)
{
   // AP210 supports only control variables starting with 'C' and data
   // variables starting with 'D'.
   //
   if (varname[0] == 'C')   return true;
   if (varname[0] == 'D')   return true;
   return false;
}
//════════════════════════════════════════════════════════════════════════════
bool
initialize(Coupled_var & var)
{
   coupled_vars.push_back(var);
//════════════════════════════════════════════════════════════════════════════
const uint32_t * varname = Svar_DB::get_svar_name(var.key);
   if (!varname)   return true;

   if (*varname == 'C')
      {
        Svar_DB::set_control(var.key, USE_BY_1);
        ++var_count_210;
      }
   else if (*varname == 'D')
      {
      }
   else
      {
        return true;  // error
      }
   return false;   // OK
}
//────────────────────────────────────────────────────────────────────────────
bool
make_counter_offer(SV_key key)
{
   return true;   // make counter offer
}
//────────────────────────────────────────────────────────────────────────────
APL_error_code
assign_value(Coupled_var & var, const string & data)
{
   // check CDR size, type, and shape of data. We expect a char string
   // for both the CTL and the DAT variable.
   //
   if (data.size() < sizeof(CDR_header))   // too short
      { error_loc = LOC;   return E_LENGTH_ERROR; }

const CDR_header * header = reinterpret_cast<const CDR_header *>(data.c_str());

const int rank = header->rank;

const uint32_t * varname = Svar_DB::get_svar_name(var.key);
   if (!varname)   return E_VALUE_ERROR;

   if (*varname == 'D')   // e.g. DAT←value
      {
        delete var.data;
        var.data = new CDR_string(reinterpret_cast<const uint8_t *>
                                  (data.c_str()), data.size());
        return E_NO_ERROR;
      }

   // CTL←value
   //
   if (rank > 1)
      { error_loc = LOC;   return E_RANK_ERROR; }

SV_key key_D = Svar_DB::find_pairing_key(var.key);
   if (key_D == 0)   { error_loc = LOC;   return E_VALUE_ERROR; }
Coupled_var * var_D = 0;
   for (size_t c = 0; c < coupled_vars.size(); ++c)
       {
         if (coupled_vars[c].key == key_D)
            {
              var_D = &coupled_vars[c];
              break;
            }
       }

   if (var_D == 0)
      {
        get_CERR() << "Could not find matching data variable for key "
                   << var.key << " (svar " << varname
                   << ")" << endl
                   << "unknown key_D = " << key_D << endl
                   << "known keys:" << endl;
        for (size_t c = 0; c < coupled_vars.size(); ++c)
            get_CERR() << " key[" << c << "] = " << coupled_vars[c].key << endl;

        error_loc = LOC;   return E_VALUE_ERROR;
      }


   delete var.data;
   var.data = new CDR_string(reinterpret_cast<const uint8_t *>(data.c_str()),
                             data.size());

SVAR_context * ctx = var.context;
   if (ctx)
      {
        ctx->state_D = SVS_NOT_SHARED;
        sub_command(*ctx);

        // sub_command() deletes ctx on the "close file" sub-command (nelm
        // == 0), after first clearing var_C.context/var_D.context to 0 --
        // ctx is a use-after-free below unless we notice that (Blake
        // McBride, Bugs15 #1). var IS ctx's var_C (both alias the same
        // Coupled_var element of coupled_vars, see the CTL/DAT dispatch
        // above), so var.context reflects that clear directly.
        //
        if (var.context == 0)   ctx = 0;

        // a sub-command was triggered by a write to C, so C is always written.
        // in addition D may have been read  or written.
        //
        Svar_DB::set_state(var.key, false, LOC);

        if (ctx && ctx->state_D != SVS_NOT_SHARED)   // var D read or written
           {
             if (!Svar_DB::get_svar_name(key_D)) { error_loc = LOC;
                                                   return E_VALUE_ERROR; }
             Svar_DB::set_state(key_D, ctx->state_D == SVS_IDLE, LOC);
           }
      }
   else
      {
        // header + 1 + 4*rank must not exceed the end of data: rank comes
        // straight from the (untrusted) CDR payload, and cmd is otherwise
        // handed to handle_cmd()/parse_arg() on the assumption that it
        // points somewhere inside data's own NUL-terminated buffer (Blake
        // McBride, Bugs15 #13) -- "happens to stay inside the owning
        // std::string's buffer today" per the report is not a guarantee.
        //
        const char * const cmd_start =
           reinterpret_cast<const char *>(header + 1) + 4*rank;
        const char * const data_end = data.c_str() + data.size();
        if (cmd_start < data.c_str() || cmd_start > data_end)
           { error_loc = LOC;   return E_LENGTH_ERROR; }

        handle_cmd(cmd_start, int(data_end - cmd_start), var, *var_D);
        if (!Svar_DB::get_svar_name(key_D))
           {
             error_loc = LOC;
             return E_VALUE_ERROR;
           }
   Svar_DB::set_state(var.key, false, LOC);
   Svar_DB::set_state(key_D, false, LOC);
      }

   error_loc = "no_error";   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
APL_error_code
get_value(Coupled_var & var, string & data)
{
   if (var.data == 0)   return E_VALUE_ERROR;

   data = string(reinterpret_cast<const char *>(var.data->get_items()),
                 var.data->size());
   error_loc = "no_error";   return E_NO_ERROR;
}
//════════════════════════════════════════════════════════════════════════════
void
retract(Coupled_var & var)
{
   // Coupled_var is a plain aggregate with no destructor, so its payload
   // must be freed explicitly here -- it never was (Blake McBride, Bugs15
   // #14), leaking one CDR_string per retracted variable.
   //
   delete var.data;
   var.data = 0;

SVAR_context * ctx = var.context;
   if (ctx)
      {
        if (ctx->file)   fclose(ctx->file);
        ctx->file = 0;

        ctx->var_C().context = ctx->var_D().context = 0;
        delete ctx;
      }

   // var_count_210 is incremented in initialize() for every 'C' variable
   // offered, regardless of whether a file is ever opened on it, but was
   // only ever decremented here when ctx existed, i.e. only when a file
   // HAD been opened (Blake McBride, Bugs15 #14): retracting a control
   // variable that never opened a file left the count permanently above
   // zero, so the exit(0) below could never fire again and this
   // auto-started AP210 process would linger forever. Match initialize()'s
   // own condition instead of ctx's.
   //
   if (const uint32_t * varname = Svar_DB::get_svar_name(var.key))
      {
        if (*varname == 'C')
           {
             --var_count_210;
             if (var_count_210 == 0)   exit(0);
           }
      }

}
//════════════════════════════════════════════════════════════════════════════
