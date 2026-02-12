# MARSVLT Keyboard API Changelog

## v1.0.0 — 2026-02-11

### Initial Release

- Hall effect sensor scanning via HC4067 analog multiplexers (up to 8 MUXes / 128 keys)
- 4-layer keymap system with MO (momentary) and TG (toggle) layer switching
- QMK-compatible keycode format (KC_A, KC_TRNS, ______, MO(1), etc.)
- WS2812 RGB lighting engine with 8 built-in effects
  - Static, Breathing, Wave, Wave Reverse, Radial, Gradient, Rainbow, Reactive
- Per-key and zone-based lighting control (main / ambient / status)
- SOCD (Simultaneous Opposing Cardinal Directions) with 3 resolution modes
- Rotary encoder support (optional, behind ENCODER_ENABLE flag)
- USB HID 4-interface architecture (Keyboard, VIA Raw, App Raw, Response Raw)
- Full Nova software compatibility (keymap editing, RGB, calibration, profiles)
- Profile system — save/load up to 10 keymap + lighting profiles to flash
- Per-key actuation threshold adjustment via Nova software
- Live ADC value streaming for performance tuning
- SignalRGB per-zone streaming support
- KLE JSON layout format for Nova software keyboard rendering
- Support for RP2040 and RP2350 microcontrollers
- Comprehensive documentation and example 60% ANSI board
