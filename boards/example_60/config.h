// ============================================================================
// MARSVLT API — Board Configuration
// ============================================================================
// This is the main configuration file for your keyboard.
// Edit the values below to match your PCB hardware.
//
// See DOCUMENTATION.md for details on every define.
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include "hardware/gpio.h"

// ============================================================================
// MCU SELECTION
// ============================================================================
// Uncomment ONE of these to match your microcontroller:
#define MCU_RP2040     // Raspberry Pi Pico / RP2040
// #define MCU_RP2350  // Raspberry Pi Pico 2 / RP2350

// ============================================================================
// USB IDENTIFICATION
// ============================================================================
// Get a unique VID/PID from https://pid.codes or use a testing range.
#define USB_VID         0x1234
#define USB_PID         0x5678
#define KEYBOARD_NAME   "Example60"
#define MANUFACTURER    "Your Name"

// ============================================================================
// FEATURES
// ============================================================================
// Define to enable, comment out to disable. No value needed.
#define RGB_ENABLE
#define CAPS_LOCK_INDICATOR
// #define ENCODER_ENABLE
// #define DISPLAY_ENABLE

// ============================================================================
// LED CONFIGURATION
// ============================================================================
#define LED_DATA_PIN        22      // GPIO pin for WS2812 data line

#define LED_COUNT           64      // Total LEDs on strip (key + ambient + status)
#define KEY_LED_COUNT       64      // Per-key LEDs
#define AMBIENT_LED_COUNT   0       // Ambient/underglow strip LEDs (0 if none)
#define STATUS_LED_COUNT    0       // Status indicator LEDs (0 if none)

// LED power gate — if your PCB has a MOSFET controlling 5V to LEDs,
// uncomment these two lines:
// #define LED_GATE_PIN        23
// #define LED_GATE_ACTIVE     LOW  // LOW = P-FET (active-low), HIGH = N-FET

// Uncomment if your LED strip data enters from the opposite end:
// #define LED_STRIP_REVERSED

// LED physical wiring map — maps logical key index to physical LED strip index.
// Accounts for serpentine (zigzag) wiring on your PCB.
static const uint8_t led_position_map[64] = {
    // Row 0 (14 keys): Forward
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    // Row 1 (14 keys): Reverse (serpentine)
    27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14,
    // Row 2 (13 keys): Forward
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    // Row 3 (12 keys): Reverse (serpentine)
    52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
    // Row 4 (11 keys): Forward
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
};

// ============================================================================
// ENCODER CONFIGURATION
// ============================================================================
// Uncomment these if ENCODER_ENABLE is defined above.
// #define ENCODER_A_PIN               7
// #define ENCODER_B_PIN               8
// #define ENCODER_SW_PIN              9
// #define ENCODER_ROTATION_INVERTED       // Uncomment to swap CW/CCW

// ============================================================================
// CAPS LOCK INDICATOR
// ============================================================================
#define CAPS_LOCK_LED_INDEX         29  // LED index of the Caps Lock key

// ============================================================================
// MUX PIN ASSIGNMENTS (HC4067 multiplexer)
// ============================================================================
// S0-S3: 4 address pins shared across ALL MUX chips
#define MUX_S0_PIN  10
#define MUX_S1_PIN  11
#define MUX_S2_PIN  12
#define MUX_S3_PIN  13

// ADC input pins — one per MUX chip (COM output -> MCU ADC)
// RP2040 ADC: GP26 (ADC0), GP27 (ADC1), GP28 (ADC2), GP29 (ADC3) — max 4
// RP2350 ADC: GP26-GP29 + GP40-GP43 — up to 8
#define MUX1_ADC_PIN    26
#define MUX2_ADC_PIN    27
#define MUX3_ADC_PIN    28
#define MUX4_ADC_PIN    29

// Total number of MUX chips on your PCB
#define MUX_COUNT       4

// ============================================================================
// SENSOR DEFINITIONS
// ============================================================================
// Define one entry per key on your keyboard. Group by row for clarity.
// The enum MUST start at 1 (value 0 = unmapped channel).
// End with SENSOR_COUNT_PLUS_1 — the API uses this to auto-calculate totals.
//
// Naming convention: S_<KEYNAME>  (e.g., S_ESC, S_A, S_SPC)
// These names are used in hallscan_keymap.h to map MUX channels to keys.
// You can also use raw numbers (1, 2, 3...) in hallscan_keymap.h if preferred.

#define MAX_KEYS 64

typedef enum sensor_names {
    // Row 0 (14 keys): Esc, 1-0, -, =, Backspace
    S_ESC = 1, S_1, S_2, S_3, S_4, S_5, S_6, S_7, S_8, S_9, S_0, S_MINS, S_EQLS, S_BSPC,

    // Row 1 (14 keys): Tab, Q-], Backslash
    S_TAB, S_Q, S_W, S_E, S_R, S_T, S_Y, S_U, S_I, S_O, S_P, S_LBRC, S_RBRC, S_BSLS,

    // Row 2 (13 keys): Caps, A-', Enter
    S_CAPS, S_A, S_S, S_D, S_F, S_G, S_H, S_J, S_K, S_L, S_SCLN, S_QUOT, S_ENT,

    // Row 3 (12 keys): LShift, Z-/, RShift
    S_LSFT, S_Z, S_X, S_C, S_V, S_B, S_N, S_M, S_COMM, S_DOT, S_SLSH, S_RSFT,

    // Row 4 (11 keys): LCtrl, Win, LAlt, Space, RAlt, Fn, Left, Down, Right, Menu, RCtrl
    S_LCTL, S_WIN, S_LALT, S_SPC, S_RALT, S_FN, S_LEFT, S_DOWN, S_RGHT, S_MENU, S_RCTL,

    SENSOR_COUNT_PLUS_1,    // <-- DO NOT REMOVE
} sensor_names_t;

#endif // CONFIG_H
