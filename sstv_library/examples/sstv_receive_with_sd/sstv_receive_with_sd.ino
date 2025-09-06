//  _  ___  _   _____ _     _                 
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___ 
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/    
//
// Copyright (c) Jonathan P Dawson 2025
// filename: sstv_decoder.ino
// description:
//
// SSTV Decoder using pi-pico.
//
// Accepts audio on ADC input, and displays on an ILI943x display.
// Works with the Martin M1/2 and Scottie S1/2 and PD50/90.
//
// License: MIT

#include "hardware/spi.h"
#include "ili934x.h"
#include "font_8x5.h"
#include "font_16x12.h"
#include "sstv_decoder.h"
#include "ADCAudio.h"
#include "splash.h"
#include <bmp_lib.h>
#include <SPI.h>
#include <SDFS.h>
#include <VFS.h>

//CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

#define PIN_MISO 12 //not used by TFT but part of SPI bus
#define PIN_CS   13
#define PIN_SCK  14
#define PIN_MOSI 15 
#define PIN_DC   11
#define SPI_PORT spi1

#define SDCARD_MISO 4
#define SDCARD_MOSI 7
#define SDCARD_CS   5
#define SDCARD_SCK  6

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


class c_bmp_writer_stdio : public c_bmp_writer
{
    bool file_open(const char* filename)
    {
      f = fopen(filename, "wb");
      return f != 0;
    }

    void file_close()
    {
      if(f) fclose(f);
    }

    void file_write(const void* data, uint32_t element_size, uint32_t num_elements)
    {
      if(f) fwrite(data, element_size, num_elements, f);
    }

    void file_seek(uint32_t offset)
    {
        fseek(f, offset, SEEK_SET);
    }

    FILE* f;
};

//c_sstv_decoder provides a reusable SSTV decoder
//We need to override some hardware specific functions to make it work with
//ADC audio input and TFT image display
class c_sstv_decoder_fileio : public c_sstv_decoder
{
  ADCAudio adc_audio;
  uint16_t tft_row_number = 0;
  const uint16_t display_width = 320;
  const uint16_t display_height = 240 - 10; //allow space for status bar
  uint16_t image_width, image_height;

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
    //write unscaled image to bmp file
    output_file.change_width(width);
    output_file.change_height(y+1);

    //write unscaled image to bmp file
    if(++bmp_row_number < height) output_file.write_row_rgb565(line_rgb565);

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

    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while(tft_row_number < scaled_y)
    {
      display->writeHLine(0, tft_row_number, display_width, scaled_row);
      tft_row_number++;
    }

    //update progress
    display->fillRect(0, display_height, 10, display_width, COLOUR_BLACK);
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%u", mode_string, width, y+1);
    display->drawString(0, display_height+2, font_8x5, buffer, COLOUR_WHITE, COLOUR_BLACK);
    Serial.println(buffer);

  }

  void scope(uint16_t mag, int16_t freq) {
   
    // Frequency
    static uint8_t row=0;
    static uint16_t count=0;
    static uint16_t w[150];
    static float mean_freq=0;

    uint16_t val=0;
    
    uint8_t f=(freq-1000)/10; //from 1500-2300 to 50-130
    
    mean_freq=(mean_freq*15+f)/16;
    f=mean_freq;
    
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
      display->fillRect(145, 231, 3, 8-val, COLOUR_BLACK);
      display->fillRect(145, 239-val, 3, val, COLOUR_GREEN);
    }
    count++;
  }

  

  c_bmp_writer_stdio output_file;
  uint16_t bmp_row_number = 0;

  public:

  void open(const char* bmp_file_name){
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.print("opening bmp file: ");
    Serial.println(bmp_file_name);
    output_file.open(bmp_file_name, 10, 10); //image size gets updated later
  }

  void close(){
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.println("closing bmp file");
    output_file.update_header();
    output_file.close();
  }

  void start(){adc_audio.begin(28, 15000);}
  void stop(){adc_audio.end();}
  c_sstv_decoder_fileio(float fs) : c_sstv_decoder{fs}{}

};

void setup() {
  Serial.begin(115200);

  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2025");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
  
  configure_display();
  initialise_sdcard();
  VFS.root(SDFS);

}

void loop() {
  c_sstv_decoder_fileio sstv_decoder(15000);
  sstv_decoder.start();
  sstv_decoder.open("temp");
  char filename[100];
  get_new_filename(filename, 100);

  while(1)
  {
    sstv_decoder.decode_image(LOST_SIGNAL_TIMEOUT_SECONDS, ENABLE_SLANT_CORRECTION);
    sstv_decoder.close();
    SDFS.rename("temp", filename);
    Serial.print("copy to: ");
    Serial.println(filename);
    get_new_filename(filename, 100);
    sstv_decoder.open("temp");
  }
  sstv_decoder.stop();
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

void initialise_sdcard()
{

  Serial.print("Initializing SD card...");
  bool sdInitialized = false;

  // Ensure the SPI pinout the SD card is connected to is configured properly
  // Select the correct SPI based on _MISO pin for the RP2040
  if (SDCARD_MISO == 0 || SDCARD_MISO == 4 || SDCARD_MISO == 16) {
    SPI.setRX(SDCARD_MISO);
    SPI.setTX(SDCARD_MOSI);
    SPI.setSCK(SDCARD_SCK);
    SDFS.setConfig(SDFSConfig(SDCARD_CS, SPI_HALF_SPEED, SPI));
    sdInitialized = SDFS.begin();
  } else if (SDCARD_MISO == 8 || SDCARD_MISO == 12) {
    SPI1.setRX(SDCARD_MISO);
    SPI1.setTX(SDCARD_MOSI);
    SPI1.setSCK(SDCARD_SCK);
    SDFS.setConfig(SDFSConfig(SDCARD_CS, SPI_HALF_SPEED, SPI1));
    sdInitialized = SDFS.begin();
  } else {
    Serial.println(F("ERROR: Unknown SPI Configuration"));
    return;
  }

  if (!sdInitialized) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

}

void get_new_filename(char *buffer, uint16_t buffer_size)
{
  static uint16_t serial_number = 0; 
  do{
    snprintf(buffer, buffer_size, "sstv_rx_%u.bmp", serial_number);
    serial_number++;
  } while(SDFS.exists(buffer));
}