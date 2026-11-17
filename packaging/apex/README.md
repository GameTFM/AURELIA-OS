# Apex — standalone package

Apex is the browser that ships with AURELIA OS: a real PyQt6 application on Qt
WebEngine (Chromium's engine) with its own tabs, address bar, bookmarks, history,
downloads, private windows, settings, permission prompts and tracker deny-list.

This directory packages that same payload as a normal Debian package so the
browser can be installed on Ubuntu 26.04 without the whole ISO.

## Build

```bash
bash packaging/apex/build-deb.sh
# -> build/ci/artifact/apex_1.0.0_amd64.deb
```

Requires `dpkg-deb` (run it on Ubuntu/Debian or inside `AURELIA-BUILD`).

## Install

```bash
sudo apt install ./apex_1.0.0_amd64.deb
apex https://example.com
```

The package depends on `python3-pyqt6` and `python3-pyqt6.qtwebengine`, which
are available in Ubuntu 26.04.

## Honest status

- This is **not** Brave and not built from `brave-core`. That project is a full
  Chromium fork; compiling it needs many hours and tens of gigabytes and cannot
  be done in this build environment. The Brave project is a reference for
  product behaviour (built-in blocking, privacy defaults), not a code base this
  package uses.
- On a machine without a DRM render node (VMs, software-only sessions) Apex
  currently starts and builds its interface but **does not present a window**.
  That defect is tracked in `../../KNOWN_ISSUES.md`; the launcher's graphics
  fallback has been corrected but window mapping is unresolved.
- Do not present this package as a finished browser until that is fixed and
  verified on real hardware.

## Uninstall

```bash
sudo apt remove apex
```
