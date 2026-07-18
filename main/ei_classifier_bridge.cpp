#include "ei_classifier_bridge.h"

#include <cstring>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "EI_BRIDGE";

static float  s_slice_buf[EI_CLASSIFIER_SLICE_SIZE];
static size_t s_slice_fill = 0;

static int get_slice_data(size_t offset, size_t length, float *out_ptr)
{
    memcpy(out_ptr, s_slice_buf + offset, length * sizeof(float));
    return 0;
}

extern "C" void ei_bridge_init(void)
{
    run_classifier_init();
    s_slice_fill = 0;
}

extern "C" int ei_bridge_slice_size(void)
{
    return EI_CLASSIFIER_SLICE_SIZE;
}

extern "C" bool ei_bridge_feed(const int16_t *samples, size_t num_samples,
                                ei_bridge_class_t *out_class, float *out_confidence)
{
    bool inferred = false;

    size_t src_off = 0;
    while (src_off < num_samples) {
        size_t room  = EI_CLASSIFIER_SLICE_SIZE - s_slice_fill;
        size_t avail = num_samples - src_off;
        size_t take  = (room < avail) ? room : avail;

        /* EI's audio pipeline trains/infers on raw int16 magnitudes cast to
         * float, not normalized to [-1,1] — matches how the Studio ingests
         * WAV int16 samples during training. */
        for (size_t i = 0; i < take; i++) {
            s_slice_buf[s_slice_fill + i] = (float)samples[src_off + i];
        }
        s_slice_fill += take;
        src_off      += take;

        if (s_slice_fill < EI_CLASSIFIER_SLICE_SIZE) {
            continue;
        }
        s_slice_fill = 0;

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
        signal.get_data     = &get_slice_data;

        /* One 250ms slice must be processed in under 250ms real-time budget
         * (EI_CLASSIFIER_SLICE_SIZE / 16kHz) or the pipeline falls behind and
         * reacts to increasingly stale audio — measured here, throttled to
         * avoid reintroducing the same UART-logging slowdown this is meant
         * to diagnose. */
        int64_t t0 = esp_timer_get_time();
        ei_impulse_result_t result = {};
        EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, false);
        int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;

        static int s_over_budget_count = 0;
        if (elapsed_ms > 250) {
            s_over_budget_count++;
            if (s_over_budget_count <= 3 || s_over_budget_count % 20 == 0) {
                ESP_LOGW(TAG, "inference took %lldms (budget 250ms) — pipeline is falling behind real-time (count=%d)",
                         elapsed_ms, s_over_budget_count);
            }
        }

        if (r != EI_IMPULSE_OK) {
            continue;
        }

        int   best_ix  = 0;
        float best_val = result.classification[0].value;
        for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > best_val) {
                best_val = result.classification[i].value;
                best_ix  = i;
            }
        }

        /* Label order is fixed by training (Noise, Play, Stop, Unknown) —
         * matched positionally rather than by string compare. */
        if (out_class)      *out_class      = (ei_bridge_class_t)best_ix;
        if (out_confidence) *out_confidence = best_val;
        inferred = true;
    }

    return inferred;
}
