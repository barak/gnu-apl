
/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2018-2026  Dr. Jürgen Sauermann

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

    Native Windows (GDI+) ⎕PLOT backend, used instead of Plot_gtk.cc /
    Plot_xcb.cc when GNU APL is cross-compiled for Windows with MinGW-w64
    (MINGW_SRC).  This file mirrors Plot_gtk.cc closely -- the same
    drawing primitives, the same grid/legend/surface drawing logic, and
    the same overall structure -- but renders with GDI+ (Gdiplus::Graphics
    et al.) instead of Cairo, and uses a native Win32 HWND + message loop
    instead of a GtkWindow.

    Unlike Plot_gtk.cc (one shared background thread running gtk_main()
    for *all* plot windows) this backend spawns one worker pthread *per*
    plot window: plot_main_WIN32() (called synchronously on the
    interpreter thread, like plot_main_GTK()) starts that thread and
    waits for it to create+show its window (via expose_sema, exactly as
    for GTK/XCB) before returning.  Each such thread owns its HWND and
    runs its own GetMessage()/DispatchMessage() loop until WM_DESTROY --
    this avoids marshaling window creation across threads, which Win32
    requires to happen on the thread that will pump the window's
    messages.
 **/

// WIN32_LEAN_AND_MEAN/UNICODE/_UNICODE must be defined before *any* header
// that might drag in <windows.h> -- including Sys.hh, which includes
// <winsock2.h> (and hence <windows.h>) itself under MINGW_SRC. UNICODE
// resolves generic macros (IDC_ARROW, ...) to their wide variants,
// matching the explicit ...W() Win32/GDI+ calls used throughout this file.
//
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

// Sys.hh (unlike Common.hh) pulls in config.h -- and hence MINGW_SRC --
// without a "using namespace std;", so the #if MINGW_SRC below can gate
// objidl.h/gdiplus.h *before* Common.hh's "using namespace std;" runs.
// Doing it the other way around (Common.hh first) makes C++17's
// std::byte and the Windows COM headers' own global `byte` typedef
// (pulled in by objidl.h, needed for gdiplus.h's IStream use) ambiguous.
//
#include "Sys.hh"

#if MINGW_SRC

#include <vector>

#include <windows.h>            // already pulled in via Sys.hh's winsock2.h,
                                // included again (harmless, header-guarded)
                                // for clarity
#include <windowsx.h>           // GET_X_LPARAM / GET_Y_LPARAM
#include <objidl.h>
#include <gdiplus.h>

#ifndef PW_RENDERFULLCONTENT
# define PW_RENDERFULLCONTENT 0x00000002   // missing in some older SDK headers
#endif

#include "Common.hh"
#include "Plot_data.hh"
#include "Plot_line_properties.hh"
#include "Plot_window_properties.hh"

#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Quad_PLOT.hh"
#include "Workspace.hh"

/// the (generic) font family used for all plot texts
static Gdiplus::FontFamily * plot_font_family = 0;

/// the size of the font to be used for texts (points, matching GTK's
/// FONT_SIZE which cairo interprets as pixels at the default 96 dpi --
/// close enough not to matter visually)
enum { FONT_SIZE = 10 };

/// name of the Win32 window class registered for plot windows
static const wchar_t * PLOT_WNDCLASS = L"GNU_APL_PLOT";

//════════════════════════════════════════════════════════════════════════════
/** a structure that aggregates:

     plot data and plot attributes (Plot_window_properties) and
     the native Windows resources (HWND, memory DC/bitmap, GDI+ Graphics).
 **/
class WIN32_context : public Quad_PLOT::PLOT_context
{
public:
   /// constructor
   WIN32_context(Plot_window_properties & props, Quad_PLOT::Handle handle)
   : PLOT_context(handle),
     w_props(props),
     hwnd(0),
     memDC(0),
     memBitmap(0),
     memBitmapOld(0),
     graphics(0),
     legend_drag(false),
     legend_drag_X(0),
     legend_drag_Y(0),
     legend_X0(0),
     legend_X1(0),
     legend_Y0(0),
     legend_Y1(0)
   {}

   /// destructor
   ~WIN32_context()
      {
        release_backbuffer();
        delete &w_props;
      }

   /// overloaded PLOT_context::plot_stop()
   virtual void plot_stop()
      {
        // PostMessage (not SendMessage/DestroyWindow) is the only
        // cross-thread-safe way to ask a window to close itself: HWNDs
        // may only be manipulated directly from the thread that created
        // them (this window's own worker pthread), while plot_stop() is
        // called from the interpreter thread.
        //
        if (hwnd)   PostMessage(hwnd, WM_CLOSE, 0, 0);
      }

   /// (re)create the off-screen double-buffer bitmap for a client area of
   /// size width × height
   void resize_backbuffer(int width, int height)
      {
        release_backbuffer();
        if (width < 1)    width = 1;
        if (height < 1)   height = 1;

        HDC screenDC = GetDC(0);
        memDC = CreateCompatibleDC(screenDC);
        memBitmap = CreateCompatibleBitmap(screenDC, width, height);
        ReleaseDC(0, screenDC);
        memBitmapOld = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

        graphics = new Gdiplus::Graphics(memDC);
        graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
      }

   /// free the off-screen double-buffer bitmap
   void release_backbuffer()
      {
        if (graphics)    { delete graphics;   graphics = 0; }
        if (memDC)
           {
             if (memBitmapOld)   SelectObject(memDC, memBitmapOld);
             DeleteDC(memDC);
             memDC = 0;
           }
        if (memBitmap)   { DeleteObject(memBitmap);   memBitmap = 0; }
      }

   /// return the required width of the entire plot area
   int get_total_width() const
       {
         return w_props.get_pa_width()
              + w_props.get_pa_border_L()
              + w_props.get_origin_X()
              + w_props.get_pa_border_R();
       }

   /// return the required height of the entire plot area
   int get_total_height() const
       {
         return w_props.get_pa_height()
              + w_props.get_pa_border_T()
              + w_props.get_origin_Y()
              + w_props.get_pa_border_B();
       }

   /// if file name: write the plot window, including its borders. Must be
   /// called while the window is still visible (typically from WM_CLOSE).
   void save_to_file_with_border();

   /// if file name: write the plot window, without its borders, from the
   /// off-screen back-buffer. May be called as soon as the first frame
   /// has been rendered.
   void save_to_file_no_border();

   /// the window properties (as choosen by the user)
   Plot_window_properties & w_props;

   /// the Win32 window of this WIN32_context
   HWND hwnd;

   /// the off-screen double-buffer (memory DC + bitmap it owns)
   HDC memDC;
   HBITMAP memBitmap;
   HBITMAP memBitmapOld;

   /// the GDI+ drawing surface bound to memDC
   Gdiplus::Graphics * graphics;

   /// true if the legend is being dragged
   bool legend_drag;

   /// the starting point of a legend drag
   int legend_drag_X, legend_drag_Y;

   Pixel_X get_legend_X0(const char * loc) const   { return legend_X0; }
   Pixel_X get_legend_X1(const char * loc) const   { return legend_X1; }
   Pixel_Y get_legend_Y0(const char * loc) const   { return legend_Y0; }
   Pixel_Y get_legend_Y1(const char * loc) const   { return legend_Y1; }

   void set_legend_X0(Pixel_X x0, const char * loc)   { legend_X0 = x0; }
   void set_legend_X1(Pixel_X x1, const char * loc)   { legend_X1 = x1; }
   void set_legend_Y0(Pixel_Y y0, const char * loc)   { legend_Y0 = y0; }
   void set_legend_Y1(Pixel_Y y1, const char * loc)   { legend_Y1 = y1; }

protected:
   /// the left and right edges of the legend rectangle (during a drag & drop
   /// operation). The final result (when the legend is dropped) will be
   /// stored in w_props.
   Pixel_X legend_X0, legend_X1;

