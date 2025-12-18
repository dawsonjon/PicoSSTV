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
// 
//
// License: MIT
//
// BRABUDU
//
// 
//
// WIFI: 
//
// Configure ssid and password
//
// Enable wifi in settings
// Look at the ip address in the menu title
// Connect from pc while the Pico sstv is in menu mode
//
// Transmission:
//
// Put your pictures (24 bit bmp) in the tx folder
// Configure your callsign in the configuration section above
// Use "reply" for replying to a cq call, then insert the receiver callsign and the rst. Select the picture and send
// Use "transmit" in the menu section for making a cq call

#include "hardware/spi.h"
#include "ili934x.h"
#include "gfxfont.h"
#include "font_8x5.h"
#include "font_16x12.h"
#include "FreeSansBold24pt7b.h"
#include "sstv_decoder.h"
#include "bmp_classes.h"
#include "ADCAudio.h"
#include "PWMAudio.h"
#include "splash.h"
#include "sstv_encoder.h"
#include "frame_buffer.h"
#include "button.h"
#include "bmp_lib.h"

#include <SPI.h>
#include <SDFS.h>
#include <VFS.h>
#include <EEPROM.h>
#include <vector>
#include <string>
#include <algorithm>


#if defined(PICO_RP2350)
#define WIFI            //Comment for disabling wifi
#endif

#ifdef WIFI
#include <WiFi.h>
#include <WiFiServer.h>

WiFiServer server(80);

bool connected=false;

#endif

//CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

#define CALLSIGN "IS0JSV\0";

const char* ssid     = "Undixedda noa";
const char* password = "Miagolina25!";


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

//Chroma key color for overlay

#define CHROMA 0

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

void draw_banner(const char* message, uint16_t y=0);
void draw_status_bar(const char* message);
void draw_button_bar(const char* btn1, const char* btn2, const char* btn3, const char* btn4);
void configure_display();
void initialise_sdcard();
void get_new_filename(char *buffer, uint16_t buffer_size);
void display_image(const char* filename, bool show_overlay=false);
void get_timeout_seconds(const char* title, uint8_t & menu_selection);
void launch_menu();
uint16_t count_bitmaps(Dir &root);
void get_bitmap_index(Dir &root, uint16_t index);
void create_thumbnail(const char* filename);

ILI934X *display;
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define STATUS_BAR_HEIGHT 20

button button_up(26); //17
button button_down(20); 
button button_right(21); 
button button_left(22);

enum e_view_mode {rx_mode, slideshow_mode};
e_view_mode view_mode;

static const uint16_t overlay_width = 320;
static const uint16_t overlay_height = 256;
uint16_t overlay_buffer[overlay_width*overlay_height];
c_frame_buffer overlay(overlay_buffer, overlay_width, overlay_height);

uint16_t scaled_image[214*160];

char txcallsign_text[10]= CALLSIGN
char rxcallsign_text[10];
char rst_text[4];

struct s_settings {
  uint8_t slideshow_timeout;
  uint8_t lost_signal_timeout;
  uint8_t min_completion;
  uint8_t transmit_mode;
  uint8_t auto_slant_correction;
  uint8_t overlay;
  uint8_t wifi;
  char overlay_text[25];
};

s_settings settings = {
  3, //5 seconds
  3, //30 seconds
  2, // 50%
  1, //martin m2
  1,  //auto slant correction on
  1, // overlay on
  0,  //wifi off
  {0}
};



//c_sstv_decoder provides a reusable SSTV decoder
//We need to override some hardware specific functions to make it work with
//ADC audio input and TFT image display
class c_sstv_decoder_fileio : public c_sstv_decoder
{
  ADCAudio adc_audio;
  uint16_t tft_row_number = 0;
  float progress=0;
  e_mode last_mode=martin_m1;

  const uint16_t display_width = DISPLAY_WIDTH;
  const uint16_t display_height = DISPLAY_HEIGHT - STATUS_BAR_HEIGHT; //allow space for status bar

  //override the get_audio_sample function to read ADC audio
  int16_t get_audio_sample()
  {
    static int16_t *samples;
    static uint16_t sample_number = ADC_BLOCK;
    //if we reach the end of a block request a new one
    if(sample_number == ADC_BLOCK) {
      //fetch a new block of 1024 samples
      samples = adc_audio.input_samples();
      sample_number = 0;
    }
    //output the next sample in the block
    return samples[sample_number++];
  }

