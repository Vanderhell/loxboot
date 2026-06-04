set(LOXBOOT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

idf_component_register(
    SRCS
        "${LOXBOOT_ROOT}/src/loxboot_core.c"
        "${LOXBOOT_ROOT}/src/loxboot_crc32.c"
        "${LOXBOOT_ROOT}/src/loxboot_state.c"
        "${LOXBOOT_ROOT}/ports/uart/loxboot_uart.c"
        "${LOXBOOT_ROOT}/adapters/esp32/loxboot_flash_esp32.c"
    INCLUDE_DIRS
        "${LOXBOOT_ROOT}/include"
        "${LOXBOOT_ROOT}/adapters/esp32"
    REQUIRES
        esp_partition
        driver
        esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LOXBOOT_BUILD_UART_PORT=1
    LOXBOOT_MAX_BOOT_ATTEMPTS=3
    LOXBOOT_UART_LISTEN_MS=30000
    LOXBOOT_UART_MAX_FRAME_PAYLOAD=1024
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Werror
)
