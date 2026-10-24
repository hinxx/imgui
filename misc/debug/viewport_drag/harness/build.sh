#!/bin/bash
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/../../../.." && pwd)"   # dear imgui repository root
D="$HERE"
CXXFLAGS="-g -O1 ${EXTRA_FLAGS} -I$SRC -I$SRC/backends $(pkg-config --cflags glfw3)"
LIBS="$(pkg-config --libs glfw3) -lGL -lX11"
mkdir -p $D/obj
# No header dependency tracking here: if imgui.h changed, ImGuiIO's layout may have changed and stale
# objects produce bogus asserts. Rebuild everything in that case.
if [ -f "$D/obj/imgui.o" ] && [ "$SRC/imgui.h" -nt "$D/obj/imgui.o" ]; then rm -f $D/obj/*.o; fi
for f in $SRC/imgui.cpp $SRC/imgui_draw.cpp $SRC/imgui_tables.cpp $SRC/imgui_widgets.cpp $SRC/backends/imgui_impl_glfw.cpp $SRC/backends/imgui_impl_opengl3.cpp; do
  o=$D/obj/$(basename $f .cpp).o
  [ -f "$o" ] && [ "$o" -nt "$f" ] || g++ $CXXFLAGS -c "$f" -o "$o"
done
g++ $CXXFLAGS -c $D/x11util.cpp -o $D/obj/x11util.o
g++ $CXXFLAGS -c $D/main.cpp -o $D/obj/main.o
g++ -o $D/repro $D/obj/*.o $LIBS
echo "built: $D/repro"
