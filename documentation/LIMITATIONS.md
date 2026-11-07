# AURELIA OS — Legacy Kernel Limitations

> **Legacy research track.** This document describes the bespoke AURELIA kernel, not the current Ubuntu 26.04 + KDE Plasma distribution. The installed/live AURELIA image is documented in [`../distro/README.md`](../distro/README.md); Ubuntu supplies its kernel, hardware support, storage and security stack.

AURELIA OS 1.0.0 Alpha "Solstice" boots and runs a real custom kernel and desktop, but it
is **not** a complete, secure or general-purpose operating system. Paging and a narrow
ring-3 test now exist; ordinary applications do not use them. This file records the gaps.

If a capability is not listed as working in [`STATUS.md`](STATUS.md), assume it is absent
or partial.

---

## 1. Architecture, privilege and processes

- **Desktop applications still run in ring 0.** All 15 built-in apps are kernel modules.
  The compositor, WM and app code share the kernel address space.
- **Ring 3 exists only as a boot-time test.** One synchronous test process is created,
  enters CPL 3, makes one write syscall and deliberately faults via `CLI`. There is no
  general user program loader, shell access to ring 3, or persistent user task.
- **No process table, PIDs, scheduler, run queue or preemption.** `SYS_GETPID` currently
  returns the single test PID; it does not reflect a real process database.
- **Cooperative kernel threads are a self-test.** Two kernel stacks rendezvous and switch
  with `sched_switch` to prove context save/restore. They are not scheduled by the timer,
  do not run applications, and are not a preemptive multitasking system.
- **Minimal syscall ABI.** Only `SYS_WRITE`, `SYS_GETPID` and `SYS_EXIT` exist. There is no
  stable ABI, IPC, memory mapping, process creation, file syscall or capability model.
  Return uses IRETQ; SYSRET is not supported because of the current GDT ordering.
- **No application isolation.** A kernel or app bug can touch any kernel memory. The page
  table user bit separates only the synthetic test page from kernel text.
- **No per-application address spaces**, no copy-on-write, no ELF loading, no demand
  paging and no process teardown. The user address space and pages from the boot test are
  not reclaimed.
- **No preemptive security boundary.** Timer IRQs occur in ring 0 and there is no scheduler
  switching between user tasks.

## 2. Memory management

- **Kernel-owned 4-level paging is new but narrow.** At boot the kernel deep-copies the
  Limine higher-half mappings into a new PML4, clears the user bit on every cloned entry,
  verifies kernel text, and switches CR3. This is a real page-table transition.
- **No general virtual memory manager.** There is no unmap, permission-change, page-fault
  resolution, demand paging, swap, COW, file-backed mapping, `mmap`, or per-process memory
  accounting. User mappings are one code page and one stack page for the test process.
- **Kernel mappings are shared** by the test user address space. They are supervisor-only,
  but a future bug in page-table updates could affect all contexts.
- **No user page reclamation.** The boot-test PML4, page tables, code page and stack page
  remain allocated.
- **No NX/SMEP/SMAP/W^X enforcement.** The kernel does not enable NXE or EFER.NX; user code
  pages are not marked executable; SMEP/SMAP are not configured. Paging is not a hardened
  isolation layer yet.
- **Paging assumes 4-level x86_64.** No 5-level paging request or support; no huge-page
  splitting for fine-grained mappings.
- **Kernel heap remains a 64 MiB boundary allocator** without per-process heaps, slab
  caches, OOM policy or swap. Physical memory is a bitmap over the Limine map only.
- **No hibernation or suspend.**
- Tested memory figures (1017 MiB usable, 64 MiB heap) belong to the documented QEMU
  configuration, not all hardware.

## 3. Boot and firmware

- **BIOS only.** The ISO is El Torito with Limine BIOS stage 2. There is no UEFI boot entry
  and no Secure Boot support.
- **QEMU testing only.** No physical-hardware validation or firmware matrix.
- **No recovery OS.** The Limine recovery label boots the same kernel with a cmdline flag;
  there is no separate repair environment, rollback image or rescue shell.
- **No update system.** Manual ISO replacement only; no signatures, A/B partitions or
  rollback.
- **Shutdown/restart are not orderly multi-service transitions**; there are no services to
  stop and no persistent filesystem to flush.

## 4. Storage and filesystem

- **RAMFS only.** The VFS tree lives in kernel memory and is recreated at every boot; all
  files vanish on shutdown.
- **No FAT32**, exFAT, ext, NTFS, ISO filesystem driver, partition parser or block-device
  layer. The boot ISO is read by firmware; AURELIA does not mount it.
- **No ATA/AHCI/SATA/NVMe/USB storage, SD or installation flow.**
- **No persistence, journaling, snapshots, encryption, quotas, symlinks, hard links or
  recovery.**
- **No enforced permissions.** Ownership/ACL semantics requested by the specification are
  not a working security model.
- **Small RAMFS.** 192 seeded nodes, 8 KiB initial data, 256 KiB per-file cap.
- **Explorer gaps:** navigation, tabs, search, create, rename, delete and open work; copy,
  cut, paste, move, multi-select, properties, previews, drag-and-drop, trash and archive
  handling do not.
- **No file associations, installer or user data volume.**

## 5. Hardware support

- **No PCI enumeration**, ACPI parsing, APIC/IOAPIC/MSI, SMP, thermal/power management or
  battery/fan control. CPUID-reported APIC support is not an initialized APIC.
- **Legacy PS/2 input only.** No USB HID, touchpad gestures, mouse wheel or hot-plug.
- **One Limine framebuffer.** No multi-monitor, EDID, mode setting, HDR, scaling or GPU
  acceleration. Rendering is entirely software.
