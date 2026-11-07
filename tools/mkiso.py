#!/usr/bin/env python3
"""Build a Rock Ridge ISO 9660 image with an El Torito no-emulation boot entry.

The bootloader and kernel are placed with long Rock Ridge names (limine.conf,
aurelia.elf, limine-bios.sys) while their ISO 9660 names stay 8.3 compatible,
which is what Limine and the firmware expect.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pycdlib

BOOT_IMAGE = "limine-bios-cd.bin"


def add_file(iso, local: Path, iso_path: str, rr_name: str) -> None:
    try:
        iso.add_file(local_path=str(local), iso_path=iso_path, rr_name=rr_name)
    except TypeError:
        iso.add_file(str(local), iso_path, rr_name=rr_name)


def add_dir(iso, iso_path: str, rr_name: str) -> None:
    try:
        iso.add_directory(iso_path=iso_path, rr_name=rr_name)
    except TypeError:
        iso.add_directory(iso_path, rr_name=rr_name)


def make_iso(root: Path, out: Path) -> None:
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=1, rock_ridge="1.09", vol_ident="AURELIA_OS")
    counter = 0
    boot_paths: list[str] = []

    def alloc(is_dir: bool) -> str:
        nonlocal counter
        counter += 1
        return ("D%06d" % counter) if is_dir else ("F%06d.DAT" % counter)

    def walk(local: Path, parent: str) -> None:
        for entry in sorted(local.iterdir(), key=lambda p: (p.is_file(), p.name)):
            if entry.is_dir():
                ipath = parent + "/" + alloc(True)
                add_dir(iso, ipath, entry.name)
                walk(entry, ipath)
            else:
                ipath = parent + "/" + alloc(False) + ";1"
                add_file(iso, entry, ipath, entry.name)
                if entry.name == BOOT_IMAGE:
                    boot_paths.append(ipath)

    walk(root, "")
    if not boot_paths:
        raise SystemExit("boot image %s not found in staging tree" % BOOT_IMAGE)

    iso.add_eltorito(
        bootfile_path=boot_paths[0],
        bootcatfile="/BOOT.CAT;1",
        media_name="noemul",
        boot_load_size=4,
        boot_info_table=True,
    )
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists():
        out.unlink()
    iso.write(str(out))
    iso.close()
    print("  iso9660: %s (%d KiB)" % (out.name, out.stat().st_size // 1024))


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: mkiso.py <iso-root> <output.iso>")
    make_iso(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())


if __name__ == "__main__":
    main()
