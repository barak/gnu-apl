
#include <stdio.h>

#include "Plot_data.hh"
#include "Plot_line_properties.hh"
#include "Plot_window_properties.hh"
#include "Unicode.hh"

/// basic colors (1 bit RGB)
enum VT100_color
{
  VT100_NONE    = -1,

  // dark colors
  //
  VT100_dark_black     =  0,
  VT100_dark_red       =  1,
  VT100_dark_green     =  2,
  VT100_dark_yellow    =  3,
  VT100_dark_blue      =  4,
  VT100_dark_magenta   =  5,
  VT100_dark_cyan      =  6,
  VT100_dark_white     =  7,

  // bright colors. Dark foreground is 3x while bright foreground is 9x.
  // Dark background is 4x while bright background is 10x for color x.
  // Therefore bright vcolors are black colors + 60.
  //
  VT100_bright_black   =  60,
  VT100_bright_red     =  61,
  VT100_bright_green   =  62,
  VT100_bright_yellow  =  63,
  VT100_bright_blue    =  64,
  VT100_bright_magenta =  65,
  VT100_bright_cyan    =  66,
  VT100_bright_white   =  67,
};
//----------------------------------------------------------------------------
/// convert a 32-bit rgb value to its nearest VT100 color

const struct
{
  VT100_color vt100;
  int r, g, b;
} VT100_RGB[16] =
{
  { VT100_dark_black    ,    0,   0,   0 },
  { VT100_dark_red      ,  255,   0,   0 },
  { VT100_dark_green    ,    0, 255,   0 },
  { VT100_dark_yellow   ,  255, 255,   0 },
  { VT100_dark_blue     ,    0,   0, 255 },
  { VT100_dark_magenta  ,  255,   0, 255 },
  { VT100_dark_cyan     ,    0, 255, 255 },
  { VT100_dark_white    ,  250, 250, 250 },
                                      
  { VT100_bright_black  ,  128, 128, 128 },
  { VT100_bright_red    ,  255, 128, 128 },
  { VT100_bright_green  ,  128, 255, 128 },
  { VT100_bright_yellow ,  255, 255, 128 },
  { VT100_bright_blue   ,  128, 128, 255 },
  { VT100_bright_magenta,  255, 128, 255 },
  { VT100_bright_cyan   ,  128, 255, 255 },
  { VT100_bright_white  ,  255, 255, 255 },
};                    

VT100_color
round_color(uint32_t rgb)
{ 
VT100_color best_idx = VT100_dark_black;
uint32_t best_sq = 0x40000;   // > 3*FF*FF
   loop(j, sizeof(VT100_RGB) / sizeof(*VT100_RGB))
      {
        const uint32_t R = VT100_RGB[j].r;
        const uint32_t G = VT100_RGB[j].g;
        const uint32_t B = VT100_RGB[j].b;

        const uint32_t r = rgb >> 16 & 0xFF;
        const uint32_t g = rgb >>  8 & 0xFF;
        const uint32_t b = rgb >>  0 & 0xFF;

        const uint32_t R_r = R - r;
        const uint32_t G_g = G - g;
        const uint32_t B_b = B - b;

        const uint32_t sq = R_r*R_r + G_g*G_g + B_b*B_b;
        if (best_sq > sq)   // sq is closer
           {
             best_idx = VT100_color(j);
             best_sq = sq;
           }
      }

   return VT100_RGB[best_idx].vt100;
};
//----------------------------------------------------------------------------
struct ASCII_Point
{
   ASCII_Point()
   : uni(UNI_SPACE),          // blank
         color(VT100_dark_black)   // black
   {}

   Unicode uni;
   VT100_color color;   /// foreground color index
};
//----------------------------------------------------------------------------
class ASCII_canvas
{
public:
   ASCII_canvas(int h, int w)
    : H(h),
      W(w),
      points(new ASCII_Point[h*w + 1])
      {}

   ~ASCII_canvas()
        { delete [] points; }

   void draw_grid(const Plot_window_properties & w_props);

   void draw_plot_line(const Plot_window_properties & w_props, int row,
                       const Plot_data & data);