  //override the image_write_line function to output images to a TFT display
  void image_write_line(uint16_t line_rgb565[], uint16_t y, uint16_t width, uint16_t height, e_mode decode_mode)
  {
    //write unscaled image to bmp file
    output_file.change_width(width);
  
    if(++bmp_row_number < height){
      output_file.change_height(y+1);
      output_file.write_row_rgb565(line_rgb565);
    } 

    //scale image to fit TFT size
    uint16_t scaled_row[display_width];
    uint16_t pixel_number = 0;
    for(uint16_t x=0; x<width; x++) {
        uint16_t scaled_x = static_cast<uint32_t>(x) * display_width / width;
        while(pixel_number <= scaled_x) {
          //display expects byteswapped data
          scaled_row[pixel_number] = ((line_rgb565[x] & 0xff) << 8) | ((line_rgb565[x] & 0xff00) >> 8);
          pixel_number++;
        }
    }

    uint32_t scaled_y = static_cast<uint32_t>(y) * display_height / height;
    while(tft_row_number <= scaled_y) {
      display->writeHLine(0, tft_row_number, display_width, scaled_row);
      tft_row_number++;
    }

    //update progress
    char buffer[15];
    snprintf(buffer, 15, "%5s %ux%u", tx_modes_abbr[decode_mode], width, y+1);
    draw_status_bar(buffer);
    //draw_banner(buffer);
    Serial.println(buffer);

    progress=y/(float)height;
    last_mode=decode_mode;
  }

 
  void scope(uint16_t mag, int16_t freq) {
  static const uint16_t palette[]={0x0000,0x0011,0x0023,0x0024,0x0043,0x0044,0x0044,0x0044,0x0044,0x0044,0x0044,0x0045,0x0045,0x0065,0x0065,0x0065,0x0065,0x0066,0x0066,0x0066,0x0066,0x0086,0x0087,0x0087,0x0087,0x0087,0x0088,0x00a8,0x00a8,0x00a8,0x00a8,0x00a9,0x00c9,0x00c9,0x00ca,0x00ca,0x00ca,0x00ea,0x00eb,0x00eb,0x00eb,0x010b,0x010c,0x010c,0x012c,0x012d,0x012d,0x012d,0x014e,0x014e,0x014e,0x016f,0x016f,0x016f,0x0190,0x0190,0x0190,0x01b1,0x01b1,0x01d1,0x01d2,0x01d2,0x01f2,0x01f3,0x0213,0x0213,0x0214,0x0234,0x0234,0x0255,0x0255,0x0276,0x0276,0x0296,0x0297,0x02b7,0x02b7,0x02d8,0x02d8,0x02f8,0x02f9,0x0319,0x0319,0x033a,0x035a,0x035a,0x037b,0x037b,0x039b,0x039c,0x03bc,0x03dc,0x03dc,0x03fd,0x03fd,0x041d,0x043e,0x043e,0x045e,0x045e,0x047f,0x049f,0x049f,0x04bf,0x04df,0x04df,0x04ff,0x051f,0x051f,0x053f,0x053f,0x055f,0x057f,0x057f,0x059f,0x05bf,0x05bf,0x0ddf,0x0ddf,0x0dff,0x0e1f,0x0e1f,0x0e3f,0x0e3f,0x0e5f,0x0e7f,0x0e7f,0x0e9f,0x0e9f,0x0ebf,0x0ebf,0x16df,0x16df,0x16ff,0x171f,0x171f,0x173f,0x173f,0x173f,0x175f,0x1f5f,0x1f7f,0x1f7f,0x1f9f,0x1f9f,0x1f9f,0x27bf,0x27bf,0x27df,0x27df,0x27df,0x27ff,0x2fff,0x2fff,0x2fff,0x2ffe,0x37fe,0x37fe,0x37fe,0x37fd,0x3ffd,0x3ffd,0x3ffc,0x3ffc,0x47fc,0x47fc,0x47fb,0x4ffb,0x4ffb,0x4ffa,0x57fa,0x57fa,0x57f9,0x5ff9,0x5ff9,0x5ff8,0x67f8,0x67f8,0x6ff7,0x6ff7,0x6ff7,0x77f6,0x77f6,0x7ff6,0x7ff5,0x87f5,0x87f4,0x8ff4,0x8ff4,0x8ff3,0x97d3,0x97d3,0x9fd2,0x9fb2,0xa7b2,0xa791,0xaf91,0xaf91,0xb770,0xb770,0xbf50,0xbf4f,0xc72f,0xc72f,0xcf2e,0xcf0e,0xd70e,0xdeed,0xdecd,0xe6cd,0xe6ac,0xeeac,0xee8c,0xf68b,0xf66b,0xfe6b,0xfe4b,0xfe2a,0xfe2a,0xfe0a,0xfe0a,0xfde9,0xfdc9,0xfdc9,0xfda8,0xfda8,0xfd88,0xfd68,0xfd68,0xfd47,0xfd27,0xfd27,0xfd07,0xfd06,0xfce6,0xfcc6,0xfcc6,0xfca6,0xfc85,0xfc85,0xfc65,0xfc45,0xfc45,0xfc25,0xfc24,0xfc04,0xfbe4,0xfbe4,0xfbc4,0xfbc4,0xfba3,0xfb83,0xfb83,0xfb63,0xfb63,0xfb43};

    const uint16_t scope_x = 168;
    const uint16_t scope_y = 234;
    const uint16_t scope_width = 150;

    const uint8_t waterfall_amp = 4;

    if(view_mode != rx_mode) return;
   
    static uint8_t row=0;
    static uint16_t count=0;
    static uint32_t spectrum[scope_width];
    static uint32_t signal_strength = 0;
    static uint8_t mean_f=0;

    const uint8_t f=(freq-1000)*scope_width/1500;
    const uint8_t Hz_1200 = (1200-1000)*scope_width/1500;
    const uint8_t Hz_1500 = (1500-1000)*scope_width/1500;
    const uint8_t Hz_2300 = (2300-1000)*scope_width/1500;
   
    mean_f=(mean_f* 7 + f)/8;

    if (mean_f>0 && mean_f<scope_width) {
      spectrum[mean_f] = (spectrum[mean_f] * 15 + mag)/16;
    }
    signal_strength = (signal_strength * 15 + mag)/16;
    if (count>200 ) {
      //display->drawRect(scope_x-1, scope_y-12, 14, scope_width+3, COLOUR_DARKGREY);
      uint16_t waterfall[scope_width];
      for (int i=0;i<scope_width;i++) {
        float scaled_dB = waterfall_amp*20*log10(spectrum[i]);
        scaled_dB = std::max(std::min(scaled_dB, 255.0f), 0.0f);          
        waterfall[i]=__builtin_bswap16(palette[(int)scaled_dB]);
   
      }
      waterfall[Hz_1200]=COLOUR_RED;
      waterfall[Hz_1500]=COLOUR_RED;
      waterfall[Hz_2300]=COLOUR_RED;
      display->writeHLine(scope_x,scope_y-12+row++,scope_width,waterfall);
      
      for (int i=0;i<scope_width;i++) {
        spectrum[i]=spectrum[i]>>1;
      }

      if (row>11) row=0;   
      count=0;

      // Draw signal bar
      float scaled_dB = 2*20*log10(signal_strength);
      scaled_dB = std::max(std::min(scaled_dB, 149.0f), 0.0f);
      display->fillRect(scope_x, scope_y, 2, scaled_dB, COLOUR_YELLOW);
      display->fillRect(scope_x+scaled_dB, scope_y, 2, scope_width-scaled_dB, COLOUR_MAROON);

    }
    count++;
  }

  
  c_bmp_writer_stdio output_file;
  uint16_t bmp_row_number = 0;

