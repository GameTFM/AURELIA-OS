#!/usr/bin/env python3
"""Type a line into a QEMU guest through the monitor's sendkey command."""
import argparse
import socket
import time

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=5558)
parser.add_argument("--text", default="")
# Raw monitor key name, e.g. --key ctrl-x or --key ret. Useful at the GRUB menu
# and for keystrokes that are not characters.
parser.add_argument("--key")
args = parser.parse_args()

# QEMU's monitor uses '-' for key chords (shift-a), never '+'. Using '+' made
# the monitor reject the command, so every uppercase letter was silently lost.
MAP = {
    " ": "spc", "-": "minus", "/": "slash", ".": "dot", "_": "shift-minus",
    "=": "equal", ",": "comma", "|": "shift-backslash", ";": "semicolon",
    ">": "shift-dot", "<": "shift-comma", ":": "shift-semicolon",
    "\"": "shift-apostrophe", "'": "apostrophe", "$": "shift-4",
    "?": "shift-slash", "&": "shift-7", "*": "shift-8", "(": "shift-9",
    ")": "shift-0", "!": "shift-1", "+": "shift-equal", "@": "shift-2",
    "[": "bracket_left", "]": "bracket_right", "\\": "backslash",
    "{": "shift-bracket_left", "}": "shift-bracket_right", "#": "shift-3",
    "~": "shift-grave_accent", "`": "grave_accent", "%": "shift-5",
    "^": "shift-6",
}

def key_for(ch: str) -> str:
    if ch in MAP:
        return MAP[ch]
    if ch.isalpha():
        if ch.isupper():
            return "shift-" + ch.lower()
        return ch
    if ch.isdigit():
        return ch
    raise ValueError(f"unsupported character: {ch!r}")

with socket.create_connection(("127.0.0.1", args.port), timeout=5) as sock:
    sock.settimeout(0.5)
    try:
        sock.recv(65536)
    except Exception:
        pass
    if args.key:
        sock.sendall(f"sendkey {args.key}\n".encode())
        time.sleep(0.3)
        print("sent key", args.key)
        raise SystemExit(0)
    for ch in args.text:
        sock.sendall(f"sendkey {key_for(ch)}\n".encode())
        time.sleep(0.15)
    sock.sendall(b"sendkey ret\n")
    time.sleep(0.3)
print("sent", args.text)
