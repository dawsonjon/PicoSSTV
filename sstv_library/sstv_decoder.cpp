#include <cstdint>
#include <algorithm>
#include <cmath>
#include "sstv_decoder.h"
#include "cordic.h"

//from the sample number work out the colour and x/y coordinates
void c_sstv_decoder :: sample_to_pixel(uint16_t &x, uint16_t &y, uint8_t &colour, int32_t image_sample)
{
  //martin and scottie colour order is g-b-r, map to r-g-b
  static const uint8_t colourmap[4] = {1, 2, 0, 4};

  if( decode_mode == martin_m1 || decode_mode == martin_m2 )
  {

    y = image_sample/mean_samples_per_line;
    image_sample -= y*mean_samples_per_line;
    colour = image_sample/modes[decode_mode].samples_per_colour_line;
    image_sample -= colour*modes[decode_mode].samples_per_colour_line;
    colour = colourmap[colour];
    x = image_sample/modes[decode_mode].samples_per_pixel;

  }

  else if( decode_mode == scottie_s1 || decode_mode == scottie_s2 )
  {


    //with scottie, sync id mid-line between blue and red.
    //subtract the red period to sync to next full line
    image_sample -= modes[decode_mode].samples_per_colour_line;
    image_sample -= modes[decode_mode].samples_per_hsync;
    if(image_sample < 0)
    {
        //return colour 4 for non-displayable pixels (e.g. during hsync)
        x = 0; y=0; colour=4;
        return;
    }

    y = image_sample/mean_samples_per_line;
    image_sample -= y*mean_samples_per_line;

    //hsync is between blue and red component (not at end of line)
    //for red component, subtract the length of the scan-line
    if( image_sample < static_cast<int32_t>(2*modes[decode_mode].samples_per_colour_line))
    {
      colour = image_sample/modes[decode_mode].samples_per_colour_line;
      image_sample -= colour*modes[decode_mode].samples_per_colour_line;
    }
    else
    {
      image_sample -= 2*modes[decode_mode].samples_per_colour_line;
      image_sample -= modes[decode_mode].samples_per_hsync;
      colour = 2 + (image_sample/modes[decode_mode].samples_per_colour_line);
    }
    if( image_sample < 0 )
    {
        //return colour 4 for non-displayable pixels (e.g. during hsync)
        x = 0; y=0; colour=4;
        return;
    }

    colour = colourmap[colour]; //scottie colour order is g-b-r, map to r-g-b
    x = image_sample/modes[decode_mode].samples_per_pixel;

  }

  else if( decode_mode == pd_50 || decode_mode == pd_90 || decode_mode == pd_120 || decode_mode == pd_180)
  {
    static const uint8_t colourmap[5] = {0, 1, 2, 3, 4};

    image_sample -= modes[decode_mode].samples_per_hsync;
    if(image_sample < 0)
    {
      //return colour 4 for non-displayable pixels (e.g. during hsync)
      x = 0; y=0; colour=4;
      return;
    }
    y = image_sample/mean_samples_per_line;
    image_sample -= y*mean_samples_per_line;
    colour = image_sample/modes[decode_mode].samples_per_colour_line;
    image_sample -= colour*modes[decode_mode].samples_per_colour_line;
    colour = colourmap[colour];
    x = image_sample/modes[decode_mode].samples_per_pixel;
  }

  else if( decode_mode == sc2_120)
  {

    y = image_sample/mean_samples_per_line;
    image_sample -= y*mean_samples_per_line;

    if( image_sample < static_cast<int32_t>(modes[decode_mode].samples_per_colour_line) )
    {
      colour = 0;
      x = image_sample/modes[decode_mode].samples_per_pixel;
    }
    else if( image_sample < static_cast<int32_t>(2*modes[decode_mode].samples_per_colour_line) )
    {
      colour = 1;
      image_sample -= modes[decode_mode].samples_per_colour_line;
      x = image_sample/modes[decode_mode].samples_per_pixel;
    }
    else if( image_sample < static_cast<int32_t>(3*modes[decode_mode].samples_per_colour_line) )
    {
      colour = 2;
      image_sample -= 2*modes[decode_mode].samples_per_colour_line;
      x = image_sample/modes[decode_mode].samples_per_pixel;
    }
    else
    {
      colour = 4;
      x = 0;
    }

    if(image_sample < 0)
    {
      //return colour 4 for non-displayable pixels (e.g. during hsync)
      x = 0; y=0; colour=4;
      return;
    }
  }

}

