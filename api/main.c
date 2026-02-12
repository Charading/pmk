#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"  // For reset_usb_boot()
#include "hardware/watchdog.h"  // For watchdog_reboot() (Reset keycode)
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>  // for abs()
// TinyUSB
#include "tusb.h"
#include "class/hid/hid.h"

// Board config + API normalization (pins, LED count, sensor types)
#include "hallscan_config.h"

// MUX channel -> sensor wiring map (board-specific)
#include "hallscan_keymap.h"

// Board-specific default keycodes
#include "default_keycodes.h"

// HID/raw handler for vendor commands
#include "hid_reports.h"

// Modern profile storage (app protocol 0x70+)
#include "profiles.h"

// SOCD + encoder
#include "socd.h"
#include "encoder.h"

// LED control
#include "lighting.h"

// Onboard LED for status indication
#define ONBOARD_LED     25   // GP25

// Custom keycodes (must match software keycodes.js)
#define KC_BOOTLOADER   0xF8
#define KC_REBOOT       0xEC
#define KC_CALIBRATE    0xF9
#define KC_LED_TOG      0xFA
#define KC_SOCD_TOG     0xFB

// Define per-sensor baseline and threshold arrays (hallscan_config.h declares them extern)
uint16_t sensor_baseline[SENSOR_COUNT];
uint16_t sensor_thresholds[SENSOR_COUNT];

// ========================================
// KEYMAP STORAGE
// ========================================
// Support up to 4 layers, each with SENSOR_COUNT keys
// Value 0 means "use default", non-zero is the HID keycode
#define MAX_LAYERS 4
static uint8_t keymap[MAX_LAYERS][SENSOR_COUNT];

// default_keycodes is defined in board-specific default_keycodes.h

// MO (Momentary) layer tracking — like Saturn60/VLT
// mo_held_sensors[L] = sensor index that activated MO(L), or -1 if not held
static int8_t mo_held_sensors[MAX_LAYERS] = {-1, -1, -1, -1};

// Get effective keycode for a sensor on a given layer
// Fallthrough: current layer → layer 0 runtime → layer 1 compiled defaults → layer 0 compiled defaults
static uint8_t get_keycode(uint8_t layer, uint8_t sensor_idx) {
    if (sensor_idx >= SENSOR_COUNT) return 0;
    if (layer >= MAX_LAYERS) layer = 0;

    // Check runtime keymap on requested layer
    uint8_t kc = keymap[layer][sensor_idx];
    if (kc != 0) return kc;

    // Check compiled-in defaults for this layer (layer 1 has F-keys etc.)
    if (layer == 1 && sensor_idx < sizeof(layer1_keycodes)) {
        kc = layer1_keycodes[sensor_idx];
        if (kc != 0) return kc;
    }

    // Fall through to layer 0 runtime
    if (layer > 0) {
        kc = keymap[0][sensor_idx];
        if (kc != 0) return kc;
    }

    // Fall through to compiled-in base defaults
    if (sensor_idx < sizeof(default_keycodes)) {
        return default_keycodes[sensor_idx];
    }
    return 0;
}

// Check if keycode is a modifier (0xE0-0xE7)
static inline bool is_modifier_keycode(uint8_t kc) {
    return kc >= HID_KEY_CONTROL_LEFT && kc <= HID_KEY_GUI_RIGHT;
}

// Get modifier bit for a modifier keycode
static inline uint8_t get_modifier_bit(uint8_t kc) {
    return 1 << (kc - HID_KEY_CONTROL_LEFT);
}

// Check if keycode is an MO layer key (0xA8-0xAA)
static inline bool is_mo_keycode(uint8_t kc) {
    return kc >= 0xA8 && kc <= 0xAA;
}

// Check if keycode is a TG layer key (0xAB-0xAD)
static inline bool is_tg_keycode(uint8_t kc) {
    return kc >= 0xAB && kc <= 0xAD;
}

// Release all keys by sending empty HID report — prevents stuck modifiers
static void hid_release_all_keys(void) {
    uint8_t report[8] = {0};
    if (tud_hid_n_ready(0)) {
        tud_hid_n_report(0, 1, report, sizeof(report));
    }
    // Also release consumer keys
    uint16_t zero = 0;
    sleep_ms(5);
    if (tud_hid_n_ready(0)) {
        tud_hid_n_report(0, 2, &zero, sizeof(zero));
    }
}

// Convert special keycodes to consumer control usage codes
static bool keycode_to_consumer_usage(uint8_t code, uint16_t *usage_out) {
    if (!usage_out) return false;
    switch (code) {
        case 0xB5: *usage_out = HID_USAGE_CONSUMER_SCAN_NEXT; return true;
        case 0xB6: *usage_out = HID_USAGE_CONSUMER_SCAN_PREVIOUS; return true;
        case 0xCD: *usage_out = HID_USAGE_CONSUMER_PLAY_PAUSE; return true;
        case 0x7F: *usage_out = HID_USAGE_CONSUMER_MUTE; return true;
        case 0x80: *usage_out = HID_USAGE_CONSUMER_VOLUME_INCREMENT; return true;
        case 0x81: *usage_out = HID_USAGE_CONSUMER_VOLUME_DECREMENT; return true;
        case 0x6F: *usage_out = HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT; return true;
        case 0x70: *usage_out = HID_USAGE_CONSUMER_BRIGHTNESS_DECREMENT; return true;
        default: return false;
    }
}

// Expose keymap for HID/profile modules
uint8_t (*get_keymap_ptr(void))[SENSOR_COUNT] {
    return keymap;
}

uint8_t keymap_get_keycount(void) {
    return (uint8_t)SENSOR_COUNT;
}

// ========================================
// ADC STREAMING STATE
// ========================================
static bool adc_streaming_enabled = false;
static bool adc_stream_key_pending = false;
static uint8_t adc_stream_key_idx = 0;
static uint16_t adc_cached_values[SENSOR_COUNT];  // Cached ADC values for streaming

// Advanced sensor calibration (per-key ADC endpoints)
static bool adv_cal_enabled = false;
static uint16_t adv_cal_release[SENSOR_COUNT] = {0};
static uint16_t adv_cal_press[SENSOR_COUNT] = {0};

