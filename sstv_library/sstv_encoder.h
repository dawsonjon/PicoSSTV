//  _  ___  _   _____ _     _
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/
//
// Copyright (c) Jonathan P Dawson 2025
// filename: sstv_encoder.h
// description: class to encode sstv from image
// License: MIT
//

#ifndef __SSTV_ENCODER_H__
#define __SSTV_ENCODER_H__

#include <cstdint>

enum e_sstv_tx_mode {tx_martin_m1, tx_martin_m2, tx_scottie_s1, tx_scottie_s2, tx_PD_50, tx_PD_90, tx_PD_120, tx_PD_180, tx_robot_36, tx_robot_72};

class c_sstv_encoder
{

  private :
  double m_Fs_Hz;
  uint32_t m_phase;
  int16_t m_sin_table[1024];
  uint32_t m_residue_f16;
  bool m_abort;
  void output_samples(uint32_t frequency, uint16_t samples);
  void generate_tone(uint16_t frequency, uint32_t time_ms_f16);
  bool calculate_parity(uint8_t number);
  void generate_vis_bit(uint8_t level);
  void generate_vis_code(e_sstv_tx_mode mode);
  uint16_t get_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour);
  void generate_scottie(e_sstv_tx_mode mode);
  void generate_martin(e_sstv_tx_mode mode);
  void generate_pd(e_sstv_tx_mode mode);
  void generate_robot(e_sstv_tx_mode mode);

  //override these application specific functions
  virtual void output_sample(int16_t sample) = 0;
  virtual uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour) = 0;
  virtual void draw_progress_bar(uint16_t row, uint16_t tot_height) {};

  public:
  c_sstv_encoder(double fs_Hz);
  void generate_sstv(e_sstv_tx_mode);
  void abort();
};

#endif

