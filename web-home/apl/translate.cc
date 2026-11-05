/* summary of tags:
{
#...            comment (no autput)
«BO ...«        bold
«IT ...»        italic
«H1 ...»        header level 1
...
"H6 ...»        header level 6

   // can be used in table ?       ─────────────────────────────────┐
   // can be usen in .tc output ?  ────────────────────────────┐    │
   // can be usen in HTML output ? ───────────────────────┐    │    │
   // continuation ?               ──────────────────┐    │    │    │
                                                     │    │    │    │
«APL ...» APL input and APL output (executed)        │    │    │    │
                                                     │    │    │    │
«APL"     one APL statement and its response         │    │    │    │
«APL1"    first APL statement and its response       │    │    │    │
«APL2"    subsequent APL statement and its response  │    │    │    │
«APL3"    last APL statement and its response        │    │    │    │
«APL1"    first APL statement and its response       │    │    │    │
«QUAD" ,                                             │    │    │    │
                                                     │    │    │    │
 IN: (normal APL input at the                        │    │    │    │
      beginning of an example                        │    │    │    │
«IN ...»                                            no   yes  yes  no
«IN1 ...»                                           yes  yes  yes  no
«IN2 ...»                                           yes  yes  no   no
«IN3 ...»                                           inl  yes  no   no
«IN7 ...»                                           no   yes  no   no
«IN8 ...»                                           no   no   yes  no
                                                     │    │    │    │
 OU: normal APL output at the                        │    │    │    │
    end of an example                                │    │    │    │
«OU ...»                                            no   yes  yes  no
«OU1 ...»                                           yes  yes  yes  no
«OU2 ...»                                           yes  yes  no   no
«OU7 ...»                                           no   yes  no   no
«OU8« ...»                                          no   no   yes  no

 HTML tags:
«»              <BR> tag
«OL ...»        <OL> ... </OL> tags (ordered list)
«UL ...»        <UL> ... </UL> tags (unordered list)
«LI ...»        <LI> ... </LI> tags (list item)
«- .»           top-level
«EMPTY»         output a &NBSP
«SU ...»        superscript
«su ...»        subscript
«TAB ...»       <TABLE> ... </TABLE> tags
«TR ...»        <TR>    ... </TR> tags
«TAB ...»       table , "table1"   , 0   , 0  },
«TD ...»        table data with CSS class "tab"      (1 column table)
«TDC ...»       table data with CSS class "tabC"     (centered)
«TD1 ...»       table data with CSS class "tab1"     (table column 1)
«TD2 ...»       table data with CSS class "tab12"    (table column 2)
«TD3 ...»       table data with CSS class "tab3"     (table column 3)
«TD4 ...»       table data with CSS class "tab4"     (table column 4)
«TD5 ...»       table data with CSS class "tab5"     (table column 5)
«TD12 ...»      table data with CSS class "tab1"     (table columns 1 and 2)
«TD45 ...»      table data with CSS class "tab4"     (table columns 4 and 5)
«TH ...»        table header with CSS class "tab"    (1 column table)
«TH1 ...»       table header with CSS class "tab1"   (table column 1)
«TH2 ...»       table header with CSS class "tab2"   (table column 2)
«TH3 ...»       table header with CSS class "tab3"   (table column 3)
«TH4 ...»       table header with CSS class "tab4"   (table column 4)
«TH5 ...»       table header with CSS class "tab5"   (table column 5)
«TH12 ...»      table header with CSS class "tab12"  (table columns 1 and 2)
«TH45 ...»      table header with CSS class "tab45"  (table columns 4 and 5)
«TO ...»        token                                <label class=\"token\">
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

#include <apl/libapl.h>

#undef STR
#define STR(x) #x
#undef LOC
#define LOC Loc(__FILE__, __LINE__)
#undef Loc
#define Loc(f, l) STR(line=l)

/// the 6-blank APL input prompt
#define PROMPT "      "

int error_count = 0;

//                 H0  H1  H2  H3  H4  H5
size_t Hx_num[] = { 0,  0,  0,  0,  0,  0 };
enum { Hx_max = sizeof(Hx_num) / sizeof(int) };

/// Integer Info. The 1-digit numeric suffix of a tag
enum IINFO { II0 = 0, II1 = 1, II2 = 2, II3 = 3,
             II4 = 4, II5 = 5, II6 = 6, II7 = 7,
             II8 = 8
            };

/// chapter number depth, e.g. 3, 3.1, 3.1.1, ...
size_t Hx_len = 0;

int in_table = 0;
int verbose = 0;

FILE * fout = 0;   /// HTML body text
FILE * fTOC = 0;   // / HTMLtable of content
FILE * ftc  = 0;    // testcase file

string stdOut_chars;
string stdErr_chars;
vector<string> quad_responses;

class StdOut : public streambuf
{
   virtual int overflow(int c)
      { stdOut_chars += (char)c;   return 0; }

} stdOut;

class StdErr : public streambuf
{
   virtual int overflow(int c)
      { stdErr_chars += (char)c;   return 0; }

} stdErr;

extern ostream COUT;
extern ostream UERR;

//-----------------------------------------------------------------------------
class Out
{
public:
   Out()
   : need_indent(false),
     in_PRE(false),
     indent_val(2)
   {}

   /// print \b cc, possibly indented. If a LF is detected in the output
   /// stream then the next character. Unless we are in a <PRE> section.
   void Putc(char cc)
      {
        if (need_indent && !in_PRE)
           {
             for (size_t i = 0; i < indent_val; ++i)   putc(' ', fout);
           }

        putc(cc & 0xFF, fout);
        need_indent = (cc == '\n');
      }

   void print(const char * str)
      { while (*str)   Putc(*str++);
      }

   void print(const uint8_t * bytes)
      { print(reinterpret_cast<const char *>(bytes));
      }

   /// print " attr=value"
   void print_attr(const char * attr, const char * value)
      {
        Putc(' ');      // attribute separator
        print(attr);
        print("=\"");   // leading quote
        print(value);
        print("\"");    // trailing quote
      }

   /// print "<PRE class=cls loc>suffix"
   Out & open_PRE(const char * cls, const char * loc, const char * suffix)
      {
        print("<PRE");             // "<PRE"
        print_class(cls);          // "class=cls"
        Putc(' ');   print(loc);   // " line=NNN"
        print(">");                // ">"
        print(suffix);
        in_PRE = true;
        return *this;
      }

   /// print the closing tag </PRE>
   void close_PRE(bool LF)
      { print("</PRE>");
        if (LF)   Putc('\n');
        in_PRE = false;
      }

   /// print "class=cls"
   void print_class(const char * cls)
      {
        print_attr("class", cls);
      }

   /// increase or decrease the indentation
   void indent(int diff)
      {
        indent_val += diff;
      }

protected:
   /// \b true iff the last output character was \n.
   bool need_indent;

   /// \b true between <PRE...> and </PRE>
   bool in_PRE;

   /// the current indentation level
   size_t  indent_val;
} out;
//-----------------------------------------------------------------------------
class Range
{
public:
   /// a range of length len, starting at cp.
   Range(const char * cp, int len);

   /// a sub-range of this range, from current position to ending at »
   Range(Range & r);

   int getc()
      {
        if (from >= to)   return EOF;

        const int cc = 0xFF & *from++;
        ++col;

        if (cc == '\n')   { ++line;   col = 0; }

        return cc;
      }

   /// the character at from is « thenn skip it and return true; else false
   bool skip_start()
      {
        if (to - from < 2)      return false;   // too short
        if (!is_start(from))    return false;   // not «

        from += 2;
        return true;
      }

   /// maybe skip », return true if skipped
   bool skip_end()
      {
        if (to - from < 2)    return false;
        if (!is_end(from))    return false;

        from += 2;
        return true;
      }

   /// recursively emit \b this range
   int emit(const char * tag);

   /// copy this range to the output
   int copy();

   /// copy this range to file out
   void fcopy(FILE * out);

   /// true if cp is '«'
   static bool is_start(const char * cp)
      { return (cp[0] & 0xFF) == 0xC2 && (cp[1] & 0xFF) == 0xAB; }

   /// true if cp is '»'
   static bool is_end(const char * cp)
      { return (cp[0] & 0xFF) == 0xC2 && (cp[1] & 0xFF) == 0xBB; }


   const char * get_from() const   { return from; }
   const char * get_to()  const    { return to;   }

protected:
   Range * parent;

   const char * from;
   const char * to;

   static int line;
   static int col;
};

int Range::line = 1;
int Range::col = 0;

//-----------------------------------------------------------------------------
Range::Range(const char * cp, int len)
   : parent(0),
     from(cp),
     to(cp + len)
{
   // this constructor is called only once (for the top level)

   // check that « and » match.
   //
int level = 0;
int lnum = 1;
const char * last_start = from;
   for (const char * p = from; p < to; ++p)
       {
          if (*p == '\n')   ++lnum;
          if (is_start(p))   // see «
             {
               last_start = p;
               ++level;
             }
          else if (is_end(p))   // see »
             {
               if (level > 0)
                  {
                    --level;
                  }
               else
                  {
                    fprintf(stderr, "No matching » for « line %d\n", lnum);
                    fprintf(stderr, "Start is around: ");
                    for (const char * p = last_start;
                         p < to && p < (last_start + 40); ++p)
                        fprintf(stderr, "%c", *p);
                    fprintf(stderr, "\n");
                    ++error_count;
//                  _exit(1);
                  }
             }
       }
}
//-----------------------------------------------------------------------------

Range::Range(Range & r)
   : parent(&r),
     from(r.from),
     to(r.to)
{
   // this constructor is called after a beginning « was skipped.
   // find the corresponding », and remove the sub-range from r.
   //
int depth = 1;   // the skipped «
   for (const char * cp = from; cp < to; ++cp)
       {
         if (is_start(cp))
            {
              ++depth;
            }
         else if (is_end(cp))
            {
              --depth;
              if (depth == 0)   // corresponding »
                 {
                   this->to = cp;
                   r.from = cp + 2;
                   return;
                 }
            }
       }

   fprintf(stderr, "No mathing » for « at line %d col %d\n", line, col);
   ++error_count;
// _exit(1);
}
//-----------------------------------------------------------------------------
int
Range::copy()
{
   for (;;)
       {
         if (skip_start())   // another «
            {
              Range sub(*this);
              char tag[20] = { 0 };
              int idx = 0;
              if (sub.from != sub.to)   // unless «»
                 {
                   while (idx < 10)
                       {
                         const int cc = sub.getc();
                         if (cc > ' ')   tag[idx++] = cc;
                         else            break;
                       }
                   tag[idx] = 0;
                 }
              const int error = sub.emit(tag);
              if (error)   return error;
            }
         else
            {
              const int cc = getc();
              if (cc == EOF)   return 1;

              out.Putc(cc);
            }
       }
}
//-----------------------------------------------------------------------------
void
Range::fcopy(FILE * out)
{
   fwrite(from, 1, to - from, out);
   fputc('\n', out);
}
//-----------------------------------------------------------------------------
struct tag_table
{
  const char * tag;
  int (*handler)(const tag_table & tab, Range & r);
  const char * cinfo;
  IINFO        iinfo;
  int          indent;
};
//-----------------------------------------------------------------------------
int
Null_handler(const tag_table & tab, Range & r)
{
   // NOTE: the top-level input is also handled by this Null_handler,
   // calling r.copy() on the entire input,
   //
   switch(tab.iinfo)
      {
        case II1: out.print("&nbsp;");   break;   // «EMPTY»
        default:  r.copy();                       // all others
      }

   return 0;
}
//-----------------------------------------------------------------------------
int
T_TE_handler(const tag_table & tab, Range & r)
{
   // handler for text attributed like «su», «SU», «BO» etc.
   assert(tab.cinfo);
   out.print("<");
   out.print(tab.cinfo);
   out.print(">");

   r.copy();

   out.print("</");
   out.print(tab.cinfo);
   out.print(">");
   if (tab.iinfo)   out.print("\n");
   return 0;
}
//-----------------------------------------------------------------------------
int
BR_handler(const tag_table & tab, Range & r)
{
   out.print("<br>\n");
   return 0;
}
//-----------------------------------------------------------------------------
int
Comment(const tag_table & tab, Range & r)
{
   while (r.get_from() < r.get_to())   r.getc();

   return 0;
}
//════════════════════════════════════════════════════════════════════════════
string
current_chapter()
{
string chapter;
   for (size_t hh = 2; hh <= Hx_len; ++hh)
       {
         chapter += to_string(Hx_num[hh]);
         if (hh < Hx_len)   chapter += ".";
       }

   return chapter;
}
//────────────────────────────────────────────────────────────────────────────
int
Hx_handler(const tag_table & tab, Range & r)
{
const int Hx = tab.iinfo;
   Hx_len = Hx;

   out.print("<h");
   out.Putc(Hx + '0');
   out.print(">");

   if (Hx > 1)
      {
        ++Hx_num[Hx];

        const string chapter = current_chapter();

        // TOC...
        {
          fprintf(fTOC, "<tr class=\"toc_H%d\"><td class=\"toc_c1\"> %s",
                  Hx, chapter.c_str());

          fprintf(fTOC, "<td class=\"toc_c2\">\n"
                        "    <a class=\"toc_a2\" href=\"#CH_%s\"> ",
                        chapter.c_str());

          const char * cp = r.get_from();
          while (cp < r.get_to())   fprintf(fTOC, "%c", *cp++ & 0xFF);
          fprintf(fTOC, " </a>\n\n</td></tr>");
        }

        // BODY...
        {
          char cc[200];
          sprintf(cc, "<a id=\"CH_%s\"></a>\n", chapter.c_str());
          for (const char * s = cc; *s; ++s)            out.Putc(*s);
          for (size_t j = 0; j < chapter.size(); ++j)   out.Putc(chapter[j]);
          out.Putc(' ');
        }

        // clear deeper sub-chapters
        //
        for (int hh = Hx + 1; hh < sizeof(Hx_num)/sizeof(*Hx_num); ++hh)
            Hx_num[hh] = 0;
      }

   r.copy();

   out.print("</h");
   out.Putc(Hx + '0');
   out.print(">");

   if (Hx == 1)
      {
        char svn_version[50] = "none";
        if (FILE * svn = popen("svnversion ~/savannah-repo-apl/trunk", "r"))
           {
              // svnversion exists and was started.
              const int len = fread(svn_version,
                                    1, sizeof(svn_version) - 1, svn);
              svn_version[len] = 0;
              fclose(svn);
           }
        timeval tv;   gettimeofday(&tv, 0);
        const tm * now = localtime(&tv.tv_sec);
        char buffer[500];
        const char * months[] = { "January", "February", "March",
                                  "April",   "May",      "June",
                                  "July",    "August",   "September",
                                  "October",   "November", "December" };

        snprintf(buffer, sizeof(buffer),
                 "<P>%s %d, %d, SVN version %s</P><BR>"
                 "Please report errors in this document to "
                 " <a href=mailto:bug-apl@gnu.org>bug-apl@gnu.org</a>"
                 "<BR><BR>\n",
                  months[now->tm_mon], now->tm_mday, 1900 + now->tm_year,
                       svn_version);
        out.print(buffer);
      }
   return 0;
}
//-----------------------------------------------------------------------------
string Quad_FX_string;

void
one_APL_line(uint8_t * buffer, int line, bool last, bool & nabla,
             bool & in_function, IINFO iinfo)
{
   verbose && fprintf(stderr, "APL: %s      nabla: %d in_function: %d\n",
                      buffer, int(nabla), int(in_function));

const bool first = line == 0 && (iinfo == II0 || iinfo == II1);
   last = last && (iinfo == II0 || iinfo == II3);

   // skip leading spaces
   //
   while (*buffer && *buffer <= ' ')   ++buffer;

   // skip trailing spaces
   //
size_t len = strlen((const char *)buffer);
   while (len && buffer[len - 1] <= ' ')   buffer[--len] = 0;

   if (len == 0)   // empty line
      {
        if (in_function)
           {
             Quad_FX_string += ' ';
             Quad_FX_string += '"';
             Quad_FX_string += '"';
           }
        return;
      }

const bool comment =   len >= 3          &&           // ⍝
                       buffer[0] == 0xe2 &&
                       buffer[1] == 0x8d &&
                       buffer[2] == 0x9d;

const bool got_nabla = len >= 3          &&           // ∇FUNCTION or ∇
                       buffer[0] == 0xE2 &&
                       buffer[1] == 0x88 &&
                       buffer[2] == 0x87;

const bool first_nabla = got_nabla && !in_function;
const bool last_nabla  = got_nabla && in_function;

   if (got_nabla)     nabla = true;
   if (first_nabla)   { in_function = true;   Quad_FX_string = "⊣ ⎕FX"; }
   if (last_nabla)    { in_function = false; }

   if (in_function)   // inside function
      {
        Quad_FX_string += ' ';
        Quad_FX_string += '"';
        for (int l = first_nabla ? 3 : 0; l < len; ++l)
            {
              if (buffer[l] == '"')   Quad_FX_string += '\\';
              Quad_FX_string += buffer[l];
            }
        Quad_FX_string += '"';
      }

   // print the APL input line to HTML
   //
   if (nabla)
      {
        if (first_nabla)
           {
             out.open_PRE("input_", LOC, PROMPT);
             out.print(buffer);
             out.print("\n");
           }
        else if (last_nabla)
           {
             out.print(PROMPT);
             out.print(buffer);
             out.print("\n");
             out.close_PRE(true);
           }
        else
           {
             out.print(PROMPT);
             out.print(buffer);
             out.print("\n");
           }
      }
   else   // not nabla
      {
        if (in_table)   // use CSS without borders
           {
             if (first)   out.open_PRE("input4", LOC, PROMPT);
             out.print(buffer);
             if (last)   out.close_PRE(true);
           }
        else
           {
            if (first)   out.open_PRE("input_T", LOC, PROMPT);
            else         out.open_PRE("input_",  LOC, PROMPT);
            out.print(buffer);
            out.close_PRE(true);
           }
      }

   if (comment)   return;

   // print chapter number to the testcase file
   //
   if (first)
      {
        const string chapter = current_chapter();
        fprintf(ftc, "      ⍝ %s ────────────────────────────"
                                "───────────────────────────\n"
                     "      ⍝\n", chapter.c_str());
      }

   // print APL input line to the testcase file
   //
   if (nabla)
      {
        if (!got_nabla)   fprintf(ftc, " ");
        fprintf(ftc, "%s\n", buffer);
        if (len == 3)   fprintf(ftc, "\n");
        return;
      }
   fprintf(ftc, "      %s\n", buffer);

   // execute the APL line
   //
   stdOut_chars.clear();
   stdErr_chars.clear();
   if (*buffer == ')' || *buffer == ']')   // APL command
      {
        const char * result = apl_command((const char *)buffer);
        if (stdOut_chars.size() == 0)   stdOut_chars = result;
        free(const_cast<char *>(result));
      }
   else                                    // APL expression
       {
         apl_exec((const char *)buffer);
       }

const bool have_err = stdErr_chars.size() > 0;

   // print the APL result
   //
   if (((stdOut_chars.size() == 1 && stdOut_chars[0] == '\n') ||
         stdOut_chars.size() == 0) && !have_err && !nabla)
      {
        // empty output
        //
        if      (in_table)   out.open_PRE("output4", LOC, "\n\n");
        else if (last)       out.open_PRE("output",  LOC, "\n\n");
        else                 out.open_PRE("output1", LOC, "\n");
        out.close_PRE(true);

        fprintf(ftc, "\n");
        return;
      }

   if (stdOut_chars.size())
      {
        const bool final = last && stdErr_chars.size() == 0;
        if      (in_table)   out.open_PRE("output4", LOC, "");
        else if (final)      out.open_PRE("output",  LOC, "");
        else                 out.open_PRE("output1", LOC, "");
        out.print(stdOut_chars.c_str());
        out.close_PRE(true);

        fprintf(ftc, "%s", stdOut_chars.c_str());
      }

   if (stdErr_chars.size())
      {
        if      (in_table)  out.open_PRE("errput4", LOC, "");
        else if (last)      out.open_PRE("errput",  LOC, "");
        else                out.open_PRE("errput1", LOC, "");
        out.print(stdErr_chars.c_str());
        out.close_PRE(true);

        fprintf(ftc, "%s", stdErr_chars.c_str());
      }

   fprintf(ftc, "\n");
}
//-----------------------------------------------------------------------------
int
APL_handler(const tag_table & tab, Range & r)
{
bool nabla = false;
int  nabla_from = -1;
int  nabla_to   = -1;

uint8_t buf[2000];
char fun[2000];
int idx = 0;
int fidx = 0;
int line = 0;
bool in_function = false;

   for (;;)
      {
        int cc = r.getc();
        if (cc == EOF)   break;   // end of «APL ...»
        fun[fidx++] = cc;
        if (cc == '\n')
           {
             buf[idx] = 0;
             one_APL_line(buf, line, false, /* may set */ nabla,
                          in_function, tab.iinfo);

             if (nabla)   // the line starts with ∇ (first ∇ or last ∇)
                {
                  if (!in_function)   // last ∇
                     {
                       // fprintf(stderr, "%s\n", Quad_FX_string.c_str());
                       const int error = apl_exec(Quad_FX_string.c_str());
                       // fprintf(stderr, "⎕FX ERROR: %d\n", error);
                       nabla = false;
                     }

                  if (nabla_from == -1)   nabla_from = line;
                  nabla_to = line + 1;
                }
             else    fidx = 0;
             idx = 0;
             ++line;
           }
        else
           {
             buf[idx++] = cc;
           }
      }

   buf[idx] = 0;
   one_APL_line(buf, line, true, nabla, in_function, tab.iinfo);

   fun[fidx] = 0;
   if (0 && nabla)
      {
        const char * ics = "input";   // assume table
        if (!in_table)
           {
             if (nabla_from == 0)
                {
                  if (nabla_to == line)   ics = "input_TB";
                  else                    ics = "input_T";
                }
             else   // nabla_from > 0
                {
                  if (nabla_to == line)   ics = "input_B";
                  else                    ics = "input_";
                }
           }

        out.open_PRE(ics, LOC, "");
        out.print(fun);
        out.close_PRE(false);
      }

   return 0;
}
//-----------------------------------------------------------------------------
int
QUAD_handler(const tag_table & tab, Range & r)
{
string response;
   for (;;)
      {
        int cc = r.getc();
        if (cc == EOF)   break;
        response += cc;
      }
   quad_responses.push_back(response);
   return 0;
}
//-----------------------------------------------------------------------------
int
IN_handler(const tag_table & ttab, Range & r)
{
const IINFO iinfo = ttab.iinfo;

   if (iinfo == II0 || iinfo == II1 || iinfo == II8)
      {
        const string chapter = current_chapter();
        if (iinfo == II0)
           {
             fprintf(ftc, "      ⍝ %s ────────────────────────────"
                                     "───────────────────────────\n"
                          "      ⍝\n",
                     chapter.c_str());
           }
        r.fcopy(ftc);
      }

   if (iinfo == II0 ||      // IN:             HTML output  .tc output
       iinfo == II1 ||      // IN1: continued  HTML output  .tc output
       iinfo == II2 ||      // IN2: continued  HTML output
       iinfo == II7)        // IN7:            HTML output
      {
        assert(ttab.cinfo);
        out.open_PRE(ttab.cinfo, LOC, "");
        r.copy();
        out.close_PRE(false);
      }
   else if (iinfo == II3)   // IN3:            HTML output
      {
        out.print("<kbd class=");
        out.print(ttab.cinfo);
        out.print(">");
        r.copy();
        out.print("</kbd>");
      }

   return 0;
}
//-----------------------------------------------------------------------------
int
OU_handler(const tag_table & tab, Range & r)
{
const IINFO iinfo = tab.iinfo;

   // testcase file
   //
const bool TC_output = iinfo == II0 ||   // OU
                       iinfo == II1 ||   // OU1
                       iinfo == II8;     // OU8
   if (TC_output)
      {
        r.fcopy(ftc);
        if (iinfo == 0)   fprintf(ftc, "\n\n");
      }

const bool HTML_output = iinfo == II0 ||   // OU
                         iinfo == II1 ||   // OU1
                         iinfo == II2 ||   // OU2
                         iinfo == II7;     // OU7

   if (HTML_output)
      {
        assert(tab.cinfo);
        out.open_PRE(tab.cinfo, LOC, "");
        r.copy();
        out.close_PRE(false);
      }

   return 0;
}
//-----------------------------------------------------------------------------
int
LI_handler(const tag_table & tab, Range & r)
{
   out.print("<li>");
   r.copy();
   out.print("</li>");
   return 0;
}
//-----------------------------------------------------------------------------
int
TAB_handler(const tag_table & tab, Range & r)
{
   ++in_table;

const char * cls = tab.cinfo;
   out.print("<table");
   if (cls)
      {
        out.print(" class=");
        out.print(cls);
      }
   out.print(" cellspacing=\"0\" cellpadding=\"0\">\n<tbody>");

   r.copy();
   out.print("</tbody></table>\n");

   --in_table;
   return 0;
}
//-----------------------------------------------------------------------------
int
TD_handler(const tag_table & tab, Range & r)
{
const int columns = tab.iinfo;
const char * cls = tab.cinfo;

   out.print("<td");
   if (cls)
      {
        out.print(" class=");
        out.print(cls);
      }

   if (columns != 1)
      {
        out.print(" colspan=");
        out.Putc('0' + columns);
      }
   out.print(">");
   r.copy();
   return 0;
}
//-----------------------------------------------------------------------------
int
TO_handler(const tag_table & tab, Range & r)
{
   out.print("<label class=token>");
   r.copy();
   out.print("</label>");
   return 0;
}
//-----------------------------------------------------------------------------
int
TH_handler(const tag_table & tab, Range & r)
{
const int columns = tab.iinfo;
const char * cls = tab.cinfo;

   out.print("<th");
   if (cls)
      {
        out.print(" class=\"");
        out.print(cls);
        out.Putc('"');
      }

   if (columns != 1)
      {
        out.print(" colspan=\"");
        out.Putc('0' + columns);
        out.print("\"");
      }
   out.print(">");
   r.copy();
   return 0;
}
//-----------------------------------------------------------------------------
int
TR_handler(const tag_table & tab, Range & r)
{
   out.print("<tr>");
   r.copy();
   return 0;
}
//-----------------------------------------------------------------------------
int
HREF_handler(const tag_table & tab, Range & r)
{
   /* the content must have the form

      URI> link-name
    */

   out.print("<A href=");
   for (const char * cp = r.get_from(); cp < r.get_to();)
       out.Putc(*cp++);
   out.print(">");
   r.copy();
   out.print("</A>");
   return 0;
}
//-----------------------------------------------------------------------------
int
OL_handler(const tag_table & tab, Range & r)
{
   out.print("<ol>\n");
   r.copy();
   out.print("</ol>\n");
   return 0;
}
//-----------------------------------------------------------------------------
int
UL_handler(const tag_table & tab, Range & r)
{
   out.print("<ul>\n");
   r.copy();
   out.print("</ul>\n");
   return 0;
}
//-----------------------------------------------------------------------------
tag_table open_tags[] =
{
 // tag     handler        cinfo        iinfo   indent
 //                        │            │       │
 { "#",     Comment      , ""         , II0   , 0  },
 { "BO"   , T_TE_handler , "B"        , II0   , 0  },   // bold
 { "IT"   , T_TE_handler , "I"        , II0   , 0  },   // italic
 { "H1"   , Hx_handler   , 0          , II1   , 0  },
 { "H2"   , Hx_handler   , 0          , II2   , 0  },
 { "H3"   , Hx_handler   , 0          , II3   , 0  },
 { "H4"   , Hx_handler   , 0          , II4   , 0  },
 { "H5"   , Hx_handler   , 0          , II5   , 0  },    
 { "H6"   , Hx_handler   , 0          , II6   , 0  },
 { "HREF" , HREF_handler , 0          , II0   , 0  },   // HTML link

                             // in table       ? ────────────────────────┐
                             // in .tc output  ? ───────────────────┐    │
                             // in HTML output ? ──────────────┐    │    │
                             // continuation   ? ─────────┐    │    │    │
/*                                                        │    │    │    │
   APL: APL normal APL input and output (executed)        │    │    │    │
                                                          │    │    │    │
   «TAG»  , C++ handler  , CSS class    iinfo             │    │    │    │ */
 { "APL"  , APL_handler  , 0          , II0   , 0  },  // │    │    │    │
 { "APL1" , APL_handler  , 0          , II1   , 0  },  // │    │    │    │
 { "APL2" , APL_handler  , 0          , II2   , 0  },  // │    │    │    │
 { "APL3" , APL_handler  , 0          , II3   , 0  },  // │    │    │    │
 { "QUAD" , QUAD_handler , 0          , II0   , 0  },  // │    │    │    │
/* IN: normal APL input at the beginning of example       │    │    │    │
                                                          │    │    │    │ */
 { "IN"   , IN_handler   , "input_T"  , II0   , 0  },  // no   yes  yes  no
 { "IN1"  , IN_handler   , "input_"   , II1   , 0  },  // yes  yes  yes  no
 { "IN2"  , IN_handler   , "input_"   , II2   , 0  },  // yes  yes  no   no
 { "IN3"  , IN_handler   , "input"    , II3   , 0  },  // inl  yes  no   no
 { "IN7"  , IN_handler   , "input_T"  , II7   , 0  },  // no   yes  no   no
 { "IN8"  , IN_handler   , "input_T"  , II8   , 0  },  // no   no   yes  no
/* OU: normal APL output with end of example              │    │    │    │
                                                          │    │    │    │ */
 { "OU"   , OU_handler   , "output"   , II0   , 0  },  // no   yes  yes  no
 { "OU1"  , OU_handler   , "output1"  , II1   , 0  },  // yes  yes  yes  no
 { "OU2"  , OU_handler   , "output1"  , II2   , 0  },  // yes  yes  no   no
 { "OU7"  , OU_handler   , "output"   , II7   , 0  },  // no   yes  no   no
 { "OU8"  , OU_handler   , "output"   , II8   , 0  },  // no   no   yes  no

 { "LI"   , LI_handler   , 0          , II0   , 0  },
 { "OL"   , OL_handler   , 0          , II0   , 0  },
 { "-"    , Null_handler , 0          , II0   , 1  },
 { "EMPTY", Null_handler , 0          , II1   , 1  },
 { "SU"   , T_TE_handler , "SUP"      , II0   , 0  },
 { "su"   , T_TE_handler , "SUB"      , II0   , 0  },
 { "TAB"  , TAB_handler  , 0          , II0   , 0  },
 { "TAB1" , TAB_handler  , "table1"   , II0   , 0  },
 { "TD"   , TD_handler   , "tab"      , II1   , 0  }, // CSS: 1 column table
 { "TDC" ,  TD_handler   , "tabC"     , II1   , 0  }, // CSS: tabC (centered)
 { "TD1"  , TD_handler   , "tab1"     , II1   , 0  }, // CSS: tab1
 { "TD2"  , TD_handler   , "tab12"    , II1   , 0  }, // CSS: tab2
 { "TD3"  , TD_handler   , "tab3"     , II1   , 0  }, // CSS: tab3
 { "TD4"  , TD_handler   , "tab4"     , II1   , 0  }, // CSS: tab4
 { "TD5"  , TD_handler   , "tab5"     , II1   , 0  }, // CSS: tab5
 { "TD12" , TD_handler   , "tab1"     , II2   , 0  }, // CSS: tab1
 { "TD45" , TD_handler   , "tab4"     , II2   , 0  }, // CSS: tab4
 { "TH"   , TH_handler   , 0          , II1   , 0  },
 { "TH1"  , TH_handler   , "tab1"     , II1   , 0  }, // CSS: tab1
 { "TH12" , TH_handler   , "tab12"    , II2   , 0  }, // CSS: tab12, 2 columns
 { "TH2"  , TH_handler   , "tab2"     , II1   , 0  }, // CSS: tab2
 { "TH3"  , TH_handler   , "tab3"     , II1   , 0  }, // CSS: tab3
 { "TH4"  , TH_handler   , "tab4"     , II1   , 0  }, // CSS: tab4
 { "TH45" , TH_handler   , "tab45"    , II2   , 0  }, // CSS: tab45, 2 columns
 { "TH5"  , TH_handler   , "tab5"     , II5   , 0  }, // CSS: tab5
 { "TO"   , TO_handler   , 0          , II0   , 0  },
 { "TR"   , TR_handler   , 0          , II0   , 2  },
 { "UL"   , UL_handler   , 0          , II0   , 0  },
 { ""     , BR_handler   , 0          , II0   , 0  },
};