uint8_t c_sstv_decoder :: frequency_to_brightness(uint16_t x)
{
  int16_t brightness = (256*(x-1500))/(2300-1500);
  return std::min(std::max(brightness, (int16_t)0), (int16_t)255);
}

bool parity_check(uint8_t x)
{
  x ^= x >> 4;
  x ^= x >> 2;
  x ^= x >> 1;
  return (~x) & 1;
}

c_sstv_decoder :: c_sstv_decoder(float Fs)
{

  m_Fs = Fs;
  static const uint32_t fraction_bits = 8;
  static const uint32_t scale = 1<<fraction_bits;
  m_scale = scale;

  m_auto_slant_correction = true;
  m_timeout = m_Fs*30;

  //martin m1
  {
  const uint16_t width = 320;
  const float hsync_pulse_ms = 4.862;
  const float colour_gap_ms = 0.572;
  const float colour_time_ms = 146.342;
  modes[martin_m1].width = width;
  modes[martin_m1].samples_per_line = scale*Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0;
  modes[martin_m1].samples_per_colour_line = scale*Fs*(colour_time_ms+colour_gap_ms)/1000.0;
  modes[martin_m1].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[martin_m1].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[martin_m1].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[martin_m1].max_height = 256;
  }

  //martin m2
  {
  const uint16_t width = 160;
  const float hsync_pulse_ms = 4.862;
  const float colour_gap_ms = 0.572;
  const float colour_time_ms = 73.216;
  modes[martin_m2].width = width;
  modes[martin_m2].samples_per_line = scale*Fs*((colour_time_ms*3)+(colour_gap_ms*4) + hsync_pulse_ms)/1000.0;
  modes[martin_m2].samples_per_colour_line = scale*Fs*(colour_time_ms+colour_gap_ms)/1000.0;
  modes[martin_m2].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[martin_m2].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[martin_m2].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[martin_m2].max_height = 256;
  }

  //scottie s1
  {
  const uint16_t width = 320;
  const float hsync_pulse_ms = 9;
  const float colour_gap_ms = 1.5;
  const float colour_time_ms = 138.240;
  modes[scottie_s1].width = width;
  modes[scottie_s1].samples_per_line = scale*Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0;
  modes[scottie_s1].samples_per_colour_line = scale*Fs*(colour_time_ms+colour_gap_ms)/1000.0;
  modes[scottie_s1].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[scottie_s1].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[scottie_s1].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[scottie_s1].max_height = 256;
  }

  //scottie s2
  {
  const uint16_t width = 160;
  const float hsync_pulse_ms = 9;
  const float colour_gap_ms = 1.5;
  const float colour_time_ms = 88.064;
  modes[scottie_s2].width = width;
  modes[scottie_s2].samples_per_line = scale*Fs*((colour_time_ms*3)+(colour_gap_ms*3) + hsync_pulse_ms)/1000.0;
  modes[scottie_s2].samples_per_colour_line = scale*Fs*(colour_time_ms+colour_gap_ms)/1000.0;
  modes[scottie_s2].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[scottie_s2].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[scottie_s2].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[scottie_s2].max_height = 256;
  }

  //pd 50
  {
  const uint16_t width = 320;
  const float hsync_pulse_ms = 20;
  const float colour_gap_ms = 2.08;
  const float colour_time_ms = 91.520;
  modes[pd_50].width = width;
  modes[pd_50].samples_per_line = scale*Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0;
  modes[pd_50].samples_per_colour_line = scale*Fs*(colour_time_ms)/1000.0;
  modes[pd_50].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[pd_50].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[pd_50].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[pd_50].max_height = 120;
  }

  //pd 90
  {
  const uint16_t width = 320;
  const float hsync_pulse_ms = 20;
  const float colour_gap_ms = 2.08;
  const float colour_time_ms = 170.240;
  modes[pd_90].width = width;
  modes[pd_90].samples_per_line = scale*Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0;
  modes[pd_90].samples_per_colour_line = scale*Fs*(colour_time_ms)/1000.0;
  modes[pd_90].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[pd_90].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[pd_90].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[pd_90].max_height = 120;
  }

  //pd 120
  {
  const uint16_t width = 640;
  const float hsync_pulse_ms = 20;
  const float colour_gap_ms = 2.08;
  const float colour_time_ms = 121.600;
  modes[pd_120].width = width;
  modes[pd_120].samples_per_line = scale*Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0;
  modes[pd_120].samples_per_colour_line = scale*Fs*(colour_time_ms)/1000.0;
  modes[pd_120].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[pd_120].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[pd_120].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[pd_120].max_height = 240;
  }

  //pd 180
  {
  const uint16_t width = 640;
  const float hsync_pulse_ms = 20;
  const float colour_gap_ms = 2.08;
  const float colour_time_ms = 183.040;
  modes[pd_180].width = width;
  modes[pd_180].samples_per_line = scale*Fs*((colour_time_ms*4)+(colour_gap_ms*1) + hsync_pulse_ms)/1000.0;
  modes[pd_180].samples_per_colour_line = scale*Fs*(colour_time_ms)/1000.0;
  modes[pd_180].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[pd_180].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[pd_180].samples_per_hsync = scale*Fs*hsync_pulse_ms/1000.0;
  modes[pd_180].max_height = 240;
  }

  //SC2120
  {
  const uint16_t width = 320;
  const float hsync_pulse_ms = 5;
  const float colour_gap_ms = 0;
  const float colour_time_ms = 156;
  modes[sc2_120].width = width;
  modes[sc2_120].samples_per_line = scale*Fs*((colour_time_ms*3) + hsync_pulse_ms)/1000.0;
  modes[sc2_120].samples_per_colour_line = scale*Fs*(colour_time_ms)/1000.0;
  modes[sc2_120].samples_per_colour_gap = scale*Fs*colour_gap_ms/1000.0;
  modes[sc2_120].samples_per_pixel = scale*Fs*colour_time_ms/(1000.0 * width);
  modes[sc2_120].samples_per_hsync = m_scale*Fs*hsync_pulse_ms/1000.0;
  modes[sc2_120].max_height = 256;
  }

  cordic_init();
}

