#!/usr/bin/env python3
"""Drive the AURELIA guest shell over a QEMU TCP serial console.

Each command is followed by a unique marker inside the guest, and the reader
waits for that marker, so output is captured completely instead of relying on
fixed sleeps (which are unreliable under TCG's slow emulation).
"""
import argparse
import re
import socket
import sys
import time

# The guest prints systemd's Unicode status glyphs; the Windows console uses
# cp1252 by default and raised UnicodeEncodeError mid-diagnosis.
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=5557)
parser.add_argument("--login", default="")
parser.add_argument("--cmd", action="append", default=[])
parser.add_argument("--timeout", type=float, default=120.0)
parser.add_argument("--settle", type=float, default=2.0)
args = parser.parse_args()

sock = socket.create_connection(("127.0.0.1", args.port), timeout=10)
sock.settimeout(0.5)


def read_until(needle: bytes, limit: float) -> bytes:
    deadline = time.time() + limit
    data = b""
    while time.time() < deadline:
        try:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data += chunk
            if needle and needle in data:
                break
        except socket.timeout:
            if not needle:
                break
    return data


def clean(raw: bytes) -> str:
    text = raw.decode("utf-8", "replace")
    text = re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]", "", text)
    text = re.sub(r"\x1b\][^\x07\x1b]*(\x07|\x1b\\)", "", text)
    return text.replace("\r", "")


if args.login:
    sock.sendall(b"\r")
    time.sleep(1)
    sock.sendall(args.login.encode() + b"\r")
    time.sleep(1.5)
    sock.sendall(b"\r")
    time.sleep(1.5)
    print(clean(read_until(b"", 2.0)))

for index, command in enumerate(args.cmd):
    marker = f"__AURELIA_DONE_{index}__"
    payload = f"{command}; echo {marker}\r"
    sock.sendall(payload.encode())
    output = clean(read_until(marker.encode(), args.timeout))
    print(f"$ {command}")
    print(output.rstrip())
    print()
    time.sleep(args.settle)
