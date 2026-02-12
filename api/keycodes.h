// MARSVLT Keyboard API — QMK-Compatible Keycode Definitions
// Maps QMK-style KC_* names to TinyUSB HID_KEY_* values
//
// Usage in keymap.c:
//   #include "keycodes.h"
//   const uint8_t default_keycodes[SENSOR_COUNT] = { KC_ESC, KC_1, KC_2, ... };

#ifndef MARSVLT_KEYCODES_H
#define MARSVLT_KEYCODES_H

#include "class/hid/hid.h"

// ============================================================================
// TRANSPARENT / NO-OP
// ============================================================================
#define KC_TRNS   0x00    // Transparent — falls through to lower layer
#define KC_NO     0x00    // No keycode (alias)
#define ______    KC_TRNS // QMK shorthand: six underscores = transparent

// ============================================================================
// LETTERS
// ============================================================================
#define KC_A      HID_KEY_A
#define KC_B      HID_KEY_B
#define KC_C      HID_KEY_C
#define KC_D      HID_KEY_D
#define KC_E      HID_KEY_E
#define KC_F      HID_KEY_F
#define KC_G      HID_KEY_G
#define KC_H      HID_KEY_H
#define KC_I      HID_KEY_I
#define KC_J      HID_KEY_J
#define KC_K      HID_KEY_K
#define KC_L      HID_KEY_L
#define KC_M      HID_KEY_M
#define KC_N      HID_KEY_N
#define KC_O      HID_KEY_O
#define KC_P      HID_KEY_P
#define KC_Q      HID_KEY_Q
#define KC_R      HID_KEY_R
#define KC_S      HID_KEY_S
#define KC_T      HID_KEY_T
#define KC_U      HID_KEY_U
#define KC_V      HID_KEY_V
#define KC_W      HID_KEY_W
#define KC_X      HID_KEY_X
#define KC_Y      HID_KEY_Y
#define KC_Z      HID_KEY_Z

// ============================================================================
// NUMBERS
// ============================================================================
#define KC_1      HID_KEY_1
#define KC_2      HID_KEY_2
#define KC_3      HID_KEY_3
#define KC_4      HID_KEY_4
#define KC_5      HID_KEY_5
#define KC_6      HID_KEY_6
#define KC_7      HID_KEY_7
#define KC_8      HID_KEY_8
#define KC_9      HID_KEY_9
#define KC_0      HID_KEY_0

// ============================================================================
// FUNCTION KEYS
// ============================================================================
#define KC_F1     HID_KEY_F1
#define KC_F2     HID_KEY_F2
#define KC_F3     HID_KEY_F3
#define KC_F4     HID_KEY_F4
#define KC_F5     HID_KEY_F5
#define KC_F6     HID_KEY_F6
#define KC_F7     HID_KEY_F7
#define KC_F8     HID_KEY_F8
#define KC_F9     HID_KEY_F9
#define KC_F10    HID_KEY_F10
#define KC_F11    HID_KEY_F11
#define KC_F12    HID_KEY_F12
#define KC_F13    HID_KEY_F13
#define KC_F14    HID_KEY_F14
#define KC_F15    HID_KEY_F15
#define KC_F16    HID_KEY_F16
#define KC_F17    HID_KEY_F17
#define KC_F18    HID_KEY_F18
#define KC_F19    HID_KEY_F19
#define KC_F20    HID_KEY_F20
#define KC_F21    HID_KEY_F21
#define KC_F22    HID_KEY_F22
#define KC_F23    HID_KEY_F23
#define KC_F24    HID_KEY_F24

// ============================================================================
// MODIFIERS
// ============================================================================
#define KC_LCTL   HID_KEY_CONTROL_LEFT
#define KC_LSFT   HID_KEY_SHIFT_LEFT
#define KC_LALT   HID_KEY_ALT_LEFT
#define KC_LGUI   HID_KEY_GUI_LEFT
#define KC_RCTL   HID_KEY_CONTROL_RIGHT
#define KC_RSFT   HID_KEY_SHIFT_RIGHT
#define KC_RALT   HID_KEY_ALT_RIGHT
#define KC_RGUI   HID_KEY_GUI_RIGHT

// Aliases
#define KC_LCTRL  KC_LCTL
#define KC_LSHIFT KC_LSFT
#define KC_RCTRL  KC_RCTL
#define KC_RSHIFT KC_RSFT
#define KC_LWIN   KC_LGUI
#define KC_RWIN   KC_RGUI

// ============================================================================
// STANDARD KEYS
// ============================================================================
#define KC_ENT    HID_KEY_ENTER
#define KC_ESC    HID_KEY_ESCAPE
#define KC_BSPC   HID_KEY_BACKSPACE
#define KC_TAB    HID_KEY_TAB
#define KC_SPC    HID_KEY_SPACE
#define KC_MINS   HID_KEY_MINUS
#define KC_EQL    HID_KEY_EQUAL
#define KC_LBRC   HID_KEY_BRACKET_LEFT
#define KC_RBRC   HID_KEY_BRACKET_RIGHT
#define KC_BSLS   HID_KEY_BACKSLASH
#define KC_SCLN   HID_KEY_SEMICOLON
#define KC_QUOT   HID_KEY_APOSTROPHE
#define KC_GRV    HID_KEY_GRAVE
#define KC_COMM   HID_KEY_COMMA
#define KC_DOT    HID_KEY_PERIOD
#define KC_SLSH   HID_KEY_SLASH
#define KC_CAPS   HID_KEY_CAPS_LOCK