  public:

  float getProgress() {
    return progress;
  }

  e_mode getLastMode() {
    return last_mode;
  }


  void open(const char* bmp_file_name){
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.print("opening output bmp file: ");
    Serial.println(bmp_file_name);
    output_file.open(bmp_file_name, 10, 10);
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

void set_overlay(const char message[])
{
  //Create a background gradient
  for(uint16_t x=0; x<overlay_width; x++) {
    for(uint16_t y=0; y<14; y++) {
     overlay.set_pixel(x, y, overlay.colour565(0, x*255/overlay_width, 255));
    }
  }
  uint16_t text_width = strlen(message) * 12;
  overlay.draw_string((overlay_width-text_width)/2, 4, font_8x5, message, COLOUR_ORANGE);
}

//Derive a class from sstv encoder and override hardware specific functions
const uint16_t audio_buffer_length = 4096u;
class c_sstv_encoder_pwm : public c_sstv_encoder
{  
  uint16_t audio_buffer[2][audio_buffer_length];
  uint16_t audio_buffer_index = 0;
  uint8_t ping_pong = 0;
  PWMAudio audio_output;
  uint16_t sample_min, sample_max;
  
  void output_sample(int16_t sample)
  {
    uint16_t scaled_sample = ((sample+32767)>>5);// + 1024;
    audio_buffer[ping_pong][audio_buffer_index++] = scaled_sample;
    if(button_left.is_pressed()) abort();
    if(audio_buffer_index == audio_buffer_length)
    {
      audio_output.output_samples(audio_buffer[ping_pong], audio_buffer_length);
      ping_pong ^= 1;
      audio_buffer_index = 0;
      sample_max = scaled_sample;
      sample_min = scaled_sample;
    }
    else
    {
      sample_max = max(sample_max, scaled_sample);
      sample_min = min(sample_min, scaled_sample);
    }
  }
  
  uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour)
  {
    uint16_t image_y = (uint32_t)y * image_height / height;
    uint16_t image_x = (uint32_t)x * image_width / width;

    while(image_y >= row_number) {
      bitmap.read_row_rgb565(row);
      row_number++;
      char status[100];
      snprintf(status, 100, "transmitting %u/%u (%u%%)", y+1, height, (100*(y+1))/height);
      draw_banner(status);
    }
    uint16_t pixel;
    //overlay a text banner
    uint16_t overlay_y = (uint32_t)y * overlay_width / width;
    uint16_t overlay_x = (uint32_t)x * overlay_width / width;
    if(settings.overlay && overlay_y < overlay_height)
    {
      pixel = overlay_buffer[(overlay_y*overlay_width) + overlay_x];
      pixel = (pixel >> 8) | (pixel << 8);
      if (pixel==CHROMA) pixel = row[image_x];
    } else {
      pixel = row[image_x];
    }

    if(colour == 0) return ((pixel >> 11) & 0x1F) << 3;     //r 
    else if(colour == 1) return ((pixel >> 5) & 0x3F) << 2; //g
    else if(colour == 2) return (pixel & 0x1F) << 3;        //b
    else return 0;
  }
  
  c_bmp_reader_stdio bitmap;
  FILE *pcm;
  uint16_t row_number = 0;
  uint16_t row[640];
  uint16_t image_width, image_height;
  
  public:
  void open(const char * bmp_file_name)
  { 
    bitmap.open(bmp_file_name, image_width, image_height);
    bitmap.read_row_rgb565(row);
    row_number=0;
    audio_output.begin(0, 15000, rp2040.f_cpu());
  }
  void close()
  { 
    bitmap.close();
    audio_output.end();
  }

  c_sstv_encoder_pwm(double fs_Hz) : c_sstv_encoder(fs_Hz){}

};

class c_slideshow
{

  private:
  bool redraw = false;
  Dir root;
  uint16_t num_bitmaps = 0;
  uint16_t bitmap_index = 0;
  String filename;
  uint32_t last_update_time = 0;

  public:
  void launch_slideshow() 
  {
    root = SDFS.openDir("/");
    num_bitmaps = count_bitmaps(root);
    bitmap_index = 0;
    last_update_time = 0;
  }

  void update_slideshow()
  {
    if(num_bitmaps == 0) return;
    delay(50);
    bool redraw = false;
    static const uint16_t timeouts[] = {0, 1, 2, 5, 10, 30, 60, 60*2, 60*5};
    uint16_t timeout_milliseconds = 1000 * timeouts[settings.slideshow_timeout];
    
    if(((millis() - last_update_time) > timeout_milliseconds) && (timeout_milliseconds != 0)) {
      last_update_time = millis();
      if(bitmap_index == num_bitmaps-1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }
    if(button_right.is_pressed()) {
      
      get_bitmap_index(root, bitmap_index);
      filename = root.fileName();
      SDFS.remove(filename);
      char msg[]="Deleted ";
      draw_banner(strcat(msg,filename.c_str()));
      bitmap_index = std::min((int)bitmap_index, num_bitmaps-2);
      root = SDFS.openDir("/");
      num_bitmaps--;
      delay(300);
      if(num_bitmaps == 0) return;
      redraw = true;
    }
    if(button_up.is_pressed()) {
      if(bitmap_index == num_bitmaps-1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }
    if(button_down.is_pressed()) {
      if(bitmap_index == 0) bitmap_index = num_bitmaps-1;
      else bitmap_index--;
      redraw = true;
    }
    if(redraw) {
      get_bitmap_index(root, bitmap_index);
      filename = root.fileName();
      Serial.println(filename);
      display_image(filename.c_str());
      uint16_t width = strlen(filename.c_str())*6+10;
      draw_banner(filename.c_str());
      draw_button_bar("Menu", "Delete", "Last", "Next");
      last_update_time = millis();
    }
  }
};

void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);
  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2025");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
  pinMode(LED_BUILTIN, OUTPUT);
  configure_display();
  initialise_sdcard();
  VFS.root(SDFS);

