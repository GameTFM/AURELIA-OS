#!/usr/bin/env python3
"""Control the running AURELIA QEMU instance through its monitor socket.

Commands:
  shot [name]        capture the framebuffer to build/<name>.png
  quit               power off the VM
  key <keyname>      send a key (QEMU sendkey syntax, e.g. ret, esc, alt-tab)
  type <text>        type ASCII text
  mouse <dx> <dy>    move the pointer by a relative delta
  click <button>     press+release a mouse button (1 left, 2 middle, 3 right)
  clickat <dx> <dy>  move, then press+release the left button in one batch
  drag <dx> <dy>     hold left button, move by a delta, release
  info               print VM status
"""
from __future__ import annotations

import os
import socket
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
PORT = int(os.environ.get("AURELIA_MONITOR_PORT", "45454"))

KEYMAP = {
    " ": "spc", "\n": "ret", "-": "minus", "=": "equal", ".": "dot", ",": "comma",
    "/": "slash", ";": "semicolon", "'": "apostrophe", "[": "bracket_left",
    "]": "bracket_right", "\\": "backslash", "`": "grave_accent", "\t": "tab",
}


def monitor(commands: list[str], wait: float = 0.35) -> str:
    with socket.create_connection(("127.0.0.1", PORT), timeout=5) as sock:
        sock.settimeout(1.0)
        try:
            sock.recv(4096)
        except socket.timeout:
            pass
        out = b""
        for command in commands:
            sock.sendall((" " + command + "\n").encode())
            time.sleep(wait)
            try:
                out += sock.recv(65536)
            except socket.timeout:
                pass
        return out.decode("utf-8", "replace")


def ppm_to_png(ppm: Path, png: Path) -> None:
    from PIL import Image
    with Image.open(ppm) as image:
        image.save(png)
    ppm.unlink(missing_ok=True)


def shot(name: str = "screen") -> Path:
    BUILD.mkdir(exist_ok=True)
    ppm = BUILD / (name + ".ppm")
    png = BUILD / (name + ".png")
    if ppm.exists():
        ppm.unlink()
    monitor(["screendump " + str(ppm).replace("\\", "/")], wait=0.6)
    if not ppm.exists():
        raise SystemExit("screendump failed (is the VM running?)")
    ppm_to_png(ppm, png)
    print(png)
    return png


def send_key(key: str) -> None:
    monitor(["sendkey " + key])


def type_text(text: str) -> None:
    commands = []
    for ch in text:
        if ch.isalpha():
            commands.append("sendkey " + ch.lower())
        elif ch.isdigit():
            commands.append("sendkey " + ch)
        elif ch in KEYMAP:
            commands.append("sendkey " + KEYMAP[ch])
        else:
            continue
    if commands:
        monitor(commands, wait=0.05)


def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        return
    command = sys.argv[1]
    if command == "shot":
        shot(sys.argv[2] if len(sys.argv) > 2 else "screen")
    elif command == "quit":
        monitor(["quit"])
    elif command == "key":
        send_key(sys.argv[2])
    elif command == "type":
        type_text(" ".join(sys.argv[2:]))
    elif command == "mouse":
        monitor(["mouse_move %s %s" % (sys.argv[2], sys.argv[3])])
    elif command == "click":
        button = sys.argv[2] if len(sys.argv) > 2 else "1"
        monitor(["mouse_button " + button], wait=0.08)
        monitor(["mouse_button 0"], wait=0.08)
    elif command == "clickat":
        monitor(["mouse_move %s %s" % (sys.argv[2], sys.argv[3]),
                 "mouse_button 1", "mouse_button 0"], wait=0.2)
    elif command == "drag":
        monitor(["mouse_button 1"], wait=0.08)
        monitor(["mouse_move %s %s" % (sys.argv[2], sys.argv[3])], wait=0.12)
        monitor(["mouse_button 0"], wait=0.08)
    elif command == "info":
        print(monitor(["info status", "info block"], wait=0.4))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
