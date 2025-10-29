#ifdef ARDUINO_ARCH_RP2040

#ifndef ADC_AUDIO_H
#define ADC_AUDIO_H

#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/dma.h"


static const int ADC_DMA_BLOCK = 4096;
static const int ADC_BLOCK = 2048;
class ADCAudio
{

    public:
    ADCAudio();
    void begin(const uint8_t audio_pin, const uint32_t audio_sample_rate);
    void end();
    int16_t* input_samples();

    private:
    int adc_dma;
    dma_channel_config cfg;
    uint32_t m_audio_sample_rate;
    uint16_t samples[2][ADC_DMA_BLOCK];
    uint8_t buffer_number = 0;
    uint16_t dc = 0;
};

#endif
#endif
