//  _  ___  _   _____ _     _
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/
//
// Copyright (c) Jonathan P Dawson 2025
// filename: sstv_encoder.cpp
// description: class to encode sstv from image
// License: MIT
//

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

void c_sstv_encoder :: generate_vis_code(e_sstv_tx_mode mode)
{
  uint8_t vis = 0u;
  switch(mode)
  {
    case tx_PD_50: vis = 93; break;
    case tx_PD_90: vis = 94; break;
    case tx_PD_120: vis = 95; break;
    case tx_PD_180: vis = 97; break;
    case tx_martin_m1: vis = 44; break;
    case tx_martin_m2: vis = 45; break;
    case tx_scottie_s1: vis = 60; break;
    case tx_scottie_s2: vis = 61; break;
	case tx_robot_36: vis = 8; break;
	case tx_robot_72: vis = 12; break;
	case tx_bw_8: vis = 2; break;
	case tx_bw_12: vis = 6; break;
	case tx_bw_24: vis = 10; break;
	case tx_bw_36: vis = 14; break;
  }
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

void c_sstv_encoder :: generate_scottie(e_sstv_tx_mode mode)
{
  uint16_t width, height;
  float colour_time_ms;

  switch(mode)
  {
    case tx_scottie_s1:
      width = 320;
      height = 240;
      colour_time_ms = 138.240;
      break;

    case tx_scottie_s2:
      width = 320;
      height = 240;
      colour_time_ms = 88.064;
      break;

    default: return;
  }

  uint32_t hsync_pulse_ms_f16 = 9.0f * (1<<16);
  uint32_t colour_gap_ms_f16 = 1.5f * (1<<16);
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; ++row)
  {
	draw_progress_bar(row,height);
	
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

    if(m_abort) return;
  }
}

void c_sstv_encoder :: generate_martin(e_sstv_tx_mode mode)
{

  uint16_t width, height;
  float colour_time_ms;

  switch(mode)
  {
    case tx_martin_m1:
      width = 320;
      height = 240;
      colour_time_ms = 146.320;
      break;

    case tx_martin_m2:
      width = 320;
      height = 240;
      colour_time_ms = 73.216;
      break;

    default: return;
  }

  uint32_t hsync_pulse_ms_f16 = 4.862 * (1<<16);
  uint32_t colour_gap_ms_f16 = 0.572 * (1<<16);
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; ++row)
  {
	draw_progress_bar(row,height);
	
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

    if(m_abort) return;
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

void c_sstv_encoder :: generate_pd(e_sstv_tx_mode mode)
{
  uint16_t width, height;
  float colour_time_ms;

  switch(mode)
  {
    case tx_PD_50:
      width = 320;
      height = 240;
      colour_time_ms = 91.520;
      break;

    case tx_PD_90:
      width = 320;
      height = 240;
      colour_time_ms = 170.240;
      break;

    case tx_PD_120:
      width = 640;
      height = 480;
      colour_time_ms = 121.600;
      break;

    case tx_PD_180:
      width = 640;
      height = 480;
      colour_time_ms = 183.040;
      break;

    default: return;
  }
  uint32_t hsync_pulse_ms_f16 = 20.0 * (1<<16);
  uint32_t colour_gap_ms_f16 = 2.08 * (1<<16);
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; row+=2)
  {
    uint8_t row_y[width];
    uint8_t row_cb[width];
    uint8_t row_cr[width];

	draw_progress_bar(row,height);
	
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

    if(m_abort) return;
  }
}