   /// the top and bottom edges of the legend rectangle (during a drag & drop
   /// operation). The final result (when the legend is dropped) will be
   /// stored in w_props.
   Pixel_Y legend_Y0, legend_Y1;
};
//════════════════════════════════════════════════════════════════════════════
/// convert a 0xRRGGBB Color (see Plot_data.hh) to an opaque Gdiplus::Color
static inline Gdiplus::Color
gdiplus_color(Color color)
{
   return Gdiplus::Color(0xFF, (color >> 16) & 0xFF,
                               (color >>  8) & 0xFF,
                               (color       ) & 0xFF);
}
//════════════════════════════════════════════════════════════════════════════
/// convert a UTF-8 C string to a UTF-16 std::wstring (for GDI+ / Win32
/// wide-character APIs)
static std::wstring
utf8_to_wide(const char * utf8)
{
   if (utf8 == 0 || *utf8 == 0)   return std::wstring();
const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
std::wstring wide(wlen, 0);
   MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wide[0], wlen);
   wide.resize(wcslen(wide.c_str()));   // drop the trailing NUL MultiByteToWideChar counted
   return wide;
}
//────────────────────────────────────────────────────────────────────────────
/// convert a UTF-8 std::string to a UTF-16 std::wstring
static inline std::wstring
utf8_to_wide(const std::string & utf8)
{
   return utf8_to_wide(utf8.c_str());
}
//════════════════════════════════════════════════════════════════════════════
/// draw a line from P0 to P1
static void
draw_line(Gdiplus::Graphics * cr, Color line_color, int line_style,
          int line_width, const Pixel_XY & P0, const Pixel_XY & P1)
{
Gdiplus::REAL dashes_2[] = { 5.0 };          // ╴╴╴╴╴╴╴╴
Gdiplus::REAL dashes_3[] = { 10.0, 5.0 };    // ─╴─╴─╴─╴

Gdiplus::Pen pen(gdiplus_color(line_color), Gdiplus::REAL(line_width));
   if (line_style == 3)
      {
        pen.SetDashPattern(dashes_3, 2);
        pen.SetDashStyle(Gdiplus::DashStyleCustom);
      }
   else if (line_style == 2)
      {
        pen.SetDashPattern(dashes_2, 1);
        pen.SetDashStyle(Gdiplus::DashStyleCustom);
      }
   else
      {
        pen.SetDashStyle(Gdiplus::DashStyleSolid);
      }

   cr->DrawLine(&pen, Gdiplus::REAL(P0.x), Gdiplus::REAL(P0.y),
                     Gdiplus::REAL(P1.x), Gdiplus::REAL(P1.y));
}
//════════════════════════════════════════════════════════════════════════════
/// draw a circle with diameter \b size around P0
static void
draw_circle(Gdiplus::Graphics * cr, Pixel_XY P0, Color color, int size)
{
Gdiplus::SolidBrush brush(gdiplus_color(color));
const Gdiplus::REAL r = 0.5*size;
   cr->FillEllipse(&brush, Gdiplus::REAL(P0.x) - r, Gdiplus::REAL(P0.y) - r,
                          2*r, 2*r);
}
//════════════════════════════════════════════════════════════════════════════
/// draw a ▲ or ▼ with given \b size around P0
static void
draw_delta(Gdiplus::Graphics * cr, Pixel_XY P0, bool up, Color color, int size)
{
const double l = 0.51*size;       // ∆ center to top vertex
const double m = 0.866025404*l;   // ∆ center to base
const double s = 0.5*l;           // ∆ base middle to left/right vertex

Gdiplus::PointF pts[3];
   if (up)   // ▲
      {
        pts[0] = Gdiplus::PointF(P0.x,     P0.y + l);   // top vertex
        pts[1] = Gdiplus::PointF(P0.x + m, P0.y - s);   // right vertex
        pts[2] = Gdiplus::PointF(P0.x - m, P0.y - s);   // left vertex
      }
   else      // ▼
      {
        pts[0] = Gdiplus::PointF(P0.x,     P0.y - l);   // bottom vertex
        pts[1] = Gdiplus::PointF(P0.x + m, P0.y + s);   // right vertex
        pts[2] = Gdiplus::PointF(P0.x - m, P0.y + s);   // left vertex
      }

Gdiplus::SolidBrush brush(gdiplus_color(color));
   cr->FillPolygon(&brush, pts, 3);
}
//════════════════════════════════════════════════════════════════════════════
/// draw a ■ or ◆ with given \b size around P0
static void
draw_quad(Gdiplus::Graphics * cr, Pixel_XY P0, bool caro, Color color, int size)
{
Gdiplus::SolidBrush brush(gdiplus_color(color));

   if (caro)   // ◆
      {
        const double dlta = 0.5 * size;
        Gdiplus::PointF pts[4] =
           { Gdiplus::PointF(P0.x,        P0.y + dlta),
             Gdiplus::PointF(P0.x + dlta, P0.y),
             Gdiplus::PointF(P0.x,        P0.y - dlta),
             Gdiplus::PointF(P0.x - dlta, P0.y) };
        cr->FillPolygon(&brush, pts, 4);
      }
   else        // ■
      {
        const double dlta = 0.35*size;
        cr->FillRectangle(&brush, Gdiplus::REAL(P0.x - dlta),
                                 Gdiplus::REAL(P0.y - dlta),
                                 Gdiplus::REAL(2*dlta), Gdiplus::REAL(2*dlta));
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw a cross with thw given \b size around P0
static void
draw_cross(Gdiplus::Graphics * cr, Pixel_XY P0, bool plus, Color color,
           double size, int size2)
{
   size *= 0.98;
   if (size2 == 0)   size2 = 2;   // default line thickness
Gdiplus::Pen pen(gdiplus_color(color), Gdiplus::REAL(size2));

const double half = 0.5*size;
   if (plus)   // +
      {
        cr->DrawLine(&pen, Gdiplus::REAL(P0.x - half), Gdiplus::REAL(P0.y),
                          Gdiplus::REAL(P0.x + half), Gdiplus::REAL(P0.y));
        cr->DrawLine(&pen, Gdiplus::REAL(P0.x), Gdiplus::REAL(P0.y + half),
                          Gdiplus::REAL(P0.x), Gdiplus::REAL(P0.y - half));
      }
   else        // ×
      {
        const double dlta = 0.707106781*half;
        cr->DrawLine(&pen, Gdiplus::REAL(P0.x - dlta), Gdiplus::REAL(P0.y + dlta),
                          Gdiplus::REAL(P0.x + dlta), Gdiplus::REAL(P0.y - dlta));
        cr->DrawLine(&pen, Gdiplus::REAL(P0.x + dlta), Gdiplus::REAL(P0.y + dlta),
                          Gdiplus::REAL(P0.x - dlta), Gdiplus::REAL(P0.y - dlta));
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw a point with given \b point_style and \b size around P
static void
draw_point(Gdiplus::Graphics * cr, Pixel_XY P, int point_style,
           const Color outer_color, int outer_dia,
           const Color inner_color, int inner_dia)
{
const bool es = ! (point_style & 1);   // even style
   switch(point_style)
      {
        case 0:                                                   return;

        case 1: draw_circle(cr, P,     outer_color, outer_dia);   break;   // ●

        case 2:                                                            // ▲
        case 3: draw_delta( cr, P, es, outer_color, outer_dia);   break;   // ▼

        case 4:                                                            // ◆
        case 5: draw_quad(  cr, P, es, outer_color, outer_dia);   break;   // ■

        case 6:                                                            // +
        case 7: draw_cross( cr, P, es, outer_color, outer_dia,
                                                    inner_dia);   return; // ×

        default: cerr << "⎕PLOT: Invalid point style: "
                      << point_style << endl;                     return;
      }

   // at this point, point_style is one of those that requires (though may not
   // yet honor) the inner_dia.
   //
   if (inner_dia)   switch(point_style)
      {
        case 1: draw_circle(cr, P,     inner_color, inner_dia);   return;   // ●
        case 2:                                                             // ▲
        case 3: draw_delta( cr, P, es, inner_color, inner_dia);   return;   // ▼
        case 4:                                                             // ◆
        case 5: draw_quad(  cr, P, es, inner_color, inner_dia);   return;   // ■
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw an arrow from O to A
static void
draw_arrow(Gdiplus::Graphics * cr, Pixel_XY O, Pixel_XY A, const Color color)
{
   // draw an arrow for an axis that starts at origin O and ends at
   // A (where the arrow starts).

   // 1. create a unit vector U of length 1 parallel to the axis O-A
   //
const int dx = A.x - O.x;
const int dy = A.y - O.y;
const double len = sqrt(double(dx*dx + dy*dy));
const double Ux = dx/len;
const double Uy = dy/len;

   // arrow dimensions (pixel)
   //
   enum { SHAFT = 20, WING  = 5, TIP = 15 };

Gdiplus::Pen pen(gdiplus_color(color), 1.0);
Gdiplus::SolidBrush brush(gdiplus_color(color));

   // draw the shaft
   //
const Pixel_XY S(A.x + SHAFT*Ux, A.y + SHAFT*Uy);
   cr->DrawLine(&pen, Gdiplus::REAL(A.x), Gdiplus::REAL(A.y),
                     Gdiplus::REAL(S.x), Gdiplus::REAL(S.y));

const Pixel_XY W(S.x - WING*Ux, S.y - WING*Uy);
const Pixel_XY W1(W.x - WING*Uy, W.y + WING*Ux);
const Pixel_XY W2(W.x + WING*Uy, W.y - WING*Ux);
const Pixel_XY T(S.x + TIP*Ux, S.y + TIP*Uy);
Gdiplus::PointF pts[4] =
   { Gdiplus::PointF(S.x,  S.y),
     Gdiplus::PointF(W1.x, W1.y),
     Gdiplus::PointF(T.x,  T.y),
     Gdiplus::PointF(W2.x, W2.y) };
   cr->FillPolygon(&brush, pts, 4);
}
//════════════════════════════════════════════════════════════════════════════
/// format \b val according to \b format (with \b percent pointing to % in
/// format, e.g. %d or %f like in printf()).
static const char *
format_user_tick(double val, int tidx, const char * format,
                 const char * percent)
{
static char cc[100] = "";   // should suffice

   if (percent[1] == 's')   // %s: inline tick texts separated by %
      {
        // static tick texts, e.g. '%sTick0%Tick1%...TickN'.
        // They depend on tidx (which selects one of them)
        //
        const char * tick_text = percent + 2;   // e.g. Tick0
        for (int idx = tidx; idx > 0; --idx)   // skip texts < idx
            {
              const char * tick_end = strchr(tick_text + 1, '%');
              if (tick_end == 0)   // no more '%'
                 {
                   CERR << "⎕PLOT: too few tick texts in " << percent
                                << " (at tick " << tidx << ")";
                   SPRINTF(cc, "Tick-%d", tidx);
                   return cc;
                 }
              tick_text = tick_end + 1;
            }

        size_t tick_len = 0;
        if (const char * tick_end = strchr(tick_text, '%'))
           tick_len = tick_end - tick_text;
        else
           tick_len = strlen(tick_text);

        if (tick_len >= (sizeof(cc) - 1))
           {
             CERR << "⎕PLOT: tick text too long";
             SPRINTF(cc, "Tick-%d", tidx);
             return cc;
           }

        strncpy(cc, tick_text, tick_len);
        cc[tick_len] = 0;
        return cc;
      }
   else if (strchr("gvSIHhDdMmQqYy", percent[1]))   // some kind of date
      {
        // dynamic tick text (a subset of those in "man date")
        //
        const time_t when = time_t(val);
        const tm * tm = localtime(&when);
        if (tm == 0)   // error in localtime()
           {
             cerr << "bad value in formatted ⎕PLOT tick";
             return format;
           }
        const int year    = tm->tm_year + 1900;   // tm 0-100 → APL
        const int month   = tm->tm_mon  + 1;      // tm 0-11  → APL
        const int quarter = tm->tm_mon / 3;       // tm 0-11  → APL

        int cc_pos = 0;
        enum { cc_max = sizeof(cc) - 1 };
        while (const char chr = *format++)
           {
             if (chr != '%')
                {
                  if (cc_pos < cc_max)   cc[cc_pos++] = chr;
                  continue;   // next char in format
                }

             // chr is % (start of field).
             //
             char item[20];
             switch (*format++)
                {
                  case 'S': SPRINTF(item, "%2.2d", tm->tm_sec);    break;
                  case 'G': SPRINTF(item, "%d", tidx + 1);         break;
                  case 'g': SPRINTF(item, "%d", tidx);             break;
                  case 'v': SPRINTF(item, "%d", int(val));         break;
                  case 'I': SPRINTF(item, "%2.2d", tm->tm_min);    break;
                  case 'H': SPRINTF(item, "%2.2d", tm->tm_hour);   break;
                  case 'h': SPRINTF(item, "%d", tm->tm_hour);      break;
                  case 'D': SPRINTF(item, "%2.2d", tm->tm_mday);   break;
                  case 'd': SPRINTF(item, "%d", tm->tm_mday);      break;
                  case 'M': SPRINTF(item, "%2.2d", month);         break;
                  case 'm': SPRINTF(item, "%d", month);            break;
                  case 'Q': SPRINTF(item, "%d", quarter + 1);      break;
                  case 'q': SPRINTF(item, "%d", quarter);          break;
                  case 'Y': SPRINTF(item, "%4.4d", year);          break;
                  case 'y': SPRINTF(item, "%2.2d", year%100);      break;

                   default: break;   // invalid/unsupported; copy cc1
                }

             // copy item
             //
             item[sizeof(item) - 1] = 0;
             for (const char * i = item; *i; ++i)
                 if (cc_pos < cc_max)   cc[cc_pos++] = *i;
           }
        cc[cc_pos] = 0;   // 0-terminator fpr cc.
        return cc;
      }

bool round = false;
   while (*percent)
      {
        const char cc = *percent++;
        if (cc == 'd')  { round = true;   break; }   // %d
        if (cc == 'i')  { round = true;   break; }   // %i
        if (cc == 'u')  { round = true;   break; }   // %u
      }

   // format is the FULL user-supplied string, used as-is below as a
   // printf format with exactly one argument (val, or a rounded
   // int(val)). Extra or mismatched conversions in format are otherwise
   // honored against a nonexistent/wrong-typed vararg: %n is a write
   // primitive, and %s/%p (or any other non-numeric conversion) reads a
   // nonexistent vararg as a pointer -- validate that format has
   // exactly one, numeric, conversion before using it.
   {
     int conversions = 0;
     bool safe = true;
     for (const char * p = format; *p; ++p)
         {
           if (*p != '%')   continue;
           ++p;
           if (*p == '%')   continue;   // %% is a literal percent
           ++conversions;
           while (*p && strchr("-+ #0123456789.", *p))   ++p;   // flags/width/precision
           if (!*p || !strchr("diouxXeEfFgGaA", *p))   safe = false;
         }
     if (conversions != 1 || !safe)
        {
          CERR << "⎕PLOT: bad tick format '" << format << "'" << endl;
          SPRINTF(cc, "%g", val);
          return cc;
        }
   }

   if (!round)         SPRINTF(cc, format, val)
   else if (val > 0)   SPRINTF(cc, format, int(rint(val) + 0.5))
   else                SPRINTF(cc, format, int(rint(val) - 0.5))

   return cc;
}
//════════════════════════════════════════════════════════════════════════════
/// format the text of tick \b idx (with value \b val) according to \b format.
static const char *
format_tick(double val, double dV, int idx, const char * format)
{
   if (*format)   // user-defined format string
      {
        if (const char * percent = strchr(format, '%'))
           return format_user_tick(val, idx, format, percent);

        cerr << "⎕PLOT: no % in user-defined format string" << endl;
        return format;
      }

   if (val == 0)   return "0";

static char cc[40];   // should suffice

   /* Avoid duplicate axis strings when dV is small. For that we need so many
      fractional digits that dV makes a difference.

      if dV ≥ 1    then adding dV will increment the integer part of val and
                        no  fractional digits are needed.
      if dV ≥ 0.1  then one fractional digit is needed to make a difference
      if dV ≥ 0.01 then two fractional digit is needed
      ...

      dV is { 1, 2, 5 }×10ⁿ and threrfore it is better to compare with 0.9
      instead of 1.0 as to avoid rounding problems with 1.0 < 1.0.
    */
char fmt[10];
short digits = 0;
   while (dV < 0.9)   { ++digits;   dV *= 10.0; }
   SPRINTF(fmt, "%%.%uf", digits);   // e.g. %.2f
   SPRINTF(cc,  fmt, val);

   // skip leading 0 in 0.xxx
   if (cc[1] == '.' && cc[0] == '0')   return cc + 1;

   // skip leading 0 in -0.xxx
   if (cc[2] == '.' && cc[1] == '0' && cc[0] == '-')
      {
      cc[1] = '-';
       return cc + 1;
      }

   // skip leading 0 in -0.xxx
   return cc;
}
//════════════════════════════════════════════════════════════════════════════
/// return the size of single-line string \b text when printed with the
/// plot font at size \b font_size
static void
gdip_string_size(double & lx, double & ly, Gdiplus::Graphics * cr,
                 const char * text, double font_size)
{
Gdiplus::Font font(plot_font_family, Gdiplus::REAL(font_size));
const std::wstring wtext = utf8_to_wide(text);

Gdiplus::RectF bbox;
   cr->MeasureString(wtext.c_str(), wtext.size(), &font,
                     Gdiplus::PointF(0.0f, 0.0f), &bbox);
   lx = bbox.Width;
   ly = bbox.Height;

  if (Quad_PLOT::get_verbosity() & SHOW_DRAW)
     {
        CERR << "    string '" << text << "' : " << lx << " pixels" << endl;
     }
}
//════════════════════════════════════════════════════════════════════════════
/// return the size len_x:len_y of the multi_line string \b lines when
/// printed with the plot font at size \b font_size
static void
gdip_multiline_size(double & len_x, double & len_y, Gdiplus::Graphics * cr,
                    const char * lines, double font_size)
{
  len_x = 0;
  len_y = 0;
std::vector<char> line(strlen(lines) + 10);

   // split lines into single strings line and cumulate the sizes
   for (;;)
       {
         double lx = 0;
         double ly = 0;
         if (const char * nl = strchr(lines, '\n'))   // more lines coming
            {
              const size_t len = nl - lines;
              memcpy(line.data(), lines, len);
              line[len] = 0;
              lines = nl + 1;

              gdip_string_size(lx, ly, cr, line.data(), font_size);
              if (len_x < lx)   len_x = lx;   // horizontal: take maximum
              if (len_y)   len_y += 2;        // space between lines
              len_y += ly;                    // vertical: add height
            }
         else                      // final line
            {
              gdip_string_size(lx, ly, cr, lines, font_size);
              if (len_x < lx)   len_x = lx;   // horizontal: take maximum
              if (len_y)   len_y += 2;        // space between lines
              len_y += ly;                    // vertical: add height
              break;   // for (;;)
            }
       }
}
//════════════════════════════════════════════════════════════════════════════
/// draw single-line string \b text at position xy (in black, matching
/// Plot_gtk.cc's draw_text() which never sets a color either -- text is
/// always black in this ⎕PLOT implementation)
///
/// xy is the *baseline*-left origin, matching Plot_gtk.cc's draw_text()
/// (cairo_move_to() + cairo_text_path() anchor at the baseline) -- so
/// that callers ported from Plot_gtk.cc's Y offsets (tuned for that
/// baseline convention) don't all need re-tuning here. GDI+'s
/// Graphics::DrawString(..., PointF, ...) instead anchors at the
/// top-left of the text's full bounding box, which without a
/// correction sits noticeably too low (by roughly the font's ascent)
/// wherever a caller passed a baseline-style Y -- e.g. the plot legend,
/// where the "Line-1" label sat about a line lower than the "--o--"
/// marker to its left, which is drawn independently of text metrics.
static void
draw_text(Gdiplus::Graphics * cr, const char * text, const Pixel_XY & xy)
{
Gdiplus::Font font(plot_font_family, Gdiplus::REAL(FONT_SIZE));
Gdiplus::SolidBrush brush(Gdiplus::Color(0xFF, 0, 0, 0));
const std::wstring wtext = utf8_to_wide(text);

const Gdiplus::REAL ascent = FONT_SIZE
      * plot_font_family->GetCellAscent(Gdiplus::FontStyleRegular)
      / Gdiplus::REAL(plot_font_family->GetEmHeight(Gdiplus::FontStyleRegular));

   cr->DrawString(wtext.c_str(), wtext.size(), &font,
                  Gdiplus::PointF(Gdiplus::REAL(xy.x),
                                  Gdiplus::REAL(xy.y) - ascent),
                  &brush);
}
//════════════════════════════════════════════════════════════════════════════
/// draw multi-line string \b lines at position xy
static void
draw_multiline(Gdiplus::Graphics * cr, const char * lines, Pixel_XY xy,
              double width)
{
std::vector<char> line(strlen(lines) + 10);

   for (;;)
       {
         if (const char * nl = strchr(lines, '\n'))   // more lines coming
            {
              const size_t len = nl - lines;
              memcpy(line.data(), lines, len);
              line[len] = 0;
              lines = nl + 1;

              double line_w = 0, line_h = 0;   // size of line
              gdip_string_size(line_w, line_h, cr, line.data(), FONT_SIZE);
              const double indent = 0.5*(width - line_w);

              draw_text(cr, line.data(), Pixel_XY(xy.x + indent, xy.y));
              xy.y += 2 + FONT_SIZE;
            }
         else   // final line
            {
              draw_text(cr, lines, xy);
              break;
            }
       }
}
//════════════════════════════════════════════════════════════════════════════
static double
longest_legend_string(Gdiplus::Graphics * cr, int line_count,
                      const Plot_line_properties * const * l_props)
{
double ly = FONT_SIZE;
double longest_len = 0.0;
   loop(l, line_count)
       {
         double lx = 0.0;
         const String & line_name = l_props[l]->get_legend_name();
         gdip_string_size(lx, ly, cr, line_name.c_str(), FONT_SIZE);
         if (longest_len < lx)   longest_len = lx;
       }
   return longest_len;
}
//════════════════════════════════════════════════════════════════════════════
/// draw a legend for the different plot lines
static void
draw_legend(Gdiplus::Graphics * cr, WIN32_context & pctx, bool surface_plot)
{
const Plot_window_properties & w_props = pctx.w_props;
   if (!w_props.get_show_legend())   return;   // no legend

const Plot_line_properties * const * l_props = w_props.get_line_properties();

const int line_count = surface_plot ? 1 : w_props.get_line_count();

const double longest_len = longest_legend_string(cr, line_count, l_props);

const Color canvas_color = w_props.get_canvas_color();
const Color legend_color = w_props.get_legend_color();

  /*
                             ┌────────────────────────┐
   every legend looks like:  │  ───o───  legend_name  │──── y0
                             └────────────────────────┘
                                │  │  │  │
                               x0 x1 x2 xt
                                │  │  │  │
                                │  │  │  └──── start of legend text
                                │  │  └─────── end of legend line
                                │  └────────── point of legend line
                                └───────────── start of legend line
  */

const Pixel_XY origin = w_props.get_origin(surface_plot);
const Pixel_X x0 = origin.x + w_props.get_legend_X();
const Pixel_Y y0 = origin.y + w_props.get_legend_Y() - w_props.get_pa_height();

const Pixel_X x1 = x0 + 30;                              // point o in --o--
const Pixel_X x2 = x1 + 30;                              // end of     --o--
const Pixel_X xt = x2 + 10;                              // text after --o--
const Pixel_X xe = xt + longest_len;                     // end of legend_name

const Pixel_Y dy = w_props.get_legend_dY();

   // draw the legend background
   {
     const double ly2 = 0.5*FONT_SIZE;

     enum { BORDER = 10 };   // border around legend block
     const double X0 = x0                             - BORDER;
     const double X1 = xe                             + BORDER;
     const double Y0 = y0 - ly2                       - BORDER;
     const double Y1 = y0 + ly2 + dy*(line_count - 1) + BORDER;

     // Remember the last legend corners in pctx. These corners are only
     // used in the WM_LBUTTONDOWN handler to quickly determine if the next
     // mouse button press is inside or outside the currently visible legend
     // rectangle (drawn by this function). Don't confuse
     // pctx.set_legend_[XY][01]() with w_props.get_legend_[XY]() which is
     // the initial legend position chosen by the user and which is
     // updated in the WM_MOUSEMOVE handler as the legend is being dragged.
     //
     pctx.set_legend_X0(X0, LOC);
     pctx.set_legend_X1(X1, LOC);
     pctx.set_legend_Y0(Y0, LOC);
     pctx.set_legend_Y1(Y1, LOC);

     if (Quad_PLOT::get_verbosity() & SHOW_DRAW)
        CERR << "draw_legend(): X0=" << X0 << " Y0=" << Y0
             << " W=" << (X1 - X0) << " H=" << (Y1 - Y0)
             << " Background: 0:0 to " << pctx.get_total_width()
             << ":" << pctx.get_total_height() << endl;

     Gdiplus::SolidBrush bg_brush(gdiplus_color(legend_color));
     cr->FillRectangle(&bg_brush, Gdiplus::REAL(X0), Gdiplus::REAL(Y0),
                       Gdiplus::REAL(X1 - X0), Gdiplus::REAL(Y1 - Y0));

     Gdiplus::Pen brim_pen(Gdiplus::Color(0xFF, 0, 0, 0), 2.0);   // black brim
     cr->DrawRectangle(&brim_pen, Gdiplus::REAL(X0), Gdiplus::REAL(Y0),
                       Gdiplus::REAL(X1 - X0), Gdiplus::REAL(Y1 - Y0));
   }
   for (int l = 0; l < line_count; ++l)
       {
         const Plot_line_properties & lp = *l_props[l];
         const Color line_color  = lp.get_line_color();
         const int line_style    = lp.get_line_style();
         const int line_width    = lp.get_line_width();
         const int point_style   = lp.get_point_style();
         const Color point_color = lp.get_point_color();
         const int point_size    = lp.get_point_size();
         const int point_size2   = lp.get_point_size2();

         const Pixel_Y y1 = y0 + l*dy;

         draw_line(cr, line_color, line_style, line_width,
                   Pixel_XY(x0, y1), Pixel_XY(x2, y1));
         draw_point(cr, Pixel_XY(x1, y1), point_style, point_color,
                    point_size, canvas_color, point_size2);
         draw_text(cr, lp.get_legend_name().c_str(), Pixel_XY(xt, y1 + 5));
       }
}
//════════════════════════════════════════════════════════════════════════════
/// swap pixels P0 with value Y0, and P1 with value Y1
static inline void
pv_swap(Pixel_XY & P0, double & Y0, Pixel_XY & P1, double & Y1)
{
const double   Y = Y0;   Y0 = Y1;   Y1 = Y;
const Pixel_XY P = P0;   P0 = P1;   P1 = P;
}
//════════════════════════════════════════════════════════════════════════════
/// draw a 3D triangle where one point P0 is at visual height H0 and the
/// other two points P1 and P2 are the same at visual height H12.
static void
draw_triangle(Gdiplus::Graphics * cr, const WIN32_context & pctx, int verbosity,
              Pixel_XY P0, double H0, Pixel_XY P1, Pixel_XY P2, double H12)
{
   // draw a triangle with P1 and P2 at the same level H12
   //
   Assert(H0 >= 0.0);    Assert(H0 <= 1.0);
   Assert(H12 >= 0.0);   Assert(H12 <= 1.0);

   if (verbosity & SHOW_DRAW)
      CERR <<   " ∆2: P0(" << P0.x << ":" << P0.y << ") @H0=" << H0
           << "     P1(" << P1.x << ":" << P1.y << ") @H12=" << H12
           << "     P2(" << P2.x << ":" << P2.y << ") @H12=" << H12 << endl;

   // every line is ~1 pixel, so the max y should suffice for steps
   //
int steps = P0.y;
   if (steps < P1.y)   steps = P1.y;
   if (steps < P2.y)   steps = P2.y;
   if (steps < 1)   steps = 1;
const double dH = (H12 - H0) / steps;

   loop(s, steps + 1)
       {
         const double alpha = H0 + s*dH;
         const double beta  = (1.0 * s)/steps;   // line lenght
         const Color line_color = pctx.w_props.get_color(alpha);
         const Pixel_XY P0_P1(P0.x + beta * (P1.x - P0.x),
                              P0.y + beta * (P1.y - P0.y));
         const Pixel_XY P0_P2(P0.x + beta * (P2.x - P0.x),
                              P0.y + beta * (P2.y - P0.y));
         draw_line(cr, line_color, 1, 1, P0_P1, P0_P2);
       }
}
//════════════════════════════════════════════════════════════════════════════
/// draw a 3D triangle where the points P0, P1, and P2 are at visual
/// heights H0, H1, and H2 respectively. This is done by splitting the
/// triangle into two triangles that each have 2 points at the same height.
static void
draw_triangle(Gdiplus::Graphics * cr, const WIN32_context & pctx, int verbosity,
              Pixel_XY P0, double H0, Pixel_XY P1, double H1,
              Pixel_XY P2, double H2)
{
   // draw a triangle with P0, P1 and P2 at levels H0, H1, and H2

   Assert(H0 >= 0);   Assert(H0 <= 1.0);
   Assert(H1 >= 0);   Assert(H1 <= 1.0);
   Assert(H2 >= 0);   Assert(H2 <= 1.0);

   if (verbosity & SHOW_DRAW)
      CERR << "\n∆1: P0(" << P0.x << ":" << P0.y << ")@H=" << H0
           << "      P1(" << P1.x << ":" << P1.y << ")@H=" << H1
           << "      P2(" << P2.x << ":" << P2.y << ")@H=" << H2 << endl;

   if (H0 < H1)   pv_swap(P0, H0, P1, H1);   // then H0 >= H1
   if (H0 < H2)   pv_swap(P0, H0, P2, H2);   // then H0 >= H2
   if (H1 < H2)   pv_swap(P1, H1, P2, H2);   // then H1 >= H2

   // here H0 >= H1 >= H2
   //
   Assert(H0 >= H1);
   Assert(H1 >= H2);

const Plot_window_properties & w_props = pctx.w_props;
const vector<level_color> & color_steps = w_props.get_gradient();
   if (color_steps.size() == 0)
      {
        CERR << "*** no color_steps" << endl;
        return;
      }

   if (H0 == H1)   // P0 and P1 have the same height
      {
        draw_triangle(cr, pctx, verbosity, P2, H2, P0, P1, H0);
      }
   else if (H1 == H2)   // P1 and P2 have the same height
      {
        draw_triangle(cr, pctx, verbosity, P0, H0, P1, P2, H1);
      }
   else            // P2 lies below P1
      {
        const double alpha = (H0 - H1) / (H0 - H2);   // alpha → 1 as P1 → P2
        Assert(alpha >= 0.0);
        Assert(alpha <= 1.0);

        // compute the point P on P0-P2 that has the heigth H1 (of P1)
        const Pixel_XY P(P0.x + alpha*(P2.x - P0.x),
                         P0.y + alpha*(P2.y - P0.y));
        draw_triangle(cr, pctx, verbosity, P0, H0, P1, P, H1);
        draw_triangle(cr, pctx, verbosity, P2, H2, P1, P, H1);
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw the (vertical) X grid-lines of the plot (starting from the Y axis)
/// and proceeding with equal distances w_props.get_value_per_tile_X().
static void
draw_X_grid(Gdiplus::Graphics * cr, const WIN32_context & pctx, bool surface_plot)
{
const Plot_window_properties & w_props = pctx.w_props;
const int line_width = w_props.get_gridX_line_width();
const Color grid_color = w_props.get_gridX_color();

const Pixel_Y py0 = w_props.valY2pixel(0);
const double dv = w_props.get_max_Y() - w_props.get_min_Y();
const Pixel_Y py1 = w_props.valY2pixel(dv);
const int grid_style = w_props.get_gridX_style();

   for (int ix = 0; ix <= w_props.get_gridX_last(); ++ix)
       {
         const double dV = w_props.get_value_per_tile_X();
         const double v = w_props.get_min_X() + ix*dV;
         const int px0 = w_props.valX2pixel(v - w_props.get_min_X())
                       + w_props.get_origin_X();

         // draw the first and last grid line solid, the others with
         // the desired line style.
         //
         if (ix == 0 || ix == w_props.get_gridX_last())
            {
              draw_line(cr, grid_color, /* solid */ 1, line_width,
                        Pixel_XY(px0, py0 + 5), Pixel_XY(px0, py1));
            }
         else
            {
              draw_line(cr, grid_color, grid_style, line_width,
                        Pixel_XY(px0, py0 + 5), Pixel_XY(px0, py1));
            }

         string format = w_props.get_format_X();
         const char * cc = format_tick(v, dV, ix, format.c_str());
         double cc_width, cc_height;
         gdip_multiline_size(cc_width, cc_height, cr, cc, FONT_SIZE);

         Pixel_XY cc_pos(px0 - 0.5*cc_width, py0 + cc_height + 8);
         if (surface_plot)
            {
              cc_pos.x -= w_props.get_origin_X();
              cc_pos.y += w_props.get_origin_Y();
            }
         draw_multiline(cr, cc, cc_pos, cc_width);
       }

   if (w_props.get_axisX_arrow())
      {
        const Pixel_XY origin = w_props.get_origin(surface_plot);
        const Pixel_X px = w_props.valX2pixel(dv) + w_props.get_origin_X();

        Pixel_XY P(px, origin.y);
        draw_arrow(cr, origin, P, grid_color);

        const string arrow_label = w_props.get_axisX_label();
        if (arrow_label.size())
           {
             draw_text(cr, arrow_label.c_str(), Pixel_XY(P.x + 40, P.y + 5));
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw (vertical) X grid-lines where plot points exist.
static void
draw_X_vargrid(Gdiplus::Graphics * cr, const WIN32_context & pctx, bool surface_plot)
{
const Plot_window_properties & w_props = pctx.w_props;
const int line_width = w_props.get_gridX_line_width();
const Color grid_color = w_props.get_gridX_color();

const Pixel_Y py0 = w_props.valY2pixel(0);
const double dv = w_props.get_max_Y() - w_props.get_min_Y();
const Pixel_Y py1 = w_props.valY2pixel(dv);
const int grid_style = w_props.get_gridX_style();

   // draw the first and last grid line solid
   {
     const double dV = w_props.get_value_per_tile_X();
     double v = w_props.get_min_X();
     int px0 = w_props.valX2pixel(v - w_props.get_min_X())
                       + w_props.get_origin_X();
     draw_line(cr, grid_color, /* solid */ 1, line_width,
               Pixel_XY(px0, py0 + 5), Pixel_XY(px0, py1));

     v = w_props.get_min_X() + w_props.get_gridX_last()*dV;
     px0 = w_props.valX2pixel(v - w_props.get_min_X())
                       + w_props.get_origin_X();
     draw_line(cr, grid_color, /* solid */ 1, line_width,
               Pixel_XY(px0, py0 + 5), Pixel_XY(px0, py1));
   }

vector<double> xvals;
const Plot_data & data = w_props.get_plot_data();
   loop(r, data.get_row_count())
      {
        const Plot_data_row & row = data[r];
        loop(col, row.get_N())
            {
              const double val = row.get_X(col);
              xvals.push_back(val);
            }
      }

   loop(ix, xvals.size())
       {
         const double dV = w_props.get_value_per_tile_X();
         const double v = xvals[ix];
         const int px0 = w_props.valX2pixel(v - w_props.get_min_X())
                       + w_props.get_origin_X();

         // draw the first other lines with the desired line style.
         //
         draw_line(cr, grid_color, grid_style, line_width,
                        Pixel_XY(px0, py0 + 5), Pixel_XY(px0, py1));

         string format = w_props.get_format_X();
         const char * cc = format_tick(v, dV, ix, format.c_str());
         double cc_width, cc_height;
         gdip_multiline_size(cc_width, cc_height, cr, cc, FONT_SIZE);

         Pixel_XY cc_pos(px0 - 0.5*cc_width, py0 + cc_height + 8);
         if (surface_plot)
            {
              cc_pos.x -= w_props.get_origin_X();
              cc_pos.y += w_props.get_origin_Y();
            }
         draw_multiline(cr, cc, cc_pos, cc_width);
       }

   if (w_props.get_axisX_arrow())
      {
        const Pixel_XY origin = w_props.get_origin(surface_plot);
        const Pixel_X px = w_props.valX2pixel(dv) + w_props.get_origin_X();

        Pixel_XY P(px, origin.y);
        draw_arrow(cr, origin, P, grid_color);

        const string arrow_label = w_props.get_axisX_label();
        if (arrow_label.size())
           {
             draw_text(cr, arrow_label.c_str(), Pixel_XY(P.x + 40, P.y + 5));
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw the (horizontal) Y grid-lines of the plot (starting at the Y axis)
static void
draw_Y_grid(Gdiplus::Graphics * cr, const WIN32_context & pctx, bool surface_plot)
{
const Plot_window_properties & w_props = pctx.w_props;
const int line_width = w_props.get_gridY_line_width();
const Color grid_color = w_props.get_gridY_color();

const Pixel_X px0 = w_props.valX2pixel(0) + w_props.get_origin_X();
const double dv = w_props.get_max_X() - w_props.get_min_X();
const Pixel_X px1 = w_props.valX2pixel(dv) + w_props.get_origin_X();
const int grid_style = w_props.get_gridY_style();

   for (int iy = 0; iy <= w_props.get_gridY_last(); ++iy)
       {
         const double dV = w_props.get_value_per_tile_Y();
         const double v = w_props.get_min_Y() + iy*dV;
         const Pixel_Y py0 = w_props.valY2pixel(v - w_props.get_min_Y());

         // draw the first and last grid line solid, the others with
         // the desired line style.
         //
         if (iy == 0 || iy == w_props.get_gridY_last())   // first or last line
            {
              draw_line(cr, grid_color, /* solid */ 1, line_width,
                        Pixel_XY(px0 - 5, py0), Pixel_XY(px1 + 1, py0));
            }
         else
            {

              draw_line(cr, grid_color, grid_style, line_width,
                        Pixel_XY(px0 - 5, py0), Pixel_XY(px1 + 1, py0));
            }
         string format = w_props.get_format_Y();
         const char * cc = format_tick(v, dV, iy, format.c_str());
         double cc_width, cc_height;
         gdip_multiline_size(cc_width, cc_height, cr, cc, FONT_SIZE);

         Pixel_XY cc_pos(px0 - cc_width - 8, py0 + 0.5 * cc_height - 1);
         if (surface_plot)
            {
              cc_pos.x -= w_props.get_origin_X();
              cc_pos.y += w_props.get_origin_Y();
            }

           if (int16_t(cc_pos.x) < 0)
              {
                CERR << "⎕PLOT warning: pa_border_L="
                     << int(w_props.get_pa_border_L())
                     << " is too small to print all Y-axis ticks (add "
                     << -int16_t(cc_pos.x) << ").";
                COUT << endl;
              }

           draw_multiline(cr, cc, cc_pos, cc_width);
       }

   if (w_props.get_axisY_arrow())
      {
        const Pixel_XY origin = w_props.get_origin(surface_plot);
        Pixel_Y Ay;
        if (surface_plot)
           {
            Ay = w_props.valXYZ2pixelXY(w_props.get_min_X(),
                                        w_props.get_max_Y(),
                                        w_props.get_min_Z()).y;
           }
        else
           {
            Ay = w_props.valY2pixel(dv);
           }

        Pixel_XY P(origin.x, Ay);
        draw_arrow(cr, origin, P, grid_color);

        const string arrow_label = w_props.get_axisY_label();
        if (arrow_label.size())
           {
             double cc_width, cc_height;
             gdip_string_size(cc_width, cc_height, cr, arrow_label.c_str(),
                              FONT_SIZE);
             draw_text(cr, arrow_label.c_str(),
                       Pixel_XY(P.x - 0.5*cc_width, P.y - 40));
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw the (slanted) Z grid-lines of the plot
static void
draw_Z_grid(Gdiplus::Graphics * cr, const WIN32_context & pctx)
{
const Plot_window_properties & w_props = pctx.w_props;
const int line_width = w_props.get_gridZ_line_width();
const Color grid_color = w_props.get_gridZ_color();

const int ix_max = w_props.get_gridX_last();
const int iy_max = w_props.get_gridY_last();
const int iz_max = w_props.get_gridZ_last();
const Pixel_X len_Zx = w_props.get_origin_X();
const Pixel_Y len_Zy = w_props.get_origin_Y();
const Pixel_XY orig = w_props.valXYZ2pixelXY(w_props.get_min_X(),
                                             w_props.get_min_Y(),
                                             w_props.get_min_Z());
const Pixel_X len_X = w_props.valX2pixel(w_props.get_max_X())
                    - w_props.valX2pixel(w_props.get_min_X());

   /* NOTE: like Cairo (and unlike XCB), GDI+'s Y-coordinates increase when
      moving down the screen. Therefore larger Y values correspond to
      smaller Y coordinates and we must subtract
      w_props.valY2pixel(w_props.get_max_Y()) from
      w_props.valY2pixel(w_props.get_min_Y()) and not the other way around.
    */
const Pixel_Y len_Y = w_props.valY2pixel(w_props.get_min_Y())
                    - w_props.valY2pixel(w_props.get_max_Y());

   // lines starting on the Z-axis...
   //
int grid_style = w_props.get_gridZ_style();
   for (int iz = 1; iz <= iz_max; ++iz)
       {
         const Pixel_X px0 = orig.x - iz * len_Zx / iz_max;
         const Pixel_Y py0 = orig.y + iz * len_Zy / iz_max;
         const Pixel_X px1 = px0 + len_X;
         const Pixel_Y py1 = py0 - len_Y;
         const  Pixel_XY PZ(px0, py0);   // point on the Z axis
         const  Pixel_XY PX(px1, py0);   // along the X axis
         const  Pixel_XY PY(px0, py1);   // along the Y axis

         // draw the first and last grid line solid, the others with
         // the desired line style.
         //
         if (iz == iz_max)   // full line
            {
              draw_line(cr, grid_color, /* solid */ 1, line_width, PZ, PX);
              draw_line(cr, grid_color, /* solid */ 1, line_width, PZ, PY);
            }
         else
            {
              draw_line(cr, grid_color, grid_style, line_width, PZ, PX);
              draw_line(cr, grid_color, grid_style, line_width, PZ, PY);
            }

         const double dV = w_props.get_value_per_tile_Z();
         const double v = w_props.get_min_Z() + iz*dV;
         string format = w_props.get_format_Z();
         const char * cc = format_tick(v, dV, iz, format.c_str());
         double cc_width, cc_height;
         gdip_multiline_size(cc_width, cc_height, cr, cc, FONT_SIZE);
         const Pixel_XY cc_pos(px1 + 10, py0 + 0.5*cc_height - 1);
         draw_multiline(cr, cc, cc_pos, cc_width);
       }

   // lines starting on the X-axis...
   //
   grid_style = w_props.get_gridX_style();
   for (int ix = 0; ix <= ix_max; ++ix)
       {
         const Pixel_X px0 = orig.x + ix * len_X / ix_max;
         const Pixel_Y py0 = orig.y;
         const Pixel_X px1 = px0 - len_Zx;
         const Pixel_Y py1 = py0 + len_Zy;
         const  Pixel_XY P0(px0, py0);   // point on the X axis
         const  Pixel_XY P1(px1, py1);   // point along the Z axis

         if (ix == 0 || ix == ix_max)   // full line
            draw_line(cr, grid_color, 1, line_width, P0, P1);
         else
            draw_line(cr, grid_color, grid_style, line_width, P0, P1);
       }

   // lines starting on the Y-axis...
   //
   grid_style = w_props.get_gridY_style();
   for (int iy = 1; iy <= iy_max; ++iy)
       {
         const Pixel_X px0 = orig.x;
         const Pixel_Y py0 = orig.y - iy * len_Y / iy_max;
         const Pixel_X px1 = px0 - len_Zx;
         const Pixel_Y py1 = py0 + len_Zy;
         const  Pixel_XY P0(px0, py0);   // point on the Y axis
         const  Pixel_XY P1(px1, py1);   // point along the Z axis

         if (iy == iy_max)   // full line
            draw_line(cr, grid_color, 1, line_width, P0, P1);
         else
            draw_line(cr, grid_color, grid_style, line_width, P0, P1);
       }

   if (w_props.get_axisZ_arrow())
      {
        const Pixel_XY origin = w_props.get_origin(true);
        const Pixel_XY P(orig.x - len_Zx, orig.y + len_Zy);
        draw_arrow(cr, origin, P, grid_color);

        const string arrow_label = w_props.get_axisZ_label();
        if (arrow_label.size())
           {
             double cc_width, cc_height;
             gdip_string_size(cc_width, cc_height, cr, arrow_label.c_str(),
                              FONT_SIZE);
             draw_text(cr, arrow_label.c_str(),
                       Pixel_XY(P.x - 30 - 0.5*cc_width, P.y + 35));
           }
      }
}
//════════════════════════════════════════════════════════════════════════════
/// draw normal (2D) data lines
static void
draw_plot_lines(Gdiplus::Graphics * cr, const WIN32_context & pctx)
{
const Plot_window_properties & w_props = pctx.w_props;
const Plot_data & data = w_props.get_plot_data();
const Plot_line_properties * const * l_props = w_props.get_line_properties();

   loop(l, data.get_row_count())
       {
         const Plot_line_properties & lp = *l_props[l];
         const Color line_color = lp.get_line_color();
         const int line_style   = lp.get_line_style();
         const int line_width   = lp.get_line_width();

         // draw lines between points
         //
         Pixel_XY last(0, 0);
         loop(n, data[l].get_N())
             {
               double vx, vy;   data.get_XY(vx, vy, l, n);

               const Pixel_XY P(w_props.valX2pixel(vx - w_props.get_min_X())
                                + w_props.get_origin_X(),
                                w_props.valY2pixel(vy - w_props.get_min_Y()));
               if (n)   // unless starting point
                  draw_line(cr, line_color, line_style, line_width, last, P);
               last = P;
             }

         // draw points...
         //
         loop(n, data[l].get_N())
             {
               double vx, vy;   data.get_XY(vx, vy, l, n);
               const Pixel_X px = w_props.valX2pixel(vx - w_props.get_min_X())
                                + w_props.get_origin_X();
               const Pixel_Y py = w_props.valY2pixel(vy - w_props.get_min_Y());
               draw_point(cr, Pixel_XY(px, py),
                              lp.get_point_style(), lp.get_point_color(),
                              lp.get_point_size(), w_props.get_canvas_color(),
                              lp.get_point_size2());
             }
       }
}
//════════════════════════════════════════════════════════════════════════════
/// draw surface (3D) data lines
static void
draw_surface_lines(Gdiplus::Graphics * cr, const WIN32_context & pctx)
{
const Plot_window_properties & w_props = pctx.w_props;
const Plot_data & data = w_props.get_plot_data();
const Plot_line_properties * const * l_props = w_props.get_line_properties();
const Plot_line_properties & lp0 = *l_props[0];

const Color canvas_color = w_props.get_canvas_color();
const Color point_color  = lp0.get_point_color();
const Color line_color   = lp0.get_line_color();
const int line_width     = lp0.get_line_width();

   // 1. draw areas between plot lines...
   //
const double Hmin = w_props.get_min_Y();
const double dH = w_props.get_max_Y() - Hmin;   // 100%
const int verbosity = w_props.get_verbosity();

   loop(row, data.get_row_count() - 1)
   loop(col, data[row].get_N() - 1)
       {
         if (verbosity & SHOW_DATA)
            CERR << "B[" << row << ";" << col << "]"
                 << " X=" << data.get_X(row, col)
                 << " Y=" << data.get_Y(row, col)
                 << " Z=" << data.get_Z(row, col) << endl;

         if (!w_props.get_gradient().size())   continue; // no gradient

         const double   X0 = data.get_X(row, col);
         const double   Y0 = data.get_Y(row, col);
         const double   Z0 = data.get_Z(row, col);
         const double   H0 = (Y0 - Hmin)/dH;
         const Pixel_XY P0 = w_props.valXYZ2pixelXY(X0, Y0, Z0);

         const double   X1 = data.get_X(row, col + 1);
         const double   Y1 = data.get_Y(row, col + 1);
         const double   Z1 = data.get_Z(row, col + 1);
         const double   H1 = (Y1 - Hmin)/dH;
         const Pixel_XY P1 = w_props.valXYZ2pixelXY(X1, Y1, Z1);

         const double   X2 = data.get_X(row + 1, col);
         const double   Y2 = data.get_Y(row + 1, col);
         const double   Z2 = data.get_Z(row + 1, col);
         const double   H2 = (Y2 - Hmin)/dH;
         const Pixel_XY P2 = w_props.valXYZ2pixelXY(X2, Y2, Z2);

         const double   X3 = data.get_X(row + 1, col + 1);
         const double   Y3 = data.get_Y(row + 1, col + 1);
         const double   Z3 = data.get_Z(row + 1, col + 1);
         const double   H3 = (Y3 - Hmin)/dH;
         const Pixel_XY P3 = w_props.valXYZ2pixelXY(X3, Y3, Z3);

         /* the surface has 4 points P0, P1, P2, and P3, but they are not
            necessarily coplanar. Fold the surface at its shorter edge into
            2 triangles that have the shorter edge in common (triangles
            are always coplanar).
         */
         if (P0.distance2(P3) < P1.distance2(P2))   // P0-P3 is shorter
            {
              draw_triangle(cr, pctx, verbosity, P1, H1, P0, H0, P3, H3);
              draw_triangle(cr, pctx, verbosity, P2, H2, P0, H0, P3, H3);
            }
         else                                       // P1-P2 is shorter
            {
              draw_triangle(cr, pctx, verbosity, P0, H0, P1, H1, P2, H2);
              draw_triangle(cr, pctx, verbosity, P3, H3, P1, H1, P2, H2);
            }
       }

   // 2. draw lines between plot points
   //
   loop(row, data.get_row_count() - 1)
   loop(col, data[row].get_N() - 1)
       {
         if (verbosity & SHOW_DATA)
            CERR << "data[" << row << "," << col << "]"
                 << " X=" << data.get_X(row, col)
                 << " Y=" << data.get_Y(row, col)
                 << " Z=" << data.get_Z(row, col) << endl;

         const double X0 = data.get_X(row, col);
         const double Y0 = data.get_Y(row, col);
         const double Z0 = data.get_Z(row, col);
         const Pixel_XY P0 = w_props.valXYZ2pixelXY(X0, Y0, Z0);

         const double X1 = data.get_X(row, col + 1);
         const double Y1 = data.get_Y(row, col + 1);
         const double Z1 = data.get_Z(row, col + 1);
         const Pixel_XY P1 = w_props.valXYZ2pixelXY(X1, Y1, Z1);
         const double X2 = data.get_X(row + 1, col);
         const double Y2 = data.get_Y(row + 1, col);
         const double Z2 = data.get_Z(row + 1, col);
         const Pixel_XY P2 = w_props.valXYZ2pixelXY(X2, Y2, Z2);

         const double X3 = data.get_X(row + 1, col + 1);
         const double Y3 = data.get_Y(row + 1, col + 1);
         const double Z3 = data.get_Z(row + 1, col + 1);
         const Pixel_XY P3 = w_props.valXYZ2pixelXY(X3, Y3, Z3);

         draw_line(cr, line_color, 1, line_width, P0, P1);
         if (row == (data.get_row_count() - 2))   // last row
            draw_line(cr, line_color, 1, line_width, P2, P3);

         draw_line(cr, line_color, 1, line_width, P0, P2);
         if (col == (data[row].get_N() - 2))   // last column
            draw_line(cr, line_color, 1, line_width, P1, P3);
       }

  // 3. draw plot points
  //
const int point_style  = lp0.get_point_style();

  loop(row, data.get_row_count())
  loop(col, data[row].get_N())
      {
        const Pixel_XY P0 = w_props.valXYZ2pixelXY(data.get_X(row, col),
                                                   data.get_Y(row, col),
                                                   data.get_Z(row, col));
        draw_point(cr, P0, point_style, point_color, 2, canvas_color, 0);
      }
}
//════════════════════════════════════════════════════════════════════════════
/// plot the data into pctx's off-screen back-buffer
static void
do_plot(WIN32_context & pctx)
{
Gdiplus::Graphics * cr = pctx.graphics;
const Plot_window_properties & w_props = pctx.w_props;
const Color canvas_color = w_props.get_canvas_color();

Gdiplus::SolidBrush bg_brush(gdiplus_color(canvas_color));
   cr->FillRectangle(&bg_brush, 0.0f, 0.0f,
                     Gdiplus::REAL(pctx.get_total_width()),
                     Gdiplus::REAL(pctx.get_total_height()));

   // draw grid lines...
   //
const bool surface_plot = w_props.get_plot_data().is_surface_plot();

   if (w_props.get_gridX_variable())   draw_X_vargrid(cr, pctx, surface_plot);
   else                                draw_X_grid(cr, pctx, surface_plot);
  draw_Y_grid(cr, pctx, surface_plot);
  if (surface_plot)    draw_Z_grid(cr, pctx);

  if  (surface_plot)   draw_surface_lines(cr, pctx);
  else                 draw_plot_lines(cr, pctx);

  // draw legend...
  //
  draw_legend(cr, pctx, surface_plot);
}
//════════════════════════════════════════════════════════════════════════════
/// look up the CLSID of the PNG image encoder (standard GDI+ idiom; PNG
/// has no fixed well-known CLSID, it must be looked up by MIME type)
static bool
get_png_encoder_clsid(CLSID & clsid)
{
UINT num = 0, size = 0;
   Gdiplus::GetImageEncodersSize(&num, &size);
   if (size == 0)   return false;

Gdiplus::ImageCodecInfo * info =
      static_cast<Gdiplus::ImageCodecInfo *>(malloc(size));
   if (info == 0)   return false;

   Gdiplus::GetImageEncoders(num, size, info);
   for (UINT j = 0; j < num; ++j)
       {
         if (wcscmp(info[j].MimeType, L"image/png") == 0)
            {
              clsid = info[j].Clsid;
              free(info);
              return true;
            }
       }
   free(info);
   return false;
}
//════════════════════════════════════════════════════════════════════════════
void
WIN32_context::save_to_file_no_border()
{
UTF8_string fname(w_props.get_output_filename().c_str());
   if (fname.size() == 0)   return;   // no output_filename: don't save

   if (!fname.ends_with(".png"))   fname << ".png";

std::wstring wfname = utf8_to_wide(std::string(fname.c_str()));

CLSID pngClsid;
   if (!get_png_encoder_clsid(pngClsid))
      {
        CERR << "*** writing output file: no PNG encoder available" << endl;
        return;
      }

Gdiplus::Bitmap bmp(memBitmap, 0);
Gdiplus::Status stat = bmp.Save(wfname.c_str(), &pngClsid, 0);
   if (stat == Gdiplus::Ok)
      {
        Quad_PLOT::get_verbosity() && CERR << "wrote output file: "
                                           << fname << endl;
      }
   else
      {
        CERR << "*** writing output file: '" << fname << "' failed "
                "(GDI+ status " << int(stat) << ")" << endl;
      }
}
//────────────────────────────────────────────────────────────────────────────
void
WIN32_context::save_to_file_with_border()
{
   /* Unlike GTK/X11, capturing a fully-decorated window on Windows needs
      no wait for the window manager: PrintWindow(..., PW_RENDERFULLCONTENT)
      captures the window (including title bar and border) synchronously,
      right now, while it is still visible.
    */
UTF8_string fname(w_props.get_output_filename().c_str());
   if (fname.size() == 0)   return;   // no output_filename: don't save

   if (!fname.ends_with(".png"))   fname << ".png";

std::wstring wfname = utf8_to_wide(std::string(fname.c_str()));

RECT wr;
   GetWindowRect(hwnd, &wr);
const int w = wr.right - wr.left;
const int h = wr.bottom - wr.top;
   if (w < 1 || h < 1)   return;

HDC screenDC = GetDC(0);
HDC captureDC = CreateCompatibleDC(screenDC);
HBITMAP captureBitmap = CreateCompatibleBitmap(screenDC, w, h);
HBITMAP old = static_cast<HBITMAP>(SelectObject(captureDC, captureBitmap));
   ReleaseDC(0, screenDC);

   PrintWindow(hwnd, captureDC, PW_RENDERFULLCONTENT);

CLSID pngClsid;
   if (get_png_encoder_clsid(pngClsid))
      {
        Gdiplus::Bitmap bmp(captureBitmap, 0);
        Gdiplus::Status stat = bmp.Save(wfname.c_str(), &pngClsid, 0);
        if (stat == Gdiplus::Ok)
           Quad_PLOT::get_verbosity() && CERR << "wrote output file: "
                                              << fname << endl;
        else
           CERR << "*** writing output file: '" << fname << "' failed "
                   "(GDI+ status " << int(stat) << ")" << endl;
      }

   SelectObject(captureDC, old);
   DeleteObject(captureBitmap);
   DeleteDC(captureDC);
}
//════════════════════════════════════════════════════════════════════════════
/// redraw pctx's off-screen back-buffer and ask Windows to repaint it
static void
redraw(WIN32_context * pctx)
{
   do_plot(*pctx);
   if (pctx->hwnd)   InvalidateRect(pctx->hwnd, 0, FALSE);
}
//════════════════════════════════════════════════════════════════════════════
/// the window procedure for all plot windows. The WIN32_context for a
/// window is stashed in its GWLP_USERDATA on WM_NCCREATE and retrieved
/// on every subsequent message.
static LRESULT CALLBACK
Plot_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   if (msg == WM_NCCREATE)
      {
        CREATESTRUCTW * cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
      }

WIN32_context * pctx = reinterpret_cast<WIN32_context *>(
                          GetWindowLongPtrW(hwnd, GWLP_USERDATA));
   if (pctx == 0)   return DefWindowProcW(hwnd, msg, wParam, lParam);

   switch (msg)
      {
        case WM_PAINT:
           {
             PAINTSTRUCT ps;
             HDC hdc = BeginPaint(hwnd, &ps);
             if (pctx->memDC)
                {
                  RECT rc;   GetClientRect(hwnd, &rc);
                  BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                        pctx->memDC, 0, 0, SRCCOPY);
                }
             EndPaint(hwnd, &ps);
             return 0;
           }

        case WM_SIZE:
           {
             const int width  = LOWORD(lParam);
             const int height = HIWORD(lParam);
             if (width > 0 && height > 0)
                {
                  pctx->w_props.set_window_size(width, height);
                  pctx->resize_backbuffer(width, height);
                  redraw(pctx);
                }
             return 0;
           }

        case WM_LBUTTONDOWN:
           {
             const int x = GET_X_LPARAM(lParam);
             const int y = GET_Y_LPARAM(lParam);
             if (x >= pctx->get_legend_X0(LOC) && x < pctx->get_legend_X1(LOC)
              && y >= pctx->get_legend_Y0(LOC) && y < pctx->get_legend_Y1(LOC))
                {
                  pctx->legend_drag = true;
                  pctx->legend_drag_X = x;
                  pctx->legend_drag_Y = y;
                  SetCapture(hwnd);   // keep receiving WM_MOUSEMOVE outside
                }
             return 0;
           }

        case WM_LBUTTONUP:
             pctx->legend_drag = false;
             pctx->legend_drag_X = 0;
             pctx->legend_drag_Y = 0;
             ReleaseCapture();
             return 0;

        case WM_MOUSEMOVE:
           {
             if (!pctx->legend_drag)   return 0;
             const int x = GET_X_LPARAM(lParam);
             const int y = GET_Y_LPARAM(lParam);
             const int dx = x - pctx->legend_drag_X;
             const int dy = y - pctx->legend_drag_Y;
             if (dx || dy)
                {
                  pctx->w_props.move_legend(dx, dy);
                  redraw(pctx);
                }
             pctx->legend_drag_X = x;
             pctx->legend_drag_Y = y;
             return 0;
           }

        case WM_CLOSE:
             // window is still fully visible at this point -- the natural
             // (and, unlike GTK/X11, synchronous) place to capture it
             // with its decorations if the user asked for that.
             //
             if (pctx->w_props.get_with_border())
                pctx->save_to_file_with_border();
             DestroyWindow(hwnd);
             return 0;

        case WM_DESTROY:
             if (Quad_PLOT::get_verbosity() & SHOW_EVENTS)
                CERR << "PLOT DESTROYED" << endl;
             Quad_PLOT::PLOT_context::remove_handle(pctx->handle);
             SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
             pctx->hwnd = 0;
             delete pctx;
             PostQuitMessage(0);
             return 0;
      }

   return DefWindowProcW(hwnd, msg, wParam, lParam);
}
//════════════════════════════════════════════════════════════════════════════
/// arguments passed from plot_main_WIN32() (interpreter thread) to the
/// per-window worker pthread
struct Win32_thread_args
{
   Plot_window_properties * w_props;
   Quad_PLOT::Handle handle;
};
//────────────────────────────────────────────────────────────────────────────
/// per-window worker thread: registers the window class (once), creates
/// the HWND, renders the first frame, shows the window, posts
/// expose_sema, then runs this window's own message loop until
/// WM_DESTROY posts WM_QUIT.
static void *
plot_window_thread(void * varg)
{
Win32_thread_args * arg = static_cast<Win32_thread_args *>(varg);
Plot_window_properties & w_props = *arg->w_props;
const Quad_PLOT::Handle handle = arg->handle;
   delete arg;

HINSTANCE hInstance = GetModuleHandleW(0);

static bool class_registered = false;
   if (!class_registered)
      {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = Plot_WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursorW(0, IDC_ARROW);
        wc.hbrBackground = 0;   // we paint the whole client area ourselves
        wc.lpszClassName = PLOT_WNDCLASS;
        RegisterClassExW(&wc);
        class_registered = true;
      }

WIN32_context * pctx = new WIN32_context(w_props, handle);
   Assert(pctx);
   Quad_PLOT::all_PLOT_windows.push_back(pctx);

std::wstring title;
   if (w_props.get_user_caption())   // the user has dictated the caption
      {
        title = utf8_to_wide(w_props.get_caption());
      }
   else   // default caption ("⎕PLOT": add the handle
      {
        char cc[50];
        SPRINTF(cc, "%s-WIN32 %d", w_props.get_caption().c_str(), handle);
        title = utf8_to_wide(cc);
      }

const int width  = pctx->get_total_width();
const int height = pctx->get_total_height();

int pos_x, pos_y;
   if (w_props.get_user_pw_pos())   // the user has dictated pw_pos
      {
        pos_x = w_props.get_pw_pos_X();
        pos_y = w_props.get_pw_pos_Y();
      }
   else                             // the default pw_pos
      {
        // display the windows at different positions so that the user
        // can see their captions (and their close buttons).
        pos_x = w_props.get_pw_pos_X() + 50*(handle - 1);
        pos_y = w_props.get_pw_pos_Y() + 50*(handle - 1);
      }

   // WS_EX_TOPMOST: stay on top even though not focused (like GTK's
   // gtk_window_set_keep_above()).
   //
HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, PLOT_WNDCLASS, title.c_str(),
                            WS_OVERLAPPEDWINDOW,
                            pos_x, pos_y, width, height,
                            0, 0, hInstance, pctx);
   Assert(hwnd);
   pctx->hwnd = hwnd;

   // the window's client area may differ slightly from the requested
   // outer size (title bar/borders); resize to make the *client* area
   // exactly width×height, matching GTK's gtk_widget_set_size_request()
   // on the drawing_area (i.e. the plot content itself, not the window).
   //
   {
     RECT wanted = { 0, 0, width, height };
     AdjustWindowRectEx(&wanted, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_TOPMOST);
     SetWindowPos(hwnd, 0, pos_x, pos_y,
                 wanted.right - wanted.left, wanted.bottom - wanted.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
   }

   pctx->resize_backbuffer(width, height);
   do_plot(*pctx);   // render the first frame into the back-buffer

   if (!w_props.get_with_border())   pctx->save_to_file_no_border();

   // SW_SHOWNOACTIVATE: do not steal the keyboard focus (which annoys
   // when working interactively), matching GTK's
   // gtk_window_set_focus_on_map(false).
   //
   ShowWindow(hwnd, SW_SHOWNOACTIVATE);
   UpdateWindow(hwnd);

   sem_post(Quad_PLOT::expose_sema);   // unleash the APL interpreter

MSG msg;
   while (GetMessageW(&msg, 0, 0, 0) > 0)
      {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }

   return 0;
}
//════════════════════════════════════════════════════════════════════════════
void
Quad_PLOT::plot_main_WIN32(void * vp_props, Handle handle)
{
Plot_window_properties & w_props =
      *reinterpret_cast<Plot_window_properties *>(vp_props);
   verbosity = w_props.get_verbosity();

static bool gdiplus_started = false;
   if (!gdiplus_started)
      {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        const Gdiplus::Status st =
              Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, 0);
        if (st != Gdiplus::Ok)
           CERR << "⎕PLOT: GdiplusStartup() failed (status " << int(st)
                << ")" << endl;

        // Go straight for the generic sans-serif family (a well-known,
        // always-available GDI+ object) rather than searching for a
        // specific font by name (e.g. "Segoe UI") -- a by-name search
        // has to enumerate every installed font to match it, which is
        // needlessly expensive (and, at least under some Wine/font-
        // fallback configurations with many exotic script fonts
        // installed, pathologically slow) for what is only a cosmetic
        // preference.
        //
        if (const Gdiplus::FontFamily * generic =
                  Gdiplus::FontFamily::GenericSansSerif())
           plot_font_family = generic->Clone();
        if (plot_font_family == 0 ||
            plot_font_family->GetLastStatus() != Gdiplus::Ok)
           {
             delete plot_font_family;
             plot_font_family = new Gdiplus::FontFamily(L"Arial");
           }
        gdiplus_started = true;
      }

Win32_thread_args * arg = new Win32_thread_args;
   arg->w_props = &w_props;
   arg->handle = handle;

pthread_t thread = 0;
   pthread_create(&thread, 0, plot_window_thread, arg);

# if HAVE_PTHREAD_SETNAME_NP
   pthread_setname_np(thread, "apl/WIN32-PLOT");
# endif
}
//════════════════════════════════════════════════════════════════════════════
#endif // MINGW_SRC
