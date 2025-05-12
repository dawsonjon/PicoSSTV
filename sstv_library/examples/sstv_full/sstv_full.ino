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
#include "sstv_encoder.h"
#include "ADCAudio.h"
#include "PWMAudio.h"
#include "splash.h"
#include <bmp_lib.h>
#include <SPI.h>
#include <SDFS.h>
#include <VFS.h>
#include "button.h"

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

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

void draw_banner(const char* message);
void draw_status_bar(const char* message);
void configure_display();
void initialise_sdcard();
void get_new_filename(char *buffer, uint16_t buffer_size);
void update_slideshow();
void display_image(const char* filename);
void get_timeout_seconds(const char* title, uint8_t & menu_selection);

ILI934X *display;
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define STATUS_BAR_HEIGHT 20

button button_up(17); 
button button_down(20); 
button button_right(21); 
button button_left(22);

enum e_view_mode {rx_mode, slideshow_mode};
struct s_settings {
  uint8_t slideshow_timeout;
  uint8_t lost_signal_timeout;
  uint8_t transmit_mode;
};
s_settings settings = {
  3, //5 seconds
  5,  //30 seconds
  1
};

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

    FILE* f;
};

//Derive a class from bitmap reader and override hardware specific functions
class c_bmp_reader_stdio : public c_bmp_reader
{
    bool file_open(const char* filename)
    {
        f = fopen(filename, "rb");
        return f != NULL;
    }

    void file_close()
    {
        fclose(f);
    }

    uint32_t file_read(void* data, uint32_t element_size, uint32_t num_elements)
    {
      return fread(data, element_size, num_elements, f);
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
  const uint16_t display_width = DISPLAY_WIDTH;
  const uint16_t display_height = DISPLAY_HEIGHT - STATUS_BAR_HEIGHT; //allow space for status bar

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
    char buffer[21];
    snprintf(buffer, 21, "%10s: %ux%u", mode_string, width, y+1);
    draw_status_bar("RX Incoming ...");
    draw_banner(buffer);
    Serial.println(buffer);

  }

  c_bmp_writer_stdio output_file;
  uint16_t bmp_row_number = 0;

  //These functions don't need to do anything when accessing a TFT display
  void image_open(const char* bmp_file_name, uint16_t width, uint16_t height, const char* mode_string){
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.print("opening output bmp file: ");
    Serial.println(bmp_file_name);
    output_file.open(bmp_file_name, width, height);
  }

  void image_close(){
    tft_row_number = 0;
    bmp_row_number = 0;
    Serial.println("closing bmp file");
    draw_status_bar("RX Complete!");
    output_file.close();
  }

  public:

  void start(){adc_audio.begin(28, 15000);}
  void stop(){adc_audio.end();}
  c_sstv_decoder_fileio(float fs) : c_sstv_decoder{fs}{}

};

//Derive a class from sstv encoder and override hardware specific functions
const uint16_t audio_buffer_length = 4096u;
class c_sstv_encoder_pwm : public c_sstv_encoder
{

  private :
  uint16_t audio_buffer[2][audio_buffer_length];
  uint16_t audio_buffer_index = 0;
  uint8_t ping_pong = 0;
  PWMAudio audio_output;
  uint16_t sample_min, sample_max;

  void output_sample(int16_t sample)
  {
    uint16_t scaled_sample = ((sample+32767)>>5);// + 1024;
    audio_buffer[ping_pong][audio_buffer_index++] = scaled_sample;
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

    while(image_y > row_number)
    {
      bitmap.read_row_rgb565(row);
      row_number++;
      draw_status_bar("   Cancel   ");
      char status[100];
      snprintf(status, 100, "transmitting %u/%u (%u%%)", y+1, height, (100*(y+1))/height);
      draw_banner(status);
    }

    uint16_t pixel = row[image_x];
    if(colour == 0) return ((pixel >> 11) & 0x1F) << 3;     //r 
    else if(colour == 1) return ((pixel >> 5) & 0x3F) << 2; //g
    else if(colour == 2) return (pixel & 0x1F) << 3;        //b
    else return 0;
  }
  
