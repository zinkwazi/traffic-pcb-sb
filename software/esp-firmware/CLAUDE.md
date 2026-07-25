# ESP-IDF Traffic PCB Firmware

An ESP-IDF project for the ESP32-S3 that drives an LED display of LA freeway traffic conditions. The board shows live and typical speed data as color-coded LEDs (red/orange/blue for slow/medium/fast) in north and south directions.

## Hardware Versions

- **V1.0**: 326 LEDs, 3 IS31FL3741A matrices, 1 I2C bus. **Deprecated and unsupported.**
- **V2.0 / V2.1**: 414 LEDs, 4 IS31FL3741A matrices, 2 I2C buses, ambient light sensing. **Current supported hardware.**

Version-specific code lives in `versions/V1_0/` and `versions/V2_0/`. All active development targets V2.0/V2.1.

## Build System

Build variants are defined in `esp_idf_project_configuration.json`. Use `idf.py -DEXECUTABLE=<name>` to select:

- `V2_0` / `V2_1` — production firmware
- `test_refresh` — unit tests for the refresh component (runs on device via Unity)
- `test_manual` — manual hardware test builds

sdkconfig files live in `configurations/`. Component sources are conditionally compiled based on Kconfig options (e.g. `CONFIG_MOCK_LED_MATRIX`, `CONFIG_TEST_REFRESH`).

## Component Overview

```
components/
  common/         - Shared types (Color, Direction, LEDData, UserSettings)
  led_matrix/     - IS31FL3741A hardware driver (I2C, matSetColor, initLedMatrix)
  refresh/        - LED animation task + FSM (under active refactoring)
  animations/     - LED ordering sequences for animations
  api_connect/    - HTTPS client to fetch traffic CSV data from server
  app_nvs/        - NVS read/write for WiFi credentials and cached speed data
  app_errors/     - Error handling (THROW_ERR macro, fatal/handleable errors)
  indicators/     - Status LEDs (WiFi, OTA, direction)
  input/          - Physical button debounce and MainCommand queue
  ota/            - OTA firmware update task
  actions/        - High-level actions scheduled at particular times or via button press
  circular_buffer/ - Ring buffer used by api_connect CSV parser
  routines/       - Periodic timers
```

## Key Types (common/include/main_types.h)

```c
typedef enum { NORTH, SOUTH, NO_DIR } Direction;
typedef enum { LIVE, TYPICAL } SpeedCategory;
typedef struct { uint8_t red; uint8_t green; uint8_t blue; } Color;
typedef struct { uint16_t ledNum; int8_t speed; } LEDData;
```

Speed-to-color thresholds (`CONFIG_SLOW_CUTOFF_PERCENT` / `CONFIG_MEDIUM_CUTOFF_PERCENT`):
- Slow (< 50% of typical): RED `{0xFF, 0x00, 0x00}`
- Medium (50–80%): ORANGE `{0x15, 0x09, 0x00}`
- Fast (> 80%): BLUE `{0x00, 0x00, 0x10}`

## The Refresh Component (Under Active Refactoring)

This component controls all traffic LEDs. It is currently being split into a focused FSM (`refresh_fsm`) and a thin task wrapper (`refresh_task`). The old monolithic implementation is in `src/refresh.c` and `src/refresh_task.c`.

### Architecture

```
External callers
  ↓  refreshLEDs() / refreshEnableNightMode() / refreshDisableNightMode()
refresh_task      — FreeRTOS task; owns command queue and frame buffers;
                    ticks the FSM; calls led_matrix based on FSM output
  ↓  refreshFSMTick()
refresh_fsm       — Pure logic; no FreeRTOS; no I2C; drives actions via output struct
  ↓  (output struct)
refresh_task      — Executes REFRESH_ACTION_SET / CLEAR / CLEAR_RANGE on hardware
```

The FSM design intentionally avoids mocking and dependency injection: it emits *actions* rather than calling hardware functions, so it can be unit tested with plain C.

### refresh_fsm.h — Public API

