
#include <stdio.h>

#include "Plot_data.hh"
#include "Plot_line_properties.hh"
#include "Plot_window_properties.hh"
#include "Unicode.hh"

/// basic colors (1 bit RGB)
enum VT100_color
{
  VT100_NONE    = -1,
  VT100_black   =  0,
  VT100_red     =  1,
  VT100_green   =  2,
  VT100_yellow  =  3,
  VT100_blue    =  4,
  VT100_magenta =  5,
  VT100_cyan    =  6,
  VT100_white   =  7,
};
//----------------------------------------------------------------------------
/// convert a 32-bit rgb value to its nearest VT100 color

const uint32_t VT100_RGB[8] =
{
  0x000000,   // VT100_black
  0xFF0000,   // VT100_red
  0x00FF00,   // VT100_green
  0xFFFF00,   // VT100_yellow
  0x0000FF,   // VT100_blue
  0xFF00FF,   // VT100_magenta
  0x00FFFF,   // VT100_cyan
  0xFFFFFF,   // VT100_white
};

/// return the VT100 color index that is closest to the RGB value \b tgb
VT100_color
round_color(uint32_t rgb)
{
VT100_color best_idx = VT100_black;
uint32_t best_sq = 0x40000;   // > 3*FF*FF
   loop(j, 8)
      {
        const uint32_t RGB = VT100_RGB[j];
        const uint32_t R = RGB >> 16 & 0xFF;
        const uint32_t G = RGB >>  8 & 0xFF;
        const uint32_t B = RGB >>  0 & 0xFF;
        const uint32_t r = rgb >> 16 & 0xFF;
        const uint32_t g = rgb >>  8 & 0xFF;
        const uint32_t b = rgb >>  0 & 0xFF;
        const uint32_t R_r = R - r;
        const uint32_t G_g = G - g;
        const uint32_t B_b = B - b;
        const uint32_t sq = R_r*R_r + G_g*G_g + B_b*B_b;
        if (best_sq > sq)
           {
             best_idx = VT100_color(j);
             best_sq = sq;
           }
      }

   return best_idx;
};
//----------------------------------------------------------------------------
struct ASCII_Point
{
   ASCII_Point()
   : uni(UNI_SPACE),          // blank
         color(VT100_black)   // black
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
        { delete points; }

   void draw_grid(const Plot_window_properties & w_props);

   void draw_plot_line(const Plot_window_properties & w_props, int row,
                       const Plot_data & data);

   void emit(ostream & out);

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
VT100_color color = round_color(w_props.get_gridX_color());
Unicode u1;
   switch(w_props.get_gridX_style())
      {
        case 0:   return;   // no grid
        case 1:  u1 = Unicode(0x2500);   break;   // │
        case 2:  u1 = Unicode(0x2504);   break;   // 
        default: u1 = Unicode(0x2500);   break;   // │
      }

const int W1 = W - 1;
const int H1 = H - 1;
   for (int y = 3; y < H1; y += 4)
       {
         set_point( 0, y, Unicode(0x255F), color);   // ╟
         set_point(W1, y, Unicode(0x2562), color);   // ╢
         for (int x = 1; x < W1; ++x)
             set_point(x, y, u1, color);
       }
}
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_vertical_grid(const Plot_window_properties & w_props)
{
   // draw vertical lines along the X axis
   //
VT100_color color = round_color(w_props.get_gridY_color());

Unicode u1, u2;
   switch(w_props.get_gridX_style())
      {
        case 0:   return;   // no grid
        case 1:  u1 = Unicode(0x2502);   u2 = Unicode(0x253C);   break;   // │ ┼
        case 2:  u1 = Unicode(0x2506);   u2 = Unicode(0x253C);   break;   // ┆ ┼
        default: u1 = Unicode(0x2502);   u2 = Unicode(0x253C);   break;   // │ ┼
      }

const int W1 = W - 1;
const int H1 = H - 1;
   for (int x = 9; x < W1; x += 10)
       {
         set_point(x,  0, Unicode(0x2564), VT100_black);   // ╤ 
         set_point(x, H1, Unicode(0x2567), VT100_black);   // ╧
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
   // draw outer frame
   //
   loop(x, W)
       {
         set_point(x,  0,  Unicode(0x2550), VT100_black);   // ═
         set_point(x, H-1, Unicode(0x2550), VT100_black);   // ═
       }

   loop(y, H)
       {
         set_point( 0,  y, Unicode(0x2551), VT100_black);   // ║
         set_point(W-1, y, Unicode(0x2551), VT100_black);   // ║
       }

   set_point( 0,   0,  Unicode(0x2554), VT100_black);
   set_point(W-1,  0,  Unicode(0x2557), VT100_black);
   set_point( 0,  H-1, Unicode(0x255A), VT100_black);
   set_point(W-1, H-1, Unicode(0x255D), VT100_black);

   draw_horizontal_grid(w_props);
   draw_vertical_grid(w_props);
}
//----------------------------------------------------------------------------
void
ASCII_canvas::draw_plot_line(const Plot_window_properties & w_props, int row,
                             const Plot_data & data)
{
Plot_line_properties const * const * l_props = w_props.get_line_properties();
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

Unicode POINT = Unicode(0x25CF);   // ●
   switch(lp.get_point_style())
      {
        case 1:  POINT = Unicode(0x25CF);   break;   // ●
        case 2:  POINT = Unicode(0x25B2);   break;   // ▲
        case 3:  POINT = Unicode(0x25BC);   break;   // ▼
        case 4:  POINT = Unicode(0x25C6);   break;   // ◆
        case 5:  POINT = Unicode(0x25A0);   break;   // ■
        case 6:  POINT = Unicode(0x2B);     break;   // +
        case 7:  POINT = Unicode(0xD7);     break;   // ×
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
ASCII_canvas::emit(ostream & out)
{

                  //   [0]   [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]
                  //                             FG             BG
char vt100_color[] = { 0x1B, '[', '0', ';', '3', '0', ';', '4', '8', 'm', 0 };
char & foreground = vt100_color[5];
VT100_color color = VT100_NONE;
   loop(h, H)
       {
         loop(w, W)
             {
               const ASCII_Point & point = get_point(w, h);
               if (point.uni != UNI_SPACE && color != point.color)
                  {
                    foreground = '0' + point.color;
                    // fprintf(stdout, "%s", vt100_color);
                    cerr << vt100_color;
                    color = point.color;
                  }
               cerr << point.uni ;
             }
         cerr << "\n";
       }
   Output::set_color_mode(Output::COLM_OUTPUT);
   Output::set_color_mode(Output::COLM_INPUT);
}
//----------------------------------------------------------------------------
void
do_plot_ASCII(const Plot_window_properties & w_props, const Plot_data & data)
{
   // usable area
   //
const int H = 24;
const int W = 80;
//ASCII_Point canvas[H*W];

ASCII_canvas ctx(H, W);

   ctx.draw_grid(w_props);

   loop(row, data.get_row_count())
       {
         ctx.draw_plot_line(w_props, row, data);
       }

   // output
   //
   ctx.emit(cerr);
}
//----------------------------------------------------------------------------
