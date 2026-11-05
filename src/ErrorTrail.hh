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

#ifndef __ERROR_TRAIL_HH_DEFINED__
#define __ERROR_TRAIL_HH_DEFINED__

#include <string>
#include <vector>

#include "Common.hh"
#include "Token.hh"
#include "UCS_string.hh"

class Error;
class Executable;
class StateIndicator;
class Token_string;

//════════════════════════════════════════════════════════════════════════════
/** The error trail (command ]EXPLAIN): how the 3-line APL error message was
    constructed, step by step, from the statement that the user entered (or
    that a defined function, ⍎, ⎕EA/⎕EB, ... executed).

    The normal error machinery works BACKWARDS, from the token range in the
    (optimized) body of an Executable that the prefix parser was reducing
    when the error occurred, to the statement displayed in line 2 and the
    carets in line 3 (see devel_doc/Carets.txt). It does so only when an
    error occurs, so that the no-error case runs fast.

    The error trail records these backward steps (B1...B6) and, in
    addition, replays the FORWARD steps (F0...F4: source text, tokenizer,
    every parser pass, body) for the failed statement, keeping track of the
    source characters from which every token was derived. It then checks
    the backward steps against the forward ones (V1...V6), which shows
    where a backward step went wrong (misplaced carets etc.).

    Nothing of this happens unless ]EXPLAIN ON or ]EXPLAIN CHECK was given.
    The forward replay is done when the error occurs (parsing is
    deterministic), so that normal parsing is not slowed down.
 **/
class ErrorTrail
{
public:
   /// the modes of ]EXPLAIN
   enum Mode
      {
        ET_OFF   = 0,   ///< no error trails (default)
        ET_ON    = 1,   ///< build trails, show them with every error
        ET_CHECK = 2,   ///< build trails, report failed checks only
      };

   /// ]EXPLAIN [ON|OFF|CHECK|LAST]
   static void cmd_EXPLAIN(ostream & out, const UCS_string & arg);

   /// the character range in the source from which a token was derived
   struct Span
      {
        /// constructor: unknown span
        Span()
        : from(-1),
          to(-1)
        {}

        /// constructor
        Span(int f, int t)
        : from(f),
          to(t)
        {}

        /// return true if the span is known
        bool known() const   { return from >= 0; }

        /// extend this span by \b other
        void add(const Span & other);

        /// first character
        int from;

        /// last character + 1
        int to;
      };

   /// a token as recorded in the trail
   struct Item
      {
        /// the token as displayed in line 2 (canonical form)
        UCS_string text;

        /// the tag of the token
        TokenTag tag;

        /// the source characters of the token
        Span span;

        /// the value of a value token (to detect a changed value with the
        /// same text, e.g. ⊂ applied to a folded value), or 0
        const void * value;
      };

   /// a list of recorded tokens
   typedef std::vector<Item> Items;

   //──── hooks in the error machinery (no-ops unless a trail is built) ───

   /// start a trail for \b error (in update_error_info())
   static void begin(const Error & error, const StateIndicator * si);

   /// B1: the SI entry from which line 2 is built
   static void note_display(const StateIndicator * si,
                            const StateIndicator * display_si,
                            bool locked, const UCS_string & line_prefix);

   /// B2: the error range in the body of \b exec
   static void note_range(const Executable * exec, Function_PC2 range_in,
                          const char * reason, Function_PC2 range);

   /// B3/B4: the failed statement and the mapping to its unoptimized form
   static void note_mapping(const Executable * exec,
                            Function_PC start, Function_PC end,
                            const Token_string & stat_after,
                            const Token_string & stat_before,
                            const std::vector<int> & ranges_before,
                            Function_PC2 after_from_to,
                            Function_PC2 before_from_to, bool optimized);

   /// B5/B6: line 2 and the carets
   static void note_line2(const Token_string & tokens, Function_PC2 range,
                          int base_left, int len_left, int len_between,
                          int wanted_left, int wanted_right, int nchars,
                          const UCS_string & line_2);

   /// B5: Prefix::adjust_right_caret() has narrowed the range of a
   /// SYNTAX ERROR from \b from to \b to
   static void note_narrowed(Function_PC2 from, Function_PC2 to);

   /// a remark on the current backward step
   static void note(const UCS_string & remark);

   /// path E: a lexical error, thrown with explicit source positions
   static void note_lexical(const Error & error, const UCS_string & line,
                            int start, int end);

   /// the error was not built (e.g. E_COMMAND_PUSHED)
   static void cancel();

   /// complete the trail (replay, checks), show or store it
   static void finish(const Error & error, bool printed);

   //──── hooks in the tokenizer and the parser (only while replaying) ────

   /// true while a trail is completed: errors raised by its replay are
   /// internal and must neither be displayed nor stored
   static bool is_finishing()   { return finishing; }

   /// the recorder of the current replay, or 0
   static ErrorTrail * recorder;

   /// suspends the recording of a replay (while an error that occurs
   /// during the replay is handled, which parses again in reparse())
   struct Suspend
      {
        /// constructor: suspend
        Suspend()
        : saved(recorder)
        { recorder = 0; }

        /// destructor: resume
        ~Suspend()
        { recorder = saved; }

        /// the suspended recorder
        ErrorTrail * saved;
      };

   /// the tokenizer has appended tos[first]... for the characters
   /// from...to-1 of its input
   void tokens_added(const Token_string & tos, size_t first,
                     int from, int to);

   /// a parser pass is about to modify \b tos
   void pass_before(const Token_string & tos);