  c_bmp_reader_stdio bitmap;
  FILE *pcm;
  uint16_t row_number = 0;
  uint16_t row[320];
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

void setup() {
  Serial.begin(115200);

  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2025");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
  
  pinMode(LED_BUILTIN, OUTPUT);
  configure_display();
  initialise_sdcard();
  VFS.root(SDFS);

}

void loop() {
  c_sstv_decoder_fileio sstv_decoder(15000);
  sstv_decoder.start();
  char rx_filename[100];
  get_new_filename(rx_filename, 100);

  bool image_in_progress = false;
  bool image_complete = false;
  e_view_mode view_mode = slideshow_mode;

  while(1){
    
    //process rx regardless of mode
    static const uint16_t timeouts[] = {UINT16_MAX, 1, 2, 5, 10, 30, 60, 60*2, 60*5};
    const uint16_t timeout_seconds = timeouts[settings.lost_signal_timeout];
    image_complete = sstv_decoder.decode_image_non_blocking(rx_filename, timeout_seconds, ENABLE_SLANT_CORRECTION, image_in_progress);
    if(image_complete) get_new_filename(rx_filename, 100);
    if(image_in_progress)
    {
      view_mode = rx_mode;
    }
    else
    {
      if(button_left.is_pressed()) launch_menu(view_mode);
    }
    
    if(view_mode == slideshow_mode)
    {
      update_slideshow();
    }

  }
  sstv_decoder.stop();
}

void draw_splash_screen()
{
  display->writeImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, splash);
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
  do{
    snprintf(buffer, buffer_size, "sstv_rx_%u.bmp", serial_number);
    serial_number++;
  } while(SDFS.exists(buffer));
}

void draw_banner(const char* message)
{
  uint16_t width = strlen(message)*6+10;
  display->fillRoundedRect((DISPLAY_WIDTH-width)/2, 3, 10, width, 3, 0);
  display->drawString((DISPLAY_WIDTH-width)/2+5, 4, font_8x5, message, COLOUR_WHITE, COLOUR_BLACK);
}
void draw_status_bar(const char* message)
{
  display->fillRect(0, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT-1, STATUS_BAR_HEIGHT, DISPLAY_WIDTH, COLOUR_BLACK);
  #define MARGIN ((STATUS_BAR_HEIGHT - 8)/2)
  display->drawString(MARGIN, DISPLAY_HEIGHT-STATUS_BAR_HEIGHT+MARGIN, font_8x5, message, COLOUR_WHITE, COLOUR_BLACK);
}

void update_slideshow()
{
  bool redraw = false;
  uint16_t filename_size = 100;
  char filename[filename_size];
  static uint16_t serial_number = 0;
  
  static const uint16_t timeouts[] = {0, 1, 2, 5, 10, 30, 60, 60*2, 60*5};
  uint16_t timeout_milliseconds = 1000 * timeouts[settings.slideshow_timeout];
  static uint32_t last_update_time = 0;
  if(((millis() - last_update_time) > timeout_milliseconds) && (timeout_milliseconds != 0))
  {
    last_update_time = millis();
    serial_number++;
    snprintf(filename, filename_size, "sstv_rx_%u.bmp", serial_number);
    if(!SDFS.exists(filename)) serial_number = 0;
    snprintf(filename, filename_size, "sstv_rx_%u.bmp", serial_number);
    if(!SDFS.exists(filename)) return;
    redraw = true;
  }

  if(button_up.is_pressed())
  {
    last_update_time = millis();
    serial_number++;
    snprintf(filename, filename_size, "sstv_rx_%u.bmp", serial_number);
    if(!SDFS.exists(filename)) serial_number = 0;
    snprintf(filename, filename_size, "sstv_rx_%u.bmp", serial_number);
    if(!SDFS.exists(filename)) return;
    redraw = true;
  }

  if(button_down.is_pressed())
  {
    last_update_time = millis();
    if(serial_number == 0)
    {
      
      //first file doesn't exit so quit
      snprintf(filename, filename_size, "sstv_rx_%u.bmp", serial_number);
      if(!SDFS.exists(filename)) return;
      
      //find the first filename that doesn't exist
      do
      {
        snprintf(filename, filename_size, "sstv_rx_%u.bmp", ++serial_number);
      } while(SDFS.exists(filename));

    }
    snprintf(filename, filename_size, "sstv_rx_%u.bmp", --serial_number);
    redraw = true;
  }

  if(redraw)
  {
    display_image(filename);
    uint16_t width = strlen(filename)*6+10;
    draw_banner(filename);
    draw_status_bar("     Menu                <- Previous      Next ->  ");
  }
}

