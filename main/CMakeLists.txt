idf_component_register(SRCS "speaker_codec.c" "ML_voice.c" "ei_classifier_bridge.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES edge_impulse esp_driver_i2c esp_driver_i2s esp_driver_gpio espressif__esp_codec_dev)

# ei_run_classifier.h (Edge Impulse SDK) is header-heavy and its inline code
# ends up compiled into whichever translation unit includes it — here that's
# this bridge file rather than the edge_impulse component's own sources, so
# the same warning relaxation applied there is needed here too.
set_source_files_properties(ei_classifier_bridge.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error=missing-field-initializers -Wno-missing-field-initializers")
