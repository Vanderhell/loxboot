# ARM Cortex-M toolchain for cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Compiler settings
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_WORKS 1)

# Compiler flags for ARM Cortex-M (no HAL, core library only)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -Wall -Wextra -Wpedantic -Werror")

# Skip compiler tests during toolchain load
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Don't look for programs in target environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
