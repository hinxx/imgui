// Timing wrappers around the ImGuiPlatformIO hooks + a raw-GLFW probe.
// Entirely in the harness: no file in the imgui repo is modified.
#pragma once
#include "imgui.h"
#include <stdio.h>
#include <GLFW/glfw3.h>

static double hk_ms() { return glfwGetTime() * 1000.0; }
static double hk_thresh = 0.5;          // only report steps slower than this
static int    hk_frame = 0;             // set from the main loop each frame
static void (*o_PlatCreate)(ImGuiViewport*);
static void (*o_PlatDestroy)(ImGuiViewport*);
static void (*o_PlatShow)(ImGuiViewport*);
static void (*o_PlatSetPos)(ImGuiViewport*, ImVec2);
static void (*o_PlatSetSize)(ImGuiViewport*, ImVec2);
static void (*o_PlatSetTitle)(ImGuiViewport*, const char*);
static void (*o_PlatUpdate)(ImGuiViewport*);
static void (*o_PlatRender)(ImGuiViewport*, void*);
static void (*o_PlatSwap)(ImGuiViewport*, void*);
static void (*o_RendCreate)(ImGuiViewport*);
static void (*o_RendDestroy)(ImGuiViewport*);
static void (*o_RendRender)(ImGuiViewport*, void*);
static void (*o_RendSwap)(ImGuiViewport*, void*);

