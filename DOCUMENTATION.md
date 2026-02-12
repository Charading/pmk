# PMK Documentation

**PMK (Pico Magnetic Keyboard)** — Build your own Hall Effect keyboard with analog sensing, per-key actuation, RGB lighting, SOCD, and full Nova software compatibility.

---

## Table of Contents

1. [Overview](#overview)
2. [Getting Started](#getting-started)
3. [Directory Structure](#directory-structure)
4. [PMK CLI](#pmk-cli)
5. [Board Configuration — config.h](#board-configuration--configh)
6. [MUX Wiring — hallscan_keymap.h](#mux-wiring--hallscan_keymaph)
7. [Keymap — keymap.c](#keymap--keymapc)
8. [Layout — layout.json](#layout--layoutjson)
9. [Building & Flashing](#building--flashing)
10. [Nova Software Compatibility](#nova-software-compatibility)
11. [Keycodes Reference](#keycodes-reference)

---

## Overview

PMK provides a complete firmware stack for Hall Effect analog keyboards on the Raspberry Pi Pico. You define your board's hardware in **3 files**, and the API handles everything else:

- **Hall effect sensor scanning** via HC4067 analog multiplexers
- **Per-key analog actuation** with configurable thresholds
- **4-layer keymap system** with MO (momentary) and TG (toggle) layer switching
- **WS2812 RGB lighting** with 8 built-in effects (static, breathing, wave, rainbow, reactive, gradient, radial)
- **SOCD (Simultaneous Opposing Cardinal Directions)** — configurable pairs with 3 resolution modes
- **Rotary encoder** support (optional)
- **USB HID** — standard keyboard + consumer keys + vendor raw interface
- **Nova software compatibility** — full integration with the Nova configuration utility
- **Profile system** — save/load up to 10 keymap + lighting profiles to flash

---

## Getting Started

### Prerequisites

**Option A — Automatic (recommended):** The `pmk` CLI downloads everything for you:

```bash
pmk setup
```

**Option B — Manual:** Install these yourself:
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.0+)
- CMake 3.13+
- ARM GCC toolchain (arm-none-eabi-gcc)
- Ninja build system

### Quick Start

1. **Install toolchain** (first time only):
   ```
   pmk setup
   ```

2. **Create your board:**
   ```
   pmk new -kb my_keyboard
   ```

3. **Edit 3 files** to match your PCB:
   - `config.h` — Hardware pins, feature flags, sensor enum
   - `hallscan_keymap.h` — MUX channel → sensor wiring
   - `keymap.c` — Default keycodes per layer

4. **Build:**
   ```
   pmk build -kb my_keyboard
   ```

5. **Flash:**
   ```
   pmk flash -kb my_keyboard
   ```

---

## Directory Structure

```
api/
├── pmk.py                            # Build CLI tool
├── pmk.bat                           # Windows wrapper (just type 'pmk')
│
├── api/                              # Core firmware (DO NOT MODIFY)
│   ├── main.c                        # Main loop, USB, key processing
│   ├── hallscan.c / hallscan.h       # Hall effect sensor scanning
│   ├── hallscan_config.h             # Internal config bridge (auto-included)
│   ├── hid_reports.c / hid_reports.h # HID command protocol
│   ├── keycodes.h                    # QMK-style KC_* keycode defines
│   ├── encoder.c / encoder.h         # Rotary encoder (optional)
│   ├── profiles.c / profiles.h       # Flash profile storage
│   ├── lighting/                     # RGB LED effects engine
│   ├── features/socd/                # SOCD module
│   ├── drivers/                      # WS2812 PIO driver
│   ├── src/usb/                      # TinyUSB configuration
│   ├── build.cmake                   # Shared CMake build logic
│   └── pico_sdk_import.cmake         # Pico SDK import helper
│
├── boards/                           # YOUR BOARDS GO HERE
│   └── example_60/                   # Example 60% ANSI keyboard
│       ├── config.h                  # ← YOU EDIT THIS
│       ├── hallscan_keymap.h         # ← YOU EDIT THIS
│       ├── keymap.c                  # ← YOU EDIT THIS
│       ├── layout.json               # KLE JSON (for Nova software)
│       └── CMakeLists.txt            # Build file
│
└── DOCUMENTATION.md                  # This file
```

**You only edit 3 files.** Everything in `api/` is the core firmware — don't touch it.

---

## PMK CLI

The `pmk` tool handles toolchain setup, building, and flashing.

### Commands

| Command | Description |
|---------|-------------|
| `pmk setup` | Download ARM GCC, CMake, Ninja, Pico SDK (first-time) |
| `pmk build -kb <board>` | Build firmware |
| `pmk flash -kb <board>` | Build + flash via picotool |
| `pmk new -kb <board>` | Create a new board from template |
| `pmk clean -kb <board>` | Remove build artifacts |
| `pmk list` | List available boards |
| `pmk doctor` | Check if all tools are installed |

### First-time Setup

```bash
cd api
pmk setup     # Downloads ~300MB of toolchain tools
pmk doctor    # Verify everything is installed
```

The toolchain is stored in `api/.toolchain/` (git-ignored). Run `setup` once and you're good.

---

## Board Configuration — config.h

This is the main configuration file for your keyboard. It contains everything about your hardware: MCU, USB IDs, features, pin assignments, LED layout, and sensor definitions.

### MCU Selection

Uncomment one to match your microcontroller:

```c
#define MCU_RP2040     // Raspberry Pi Pico / RP2040
// #define MCU_RP2350  // Raspberry Pi Pico 2 / RP2350
```

### USB Identification

```c
#define USB_VID         0x1234          // USB Vendor ID
#define USB_PID         0x5678          // USB Product ID
#define KEYBOARD_NAME   "MyBoard60"     // Shown in OS device list
#define MANUFACTURER    "Your Name"     // Manufacturer string
```

Get a unique VID/PID from [pid.codes](https://pid.codes) or use a testing range.

### Feature Flags

Features use **presence-based defines** — just `#define` them to enable, or comment them out to disable. No `= 1` needed.

```c
// Define to enable, comment out to disable. No value needed.
#define RGB_ENABLE
#define CAPS_LOCK_INDICATOR
// #define ENCODER_ENABLE
// #define DISPLAY_ENABLE
```

| Flag | What it enables |
|------|----------------|
| `RGB_ENABLE` | WS2812 LED support (requires LED pin/count config below) |
| `CAPS_LOCK_INDICATOR` | Caps Lock LED highlight (requires `CAPS_LOCK_LED_INDEX`) |
| `ENCODER_ENABLE` | Rotary encoder input (requires encoder pins below) |
| `DISPLAY_ENABLE` | SPI TFT display (advanced) |

### LED Configuration

```c
#define LED_DATA_PIN        22      // GPIO pin for WS2812 data line

#define LED_COUNT           64      // Total LEDs on strip (key + ambient + status)
#define KEY_LED_COUNT       64      // Per-key LEDs
#define AMBIENT_LED_COUNT   0       // Underglow strip LEDs (0 if none)
#define STATUS_LED_COUNT    0       // Status indicator LEDs (0 if none)
```

### LED Power Gate (Optional)

If your PCB has a MOSFET controlling the 5V rail to LEDs, define both lines:

```c
#define LED_GATE_PIN        23      // GPIO controlling the MOSFET gate
#define LED_GATE_ACTIVE     LOW     // LOW = P-FET (active-low), HIGH = N-FET
```

If your board has no LED gate MOSFET, leave both lines commented out.

### LED Strip Direction (Optional)

If your LED strip data enters from the opposite end of your PCB, uncomment this:

```c
// #define LED_STRIP_REVERSED
```

### LED Position Map

Maps logical key index to physical LED strip position. Required to handle serpentine (zigzag) PCB wiring:

```c
static const uint8_t led_position_map[64] = {
    // Row 0: Forward (left-to-right)
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
    // Row 1: Reverse (serpentine)
    27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14,
    // Row 2: Forward
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    // Row 3: Reverse
    52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
    // Row 4: Forward
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
};
```

If all your LED rows go left-to-right, just use `0, 1, 2, 3, ...` in order.

### Brightness

The default USB brightness is **50%**. To override it, add:

```c
#define USB_BRIGHTNESS_PERCENT  60  // 0-100
```

### Encoder Configuration

Only needed if `ENCODER_ENABLE` is defined:

```c
#define ENCODER_A_PIN               7       // Quadrature signal A
#define ENCODER_B_PIN               8       // Quadrature signal B
#define ENCODER_SW_PIN              9       // Push switch (active LOW)
// #define ENCODER_ROTATION_INVERTED       // Uncomment to swap CW/CCW
```

### Caps Lock Indicator

Only needed if `CAPS_LOCK_INDICATOR` is defined:

```c
#define CAPS_LOCK_LED_INDEX         29      // LED index of the Caps Lock key
```

For boards with an ambient strip that should also indicate Caps Lock:

```c
#define HAS_AMBIENT_CAPS_INDICATOR  1
#define AMBIENT_START_INDEX         64
#define AMBIENT_CAPS_INDICATOR_COUNT 8
```

### MUX Pin Assignments

The HC4067 multiplexer uses 4 shared address lines (S0-S3) and one ADC input per chip:

```c
// Address lines (shared across ALL MUX chips)
#define MUX_S0_PIN  10
#define MUX_S1_PIN  11
#define MUX_S2_PIN  12
#define MUX_S3_PIN  13

// ADC input pins — one per MUX chip
#define MUX1_ADC_PIN    26      // MUX 1 → ADC0
#define MUX2_ADC_PIN    27      // MUX 2 → ADC1
#define MUX3_ADC_PIN    28      // MUX 3 → ADC2
#define MUX4_ADC_PIN    29      // MUX 4 → ADC3

// Total number of MUX chips on your PCB
#define MUX_COUNT       4
```

**RP2040 ADC pins:** GP26 (ADC0), GP27 (ADC1), GP28 (ADC2), GP29 (ADC3) — max 4 MUXes, 64 keys.

**RP2350 ADC pins:** GP26-GP29 + GP40-GP43 — up to 8 MUXes, 128 keys.

### Sensor Enum

Define one entry per key on your keyboard. This enum maps human-readable names (`S_ESC`, `S_A`, etc.) to sensor indices used throughout the firmware.

**Rules:**
- Must start at `1` (value `0` means "unmapped channel")
- Must end with `SENSOR_COUNT_PLUS_1`
- Group by keyboard row for clarity
- Use `S_<KEYNAME>` naming convention

```c
#define MAX_KEYS 64     // Maximum number of keys

typedef enum sensor_names {
    // Row 0 (14 keys): Esc, 1-0, -, =, Backspace
    S_ESC = 1, S_1, S_2, S_3, S_4, S_5, S_6, S_7, S_8, S_9, S_0, S_MINS, S_EQLS, S_BSPC,

    // Row 1 (14 keys): Tab, Q-], Backslash
    S_TAB, S_Q, S_W, S_E, S_R, S_T, S_Y, S_U, S_I, S_O, S_P, S_LBRC, S_RBRC, S_BSLS,

    // Row 2 (13 keys): Caps, A-', Enter
    S_CAPS, S_A, S_S, S_D, S_F, S_G, S_H, S_J, S_K, S_L, S_SCLN, S_QUOT, S_ENT,

    // Row 3 (12 keys): LShift, Z-/, RShift
    S_LSFT, S_Z, S_X, S_C, S_V, S_B, S_N, S_M, S_COMM, S_DOT, S_SLSH, S_RSFT,

    // Row 4 (11 keys): LCtrl, Win, LAlt, Space, RAlt, Fn, Arrows, Menu, RCtrl
    S_LCTL, S_WIN, S_LALT, S_SPC, S_RALT, S_FN, S_LEFT, S_DOWN, S_RGHT, S_MENU, S_RCTL,

    SENSOR_COUNT_PLUS_1,    // <-- DO NOT REMOVE
} sensor_names_t;
```

The enum values auto-increment from the starting `= 1`, so you only need to set the first value. The API uses `SENSOR_COUNT_PLUS_1` to derive the total key count automatically.

---

## MUX Wiring — hallscan_keymap.h

This file maps each MUX chip's 16 channels to the sensor (key) connected to it. You need one array per MUX chip.

### How It Works

Each HC4067 MUX has 16 channels (0-15), selected by the shared address pins (S0-S3). Each channel connects to one Hall effect sensor under a key. This file tells the firmware which sensor is on which MUX channel.

### How To Fill It In

1. Look at your PCB schematic or wiring diagram.
2. For each MUX chip, note which key's sensor is wired to each channel.
3. Fill in the arrays using `S_<KEYNAME>` from your sensor enum in config.h.
4. Use `{ 0 }` for channels with no sensor connected.

```c
#include "hallscan_config.h"

// MUX 1 — Connected to MUX1_ADC_PIN
const mux16_ref_t mux1_channels[16] = {
    [0]  = { S_ESC  },     // Channel 0 → Esc sensor
    [1]  = { S_1    },     // Channel 1 → 1 key sensor
    [2]  = { S_2    },
    ...
    [15] = { S_Q    },
};

// MUX 2 — Connected to MUX2_ADC_PIN
const mux16_ref_t mux2_channels[16] = {
    [0]  = { S_W    },
    ...
    [15] = { 0      },     // Unused channel
};
```

**You can use enum names or raw numbers:**

```c
[3] = { S_E },      // Using enum name (recommended)
[3] = { 18 },       // Using raw number (S_E = 18 in 60% layout)
```

Both are equivalent. The channel order doesn't need to follow any pattern — it depends entirely on your PCB wiring. Just match each channel to the sensor it's physically connected to.

---

## Keymap — keymap.c

Define your keyboard's default keycodes using QMK-style `KC_*` names. Each array has `SENSOR_COUNT` entries, one per key, ordered by sensor enum.

```c
#include "hallscan_config.h"
#include "keycodes.h"

// Layer 0 (Base)
const uint8_t default_keycodes[SENSOR_COUNT] = {
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    ...
    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    ...
    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    ...
    KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    ...
    KC_LCTL, KC_LGUI, KC_LALT, KC_SPC,  KC_RALT, KC_FN,   KC_LEFT, ...
};

// Layer 1 (Fn)
const uint8_t layer1_keycodes[SENSOR_COUNT] = {
    KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   ...
    ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ...
    // Use ______ (six underscores) for transparent keys
};
```

### Transparent Keys

`KC_TRNS` (or `______`) means "fall through to the layer below." When Layer 1 is active and a key is `KC_TRNS`, the firmware uses the Layer 0 keycode instead.

### Layer Control Keycodes

| Keycode | Action |
|---------|--------|
| `MO(1)`, `MO(2)`, `MO(3)` | Momentary layer — active while held |
| `TG(1)`, `TG(2)`, `TG(3)` | Toggle layer — tap to switch |
| `KC_FN` | Alias for `MO(1)` |

### Special Keycodes

| Keycode | Action |
|---------|--------|
| `KC_BOOTLOADER` | Enter USB bootloader (BOOTSEL mode) |
| `KC_CALIBRATE` | Trigger hall sensor recalibration |
| `KC_LED_TOG` | Toggle RGB LED power on/off |
| `KC_SOCD_TOG` | Toggle SOCD on/off |
| `KC_MUTE`, `KC_VOLU`, `KC_VOLD` | Volume control |
| `KC_MPLY`, `KC_MNXT`, `KC_MPRV` | Media control |

---

## Layout — layout.json

Export your keyboard layout from [Keyboard Layout Editor (KLE)](http://www.keyboard-layout-editor.com/) as JSON and save it as `layout.json`. This file is used by the Nova software to render your keyboard visually.

1. Go to http://www.keyboard-layout-editor.com/
2. Design your layout (or load a preset)
3. Go to the "Raw data" tab
4. Copy the JSON and save as `layout.json` in your board folder

The layout JSON is **not used by the firmware** — it's only for the software visualization.

---

## Building & Flashing

### Using pmk (recommended)

```bash
pmk build -kb my_keyboard     # Build firmware
pmk flash -kb my_keyboard     # Build + flash in one step
pmk clean -kb my_keyboard     # Remove build artifacts
```

The CLI auto-detects the Pico SDK and toolchain (from `pmk setup` or system PATH).

### Manual Build

If you prefer running cmake/ninja directly:

```bash
cd api/boards/my_keyboard
mkdir build && cd build
cmake -G Ninja -DPICO_SDK_PATH="/path/to/pico-sdk" ..
ninja
```

This produces:
- `my_keyboard.uf2` — Flash via BOOTSEL or picotool
- `my_keyboard.elf` — Flash via SWD debug probe

### Flashing Methods

**pmk flash** (easiest):
```bash
pmk flash -kb my_keyboard
```

**UF2 drag-and-drop:**
1. Hold the **BOOTSEL** button on your Pico while plugging in USB.
2. A drive named `RPI-RP2` appears.
3. Drag your `.uf2` file onto the drive.
4. The Pico reboots and runs your firmware.

**picotool** (no button needed):
```bash
picotool load -fx my_keyboard.uf2
```

### Method 3: SWD via Debug Probe

```bash
openocd \
  -f interface/cmsis-dap.cfg \
  -f target/rp2040.cfg \
  -c "adapter speed 5000" \
  -c "program my_keyboard.elf verify reset exit"
```

For RP2350, use `-f target/rp2350.cfg`.

---

## Nova Software Compatibility

Your keyboard automatically works with the Nova configuration utility. The firmware exposes 4 USB HID interfaces:

| Interface | Direction | Purpose |
|-----------|-----------|---------|
| 0 - Keyboard | Keyboard → OS | Standard keyboard reports |
| 1 - VIA Raw | Bidirectional | VIA/SignalRGB compatibility |
| 2 - App Raw | Software → Keyboard | Commands from Nova |
| 3 - Response Raw | Keyboard → Software | Async responses to Nova |

Nova identifies your keyboard by `USB_VID` and `USB_PID`. To add your board to Nova, you'll need to register these IDs in the Nova device configuration.

### Supported Nova Features

- Keymap editing (all 4 layers)
- Per-key actuation threshold adjustment
- RGB lighting effects and per-key color painting
- SOCD pair configuration
- Sensor calibration
- Profile save/load
- Live ADC value streaming (Performance tab)
- SignalRGB integration

---

## Keycodes Reference

### Letters & Numbers
`KC_A`..`KC_Z`, `KC_1`..`KC_0`

### Function Keys
`KC_F1`..`KC_F24`

### Standard Keys
| Keycode | Key | Keycode | Key |
|---------|-----|---------|-----|
| `KC_ENT` | Enter | `KC_ESC` | Escape |
| `KC_BSPC` | Backspace | `KC_TAB` | Tab |
| `KC_SPC` | Space | `KC_CAPS` | Caps Lock |
| `KC_MINS` | - | `KC_EQL` | = |
| `KC_LBRC` | [ | `KC_RBRC` | ] |
| `KC_BSLS` | \\ | `KC_SCLN` | ; |
| `KC_QUOT` | ' | `KC_GRV` | \` |
| `KC_COMM` | , | `KC_DOT` | . |
| `KC_SLSH` | / | `KC_DEL` | Delete |

### Modifiers
| Left | Right |
|------|-------|
| `KC_LCTL` / `KC_LCTRL` | `KC_RCTL` / `KC_RCTRL` |
| `KC_LSFT` / `KC_LSHIFT` | `KC_RSFT` / `KC_RSHIFT` |
| `KC_LALT` | `KC_RALT` |
| `KC_LGUI` / `KC_LWIN` | `KC_RGUI` / `KC_RWIN` |

### Navigation
`KC_INS`, `KC_HOME`, `KC_PGUP`, `KC_DEL`, `KC_END`, `KC_PGDN`
`KC_UP`, `KC_DOWN`, `KC_LEFT`, `KC_RGHT`

### Layer Control
| Keycode | Action |
|---------|--------|
| `MO(1)` / `KC_FN` | Momentary layer 1 |
| `MO(2)`, `MO(3)` | Momentary layer 2, 3 |
| `TG(1)`, `TG(2)`, `TG(3)` | Toggle layer 1, 2, 3 |
| `KC_TRNS` / `______` | Transparent (fall through) |

### Media
`KC_MUTE`, `KC_VOLU`, `KC_VOLD`, `KC_MPLY`, `KC_MNXT`, `KC_MPRV`

### System
`KC_BOOTLOADER`, `KC_CALIBRATE`, `KC_LED_TOG`, `KC_SOCD_TOG`, `KC_PSCR`, `KC_SCRL`, `KC_PAUS`