   /// parser pass \b pass has modified \b tos (TOK_VOIDs not yet removed)
   void pass_after(const char * pass, const Token_string & tos);

protected:
   /// a step of the forward replay
   struct Stage
      {
        /// the name of the step
        UCS_string name;

        /// true if the step has changed the tokens
        bool changed;

        /// what the step did
        std::vector<UCS_string> lines;
      };

   /// the result of a checked step
   struct Check
      {
        /// the name of the check (V1...)
        const char * name;

        /// true if the check passed
        bool ok;

        /// true for an information only (never a failure)
        bool info;

        /// a description
        UCS_string text;
      };

   /// constructor
   ErrorTrail();

   /// replay the parsing of the failed statement (with or without
   /// optimizations) into \b items and \b stages
   bool replay(bool optimize, Items & items, std::vector<Stage> & stages);

   /// the current pass has inserted tokens into \b after: follow the spans
   /// into rec_items and describe the pass in \b st. Return false if the
   /// pass did more than inserting tokens.
   bool insertions(Stage & st, const Items & after);

   /// run the checks
   void run_checks();

   /// add a check result
   void check(const char * name, bool ok, const UCS_string & text);

   /// add an information to the checks
   void info(const char * name, const UCS_string & text);

   /// return the trail as text
   UCS_string render() const;

   /// return the recorded form of \b tok
   static Item make_item(const Token & tok, Span span);

   /// return the recorded form of \b tos (in the order of tos)
   static Items make_items(const Token_string & tos);

   /// return the texts of \b items, separated by blanks
   static UCS_string items_text(const Items & items, bool with_spans);

   /// the trail being built (by the error machinery), or 0
   static ErrorTrail * current;

   /// true while finish() runs (its replay may raise errors, which are not
   /// traced)
   static bool finishing;

   /// the mode of ]EXPLAIN
   static Mode mode;

   /// the text of the last trail
   static UCS_string last_trail;

   /// the number of errors with failed checks (]EXPLAIN CHECK)
   static int failed_errors;

   //──── the error ───────────────────────────────────────────────────────

   /// the error code
   ErrorCode error_code;

   /// where the error was thrown
   const char * throw_loc;

   /// the SI entries when the error occurred (F5)
   std::vector<UCS_string> si_lines;

   /// the prefix parser stack when the error occurred (F5)
   std::vector<UCS_string> prefix_lines;

   //──── backward steps ──────────────────────────────────────────────────

   /// B1: remarks on the SI entry from which line 2 is built
   std::vector<UCS_string> display_lines;

   /// the executable whose body the error range refers to (B2)
   const Executable * exec;

   /// B2: the remarks on the error range
   std::vector<UCS_string> range_lines;

   /// B2: the final error range in the body of exec
   Function_PC2 body_range;

   /// B3: the failed statement in the body (start/end PC)
   Function_PC stat_start;

   /// B3: end PC of the failed statement
   Function_PC stat_end;

   /// B3: the tokens of the failed statement in the body (reversed)
   Items stat_after;

   /// B3: the tokens of the reparsed (unoptimized) statement (reversed)
   Items stat_before;

   /// B4: stat_before index → stat_after index
   std::vector<int> ranges_before;

   /// B4: the error range in stat_after
   Function_PC2 after_range;

   /// B4: the error range in stat_before
   Function_PC2 before_range;

   /// B4: true if the statement was optimized (line 2 from stat_before)
   bool optimized;

   /// true if note_mapping() was called
   bool have_mapping;

   /// B5: the tokens printed in line 2 (reversed)
   Items line2_items;

   /// B5: the error range in line2_items
   Function_PC2 line2_range;

   /// B5: true if adjust_right_caret() has narrowed the range
   bool narrowed;

   /// B5: the range before adjust_right_caret()
   Function_PC2 narrowed_from;

   /// B5: the left caret before the statement (e.g. after "FOO[3]  ")
   int base_left;

   /// B5: characters before the left caret
   int len_left;

   /// B5: characters between the carets
   int len_between;

   /// B6: the caret columns before clamping
   int wanted_left;

   /// B6: right caret column before clamping
   int wanted_right;

   /// B6: the length of line 2 (as stored)
   int nchars;

   /// B5: line 2 as built
   UCS_string line_2;

   /// true if note_line2() was called
   bool have_line2;

   /// path E: a lexical error with explicit positions
   bool lexical;

   /// remarks on backward steps (note())
   std::vector<UCS_string> remarks;

   //──── forward replay ──────────────────────────────────────────────────

   /// F0: the source of the failed statement
   UCS_string source;

   /// F0: where the source comes from
   UCS_string source_origin;

   /// F0: the segment of a conditional statement (0 if none)
   int cond_segment;

   /// the steps of the optimized replay
   std::vector<Stage> stages_opt;

   /// the steps of the unoptimized replay (only the result is shown)
   std::vector<Stage> stages_unopt;

   /// the result of the optimized replay
   Items replay_opt;

   /// the result of the unoptimized replay
   Items replay_unopt;

   /// true if the replay succeeded
   bool replayed;

   //──── final lines and checks ──────────────────────────────────────────

   /// the 3 lines as displayed
   UCS_string final_lines[3];

   /// the checks
   std::vector<Check> checks;

   //──── replay state (recorder) ─────────────────────────────────────────

   /// the tokens being processed, with their spans
   Items rec_items;

   /// the tokens before the current pass
   Items rec_before;

   /// the stages of the current replay
   std::vector<Stage> * rec_stages;

   /// true if the spans could not be followed
   bool rec_lost;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __ERROR_TRAIL_HH_DEFINED__
