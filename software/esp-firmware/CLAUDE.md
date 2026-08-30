# ESP-IDF Traffic PCB Firmware

An ESP-IDF project for the ESP32-S3 that drives an LED display of LA freeway traffic conditions. The board shows live and typical speed data as color-coded LEDs (red/orange/blue for slow/medium/fast) in north and south directions.

## Hardware Versions

- **V1.0**: 326 LEDs, 3 IS31FL3741A matrices, 1 I2C bus. **Deprecated and unsupported.**
- **V2.0 / V2.1**: 414 LEDs, 4 IS31FL3741A matrices, 2 I2C buses, ambient light sensing. **Current supported hardware.**

Version-specific code lives in `versions/V1_0/` and `versions/V2_0/`. All active development targets V2.0/V2.1.

## Build System

Build variants are defined in `esp_idf_project_configuration.json`, keyed by name (e.g. `test_refresh`, `test_hardware`, `production_V2_1`, `test_nonmock`, `test_manual`, `test_ota`, `test_actions1`, `test_actions2`). This JSON is a **VS Code ESP-IDF extension format only** — plain `idf.py` never reads it. Building the same variant from a bare CLI invocation requires reproducing two things from that JSON entry yourself, or the build silently uses the wrong config:

1. **The `executable` env var** selects which branch of the root `CMakeLists.txt` runs (`$ENV{executable}`, lowercase) — this picks `TEST_COMPONENTS`, which sources get compiled, etc. It comes from the variant's `"env": {"executable": "..."}` in `esp_idf_project_configuration.json`.
   **`idf.py -DEXECUTABLE=<name>` does NOT work** — that sets an unused CMake cache variable named `EXECUTABLE`; nothing reads it. Always `export executable=<name>` first.
2. **`sdkconfigDefaults` and `sdkconfigFilePath`** select which Kconfig options actually land in the build (`CONFIG_HARDWARE_VERSION`, `CONFIG_TEST_REFRESH`, etc.). These must be passed explicitly as `-DSDKCONFIG_DEFAULTS=...` (semicolon-joined) and `-DSDKCONFIG=...` — copy them verbatim from the variant's `"build"` block in `esp_idf_project_configuration.json`.

Skipping step 2 doesn't fail the build — it silently falls back to `<project_dir>/sdkconfig` (creating one from bare Kconfig defaults if absent), which does **not** set `CONFIG_HARDWARE_VERSION=2` or `CONFIG_TEST_REFRESH=y` etc. This is invisible until either a `CONFIG_HARDWARE_VERSION`-gated file fails to compile, or (worse) it compiles a materially different, half-configured build that looks fine at a glance. It's most likely to bite right after `idf.py fullclean`, since the build directory's cached sdkconfig association (set up once by the extension) is gone at that point and every subsequent plain `idf.py build` regenerates the fallback.

Locate ESP-IDF first — `<path-to-esp-idf>` below is not a project-relative path; on this machine it's `~/.espressif/v6.0.1/esp-idf` (find it generically with `cat ~/.espressif/idf-env.json` — the `idfInstalled` keys are the install paths — or `find ~/.espressif -maxdepth 3 -path "*/esp-idf" -type d`).