  load();

#ifdef WIFI
  if (settings.wifi) connectToWiFi();
 #endif
}

void loop() {
  c_sstv_decoder_fileio sstv_decoder(15000);
  sstv_decoder.start();
  sstv_decoder.open("temp");
  char rx_filename[100];
  get_new_filename(rx_filename, 100);
  c_slideshow slideshow;
  bool draw = true;
  bool image_in_progress = false;
  bool last_image_in_progress = false;
  bool image_complete = false;
  view_mode = rx_mode;
  draw_blank_screen();
  strncpy(settings.overlay_text, "Pi Pico SSTV", 24);
  load();
  //set_overlay(settings.overlay_text);

  while(1) {

    //process rx regardless of mode
    static const uint16_t timeouts[] = {UINT16_MAX, 1, 2, 5, 10, 30, 60, 60*2, 60*5};
    static const float completion[] = {0.9, 0.75, 0.5};
    const uint16_t timeout_seconds = timeouts[settings.lost_signal_timeout];
    
    image_complete = sstv_decoder.decode_image_non_blocking(timeout_seconds, settings.auto_slant_correction, image_in_progress);
    
    if ((image_in_progress)&&(!last_image_in_progress)) {
      draw_blank_screen();
      draw_button_bar("", "Stop", "", "");
    }
    last_image_in_progress=image_in_progress;
    
    if(image_complete) {
      sstv_decoder.close();
      if (sstv_decoder.getProgress()>completion[settings.min_completion]) {
        SDFS.rename("temp", rx_filename);
        create_thumbnail(rx_filename);
        get_new_filename(rx_filename, 100);
      }
      sstv_decoder.open("temp");
      draw = true;
    }
    if(image_in_progress) {
      view_mode = rx_mode;
      if (button_right.is_pressed()) {
          sstv_decoder.stop();
          return;
      }
    } else {
      if(button_left.is_pressed()) {
        launch_menu();
        if(view_mode == slideshow_mode) slideshow.launch_slideshow();
        if(view_mode == rx_mode) {
          draw_blank_screen();
          draw = true;
        }
      } else if (button_right.is_pressed() && (view_mode != slideshow_mode)) {    
       
        text_entry(rxcallsign_text, 10);
        rst_entry(rst_text);
        rst_text[3]=0;
        overlay.clear(0);
        overlay.draw_image(20, 130, 106, 80, scaled_image);
        overlay.draw_rect(19,129,108,82,COLOUR_WHITE);
        settings.transmit_mode=sstv_decoder.getLastMode();
        tx_file_browser();
        draw = true;
      }

    }
    if(view_mode == slideshow_mode) {
      
      slideshow.update_slideshow();
    } else if(view_mode == rx_mode && draw) {
      
      draw_button_bar("Menu", "Reply", "", "");
      
      display->fillRect(DISPLAY_WIDTH/2, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT-1, STATUS_BAR_HEIGHT, DISPLAY_WIDTH/2, COLOUR_BLACK);
      draw = false;
    }
  #ifdef WIFI    
    if ((WiFi.status() == WL_CONNECTED)&&(!connected)) {
      server.begin();
      connected=true;
    }
  #endif

  }
  sstv_decoder.stop();
  
}

void draw_splash_screen()
{
  display->writeImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, splash);
  sleep_ms(1000);
}

