#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init the ES8311 speaker codec (I2S0 + shared I2C bus) and enable the
 * onboard amplifier. Must be called after the I2C bus used by the mic's
 * ES7243E is created, since both codecs share GPIO18(SDA)/GPIO23(SCL). */
esp_err_t speaker_codec_init(i2c_master_bus_handle_t i2c_bus);

/* Blocking write of 16kHz mono 16-bit PCM samples to the speaker. */
esp_err_t speaker_write(const int16_t *pcm, size_t num_samples);

/* Enable/disable the amp (PA_CTRL) and unmute/mute the DAC. Keep the amp
 * off except during actual playback to avoid injecting noise near the mic. */
void speaker_set_active(bool active);

#ifdef __cplusplus
}
#endif