- **PC speaker only.** No HDA/AC'97 codec, mixer, microphone or media-file audio.
- **No Ethernet, Wi-Fi, Bluetooth or modem device support.**
- **No hardware backlight control.** The Settings brightness control dims the compositor.
- **No RTC alarm/watchdog or persistent clock writes.**
- **One tested configuration:** QEMU 11.1.0, BIOS, 1024 MiB, one CPU, 1920×1080. Real
  hardware is unvalidated.

## 6. Desktop and window manager

- **Single display**, no display scaling, rotation, mixed DPI or colour management.
- **Basic window operations.** Drag, edge resize, snap, maximize/restore, minimize, close,
  focus/z-order, Alt+Tab and four workspaces exist. No animations, tiling layouts,
  always-on-top, window rules or remembered geometry.
- **No mouse wheel or touch gestures.**
- **No system clipboard**, so Explorer copy/cut/paste and cross-app text copy are absent.
- **No desktop context menus or icon drag-and-drop.**
- **Limited status tray.** Clock/workspace/title/status pill only; no network, battery or
  Bluetooth indicators because those services do not exist.
- **Transient notifications only**, no notification center/history.
- **No full system search** beyond the launcher's app filter.
- **No font customization or full display scaling.** Theme, accent, wallpaper, density
  token, reduced-motion flag and compositor brightness exist.
- **No automatic theme schedule**, multi-user switching, custom lock-screen background or
  full accessibility stack (screen reader, magnifier, sticky keys, on-screen keyboard).

## 7. Applications

- **Browser is not a web browser.** Offline local-document reader only; no HTML/CSS/JS,
  TLS, HTTP, DNS, downloads, bookmarks synchronization, private mode or web tabs. A modern
  engine cannot be integrated until processes, isolation, networking and a porting layer
  exist.
- **Store cannot install/uninstall.** It lists 15 built-ins only; no catalog, package,
  permissions, signatures, dependencies or updates.
- **Missing Weather, Screenshot utility, Archive manager and PDF viewer.** No codecs for
  images, audio or video. Image Viewer shows procedural art; Tones uses the PC speaker.
- **System Monitor has no process/thread/network/storage-IO data**, because those layers
  are absent. It reports real kernel counters (heap, PMM, VFS, timer, display, window
  table). A flat heap graph is real idle behavior.
- **Terminal is a built-in command interpreter**, not a general shell: no pipelines,
  redirection, environment, scripting, packages, or arbitrary binaries.
- **Calculator is integer/fixed-point** because the kernel is built without SSE.
- **Notes/Editor persistence is session-only** via RAMFS; no rich text or file picker.
- **Clock/Calendar** have no alarms, events, time-zone or sync support.
- **No application sandbox, permissions, trust prompts or crash isolation.**

## 8. Accounts, security and the ring-3 test

- **One public demo account** (`aurelia`/`solstice`) seeded at build time. Never reuse it.
- **Salted SHA-256 password verifier**, not a memory-hard KDF; no pepper, rate limiting,
  lockout, audit trail or account management.
- **No user session isolation.** All apps share ring 0 and one kernel address space.
- **No KASLR/ASLR, hardened allocator, full stack protection, W^X, SMEP/SMAP or secure
  boot.** Do not call the alpha secure.
- **The ring-3 test is not a security boundary for applications.** It proves a page-flag
  separation and a contained CLI fault, not a hardened process model.
- **The page-fault path is not a general user fault service.** The current test uses `CLI`
  to generate a #GP; arbitrary user page faults are not a supported lifecycle.
- **No application permission model**, key store, encryption or security settings pane.
- Physical access and kernel-level bugs defeat the demo login.

## 9. Networking

Entirely absent: no link driver, ARP, IPv4/IPv6, ICMP, DHCP, DNS, TCP/UDP, sockets, firewall
or connection state. Settings' Network pane reports that no interface exists. The Web is
not reachable from AURELIA.

## 10. Updates, packages, recovery, developer platform

- **No package manager**, repository, manifest, dependency resolver, permissions,
  signature verification or install transaction.
- **No update client**, version service, delta image, rollback or integrity check.
- **No separate recovery environment**, safe mode or rescue shell.
- **No SDK, stable third-party ABI, public system headers, packaging tool, example app or
  developer debugging tools.**
- Empty top-level `applications/`, `desktop/`, `libraries/`, `sdk/`, `system/` and `tests/`
  directories are reserved scaffolding. Implementation lives in `kernel/`, `tools/`,
  `scripts/` and `assets/`.

## 11. Testing, performance and claims

- **Manual QEMU testing only.** No CI, automated boot test, unit-test framework, fuzzing,
  coverage or hardware matrix.
- **One configuration**: BIOS, QEMU 11.1.0, 1024 MiB, one CPU, 1920×1080. Other RAM sizes,
  resolutions and real machines are unvalidated.
- **No benchmarks.** No substantiated boot-time, frame-rate, scheduler or memory claims.
- **QEMU relative mouse injection is timing-sensitive**; a missed synthetic click is a
  harness issue, not automatically an app bug. Manual review is required for visual checks.
- **No long-soak, OOM, filesystem-fault or page-fault stress tests.**
- **No preemption test** exists because the scheduler is not implemented.
- **No UEFI, persistence, networking or audio tests** exist because those systems are absent.

---

## Claims this project does not make

- It is not production-ready, secure, or suitable for personal data.
- It does not support UEFI, Secure Boot, real hardware storage, networking, audio or USB.
- It does not run third-party applications, Linux binaries or a modern web browser.
- It has no general preemptive process scheduler, user application runtime, or process
  isolation beyond the boot-time ring-3 test.
- It is not affiliated with Microsoft, Apple or any Linux distribution; branding and design
  are original AURELIA work.

For what does work, see [`STATUS.md`](STATUS.md).