uint16_t count_bitmaps(Dir &root)
{
  uint16_t count = 0;
  root.rewind();
  while(root.next())
  {
    String filename = root.fileName();
    if(root.isFile() && filename.endsWith(".bmp")) count++; 
  }
  return count;
}

void get_bitmap_index(Dir &root, uint16_t index)
{
  uint16_t count = 0;
  root.rewind();
  while(root.next())
  {
    String filename = root.fileName();
    if(root.isFile() && filename.endsWith(".bmp"))
    {
      if(count == index) return;
      count++;
    }
  }
}

void transmit_image(const char* filename)
{
  const uint16_t divider = rp2040.f_cpu()/15000;
  const float sample_rate_Hz = (double)rp2040.f_cpu()/divider;
  Serial.println(sample_rate_Hz);
  c_sstv_encoder_pwm sstv_encoder(sample_rate_Hz);
  sstv_encoder.open(filename);
  digitalWrite(LED_BUILTIN, 1);
  sstv_encoder.generate_sstv((e_sstv_tx_mode)settings.transmit_mode);
  digitalWrite(LED_BUILTIN, 0);
  sstv_encoder.close();
}

void tx_file_browser()
{
  bool redraw = true;
  Dir root = SDFS.openDir("/");
  const uint16_t num_bitmaps = count_bitmaps(root);
  if(num_bitmaps == 0) return;
  uint16_t bitmap_index = 0;
  String filename;
  
  while(1)
  {
    
    if(button_up.is_pressed())
    {
      if(bitmap_index == num_bitmaps-1) bitmap_index = 0;
      else bitmap_index++;
      redraw = true;
    }

    if(button_down.is_pressed())
    {
      if(bitmap_index == 0) bitmap_index = num_bitmaps-1;
      else bitmap_index--;
      redraw = true;
    }

    if(redraw)
    {
      get_bitmap_index(root, bitmap_index);
      filename = root.fileName();
      Serial.println(filename);
      display_image(filename.c_str());
      draw_banner(filename.c_str());
      draw_status_bar("  Transmit      Cancel   <- Previous       Next ->");
      redraw = false;
    }

    if(button_left.is_pressed())
    {
      Serial.println("transmitting");
      transmit_image(filename.c_str());
      return;
    }

    if(button_right.is_pressed())
    {
      return;
    }
  }

}

void display_image(const char* filename)
{
  c_bmp_reader_stdio bitmap;
  uint16_t width, height;
  bitmap.open(filename, width, height);

  const uint16_t display_width = DISPLAY_WIDTH, display_height = DISPLAY_HEIGHT-STATUS_BAR_HEIGHT;
  uint16_t tft_row_number = 0;

  for(uint16_t y=0; y<height; y++)
  {
    uint16_t line_rgb565[width];
    bitmap.read_row_rgb565(line_rgb565);

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
  }

  bitmap.close();
}

