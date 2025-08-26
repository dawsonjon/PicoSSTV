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
// Adaptation for Seeed Wio Terminal by IS0JSV Franciscu Capuzzi "Brabudu" 2025
//
// License: MIT
//

#include "sstv_decoder.h"
#include "ADCAudio.h"
#include "TFT_eSPI.h"

#define ENABLE_SLANT_CORRECTION true
//#define ENABLE_SLANT_CORRECTION false

#define LOST_SIGNAL_TIMEOUT_SECONDS 40

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

TFT_eSPI display=TFT_eSPI();


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
          scaled_row[pixel_number] = line_rgb565[x];
          pixel_number++;
        }
    }

    Serial.println(row_number);
    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while(row_number < scaled_y)
    {
      writeHLine(0, row_number, display_width, scaled_row);
      row_number++;
    }

    //update progress
    display.fillRect(0, display_height, 10, display_width, TFT_BLACK);
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%03u", mode_string, width, y+1);
    display.drawString(buffer, 0, display_height+2);
    //Serial.println(buffer);

  }

  void writeHLine(int16_t x, int16_t y, int16_t w, uint16_t* color) {
  for (int i=x;i<w+x;i++) display.drawPixel(i,y, color[i]);
}

  public:

  void start(){adc_audio.begin(28, 15000); row_number = 0;}
  void stop(){adc_audio.end();}
  c_sstv_decoder_fileio(float fs) : c_sstv_decoder{fs}{}

};

void setup() {
  Serial.begin(115200); 
  configure_display();
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

void configure_display()
{
    display.begin();
    display.setRotation(3);
    digitalWrite(LCD_BACKLIGHT, HIGH); // turn on the backlight
    display.drawString("SSTV decoder ready.",2,2);
}

