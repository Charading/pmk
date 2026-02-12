# PMK — Pico Magnetic Keyboard

**Open-source Hall Effect keyboard firmware for the Raspberry Pi Pico.**

Build your own analog keyboard with per-key magnetic sensing, adjustable actuation, RGB lighting, SOCD, and a full desktop configuration app — no soldering iron for the firmware side, just edit 3 files and flash.

---

## Features

- **Hall Effect analog sensing** — HC4067 multiplexers scan up to 128 keys (8 MUXes) with per-key adjustable actuation thresholds
- **RP2040 & RP2350** — Works with the original Pico and the Pico 2
- **4-layer keymap** — QMK-style keycodes (`KC_A`, `MO(1)`, `TG(2)`, `______`)
- **WS2812 RGB** — 8 built-in effects: static, breathing, wave, rainbow, reactive, gradient, radial
- **SOCD** — 3 resolution modes for competitive gaming (neutral, last-input-wins, first-input-wins)
- **Rotary encoder** — Optional quadrature encoder with push switch
- **Profile system** — Save/load up to 10 full profiles (keymap + lighting) to flash
- **Nova software** — Full GUI configuration: keymap editing, per-key RGB painting, calibration, live ADC streaming
- **SignalRGB** — Per-zone LED streaming support
- **PMK CLI** — One-command build and flash, automatic toolchain download

---

## Quick Start

### 1. Clone

```bash
git clone https://github.com/Charading/pmk.git
cd pmk
```

### 2. Install Toolchain

```bash
pmk setup
```

This downloads ARM GCC, CMake, Ninja, and the Pico SDK (~300 MB) into a local `.toolchain/` directory. Run it once.

Already have the [Raspberry Pi Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)? PMK auto-detects its toolchain — no setup needed.

### 3. Create Your Board

```bash
pmk new -kb my_keyboard
```

### 4. Configure 3 Files

| File | What you define |
|------|----------------|
| `config.h` | MCU, USB IDs, features, pins, LED map, sensor enum |
| `hallscan_keymap.h` | MUX channel-to-key wiring |
| `keymap.c` | Default keycodes per layer |

### 5. Build & Flash

```bash
pmk build -kb my_keyboard     # compile
pmk flash -kb my_keyboard     # compile + flash
```

---

## Directory Structure

```
pmk/
├── pmk.py                  # CLI build tool
├── pmk.bat                 # Windows shortcut
├── PMK Terminal.bat        # Opens a pre-configured terminal window
│
├── api/                    # Core firmware  (do not modify)
│   ├── main.c              # Main loop, USB, scanning
│   ├── hallscan.c/.h       # Hall sensor scanning engine
│   ├── hallscan_config.h   # Internal config normalization
│   ├── keycodes.h          # QMK-style KC_* defines
│   ├── encoder.c/.h        # Rotary encoder driver
│   ├── profiles.c/.h       # Flash profile storage
│   ├── hid_reports.c/.h    # USB HID protocol
│   ├── lighting/           # RGB effects engine
│   ├── features/socd/      # SOCD module
│   ├── drivers/            # WS2812 PIO driver
│   └── src/usb/            # TinyUSB descriptors
│
├── boards/                 # Your keyboard definitions
│   └── example_60/         # 60% ANSI reference board
│       ├── config.h
│       ├── hallscan_keymap.h
│       ├── keymap.c
│       ├── layout.json
│       └── CMakeLists.txt
│
└── DOCUMENTATION.md        # Full reference docs
```

---

## PMK CLI

| Command | Description |
|---------|-------------|
| `pmk setup` | Download the full toolchain (ARM GCC, CMake, Ninja, Pico SDK) |
| `pmk build -kb <board>` | Compile firmware |
| `pmk flash -kb <board>` | Compile and flash via picotool |
| `pmk new -kb <board>` | Scaffold a new board from the template |
| `pmk clean -kb <board>` | Delete build artifacts |
| `pmk list` | Show all boards |
| `pmk doctor` | Verify toolchain installation |

