# Multi-viewport window dragging on X11 — repro & measurement tools

Tools used to diagnose two separate defects when dragging a floating window out of the main
window with `ImGuiConfigFlags_ViewportsEnable` on X11 (relates to ocornut/imgui#2117):

1. **A stall** (~40–110 ms, one frame) at the moment the window leaves the host viewport, from
   `glfwCreateWindow()` + GLX context creation.
2. **Wobble while dragging**: the OS window position is updated once per 60 Hz frame, so it
   chases a stale cursor and the error varies frame to frame (and grows over a long drag).

See `NOTES.md` for what is fixed, what is not, and what is still unknown.

## Trying it

Build `examples/example_glfw_opengl3` and use the environment switches (all off by default, so a
plain run reproduces stock behaviour):

| variable | effect |
| --- | --- |
| `IMGUI_LATELATCH=<hz>` | After presenting a frame, re-poll the mouse and re-issue only the dragged window's OS position until just before the next vblank. 500 removes the wobble at 60 fps. |
| `IMGUI_OWNVP=1` | `io.ConfigViewportsOwnViewportOnMove`: create the platform window on mouse-down instead of mid-drag. |
| `IMGUI_GLFW_WINDOW_POOL=<n>` | Backend window pool size. `0` = stock create-on-demand. Default 2. |
| `IMGUI_DIAG=1` | Print viewport events, any frame over 30 ms, and the dragged window's lag behind the mouse. |
| `IMGUI_NOVSYNC=1`, `IMGUI_FPSCAP=<n>` | Run off vsync (superseded by `IMGUI_LATELATCH`, kept for comparison). |

```
cd examples/example_glfw_opengl3
./example_glfw_opengl3                                  # stock: stall + wobble
IMGUI_DIAG=1 IMGUI_GLFW_WINDOW_POOL=0 ./example_glfw_opengl3
IMGUI_OWNVP=1 IMGUI_LATELATCH=500 ./example_glfw_opengl3    # both defects gone
```

After changing `imgui.h`, run `make clean`: the example Makefile has no header dependencies and
stale objects give a bogus assert in `ColorEdit4()`.

## Measuring

Needs `xdotool` (synthetic drags) and X11 headers. Logs land in `logs/`.

```
harness/build.sh                          # standalone instrumented app (harness/main.cpp)
./drive_fast.sh   <log> Small [args]      # drag out of the host and back, ~1000 px/s
./drive_inside.sh <log> Small [args]      # drag that never leaves the host
./drive_long.sh   <log> Small [args]      # 25 s continuous drag (shows the error growing)
./drive_cycle.sh  <log> Small [args]      # 3 out/in cycles with releases
python3 analyze3.py logs/<log>.log        # split by regime: merged / own window over host / off host
python3 analyze4.py logs/<log>.log        # same error, split into quarters of a long drag
```

`[args]` are harness flags: `--hooks` (time every `ImGuiPlatformIO` callback), `--latelatch <hz>`,
`--ownvp`, `--nomerge`, `--novsync`, `--fpscap <n>`, `--prewarm`.

The two drivers below drive the real example instead of the harness, and read their settings from
the environment. `drive_cross.sh` overwrites `examples/example_glfw_opengl3/imgui.ini` with a fixed
layout, because synthetic input needs a predictable window position.

```
IMGUI_DIAG=1 IMGUI_GLFW_WINDOW_POOL=0 ./drive_cross.sh <log>    # pull out of / push into the host
IMGUI_DIAG=1 ./drive_desktop.sh <log>                           # drag around on the desktop
python3 lag2.py logs/<log>.log
```

Reported "jitter" is the deviation from the constant grab offset, i.e. how much the window's
distance from the cursor *changes*; a constant offset is invisible, a varying one is the wobble.
The synthetic pointer moves in 16 px steps at 62.5 Hz, so 16 px is the measurement floor and that
cadence beats against the 60 Hz frame loop — judge the result by eye as well.
