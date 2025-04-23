#include "sstv_encoder.h"

#include <cmath>

c_sstv_encoder :: c_sstv_encoder()
{
    m_Fs_kHz = 15;
    m_phase = 0;
    m_residue_f16 = 0;

    for(uint16_t idx=0; idx<1024; ++idx)
    {
      m_sin_table[idx] = round(32767.0f*sin(2.0f*idx*M_PI/1024.0f));
    }
}

void c_sstv_encoder :: output_samples(uint32_t frequency, uint16_t samples)
{
    uint32_t step = (static_cast<uint64_t>(frequency)<<32)/(m_Fs_kHz*1000);
    for(uint16_t idx = 0; idx < samples; ++idx)
    {
      output_sample(m_sin_table[m_phase >> 22]);
      m_phase += step;
    }
}

void c_sstv_encoder :: generate_tone(uint16_t frequency, uint32_t time_ms_f16)
{
    uint32_t samples_exact_f16 = (m_Fs_kHz*time_ms_f16) + m_residue_f16;
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
  //y = int(y*h/height)
  //x = int(x*w/width)
  //pixel=im[y][x][colour] 
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
    

void c_sstv_encoder :: generate_sstv(e_sstv_tx_mode mode, uint16_t width, uint16_t height)
{

  generate_tone(1900, 300 << 16);
  generate_tone(1200, 10 << 16);
  generate_tone(1900, 300 << 16);
  generate_vis_code(mode, width, height);

  if (mode == martin) generate_martin(width, height);
  else generate_scottie(width, height);

}