/*
int16_t c_sstv_decoder :: get_audio_sample()
{
    static uint16_t *samples;
    static uint16_t sample_number = 4096;


    if(sample_number == 4096)
    {
      //fetch a new block of 4096 samples
      adc_audio.input_samples(samples);

      //decimate down to 1024 samples
      for(uint16_t idx=0; idx<1024; idx++)
      {
        samples[idx] = samples[idx*4] + samples[idx*4+1] + samples[idx*4+2] + samples[idx*4+3];
      }

      sample_number = 0;
    }

    //remove DC offset
    int16_t sample = samples[sample_number++];
    dc = dc + sample/2;
    sample -= dc;

    return sample;
}
*/

//return a single sample in i/q format
void c_sstv_decoder :: get_iq_sample(int16_t &i, int16_t &q)
{
    int16_t audio = get_audio_sample();

    // shift frequency by +FS/4
    //       __|__
    //   ___/  |  \___
    //         |
    //   <-----+----->

    //        | ____
    //  ______|/    \.
    //        |
    //  <-----+----->

    // filter -Fs/4 to +Fs/4

    //        | __
    //  ______|/  \__
    //        |
    //  <-----+----->

    ssb_phase = (ssb_phase + 1) & 3u;
    audio = audio >> 1;

    const int16_t audio_i[4] = {audio, 0, (int16_t)-audio, 0};
    const int16_t audio_q[4] = {0, (int16_t)-audio, 0, audio};
    int16_t ii = audio_i[ssb_phase];
    int16_t qq = audio_q[ssb_phase];
    ssb_filter.filter(ii, qq);

    // shift frequency by -FS/4
    //         | __
    //   ______|/  \__
    //         |
    //   <-----+----->

    //     __ |
    //  __/  \|______
    //        |
    //  <-----+----->

    const int16_t sample_i[4] = {(int16_t)-qq, (int16_t)-ii, qq, ii};
    const int16_t sample_q[4] = {ii, (int16_t)-qq, (int16_t)-ii, qq};
    i = sample_i[ssb_phase];
    q = sample_q[ssb_phase];

}

