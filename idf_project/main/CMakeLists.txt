idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES loxboot esp_driver_usb_serial_jtag esp_timer app_update
)

set(LOXBOOT_ESP32_AUTO_CONFIRM "1" CACHE STRING "Set to 1 to auto-confirm pending OTA images on boot, 0 to leave them pending for rollback tests")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOXBOOT_ESP32_AUTO_CONFIRM=${LOXBOOT_ESP32_AUTO_CONFIRM}
)
