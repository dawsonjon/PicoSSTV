#include <cstdint>

#ifndef __BMP_LIB_H__
#define __BMP_LIB_H__

class c_bmp_writer
{
  public:
  void open(const char* filename, uint16_t width, uint16_t height);
  void close();
  void write_row_rgb565(uint16_t* rgb565_data);
  void change_width(uint16_t width);
  void change_height(uint16_t height);
  void update_header();

  private:
  uint16_t m_width;
  uint16_t m_height;
  uint16_t m_width_padded;
  uint32_t m_image_size;
  uint16_t m_y;

  virtual bool file_open(const char* filename)=0;
  virtual void file_close()=0;
  virtual void file_write(const void* data, uint32_t element_size, uint32_t num_elements)=0;
  virtual void file_seek(uint32_t offset)=0;
  virtual uint32_t file_tell()=0;

};

class c_bmp_reader
{
  public:
  uint8_t open(const char* filename, uint16_t &width, uint16_t &height);
  void read_row_rgb565(uint16_t *rgb565_data);
  void close();

  private:
  uint16_t m_width;
  uint16_t m_height;
  uint8_t m_bpp;
  bool m_top_down;
  uint16_t m_row_bytes;
  long int m_start_of_image;
  uint32_t m_palette[256];
  uint16_t m_y;

  virtual bool file_open(const char* filename)=0;
  virtual void file_close()=0;
  virtual uint32_t file_read(void* data, uint32_t element_size, uint32_t num_elements)=0;
  virtual void file_seek(uint32_t offset)=0;

};


#endif
