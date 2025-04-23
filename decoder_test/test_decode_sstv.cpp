#include <cstdint>
#include <iostream>

#include "../sstv_library/decode_sstv.h"


int main()
{
  c_sstv_decoder decoder(15000);

  while(1)
  {

    uint16_t sample;
    std::cin.clear();
    std::cin >> sample;

    bool pixel_complete;
    uint16_t line, col;
    uint8_t colour, pixel;
    e_state state;

    pixel_complete = decoder.decode(sample, line, col, colour, pixel, state);

    std::cout << pixel_complete << " " << line << " " << col << " " << (int)colour << " " << (int)pixel << " " << (int)state << std::endl;

  }
}
