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
#include "sstv_decoder.h"
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
//#define ROTATION R90DEG
//#define ROTATION R180DEG
#define ROTATION R270DEG
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

#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 40

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

ILI934X *display;


//c_sstv_decoder provides a reusable SSTV decoder
//We need to override some hardware specific functions to make it work with
//ADC audio input and TFT image display
class c_sstv_decoder_fileio : public c_sstv_decoder
{
  ADCAudio adc_audio;
  uint16_t row_number = 0;
  const uint16_t display_width = 320;
  const uint16_t display_height = 240 - 10; //allow space for status bar

  //override the get_audio_sample function to read ADC audio
  int16_t get_audio_sample()
  {
    static int16_t *samples;
    static uint16_t sample_number = 1024;

    //if we reach the end of a block request a new one
    if(sample_number == 1024)
    {
      //fetch a new block of 1024 samples
      samples = adc_audio.input_samples();
      sample_number = 0;
    }

    //output the next sample in the block
    return samples[sample_number++];
  }

  //override the image_write_line function to output images to a TFT display
  void image_write_line(uint16_t line_rgb565[], uint16_t y, uint16_t width, uint16_t height, const char* mode_string)
  {

    Serial.println(y);

    //scale image to fit TFT size
    uint16_t scaled_row[display_width];
    uint16_t pixel_number = 0;
    for(uint16_t x=0; x<width; x++)
    {
        uint16_t scaled_x = static_cast<uint32_t>(x) * display_width / width;
        while(pixel_number < scaled_x)
        {
          //display expects byteswapped data
          scaled_row[pixel_number] = ((line_rgb565[x] & 0xff) << 8) | ((line_rgb565[x] & 0xff00) >> 8);
          pixel_number++;
        }
    }

    Serial.println(row_number);
    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while(row_number < scaled_y)
    {
      display->writeHLine(0, row_number, display_width, scaled_row);
      row_number++;
    }

    //update progress
    display->fillRect(0, display_height, 10, display_width, COLOUR_BLACK);
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%u", mode_string, width, y+1);
    display->drawString(0, display_height+2, font_8x5, buffer, COLOUR_WHITE, COLOUR_BLACK);
    //Serial.println(buffer);

  }

  void scope(uint16_t mag, int16_t freq) {
   
    // Frequency
    static uint8_t row=0;
    static uint16_t count=0;
    static uint16_t w[150];

    uint16_t val=0;
    
    uint8_t f=(freq-1000)/10; //from 1500-2300 to 50-130
    
    if (f>0 && f<150) {
      w[f]=(w[f]<<1)|3; //Pseudo exponential increment
    }
    if (count>200 ) {
      w[20]=0xF800;  //1200 hz red line
      w[50]=0xF800;  //1500 hz red line
      w[130]=0xF800; //2300 hz red line
      display->writeHLine(150,231+row++,150,w);
      
      for (int i=0;i<150;i++) {
        w[i]=w[i]>>2;  //Exponential decay
        val+=w[i]/150; //Accumulator for signal strength
      }

      if (row>8) row=0;   
      count=0;

      val=(val-300)/500; //Scale to 0-8
      // Draw signal bar
      display->fillRect(145, 231, 3, 8-val, TFT_BLACK);
      display->fillRect(145, 239-val, 3, val, TFT_GREEN);
    }
    count++;
  }

  public:

  void start(){adc_audio.begin(28, 15000); row_number = 0;}
  void stop(){adc_audio.end();}
  c_sstv_decoder_fileio(float fs) : c_sstv_decoder{fs}{}

};

void setup() {
  Serial.begin(115200);
  configure_display();
  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2024");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
}

void loop() {
  c_sstv_decoder_fileio sstv_decoder(15000);
  while(1)
  {
    sstv_decoder.start();
    sstv_decoder.decode_image(LOST_SIGNAL_TIMEOUT_SECONDS, ENABLE_SLANT_CORRECTION);
    sstv_decoder.stop();
  }
}

void draw_splash_screen()
{
  display->writeImage(0, 0, 320, 240, splash);
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
  display = new ILI934X(SPI_PORT, PIN_CS, PIN_DC, 320, 240);
  display->init(ROTATION, INVERT_COLOURS, INVERT_DISPLAY, ILI9341_2);
  display->powerOn(true);
  display->clear();
  draw_splash_screen();
}


