add_executable(generic_custom_adapter
    generic_custom_adapter/main.c
)

target_link_libraries(generic_custom_adapter PRIVATE loxboot)

target_compile_definitions(generic_custom_adapter PRIVATE
    LOXBOOT_MAX_BOOT_ATTEMPTS=${LOXBOOT_MAX_BOOT_ATTEMPTS}
    LOXBOOT_UART_LISTEN_MS=${LOXBOOT_UART_LISTEN_MS}
    LOXBOOT_UART_MAX_FRAME_PAYLOAD=${LOXBOOT_UART_MAX_FRAME_PAYLOAD}
)

if(MSVC)
    target_compile_options(generic_custom_adapter PRIVATE /W4 /WX)
else()
    target_compile_options(generic_custom_adapter PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
