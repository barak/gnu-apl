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
    Cmd_DIAG:: implements the diagnostic/debug commands
    )CHECK, )DOXY, )HISTORY, ]OPTIM, ]PSTAT.
*/

#include "config.h"

#include <errno.h>
#include <fstream>

#include "Cmd_DIAG.hh"
#include "Common.hh"
#include "Doxy.hh"
#include "DynamicObject.hh"
#include "Heapsort.hh"
#include "IndexExpr.hh"
#include "IO_Files.hh"
#include "LineInput.hh"
#include "Parallel.hh"
#include "Performance.hh"
#include "Svar_DB.hh"
#include "UserPreferences.hh"
#include "Value.hh"
#include "ValueHistory.hh"
#include "Workspace.hh"

//════════════════════════════════════════════════════════════════════════════
void
Cmd_DIAG::cmd_CHECK(ostream & out, const UCS_string & arg)
{
bool show_OK = true;   // assume more verbose output
   if (arg.size())   // check with argument (supposedly BRIEF).
      {
        UCS_string arg0(arg);
        arg0.remove_leading_and_trailing_whitespaces();
        const UCS_string brief(U"BRIEF");
        show_OK = arg0.compare(brief) != COMP_EQ;   // not BRIEF
        if (show_OK)   // still show_OK
           {
             out << arg << "? Expecting BRIEF." << endl;
             return;

           }
      }
   // 1. erase stale functions from failed ⎕EX
   //
   {
     bool erased = false;
     if (const int stale = Workspace::cleanup_expunged(CERR, erased))
        {
          out << "WARNING - " << stale << " stale functions ("
               << (erased ? "" : "not ") << "erased)" << endl;
        }
     else if (show_OK)
        {
          out << "OK      - no stale functions" << endl;
        }
   }

   // 2. print stale values (if any)
   //
   {
     if (const int stale = Value::print_stale(CERR))
        {
          out << "ERROR   - " << stale << " stale values" << endl;
          IO_Files::apl_error(LOC);
        }
     else if (show_OK)
        {
          out << "OK      - no stale values" << endl;
        }
   }

   // 3. print stale index expressions (if any)
   {
     if (const int stale = IndexExpr::print_stale(CERR))
        {
          out << "ERROR   - " << stale << " stale indices" << endl;
          IO_Files::apl_error(LOC);
        }
     else if (show_OK)
        {
          out << "OK      - no stale indices" << endl;
        }
   }

   // 4. discover duplicate parents. In the old clone() scheme every nested
   //    value has (at most) one parent. In the new clone() scheme, however,
   //    a nested value can be reused and have multiple parents.
   {
#ifndef NEW_CLONE   // old clone
     // 4a. create a { parent = 0, value } vector<val_val> of all values
     //
     std::vector<val_val> val_vals;
     ShapeItem duplicate_parents = 0;
     for (const DynamicObject * obj =
                DynamicObject::get_all_values()->get_next();
          obj != DynamicObject::get_all_values(); obj = obj->get_next())
         {
           const cValue * val = static_cast<const cValue *>(obj);

           val_val vv = { 0, val };   // no parent
           val_vals.push_back(vv);
         }

     // 4b. sort vector<val_val> val_vals by address so that we can search it.
     //
     // .data() (not .front()) since val_vals can legitimately be empty
     // (Blake McBride, Bugs17 #6, related finding) -- .data() is well
     // defined on an empty vector, .front() is not.
     Heapsort<val_val>::sort(val_vals.data(), val_vals.size(), 0,
                             &val_val::greater);
     loop(v, (val_vals.size() - 1))
         Assert(&val_vals[v].child < &val_vals[v + 1].child);

      // 4c. set parents of pointer cells
      //
      loop(v, val_vals.size())   // for every .child (acting as parent here)
          {
            const cValue * val = val_vals[v].child;
            const ShapeItem ec = val->nz_element_count();
            loop(e, ec)   // for every ravel cell of the (parent-) value
                {
                  Cell cache;
                  const Cell & cP = val->get_cravel(e, cache);
                  if (!cP.is_pointer_cell())   continue;   // not a parent

                  const cValue * sub = cP.get_pointer_value().get();
                  Assert1(sub);

                  val_val * vvp = reinterpret_cast<val_val *>
                       Heapsort<val_val>::
                        search(sub, val_vals, val_val::compare, 0);
                  Assert(vvp);
                  if (vvp->parent == 0)   // child has no parent (OK)
                     {
                       vvp->parent = val;
                       continue;
                     }

                  // the child already has a parent (which is bad).
                  // print its history to help figuring why.
                  //
                  ++duplicate_parents;
                  out << "Value * vvp=" << voidP(vvp) << " already has parent "
                      << voidP(vvp->parent) << " when checking Value * val="
                      << voidP(vvp) << endl;

                  out << "History of the child:" << endl;
                  VH_entry::print_history(out, *vvp->child, LOC);
                  out << "History of the first parent:" << endl;
                  VH_entry::print_history(out, *vvp->parent, LOC);
                  out << "History of the second parent:" << endl;
                  VH_entry::print_history(out, *val, LOC);
                  out << endl;
               }
          }

     if (duplicate_parents)
          {
            out << "ERROR   - " << duplicate_parents
                << " duplicate parents" << endl;
            IO_Files::apl_error(LOC);
          }
#endif
          {
            if (show_OK)
               out << "OK      - no duplicate parents" << endl;
          }
   }

   // 5. discover strange Cells and counters.
   //
   Value::check_all_Cells(out);
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_DIAG::cmd_DOXY(ostream & out, const UCS_string_vector & args)
{
UTF8_string root("/tmp");
   if (args.size())   root = UTF8_string(args.front());

   try
     {
       Doxy doxy(out, root);
       doxy.gen();

       if (doxy.get_errors())
          out << "Command ]DOXY failed (" << doxy.get_errors() << " errors)"
              << endl;
      else
         out << "Command ]DOXY finished successfully." << endl
             << "    The generated documentation was stored in directory "
             << doxy.get_root_dir() << endl
             << "    You may now browse it from file://"
             << doxy.get_root_dir()
             << "/index.html" << endl;
     }
   catch (Error &)          {}
   catch (std::bad_alloc &) { WS_FULL; }
   catch (...)              { FIXME; }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_DIAG::cmd_HISTORY(ostream & out, const UCS_string & arg)
{
   if (arg.size())   // )HISTORY  CLEAR (or else line filter)
      {
        if (arg.starts_iwith("CLEAR"))   LineInput::clear_history(out);
        else                             LineInput::print_history(out, arg);
      }
   else              // )HISTORY (with no argument/filter)
      {
        UCS_string no_filter;
        LineInput::print_history(out, no_filter);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_DIAG::cmd_OPTIM(ostream & out, const UCS_string & arg)
{
   if (arg.starts_iwith("CLEAR"))
      {
        out << "Optimization counters cleared" << endl;
        OptmizationStatistics::reset_all();
        return;
      }

int ulen;
#define optim(ena, opt, text) ulen = 40 + UTF8_string::bytes_chars(text);   \
   out << left << setw(ulen) << text << right << " : ";                     \
   if (ena) out << setw(6) << OptmizationStatistics::get(OPTI_ ## opt);     \
   else     out << "disabled";                                              \
   out << endl;
#include "Performance.def"
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_DIAG::cmd_STATUS(ostream & out)
{
   // a UCS_string from a UTF-8 string literal
   auto U = [](const char * utf) { return UCS_string(UTF8_string(utf)); };

   // one row of the table: a feature, its state, and if it is disabled
   // the (first) reason for that: a source like in )LIBS and a detail.
   //
   struct Row
      {
        UCS_string group;    ///< non-empty for a group heading
        UCS_string name;     ///< the feature
        bool enabled;        ///< its state
        UCS_string source;   ///< DEF, CONF, PSYS, PUSER, ARGV, or RUN
        UCS_string detail;   ///< e.g. the preferences line or the option
        bool security = false;   ///< a security-relevant facility
      };

std::vector<Row> rows;
   // a group heading
   auto group = [&](const char * name)
      {
        Row r;
        r.group = U(name);
        r.enabled = true;
        rows.push_back(r);
      };

   // a feature that can only be switched by ./configure
   auto built = [&](const char * name, bool enabled, const char * detail)
      {
        Row r;
        r.name = U(name);
        r.enabled = enabled;
        if (!enabled)
           {
             r.source = U("CONF");
             r.detail = U(detail);
           }
        rows.push_back(r);
      };

   // a feature of the X window system, which also needs a display
   const bool have_display = getenv("DISPLAY") && *getenv("DISPLAY");
   auto x11 = [&](const char * name, bool built_in, const char * detail)
      {
        if (!built_in)   { built(name, false, detail);   return; }
        Row r;
        r.name = U(name);
        r.enabled = have_display;
        if (!have_display)
           {
             r.source = U("RUN");
             r.detail = U("no X display ($DISPLAY is not set)");
           }
        rows.push_back(r);
      };

   // a feature that can be switched at run time (see UserPreferences)
   const std::map<std::string, bool> states =
                    UserPreferences::uprefs.feature_states();
   auto runtime = [&](const char * name, const char * key)
      {
        Row r;
        r.name = U(name);
        const auto it = states.find(key);
        Assert(it != states.end());
        r.enabled = it->second;
        if (!r.enabled)
           {
             if (const UserPreferences::Off_reason * why =
                    UserPreferences::uprefs.get_off_reason(key))
                {
                  r.source = U(why->source.c_str());
                  r.detail = U(why->detail.c_str());
                }
             else
                {
                  r.source = U("DEF");
                  r.detail = U("disabled by default");
                }
           }
        rows.push_back(r);
      };

   // a security-relevant facility: like runtime(), but everything is
   // disabled by ./configure SECURITY_LEVEL_WANTED=2
   auto security = [&](const char * name, const char * key)
      {
        runtime(name, key);
        rows.back().security = true;
#if cfg_SECURITY_LEVEL_WANTED == 2
        Row & r = rows.back();
        if (!r.enabled && r.source == U("DEF"))
           {
             r.source = U("CONF");
             r.detail = U("SECURITY_LEVEL_WANTED=2");
           }
#endif
      };

   group("Shared variables (⎕SVO ⎕SVR ⎕SVC ⎕SVQ ⎕SVE ⎕SVS, APs)");
   runtime("shared variables", "shared variables");
   {
     // the connection to APserver (only attempted if shared variables are
     // wanted)
     Row r;
     r.name = U("APserver");
     r.enabled = Svar_DB::APserver_available();
     if (!r.enabled)
        {
          if (!UserPreferences::uprefs.user_do_svars)
             {
               r.source = U("-");
               r.detail = U("not used (shared variables disabled)");
             }
          else
             {
               r.source = U("RUN");
               r.detail = U(*Svar_DB::get_connect_error()
                            ? Svar_DB::get_connect_error()
                            : "no connection to APserver");
             }
        }
     rows.push_back(r);
   }

   group("Optional system functions");
   built("⎕FFT",             apl_FFT,      "libfftw3 not found or disabled");
   x11  ("⎕GTK",             apl_GTK3 && apl_X11,
                             "GTK3 or X11 not found or disabled");
   x11  ("⎕PLOT (GTK)",      apl_GTK3,     "GTK3 not found or disabled");
   x11  ("⎕PLOT (XCB)",      apl_XCB,      "libxcb not found or disabled");
   built("⎕PNG",             apl_PNG,      "libpng not found or disabled");
   built("⎕RE",              apl_PCRE,     "libpcre2-32 not found or disabled");
   built("⎕SQL (PostgreSQL)", apl_POSTGRES, "libpq not found or disabled");
   built("⎕SQL (SQLite3)",   apl_SQLITE3,  "libsqlite3 not found or disabled");
   built("GSL (⎕MX, ⌹[2]…⌹[6])", apl_GSL, "libgsl not found or disabled");

   group("Execution");
#if PARALLEL_ENABLED
   built("parallel execution", true, "");
#else
   built("parallel execution", false, "CORE_COUNT_WANTED=0 (or no threads)");
#endif
#ifdef cfg_RATIONAL_NUMBERS_WANTED
   built("rational numbers", true, "");
#else
   built("rational numbers", false, "RATIONAL_NUMBERS_WANTED=no");
#endif
#ifdef cfg_DYNAMIC_LOG_WANTED
   built("dynamic logging (]LOG)", true, "");
#else
   built("dynamic logging (]LOG)", false, "DYNAMIC_LOG_WANTED=no");
#endif

#define SEC_STR2(x) #x
#define SEC_STR(x) SEC_STR2(x)
   group("Security (SECURITY_LEVEL_WANTED=" SEC_STR(cfg_SECURITY_LEVEL_WANTED) ")");
#undef SEC_STR
#undef SEC_STR2
   security(")HOST",                       ")HOST");
   security("⎕FIO processes (popen etc.)", "⎕FIO processes");
   security("native functions (⎕FX)",      "native functions");
   security("⎕FIO (all functions)",        "⎕FIO");
   security("⎕FIO open/close",             "⎕FIO open/close");
   security("⎕FIO sockets",                "⎕FIO sockets");
   security("⎕FIO write",                  "⎕FIO write");
   security("⎕GTK",                        "⎕GTK (security)");
   security("⎕PLOT",                       "⎕PLOT (security)");
   security("⎕SQL",                        "⎕SQL (security)");
   security(")SAVE and )DUMP",             ")SAVE and )DUMP");

   group("Output");
   runtime("colours",                      "colours");
   runtime("IBM APL2 formatting",          "IBM APL2 formatting");
   runtime("nested items in parentheses",  "nested items in parentheses");
   runtime("new multi-line strings",       "new multi-line strings");
   runtime("old multi-line strings",       "old multi-line strings");
   runtime("discard indentation (∇)",      "discard indentation");

   group("Session");
   runtime("welcome banner",               "welcome banner");
   runtime(")LOAD CONTINUE at start",      "CONTINUE workspace");
   runtime("echo of input lines",          "echo of input lines");
   runtime("emacs mode",                   "emacs mode");
   runtime("raw input",                    "raw input");
   runtime("automatic )OFF",               "automatic )OFF");
   runtime("backup before )SAVE",          "backup before )SAVE");
   runtime("∇-editor lines in history",    "∇-editor lines in history");
   runtime("xmodmap",                      "xmodmap");

   // The table shall fit into a standard 80-column terminal: the state
   // column shows ✓ for an enabled feature, or else the source of the
   // (first) reason why it is disabled, and the detail column gets the
   // remaining width (longer details are truncated).
   //
   enum { MAX_WIDTH = 79 };
size_t w_name = 7;   // "Feature"
size_t w_st   = 5;   // "State", and the longest source (PUSER)
   for (const Row & r : rows)
       {
         if (r.group.size())   continue;
         if (w_name < r.name.size())     w_name = r.name.size();
         if (w_st   < r.source.size())   w_st   = r.source.size();
       }

   // the frame and separators take 2 + 3 + 3 + 2 = 10 columns
size_t w_det = 6;    // "Detail"
   for (const Row & r : rows)
       {
         if (r.group.size() == 0 && w_det < r.detail.size())
            w_det = r.detail.size();
       }
   if (w_name + w_st + w_det + 10 > MAX_WIDTH)
      w_det = MAX_WIDTH - 10 - w_name - w_st;
const size_t inner = w_name + w_st + w_det + 8;   // between ║ and ║

   auto pad = [](UCS_string ucs, size_t width)
      {
        if (ucs.size() > width)   // truncate
           {
             ucs.resize(width - 1);
             ucs << Unicode(0x2026);   // …
           }
        while (ucs.size() < width)   ucs << UNI_SPACE;
        return ucs;
      };
   auto center = [](const UCS_string & ucs, size_t width)
      {
        UCS_string ret;
        while (ret.size() < (width - ucs.size()) / 2)   ret << UNI_SPACE;
        ret << ucs;
        while (ret.size() < width)   ret << UNI_SPACE;
        return ret;
      };
   auto line = [&](const char * L, const char * M, const char * R,
                   const char * H = "─")
      {
        const Unicode hori = U(H)[0];
        UCS_string ucs = U(L);
        ucs << UCS_string(w_name + 2, hori) << U(M)
            << UCS_string(w_st + 2, hori) << U(M)
            << UCS_string(w_det + 2, hori) << U(R);
        out << ucs << endl;
      };
   auto row = [&](const UCS_string & name, const UCS_string & state,
                  const UCS_string & detail)
      {
        UCS_string ucs = U("║ ");
        ucs << pad(name, w_name) << U(" │ ") << center(state, w_st)
            << U(" │ ") << pad(detail, w_det) << U(" ║");
        out << ucs << endl;
      };

   line("╔", "╤", "╗", "═");
   row(U("Feature"), U("State"), U("Detail"));

bool first_group = true;
   for (const Row & r : rows)
       {
         if (r.group.size())
            {
              if (first_group)   line("╠", "╧", "╣", "═");   // below header
              else               line("╟", "┴", "╢");
              first_group = false;
              UCS_string ucs = U("║ ");
              ucs << center(r.group, inner - 2) << U(" ║");
              out << ucs << endl;
              line("╟", "┬", "╢");
              continue;
            }
         row(r.name, r.enabled ? U("✓") : r.source,
             r.enabled ? U(r.security ? "ALLOWED" : "OK") : r.detail);
       }
   line("╚", "╧", "╝", "═");

   out <<
"State: ✓ if enabled, otherwise the source of the (first) reason why not:\n"
"   DEF:   disabled by default (no preference or option enables it)\n"
"   CONF:  how GNU APL was built (./configure)\n"
"   PSYS:  the system preferences file (" << apl_DIR__sysconf <<
                                         "/gnu-apl.d/preferences)\n"
"   PUSER: the user preferences file ($HOME/.gnu-apl or $HOME/.config/gnu-apl)\n"
"   ARGV:  a command line option\n"
"   RUN:   a condition at run time" << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
Cmd_DIAG::cmd_PSTAT(ostream & out, const UCS_string & arg)
{
#ifndef cfg_PERFORMANCE_COUNTERS_WANTED
   out << "\n"
<< "Command ]PSTAT is not available, since performance counters were not\n"
"configured for this APL interpreter. To enable performance counters (which\n"
"will slightly decrease performance), recompile the interpreter as follows:"

<< "\n\n"
"   ./configure PERFORMANCE_COUNTERS_WANTED=yes (... "
<< "other configure options"
<< ")\n"
"   make\n"
"   make install (or try: src/apl to test without installing)\n"
"\n"

<< "above the src directory."
<< "\n";

   return;
#endif

   if (arg.starts_iwith("CLEAR"))
      {
        out << "Performance counters cleared" << endl;
        Performance::reset_all();
        return;
      }

   if (arg.starts_iwith("SAVE"))
      {
        const char * filename = "./PerformanceData.def";
        ofstream outf(filename, ofstream::out);
        if (!outf.is_open())
           {
             out << "opening " << filename
                 << " failed: " << strerror(errno) << endl;
             return;
           }

        out << "Writing performance data to file " << filename << endl;
        Performance::save_data(out, outf);
        return;
      }

Pfstat_ID iarg = PFS_ALL;
   if (arg.size() > 0)   iarg = Pfstat_ID(arg.atoi());

   Performance::print(iarg, out);
}
//════════════════════════════════════════════════════════════════════════════
