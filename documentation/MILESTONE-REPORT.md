# AURELIA OS — Milestone Report

**Report date:** 2026-11-06  
**Build:** AURELIA OS 1.0.0 Alpha "Solstice"  
**Scope:** Desktop alpha plus newly added kernel paging, minimal ring-3/syscall and
cooperative kernel-thread foundations. A preemptive scheduler and general process model
remain unfinished.

AURELIA boots as a real BIOS ISO through Limine into a freestanding kernel and reaches a
working graphical desktop. The new paging and ring-3 layers were exercised by a boot-time
program that makes a real syscall and is then stopped by a hardware privilege fault. This
is a foundation, not a claim that applications are isolated.

---

## 1. Files created or completed in this pass

### New architecture files

- `kernel/memory/paging.c`, `kernel/include/aurelia/paging.h`
  - Kernel-owned 4-level page tables, clone of Limine higher-half mappings, user mappings,
    page-table walker, user-range validation and paging self-test.
- `kernel/core/process.c`, `kernel/include/aurelia/process.h`
  - User address-space creation, user test program, contained user exceptions and context
    exit for the boot-time ring-3 test.
- `kernel/core/syscall.c`, `kernel/include/aurelia/syscall.h`
  - EFER/STAR/LSTAR/FMASK setup, write/getpid/exit dispatch, user buffer validation.
- `kernel/arch/x86_64/usermode.S`
  - IRETQ ring-3 entry, SYSCALL entry, kernel context restoration and iretq return.
- `kernel/core/sched.c`, `kernel/include/aurelia/sched.h`,
  `kernel/arch/x86_64/sched_switch.S`
  - Two cooperative kernel threads, context switch, stack setup and validation.

### Integration and fixes

- `kernel/core/main.c` — initializes paging/process/SYSCALL and runs the ring-3 and thread
  self-tests before the desktop.
- `kernel/arch/x86_64/idt.c` — routes ring-3 exceptions to the contained user fault path
  instead of panicking.
- `kernel/arch/x86_64/gdt.c` — removed an invalid `LTR` reload from
  `gdt_set_kernel_stack`; an already-busy TSS cannot be loaded again. Updating TSS.RSP0 in
  memory is sufficient. This bug was found and fixed during the first paging boot.
- `kernel/apps/about.c` — capability matrix now reports paging, ring 3/syscalls and kernel
  threads; row spacing reduced so all ten rows fit.
- `kernel/apps/browser.c` — architecture/security/roadmap documents updated to match the
  new kernel state.
- `README.md`, `documentation/STATUS.md`, `documentation/LIMITATIONS.md`,
  `documentation/MILESTONE-REPORT.md` — revised for the new architecture and limits.

### Earlier desktop work included in this alpha

Settings hit-testing, Store three-column layout, Explorer geometry, real heap sparkline,
System Information sizing, Clock height, wallpaper cache signature invalidation and batched
QEMU `clickat`/`drag` support remain in the build.

---

## 2. Functionality implemented

### Paging and address spaces

- At boot, AURELIA allocates a new PML4 and recursively clones the Limine higher-half page
  tables. Every cloned kernel entry has the user bit cleared.
- The kernel verifies that its own text resolves supervisor-only, switches CR3, and checks
  the active root.
- `paging_create_user_space()` shares the supervisor-only kernel mappings and adds user
  page tables. The boot test maps a code page and stack with U=1; kernel text remains U=0.
- The walker supports 4 KiB, 2 MiB and 1 GiB leaves and validates user buffer ranges for
  syscalls.
- This is not demand paging, swap, COW, unmap/reclaim or a general VMM.

### Ring 3 and syscalls

- `process_init()` enables EFER.SCE and programs SYSCALL MSRs.
- A small machine-code program is copied into a user page. It enters CPL 3 via an IRETQ
  frame, calls `SYS_WRITE` with a user pointer, then executes `CLI`.
- `SYS_WRITE` validates the user range against the active page tables, copies the bytes and
  emits them to the kernel log.
- `CLI` at CPL 3 raises #GP vector 13. The kernel records the fault, discards the user
  context, restores kernel CR3 and resumes the boot path. The desktop then starts normally.
- `SYS_GETPID` and `SYS_EXIT` exist; the test exercises write and the fault path, not exit.
- No general process table, scheduler, ELF loader or app sandbox exists.

### Cooperative kernel threads

- Two 8 KiB kernel stacks are initialized as switch frames.
- `sched_switch` saves callee-saved registers and stack pointers and switches stacks.
- The two threads increment independent counters, yield round-robin four times each, mark
  themselves done, and return to the boot thread. The test validates both counters and
  completion flags.
- This is cooperative context switching only. The PIT does not preempt tasks and no user
  process is scheduled.

### Existing kernel and desktop

- GDT/TSS, IDT, PIC/PIT, PMM, 64 MiB heap, PS/2 keyboard/mouse, RTC, serial and RAMFS.
- Software compositor, window manager, shell, dock, launcher, login/lock, quick settings,
  notifications, widgets and 15 built-in applications.
