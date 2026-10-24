// Instrumented repro harness for Dear ImGui #2117 (Linux/X11 multi-viewport drag jitter).
// Logs, per frame: ground-truth pointer pos (X11), io.MousePos, target window pos/size,
// its viewport, PlatformRequestMove/Resize, mouse viewport selection, viewport count.
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "hooks.h"

void x11_open();
void x11_pointer(int* x, int* y);

static void glfw_error_callback(int error, const char* description) { fprintf(stderr, "GLFW Error %d: %s\n", error, description); }

int main(int argc, char** argv)
{
    const char* target_name = "Small";
    bool opt_nomerge = false;
    bool opt_dock = false;
    const char* opt_ini = NULL;
    bool opt_novsync = false;
    int main_x = 80, main_y = 60, main_w = 1280, main_h = 800;
    int quit_after_frames = 0;
    double opt_fpscap = 0.0;
    bool opt_ownvp = false;
    double opt_latch = 0.0;     // Hz: re-issue the dragged window's OS position this often, without redrawing
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--target") && i + 1 < argc)       target_name = argv[++i];
        else if (!strcmp(argv[i], "--nomerge"))                 opt_nomerge = true;
        else if (!strcmp(argv[i], "--dock"))                    opt_dock = true;
        else if (!strcmp(argv[i], "--ini") && i + 1 < argc)     opt_ini = argv[++i];
        else if (!strcmp(argv[i], "--novsync"))                 opt_novsync = true;
        else if (!strcmp(argv[i], "--fpscap") && i + 1 < argc)  opt_fpscap = atof(argv[++i]);
        else if (!strcmp(argv[i], "--latelatch") && i + 1 < argc) opt_latch = atof(argv[++i]);
        else if (!strcmp(argv[i], "--ownvp"))                   opt_ownvp = true;
        else if (!strcmp(argv[i], "--probe"))                   { /* handled after init */ }
        else if (!strcmp(argv[i], "--hooks"))                   { /* handled after init */ }
        else if (!strcmp(argv[i], "--quit") && i + 1 < argc)    quit_after_frames = atoi(argv[++i]);
        else { fprintf(stderr, "unknown arg '%s'\n", argv[i]); return 1; }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    x11_open();

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(main_w, main_h, "IMGUI_REPRO_HOST", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(opt_novsync ? 0 : 1);
    glfwSetWindowPos(window, main_x, main_y);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = opt_ini;                                 // nullptr = deterministic, no saved state
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = opt_nomerge;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    io.ConfigViewportsOwnViewportOnMove = opt_ownvp;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    bool opt_probe = false, opt_hooks = false;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i],"--probe")) opt_probe = true; if (!strcmp(argv[i],"--hooks")) opt_hooks = true; }
    if (opt_hooks) hk_install();
    if (opt_probe) { hk_probe(window, 6); }

    ImGuiContext& g = *ImGui::GetCurrentContext();
    g.DebugLogFlags = ImGuiDebugLogFlags_EventViewport | ImGuiDebugLogFlags_EventDocking | ImGuiDebugLogFlags_EventFocus | ImGuiDebugLogFlags_OutputToTTY;

    printf("# CONFIG target=%s nomerge=%d host=%d,%d %dx%d backend=%s glfw=%s\n",
        target_name, (int)opt_nomerge, main_x, main_y, main_w, main_h, io.BackendPlatformName, glfwGetVersionString());

    while (!glfwWindowShouldClose(window))
    {
        double st_a = hk_ms(); glfwPollEvents(); double st_poll = hk_ms() - st_a;
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
            continue;

        double st_f = hk_ms();
        ImGui_ImplOpenGL3_NewFrame();
        double st_nf_gl = hk_ms() - st_f;
        double st_g = hk_ms();
        ImGui_ImplGlfw_NewFrame();
        double st_nf_glfw = hk_ms() - st_g;
        double st_h = hk_ms();
        ImGui::NewFrame();
        double st_nf_core = hk_ms() - st_h;

        ImGuiViewport* mv = ImGui::GetMainViewport();
        if (opt_dock && g.FrameCount == 1)
        {
            ImGuiID node_id = ImGui::DockBuilderAddNode(0, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodePos(node_id, ImVec2(mv->WorkPos.x + 60, mv->WorkPos.y + 60));
            ImGui::DockBuilderSetNodeSize(node_id, ImVec2(320, 140));
            ImGui::DockBuilderDockWindow(target_name, node_id);
            ImGui::DockBuilderFinish(node_id);
            printf("# docked '%s' into floating node %08X\n", target_name, node_id);
        }
        // "Small": fits inside the host window -> starts merged into the main viewport.
        ImGui::SetNextWindowPos(ImVec2(mv->WorkPos.x + 60, mv->WorkPos.y + 60), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 140), ImGuiCond_FirstUseEver);
        ImGui::Begin("Small");
        ImGui::Button("Button");
        ImGui::Text("small window");
        ImGui::End();

        // "Demoish": same placement/size as the real Dear ImGui Demo window.
        ImGui::SetNextWindowPos(ImVec2(mv->WorkPos.x + 650, mv->WorkPos.y + 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
        ImGui::Begin("Demoish");
        ImGui::Button("Button");
        ImGui::Text("demo-sized window");
        ImGui::End();

        double st_b = hk_ms(); ImGui::Render(); double st_render = hk_ms() - st_b;

        // ---- instrumentation (before UpdatePlatformWindows, which clears the request flags)
        int tx = -1, ty = -1;
        x11_pointer(&tx, &ty);
        ImGuiWindow* w = ImGui::FindWindowByName(target_name);
        ImGuiViewportP* host = (ImGuiViewportP*)(void*)mv;
        if (w != NULL)
        {
            ImGuiWindow* geo = (w->DockNode && w->DockNode->HostWindow) ? w->DockNode->HostWindow : w;
            ImGuiViewportP* vp = geo->Viewport;
            bool contained = host->GetMainRect().Contains(geo->Rect());
            float grab_x = (geo != w) ? geo->Pos.x + geo->Size.x * 0.60f : geo->Pos.x + geo->Size.x * 0.5f;
            int os_x = -1, os_y = -1, os_w = -1, os_h = -1;
            if (vp && vp->PlatformWindowCreated && vp->PlatformHandle)
            {
                glfwGetWindowPos((GLFWwindow*)vp->PlatformHandle, &os_x, &os_y);
                glfwGetWindowSize((GLFWwindow*)vp->PlatformHandle, &os_w, &os_h);
            }
            printf("F%05d dt=%.1f os=%d,%d %dx%d truth=%d,%d mouse=%.0f,%.0f drift=%.0f,%.0f | moving=%s grab=%.0f,%.0f"
                   " | pos=%.0f,%.0f size=%.0fx%.0f owned=%d vp=%08X vppos=%.0f,%.0f reqM=%d reqR=%d"
                   " | mouseVP=%08X ioHovVP=%08X lastHovVP=%08X contained=%d nvp=%d\n",
                g.FrameCount, io.DeltaTime * 1000.0f, os_x, os_y, os_w, os_h, tx, ty, io.MousePos.x, io.MousePos.y, io.MousePos.x - tx, io.MousePos.y - ty,
                g.MovingWindow ? g.MovingWindow->Name : "-",
                grab_x, geo->Pos.y + 10.0f,
                geo->Pos.x, geo->Pos.y, geo->Size.x, geo->Size.y, (int)geo->ViewportOwned,
                vp ? vp->ID : 0, vp ? vp->Pos.x : -1, vp ? vp->Pos.y : -1,
                vp ? (int)vp->PlatformRequestMove : -1, vp ? (int)vp->PlatformRequestResize : -1,
                g.MouseViewport ? g.MouseViewport->ID : 0, io.MouseHoveredViewport,
                g.MouseLastHoveredViewport ? g.MouseLastHoveredViewport->ID : 0,
                (int)contained, g.Viewports.Size);
            if (geo != w)
                printf("F%05d   dock: host='%s' node=%08X dockactive=%d nodepos=%.0f,%.0f nodesize=%.0fx%.0f\n",
                    g.FrameCount, geo->Name, w->DockId, (int)w->DockIsActive,
                    w->DockNode->Pos.x, w->DockNode->Pos.y, w->DockNode->Size.x, w->DockNode->Size.y);
        }

        GLFWwindow* dragged_os_win = NULL;
        if (w != NULL && w->Viewport && w->Viewport->PlatformWindowCreated && w->ViewportOwned)
            dragged_os_win = (GLFWwindow*)w->Viewport->PlatformHandle;

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        double st_c = hk_ms(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); double st_draw = hk_ms() - st_c;

        hk_frame = g.FrameCount;
        double st_upw = 0.0, st_rpw = 0.0;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            double t_upw = hk_ms();
            ImGui::UpdatePlatformWindows();
            st_upw = hk_ms() - t_upw;
            if (st_upw >= 1.0) printf("H%05d UpdatePlatformWindows   %7.2f ms\n", g.FrameCount, st_upw);
            double st_d = hk_ms();
            ImGui::RenderPlatformWindowsDefault();
            st_rpw = hk_ms() - st_d;
            glfwMakeContextCurrent(window);
        }
        double st_e = hk_ms(); glfwSwapBuffers(window); double st_swap = hk_ms() - st_e;
        if (opt_fpscap > 0.0)
        {
            // Uncapped-but-limited: keep the loop running well above the display rate without spinning flat out.
            static double next_deadline = 0.0;
            double period = 1000.0 / opt_fpscap;
            double now = hk_ms();
            if (next_deadline < now - period) next_deadline = now;
            next_deadline += period;
            double wait = next_deadline - now;
            if (wait > 0.0) { struct timespec ts; ts.tv_sec = (time_t)(wait / 1000.0); ts.tv_nsec = (long)((wait - ts.tv_sec * 1000.0) * 1e6); nanosleep(&ts, nullptr); }
        }
        double st_total = hk_ms() - st_a;
        if (st_total >= 25.0)
            printf("H%05d STAGES total=%.1f poll=%.1f nf_gl=%.1f nf_glfw=%.1f nf_core=%.1f render=%.1f draw=%.1f updateplat=%.1f renderplat=%.1f mainswap=%.1f\n",
                   g.FrameCount, st_total, st_poll, st_nf_gl, st_nf_glfw, st_nf_core, st_render, st_draw, st_upw, st_rpw, st_swap);

        // ---- LATE LATCH ----------------------------------------------------------------
        // A window being dragged only changes POSITION; its pixels are identical. So after the frame
        // has been presented we can keep re-polling the mouse and re-issuing just the OS window position
        // until the next vblank, at a much higher rate than we render, for almost no CPU.
        int latch_count = 0;
        if (opt_latch > 0.0)
        {
            ImGuiWindow* mw = g.MovingWindow;
            ImGuiViewportP* mvp = mw ? (ImGuiViewportP*)(void*)mw->Viewport : NULL;
            if (mw && mvp && mvp != (ImGuiViewportP*)(void*)ImGui::GetMainViewport() && mvp->PlatformWindowCreated)
            {
                const double budget = 1000.0 / 60.0 - 5.0;      // leave headroom before the next vblank
                const double step = 1000.0 / opt_latch;
                const double t_begin = hk_ms();
                while (hk_ms() - t_begin < budget)
                {
                    double wait = step - (hk_ms() - t_begin) + latch_count * step;
                    if (wait > 0.0)
                    {
                        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = (long)(wait * 1e6);
                        nanosleep(&ts, nullptr);
                    }
                    glfwPollEvents();
                    double cx = 0.0, cy = 0.0; int wx = 0, wy = 0;
                    glfwGetCursorPos(window, &cx, &cy);
                    glfwGetWindowPos(window, &wx, &wy);
                    ImVec2 pos((float)(wx + cx) - g.ActiveIdClickOffset.x, (float)(wy + cy) - g.ActiveIdClickOffset.y);
                    // Go through the backend hook so it sets its ignore-event frame and we don't
                    // get the move echoed back as a user-initiated move.
                    ImGui::GetPlatformIO().Platform_SetWindowPos(mvp, pos);
                    latch_count++;
                }
            }
        }

        // End-of-frame truth: where the OS window actually is now, and where the pointer actually is now.
        if (dragged_os_win != NULL)
        {
            int ox, oy, px, py;
            glfwGetWindowPos(dragged_os_win, &ox, &oy);
            x11_pointer(&px, &py);
            printf("E%05d osend=%d,%d ptrend=%d,%d err=%d\n", g.FrameCount, ox, oy, px, py, (ox - px));
        }

        if (quit_after_frames > 0 && g.FrameCount >= quit_after_frames)
            break;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