static void hk_report(const char* name, ImGuiViewport* vp, double t0)
{
    double d = hk_ms() - t0;
    if (d >= hk_thresh)
        printf("H%05d   %-24s %7.2f ms   vp=%08X\n", hk_frame, name, d, vp ? vp->ID : 0);
}
#define HK0(fn, orig) static void fn(ImGuiViewport* vp) \
    { double t0 = hk_ms(); orig(vp); hk_report(#orig + 2, vp, t0); }
HK0(w_PlatCreate,  o_PlatCreate)
HK0(w_PlatDestroy, o_PlatDestroy)
HK0(w_PlatShow,    o_PlatShow)
HK0(w_PlatUpdate,  o_PlatUpdate)
HK0(w_RendCreate,  o_RendCreate)
HK0(w_RendDestroy, o_RendDestroy)
static void w_PlatSetPos(ImGuiViewport* vp, ImVec2 v)   { double t=hk_ms(); o_PlatSetPos(vp,v);  hk_report("PlatSetWindowPos", vp, t); }
static void w_PlatSetSize(ImGuiViewport* vp, ImVec2 v)  { double t=hk_ms(); o_PlatSetSize(vp,v); hk_report("PlatSetWindowSize", vp, t); }
static void w_PlatSetTitle(ImGuiViewport* vp, const char* s) { double t=hk_ms(); o_PlatSetTitle(vp,s); hk_report("PlatSetWindowTitle", vp, t); }
static void w_PlatRender(ImGuiViewport* vp, void* a)    { double t=hk_ms(); o_PlatRender(vp,a); hk_report("PlatRenderWindow", vp, t); }
static void w_PlatSwap(ImGuiViewport* vp, void* a)      { double t=hk_ms(); o_PlatSwap(vp,a);   hk_report("PlatSwapBuffers", vp, t); }
static void w_RendRender(ImGuiViewport* vp, void* a)    { double t=hk_ms(); o_RendRender(vp,a); hk_report("RendRenderWindow", vp, t); }
static void w_RendSwap(ImGuiViewport* vp, void* a)      { double t=hk_ms(); o_RendSwap(vp,a);   hk_report("RendSwapBuffers", vp, t); }

static ImVec2 (*o_PlatGetPos)(ImGuiViewport*);
static ImVec2 (*o_PlatGetSize)(ImGuiViewport*);
static bool   (*o_PlatGetFocus)(ImGuiViewport*);
static bool   (*o_PlatGetMinimized)(ImGuiViewport*);
static ImVec2 w_PlatGetPos(ImGuiViewport* vp)      { double t=hk_ms(); ImVec2 r=o_PlatGetPos(vp);  hk_report("PlatGetWindowPos", vp, t); return r; }
static ImVec2 w_PlatGetSize(ImGuiViewport* vp)     { double t=hk_ms(); ImVec2 r=o_PlatGetSize(vp); hk_report("PlatGetWindowSize", vp, t); return r; }
static bool   w_PlatGetFocus(ImGuiViewport* vp)    { double t=hk_ms(); bool r=o_PlatGetFocus(vp);  hk_report("PlatGetWindowFocus", vp, t); return r; }
static bool   w_PlatGetMinimized(ImGuiViewport* vp){ double t=hk_ms(); bool r=o_PlatGetMinimized(vp); hk_report("PlatGetWindowMinimized", vp, t); return r; }

static void hk_install()
{
    ImGuiPlatformIO& p = ImGui::GetPlatformIO();
    #define HKSET(field, saved, wrapper) if (p.field) { saved = p.field; p.field = wrapper; }
    HKSET(Platform_CreateWindow,   o_PlatCreate,   w_PlatCreate)
    HKSET(Platform_DestroyWindow,  o_PlatDestroy,  w_PlatDestroy)
    HKSET(Platform_ShowWindow,     o_PlatShow,     w_PlatShow)
    HKSET(Platform_UpdateWindow,   o_PlatUpdate,   w_PlatUpdate)
    HKSET(Platform_SetWindowPos,   o_PlatSetPos,   w_PlatSetPos)
    HKSET(Platform_SetWindowSize,  o_PlatSetSize,  w_PlatSetSize)
    HKSET(Platform_SetWindowTitle, o_PlatSetTitle, w_PlatSetTitle)
    HKSET(Platform_RenderWindow,   o_PlatRender,   w_PlatRender)
    HKSET(Platform_SwapBuffers,    o_PlatSwap,     w_PlatSwap)
    HKSET(Renderer_CreateWindow,   o_RendCreate,   w_RendCreate)
    HKSET(Renderer_DestroyWindow,  o_RendDestroy,  w_RendDestroy)
    HKSET(Renderer_RenderWindow,   o_RendRender,   w_RendRender)
    HKSET(Renderer_SwapBuffers,    o_RendSwap,     w_RendSwap)
    HKSET(Platform_GetWindowPos,       o_PlatGetPos,       w_PlatGetPos)
    HKSET(Platform_GetWindowSize,      o_PlatGetSize,      w_PlatGetSize)
    HKSET(Platform_GetWindowFocus,     o_PlatGetFocus,     w_PlatGetFocus)
    HKSET(Platform_GetWindowMinimized, o_PlatGetMinimized, w_PlatGetMinimized)
    #undef HKSET
    printf("hooks installed\n");
}

// Raw GLFW cost of what ImGui_ImplGlfw_CreateWindow does, step by step, N times.
static void hk_probe(GLFWwindow* share, int n)
{
    // Variant A: no GL context at all (GLFW_NO_API) -> cost of the bare X11 window.
    for (int i = 0; i < n; i++)
    {
        double t0 = hk_ms();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, false);
        glfwWindowHint(GLFW_DECORATED, false);
        GLFWwindow* w = glfwCreateWindow(320, 140, "noapi", nullptr, nullptr);
        double t1 = hk_ms();
        glfwShowWindow(w);
        double t2 = hk_ms();
        glfwDestroyWindow(w);
        double t3 = hk_ms();
        printf("A%d noapi_create=%.2f show=%.2f destroy=%.2f  TOTAL=%.2f ms\n", i, t1-t0, t2-t1, t3-t2, t3-t0);
        glfwPollEvents();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    // Variant B: exactly what the backend does (shared GL context).
    for (int i = 0; i < n; i++)
    {
        double t0 = hk_ms();
        glfwWindowHint(GLFW_VISIBLE, false);
        glfwWindowHint(GLFW_FOCUSED, false);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
        glfwWindowHint(GLFW_DECORATED, false);
        double t1 = hk_ms();
        GLFWwindow* w = glfwCreateWindow(320, 140, "No Title Yet", nullptr, share);
        double t2 = hk_ms();
        glfwSetWindowPos(w, 400 + i * 10, 300);
        double t3 = hk_ms();
        glfwMakeContextCurrent(w); glfwSwapInterval(0);
        double t4 = hk_ms();
        glfwShowWindow(w);
        double t5 = hk_ms();
        glfwSwapBuffers(w);
        double t6 = hk_ms();
        glfwMakeContextCurrent(share);
        glfwDestroyWindow(w);
        double t7 = hk_ms();
        printf("P%d hints=%.2f create=%.2f setpos=%.2f makecurrent=%.2f show=%.2f firstswap=%.2f destroy=%.2f  TOTAL=%.2f ms\n",
               i, t1-t0, t2-t1, t3-t2, t4-t3, t5-t4, t6-t5, t7-t6, t7-t0);
        glfwPollEvents();
    }
}
