# OxideOS

A 64-bit x86_64 operating system written from scratch in C++, with a GUI styled after
Windows 95/98/2000. Booted via [Limine](https://github.com/limine-bootloader/limine),
developed and tested primarily in `qemu-system-x86_64`.

OxideOS is a hobby project built as a human+AI pair — every subsystem below was written,
debugged, and independently verified (via `tcpdump`, GDB, `debugfs`/`e2fsck`, and headless
QEMU regression scripts) rather than assumed to work from reading the log output alone.

## Goal

The GUI is meant to look and feel like late-90s/early-2000s Windows, but the system should
be genuinely usable, not just a visual pastiche. The long-standing measure of success is a
self-imposed challenge: run OxideOS as a daily driver for 7 days straight (once a browser
is ported — see the roadmap below).

## What works today (09.30.2026)

- **Kernel** (x86_64, own from scratch): GDT/IDT/PIC/PIT, cooperative task scheduler,
  physical + virtual memory management, interrupt-driven drivers.
- **Filesystem**: a real **ext2** driver (block/inode allocation bitmaps, directory
  entries, file read/write/grow/shrink) backing the virtual filesystem layer. Disk images
  are built with genuine `mke2fs`/`debugfs`, so they're readable by any standard Linux
  ext2 tooling, not just by OxideOS itself.
- **Networking**: a full stack built from the hardware up — **RTL8139** NIC driver (PCI
  detection, RX/TX ring buffers, interrupt-driven), Ethernet framing, **ARP**, **IPv4**,
  **ICMP** (ping out to a real gateway), **UDP**, a **DHCP** client (fills in our IP/gateway
  at boot, no more hardcoded addresses), a single-connection **TCP** client, and a minimal
  **HTTP/1.0** client on top of it — each layer independently verified against `tcpdump`
  packet captures and real external processes (not just the kernel's own log output).
- **GUI**: a window manager with Z-ordering, a taskbar with a system tray, alpha-blended
  compositing, 24/32-bit BMP rendering, and a Start Menu that reflects the live contents of
  `/usr/bin`.
- **UI toolkit for apps** ("WinForms-lite", `apps/libgui/widgets.*`): a small `Control`/
  `Form` widget library with real event dispatch (hit-testing + callbacks), not just shared
  drawing helpers — apps declare their controls once and stop hand-rolling click detection.
- **Userspace apps**, each a standalone `.ELF` binary loaded from `/usr/bin` and run in
  Ring 3: Calculator, Notepad, Control Panel (Settings), Calendar, Paint, Task Manager,
  Clock, WinVer (About), and a launcher (Hello).
- **Sound**: WAV playback through an AC97 driver (startup sound, UI feedback sounds).
- **Build tooling**: a fresh clone builds and boots with no manual setup step — Limine
  (bootloader) is bootstrapped automatically from the vendored source on first build — and
  Windows is a first-class target via WSL2 (`scripts\setup_windows.bat` for one-time setup,
  `.bat` wrappers for every script — see "Running on Windows" below).

## Building and running

Requirements: `git`, `cmake`, `gcc`/`ld` (`build-essential` on Debian/Ubuntu), `xorriso`,
`qemu-system-x86_64`, `ImageMagick`, `e2fsprogs` (`mke2fs`/`debugfs`), `nasm`, `mtools`
(the last two build Limine itself from the vendored `limine-12.5.2/` source — see below),
`python3`. `gdb` is only needed for the optional `scripts/gdb_inspect.sh`.

```bash
./scripts/run.sh
```

This builds the kernel and userspace apps, generates an ext2 disk image (`disk.img`) via
`mke2fs`/`debugfs`, converts assets from `assets/` (PNG → BMP), assembles `oxideos.iso`,
and boots it in QEMU. On first run it also bootstraps `limine_dir/` and
`iso_root/boot/limine/` (both gitignored, so a fresh clone starts without them) straight
from the `limine-12.5.2/` source that *is* committed — no manual Limine setup step needed.

For automated, headless verification (no display, used for regression testing after every
change) see `scripts/test_headless.sh` and `scripts/headless_interact.sh` — the latter
drives mouse/keyboard through QMP from a small scenario file, useful for scripted UI
regression without a human at the keyboard.

### Running on Windows

The build toolchain above (`mke2fs`/`debugfs`, `xorriso`, and the freestanding-ELF `gcc`/
`ld` invocations that link userspace apps) is Linux-specific tooling — there's no native
Windows equivalent of it. So the Windows `.bat` files in `scripts/` (one per `.sh` script:
`build.bat`, `make_disk.bat`, `run.bat`, `test_headless.bat`, `screendump.bat`,
`headless_interact.bat`, `gdb_inspect.bat`) are thin wrappers that run the real `.sh`
script inside **WSL2** (Windows Subsystem for Linux), rather than reimplementing the build
in batch — that keeps exactly one real implementation of each script instead of two that
could quietly drift apart. There's one extra one with no `.sh` counterpart,
`setup_windows.bat` — one-time environment setup, see below.

Setup, once:
1. Install WSL2: `wsl --install` (or see
   [learn.microsoft.com/windows/wsl/install](https://learn.microsoft.com/windows/wsl/install)),
   then install a distro (e.g. Ubuntu) from the Microsoft Store, and finish its first-time
   setup (it asks you to pick a Linux username/password — that's separate from Windows).
2. Download **just one file**, `scripts/setup_windows.bat` — you don't have the repo yet at
   this point, so you can't run a script that lives inside it. Either open
   [this raw link](https://raw.githubusercontent.com/Kocurowy96/OxideOS/main/scripts/setup_windows.bat)
   in a browser and "Save As", or from PowerShell:
   ```powershell
   curl.exe -o setup_windows.bat https://raw.githubusercontent.com/Kocurowy96/OxideOS/main/scripts/setup_windows.bat
   ```
   Save it anywhere (e.g. your Downloads folder) — where doesn't matter, see the next step.
3. Run it (double-click it, or `setup_windows.bat` from a Command Prompt in the folder you
   saved it to). It detects whether your default WSL distro uses `apt` (Debian/Ubuntu) or
   `pacman` (Arch) and installs the build toolchain accordingly (`git`, a C/C++ toolchain,
   `cmake`, `xorriso`, `qemu-system-x86_64`, ImageMagick, `e2fsprogs`, `nasm`, `mtools`,
   `gdb`, `python3` — you'll be prompted for your WSL sudo password, that's expected), then
   clones OxideOS into a folder you pick (default: `%USERPROFILE%\OxideOS`). On any other
   distro it skips the automatic install and tells you what to install by hand instead of
   failing on a package manager that isn't there. This exists specifically so a fresh setup
   can't go half-right by someone cloning into the wrong place or skipping a package — one
   script does both steps the same way every time, and downloading only this one file first
   means there's no "clone with the wrong settings" step to get wrong before it even runs.
   Already have a manual WSL setup or a clone? Skip steps 2-3 and just make sure the
   packages above are installed — or run `scripts\setup_windows.bat` from inside your
   existing clone any time to double-check the toolchain or pull the latest changes; it
   detects it's already in a checkout and updates that one instead of cloning a second copy.

Then, from a Windows Command Prompt / PowerShell in the cloned repo root:

```bat
scripts\run.bat
```

What opens: this builds the kernel and apps and the disk image inside WSL, then boots
`oxideos.iso` in `qemu-system-x86_64` — the QEMU window appears on the normal Windows
desktop (WSL2/WSLg forwards Linux GUI apps automatically on current Windows 11/10
builds), exactly like running `./scripts/run.sh` directly in a Linux terminal.
`scripts\test_headless.bat` and `scripts\screendump.bat` instead run QEMU with
`-display none` (as their `.sh` counterparts do) and print PASS/FAIL or write a
screenshot file — nothing opens on screen for those.

Each `.bat` file just calls `wsl bash -lc "./scripts/<name>.sh ..."`, forwarding any
arguments straight through, so paths for `headless_interact.bat`/`gdb_inspect.bat` should
be given as WSL sees them (plain relative paths from the repo root, or `/mnt/c/...` for a
Windows path) — they are not translated. **Not tested on a real Windows machine** — the
`.sh` scripts themselves are unchanged and already verified, but running them through
these wrappers hasn't been independently confirmed on Windows yet; if something doesn't
work as documented here, please open an issue.

## Project structure

```
kernel/       Kernel source: cpu/ (GDT/IDT/ISR/syscalls), drivers/, fs/ (VFS + ext2),
              mem/ (PMM/VMM), net/ (Ethernet/ARP/IP/ICMP), gui/ (compositor/window
              manager), proc/ (scheduler)
apps/         Userspace .ELF applications, plus apps/libgui/ (the shared GUI toolkit)
scripts/      Build, disk image, and QEMU test/automation scripts
assets/       Wallpapers, icons, cursors, sounds (source .png/.wav, converted at build time)
CoworkWithClaude/  Planning docs, task tracker, and working notes for the human+AI workflow
```

## Documentation

In-depth write-ups of how individual subsystems actually work (networking stack, ext2
driver, GUI/widget architecture, syscall interface, boot process, build tooling) live in
[`docs/`](docs/) — start at [`docs/README.md`](docs/README.md).

## Collaboration

This project is developed as a human+AI duo:

- **Code** (kernel, drivers, GUI, syscalls, apps, tooling) — written jointly with Claude
  (Anthropic), with every non-trivial change independently verified rather than taken on
  faith.
- **Direction, assets, and hands-on testing** —
  [Kocurowy96](https://github.com/Kocurowy96): wallpapers, icons, sounds, artwork, and
  real-machine/QEMU testing.
