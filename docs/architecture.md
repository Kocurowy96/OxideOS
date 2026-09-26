# Architecture

## Boot sequence

OxideOS boots via [Limine](https://github.com/limine-bootloader/limine) (BIOS El Torito,
protocol revision 3). Limine hands control to `_start()` in `kernel/main.cpp` already in
64-bit long mode, with a framebuffer request already answered — there is no separate
16/32-bit bootstrap stage to maintain in this codebase.

`_start()` brings subsystems up roughly in this order (see `kernel/main.cpp` for the exact,
current sequence — this is the shape of it, not a line-by-line mirror):

1. Serial port (all kernel logging goes here — `SerialPort::WriteString`, visible via
   QEMU's `-serial` redirection).
2. `GDT::Init()`, `IDT::Init()`, `PIC::Init()` — segment/interrupt descriptor tables, PIC
   remapped off the BIOS default vectors.
3. `PMM::Init()` — physical memory bitmap allocator, sized from the Limine memory map.
4. `VMM::Init()` — paging; a TSS stack is set up for Ring 3 → Ring 0 transitions.
5. `Framebuffer::Init()` — from the Limine framebuffer response; nothing below this point
   can render if this fails, so a missing framebuffer halts the boot (`hcf()`).
6. Input drivers (`PS2` keyboard/mouse), `ATA::Init()`, then `VFS::Init()` (which now
   delegates to `Ext2::Init()` — see [`filesystem.md`](filesystem.md)).
7. `PCI::Init()`, then PCI-discovered device drivers: `AC97::Init()`, `RTL8139::Init()`
   (see [`networking.md`](networking.md)).
8. `Scheduler::Init()`, then the first two tasks are created: one that loads and jumps into
   `/usr/bin/HELLO.ELF` in Ring 3, and one that runs the desktop compositor loop.
9. `PIT::Init(100)` (100 Hz tick), interrupts enabled (`sti`), and the boot thread parks in
   an idle `hlt` loop — from here on, everything runs as scheduled tasks.

## Memory management

- **Physical**: `PMM` (`kernel/mem/pmm.*`) is a bitmap allocator over all usable memory
  reported by Limine. `AllocatePage(s)`/`FreePage(s)` operate in 4 KiB pages;
  `GetTotalMemory`/`GetFreeMemory` back the RAM figures shown in WinVer and the Control
  Panel's System tab.
- **Virtual**: `VMM` (`kernel/mem/vmm.*`) walks/builds standard x86_64 4-level page tables
  (`PAGE_PRESENT`/`PAGE_WRITABLE`/`PAGE_USER`/`PAGE_LARGE` flags). `MapPage` is the only
  entry point apps/kernel code actually call; `SwitchPML4` exists for switching the active
  address space but nothing currently calls it with more than one PML4 (see the isolation
  gap below).
- **Important known gap**: there is currently **one shared address space for every task**,
  not one page table per process. A wild write from one Ring 3 app can corrupt another
  app's memory undetected. Giving every task its own PML4 (built on the existing
  `VMM::SwitchPML4`) is tracked as a pre-real-hardware priority — see the roadmap in
  `CoworkWithClaude/`.

## Scheduling

`Scheduler` (`kernel/proc/sched.*`) is a simple, cooperative-via-timer-interrupt round-robin
scheduler. Each `Task` carries its own saved `Registers` (built by `kernel/cpu/isr.cpp`'s
interrupt entry/exit path) and its own kernel stack (`kernel_stack_top`) — needed because a
Ring 3 task can be interrupted (timer, syscall) at any point and the kernel must not run on
that task's user stack. `Scheduler::Schedule()` is invoked from the PIT interrupt handler
and returns the `Registers*` of the task to resume. `GetTasks`/`KillTaskById` back the Task
Manager app.

## Ring 0 / Ring 3 boundary

The window manager/compositor, input drivers, filesystem, and network stack all run in
Ring 0 (in-kernel) for simplicity and responsiveness — there is no microkernel-style driver
isolation in this codebase today. Userspace (`apps/*.ELF`) runs in genuine Ring 3, reaching
the kernel only through the syscall interface (`kernel/cpu/syscall.cpp`,
[`syscalls.md`](syscalls.md)) via `int` (see `kernel/cpu/isr.cpp`'s syscall dispatch and
`SwitchToUserMode` in `sched.h`). GUI windows are still real Ring 3 memory: each window's
framebuffer is a physically-allocated, page-mapped region the app writes into directly
(`sys_create_window` maps it at a fixed per-window virtual address), and the Ring 0
compositor blits it into the screen framebuffer every frame.

## Interrupts

`kernel/cpu/isr.cpp` dispatches hardware interrupts (`int_no` 32–47 after PIC remapping):
32 = PIT timer (drives scheduling), 33 = PS/2 keyboard, 44 = PS/2 mouse, and a
dynamically-assigned line (read from PCI config space, not hardcoded) for the RTL8139 NIC —
see `RTL8139::GetIrqLine()`. Exceptions (divide-by-zero, page fault, etc.) fall through to
the kernel panic screen (`kernel/gui/osod.cpp`, "OSOD" — a full-screen dump of the faulting
registers, not just a serial log line, so a crash is visible even without a serial capture).
