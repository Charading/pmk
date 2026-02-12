# MARSVLT Keyboard API — Shared CMake Build Logic
#
# Usage in your board's CMakeLists.txt:
#   set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR})
#   set(API_DIR   ${CMAKE_CURRENT_LIST_DIR}/../../api)
#   include(${API_DIR}/build.cmake)
#
# Before including, define your target:
#   add_executable(my_keyboard ${API_COMMON_SOURCES} ${MY_USB_DESCRIPTORS})

# ============================================================================
# Common source files (relative to API_DIR)
# ============================================================================
set(API_COMMON_SOURCES
    ${API_DIR}/main.c
    ${API_DIR}/hid_reports.c
    ${API_DIR}/profiles.c
    ${API_DIR}/encoder.c
    ${API_DIR}/features/socd/socd.c
    ${API_DIR}/lighting/lighting.c
)

# ============================================================================
# Macro: marsvlt_configure_target(target_name)
# Call this after add_executable() to set up includes, libs, PIO, etc.
# ============================================================================
macro(marsvlt_configure_target TARGET_NAME)
    # Generate PIO header for WS2812 LEDs
    pico_generate_pio_header(${TARGET_NAME} ${API_DIR}/drivers/ws2812.pio)

    # UART debug output (GP0/GP1), no USB stdio (TinyUSB handles USB)
    pico_enable_stdio_uart(${TARGET_NAME} 1)
    pico_enable_stdio_usb(${TARGET_NAME} 0)

    # Include paths: board dir FIRST (so board's config.h wins), then API dirs
    target_include_directories(${TARGET_NAME} PRIVATE
        ${BOARD_DIR}
        ${API_DIR}
        ${API_DIR}/src/usb
        ${API_DIR}/features/socd
        ${API_DIR}/lighting
        ${API_DIR}/drivers
    )

    # Link required Pico SDK libraries
    target_link_libraries(${TARGET_NAME}
        pico_stdlib
        hardware_spi
        hardware_gpio
        hardware_pio
        hardware_flash
        hardware_watchdog
        tinyusb_device
        tinyusb_board
    )

    # Generate .uf2 and other output formats
    pico_add_extra_outputs(${TARGET_NAME})
endmacro()
