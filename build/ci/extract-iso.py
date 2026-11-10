#!/usr/bin/env python3
"""Extract the live kernel and initramfs from an AURELIA ISO."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "pylibs"))
import pycdlib  # noqa: E402

parser = argparse.ArgumentParser()
parser.add_argument("iso")
parser.add_argument("outdir")
args = parser.parse_args()

out = Path(args.outdir)
out.mkdir(parents=True, exist_ok=True)
iso = pycdlib.PyCdlib()
iso.open(args.iso)
names = [c.file_identifier().decode(errors="replace") for c in iso.list_children(rr_path="/casper")]
wanted = {
    "vmlinuz": next(n for n in names if n.startswith("VMLINUZ")),
    "initrd.img": next(n for n in names if n.startswith("INITRD")),
}
for target_name, iso_name in wanted.items():
    target = out / target_name
    with target.open("wb") as handle:
        iso.get_file_from_iso_fp(handle, iso_path=f"/CASPER/{iso_name}")
    print(target, target.stat().st_size)
iso.close()
