/*
 * Voice-controlled music playback — ESP32 LyraT-Mini v1.2
 * Say "play" → music starts. Say "stop" → music stops.
 *
 * Wake-word detection (Espressif's deprecated WakeNet5 "Hi LeXin"/"Nihao
 * Xiaozhi" models) never reliably triggered on this ESP32/IDF v6.0.2
 * combination even with clean audio, correct pronunciation, and a confirmed
 * threshold — see project history. Replaced with a custom Edge Impulse
 * keyword-spotting model (4 classes: Noise, Play, Stop, Unknown) trained on
 * this board's own mic, run continuously via ei_classifier_bridge.
 *
 * Microphone: ES7243E via I2S1
 *   MCLK=GPIO0, BCK=GPIO32, WS=GPIO33, DATA=GPIO36
 * I2C (ES7243E init): SDA=GPIO18, SCL=GPIO23
 *
 * CRITICAL: I2S must start first (provides MCLK to ES7243E), then ES7243E
 * I2C registers are written after MCLK is stable.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es7243e_adc.h"
#include "speaker_codec.h"
#include "audio_clip.h"
#include "ei_classifier_bridge.h"

#define TAG         "VOICE"
#define GREEN_LED   GPIO_NUM_22
#define BLUE_LED    GPIO_NUM_27
#define I2S_PORT    I2S_NUM_1
#define I2S_MCLK    GPIO_NUM_0    /* LyraT-Mini MCLK for ES7243 */
#define I2S_BCK     GPIO_NUM_32
#define I2S_WS      GPIO_NUM_33
#define I2S_DIN     GPIO_NUM_36
#define I2C_PORT    I2C_NUM_0
#define I2C_SDA     GPIO_NUM_18
#define I2C_SCL     GPIO_NUM_23
#define SAMPLE_RATE 16000

/* ── I2S mic input ───────────────────────────────────────────────────────── */
static i2s_chan_handle_t i2s_rx;

/* Mic init ported from a confirmed-working reference project on the same
 * board (esp32_projects/mic_test/i2s_es8311_example.c) — same fix pattern
 * that resolved the speaker: use the proper es7243e_codec_new() driver via
 * esp_codec_dev instead of hand-written raw ES7243E register writes, and
 * 16-bit I2S slots (matching what that driver actually configures the chip
 * for) instead of manually unpacking a 24-in-32-bit Philips frame. The old
 * manual-register approach is what's suspected to have produced the
 * persistently flat/unresponsive mic signal seen in extensive testing. */
static void i2s_mic_init(void)
{
    i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    i2s_new_channel(&ch_cfg, NULL, &i2s_rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK,     /* MCLK must be provided to ES7243 */
            .bclk = I2S_BCK,
            .ws   = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN,
        },
    };
    i2s_channel_init_std_mode(i2s_rx, &std_cfg);
    i2s_channel_enable(i2s_rx);
    ESP_LOGI(TAG, "I2S started — MCLK on GPIO0, BCK=32, WS=33, DIN=36");
}

/* ── Shared I2C bus (ES7243E mic codec + ES8311 speaker codec) ───────────── */
static i2c_master_bus_handle_t i2c_bus;

static void i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = I2C_PORT,
        .sda_io_num          = I2C_SDA,
        .scl_io_num          = I2C_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
}

/* ── ES7243E codec init via the proper esp_codec_dev driver ──────────────── */
static esp_codec_dev_handle_t adc_dev;

static void es7243_init(void)
{
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = I2C_NUM_0,
        .addr       = ES7243E_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port      = I2S_PORT,
        .tx_handle = NULL,
        .rx_handle = i2s_rx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_data_cfg);

    es7243e_codec_cfg_t es7243_cfg = {
        .ctrl_if = ctrl_if,
    };
    const audio_codec_if_t *codec_if = es7243e_codec_new(&es7243_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    adc_dev = esp_codec_dev_new(&dev_cfg);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel         = 2,
        .channel_mask    = 0x03,
        .sample_rate     = SAMPLE_RATE,
    };
    int r = esp_codec_dev_open(adc_dev, &fs);
    if (r != 0) {
        ESP_LOGE(TAG, "ES7243E esp_codec_dev_open failed: %d", r);
        return;
    }
    esp_codec_dev_set_in_gain(adc_dev, 6.0);

    ESP_LOGI(TAG, "ES7243E initialised via esp_codec_dev at I2C addr 0x%02X",
             ES7243E_CODEC_DEFAULT_ADDR);
}

/* ── Playback task ───────────────────────────────────────────────────────── */
static volatile bool s_music_playing = false;

