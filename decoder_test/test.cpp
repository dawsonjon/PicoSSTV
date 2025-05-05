#include "../sstv_library/bmp_lib.h"
#include "../sstv_library/sstv_decoder.h"
#include <cstdio>
#include <cstdlib>

//define a class to write bitmaps from a file
class c_bmp_writer_stdio : public c_bmp_writer
{
    bool file_open(const char* filename)
    {
        f = fopen(filename, "wb");
        return f != 0;
    }

    void file_close()
    {
        fclose(f);
    }

    void file_write(const void* data, uint32_t element_size, uint32_t num_elements)
    {
        fwrite(data, element_size, num_elements, f);
    }

    FILE* f;
};

class c_sstv_decoder_fileio : public c_sstv_decoder
{
  
  FILE *pcm;
  c_bmp_writer_stdio bitmap;

  //The decoder can work with frequency data, IQ data or (real, mono) audio samples.
  //override one of these deneding on what you need.
  int16_t get_audio_sample()
  {
    int16_t sample;
    fread(&sample, 2, 1, pcm);
    return sample;
  }

  void image_write_line(uint16_t line_rgb565[], uint16_t y, uint16_t width, uint16_t height)
  {
    bitmap.write_row_rgb565(line_rgb565);
  }

  void image_open(const char* bmp_file_name, uint16_t width, uint16_t height)
  {
    bitmap.open(bmp_file_name, width, height);
  }

  void image_close()
  {
    bitmap.close();
  }

  public:

  void open(const char * pcm_file_name)
  {
    pcm = fopen(pcm_file_name, "rb");
  }
  void close()
  {
    fclose(pcm);
  }
  c_sstv_decoder_fileio(float fs) : c_sstv_decoder{fs}
  {
  }

};

int main(int argc, char ** argv)
{

  if(argc < 3)
  {
    printf("usage: test input.wav output.pcm\n");
    return -1;
  }
  c_sstv_decoder_fileio sstv_decoder(15000);
  printf("%s\n", argv[1]);
  sstv_decoder.open(argv[1]);
  sstv_decoder.decode_image(argv[2], 30, true);
  sstv_decoder.close();
  return 0;

}
