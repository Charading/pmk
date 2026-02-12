#ifndef LIGHTING_H
#define LIGHTING_H

#include <stdint.h>
#include <stdbool.h>

// ========================================
// LED EFFECT TYPES
// ========================================
typedef enum {
    LED_EFFECT_STATIC = 0,
    LED_EFFECT_BREATHING,
    LED_EFFECT_WAVE,
    LED_EFFECT_WAVE_REVERSE,
    LED_EFFECT_RADIAL,
    LED_EFFECT_GRADIENT,
    LED_EFFECT_RAINBOW,
    LED_EFFECT_REACTIVE,
    LED_EFFECT_COUNT
} led_effect_t;

// ========================================
// BASIC FUNCTIONS
// ========================================
void lighting_init(void);
void lighting_set_pixel_rgb(int idx, uint8_t r, uint8_t g, uint8_t b);
void lighting_set_all_rgb(uint8_t r, uint8_t g, uint8_t b);

// Set maximum brightness cap 0..100 percent. Use 100 for no cap.
void lighting_set_max_brightness_percent(uint8_t percent);

// ========================================
// EFFECT CONTROL
// ========================================
// Set active effect (animated effects run in lighting_update)
void lighting_set_effect(led_effect_t effect);

// Set effect speed (0-255, default 128)
void lighting_set_effect_speed(uint8_t speed);

// Set effect direction (0 = forward, 1 = reverse)
void lighting_set_effect_direction(uint8_t direction);

// Set primary/secondary colors for effects
void lighting_set_effect_color1(uint8_t r, uint8_t g, uint8_t b);
void lighting_set_effect_color2(uint8_t r, uint8_t g, uint8_t b);

// Set gradient colors (up to 8 stops)
void lighting_set_gradient(uint8_t num_colors, const uint8_t *colors);

// Set gradient orientation and rotation
// orientation: 0=horizontal, 1=vertical, 2=diag TL-BR, 3=diag TR-BL
// rotation_deg: 0-360 degrees (overrides orientation if non-zero)
void lighting_set_gradient_params(uint8_t orientation, uint16_t rotation_deg);

// Call periodically from main loop to update animated effects
void lighting_update(void);

// Store LED buffer for static mode (host sends full LED array)
void lighting_set_led_buffer(const uint8_t *buffer, uint16_t len);

// Paint overlay - highest priority layer, overrides any effect
void lighting_set_paint_led(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);
void lighting_clear_paint_overlay(void);

// Notify a key press for reactive effects
void lighting_notify_keypress(uint8_t key_idx);

// ========================================
// GETTERS FOR FLASH STORAGE
// ========================================
void lighting_get_led_buffer(uint8_t *buffer, uint16_t len);
uint8_t lighting_get_brightness(void);
uint8_t lighting_get_effect(void);
uint8_t lighting_get_effect_speed(void);
uint8_t lighting_get_effect_direction(void);
void lighting_get_effect_color1(uint8_t *r, uint8_t *g, uint8_t *b);
void lighting_get_effect_color2(uint8_t *r, uint8_t *g, uint8_t *b);

// Gradient palette/params (for persisting animated/palette effects)
// colors_out must have space for up to 8*3 bytes.
void lighting_get_gradient(uint8_t *num_colors_out, uint8_t *colors_out, uint16_t colors_out_len);
void lighting_get_gradient_params(uint8_t *orientation_out, uint16_t *rotation_deg_out);

// ========================================
// SIGNALRGB / ZONE STREAMING
// ========================================
// Zone mask bits: bit0=main, bit1=ambient, bit2=status (ignored on 2-zone boards)
void lighting_set_streaming_zones(uint8_t zone_mask);
uint8_t lighting_get_streaming_zones(void);

// Provide a full-frame LED buffer from host (e.g. SignalRGB). Lighting module
// applies it only to zones enabled by the streaming zone mask.
void lighting_set_signalrgb_buffer(const uint8_t *buffer, uint16_t len);

// ========================================
// CAPS LOCK INDICATOR OVERLAY
// ========================================
// Enable/disable caps lock indicator overlay
// When enabled and active, highlights caps lock key (Taki) or ambient strip (Mina)
void lighting_set_caps_lock_overlay(bool enabled, bool active);

// Set caps lock indicator color
void lighting_set_caps_lock_color(uint8_t r, uint8_t g, uint8_t b);

// ========================================
// LAYER INDICATOR OVERLAY
// ========================================
// Set/get layer indicator color (layer 0 ignored — base layer never has indicator)
void lighting_set_layer_color(uint8_t layer, uint8_t r, uint8_t g, uint8_t b);
void lighting_get_layer_color(uint8_t layer, uint8_t *r, uint8_t *g, uint8_t *b);

// Called when active layer changes — enables ambient overlay for non-base layers with color
void lighting_set_active_layer(uint8_t layer);
uint8_t lighting_get_active_layer(void);

// Trigger SOCD toggle animation on ambient strip (green wave = enable, red wave = disable)
void lighting_socd_animation(bool enabled);

#endif // LIGHTING_H
