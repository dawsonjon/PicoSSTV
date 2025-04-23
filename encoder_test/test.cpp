#include "../sstv_library/bmp_lib.h"
#include "../sstv_library/sstv_encoder.h"
#include <cstdio>
#include <cstdlib>

//define a class to read bitmaps from a file
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

    void file_read(void* data, uint32_t element_size, uint32_t num_elements)
    {
        fread(data, element_size, num_elements, f);
    }

    void file_seek(uint32_t offset)
    {
        fseek(f, offset, SEEK_SET);
    }

    FILE* f;
};

//override methods of sstv encoder so that it can read from a bitmap file and write audio to a pcm file
class c_sstv_encoder_fileio : public c_sstv_encoder
{

  private :

  void output_sample(int16_t sample)
  {
    fwrite(&sample, 2, 1, pcm);
  }

  uint8_t get_image_pixel(uint16_t width, uint16_t height, uint16_t y, uint16_t x, uint8_t colour) 
  {
    if(y > row_number)
    {
      bitmap.read_row_rgb565(row);
      row_number++;
    }

    uint16_t pixel = row[x];
    if(colour == 0) return ((pixel >> 11) & 0x1F) << 3; //r
    else if(colour == 1) return ((pixel >> 5) & 0x3F) << 2; //g
    else if(colour == 2) return (pixel & 0x1F) << 3; //b
    else return 0;
  }

  c_bmp_reader_stdio bitmap;
  FILE *pcm;
  uint16_t row_number = 0;
  uint16_t row[320];
  uint16_t image_width, image_height;

  public:
  void open(const char * bmp_file_name, const char* pcm_file_name)
  {
    bitmap.open(bmp_file_name, image_width, image_height);
    bitmap.read_row_rgb565(row);
    row_number=0;
    pcm = fopen(pcm_file_name, "wb");
  }
  void close()
  {
    bitmap.close();
    fclose(pcm);
  }

};

int main()
{

  const uint16_t width = 320;
  const uint16_t height = 240;
  c_sstv_encoder_fileio sstv_encoder;
  sstv_encoder.open("input.bmp", "output.pcm");
  sstv_encoder.generate_sstv(martin, width, height);
  sstv_encoder.close();
  return 0;

}
