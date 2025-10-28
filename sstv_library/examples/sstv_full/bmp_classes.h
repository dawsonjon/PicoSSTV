#include "bmp_lib.h"

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