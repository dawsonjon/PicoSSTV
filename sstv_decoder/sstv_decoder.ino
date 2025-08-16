//  _  ___  _   _____ _     _                 
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___ 
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/    
//
// Copyright (c) Jonathan P Dawson 2024
// filename: sstv_decoder.ino
// description:
//
// SSTV Decoder using pi-pico.
//
// Accepts audio on ADC input, and displays on an ILI943x display.
// Works with the Martin M1/2 and Scottie S1/2 and PD50/90.
//
// License: MIT
//
// MODS BY ON4ABR - 2025
// * Option to invert the display when images ar negative

#include "hardware/spi.h"
#include "ili934x.h"
#include "font_8x5.h"
#include "font_16x12.h"
#include "decode_sstv.h"
#include "ADCAudio.h"
#include "splash.h"

//CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

#define PIN_MISO 12 //not used by TFT but part of SPI bus
#define PIN_CS   13
#define PIN_SCK  14
#define PIN_MOSI 15 
#define PIN_DC   11
#define SPI_PORT spi1

//!!Note, can be quite a bit of variation between TFT displays
//if the display doesn't look right it can be fixed by changing these settings!!

//If the image doesn't fill the display, or is rotated try changing
//the ROTATION.

//#define ROTATION R0DEG
#define ROTATION R90DEG
//#define ROTATION R180DEG
//#define ROTATION R270DEG
//#define ROTATION MIRRORED0DEG
//#define ROTATION MIRRORED90DEG
//#define ROTATION MIRRORED180DEG
//#define ROTATION MIRRORED270DEG

//The splash screen should have blue lettering, if you see red lettering 
//try changing the INVERT_COLOURS setting.

//#define INVERT_COLOURS false
#define INVERT_COLOURS true

//The splash screen should have a black background, if you have a white
//background try changing this setting. Many thans to ON4ABR for adding 
//this option.
#define INVERT_DISPLAY false
//#define INVERT_DISPLAY true

#define STRETCH true
//#define STRETCH false

#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 40

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

ILI934X *display;

void setup() {
  configure_display();
  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2024");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
}