   /// print canvas to screen
   void emit(ostream & out, const Plot_window_properties & w_props);

   /// return canvas as APL value
   Value_P emit_value(const Plot_window_properties & w_props);

protected:
   void draw_horizontal_grid(const Plot_window_properties & w_props);
   void draw_vertical_grid(const Plot_window_properties & w_props);

   const int H;            ///< number of lines
   const int W;            ///< number of columns
   ASCII_Point * points;   /// characters and colors

   void set_point(int x, int y, Unicode uni, VT100_color color)
      {
         Assert(x >= 0);   Assert(x < W);
         Assert(y >= 0);   Assert(y < H);
         ASCII_Point & point = points[x + y * W];
         point.uni = uni;
         point.color = color;
      }

   const ASCII_Point & get_point(int x, int y) const
      { return points[x + y * W]; }
};
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_horizontal_grid(const Plot_window_properties & w_props)
{
   // draw horizontal lines along the Y axis
   //
const VT100_color color = round_color(w_props.get_gridX_color());
Unicode u1;
   switch(w_props.get_gridX_style())
      {
        case 0:   return;   // no grid
        case 1:  u1 = Unicode(U'─');   break;
        case 2:  u1 = Unicode(U'┄');   break;
        default: u1 = Unicode(U'─');   break;
      }

const int W1 = W - 1;
const int H1 = H - 1;
   for (int y = 3; y < H1; y += 4)
       {
         set_point( 0, y, Unicode(U'╟'), color);
         set_point(W1, y, Unicode(U'╢'), color);
         for (int x = 1; x < W1; ++x)   set_point(x, y, u1, color);
       }
}
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_vertical_grid(const Plot_window_properties & w_props)
{
   // draw vertical lines along the X axis
   //
const VT100_color color = round_color(w_props.get_gridY_color());

Unicode u1, u2;
   switch(w_props.get_gridX_style())
      {
        case 0:   return;   // no grid
        case 1:  u1 = Unicode(0x2502);   u2 = Unicode(U'┼');   break;
        case 2:  u1 = Unicode(0x2506);   u2 = Unicode(U'┼');   break;
        default: u1 = Unicode(0x2502);   u2 = Unicode(U'┼');   break;
      }

const int W1 = W - 1;
const int H1 = H - 1;
   for (int x = 9; x < W1; x += 10)
       {
         set_point(x,  0, Unicode(U'╤'), VT100_dark_black);
         set_point(x, H1, Unicode(U'╧'), VT100_dark_black);
         for (int y = 1; y < H1; ++y)
             {
               if (get_point(x, y).uni == UNI_SPACE)
                  set_point(x, y, u1, color);   // │
               else
                  set_point(x, y, u2, color);   // ┼
             }
       }
}
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_grid(const Plot_window_properties & w_props)
{
const VT100_color color = VT100_dark_black;
   // draw outer frame
   //
   loop(x, W)
       {
         set_point(x,  0,  Unicode(U'═'), color);   // ═
         set_point(x, H-1, Unicode(U'═'), color);   // ═
       }

   loop(y, H)
       {
         set_point( 0,  y, Unicode(U'║'), color);   // ║
         set_point(W-1, y, Unicode(U'║'), color);   // ║
       }

   set_point( 0,   0,  Unicode(U'╔'), color);
   set_point( 0,  H-1, Unicode(U'╚'), color);
   set_point(W-1,  0,  Unicode(U'╗'), color);
   set_point(W-1, H-1, Unicode(U'╝'), color);

   draw_horizontal_grid(w_props);
   draw_vertical_grid(w_props);
}
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_plot_line(const Plot_window_properties & w_props,
                             int row, const Plot_data & data)
{
const Plot_line_properties * const * l_props = w_props.get_line_properties();
const Plot_line_properties & lp = *l_props[row];

const double max_X = w_props.get_max_X();
const double min_X = w_props.get_min_X();
const double max_Y = w_props.get_max_Y();
const double min_Y = w_props.get_min_Y();

const double delta_X = max_X - min_X ;
const double delta_Y = max_Y - min_Y ;

const int H1 = H - 1;   // right frame column
const int W1 = W - 1;   // bottom frame column
const int H2 = H - 2;   // space inside the frame
const int W2 = W - 2;   // space inside the frame

Unicode POINT = Unicode(U'●');
   switch(lp.get_point_style())
      {
        case 1:  POINT = Unicode(U'●');   break;
        case 2:  POINT = Unicode(U'▲');   break;
        case 3:  POINT = Unicode(U'▼');   break;
        case 4:  POINT = Unicode(U'◆');   break;
        case 5:  POINT = Unicode(U'■');   break;
        case 6:  POINT = Unicode(U'+');   break;
        case 7:  POINT = Unicode(U'×');   break;
      }

   // scale ranges to screen size. Actually a litte less to avoud rounding
   // problems.
const double scale_X = (W2 - 0.1) / delta_X;
const double scale_Y = (H2 - 0.1) / delta_Y;

const VT100_color color = round_color(lp.get_point_color());
   loop(n, data[row].get_N())
      {
        // get the values to plot
        //
        double Vx, Vy;
        data.get_XY(Vx, Vy, row, n);

        // scale the values to the canvas size
        //
        const double  x = (Vx - min_X) * scale_X;
        const double  y = (Vy - min_Y) * scale_Y;

        Assert(x >= 0.0);   Assert(x < W2);   // x fits into frame
        Assert(y >= 0.0);   Assert(y < H2);   // y fits into frame

        // round z, round y, and mirror y
        //
        const int X = 1 + int(x);   // first after left frame border
        const int Y = 1 + int(y);   // first above  bottom frame border
        Assert(X > 0);   Assert(X < W1);   // X is inside frame
        Assert(Y > 0);   Assert(Y < H1);   // Y is inside frame
        set_point(X, H1 - Y, POINT, color);
      }
}
//----------------------------------------------------------------------------
void
ASCII_canvas::emit(ostream & out, const Plot_window_properties & w_props)
{
VT100_color bg_color = round_color(w_props.get_canvas_color());

   // reset all character attributes and set the background color
   cerr << "\x1B[0;" << (40 + bg_color) << 'm';

VT100_color color = VT100_NONE;   // to force an initial color change
   loop(h, H)
       {
         loop(w, W)
             {
               const ASCII_Point & point = get_point(w, h);
               if (point.uni != UNI_SPACE && color != point.color)
                  {
                    color = point.color;
                    cerr << "\x1B[" << (30 + color) << 'm';
                  }

               cerr << point.uni;
             }

         // end of line: switch back to COUT color
         //
         cerr << Output::color_COUT << Output::clear_EOL
              << "\n" "\x1B[" << (40 + bg_color) << 'm';
       }

   cerr << Output::color_CIN << Output::clear_EOL << "\n";
}
//----------------------------------------------------------------------------
Value_P
ASCII_canvas::emit_value(const Plot_window_properties & w_props)
{
const Shape shape_Z(3, H, W);
Value_P Z(shape_Z, LOC);

   // character plane
   //
   loop(h, H)
   loop(w, W)
       {
         const ASCII_Point & point = get_point(w, h);
         Z->next_ravel_Char(point.uni);
       }

   // vt100 foreground color plane
   //
   loop(h, H)
   loop(w, W)
       {
         const ASCII_Point & point = get_point(w, h);
         Z->next_ravel_Int(point.color);
       }

   // vt100 background color plane
   //
const VT100_color color = round_color(w_props.get_canvas_color());
   loop(h, H)
   loop(w, W)
       {
         Z->next_ravel_Int(color);
       }

   Z->check_value(LOC);
   return Z;
}
//----------------------------------------------------------------------------
Value_P
do_plot_ASCII(const Plot_window_properties & w_props, const Plot_data & data)
{
   // usable area
   //
const int H = w_props.get_terminal_rows();
const int W = w_props.get_terminal_cols();
ASCII_canvas ctx(H, W);

   ctx.draw_grid(w_props);

   loop(row, data.get_row_count())
       {
         ctx.draw_plot_line(w_props, row, data);
       }

   // output to screen
   //
   ctx.emit(cerr, w_props);
   return ctx.emit_value(w_props);
}
//----------------------------------------------------------------------------