**Defined in** `components/refresh/include/refresh_fsm.h`  
**Implemented in** `components/refresh/src/refresh_fsm.c`

```c
void refreshFSMInit(RefreshFSM *fsm, RefreshFSMResources *resources);
RefreshFSMOutput refreshFSMTick(RefreshFSM *fsm, RefreshFSMCommand *cmd);
```

**`RefreshFSM`** is an opaque struct (forward-declared). Callers allocate it; the FSM owns its contents.

**`RefreshFSMResources`** — provided at init, shallow-copied into the FSM:
```c
typedef struct {
    LEDReg    *LEDNumToReg;    // LED number → register lookup table
    uint32_t   LEDNumToRegLen; // highest possible LED number + 1
    LEDSpeed  *LEDFrames;      // flat frame buffer array (owned externally)
    uint32_t   LEDFramesLen;   // number of slots in LEDFrames
    Color      slowLEDColor;
    Color      mediumLEDColor;
    Color      fastLEDColor;
} RefreshFSMResources;
```

**`LEDSpeed`** — the frame element type:
```c
typedef struct { uint16_t ledNum; uint8_t speed; } LEDSpeed;
```
Note: this is different from the old `LEDColor` used in the pre-refactor task. The FSM computes colors dynamically from live speed vs. typical speed.

**`RefreshFSMCommand`** — passed into every tick (use `REFRESH_CMD_NONE` when no new command):
```c
typedef struct {
    RefreshFSMCommandType type;   // REFRESH_CMD_NEW_FRAME | UPDATE_TYPICAL | NIGHT_MODE_ON/OFF | NONE
    uint32_t frameNdx;            // index into LEDFrames; NUM_LED_FRAMES if unused
    uint32_t frameLen;            // number of valid entries in frame; UINT32_MAX if unused
    Direction dir;                // NORTH or SOUTH; NO_DIR if unused
} RefreshFSMCommand;
```

**`RefreshFSMOutput`** — returned from every tick:
```c
typedef struct {
    bool isIdle;                  // FSM has nothing left to do; task may block
    uint32_t releaseFrameNdx;     // frame the task must release (UINT32_MAX if none)
    RefreshFSMAction action;      // what the task must do this tick
    union {
        struct { uint16_t ledNum; Color color; } set;
        struct { uint32_t ledNum; } clear;
        struct { uint32_t startLedNum; Direction dir; } clearRange;
    } actionParams;
} RefreshFSMOutput;
```

**FSM States** (internal, not exposed):
```
WAITING_FOR_FRAMES  — no current or typical frame yet; idle until both arrive
INSTALLING_FRAME    — setting one LED per tick
CLEARING_FRAME      — clearing one LED per tick before installing next frame
QUICK_CLEARING_FRAME — clears entire board in one tick (CLEAR_RANGE action)
CLEARED             — board blank; ready to instantly install next frame
FRAME_INSTALLED     — frame on board; idle until new command
```

**Command behavior summary:**
| Command | From state | Effect |
|---------|-----------|--------|
| `NEW_FRAME` | WAITING_FOR_FRAMES | Store as curr; stay waiting |
| `NEW_FRAME` | CLEARED | Transition to INSTALLING_FRAME |
| `NEW_FRAME` | FRAME_INSTALLED | Transition to CLEARING_FRAME; store as next |
| `NEW_FRAME` | INSTALLING/CLEARING/QUICK_CLEARING | Transition to QUICK_CLEARING_FRAME |
| `UPDATE_TYPICAL` | WAITING + both dirs now have typical | Transition to INSTALLING_FRAME |
| `NIGHT_MODE_ON` | any | Set nightMode flag; begin clearing |
| `NIGHT_MODE_OFF` | any | Clear nightMode flag; reinstall curr frame |

### refresh_task.h — Public API

**Defined in** `components/refresh/include/refresh_task.h`  
**Implemented in** `components/refresh/src/refresh_task.c`

