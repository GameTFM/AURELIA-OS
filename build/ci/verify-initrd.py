#!/usr/bin/env python3
"""Check the casper patch and overlayfs module inside an AURELIA initrd.

An initrd can be several concatenated archives: uncompressed cpio segments
(microcode, firmware) followed by a gzip cpio. This walks each segment.
"""
from __future__ import annotations

import argparse
import zlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("initrd")
args = parser.parse_args()

data = Path(args.initrd).read_bytes()


def parse_cpio(body: bytes, entries: dict[str, bytes]) -> None:
    pos = 0
    while pos + 110 <= len(body):
        if body[pos:pos + 6] not in (b"070701", b"070702"):
            break
        def field(index: int) -> int:
            start = pos + 6 + 8 * index
            return int(body[start:start + 8], 16)
        size = field(6)
        namesize = field(11)
        name = body[pos + 110:pos + 110 + namesize - 1].decode(errors="replace")
        pos = (pos + 110 + namesize + 3) & ~3
        content = body[pos:pos + size]
        pos = (pos + size + 3) & ~3
        if name == "TRAILER!!!":
            break
        entries[name] = content


entries: dict[str, bytes] = {}
pos = 0
segments = 0
while pos < len(data):
    if data[pos:pos + 6] in (b"070701", b"070702"):
        # Find the end of this archive, then resume scanning after it.
        scan = pos
        while scan + 110 <= len(data):
            if data[scan:scan + 6] not in (b"070701", b"070702"):
                break
            def field(index: int, base: int = scan) -> int:
                start = base + 6 + 8 * index
                return int(data[start:start + 8], 16)
            size = field(6)
            namesize = field(11)
            name = data[scan + 110:scan + 110 + namesize - 1]
            scan = (scan + 110 + namesize + 3) & ~3
            scan = (scan + size + 3) & ~3
            if name == b"TRAILER!!!":
                break
        parse_cpio(data[pos:scan], entries)
        segments += 1
        pos = scan
        continue
    if data.startswith(b"\x1f\x8b\x08", pos):
        decomp = zlib.decompressobj(31)
        try:
            body = decomp.decompress(data[pos:]) + decomp.flush()
        except zlib.error:
            pos += 1
            continue
        consumed = len(data) - pos - len(decomp.unused_data)
        parse_cpio(body, entries)
        segments += 1
        pos += consumed
        continue
    pos += 1

overlay = sorted(n for n in entries if "overlayfs/overlay.ko" in n)
casper = entries.get("scripts/casper", b"").decode(errors="replace")
helpers = entries.get("scripts/casper-helpers", b"").decode(errors="replace")
casper_conf = entries.get("etc/casper.conf", b"").decode(errors="replace")
kos = sum(1 for n in entries if n.endswith((".ko", ".ko.zst")))

print("archive segments:", segments)
print("kernel modules:", kos)
print("overlay module:", overlay or "MISSING")
print("patched modprobe call:", 'modprobe ${MP_QUIET} -b overlay' in casper)
print("quoted call remains:", 'modprobe "${MP_QUIET}" -b overlay' in casper)
print("casper.conf BUILD_SYSTEM:", [l for l in casper_conf.splitlines() if "BUILD_SYSTEM" in l])
print("helpers default:", [l.strip() for l in helpers.splitlines() if "MP_QUIET" in l][:3])
if not overlay or "modprobe ${MP_QUIET} -b overlay" not in casper:
    raise SystemExit(1)
print("OK: initramfs carries overlayfs and the unquoted casper call")
