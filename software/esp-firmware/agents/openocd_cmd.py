#!/usr/bin/env python3
"""Sends one command to a running OpenOCD over its Tcl RPC port and prints the reply.

Usage: agents/openocd_cmd.py "<command>" [--port 6666] [--timeout SECONDS]

OpenOCD's Tcl RPC protocol: send the command terminated by 0x1a, read the
reply until 0x1a. Commands like `esp gcov` log their progress to OpenOCD's
own log rather than the RPC reply, so callers should verify side effects
(e.g. .gcda files appearing) rather than rely on the reply text.

Exit codes: 0 = reply received, 1 = could not connect, 2 = timed out.
"""
import argparse
import socket
import sys

TERMINATOR = b"\x1a"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command")
    parser.add_argument("--port", type=int, default=6666)
    parser.add_argument("--timeout", type=float, default=60.0)
    args = parser.parse_args()

    try:
        sock = socket.create_connection(("127.0.0.1", args.port), timeout=5)
    except OSError as e:
        print(f"could not connect to OpenOCD on port {args.port}: {e}", file=sys.stderr)
        return 1

    sock.settimeout(args.timeout)
    sock.sendall(args.command.encode() + TERMINATOR)

    reply = b""
    try:
        while not reply.endswith(TERMINATOR):
            chunk = sock.recv(4096)
            if not chunk:
                break
            reply += chunk
    except socket.timeout:
        print(f"timed out waiting for OpenOCD reply to '{args.command}'", file=sys.stderr)
        return 2
    finally:
        sock.close()

    text = reply.rstrip(TERMINATOR).decode(errors="replace").strip()
    if text:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
