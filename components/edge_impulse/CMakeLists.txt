# Edge Impulse inferencing SDK wrapped as an ESP-IDF component.
#
# The SDK ships pre-built for many platforms; only the portable (non-assembly)
# sources are compiled here. ESP-NN and esp-dsp under porting/espressif ship
# ESP32-S3/P4-only accelerated assembly (.S) kernels that don't assemble on
# plain ESP32, so those two directories are excluded entirely — TFLite Micro's
# portable reference kernels and the SDK's bundled kissfft cover the same
# functionality in pure C/C++, just without the S3/P4-specific speedup.
file(GLOB_RECURSE EI_CMSIS_SRCS      "edge-impulse-sdk/CMSIS/*.c")
file(GLOB_RECURSE EI_CLASSIFIER_SRCS "edge-impulse-sdk/classifier/*.cpp")
file(GLOB_RECURSE EI_DSP_SRCS        "edge-impulse-sdk/dsp/*.cpp" "edge-impulse-sdk/dsp/*.c")
file(GLOB_RECURSE EI_TFLITE_SRCS     "edge-impulse-sdk/tensorflow/*.cc" "edge-impulse-sdk/tensorflow/*.cpp" "edge-impulse-sdk/tensorflow/*.c")
file(GLOB_RECURSE EI_MODEL_SRCS      "tflite-model/*.cpp")

# edge-impulse-sdk/dsp/numpy.hpp unconditionally pulls in the ESP-DSP FFT
# engine on Espressif targets (not opt-in as its name suggests). Only the
# portable "ansi" fallback is needed on plain ESP32 (no assembly, unlike the
# esp32s3/esp32p4-specific .S kernels elsewhere in this module which won't
# assemble here) — pulled in explicitly rather than via the broader glob
# above, which would also drag in those incompatible assembly files.
set(EI_ESP_DSP_SRCS
    "edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/float/dsps_fft2r_fc32_ansi.c"
    "edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/float/dsps_fft2r_bitrev_tables_fc32.c"
    "edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.cpp"
)

idf_component_register(
    SRCS
        ${EI_CMSIS_SRCS}
        ${EI_CLASSIFIER_SRCS}
        ${EI_DSP_SRCS}
        ${EI_TFLITE_SRCS}
        ${EI_MODEL_SRCS}
        ${EI_ESP_DSP_SRCS}
        "edge-impulse-sdk/porting/espressif/debug_log.cpp"
        "edge-impulse-sdk/porting/espressif/ei_classifier_porting.cpp"
    INCLUDE_DIRS
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/third_party/flatbuffers/include"
        "edge-impulse-sdk/third_party/gemmlowp"
        "edge-impulse-sdk/third_party/ruy"
        "edge-impulse-sdk/dsp/kissfft"
        "edge-impulse-sdk/CMSIS/Core/Include"
        "edge-impulse-sdk/CMSIS/DSP/Include"
        "edge-impulse-sdk/CMSIS/DSP/PrivateInclude"
        "edge-impulse-sdk/CMSIS/NN/Include"
        "edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/include"
        "edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/include"
        "model-parameters"
        "tflite-model"
    REQUIRES
        esp_timer
        driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -DEIDSP_QUANTIZE_FILTERBANK=0
)
