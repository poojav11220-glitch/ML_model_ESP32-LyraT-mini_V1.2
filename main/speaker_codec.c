/*
 * ES8311 speaker codec — ESP32 LyraT-Mini v1.2
 *
 * I2S0 (verified against Espressif's official hardware reference table):
 *   BCK=GPIO5, WS=GPIO25, DOUT=GPIO26
 *
 * MCLK (GPIO0) is a single physical net shared with the mic's ES7243E on
 * I2S1, which already drives it as an output. A second I2S peripheral
 * (I2S0) must not also try to drive that same pin — two masters on one
 * wire is a bus conflict. So ES8311 is configured with use_mclk=false,
 * deriving its internal clocks from BCK instead, and I2S0's mclk gpio is
 * left unused.
 *
 * I2C bus (GPIO18 SDA / GPIO23 SCL) is shared with the ES7243E mic codec;
 * the caller must pass in the same bus handle used for that init.
 *
 * Config here (stereo slot mode, master_mode=false, explicit hw_gain,
 * mclk_div, pa_pin handed to the codec driver, and writing PCM via a raw
 * i2s_channel_write instead of esp_codec_dev_write) is ported verbatim
 * from a confirmed-working reference project (mic_test/i2s_es8311_example.c)
 * on the same board/pinout — our original config (mono slot, master_mode=
 * true, zeroed hw_gain, manually-driven PA_CTRL, esp_codec_dev_write)
 * produced complete silence despite the trigger/write calls all succeeding.
 */

#include "speaker_codec.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

#define TAG           "SPEAKER"
#define I2S_PORT      I2S_NUM_0
#define I2S_BCK       GPIO_NUM_5
#define I2S_WS        GPIO_NUM_25
#define I2S_DOUT      GPIO_NUM_26
#define PA_CTRL_PIN   GPIO_NUM_21
#define SAMPLE_RATE   16000
#define MCLK_MULTIPLE 256

static i2s_chan_handle_t      i2s_tx;
static esp_codec_dev_handle_t codec_dev;

esp_err_t speaker_codec_init(i2c_master_bus_handle_t i2c_bus)
{
    i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    ch_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&ch_cfg, &i2s_tx, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   /* shared MCLK already driven by mic I2S1 */
            .bclk = I2S_BCK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    std_cfg.clk_cfg.mclk_multiple = MCLK_MULTIPLE;
    ret = i2s_channel_init_std_mode(i2s_tx, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2s_channel_enable(i2s_tx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = I2C_NUM_0,
        .addr       = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if     = ctrl_if,
        .gpio_if     = gpio_if,
        .codec_mode  = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin      = PA_CTRL_PIN,   /* codec driver manages the amp */
        .pa_reverted = false,
        .master_mode = false,         /* ESP32 I2S0 is master; codec is slave */
        .use_mclk    = false,
        .hw_gain     = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
        .mclk_div    = MCLK_MULTIPLE,
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);
    if (!codec_if) {
        ESP_LOGE(TAG, "es8311_codec_new failed");
        return ESP_FAIL;
    }

    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port      = I2S_PORT,
        .tx_handle = i2s_tx,
        .rx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_data_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    codec_dev = esp_codec_dev_new(&dev_cfg);
    if (!codec_dev) {
        ESP_LOGE(TAG, "esp_codec_dev_new failed");
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = 2,
        .channel_mask    = 0x03,
        .sample_rate     = SAMPLE_RATE,
    };
    int r = esp_codec_dev_open(codec_dev, &fs);
    if (r != 0) {
        ESP_LOGE(TAG, "esp_codec_dev_open failed: %d", r);
        return ESP_FAIL;
    }
    esp_codec_dev_set_out_vol(codec_dev, 90);

    ESP_LOGI(TAG, "ES8311 speaker codec ready — BCK=%d WS=%d DOUT=%d PA_CTRL=%d",
             I2S_BCK, I2S_WS, I2S_DOUT, PA_CTRL_PIN);
    return ESP_OK;
}

/* Duplicates mono samples into an interleaved stereo buffer and writes them
 * directly to the I2S channel — bypassing esp_codec_dev_write, matching the
 * confirmed-working reference implementation's data path. */
esp_err_t speaker_write(const int16_t *pcm, size_t num_samples)
{
    int16_t *stereo = malloc(num_samples * 2 * sizeof(int16_t));
    if (!stereo) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < num_samples; i++) {
        stereo[2 * i]     = pcm[i];
        stereo[2 * i + 1] = pcm[i];
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(i2s_tx, stereo, num_samples * 2 * sizeof(int16_t),
                                       &bytes_written, 2000);
    free(stereo);
    return ret;
}

void speaker_set_active(bool active)
{
    esp_codec_dev_set_out_mute(codec_dev, !active);
}