```c
esp_err_t createRefreshTask(TaskHandle_t *handle, const UBaseType_t prio);
esp_err_t refreshLEDs(LEDColor *data, uint32_t dataLen, Direction dir, TickType_t blockTime);
esp_err_t refreshEnableNightMode(TickType_t blockTime);
esp_err_t refreshDisableNightMode(TickType_t blockTime);

// Only available when CONFIG_TEST_REFRESH is set:
void refreshResetTaskResources(void);
```

**Requires** `initLedMatrix()` to have been called before `createRefreshTask`.

**Frame buffer system:** The task maintains `NUM_LED_FRAMES` (4) frame slots in a 2D array. A frame index queue (`ledFramesEmptyNdxQueue`) tracks available slots. Callers claim a slot via `reserveLEDFrame()`, fill it, and attach its index to the queued command. The task releases the slot back to the queue after processing.

**`MAX_FRAME_SIZE`** (512): maximum number of `LEDColor` entries in a frame.

**Internal timing:** `LED_UPDATE_DELAY_MS` (2 ms) between individual LED updates. `xQueueReceive` on the command queue serves as the sleep; a new command arriving early wakes the task immediately.

**`refreshResetTaskResources()`**: resets static state to uninitialized. Only compiled when `CONFIG_TEST_REFRESH=y`. Required in test setUp/tearDown around `createRefreshTask` calls.

### Dependency chain for refresh

```
refresh_task  →  refresh_fsm  (ticks the FSM)
refresh_task  →  led_matrix   (executes FSM actions)
refresh_task  →  led_registers (LEDNumToReg lookup table)
refresh_task  →  animations   (LED ordering)
refresh_fsm   →  main_types   (Color, Direction)
refresh_fsm   →  led_types    (LEDReg, isLEDValid)
```

## Testing

Tests use the **Unity** framework running natively on the ESP32-S3 (not host-side).

**Test structure:**
```
components/refresh/test/
  test_refresh_main.c        — app_main: runs unity_run_all_tests()
  CMakeLists.txt             — registers test component; REQUIRES refresh, led_matrix, common
  tests/
    test_refresh_task.c      — TEST_CASE macros for refresh_task.h
  resources/
    ledFrame1.txt            — embedded test data
```

**Writing tests:**
```c
TEST_CASE("description", "groupName")
{
    /* setup */
    refreshResetTaskResources();

    /* test */
    esp_err_t err = createRefreshTask(NULL, 1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* teardown */
    vTaskDelete(refreshTaskHandle);
    refreshResetTaskResources();
}
```

**Test priority pattern:** Tests that create `refreshTask` at priority 1 must lower the *test task's* priority first (`vTaskPrioritySet(NULL, 2)`) so the refresh task can start before the test checks its state.

**Mocking:** CMock-generated mocks live in `components/led_matrix/mocks/`. Include both the real header and the mock header — the mock header overrides the real implementation under `CONFIG_MOCK_LED_MATRIX=y`.

**Building and flashing tests:**
```sh
idf.py -DEXECUTABLE=test_refresh build flash monitor
```

## Error Handling

`THROW_ERR(err)` macro (from `app_errors/include/app_err.h`): logs the error code with a 5-frame backtrace and returns `err` from the current function.

App-level error codes start at `APP_ERR_BASE` (0xe000) to avoid colliding with ESP-IDF codes. Relevant ones for refresh:
- `APP_ERR_INVALID_PAGE` — LED number not found in lookup table (treated as a no-op, not a failure)
- `APP_ERR_MUTEX_FAIL / TIMEOUT / RELEASE` — traffic data mutex errors

## FreeRTOS Conventions

- Task priorities: main (4) > OTA (3) > refresh (configured at creation)
- Queues are sized conservatively (`COMMAND_QUEUE_LEN` = 5)
- `portMAX_DELAY` used when blocking is safe; explicit `TickType_t blockTime` parameters on public APIs
- `for (;;) {}` as a placeholder for unimplemented fatal error handling (search `TODO: error`)