//return the frequency of a single sample
uint16_t c_sstv_decoder :: get_frequency_sample()
{

  uint16_t magnitude;
  int16_t phase;
  int16_t sample_i;
  int16_t sample_q;

  //convert to magnitude/phase representation
  get_iq_sample(sample_i, sample_q);
  cordic_rectangular_to_polar(sample_i, sample_q, magnitude, phase);

  //convert phase to frequency in Hz
  frequency = last_phase-phase;
  last_phase = phase;
  int16_t sample = (int32_t)frequency*15000>>16;

  //apply a smoothing filter
  static uint32_t smoothed_sample = 0;
  smoothed_sample = ((smoothed_sample << 3) + sample - smoothed_sample) >> 3;
  int16_t smoothed_sample_16 = std::min(std::max(smoothed_sample, (uint32_t)1000u), (uint32_t)2500u);

  return smoothed_sample_16;
}

void c_sstv_decoder :: decode_sample(uint16_t sample, uint16_t &pixel_y, uint16_t &pixel_x, uint8_t &pixel_colour, uint8_t &pixel, bool &pixel_complete, bool &line_complete, bool &image_complete)
{

  pixel_complete = false;
  line_complete = false;
  image_complete = false;

  //detect scan syncs
  bool sync_found = false;
  uint32_t line_length = 0u;
  if(sync_state == detect)
  {
    if( sample < 1400 && last_sample >= 1400)
    {
      sync_state = confirm;
      sync_counter = 0;
    }
  }
  else if(sync_state == confirm)
  {
    if( sample < 1400)
    {
      sync_counter++;
    }
    else if(sync_counter > 0)
    {
      sync_counter--;
    }

    if(sync_counter == 40)
    {
      sync_found = true;
      line_length = sample_number-last_hsync_sample;
      last_hsync_sample = sample_number;
      sync_state = detect;
    }
  }


  switch(state)
  {

    case detect_sync:

      if(sync_found)
      {
        uint32_t least_error = UINT32_MAX;
        for(uint8_t mode = 0; mode < num_modes; ++mode)
        {
          if( line_length > (99*modes[mode].samples_per_line)/(100*m_scale) and line_length < (101*modes[mode].samples_per_line)/(100*m_scale) )
          {
            mean_samples_per_line = modes[mode].samples_per_line;
            uint32_t error = abs(((int32_t)(line_length*m_scale))-(int32_t)modes[mode].samples_per_line);
            if(error < least_error)
            {
              decode_mode = (e_mode)mode;
              least_error = error;
            }
            confirm_count = 0;
            state = confirm_sync;
          }
        }
      }

      break;

    case confirm_sync:

      if(sync_found)
      {
        if( line_length > (99*modes[decode_mode].samples_per_line)/(100*m_scale) and line_length < (101*modes[decode_mode].samples_per_line)/(100*m_scale) )
        {
          state = decode_line;
          confirmed_sync_sample = sample_number;
          pixel_accumulator = 0;
          pixel_n = 0;
          last_x = 0;
          image_sample = 0;
          sync_timeout = m_timeout;
        }
        else
        {
          confirm_count ++;
          if(confirm_count == 4)
          {
            state = detect_sync;
          }
        }
      }

      break;

    case decode_line:
    {

      uint16_t x, y;
      uint8_t colour;
      sample_to_pixel(x, y, colour, image_sample);

      if(x != last_x && colour < 4 && pixel_n)
      {
        //output pixel
        pixel_complete = true;
        pixel = pixel_accumulator/pixel_n;
        line_complete = y > last_y;
        pixel_y = last_y;
        pixel_x = last_x;
        pixel_colour = colour;

        //reset accumulator for next pixel
        pixel_accumulator = 0;
        pixel_n = 0;
        last_x = x;
        last_y = y;
      }

      //end of image
      if(y == 256 || y == modes[decode_mode].max_height)
      {
        state = detect_sync;
        sync_counter = 0;
        image_complete = true;
        break;
      }

      //Auto Slant Correction
      if(sync_found)
      {
        //confirm sync if close to expected time
        if( line_length > (99*modes[decode_mode].samples_per_line)/(100*m_scale) && line_length < (101*modes[decode_mode].samples_per_line)/(100*m_scale) )
        {
          sync_timeout = m_timeout; //reset timeout on each good sync pulse
          const uint32_t samples_since_confirmed = sample_number-confirmed_sync_sample;
          const uint16_t num_lines = round(1.0*m_scale*samples_since_confirmed/modes[decode_mode].samples_per_line);
          if(m_auto_slant_correction)
          {
            mean_samples_per_line = mean_samples_per_line - (mean_samples_per_line >> 2) + ((m_scale*samples_since_confirmed/num_lines) >> 2);
          }
        }
      }

      //if no hsync seen, go back to idle
      else
      {
        sync_timeout--;
        if(!sync_timeout)
        {
          state = detect_sync;
          sync_counter = 0;
          break;
        }
      }

      //colour pixels
      pixel_accumulator += frequency_to_brightness(sample);
      pixel_n++;
      image_sample+=m_scale;

      break;

    }
  }

  sample_number++;
  last_sample = sample;

}

static uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

static uint16_t ycrcb_to_rgb565(int16_t y, int16_t cr, int16_t cb)
{
    cr = cr - 128;
    cb = cb - 128;
    int16_t r = y + 45 * cr / 32;
    int16_t g = y - (11 * cb + 23 * cr) / 32;
    int16_t b = y + 113 * cb / 64;
    r = r<0?0:(r>255?255:r);
    g = g<0?0:(g>255?255:g);
    b = b<0?0:(b>255?255:b);
    return rgb_to_rgb565(r, g, b);
}

void c_sstv_decoder :: decode_image(const char* image_filename, uint8_t timeout_s, bool slant_correction)
{
  m_timeout = timeout_s * m_Fs;
  m_auto_slant_correction = slant_correction;

  uint8_t line[640][4]; //array to contain seperate colour components of each decoded line
  bool image_open_flag = false;

  while(1)
  {
      uint16_t pixel_x, pixel_y;
      uint8_t pixel_colour;
      uint8_t pixel;
      bool pixel_complete, line_complete, image_complete;

      int16_t sample = get_frequency_sample();
      decode_sample(sample, pixel_y, pixel_x, pixel_colour, pixel, pixel_complete, line_complete, image_complete);

      if(pixel_complete)
      {
        if(pixel_x < 640 && pixel_colour < 4) line[pixel_x][pixel_colour] = pixel;
      }

      if(line_complete)
      {
        uint16_t line_rgb565[640]; //array to hold one line of image in rgb565 format

        if(decode_mode == pd_50 || decode_mode == pd_90 || decode_mode == pd_120 || decode_mode == pd_180)
        {
            if(!image_open_flag) image_open(image_filename, modes[decode_mode].width, modes[decode_mode].max_height*2);
            image_open_flag = true;

            for(uint16_t x=0; x<modes[decode_mode].width; ++x)
            {
              int16_t y  = line[x][0];
              int16_t cr = line[x][1];
              int16_t cb = line[x][2];
              line_rgb565[x] = ycrcb_to_rgb565(y, cr, cb);
            }
            image_write_line(line_rgb565, pixel_y*2, modes[decode_mode].width, modes[decode_mode].max_height);

            for(uint16_t x=0; x<modes[decode_mode].width; ++x)
            {
              int16_t y  = line[x][3];
              int16_t cr = line[x][1];
              int16_t cb = line[x][2];
              line_rgb565[x] = ycrcb_to_rgb565(y, cr, cb);
            }
            image_write_line(line_rgb565, pixel_y*2+1, modes[decode_mode].width, modes[decode_mode].max_height);
        }
        else
        {
            if(!image_open_flag) image_open(image_filename, modes[decode_mode].width, modes[decode_mode].max_height);
            image_open_flag = true;

            for(uint16_t x=0; x<modes[decode_mode].width; ++x)
            {
              int16_t r = line[x][0];
              int16_t g = line[x][1];
              int16_t b = line[x][2];
              line_rgb565[x] = rgb_to_rgb565(r, g, b);
            }
            image_write_line(line_rgb565, pixel_y*2+1, modes[decode_mode].width, modes[decode_mode].max_height);
        }
      }

      if(image_complete)
      {
        image_close();
        return;
      }
   }
}