static inline uint8_t compute_depth_x10(uint8_t key_idx, uint16_t adc_val) {
    if (key_idx >= SENSOR_COUNT) return 0;
    const uint16_t baseline = sensor_baseline[key_idx];
    if (baseline == 0) return 0;

    // If advanced calibration is enabled and valid for this key, map ADC -> 0..40.
    if (adv_cal_enabled) {
        const uint16_t rel = adv_cal_release[key_idx];
        const uint16_t prs = adv_cal_press[key_idx];
        if (rel != 0 && prs != 0 && rel != prs) {
            const bool inverted = prs > rel;
            const int32_t num = inverted ? ((int32_t)adc_val - (int32_t)rel) : ((int32_t)rel - (int32_t)adc_val);
            const int32_t den = inverted ? ((int32_t)prs - (int32_t)rel) : ((int32_t)rel - (int32_t)prs);
            if (den > 0) {
                if (num <= 0) return 0;
                uint32_t depth = ((uint32_t)num * 40u + (uint32_t)(den / 2)) / (uint32_t)den;
                if (depth > 40u) depth = 40u;
                return (uint8_t)depth;
            }
        }
    }

    // Legacy fallback: assume full travel (4mm) corresponds to ~500 ADC units drop.
    if (adc_val >= baseline) return 0;
    const uint16_t drop = (uint16_t)(baseline - adc_val);
    uint32_t depth = ((uint32_t)drop * 40u) / 500u;
    if (depth > 40u) depth = 40u;
    return (uint8_t)depth;
}

// ========================================
// FLASH STORAGE FOR PERSISTENT SETTINGS
// ========================================
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)  // Last sector
#define SETTINGS_MAGIC 0x4D494E41  // "MINA" magic number
#define SETTINGS_VERSION 3

// Global state variables (referenced by flash storage)
// socd_enabled is now managed by socd.h: socd_get_enabled() / socd_set_enabled()
static bool leds_enabled = true;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t keymap[MAX_LAYERS][SENSOR_COUNT];
    uint16_t actuations[SENSOR_COUNT];  // Stored as 0.1mm units
    uint16_t hysteresis[SENSOR_COUNT];  // Stored as 0.1mm units
    bool adv_cal_enabled;
    uint16_t adv_cal_release[SENSOR_COUNT];
    uint16_t adv_cal_press[SENSOR_COUNT];
    uint8_t led_colors[LED_COUNT * 3];  // RGB data
    uint8_t brightness;
    uint8_t led_effect;
    uint8_t effect_speed;
    uint8_t effect_direction;
    uint8_t effect_color1[3];
    uint8_t effect_color2[3];
    // Gradient palette / params (used by Wave/Gradient/Radial/etc)
    uint8_t gradient_num_colors;        // 1..8
    uint8_t gradient_colors[8 * 3];     // RGB stops
    uint8_t gradient_orientation;       // 0..3
    uint16_t gradient_rotation_deg;     // 0..360
    bool socd_enabled;
    bool leds_enabled;
    uint32_t checksum;
} settings_t;

// v2 settings layout (pre-gradient persistence)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t keymap[MAX_LAYERS][SENSOR_COUNT];
    uint16_t actuations[SENSOR_COUNT];  // Stored as 0.1mm units
    uint16_t hysteresis[SENSOR_COUNT];  // Stored as 0.1mm units
    bool adv_cal_enabled;
    uint16_t adv_cal_release[SENSOR_COUNT];
    uint16_t adv_cal_press[SENSOR_COUNT];
    uint8_t led_colors[LED_COUNT * 3];  // RGB data
    uint8_t brightness;
    uint8_t led_effect;
    uint8_t effect_speed;
    uint8_t effect_direction;
    uint8_t effect_color1[3];
    uint8_t effect_color2[3];
    bool socd_enabled;
    bool leds_enabled;
    uint32_t checksum;
} settings_v2_t;

// v1 settings layout (pre-advanced-calibration)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t keymap[MAX_LAYERS][SENSOR_COUNT];
    uint16_t actuations[SENSOR_COUNT];
    uint16_t hysteresis[SENSOR_COUNT];
    uint8_t led_colors[LED_COUNT * 3];
    uint8_t brightness;
    uint8_t led_effect;
    uint8_t effect_speed;
    uint8_t effect_direction;
    uint8_t effect_color1[3];
    uint8_t effect_color2[3];
    bool socd_enabled;
    bool leds_enabled;
    uint32_t checksum;
} settings_v1_t;

static uint32_t calculate_checksum(const settings_t *settings) {
    const uint8_t *data = (const uint8_t *)settings;
    uint32_t sum = 0;
    // Calculate over everything except the checksum field itself
    for (size_t i = 0; i < offsetof(settings_t, checksum); i++) {
        sum += data[i];
    }
    return sum;
}

static uint32_t calculate_checksum_v2(const settings_v2_t *settings) {
    const uint8_t *data = (const uint8_t *)settings;
    uint32_t sum = 0;
    for (size_t i = 0; i < offsetof(settings_v2_t, checksum); i++) {
        sum += data[i];
    }
    return sum;
}

static void save_settings_to_flash(void) {
    settings_t settings = {0};
    settings.magic = SETTINGS_MAGIC;
    settings.version = SETTINGS_VERSION;
    
    // Copy current keymap
    memcpy(settings.keymap, keymap, sizeof(keymap));
    
    // Copy actuation/hysteresis (we'll need to convert from thresholds back to settings)
    // For now, just save the raw threshold values
    for (int i = 0; i < SENSOR_COUNT; i++) {
        // Calculate actuation percentage from threshold
        if (sensor_baseline[i] > 0) {
            uint32_t drop = sensor_baseline[i] - sensor_thresholds[i];
            uint32_t pct = (drop * 100) / sensor_baseline[i];
            settings.actuations[i] = (uint16_t)pct;
        } else {
            settings.actuations[i] = 16;  // Default 1.6mm
        }
        settings.hysteresis[i] = 13;  // Default 1.3mm (not currently tracked separately)
    }

    settings.adv_cal_enabled = adv_cal_enabled;
    memcpy(settings.adv_cal_release, adv_cal_release, sizeof(adv_cal_release));
    memcpy(settings.adv_cal_press, adv_cal_press, sizeof(adv_cal_press));
    
    // Get LED data from lighting module
    lighting_get_led_buffer(settings.led_colors, sizeof(settings.led_colors));
    settings.brightness = lighting_get_brightness();
    settings.led_effect = lighting_get_effect();
    settings.effect_speed = lighting_get_effect_speed();
    settings.effect_direction = lighting_get_effect_direction();
    lighting_get_effect_color1(&settings.effect_color1[0], &settings.effect_color1[1], &settings.effect_color1[2]);
    lighting_get_effect_color2(&settings.effect_color2[0], &settings.effect_color2[1], &settings.effect_color2[2]);

    // Persist gradient palette/params so Wave/Gradient restores correctly on boot.
    lighting_get_gradient(&settings.gradient_num_colors, settings.gradient_colors, sizeof(settings.gradient_colors));
    lighting_get_gradient_params(&settings.gradient_orientation, &settings.gradient_rotation_deg);
    
    // Store current state flags
    settings.socd_enabled = socd_get_enabled();
    settings.leds_enabled = leds_enabled;
    
    settings.checksum = calculate_checksum(&settings);
    
    // Write to flash (must disable interrupts)
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t *)&settings, sizeof(settings));
    restore_interrupts(ints);
    
    printf("Settings saved to flash\n");
}

