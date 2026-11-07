#!/usr/bin/env python3
"""Build the AURELIA OS Legacy Alpha Windows installer.

Requires:
  - Windows + .NET Framework csc.exe
  - toolchain/qemu/ from the legacy kernel setup
  - build/aurelia.iso from `python scripts/build.py`
"""
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "windows-installer"
BUILD = ROOT / "build" / "windows-installer"
STAGE = BUILD / "payload"
QEMU = ROOT / "toolchain" / "qemu"
ISO = ROOT / "build" / "aurelia.iso"
CSC = Path(os.environ.get(
    "CSC",
    r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
))
MANIFEST = SOURCE / "app.manifest"
ASSEMBLY = SOURCE / "AssemblyInfo.cs"
ICON = BUILD / "Aurelia.ico"
PAYLOAD_ZIP = BUILD / "AureliaPayload.zip"
OUT = BUILD / "AURELIA-Setup.exe"


def run(command: list[str]) -> None:
    print("+", " ".join(str(part) for part in command))
    subprocess.run(command, check=True)


def make_icon() -> None:
    logo = ROOT / "distro" / "assets" / "branding" / "aurelia-logo.png"
    image = Image.open(logo).convert("RGBA")
    image.save(
        ICON,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )


def copy_qemu() -> None:
    target = STAGE / "qemu"
    share = target / "share"
    target.mkdir(parents=True)
    share.mkdir()

    # The GUI binary avoids a console window. The console binary is kept for
    # the installer's headless boot test.
    for name in ("qemu-system-x86_64.exe", "qemu-system-x86_64w.exe"):
        source = QEMU / name
        if not source.is_file():
            raise SystemExit(f"missing QEMU binary: {source}")
        shutil.copy2(source, target / name)

    dlls = sorted(QEMU.glob("*.dll"))
    if not dlls:
        raise SystemExit(f"no QEMU DLLs found in {QEMU}")
    for dll in dlls:
        shutil.copy2(dll, target / dll.name)

    for pattern in ("bios*.bin", "vgabios*.bin", "efi-*.rom", "kvmvapic.bin"):
        for source in (QEMU / "share").glob(pattern):
            shutil.copy2(source, share / source.name)

    keymaps = QEMU / "share" / "keymaps"
    if keymaps.is_dir():
        shutil.copytree(keymaps, share / "keymaps")


def make_payload() -> None:
    if not ISO.is_file():
        raise SystemExit(
            f"missing {ISO}; run `python scripts/build.py` before building the installer"
        )

    if STAGE.exists():
        shutil.rmtree(STAGE)
    (STAGE / "os").mkdir(parents=True)
    copy_qemu()
    shutil.copy2(ISO, STAGE / "os" / "aurelia.iso")
    shutil.copy2(SOURCE / "README.txt", STAGE / "README.txt")

    run([
        str(CSC),
        "/nologo",
        "/target:winexe",
        "/optimize+",
        "/platform:anycpu",
        "/langversion:5",
        "/out:" + str(STAGE / "AureliaLauncher.exe"),
        "/win32icon:" + str(ICON),
        "/win32manifest:" + str(MANIFEST),
        str(SOURCE / "AureliaLauncher.cs"),
        str(ASSEMBLY),
    ])

    if PAYLOAD_ZIP.exists():
        PAYLOAD_ZIP.unlink()
    with zipfile.ZipFile(PAYLOAD_ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(STAGE.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(STAGE).as_posix())


def make_installer() -> None:
    if OUT.exists():
        OUT.unlink()

    run([
        str(CSC),
        "/nologo",
        "/target:winexe",
        "/optimize+",
        "/platform:anycpu",
        "/langversion:5",
        "/out:" + str(OUT),
        "/win32icon:" + str(ICON),
        "/win32manifest:" + str(MANIFEST),
        "/resource:" + str(PAYLOAD_ZIP) + ",AureliaSetup.payload.zip",
        "/reference:System.dll",
        "/reference:System.Core.dll",
        "/reference:System.Windows.Forms.dll",
        "/reference:System.Drawing.dll",
        "/reference:System.IO.Compression.dll",
        "/reference:System.IO.Compression.FileSystem.dll",
        "/reference:Microsoft.CSharp.dll",
        str(SOURCE / "AureliaSetup.cs"),
        str(ASSEMBLY),
    ])


def digest(path: Path) -> str:
    hash_value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            hash_value.update(block)
    return hash_value.hexdigest()


def main() -> int:
    if not CSC.is_file():
        raise SystemExit(f"csc.exe not found: {CSC}")
    BUILD.mkdir(parents=True, exist_ok=True)
    make_icon()
    make_payload()
    make_installer()

    checksum = digest(OUT)
    (BUILD / "AURELIA-Setup.exe.sha256").write_text(
        checksum + "  AURELIA-Setup.exe\n",
        encoding="ascii",
    )
    print()
    print(f"PAYLOAD={PAYLOAD_ZIP} ({PAYLOAD_ZIP.stat().st_size / (1024 * 1024):.1f} MiB)")
    print(f"INSTALLER={OUT} ({OUT.stat().st_size / (1024 * 1024):.1f} MiB)")
    print(f"SHA256={checksum}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
