#!/usr/bin/env python3
"""Ask a running QEMU monitor for a PPM screen dump and convert it to PNG."""
from __future__ import annotations

import argparse
import socket
import time
from pathlib import Path
import sys

from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=5556)
parser.add_argument("--out", default="screen.png")
args = parser.parse_args()

root = Path(__file__).resolve().parent
out = (root / (Path(args.out).stem + ".ppm")).resolve()
png = (root / args.out).resolve()

with socket.create_connection(("127.0.0.1", args.port), timeout=5) as sock:
    sock.settimeout(2)
    try:
        sock.recv(4096)
    except TimeoutError:
        pass
    command = f"screendump {out.as_posix()}\n".encode()
    sock.sendall(command)
    time.sleep(1)
    try:
        sock.recv(4096)
    except TimeoutError:
        pass

if not out.is_file() or out.stat().st_size == 0:
    print(f"no screenshot produced at {out}", file=sys.stderr)
    raise SystemExit(1)

with Image.open(out) as image:
    image.convert("RGB").save(png, "PNG")
print(f"{png} {png.stat().st_size} bytes")
