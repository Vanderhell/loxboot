idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES loxboot esp_driver_usb_serial_jtag esp_timer
)