enum { open_tags_count  = sizeof(open_tags)  / sizeof(tag_table) };

//-----------------------------------------------------------------------------
int
Range::emit(const char * tag)
{
   // find handler for tag. Search backwards so that we can store shorter
   // tags before longer ones (like OU before OU1) and still get the longer
   // match.
   //
   for (int h = 0; h < open_tags_count; ++h)
       {
         const tag_table & tab = open_tags[open_tags_count - h - 1];
         if (!strcmp(tag, tab.tag))
            {
              out.indent(tab.indent);
              const int error = tab.handler(tab, *this);
              out.indent(-tab.indent);
              return error;
            }
       }

   fprintf(stderr, "NO handler for '«%s' at line %d col %d\n", tag, line, col);
   ++error_count;
   return 1;
}
//-----------------------------------------------------------------------------
const char *
get_line_callback(int mode, const char * prompt)
{
static char buffer[2000];

   strncpy(buffer, quad_responses.back().c_str(), sizeof(buffer));
   quad_responses.pop_back();
   return buffer;
}
//-----------------------------------------------------------------------------
int
main(int argc, char * argv[])
{
  init_libapl(argv[0], /* do not log startup */ 0);
  disable_safe_mode(); // so )HOST works
  install_get_line_from_user_cb(&get_line_callback);

   assert(argc == 5 && "missing filename(s). Stop.");

const int fd = open(argv[1], O_RDONLY);   // the input file
   assert(fd != -1 && "bad input filename");

   // Content file apl-intro.src.cont
   fout = fopen(argv[2], "w");
   assert(fout && "bad output filename");

   // Table Of Content file apl-intro.src.toc
   fTOC = fopen(argv[3], "w");
   assert(fTOC && "bad TOC filename");

   // testcase file Book.tc
   ftc =  fopen(argv[4], "w");
   assert(ftc && "bad TC filename");

struct stat st;
   fstat(fd, &st);

void * vp = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
   assert(vp != MAP_FAILED);
   close(fd);

   COUT.rdbuf(&stdOut);   // install COUT buffer
   UERR.rdbuf(&stdErr);   // install CERR buffer


Range r((const char *)vp, st.st_size);

const int error = r.emit("-");

   fclose(ftc);
   fclose(fTOC);
   fclose(fout);

   if (error_count)
      {
        fprintf(stderr, "\n\n\n\n\n\n\n"
"     EEEEE  RRRR  RRRR   OOO  RRRR  \n"
"     E      R   R R   R O   O R   R \n"
"     EEEEE  RRRR  RRRR  O   O RRRR  \n"
"     E      R R   R R   O   O R R   \n"
"     EEEEE  R  R  R  R   OOO  R  R  \n"
"                                    \n\n\n\n\n\n\n");
      }
   else
      {
        fprintf(stderr, "\n\n\n\n\n\n\n"
"      OOO    K  K  \n"
"     O   O   K K   \n"
"     O   O   KK    \n"
"     O   O   K K   \n"
"      OOO    K  K  \n"
"                 \n\n\n\n\n\n\n");
      }

   munmap(vp, st.st_size);
const char * kill[] = { "/usr/bin/killall", "tee", 0 };
   execve(kill[0], (char **)kill, 0);
   return 0;
}
//-----------------------------------------------------------------------------