void draw_blank_screen()
{
  display->clear(COLOUR_NAVY); 
 display->drawString((DISPLAY_WIDTH-(12*strlen("Pico SSTV")))/2, 100, font_16x12, "Pico SSTV", COLOUR_GREY, COLOUR_NAVY);
 //display->drawString(20,40,"Ciao",COLOUR_GREY,&FreeSansBold24pt7b);
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
  display = new ILI934X(SPI_PORT, PIN_CS, PIN_DC, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  display->init(ROTATION, INVERT_COLOURS, INVERT_DISPLAY, ILI9341_2);
  display->powerOn(true);
  display->clear();
  draw_splash_screen();
}

void initialise_sdcard()
{

  Serial.print("Initializing SD card...");
  bool sdInitialized = false;
  SDFSConfig c2;
  c2.setAutoFormat(true);
  SDFS.setConfig(c2);
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
  do {
    snprintf(buffer, buffer_size, "sstv_rx_%u.bmp", serial_number);
    serial_number++;
  } while(SDFS.exists(buffer));
}

void draw_banner(const char* message, uint16_t y)
{
  uint16_t width = strlen(message)*6+10;
  display->fillRoundedRect((DISPLAY_WIDTH-width)/2, y, 10, width, 3, 0);
  display->drawString((DISPLAY_WIDTH-width)/2+5, y+1, font_8x5, message, COLOUR_WHITE, COLOUR_BLACK);
}

void draw_status_bar(const char* message)
{
  display->fillRect(0, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT-1, STATUS_BAR_HEIGHT, DISPLAY_WIDTH/4, COLOUR_BLACK);
  #define MARGIN ((STATUS_BAR_HEIGHT - 8)/2)
  display->drawString(MARGIN, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT+MARGIN, font_8x5, message, COLOUR_WHITE, COLOUR_BLACK);
}

void draw_button_bar(const char* btn1, const char* btn2, const char* btn3, const char* btn4)
{
  display->fillRect(0, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT-1, STATUS_BAR_HEIGHT, DISPLAY_WIDTH, COLOUR_BLACK);
  const uint16_t button_width = 60;
  const uint16_t button_height = 14;
  const uint16_t padding = (DISPLAY_WIDTH - (4*button_width))/5;
  const char *btn_txt[] = {btn1, btn2, btn3, btn4};
  uint16_t button_x = padding;
  for(uint8_t idx=0; idx<4; ++idx) {
    bool active = strlen(btn_txt[idx]);
    display->fillRoundedRect(button_x, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT+2, button_height, button_width, 3, active?COLOUR_BLUE:COLOUR_GREY);
    display->drawRoundedRect(button_x, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT+2, button_height, button_width, 3, active?COLOUR_WHITE:COLOUR_LIGHTGREY);
    display->drawString(button_x + ((button_width-(6*strlen(btn_txt[idx])))/2), DISPLAY_HEIGHT-STATUS_BAR_HEIGHT+MARGIN, font_8x5, btn_txt[idx], COLOUR_WHITE, COLOUR_BLUE);
    button_x += button_width + padding;
  }
}

uint16_t count_bitmaps(Dir &root)
{
  uint16_t count = 0;
  root.rewind();
  while(root.next()) {
    String filename = root.fileName();
    if(root.isFile() && filename.endsWith(".bmp")) count++; 
  }
  return count;
}

void get_bitmap_index(Dir &root, uint16_t index) 
{
  uint16_t count = 0;
  root.rewind();
  while(root.next()) {
    String filename = root.fileName();
    if(root.isFile() && filename.endsWith(".bmp"))
    {
      if(count == index) return;
      count++;
    }
  }
}

void transmit_image(const char* filename) {
  const uint16_t divider = rp2040.f_cpu()/15000;
  const float sample_rate_Hz = (double)rp2040.f_cpu()/divider;
  c_sstv_encoder_pwm sstv_encoder(sample_rate_Hz);
  sstv_encoder.open(filename);
  digitalWrite(LED_BUILTIN, 1);
  sstv_encoder.generate_sstv((e_sstv_tx_mode)settings.transmit_mode);
  digitalWrite(LED_BUILTIN, 0);
  sstv_encoder.close();
  display->clear(COLOUR_NAVY);
  display->drawString((DISPLAY_WIDTH-(12*strlen("Pico SSTV")))/2, 100, font_16x12, "Pico SSTV", COLOUR_GREY, COLOUR_NAVY);
}

void tx_file_browser() {
  bool redraw = true;
  Dir root = SDFS.openDir("/tx/");
  const uint16_t num_bitmaps = count_bitmaps(root);
  if(num_bitmaps == 0) return;
  uint16_t bitmap_index = 0;
  String filename;
  
  while(1) {
    if(button_up.is_pressed()) {
      if(bitmap_index == num_bitmaps-1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }
    if(button_down.is_pressed()) {
      if(bitmap_index == 0) bitmap_index = num_bitmaps-1;
      else bitmap_index--;
      redraw = true;
    }
    if(redraw) {
      get_bitmap_index(root, bitmap_index);
      filename = "/tx/"+root.fileName();
      
      drawOverlay(txcallsign_text,rxcallsign_text,rst_text);
      set_overlay(settings.overlay_text);
      display_image(filename.c_str(), settings.overlay);
      //draw_banner(filename.c_str(), settings.overlay?30:0);
      draw_banner(tx_modes[settings.transmit_mode]);
      draw_button_bar("Transmit", "Cancel", "Last", "Next");
      redraw = false;
    }
    if(button_left.is_pressed()) {
      draw_button_bar("Cancel", "", "", "");
      transmit_image(filename.c_str());
      return;
    }
    if(button_right.is_pressed()) {
      draw_blank_screen();
      return;
    }
  }

}

void display_image(const char* filename, bool show_overlay)
{
  c_bmp_reader_stdio bitmap;
  uint16_t width, height;
  bitmap.open(filename, width, height);

  const uint16_t display_width = DISPLAY_WIDTH, display_height = DISPLAY_HEIGHT-STATUS_BAR_HEIGHT;
  uint16_t tft_row_number = 0;

  for(uint16_t y=0; y<height; y++) {
    uint16_t line_rgb565[width];
    bitmap.read_row_rgb565(line_rgb565);

    //scale image to fit TFT size
    uint16_t scaled_row[display_width];
    uint16_t pixel_number = 0;
    uint16_t overlay_y = (uint32_t)y * overlay_width / width;
  
      for(uint16_t x=0; x<width; x++) {
        uint16_t scaled_x = (static_cast<uint32_t>(x) * display_width + (display_width/2)) / width;
        uint16_t overlay_x = (uint32_t)x * overlay_width / width;
        while(pixel_number <= scaled_x) {
          //display expects byteswapped data
          uint16_t pixel = overlay_buffer[(overlay_y*overlay_width) + overlay_x];
          if (pixel==CHROMA || !show_overlay) pixel=((line_rgb565[x] & 0xff) << 8) | ((line_rgb565[x] & 0xff00) >> 8);
          scaled_row[pixel_number] = pixel;
          pixel_number++;
        }
      }

    uint32_t scaled_y = (static_cast<uint32_t>(y) * display_height + (display_height/2)) / height;
    while(tft_row_number <= scaled_y) {
      display->writeHLine(0, tft_row_number, display_width, scaled_row);
      tft_row_number++;
    }
  }

  bitmap.close();
}

void drawOutlined(uint16_t x, uint16_t y, String msg, uint16_t fg, uint16_t bg ) {
    overlay.draw_string(x-2,y-2,&FreeSansBold24pt7b,msg.c_str(),bg);
    overlay.draw_string(x+2,y-2,&FreeSansBold24pt7b,msg.c_str(),bg);
    overlay.draw_string(x-2,y+2,&FreeSansBold24pt7b,msg.c_str(),bg);
    overlay.draw_string(x+2,y+2,&FreeSansBold24pt7b,msg.c_str(),bg);
    overlay.draw_string(x,y,&FreeSansBold24pt7b,msg.c_str(),fg);
}


void drawOverlay(String callsignSender, String callsignReceiver, String msg ) {
    if (callsignReceiver=="") callsignReceiver="CQ CQ";
    drawOutlined(20,60,callsignReceiver,COLOUR_YELLOW,COLOUR_WHITE);
    drawOutlined(40,110,msg,COLOUR_ORANGE,COLOUR_WHITE);
    drawOutlined(140,210,callsignSender,COLOUR_RED,COLOUR_WHITE);
}

void launch_menu()
{
  uint8_t menu_selection = 0;
  const char * const menu_selections[] = {
    "Receive",
    "Transmit",
    "Slideshow",
    "Settings"
  };

#ifdef WIFI
  String title="Menu "+WiFi.localIP().toString();
#else
   String title="Menu";
#endif

  menu(title.c_str(), menu_selection, menu_selections, 4);
  if(menu_selection == 0) {
    view_mode = rx_mode;
    return;
  } else if(menu_selection == 1) {
    overlay.clear(0);
    tx_file_browser();
    return;
  } else if(menu_selection == 2) {
    view_mode = slideshow_mode;
    return;
  } else {
    uint8_t menu_selection = 0;
    const char * const menu_selections[] = {
      "Auto Slant Correction",
      "Lost Signal Timeout",
      "Min save %",
      "Transmit Mode",
      "Slideshow Timeout",
      "Overlay",
      "Overlay Text",
      "Wifi"
    };
    if(menu("Settings", menu_selection, menu_selections, 8)) {
      switch (menu_selection) 
      {
        case 0: { //Auto slant correction
          const char * const menu_selections[] = {"Off", "On"};
          menu("Auto Slant Correction", settings.auto_slant_correction, menu_selections, 2);
        }
          break;
        case 1: { //lost signal timeout
          get_timeout_seconds("Lost Signal Timeout", settings.lost_signal_timeout);
        }
          break;
        case 2: { //transmit mode
           const char * const menu_selections[] = {"90%", "75%","50%"};
          menu("Min % for save image", settings.min_completion, menu_selections, 3);
        }
          break;
        case 3: { //transmit mode
          get_transmit_mode(settings.transmit_mode);
        }
          break;
        case 4: { //slideshow_timeout
          get_timeout_seconds("Slideshow Timeout", settings.slideshow_timeout);
        }
          break;
        case 5: { //overlay
          const char * const menu_selections[] = {"Off", "On"};
          menu("Overlay text", settings.overlay, menu_selections, 2);
        }
          break;
        case 6: { //overlay_text
          text_entry(settings.overlay_text, 24);      
        }
          break;
        #ifdef WIFI
        case 7: { //wifi
          const char * const menu_selections[] = {"Off", "On"};
          menu("Wifi", settings.wifi, menu_selections, 2);    
          if (settings.wifi) {
            reconnectWiFiAndClient();
          } else {
            disconnectWiFi();
          }
        }
          break;
        #endif
      }
    }
    save();
  } 
}

void get_timeout_seconds(const char* title, uint8_t & menu_selection)
{
  const char * const menu_selections[] = {
    "Never",
    "1 Second",
    "2 Seconds",
    "5 Seconds",
    "10 Seconds",
    "30 Seconds",
    "1 Minute",
    "2 Minutes",
    "5 Minutes",
  };
  menu(title, menu_selection, menu_selections, 9);
}

void get_transmit_mode(uint8_t & menu_selection)
{


  menu("Transmit Mode", menu_selection, tx_modes, 16);
}

bool menu(const char* title, uint8_t &selection, const char * const menu_items[], uint8_t num_selections)
{
  const uint8_t num_menu_items = num_selections;
  const uint8_t num_items_on_screen = 7;
  uint8_t offset = 0;
  uint8_t menu_item = selection;
  bool draw = true;

  if(menu_item > offset+num_items_on_screen-1) {
    offset = menu_item - num_items_on_screen;
  }

  display->fillRect(0, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, COLOUR_BLACK);
  display->fillRoundedRect(20, 0, 20, DISPLAY_WIDTH-40, 5, COLOUR_BLUE);
  display->drawRoundedRect(20, 0, 20, DISPLAY_WIDTH-40, 5, COLOUR_WHITE);
  uint16_t width = strlen(title)*12;
  display->drawString((DISPLAY_WIDTH-width)/2, 2, font_16x12, title, COLOUR_WHITE, COLOUR_BLUE);
  draw_button_bar("OK", "Cancel", "Up", "Down");
  
  while(1) {
    
    #ifdef WIFI
    if (settings.wifi) poll_wifi();
    #endif

    if(button_down.is_pressed() && menu_item > 0){menu_item--; draw = true;} 
    if(button_up.is_pressed() && menu_item < num_menu_items-1){menu_item++; draw = true;}
    if(button_left.is_pressed()){selection = menu_item; return true;} //ok
    if(button_right.is_pressed()) return false; //cancel
    if(menu_item < offset) {display->fillRect(0, 20, 200, 320, COLOUR_BLACK); offset--;} 
    if(menu_item > offset+num_items_on_screen-1) {display->fillRect(0, 20, 200, 320, COLOUR_BLACK); offset++;}

    if(draw) {
      display->fillRect(0, 20, 200, 20, COLOUR_BLACK);
      for(uint8_t idx=0; idx < num_items_on_screen; ++idx) {
        const uint8_t menu_item_index = idx + offset;
        if(menu_item_index < num_selections)
        {
          const bool active = menu_item == menu_item_index;
          if(active) display->fillCircle(10, 40 + ((idx)*25), 5, COLOUR_BLUE);
          display->drawString(40, 32 + ((idx)*25), font_16x12, menu_items[menu_item_index],  active?COLOUR_BLUE:COLOUR_GREY, COLOUR_BLACK);
        } 
      }
      draw = false;
    }
  }
}

button* buttons[] = {&button_left, &button_right, &button_down, &button_up};
static char char_select[4][4][4] = {
{{'A', 'B', 'C', 'D'},
 {'E', 'F', 'G', 'H'},
 {' ', ' ', ' ', '_'},
 {'<', '>', '#', '!'},},
{{'I', 'J', 'K', 'L'},
 {'M', 'N', 'O', 'P'},
 {'Q', ' ', ' ', '_'},
 {'<', '>', '#', '!'},},
{{'R', 'S', 'T', 'U'},
 {'V', 'W', 'X', 'Y'},
 {'Z', ' ', ' ', '_'},
 {'<', '>', '#', '!'},},
{{'0', '1', '2', '3'},
 {'4', '5', '6', '7'},
 {'8', '9', ' ', '_'},
 {'<', '>', '#', '!'},}};

uint8_t get_char0()
{
  display->fillRect(0, 120, 120, 320, COLOUR_BLACK);
  for(uint8_t i=0; i<4; i++) {
    for(uint8_t j=0; j<4; j++) {
      char disp[20];
      snprintf(disp, 20, " %c%c%c%c", char_select[i][j][0], char_select[i][j][1],char_select[i][j][2],char_select[i][j][3]);
      display->drawString(10+i*75, 120+j*20, font_16x12, disp, COLOUR_WHITE, COLOUR_BLACK);
    }
  }

  while(1) {
    for(uint8_t i=0; i<4; i++) {
      if(buttons[i]->is_pressed()) return i; 
    }
  }
}

uint8_t get_char1(uint8_t sel1)
{
  display->fillRect(0, 120, 120, 320, COLOUR_BLACK);
  for(uint8_t i=0; i<4; i++) {
      char disp[20];
      snprintf(disp, 20, " %c%c%c%c", char_select[sel1][i][0], char_select[sel1][i][1],char_select[sel1][i][2],char_select[sel1][i][3]);
      display->drawString(10+i*75, 120, font_16x12, disp, COLOUR_WHITE, COLOUR_BLACK);
  }
  while(1) {
    for(uint8_t i=0; i<4; i++) {
      if(buttons[i]->is_pressed()) return i; 
    }
  }
}

uint8_t get_char2(uint8_t sel0, uint8_t sel1)
{
  display->fillRect(0, 120, 120, 320, COLOUR_BLACK);
  if(sel1==3) {
    display->drawString(16, 120, font_16x12, " LEFT RIGHT ENTER CLEAR", COLOUR_WHITE, COLOUR_BLACK);
  } else {
    for(uint8_t i=0; i<4; i++) {
      char disp[20];
      snprintf(disp, 20, "%c", char_select[sel0][sel1][i]);
      display->drawString(10+i*75, 120, font_16x12, disp, COLOUR_WHITE, COLOUR_BLACK);
    }
  }
  while(1) {
    for(uint8_t i=0; i<4; i++) {
      if(buttons[i]->is_pressed()) return i; 
    }
  }
}

void text_entry(char string[], uint8_t n)
{
  uint8_t cursor = 0;
  while(1) {
    display->clear(COLOUR_BLACK);
    display->drawRect((DISPLAY_WIDTH-(n*12))/2, 20, 16, n*12, COLOUR_NAVY);
    display->drawString((DISPLAY_WIDTH-(n*12))/2, 20, font_16x12, string, COLOUR_WHITE, COLOUR_NAVY);
    display->drawRect((DISPLAY_WIDTH-(n*12))/2 + cursor*12, 20, 16, 12, COLOUR_RED);
    uint8_t sel0 = get_char0();
    uint8_t sel1 = get_char1(sel0);
    uint8_t sel2 = get_char2(sel0, sel1);
    char entry = char_select[sel0][sel1][sel2];
    if(entry == '<'){
      cursor--;
    } else if (entry == '>'){
      cursor++;
    } else if (entry == '#'){
      return;
    } else if (entry == '!'){
      for(uint8_t i=0; i<n; i++) string[i] = 0;
      cursor=0;
    } else if (entry == '_'){
      string[cursor++] = ' ';
    } else {
      string[cursor++] = entry;
    }
    if(cursor == n) return;
    cursor %= n;
  }
}

void rst_entry(char string[])
{
  uint8_t cursor = 0;
  uint8_t n=3;
  display->clear(COLOUR_BLACK);
  display->drawString((DISPLAY_WIDTH-(18*12))/2, 90, font_16x12, "Select RST (or 73)", COLOUR_YELLOW, COLOUR_BLACK);
  draw_button_bar("<", ">", "+", "-");

  while(1) {
    
    display->drawRect((DISPLAY_WIDTH-(n*12))/2, 120, 16, n*12, COLOUR_NAVY);
    display->drawString((DISPLAY_WIDTH-(n*12))/2, 120, font_16x12, string, COLOUR_WHITE, COLOUR_NAVY);
    display->drawRect((DISPLAY_WIDTH-(n*12))/2 + cursor*12, 120, 16, 12, COLOUR_RED);
    
    if(button_down.is_pressed()) string[cursor]++; 
    if(button_up.is_pressed()) string[cursor]--;
    if(button_left.is_pressed()) cursor--;
    if(button_right.is_pressed()) cursor++;

    if (cursor<0) cursor=0;
    if (string[cursor]<' ') string[cursor]=' ';
    else if (string[cursor]=='/') string[cursor]=' ';
    else if (string[cursor]=='!') string[cursor]='0';
    else if (string[cursor]>'9') string[cursor]='9';
    if(cursor == n) return;
    cursor %= n;
    delay(10);
  }
}

#define version 100

void save() {
  EEPROM.put(sizeof(settings), settings);
  uint32_t scores_stored = 0;
  EEPROM.get(0, scores_stored);
  if(scores_stored != version) EEPROM.put(0, version);
  EEPROM.commit();
}

void load() {
  uint32_t scores_stored = 0;
  EEPROM.get(0, scores_stored);
  if(scores_stored == version) EEPROM.get(sizeof(settings), settings);
}

void create_thumbnail(const char* filename)
{
  c_bmp_reader_stdio bitmap;
  uint16_t width, height;
  bitmap.open(filename, width, height);
  

  const uint16_t t_width = width/3, ty_height = height/3;
  uint16_t tft_row_number = 0;

  for(uint16_t y=0; y<height; y++) {
    uint16_t line_rgb565[width];
    bitmap.read_row_rgb565(line_rgb565);

    uint16_t pixel_number = 0;
     
      for(uint16_t x=0; x<width; x+=3) {
       
        //while(pixel_number <= x) {
          //display expects byteswapped data
          uint16_t pixel=((line_rgb565[x] & 0xff) << 8) | ((line_rgb565[x] & 0xff00) >> 8);
          scaled_image[x/3+((y/3)*t_width)] = pixel;
         // pixel_number++;
        }
      } 
  
  bitmap.close();
}

/////////////////////////////////////////////////////////

#ifdef WIFI

bool isImage(String name) {
  name.toLowerCase();
  return name.endsWith(".bmp");
}


void sendImage(WiFiClient &client, String filename) {
  FILE* file = fopen(filename.c_str(), "rb");
  if (file==NULL) {
    client.println("HTTP/1.1 404 Not Found\r\n");
    client.println("Content-Type: text/plain\r\n\r\nFile not found");
    return;
  }

  String contentType = "application/octet-stream";
  contentType = "image/bmp";

  client.println("HTTP/1.0 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();
  int n;
  uint8_t buffer[1024];
  while ((n = fread(buffer, 1, sizeof(buffer) - 1, file)) > 0){
    client.write(buffer, n);
  } 
  fclose(file);
}
void send404(WiFiClient &client) {
  client.println("HTTP/1.0 404 Not found");
  client.println();
}

void sendGallery(WiFiClient &client, int page, const char* folder) {
  client.println("HTTP/1.0 200 OK");
  client.println("Connection: close");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  client.println("<title>Galleria SD</title>");
  client.println("<style>body{background: antiquewhite;}.foto{float:left;border:1px lightgray solid;padding: 5px;margin:10px;border-radius: 10px;background:white;height:322px;}img{margin:20px;width:320px;border:2px black solid}</style></head>");
  client.println("<body><h1>Galleria immagini su SD</h1><hr><h2>Pagina ");
  client.print(page);
  client.print("</h2><h2> Folder ");
  client.print(folder);
  client.println("</h2><div style='display: inline flow-root list-item;'>");

  Dir root = SDFS.openDir(folder);
  int num=count_bitmaps(root);
  root.rewind();
  int n=0;
  int disp=0;

  while((root.next())&&(disp<4)) {
    String name = root.fileName();;
    if (isImage(name)) {
      if (n>=page*4) {
        client.print("<div class='foto'>");
        client.print("<a href=?del=");
        client.print(name);
        client.print("><button style='width:100%'>Delete ");
        client.print(name);
        client.print("</button></a></br>");
        client.print("<a href='img");
        client.print(folder);
        client.print("/");
        client.print(name);
        client.print("'><img src='img");
        client.print(folder);
        client.print("/");
        client.print(name);
        client.print("' /></a>");
       
        client.print("</div>");
        disp++;
      }
      n++;
    }
  }
  client.print("</div><hr>");

  for (int i=0;i<=num/4;i++) {
    client.println("<a href='?page=");
    client.print(i);
    client.println("'><button style='margin:5px;'>Page ");
    client.print(i);
    client.print("</button></a>");
  }

  client.println("</body></html>");
  
}

void poll_wifi() {
  static int page=0;
  WiFiClient client = server.accept();
 
  if (!client) return;

  String request = client.readStringUntil('\r');
  client.readStringUntil('\n');
  request=request.substring(5,request.length()-8); //Remove "GET /" and "HTTP 1.1"

  if (request.startsWith("img/")) {
    int pos = request.indexOf('/');
    String filename = request.substring(4);
    filename.trim();
    Serial.println(filename);
    sendImage(client, filename);
  } else
  if (request.startsWith("?del=")) {
    String filename = request.substring(5);
    Serial.print("deleting ");
    Serial.println(filename);
    SDFS.remove(filename);
    sendGallery(client,page,"/");
  } else if (request.startsWith("?page=")) {
    page=request.substring(6).toInt();
    sendGallery(client,page,"/");
  } else if (request.startsWith("favicon.ico")) {
    send404(client);
  } else if (request.startsWith("?tx=")) {
    page=request.substring(4).toInt();
     sendGallery(client,page,"/tx");
  } else 
   {
    sendGallery(client,page,"/");
  }

  delay(1);
  client.flush();

  while (client.available()) {
    client.read();
  }
  delay(100);
  client.stop();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.setTimeout(5000);
  WiFi.begin(ssid, password);
}

// Function to disconnect WiFi and turn off WiFi chip power
void disconnectWiFi() {
  Serial.println("Disconnecting WiFi...");  
  WiFi.disconnect();               // Disconnect WiFi
  delay(100);                      // Wait a bit
  WiFi.mode(WIFI_OFF);             // Turn off WiFi mode
  delay(100);                      // Wait a bit
  digitalWrite(23, LOW);           // Turn off WiFi chip power
  Serial.println("WiFi disconnected and power to WiFi chip is off.");
}

// Function to reconnect WiFi and WiFiClient
void reconnectWiFiAndClient() {
  digitalWrite(23, HIGH);          // Turn on WiFi chip power
  delay(100);                      // Wait for stabilization
  Serial.println("Reconnecting WiFi...");
  WiFi.mode(WIFI_STA);             // Set WiFi mode to STA
  connectToWiFi();  
}  // Reconnect to WiFi
#endif