// Aliases (long names)
#define KC_ENTER     KC_ENT
#define KC_ESCAPE    KC_ESC
#define KC_BACKSPACE KC_BSPC
#define KC_SPACE     KC_SPC
#define KC_MINUS     KC_MINS
#define KC_EQUAL     KC_EQL
#define KC_SEMICOLON KC_SCLN
#define KC_QUOTE     KC_QUOT
#define KC_GRAVE     KC_GRV
#define KC_COMMA     KC_COMM
#define KC_SLASH     KC_SLSH

// ============================================================================
// NAVIGATION
// ============================================================================
#define KC_INS    HID_KEY_INSERT
#define KC_HOME   HID_KEY_HOME
#define KC_PGUP   HID_KEY_PAGE_UP
#define KC_DEL    HID_KEY_DELETE
#define KC_END    HID_KEY_END
#define KC_PGDN   HID_KEY_PAGE_DOWN
#define KC_RIGHT  HID_KEY_ARROW_RIGHT
#define KC_LEFT   HID_KEY_ARROW_LEFT
#define KC_DOWN   HID_KEY_ARROW_DOWN
#define KC_UP     HID_KEY_ARROW_UP
#define KC_RGHT   KC_RIGHT

// Aliases
#define KC_INSERT    KC_INS
#define KC_DELETE    KC_DEL
#define KC_PAGE_UP   KC_PGUP
#define KC_PAGE_DOWN KC_PGDN

// ============================================================================
// PRINT SCREEN / SCROLL LOCK / PAUSE
// ============================================================================
#define KC_PSCR   HID_KEY_PRINT_SCREEN
#define KC_SCRL   HID_KEY_SCROLL_LOCK
#define KC_PAUS   HID_KEY_PAUSE
#define KC_BRK    KC_PAUS

// ============================================================================
// NUMPAD
// ============================================================================
#define KC_NUM    HID_KEY_NUM_LOCK
#define KC_PSLS   HID_KEY_KEYPAD_DIVIDE
#define KC_PAST   HID_KEY_KEYPAD_MULTIPLY
#define KC_PMNS   HID_KEY_KEYPAD_SUBTRACT
#define KC_PPLS   HID_KEY_KEYPAD_ADD
#define KC_PENT   HID_KEY_KEYPAD_ENTER
#define KC_P1     HID_KEY_KEYPAD_1
#define KC_P2     HID_KEY_KEYPAD_2
#define KC_P3     HID_KEY_KEYPAD_3
#define KC_P4     HID_KEY_KEYPAD_4
#define KC_P5     HID_KEY_KEYPAD_5
#define KC_P6     HID_KEY_KEYPAD_6
#define KC_P7     HID_KEY_KEYPAD_7
#define KC_P8     HID_KEY_KEYPAD_8
#define KC_P9     HID_KEY_KEYPAD_9
#define KC_P0     HID_KEY_KEYPAD_0
#define KC_PDOT   HID_KEY_KEYPAD_DECIMAL

// ============================================================================
// INTERNATIONAL
// ============================================================================
#define KC_NUHS   HID_KEY_EUROPE_2        // Non-US # (ISO hash)
#define KC_NUBS   HID_KEY_KANJI3          // Non-US \ (ISO backslash)
#define KC_APP    HID_KEY_APPLICATION      // Menu / Context key

// ============================================================================
// LAYER MODIFIERS
// ============================================================================
// MO(n) — Momentary: layer active while held
#define MO(n)     (0xA7 + (n))   // MO(1)=0xA8, MO(2)=0xA9, MO(3)=0xAA
#define KC_MO1    MO(1)
#define KC_MO2    MO(2)
#define KC_MO3    MO(3)

// TG(n) — Toggle: tap to switch layer on/off
#define TG(n)     (0xAA + (n))   // TG(1)=0xAB, TG(2)=0xAC, TG(3)=0xAD
#define KC_TG1    TG(1)
#define KC_TG2    TG(2)
#define KC_TG3    TG(3)

// Fn = MO(1) (convenience alias)
#define KC_FN     MO(1)

// ============================================================================
// SPECIAL / SYSTEM
// ============================================================================
#define KC_BOOTLOADER  0xF8   // Enter USB bootloader (BOOTSEL)
#define KC_REBOOT      0xEC   // Watchdog reboot
#define KC_CALIBRATE   0xF9   // Trigger hall sensor calibration
#define KC_LED_TOG     0xFA   // Toggle LED power on/off
#define KC_SOCD_TOG    0xFB   // Toggle SOCD on/off

// ============================================================================
// CONSUMER / MEDIA KEYS (sent via Consumer Control report)
// ============================================================================
// These use the 0xE0-0xE7 range (processed specially in HID report building)
#define KC_MUTE   0xE0   // Mute
#define KC_VOLD   0xE1   // Volume down
#define KC_VOLU   0xE2   // Volume up
#define KC_MNXT   0xE3   // Next track
#define KC_MPRV   0xE4   // Previous track
#define KC_MSTP   0xE5   // Stop
#define KC_MPLY   0xE6   // Play/Pause
#define KC_EJCT   0xE7   // Eject

#endif // MARSVLT_KEYCODES_H