- The account remains a demo salted-SHA-256 gate with no isolation.

---

## 3. Build results

```sh
python scripts/build.py
```

Final observed result:

```text
linked build\aurelia.elf (~1.72 MiB)
limine BIOS stage 2 installed
ISO: build\aurelia.iso (5.5 MiB)
build OK
```

- ELF: `build/aurelia.elf`, 1722.2 KiB at the captured build.
- ISO: ISO9660 + Rock Ridge + El Torito BIOS at `build/aurelia.iso`.
- No compiler/linker errors after the GDT TSS fix.
- UEFI is not built; the image is BIOS-only.

The build output directory and local toolchain are ignored by `.gitignore`.

---

## 4. Test results

### Environment

QEMU 11.1.0, 1024 MiB RAM, one CPU, 1920×1080 framebuffer, BIOS boot. Serial captured to
`build/serial.log`; screenshots captured through the QEMU monitor.

### Passed boot checks

| Check | Result |
|---|---|
| ISO build and BIOS Limine boot | Pass |
| Kernel-owned page-table creation and CR3 switch | Pass |
| Kernel text resolves supervisor-only | Pass |
| User page U=1 / kernel text U=0 in user address space | Pass |
| Ring-3 entry via IRETQ | Pass |
| `SYS_WRITE` from CPL 3 reaches the kernel | Pass |
| `CLI` at CPL 3 raises vector 13 and is contained | Pass |
| Kernel resumes at CPL 0 and reaches desktop | Pass |
| Cooperative thread switch A=4 / B=4 with stacks restored | Pass |
| Login and desktop after paging switch | Pass |
| All 15 built-in applications launch after paging | Pass (15 serial open events) |
| Window drag, minimize/restore/maximize, Alt+Tab | Pass |
| Settings pane switching and Ivory theme | Pass |
| Store/Explorer/Monitor/System Information layouts | Pass |
| System Information shows all 10 capability rows | Pass (`v10_about_fit.png`) |
| No kernel panic in the verified boot log | Pass |

Representative serial lines:

```text
[ok] paging: kernel-owned 4-level tables at phys 0x4100000 (2 high entries, 11 tables)
[ok] paging: kernel text supervisor-only; user address space available
[ok] isolation: user page U=1, kernel text U=0 in the user address space
[ring3] AURELIA ring 3: write syscall reached the kernel
[ok] ring-3 self-test: syscall write succeeded; CLI raised vector 13 at CPL 3
[ok] ring-3 self-test: user fault contained, kernel resumed at CPL 0
[ok] threads: cooperative context switch A=4 B=4, stacks restored
```

Evidence files include `build/v8_post_paging_desktop.png`,
`build/v10_about_fit.png`, `build/v11_all_apps_post_paging.png`, `build/v4_min.png`,
`build/v4_restore.png`, `build/v4_max.png`,
`build/v4_two.png`, `build/v4_alt.png`, `build/v3_ivory.png`, `build/v2_store.png`,
`build/fix_explorer.png`, `build/fix_monitor.png` and `build/fix_about.png`.

### Test caveats and untested paths

- The final wallpaper-background capture after selecting Dune was inconclusive due lost
  QEMU pointer synchronization; cache invalidation builds and boots but needs a manual
  visual check.
- `SYS_EXIT`, user page faults, multiple user address spaces and user page reclamation are
  not exercised.
- Window close/resize/snap, lock, audio output and shutdown were not re-captured in this
  final pass.
- No real hardware, UEFI, SMP, alternate resolution/RAM, long-soak, OOM, fuzzing or
  automated test suite exists.

---

## 5. Known limitations

The comprehensive list is in [`LIMITATIONS.md`](LIMITATIONS.md). The key remaining items:

- No preemptive scheduler, process table, ELF loader or general user runtime.
- Apps still run in ring 0; ring 3 is a single boot-time test.
- No per-app isolation, full syscall ABI or user fault lifecycle.
- Page tables are kernel-owned but there is no demand paging, swap, COW, unmap/reclaim or
  NX/SMEP/SMAP hardening.
- No FAT32/ATA/NVMe/USB storage; RAMFS is volatile.
- No PCI/ACPI/APIC/SMP, networking, audio codec, GPU acceleration or multi-monitor.
- No packages, updates, recovery environment, SDK or third-party runtime.
- Browser/Store/media apps are reduced-scope implementations, not substitutes for web,
  package and codec stacks.
- Demo authentication is not a multi-user security boundary.
- Manual QEMU verification only; no CI or benchmarks.

---

## 6. Next milestone

The next milestone is a real **preemptive process layer**:

1. Process objects, PID table, kernel/user stacks and an ELF loader.
2. Timer-driven preemption with safe context switching and per-process address spaces.
3. Move built-in applications from ring 0 onto the syscall ABI.
4. Add a block-device path and FAT32 persistence.
5. Then networking, package/update/recovery infrastructure and a public SDK.

Until those are implemented and tested, AURELIA remains a graphical alpha with a verified
kernel paging and ring-3 foundation, not a complete operating system.
