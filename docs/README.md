# OxideOS Documentation

This is the deep-dive documentation for OxideOS — how each subsystem actually works,
written for whoever picks up this codebase next (including a future session of the two of
us). The top-level [`README.md`](../README.md) is the pitch; these pages are the reference.

Every page here describes what is **actually implemented and verified**, not the aspirational
plan. Forward-looking design decisions and in-progress phases live in
[`../CoworkWithClaude/`](../CoworkWithClaude/) (`PLAN_*.md`, `TASKS.md`) instead — once a
feature lands and is verified, its real behavior should be reflected here.

## Pages

- [`architecture.md`](architecture.md) — boot sequence, kernel layout, memory management,
  the task scheduler, and how Ring 0/Ring 3 fit together.
- [`filesystem.md`](filesystem.md) — the ext2 driver: on-disk layout, allocation, the VFS
  layer above it, and how disk images are built/inspected from the host side.
- [`networking.md`](networking.md) — the network stack from the RTL8139 driver up through
  Ethernet/ARP/IP/ICMP, byte-order/checksum details, and how it's verified against
  ground-truth tools.
- [`gui.md`](gui.md) — the compositor/window manager, and the `Control`/`Form` widget
  toolkit apps are built on ("WinForms-lite").
- [`syscalls.md`](syscalls.md) — the full userspace/kernel syscall interface (calling
  convention, syscall numbers, arguments).
- [`tooling.md`](tooling.md) — the build/run/test scripts, headless QEMU regression
  testing, and the independent-verification tools (QMP, GDB, `tcpdump`, `debugfs`) used to
  cross-check the kernel's own claims about itself.

## A note on verification

A recurring theme across this codebase: **the kernel's own log output is never treated as
proof that something works.** Every non-trivial subsystem here was checked against an
external, independent source of truth — a `tcpdump` capture for anything on the wire, the
QEMU monitor (`info pci`, raw I/O-port reads) for hardware/register state, `debugfs`/
`e2fsck` for on-disk filesystem state, GDB for in-kernel state. If a doc page below
describes something as "verified", that's what it means.
