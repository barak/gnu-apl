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
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "Avec.hh"
#include "Backtrace.hh"
#include "Error.hh"
#include "Output.hh"
#include "Parser.hh"
#include "StateIndicator.hh"
#include "Tokenizer.hh"
#include "Symbol.hh"
#include "UserFunction_header.hh"
#include "Value.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
//// constructor for a normal (non-lambda) defined function header
//════════════════════════════════════════════════════════════════════════════
UserFunction_header::UserFunction_header(const UCS_string & text0, bool macro)
  : error(E_DEFN_ERROR),   // assume bad headr
    error_info("Bad header"),
    sym_Z(0),
    sym_A(0),
    sym_LO(0),
    sym_FUN(0),
    sym_RO(0),
    sym_X(0),
    sym_B(0)
{
UCS_string text(text0);
   text.remove_comment();

UCS_string signature_text;
UCS_string lvar_text;

   // split header text into signature and local variables strings
   {
     bool in_signature = true;
     loop(t, text.size())
         {
           const Unicode uni = text[t];
           if (uni == UNI_CR)   continue;   // ignore CR
           if (uni == UNI_LF)   break;      // stop at LF
           if (uni == UNI_SEMICOLON)   in_signature = false;
           if (in_signature)   signature_text << uni;
           else                lvar_text << uni;
         }
   }

   if (signature_text.size() == 0)
      {
        error_info = "Empty header line";
        return;
      }

   Log(LOG_UserFunction__set_line)
      {
        CERR << "[0] " << signature_text << lvar_text << endl;
        // show_backtrace(__FILE__, __LINE__);
      }

   if ((error_info = init_signature(signature_text, macro)))   return;

   // λ-prefixed names are reserved for genuine, machine-generated lambdas
   // (built via the OTHER, lambda_num-based constructor below, never this
   // text-based one) -- a user-supplied header naming a regular function
   // "λ1" or the like must not be accepted as if it were one: is_lambda()
   // goes by name alone, so such a function would misreport itself as a
   // lambda (refcount tracking, ∇-edit warnings, ⎕NC/⎕EX confusion) while
   // never having been through the actual lambda bookkeeping (Blake
   // McBride, Bugs28 #69).
   //
   if (function_name.size() && function_name[0] == UNI_LAMBDA)
      {
        error_info = "Bad function name (λ is reserved for lambdas)";
        return;
      }

   if ((error_info = init_local_vars(lvar_text, macro)))       return;

   error = E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
//s/ constructor for lambda header
UserFunction_header::UserFunction_header(Fun_signature sig, int lambda_num)
  : error(E_DEFN_ERROR),
    error_info("Bad header"),
    sym_Z(0),
    sym_A(0),
    sym_LO(0),
    sym_FUN(0),
    sym_RO(0),
    sym_X(0),
    sym_B(0)
{
   function_name << UNI_LAMBDA << lambda_num;

   if (!signature_is_valid(sig))
      {
         error_info = "Invalid signature";
         return;
      }

                       sym_Z  = &Workspace::get_v_LAMBDA();
   if (sig & SIG_A)    sym_A  = &Workspace::get_v_ALPHA();
   if (sig & SIG_LO)   sym_LO = &Workspace::get_v_ALPHA_U();
   if (sig & SIG_RO)   sym_RO = &Workspace::get_v_OMEGA_U();
   if (sig & SIG_B)    sym_B  = &Workspace::get_v_OMEGA();
   if (sig & SIG_X)    sym_X  = &Workspace::get_v_CHI();

   error_info = 0;
   error = E_NO_ERROR;
}
//────────────────────────────────────────────────────────────────────────────
bool
UserFunction_header::localizes(const Symbol * sym) const
{
   if (sym == sym_Z)   return true;
   if (sym == sym_A)   return true;
   if (sym == sym_LO)  return true;
   if (sym == sym_RO)  return true;
   if (sym == sym_X)   return true;
   if (sym == sym_B)   return true;

   loop(l, local_vars.size())     if (sym == local_vars[l])         return true;
   loop(l, label_values.size())   if (sym == label_values[l].sym)   return true;

   return false;
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::print_properties(ostream & out, int indent) const
{
UCS_string ind(indent, UNI_SPACE);
   out << (is_operator() ? "Operator " : "Function ")
       << function_name << endl;

   if (sym_Z)    out << ind << "Result:        " << *sym_Z  << endl;
   if (sym_A)    out << ind << "Left Val Arg:  " << *sym_A  << endl;
   if (sym_LO)   out << ind << "Left Fun Arg:  " << *sym_LO << endl;
   if (sym_RO)   out << ind << "Right Fun Arg: " << *sym_RO << endl;
   if (sym_B)    out << ind << "Right Val Arg: " << *sym_B  << endl;

   if (local_vars.size())
      {
        out << ind << "Local Variables:";
        loop(l, local_vars.size())   out << " " << *local_vars[l];
        out << endl;
      }

   if (label_values.size())
      {
        out << ind << "Labels:        ";
        loop(l, label_values.size())
           {
             if (l)   out << ",";
             out << " " << *label_values[l].sym
                 << "=" << label_values[l].line;
           }
        out << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::pop_local_vars() const
{
   loop(l, label_values.size())   label_values[l].sym->pop();

   loop(l, local_vars.size())   local_vars[l]->pop_local();   // Bugs35 #47

   if (sym_B)    sym_B ->pop();
   if (sym_X)    sym_X ->pop();
   if (sym_RO)   sym_RO->pop();
   if (sym_LO)   sym_LO->pop();
   if (sym_A)    sym_A ->pop();
   if (sym_Z)    sym_Z ->pop();
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::print_local_vars(ostream & out) const
{
   if (sym_Z)     out << " " << *sym_Z;
   if (sym_A)     out << " " << *sym_A;
   if (sym_LO)    out << " " << *sym_LO;
   if (sym_RO)    out << " " << *sym_RO;
   if (sym_B)     out << " " << *sym_B;

   loop(l, local_vars.size())   out << " " << *local_vars[l];
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::reverse_local_vars()
{
const ShapeItem half = local_vars.size() / 2;   // = rounded down!
   loop(v, half)
      {
        Symbol * tmp = local_vars[v];
        local_vars[v] = local_vars[local_vars.size() - v - 1];
        local_vars[local_vars.size() - v - 1] = tmp;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::remove_duplicate_local_variables()
{
   // remove local vars that are also labels, arguments or return values.
   // This is to avoid pushing them twice
   //
   remove_duplicate_local_var(sym_Z,   0);
   remove_duplicate_local_var(sym_A,   0);

   // sym_LO, sym_FUN, and sym_RO are not pushed, so they are never duplicates

   remove_duplicate_local_var(sym_X,   0);
   remove_duplicate_local_var(sym_B,   0);

   loop(l, label_values.size())
      remove_duplicate_local_var(label_values[l].sym, 0);

   loop(l, local_vars.size())
      remove_duplicate_local_var(local_vars[l], l + 1);
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UserFunction_header::lambda_header(Fun_signature sig, int lambda_num)
{
UCS_string ucs;

   if (sig & SIG_Z)      ucs << "λ←";
   if (sig & SIG_A)      ucs << "⍺ ";
   if (sig & SIG_LORO)   ucs << "(";
   if (sig & SIG_LO)     ucs << "⍶ ";
   ucs << UNI_LAMBDA << lambda_num;
   if (sig & SIG_RO)     ucs << " ⍹ ";
   if (sig & SIG_LORO)   ucs << ")";
   if (sig & SIG_X)      ucs << "[χ]";
   if (sig & SIG_B)      ucs << " ⍵";

   return ucs;
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::eval_common() const
{
   Log(LOG_UserFunction__enter_leave)   CERR << "eval_common()" << endl;

   // push local variables...
   //
   loop(l, local_vars.size())   local_vars[l]->push();

   // push labels...
   //
   loop(l, label_values.size())
       label_values[l].sym->push_label(label_values[l].line);
}
//────────────────────────────────────────────────────────────────────────────
/// return true iff \b sym is a system variable (⎕IO, bare ⎕, ...) -- these
/// are TC_SYMBOL tokens too (see Token.def), so every "is this a symbol"
/// check in init_signature() below accepts them, but none of the header
/// positions (function name, Z, A, B, LO, RO, axis) may legally be one:
/// unlike init_local_vars()'s explicit small whitelist of localizable
/// system variables (⎕CT, ⎕IO, ...), a header position permanently binds
/// the name, and a system variable's identity/semantics cannot be
/// shadowed that way.
///
/// Checked by NAME, not by get_NC() == NC_SYSTEM_VAR: at this point in
/// header parsing the tokens haven't necessarily been resolved against
/// the real, permanent system-variable Symbol objects yet (the function
/// *name* specifically is looked up later, in UserFunction::fix(), via
/// Workspace::lookup_symbol() on the plain name text) -- so a symbol's
/// NC here can still be unclassified even for ⎕IO. Avec::is_quad() on
/// the first character is what SymbolTable::lookup_symbol() itself
/// guards against ("should not be called for ⎕xx"); checking the same
/// thing here, before that call ever happens, is what actually avoids
/// hitting that internal-error guard.
static bool
is_system_var(const Symbol * sym)
{
   const UCS_string & name = sym->get_name();
   if (name.size() == 0)   return false;
   if (Avec::is_quad(name[0]))   return true;

   // ⍺ ⍶ χ ⍵ ⍹ (Bugs28 #89): the tokenizer gives these their own
   // distinguished Symbol (Workspace::get_v_ALPHA() etc., not an
   // ordinary SymbolTable entry), same as it does for ⎕xx, but they
   // all still carry the generic TC_SYMBOL class that every "is this a
   // symbol" check here accepts -- so e.g. ⎕FX 'Z←⍵ B' 'Z←B+1' silently
   // bound the function to the *distinguished* ⍵ instead of rejecting
   // it: the function became callable as "⍵ 5" but could not be
   // listed, expunged, or saved, since none of those operate on
   // anything outside the real SymbolTable, and re-defining it leaked
   // the previous body (UserFunction::fix() looks the name up via
   // Workspace::lookup_symbol(), which creates an ordinary new Symbol
   // for χ rather than returning the distinguished one, so the old
   // UserFunction pointer is never found to be deleted). λ is
   // deliberately not included here: it is the header GNU APL itself
   // uses for a lambda (λ←λ1 ⍵), so rejecting it here would break every
   // lambda definition.
   //
   // ⍞ (Bugs28 #100(b)): same reasoning as above -- its own
   // distinguished Symbol (Workspace::get_v_Quad_QUOTE()), tokenized
   // separately from (and not covered by) Avec::is_quad()'s plain ⎕
   // check above, even though ⍞ is exactly as much a distinguished
   // name as ⎕ itself. ⎕FX 'Z←F1 ⍞' 'Z←1' used to be accepted (and
   // callable, printing through the real ⍞), unlike e.g. ⎕FX 'Z←F2
   // ⎕IO' 'Z←1', which was already correctly rejected.
   //
   return name[0] == UNI_ALPHA  || name[0] == UNI_ALPHA_UNDERBAR ||
          name[0] == UNI_CHI    || name[0] == UNI_QUOTE_Quad     ||
          name[0] == UNI_OMEGA  || name[0] == UNI_OMEGA_UNDERBAR;
}
//────────────────────────────────────────────────────────────────────────────
const char *
UserFunction_header::init_signature(const UCS_string & text, bool macro)
{
Token_string tos;
   {
     const Tokenizer tokenizer(PM_FUNCTION, LOC, macro);
     try   { tokenizer.tokenize(text, tos); }
     catch (const Error & err)
        {
          error = err.get_error_code();
          return "Tokenize error (function signature)";
        }
   }

size_t start = 0;
size_t len   = tos.size();

   if (len >= 2 && tos[1].get_Class() == TC_ASSIGN)   // expect Z ← ...
      {
        if (tos[0].get_Class() != TC_SYMBOL)   return "Bad Z in Z ←";

        sym_Z = tos[0].get_sym_ptr();   // Z ← or λ ←
        if (is_system_var(sym_Z))   return "Bad Z in Z ← (system variable)";
        start = 2;
        len -= 2;
      }

   if (len <= 3)   // F0, F1 B, or A F2 B
      {
        if (len == 0)   // error: empty signature
           return sym_Z ? "Empty header (after Z ←)" : "Empty header";

        if (len == 1)   // F0
           {
             if (tos[start].get_Class() != TC_SYMBOL)   return "Bad F0";

             sym_FUN = tos[start].get_sym_ptr();
             if (is_system_var(sym_FUN))   return "Bad F0 (system variable)";
             function_name = sym_FUN->get_name();
             return 0;   // OK
           }

        if (len == 2)   // F1 B
           {
             if (tos[start].get_Class() != TC_SYMBOL)
                return "Bad F1 in F1 B";

             if (tos[start + 1].get_Class() != TC_SYMBOL)
                return "Bad B in F1 B";

             sym_FUN = tos[start].get_sym_ptr();
             sym_B = tos[start + 1].get_sym_ptr();
             if (is_system_var(sym_FUN))
                return "Bad F1 in F1 B (system variable)";
             if (is_system_var(sym_B))
                return "Bad B in F1 B (system variable)";
             function_name = sym_FUN->get_name();
             return 0;   // OK
           }

          // otherwise: A F2 B
             if (tos[start].get_Class() != TC_SYMBOL)
                return "Bad A in A F2 B";

             if (tos[start + 1].get_Class() != TC_SYMBOL)
                return "Bad F2 in A F2 B";

             if (tos[start + 2].get_Class() != TC_SYMBOL)
                return "Bad B in A F2 B";
             sym_A   = tos[start]    .get_sym_ptr();
             sym_FUN = tos[start + 1].get_sym_ptr();
             sym_B   = tos[start + 2].get_sym_ptr();
             if (is_system_var(sym_A))
                return "Bad A in A F2 B (system variable)";
             if (is_system_var(sym_FUN))
                return "Bad F2 in A F2 B (system variable)";
             if (is_system_var(sym_B))
                return "Bad B in A F2 B (system variable)";
             function_name = sym_FUN->get_name();
             return 0;   // OK
      }

   // at this point the signature has 4 or more token after the
   // optional Z← or λ←. Strip of the final B.
   //
   if (tos[start + len - 1].get_Class() != TC_SYMBOL)
      return "Bad B in ... B";
   sym_B   = tos[start + len - 1].get_sym_ptr();
   if (is_system_var(sym_B))   return "Bad B in ... B (system variable)";
   len--;

   // maybe strip off the optional axis [X]
   //
   if (tos[start + len - 1].get_Class() == TC_R_BRACK)
      {
        if (tos[start + len - 2].get_Class() != TC_SYMBOL)
           return "Bad X in ... [X] B";

        if (tos[start + len - 3].get_tag() != TOK_L_BRACK)
             return "Bad [ in ... [X] B";

        sym_X   = tos[start + len - 2].get_sym_ptr();
        if (is_system_var(sym_X))
           return "Bad X in ... [X] B (system variable)";
        len -= 3;
      }

   // at this point we should have one of:
   //
   // 1.          F1
   // 2.   A      F2
   // 3.   A ( LO OP1 )
   // 4.     ( LO OP1 )
   // 5.   A ( LO OP2 RO )
   // 6.     ( LO OP2 RO )
   //
   if (len == 1)   // case 1.
      {
        if (tos[start].get_tag() != TOK_SYMBOL)
           return "Bad F1 in F1 [X] B";

        sym_FUN = tos[start].get_sym_ptr();
        if (is_system_var(sym_FUN))
           return "Bad F1 in F1 [X] B (system variable)";
        function_name = sym_FUN->get_name();
        return 0;   // OK
      }

   // cases 2-6
   //
   if (tos[start].get_Class() == TC_SYMBOL)   // case 2, 3, or 5: strip A
      {
        sym_A = tos[start].get_sym_ptr();
        if (is_system_var(sym_A))
           return "Bad A in A F2 [X] B (system variable)";
        ++start;
        --len;
      }

   // cases 2, 4, or 6
   //
   if (len == 1)   // case 2.
      {
        if (tos[start].get_Class() != TC_SYMBOL)
           return "Bad F2 in A F2 [X] B";

        sym_FUN = tos[start].get_sym_ptr();
        if (is_system_var(sym_FUN))
           return "Bad F2 in A F2 [X] B (system variable)";
        function_name = sym_FUN->get_name();
        return 0;   // OK
      }

   // cases 3-6
   //
   if (tos[start].get_tag() != TOK_L_PARENT)
      return "Bad ( in (F2 OP ... )";

   if (tos[start + len - 1].get_tag() != TOK_R_PARENT)
      return "Bad ) in (F2 OPn ... )";

   if (tos[start + 1].get_Class() != TC_SYMBOL)
      return "Bad F2 in (F2 OPn ... )";

   sym_LO = tos[start + 1].get_sym_ptr();
   if (is_system_var(sym_LO))
      return "Bad F2 in (F2 OPn ... ) (system variable)";

   if (tos[start + 2].get_Class() != TC_SYMBOL)   // LO
      return "Bad OPn in (F2 OPn ... )";

   sym_FUN = tos[start + 2].get_sym_ptr();
   if (is_system_var(sym_FUN))
      return "Bad OPn in (F2 OPn ... ) (system variable)";
   function_name = sym_FUN->get_name();

   if (len == 4)   return 0;   // OK: ( LO OP1 )

   if (len != 5)   // ( LO OP2 RO )
      return "Bad length in (F2 OPn ... )";

   if (tos[start + 3].get_Class() != TC_SYMBOL)
      return "Bad G2 in (F2 OP2 G2)";

   sym_RO = tos[start + 3].get_sym_ptr();
   if (is_system_var(sym_RO))
      return "Bad G2 in (F2 OP2 G2) (system variable)";
   return 0;
}
//────────────────────────────────────────────────────────────────────────────
const char *
UserFunction_header::init_local_vars(const UCS_string & text, bool macro)
{
Token_string tos;
   {

     const Tokenizer tokenizer(PM_FUNCTION, LOC, macro);
     try   { tokenizer.tokenize(text, tos); }
     catch (const Error & err)
        {
          error = err.get_error_code();
          return "Tokenize error (local variables)";
        }
   }

   // the local names follow a semicolon and are separated by semicolons
   // and/or blanks (LRM p.293: "The header may have blanks instead of
   // semicolons between local names", Bugs35 #49)
   //
bool expect_name = false;   // after a ;
   loop(tos_idx, tos.size())
      {
        const TokenTag tag = tos[tos_idx].get_tag();
        if (tag == TOK_SEMICOL)
           {
             if (expect_name)   return "Name expected (;; in local variables)";
             expect_name = true;
             continue;
           }

        if (tos_idx == 0)   return "Semicolon expected";

        // every system variable may be localized (LRM p.259). For some of
        // them (⎕AI, ⎕EM, ⎕TS, ...) localizing has no effect: their push()
        // and pop() do nothing (NL_SystemVariable). ⎕ and ⍞ are not names,
        // and system functions are not variables (Bugs35 #49, D16).
        //
        const bool sys_var = tos[tos_idx].get_Class() == TC_SYMBOL &&
                             tag != TOK_Quad_Quad && tag != TOK_Quad_QUOTE &&
                             tos[tos_idx].get_sym_ptr()->get_name().size() &&
                             Avec::is_quad(tos[tos_idx].get_sym_ptr()
                                                      ->get_name()[0]);
        if (tag != TOK_SYMBOL && !sys_var)
           {
             // Bugs28 #100(c), same class as Bugs27 #59(i): this
             // used to unconditionally leak an internal debug
             // trace (the raw offending Token) to the session
             // before returning; the returned string alone is
             // what the caller (⎕FX's row-number result, or the
             // ∇ editor's DEFN ERROR) actually reports.
             //
             return "Bad local variable (not a name)";
           }

        local_vars.push_back(tos[tos_idx].get_sym_ptr());
        expect_name = false;
      }

   if (expect_name)   return "Name expected (; at the end of the header)";

   // NOTE: duplicated variables are not an APL error,
   // but we want to localize them only once.
   //
   remove_duplicate_local_variables();

   return 0;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
UserFunction_header::canonical_locals(const UCS_string & text)
{
   // the local names start at the first ; and end at ⍝ (if any)
   //
ShapeItem semi = -1;
ShapeItem end = text.size();
   loop(t, text.size())
      {
        if (text[t] == UNI_COMMENT)                   { end = t;    break; }
        if (text[t] == UNI_SEMICOLON && semi == -1)     semi = t;
      }
   if (semi == -1)   return text;   // no local names

UCS_string ret(text, 0, semi);
   for (ShapeItem t = semi; t < end;)
       {
         if (text[t] != UNI_SPACE)   { ret << text[t++];   continue; }

         // a run of blanks: replace it by ; if it separates two names
         //
         ShapeItem t1 = t;
         while (t1 < end && text[t1] == UNI_SPACE)   ++t1;
         const bool between_names = ret.size() &&
                                    ret.back() != UNI_SEMICOLON &&
                                    t1 < end && text[t1] != UNI_SEMICOLON;
         if (between_names)   ret << UNI_SEMICOLON;
         else                 while (t < t1)   ret << text[t++];
         t = t1;
       }

   while (end < text.ssize())   ret << text[end++];   // the comment
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
bool
UserFunction_header::signature_is_valid(Fun_signature sig)
{
   // if sig is valid then (sig | SIG_Z) is also valid. We can therefore
   // reduce the number of cases.by pretending that SIG_Z is set.
   //
   switch(sig | SIG_Z)
      {
        // niladic
        //
        case SIG_Z_F0:

        // monadic
        //
        case SIG_Z_F1_B:
        case SIG_Z_F1_X_B:
        case SIG_Z_LO_OP1_B:
        case SIG_Z_LO_OP1_X_B:
        case SIG_Z_LO_OP2_RO_B:
        case SIG_Z_LO_OP2_X_RO_B:

        // dyadic
        //
        case SIG_Z_A_F2_B:
        case SIG_Z_A_F2_X_B:
        case SIG_Z_A_LO_OP1_B:
        case SIG_Z_A_LO_OP1_X_B:
        case SIG_Z_A_LO_OP2_RO_B:
        case SIG_Z_A_LO_OP2_X_RO_B: return true;    // valid signature
        default:                    return false;   // invalid signature
      }
}
//────────────────────────────────────────────────────────────────────────────
void
UserFunction_header::remove_duplicate_local_var(const Symbol * sym, size_t pos)
{
   // remove sym from the vector of local variables. Only the local vars
   // at pos or higher are being removed
   //
   if (sym == 0)   return;   // unused symbol

   while (pos < local_vars.size())
       {
         if (sym == local_vars[pos])
            {
              local_vars[pos] = local_vars.back();
              local_vars.pop_back();
              continue;
            }
         ++pos;
       }
}
//════════════════════════════════════════════════════════════════════════════
