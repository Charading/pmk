/**
 * @file hid_reports.c
 * @brief HID raw report handler implementation
 */

#include "hid_reports.h"
#include "tusb.h"
#include "lighting.h"
#include "profiles.h"
#include "socd.h"
#include <string.h>
#include <stdio.h>

// Keymap access provided by main.c
extern uint8_t (*get_keymap_ptr(void))[SENSOR_COUNT];
extern uint8_t keymap_get_keycount(void);

// Internal state flags for deferred processing
static volatile bool flag_led_power_toggle = false;
static volatile int flag_led_power_set = -1;
static volatile bool flag_socd_toggle = false;
static volatile int flag_socd_set = -1;
static volatile int flag_brightness_set = -1;
static volatile bool flag_actuation_set = false;
static volatile uint8_t actuation_key_idx = 0;
static volatile uint8_t actuation_threshold = 0;
static volatile int flag_layer_set = -1;
static volatile bool flag_keymap_set = false;
static volatile uint8_t keymap_layer = 0;
static volatile uint8_t keymap_key_idx = 0;
static volatile uint8_t keymap_keycode = 0;
static volatile bool flag_calibrate = false;
static volatile bool flag_bootloader = false;
static volatile bool flag_save_profile = false;
static volatile bool flag_load_profile = false;
static volatile bool flag_settings_changed = false;  // Mark that settings need to be saved to flash
static volatile bool flag_led_update = false;
static uint8_t led_update_buffer[LED_COUNT * 3];

// Modern profile commands (deferred; handled in main loop)
static volatile bool flag_profile_save = false;
static volatile bool flag_profile_load = false;
static volatile bool flag_profile_delete = false;
static volatile bool flag_profile_blank = false;
static volatile uint8_t profile_slot = 0;

// Chunked LED update state
static uint8_t led_chunk_buffer[LED_COUNT * 3];
static bool led_chunking_active = false;

// ADC streaming state
static volatile bool flag_adc_stream_enable = false;
static volatile bool adc_stream_enabled = false;
static volatile bool flag_get_key_adc = false;
static volatile uint8_t get_key_adc_idx = 0;

// Advanced calibration state (deferred; handled in main loop)
static volatile bool flag_set_adv_cal_enabled = false;
static volatile bool adv_cal_enabled_req = false;
static volatile bool flag_set_adv_cal_key = false;
static volatile uint8_t adv_cal_key_idx = 0;
static volatile uint16_t adv_cal_release_adc = 0;
static volatile uint16_t adv_cal_press_adc = 0;
static volatile bool flag_get_adv_cal_key = false;
static volatile uint8_t get_adv_cal_key_idx = 0;

// Status reporting
static uint8_t status_flags = 0;
static uint8_t current_layer = 0;
static bool key_states[128] = {0};  // Max 128 keys
static size_t key_count = 0;

// Track last RAW HID interface instance used by the host
// With 4-interface structure: 0=kbd, 1=VIA, 2=AppRaw, 3=RespRaw
// Default to 2 (App Raw) since that's what the software uses
static volatile uint8_t last_raw_instance = 2;

