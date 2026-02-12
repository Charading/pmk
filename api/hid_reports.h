/**
 * @file hid_reports.h
 * @brief HID raw report handler for vendor-specific commands
 * 
 * Provides functions to handle HID reports from the host software
 * for LED control, SOCD settings, actuation tuning, and more.
 */

#ifndef HID_REPORTS_H
#define HID_REPORTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hallscan_config.h"
#include "usb_descriptors.h"

// Keymap dimensions (must match main.c)
#ifndef MAX_LAYERS
#define MAX_LAYERS 4
#endif

// HID Commands (must match software main.js CMD constants)
#define CMD_SET_LEDS           0x01  // Deprecated, use chunked
#define CMD_SET_ALL_LEDS       0x02
#define CMD_TOGGLE_LED_POWER   0x03
#define CMD_SET_LED_POWER      0x04
#define CMD_TOGGLE_SOCD        0x06
#define CMD_SET_SOCD           0x07
#define CMD_SET_ACTUATION      0x08
#define CMD_GET_ACTUATION      0x09
#define CMD_SET_KEYMAP_LEGACY  0x0A
#define CMD_GET_KEYMAP_LEGACY  0x0B
#define CMD_SET_LAYER_LEGACY   0x0C
#define CMD_GET_STATUS         0x0D
#define CMD_SAVE_PROFILE_LEGACY 0x0E
#define CMD_LOAD_PROFILE_LEGACY 0x0F
#define CMD_SET_BRIGHTNESS     0x10
#define CMD_GET_KEY_STATE      0x11
#define CMD_SET_ALL_ACTUATIONS 0x12
#define CMD_SET_LED_CHUNK      0x13  // Chunked LED update
#define CMD_LED_CHUNK_DONE     0x14  // Signal chunks complete
#define CMD_SET_LED_EFFECT     0x15  // Set LED effect type
#define CMD_SET_EFFECT_SPEED   0x16  // Set effect animation speed
#define CMD_SET_EFFECT_DIR     0x17  // Set effect direction
#define CMD_SET_EFFECT_COLOR1  0x18  // Set primary color
#define CMD_SET_EFFECT_COLOR2  0x19  // Set secondary color
#define CMD_SET_GRADIENT       0x1A  // Set gradient colors
#define CMD_SET_PAINT_LED      0x1B  // Set single painted LED [idx, r, g, b]
#define CMD_CLEAR_PAINT        0x1C  // Clear all painted LEDs
#define CMD_SET_GRADIENT_PARAMS 0x36  // Set gradient orientation/rotation [orientation, rot_lo, rot_hi]

// Zone-based lighting (zone = 0 main, 1 ambient, 2 status)
#define CMD_SET_ZONE_LED_EFFECT        0x37  // [zone, effect]
#define CMD_SET_ZONE_EFFECT_SPEED      0x38  // [zone, speed]
#define CMD_SET_ZONE_EFFECT_DIR        0x39  // [zone, direction]
#define CMD_SET_ZONE_EFFECT_COLOR1     0x3A  // [zone, r, g, b]
#define CMD_SET_ZONE_EFFECT_COLOR2     0x3B  // [zone, r, g, b]
#define CMD_SET_ZONE_GRADIENT          0x3C  // [zone, num_colors, (r,g,b)*]
#define CMD_SET_ZONE_GRADIENT_PARAMS   0x3D  // [zone, orientation, rot_lo, rot_hi]
#define CMD_CALIBRATE          0x1F  // Trigger sensor calibration
#define CMD_BOOTLOADER         0x20  // Reboot to bootloader mode
#define CMD_GET_LED_SETTINGS   0x21  // Get current LED settings
#define CMD_GET_LED_COLORS     0x22  // Get current LED colors buffer

// ADC streaming for live preview
#define CMD_SET_ADC_STREAM     0x55  // Enable/disable ADC streaming [enabled(1)]
#define CMD_GET_KEY_ADC        0x56  // Request ADC value for specific key [key_idx]

// Advanced sensor calibration (per-key ADC endpoints)
// - Enable: [enabled(1)]
// - Set key: [key_idx, release_lo, release_hi, press_lo, press_hi]
// - Get key: [key_idx] -> RESP_ADV_CALIBRATION
#define CMD_SET_ADV_CAL_ENABLED 0x61
#define CMD_SET_ADV_CAL_KEY     0x62
#define CMD_GET_ADV_CAL_KEY     0x63

// Layer and keymap commands (modern)
#define CMD_SET_LAYER          0x23  // Set current layer (0-3)
#define CMD_GET_LAYER          0x24  // Get current layer
#define CMD_SET_KEYCODE        0x25  // Set keycode for layer/sensor [layer, sensor_idx, keycode]
#define CMD_GET_KEYCODE        0x26  // Get keycode for layer/sensor [layer, sensor_idx]
#define CMD_GET_KEYMAP         0x27  // Get entire keymap for layer (legacy or chunked depending on args)
#define CMD_SET_KEYMAP         0x28  // Set entire keymap for layer

