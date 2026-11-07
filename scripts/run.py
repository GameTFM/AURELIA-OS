#!/usr/bin/env python3
"""Boot AURELIA OS in QEMU (headless with serial log + monitor socket)."""
from __future__ import annotations

import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
ISO = BUILD / "aurelia.iso"
MONITOR_PORT = int(os.environ.get("AURELIA_MONITOR_PORT", "45454"))

DETACHED_PROCESS = 0x00000008
CREATE_NEW_PROCESS_GROUP = 0x00000200


def find_qemu() -> str:
    env = os.environ.get("QEMU")
    if env and Path(env).exists():
        return env
    local = ROOT / "toolchain" / "qemu" / "qemu-system-x86_64.exe"
    if local.exists():
        return str(local)
    found = shutil.which("qemu-system-x86_64")
    if found:
        return found
    raise SystemExit(
        "QEMU not found. Install it (winget install SoftwareFreedomConservancy.QEMU) "
        "or unpack it into toolchain/qemu/.")


def port_open(port: int) -> bool:
    with socket.socket() as s:
        s.settimeout(0.2)
        return s.connect_ex(("127.0.0.1", port)) == 0


def main() -> None:
    if not ISO.exists():
        raise SystemExit("missing %s - run python scripts/build.py first" % ISO)
    qemu = find_qemu()
    ram = os.environ.get("AURELIA_RAM", "1024")
    serial = BUILD / "serial.log"
    qemu_log = BUILD / "qemu.log"
    if serial.exists():
        serial.unlink()

    args = [
        qemu,
        "-name", "AURELIA OS 1.0.0",
        "-m", ram,
        "-cdrom", str(ISO),
        "-boot", "d",
        "-vga", "std",
        "-display", "none",
        "-serial", "file:" + str(serial),
        "-monitor", "telnet:127.0.0.1:%d,server,nowait" % MONITOR_PORT,
        "-rtc", "base=localtime",
        "-no-reboot",
        "-no-shutdown",
        "-net", "none",
    ]
    print("launching:", " ".join(args[:4]), "...")
    with open(qemu_log, "wb") as log:
        proc = subprocess.Popen(args, cwd=str(ROOT), stdout=log, stderr=subprocess.STDOUT,
                                creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP)
    (BUILD / "qemu.pid").write_text(str(proc.pid))

    deadline = time.time() + 15
    while time.time() < deadline:
        if port_open(MONITOR_PORT):
            break
        time.sleep(0.2)
    print("qemu pid %d, monitor on 127.0.0.1:%d" % (proc.pid, MONITOR_PORT))
    print("serial log:", serial)
    time.sleep(2)
    if serial.exists():
        text = serial.read_text(errors="replace")
        print("--- serial (first lines) ---")
        print("\n".join(text.splitlines()[:28]))
    if qemu_log.exists() and qemu_log.stat().st_size:
        print("--- qemu log ---")
        print(qemu_log.read_text(errors="replace")[-2000:])


if __name__ == "__main__":
    main()