void hid_raw_receive(uint8_t instance, uint8_t report_id, uint8_t const* buffer, uint16_t len)
{
    // 'instance' identifies which HID interface (kbd vs vendor RAW) invoked us.
    // Use it for responses so we reply on the same RAW interface the host is using.

    last_raw_instance = instance;
    
    if (buffer == NULL || len == 0) return;
    
    // Skip report ID if included in buffer
    uint8_t offset = 0;
    if (buffer[0] == REPORT_ID_RAW) {
        offset = 1;
        if (len <= 1) return;
    }
    
    uint8_t cmd = buffer[offset];
    const uint8_t *data = &buffer[offset + 1];
    uint16_t data_len = len - offset - 1;
    

    
    switch (cmd) {
        case CMD_TOGGLE_LED_POWER:
            flag_led_power_toggle = true;
            break;
            
        case CMD_SET_LED_POWER:
            if (data_len >= 1) {
                flag_led_power_set = data[0] ? 1 : 0;
            }
            break;
            
        case CMD_TOGGLE_SOCD:
            flag_socd_toggle = true;
            break;

        case CMD_SET_SOCD:
            if (data_len >= 1) {
                flag_socd_set = data[0] ? 1 : 0;
            }
            break;

        case CMD_SET_SOCD_PAIR: {
            // Set SOCD pair: [pair_idx, key1_idx, key2_idx, mode]
            printf("[HID] CMD_SET_SOCD_PAIR\n");
            if (data_len >= 4) {
                uint8_t pair_idx = data[0];
                uint8_t key1_idx = data[1];
                uint8_t key2_idx = data[2];
                uint8_t mode = data[3];
                bool ok = socd_add_pair(pair_idx, key1_idx, key2_idx, mode);
                (void)ok;
                flag_settings_changed = true;
            }
            break;
        }

        case CMD_GET_SOCD_PAIR: {
            // Get SOCD pair: [pair_idx] -> RESP_SOCD_PAIR
            printf("[HID] CMD_GET_SOCD_PAIR\n");
            if (data_len >= 1) {
                uint8_t pair_idx = data[0];
                socd_pair_t pair;
                bool valid = socd_get_pair(pair_idx, &pair);

                uint8_t resp[64] = {0};
                resp[0] = RESP_SOCD_PAIR;
                resp[1] = pair_idx;
                resp[2] = valid ? pair.key1_idx : 0;
                resp[3] = valid ? pair.key2_idx : 0;
                resp[4] = valid ? pair.mode : 0;
                resp[5] = valid ? 1 : 0;
                if (tud_hid_n_ready(instance)) {
                    tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            }
            break;
        }

        case CMD_DELETE_SOCD_PAIR: {
            // Delete SOCD pair: [pair_idx]
            printf("[HID] CMD_DELETE_SOCD_PAIR\n");
            if (data_len >= 1) {
                uint8_t pair_idx = data[0];
                bool ok = socd_delete_pair(pair_idx);
                (void)ok;
                flag_settings_changed = true;
            }
            break;
        }

        case CMD_GET_ALL_SOCD_PAIRS: {
            // Get all SOCD pairs -> send RESP_SOCD_PAIR for each
            printf("[HID] CMD_GET_ALL_SOCD_PAIRS\n");
            for (uint8_t i = 0; i < SOCD_MAX_PAIRS; i++) {
                socd_pair_t pair;
                bool valid = socd_get_pair(i, &pair);

                uint8_t resp[64] = {0};
                resp[0] = RESP_SOCD_PAIR;
                resp[1] = i;
                resp[2] = valid ? pair.key1_idx : 0;
                resp[3] = valid ? pair.key2_idx : 0;
                resp[4] = valid ? pair.mode : 0;
                resp[5] = valid ? 1 : 0;
                if (tud_hid_n_ready(instance)) {
                    tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            }
            break;
        }

        case CMD_SET_SOCD_MODE: {
            // Set global SOCD mode: [mode]
            printf("[HID] CMD_SET_SOCD_MODE\n");
            if (data_len >= 1) {
                socd_set_global_mode(data[0]);
                flag_settings_changed = true;
            }
            break;
        }

        case CMD_GET_SOCD_MODE: {
            // Get global SOCD mode -> RESP_SOCD_MODE
            printf("[HID] CMD_GET_SOCD_MODE\n");
            uint8_t resp[64] = {0};
            resp[0] = RESP_SOCD_MODE;
            resp[1] = socd_get_global_mode();
            resp[2] = socd_get_enabled() ? 1 : 0;
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }

        case CMD_SET_ACTUATION:
            if (data_len >= 2) {
                actuation_key_idx = data[0];
                actuation_threshold = data[1];
                flag_actuation_set = true;
            }
            break;
            
        case CMD_GET_ACTUATION: {
            // TODO: implement actuation query response
            break;
        }
            
        case CMD_SET_KEYMAP_LEGACY:
            if (data_len >= 3) {
                keymap_layer = data[0];
                keymap_key_idx = data[1];
                keymap_keycode = data[2];
                flag_keymap_set = true;
            }
            break;

        case CMD_SET_KEYMAP: {
            // [layer, k0, k1, ...] (up to 62 bytes per report)
            if (data_len >= 2) {
                uint8_t layer = data[0];
                uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
                if (km && layer < MAX_LAYERS) {
                    uint16_t count = data_len - 1;
                    if (count > SENSOR_COUNT) count = SENSOR_COUNT;
                    for (uint16_t i = 0; i < count; i++) {
                        km[layer][i] = data[1 + i];
                    }
                    flag_settings_changed = true;  // Mark for flash save
                }
            }
            break;
        }
            
        case CMD_SET_KEYCODE:
            // [layer, key_idx, keycode]
            if (data_len >= 3) {
                keymap_layer = data[0];
                keymap_key_idx = data[1];
                keymap_keycode = data[2];
                flag_keymap_set = true;
                flag_settings_changed = true;  // Mark for flash save
            }
            break;

        case CMD_GET_KEYCODE: {
            // [layer, key_idx] -> RESP_GET_KEYCODE [layer, key_idx, keycode]
            uint8_t resp[64] = {0};
            resp[0] = RESP_GET_KEYCODE;
            uint8_t layer = (data_len >= 1) ? data[0] : 0;
            uint8_t key_idx = (data_len >= 2) ? data[1] : 0;
            uint8_t keycode = 0;
            uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
            if (km && layer < MAX_LAYERS && key_idx < SENSOR_COUNT) {
                keycode = km[layer][key_idx];
            }
            resp[1] = layer;
            resp[2] = key_idx;
            resp[3] = keycode;
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }

        case CMD_GET_KEYMAP:
        case CMD_GET_KEYMAP_LEGACY: {
            // Legacy: [layer] -> RESP_GET_KEYMAP [layer, k0..k61]
            // Chunk:  [layer, offset] -> RESP_GET_KEYMAP_CHUNK [layer, total, offset, count, k...]
            const uint8_t layer = (data_len >= 1) ? data[0] : 0;
            const uint8_t total = keymap_get_keycount();
            const bool chunk = (data_len >= 2);
            const uint8_t offset0 = chunk ? data[1] : 0;

            uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
            if (!km || layer >= MAX_LAYERS) break;

            if (!chunk || total <= 62) {
                uint8_t resp[64] = {0};
                resp[0] = RESP_GET_KEYMAP;
                resp[1] = layer;
                const uint8_t n = (total > 62) ? 62 : total;
                for (uint8_t i = 0; i < n; i++) {
                    resp[2 + i] = km[layer][i];
                }
                if (tud_hid_n_ready(instance)) {
                    tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            } else {
                uint8_t resp[64] = {0};
                resp[0] = RESP_GET_KEYMAP_CHUNK;
                resp[1] = layer;
                resp[2] = total;
                resp[3] = offset0;
                const uint8_t maxChunk = 59; // 5-byte header on 64B report
                uint8_t count = 0;
                if (offset0 < total) {
                    count = (uint8_t)((total - offset0) > maxChunk ? maxChunk : (total - offset0));
                }
                resp[4] = count;
                for (uint8_t i = 0; i < count; i++) {
                    resp[5 + i] = km[layer][(uint8_t)(offset0 + i)];
                }
                if (tud_hid_n_ready(instance)) {
                    tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            }
            break;
        }

        case CMD_SET_LAYER:
        case CMD_SET_LAYER_LEGACY:
            if (data_len >= 1 && data[0] < 4) {
                flag_layer_set = data[0];
            }
            break;

        case CMD_GET_LAYER: {
            uint8_t resp[64] = {0};
            resp[0] = RESP_GET_LAYER;
            resp[1] = current_layer;
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }
            
        case CMD_GET_STATUS: {
            uint8_t resp[64] = {0};
            resp[0] = RESP_STATUS;
            resp[1] = status_flags;
            resp[2] = current_layer;
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }
            
        case CMD_SAVE_PROFILE_LEGACY:
            flag_save_profile = true;
            break;

        case CMD_LOAD_PROFILE_LEGACY:
            flag_load_profile = true;
            break;

        case CMD_SAVE_PROFILE:
            if (data_len >= 1) {
                profile_slot = data[0];
                flag_profile_save = true;
            }
            break;

        case CMD_LOAD_PROFILE:
            if (data_len >= 1) {
                profile_slot = data[0];
                flag_profile_load = true;
            }
            break;

        case CMD_DELETE_PROFILE:
            if (data_len >= 1) {
                profile_slot = data[0];
                flag_profile_delete = true;
            }
            break;

        case CMD_CREATE_BLANK_PROFILE:
            if (data_len >= 1) {
                profile_slot = data[0];
                flag_profile_blank = true;
            }
            break;

        case CMD_GET_PROFILE_LIST: {
            // Send RESP_PROFILE_INFO for all slots.
            for (uint8_t slot = 0; slot < 10; slot++) {
                uint8_t resp[64] = {0};
                resp[0] = RESP_PROFILE_INFO;
                resp[1] = slot;
                resp[2] = profiles_slot_valid(slot) ? 1 : 0;
                uint8_t r = 0, g = 0, b = 0;
                profiles_get_slot_color(slot, &r, &g, &b);
                resp[3] = r;
                resp[4] = g;
                resp[5] = b;
                resp[6] = profiles_static_indicator_enabled() ? 1 : 0;
                if (tud_hid_n_ready(last_raw_instance)) {
                    tud_hid_n_report(last_raw_instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            }
            break;
        }

        case CMD_GET_CURRENT_PROFILE: {
            uint8_t resp[64] = {0};
            resp[0] = RESP_CURRENT_PROFILE;
            resp[1] = profiles_get_current_slot();
            resp[2] = 0; // lighting profile unsupported
            resp[3] = profiles_static_indicator_enabled() ? 1 : 0;
            if (tud_hid_n_ready(last_raw_instance)) {
                tud_hid_n_report(last_raw_instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }

        case CMD_SET_PROFILE_COLOR:
            // [slot, r, g, b]
            if (data_len >= 4) {
                profiles_set_slot_color(data[0], data[1], data[2], data[3]);
            }
            break;

        case CMD_SET_STATIC_INDICATOR:
            // [enabled]
            if (data_len >= 1) {
                profiles_set_static_indicator(data[0] ? true : false);
            }
            break;

        case CMD_SET_SIGNALRGB_ZONES:
            if (data_len >= 1) {
                lighting_set_streaming_zones(data[0]);
            }
            break;

        case CMD_GET_SIGNALRGB_ZONES: {
            uint8_t resp[3] = {0};
            resp[0] = RESP_SIGNALRGB_ZONES;
            resp[1] = lighting_get_streaming_zones();
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }

        case CMD_GET_MODIFIED_KEYS: {
            // Send one RESP_MODIFIED_KEY per modified entry (km[layer][idx] != 0)
            uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
            if (!km) break;
            for (uint8_t layer = 0; layer < MAX_LAYERS; layer++) {
                for (uint8_t key_idx = 0; key_idx < SENSOR_COUNT; key_idx++) {
                    const uint8_t v = km[layer][key_idx];
                    if (v == 0) continue;
                    uint8_t resp[64] = {0};
                    resp[0] = RESP_MODIFIED_KEY;
                    resp[1] = layer;
                    resp[2] = key_idx;
                    resp[3] = v;
                    if (tud_hid_n_ready(instance)) {
                        tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                    }
                }
            }
            break;
        }
            
        case CMD_SET_BRIGHTNESS:
            if (data_len >= 1) {
                flag_brightness_set = data[0];
                if (flag_brightness_set > 100) flag_brightness_set = 100;
                // Persist brightness across reboots.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_GET_KEY_STATE: {
            // Send key states response
            uint8_t resp[64] = {0};
            resp[0] = RESP_KEY_STATE;
            // Pack key states as bits
            for (size_t i = 0; i < key_count && i < 128; i++) {
                if (key_states[i]) {
                    resp[1 + (i / 8)] |= (1 << (i % 8));
                }
            }
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }
            
        case CMD_SET_LED_CHUNK:
            // Format: start (1 byte), count (1 byte), RGB data...
            // Same format as Shego - matches host's set-led-color call
            if (data_len >= 3) {
                uint8_t start = data[0];
                uint8_t count = data[1];
                led_chunking_active = true;
                
                for (uint8_t i = 0; i < count && (start + i) < LED_COUNT; i++) {
                    uint8_t idx = start + i;
                    led_chunk_buffer[idx * 3 + 0] = data[2 + i * 3 + 0];
                    led_chunk_buffer[idx * 3 + 1] = data[2 + i * 3 + 1];
                    led_chunk_buffer[idx * 3 + 2] = data[2 + i * 3 + 2];
                }
            }
            break;
            
        case CMD_LED_CHUNK_DONE:
            // All chunks received, copy to update buffer
            if (led_chunking_active) {
                memcpy(led_update_buffer, led_chunk_buffer, sizeof(led_update_buffer));
                flag_led_update = true;
                led_chunking_active = false;
                // Persist Static per-LED colors across reboots.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_LED_EFFECT:
            if (data_len >= 1) {
                lighting_set_effect((led_effect_t)data[0]);
                // Persist effect selection across reboots.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_EFFECT_SPEED:
            if (data_len >= 1) {
                lighting_set_effect_speed(data[0]);
                // Persist animation speed.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_EFFECT_DIR:
            if (data_len >= 1) {
                lighting_set_effect_direction(data[0]);
                // Persist direction.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_EFFECT_COLOR1:
            if (data_len >= 3) {
                lighting_set_effect_color1(data[0], data[1], data[2]);
                // Persist primary color.
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_EFFECT_COLOR2:
            if (data_len >= 3) {
                lighting_set_effect_color2(data[0], data[1], data[2]);
                // Persist secondary color.
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_GRADIENT:
            // [num_colors, (r,g,b)*]
            if (data_len >= 1) {
                uint8_t num = data[0];
                if (num > 8) num = 8;
                if (data_len >= (uint16_t)(1 + num * 3)) {
                    lighting_set_gradient(num, &data[1]);
                    // Persist palette for Wave/Gradient/Radial.
                    flag_settings_changed = true;
                }
            }
            break;

        case CMD_SET_GRADIENT_PARAMS:
            // [orientation, rot_lo, rot_hi]
            if (data_len >= 3) {
                uint8_t orientation = data[0];
                uint16_t rotation = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
                lighting_set_gradient_params(orientation, rotation);
                // Persist gradient axis params.
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_PAINT_LED:
            // [led_idx, r, g, b] - Paint a single LED (highest priority overlay)
            if (data_len >= 4) {
                lighting_set_paint_led(data[0], data[1], data[2], data[3]);
            }
            break;

        case CMD_CLEAR_PAINT:
            // Clear all painted LEDs - return to base effect
            lighting_clear_paint_overlay();
            break;

        // Zone-based lighting (zone byte is ignored; taki_mina uses a single lighting pipeline)
        case CMD_SET_ZONE_LED_EFFECT:
            if (data_len >= 2) {
                lighting_set_effect((led_effect_t)data[1]);
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ZONE_EFFECT_SPEED:
            if (data_len >= 2) {
                lighting_set_effect_speed(data[1]);
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ZONE_EFFECT_DIR:
            if (data_len >= 2) {
                lighting_set_effect_direction(data[1]);
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ZONE_EFFECT_COLOR1:
            if (data_len >= 4) {
                lighting_set_effect_color1(data[1], data[2], data[3]);
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ZONE_EFFECT_COLOR2:
            if (data_len >= 4) {
                lighting_set_effect_color2(data[1], data[2], data[3]);
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ZONE_GRADIENT:
            // [zone, num_colors, (r,g,b)*]
            if (data_len >= 2) {
                uint8_t num = data[1];
                if (num > 8) num = 8;
                if (data_len >= (uint16_t)(2 + num * 3)) {
                    lighting_set_gradient(num, &data[2]);
                    flag_settings_changed = true;
                }
            }
            break;

        case CMD_SET_ZONE_GRADIENT_PARAMS:
            // [zone, orientation, rot_lo, rot_hi] - zone ignored (single pipeline)
            if (data_len >= 4) {
                uint8_t orientation = data[1];
                uint16_t rotation = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
                lighting_set_gradient_params(orientation, rotation);
                flag_settings_changed = true;
            }
            break;
            
        case CMD_SET_ALL_LEDS:
            // Full LED buffer in one report (may be truncated)
            if (data_len >= 3) {
                size_t copy_len = data_len;
                if (copy_len > sizeof(led_update_buffer)) {
                    copy_len = sizeof(led_update_buffer);
                }
                memcpy(led_update_buffer, data, copy_len);
                flag_led_update = true;
            }
            break;
            
        case CMD_CALIBRATE:
            flag_calibrate = true;
            break;
            
        case CMD_BOOTLOADER:
            flag_bootloader = true;
            break;
            
        case CMD_SET_ADC_STREAM:
            // [enabled(1)]
            if (data_len >= 1) {
                adc_stream_enabled = data[0] ? true : false;
                flag_adc_stream_enable = true;
            }
            break;
            
        case CMD_GET_KEY_ADC:
            // [key_idx] - Request ADC value for specific key
            if (data_len >= 1) {
                get_key_adc_idx = data[0];
                flag_get_key_adc = true;
            }
            break;

        case CMD_SET_ADV_CAL_ENABLED:
            // [enabled(1)]
            if (data_len >= 1) {
                adv_cal_enabled_req = data[0] ? true : false;
                flag_set_adv_cal_enabled = true;
                flag_settings_changed = true;
            }
            break;

        case CMD_SET_ADV_CAL_KEY:
            // [key_idx, release_lo, release_hi, press_lo, press_hi]
            if (data_len >= 5) {
                adv_cal_key_idx = data[0];
                adv_cal_release_adc = (uint16_t)(data[1] | (data[2] << 8));
                adv_cal_press_adc = (uint16_t)(data[3] | (data[4] << 8));
                flag_set_adv_cal_key = true;
                flag_settings_changed = true;
            }
            break;

        case CMD_GET_ADV_CAL_KEY:
            // [key_idx] -> RESP_ADV_CALIBRATION
            if (data_len >= 1) {
                get_adv_cal_key_idx = data[0];
                flag_get_adv_cal_key = true;
            }
            break;
            
        case CMD_GET_LED_SETTINGS: {
            uint8_t resp[64] = {0};
            resp[0] = 0xA1;  // LED settings response
            resp[1] = lighting_get_effect();
            resp[2] = lighting_get_effect_speed();
            resp[3] = lighting_get_effect_direction();
            resp[4] = lighting_get_brightness();
            lighting_get_effect_color1(&resp[5], &resp[6], &resp[7]);
            lighting_get_effect_color2(&resp[8], &resp[9], &resp[10]);
            if (tud_hid_n_ready(instance)) {
                tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
            }
            break;
        }
            
        case CMD_SET_LAYER_COLOR:
            // Format: [layer, r, g, b]
            if (data_len >= 4) {
                uint8_t layer = data[0];
                lighting_set_layer_color(layer, data[1], data[2], data[3]);
                if (layer == lighting_get_active_layer()) {
                    lighting_set_active_layer(layer);
                }
                flag_settings_changed = true;
            }
            break;

        case CMD_GET_LAYER_COLORS:
            // Return all 4 layer colors
            {
                uint8_t resp[64] = {0};
                resp[0] = RESP_LAYER_COLORS;
                for (int i = 0; i < 4; i++) {
                    uint8_t r, g, b;
                    lighting_get_layer_color(i, &r, &g, &b);
                    resp[1 + i*3 + 0] = r;
                    resp[1 + i*3 + 1] = g;
                    resp[1 + i*3 + 2] = b;
                }
                if (tud_hid_n_ready(instance)) {
                    tud_hid_n_report(instance, REPORT_ID_RAW, resp, sizeof(resp));
                }
            }
            break;

        default:
            break;
    }
}

bool hid_consume_led_power_toggle(void)
{
    if (flag_led_power_toggle) {
        flag_led_power_toggle = false;
        return true;
    }
    return false;
}

int hid_consume_led_power_set(void)
{
    int val = flag_led_power_set;
    flag_led_power_set = -1;
    return val;
}

bool hid_consume_socd_toggle(void)
{
    if (flag_socd_toggle) {
        flag_socd_toggle = false;
        return true;
    }
    return false;
}

int hid_consume_socd_set(void)
{
    int val = flag_socd_set;
    flag_socd_set = -1;
    return val;
}

int hid_consume_brightness_set(void)
{
    int val = flag_brightness_set;
    flag_brightness_set = -1;
    return val;
}

bool hid_consume_actuation_set(uint8_t *key_idx, uint8_t *threshold)
{
    if (flag_actuation_set) {
        flag_actuation_set = false;
        if (key_idx) *key_idx = actuation_key_idx;
        if (threshold) *threshold = actuation_threshold;
        return true;
    }
    return false;
}

int hid_consume_layer_set(void)
{
    int val = flag_layer_set;
    flag_layer_set = -1;
    return val;
}

bool hid_consume_keymap_set(uint8_t *layer, uint8_t *key_idx, uint8_t *keycode)
{
    if (flag_keymap_set) {
        flag_keymap_set = false;
        if (layer) *layer = keymap_layer;
        if (key_idx) *key_idx = keymap_key_idx;
        if (keycode) *keycode = keymap_keycode;
        return true;
    }
    return false;
}

bool hid_consume_calibrate(void)
{
    if (flag_calibrate) {
        flag_calibrate = false;
        return true;
    }
    return false;
}

bool hid_consume_bootloader(void)
{
    if (flag_bootloader) {
        flag_bootloader = false;
        return true;
    }
    return false;
}

bool hid_consume_save_profile(void)
{
    if (flag_save_profile) {
        flag_save_profile = false;
        return true;
    }
    return false;
}

bool hid_consume_load_profile(void)
{
    if (flag_load_profile) {
        flag_load_profile = false;
        return true;
    }
    return false;
}

bool hid_consume_settings_changed(void)
{
    if (flag_settings_changed) {
        flag_settings_changed = false;
        return true;
    }
    return false;
}

bool hid_consume_profile_save(uint8_t *slot)
{
    if (!flag_profile_save) return false;
    flag_profile_save = false;
    if (slot) *slot = profile_slot;
    return true;
}

bool hid_consume_profile_load(uint8_t *slot)
{
    if (!flag_profile_load) return false;
    flag_profile_load = false;
    if (slot) *slot = profile_slot;
    return true;
}

bool hid_consume_profile_delete(uint8_t *slot)
{
    if (!flag_profile_delete) return false;
    flag_profile_delete = false;
    if (slot) *slot = profile_slot;
    return true;
}

bool hid_consume_profile_blank(uint8_t *slot)
{
    if (!flag_profile_blank) return false;
    flag_profile_blank = false;
    if (slot) *slot = profile_slot;
    return true;
}

bool hid_consume_led_update(uint8_t *buffer, size_t bufsize)
{
    if (flag_led_update) {
        flag_led_update = false;
        size_t copy_len = bufsize < sizeof(led_update_buffer) ? bufsize : sizeof(led_update_buffer);
        memcpy(buffer, led_update_buffer, copy_len);
        return true;
    }
    return false;
}

void hid_set_status_flags(uint8_t flags, uint8_t layer)
{
    status_flags = flags;
    current_layer = layer;
}

void hid_set_key_states(const bool *states, size_t count)
{
    if (count > sizeof(key_states)) {
        count = sizeof(key_states);
    }
    for (size_t i = 0; i < count; i++) {
        key_states[i] = states[i];
    }
    key_count = count;
}

bool hid_consume_adc_stream_enable(bool *enabled) {
    if (!flag_adc_stream_enable) return false;
    if (enabled) *enabled = adc_stream_enabled;
    flag_adc_stream_enable = false;
    return true;
}

bool hid_consume_get_key_adc(uint8_t *key_idx) {
    if (!flag_get_key_adc) return false;
    if (key_idx) *key_idx = get_key_adc_idx;
    flag_get_key_adc = false;
    return true;
}

bool hid_consume_set_adv_cal_enabled(bool *enabled) {
    if (!flag_set_adv_cal_enabled) return false;
    if (enabled) *enabled = adv_cal_enabled_req;
    flag_set_adv_cal_enabled = false;
    return true;
}

bool hid_consume_set_adv_cal_key(uint8_t *key_idx, uint16_t *release_adc, uint16_t *press_adc) {
    if (!flag_set_adv_cal_key) return false;
    if (key_idx) *key_idx = adv_cal_key_idx;
    if (release_adc) *release_adc = adv_cal_release_adc;
    if (press_adc) *press_adc = adv_cal_press_adc;
    flag_set_adv_cal_key = false;
    return true;
}

bool hid_consume_get_adv_cal_key(uint8_t *key_idx) {
    if (!flag_get_adv_cal_key) return false;
    if (key_idx) *key_idx = get_adv_cal_key_idx;
    flag_get_adv_cal_key = false;
    return true;
}

void hid_send_adv_calibration(uint8_t key_idx, bool enabled, uint16_t release_adc, uint16_t press_adc) {
    uint8_t resp[64] = {0};
    resp[0] = RESP_ADV_CALIBRATION;
    resp[1] = key_idx;
    resp[2] = enabled ? 1 : 0;
    resp[3] = release_adc & 0xFF;
    resp[4] = (release_adc >> 8) & 0xFF;
    resp[5] = press_adc & 0xFF;
    resp[6] = (press_adc >> 8) & 0xFF;

    // Match Shego: responses go out over Response Raw (IF3), no report ID.
    if (tud_hid_n_ready(3)) {
        tud_hid_n_report(3, 0, resp, sizeof(resp));
    }
}

void hid_send_adc_values(const uint8_t *values, uint8_t count) {
    // IMPORTANT: Match software parsing behavior in software/main.js
    // - Legacy single-key format:  [RESP_ADC_VALUE, key_idx, adc_lo, adc_hi, depth_mm_x10]
    // - Batched format:            [RESP_ADC_VALUE, count, {key_idx, adc_lo, adc_hi, depth}*count]
    // The app's heuristic can mis-detect a batched packet with count=1 when depth==0,
    // so we MUST send legacy format for count==1 (same as Shego).
    uint8_t resp[64] = {0};
    resp[0] = RESP_ADC_VALUE;
    if (count == 1) {
        // values = {key_idx, adc_lo, adc_hi, depth}
        resp[1] = values[0];
        resp[2] = values[1];
        resp[3] = values[2];
        resp[4] = values[3];
    } else {
        resp[1] = count;
        // Each entry is 4 bytes
        uint8_t max_count = (sizeof(resp) - 2) / 4;
        if (count > max_count) count = max_count;
        memcpy(&resp[2], values, count * 4);
    }

    // Match Shego: ADC responses go out over the Response Raw interface (IF3), no report ID.
    if (tud_hid_n_ready(3)) {
        tud_hid_n_report(3, 0, resp, sizeof(resp));
    }
}
