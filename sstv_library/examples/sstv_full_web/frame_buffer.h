#ifndef __FRAME_BUFFER_H__
#define __FRAME_BUFFER_H__

#include <cstdint>

class c_frame_buffer
{

  uint16_t m_width;
  uint16_t m_height;
  uint16_t *m_buffer;

  public:
  uint16_t colour565(uint8_t r, uint8_t g, uint8_t b);
  void colour_rgb(uint16_t colour_565, uint8_t &r, uint8_t &g, uint8_t &b);
  uint16_t colour_scale(uint16_t colour, uint16_t alpha=256);
  uint16_t alpha_blend(uint16_t old_colour, uint16_t colour, uint16_t alpha);
  void set_pixel(uint16_t x, uint16_t y, uint16_t colour, uint16_t alpha=256);
  void draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t colour, uint16_t alpha=256);
  void draw_line_antialiased(int x0, int y0, int x1, int y1, uint16_t colour, uint16_t alpha=256); 
  void fill_circle(uint16_t xc, uint16_t yc, uint16_t radius, uint16_t colour, uint16_t alpha=256);
  void draw_circle(uint16_t xc, uint16_t yc, uint16_t radius, uint16_t colour, uint16_t alpha=256);
  void draw_string(uint16_t x, uint16_t y, const uint8_t *font, const char *s, uint16_t fg, uint16_t alpha=256);
  void draw_char(uint16_t x, uint16_t y, const uint8_t *font, char c, uint16_t fg, uint16_t alpha=256);
  void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour, uint16_t alpha=256);
  void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour, uint16_t alpha=256);
  void draw_object(uint16_t x, uint16_t y, uint16_t r, const uint16_t* image);
  void draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* image);
  void clear(uint16_t colour);


  c_frame_buffer(uint16_t *buffer, uint16_t width, uint16_t height)
  {
    m_width = width;
    m_height = height;
    m_buffer = buffer;
  }
};

#endif
