#ifdef RASPBERRY_PI_PICO

#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"



class PWMAudio
{

    public:
    PWMAudio();
    void end();
    void begin(uint8_t audio_pin, const uint32_t audio_sample_rate, const uint32_t cpuFrequencyHz);
    void output_samples(const uint16_t samples[], const uint16_t len);

    private:
    int pwm_dma;
    int dma_timer;
    dma_channel_config audio_cfg;
    int audio_pwm_slice_num;

};

#endif
#endif
