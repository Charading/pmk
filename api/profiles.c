#include "profiles.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "hallscan_config.h"

#ifndef MAX_LAYERS
#define MAX_LAYERS 4
#endif

#define PROFILE_COUNT 10

// Store profiles in second-to-last flash sector (last sector is used by main settings).
#define PROFILES_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (2 * FLASH_SECTOR_SIZE))
#define PROFILES_MAGIC 0x50524F46u // "PROF"
#define PROFILES_VERSION 2u
#define PROFILES_FLASH_PTR ((const uint8_t *)(XIP_BASE + PROFILES_FLASH_OFFSET))

// Provided by main.c
extern uint8_t (*get_keymap_ptr(void))[SENSOR_COUNT];

typedef struct {
    uint32_t magic;
    uint32_t version;

    uint8_t current_slot;
    uint16_t valid_mask; // bit i => slot i valid (slot0 forced valid)

    // UI indicator color per slot
    uint8_t colors[PROFILE_COUNT][3];

    // Global static indicator flag (not per-slot in current app)
    uint8_t static_indicator_enabled;

    // Stored keymap overrides (0 means "use default")
    uint8_t keymaps[PROFILE_COUNT][MAX_LAYERS][SENSOR_COUNT];

    // Padding / future expansion
    uint8_t _pad[8];

    uint32_t checksum;
} profiles_flash_t;

#define PROFILES_PROGRAM_SIZE ((sizeof(profiles_flash_t) + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1))

static uint8_t g_current_slot = 0;
static uint16_t g_valid_mask = 0x0001; // slot0 always valid
static uint8_t g_colors[PROFILE_COUNT][3] = {0};
static bool g_static_indicator = false;
static uint8_t g_keymaps[PROFILE_COUNT][MAX_LAYERS][SENSOR_COUNT];
static bool g_dirty = false;

static uint32_t profiles_checksum(const profiles_flash_t *p)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t sum = 0;
    for (size_t i = 0; i < offsetof(profiles_flash_t, checksum); i++) {
        sum += b[i];
    }
    return sum;
}

static void profiles_flush_to_flash(void)
{
    profiles_flash_t out;
    memset(&out, 0, sizeof(out));
    out.magic = PROFILES_MAGIC;
    out.version = PROFILES_VERSION;
    out.current_slot = g_current_slot;
    out.valid_mask = (uint16_t)(g_valid_mask | 0x0001);
    memcpy(out.colors, g_colors, sizeof(g_colors));
    out.static_indicator_enabled = g_static_indicator ? 1 : 0;
    memcpy(out.keymaps, g_keymaps, sizeof(g_keymaps));
    out.checksum = profiles_checksum(&out);

    // flash_range_program requires FLASH_PAGE_SIZE alignment for length.
    uint8_t program_buf[PROFILES_PROGRAM_SIZE];
    memset(program_buf, 0xFF, sizeof(program_buf));
    memcpy(program_buf, &out, sizeof(out));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(PROFILES_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(PROFILES_FLASH_OFFSET, program_buf, sizeof(program_buf));
    restore_interrupts(ints);

    g_dirty = false;
    printf("[PROFILES] Saved to flash\n");
}

static bool profiles_load_from_flash(void)
{
    const profiles_flash_t *in = (const profiles_flash_t *)PROFILES_FLASH_PTR;
    if (in->magic != PROFILES_MAGIC) return false;
    if (in->version != PROFILES_VERSION) return false;
    const uint32_t got = in->checksum;
    const uint32_t exp = profiles_checksum(in);
    if (got != exp) return false;

    g_current_slot = in->current_slot;
    g_valid_mask = (uint16_t)(in->valid_mask | 0x0001);
    memcpy(g_colors, in->colors, sizeof(g_colors));
    g_static_indicator = in->static_indicator_enabled ? true : false;
    memcpy(g_keymaps, in->keymaps, sizeof(g_keymaps));
    g_dirty = false;
    return true;
}

static bool slot_valid(uint8_t slot)
{
    if (slot >= PROFILE_COUNT) return false;
    if (slot == 0) return true;
    return (g_valid_mask & (1u << slot)) != 0;
}

uint16_t profiles_get_valid_mask(void)
{
    return (uint16_t)(g_valid_mask | 0x0001);
}

bool profiles_slot_valid(uint8_t slot)
{
    return slot_valid(slot);
}

void profiles_get_slot_color(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (slot >= PROFILE_COUNT) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }
    if (r) *r = g_colors[slot][0];
    if (g) *g = g_colors[slot][1];
    if (b) *b = g_colors[slot][2];
}

