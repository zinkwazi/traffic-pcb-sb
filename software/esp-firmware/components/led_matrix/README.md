# `led_matrix`

Hardware abstraction layer for the IS31FL3741A LED matrix driver ICs, controlled over I2C.

Datasheet: https://www.lumissil.com/assets/pdf/core/IS31FL3741A_DS.pdf

## What it does

- Wraps the IC's I2C register interface (operating mode, current control, resistor/logic-level settings, device ID, etc.) behind a small set of `mat*` functions.
- Holds the LED-number → matrix/register lookup tables (`LEDNumToReg`) for both hardware versions, mapping each LED's KiCad reference number to the IC and register that control it.
- Exposes `matSetColor` / `matGetColor` so callers can address LEDs by number without knowing which matrix or register they live on.

## Hardware versions

| Version | LEDs | Matrices | I2C buses |
|---|---|---|---|
| V1.0 (deprecated) | 326 | 3 | 1 |
| V2.0 / V2.1 (current) | 414 | 4 | 2 |

Version-specific register tables live in `src/V1_0_led_registers.c` and `src/V2_0_led_registers.c`, selected at build time via `CONFIG_HARDWARE_VERSION`.

## Layout

```
include/
  led_matrix.h      - public API: init, per-register mat* accessors, matSetColor/matGetColor
  led_registers.h    - LEDNumToReg lookup table declaration, MAX_NUM_LEDS_REG
  led_types.h        - LEDReg and related types
src/
  led_matrix.c        - I2C driver implementation
  led_registers.c     - isLEDValid() and shared lookup helpers
  V1_0_led_registers.c - LEDNumToReg table for V1.0 hardware
  V2_0_led_registers.c - LEDNumToReg table for V2.0/V2.1 hardware
fake/
  led_matrix.c        - software-only stand-in for led_matrix.c (not yet wired into the build)
test/
  tests/test_led_matrix.c - Unity test cases (real hardware required)
```

## Key API

```c
esp_err_t initLedMatrix(void);
esp_err_t getLedMatrixStatus(void);

esp_err_t matSetColor(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetColor(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);
esp_err_t matSetScaling(uint16_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
esp_err_t matGetScaling(uint16_t ledNum, uint8_t *red, uint8_t *green, uint8_t *blue);

esp_err_t matReset(void);
esp_err_t matGetDeviceID(uint8_t *id, Matrix matrix);
```

Plus one `matSet*`/`matGet*` pair per IC configuration register (operating mode, open/short detection, logic level, SWx setting, global current control, pull-up/pull-down resistors — see `led_matrix.h` for the full list).

> **Note:** PWM frequency select (register `0x36`) was removed — it never worked on the hardware this was tested against.

Version-specific entry points:
- **V1.0:** `matInitialize(i2c_port_num_t port, gpio_num_t sdaPin, gpio_num_t sclPin)`
- **V2.0/V2.1:** `matSetGCCByAmbientLight(void)` — adjusts global current control from the ambient light sensor.

## Testing

Testing the `led_matrix` component and anything depending on it requires real hardware to test with, so a software fake exists to allow software-only testing. The fake can be enabled via the `CONFIG_FAKE_LED_MATRIX` option. This fake lives in `fake/` and replaces each respective source file in `src/`.