void loop() {
  ADCAudio adc_audio;
  adc_audio.begin(28, 15000);
  c_sstv_decoder sstv_decoder(15000);
  s_sstv_mode *modes = sstv_decoder.get_modes();
  int16_t dc;
  uint8_t line_rgb[320][4];
  uint16_t last_pixel_y=0;

  sstv_decoder.set_auto_slant_correction(ENABLE_SLANT_CORRECTION);
  sstv_decoder.set_timeout_seconds(LOST_SIGNAL_TIMEOUT_SECONDS);
  
  while(1)
  {
    uint16_t *samples;
    adc_audio.input_samples(samples);

    for(uint16_t idx=0; idx<1024; idx++)
    {
      dc = dc + (samples[idx] - dc)/2;
      int16_t sample = samples[idx] - dc;
      uint16_t pixel_y;
      uint16_t pixel_x;
      uint8_t pixel_colour;
      uint8_t pixel;
      int16_t frequency;
      const bool new_pixel = sstv_decoder.decode_audio(sample, pixel_y, pixel_x, pixel_colour, pixel, frequency);

      if(new_pixel)
      {
          e_mode mode = sstv_decoder.get_mode();

          if(pixel_y > last_pixel_y)
          {

            //convert from 24 bit to 16 bit colour
            uint16_t line_rgb565[320];
            uint16_t scaled_pixel_y = 0;

            if(mode == pd_50 || mode == pd_90 || mode == pd_120 || mode == pd_180)
            {

              //rescale imaagesto fit on screen
              if(mode == pd_120 || mode == pd_180)
              {
                scaled_pixel_y = (uint32_t)last_pixel_y * 240 / 496; 
              }
              else
              {
                scaled_pixel_y = last_pixel_y;
              }

              for(uint16_t x=0; x<320; ++x)
              {
                int16_t y  = line_rgb[x][0];
                int16_t cr = line_rgb[x][1];
                int16_t cb = line_rgb[x][2];
                cr = cr - 128;
                cb = cb - 128;
                int16_t r = y + 45 * cr / 32;
                int16_t g = y - (11 * cb + 23 * cr) / 32;
                int16_t b = y + 113 * cb / 64;
                r = r<0?0:(r>255?255:r);
                g = g<0?0:(g>255?255:g);
                b = b<0?0:(b>255?255:b);
                line_rgb565[x] = display->colour565(r, g, b);
              }
              display->writeHLine(0, scaled_pixel_y*2, 320, line_rgb565);
              for(uint16_t x=0; x<320; ++x)
              {
                int16_t y  = line_rgb[x][3];
                int16_t cr = line_rgb[x][1];
                int16_t cb = line_rgb[x][2];
                cr = cr - 128;
                cb = cb - 128;
                int16_t r = y + 45 * cr / 32;
                int16_t g = y - (11 * cb + 23 * cr) / 32;
                int16_t b = y + 113 * cb / 64;
                r = r<0?0:(r>255?255:r);
                g = g<0?0:(g>255?255:g);
                b = b<0?0:(b>255?255:b);
                line_rgb565[x] = display->colour565(r, g, b);
              }
              display->writeHLine(0, scaled_pixel_y*2 + 1, 320, line_rgb565);
            }
              else if (mode == robot36) {
                //Detect crominance phase
                uint8_t count=0;
                for(uint16_t x=0; x<40; ++x) {
                  if (line_rgb[x][3]>128) count++;
                }

                uint8_t crc=2;
                uint8_t cbc=1;
               
                if ((count<20 && (last_pixel_y%2==0))||(count>20)&& (last_pixel_y%2==1)) {
                  crc=1;
                  cbc=2;
                }

                for(uint16_t x=0; x<320; ++x)
                {
                  int16_t y  = line_rgb[x][0];    
                  int16_t cr = line_rgb[x][crc];
                  int16_t cb = line_rgb[x][cbc]; 
                  
                  cr = cr - 128;
                  cb = cb - 128;
                  int16_t r = y + 45 * cr / 32;
                  int16_t g = y - (11 * cb + 23 * cr) / 32;
                  int16_t b = y + 113 * cb / 64;
                  r = r<0?0:(r>255?255:r);
                  g = g<0?0:(g>255?255:g);
                  b = b<0?0:(b>255?255:b);

                 line_rgb565[x] = display->color565(r, g, b);            
                 
                }
              display->writeHLine(0, last_pixel_y, 320, line_rgb565);
            } else
            {
              for(uint16_t x=0; x<320; ++x)
              {
                line_rgb565[x] = display->colour565(line_rgb[x][0], line_rgb[x][1], line_rgb[x][2]);
              }
              display->writeHLine(0, last_pixel_y, 320, line_rgb565);
              
            }
            
            for(uint16_t x=0; x<320; ++x) {
              line_rgb[x][0] = 0;
              //Robot36 cr and cb must persist 2 lines
              if (mode != robot36 ) line_rgb[x][1] = line_rgb[x][2] = 0;
            }        

            //update progress
            display->fillRect(320-(21*6)-2, 240-10, 10, 21*6+2, COLOUR_BLACK);
            char buffer[21];
            if(mode==martin_m1)
            {
              snprintf(buffer, 21, "Martin M1: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==martin_m2)
            {
              snprintf(buffer, 21, "Martin M2: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_s1)
            {
              snprintf(buffer, 21, "Scottie S1: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_s2)
            {
              snprintf(buffer, 21, "Scottie S2: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==scottie_dx)
            {
              snprintf(buffer, 21, "Scottie DX: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==sc2_120)
            {
              snprintf(buffer, 21, "SC2 120: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_50)
            {
              snprintf(buffer, 21, "PD 50: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_90)
            {
              snprintf(buffer, 21, "PD 90: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_120)
            {
              snprintf(buffer, 21, "PD 120: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==pd_180)
            {
              snprintf(buffer, 21, "PD 180: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            else if(mode==robot36)
            {
              snprintf(buffer, 21, "Robot36: %ux%u", modes[mode].width, last_pixel_y+1);
            }
            display->drawString(320-(21*6), 240-8, font_8x5, buffer, COLOUR_WHITE, COLOUR_BLACK);
            Serial.println(buffer);

          }
          last_pixel_y = pixel_y;
          

          if(pixel_x < 320 && pixel_y < 256 && pixel_colour < 4) {
            if(STRETCH && modes[mode].width==160)
            {
              if(pixel_x < 160)
              {
                line_rgb[pixel_x*2][pixel_colour] = pixel;
                line_rgb[pixel_x*2+1][pixel_colour] = pixel;
              }
            }
            else
            {
              line_rgb[pixel_x][pixel_colour] = pixel;
            }

          }
          
      }

    }
  }
  adc_audio.end();
}

void draw_splash_screen()
{
  uint32_t z=0;
  for(uint16_t y=0;y<240; ++y)
  {
    display->writeHLine(0, y, 320, &splash[z]);
    z+=320;
  }
}

void configure_display()
{
  spi_init(SPI_PORT, 62500000);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_init(PIN_CS);
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_init(PIN_DC);
  gpio_set_dir(PIN_DC, GPIO_OUT);
  display = new ILI934X(SPI_PORT, PIN_CS, PIN_DC, 320, 240, R0DEG, INVERT_DISPLAY);
  display->setRotation(ROTATION, INVERT_COLOURS);
  display->init();
  display->powerOn(true);
  display->clear();
  draw_splash_screen();
}


