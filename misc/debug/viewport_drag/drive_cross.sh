#!/bin/bash
# drive_cross.sh <logname>
# Writes a deterministic imgui.ini first: "Hello, world!" starts as its OWN OS window on the desktop at
# 300,746, so the driver can locate it by name with xdotool. It is then pushed INTO the host window
# (merge + platform window destroyed/recycled) and pulled back OUT (platform window created mid-drag).
# Pulls it out past the host's left edge (platform window created mid-drag), then pushes it back in.
# App env is inherited:  IMGUI_DIAG=1 IMGUI_GLFW_WINDOW_POOL=0 ./drive_cross.sh name
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"          # dear imgui repository root
D="$HERE/logs"; mkdir -p "$D"
REPRO="$HERE/harness/repro"
APPDIR="$ROOT/examples/example_glfw_opengl3"
LOG="$D/$1.log"; rm -f "$LOG"
cat > "$APPDIR/imgui.ini" <<'INI'
[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Dear ImGui Demo]
Pos=650,20
Size=550,680
Collapsed=0

[Window][Hello, world!]
ViewportPos=300,746
ViewportId=0xC8A106E5
Size=338,180
Collapsed=0

[Docking][Data]
INI
( cd "$APPDIR" && ./example_glfw_opengl3 > "$LOG" 2>&1 & )
for i in $(seq 1 60); do H=$(xdotool search --name "Dear ImGui GLFW\+OpenGL3 example" 2>/dev/null | head -1) || true; [ -n "$H" ] && break; xdotool sleep 0.2; done
[ -n "$H" ] || { echo "host window never appeared"; exit 1; }
xdotool sleep 2
read HX HY < <(xdotool getwindowgeometry $H | sed -n '2p' | sed 's/.*Position: \([0-9-]*\),\([0-9-]*\).*/\1 \2/')
for i in $(seq 1 40); do W=$(xdotool search --name "^Hello, world!$" 2>/dev/null | head -1) || true; [ -n "$W" ] && break; xdotool sleep 0.2; done
[ -n "$W" ] || { echo "Hello never got its own OS window"; pids=$(pgrep -f 'example_glfw_opengl3$'); [ -n "$pids" ] && kill $pids; exit 1; }
read PX PY < <(xdotool getwindowgeometry $W | sed -n '2p' | sed 's/.*Position: \([0-9-]*\),\([0-9-]*\).*/\1 \2/')
GX=$((PX+80)); GY=$((PY+10))
echo "host at $HX,$HY -- Hello at $PX,$PY -- grabbing at $GX,$GY"
xdotool windowactivate --sync $W; xdotool sleep 0.6
# IN: push it into the middle of the host -> merges back, platform window destroyed or recycled
xdotool mousemove $GX $GY sleep 0.4 mousedown 1 sleep 0.4
x=$GX; y=$GY
TX=$((HX+500)); TY=$((HY+400))
while [ $x -lt $TX ] || [ $y -gt $TY ]; do
  [ $x -lt $TX ] && x=$((x+16))
  [ $y -gt $TY ] && y=$((y-16))
  xdotool mousemove $x $y sleep 0.016
done
xdotool sleep 0.4 mouseup 1 sleep 1.5
# OUT: pull it back out past the host's left edge -> platform window created mid-drag
xdotool windowactivate --sync $H; xdotool sleep 0.6
xdotool mousemove $x $y sleep 0.4 mousedown 1 sleep 0.4
while [ $x -gt $((HX-100)) ]; do x=$((x-16)); xdotool mousemove $x $y sleep 0.016; done
xdotool sleep 0.4 mouseup 1 sleep 1.5
pids=$(pgrep -f 'example_glfw_opengl3$'); [ -n "$pids" ] && kill $pids
sleep 1
grep -E '^\[repro\]|\[observe\]|Platform Window' "$LOG"