### Windows Terminal

Double-click **PMK Terminal.bat** to open a command prompt with all paths pre-configured. Type `pmk` and go.

---

## Hardware

### What You Need

| Component | Details |
|-----------|---------|
| MCU | Raspberry Pi Pico (RP2040) or Pico 2 (RP2350) |
| Sensors | 49E linear Hall effect sensors (one per key) |
| Multiplexers | HC4067 16-channel analog MUX (1 per 16 keys) |
| Magnets | Small neodymium disc magnets in each keycap/stem |
| LEDs | WS2812B strip or per-key LEDs (optional) |
| Encoder | EC11 rotary encoder (optional) |

### ADC Pins

| MCU | ADC GPIOs | Max MUXes | Max Keys |
|-----|-----------|-----------|----------|
| RP2040 | GP26-GP29 | 4 | 64 |
| RP2350 | GP26-GP29, GP40-GP43 | 8 | 128 |

---

## Configuration at a Glance

### Feature Flags (config.h)

Presence-based — just `#define` to enable, comment out to disable:

```c
#define RGB_ENABLE              // WS2812 LEDs
#define CAPS_LOCK_INDICATOR     // Caps Lock LED highlight
// #define ENCODER_ENABLE       // Rotary encoder
// #define DISPLAY_ENABLE       // SPI TFT display
```

### Sensor Enum (config.h)

One entry per key, starting at 1:

```c
typedef enum sensor_names {
    S_ESC = 1, S_1, S_2, S_3, S_4, S_5, ...
    SENSOR_COUNT_PLUS_1,
} sensor_names_t;
```

### MUX Wiring (hallscan_keymap.h)

Map each MUX channel to the key wired to it:

```c
const mux16_ref_t mux1_channels[16] = {
    [0]  = { S_ESC  },
    [1]  = { S_1    },
    ...
};
```

### Keymap (keymap.c)

Standard QMK-style keycodes:

```c
const uint8_t default_keycodes[SENSOR_COUNT] = {
    KC_ESC,  KC_1,  KC_2,  KC_3,  KC_4,  KC_5, ...
};
```

---

## Nova Software

PMK keyboards are fully compatible with the **Nova** desktop configuration app:

- Remap all 4 layers with drag-and-drop
- Paint per-key RGB colors and choose effects
- Adjust per-key actuation thresholds
- Configure SOCD pairs and resolution mode
- Calibrate Hall sensors
- Save and load profiles
- Live ADC value streaming (Performance tab)
- SignalRGB integration

Nova communicates over 4 USB HID interfaces:

| Interface | Purpose |
|-----------|---------|
| 0 — Keyboard | Standard HID keyboard + consumer keys |
| 1 — VIA Raw | VIA / SignalRGB protocol |
| 2 — App Raw | Commands from Nova to keyboard |
| 3 — Response Raw | Async responses from keyboard |

---

## Flashing

Three ways to get firmware onto your Pico:

**PMK CLI** (recommended):
```bash
pmk flash -kb my_keyboard
```

**UF2 drag-and-drop:**
Hold BOOTSEL while plugging in USB, then drag the `.uf2` file onto the `RPI-RP2` drive.

**picotool:**
```bash
picotool load -fx my_keyboard.uf2
```

---

## Documentation

See [DOCUMENTATION.md](DOCUMENTATION.md) for the complete reference:

- Every `config.h` define explained
- MUX wiring walkthrough
- Keymap and layer system
- LED position map and serpentine wiring
- Full keycodes table
- Manual build instructions
- SWD flashing

---

## License

MIT

---

## Links

- **PMK repo**: [github.com/Charading/pmk](https://github.com/Charading/pmk)
- **Nova software**: [github.com/Charading/MARSVLT](https://github.com/Charading/MARSVLT)
- **Raspberry Pi Pico SDK**: [github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- **Keyboard Layout Editor**: [keyboard-layout-editor.com](http://www.keyboard-layout-editor.com/)