bool profiles_set_slot_color(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    if (slot >= PROFILE_COUNT) return false;
    g_colors[slot][0] = r;
    g_colors[slot][1] = g;
    g_colors[slot][2] = b;
    g_dirty = true;
    return true;
}

bool profiles_static_indicator_enabled(void)
{
    return g_static_indicator;
}

bool profiles_set_static_indicator(bool enabled)
{
    g_static_indicator = enabled;
    g_dirty = true;
    return true;
}

uint8_t profiles_get_current_slot(void)
{
    return g_current_slot;
}

void profiles_init(void)
{
    memset(g_keymaps, 0, sizeof(g_keymaps));
    g_current_slot = 0;
    g_valid_mask = 0x0001;
    memset(g_colors, 0, sizeof(g_colors));
    g_static_indicator = false;

    if (profiles_load_from_flash()) {
        printf("[PROFILES] Loaded from flash (slot=%u mask=0x%04X)\n", g_current_slot, g_valid_mask);
    } else {
        printf("[PROFILES] No valid flash data; using defaults\n");
    }

    // Ensure slot0 reflects the current live keymap at first boot.
    uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
    if (km) {
        memcpy(g_keymaps[0], km, sizeof(g_keymaps[0]));
    }

    // If the stored current slot is valid, load it.
    if (slot_valid(g_current_slot) && km) {
        memcpy(km, g_keymaps[g_current_slot], sizeof(g_keymaps[g_current_slot]));
    } else {
        g_current_slot = 0;
    }
}

void profiles_task(void)
{
    if (g_dirty) {
        profiles_flush_to_flash();
    }
}

bool profiles_save_slot(uint8_t slot, uint8_t r, uint8_t g, uint8_t b, bool static_indicator)
{
    if (slot >= PROFILE_COUNT) return false;
    uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
    if (!km) return false;

    memcpy(g_keymaps[slot], km, sizeof(g_keymaps[slot]));
    g_colors[slot][0] = r;
    g_colors[slot][1] = g;
    g_colors[slot][2] = b;
    g_static_indicator = static_indicator;
    g_valid_mask |= (uint16_t)(1u << slot);
    g_current_slot = slot;
    g_dirty = true;

    printf("[PROFILES] Save slot %u\n", slot);
    return true;
}

bool profiles_load_slot(uint8_t slot)
{
    if (slot >= PROFILE_COUNT) return false;
    if (!slot_valid(slot)) return false;

    uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
    if (!km) return false;

    memcpy(km, g_keymaps[slot], sizeof(g_keymaps[slot]));
    g_current_slot = slot;
    g_dirty = true; // persist current slot

    printf("[PROFILES] Load slot %u\n", slot);
    return true;
}

bool profiles_delete_slot(uint8_t slot)
{
    if (slot == 0) return false;
    if (slot >= PROFILE_COUNT) return false;

    memset(g_keymaps[slot], 0, sizeof(g_keymaps[slot]));
    memset(g_colors[slot], 0, sizeof(g_colors[slot]));
    g_valid_mask &= (uint16_t)~(1u << slot);

    if (g_current_slot == slot) {
        g_current_slot = 0;
        // restore base slot0
        uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
        if (km) {
            memcpy(km, g_keymaps[0], sizeof(g_keymaps[0]));
        }
    }

    g_dirty = true;
    printf("[PROFILES] Delete slot %u\n", slot);
    return true;
}

bool profiles_create_blank_slot(uint8_t slot)
{
    if (slot == 0) return false;
    if (slot >= PROFILE_COUNT) return false;

    memset(g_keymaps[slot], 0, sizeof(g_keymaps[slot]));
    g_valid_mask |= (uint16_t)(1u << slot);
    memset(g_colors[slot], 0, sizeof(g_colors[slot]));

    // Also load it immediately (matches UI expectation).
    uint8_t (*km)[SENSOR_COUNT] = get_keymap_ptr();
    if (km) {
        memset(km, 0, sizeof(g_keymaps[slot]));
    }
    g_current_slot = slot;

    g_dirty = true;
    printf("[PROFILES] Create blank slot %u\n", slot);
    return true;
}
