#include "bmp_lib.h"
#include <cstdlib>
#include <cstdio>

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // File type ("BM")
    uint32_t bfSize;      // Size of the file (in bytes)
    uint16_t bfReserved1; // Reserved (0)
    uint16_t bfReserved2; // Reserved (0)
    uint32_t bfOffBits;   // Offset to image data (54 bytes for 24-bit)
} BMPFileHeader;

typedef struct {
    uint32_t biSize;          // Size of this header (40 bytes)
    int32_t  biWidth;         // Image width
    int32_t  biHeight;        // Image height
    uint16_t biPlanes;        // Color planes (must be 1)
    uint16_t biBitCount;      // Bits per pixel (24 for RGB)
    uint32_t biCompression;   // Compression (0 = uncompressed)
    uint32_t biSizeImage;     // Image size (including padding)
    int32_t  biXPelsPerMeter; // Horizontal resolution (pixels/meter)
    int32_t  biYPelsPerMeter; // Vertical resolution (pixels/meter)
    uint32_t biClrUsed;       // Colors used (0 = all)
    uint32_t biClrImportant;  // Important colors (0 = all)
} BMPInfoHeader;
#pragma pack(pop)


void c_bmp_writer :: open(const char* filename, uint16_t width, uint16_t height)
{
    if(!file_open(filename)) return;

    m_width = width;
    m_height = height;
    m_width_padded = (width * 3 + 3) & ~3;
    m_image_size = m_width_padded * height;
    m_y = 0;

    BMPFileHeader file_header = {
        .bfType = 0x4D42,
        .bfSize = static_cast<uint32_t>(sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + m_image_size),
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader),
    };

    BMPInfoHeader info_header = {
        .biSize = sizeof(BMPInfoHeader),
        .biWidth = width,
        .biHeight = -(int32_t)height,
        .biPlanes = 1,
        .biBitCount = 24,
        .biCompression = 0,
        .biSizeImage = 0,
        .biXPelsPerMeter = 0x0B13,
        .biYPelsPerMeter = 0x0B13,
        .biClrUsed = 0,
        .biClrImportant = 0,
    };

    file_write(&file_header, sizeof(file_header), 1);
    m_file_size = sizeof(file_header);
    file_write(&info_header, sizeof(info_header), 1);
    m_file_size += sizeof(info_header);

}

//Usually width and height should be set when opening, but in SSTV applications, it might be necassary
//to open the file ahead of time, then change the width only after the mode has been determined. This
//must still be done before writing the first row.
void c_bmp_writer :: change_width(uint16_t width)
{
    m_width = width;
    m_width_padded = (width * 3 + 3) & ~3;
    m_image_size = m_width_padded * m_height;
}

//It might also be helpful to change the height after the file is opened, we can't tell how many rows
//we will be sent ahead of time. The height can be changed any time before the file is closed.
void c_bmp_writer :: change_height(uint16_t height)
{
    m_height = height;
    m_image_size = m_width_padded * height;
}

//If the width or height change, it will be necassary to update the file header
void c_bmp_writer :: update_header()
{

    BMPFileHeader file_header = {
        .bfType = 0x4D42,
        .bfSize = m_file_size,
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader),
    };

    BMPInfoHeader info_header = {
        .biSize = sizeof(BMPInfoHeader),
        .biWidth = m_width,
        .biHeight = -(int32_t)m_height,
        .biPlanes = 1,
        .biBitCount = 24,
        .biCompression = 0,
        .biSizeImage = 0,
        .biXPelsPerMeter = 0x0B13,
        .biYPelsPerMeter = 0x0B13,
        .biClrUsed = 0,
        .biClrImportant = 0,
    };

    file_seek(0);
    file_write(&file_header, sizeof(file_header), 1);
    file_write(&info_header, sizeof(info_header), 1);
}

void c_bmp_writer :: close() 
{
    file_close();
}

void c_bmp_writer :: write_row_rgb565(uint16_t* rgb565_data) 
{
    uint8_t row[m_width_padded];

    for (int x = 0; x < m_width; ++x) {
        uint16_t pixel = rgb565_data[x];
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;

        // Optional: Add lower bits back in for smoother tone
        r |= r >> 5;
        g |= g >> 6;
        b |= b >> 5;

        row[x * 3 + 0] = b;
        row[x * 3 + 1] = g;
        row[x * 3 + 2] = r;
    }

    // Pad the rest of the row
    for (int i = m_width * 3; i < m_width_padded; i++) {
        row[i] = 0;
    }

    file_write(row, m_width_padded, 1);
    m_file_size += m_width_padded;
    m_y++;
}

uint8_t c_bmp_reader :: open(const char* filename, uint16_t &width, uint16_t &height) 
{
    if(!file_open(filename)) return -1;
    BMPFileHeader file_header;
    file_read(&file_header, sizeof(file_header), 1);
    if (file_header.bfType != 0x4D42) {
        file_close();
        return -2;
    }

    BMPInfoHeader info_header;
    file_read(&info_header, sizeof(info_header), 1);

    m_width = width = info_header.biWidth;
    m_height = height = abs(info_header.biHeight);  // Support top-down and bottom-up
    m_bpp = info_header.biBitCount;
    m_top_down = (info_header.biHeight < 0);
    m_row_bytes = (((width * m_bpp) + 31) / 32) * 4;
    m_start_of_image = file_header.bfOffBits;
    m_y = 0;

    if (m_bpp == 8) {
        uint16_t num_colors = info_header.biClrUsed ? info_header.biClrUsed : 256;
        file_read(m_palette, 4, num_colors);  // Each palette entry: BGRA
    } else if (m_bpp == 24 ) {
    } else if (m_bpp == 32 ) {
    } else return -3; //unsupported bmp file

    file_seek(m_start_of_image);
    
    return 1;

}

void c_bmp_reader :: read_row_rgb565(uint16_t *rgb565_data) 
{
    uint8_t row[m_row_bytes];
    if(!m_top_down)
    {
      uint32_t start_of_row = m_start_of_image + ((m_height - 1 - m_y) * m_row_bytes);
      file_seek(start_of_row);
    }
    uint32_t bytes_read = file_read(row, 1, m_row_bytes);

    for (int x = 0; x < m_width; x++) {
        uint8_t r = 0, g = 0, b = 0;

        if (bytes_read < m_row_bytes){
            b = 0xff;
            g = 0x00;
            r = 0x00;
        } else if(m_bpp == 24) {
            b = row[x * 3 + 0];
            g = row[x * 3 + 1];
            r = row[x * 3 + 2];
        } else if (m_bpp == 32) {
            b = row[x * 4 + 0];
            g = row[x * 4 + 1];
            r = row[x * 4 + 2];
        } else if (m_bpp == 8) {
            uint8_t index = row[x];
            uint32_t color = m_palette[index];
            b = (color >> 0) & 0xFF;
            g = (color >> 8) & 0xFF;
            r = (color >> 16) & 0xFF;
        }

        uint16_t pixel = ((r & 0xF8) << 8) |
                         ((g & 0xFC) << 3) |
                         ((b & 0xF8) >> 3);
        rgb565_data[x] = pixel;
    }
    m_y++;
}

void c_bmp_reader :: close() 
{
    file_close();
}
