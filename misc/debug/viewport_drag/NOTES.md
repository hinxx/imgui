# Notes: rough edges and open questions

Measured on Linux/X11, XFCE (xfwm4, compositing on), GLFW 3.3.10, Mesa Intel HD 630, single
1920x1080 60 Hz monitor. Nothing here was tested on Windows, macOS, Wayland, other WMs, other
drivers, or multiple monitors.

## Rough edges in the current changes

- **Late latching lives in the example's main loop**, not in Dear ImGui or the backend, so no other
  app gets it by copying a flag. Only the app knows when it has idle time before the next vblank,
  so a core version would probably be an opt-in helper the app calls after presenting.
- **The latch assumes 60 Hz**: it uses a fixed `1000/60 - 5 ms` budget. It should come from the
  monitor refresh rate or a measured frame time. With the current constant, ~1% of frames (19 of
  ~2000 in a 25 s drag) still overran their vblank.
- **The latch only covers moving a window.** Resize-dragging an edge still updates once per frame
  and was not measured.
- **The first platform window creation still costs 40–77 ms**, and `io.ConfigViewportsOwnViewportOnMove`
  only moves that cost onto the mouse-down. It cannot be pre-paid: X/GLX allocates the framebuffer
  when the window is *mapped*, so pre-sizing and pre-binding a hidden pooled window ahead of time
  changed nothing (verified). The pool is drained by minimize/restore, so the cost returns.
- **Keeping a pooled window mapped but hidden does not work on xfwm4**: parking it off-screen is
  clamped back to 0,0 and the WM lists it in the taskbar and alt-tab.
- **Pooled windows live for the process lifetime** (2 windows + GL contexts by default).
- **`ConfigViewportsOwnViewportOnMove` makes the dragged window WM-moved for the whole gesture.**
  Without late latching it is *worse* than stock while the window is still over the host window
  (error changed on 44% of frames vs 15%). The two changes belong together.
- **Pool reuse for a topmost (tooltip) viewport is untested.** It is handled by `glfwSetWindowAttrib()`,
  and on GLFW < 3.3, where that function does not exist, reuse is restricted to windows whose
  attributes already match — also untested.
- **The example Makefile has no header dependencies.** After the `ImGuiIO` field was added, stale
  objects produced a bogus `IM_ASSERT` in `ColorEdit4()`. `harness/build.sh` guards against this by
  wiping its objects when `imgui.h` is newer; the example Makefile does not.

## Worth investigating

- `Platform_GetWindowPos/GetWindowFocus/GetWindowMinimized` are called per viewport per frame and
  each is an X round trip: spikes of 1–7 ms were common in the logs. Caching them per frame may be
  worth more than it sounds.
- `ImGui_ImplGlfw_NewFrame()` cost 22–43 ms on the frame *after* a viewport was destroyed. Not
  investigated; also X round trips.
- Merge-back is refused when another viewport with a platform window overlaps where the window is
  dropped (the `viewport_obstructing` loop in `UpdateTryMergeWindowIntoHostViewport()`). This is
  deliberate, but it makes behaviour look random to a user: dropping a window behind a floating one
  leaves it as a separate OS window permanently.
- Whether either defect exists on Windows and macOS, and on other Linux WMs. The stall is X11/GLX
  window+context creation, so a cheaper platform would show a smaller stall; the wobble is
  once-per-frame position updates, which is platform-independent in principle.
- Whether `imgui_test_engine` could assert on drag latency, so this does not regress silently.
- Turning off the XFCE compositor removes the compositor's contribution (the OS window then lands
  exactly where ImGui asked, every frame) but does *not* fix the wobble, because the remaining error
  is ImGui sampling the mouse once per frame. Worth re-checking on a compositor that applies moves
  synchronously.
