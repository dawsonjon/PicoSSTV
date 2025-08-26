#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#ifndef WIO_TERMINAL
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#else
#include "stdlib.h"
#include <Arduino.h>
#endif


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
#ifndef WIO_TERMINAL
    dma_channel_config audio_cfg;
#endif
    int audio_pwm_slice_num;

};

#endif