static bool load_settings_from_flash(void) {
    const settings_t *flash_settings = (const settings_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    
    // Verify magic and checksum
    if (flash_settings->magic != SETTINGS_MAGIC) {
        printf("No valid settings found in flash\n");
        return false;
    }

    if (flash_settings->version == 1) {
        const settings_v1_t *v1 = (const settings_v1_t *)flash_settings;
        uint32_t stored_checksum = v1->checksum;
        // checksum over v1 layout (excluding its checksum field)
        const uint8_t *data = (const uint8_t *)v1;
        uint32_t sum = 0;
        for (size_t i = 0; i < offsetof(settings_v1_t, checksum); i++) sum += data[i];
        if (stored_checksum != sum) {
            printf("Settings checksum mismatch\n");
            return false;
        }

        memcpy(keymap, v1->keymap, sizeof(keymap));

        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (sensor_baseline[i] > 0 && v1->actuations[i] > 0) {
                uint32_t thr = ((uint32_t)sensor_baseline[i] * (100 - (uint32_t)v1->actuations[i])) / 100;
                if (thr > 0xFFFF) thr = 0xFFFF;
                sensor_thresholds[i] = (uint16_t)thr;
            }
        }

        lighting_set_led_buffer(v1->led_colors, sizeof(v1->led_colors));
        lighting_set_max_brightness_percent(v1->brightness);
        lighting_set_effect((led_effect_t)v1->led_effect);
        lighting_set_effect_speed(v1->effect_speed);
        lighting_set_effect_direction(v1->effect_direction);
        lighting_set_effect_color1(v1->effect_color1[0], v1->effect_color1[1], v1->effect_color1[2]);
        lighting_set_effect_color2(v1->effect_color2[0], v1->effect_color2[1], v1->effect_color2[2]);

        socd_set_enabled(v1->socd_enabled);
        leds_enabled = v1->leds_enabled;

        adv_cal_enabled = false;
        memset(adv_cal_release, 0, sizeof(adv_cal_release));
        memset(adv_cal_press, 0, sizeof(adv_cal_press));

        printf("Settings loaded from flash (v1)\n");
        return true;
    }

    if (flash_settings->version == 2) {
        const settings_v2_t *v2 = (const settings_v2_t *)flash_settings;
        uint32_t stored_checksum = v2->checksum;
        uint32_t calculated_checksum = calculate_checksum_v2(v2);
        if (stored_checksum != calculated_checksum) {
            printf("Settings checksum mismatch\n");
            return false;
        }

        memcpy(keymap, v2->keymap, sizeof(keymap));

        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (sensor_baseline[i] > 0 && v2->actuations[i] > 0) {
                uint32_t thr = ((uint32_t)sensor_baseline[i] * (100 - (uint32_t)v2->actuations[i])) / 100;
                if (thr > 0xFFFF) thr = 0xFFFF;
                sensor_thresholds[i] = (uint16_t)thr;
            }
        }

        adv_cal_enabled = v2->adv_cal_enabled;
        memcpy(adv_cal_release, v2->adv_cal_release, sizeof(adv_cal_release));
        memcpy(adv_cal_press, v2->adv_cal_press, sizeof(adv_cal_press));

        lighting_set_led_buffer(v2->led_colors, sizeof(v2->led_colors));
        lighting_set_max_brightness_percent(v2->brightness);
        lighting_set_effect((led_effect_t)v2->led_effect);
        lighting_set_effect_speed(v2->effect_speed);
        lighting_set_effect_direction(v2->effect_direction);
        lighting_set_effect_color1(v2->effect_color1[0], v2->effect_color1[1], v2->effect_color1[2]);
        lighting_set_effect_color2(v2->effect_color2[0], v2->effect_color2[1], v2->effect_color2[2]);

        // v2 did not store gradient palette/params; keep lighting.c defaults.

        socd_set_enabled(v2->socd_enabled);
        leds_enabled = v2->leds_enabled;

        printf("Settings loaded from flash (v2)\n");
        return true;
    }

    if (flash_settings->version != SETTINGS_VERSION) {
        printf("Settings version mismatch\n");
        return false;
    }

    uint32_t stored_checksum = flash_settings->checksum;
    uint32_t calculated_checksum = calculate_checksum(flash_settings);
    if (stored_checksum != calculated_checksum) {
        printf("Settings checksum mismatch\n");
        return false;
    }

    memcpy(keymap, flash_settings->keymap, sizeof(keymap));

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_baseline[i] > 0 && flash_settings->actuations[i] > 0) {
            uint32_t thr = ((uint32_t)sensor_baseline[i] * (100 - (uint32_t)flash_settings->actuations[i])) / 100;
            if (thr > 0xFFFF) thr = 0xFFFF;
            sensor_thresholds[i] = (uint16_t)thr;
        }
    }

    adv_cal_enabled = flash_settings->adv_cal_enabled;
    memcpy(adv_cal_release, flash_settings->adv_cal_release, sizeof(adv_cal_release));
    memcpy(adv_cal_press, flash_settings->adv_cal_press, sizeof(adv_cal_press));

    lighting_set_led_buffer(flash_settings->led_colors, sizeof(flash_settings->led_colors));
    lighting_set_max_brightness_percent(flash_settings->brightness);
    lighting_set_effect((led_effect_t)flash_settings->led_effect);
    lighting_set_effect_speed(flash_settings->effect_speed);
    lighting_set_effect_direction(flash_settings->effect_direction);
    lighting_set_effect_color1(flash_settings->effect_color1[0], flash_settings->effect_color1[1], flash_settings->effect_color1[2]);
    lighting_set_effect_color2(flash_settings->effect_color2[0], flash_settings->effect_color2[1], flash_settings->effect_color2[2]);

    // Restore gradient palette/params (v3+)
    lighting_set_gradient(flash_settings->gradient_num_colors, flash_settings->gradient_colors);
    lighting_set_gradient_params(flash_settings->gradient_orientation, flash_settings->gradient_rotation_deg);

    socd_set_enabled(flash_settings->socd_enabled);
    leds_enabled = flash_settings->leds_enabled;

    printf("Settings loaded from flash\n");
    return true;
}

