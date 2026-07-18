#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* C-compatible wrapper around the Edge Impulse C++ inferencing SDK, so the
 * rest of the (plain C) app doesn't need to include EI's C++-only headers
 * (signal_t uses std::function unless EIDSP_SIGNAL_C_FN_POINTER is set). */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EI_BRIDGE_NOISE = 0,
    EI_BRIDGE_PLAY,
    EI_BRIDGE_STOP,
    EI_BRIDGE_UNKNOWN,
} ei_bridge_class_t;

void ei_bridge_init(void);

/* Samples per inference slice (EI_CLASSIFIER_SLICE_SIZE) — size your
 * accumulation chunks against this if you want exactly one slice per call,
 * though ei_bridge_feed() also accepts smaller chunks and accumulates. */
int ei_bridge_slice_size(void);

/* Feed raw 16kHz mono PCM16 samples (any chunk size). Internally accumulates
 * into a slice buffer and runs inference once a full slice is collected.
 * Returns true (and fills out_class/out_confidence with the top scoring
 * class) exactly on the calls where a slice completed and inference ran. */
bool ei_bridge_feed(const int16_t *samples, size_t num_samples,
                     ei_bridge_class_t *out_class, float *out_confidence);

#ifdef __cplusplus
}
#endif
