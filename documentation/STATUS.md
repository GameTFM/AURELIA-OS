# AURELIA OS — Legacy Kernel Implementation Status

> **Legacy research track.** This document describes the earlier bespoke AURELIA kernel, not the current Ubuntu 26.04 + KDE Plasma distribution. The shipping image profile is documented in [`../distro/README.md`](../distro/README.md). The legacy kernel still builds with `python scripts/build.py` and remains intentionally preserved; it is not the distro base.

**Version:** 1.0.0 Alpha "Solstice"  
**Target:** x86_64 PC, BIOS/El Torito, tested under QEMU  
**Last verified:** 2026-11-06

AURELIA is a real bootable operating system. Limine loads the AURELIA kernel; the kernel
brings up CPU tables, interrupts, physical and virtual memory, drivers, a filesystem, a
software compositor, a window manager and a desktop shell. This is an alpha with major
architectural layers still missing, not a production OS. See
[`LIMITATIONS.md`](LIMITATIONS.md).

---

## Build identity

| Property | Value |
|---|---|
| Kernel | Freestanding C11 + GAS x86_64 assembly, higher half at `0xffffffff80000000` |
| Bootloader | Limine 12.9.6, BIOS El Torito only (no UEFI entry) |
| Compiler / linker | Zig 0.17.1 bundled clang/lld (`zig cc`) |
| Build driver | `scripts/build.py`; POSIX wrappers in `Makefile` |
| ISO builder | `tools/mkiso.py` (ISO9660 + Rock Ridge + El Torito) |
| Graphics | Limine linear framebuffer, software compositor |
| Input | PS/2 keyboard and PS/2 three-byte mouse |
| Test machine | QEMU 11.1.0, 1024 MiB RAM, 1920×1080 |
| Last build | `build/aurelia.iso` ~5.5 MiB, ELF ~1.72 MiB, `build OK` |

---

## Boot path

1. BIOS boots the El Torito ISO and Limine loads the higher-half kernel.
2. Serial, GDT/TSS, IDT, PIC remap, PIT at 1000 Hz, PMM, 64 MiB heap and RAMFS initialize.
3. `paging_init()` clones the Limine higher-half mappings into a kernel-owned PML4, clears
   the user bit, verifies kernel text resolves supervisor-only and switches CR3.
4. `process_init()` programs EFER.SCE, STAR, LSTAR and FMASK for SYSCALL.
5. A ring-3 test process runs with its own user address space, performs `SYS_WRITE`, then
   executes a privileged `CLI`; the #GP is contained and the kernel resumes.
6. Two cooperative kernel threads run a context-switch self-test with saved stacks.
7. PS/2 input, RTC, boot artwork, login, lock and the desktop shell start.

There is no separate recovery environment and no UEFI boot path.

---

## Subsystem matrix

| Subsystem | State | Detail |
|---|---|---|
| BIOS ISO / Limine boot | **Working** | Limine BIOS stage 2, serial boot trace, desktop reached |
| GDT / TSS / IDT | **Working** | Seven GDT entries, TSS, exception/IRQ stubs |
| Interrupts | **Working** | 8259 PIC, IRQ0 timer, IRQ1 keyboard, IRQ12 mouse |
| Physical memory | **Working** | Limine memory map + bitmap allocator; 1017 MiB usable in test VM |
| Kernel heap | **Working** | 64 MiB boundary-tag allocator with split/coalesce and self-test |
| Kernel-owned paging | **Working** | 4-level PML4 cloned by the kernel, higher-half supervisor mappings, CR3 switch self-check |
| User address spaces | **Partial** | User code/stack pages mapped into a fresh PML4 for the boot test; no ELF loader, unmap, reclaim or per-app spaces |
| Ring 3 | **Partial** | One synchronous boot-time process enters CPL 3 via IRETQ and faults back into the kernel; desktop remains ring 0 |
| System calls | **Partial** | `SYS_WRITE` (validated user buffer), `SYS_GETPID`, `SYS_EXIT`; no stable ABI or general user runtime |
| Process table / scheduler | **Not implemented** | No PID table, run queue, preemption or ELF process loader |
| Kernel threads | **Partial** | Two cooperative stacks switch round-robin in a self-test; no timer preemption |
| ACPI / PCI / APIC / IOAPIC / SMP | **Not implemented** | No enumeration or configuration; CPUID capability only |
| RTC | **Working** | CMOS date/time shown by shell and apps |
| Keyboard / mouse | **Working** | PS/2 set-1 keyboard, three-byte mouse; no wheel/USB |
| Compositor | **Working** | Clipped canvas, atlas text, rounded surfaces, shadows, back buffer |
| Window manager | **Working core** | Move, resize, focus/z-order, minimize, maximize/restore, close, snapping, Alt+Tab, four workspaces |
| Multi-monitor | **Not implemented** | Single framebuffer |
| RAID/FAT32/ATA/NVMe/USB storage | **Not implemented** | RAM filesystem only; no block device layer |
| Persistence | **Not implemented** | RAMFS rebuilt each boot; ISO read-only |
| File permissions | **Not implemented** | No enforced ownership or ACLs |
| User accounts | **Partial** | One demo user, salted SHA-256 verifier, login/lock; no account management or isolation |
| Audio | **PC speaker only** | PIT channel 2 tones; no HDA/AC'97 or codecs |
| Networking | **Not implemented** | No Ethernet/Wi-Fi driver, IP, DHCP, DNS or sockets |
| Package manager / updates | **Not implemented** | Store lists built-ins only |
| Recovery environment | **Not implemented** | No separate recovery image or rollback |
| SDK / third-party runtime | **Not implemented** | `sdk/` is reserved scaffolding |
| Automated tests / CI | **Not implemented** | Manual QEMU serial and screenshot verification |

