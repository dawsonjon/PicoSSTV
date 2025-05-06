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

enum e_sstv_tx_mode {martin, scottie, pd};

class c_sstv_encoder
{

  private :
  double m_Fs_Hz;
  uint32_t m_phase;
  int16_t m_sin_table[1024];
  uint32_t m_residue_f16;
  void output_samples(uint32_t frequency, uint16_t samples);
  void generate_tone(uint16_t frequency, uint32_t time_ms_f16);
  bool calculate_parity(uint8_t number);
  void generate_vis_bit(uint8_t level);
  void generate_vis_code(e_sstv_tx_mode mode, uint16_t width, uint16_t height);
  uint16_t get_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour);
  void generate_scottie(uint16_t width, uint16_t height);
  void generate_martin(uint16_t width, uint16_t height);
  void generate_pd(uint16_t width, uint16_t height);

  //override these application specific functions
  virtual void output_sample(int16_t sample) = 0;
  virtual uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour) = 0;

  public:
  c_sstv_encoder(double fs_Hz);
  void generate_sstv(e_sstv_tx_mode, uint16_t width, uint16_t height);

};

#endif
