// SOCD implementation - Configurable key pairs with multiple resolution modes
// Matches firmware/keyboard/features/socd.c architecture

#include "socd.h"
#include "hallscan_config.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>

static bool socd_enabled = true;
static bool socd_state_changed = false;
static uint8_t socd_global_mode = SOCD_MODE_LAST_WINS;

// SOCD pair storage
static socd_pair_t socd_pairs[SOCD_MAX_PAIRS];

// Per-key timestamp tracking for last-input-wins
static uint32_t key_timestamps[SENSOR_COUNT];
static bool key_raw_states[SENSOR_COUNT];

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void socd_init(void) {
    socd_enabled = true;
    socd_global_mode = SOCD_MODE_LAST_WINS;
    memset(socd_pairs, 0, sizeof(socd_pairs));
    memset(key_timestamps, 0, sizeof(key_timestamps));
    memset(key_raw_states, 0, sizeof(key_raw_states));

    // Add default WASD preset
    socd_add_wasd_preset();
}

void socd_update_key(uint8_t key_idx, bool pressed) {
    if (key_idx >= SENSOR_COUNT) return;

    // Track state change timestamp for last-input-wins
    if (pressed && !key_raw_states[key_idx]) {
        key_timestamps[key_idx] = now_ms();
    }
    key_raw_states[key_idx] = pressed;
}

void socd_process_keys(bool key_states[], int count) {
    if (!socd_enabled) return;

    // Update internal state tracking
    for (int i = 0; i < count && i < SENSOR_COUNT; i++) {
        socd_update_key(i, key_states[i]);
    }

    // Process each SOCD pair
    for (int p = 0; p < SOCD_MAX_PAIRS; p++) {
        socd_pair_t *pair = &socd_pairs[p];
        if (!pair->valid) continue;

        uint8_t k1 = pair->key1_idx;
        uint8_t k2 = pair->key2_idx;

        if (k1 >= count || k2 >= count) continue;

        // Only need to resolve if both keys are pressed
        if (!key_states[k1] || !key_states[k2]) continue;

        uint8_t mode = pair->mode;

        switch (mode) {
            case SOCD_MODE_LAST_WINS:
                // The key pressed most recently wins
                if (key_timestamps[k1] >= key_timestamps[k2]) {
                    key_states[k2] = false;  // k1 wins
                } else {
                    key_states[k1] = false;  // k2 wins
                }
                break;

            case SOCD_MODE_NEUTRAL:
                // Both cancel out
                key_states[k1] = false;
                key_states[k2] = false;
                break;

            case SOCD_MODE_FIRST_WINS:
                // The key pressed first wins
                if (key_timestamps[k1] <= key_timestamps[k2]) {
                    key_states[k2] = false;  // k1 wins (pressed first)
                } else {
                    key_states[k1] = false;  // k2 wins (pressed first)
                }
                break;
        }
    }
}

bool socd_add_pair(uint8_t pair_idx, uint8_t key1_idx, uint8_t key2_idx, uint8_t mode) {
    if (pair_idx >= SOCD_MAX_PAIRS) return false;
    if (key1_idx >= SENSOR_COUNT || key2_idx >= SENSOR_COUNT) return false;
    if (key1_idx == key2_idx) return false;

    socd_pairs[pair_idx].key1_idx = key1_idx;
    socd_pairs[pair_idx].key2_idx = key2_idx;
    socd_pairs[pair_idx].mode = mode;
    socd_pairs[pair_idx].valid = true;

    printf("[SOCD] Added pair %d: key %d <-> key %d (mode %d)\n",
           pair_idx, key1_idx, key2_idx, mode);
    return true;
}

bool socd_delete_pair(uint8_t pair_idx) {
    if (pair_idx >= SOCD_MAX_PAIRS) return false;

    socd_pairs[pair_idx].valid = false;
    printf("[SOCD] Deleted pair %d\n", pair_idx);
    return true;
}

bool socd_get_pair(uint8_t pair_idx, socd_pair_t *pair) {
    if (pair_idx >= SOCD_MAX_PAIRS || !pair) return false;

    *pair = socd_pairs[pair_idx];
    return socd_pairs[pair_idx].valid;
}

uint8_t socd_get_pair_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < SOCD_MAX_PAIRS; i++) {
        if (socd_pairs[i].valid) count++;
    }
    return count;
}

void socd_set_enabled(bool enabled) {
    socd_enabled = enabled;
    socd_state_changed = true;
    printf("[SOCD] %s\n", enabled ? "enabled" : "disabled");
}

bool socd_get_enabled(void) {
    return socd_enabled;
}

void socd_toggle(void) {
    socd_set_enabled(!socd_enabled);
}

void socd_set_global_mode(uint8_t mode) {
    if (mode > SOCD_MODE_FIRST_WINS) mode = SOCD_MODE_LAST_WINS;
    socd_global_mode = mode;

    // Update all existing pairs to new mode
    for (int i = 0; i < SOCD_MAX_PAIRS; i++) {
        if (socd_pairs[i].valid) {
            socd_pairs[i].mode = mode;
        }
    }
    printf("[SOCD] Global mode set to %d\n", mode);
}

uint8_t socd_get_global_mode(void) {
    return socd_global_mode;
}

bool socd_consume_state_changed(void) {
    bool val = socd_state_changed;
    socd_state_changed = false;
    return val;
}

void socd_add_wasd_preset(void) {
    // Mina65 sensor enum is 1-based, SOCD uses 0-based indices
    // S_A = 31, S_D = 33, S_W = 17, S_S = 32 (1-based)
    // 0-based: A=30, D=32, W=16, S=31
    socd_add_pair(0, S_A - 1, S_D - 1, socd_global_mode);  // A <-> D
    socd_add_pair(1, S_W - 1, S_S - 1, socd_global_mode);  // W <-> S
    printf("[SOCD] WASD preset added\n");
}

void socd_add_arrows_preset(void) {
    // S_LEFT = 64, S_RGHT = 66, S_UP = 56, S_DOWN = 65 (1-based)
    // 0-based: LEFT=63, RIGHT=65, UP=55, DOWN=64
    socd_add_pair(2, S_LEFT - 1, S_RGHT - 1, socd_global_mode);  // Left <-> Right
    socd_add_pair(3, S_UP - 1, S_DOWN - 1, socd_global_mode);    // Up <-> Down
    printf("[SOCD] Arrow keys preset added\n");
}

void socd_clear_all_pairs(void) {
    for (int i = 0; i < SOCD_MAX_PAIRS; i++) {
        socd_pairs[i].valid = false;
    }
    printf("[SOCD] All pairs cleared\n");
}

void socd_get_all_pairs(socd_pair_t pairs[SOCD_MAX_PAIRS]) {
    memcpy(pairs, socd_pairs, sizeof(socd_pairs));
}

void socd_set_all_pairs(const socd_pair_t pairs[SOCD_MAX_PAIRS]) {
    memcpy(socd_pairs, pairs, sizeof(socd_pairs));
    printf("[SOCD] All pairs restored from flash\n");
}
