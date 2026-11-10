#!/usr/bin/env python3
"""Inspect an AURELIA ISO: size, MBR/GPT, El Torito entries and key paths."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

SECTOR = 2048


def inspect(path: Path) -> int:
    size = path.stat().st_size
    print(f"ISO      {path}")
    print(f"size     {size} bytes ({size / 2**30:.2f} GiB)")
    problems = 0
    with path.open("rb") as handle:
        mbr = handle.read(512)
        if mbr[510:512] == b"\x55\xaa":
            print("MBR      0x55AA signature present")
        else:
            print("MBR      0x55AA signature MISSING")
            problems += 1
        for index in range(4):
            entry = mbr[446 + 16 * index : 446 + 16 * (index + 1)]
            if entry[4]:
                start = struct.unpack("<I", entry[8:12])[0]
                count = struct.unpack("<I", entry[12:16])[0]
                print(f"  part{index + 1} type=0x{entry[4]:02x} start={start} sectors={count}")

        handle.seek(512)
        print("GPT      header " + ("present" if handle.read(8) == b"EFI PART" else "absent"))

        handle.seek(16 * SECTOR)
        catalog_lba = None
        for _ in range(32):
            vd = handle.read(SECTOR)
            if len(vd) < SECTOR:
                break
            if vd[0] == 0 and vd[1:6] == b"CD001":
                catalog_lba = struct.unpack("<I", vd[0x47:0x4B])[0]
                break
            if vd[0] == 255:
                break
        if catalog_lba is None:
            print("El Torito catalog MISSING")
            problems += 1
        else:
            print(f"El Torito catalog at sector {catalog_lba}")
            handle.seek(catalog_lba * SECTOR)
            catalog = handle.read(SECTOR)
            platforms = {0x00: "x86 BIOS", 0x01: "PowerPC", 0x02: "Mac", 0xEF: "UEFI"}
            offset = 32
            while offset + 32 <= len(catalog):
                entry = catalog[offset : offset + 32]
                if entry[0] in (0x88, 0x00) and entry[1] in (0, 1, 2, 3, 4):
                    sectors = struct.unpack("<H", entry[6:8])[0]
                    rba = struct.unpack("<I", entry[8:12])[0]
                    print(
                        f"  boot entry platform={platforms.get(entry[1], hex(entry[1]))} "
                        f"media=0x{entry[1]:02x} sectors={sectors} lba={rba}"
                    )
                    handle.seek(rba * SECTOR)
                    head = handle.read(8)
                    print(f"    image head {head.hex()}")
                elif entry[0] in (0x90, 0x91):
                    print(f"  section header platform={platforms.get(entry[1], hex(entry[1]))}")
                offset += 32

    try:
        import pycdlib  # type: ignore

        iso = pycdlib.PyCdlib()
        iso.open(str(path))
        for name in ("/CASPER", "/BOOT/GRUB", "/EFI"):
            try:
                children = sorted(child.file_identifier().decode(errors="replace") for child in iso.list_children(iso_path=name))
                print(f"{name}: {children[:14]}{' ...' if len(children) > 14 else ''}")
            except Exception as exc:  # noqa: BLE001
                print(f"{name}: {exc}")
        iso.close()
    except ImportError:
        print("pycdlib not installed; filesystem listing skipped")
    except Exception as exc:  # noqa: BLE001
        print(f"filesystem listing failed: {exc}")

    return problems


if __name__ == "__main__":
    target = Path(sys.argv[1] if len(sys.argv) > 1 else "live-image-amd64.hybrid.iso")
    raise SystemExit(1 if inspect(target) else 0)