void c_sstv_encoder :: generate_robot(e_sstv_tx_mode mode)
{
  uint16_t width, height;
  float colour_time_ms;
  float hsync_pulse_ms;
  float colour_gap_ms;

  switch(mode)
  {
    case tx_robot_24:
      width = 160;
      height = 120;
      colour_time_ms = 88.0;
	    hsync_pulse_ms = 9;
	    colour_gap_ms = 6;
      break;
    case tx_robot_36:
      width = 320;
      height = 240;
      colour_time_ms = 90.0;
	    hsync_pulse_ms = 7.5;
	    colour_gap_ms = 4.5;
      break;
	  case tx_robot_72:
      width = 320;
      height = 240;
      colour_time_ms = 138.0;
	    hsync_pulse_ms = 9;
	    colour_gap_ms = 6;
      break;

    default: return;
  }
  uint32_t hsync_pulse_ms_f16 = hsync_pulse_ms * (1<<16);
  uint32_t pulse_gap_ms_f16 = 3 * (1<<16);
  uint32_t colour_gap_ms_f16 = colour_gap_ms * (1<<16);
  uint32_t pixel_time_ms_f16 = (colour_time_ms*(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; row+=2)
  {
    uint8_t row_y[width];
    uint8_t row_cb[width];
    uint8_t row_cr[width];

    draw_progress_bar(row,height);
	
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
    generate_tone(1500, pulse_gap_ms_f16);
	
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_y[col]/256), pixel_time_ms_f16);

    generate_tone(1500, colour_gap_ms_f16);
	
    for(uint16_t col=0u; col < width; ++col)
        generate_tone(1500 + ((2300-1500)*(uint16_t)row_cb[col]/256), pixel_time_ms_f16/2);

    if (mode==tx_robot_72 || mode==tx_robot_24) {
      generate_tone(2300, colour_gap_ms_f16);
    
      for(uint16_t col=0u; col < width; ++col)
        generate_tone(1500 + ((2300-1500)*(uint16_t)row_cr[col]/256), pixel_time_ms_f16/2);
    
    }

    for(uint16_t col=0u; col < width; ++col)
    {
      uint8_t r = get_image_pixel(width, height, row+1, col, 0);
      uint8_t g = get_image_pixel(width, height, row+1, col, 1);
      uint8_t b = get_image_pixel(width, height, row+1, col, 2);
      uint8_t y, cr, cb;
      rgb_to_ycrcb_fixed(r, g, b, y, cr, cb);
      row_y[col] = y;
      row_cb[col] = cr;
      row_cr[col] = cb;
    }

    generate_tone(1200, hsync_pulse_ms_f16);
    generate_tone(1500, pulse_gap_ms_f16);
	
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_y[col]/256), pixel_time_ms_f16);

    if (mode==tx_robot_72 || mode == tx_robot_24) {
      generate_tone(1500, colour_gap_ms_f16);
	
      for(uint16_t col=0u; col < width; ++col)
        generate_tone(1500 + ((2300-1500)*(uint16_t)row_cb[col]/256), pixel_time_ms_f16/2);
	
	  }

    generate_tone(2300, colour_gap_ms_f16);
    for(uint16_t col=0u; col < width; ++col)
        generate_tone(1500 + ((2300-1500)*(uint16_t)row_cr[col]/256), pixel_time_ms_f16/2);
        if(m_abort) return;
    }
}

void c_sstv_encoder :: generate_bw(e_sstv_tx_mode mode)
{
  uint16_t width, height;
  float hsync_pulse_ms;
  float scan_line_ms;

  switch(mode)
  {
    case tx_bw_8:
      width = 160;
      height = 120;
	  hsync_pulse_ms = 10;
	  scan_line_ms = 56;
	  break;
	case tx_bw_12:
      width = 160;
      height = 120;
	  hsync_pulse_ms = 7;
	  scan_line_ms = 93;
	  break;
	case tx_bw_24:
      width = 320;
      height = 240;
	  hsync_pulse_ms = 12;
	  scan_line_ms = 88;
	  break;
	case tx_bw_36:
      width = 320;
      height = 240;
	  hsync_pulse_ms = 12;
	  scan_line_ms = 138;
      break;
	

    default: return;
  }
  uint32_t hsync_pulse_ms_f16 = hsync_pulse_ms * (1<<16); 
  uint32_t scan_line_ms_f16 = scan_line_ms * (1<<16);
  uint32_t pixel_time_ms_f16 = (scan_line_ms *(1<<16))/width;

  //send rows
  for(uint16_t row=0u; row < height; row++)
  {
    uint8_t row_y[width];

	draw_progress_bar(row,height);
	
    for(uint16_t col=0u; col < width; ++col)
    {
      uint8_t r = get_image_pixel(width, height, row, col, 0);
      uint8_t g = get_image_pixel(width, height, row, col, 1);
      uint8_t b = get_image_pixel(width, height, row, col, 2);
      uint8_t y, cr, cb;
      rgb_to_ycrcb_fixed(r, g, b, y, cr, cb);
      row_y[col] = y;
      
    }

    generate_tone(1200, hsync_pulse_ms_f16);

	
    for(uint16_t col=0u; col < width; ++col)
      generate_tone(1500 + ((2300-1500)*(uint16_t)row_y[col]/256), pixel_time_ms_f16);

   
    if(m_abort) return;
  }
}


void c_sstv_encoder :: generate_sstv(e_sstv_tx_mode mode)
{
  m_abort = false;
  generate_tone(1900, 300 << 16);
  generate_tone(1200, 10 << 16);
  generate_tone(1900, 300 << 16);
  generate_vis_code(mode);

  switch(mode)
  {
    case tx_PD_50:
    case tx_PD_90:
    case tx_PD_120:
    case tx_PD_180:
      generate_pd(mode);
      break;

    case tx_martin_m1:
    case tx_martin_m2:
      generate_martin(mode);
      break;

    case tx_scottie_s1:
    case tx_scottie_s2:
      generate_scottie(mode);
      break;
	case tx_robot_36:
	case tx_robot_72:
      generate_robot(mode);
      break;
	case tx_bw_8:
	case tx_bw_12:
	case tx_bw_24:
	case tx_bw_36:
      generate_bw(mode);
      break;
  }
}

void c_sstv_encoder :: abort()
{

  m_abort = true;

}