---

## Desktop shell

Implemented in the kernel:

- Boot artwork, login, lock, logout and power menu.
- Midnight/Ivory appearance, five accents, four wallpapers, density, brightness and
  reduced-motion preference; theme preferences saved to `/Configuration/theme.conf` in RAMFS.
- Top bar with RTC clock, workspace indicators, active-window title and status pill.
- Centered dock with 15 app tiles, running indicators and launcher (`F1`/Super).
- Searchable application grid, quick settings, notification cards, desktop clock and a
  live memory/system widget.
- Window manager with independent windows, drag/resize, traffic lights, dock restore, snap
  zones, Alt+Tab and four workspaces.

The light theme and theme selection were exercised with real clicks. Wallpaper cache
invalidation is now tied to a theme signature; see the test caveat in the milestone report.

---

## Built-in applications

| Application | State |
|---|---|
| Terminal | Working shell with filesystem/system/diagnostic commands |
| Explorer | Working navigation, tabs, filter, create/rename/delete/open; no clipboard or properties |
| Settings | Seven live panes: Personalization, Display, Sound, Network, Storage, Accessibility, About |
| Notes | Working RAMFS-backed text notes |
| Text Editor | Working plain-text editor for RAMFS files |
| Calculator | Integer/fixed-point expression calculator (kernel has SSE disabled) |
| System Monitor | Real heap, PMM, VFS, timer, display and window-table telemetry |
| Clock / Calendar | RTC-driven clock and month view |
| Store | Lists the 15 real built-ins; no installation exists |
| Browser | Offline local-document reader; no web engine/network |
| Image Viewer | Procedural AURELIA artwork; no image codecs |
| Tones | PC-speaker melody player; no recorded audio |
| Diagnostics | Kernel log, boot trace and capability checks |
| System Information | CPUID identity, memory, display, kernel and 10-row capability matrix |

All applications currently run in ring 0. Explorer copy/cut/paste/move/properties/previews
and the remaining Settings panes are not implemented. See
[`LIMITATIONS.md`](LIMITATIONS.md).

---

## Milestone status

| Milestone | State | Notes |
|---|---|---|
| 1 — Toolchain, boot, framebuffer, serial | **Complete** | BIOS ISO, Limine, higher-half kernel, serial, 1920×1080 framebuffer |
| 2 — GDT, IDT, interrupts, memory, input | **Complete** | PMM, PIC/PIT, keyboard, mouse, RTC, panic path |
| 3 — Heap, paging, HAL, multitasking | **Partial** | Heap, HAL/timer, kernel-owned paging and a cooperative context-switch test; no preemptive scheduler or process table |
| 4 — Filesystem, storage, syscalls, user mode | **Partial** | RAMFS + minimal SYSCALL/LSTAR ring-3 test; no FAT32/storage driver, general process model or user apps |
| 5 — Graphics, compositor, WM, input | **Complete for current scope** | Compositor and WM verified interactively |
| 6 — Desktop environment | **Mostly complete** | Shell, dock, launcher, quick settings, notifications; hardware-status tray entries remain absent |
| 7 — Built-in applications | **Mostly complete** | 15 apps; Explorer clipboard and several specified apps remain |
| 8 — Users, security, networking, packages | **Partial** | Demo authentication only; no isolation, account model, networking or package service |
| 9 — SDK, developer tools, updates, recovery | **Not started** | Reserved directories only |
| 10 — Performance, testing, docs, release | **In progress** | Docs and repeatable manual tests; no CI, benchmarks or hardening audit |

---

## Verification evidence

Final build:

```text
python scripts/build.py
  linked build\aurelia.elf (~1.72 MiB)
  limine BIOS stage 2 installed
  ISO: build\aurelia.iso (5.5 MiB)
  build OK
```

Guest serial evidence:

```text
[ok] paging: kernel-owned 4-level tables at phys 0x4100000 (2 high entries, 11 tables)
[ok] paging: kernel text supervisor-only; user address space available
[ok] isolation: user page U=1, kernel text U=0 in the user address space
[ring3] AURELIA ring 3: write syscall reached the kernel
[ok] ring-3 self-test: syscall write succeeded; CLI raised vector 13 at CPL 3
[ok] ring-3 self-test: user fault contained, kernel resumed at CPL 0
[ok] threads: cooperative context switch A=4 B=4, stacks restored
[ok] entering desktop event loop
```

Interactive checks:

- Login accepted the demo account and entered the desktop after the new kernel changes.
- All 15 applications launched after the paging switch; Store, Explorer, Monitor and
  System Information layouts were corrected and reviewed.
- Terminal drag, Settings minimize/restore/maximize, two-window Alt+Tab and Settings pane
  switching were exercised.
- System Information (`build/v10_about_fit.png`) shows all 10 capability rows without
  clipping after the paging/ring-3/thread updates.
- Fresh desktop evidence: `build/v8_post_paging_desktop.png`; the post-paging app sweep
  is captured in `build/v11_all_apps_post_paging.png` and the serial open log.

Caveat: the final attempt to capture a changed wallpaper background after selecting Dune
was inconclusive because rapid QEMU relative-mouse injection lost pointer synchronization.
The cache fix builds and boots; manual click-through is recommended. Window resize/snap/
close, lock, audio output and shutdown were not individually re-captured in this pass.

---

## Next milestone

The next work is a genuine **preemptive process layer**, not more desktop surface:

1. Process/task objects, PID table, user stacks and an ELF loader.
2. Timer-driven preemption with per-task address spaces and safe context switches.
3. Move applications out of ring 0 onto the syscall ABI.
4. A block-device path and FAT32 persistence.
5. Then networking, package management, updates/recovery and the SDK.

Until those exist, AURELIA remains a graphical alpha with a verified but narrow ring-3
foundation.