static void playback_task(void *arg)
{
    const int chunk = 1600;  /* 100ms @ 16kHz — bounds stop latency */
    while (1) {
        if (!s_music_playing) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        speaker_set_active(true);
        for (int off = 0; off < AUDIO_CLIP_NUM_SAMPLES && s_music_playing; off += chunk) {
            int n = chunk;
            if (off + n > AUDIO_CLIP_NUM_SAMPLES) n = AUDIO_CLIP_NUM_SAMPLES - off;
            speaker_write(&audio_clip_pcm[off], n);
        }
        speaker_set_active(false);
    }
}

/* Minimum classifier confidence to act on a Play/Stop result — below this,
 * treat it like Noise/Unknown and ignore it. Tuned against Studio validation
 * (Play 100%/Stop 93% recall at this kind of margin); revisit if real-world
 * false triggers/misses turn out to need a different balance. */
#define EI_CONFIDENCE_THRESHOLD 0.3f

/* ── LED init ────────────────────────────────────────────────────────────── */
static void led_init(void)
{
    gpio_reset_pin(GREEN_LED);
    gpio_reset_pin(BLUE_LED);
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GREEN_LED) | (1ULL << BLUE_LED),
        .mode         = GPIO_MODE_OUTPUT,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(GREEN_LED, 0);
    gpio_set_level(BLUE_LED,  1);
}

/* ── app_main ────────────────────────────────────────────────────────────── */
void app_main(void)
{
    led_init();

    /* I2S first — provides MCLK to ES7243 before I2C config */
    i2s_mic_init();
    vTaskDelay(pdMS_TO_TICKS(100));  /* let MCLK stabilise */

    i2c_bus_init();
    es7243_init();
    vTaskDelay(pdMS_TO_TICKS(50));   /* let ADC settle */

    ESP_ERROR_CHECK(speaker_codec_init(i2c_bus));
    xTaskCreate(playback_task, "playback", 4096, NULL, 5, NULL);

    ei_bridge_init();
    ESP_LOGI(TAG, "Edge Impulse classifier ready — slice_size=%d  === Say 'play' or 'stop' ===",
             ei_bridge_slice_size());

    const int chunk = 480;  /* 30ms @ 16kHz mic read granularity */
    int16_t *buf = malloc(chunk * sizeof(int16_t));

    while (1) {
        /* Stereo DMA: chunk L+R pairs interleaved → chunk*2 int16_t words,
         * matching the codec driver's actual 16-bit output format (no more
         * 24-in-32-bit manual unpacking needed, see i2s_mic_init/es7243_init).
         * DMA ring delivers data in bursts; loop accumulates until we have a
         * full chunk so no garbage half-reads reach the classifier. */
        int n_need = chunk * 2 * (int)sizeof(int16_t);
        int16_t *raw = malloc(chunk * 2 * sizeof(int16_t));
        int total = 0;
        while (total < n_need) {
            size_t got = 0;
            i2s_channel_read(i2s_rx, (uint8_t*)raw + total,
                             n_need - total, &got, pdMS_TO_TICKS(200));
            if (got == 0) break;
            total += (int)got;
        }

        if (total < n_need) {
            /* DMA stalled — skip; don't feed partial/garbage to the classifier */
            free(raw);
            continue;
        }

        for (int i = 0; i < chunk; i++) {
            buf[i] = raw[i * 2];   /* feed LEFT channel to the classifier */
        }
        free(raw);

        ei_bridge_class_t cls;
        float              confidence;
        if (ei_bridge_feed(buf, chunk, &cls, &confidence)) {
            /* Only Play/Stop are logged (not every classification) — rare
             * enough not to reintroduce the UART-logging slowdown, but
             * enough to see actual confidence values for threshold tuning. */
            if (cls == EI_BRIDGE_PLAY || cls == EI_BRIDGE_STOP) {
                ESP_LOGI(TAG, "%s (%.2f)", cls == EI_BRIDGE_PLAY ? "Play" : "Stop", confidence);
            }

            if (cls == EI_BRIDGE_PLAY && !s_music_playing && confidence >= EI_CONFIDENCE_THRESHOLD) {
                s_music_playing = true;
                gpio_set_level(GREEN_LED, 1);
                ESP_LOGI(TAG, "PLAY (%.2f) -> music started", confidence);
            } else if (cls == EI_BRIDGE_STOP && s_music_playing && confidence >= EI_CONFIDENCE_THRESHOLD) {
                s_music_playing = false;
                gpio_set_level(GREEN_LED, 0);
                ESP_LOGI(TAG, "STOP (%.2f) -> music stopped", confidence);
            }
        }
    }

    free(buf);
}