#define SPI_PORT spi0
#define PIN_SCK  18   // GP18
#define PIN_MOSI 19   // GP19
#define PIN_MISO 16   // GP16
#define PIN_CS   17   // GP17

// Use MUX address pins from user config
#define PIN_MUX_S0 MUX_S0_PIN
#define PIN_MUX_S1 MUX_S1_PIN
#define PIN_MUX_S2 MUX_S2_PIN
#define PIN_MUX_S3 MUX_S3_PIN

#define VREF_VOLTS 3.300f

// MUX_COUNT is defined in the user's config.h
static const uint8_t mux_to_adc[MUX_COUNT] = {
    0,
    1,
#if MUX_COUNT >= 3
    2,
#endif
#if MUX_COUNT >= 4
    3,
#endif
#if MUX_COUNT >= 5
    4,
#endif
#if MUX_COUNT >= 6
    5,
#endif
#if MUX_COUNT >= 7
    6,
#endif
#if MUX_COUNT >= 8
    7,
#endif
};
#define MUX_SETTLE_US 200u
#define SCAN_DELAY_MS 5u

static uint16_t mcp3208_read(uint8_t ch) {
    uint8_t tx[3];
    uint8_t rx[3];

    tx[0] = 0x06 | ((ch & 0x07) >> 2);
    tx[1] = (uint8_t)((ch & 0x03) << 6);
    tx[2] = 0x00;

    gpio_put(PIN_CS, 0);
    spi_write_read_blocking(SPI_PORT, tx, rx, 3);
    gpio_put(PIN_CS, 1);

    uint16_t value = ((rx[1] & 0x0F) << 8) | rx[2]; // 12 bits
    return value;
}

static inline void mux_set(uint8_t sel) {
    gpio_put(PIN_MUX_S0, sel & 0x1);
    gpio_put(PIN_MUX_S1, (sel >> 1) & 0x1);
    gpio_put(PIN_MUX_S2, (sel >> 2) & 0x1);
    gpio_put(PIN_MUX_S3, (sel >> 3) & 0x1);
}

static uint16_t sample_adc_avg_for_adc(uint8_t adc_ch)
{
    uint32_t sum = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; ++i) {
        sum += mcp3208_read(adc_ch);
        sleep_us(500);
    }
    return (uint16_t)(sum / CALIBRATION_SAMPLES);
}

// LED gate helper: drive LED gate pin according to configured polarity.
static inline void led_power_set(bool on) {
#ifdef LED_GATE_PIN
  #if LED_GATE_ACTIVE_LOW
    gpio_put(LED_GATE_PIN, on ? 0 : 1);
  #else
    gpio_put(LED_GATE_PIN, on ? 1 : 0);
  #endif
#else
    (void)on;
#endif
}

void mcp3208_hallscan_calibrate(void)
{
    const mux16_ref_t* mux_maps_local[MUX_COUNT] = {
        mux1_channels,
        mux2_channels,
#if MUX_COUNT >= 3
        mux3_channels,
#endif
#if MUX_COUNT >= 4
        mux4_channels,
#endif
#if MUX_COUNT >= 5
        mux5_channels,
#endif
#if MUX_COUNT >= 6
        mux6_channels,
#endif
#if MUX_COUNT >= 7
        mux7_channels,
#endif
#if MUX_COUNT >= 8
        mux8_channels,
#endif
    };

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        sensor_baseline[i] = 0;
        sensor_thresholds[i] = 0;
    }

    for (uint8_t m = 0; m < MUX_COUNT; ++m) {
        uint8_t adc_ch = mux_to_adc[m];
        for (uint8_t ch = 0; ch < 16; ++ch) {
            const mux16_ref_t *ref = &mux_maps_local[m][ch];
            if (ref->sensor == 0 || ref->sensor > SENSOR_COUNT) continue;

            mux_set(ch);
            sleep_us(MUX_SETTLE_US);

            uint16_t sample = sample_adc_avg_for_adc(adc_ch);
            if (sample < ADC_MIN_VALID) continue;

            uint8_t sidx = (uint8_t)(ref->sensor - 1);
            sensor_baseline[sidx] = sample;

            uint32_t thr = ((uint32_t)sensor_baseline[sidx] * (100 - (uint32_t)SENSOR_THRESHOLD)) / 100;
            if (thr > 0xFFFF) thr = 0xFFFF;
            sensor_thresholds[sidx] = (uint16_t)thr;
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void)report_id;

    // Instance 0 is the keyboard interface. The host sends an OUTPUT report
    // containing LED state (NumLock/CapsLock/etc.). Do NOT forward this to raw.
    if (instance == ITF_NUM_HID_KBD && report_type == HID_REPORT_TYPE_OUTPUT && buffer && bufsize >= 1) {
        bool caps_on = (buffer[0] & 0x02) != 0;
        lighting_set_caps_lock_overlay(true, caps_on);
        return;
    }

    // Route vendor/raw OUT reports to the raw handler.
    if (instance == ITF_NUM_HID_VIA_RAW || instance == ITF_NUM_HID_APP_RAW || instance == ITF_NUM_HID_RESP_RAW) {
        hid_raw_receive(instance, report_id, buffer, bufsize);
        return;
    }
}

// Some TinyUSB builds invoke this callback for OUT/IN reports received via
// interrupt OUT endpoint. Forward to the same raw handler so we reliably
// catch reports regardless of which callback TinyUSB uses.
void tud_hid_report_received_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    if (report == NULL || len == 0) return;

    // Some TinyUSB builds deliver OUT reports here. Apply the same routing as
    // tud_hid_set_report_cb.
    if (instance == ITF_NUM_HID_KBD && len >= 1) {
        // Keyboard LED output (CapsLock/etc.)
        bool caps_on = (report[0] & 0x02) != 0;
        lighting_set_caps_lock_overlay(true, caps_on);
        return;
    }

    if (instance == ITF_NUM_HID_VIA_RAW || instance == ITF_NUM_HID_APP_RAW || instance == ITF_NUM_HID_RESP_RAW) {
        hid_raw_receive(instance, 0 /* report_id */, report, len);
        return;
    }
}

