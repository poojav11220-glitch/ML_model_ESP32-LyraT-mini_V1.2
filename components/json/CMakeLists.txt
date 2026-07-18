# Shim: provides cJSON.h for esp-sr which was written for IDF v4.
# IDF v6 dropped the built-in json/cJSON component entirely.
# cJSON is only used in dl_lib_coefgetter_if.h as a struct type,
# so only the header is needed — no compiled cJSON.c required.
idf_component_register(INCLUDE_DIRS ".")
