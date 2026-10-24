// Ground-truth pointer position, independent of GLFW/imgui event plumbing.
#include <X11/Xlib.h>
static Display* g_display = 0;
void x11_open()  { g_display = XOpenDisplay(0); }
void x11_pointer(int* out_x, int* out_y)
{
    if (!g_display) { *out_x = *out_y = -1; return; }
    Window root_ret, child_ret; int rx = -1, ry = -1, wx, wy; unsigned int mask;
    XQueryPointer(g_display, DefaultRootWindow(g_display), &root_ret, &child_ret, &rx, &ry, &wx, &wy, &mask);
    *out_x = rx; *out_y = ry;
}
