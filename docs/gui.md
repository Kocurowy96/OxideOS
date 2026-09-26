# GUI

## Compositor / window manager (`kernel/gui/compositor.cpp`)

Runs entirely in Ring 0, as one of the two boot tasks (`DesktopTask` in `kernel/main.cpp`).
Each frame, `Compositor::Render()`:

1. Cleans up windows marked `pending_remove` (set by `sys_exit`/`sys_destroy_window` — the
   actual free happens here, between frames, never mid-render).
2. Draws the wallpaper, then every `Window` back-to-front in Z-order (`windows[]`,
   `MAX_WINDOWS = 10`), blitting each window's own framebuffer into the screen framebuffer.
3. Handles the taskbar, system tray, and the Start Menu — the Start Menu's contents are
   **not** a fixed list: it's read live from `/usr/bin` via `VFS::ListDirectory` every time
   it's opened (`RefreshStartMenu`), so dropping a new `.ELF` into `/usr/bin` makes it appear
   without any GUI code changes.
4. Draws the mouse cursor last, on top of everything.

Click detection samples mouse-button state once per render pass
(`mouse_clicked = mouse_left && !prev_mouse_left`) rather than reacting to a hardware
edge interrupt. **Known limitation**: with several windows open at once, the render pass
takes longer, which lowers the sampling rate and makes it easier for the physical
down→up edge of a very fast click to be missed entirely between samples — observed under
scripted, ~0.15s-cadence automated clicks with 4-5 windows open; a real human's slower,
less precisely-timed click likely doesn't hit this in practice. Not fixed, just noted for
awareness — see `CoworkWithClaude/TASKS.md`.

### Windows and events (`kernel/gui/window.h`)

Each `Window` owns a physically-allocated, page-mapped framebuffer that the owning Ring 3
app writes into directly (mapped at a fixed per-window virtual address by
`sys_create_window`), plus a small ring buffer of `Event`s (`MouseClick`/`KeyPress`) that
the app drains via `sys_get_event`. The compositor is the producer of these events; the
app's own event loop (`gui_form_run`, see below) is the consumer.

## The app-side toolkit: `Control`/`Form` ("WinForms-lite")

`apps/libgui/widgets.h`/`widgets.c` — a small, real widget library used by every app that
has more than a static display (Calculator, Settings, Calendar, WinVer, Notepad, Task
Manager). The design choice that matters: **it dispatches events, not just draws** — apps
declare `Control`s once and never write their own click hit-testing loop.

```c
struct Control {
    int x, y, w, h;
    char text[64];
    ControlRenderFn render;   // draws itself
    ControlClickFn on_click;  // called by Form on a hit, may be NULL
    void* user_data;
};

struct Form {
    ...
    Control controls[FORM_MAX_CONTROLS]; // 40
    FormTickFn on_tick;  // called once per gui_form_run loop iteration, independent of clicks
    FormKeyFn on_key;    // called on GUI_EVENT_KEY_PRESS
    int active_tab;      // used by gui_form_add_tab-based controls
};
```

`gui_form_run()` is the shared loop every migrated app's `_start()` ends with: pop an event
→ hit-test against `controls[]` (first match wins, loop then `break`s) → call `on_click` →
repaint → `sys_update_window`. It also handles `GUI_EVENT_CLOSE` (closing via the titlebar
`X`) by default — one migrated app (the Calculator, during the Phase 1→2 migration) had its
`X` button silently do nothing before this became a shared default, fixed as a side effect
of unifying the dispatcher rather than as a targeted fix.

### Built-in control styles vs. local, per-app renders

The library ships a few generic controls (`gui_form_add_button`/`add_label`/`add_tab`/
`add_thin_button`), but several apps have controls with genuinely unique looks (the
Calculator's display, WinVer's banner, Calendar's day cells, Notepad's whole text area) —
those stay as one-off `ControlRenderFn`s local to the app rather than being forced into the
shared library. The rule of thumb this codebase follows: **a style becomes a shared
`widgets.c` helper only once at least three independent apps have converged on it by
coincidence** (this happened once — `gui_form_add_thin_button`, after WinVer's OK button,
Settings' `-`/`+`/"test sound" buttons, and Task Manager's "End task" button all
independently arrived at the same 1px-border/`0xC0C0C0` look). Two apps sharing a look is
treated as coincidence, not yet a pattern worth abstracting.

### Text input without a dedicated "text field" control

Calendar's notes area and Notepad's whole document are both, in effect, cursor-aware text
fields — but neither required a new formal control type in the library. `Form::on_key` (a
simple, optional keyboard callback) plus ordinary local rendering logic in the app itself
turned out to be enough; a heavier, reusable "text field" widget was anticipated in early
planning but never actually became necessary in practice.

### Dynamic content instead of a "list" control

Task Manager's task list and Settings' per-tab content are both rebuilt on demand
(`RebuildTaskList`/`RebuildContent`/`RebuildDayGrid` in Task Manager/Settings/Calendar
respectively): `Form::control_count` is truncated back to a fixed "static controls end
here" index, then the controls for the current state are re-added fresh. Like the text
field case above, an anticipated "list/table" control type turned out to be unnecessary —
the existing dynamic-rebuild pattern covered it.
