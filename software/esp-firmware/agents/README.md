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
`CMakePresets.json` (so they can't drift from what the
VS Code extension would use), sources the ESP-IDF `export.sh` itself, and
sets `IDF_CCACHE_ENABLE=1` so rebuilding a second variant that shares most
sources (e.g. `test_hardware` after `test_refresh`) doesn't recompile
everything from scratch.

Prints the full serial output and exits 0/1/2 (pass/fail/timeout) based on
the Unity summary line, so it's safe to use directly in a script:
`agents/run_test.sh test_refresh || echo "tests failed"`.

The serial output alone (no build/flash logs; `\r` and ANSI escapes
stripped) is also written to `builds/<variant>/test/test_results.txt`,
overwritten every run, so results can be queried without wading through
stdout:

```sh
agents/run_test.sh test_refresh > /dev/null 2>&1; echo "exit=$?"
grep -E ':(PASS|FAIL)' builds/test_refresh/test/test_results.txt
```

For variants built with `--coverage` (currently `test_refresh`), it then
dumps gcov data through OpenOCD (`esp gcov`, reusing a running OpenOCD or
starting its own) and runs `gcovr`, writing to the same `test/` folder:
`coverage_summary.txt` (line coverage + uncovered line numbers),
`coverage_branches.txt` (branch coverage + lines with untaken branches), and
`coverage_report/index.html`. Skip with `--no-coverage`. Coverage failures
are reported on stderr and never change the exit code. See CLAUDE.md's
"Code coverage" section.

## openocd_cmd.py

Sends one command to a running OpenOCD over its Tcl RPC port (6666) and
prints the reply: `agents/openocd_cmd.py "esp gcov"`. Used by `run_test.sh`
for the coverage dump.

## monitor_unity.py

The serial half of `run_test.sh`, usable standalone if you've already
flashed: `agents/monitor_unity.py /dev/ttyACM0 60`. Resets the board, then
reads until it sees Unity's `N Tests X Failures Y Ignored` summary line and
exits immediately — no fixed sleep, so a fast test run finishes fast. The
timeout argument is only a safety net for a hung board.
