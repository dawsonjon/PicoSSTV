#include "ADCAudio.h"
#include <stdio.h>

ADCAudio ::ADCAudio()
{
}

void ADCAudio::end() 
{
    // Configure DMA for ADC transfers
    dma_channel_unclaim(adc_dma);
}

void ADCAudio::begin(const uint8_t audio_pin, const uint32_t audio_sample_rate) 
{

  // ADC Configuration
  adc_init();
  adc_gpio_init(audio_pin); // I channel (0) - configure pin for ADC use
  const uint32_t usb_clock_frequency = 48000000;
  adc_set_clkdiv((usb_clock_frequency / (audio_sample_rate*4)) - 1);

  // Configure DMA for ADC transfers
  adc_dma = dma_claim_unused_channel(true);
  cfg = dma_channel_get_default_config(adc_dma);

  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_dreq(
      &cfg, DREQ_ADC); // Pace transfers based on availability of ADC samples

  // start ADC
  adc_select_input(2);
  hw_clear_bits(&adc_hw->fcs, ADC_FCS_UNDER_BITS);
  hw_clear_bits(&adc_hw->fcs, ADC_FCS_OVER_BITS);
  adc_fifo_setup(true, true, 1, false, false);
  adc_run(true);

  // pre-fill ping buffer
  buffer_number = 0;
  dma_channel_configure(adc_dma, &cfg, samples[buffer_number], &adc_hw->fifo, 4096, true);
}

// samples is a reference to a buffer containing block size samples
int16_t * ADCAudio ::input_samples() {
    
  // wait for ping transfer to complete
  dma_channel_wait_for_finish_blocking(adc_dma);
    
  // start a transfer into pong buffer for next time
  dma_channel_configure(adc_dma, &cfg, samples[buffer_number^1], &adc_hw->fifo, 4096, true);
    
  //decimate down to 1024 samples
  for(uint16_t idx=0; idx<1024; idx++)
  {
    //combine 4 samples
    int16_t sample = samples[buffer_number][idx*4] + samples[buffer_number][idx*4+1] + samples[buffer_number][idx*4+2] + samples[buffer_number][idx*4+3];
    dc += (sample-dc)/2; //measure dc
    sample -= dc; //remove dc (convert to signed)
    samples[buffer_number][idx] = sample << 2; //convert to 16 bit
  }

  int16_t *output_samples = (int16_t*)&(samples[buffer_number]);
  buffer_number ^= 1;
  return output_samples;

}
