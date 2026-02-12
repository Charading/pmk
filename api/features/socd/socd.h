// SOCD (Simultaneous Opposing Cardinal Directions) handler
// Configurable key pairs with multiple resolution modes
// Matches firmware/keyboard/features/socd.h API

#ifndef SOCD_H
#define SOCD_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of SOCD pairs that can be configured
#define SOCD_MAX_PAIRS 8

// SOCD resolution modes
typedef enum {
    SOCD_MODE_LAST_WINS = 0,  // Last key pressed wins (recommended for gaming)
    SOCD_MODE_NEUTRAL = 1,    // Both keys cancel out (neither active)
    SOCD_MODE_FIRST_WINS = 2, // First key pressed wins
} socd_mode_t;

// SOCD pair configuration
typedef struct {
    uint8_t key1_idx;    // First key index (0-based)
    uint8_t key2_idx;    // Second key index (opposite key)
    uint8_t mode;        // Resolution mode (socd_mode_t)
    bool valid;          // Whether this pair is configured
} socd_pair_t;

// Initialize SOCD module
void socd_init(void);

// Update key state - call when any key changes
void socd_update_key(uint8_t key_idx, bool pressed);

// Process all keys through SOCD - modifies key_states array in place
void socd_process_keys(bool key_states[], int count);

// Pair management
bool socd_add_pair(uint8_t pair_idx, uint8_t key1_idx, uint8_t key2_idx, uint8_t mode);
bool socd_delete_pair(uint8_t pair_idx);
bool socd_get_pair(uint8_t pair_idx, socd_pair_t *pair);
uint8_t socd_get_pair_count(void);

// Global enable/disable
void socd_set_enabled(bool enabled);
bool socd_get_enabled(void);
void socd_toggle(void);

// Global mode (default for new pairs)
void socd_set_global_mode(uint8_t mode);
uint8_t socd_get_global_mode(void);

// Check and clear state-change flag (for notifying software)
bool socd_consume_state_changed(void);

// Helper to add common presets
void socd_add_wasd_preset(void);
void socd_add_arrows_preset(void);
void socd_clear_all_pairs(void);

// Persistence support - get/set all pairs for flash storage
void socd_get_all_pairs(socd_pair_t pairs[SOCD_MAX_PAIRS]);
void socd_set_all_pairs(const socd_pair_t pairs[SOCD_MAX_PAIRS]);

#endif // SOCD_H
