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

#include <sstream>

#include "Avec.hh"
#include "Error.hh"
#include "ErrorTrail.hh"
#include "Executable.hh"
#include "Parser.hh"
#include "Prefix.hh"
#include "StateIndicator.hh"
#include "Token_string.hh"
#include "UserFunction.hh"
#include "Workspace.hh"

using namespace std;

ErrorTrail::Mode ErrorTrail::mode = ErrorTrail::ET_OFF;
ErrorTrail * ErrorTrail::current = 0;
ErrorTrail * ErrorTrail::recorder = 0;
bool ErrorTrail::finishing = false;
UCS_string ErrorTrail::last_trail;
int ErrorTrail::failed_errors = 0;

/// the inner width of the boxes
enum { BOX_WIDTH = 77 };

//────────────────────────────────────────────────────────────────────────────
/// return \b n as UCS_string
static UCS_string
num(long n)
{
UCS_string ret;
   ret << n;
   return ret;
}
//────────────────────────────────────────────────────────────────────────────
/// return the name of token tag \b tag, e.g. TOK_APL_VALUE1
static UCS_string
tag_name(TokenTag tag)
{
   switch(tag)
      {
#define TD(tag, _tc, _tv, _id) case tag: return UCS_string(UTF8_string(#tag));
#include "Token.def"
        default: break;
      }
   return UCS_string(UTF8_string("?"));
}
//────────────────────────────────────────────────────────────────────────────
/// return \b str as UCS_string
static UCS_string
ucs(const char * str)
{
   return UCS_string(UTF8_string(str));
}
//────────────────────────────────────────────────────────────────────────────
/// ═
static const Unicode BOX_DOUBLE = Unicode(0x2550);
/// ─
static const Unicode BOX_SINGLE = Unicode(0x2500);
//════════════════════════════════════════════════════════════════════════════
/// a box with sections, drawn with box characters
class TrailBox
{
public:
   /// constructor
   TrailBox(const UCS_string & title)
   {
     // ╔═══ title ═══╗
     UCS_string top(ucs("╔"));
     const int fill = BOX_WIDTH - title.size() - 2;
     loop(f, fill/2)   top << BOX_DOUBLE;
     top << UNI_SPACE << title << UNI_SPACE;
     loop(f, fill - fill/2)   top << BOX_DOUBLE;
     top << ucs("╗");
     lines.push_back(top);
   }

   /// start a new section
   void section(const UCS_string & name, bool heavy = false)
      {
        UCS_string sep(heavy ? ucs("╠═ ") : ucs("╟─ "));
        sep << name << UNI_SPACE;
        while (sep.size() < BOX_WIDTH + 1)
           sep << (heavy ? BOX_DOUBLE : BOX_SINGLE);
        sep << (heavy ? ucs("╣") : ucs("╢"));
        lines.push_back(sep);
      }

   /// add a line (wrapped if needed), indented by \b indent
   void line(const UCS_string & text, int indent = 1)
      {
        UCS_string rest(text);
        bool first = true;
        while (rest.size() || first)
           {
             // continuation lines are indented by 2 more
             const int width = BOX_WIDTH - indent - (first ? 0 : 2) - 1;
             int len = rest.size();
             if (len > width)   // wrap at a blank, if possible
                {
                  len = width;
                  for (int b = width; b > width/2; --b)
                      if (rest[b] == UNI_SPACE)   { len = b;   break; }
                }
             UCS_string ln(ucs("║"));
             loop(i, indent + (first ? 0 : 2))   ln << UNI_SPACE;
             loop(i, len)   ln << rest[i];
             while (ln.size() < BOX_WIDTH + 1)   ln << UNI_SPACE;
             ln << ucs("║");
             lines.push_back(ln);
             rest = UCS_string(rest, len, rest.size() - len);
             rest.remove_leading_whitespaces();
             first = false;
           }
      }

