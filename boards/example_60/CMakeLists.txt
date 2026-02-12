# ============================================================================
# MARSVLT API — Example 60% Keyboard Build
# ============================================================================
# This is the build entry point for your keyboard.
# Copy this file to your board folder and change the target name.
#
# Build instructions:
#   mkdir build && cd build
#   cmake -G Ninja ..
#   ninja
# ============================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# RP2040 or RP2350 board type
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../api/pico_sdk_import.cmake)
project(example_60_firmware C CXX ASM)
pico_sdk_init()

# Override flash size if your board has larger flash (e.g., 4MB)
# add_compile_definitions(PICO_FLASH_SIZE_BYTES=4194304)

# ============================================================================
# Board and API paths
# ============================================================================
set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR})
set(API_DIR   ${CMAKE_CURRENT_LIST_DIR}/../../api)

# Include the shared build logic (defines API_COMMON_SOURCES)
include(${API_DIR}/build.cmake)

# ============================================================================
# Create your firmware target
# ============================================================================
# The USB descriptors file defines your VID/PID/product name.
# You can copy and modify api/src/usb/usb_descriptors.c, or use the
# default one which reads from config.h defines.
add_executable(example_60
    ${API_COMMON_SOURCES}
    ${API_DIR}/src/usb/usb_descriptors.c
)

pico_set_program_name(example_60 "example_60")
pico_set_program_version(example_60 "1.0")

# Apply all MARSVLT includes, libraries, and PIO configuration
marsvlt_configure_target(example_60)
