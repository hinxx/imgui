#!/bin/bash
# drive_fast.sh <logname> <target> [repro args...]
# Hand-speed drag (~1000 px/s): strictly INSIDE the host window, twice (~1000 px/s).
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
for i in $(seq 1 50); do
  WID=$(xdotool search --name IMGUI_REPRO_HOST 2>/dev/null | head -1) || true
  [ -n "$WID" ] && break
  xdotool sleep 0.2
done
[ -n "$WID" ] || { echo "host window never appeared"; kill $APP; exit 1; }
xdotool windowactivate --sync "$WID"; xdotool sleep 0.6
# if the target already owns a platform window (e.g. --nomerge), raise it so the press lands on it
TWID=$(xdotool search --name "^$TARGET$" 2>/dev/null | head -1) || true
[ -n "$TWID" ] && { xdotool windowactivate --sync "$TWID" || true; xdotool windowraise "$TWID" || true; xdotool sleep 0.4; }
GRAB=$(grep -o 'grab=[0-9-]*,[0-9-]*' "$LOG" | tail -1 | cut -d= -f2)
GX=${GRAB%,*}; GY=${GRAB#*,}
echo "host wid=$WID target=$TARGET grab=$GX,$GY"
CMD=(mousemove $GX $GY sleep 0.4 mousedown 1 sleep 0.4)
for pass in 1 2; do
  x=$GX; while [ $x -lt 1120 ]; do x=$((x+16)); CMD+=(mousemove $x $GY sleep 0.016); done   # stay inside host
  CMD+=(sleep 0.3)
  while [ $x -gt $GX ]; do x=$((x-16)); CMD+=(mousemove $x $GY sleep 0.016); done           # back
  CMD+=(sleep 0.3)
done
CMD+=(mouseup 1 sleep 0.8)
xdotool "${CMD[@]}"
kill $APP 2>/dev/null || true
wait $APP 2>/dev/null || true
echo "log: $LOG ($(wc -l < "$LOG") lines)"
