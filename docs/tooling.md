# Tooling

## Build & run

- `scripts/build.sh` — builds the kernel (`cmake`/`make`, producing `kernel.elf`) and
  compiles/links every app in `apps/` into its own freestanding `.ELF` binary (no libc;
  everything from `sys_*` wrappers to string helpers is written per-app or in
  `apps/libgui/`).
- `scripts/make_disk.sh` — builds `disk.img` via `mke2fs -t ext2` + a single `debugfs -w -f`
  batch command file (writes every app `.ELF` into `/usr/bin`, assets from `assets/` after
  PNG→BMP conversion, etc.) — see [`filesystem.md`](filesystem.md). Always rebuilds the
  image from scratch; there's no incremental-update path, because `mke2fs` on a small image
  is fast enough that one isn't needed.
- `scripts/run.sh` — the normal interactive entry point: build → make disk → assemble
  `oxideos.iso` (Limine + `xorriso`) → boot in QEMU with a display.

## Headless / automated testing

Everything below runs **without** a display (`-display none`), which is what makes them
usable for unattended regression testing after every change, including from an agent with
no monitor to look at.

- `scripts/test_headless.sh` — boots the built image, waits, greps the serial log for a
  known-good marker line (currently `"Ext2: Initialized successfully."`), and reports
  PASS/FAIL. This is the fast smoke test run after nearly every change.
- `scripts/headless_interact.sh <scenario> [out_dir] [boot_wait_s]` — drives real
  mouse/keyboard input into a running headless QEMU instance via the QMP protocol
  (`scripts/qmp_client.py`/`qmp_input.py`), and can take screenshots (`qmp_screendump.py`)
  at any point. Scenario files are one action per line:

  ```
  move X Y
  click left|right|middle [X Y]
  scroll up|down [amount]
  key <QEMU QKeyCode>
  type some text
  wait SECONDS
  shot filename.png
  ```

  This is how UI changes get verified end-to-end (e.g. "open the Calculator from the Start
  Menu, click 7 + 3 =, confirm the display shows 10, confirm the result is pixel-identical
  to before the change") without a human driving the mouse.

  **Coordinates are derived from the code, not guessed**: control positions come from the
  window's `sys_create_window` origin plus each `Control`'s `x`/`y`/`w`/`h` as declared in
  the app's `_start()` (see [`gui.md`](gui.md)), plus the fixed titlebar height (20px) and
  border (2px) the compositor adds. Screen resolution is auto-detected via QMP `screendump`
  (`qmp_input.py`'s `detect_screen_size()`) rather than hardcoded — a hardcoded value
  previously drifted between environments (1280x720 on one machine, 1280x800 in a cloud
  container) and caused every click to land at a silently wrong Y coordinate.

- `scripts/screendump.sh [out.png] [wait_s]` — single screenshot after a boot delay, no
  interaction. Useful for a quick "does it still boot and look right" check.
- `scripts/gdb_inspect.sh <gdb_command_file> [wait_s]` — attaches GDB to QEMU's built-in
  stub (`-s`, TCP :1234) against an already-booted (not `-S`) guest, to inspect
  registers/backtraces/memory at a point in time — e.g. confirming a specific in-kernel
  boolean actually flipped when a bug looked like it might be elsewhere. Deliberately run
  **without** `-enable-kvm` (the GDB stub is fully reliable under TCG software emulation;
  under KVM it can be limited depending on the QEMU/host kernel version).

## Independent verification tools (not part of this repo, but part of the workflow)

These aren't OxideOS scripts, but they're a core part of how changes here get verified —
see the "Verifying..." sections in [`filesystem.md`](filesystem.md) and
[`networking.md`](networking.md):

- `tcpdump -r net_dump.pcap` — reads the packet capture QEMU produces via
  `-object filter-dump,...` (already wired into `run.sh`/`test_headless.sh` when the
  RTL8139 device is present) to confirm what actually went out/came in on the wire.
- QEMU monitor (`-monitor unix:sock,server,nowait`, driven with `socat` or similar) —
  `info pci` for BAR/IRQ assignment, `i /1xb <port>` / `i /2xw <port>` for raw I/O-port
  reads, independent of anything the driver itself logs.
- `debugfs`/`e2fsck` on `disk.img` — read files or check filesystem consistency directly,
  independent of the in-kernel ext2 driver.

## Why so much of this exists

A recurring failure mode this tooling is built to catch: the kernel's own serial log
*claiming* something worked isn't proof it did — a bug in the driver can produce a
plausible-looking log line for the wrong reason. Every subsystem in this codebase was
checked against at least one of the tools above before being considered done, not just
"boots without a panic and the log looks right."
