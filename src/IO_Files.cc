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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "Backtrace.hh"
#include "Common.hh"
#include "Command.hh"
#include "IndexExpr.hh"
#include "InputFile.hh"
#include "IO_Files.hh"
#include "Output.hh"
#include "UTF8_string.hh"
#include "UserPreferences.hh"
#include "Workspace.hh"

#include "Workspace.icc"

int IO_Files::testcase_count = 0;
int IO_Files::testcases_done = 0;
int IO_Files::total_errors = 0;
int IO_Files::apl_errors = 0;
const char * IO_Files::last_apl_error_loc = "";
int IO_Files::last_apl_error_line = -1;
int IO_Files::assert_errors = 0;
int IO_Files::diff_errors = 0;
int IO_Files::parse_errors = 0;
IO_Files::TestMode IO_Files::test_mode = TM_EXIT_AFTER_LAST_FILE;
IO_Files::Skip_mode IO_Files::skip_mode = SKIP_NONE;
bool IO_Files::need_total = false;
ofstream IO_Files::current_testreport;
//════════════════════════════════════════════════════════════════════════════
UTF8_string IO_Files::summary_path("testcases/summary.log");
APL_time_us IO_Files::start_usecs = 0;

static  ios_base::openmode summary_flags = ofstream::trunc;

