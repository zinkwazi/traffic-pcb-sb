# agents/

Tooling for AI agents working in this repo — not part of the firmware build.

## run_test.sh

Builds, flashes, and runs an on-device Unity test variant in one command,
replacing the multi-step build/flash/monitor dance described in CLAUDE.md's
Testing section:

```sh
agents/run_test.sh test_refresh
agents/run_test.sh test_hardware --app-only          # skip bootloader/partition reflash
agents/run_test.sh test_refresh --no-build            # already built, just flash + run
agents/run_test.sh test_refresh --port /dev/ttyUSB0
```

It reads the variant's `executable`/sdkconfig settings straight out of
`esp_idf_project_configuration.json` (so they can't drift from what the
VS Code extension would use), sources the ESP-IDF `export.sh` itself, and
sets `IDF_CCACHE_ENABLE=1` so rebuilding a second variant that shares most
sources (e.g. `test_hardware` after `test_refresh`) doesn't recompile
everything from scratch.

Prints the full serial output and exits 0/1/2 (pass/fail/timeout) based on
the Unity summary line, so it's safe to use directly in a script:
`agents/run_test.sh test_refresh || echo "tests failed"`.

## monitor_unity.py

The serial half of `run_test.sh`, usable standalone if you've already
flashed: `agents/monitor_unity.py /dev/ttyACM0 60`. Resets the board, then
reads until it sees Unity's `N Tests X Failures Y Ignored` summary line and
exits immediately — no fixed sleep, so a fast test run finishes fast. The
timeout argument is only a safety net for a hung board.
