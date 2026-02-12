// ============================================================================
// MARSVLT API — Internal Configuration Bridge
// ============================================================================
// This file is part of the MARSVLT API core. DO NOT MODIFY.
//
// It includes the user's config.h and:
//   - Normalizes presence-based feature flags to 0/1 values
//   - Provides pin name aliases (user-facing -> internal)
//   - Sets sensible defaults for hidden tuning parameters
//   - Defines derived sensor types (sensor_id_t, mux16_ref_t, etc.)
// ============================================================================

#ifndef HALLSCAN_CONFIG_H
#define HALLSCAN_CONFIG_H

#include <stdint.h>

// Include the user's board configuration
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LOW / HIGH tokens for LED_GATE_ACTIVE
// ============================================================================
// User writes: #define LED_GATE_ACTIVE LOW
// The token LOW is stored unexpanded; it resolves here when evaluated in #if.
#ifndef LOW
#define LOW  0
#endif
#ifndef HIGH
#define HIGH 1
#endif

// ============================================================================
// FEATURE FLAG NORMALIZATION
// ============================================================================
// Presence-based: user writes `#define RGB_ENABLE` (no value needed).
// We normalize to 1 so internal code can use `#if RGB_ENABLE`.
// If the user comments it out, we define it as 0.

#ifdef RGB_ENABLE
  #undef  RGB_ENABLE
  #define RGB_ENABLE 1
#else
  #define RGB_ENABLE 0
#endif

#ifdef ENCODER_ENABLE
  #undef  ENCODER_ENABLE
  #define ENCODER_ENABLE 1
#else
  #define ENCODER_ENABLE 0
#endif

#ifdef DISPLAY_ENABLE
  #undef  DISPLAY_ENABLE
  #define DISPLAY_ENABLE 1
#else
  #define DISPLAY_ENABLE 0
#endif

#ifdef CAPS_LOCK_INDICATOR
  #undef  CAPS_LOCK_INDICATOR
  #define CAPS_LOCK_INDICATOR 1
#else
  #define CAPS_LOCK_INDICATOR 0
#endif

// Legacy compatibility aliases for internal code
#define RGB_ENABLED                  RGB_ENABLE
#define CAPS_LOCK_INDICATOR_ENABLED  CAPS_LOCK_INDICATOR
#define DISPLAY_ENABLED              DISPLAY_ENABLE

// ============================================================================
// PIN NAME ALIASES (user-facing -> internal)
// ============================================================================

// LED data pin: user writes LED_DATA_PIN, internal code uses LED_PIN
#ifdef LED_DATA_PIN
  #ifndef LED_PIN
    #define LED_PIN LED_DATA_PIN
  #endif
#endif

// Encoder pins: user writes ENCODER_A_PIN etc., internal uses ENC_A_PIN etc.
#ifdef ENCODER_A_PIN
  #ifndef ENC_A_PIN
    #define ENC_A_PIN ENCODER_A_PIN
  #endif
#endif

#ifdef ENCODER_B_PIN
  #ifndef ENC_B_PIN
    #define ENC_B_PIN ENCODER_B_PIN
  #endif
#endif

#ifdef ENCODER_SW_PIN
  #ifndef ENC_SW_PIN
    #define ENC_SW_PIN ENCODER_SW_PIN
  #endif
#endif

// Encoder inversion: presence-based -> value-based
#ifdef ENCODER_ROTATION_INVERTED
  #ifndef ENCODER_INVERT
    #define ENCODER_INVERT 1
  #endif
#else
  #ifndef ENCODER_INVERT
    #define ENCODER_INVERT 0
  #endif
#endif

// LED strip reversed: presence-based -> value-based
// Handle both `#define LED_STRIP_REVERSED` (no value) and legacy `= 1`
#ifdef LED_STRIP_REVERSED
  #undef  LED_STRIP_REVERSED
  #define LED_STRIP_REVERSED 1
#else
  #define LED_STRIP_REVERSED 0
#endif

// ============================================================================
// LED GATE NORMALIZATION
// ============================================================================
// User writes:  #define LED_GATE_ACTIVE LOW   (or HIGH)
// After LOW/HIGH are defined above, LED_GATE_ACTIVE expands to 0 or 1.
// Internal code uses LED_GATE_ACTIVE_LOW (1 = active-low, 0 = active-high).

#ifdef LED_GATE_ACTIVE
  #if (LED_GATE_ACTIVE) == 0
    #define LED_GATE_ACTIVE_LOW 1
  #else
    #define LED_GATE_ACTIVE_LOW 0
  #endif
#else
  #ifndef LED_GATE_ACTIVE_LOW
    #define LED_GATE_ACTIVE_LOW 0
  #endif
#endif

// ============================================================================
// BRIGHTNESS DEFAULT
// ============================================================================
#ifndef USB_BRIGHTNESS_PERCENT
  #define USB_BRIGHTNESS_PERCENT 50
#endif

// ============================================================================
// INTERNAL ADC / SENSOR DEFAULTS (hidden from user)
// ============================================================================
// These can still be overridden in config.h if an advanced user needs to.

#ifndef SENSOR_THRESHOLD
  #define SENSOR_THRESHOLD 7
#endif

#ifndef DEBOUNCE_MS
  #define DEBOUNCE_MS 50
#endif

#ifndef CALIBRATION_SAMPLES
  #define CALIBRATION_SAMPLES 8
#endif

#ifndef ADC_MIN_VALID
  #define ADC_MIN_VALID 200
#endif

#ifndef HALLSCAN_HYSTERESIS_PERCENT
  #define HALLSCAN_HYSTERESIS_PERCENT 6
#endif

#ifndef ADC_PRINT_ENABLED
  #define ADC_PRINT_ENABLED 0
#endif

// ============================================================================
// DERIVED SENSOR TYPES
// ============================================================================
// The user's config.h defines sensor_names_t enum ending with
// SENSOR_COUNT_PLUS_1. We derive SENSOR_COUNT and types here.

typedef sensor_names_t sensor_id_t;
#define SENSOR_COUNT (SENSOR_COUNT_PLUS_1 - 1)

// MUX channel -> sensor mapping structure
typedef struct {
    sensor_id_t sensor;     // Sensor ID from enum, or 0 for unmapped
} mux16_ref_t;

// Per-sensor calibration data (storage defined in main.c)
extern uint16_t sensor_baseline[SENSOR_COUNT];
extern uint16_t sensor_thresholds[SENSOR_COUNT];

// ============================================================================
// MUX CHANNEL TABLE EXTERN DECLARATIONS
// ============================================================================
// Declared here so API code can reference them; defined in hallscan_keymap.h

extern const mux16_ref_t mux1_channels[16];
extern const mux16_ref_t mux2_channels[16];
#if MUX_COUNT >= 3
extern const mux16_ref_t mux3_channels[16];
#endif
#if MUX_COUNT >= 4
extern const mux16_ref_t mux4_channels[16];
#endif
#if MUX_COUNT >= 5
extern const mux16_ref_t mux5_channels[16];
#endif
#if MUX_COUNT >= 6
extern const mux16_ref_t mux6_channels[16];
#endif
#if MUX_COUNT >= 7
extern const mux16_ref_t mux7_channels[16];
#endif
#if MUX_COUNT >= 8
extern const mux16_ref_t mux8_channels[16];
#endif

#ifdef __cplusplus
}
#endif

#endif // HALLSCAN_CONFIG_H