//────────────────────────────────────────────────────────────────────────────
UTF8_string
IO_Files::get_crash_backtrace_path()
{
   if (!InputFile::is_validating())   return UTF8_string();
   if (!InputFile::current_file())    return UTF8_string();

UTF8_string path(InputFile::current_filename());
   path << ".crash.txt";
   return path;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::report_abnormal_exit()
{
   if (!InputFile::is_validating())   return;   // not running a -T testcase
   if (!InputFile::current_file())    return;   // no file context at all

   // this file is about to be abandoned mid-run by a direct exit() call
   // (crash or FIXME) that never reaches end_of_current_file() -- make
   // sure it still shows up as failed rather than silently missing.
   if (error_count() == 0)   ++diff_errors;

   const char * path = summary_path.c_str();
   ofstream summary(path, summary_flags);
   if (summary_flags == ofstream::trunc)   // first file
      {
        summary << "Errors   Time File\n-------------------------"
                   "---------------------------------------------" << endl;
        summary_flags = ofstream::app;
      }
   summary << endl   // to find it quickly, matching the normal error format
           << setw(3) << error_count() << "  abnormal termination  ("
           << apl_errors << " APL, " << assert_errors << " assert, "
           << diff_errors << " diff, " << parse_errors << " parse) "
           << InputFile::current_filename() << endl;

   total_errors += error_count();

   // persist a raw backtrace to disk (the same signal-safe mechanism
   // used for a live SIGSEGV, see Backtrace::show_signal_safe()) so it
   // survives independent of whatever, if anything, later captures CERR
   // -- e.g. src/Automated_Test_Report.sh, run well after this process
   // has exited. open()/close() are not guaranteed async-signal-safe,
   // but this function already isn't strictly signal-safe either (the
   // summary.log ofstream above), so that is an existing, not a new,
   // risk when this runs from CrashDump::crash_handler().
const UTF8_string crash_path = get_crash_backtrace_path();
   if (crash_path.size())
      {
        const int fd = open(crash_path.c_str(),
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
           {
             Backtrace::show_signal_safe(fd);
             close(fd);
           }
      }

   CERR << endl
        << "Stopping test execution since an abnormal termination has"
           " occurred" << endl
        << "Failed testcase is " << InputFile::current_filename() << endl;
   if (crash_path.size())
      CERR << "Backtrace saved to " << crash_path << endl;
   CERR << endl;
}
//────────────────────────────────────────────────────────────────────────────
int
fixme_exit_code()
{
   IO_Files::report_abnormal_exit();
   return IO_Files::is_stop_after_file_error() ? 3 : 0;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::syntax_error()
{
   if (!InputFile::current_file())         return;
   if (!InputFile::current_file()->file)   return;

   ++parse_errors;

   Log(LOG_test_execution)
      CERR << "parse errors incremented to " << parse_errors << endl;

   current_testreport << "**\n** Parse Error ********\n**" << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::expect_apl_errors(int max_cnt)
{
   if (apl_errors <= max_cnt)
      {
        // <= rather than == : some of the errors ]EXPECT accounts for
        // (e.g. a build-configuration-dependent internal error) may not
        // reproduce on every platform, so cnt is an upper bound, not an
        // exact count.
        //
        Log(LOG_test_execution)
           CERR << "APL errors reset from (expected at most) " << max_cnt
                << ", got " << apl_errors << ") to 0" << endl;
        apl_errors = 0;
        last_apl_error_line = -1;
        last_apl_error_loc = "";
      }
   else
      {
        Log(LOG_test_execution)
           CERR << "*** Not resetting APL errors (got " << apl_errors
                << " expecting at most " << max_cnt << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::apl_error(const char * loc)
{
   if (!InputFile::current_file())         return;
   if (!InputFile::current_file()->file)   return;

   ++apl_errors;
   last_apl_error_loc = loc;
   last_apl_error_line = InputFile::current_line_no();

   Log(LOG_test_execution)
      CERR << "APL errors incremented to " << apl_errors << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::assert_error()
{
   if (!InputFile::current_file())         return;
   if (!InputFile::current_file()->file)   return;

   ++assert_errors;
   Log(LOG_test_execution)
      CERR << "Assert errors incremented to " << assert_errors << endl;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::diff_error()
{
   if (!InputFile::current_file())         return;
   if (!InputFile::current_file()->file)   return;

   ++diff_errors;
   Log(LOG_test_execution)
      CERR << "Diff errors incremented to " << diff_errors << endl;

   if (test_mode & TM_DONE_AFTER_LINE_ERROR)
      {
        ++total_errors;
        ++testcases_done;
        InputFile::close_current_file();
        InputFile::files_todo.clear();
        if (test_mode == TM_EXIT_AFTER_LINE_ERROR)   Command::cmd_OFF(1);
      }
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::get_file_line(UTF8_string & line, bool & eof)
{
   while (InputFile::current_file())   // as long as we have input files
      {
        if (!InputFile::current_file()->file)
           {
             open_next_file();
             if (!InputFile::current_file())         break;   // no more files
             if (!InputFile::current_file()->file)   break;   // no more files
           }

        // At this point current_file and current_file->file are valid.
        // Read a line with CR and LF removed.
        //
        eof = false;
        read_file_line(line, eof);
        if (eof)   // end of file reached: do some global checks
           {
             if (Command::inside_multi())
             {
                  CERR << "\n*** WARNING: Multiline string not closed "
                          "at the end of a file.\n";
                  if (InputFile::current_file())
                     {
                       CERR << "*** File: "
                            << InputFile::current_file()->filename;
                       if (const int start = Command::get_multiline_start() + 1)
                          {
                            CERR << " (the string starts near line "
                                 << start << ")";
                          }
                       CERR << ".\n";
                     }
                }

             if (InputFile::current_file()->with_LX == do_LX)
                {
                  InputFile::current_file()->with_LX = no_LX;
                  InputFile::current_file()->echo = true;
                  UCS_string LX = Workspace::get_LX();
                  if (LX.size())   // ⎕LX pending
                     {
                       Command::process_line(LX, 0);
                       eof = false;
                       return;
                     }
                }
             if (end_of_current_file())   continue;   // try again.
              else                        break;      // done
           }

        // lines of a script executed by the testcase (e.g. )COPY 5 X.apl)
        // are marked differently, so that tools that read the .tc.log
        // (accept_tc.py, Automated_Test_Report.sh) can tell them from the
        // input lines of the testcase itself.
        //
        if (InputFile::current_file()->test)
           current_testreport << "----> " << line.c_str() << endl;
        else
           current_testreport << "-scr> " << line.c_str() << endl;
        return;
      }

   // arrive here when all testfiles have been read.
   // Maybe print a testcase total summary
   //
   if (need_total)   // we had a -T option
      {
        print_summary();
        need_total = false;   // forget the -T option

        if ((test_mode == TM_EXIT_AFTER_LAST_FILE) ||
            (test_mode == TM_EXIT_AFTER_LAST_FILE_IF_OK && !error_count()))
          {
            CERR << "Exiting (test_mode " << test_mode << ") : "
                 << total_errors << " errors" << endl;
            cleanup(true);
            if (total_errors)   Command::cmd_OFF(1);
            else                Command::cmd_OFF(0);
          }
      }

   eof = true;
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::open_next_file()
{
   if (InputFile::current_file() == 0)
      {
        CERR << "IO_Files::open_next_file(): no more files" << endl;
        return;
      }

   if (InputFile::current_file()->file)
      {
        if (InputFile::current_file()->is_pushed_pending())
           {
             InputFile::current_file()->set_pushed_pending(false);
             CERR << "*** Leaving the pushed immediate execution context"
                  << endl;
           }
        else
           {
             CERR << "IO_Files::open_next_file(): already open" << endl;
           }
        return;
      }

     while (InputFile::current_file())
         {
           char log_file_name[FILENAME_MAX] = "";

           if (InputFile::current_file()->test)
              {
                SPRINTF(log_file_name, "%s.log",
                        InputFile::current_filename());
                if (const char * slash = strrchr(log_file_name, '/'))
                   {
                     summary_path = UTF8_string(log_file_name,
                                                slash + 1 - log_file_name);
                     summary_path << "summary.log";
                   }

                CERR << "  #######################################"
                        "#####################################\n"
                     << " ########################################"
                        "######################################\n"
                     << " ##    Testfile: " << left << setw(60)
                     << InputFile::current_filename() << "##\n"
                     << " ##    Log file: " << setw(60) << log_file_name
                     << "##\n"
                     << " ########################################"
                        "######################################" << endl
                     << "  #######################################"
                        "#####################################\n" << endl
                     << reset_format;
              }

           InputFile::open_current_file();
           if (InputFile::current_file()->file == 0)
              {
                CERR << "could not open "
                     << InputFile::current_filename() << endl;
                InputFile::files_todo.erase(InputFile::files_todo.begin());

                if (UserPreferences::uprefs.auto_OFF &&
                    ! InputFile::files_todo.size())   Command::cmd_OFF(0);

                continue;
              }

           Log(LOG_test_execution)
              CERR << "opened testcase file "
                   << InputFile::current_filename() << endl;

           Output::reset_dout();
           reset_errors();

           current_testreport.close();

           if (InputFile::current_file()->test)
              {
                // open a testcase file
                //
                current_testreport.open(log_file_name,
                                        ofstream::out | ofstream::trunc);
                if (!current_testreport.is_open())
                   {
                     CERR << "could not open testcase log file "
                          << log_file_name
                          << "; producing no .log file" << endl;
                   }
              }

           // remember when test execution was started
           start_usecs = now();
           return;
         }
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::next_file()
{
   if (InputFile::current_file() &&
       InputFile::current_file()->file)
      {
        end_of_current_file();
      }
   else
      {
        CERR << "]NEXTFILE: no current file" << endl;
      }
   open_next_file();
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::read_file_line(UTF8_string & file_line, bool & eof)
{
InputFile * input = InputFile::current_file();

   // skip a UTF-8 BOM (EF BB BF) at the very start of a freshly opened
   // file -- some editors/exporters prepend one, and left unstripped it
   // decodes (via UTF8_string/UCS_string's ordinary UTF-8 decoding) to a
   // literal U+FEFF as the first character of the first token, which the
   // tokenizer doesn't recognize. Checking input->bom_checked (a flag
   // reset once per open, see InputFile::open_current_file()) rather
   // than ftell(input->file)==0: ftell() fails (returns -1, never 0) on
   // a non-seekable stream -- e.g. a real pipe via `-f -`, as opposed to
   // a shell `<` redirection of a regular file, which IS seekable and
   // used to mask this -- silently skipping the BOM strip for that case.
   // current_line_no()==1 was tried and rejected too: line_no's
   // reset-to-0-on-open (InputFile::open_current_file()) can land AFTER
   // this function's caller already incremented it once, so it is not a
   // reliable "first byte of the file" signal either. See Bugs27 #59(e).
   //
   if (file_line.size() == 0 && !input->bom_checked)
      {
        input->bom_checked = true;

        // The first read of a file (opened by fopen() or, for )COPY, by
        // fdopen()): it starts without ]FILE_CTL skipping, even if the
        // input source that opened it was skipping (e.g. the console that
        // issued a )COPY): the skip mode belongs to the file that set it
        // (Blake McBride, Bugs32 #10: ]FILE_CTL SKIP_ALL followed by )COPY
        // silently imported nothing). The opener's skip mode is restored
        // by InputFile::close_current_file(), so that a file that ends
        // while skipping never swallows the lines of its opener, and )COPY
        // can be nested to any depth.
        //
        input->saved_skip_mode = skip_mode;
        skip_mode = SKIP_NONE;
        const int b0 = fgetc(input->file);
        if (b0 == 0xEF)
           {
             const int b1 = fgetc(input->file);
             const int b2 = fgetc(input->file);
             if (b1 == 0xBB && b2 == 0xBF)   // BOM: discard all 3 bytes
                ;
             else                            // not a BOM: put back
                {
                  if (b2 != EOF)   ungetc(b2, input->file);
                  if (b1 != EOF)   ungetc(b1, input->file);
                  ungetc(b0, input->file);
                }
           }
        else if (b0 != EOF)   ungetc(b0, input->file);
      }

   for (;;)
       {
         const int cc = fgetc(input->file);
         if (cc == EOF)   // end of file
            {
              if (file_line.size())   break;   // EOF, but we have chars

              // the ]FILE_CTL skip mode is restored when the file is
              // closed, see InputFile::close_current_file()
              eof = true;
              return;
            }

         if (cc == '\n' || cc == 2)   // end of line or ^B
            {
              if (input->current_line_no() == 1 &&
                  file_line.starts_with("<!"))
                 {
                   // first line of the file starts with <! (so we assume that
                   // this file is a HTML tagged file
                   //
                   input->set_html(1);
                 }

              if (input->get_html() > 0)   // HTML file
                 {
                  input->set_html(file_line.un_HTML(input->get_html()));
                  if (file_line.size() == 0)   continue;   // line with tag(s)
                 }

              if (input->copy_filter.is_active())
                 {
                    const bool line_allowed =
                          input->copy_filter.check_filter(file_line);
                    if (!line_allowed)
                       {
                         file_line.clear();
                         continue;
                       }
                 }

              // ]FILE_CTL (see IO_Files::Skip_mode / Command.def):
              // discard every line while skip_mode says to skip for GNU
              // APL, EXCEPT a ]FILE_CTL command itself (to turn skipping
              // back off) or a )CLEAR command (plan.txt: "EOF and )CLEAR
              // shall reset to NO_SKIP" -- )CLEAR must survive even
              // during SKIP_ALL, or it could never run to do that reset,
              // confirmed live: a naive version that only exempted
              // ]FILE_CTL swallowed a subsequent )CLEAR too, leaving
              // SKIP_ALL active for the rest of the file -- including
              // )OFF -- and hanging on stdin at EOF). Both commands are
              // recognized here (not merely left to fall through to the
              // normal command dispatcher) so they survive regardless of
              // the current mode; Cmd_HOST::cmd_FILE_CTL()/Cmd_WS::
              // cmd_CLEAR() still do the actual work once the line
              // reaches them normally afterwards.
              if (skip_mode == SKIP_GNU || skip_mode == SKIP_ALL)
                 {
                   size_t p = 0;
                   while (p < file_line.size() &&
                          (file_line[p] == ' ' || file_line[p] == '\t'))
                         ++p;
                   bool survives = false;
                   if (file_line.size() - p >= 9)
                      {
                        std::string prefix(file_line, p, 9);
                        for (char & c : prefix)   c = toupper((unsigned char)c);
                        survives = (prefix == "]FILE_CTL");
                      }
                   if (!survives && file_line.size() - p >= 6)
                      {
                        std::string prefix(file_line, p, 6);
                        for (char & c : prefix)   c = toupper((unsigned char)c);
                        survives = (prefix == ")CLEAR");
                      }
                   if (!survives)
                      {
                        file_line.clear();
                        continue;
                      }
                 }
              break;
            }

         if (cc == '\r')   continue;   // ignore carriage returns

          file_line += cc;
       }

   Log(LOG_test_execution)
      CERR << "read_file_line() -> " << file_line << endl;
}
//════════════════════════════════════════════════════════════════════════════
bool
IO_Files::end_of_current_file()
{
   // the end of a file that a testcase has opened (e.g. a .apl file of
   // )COPY or )PCOPY) is not the end of the testcase: its error counts
   // must survive (they were reset below, so that every diff before a
   // )COPY of a .apl file was silently lost)
   //
const bool testcase_ends = InputFile::is_validating();

   if (testcase_ends)   // running a .tc file
      {
        // we expect )SI to be clear after a testcase has finished and
        // complain if it is not.
        //
        if (Workspace::SI_entry_count() > 1)
           {
             CERR << endl << ")SI not cleared at the end of "
                  << InputFile::current_filename() << ":" << endl;
             Workspace::list_SI(CERR, SIM_SIS);
             CERR << endl;

             if (current_testreport.is_open())
                {
                  current_testreport << endl
                                     << ")SI not cleared at the end of "
                                     << InputFile::current_filename()<< ":"
                                     << endl;
                  Workspace::list_SI(current_testreport, SIM_SIS);
                  current_testreport << endl;
                }
             apl_error(LOC);

             // do not let one testcase file's dirty )SI leak into the
             // next one and be reported (and charged) there too, and
             // in every file after it -- mirrors the stale-value
             // check below, which already recovers the same way
             // (Blake McBride, Bugs24 #3).
             Workspace::clear_SI(CERR);
           }
        else if (Workspace::SI_entry_count() == 1)
           {
             // a testcase may end with an error in immediate execution
             // (e.g. NaturalLogarithm.tc: ⍟0J0), which is not complained
             // about. But its SI entry must not leak into the next
             // testcase either: it keeps the functions that the next
             // testcase )ERASEs alive, and a )CHECK there reports them
             // as stale functions (OperPower.tc).
             //
             Workspace::clear_SI(CERR);
           }

        // check for stale values and indices
        //
        if (current_testreport.is_open())
           {
             if (Value::print_incomplete(current_testreport))
                {
                  current_testreport
                     << " (automatic check for incomplete values failed)"
                     << endl;
                  apl_error(LOC);
                }

             if (Value::print_stale(current_testreport))
                {
                  current_testreport
                     << " (automatic check for stale values failed,"
                        " offending Value erased)." << endl;

                  apl_error(LOC);
                  Value::erase_stale(LOC);
                }

             if (IndexExpr::print_stale(current_testreport))
                {
                  current_testreport
                     << " (automatic check for stale indices failed)."
                     << endl;
                  apl_error(LOC);
                }
           }
      }

   InputFile::close_current_file();
   ++testcases_done;

   Log(LOG_test_execution)
      CERR << "closed testcase file " << InputFile::current_filename()
           << endl;

   if (testcase_ends)   // running a .tc file
      {
        const char * path = summary_path.c_str();
        ofstream summary(path, summary_flags);
        if (summary_flags == ofstream::trunc)   // first file
           {
             summary << "Errors   Time File\n-------------------------"
                        "---------------------------------------------" << endl;
             summary_flags = ofstream::app;
           }
        if (error_count())   summary << endl;   // to find it quickly
        summary << setw(3) << error_count();

        // cannot use SRINTF's ' flag uses locales (which suck).
        const long long duration      = now() - start_usecs;
        const long long duration_usec = duration % 1000;
        const long long duration_msec = duration / 1000 % 1000;
        const long long duration_secs = duration  / 1000000 % 1000;

        // print seconds if nonzero, else blanks
        //
        summary << "  ";
        if (duration >= 1000000)     // ≥ 1 second
           {
             // sss.mmm
             // 
             summary << setw(3) << duration_secs << "."
                     << setfill('0') << setw(3) << duration_msec << " s  ";
           }
        else if (duration >= 1000)   // ≥ 1 milli second
           {
             // mmm.uuu
             //
             summary << setw(3) << duration_msec << "."
                     << setfill('0') << setw(3) << duration_usec
                     << setfill(' ') << " ms ";
           }
        else
           {
             // uuu
             //
             summary << "    " << setw(3) << duration_usec << " μs ";
           }
        summary << setfill(' ') << " ";

        if (error_count())
           {
             total_errors += error_count();
             summary << "(" << apl_errors    << " APL, ";
             if (apl_errors)
                summary << "    loc=" << last_apl_error_loc 
                        << " .tc line="  << last_apl_error_line << endl
                   << "    ";

             summary << assert_errors << " assert, "
                     << diff_errors   << " diff, "
                     << parse_errors  << " parse) ";
           }

        summary << InputFile::current_filename() << endl;

        if ((test_mode == TM_STOP_AFTER_FILE_ERROR ||
             test_mode == TM_EXIT_AFTER_FILE_ERROR) && error_count())
           {
             CERR << endl
                  << "Stopping test execution since an error has occurred"
                  << endl
                  << "The error count is " << error_count() << endl
                  << "Failed testcase is " << InputFile::current_filename()
                  << endl 
                  << endl;

             InputFile::files_todo.clear();
             return false;
           }
      }

   InputFile::files_todo.erase(InputFile::files_todo.begin());

   if (UserPreferences::uprefs.auto_OFF &&
       ! InputFile::files_todo.size())   Command::cmd_OFF(0);

   Output::reset_dout();
   if (testcase_ends)   reset_errors();
   return true;   // continue processing
}
//────────────────────────────────────────────────────────────────────────────
void
IO_Files::print_summary()
{
const char * path = charP(summary_path.c_str());
ofstream summary(path, ios_base::app);

int done = testcases_done;
   if (done > testcase_count)   done = testcase_count;

   summary << "======================================="
              "=======================================" << endl
           << total_errors << " errors in " << done
           << "(" << testcase_count << ")"
           << " testcase files" << endl;

   CERR    << endl
           << "======================================="
           "=======================================" << endl
           << total_errors << " errors in " << done
           << "(" << testcase_count << ")"
           << " testcase files" << endl;
}
//════════════════════════════════════════════════════════════════════════════
