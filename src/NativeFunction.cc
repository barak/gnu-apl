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

#include "config.h"

#if MINGW_SRC
# define dlsym(x, y) 0
# define dlclose(x)
#else
# include <dlfcn.h>
#endif // ! MINGW_SRC

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "Common.hh"
#include "Error.hh"
#include "LibPaths.hh"
#include "NativeFunction.hh"
#include "Native_interface.hh"
#include "Symbol.hh"
#include "Workspace.hh"

std::vector<NativeFunction *> NativeFunction::valid_functions;

//════════════════════════════════════════════════════════════════════════════
NativeFunction *
NativeFunction::fix(const UCS_string & so_name,
                    const UCS_string & function_name)
{
   // if the function already exists then return it.
   //
   loop(v, valid_functions.size())
      {
        NativeFunction * fun = valid_functions[v];
        if (so_name == fun->so_path)
           {
             // the NativeFunction object exists, but may have been
             // )ERASEd at APL level. If so, then re-install it.
             //
             Symbol * sym = Workspace::lookup_symbol(fun->get_name());
             Assert(sym);

             const char * why = sym->cant_be_defined();
             if (why)
                {
                  MORE_ERROR() << why;
                  return 0;
                }

             if (fun->is_operator())   sym->set_NC(NC_OPERATOR, fun);
             else                      sym->set_NC(NC_FUNCTION, fun);
             return fun;
           }
      }

NativeFunction * new_function = new NativeFunction(so_name, function_name);
   Log(LOG_delete)
      CERR << "new    " << voidP(new_function) << " at " LOC << endl;


   if (!new_function->valid)   // something went wrong
      {
        Log(LOG_delete)
          CERR << "delete " << voidP(new_function) << " at " LOC << endl;
        delete new_function;
        return 0;
      }

   return new_function;
}
//────────────────────────────────────────────────────────────────────────────
void
NativeFunction::cleanup()
{
   // delete in reverse construction order
   //
   while(valid_functions.size())
      {
        NativeFunction * fun = valid_functions.back();
        valid_functions.pop_back();

        if (fun->close_fun && fun->handle)
           {
             const bool do_dlclose = (*fun->close_fun)(CAUSE_SHUTDOWN, fun);
             if (do_dlclose)
                {
#if MINGW_SRC
                  MORE_ERROR() << "Native functions work only on GNU/Linux.";
                  DOMAIN_ERROR;
#else
                  dlclose(fun->handle);
#endif
                  fun->handle = 0;
                }
           }

        // don't delete fun since this will be done when the symbol for the
        // fun is being deleted
      }
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
NativeFunction::load_emacs_library(const char * emacs_arg)
{
UCS_string so_path(U"libemacs");
UCS_string t4;

void * handle = open_so_file(t4, so_path);
   if (handle == 0)   return t4;

   t4 = UCS_ASCII_string("found emacs library ");
   t4 << so_path;

void * emacs_start = dlsym(handle, "emacs_start");
   if (emacs_start == 0)
      {
        t4 << ", but it\n   it is lacking the mandatory "
                       "function emacs_start()\n";
        dlclose(handle);
        return t4;
      }

UTF8_string so_path_utf(so_path);
const int error =
    reinterpret_cast<int (*)(const char *, const char *)>(emacs_start)
            (emacs_arg, so_path_utf.c_str());

   if (error)
      {
        dlclose(handle);
        return t4 << ", but emacs_start()  returned error " << error << UNI_LF;
      }

   t4.clear();   // success
   return t4;
}
//────────────────────────────────────────────────────────────────────────────
NativeFunction::NativeFunction(const UCS_string & so_name,
                               const UCS_string & apl_name)
   : Function(ID_USER_SYMBOL, TOK_FUN2),
     handle(0),
     name(apl_name),
     original_so_path(so_name),
     so_path(so_name),
     valid(false),
     close_fun(0)
{
UCS_string t4;
   handle = open_so_file(t4, so_path);
   if (handle == 0)
      {
         MORE_ERROR() << t4;
         return;
      }

   t4 = UCS_ASCII_string("shared library ");
   t4 << so_name << " ";

   // get the function multiplexer
   //
void * fmux = dlsym(handle, "get_function_mux");
     if (!fmux)
        {
          CERR << "shared library " << so_name << " is lacking the mandatory "
                  "function get_function_mux() !" << endl;
          t4 << "is invalid (no get_function_mux())";
          MORE_ERROR() = t4;
          return;
        }

void * (*get_function_mux)(const char *) =
                 reinterpret_cast<void * (*)(const char *)>(fmux);

   // get the mandatory function get_ABI_version() and reject this library
   // if it is missing, or if it does not match the ABI contract version
   // (see Native_interface.hh) that this interpreter itself was built
   // against. Without this check, a .so built against a different,
   // binary-incompatible GNU APL (for example a stale library left behind
   // in an unversioned, bare-name-resolved directory like
   // /usr/local/lib/apl/ after an upgrade) would be dlopen()ed and its
   // function pointers called anyway, silently corrupting the
   // interpreter's heap through a mismatched Value/Cell layout instead of
   // failing cleanly. This check must happen before any other function
   // pointer obtained from this library is called.
   //
   {
     void * get_abi = get_function_mux("get_ABI_version");
     if (!get_abi)
        {
          t4 << "is invalid (no get_ABI_version(); built against an "
                "older, incompatible GNU APL and needs to be rebuilt)";
          MORE_ERROR() << t4;
          return;
        }

     const int lib_abi = reinterpret_cast<int (*)()>(get_abi)();
     if (lib_abi != NATIVE_ABI_VERSION)
        {
          t4 << "is invalid (ABI version " << lib_abi << ", but this "
                "interpreter uses ABI version " << NATIVE_ABI_VERSION
             << "; the library needs to be rebuilt against this GNU APL)";
          MORE_ERROR() << t4;
          return;
        }
   }

   // get the mandatory function get_signature() which returns
   //  the function signature
   //
   {
     void * get_sig = get_function_mux("get_signature");
     if (!get_sig)
        {
          CERR << "shared library is lacking the mandatory "
                  "function signature() !" << endl;
          t4 << "is invalid (no get_signature())";
          MORE_ERROR() << t4;
          return;
        }


     signature = reinterpret_cast<Fun_signature (*)()>(get_sig)();
   }

   // get the optional function close_fun(), which is called before
   // this function disappears
   {
     void * cfun = get_function_mux("close_fun");
     if (cfun)   close_fun = reinterpret_cast
                             <bool (*)(Cause, const NativeFunction *)>(cfun);
     else        close_fun = 0;
   }

   // create an entry in the symbol table
   //
Symbol * sym = Workspace::lookup_symbol(apl_name);
   Assert(sym);

const char * why = sym->cant_be_defined();
   if (why)
      {
        MORE_ERROR() << why;
        return;
      }

   /// read function pointers...
   //
#define Th const NativeFunction * th

#define ev(fun, args) \
   f_ ## fun = reinterpret_cast<Token (*) args>(get_function_mux(#fun))

   ev(eval_        , (                                Th));

   ev(eval_B       , (                          Vr B, Th));
   ev(eval_AB      , (Vr A,                     Vr B, Th));
   ev(eval_XB      , (                    Vr X, Vr B, Th));
   ev(eval_AXB     , (Vr A,               Vr X, Vr B, Th));

   ev(eval_LB      , (      Fr LO,              Vr B, Th));
   ev(eval_ALB     , (Vr A, Fr LO,              Vr B, Th));
   ev(eval_LXB     , (      Fr LO,        Vr X, Vr B, Th));
   ev(eval_ALXB    , (Vr A, Fr LO,        Vr X, Vr B, Th));

   ev(eval_LRB     , (      Fr LO, Fr RO,       Vr B, Th));
   ev(eval_LRXB    , (      Fr LO, Fr RO, Vr X, Vr B, Th));
   ev(eval_ALRB    , (Vr A, Fr LO, Fr RO,       Vr B, Th));
   ev(eval_ALRXB   , (Vr A, Fr LO, Fr RO, Vr X, Vr B, Th));

   ev(eval_fill_B  , (                          Vr B, Th));
   ev(eval_fill_AB , (Vr A,                     Vr B, Th));
   ev(eval_ident_Bx, (Vr B,            sAxis x,       Th));
#undef ev

   // compute function tag based on the signature
   //
   if      (signature & SIG_RO)   tag = TOK_OPER2;
   else if (signature & SIG_LO)   tag = TOK_OPER1;
   else if (signature & SIG_B )   tag = TOK_FUN2;
   else                           tag = TOK_FUN0;

   if (is_operator())   sym->set_NC(NC_OPERATOR, this);
   else                 sym->set_NC(NC_FUNCTION, this);

   Workspace::more_error().clear();
   valid = true;
   valid_functions.push_back(this);
}
//════════════════════════════════════════════════════════════════════════════
NativeFunction::~NativeFunction()
{
  Log(LOG_UserFunction__enter_leave)
      get_CERR() << "Native function " << get_name() << " deleted." << endl;

   loop(v, valid_functions.size())
      {
        if (valid_functions[v] == this)
           {
             valid_functions.erase(valid_functions.begin() + v);
             break;   // there is at most one entry for `this`
           }
      }

   // the constructor dlopen()s handle before it can fail (missing
   // get_function_mux()/get_signature(), or cant_be_defined()); on that
   // failure path fix() deletes this object with handle still live, and
   // (unlike destroy()/cleanup()) this destructor never dlclose()d it --
   // each failed ⎕FX leaked a handle and kept the library mapped.
   if (handle)   { dlclose(handle);   handle = 0; }
}
//────────────────────────────────────────────────────────────────────────────
/// search \b dir_count directories in \b dirs for \b so_name (trying the
/// name as given, then with .so, then with .dylib appended, unless it
/// already has one of those extensions), WITHOUT opening it. Returns
/// true and sets \b resolved on the first existing + readable match; t4
/// accumulates a "directories/files tried" diagnostic for every miss
/// (only ever shown to the user if the whole open_so_file() search
/// ultimately fails).
static bool
find_so_candidate(const char * const * dirs, int dir_count,
                  const UTF8_string & so_name, UCS_string & t4,
                  UTF8_string & resolved)
{
   loop(d, dir_count)
       {
         if (dirs[d] == 0 || dirs[d][0] == 0)   continue;

         UTF8_string dir_so_path(dirs[d]);
         dir_so_path += '/';
         dir_so_path << so_name;

         UTF8_string dir_only(dir_so_path);
         dir_only[strrchr(dir_only.c_str(), '/') - dir_only.c_str()] = 0;
         if (access(dir_only.c_str(), R_OK | X_OK))
            {
              t4 << "    directory " << dir_only << UNI_LF;
              continue;   // new directory
            }

         // try filename, then filename.so, and then filename.dylib
         // unless the filename has an extension already.
         //
         const char * exts[] = { ".so", ".dylib", "" };

         bool has_extension = false;
         loop(e, sizeof(exts)/sizeof(*exts))
            {
              if (*exts[e])   // not "no extension"
                 {
                   const char * ext = exts[e];
                   const char * end = dir_so_path.c_str();
                   end += strlen(end) - strlen(ext);
                   if (!strcmp(exts[e],  end))
                      {
                        has_extension = true;
                        break;
                      }
                 }
            }

         loop(e, sizeof(exts)/sizeof(*exts))
             {
               if (has_extension && *exts[e])   continue;

               UTF8_string filename(dir_so_path);
               if (exts[e])   filename << UTF8_string(exts[e]);

               if (access(filename.c_str(), R_OK) == 0)
                  {
                    resolved = filename;
                    return true;
                  }

               const int t4_len = t4.size();
               t4 << "    file " << filename.c_str();
               while (t4.ssize() < t4_len + 44)     t4 << UNI_SPACE;
               t4 << " (" << strerror(errno) << ")\n";
             }
       }

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void *
NativeFunction::open_so_file(UCS_string & t4, UCS_string & so_path)
{
   // prepare a )MORE error message containing the file names tried.
   //
   t4.clear();
   t4 << "Could not find shared library '" << so_path << "'\n"
         "The following directories and file names were tried:\n";

   // if the name starts with / (or \ on Windows) or .
   // then take it as is without changes.
   //
   if (so_path[0] == UNI_SLASH     ||
       so_path[0] == UNI_BACKSLASH ||
       so_path[0] == UNI_FULLSTOP)
      {
        UTF8_string filename(so_path);
        void * handle = try_one_file(filename.c_str(), t4);

        if (handle == 0)
           {
             t4 << "NOTE: Filename extensions are NOT automatically added "
                   "when a full path\n"
                   "      (i.e. a path starting with / or .) is used.";
           }
        return handle;
      }

   // otherwise search two groups of directories: the traditional
   // installed locations, and (Bill Heagy: "have apl look for libraries
   // relative to its location, rather than absolute, so that I don't
   // have to install to test") a location next to the running apl
   // binary itself. The latter is an ADDITION, not a replacement --
   // installing .so files directly next to the binary (e.g. in
   // /usr/local/bin) would violate the Filesystem Hierarchy Standard,
   // so the installed locations must still be searched too. When both
   // groups have a matching file, the more recently modified one wins
   // (so a fresh build-tree .so found next to the binary is preferred
   // over a stale installed one, without silently ignoring an installed
   // one that happens to be newer, e.g. after a real upgrade).
   //
UTF8_string utf_so_path(so_path);

const char * std_dirs[] =
{
  apl_DIR__pkglib,    // the normal case
  "/usr/lib/apl",
  "/usr/local/lib/apl",
  ".",
  "./native",             // if make install was not performed
  "./native/.libs",       // libtool's actual .so lives here, not directly
                          // in ./native, until 'make install' copies it
  "./emacs_mode",
  "./emacs_mode/.libs",
};

   // most likely apl_DIR__pkglib is /usr/lib/apl or /usr/local/lib/apl.
   // don't try them twice.
   //
   if (!strcmp(apl_DIR__pkglib, std_dirs[1]))   std_dirs[1] = 0;
   if (!strcmp(apl_DIR__pkglib, std_dirs[2]))   std_dirs[2] = 0;

const std::string bin_path(LibPaths::get_APL_bin_path());
const std::string bin_native      = bin_path + "/native";
const std::string bin_native_libs = bin_path + "/native/.libs";
const std::string bin_emacs       = bin_path + "/emacs_mode";
const std::string bin_emacs_libs  = bin_path + "/emacs_mode/.libs";
const char * bin_dirs[] =
{
  bin_path.c_str(),
  bin_native.c_str(),
  bin_native_libs.c_str(),
  bin_emacs.c_str(),
  bin_emacs_libs.c_str(),
};

UTF8_string std_resolved, bin_resolved;
const bool std_found = find_so_candidate(std_dirs, 8, utf_so_path,
                                         t4, std_resolved);
const bool bin_found = find_so_candidate(bin_dirs, 5, utf_so_path,
                                         t4, bin_resolved);

const UTF8_string * winner = 0;
   if (std_found && bin_found)
      {
        struct stat st_std, st_bin;
        winner = &std_resolved;   // default/fallback if either stat() fails
        if (stat(std_resolved.c_str(), &st_std) == 0 &&
            stat(bin_resolved.c_str(), &st_bin)  == 0 &&
            st_bin.st_mtime > st_std.st_mtime)
           {
             winner = &bin_resolved;
           }
      }
   else if (std_found)   winner = &std_resolved;
   else if (bin_found)   winner = &bin_resolved;

   if (winner)
      {
        void * handle = try_one_file(winner->c_str(), t4);
        if (handle)
           {
             so_path = UCS_string(*winner);   // update so_path
             return handle;
           }
      }

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
void *
NativeFunction::try_one_file(const char * filename, UCS_string & t4)
{
#if MINGW_SRC
   MORE_ERROR() << "Native functions work only on GNU/Linux.";
   DOMAIN_ERROR;
#else
const int t4_len = t4.size();
   t4 << "    file " << filename;

   if (access(filename, R_OK) != 0)
      {
        while (t4.ssize() < t4_len + 44)     t4 << UNI_SPACE;
        t4 << " (" << strerror(errno) << ")\n";
        return 0;
      }

   if (void * handle = dlopen(filename, RTLD_LAZY))   return handle;

   // dlerror() returns 0 when no error is pending. POSIX guarantees a
   // message "since the last call to dlerror()" after a failed dlopen(),
   // but that contract is easy to lose (e.g. an intervening dlerror()
   // call elsewhere, a threaded caller) -- a null here previously crashed
   // inside strrchr() (Blake McBride, Bugs15 #15).
   //
const char * err = dlerror();
   if (err == 0)             err = "dlopen() failed";
   else if (strrchr(err, ':'))   err = 1 + strrchr(err, ':');

   while (t4.ssize() < t4_len + 44)     t4 << UNI_SPACE;
   t4 << " (" << err << " )\n";
   return 0;
#endif
}
//────────────────────────────────────────────────────────────────────────────
bool
NativeFunction::has_result() const
{
   return signature & SIG_Z;
}
//────────────────────────────────────────────────────────────────────────────
bool
NativeFunction::is_operator() const
{
   // this function is an operator if one of the eval functions with a
   // function argument is present...
   //
   return f_eval_LB
       || f_eval_ALB
       || f_eval_LRB
       || f_eval_ALRB
       || f_eval_LXB
       || f_eval_ALXB
       || f_eval_LRXB
       || f_eval_ALRXB;
}
//────────────────────────────────────────────────────────────────────────────
void
NativeFunction::print_properties(ostream & out, int indent) const
{
UCS_string ind(indent, UNI_SPACE);
   out << ind << "Native Function " << endl;
}
//────────────────────────────────────────────────────────────────────────────
ostream &
NativeFunction::print(std::ostream & out) const
{
   if (is_operator())   out << "native Operator " << name << endl;
   else                 out << "native Function " << name << endl;

   return out;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
NativeFunction::canonical(bool with_lines) const
{
   return original_so_path;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_() const
{
   if (f_eval_)   return (*f_eval_)(this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_B(cValue_R B) const
{
   if (f_eval_B)   return (*f_eval_B)(&B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_AB(cValue_R A, cValue_R B) const
{
   if (f_eval_AB)   return (*f_eval_AB)(&A, &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_LB(Token & LO, cValue_R B) const
{
   if (f_eval_LB)   return (*f_eval_LB)(*LO.get_function(), &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_ALB(cValue_R A, Token & LO, cValue_R B) const
{
   if (f_eval_ALB)   return (*f_eval_ALB)(&A, *LO.get_function(), &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_LRB(Token & LO, Token & RO, cValue_R B) const
{
   if (f_eval_LRB)   return (*f_eval_LRB)(*LO.get_function(), *RO.get_function(), &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_ALRB(cValue_R A, Token & LO, Token & RO, cValue_R B) const
{
   if (f_eval_ALRB)   return (*f_eval_ALRB)(&A, *LO.get_function(), *RO.get_function(), &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_XB(cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_XB)   return (*f_eval_XB)(&X, &B, this);
   else             return eval_B(B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_AXB(cValue_R A, cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_AXB)   return (*f_eval_AXB)(&A, &X, &B, this);
   else              return eval_AB(A, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_LXB(Token & LO, cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_LXB)   return (*f_eval_LXB)(*LO.get_function(), &X, &B, this);
   else              return eval_LB(LO, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_ALXB(cValue_R A, Token & LO, cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_ALXB)   return (*f_eval_ALXB)(&A, *LO.get_function(), &X, &B, this);
   else               return eval_ALB(A, LO, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_LRXB(Token & LO, Token & RO, cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_LRXB)   return (*f_eval_LRXB)(*LO.get_function(), *RO.get_function(), &X, &B, this);
   else                return eval_LRB(LO, RO, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_ALRXB(cValue_R A, Token & LO, Token & RO, cValue_R X, cValue_R B) const
{
   // call axis variant if present, or else the non-axis variant.
   //
   if (f_eval_ALRXB)
      return (*f_eval_ALRXB)(&A, *LO.get_function(), *RO.get_function(), &X, &B, this);
   return eval_ALRB(A, LO, RO, B);
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_fill_B(cValue_R B) const
{
   if (f_eval_fill_B)   return (*f_eval_fill_B)(&B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_fill_AB(cValue_R A, cValue_R B) const
{
   if (f_eval_fill_AB)   return (*f_eval_fill_AB)(&A, &B, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
Token
NativeFunction::eval_identity_fun(cValue_R B, sAxis axis) const
{
   if (f_eval_ident_Bx)   return (*f_eval_ident_Bx)(&B, axis, this);

   SYNTAX_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
void
NativeFunction::destroy()
{
   if (close_fun && handle)
      {
        const bool do_dlclose = (*close_fun)(CAUSE_ERASED, this);
        if (do_dlclose)
           {
             dlclose(handle);
             handle = 0;
             close_fun = 0;
           }
      }
   else if (handle)
      {
        dlclose(handle);
        handle = 0;
      }

   delete this;
}
//════════════════════════════════════════════════════════════════════════════

