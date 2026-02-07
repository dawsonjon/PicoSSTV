//  _  ___  _   _____ _     _                 
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___ 
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/    
//
// Copyright (c) Jonathan P Dawson 2026
// filename: sstv_transmit_demo.ino
// description:
//
// Transmits image in flash using SSTV.
// Connect wire to pin 1. Press bootsel to transmit!
//
// License: MIT

#include <sstv_encoder.h>
#include "transmit_nco.h"
#include "pico/multicore.h"
#include "test_card.h"

//CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

const uint8_t RF_PIN = 0;
const float RF_FREQUENCY = 14.230e6; //20M SSTV
const e_sstv_tx_mode tx_mode = tx_PD_90;
const uint16_t image_width = 320;
const uint16_t image_height = 240;

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////
uint8_t waveforms_per_sample;
double sample_rate_Hz;
queue_t data_queue;

//Derive a class from sstv encoder and override hardware specific functions
class c_sstv_encoder_nco : public c_sstv_encoder
{

  private :
    double sample_frequency_Hz;
    uint32_t m_residue_f16;
    uint32_t m_phase;

  void output_sample(int16_t sample){}

  void generate_tone(uint16_t frequency, uint32_t time_ms_f16)
  {
    
    uint32_t samples_exact_f16 = (sample_frequency_Hz*time_ms_f16/1000) + m_residue_f16;
    uint32_t samples = samples_exact_f16 >> 16u;
    m_residue_f16 = samples_exact_f16-(samples << 16u);
    uint32_t step = (static_cast<uint64_t>(frequency)<<32)/sample_frequency_Hz;
    for(uint16_t idx = 0; idx < samples; ++idx)
    {
      m_phase += step;
      const int16_t phase = m_phase >> 16;
      queue_add_blocking(&data_queue, (void*)&phase);
    }
    
  }
  
  uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour)
  {
    uint16_t image_y = (uint32_t)y * image_height / height;
    uint16_t image_x = (uint32_t)x * image_width / width;
    uint16_t pixel = test_card[image_width * image_y + image_x];
    pixel = pixel >> 8 | pixel << 8;
    
    if(colour == 0) return ((pixel >> 11) & 0x1F) << 3;     //r 
    else if(colour == 1) return ((pixel >> 5) & 0x3F) << 2; //g
    else if(colour == 2) return (pixel & 0x1F) << 3;        //b
    else return 0;
  }
  
  public:
  c_sstv_encoder_nco(double fs_Hz, double frequency_Hz) : 
  c_sstv_encoder(fs_Hz), 
  sample_frequency_Hz(fs_Hz)
  {
    m_phase = 0;
    m_residue_f16 = 0;
  }

};

void setup() {
  Serial.begin(115200);

  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2026");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
  Serial.println("Connect wire to pin 1. Press bootsel to transmit!");
  
  waveforms_per_sample = (double)rp2040.f_cpu() / (256 * 15000);
  sample_rate_Hz = (double)rp2040.f_cpu() / (256 * waveforms_per_sample);
  queue_init(&data_queue, 2, 2048);

}

void loop() {
  if(BOOTSEL) {
    c_sstv_encoder_nco sstv_encoder(sample_rate_Hz, RF_FREQUENCY);
    sstv_encoder.generate_sstv(tx_mode);
  }
}

void loop1() {
  transmit_nco rf_nco(RF_PIN, rp2040.f_cpu(), RF_FREQUENCY);

  int count=0;
  while (true) {
    // Block until something is available
    uint16_t phase;
    queue_remove_blocking(&data_queue, &phase);
    rf_nco.output_sample(phase, waveforms_per_sample);
  }
}