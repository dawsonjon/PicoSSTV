#include "ADCAudio.h"
#include <stdio.h>
#include <Arduino.h>

ADCAudio ::ADCAudio()
{
}

#ifndef WIO_TERMINAL

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
    
  uint32_t start = time_us_32();
  // wait for ping transfer to complete
  dma_channel_wait_for_finish_blocking(adc_dma);
  uint32_t duration = time_us_32() - start;
    
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

#else 
	
/* 
Adaptation by Franciscu Capuzzi "Brabudu" 2025
Based on 
https://github.com/ShawnHymel/ei-keyword-spotting/blob/master/embedded-demos/arduino/wio-terminal/wio-terminal.ino
* Author: Shawn Hymel
*/
	
enum { ADC_BUF_LEN = 1024 }; 

// DMAC descriptor structure
typedef struct {
  uint16_t btctrl;
  uint16_t btcnt;
  uint32_t srcaddr;
  uint32_t dstaddr;
  uint32_t descaddr;
} dmacdescriptor;

// Globals - DMA and ADC
volatile uint8_t buffer_number=0;

volatile dmacdescriptor wrb[DMAC_CH_NUM] __attribute__ ((aligned (16)));          // Write-back DMAC descriptors
dmacdescriptor descriptor_section[DMAC_CH_NUM] __attribute__ ((aligned (16)));    // DMAC channel descriptors
dmacdescriptor descriptor __attribute__ ((aligned (16)));                         // Place holder descriptor

bool volatile data_ready=false;

void DMAC_1_Handler() {

 
  static uint16_t idx = 0;
 
  // Check if DMAC channel 1 has been suspended (SUSP)
  if (DMAC->Channel[1].CHINTFLAG.bit.SUSP) {

    // Restart DMAC on channel 1 and clear SUSP interrupt flag
    DMAC->Channel[1].CHCTRLB.reg = DMAC_CHCTRLB_CMD_RESUME;
    DMAC->Channel[1].CHINTFLAG.bit.SUSP = 1;
    
    data_ready=true;
    buffer_number ^= 1;
  }
}

void ADCAudio::end() 
{
  
}

