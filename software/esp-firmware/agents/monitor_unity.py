#!/usr/bin/env python3
"""Reads Unity test output from a serial port and exits as soon as the run finishes.

Replaces the fixed-sleep approach (e.g. "wait 40s and hope it's done") with
early-exit on the Unity summary line, so a fast test run doesn't cost the
full timeout every time. Prints all captured output; the timeout is only a
safety net for a hung board.

Exit codes: 0 = all tests passed, 1 = at least one failure, 2 = timed out
before seeing a summary line (board hung, wrong baud rate, etc).
"""
import re
import sys
import time

import serial

SUMMARY_RE = re.compile(rb"\d+ Tests \d+ Failures \d+ Ignored")


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <port> [timeout_seconds]", file=sys.stderr)
        return 2

    port = sys.argv[1]
    timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0

    ser = serial.Serial(port, 115200, timeout=0.5)
    # Reset the board over USB-Serial/JTAG so the test run starts from boot.
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setRTS(False)

    buf = b""
    deadline = time.time() + timeout
    saw_summary = False

    while time.time() < deadline:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            if not saw_summary and SUMMARY_RE.search(buf):
                saw_summary = True
                # Summary line is followed by a trailing OK/FAIL line; give it
                # a brief moment to arrive instead of exiting mid-line.
                deadline = min(deadline, time.time() + 1.0)
        elif saw_summary:
            break

    ser.close()

    text = buf.decode("utf-8", errors="replace")
    print(text)

    lines = [l.strip() for l in text.splitlines() if l.strip()]
    last = lines[-1] if lines else ""

    if last == "OK":
        return 0
    if last == "FAIL":
        return 1
    if SUMMARY_RE.search(buf):
        m = re.search(rb"\d+ Tests (\d+) Failures", buf)
        return 0 if m and m.group(1) == b"0" else 1

    print("TIMEOUT: no Unity summary line seen before deadline", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
