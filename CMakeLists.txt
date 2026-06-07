cmake_minimum_required(VERSION 3.16)
project(loxboot C)

enable_testing()

# Global language level
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(LOXBOOT_BUILD_TESTS       "Build and register CTest tests"          ON)
option(LOXBOOT_BUILD_UART_PORT   "Include ports/uart/ in build"            OFF)
option(LOXBOOT_BUILD_STM32_ADAPTER "Include adapters/stm32/ in build"      OFF)
option(LOXBOOT_BUILD_ESP32_ADAPTER "Include adapters/esp32/ in build"      OFF)

set(LOXBOOT_MAX_BOOT_ATTEMPTS  "3"    CACHE STRING "Max boot attempts before rollback")
set(LOXBOOT_UART_LISTEN_MS     "3000" CACHE STRING "UART listen window in ms (0=disabled)")
set(LOXBOOT_UART_MAX_FRAME_PAYLOAD "1024" CACHE STRING "Max UART frame payload in bytes")

function(loxboot_require_file path)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
        message(FATAL_ERROR "Required file missing: ${path}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Core sources (v0.2.0+)
# ---------------------------------------------------------------------------

set(LOXBOOT_CORE_SOURCES
    src/loxboot_core.c
    src/loxboot_crc32.c
    src/loxboot_state.c
)

# ---------------------------------------------------------------------------
# UART port sources (v0.4.0+)
# ---------------------------------------------------------------------------

set(LOXBOOT_UART_SOURCES
    ports/uart/loxboot_uart.c
)

# ---------------------------------------------------------------------------
# Adapter sources
# ---------------------------------------------------------------------------

set(LOXBOOT_STM32_SOURCES  adapters/stm32/loxboot_flash_stm32.c)
set(LOXBOOT_ESP32_SOURCES  adapters/esp32/loxboot_flash_esp32.c)

# ---------------------------------------------------------------------------
# Assemble source list
# ---------------------------------------------------------------------------

set(LOXBOOT_SOURCES "")

foreach(f IN LISTS LOXBOOT_CORE_SOURCES)
    loxboot_require_file("${f}")
    list(APPEND LOXBOOT_SOURCES "${f}")
endforeach()

if(LOXBOOT_BUILD_UART_PORT)
    foreach(f IN LISTS LOXBOOT_UART_SOURCES)
        loxboot_require_file("${f}")
        list(APPEND LOXBOOT_SOURCES "${f}")
    endforeach()
endif()

if(LOXBOOT_BUILD_STM32_ADAPTER)
    foreach(f IN LISTS LOXBOOT_STM32_SOURCES)
        loxboot_require_file("${f}")
        list(APPEND LOXBOOT_SOURCES "${f}")
    endforeach()
endif()

if(LOXBOOT_BUILD_ESP32_ADAPTER)
    foreach(f IN LISTS LOXBOOT_ESP32_SOURCES)
        loxboot_require_file("${f}")
        list(APPEND LOXBOOT_SOURCES "${f}")
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------

if(LOXBOOT_SOURCES)
    add_library(loxboot STATIC ${LOXBOOT_SOURCES})
else()
    # v0.1.0-spec: no sources yet — INTERFACE target
    add_library(loxboot INTERFACE)
endif()

if(LOXBOOT_SOURCES)
    target_include_directories(loxboot PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(loxboot PUBLIC
        LOXBOOT_MAX_BOOT_ATTEMPTS=${LOXBOOT_MAX_BOOT_ATTEMPTS}
        LOXBOOT_UART_LISTEN_MS=${LOXBOOT_UART_LISTEN_MS}
        LOXBOOT_UART_MAX_FRAME_PAYLOAD=${LOXBOOT_UART_MAX_FRAME_PAYLOAD}
    )
    if(LOXBOOT_BUILD_UART_PORT)
        target_compile_definitions(loxboot PUBLIC LOXBOOT_BUILD_UART_PORT=1)
    endif()
    if(LOXBOOT_BUILD_TESTS)
        target_compile_definitions(loxboot PRIVATE LOXBOOT_TEST_HOOKS=1)
    endif()
    if(MSVC)
        target_compile_options(loxboot PRIVATE /W4 /WX)
    else()
        target_compile_options(loxboot PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
else()
    target_include_directories(loxboot INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(loxboot INTERFACE
        LOXBOOT_MAX_BOOT_ATTEMPTS=${LOXBOOT_MAX_BOOT_ATTEMPTS}
        LOXBOOT_UART_LISTEN_MS=${LOXBOOT_UART_LISTEN_MS}
        LOXBOOT_UART_MAX_FRAME_PAYLOAD=${LOXBOOT_UART_MAX_FRAME_PAYLOAD}
    )
endif()

# ---------------------------------------------------------------------------
# Tests (v0.2.0+)
# ---------------------------------------------------------------------------

if(LOXBOOT_BUILD_TESTS)
    # Test hook support
    set(LOXBOOT_TEST_DEFS LOXBOOT_TEST_HOOKS=1)

    # List of test files — agent adds entries here as each milestone is implemented
    set(LOXBOOT_TEST_FILES
        # v0.2.0 — core
        tests/test_loxboot_crc32.c
        tests/test_loxboot_init.c
        tests/test_loxboot_slot_state.c
        tests/test_loxboot_commit_slot.c
        tests/test_loxboot_invalidate_slot.c
        tests/test_loxboot_confirm_boot.c
        tests/test_loxboot_request_slot.c
        tests/test_loxboot_state_edges.c
        # v0.3.0 — boot sequence
        tests/test_loxboot_boot_sequence.c
        tests/test_loxboot_rollback.c
        tests/test_loxboot_crash_loop.c
    )

    if(LOXBOOT_BUILD_UART_PORT)
        list(APPEND LOXBOOT_TEST_FILES
            # v0.4.0 — UART
            tests/test_loxboot_uart_frame.c
            tests/test_loxboot_uart_receive.c
            # v0.7.0 — ESP32 platform (stub-based, no ESP-IDF required)
            tests/test_loxboot_esp32_platform.c
        )
    endif()

    foreach(test_file IN LISTS LOXBOOT_TEST_FILES)
        loxboot_require_file("${test_file}")
        get_filename_component(test_name "${test_file}" NAME_WE)

        # ESP32 platform test needs its adapter sources compiled in (stubs provided by test)
        if(test_name STREQUAL "test_loxboot_esp32_platform")
            loxboot_require_file("adapters/esp32/loxboot_esp32_platform.c")
            add_executable(${test_name}
                "${test_file}"
                tests/test_support.c
                adapters/esp32/loxboot_esp32_platform.c
            )
        else()
            add_executable(${test_name}
                "${test_file}"
                tests/test_support.c
            )
        endif()

        target_link_libraries(${test_name} PRIVATE loxboot)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/adapters/esp32
        )
        target_compile_definitions(${test_name} PRIVATE ${LOXBOOT_TEST_DEFS})
        # ESP32 platform test: inject stub defines so #ifdef ESP_PLATFORM paths compile
        if(test_name STREQUAL "test_loxboot_esp32_platform")
            target_compile_definitions(${test_name} PRIVATE
                ESP_PLATFORM=1
                LOXBOOT_ESP32_STUB_BUILD=1
            )
        endif()
        if(MSVC)
            target_compile_options(${test_name} PRIVATE /W4 /WX)
        else()
            target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    # -----------------------------------------------------------------------
    # UART device simulator — used by tools/test_e2e.py for E2E testing
    # -----------------------------------------------------------------------
    if(LOXBOOT_BUILD_UART_PORT)
        loxboot_require_file("tools/loxboot_sim.c")
        add_executable(loxboot_sim
            tools/loxboot_sim.c
            tests/test_support.c
        )
        target_link_libraries(loxboot_sim PRIVATE loxboot)
        target_include_directories(loxboot_sim PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )
        target_compile_definitions(loxboot_sim PRIVATE ${LOXBOOT_TEST_DEFS})
        if(MSVC)
            target_compile_options(loxboot_sim PRIVATE /W4 /WX)
        else()
            target_compile_options(loxboot_sim PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()

        # Wire E2E Python test into CTest (requires Python3)
        find_program(PYTHON3 NAMES python3 python)
        if(PYTHON3)
            add_test(
                NAME loxboot_e2e
                COMMAND ${PYTHON3}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_e2e.py
                    --sim $<TARGET_FILE:loxboot_sim>
            )
            set_tests_properties(loxboot_e2e PROPERTIES
                DEPENDS loxboot_sim
                TIMEOUT 60
            )
        endif()
    endif()
endif()