Full working example for `test_refresh` (read the values from `esp_idf_project_configuration.json` for any other variant — e.g. swap `test_refresh` for `test_hardware` everywhere below, including inside `-DSDKCONFIG_DEFAULTS`/`-DSDKCONFIG`, to build/run `led_matrix`'s own hardware tests instead):
```sh
source ~/.espressif/v6.0.1/esp-idf/export.sh
export executable=test_refresh
idf.py -B builds/test_refresh \
  -DSDKCONFIG_DEFAULTS="configurations/sdkconfig.settings;configurations/secrets/sdkconfig.test_secrets;configurations/sdkconfig.test;components/refresh/test/sdkconfig.test;configurations/sdkconfig.V2_0_default" \
  -DSDKCONFIG=configurations/current/sdkconfig.test_refresh \
  build
```

The VS Code ESP-IDF extension handles both of the above automatically per-profile when you build through its UI. But if `.vscode/settings.json` has a global `idf.customExtraVars.executable` entry, that **overrides** the per-profile `executable` value for every build regardless of which profile is selected — this has silently pinned the wrong test target before (a stale `test_hardware` value made `test_refresh` link in `led_matrix`'s tests). If build profile switching in the extension doesn't seem to be taking effect, check `.vscode/settings.json` for a stale `idf.customExtraVars.executable` first.

sdkconfig files live in `configurations/`. Component sources are conditionally compiled based on Kconfig options (e.g. `CONFIG_TEST_REFRESH`, `CONFIG_HARDWARE_VERSION`, `CONFIG_FAKE_LED_MATRIX`).

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

Boundary exactness (e.g. whether a value exactly at a cutoff is classified into the lower or upper bucket) does not matter for this project — either classification is acceptable at the cutoff itself.

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
main/src/tests/test_main.c        — app_main for test_refresh/test_hardware: runs unity_run_all_tests()
components/refresh/test/
  CMakeLists.txt                  — registers test component; REQUIRES unity, refresh, common
  sdkconfig.test                  — sdkconfig fragment merged in for the test_refresh build
  tests/
    test_refresh_fsm.c            — TEST_CASE macros for refresh_fsm.h (no hardware, no mocks)
    test_refresh_task.c           — TEST_CASE macros for refresh_task.h
    test_refresh_utilities.c      — TEST_CASE macros for refresh_utilities.h (real led_matrix hardware)
components/led_matrix/test/
  CMakeLists.txt
  tests/
    test_led_matrix.c             — TEST_CASE macros for led_matrix.h (real hardware; test_hardware build only)
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

**Mocking (architectural policy: fake only hardware dependencies):** Nothing in `refresh`'s test suite is CMock-mocked anymore — `refresh_utilities.c` (real, always built — no more `CONFIG_TEST_REFRESH`-gated swap to a mock) and `led_matrix` (real, no software fake yet) are both exercised against real hardware by `test_refresh_utilities.c` and `test_refresh_task.c` (board must be connected and flashed). `CONFIG_FAKE_LED_MATRIX` (`led_matrix/Kconfig.projbuild`) is reserved for a planned hand-written fake that will let these tests run without hardware, but it isn't wired up in `led_matrix/CMakeLists.txt` yet. Round-trip hardware tests follow a strict save/set/verify/restore pattern (see `test_led_matrix.c`) so they leave the matrix ICs as they found them; failure-injection tests (forcing a call to return an error) aren't possible against real hardware and are deferred until the fake exists. The `cmock` component/CMock-generated mocks (`components/*/mocks/Mock*.c/h`) remain available in principle for a pure-logic dependency with no hardware behind it, but nothing in `refresh` currently uses one.

**Building tests:** see the full `test_refresh` example (env var + `-DSDKCONFIG_DEFAULTS`/`-DSDKCONFIG`) under **Build System** above — a bare `export executable=test_refresh && idf.py -B builds/test_refresh build` only works if the build directory already has a cached sdkconfig association from a prior extension-driven configure; after `fullclean` it silently produces a broken config.

**Fast path for agents:** `agents/run_test.sh <variant>`** builds, flashes, and reads back results for a test variant (e.g. `test_refresh`, `test_hardware`) in one command, deriving the env var/sdkconfig args from `esp_idf_project_configuration.json` itself and exiting as soon as the Unity summary line appears instead of polling on a fixed sleep. See `agents/README.md`. Prefer this over the manual steps below unless you're debugging the build/flash process itself.

**Flashing tests to hardware:**
```sh
idf.py -B builds/test_refresh -p /dev/ttyACM0 flash
```
(Adjust the port; find it with `ls /dev/ttyACM*` or `ls /dev/ttyUSB*`. Only one process can hold the serial port at a time — check first with `fuser /dev/ttyACM0` or `ps aux | grep idf_monitor`, since a leftover `idf.py monitor` session (including one opened by the VS Code extension) will make `flash` fail with "port is busy". Don't kill someone else's active monitor session without asking first.)

**Reading test results without a human at the terminal:** `idf.py monitor` is interactive by design — it reads live keystrokes from stdin for its menu/quit shortcuts, so it refuses to start at all when stdin isn't a real TTY ("Monitor requires standard input to be attached to TTY"), which is always the case when run from an automated/non-interactive shell. Work around this by wrapping it in `script` to allocate a pseudo-terminal, and always bound it with `timeout` — the test binary prints results once and then loops forever (`for (;;) {}` in `test_main.c`), so `monitor` never exits on its own:

```sh
timeout 45 script -qec "idf.py -B builds/test_refresh -p /dev/ttyACM0 monitor" /path/to/log.txt
```

Then read `/path/to/log.txt`, stripping ANSI escapes and carriage returns:
```sh
cat -v /path/to/log.txt | sed 's/\^\[\[[0-9;]*[a-zA-Z]//g; s/\^M//g'
```

Each test prints `path/to/file.c:LINE:test_name:PASS` or `:FAIL:<message>` (the message is sometimes empty — a bare `:FAIL` with nothing after it is normal, not truncated output). The run ends with a Unity summary line — `N Tests M Failures K Ignored` followed by `OK` or `FAIL`. A raw `pyserial` read on the port (without `idf.py monitor`) is unreliable here and reliably returned zero bytes in practice — use the `script`-wrapped `monitor` approach instead.

**Don't confuse `THROW_ERR` noise with a failure.** Negative-path tests (invalid-argument checks, "rejects null", etc.) deliberately trigger real `THROW_ERR` calls, which print an `E (...) tag: ...` line and a `Backtrace: ...` line — this is expected, passing behavior, not a crash. `led_matrix`'s hardware test suite in particular produces many of these. When scanning output programmatically, match on `:FAIL` / `:PASS` (or the final summary line) specifically — don't treat the presence of `E (...)` or `Backtrace` lines alone as a sign anything failed.

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
