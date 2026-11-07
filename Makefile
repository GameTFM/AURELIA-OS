# AURELIA OS build system
# The canonical build pipeline lives in scripts/build.py (works on Windows and POSIX);
# this Makefile is a thin, portable wrapper for environments with GNU Make.

PY ?= python
QEMU ?= qemu-system-x86_64

.PHONY: all iso run shot clean toolchain

all:
	$(PY) scripts/build.py

iso:
	$(PY) scripts/build.py

run: all
	$(PY) scripts/run.py

shot:
	$(PY) tools/qemu_ctl.py shot

toolchain:
	$(PY) toolchain/bootstrap.py

clean:
	rm -rf build
