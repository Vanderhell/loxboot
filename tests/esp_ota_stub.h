/*
 * ESP-IDF OTA stubs for host unit tests.
 * Include this BEFORE loxboot_esp32_platform.h to activate stub mode.
 * All types and declarations come from loxboot_esp32_platform.h itself;
 * this file only sets the build flags and defines struct esp_partition_t.
 */
#ifndef ESP_OTA_STUB_H
#define ESP_OTA_STUB_H

/* Activate stub paths in adapter code */
#define LOXBOOT_ESP32_STUB_BUILD 1
#define ESP_PLATFORM             1

/* Concrete struct — must be defined before platform header is included */
struct esp_partition_t {
    int         id;
    const char *label;
};

#endif /* ESP_OTA_STUB_H */
