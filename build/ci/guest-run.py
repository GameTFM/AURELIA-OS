#!/usr/bin/env python3
"""Run a batch of commands in the AURELIA guest over its serial console.

Two parsing rules matter here and both were learned from failed runs:

* A marker string appears in the *echoed command line* before the shell runs
  anything, so matching it anywhere returns the echo and loses the output. A
  command is finished only when the marker appears on a line of its own.
* Boot text also contains "login:" and "$", so a shell must be proven by
  running a probe command and reading its output line, not by matching a prompt.

QEMU's TCP serial chardev accepts one client at a time, so this process connects
once and must stay connected for the whole boot.
"""
import argparse
import re
import socket
import sys
import time
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=5557)
parser.add_argument("--login", default="aurelia")
parser.add_argument("--out", default="build/ci/guest-session.log")
parser.add_argument("--boot-timeout", type=float, default=900.0)
parser.add_argument("--cmd-timeout", type=float, default=240.0)
parser.add_argument("--cmd", action="append", default=[])
args = parser.parse_args()

log = Path(args.out)
log.parent.mkdir(parents=True, exist_ok=True)
log.write_text("", encoding="utf-8")

ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07\x1b]*(\x07|\x1b\\)")


def clean(raw: bytes) -> str:
    return OSC.sub("", ANSI.sub("", raw.decode("utf-8", "replace"))).replace("\r", "")


def emit(text: str) -> None:
    print(text, flush=True)
    with log.open("a", encoding="utf-8") as handle:
        handle.write(text + "\n")


for _ in range(120):
    try:
        sock = socket.create_connection(("127.0.0.1", args.port), timeout=10)
        break
    except OSError:
        time.sleep(1)
else:
    raise SystemExit(f"cannot reach the serial console on port {args.port}")
sock.settimeout(0.5)


def drain(seconds: float) -> str:
    deadline = time.time() + seconds
    text = ""
    while time.time() < deadline:
        try:
            chunk = sock.recv(65536)
            if not chunk:
                break
            text += clean(chunk)
        except socket.timeout:
            continue
    return text


def read_until_line(marker: str, limit: float) -> str:
    """Read until `marker` is a line of its own: the shell finished the command."""
    deadline = time.time() + limit
    text = ""
    while time.time() < deadline:
        try:
            chunk = sock.recv(65536)
            if not chunk:
                break
            text += clean(chunk)
            if any(line.strip() == marker for line in text.splitlines()):
                break
        except socket.timeout:
            continue
    return text


PROBE = "__AURELIA_SHELL_PROBE__"


def probe(attempts: int = 3, window: float = 15.0) -> bool:
    for _ in range(attempts):
        sock.sendall(b"\r")
        drain(1)
        sock.sendall(f"echo {PROBE}\r".encode())
        text = read_until_line(PROBE, window)
        if any(line.strip() == PROBE for line in text.splitlines()):
            return True
    return False


emit("== finding a shell ==")
sock.sendall(b"\r")
drain(3)

if probe():
    emit("== shell already active ==")
else:
    emit("== logging in ==")
    deadline = time.time() + args.boot_timeout
    saw_prompt = False
    while time.time() < deadline:
        text = drain(5)
        lines = [line for line in text.splitlines() if line.strip()]
        if lines and lines[-1].rstrip().endswith("login:"):
            emit(text.rstrip())
            saw_prompt = True
            break
    if not saw_prompt:
        emit("FAIL: no login prompt on the serial console")
        raise SystemExit(2)

    sock.sendall((args.login + "\r").encode())
    answer = drain(8)
    emit(answer.rstrip())
    if "assword" in answer:
        sock.sendall(b"\r")
        drain(6)
    if probe():
        emit("== shell ready ==")
    else:
        emit("WARN: the shell probe got no output; running the batch anyway")

for index, command in enumerate(args.cmd):
    marker = f"__AURELIA_DONE_{index}__"
    emit(f"\n$ {command}")
    sock.sendall(f"{command}; echo {marker}\r".encode())
    output = read_until_line(marker, args.cmd_timeout)
    lines = []
    for line in output.splitlines():
        stripped = line.strip()
        if stripped == marker or stripped == f"{command}; echo {marker}":
            continue
        lines.append(line)
    emit("\n".join(lines).strip())

emit("== batch complete ==")