void launch_menu(e_view_mode &view_mode)
{
  uint8_t menu_selection = 0;
  const char * const menu_selections[] = {
    "Recieve",
    "Transmit",
    "Slideshow",
    "Settings"
  };
  menu("Menu", menu_selection, menu_selections, 4);

  if(menu_selection == 0)
  {
    view_mode = rx_mode;
    display->clear(COLOUR_NAVY);
    draw_status_bar("RX waiting...");
    return;
  }
  else if(menu_selection == 1)
  {
    tx_file_browser();
    return;
  }
  else if(menu_selection == 2)
  {
    view_mode = slideshow_mode;
    display->clear(COLOUR_NAVY);
    draw_status_bar("Slideshow");
    return;
  }
  else
  {
    uint8_t menu_selection = 0;
    const char * const menu_selections[] = {
      "Auto Slant Correction",
      "Lost Signal Timeout",
      "Transmit Mode",
      "Slideshow Timeout",
    };
    if(menu("Settings", menu_selection, menu_selections, 4))
    {
      if(menu_selection == 0)//Auto slant correction
      {

      }
      else if(menu_selection == 1)//lost signal timeout
      {
        get_timeout_seconds("Lost Signal Timeout", settings.lost_signal_timeout);
      }
      else if(menu_selection == 2)//transmit mode
      {
        get_transmit_mode(settings.transmit_mode);
      }
      else if(menu_selection == 3)//slideshow_timeout
      {
        get_timeout_seconds("Slideshow Timeout", settings.slideshow_timeout);
      }
    }
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
  const char * const menu_selections[] = {
    "Martin M1",
    "Martin M2",
    "Scottie S1",
    "Scottie S2",
    "PD 50",
    "PD 90",
    "PD 120",
    "PD 180"
  };
  menu("Transmit Mode", menu_selection, menu_selections, 8);
}

bool menu(const char* title, uint8_t &selection, const char * const menu_items[], uint8_t num_selections)
{
  const uint8_t num_menu_items = num_selections;
  const uint8_t num_items_on_screen = 9;
  uint8_t offset = 0;
  uint8_t menu_item = selection;
  bool draw = true;

  display->fillRect(0, 0, 240, 320, 0);
  uint16_t width = strlen(title)*12;
  display->drawString((DISPLAY_WIDTH-width)/2, 0, font_16x12, title, display->colour565(0, 255, 128), 0);
  display->drawFastHline(0, DISPLAY_WIDTH, 30, display->colour565(0, 255, 128)); 
  display->drawFastHline(0, DISPLAY_WIDTH, DISPLAY_HEIGHT-30, display->colour565(0, 255, 128));
  display->drawString(7, DISPLAY_HEIGHT-30+7, font_16x12, "  OK   Cancel  Up   Down ", display->colour565(0, 255, 128), 0);
  
  while(1)
  {

    if(button_down.is_pressed() && menu_item > 0)
    {
      menu_item--;
      draw = true;
    } 
    if(button_up.is_pressed() && menu_item < num_menu_items-1)
    {
      menu_item++;
      draw = true;
    }
    
    //ok
    if(button_left.is_pressed())
    {
      selection = menu_item;
      return true;
    }

    //cancel
    if(button_right.is_pressed()) return false;
    
    if(menu_item < offset) offset--;
    if(menu_item > offset+num_items_on_screen-1) offset++;

    if(draw)
    {
      display->fillRect(0, 31, 179, 320, COLOUR_BLACK);
      for(uint8_t idx=0; idx < num_items_on_screen; ++idx)
      {
        const uint8_t menu_item_index = idx + offset;
        
        if(menu_item_index < num_selections)
        {
          uint16_t colour = menu_item == menu_item_index?display->colour565(255, 0, 255):display->colour565(128, 0, 128);
          uint16_t text_width = strlen(menu_items[menu_item_index])*12;
          display->drawString((DISPLAY_WIDTH-text_width)/2, 32 + ((idx)*20), font_16x12, menu_items[menu_item_index],  colour, COLOUR_BLACK);
        } 
      }
      draw = false;
    }
  }
}