#!/bin/bash
# drive_desktop.sh <logname>  -- put "Hello, world!" on the desktop, then oscillate it there and log lag.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"          # dear imgui repository root
D="$HERE/logs"; mkdir -p "$D"
REPRO="$HERE/harness/repro"
APPDIR="$ROOT/examples/example_glfw_opengl3"
LOG="$D/$1.log"; rm -f "$LOG"
( cd "$APPDIR" && ./example_glfw_opengl3 > "$LOG" 2>&1 & )
for i in $(seq 1 60); do W=$(xdotool search --name "^Hello, world!$" 2>/dev/null | head -1) || true; [ -n "$W" ] && break; xdotool sleep 0.2; done
[ -n "$W" ] || { echo "Hello window has no OS window"; exit 1; }
xdotool sleep 1.5
geom() { xdotool getwindowgeometry $W | sed -n '2p' | sed 's/.*Position: \([0-9-]*\),\([0-9-]*\).*/\1 \2/'; }
read PX PY < <(geom); echo "start pos: $PX,$PY"
xdotool windowactivate --sync $W; xdotool sleep 0.5
# 1) move it onto the desktop, left of the host window
GX=$((PX+80)); GY=$((PY+10))
xdotool mousemove $GX $GY sleep 0.4 mousedown 1 sleep 0.4
x=$GX; while [ $x -gt 380 ]; do x=$((x-24)); xdotool mousemove $x $GY sleep 0.016; done
xdotool sleep 0.4 mouseup 1 sleep 1.5
read PX PY < <(geom); echo "desktop pos: $PX,$PY"
xdotool windowactivate --sync $W; xdotool sleep 0.5
# 2) MEASURED: oscillate left-right entirely on the desktop, ~1000 px/s, twice
GX=$((PX+80)); GY=$((PY+10))
CMD=(mousemove $GX $GY sleep 0.4 mousedown 1 sleep 0.4)
for pass in 1 2; do
  x=$GX; while [ $x -lt $((GX+300)) ]; do x=$((x+16)); CMD+=(mousemove $x $GY sleep 0.016); done
  CMD+=(sleep 0.2)
  while [ $x -gt $GX ]; do x=$((x-16)); CMD+=(mousemove $x $GY sleep 0.016); done
  CMD+=(sleep 0.2)
done
CMD+=(mouseup 1 sleep 1)
xdotool "${CMD[@]}"
pids=$(pgrep -f 'example_glfw_opengl3$'); [ -n "$pids" ] && kill $pids
sleep 1; echo "log: $LOG ($(grep -c '\[drag\]' "$LOG") drag frames)"
