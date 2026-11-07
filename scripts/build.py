#!/usr/bin/env python3
"""AURELIA OS build driver.

Cross-compiles the kernel with Zig's bundled clang/lld (ELF, x86_64 freestanding),
stages the ISO root, builds a Rock Ridge ISO 9660 image with an El Torito boot
entry, then installs Limine's BIOS stage 2 into that image.

Usage: python scripts/build.py [--no-iso] [--run]
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
OBJ = BUILD / "obj"
TOOLCHAIN = ROOT / "toolchain"
IS_NT = sys.platform.startswith("win")

ZIG = TOOLCHAIN / "zig" / ("zig.exe" if IS_NT else "zig")
LIMINE_BIN = TOOLCHAIN / "limine" / "limine-binary"
LIMINE_TOOL = LIMINE_BIN / ("limine-tool-windows-x86/limine.exe" if IS_NT else "limine")
TARGET = "x86_64-freestanding-none"

CFLAGS = [
    "-target", TARGET,
    "-std=c11",
    "-O2",
    "-g",
    "-Wall",
    "-Wextra",
    "-Wno-unused-parameter",
    "-ffreestanding",
    "-fno-stack-protector",
    "-fno-stack-check",
    "-fno-pic",
    "-fno-pie",
    "-fno-builtin",
    "-mno-red-zone",
    "-mno-sse",
    "-mno-sse2",
    "-mno-mmx",
    "-mno-80387",
    "-mcmodel=kernel",
    "-mno-avx",
    "-I", str(ROOT / "kernel" / "include"),
]

LDFLAGS = [
    "-target", TARGET,
    "-nostdlib",
    "-static",
    "-no-pie",
    "-Wl,-T," + str(ROOT / "kernel" / "arch" / "x86_64" / "linker.ld"),
    "-Wl,--build-id=none",
    "-Wl,-z,max-page-size=0x1000",
    "-Wl,--no-undefined",
]


def run(cmd: list[str], quiet: bool = False) -> subprocess.CompletedProcess:
    if not quiet:
        printable = " ".join(str(c) for c in cmd)
        print("  $ " + (printable if len(printable) < 240 else printable[:237] + "..."))
    result = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("command failed (%d): %s" % (result.returncode, cmd[0]))
    if result.stderr.strip() and not quiet:
        sys.stderr.write(result.stderr)
    return result


def collect_sources() -> list[Path]:
    kernel = ROOT / "kernel"
    sources = []
    for entry in kernel.rglob("*"):
        if entry.suffix in (".c", ".S", ".s") and entry.is_file():
            sources.append(entry)
    return sorted(sources)


def object_path(source: Path) -> Path:
    rel = source.relative_to(ROOT).as_posix().replace("/", "_")
    return OBJ / (rel + ".o")


def compile_kernel() -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    OBJ.mkdir(parents=True, exist_ok=True)
    sources = collect_sources()
    if not sources:
        raise SystemExit("no kernel sources found")
    if not ZIG.exists():
        raise SystemExit("zig toolchain missing: run python toolchain/bootstrap.py")

    objects = []
    start = time.time()
    for source in sources:
        obj = object_path(source)
        obj.parent.mkdir(parents=True, exist_ok=True)
        if obj.exists() and obj.stat().st_mtime > source.stat().st_mtime:
            objects.append(obj)
            continue
        run([ZIG, "cc", *CFLAGS, "-c", source, "-o", obj])
        objects.append(obj)
    print("  compiled %d objects in %.2fs" % (len(objects), time.time() - start))

    elf = BUILD / "aurelia.elf"
    run([ZIG, "cc", *LDFLAGS, *objects, "-o", elf])
    size = elf.stat().st_size
    print("  linked %s (%.1f KiB)" % (elf.relative_to(ROOT), size / 1024))
    return elf


def stage_iso_root(elf: Path) -> Path:
    iso = BUILD / "isodir"
    if iso.exists():
        shutil.rmtree(iso)
    (iso / "EFI" / "BOOT").mkdir(parents=True)
    (iso / "assets").mkdir(parents=True)

    needed = [
        "limine-bios.sys",
        "limine-bios-cd.bin",
        "limine-uefi-cd.bin",
    ]
    for name in needed:
        src = LIMINE_BIN / name
        if not src.exists():
            raise SystemExit("missing bootloader file: %s" % src)
        shutil.copy2(src, iso / name)
    shutil.copy2(LIMINE_BIN / "BOOTX64.EFI", iso / "EFI" / "BOOT" / "BOOTX64.EFI")
    shutil.copy2(elf, iso / "aurelia.elf")
    shutil.copy2(ROOT / "bootloader" / "limine.conf", iso / "limine.conf")

    assets = ROOT / "assets"
    if assets.is_dir():
        for item in assets.iterdir():
            if item.is_file():
                shutil.copy2(item, iso / "assets" / item.name)
    return iso


def build_iso(iso_root: Path) -> Path:
    out = BUILD / "aurelia.iso"
    run([sys.executable, str(ROOT / "tools" / "mkiso.py"), str(iso_root), str(out)])
    if LIMINE_TOOL.exists():
        # --force is required because the image is a pure El Torito ISO (no MBR
        # partition table); Limine installs its stage 2 into the ISO system area.
        result = subprocess.run([str(LIMINE_TOOL), "bios-install", "--force", str(out)],
                                capture_output=True, text=True)
        if result.returncode != 0:
            print("  limine bios-install failed:")
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
        else:
            print("  limine BIOS stage 2 installed")
    else:
        print("  limine install tool not found; BIOS boot will not work")
    print("  ISO: %s (%.1f MiB)" % (out.relative_to(ROOT), out.stat().st_size / (1024 * 1024)))
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description="Build AURELIA OS")
    parser.add_argument("--no-iso", action="store_true", help="only build the kernel ELF")
    parser.add_argument("--run", action="store_true", help="launch QEMU after building")
    args = parser.parse_args()

    print("AURELIA OS build")
    elf = compile_kernel()
    if not args.no_iso:
        iso_root = stage_iso_root(elf)
        build_iso(iso_root)
    print("build OK")
    if args.run:
        subprocess.run([sys.executable, str(ROOT / "scripts" / "run.py")], check=False)


if __name__ == "__main__":
    main()
