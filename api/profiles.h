#ifndef PROFILES_H
#define PROFILES_H

#include <stdbool.h>
#include <stdint.h>

// Simple keymap profile storage for taki_mina to match the app's 0x70+ protocol.
// Slots: 0..9 (0 is base/default and always valid)

void profiles_init(void);
void profiles_task(void);

bool profiles_save_slot(uint8_t slot, uint8_t r, uint8_t g, uint8_t b, bool static_indicator);
bool profiles_load_slot(uint8_t slot);
bool profiles_delete_slot(uint8_t slot);
bool profiles_create_blank_slot(uint8_t slot);

uint8_t profiles_get_current_slot(void);

uint16_t profiles_get_valid_mask(void);
bool profiles_slot_valid(uint8_t slot);

void profiles_get_slot_color(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b);
bool profiles_set_slot_color(uint8_t slot, uint8_t r, uint8_t g, uint8_t b);

bool profiles_static_indicator_enabled(void);
bool profiles_set_static_indicator(bool enabled);

#endif // PROFILES_H
