# Syscall interface

All syscalls go through a single interrupt-based entry point handled by
`Syscall::Handler()` in `kernel/cpu/syscall.cpp`. Calling convention: the syscall number is
in `rax`, arguments in `rdi`, `rsi`, `rdx`, `r10`, `r8` (in that order — `r10` instead of
`rcx` because `rcx` is clobbered by the `syscall`/interrupt mechanism, standard x86_64
practice), and the return value comes back in `rax`. Userspace wrappers live in
`apps/libgui/gui.h`/`gui.c` (`sys_*` functions) — apps call those, not raw syscall numbers.

This table reflects what's actually implemented; treat it as generated from
`kernel/cpu/syscall.cpp`, not hand-maintained prose — if it drifts from the code, the code
is right.

| # | Name | Args (`rdi`, `rsi`, `rdx`, `r10`, `r8`) | Returns |
|---|------|------------------------------------------|---------|
| 1 | `sys_print` | `const char* str` | — |
| 2 | `sys_exit` | — | never returns |
| 3 | `sys_write_file` | `const char* path`, `const uint8_t* buf`, `uint32_t size` | `1`/`0` |
| 4 | `sys_get_time` | `DateTime* out` (year/month/day/hour/minute/second, binary not BCD) | `1`/`0` |
| 5 | `sys_read_file` | `const char* path`, `uint8_t* out_buf`, `uint32_t max_size` | bytes copied (`0` if not found) |
| 6 | `sys_get_mem_info` | `uint64_t* total`, `uint64_t* free` | `1` |
| 7 | `sys_get_display_info` | `uint32_t* width`, `uint32_t* height`, `uint32_t* bpp` | `1` |
| 8 | `sys_play_wav` | `const char* path` | `1`/`0` |
| 9 | `sys_set_volume` | `uint32_t percent` (0-100) | `1` |
| 10 | `sys_get_volume` | `uint32_t* out_percent` | `1` |
| 11 | `sys_set_mouse_speed` | `uint32_t percent` (25-300) | `1` |
| 12 | `sys_get_mouse_speed` | `uint32_t* out_percent` | `1` |
| 50 | `sys_create_window` | `const char* title`, `int w`, `int h`, `int x`, `int y`, `{int id; uint64_t fb_vaddr}* out` | window id (`0` on failure) |
| 51 | `sys_update_window` | — (the compositor picks up framebuffer changes automatically every frame; this is currently a no-op reserved for forcing a redraw) | `0` |
| 52 | `sys_get_event` | `int win_id`, `Window::Event* out` | `1` if an event was popped, else `0` |
| 53 | `sys_reload_wallpaper` | — | `1` |
| 54 | `sys_destroy_window` | `int win_id` | `1`/`0` |
| 55 | `sys_get_tasks` | `TaskInfo* buffer`, `int max_count` | number of tasks written |
| 56 | `sys_kill_task` | `uint64_t task_id` | `1`/`0` |
| 57 | `sys_draw_bmp` | `int win_id`, `const char* path`, `int x`, `int y` | `1`/`0` |

Numbers 13-49 are unused gaps, not a typo — new syscalls have generally been added in the
next free block relevant to their subsystem (13-49 is effectively reserved headroom before
the GUI block starting at 50) rather than packed contiguously.

## Notes

- `sys_create_window`'s output struct is two fields packed back to back in a
  caller-provided buffer (`int id` at offset 0, `uint64_t fb_vaddr` at offset 8) rather than
  a shared struct type between kernel and userspace — see the exact layout in
  `kernel/cpu/syscall.cpp` if adding a new field.
- `sys_write_file`/`sys_read_file` go through `VFS::` → `Ext2::` — see
  [`filesystem.md`](filesystem.md) for what's actually backing them on disk.
- Window creation/destruction (`sys_create_window`/`sys_destroy_window`) is wrapped in
  `EnterCritical()`/`ExitCritical()` (`kernel/cpu/critical.h`) because scanning for a free
  window slot and reserving it must be atomic across tasks — two tasks creating windows at
  the same time must not be able to land in the same slot.