// Keymap sync helpers
#define CMD_GET_MODIFIED_KEYS  0x60  // Get all modified keys -> RESP_MODIFIED_KEY (one per key)

// SignalRGB per-zone streaming control
#define CMD_SET_SIGNALRGB_ZONES 0x68  // Set SignalRGB per-zone control [zone_mask]
#define CMD_GET_SIGNALRGB_ZONES 0x69  // Get SignalRGB zone mask -> RESP_SIGNALRGB_ZONES

// SOCD pair configuration (must match keyboard/hid/hid_reports.h)
#define CMD_SET_SOCD_PAIR      0x6A  // Set SOCD pair [pair_idx, key1_idx, key2_idx, mode]
#define CMD_GET_SOCD_PAIR      0x6B  // Get SOCD pair [pair_idx] -> RESP_SOCD_PAIR
#define CMD_DELETE_SOCD_PAIR   0x6C  // Delete SOCD pair [pair_idx]
#define CMD_GET_ALL_SOCD_PAIRS 0x6D  // Get all SOCD pairs -> multiple RESP_SOCD_PAIR
#define CMD_SET_SOCD_MODE      0x6E  // Set global SOCD resolution mode [mode]
#define CMD_GET_SOCD_MODE      0x6F  // Get global SOCD mode -> RESP_SOCD_MODE

// Profile management commands (0x70-0x7F range)
#define CMD_SAVE_PROFILE        0x70  // Save keymap profile [slot]
#define CMD_LOAD_PROFILE        0x71  // Load keymap profile [slot]
#define CMD_DELETE_PROFILE      0x72  // Delete/reset profile [slot]
#define CMD_GET_PROFILE_LIST    0x73  // Get list of valid profiles -> RESP_PROFILE_LIST
#define CMD_GET_CURRENT_PROFILE 0x74  // Get current active profile -> RESP_CURRENT_PROFILE
#define CMD_SET_PROFILE_COLOR   0x75  // Set profile indicator color [slot, r, g, b]
#define CMD_SET_STATIC_INDICATOR 0x76 // Enable/disable static indicator [enabled]
#define CMD_SAVE_LIGHTING_PROFILE 0x77 // Save lighting profile to slot [slot]
#define CMD_LOAD_LIGHTING_PROFILE 0x78 // Load lighting profile from slot [slot]
#define CMD_GET_LIGHTING_PROFILE_INFO 0x79 // Get lighting profile info [slot]
#define CMD_CREATE_BLANK_PROFILE 0x7A // Create a blank profile at slot [slot]
#define CMD_SET_LAYER_COLOR     0x7B  // Set layer indicator color [layer, r, g, b]
#define CMD_GET_LAYER_COLORS    0x7C  // Get all layer colors -> RESP_LAYER_COLORS

// Response commands
#define RESP_ACTUATION  0x89
#define RESP_KEYMAP     0x8B
#define RESP_STATUS     0x8D
#define RESP_KEY_STATE  0x91

// ADC value response
#define RESP_ADC_VALUE  0xB6  // ADC value response [key_idx, adc_lo, adc_hi, depth_mm_x10]

// Modern responses used by the app
#define RESP_GET_LAYER        0xA4
#define RESP_GET_KEYCODE      0xA6
#define RESP_GET_KEYMAP       0xA7
#define RESP_GET_KEYMAP_CHUNK 0xAB

// Keymap sync + profiles + SignalRGB
#define RESP_MODIFIED_KEY     0xBC  // Modified key response [layer, key_idx, keycode]
#define RESP_PROFILE_INFO     0xBD  // Profile info [slot, valid, r, g, b, static_indicator]
#define RESP_CURRENT_PROFILE  0xBE  // Current profile [keymap_profile, lighting_profile, static_enabled]
#define RESP_LIGHTING_PROFILE_INFO 0xBF // Lighting profile info [slot, valid]
#define RESP_PROFILE_CHANGED  0xC0  // Profile changed notification [keymap_profile, lighting_profile]
#define RESP_PROFILE_LIST     0xC1  // Profile list [valid_lo, valid_hi, (r,g,b)*10]

// Advanced calibration response
// [key_idx, enabled(1), release_lo, release_hi, press_lo, press_hi]
#define RESP_ADV_CALIBRATION  0xC2
#define RESP_LAYER_COLORS     0xC5  // Layer colors response [r0,g0,b0, r1,g1,b1, r2,g2,b2, r3,g3,b3]
#define RESP_SIGNALRGB_ZONES  0xC6  // SignalRGB zones response [zone_mask]
#define RESP_SOCD_PAIR        0xC7  // SOCD pair response [pair_idx, key1_idx, key2_idx, mode, valid]
#define RESP_SOCD_MODE        0xC8  // SOCD mode response [mode, enabled]

