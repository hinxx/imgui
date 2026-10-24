#!/bin/bash
# drive_cycle.sh <logname> <target> [args...]  -- N full out-release-in-release cycles, then count X windows.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"          # dear imgui repository root
D="$HERE/logs"; mkdir -p "$D"
REPRO="$HERE/harness/repro"
APPDIR="$ROOT/examples/example_glfw_opengl3"
LOG="$D/$1.log"; TARGET="$2"; shift 2
rm -f "$LOG"
"$REPRO" --target "$TARGET" "$@" > "$LOG" 2>&1 &
APP=$!
for i in $(seq 1 50); do WID=$(xdotool search --name IMGUI_REPRO_HOST 2>/dev/null | head -1) || true; [ -n "$WID" ] && break; xdotool sleep 0.2; done
xdotool windowactivate --sync "$WID"; xdotool sleep 0.6
GRAB=$(grep -o 'grab=[0-9-]*,[0-9-]*' "$LOG" | tail -1 | cut -d= -f2); GX=${GRAB%,*}; GY=${GRAB#*,}
for cycle in 1 2 3; do
  T=$(xdotool search --name "^$TARGET$" 2>/dev/null | head -1) || true
  [ -n "$T" ] && { xdotool windowactivate --sync "$T" 2>/dev/null || true; xdotool sleep 0.3; } || { xdotool windowactivate --sync "$WID"; xdotool sleep 0.3; }
  CMD=(mousemove $GX $GY sleep 0.3 mousedown 1 sleep 0.3)
  x=$GX; while [ $x -lt 1650 ]; do x=$((x+24)); CMD+=(mousemove $x $GY sleep 0.016); done
  CMD+=(sleep 0.3 mouseup 1 sleep 0.8)                       # release OUTSIDE -> keeps own viewport
  CMD+=(mousemove $x $GY sleep 0.3 mousedown 1 sleep 0.3)
  while [ $x -gt $GX ]; do x=$((x-24)); CMD+=(mousemove $x $GY sleep 0.016); done
  CMD+=(sleep 0.3 mouseup 1 sleep 1.0)                       # release INSIDE -> merge back, recycle
  xdotool "${CMD[@]}"
done
echo "X windows owned by app at end:"
for w in $(xdotool search --name 'IMGUI_REPRO_HOST|No Title Yet|^Small$|^Demoish$' 2>/dev/null); do printf "  %-18s" "$(xdotool getwindowname $w 2>/dev/null)"; xdotool getwindowgeometry $w | sed -n '2p'; done
kill $APP 2>/dev/null || true; wait $APP 2>/dev/null || true
echo "log: $LOG"