int main() {
    // Initialize stdio - but don't block if no USB
    stdio_init_all();
    
    // Print startup message (only visible if USB connected)
    printf("main_firmware: starting v1.0\n");

    // Initialize USB
    tusb_init();

    // Initialize LED gate (controls 5V LED power rail)
#ifdef LED_GATE_PIN
    gpio_init(LED_GATE_PIN);
    gpio_set_dir(LED_GATE_PIN, GPIO_OUT);
#endif

    // Initialize encoder (quadrature + switch)
    encoder_init();

    // Initialize onboard LED (GP25) - for status indication
    gpio_init(ONBOARD_LED);
    gpio_set_dir(ONBOARD_LED, GPIO_OUT);
    gpio_put(ONBOARD_LED, 0);

    spi_init(SPI_PORT, 1000 * 1000); // 1 MHz
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    gpio_init(PIN_MUX_S0);
    gpio_set_dir(PIN_MUX_S0, GPIO_OUT);
    gpio_put(PIN_MUX_S0, 0);

    gpio_init(PIN_MUX_S1);
    gpio_set_dir(PIN_MUX_S1, GPIO_OUT);
    gpio_put(PIN_MUX_S1, 0);

    gpio_init(PIN_MUX_S2);
    gpio_set_dir(PIN_MUX_S2, GPIO_OUT);
    gpio_put(PIN_MUX_S2, 0);

    gpio_init(PIN_MUX_S3);
    gpio_set_dir(PIN_MUX_S3, GPIO_OUT);
    gpio_put(PIN_MUX_S3, 0);

    // Wait for USB with timeout (don't block forever if no USB data connection)
    uint32_t usb_wait_start = to_ms_since_boot(get_absolute_time());
    while (!tud_mounted() && (to_ms_since_boot(get_absolute_time()) - usb_wait_start) < 2000) {
        tud_task();
        sleep_ms(1);
    }
    
    if (tud_mounted()) {
        printf("USB mounted\n");
    } else {
        printf("USB not connected\n");
    }

    // Initialize LEDs and run calibration
    lighting_init();

    // Set brightness
    lighting_set_max_brightness_percent(USB_BRIGHTNESS_PERCENT);
    
    // Enable LED power rail (always HIGH on startup)
    led_power_set(true);

    // Track LED gate state in firmware (true = enabled)
    // GP23 (LED_GATE_PIN) is controlled via HID commands from software
    // leds_enabled is a global variable; SOCD state managed by socd.h API
    
    // Current active layer (0 = base, 1 = Fn layer, 2-3 = extra layers)
    // Fn key (S_FN) acts as MO(1) - momentary layer 1
    uint8_t current_layer = 0;

    // Initialize SOCD and encoder modules
    socd_init();
    encoder_init();
    
    // Skip startup animation - just initialize LEDs to off
    // (Startup animation was causing issues with lighting state)
    
    printf("Ready\n");
    
    mcp3208_hallscan_calibrate();
    
    // Try to load saved settings from flash
    if (!load_settings_from_flash()) {
        printf("Using default settings\n");
    }
    
    // Sync LED power gate with loaded settings
    led_power_set(leds_enabled);
    
    // Apply the saved effect immediately so LEDs light up on startup
    if (leds_enabled) {
        lighting_update();
    }

    // Initialize modern profile storage (separate flash sector)
    profiles_init();

    // Debounced flash save for settings changes.
    bool pending_settings_save = false;
    uint32_t last_settings_change_ms = 0;
    const uint32_t SETTINGS_SAVE_DEBOUNCE_MS = 350;

    while (true) {
        tud_task();

        // Debounced save: commit settings after a quiet period.
        if (pending_settings_save) {
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if ((now_ms - last_settings_change_ms) >= SETTINGS_SAVE_DEBOUNCE_MS) {
                save_settings_to_flash();
                pending_settings_save = false;
            }
        }

        // ========== ENCODER HANDLING ==========
        // Accumulate quadrature edges and emit on a full detent.
        // Most encoders produce 2 or 4 edges per detent; 2 is the common default.
        static int32_t accumulated_steps = 0;
        const int32_t DETENT_STEPS = 2;
        const int32_t MAX_ACCUMULATED = 8;

        int enc_steps = encoder_poll();

        if (enc_steps != 0) {
            accumulated_steps += enc_steps;

            // Clamp to prevent runaway
            if (accumulated_steps > MAX_ACCUMULATED) accumulated_steps = MAX_ACCUMULATED;
            else if (accumulated_steps < -MAX_ACCUMULATED) accumulated_steps = -MAX_ACCUMULATED;
        }

        // Emit one consumer tap per detent
        while ((accumulated_steps >= DETENT_STEPS || accumulated_steps <= -DETENT_STEPS) && tud_hid_n_ready(0)) {
            uint16_t consumer_code = (accumulated_steps > 0)
                ? HID_USAGE_CONSUMER_VOLUME_INCREMENT
                : HID_USAGE_CONSUMER_VOLUME_DECREMENT;

            tud_hid_n_report(0, 2, &consumer_code, sizeof(consumer_code));
            sleep_ms(5);
            consumer_code = 0;
            tud_hid_n_report(0, 2, &consumer_code, sizeof(consumer_code));

            accumulated_steps += (accumulated_steps > 0) ? -DETENT_STEPS : DETENT_STEPS;
        }
        
        if (encoder_switch_pressed()) {
            printf("Encoder switch pressed\n");
            if (tud_hid_n_ready(0)) {
                // Mute using consumer control
                uint16_t consumer_code = HID_USAGE_CONSUMER_MUTE;
                tud_hid_n_report(0, 2, &consumer_code, sizeof(consumer_code));
                sleep_ms(10);
                consumer_code = 0;
                tud_hid_n_report(0, 2, &consumer_code, sizeof(consumer_code));
            }
        }

        // ========== HID COMMAND HANDLING ==========
        // GP23 (LED_GATE_PIN) toggle is controlled via HID from software
        // Handle LED power toggle from HID
        if (hid_consume_led_power_toggle()) {
            leds_enabled = !leds_enabled;
            led_power_set(leds_enabled);
            printf("HID: LED gate toggled: %s\n", leds_enabled ? "ENABLED" : "DISABLED");
        }
        
        // Handle LED power set from HID
        int led_pwr_set = hid_consume_led_power_set();
        if (led_pwr_set >= 0) {
            leds_enabled = (led_pwr_set != 0);
            led_power_set(leds_enabled);
            printf("HID: LED gate set: %s\n", leds_enabled ? "ENABLED" : "DISABLED");
        }
        
        // Handle SOCD toggle from HID
        if (hid_consume_socd_toggle()) {
            socd_toggle();
            bool socd_now = socd_get_enabled();
            lighting_socd_animation(socd_now);
            printf("HID: SOCD toggled: %s\n", socd_now ? "ENABLED" : "DISABLED");
        }

        // Handle SOCD set from HID
        int socd_set = hid_consume_socd_set();
        if (socd_set >= 0) {
            socd_set_enabled(socd_set != 0);
            bool socd_now = socd_get_enabled();
            lighting_socd_animation(socd_now);
            printf("HID: SOCD set: %s\n", socd_now ? "ENABLED" : "DISABLED");
        }
        
        // Handle brightness set from HID
        int brightness_val = hid_consume_brightness_set();
        if (brightness_val >= 0) {
            lighting_set_max_brightness_percent((uint8_t)brightness_val);
            printf("HID: Brightness set to %d%%\n", brightness_val);
        }
        
        // Handle per-key actuation set from HID
        uint8_t act_key_idx, act_threshold;
        if (hid_consume_actuation_set(&act_key_idx, &act_threshold)) {
            if (act_key_idx < SENSOR_COUNT) {
                // Compute new threshold from baseline
                uint32_t thr = ((uint32_t)sensor_baseline[act_key_idx] * (100 - (uint32_t)act_threshold)) / 100;
                if (thr > 0xFFFF) thr = 0xFFFF;
                sensor_thresholds[act_key_idx] = (uint16_t)thr;
                printf("HID: Key %d actuation set to %d%%\n", act_key_idx, act_threshold);
            }
        }
        
        // Handle layer change from HID
        int new_layer = hid_consume_layer_set();
        if (new_layer >= 0) {
            current_layer = (uint8_t)new_layer;
            lighting_set_active_layer(current_layer);
            printf("HID: Layer set to %d\n", new_layer);
        }
        
        // Handle keymap change from HID (individual key)
        uint8_t km_layer, km_key_idx, km_hid_keycode;
        if (hid_consume_keymap_set(&km_layer, &km_key_idx, &km_hid_keycode)) {
            if (km_layer < MAX_LAYERS && km_key_idx < SENSOR_COUNT) {
                keymap[km_layer][km_key_idx] = km_hid_keycode;
                printf("HID: Keymap updated - Layer %d, Key %d = 0x%02X\n", km_layer, km_key_idx, km_hid_keycode);
                // Debounced save will be triggered by settings_changed flag
            }
        }
        
        // Handle bulk keymap/settings changes - save to flash
        if (hid_consume_settings_changed()) {
            pending_settings_save = true;
            last_settings_change_ms = to_ms_since_boot(get_absolute_time());
        }
        
        // Handle calibration request from HID
        if (hid_consume_calibrate()) {
            printf("HID: Recalibrating sensors...\n");
            mcp3208_hallscan_calibrate();
            printf("HID: Calibration complete\n");
        }
        
        // Handle bootloader request from HID
        if (hid_consume_bootloader()) {
            printf("HID: Rebooting to bootloader...\n");
            hid_release_all_keys();
            sleep_ms(150);
            led_power_set(false);
            sleep_ms(50);
            reset_usb_boot(0, 0);
        }
        
        // Handle save profile request from HID
        if (hid_consume_save_profile()) {
            printf("HID: Saving profile to flash...\n");
            save_settings_to_flash();
            pending_settings_save = false;
        }
        
        // Handle load profile request from HID
        if (hid_consume_load_profile()) {
            printf("HID: Loading profile from flash...\n");
            load_settings_from_flash();
        }

        // Handle ADC streaming enable/disable
        bool adc_stream_req;
        if (hid_consume_adc_stream_enable(&adc_stream_req)) {
            adc_streaming_enabled = adc_stream_req;
            printf("HID: ADC streaming %s\n", adc_streaming_enabled ? "enabled" : "disabled");
        }

        // Advanced calibration enable
        bool adv_en;
        if (hid_consume_set_adv_cal_enabled(&adv_en)) {
            adv_cal_enabled = adv_en;
        }

        // Advanced calibration set key
        uint8_t cal_key = 0;
        uint16_t cal_rel = 0, cal_prs = 0;
        if (hid_consume_set_adv_cal_key(&cal_key, &cal_rel, &cal_prs)) {
            if (cal_key < SENSOR_COUNT) {
                adv_cal_release[cal_key] = cal_rel;
                adv_cal_press[cal_key] = cal_prs;
            }
        }

        // Advanced calibration get key
        uint8_t get_cal_key = 0;
        if (hid_consume_get_adv_cal_key(&get_cal_key)) {
            uint16_t rel = 0, prs = 0;
            if (get_cal_key < SENSOR_COUNT) {
                rel = adv_cal_release[get_cal_key];
                prs = adv_cal_press[get_cal_key];
            }
            hid_send_adv_calibration(get_cal_key, adv_cal_enabled, rel, prs);
        }

        // Handle single key ADC request (for live preview)
        uint8_t adc_key_idx;
        if (hid_consume_get_key_adc(&adc_key_idx)) {
            adc_stream_key_idx = adc_key_idx;
            adc_stream_key_pending = true;
        }

        // Modern profile commands (0x70+)
        uint8_t slot = 0;
        if (hid_consume_profile_save(&slot)) {
            uint8_t r = 0, g = 0, b = 0;
            profiles_get_slot_color(slot, &r, &g, &b);
            profiles_save_slot(slot, r, g, b, profiles_static_indicator_enabled());
        }
        if (hid_consume_profile_load(&slot)) {
            profiles_load_slot(slot);
        }
        if (hid_consume_profile_delete(&slot)) {
            profiles_delete_slot(slot);
        }
        if (hid_consume_profile_blank(&slot)) {
            profiles_create_blank_slot(slot);
        }

        profiles_task();
        
        // GP20 state handling removed - LEDs always stay on unless controlled by software

        // Check for LED updates queued by HID handler and apply from
        // main loop context (safe to call PIO functions here).
        uint8_t ledbuf[LED_COUNT * 3];
        if (hid_consume_led_update(ledbuf, sizeof(ledbuf))) {
            // Apply LED buffer for paint mode / per-key color updates
            lighting_set_led_buffer(ledbuf, sizeof(ledbuf));
        }

        static bool prev_pressed[SENSOR_COUNT + 1] = {0};
        bool cur_pressed[SENSOR_COUNT + 1];
        for (int i = 0; i <= SENSOR_COUNT; i++) cur_pressed[i] = false;
        char outbuf[2048];
        size_t off = 0;
        size_t left = sizeof(outbuf);

        uint16_t mux_vals[MUX_COUNT][16];
        for (uint8_t i = 0; i < MUX_COUNT; i++) for (int s = 0; s < 16; s++) mux_vals[i][s] = 0;

        for (uint8_t sel = 0; sel < 16; sel++) {
            mux_set(sel);
            sleep_us(MUX_SETTLE_US);
            // Read all 5 MUX channels
            for (uint8_t m = 0; m < MUX_COUNT; m++) {
                mux_vals[m][sel] = mcp3208_read(mux_to_adc[m]);
            }
        }

        const mux16_ref_t *mux_maps[MUX_COUNT] = {
            mux1_channels,
            mux2_channels,
#if MUX_COUNT >= 3
            mux3_channels,
#endif
#if MUX_COUNT >= 4
            mux4_channels,
#endif
#if MUX_COUNT >= 5
            mux5_channels,
#endif
#if MUX_COUNT >= 6
            mux6_channels,
#endif
#if MUX_COUNT >= 7
            mux7_channels,
#endif
#if MUX_COUNT >= 8
            mux8_channels,
#endif
        };
        for (uint8_t m = 0; m < MUX_COUNT; m++) {
            for (uint8_t s = 0; s < 16; s++) {
                sensor_id_t sid = mux_maps[m][s].sensor;
                if (sid != 0) {
                    uint8_t sidx = (uint8_t)(sid - 1);
                    uint16_t thr = sensor_thresholds[sidx];
                    uint16_t val = mux_vals[m][s];
                    
                    // Cache ADC value for streaming
                    adc_cached_values[sidx] = val;
                    
                    if (thr == 0) continue;
                    // Hysteresis: compute release threshold as thr + baseline * HYST_PERCENT/100
                    uint32_t delta = ((uint32_t)sensor_baseline[sidx] * (uint32_t)HALLSCAN_HYSTERESIS_PERCENT) / 100;
                    uint32_t release_thr = (uint32_t)thr + delta;
                    if (prev_pressed[sid]) {
                        // stay pressed until value rises above release_thr
                        if (val <= release_thr) cur_pressed[sid] = true;
                        else cur_pressed[sid] = false;
                    } else {
                        // not pressed: press when below thr
                        if (val < thr) cur_pressed[sid] = true;
                    }
                }
            }
        }
        
        // ADC streaming (Shego-style): stream small batches and cycle through keys.
        // This keeps USB traffic bounded and ensures every key eventually updates.
        {
            static uint32_t last_adc_stream_ms = 0;
            static uint8_t adc_stream_next_idx = 0;
            const uint32_t now_adc = to_ms_since_boot(get_absolute_time());

            // Always service explicit single-key requests immediately.
            if (adc_stream_key_pending) {
                uint8_t values[4];
                const uint8_t idx = adc_stream_key_idx;
                if (idx < SENSOR_COUNT) {
                    const uint16_t adc = adc_cached_values[idx];
                    const uint8_t depth = compute_depth_x10(idx, adc);
                    values[0] = idx;
                    values[1] = (uint8_t)(adc & 0xFF);
                    values[2] = (uint8_t)((adc >> 8) & 0xFF);
                    values[3] = depth;
                    hid_send_adc_values(values, 1);
                }
                adc_stream_key_pending = false;
            }

            // Background streaming for auto-detect / calibration capture.
            // Match Shego's effective rate (~60Hz). Each packet carries up to 15 keys.
            if (adc_streaming_enabled && (now_adc - last_adc_stream_ms >= 16)) {
                last_adc_stream_ms = now_adc;

                uint8_t values[15 * 4];
                uint8_t count = 0;

                while (count < 15) {
                    const uint8_t idx = adc_stream_next_idx;
                    adc_stream_next_idx++;
                    if (adc_stream_next_idx >= SENSOR_COUNT) adc_stream_next_idx = 0;

                    if (idx >= SENSOR_COUNT) continue;

                    const uint16_t adc = adc_cached_values[idx];
                    const uint8_t depth = compute_depth_x10(idx, adc);

                    values[count * 4 + 0] = idx;
                    values[count * 4 + 1] = (uint8_t)(adc & 0xFF);
                    values[count * 4 + 2] = (uint8_t)((adc >> 8) & 0xFF);
                    values[count * 4 + 3] = depth;
                    count++;

                    // Stop early if we've wrapped (prevents infinite loop if SENSOR_COUNT < 15)
                    if (SENSOR_COUNT < 15 && adc_stream_next_idx == 0) break;
                }

                if (count > 0) {
                    hid_send_adc_values(values, count);
                }
            }
        }

        bool changed = false;
        for (int i = 1; i <= SENSOR_COUNT; i++) {
            if (cur_pressed[i] != prev_pressed[i]) { changed = true; break; }
        }

        // ========== LAYER HANDLING (MO/TG, like Saturn60/VLT) ==========
        // Process MO (momentary) and TG (toggle) layer keys based on keycodes,
        // not hardcoded sensor IDs.  Supports nested MO: Fn(MO1) + key(MO2) = Layer 2.
        uint8_t prev_layer = current_layer;
        for (int i = 1; i <= SENSOR_COUNT; i++) {
            bool pressed = cur_pressed[i];
            bool was_pressed = prev_pressed[i];
            if (pressed == was_pressed) continue;

            uint8_t kc = get_keycode(current_layer, i - 1);

            // MO key PRESSED — activate target layer
            if (pressed && is_mo_keycode(kc)) {
                uint8_t target = kc - 0xA8 + 1;
                mo_held_sensors[target] = (int8_t)(i - 1);
                current_layer = target;
                continue;
            }

            // Key RELEASED — check if this sensor was holding ANY MO layer
            // (keycode may differ now because layer changed since press)
            if (!pressed) {
                bool was_mo = false;
                for (int layer = 1; layer < MAX_LAYERS; layer++) {
                    if (mo_held_sensors[layer] == (int8_t)(i - 1)) {
                        mo_held_sensors[layer] = -1;
                        was_mo = true;
                        break;
                    }
                }
                if (was_mo) {
                    // Find highest layer still held
                    uint8_t highest = 0;
                    for (int layer = MAX_LAYERS - 1; layer >= 1; layer--) {
                        if (mo_held_sensors[layer] != -1) {
                            highest = layer;
                            break;
                        }
                    }
                    current_layer = highest;
                    continue;
                }
            }

            // TG key PRESSED — toggle layer
            if (pressed && is_tg_keycode(kc)) {
                uint8_t target = kc - 0xAB + 1;
                current_layer = (current_layer == target) ? 0 : target;
                continue;
            }
        }
        // Notify lighting when layer changes (ambient strip layer indicator)
        if (current_layer != prev_layer) {
            lighting_set_active_layer(current_layer);
        }

        // SOCD key state tracking is handled inside socd_process_keys()

        // Update status flags for HID GET_STATUS response
        {
            uint8_t status_flags = 0;
            if (leds_enabled) status_flags |= 0x01;
            if (socd_get_enabled()) status_flags |= 0x02;
            hid_set_status_flags(status_flags, current_layer);
        }

        // Update key states for HID preview feature
        // Convert cur_pressed array (1-indexed) to 0-indexed bool array
        bool key_states_0idx[SENSOR_COUNT];
        for (int i = 0; i < SENSOR_COUNT; i++) {
            key_states_0idx[i] = cur_pressed[i + 1];
        }
        hid_set_key_states(key_states_0idx, SENSOR_COUNT);

        if (changed) {
            // Build the key report using keycode-based detection (not hardcoded sensor IDs)
            // This respects layer remapping — a key remapped from modifier to regular works correctly
            uint8_t keys[6] = {0};
            uint8_t modifiers = 0;
            int ki = 0;

            // Apply SOCD resolution — modifies key_states_0idx in place
            // The pair-based system resolves any configured opposing-key pairs
            if (socd_get_enabled()) {
                socd_process_keys(key_states_0idx, SENSOR_COUNT);
            }

            // Track active consumer key to send only on state change
            static uint16_t active_consumer_usage = 0;
            uint16_t new_consumer_usage = 0;

            for (int i = 0; i < SENSOR_COUNT; i++) {
                if (!key_states_0idx[i]) continue;

                uint8_t hidk = get_keycode(current_layer, i);
                if (hidk == 0) continue;

                // Skip MO/TG layer keys (handled in layer section above)
                if (is_mo_keycode(hidk) || is_tg_keycode(hidk)) continue;

                // Modifier keys → set modifier bits
                if (is_modifier_keycode(hidk)) {
                    modifiers |= get_modifier_bit(hidk);
                    continue;
                }

                // Consumer control keys
                uint16_t consumer_usage = 0;
                if (keycode_to_consumer_usage(hidk, &consumer_usage)) {
                    if (new_consumer_usage == 0) new_consumer_usage = consumer_usage;
                    continue;
                }

                // Custom keycodes — fire on press edge only
                if (!prev_pressed[i + 1]) {
                    if (hidk == KC_BOOTLOADER) {
                        printf("Keycode: entering bootloader...\n");
                        hid_release_all_keys();
                        sleep_ms(150);
                        led_power_set(false);
                        sleep_ms(50);
                        reset_usb_boot(0, 0);
                    }
                    if (hidk == KC_REBOOT) {
                        printf("Keycode: rebooting...\n");
                        hid_release_all_keys();
                        sleep_ms(150);
                        led_power_set(false);
                        sleep_ms(50);
                        watchdog_reboot(0, 0, 100);
                    }
                    if (hidk == KC_CALIBRATE) {
                        printf("Keycode: recalibrating...\n");
                        mcp3208_hallscan_calibrate();
                    }
                    if (hidk == KC_LED_TOG) {
                        leds_enabled = !leds_enabled;
                        led_power_set(leds_enabled);
                        printf("Keycode: LED toggle -> %s\n", leds_enabled ? "ON" : "OFF");
                    }
                    if (hidk == KC_SOCD_TOG) {
                        socd_toggle();
                        lighting_socd_animation(socd_get_enabled());
                        printf("Keycode: SOCD toggle -> %s\n", socd_get_enabled() ? "ON" : "OFF");
                    }
                }

                // Skip non-HID keycodes from the report
                if (hidk == KC_BOOTLOADER || hidk == KC_REBOOT ||
                    hidk == KC_CALIBRATE || hidk == KC_LED_TOG || hidk == KC_SOCD_TOG) continue;

                // Regular HID key
                if (ki < 6) keys[ki++] = hidk;
            }

            // Only send consumer report on state change
            if (new_consumer_usage != active_consumer_usage) {
                active_consumer_usage = new_consumer_usage;
                if (tud_hid_n_ready(0)) {
                    tud_hid_n_report(0, 2, &active_consumer_usage, sizeof(active_consumer_usage));
                }
            }

            // Send keyboard report over USB
            if (tud_hid_n_ready(0)) {
                uint8_t kbd_report[8];
                kbd_report[0] = modifiers;
                kbd_report[1] = 0;
                memcpy(&kbd_report[2], keys, 6);
                tud_hid_n_report(0, 1, kbd_report, sizeof(kbd_report));
            }
        }

        for (int i = 1; i <= SENSOR_COUNT; i++) prev_pressed[i] = cur_pressed[i];

        for (uint8_t mux_idx = 0; mux_idx < MUX_COUNT; mux_idx++) {
            int n = snprintf(outbuf + off, left, "MUX %u =", (unsigned)(mux_idx + 1));
            if (n < 0) n = 0;
            if ((size_t)n >= left) { off = sizeof(outbuf) - 1; left = 0; break; }
            off += (size_t)n; left -= (size_t)n;

            for (uint8_t sel = 0; sel < 16; sel++) {
                uint16_t raw = mux_vals[mux_idx][sel];
                if (left > 0) {
                    n = snprintf(outbuf + off, left, " %s %u: %04u", (sel==0?"|":"|"), (unsigned)sel, (unsigned)raw);
                    if (n < 0) n = 0;
                    if ((size_t)n >= left) { off = sizeof(outbuf) - 1; left = 0; break; }
                    off += (size_t)n; left -= (size_t)n;
                }
            }

            if (left > 0) {
                int e = snprintf(outbuf + off, left, "\n");
                if (e < 0) e = 0;
                if ((size_t)e >= left) { off = sizeof(outbuf) - 1; left = 0; break; }
                off += (size_t)e; left -= (size_t)e;
            }
        }

        if (left > 0) {
            int s = snprintf(outbuf + off, left, "-----------\n");
            if (s > 0 && (size_t)s < left) { off += (size_t)s; left -= (size_t)s; }
        }
        if (off > 0) {
            outbuf[off < sizeof(outbuf) ? off : (sizeof(outbuf) - 1)] = '\0';
#if ADC_PRINT_ENABLED
            printf("%s", outbuf);
#endif
        }
        
        // Update animated LED effects (only if LEDs are enabled)
        if (leds_enabled) {
            lighting_update();
        }
        
        // small pause between full scans
        sleep_ms(SCAN_DELAY_MS);

        // keep loop cooperative for USB
        sleep_ms(0);
    }
}
