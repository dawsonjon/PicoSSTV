#include "PWMAudio.h"

PWMAudio ::PWMAudio() {
}

void PWMAudio::end() 
{
    dma_channel_unclaim(pwm_dma);
    dma_timer_unclaim(dma_timer);
}

void PWMAudio::begin(const uint8_t audio_pin, const uint32_t audio_sample_rate, const uint32_t cpuFrequencyHz) 
{
  // audio output pin
  gpio_set_function(audio_pin, GPIO_FUNC_PWM);
  gpio_set_drive_strength(audio_pin, GPIO_DRIVE_STRENGTH_12MA);

  // configure PWM
  const uint16_t pwm_max = 4096; // 12 bit pwm at 30517Hz
  audio_pwm_slice_num = pwm_gpio_to_slice_num(audio_pin);
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 1.f); // 125MHz
  pwm_config_set_wrap(&config, pwm_max);
  pwm_init(audio_pwm_slice_num, &config, true);

  // configure DMA for audio transfers
  pwm_dma = dma_claim_unused_channel(true);
  audio_cfg = dma_channel_get_default_config(pwm_dma);
  channel_config_set_transfer_data_size(&audio_cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&audio_cfg, true);
  channel_config_set_write_increment(&audio_cfg, false);

  dma_timer = dma_claim_unused_timer(true);
  dma_timer_set_fraction(dma_timer, 1, cpuFrequencyHz / audio_sample_rate);
  channel_config_set_dreq(&audio_cfg, dma_get_timer_dreq(dma_timer));

}

void PWMAudio ::output_samples(const uint16_t samples[], const uint16_t len) {

  uint32_t start_time = time_us_32();
  dma_channel_wait_for_finish_blocking(pwm_dma);
  uint32_t idle_time_us = time_us_32()-start_time;
  dma_channel_configure(pwm_dma, &audio_cfg,
                        &pwm_hw->slice[audio_pwm_slice_num].cc, samples, len,
                        true);
  uint32_t block_time_us = (uint32_t)len * 1000000/15000;
  uint32_t busy_time_us = block_time_us - idle_time_us;

}
