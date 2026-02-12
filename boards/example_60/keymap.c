// ============================================================================
// MARSVLT API — Default Keymap
// ============================================================================
// Define your keyboard's default keycodes using QMK-style KC_* names.
// Each array has SENSOR_COUNT entries, one per key, ordered by sensor enum.
//
// KEYCODES:
//   KC_A..KC_Z         — Letters
//   KC_1..KC_0         — Numbers
//   KC_F1..KC_F12      — Function keys
//   KC_ENT, KC_ESC, KC_BSPC, KC_TAB, KC_SPC — Standard keys
//   KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI      — Left modifiers
//   KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI      — Right modifiers
//   MO(1), MO(2), MO(3) — Momentary layer (active while held)
//   TG(1), TG(2), TG(3) — Toggle layer (tap to switch)
//   KC_FN               — Alias for MO(1)
//   KC_TRNS or ______   — Transparent (fall through to lower layer)
//   KC_BOOTLOADER       — Enter USB bootloader
//   KC_CALIBRATE        — Trigger sensor calibration
//   KC_LED_TOG          — Toggle LED power
//   KC_SOCD_TOG         — Toggle SOCD
//
// Full list: see api/keycodes.h
// ============================================================================

#include "hallscan_config.h"
#include "keycodes.h"

// ============================================================================
// LAYER 0 — Base Layer
// ============================================================================
const uint8_t default_keycodes[SENSOR_COUNT] = {
    // Row 0: Esc, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, -, =, Backspace
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC,

    // Row 1: Tab, Q, W, E, R, T, Y, U, I, O, P, [, ], Backslash
    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,

    // Row 2: Caps, A, S, D, F, G, H, J, K, L, ;, ', Enter
    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,

    // Row 3: LShift, Z, X, C, V, B, N, M, Comma, Period, Slash, RShift
    KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT,

    // Row 4: LCtrl, Win, LAlt, Space, RAlt, Fn, Left, Down, Right, Menu, RCtrl
    KC_LCTL, KC_LGUI, KC_LALT, KC_SPC,  KC_RALT, KC_FN,   KC_LEFT, KC_DOWN, KC_RGHT, KC_APP,  KC_RCTL,
};

// ============================================================================
// LAYER 1 — Fn Layer
// ============================================================================
// Keys set to KC_TRNS (or ______) fall through to Layer 0.
const uint8_t layer1_keycodes[SENSOR_COUNT] = {
    // Row 0: ~, F1-F12, Delete
    KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL,

    // Row 1: mostly transparent
    ______, ______, ______, ______, ______, ______, ______, ______, KC_INS, ______, KC_PSCR, ______, ______, ______,

    // Row 2: transparent
    ______, ______, ______, ______, ______, ______, ______, ______, ______, ______, ______, ______, ______,

    // Row 3: transparent
    ______, ______, ______, ______, ______, ______, ______, ______, ______, ______, ______, ______,

    // Row 4: transparent, with MO(2) on RAlt position
    ______, ______, ______, ______, MO(2), ______, KC_HOME, KC_PGDN, KC_END, ______, ______,
};
