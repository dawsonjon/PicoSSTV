//  _  ___  _   _____ _     _                 
// / |/ _ \/ | |_   _| |__ (_)_ __   __ _ ___ 
// | | | | | |   | | | '_ \| | '_ \ / _` / __|
// | | |_| | |   | | | | | | | | | | (_| \__ \.
// |_|\___/|_|   |_| |_| |_|_|_| |_|\__, |___/
//                                  |___/    
//
// Copyright (c) Jonathan P Dawson 2025
// filename: sstv_encoder.ino
// description:
//
// SSTV Encoder using pi-pico.
//
// Reads bmp image from SD card and transmits via SSTV
// Supports Martin and Scottie modes
//
// License: MIT

#include <sstv_encoder.h>
#include <PWMAudio.h>
#include <bmp_lib.h>
#include <SPI.h>
#include <SDFS.h>
#include <VFS.h>

//CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////

const int SDCARD_MISO = 4;
const int SDCARD_MOSI = 7;
const int SDCARD_CS = 5;
const int SDCARD_SCK = 6;

//END OF CONFIGURATION SECTION
///////////////////////////////////////////////////////////////////////////////



void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);  // wait for serial port to connect.

  Serial.println("Pico SSTV Copyright (C) Jonathan P Dawson 2025");
  Serial.println("github: https://github.com/dawsonjon/101Things");
  Serial.println("docs: 101-things.readthedocs.io");
  
  initialise_sdcard();
  VFS.root(SDFS);

}

//Derive a class from bitmap reader and override hardware specific functions
class c_bmp_reader_stdio : public c_bmp_reader
{
    bool file_open(const char* filename)
    {
        Serial.println("opening testcard.bmp");
        f = fopen(filename, "rb");
        if(f==NULL) Serial.println("failed to open testcard.bmp");
        return f != NULL;
    }

    void file_close()
    {
        Serial.println("closing testcard.bmp");
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
      //Serial.println(sample_min);
      //Serial.println(sample_max);
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

void loop() {
  const uint16_t divider = rp2040.f_cpu()/15000;
  const float sample_rate_Hz = (double)rp2040.f_cpu()/divider;
  Serial.println(sample_rate_Hz);
  c_sstv_encoder_pwm sstv_encoder(sample_rate_Hz);
  sstv_encoder.open("test_card.bmp");
  sstv_encoder.generate_sstv(martin, 320, 240);
  sstv_encoder.close();
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