   /// return the box as text
   UCS_string text() const
      {
        UCS_string ret;
        loop(l, lines.size())   ret << lines[l] << UNI_LF;
        ret << ucs("╚");
        loop(b, BOX_WIDTH)   ret << BOX_DOUBLE;
        ret << ucs("╝") << UNI_LF;
        return ret;
      }

protected:
   /// the lines of the box
   std::vector<UCS_string> lines;
};
//════════════════════════════════════════════════════════════════════════════
void
ErrorTrail::Span::add(const Span & other)
{
   if (!other.known())   return;
   if (!known())   { *this = other;   return; }
   if (from > other.from)   from = other.from;
   if (to   < other.to)     to   = other.to;
}
//════════════════════════════════════════════════════════════════════════════
ErrorTrail::ErrorTrail()
   : error_code(E_NO_ERROR),
     throw_loc(0),
     exec(0),
     body_range(Function_PC_invalid, Function_PC_invalid),
     stat_start(Function_PC_invalid),
     stat_end(Function_PC_invalid),
     after_range(Function_PC_invalid, Function_PC_invalid),
     before_range(Function_PC_invalid, Function_PC_invalid),
     optimized(false),
     have_mapping(false),
     line2_range(Function_PC_invalid, Function_PC_invalid),
     narrowed(false),
     narrowed_from(Function_PC_invalid, Function_PC_invalid),
     base_left(0),
     len_left(0),
     len_between(0),
     wanted_left(0),
     wanted_right(0),
     nchars(0),
     have_line2(false),
     lexical(false),
     cond_segment(0),
     replayed(false),
     rec_stages(0),
     rec_lost(false)
{
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::cmd_EXPLAIN(ostream & out, const UCS_string & arg)
{
#ifndef cfg_EXPLAIN_WANTED
   // the forward hooks (tokenizer, parser) are not compiled in, so no
   // trail could be built
   //
   out << "    ]EXPLAIN is not available: it was disabled by ./configure "
          "(enable it with ./configure EXPLAIN_WANTED=yes)." << endl;
   return;
#endif // cfg_EXPLAIN_WANTED

UCS_string a(arg);
   a.remove_leading_and_trailing_whitespaces();
const UTF8_string arg_utf(a);
const char * cmd = arg_utf.c_str();

   if (!strcasecmp(cmd, "ON"))
      {
        mode = ET_ON;
        out << "    ]EXPLAIN is ON: every error is explained." << endl;
        return;
      }

   if (!strcasecmp(cmd, "OFF"))
      {
        mode = ET_OFF;
        out << "    ]EXPLAIN is OFF." << endl;
        return;
      }

   if (!strcasecmp(cmd, "CHECK"))
      {
        mode = ET_CHECK;
        failed_errors = 0;
        out << "    ]EXPLAIN is in CHECK mode: errors whose construction "
               "fails a check are reported." << endl;
        return;
      }

   if (a.size() && strcasecmp(cmd, "LAST"))
      {
        out << "BAD COMMAND+" << endl;
        MORE_ERROR() << "]EXPLAIN: expecting ON, OFF, CHECK, or LAST";
        return;
      }

   // ]EXPLAIN or ]EXPLAIN LAST: show the trail of the last error
   //
   if (last_trail.size())
      {
        out << last_trail;
        return;
      }

   out << "    No error was explained yet";
   if (mode == ET_OFF)   out << " (]EXPLAIN is OFF, see ]EXPLAIN ON)";
   out << "." << endl;
   if (mode == ET_CHECK)
      out << "    Errors with failed checks so far: " << failed_errors << endl;
}
//────────────────────────────────────────────────────────────────────────────
ErrorTrail::Item
ErrorTrail::make_item(const Token & tok, Span span)
{
Item item;
   item.tag = tok.get_tag();
   item.span = span;
   item.value = 0;
   if (item.tag == TOK_VOID)   return item;

   if (tok.get_Class() == TC_VALUE && tok.get_ValueType() == TV_VAL)
      item.value = tok.get_apl_val().get();

   // Token::canonical() ends in a FIXME (i.e. exits) for some tokens (e.g.
   // SI_PUSHED on the prefix parser stack): use it only for the tokens
   // that it handles, and the tag name otherwise.
   //
bool can_print = false;
   switch(tok.get_Class())
      {
        case TC_VALUE:   // PrintBuffer can raise errors for other values
             {
               if (tok.get_ValueType() == TV_CHAR)   // a raw character
                  {
                    item.text << UNI_SINGLE_QUOTE << tok.get_char_val()
                              << UNI_SINGLE_QUOTE;
                    break;
                  }
               if (tok.get_ValueType() != TV_VAL)   break;   // tag name
               const cValue * val = tok.get_apl_val().get();
               if (val && val->get_rank() <= 1 && val->is_simple() &&
                   val->element_count() > 0 && val->element_count() <= 20)
                  can_print = true;
               else if (val)
                  {
                    item.text << ucs("value ⍴ ");
                    if (val->get_rank() == 0)   item.text << ucs("(scalar)");
                    loop(r, val->get_rank())
                       {
                         if (r)   item.text << UNI_SPACE;
                         item.text << num(val->get_shape_item(r));
                       }
                  }
             }
             break;

        case TC_ASSIGN:  case TC_R_ARROW:  case TC_COLON:   case TC_R_BRACK:
        case TC_SYMBOL:  case TC_L_PARENT: case TC_R_PARENT:
        case TC_L_CURLY: case TC_R_CURLY:  case TC_FUN0:    case TC_FUN12:
        case TC_OPER1:   case TC_OPER2:
             can_print = true;
             break;

        case TC_L_BRACK:
             can_print = item.tag == TOK_L_BRACK || item.tag == TOK_SEMICOL;
             break;

        case TC_INDEX:
             can_print = item.tag == TOK_AXIS || item.tag == TOK_FAXIS ||
                         item.tag == TOK_MARKER;
             break;

        case TC_LIT_ITEM:
             can_print = tok.get_ValueType() == TV_INT ||
                         tok.get_ValueType() == TV_FLT ||
                         tok.get_ValueType() == TV_CPX;
             break;

        default: break;
      }

   if (can_print)
      {
        try { item.text = tok.canonical(PR_APL_FUN).remove_pad(); }
        catch (...) {}
      }
   if (item.text.size() == 0)   item.text = tag_name(item.tag);
   return item;
}
//────────────────────────────────────────────────────────────────────────────
ErrorTrail::Items
ErrorTrail::make_items(const Token_string & tos)
{
Items items;
   loop(t, tos.size())   items.push_back(make_item(tos[t], Span()));
   return items;
}
//────────────────────────────────────────────────────────────────────────────
UCS_string
ErrorTrail::items_text(const Items & items, bool with_spans)
{
UCS_string ret;
   loop(i, items.size())
      {
        if (i)   ret << UNI_SPACE;
        ret << UNI_LEFT_DAQ << items[i].text << UNI_RIGHT_DAQ;
        if (with_spans && items[i].span.known())
           {
             ret << UNI_AT_SIGN << num(items[i].span.from);
             if (items[i].span.to == items[i].span.from)   // inserted
                ret << ucs("(inserted)");
             else if (items[i].span.to > items[i].span.from + 1)
                ret << UNI_MINUS << num(items[i].span.to - 1);
           }
      }
   return ret;
}
//════════════════════════════════════════════════════════════════════════════
void
ErrorTrail::begin(const Error & error, const StateIndicator * si)
{
#ifndef cfg_EXPLAIN_WANTED
   return;   // ]EXPLAIN disabled by ./configure: never build a trail
#endif // cfg_EXPLAIN_WANTED

   if (mode == ET_OFF)   return;
   if (finishing)        return;   // an error during a replay
   if (current)          delete current;   // an unfinished trail

   current = new ErrorTrail();
   current->error_code = error.get_error_code();
   current->throw_loc  = error.get_throw_loc();

   // F5: the SI when the error occurred
   //
   for (const StateIndicator * s = si; s; s = s->get_parent())
       {
         UCS_string ln(ucs("SI level "));
         ln << num(s->get_level()) << ucs(": ");
         switch(s->get_parse_mode())
            {
              case PM_FUNCTION:       ln << ucs("defined function ");   break;
              case PM_STATEMENT_LIST: ln << ucs("immediate execution "); break;
              case PM_EXECUTE:        ln << ucs("⍎ ");                  break;
              default:                ln << ucs("? ");
            }
         if (const UserFunction * uf = s->get_executable()->get_exec_ufun())
            {
              if (uf->is_macro())    ln << ucs("(macro) ");
              if (uf->is_lambda())   ln << ucs("(lambda) ");
            }
         ln << s->function_name() << UNI_L_BRACK << num(s->get_line())
            << UNI_R_BRACK << ucs("  PC ") << num(s->get_PC());
         current->si_lines.push_back(ln);
       }

   if (si)
      {
        const Prefix & prefix = si->get_prefix();
        UCS_string ln(ucs("prefix parser stack (APL order): "));
        if (prefix.ssize() == 0)   ln << ucs("(empty)");
        loop(t, prefix.ssize())
           {
             const Token_loc & tl = prefix.at(t);
             const Item item = make_item(tl.get_token(), Span());
             ln << UNI_LEFT_DAQ << item.text << UNI_RIGHT_DAQ
                << ucs("@PC") << num(tl.get_PC()) << UNI_SPACE;
           }
        current->prefix_lines.push_back(ln);
        UCS_string rng(ucs("range of the phrase being reduced: PC "));
        rng << num(prefix.get_range_low()) << ucs(" to ")
            << num(prefix.get_range_high());
        current->prefix_lines.push_back(rng);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_display(const StateIndicator * si,
                         const StateIndicator * display_si,
                         bool locked, const UCS_string & line_prefix)
{
   if (!current)   return;

   if (display_si == si)
      current->display_lines.push_back(
             ucs("line 2 is built from the SI entry of the error"));
   else if (display_si)
      {
        UCS_string ln(ucs("macro frame(s) skipped: line 2 is built from "
                          "SI level "));
        ln << num(display_si->get_level()) << ucs(" (")
           << display_si->function_name() << ucs(")");
        current->display_lines.push_back(ln);
      }
   else
      current->display_lines.push_back(ucs("no SI entry for line 2"));

   if (locked)
      current->display_lines.push_back(ucs("locked or non-suspendable "
                     "function: the error is shown at its calling point"));

   UCS_string ln(ucs("line 2 starts with: "));
   ln << UNI_LEFT_DAQ << line_prefix << UNI_RIGHT_DAQ;
   current->display_lines.push_back(ln);
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_range(const Executable * exec, Function_PC2 range_in,
                       const char * reason, Function_PC2 range)
{
   if (!current)   return;

   current->exec = exec;
   current->body_range = range;

UCS_string ln(ucs("prefix range PC "));
   ln << num(range_in.low) << ucs("…") << num(range_in.high)
      << ucs(" → error range PC ") << num(range.low) << ucs("…")
      << num(range.high) << ucs(" (") << ucs(reason) << ucs(")");
   current->range_lines.push_back(ln);
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_mapping(const Executable * exec,
                         Function_PC start, Function_PC end,
                         const Token_string & stat_after,
                         const Token_string & stat_before,
                         const std::vector<int> & ranges_before,
                         Function_PC2 after_from_to,
                         Function_PC2 before_from_to, bool optimized)
{
   if (!current)   return;

   current->exec           = exec;
   current->stat_start     = start;
   current->stat_end       = end;
   current->stat_after     = make_items(stat_after);
   current->stat_before    = make_items(stat_before);
   current->ranges_before  = ranges_before;
   current->after_range    = after_from_to;
   current->before_range   = before_from_to;
   current->optimized      = optimized;
   current->have_mapping   = true;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_line2(const Token_string & tokens, Function_PC2 range,
                       int base_left, int len_left, int len_between,
                       int wanted_left, int wanted_right, int nchars,
                       const UCS_string & line_2)
{
   if (!current)   return;

   // the texts exactly as Token::error_info() has just printed them (safe
   // for these tokens, unlike for arbitrary ones, see make_item())
   //
   current->line2_items  = make_items(tokens);
   loop(t, tokens.size())
      {
        if (tokens[t].get_Class() == TC_VOID)   continue;
        current->line2_items[t].text =
                 tokens[t].canonical(PR_APL_FUN).remove_pad();
      }
   current->line2_range  = range;
   current->base_left    = base_left;
   current->len_left     = len_left;
   current->len_between  = len_between;
   current->wanted_left  = wanted_left;
   current->wanted_right = wanted_right;
   current->nchars       = nchars;
   current->line_2       = line_2;
   current->have_line2   = true;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_narrowed(Function_PC2 from, Function_PC2 to)
{
   if (!current)   return;

   current->narrowed      = true;
   current->narrowed_from = from;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note(const UCS_string & remark)
{
   if (current)   current->remarks.push_back(remark);
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::note_lexical(const Error & error, const UCS_string & line,
                         int start, int end)
{
#ifndef cfg_EXPLAIN_WANTED
   return;   // ]EXPLAIN disabled by ./configure: never build a trail
#endif // cfg_EXPLAIN_WANTED

   if (mode == ET_OFF || finishing)   return;

   if (current)   delete current;
   current = new ErrorTrail();
   current->error_code = error.get_error_code();
   current->throw_loc  = error.get_throw_loc();
   current->lexical    = true;
   current->source     = line;
   current->source_origin = ucs("the input line (path E: lexical error, "
                                "found by the tokenizer)");

UCS_string ln(ucs("the tokenizer failed at characters "));
   ln << num(start) << ucs("…") << num(end - 1)
      << ucs(": line 2 is that part of the input, and the carets mark its "
             "first and last character (no backward mapping)");
   current->remarks.push_back(ln);
   finish(error, true);
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::cancel()
{
   delete current;
   current = 0;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::finish(const Error & error, bool printed)
{
   if (!current)   return;

ErrorTrail & trail = *current;
   current = 0;
   finishing = true;   // errors during the replay below are not traced

   // F0..F4: the forward replay of the failed statement
   //
   if (trail.exec && trail.have_mapping && !trail.lexical)
      {
        int line = 0;
        int statement = 0;
        trail.source = trail.exec->statement_text(trail.stat_start, line,
                                                  statement);
        UCS_string origin;
        switch(trail.exec->get_parse_mode())
           {
             case PM_FUNCTION:
                  origin << ucs("defined function ");
                  if (const UserFunction * uf = trail.exec->get_exec_ufun())
                     origin << uf->get_name();
                  origin << UNI_L_BRACK << num(line) << UNI_R_BRACK;
                  break;

             case PM_STATEMENT_LIST:
                  origin << ucs("immediate execution");
                  break;

             case PM_EXECUTE:
                  origin << ucs("the argument of ⍎ (or of ⎕EA/⎕EB)");
                  break;

             default: origin << ucs("?");
           }
        origin << ucs(", statement ") << num(statement + 1);
        trail.cond_segment = trail.exec->get_cond_segment(trail.stat_start);
        if (trail.cond_segment)
           origin << ucs(", segment ") << num(trail.cond_segment)
                  << ucs(" of a conditional statement (segments are "
                         "separated by →→, ←→, and ←←)");
        trail.source_origin = origin;

        // an error raised by a replay must not change )MORE
        const UCS_string more = Workspace::more_error();
        const bool ok1 = trail.replay(true,  trail.replay_opt,
                                      trail.stages_opt);
        const bool ok2 = trail.replay(false, trail.replay_unopt,
                                      trail.stages_unopt);
        Workspace::more_error() = more;
        trail.replayed = ok1 && ok2;
      }

   trail.final_lines[0] = error.get_error_line_1();
   trail.final_lines[1] = error.get_error_line_2();
   trail.final_lines[2] = error.get_error_line_3();

   trail.run_checks();
   last_trail = trail.render();

int failed = 0;
   loop(c, trail.checks.size())   if (!trail.checks[c].ok)   ++failed;

   if (mode == ET_ON && printed)   CERR << last_trail;
   else if (mode == ET_CHECK && failed)
      {
        ++failed_errors;
        CERR << "]EXPLAIN CHECK: " << trail.final_lines[0] << " in «"
             << trail.final_lines[1] << "»:";
        loop(c, trail.checks.size())
           if (!trail.checks[c].ok)   CERR << " " << trail.checks[c].name;
        CERR << " failed (see ]EXPLAIN)" << endl;
      }

   delete &trail;
   finishing = false;
}
//════════════════════════════════════════════════════════════════════════════
bool
ErrorTrail::replay(bool optimize, Items & items, std::vector<Stage> & stages)
{
   items.clear();
   stages.clear();
   rec_items.clear();
   rec_stages = &stages;
   rec_lost = false;

   recorder = this;
const Parser parser(exec->get_parse_mode(), LOC, false);
Token_string tos;
ErrorCode ec = E_NO_ERROR;
   try   { ec = parser.parse(source, tos, optimize); }
   catch (...)   { ec = E_SYNTAX_ERROR; }
   recorder = 0;

   // a statement with a single token has no parser pass that would
   // record the tokenizer result
   if (stages.size() == 0)
      {
        Stage st;
        st.name = ucs("F2 Tokenizer");
        st.changed = true;
        st.lines.push_back(items_text(rec_items, true));
        stages.push_back(st);
      }

   if (ec != E_NO_ERROR)   return false;

   if (rec_lost || rec_items.size() != tos.size())
      {
        items = make_items(tos);   // without spans
        return false;
      }

   items = rec_items;
   return true;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::tokens_added(const Token_string & tos, size_t first,
                         int from, int to)
{
   for (size_t t = first; t < tos.size(); ++t)
       rec_items.push_back(make_item(tos[t], Span(from, to)));
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::pass_before(const Token_string & tos)
{
   if (rec_stages && rec_stages->size() == 0)   // first pass: tokenizer done
      {
        Stage st;
        st.name = ucs("F2 Tokenizer");
        st.changed = true;
        st.lines.push_back(items_text(rec_items, true));
        rec_stages->push_back(st);
      }

   rec_before = make_items(tos);
   if (rec_items.size() == tos.size())
      {
        loop(t, tos.size())   rec_before[t].span = rec_items[t].span;
      }
   else
      {
        rec_lost = true;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::pass_after(const char * pass, const Token_string & tos)
{
   if (!rec_stages)   return;

Stage st;
   st.name = ucs("F3 ");
   st.name << ucs(pass);
   st.changed = false;

const Items after = make_items(tos);
   if (!rec_lost && after.size() > rec_before.size() &&
       insertions(st, after))
      {
        rec_stages->push_back(st);
        return;
      }

   if (rec_lost || after.size() != rec_before.size())
      {
        rec_lost = true;
        st.changed = true;
        st.lines.push_back(ucs("(the source positions could not be "
                               "followed through this step)"));
        Items next;
        loop(t, after.size())
            if (after[t].tag != TOK_VOID)   next.push_back(after[t]);
        rec_items = next;
        rec_stages->push_back(st);
        return;
      }

const int n = after.size();
std::vector<bool> voided(n, false);    // voided by this pass
std::vector<bool> changed(n, false);   // modified by this pass
   loop(t, n)
      {
        const bool was_void = rec_before[t].tag == TOK_VOID;
        const bool is_void  = after[t].tag == TOK_VOID;
        if (is_void)   { voided[t] = !was_void;   continue; }
        changed[t] = rec_before[t].tag  != after[t].tag ||
                     rec_before[t].value != after[t].value ||
                     !(rec_before[t].text == after[t].text);
      }

   // a changed token absorbs the tokens voided next to it (e.g. A ⍴ B →
   // VALUE4, or [ N ] → AXIS); voided tokens next to unchanged tokens only
   // (e.g. non-grouping parentheses) are simply removed.
   //
std::vector<bool> absorbed(n, false);
Items next;
   loop(t, n)
      {
        if (after[t].tag == TOK_VOID)   continue;
        Item item = after[t];
        item.span = rec_before[t].span;
        if (changed[t])
           {
             int from = t;
             int to = t;
             while (from > 0 && voided[from - 1] && !absorbed[from - 1])
                   --from;
             while (to < n - 1 && voided[to + 1] && !absorbed[to + 1])
                   ++to;
             Items old;
             for (int j = from; j <= to; ++j)
                 {
                   if (j != t && !voided[j])   continue;
                   absorbed[j] = true;
                   item.span.add(rec_before[j].span);
                   old.push_back(rec_before[j]);
                 }

             UCS_string ln(items_text(old, false));
             ln << ucs(" → ") << items_text(Items(1, item), true);
             if (rec_before[t].tag != after[t].tag &&
                 rec_before[t].text == after[t].text)
                {
                  ln << ucs("  (") << tag_name(rec_before[t].tag)
                     << ucs(" → ") << tag_name(after[t].tag) << ucs(")");
                }
             st.lines.push_back(ln);
             st.changed = true;
           }
        next.push_back(item);
      }

Items removed;
   loop(t, n)   if (voided[t] && !absorbed[t])   removed.push_back(rec_before[t]);
   if (removed.size())
      {
        UCS_string ln(ucs("removed: "));
        ln << items_text(removed, true);
        st.lines.push_back(ln);
        st.changed = true;
      }

   if (!st.changed)
      {
        const UTF8_string name_utf(st.name);
        if (strstr(name_utf.c_str(), "(skipped") ||
            strstr(name_utf.c_str(), "(disabled"))
           st.lines.push_back(ucs("not performed"));
        else
           st.lines.push_back(ucs("performed, no change"));
      }

   rec_items = next;
   rec_stages->push_back(st);
}
//────────────────────────────────────────────────────────────────────────────
bool
ErrorTrail::insertions(Stage & st, const Items & after)
{
   // a pass that inserts tokens (e.g. the parentheses that
   // fix_RANK_syntax() and fix_POWER_syntax() put around A ⍤ y and
   // A ⍣ N). Align the tokens after the pass with those before it: an
   // unchanged token keeps its span, an inserted one has none.
   //
Items next;
Items inserted;
size_t b = 0;
   loop(a, after.size())
      {
        if (b < rec_before.size() && after[a].text == rec_before[b].text)
           {
             Item item = after[a];
             item.span = rec_before[b++].span;
             next.push_back(item);
           }
        else
           {
             next.push_back(after[a]);
             inserted.push_back(after[a]);
           }
      }

   if (b != rec_before.size() ||
       inserted.size() != after.size() - rec_before.size())   return false;

   // an inserted token has no source characters: give it an empty span
   // where it was inserted, i.e. before the next token (e.g. a () or
   // after the previous one (e.g. a ))
   //
   loop(n, next.size())
      {
        if (next[n].span.known())   continue;
        if (next[n].tag == TOK_R_PARENT)
           {
             for (int p = n - 1; p >= 0; --p)
                 if (next[p].span.known())
                    {
                      next[n].span = Span(next[p].span.to, next[p].span.to);
                      break;
                    }
           }
        else
           {
             for (size_t q = n + 1; q < next.size(); ++q)
                 if (next[q].span.known())
                    {
                      next[n].span = Span(next[q].span.from,
                                          next[q].span.from);
                      break;
                    }
           }
      }

UCS_string ln(ucs("inserted: "));
   ln << items_text(inserted, false) << ucs("  →  ")
      << items_text(next, true);
   st.lines.push_back(ln);
   st.changed = true;

   // TOK_VOIDs (if any) are removed after the pass
   //
Items items;
   loop(n, next.size())
      if (next[n].tag != TOK_VOID)   items.push_back(next[n]);
   rec_items = items;
   return true;
}
//════════════════════════════════════════════════════════════════════════════
void
ErrorTrail::check(const char * name, bool ok, const UCS_string & text)
{
Check c;
   c.name = name;
   c.ok   = ok;
   c.info = false;
   c.text = text;
   checks.push_back(c);
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::info(const char * name, const UCS_string & text)
{
Check c;
   c.name = name;
   c.ok   = true;
   c.info = true;
   c.text = text;
   checks.push_back(c);
}
//────────────────────────────────────────────────────────────────────────────
/// return true if \b tag ends a statement in a body
static bool
is_statement_end(TokenTag tag)
{
   return tag == TOK_END || tag == TOK_ENDL ||
          TokenClass(int(tag) & int(TC_MASK)) == TC_RETURN;
}
//────────────────────────────────────────────────────────────────────────────
void
ErrorTrail::run_checks()
{
   if (lexical)
      {
        check("V0", true, ucs("lexical error: positions taken directly "
                              "from the tokenizer"));
        return;
      }

   if (!have_mapping)
      {
        check("V0", true, ucs("no statement mapping (the error is not "
                              "located in a statement, e.g. a locked "
                              "function or an empty SI)"));
        return;
      }

   // the replays, without a leading label (as reparse() does)
   //
Items opt = replay_opt;
Items unopt = replay_unopt;
   if (opt.size() > 1   && opt[1].tag == TOK_COLON)
      opt.erase(opt.begin(), opt.begin() + 2);
   if (unopt.size() > 1 && unopt[1].tag == TOK_COLON)
      unopt.erase(unopt.begin(), unopt.begin() + 2);

   // a conditional statement C →→ T ←→ F ←← is one statement in the text
   // but several (C, T, F) in the body: keep the segment of the error
   //
   {
     bool cond = false;
     loop(o, opt.size())
        if (opt[o].tag == TOK_IF_THEN || opt[o].tag == TOK_IF_ELSE ||
            opt[o].tag == TOK_IF_END)   cond = true;
     if (cond)
        {
          Items * replays[] = { &opt, &unopt };
          loop(r, 2)
             {
               Items & items = *replays[r];
               Items seg_items;
               int seg = 0;
               loop(i, items.size())
                  {
                    const TokenTag tag = items[i].tag;
                    if (tag == TOK_IF_THEN || tag == TOK_IF_ELSE ||
                        tag == TOK_IF_END)   { ++seg;   continue; }
                    if (seg == cond_segment)   seg_items.push_back(items[i]);
                  }
               items = seg_items;
             }
          UCS_string t(ucs("conditional statement: the body has segment "));
          t << num(cond_segment) << ucs(" of it as a statement of its own: ")
            << items_text(opt, true);
          info("V0", t);
        }
   }

   check("V0", replayed, replayed
         ? ucs("the forward replay of the statement succeeded")
         : ucs("the forward replay of the statement failed; the checks "
               "below that need it are skipped"));

   // V1: the error range lies in the failed statement
   //
   {
     const bool ok = stat_start <= body_range.low   &&
                     body_range.low <= body_range.high &&
                     body_range.high <= stat_end;
     UCS_string t(ucs("error range PC "));
     t << num(body_range.low) << ucs("…") << num(body_range.high)
       << (ok ? ucs(" lies in") : ucs(" does NOT lie in"))
       << ucs(" the statement PC ") << num(stat_start) << ucs("…")
       << num(stat_end);
     check("V1", ok, t);
   }

   // F4: lambdas. Executable::setup_lambdas() (after the parser) replaces
   // every {...} by a name λN; merge them into one item, as in the body
   //
   {
     Items merged;
     for (size_t o = 0; o < opt.size(); ++o)
         {
           if (opt[o].tag != TOK_L_CURLY)   { merged.push_back(opt[o]);   continue; }
           Item lambda = opt[o];
           int depth = 0;
           for (; o < opt.size(); ++o)
               {
                 if (opt[o].tag == TOK_L_CURLY)   ++depth;
                 if (opt[o].tag == TOK_R_CURLY)   --depth;
                 if (o > 0 && opt[o].tag != TOK_L_CURLY)
                    {
                      if (lambda.text.size() && lambda.text.back() != UNI_L_CURLY)
                         lambda.text << UNI_SPACE;
                      lambda.text << opt[o].text;
                    }
                 lambda.span.add(opt[o].span);
                 if (depth == 0)   break;
               }
           merged.push_back(lambda);
         }
     opt = merged;
   }

   // V2: the body (stat_after, reversed) = the optimized replay
   //
std::vector<int> after2opt(stat_after.size(), -1);
bool body_ok = false;
   if (replayed)
      {
        std::vector<int> idx;   // stat_after indices without END tokens
        loop(a, stat_after.size())
            if (!is_statement_end(stat_after[a].tag))   idx.push_back(a);
        body_ok = idx.size() == opt.size();
        loop(i, idx.size())
            {
              if (!body_ok)   break;
              const int o = opt.size() - 1 - i;
              const bool lambda = opt[o].tag == TOK_L_CURLY &&
                                  stat_after[idx[i]].text.size() &&
                                  stat_after[idx[i]].text[0] == UNI_LAMBDA;
              // a multi-line string or literal is a marker @N@ in the
              // text (and in the replay); UserFunction::parse_body()
              // replaces it by its value (which the replay cannot do)
              const bool marker = opt[o].tag == TOK_MARKER &&
                                  stat_after[idx[i]].tag == TOK_APL_VALUE1;
              if (!lambda && !marker &&
                  !(stat_after[idx[i]].text == opt[o].text))
                 body_ok = false;
              else after2opt[idx[i]] = o;
            }
        UCS_string t;
        if (body_ok)   t << ucs("the body of the statement is the optimized "
                                "replay (reversed)");
        else
           {
             t << ucs("the body ") << items_text(stat_after, false)
               << ucs(" differs from the optimized replay ")
               << items_text(opt, false);
           }
        check("V2", body_ok, t);
      }

   // V3: the reparse (stat_before, reversed) = the unoptimized replay
   //
bool before_ok = false;
   if (replayed)
      {
        before_ok = stat_before.size() == unopt.size();
        loop(b, stat_before.size())
           {
             if (!before_ok)   break;
             if (!(stat_before[b].text == unopt[unopt.size() - 1 - b].text))
                before_ok = false;
           }
        UCS_string t;
        if (before_ok)
           t << ucs("reparse() gives the unoptimized replay (reversed)");
        else
           t << ucs("reparse() ") << items_text(stat_before, false)
             << ucs(" differs from the unoptimized replay ")
             << items_text(unopt, false);
        check("V3", before_ok, t);
      }

   // V4: the carets mark the source characters of the error range
   //
   if (replayed && body_ok && before_ok)
      {
        // the source span of the left- and rightmost token of the error
        // range in the body (forward), and of the tokens that the
        // backward mapping chose for the carets
        //
        Span fwd_left, fwd_right, bwd_left, bwd_right;
        if (after_range.high >= 0 &&
            after_range.high < Function_PC(after2opt.size()) &&
            after2opt[after_range.high] >= 0)
           fwd_left = opt[after2opt[after_range.high]].span;
        if (after_range.low >= 0 &&
            after_range.low < Function_PC(after2opt.size()) &&
            after2opt[after_range.low] >= 0)
           fwd_right = opt[after2opt[after_range.low]].span;

        // a range at the end token of the statement (e.g. the →→ after a
        // condition) is left of its first token
        //
        if (opt.size() && after_range.high == Function_PC(stat_after.size()))
           fwd_left = opt[0].span;
        if (opt.size() && after_range.low == Function_PC(stat_after.size()))
           fwd_right = opt[0].span;

        if (optimized)   // carets from stat_before
           {
             const int nb = stat_before.size();
             if (before_range.high >= 0 && before_range.high < nb)
                bwd_left = unopt[nb - 1 - before_range.high].span;
             if (before_range.low >= 0 && before_range.low < nb)
                bwd_right = unopt[nb - 1 - before_range.low].span;
             if (nb && before_range.high == nb)   bwd_left  = unopt[0].span;
             if (nb && before_range.low  == nb)   bwd_right = unopt[0].span;
           }
        else             // carets from stat_after (the body)
           {
             bwd_left = fwd_left;
             bwd_right = fwd_right;
           }

        const bool ok = fwd_left.known() && fwd_right.known() &&
                        bwd_left.from == fwd_left.from &&
                        bwd_right.from == fwd_right.from;
        UCS_string t(ucs("error range: left end at source column "));
        t << num(bwd_left.from) << ucs(" (forward: ") << num(fwd_left.from)
          << ucs("), right end at source column ") << num(bwd_right.from)
          << ucs(" (forward: ") << num(fwd_right.from) << ucs(")");
        if (!optimized)
           t << ucs(" -- not optimized: line 2 shows the body itself");
        check("V4", ok, t);
      }

   // V5: the caret columns are the columns of the tokens in line 2
   //
   if (have_line2)
      {
        // rebuild line 2 like Executable::set_error_info() (and
        // Token::error_info()) do, recording the column of every token
        //
        UCS_string built(line_2, 0, base_left);
        std::vector<int> column(line2_items.size(), -1);
        for (int q = line2_items.size() - 1; q >= 0; --q)
            {
              if (line2_items[q].tag == TOK_JOT && q > 0 &&
                  line2_items[q - 1].tag == TOK_OPER2_OUTER)   continue;

              const UCS_string & txt = line2_items[q].text;
              const Unicode c1 = built.size() ? built.back()
                                               : Invalid_Unicode;
              const Unicode c2 = txt.size() ? txt[0] : Invalid_Unicode;
              if (!(Avec::no_space_after(c1) || Avec::no_space_before(c2)))
                 built << UNI_SPACE;
              column[q] = built.size();
              built << txt;
            }

        // the ∘ of ∘.f is not printed (∘. is): it starts where ∘. starts
        //
        for (int q = 1; q < int(line2_items.size()); ++q)
            if (column[q] == -1 && line2_items[q].tag == TOK_JOT &&
                line2_items[q - 1].tag == TOK_OPER2_OUTER)
               column[q] = column[q - 1];

        const int nl = line2_items.size();
        const int hi = line2_range.high;
        const int lo = line2_range.low;
        // an index nl is the end token of the statement (e.g. the
        // →→ after a condition), which is left of its first token
        //
        const int exp_left  = (hi >= 0 && hi < nl) ? column[hi]
                            : hi == nl ? base_left : -1;
        const int exp_right = (lo >= 0 && lo < nl) ? column[lo]
                            : lo == nl ? base_left : -1;
        const bool ok = exp_left == wanted_left && exp_right == wanted_right;
        UCS_string t(ucs("caret columns "));
        t << num(wanted_left) << ucs(" and ") << num(wanted_right)
          << ucs(", the tokens «")
          << ((hi >= 0 && hi < nl) ? line2_items[hi].text
             : hi == nl ? ucs("(end of statement)") : ucs("?"))
          << ucs("» and «")
          << ((lo >= 0 && lo < nl) ? line2_items[lo].text
             : lo == nl ? ucs("(end of statement)") : ucs("?"))
          << ucs("» start at columns ") << num(exp_left) << ucs(" and ")
          << num(exp_right);
        check("V5", ok, t);

        const bool ok6 = wanted_left <= nchars && wanted_right <= nchars;
        UCS_string t6;
        if (ok6)   t6 << ucs("line 2 was not truncated");
        else       t6 << ucs("line 2 was truncated to ") << num(nchars)
                      << ucs(" characters, the carets were clamped");
        check("V6", ok6, t6);

        // line 2 vs. the source: the normalizations (blanks, constants
        // displayed differently, λN for a lambda, ...)
        //
        {
          UCS_string shown;
          if (line_2.size() > size_t(base_left))
             shown = UCS_string(line_2, base_left, line_2.size() - base_left);
          UCS_string src(source);
          src.remove_leading_and_trailing_whitespaces();
          if (shown == src)
             info("V7", ucs("line 2 shows the statement as it was entered"));
          else
             {
               UCS_string t(ucs("line 2 differs from the source: «"));
               t << shown << ucs("» was entered as «") << src << ucs("»");
               info("V7", t);
             }
        }
      }

   // V8: line 2 and the carets are built for the range of B4
   //
   if (have_line2 && have_mapping)
      {
        const Function_PC2 b4 = optimized ? before_range : after_range;
        const Function_PC2 used = narrowed ? narrowed_from : line2_range;
        const bool ok = used.low == b4.low && used.high == b4.high;
        UCS_string t(ucs("the carets are built for the range "));
        t << num(used.low) << ucs("…") << num(used.high)
          << (ok ? ucs(", the range of B4") : ucs(", but B4 has "));
        if (!ok)   t << num(b4.low) << ucs("…") << num(b4.high);
        check("V8", ok, t);
        if (narrowed)
           {
             UCS_string n(ucs("SYNTAX ERROR: the carets show the narrowed "
                              "range "));
             n << num(line2_range.low) << ucs("…") << num(line2_range.high)
               << ucs(" (B5), not the error range");
             info("V8", n);
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
UCS_string
ErrorTrail::render() const
{
UCS_string title(ucs("]EXPLAIN "));
   title << final_lines[0];
TrailBox box(title);

   //──── forward ─────────────────────────────────────────────────────────
   box.section(ucs("forward"), true);

   box.section(ucs("F0 Source"));
   box.line(source_origin);
   box.line(source.size() ? source : ucs("(unknown)"), 3);

   if (!lexical && source.size())
      {
        box.section(ucs("F1 Multi-line strings"));
        bool marker = false;
        loop(r, replay_unopt.size())
            if (replay_unopt[r].tag == TOK_MARKER)   marker = true;
        if (marker)
           box.line(ucs("the statement was joined from several lines of the "
                        "function: a multi-line string or literal became the "
                        "marker @N@, which the body has replaced by its value "
                        "(the replay cannot)"));
        else if (source.multi_pos() == -1)
           box.line(ucs("performed, no change (no multi-line string in the "
                        "statement)"));
        else
           box.line(ucs("the statement contains a multi-line string: "
                        "positions refer to the joined text"));
      }

   loop(s, stages_opt.size())
      {
        const Stage & st = stages_opt[s];
        box.section(st.name);
        loop(l, st.lines.size())   box.line(st.lines[l]);
      }

   if (replay_unopt.size())
      {
        box.section(ucs("F3 Result without optimizations (as in reparse())"));
        box.line(items_text(replay_unopt, true));
      }

   if (stat_after.size())
      {
        box.section(ucs("F4 Body of the statement (reversed)"));
        UCS_string ln;
        loop(a, stat_after.size())
           {
             if (a)   ln << UNI_SPACE;
             ln << ucs("PC") << num(long(stat_start) + long(a)) << UNI_SPACE
                << UNI_LEFT_DAQ << stat_after[a].text << UNI_RIGHT_DAQ;
           }
        box.line(ln);
      }

   if (si_lines.size() || prefix_lines.size())
      {
        box.section(ucs("F5 Execution: the error"));
        UCS_string ln(ucs("error: "));
        ln << final_lines[0];
        if (throw_loc)   ln << ucs("  thrown at ") << ucs(throw_loc);
        box.line(ln);
        loop(l, si_lines.size())       box.line(si_lines[l]);
        loop(l, prefix_lines.size())   box.line(prefix_lines[l]);
      }

   //──── backward ────────────────────────────────────────────────────────
   box.section(ucs("backward"), true);

   if (display_lines.size())
      {
        box.section(ucs("B1 SI entry for line 2"));
        loop(l, display_lines.size())   box.line(display_lines[l]);
      }

   if (range_lines.size())
      {
        box.section(ucs("B2 Error range"));
        loop(l, range_lines.size())   box.line(range_lines[l]);
      }

   if (have_mapping)
      {
        box.section(ucs("B3 Failed statement"));
        UCS_string ln(ucs("statement PC "));
        ln << num(stat_start) << ucs("…") << num(stat_end)
           << ucs(", error range in it: ") << num(after_range.low)
           << ucs("…") << num(after_range.high);
        box.line(ln);
        UCS_string rb(ucs("reparse() (unoptimized, reversed): "));
        rb << items_text(stat_before, false);
        box.line(rb);

        box.section(ucs("B4 Optimized → unoptimized"));
        UCS_string mp(ucs("unoptimized token → body token: "));
        loop(r, ranges_before.size())
           {
             if (r)   mp << UNI_SPACE;
             mp << num(r) << ucs("→") << num(ranges_before[r]);
           }
        box.line(mp);
        UCS_string br(ucs("error range "));
        br << num(after_range.low) << ucs("…") << num(after_range.high)
           << ucs(" → ") << num(before_range.low) << ucs("…")
           << num(before_range.high);
        box.line(br);
        box.line(optimized
                 ? ucs("the statement was optimized: line 2 is built from "
                       "the unoptimized tokens")
                 : ucs("the statement was not optimized: line 2 is built "
                       "from the body"));
      }

   if (have_line2)
      {
        box.section(ucs("B5 Line 2 and carets"));
        UCS_string ln(ucs("tokens (reversed): "));
        ln << items_text(line2_items, false);
        box.line(ln);
        UCS_string c(ucs("left caret = "));
        c << num(base_left) << ucs(" + ") << num(len_left)
          << ucs(" (before token ") << num(line2_range.high)
          << ucs("), right caret = left caret + ") << num(len_between)
          << ucs(" (to token ") << num(line2_range.low) << ucs(")");
        if (narrowed)
           {
             UCS_string n(ucs("SYNTAX ERROR: Prefix::adjust_right_caret() "
                              "has narrowed the range "));
             n << num(narrowed_from.low) << ucs("…")
               << num(narrowed_from.high) << ucs(" to ")
               << num(line2_range.low) << ucs("…") << num(line2_range.high)
               << ucs(" (a guess from the tokens; it also sets )MORE)");
             box.line(n);
           }
        box.line(c);
      }

   if (remarks.size())
      {
        box.section(ucs("Remarks"));
        loop(r, remarks.size())   box.line(remarks[r]);
      }

   //──── result ──────────────────────────────────────────────────────────
   box.section(ucs("B6 The 3 lines"), true);
   loop(l, 3)   box.line(final_lines[l], 0);

   if (checks.size())
      {
        box.section(ucs("checks"), true);
        loop(c, checks.size())
           {
             UCS_string ln(ucs(checks[c].info ? "ℹ "
                               : checks[c].ok ? "✓ " : "✗ "));
             ln << ucs(checks[c].name) << ucs(": ") << checks[c].text;
             box.line(ln);
           }
      }

   return box.text();
}
//════════════════════════════════════════════════════════════════════════════
