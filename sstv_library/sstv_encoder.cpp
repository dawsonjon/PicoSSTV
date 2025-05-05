#include "sstv_encoder.h"

#include <cmath>

c_sstv_encoder :: c_sstv_encoder(double Fs_Hz)
{
    m_Fs_Hz = Fs_Hz;
    m_phase = 0;
    m_residue_f16 = 0;

    for(uint16_t idx=0; idx<1024; ++idx)
    {
      m_sin_table[idx] = round(32767.0f*sin(2.0f*idx*M_PI/1024.0f));
    }
}

void c_sstv_encoder :: output_samples(uint32_t frequency, uint16_t samples)
{
    uint32_t step = (static_cast<uint64_t>(frequency)<<32)/m_Fs_Hz;
    for(uint16_t idx = 0; idx < samples; ++idx)
    {
      output_sample(m_sin_table[m_phase >> 22]);
      m_phase += step;
    }
}

void c_sstv_encoder :: generate_tone(uint16_t frequency, uint32_t time_ms_f16)
{
    uint32_t samples_exact_f16 = (m_Fs_Hz*time_ms_f16/1000) + m_residue_f16;
    uint32_t samples = samples_exact_f16 >> 16u;
    m_residue_f16 = samples_exact_f16-(samples << 16u);
    output_samples(frequency, samples);
}

bool c_sstv_encoder :: calculate_parity(uint8_t number)
{
    uint8_t result = 0;
    while(number)
    {
        result ^= (number & 1);
        number >>= 1;
    }
    return result;
}

void c_sstv_encoder :: generate_vis_bit(uint8_t level)
{
    if(level) generate_tone(1100, 30<<16);
    else generate_tone(1300, 30<<16);
}

void c_sstv_encoder :: generate_vis_code(e_sstv_tx_mode mode, uint16_t width, uint16_t height)
{

  uint8_t vis = 0u;

  if(mode == martin) vis |= 0x20;
  else vis |= 0x30;
  if(width == 320) vis |= 0x4;
  if(height == 256) vis |= 0x8;

  generate_tone(1200, 30<<16);//start bit
  for(uint8_t i=0; i<8; ++i)
  {
    generate_vis_bit(vis&1);
    vis >>= 1;
  }
  generate_vis_bit(calculate_parity(vis));
  generate_tone(1200, 30<<16);//stop bit
}

uint16_t c_sstv_encoder :: get_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour)
{
  uint16_t pixel = get_image_pixel(width, height, y, x, colour);
  pixel = 1500 + ((2300-1500)*pixel/256);
  return pixel;
}

void c_sstv_encoder :: generate_scottie(uint16_t width, uint16_t height)
{

  uint32_t hsync_pulse_ms_f16 = 9.0f * (1<<16);
  uint32_t colour_gap_ms_f16 = 1.5f * (1<<16);
  float colour_time_ms = (width == 320)?138.240:88.064;
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; ++row)
  {
    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 1), pixel_time_ms_f16);

    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 2), pixel_time_ms_f16);

    generate_tone(1200, hsync_pulse_ms_f16);
    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 0), pixel_time_ms_f16);
  }
}

void c_sstv_encoder :: generate_martin(uint16_t width, uint16_t height)
{

  uint32_t hsync_pulse_ms_f16 = 4.862 * (1<<16);
  uint32_t colour_gap_ms_f16 = 0.572 * (1<<16);
  float colour_time_ms = (width == 320)?146.342:73.216;
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; ++row)
  {
    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 1), pixel_time_ms_f16);

    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 2), pixel_time_ms_f16);

    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(get_pixel(width, height, row, col, 0), pixel_time_ms_f16);

    generate_tone(1500, colour_gap_ms_f16);
    generate_tone(1200, hsync_pulse_ms_f16);
  }
}

// Clamp macro to [0, 255]
#define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

// Coefficients scaled by 256
#define Y_R  77   // 0.299 * 256
#define Y_G 150   // 0.587 * 256
#define Y_B  29   // 0.114 * 256

#define CB_R -43  // -0.168736 * 256
#define CB_G -85  // -0.331264 * 256
#define CB_B 128  //  0.5 * 256

#define CR_R 128  //  0.5 * 256
#define CR_G -107 // -0.418688 * 256
#define CR_B -21  // -0.081312 * 256

void rgb_to_ycrcb_fixed(uint8_t R, uint8_t G, uint8_t B, uint8_t &Y, uint8_t &Cr, uint8_t &Cb)
{
    int y  = (Y_R * R + Y_G * G + Y_B * B) >> 8;
    int cb = ((CB_R * R + CB_G * G + CB_B * B) >> 8) + 128;
    int cr = ((CR_R * R + CR_G * G + CR_B * B) >> 8) + 128;

    Y  = (unsigned char)CLAMP(y);
    Cb = (unsigned char)CLAMP(cb);
    Cr = (unsigned char)CLAMP(cr);
}

void c_sstv_encoder :: generate_pd(uint16_t width, uint16_t height)
{
  uint32_t hsync_pulse_ms_f16 = 20.0 * (1<<16);
  uint32_t colour_gap_ms_f16 = 2.08 * (1<<16);
  float colour_time_ms = (width == 640)?121.6:91.520; //pd120 - pd90
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; row+=2)
  {
    uint8_t row_y[width];
    uint8_t row_cb[width];
    uint8_t row_cr[width];

    for(uint16_t col=0u; col < width; ++col)
    {
      uint8_t r = get_image_pixel(width, height, row, col, 0);
      uint8_t g = get_image_pixel(width, height, row, col, 1);
      uint8_t b = get_image_pixel(width, height, row, col, 2);
      uint8_t y, cr, cb;
      rgb_to_ycrcb_fixed(r, g, b, y, cr, cb);
      row_y[col] = y;
      row_cb[col] = cr;
      row_cr[col] = cb;
    }

    generate_tone(1200, hsync_pulse_ms_f16);
    generate_tone(1500, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_y[col]/256), pixel_time_ms_f16);

    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_cb[col]/256), pixel_time_ms_f16);

    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_cr[col]/256), pixel_time_ms_f16);

    for(uint16_t col=0u; col < width; ++col)
    {
      uint8_t r = get_image_pixel(width, height, row+1, col, 0);
      uint8_t g = get_image_pixel(width, height, row+1, col, 1);
      uint8_t b = get_image_pixel(width, height, row+1, col, 2);
      uint8_t y, cr, cb;
      rgb_to_ycrcb_fixed(r, g, b, y, cr, cb);
      row_y[col] = y;
    }

    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_y[col]/256), pixel_time_ms_f16);
  }
}

void c_sstv_encoder :: generate_sstv(e_sstv_tx_mode mode, uint16_t width, uint16_t height)
{

  generate_tone(1900, 300 << 16);
  generate_tone(1200, 10 << 16);
  generate_tone(1900, 300 << 16);
  generate_vis_code(mode, width, height);


  if (mode == martin) generate_martin(width, height);
  else if (mode == scottie) generate_scottie(width, height);
  else generate_pd(width, height);

}