void ADCAudio::begin(const uint8_t audio_pin, const uint32_t audio_sample_rate) 
{
  //TODO: audio pin and sample rate!
   
  // Configure DMA to sample from ADC at a regular interval (triggered by timer/counter)
  DMAC->BASEADDR.reg = (uint32_t)descriptor_section;                          // Specify the location of the descriptors
  DMAC->WRBADDR.reg = (uint32_t)wrb;                                          // Specify the location of the write back descriptors
  DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xf);                // Enable the DMAC peripheral
  DMAC->Channel[1].CHCTRLA.reg = DMAC_CHCTRLA_TRIGSRC(TC5_DMAC_ID_OVF) |      // Set DMAC to trigger on TC5 timer overflow
                                 DMAC_CHCTRLA_TRIGACT_BURST;                  // DMAC burst transfer
  descriptor.descaddr = (uint32_t)&descriptor_section[1];                     // Set up a circular descriptor
  descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;                           // Take the result from the ADC0 RESULT register
  descriptor.dstaddr = (uint32_t)samples[0] + sizeof(uint16_t) * ADC_BUF_LEN;  // Place it in the adc_buf_0 array
  descriptor.btcnt = ADC_BUF_LEN;                                             // Beat count
  descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |                            // Beat size is HWORD (16-bits)
                      DMAC_BTCTRL_DSTINC |                                    // Increment the destination address
                      DMAC_BTCTRL_VALID |                                     // Descriptor is valid
                      DMAC_BTCTRL_BLOCKACT_SUSPEND;                           // Suspend DMAC channel 0 after block transfer
  memcpy(&descriptor_section[0], &descriptor, sizeof(descriptor));            // Copy the descriptor to the descriptor section
  descriptor.descaddr = (uint32_t)&descriptor_section[0];                     // Set up a circular descriptor
  descriptor.srcaddr = (uint32_t)&ADC1->RESULT.reg;                           // Take the result from the ADC0 RESULT register
  descriptor.dstaddr = (uint32_t)samples[1] + sizeof(uint16_t) * ADC_BUF_LEN;  // Place it in the adc_buf_1 array
  descriptor.btcnt = ADC_BUF_LEN;                                             // Beat count
  descriptor.btctrl = DMAC_BTCTRL_BEATSIZE_HWORD |                            // Beat size is HWORD (16-bits)
                      DMAC_BTCTRL_DSTINC |                                    // Increment the destination address
                      DMAC_BTCTRL_VALID |                                     // Descriptor is valid
                      DMAC_BTCTRL_BLOCKACT_SUSPEND;                           // Suspend DMAC channel 0 after block transfer
  memcpy(&descriptor_section[1], &descriptor, sizeof(descriptor));            // Copy the descriptor to the descriptor section

  // Configure NVIC
  NVIC_SetPriority(DMAC_1_IRQn, 0);    // Set the Nested Vector Interrupt Controller (NVIC) priority for DMAC1 to 0 (highest)
  NVIC_EnableIRQ(DMAC_1_IRQn);         // Connect DMAC1 to Nested Vector Interrupt Controller (NVIC)

  // Activate the suspend (SUSP) interrupt on DMAC channel 1
  DMAC->Channel[1].CHINTENSET.reg = DMAC_CHINTENSET_SUSP;
  
  // Configure ADC
  ADC1->INPUTCTRL.bit.MUXPOS = ADC_INPUTCTRL_MUXPOS_AIN12_Val; // Set the analog input to ADC0/AIN2 (PB08 - A4 on Metro M4)
  while(ADC1->SYNCBUSY.bit.INPUTCTRL);                // Wait for synchronization
  ADC1->SAMPCTRL.bit.SAMPLEN = 0x00;                  // Set max Sampling Time Length to half divided ADC clock pulse (2.66us)
  while(ADC1->SYNCBUSY.bit.SAMPCTRL);                 // Wait for synchronization 
  ADC1->CTRLA.reg = ADC_CTRLA_PRESCALER_DIV128;       // Divide Clock ADC GCLK by 128 (48MHz/128 = 375kHz)
  ADC1->CTRLB.reg = ADC_CTRLB_RESSEL_12BIT |          // Set ADC resolution to 12 bits
                    ADC_CTRLB_FREERUN;                // Set ADC to free run mode       
  while(ADC1->SYNCBUSY.bit.CTRLB);                    // Wait for synchronization
  ADC1->CTRLA.bit.ENABLE = 1;                         // Enable the ADC
  while(ADC1->SYNCBUSY.bit.ENABLE);                   // Wait for synchronization
  ADC1->SWTRIG.bit.START = 1;                         // Initiate a software trigger to start an ADC conversion
  while(ADC1->SYNCBUSY.bit.SWTRIG);                   // Wait for synchronization

  // Enable DMA channel 1
  DMAC->Channel[1].CHCTRLA.bit.ENABLE = 1;

  // Configure Timer/Counter 5
  GCLK->PCHCTRL[TC5_GCLK_ID].reg = GCLK_PCHCTRL_CHEN |        // Enable perhipheral channel for TC5
                                   GCLK_PCHCTRL_GEN_GCLK1;    // Connect generic clock 0 at 48MHz
   
  TC5->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;               // Set TC5 to Match Frequency (MFRQ) mode
  TC5->COUNT16.CC[0].reg = 3200 - 1;                          // Set the trigger to 15 kHz: (48Mhz / 15000) - 1
  while (TC5->COUNT16.SYNCBUSY.bit.CC0);                      // Wait for synchronization

  // Start Timer/Counter 5
  TC5->COUNT16.CTRLA.bit.ENABLE = 1;                          // Enable the TC5 timer
  while (TC5->COUNT16.SYNCBUSY.bit.ENABLE);                   // Wait for synchronization
  buffer_number = 0;
}

// samples is a reference to a buffer containing block size samples
int16_t * ADCAudio ::input_samples() {
  
  while (!data_ready);
  data_ready=false;
  
  for(uint16_t idx=0; idx<1024; idx++)
    {
      dc = dc + (samples[buffer_number][idx] - dc)/2;
      samples[buffer_number][idx] = samples[buffer_number][idx] - dc;
	}
  int16_t *output_samples = (int16_t*)&(samples[buffer_number]);
  return output_samples;
}

#endif