/**
 * @brief Handle incoming raw HID report from host
 * 
 * @param instance TinyUSB instance
 * @param report_id Report ID (may be 0 if included in buffer)
 * @param buffer Report data buffer
 * @param len Length of buffer
 */
void hid_raw_receive(uint8_t instance, uint8_t report_id, uint8_t const* buffer, uint16_t len);

/**
 * @brief Check if LED power toggle was requested
 * @return true if toggle requested (clears flag)
 */
bool hid_consume_led_power_toggle(void);

/**
 * @brief Get LED power set request
 * @return -1 if no request, 0 or 1 for requested state (clears flag)
 */
int hid_consume_led_power_set(void);

/**
 * @brief Check if SOCD toggle was requested
 * @return true if toggle requested (clears flag)
 */
bool hid_consume_socd_toggle(void);

/**
 * @brief Get SOCD set request
 * @return -1 if no request, 0 or 1 for requested state (clears flag)
 */
int hid_consume_socd_set(void);

/**
 * @brief Get brightness set request
 * @return -1 if no request, 0-100 for brightness percent (clears flag)
 */
int hid_consume_brightness_set(void);

/**
 * @brief Get actuation set request
 * @param key_idx Output: key index
 * @param threshold Output: threshold value
 * @return true if request pending (clears flag)
 */
bool hid_consume_actuation_set(uint8_t *key_idx, uint8_t *threshold);

/**
 * @brief Get layer set request
 * @return -1 if no request, 0-3 for layer number (clears flag)
 */
int hid_consume_layer_set(void);

/**
 * @brief Get keymap set request
 * @param layer Output: layer number
 * @param key_idx Output: key index
 * @param keycode Output: HID keycode
 * @return true if request pending (clears flag)
 */
bool hid_consume_keymap_set(uint8_t *layer, uint8_t *key_idx, uint8_t *keycode);

/**
 * @brief Check if calibration was requested
 * @return true if calibration requested (clears flag)
 */
bool hid_consume_calibrate(void);

/**
 * @brief Check if bootloader reboot was requested
 * @return true if bootloader requested (clears flag)
 */
bool hid_consume_bootloader(void);

/**
 * @brief Check if save profile was requested
 * @return true if save requested (clears flag)
 */
bool hid_consume_save_profile(void);

/**
 * @brief Check if load profile was requested
 * @return true if load requested (clears flag)
 */
bool hid_consume_load_profile(void);

/**
 * @brief Check if settings were changed and need to be saved
 * @return true if settings changed (clears flag)
 */
bool hid_consume_settings_changed(void);

// Modern profile commands (0x70+)
bool hid_consume_profile_save(uint8_t *slot);
bool hid_consume_profile_load(uint8_t *slot);
bool hid_consume_profile_delete(uint8_t *slot);
bool hid_consume_profile_blank(uint8_t *slot);

/**
 * @brief Get pending LED update buffer
 * @param buffer Output buffer (must be LED_COUNT*3 bytes)
 * @param bufsize Size of output buffer
 * @return true if LED update pending (clears flag)
 */
bool hid_consume_led_update(uint8_t *buffer, size_t bufsize);

/**
 * @brief Set status flags for HID reporting
 * @param flags Status flags (bit0=LED power, bit1=SOCD)
 * @param layer Current layer number
 */
void hid_set_status_flags(uint8_t flags, uint8_t layer);

/**
 * @brief Set key states for host monitoring
 * @param states Array of key states (0-indexed)
 * @param count Number of keys
 */
void hid_set_key_states(const bool *states, size_t count);

/**
 * @brief Check if ADC streaming enable/disable was requested
 * @param enabled Output: requested enable state
 * @return true if request pending (clears flag)
 */
bool hid_consume_adc_stream_enable(bool *enabled);

/**
 * @brief Check if ADC value request was made
 * @param key_idx Output: key index to read
 * @return true if request pending (clears flag)
 */
bool hid_consume_get_key_adc(uint8_t *key_idx);

/**
 * @brief Send ADC values for multiple keys
 * @param values Array of {key_idx, adc_lo, adc_hi, depth} tuples
 * @param count Number of keys
 */
void hid_send_adc_values(const uint8_t *values, uint8_t count);

bool hid_consume_set_adv_cal_enabled(bool *enabled);
bool hid_consume_set_adv_cal_key(uint8_t *key_idx, uint16_t *release_adc, uint16_t *press_adc);
bool hid_consume_get_adv_cal_key(uint8_t *key_idx);
void hid_send_adv_calibration(uint8_t key_idx, bool enabled, uint16_t release_adc, uint16_t press_adc);

#endif // HID_REPORTS_H
