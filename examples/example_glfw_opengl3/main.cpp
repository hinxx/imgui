// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "imipmi.h"

extern "C" {
// for ipmitool
int verbose = 0;
int csv_output = 0;
}


// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}


/*

Example 3U crate with one digitizer AMC

----------------------------------------------
FRU   type  i2c state sensors name
----------------------------------------------
  0   MCH3   80   M4      2   NAT-MCH-CM
  1 ShFRU1   f4   M0      0   <none>
  2 ShFRU1   f4   M0      0   <none>
  3   MCH1   10   M4     14   NAT-MCH-MCMC
  5   AMC1   72   M4     14   mTCA-EVR-300 
  6   AMC2   74   M4     12   CCT AM G64/471
  8   AMC4   78   M4     20   SIS8300KU AMC
 31 ShFRU1   a6   M0      0   <none>
 40    CU1   a8   M4     14   Schroff uTCA CU
 50    PM1   c2   M4     32   NAT-PM-AC600D
 61   MCH1   16   M4     12   MCH-PCIe
 93   RTM4   78   M4      5   <none>
253 ShFRU1   f4   M0      0   <Carrier FRU>
254 ShFRU1   f4   M0      0   <Shelf FRU>
----------------------------------------------

ipmitool> sdr elist fru
SIS8300KU RTM    | 00h | ns  | 192.100 | Logical FRU @01h
SIS8300KU AMC    | 00h | ns  | 193.100 | Logical FRU @08h
CCT AM G64/471   | 00h | ns  | 193.98 | Logical FRU @06h
Schroff uTCA CU  | 00h | ns  | 30.97 | Logical FRU @28h
NAT-MCH-MCMC     | 00h | ns  | 194.97 | Logical FRU @03h
mTCA-EVR-300     | 00h | ns  | 193.97 | Logical FRU @05h
NAT-MCH-CM       | 00h | ns  | 194.102 | Logical FRU @00h
NAT-PM-AC600D   | 00h | ns  | 10.97 | Logical FRU @32h
MCH-PCIe         | 00h | ns  | 194.100 | Logical FRU @3Dh
BP-FRU-253       | 00h | ns  | 208.97 | Physical FRU @A4h
BP-FRU-254       | 00h | ns  | 242.98 | Physical FRU @A4h
SH-FRU-001       | 00h | ns  | 242.99 | Physical FRU @A4h
SH-FRU-002       | 00h | ns  | 242.100 | Physical FRU @A4h
MCH-Clock        | 00h | ns  | 194.99 | Logical FRU @3Ch

ipmitool> sdr list fru
SIS8300KU RTM    | Log FRU @01h c0.64 | ok
SIS8300KU AMC    | Log FRU @08h c1.64 | ok
CCT AM G64/471   | Log FRU @06h c1.62 | ok
Schroff uTCA CU  | Log FRU @28h 1e.61 | ok
NAT-MCH-MCMC     | Log FRU @03h c2.61 | ok
mTCA-EVR-300     | Log FRU @05h c1.61 | ok
NAT-MCH-CM       | Log FRU @00h c2.66 | ok
NAT-PM-AC600D   | Log FRU @32h 0a.61 | ok
MCH-PCIe         | Log FRU @3Dh c2.64 | ok
BP-FRU-253       | Phy FRU @A4h d0.61 | ok
BP-FRU-254       | Phy FRU @A4h f2.62 | ok
SH-FRU-001       | Phy FRU @A4h f2.63 | ok
SH-FRU-002       | Phy FRU @A4h f2.64 | ok
MCH-Clock        | Log FRU @3Ch c2.63 | ok

Example 9U crate with four digitizer AMC

----------------------------------------------
FRU   type  i2c state sensors name
----------------------------------------------
  0   MCH3   80   M4      2   NAT-MCH-CM
  1 ShFRU1   f4   M0      0   <none>
  2 ShFRU1   f4   M0      0   <none>
  3   MCH1   10   M4     14   NAT-MCH-MCMC
  5   AMC1   72   M4     13   CCT AM 900/412
  6   AMC2   74   M4     14   mTCA-EVR-300 
  7   AMC3   76   M4     20   SIS8300KU AMC
  8   AMC4   78   M4     20   SIS8300KU AMC
  9   AMC5   7a   M4     20   SIS8300KU AMC
 10   AMC6   7c   M4     20   SIS8300KU AMC
 16   AMC12   88   M1     12   CCT AM G64/471
 31 ShFRU1   a6   M0      0   <none>
 40    CU1   a8   M4     16   Schroff uTCA CU
 41    CU2   aa   M4     16   Schroff uTCA CU
 51    PM2   c4   M4     31   PM-AC1000
 61   MCH1   16   M4     12   MCH-PCIe
 92   RTM3   76   M4      5   <none>
 93   RTM4   78   M4      5   <none>
 94   RTM5   7a   M4      5   <none>
 95   RTM6   7c   M4      5   <none>
253 ShFRU1   f4   M0      0   <Carrier FRU>
254 ShFRU1   f4   M0      0   <Shelf FRU>
----------------------------------------------

ipmitool> sdr elist fru
SIS8300KU RTM    | 00h | ns  | 192.101 | Logical FRU @01h
SIS8300KU RTM    | 00h | ns  | 192.102 | Logical FRU @01h
SIS8300KU RTM    | 00h | ns  | 192.99 | Logical FRU @01h
SIS8300KU RTM    | 00h | ns  | 192.100 | Logical FRU @01h
Schroff uTCA CU  | 00h | ns  | 30.98 | Logical FRU @29h
NAT-MCH-MCMC     | 00h | ns  | 194.97 | Logical FRU @03h
Schroff uTCA CU  | 00h | ns  | 30.97 | Logical FRU @28h
CCT AM G64/471   | 00h | ns  | 193.108 | Logical FRU @10h
CCT AM 900/412   | 00h | ns  | 193.97 | Logical FRU @05h
SIS8300KU AMC    | 00h | ns  | 193.99 | Logical FRU @07h
SIS8300KU AMC    | 00h | ns  | 193.102 | Logical FRU @0Ah
SIS8300KU AMC    | 00h | ns  | 193.101 | Logical FRU @09h
SIS8300KU AMC    | 00h | ns  | 193.100 | Logical FRU @08h
mTCA-EVR-300     | 00h | ns  | 193.98 | Logical FRU @06h
NAT-MCH-CM       | 00h | ns  | 194.102 | Logical FRU @00h
PM-AC1000        | 00h | ns  | 10.98 | Logical FRU @33h
MCH-PCIe         | 00h | ns  | 194.100 | Logical FRU @3Dh
BP-FRU-253       | 00h | ns  | 208.97 | Physical FRU @A4h
BP-FRU-254       | 00h | ns  | 242.98 | Physical FRU @A4h
SH-FRU-001       | 00h | ns  | 242.99 | Physical FRU @A4h
SH-FRU-002       | 00h | ns  | 242.100 | Physical FRU @A4h
MCH-Clock        | 00h | ns  | 194.99 | Logical FRU @3Ch

ipmitool> sdr list fru
SIS8300KU RTM    | Log FRU @01h c0.65 | ok
SIS8300KU RTM    | Log FRU @01h c0.66 | ok
SIS8300KU RTM    | Log FRU @01h c0.63 | ok
SIS8300KU RTM    | Log FRU @01h c0.64 | ok
Schroff uTCA CU  | Log FRU @29h 1e.62 | ok
NAT-MCH-MCMC     | Log FRU @03h c2.61 | ok
Schroff uTCA CU  | Log FRU @28h 1e.61 | ok
CCT AM G64/471   | Log FRU @10h c1.6c | ok
CCT AM 900/412   | Log FRU @05h c1.61 | ok
SIS8300KU AMC    | Log FRU @07h c1.63 | ok
SIS8300KU AMC    | Log FRU @0Ah c1.66 | ok
SIS8300KU AMC    | Log FRU @09h c1.65 | ok
SIS8300KU AMC    | Log FRU @08h c1.64 | ok
mTCA-EVR-300     | Log FRU @06h c1.62 | ok
NAT-MCH-CM       | Log FRU @00h c2.66 | ok
PM-AC1000        | Log FRU @33h 0a.62 | ok
MCH-PCIe         | Log FRU @3Dh c2.64 | ok
BP-FRU-253       | Phy FRU @A4h d0.61 | ok
BP-FRU-254       | Phy FRU @A4h f2.62 | ok
SH-FRU-001       | Phy FRU @A4h f2.63 | ok
SH-FRU-002       | Phy FRU @A4h f2.64 | ok
MCH-Clock        | Log FRU @3Ch c2.63 | ok

Example 9U crate with ten digitizer AMC

----------------------------------------------
FRU   type  i2c state sensors name
----------------------------------------------
  0   MCH3   80   M4      2   NAT-MCH-CM
  1 ShFRU1   f4   M0      0   <none>
  2 ShFRU1   f4   M0      0   <none>
  3   MCH1   10   M4     14   NAT-MCH-MCMC
  5   AMC1   72   M4     12   CCT AM G64/471
  6   AMC2   74   M4     14   mTCA-EVR-300 
  7   AMC3   76   M4     35   DAMC-FMC2ZUP-11E
  8   AMC4   78   M4     36   DAMC-FMC2ZUP-11E
  9   AMC5   7a   M4     34   DAMC-FMC2ZUP-11E
 10   AMC6   7c   M4     35   DAMC-FMC2ZUP-11E
 11   AMC7   7e   M4     35   DAMC-FMC2ZUP-11E
 12   AMC8   80   M4     34   DAMC-FMC2ZUP-11E
 13   AMC9   82   M4     35   DAMC-FMC2ZUP-11E
 14   AMC10   84   M4     36   DAMC-FMC2ZUP-11E
 15   AMC11   86   M4     36   DAMC-FMC2ZUP-11E
 16   AMC12   88   M4     36   DAMC-FMC2ZUP-11E
 31 ShFRU1   a6   M0      0   <none>
 40    CU1   a8   M4     16   Schroff uTCA CU
 41    CU2   aa   M4     16   Schroff uTCA CU
 51    PM2   c4   M4     31   PM-AC1000
 53    PM4   c8   M4     31   PM-AC1000
 61   MCH1   16   M4     12   MCH-PCIe
 96   RTM7   7e   M4      4   <none>
253 ShFRU1   f4   M0      0   <Carrier FRU>
254 ShFRU1   f4   M0      0   <Shelf FRU>
----------------------------------------------


HOW TO MAKE SENSE OF ALL THESE NUMBERS???

output of 'sdr list fru' shows instances offset by 0x60.. it seems

--------------------------------------------------------------------------------------

/home/hinxx/Downloads/PICMG_MTCA.0_R1.0_forHinko.pdf

Site Number     The part of a Carrier Local Address that identifies a particular instance of a given FRU
                Site Type within a MicroTCA Carrier. For instance, Site Number would distinguish
                among multiple CUs or PMs.
Site Type       The part of a Carrier Local Address that identifies the type of a FRU site,
                distinguishing for instance, a AdvancedMC site from a CU site
EMMC            Enhanced Module Management Controller, used on Cooling Units, Power Module,
                and OEM Modules. See Module Management Controller (MMC).
MCMC            MicroTCA Carrier Management Controller (MCMC)
                Management controller on the MCH. The required management controller that
                interfaces to AdvancedMC MMCs via IPMB-L and to CU, PM, and OEM Module
                EMMCs via IPMB-0.


REQ 3.392 Each MCMC shall use its own Site Number + 60h as a device-relative entity instance number.

REQ 3.401 Each EMMC shall use its own Site Number + 60h as a device-relative entity instance number.

3.13.4 Carrier Manager SDR requirements

¶ 239 The Carrier Manager uses predefined FRU Device IDs for all Modules installed in a
MicroTCA Carrier, based on Site Type and Site Number. The local FRU Device ID for an
(E)MMC is always 0. The Carrier Manager assigns unique FRU Device IDs to all (E)MMCs,
as specified in Table 3-3, “Carrier Manager FRU Device IDs.” (E)MMC SDRs are linked
with a Module using the entity fields of the SDR. The Entity ID identifies that SDR as
coming from an MCH, AdvancedMC, PM, CU or OEM Module FRU, and the entity instance
is set to the Site Number + 60h. 60h is added to make the entity instance device-relative, in
accordance with Section 3.4.3 “Entities” in the AdvancedTCA Base specification. Refer to
this section of the AdvancedTCA specification and to Chapter 33.1 “System- and Device-
relative Entity Instance Values” of the IPMI specification for more information on IPMI
entities.

REQ 3.411
Each Carrier Manager’s Device SDR Repository shall use an Entity ID for a Module as
defined in Table 3-49, “MicroTCA Entity ID assignments” and the Module Site
Number + 60h as device-relative entity instances for Module SDRs.


--------------------------------------------------------------------------------------
we can rely on the 'sensor->entity.id' value to determine the 
type of the FRU and then 'sensor->entity.instance - 0x60' to get
its instance number; ends up being index x of AMCx, RTMx, CUx, PMx, ..

--------------------------------------------------------------------------------------

ipmitool> sdr entity help

Entity IDs:

     0  Unspecified                            1  Other                           
     2  Unknown                                3  Processor                       
     4  Disk or Disk Bay                       5  Peripheral Bay                  
     6  System Management Module               7  System Board                    
     8  Memory Module                          9  Processor Module                
    10  Power Supply                          11  Add-in Card                     
    12  Front Panel Board                     13  Back Panel Board                
    14  Power System Board                    15  Drive Backplane                 
    16  System Internal Expansion Board       17  Other System Board              
    18  Processor Board                       19  Power Unit                      
    20  Power Module                          21  Power Management                
    22  Chassis Back Panel Board              23  System Chassis                  
    24  Sub-Chassis                           25  Other Chassis Board             
    26  Disk Drive Bay                        27  Peripheral Bay                  
    28  Device Bay                            29  Fan Device                      
    30  Cooling Unit                          31  Cable/Interconnect              
    32  Memory Device                         33  System Management Software      
    34  BIOS                                  35  Operating System                
    36  System Bus                            37  Group                           
    38  Remote Management Device              39  External Environment            
    40  Battery                               41  Processing Blade                
    42  Connectivity Switch                   43  Processor/Memory Module         
    44  I/O Module                            45  Processor/IO Module             
    46  Management Controller Firmware        47  IPMI Channel                    
    48  PCI Bus                               49  PCI Express Bus                 
    50  SCSI Bus (parallel)                   51  SATA/SAS Bus                    
    52  Processor/Front-Side Bus              53  Real Time Clock(RTC)            
    54  Reserved                              55  Air Inlet                       
    56  Reserved                              57  Reserved                        
    58  Reserved                              59  Reserved                        
    60  Reserved                              61  Reserved                        
    62  Reserved                              63  Reserved                        
    64  Air Inlet                             65  Processor                       
    66  Baseboard/Main System Board          160  PICMG Front Board               
   192  PICMG Rear Transition Module         193  PICMG AdvancedMC Module         
   194  MicroTCA Carrier Hub (MCH)           208  OEM Module                      
   240  PICMG Shelf Management Controller     241  PICMG Filtration Unit           
   242  PICMG Shelf FRU Information          243  PICMG Alarm Panel               

ipmitool> sdr type help
Sensor Types:
	Temperature               (0x01)   Voltage                   (0x02)
	Current                   (0x03)   Fan                       (0x04)
	Physical Security         (0x05)   Platform Security         (0x06)
	Processor                 (0x07)   Power Supply              (0x08)
	Power Unit                (0x09)   Cooling Device            (0x0a)
	Other                     (0x0b)   Memory                    (0x0c)
	Drive Slot / Bay          (0x0d)   POST Memory Resize        (0x0e)
	System Firmwares          (0x0f)   Event Logging Disabled    (0x10)
	Watchdog1                 (0x11)   System Event              (0x12)
	Critical Interrupt        (0x13)   Button                    (0x14)
	Module / Board            (0x15)   Microcontroller           (0x16)
	Add-in Card               (0x17)   Chassis                   (0x18)
	Chip Set                  (0x19)   Other FRU                 (0x1a)
	Cable / Interconnect      (0x1b)   Terminator                (0x1c)
	System Boot Initiated     (0x1d)   Boot Error                (0x1e)
	OS Boot                   (0x1f)   OS Critical Stop          (0x20)
	Slot / Connector          (0x21)   System ACPI Power State   (0x22)
	Watchdog2                 (0x23)   Platform Alert            (0x24)
	Entity Presence           (0x25)   Monitor ASIC              (0x26)
	LAN                       (0x27)   Management Subsys Health  (0x28)
	Battery                   (0x29)   Session Audit             (0x2a)
	Version Change            (0x2b)   FRU State                 (0x2c)

*/

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    Context *ctx = InitContext();

    // float KeepAlive = 0.0;

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        {
            ImGui::Begin("Ipmitool debug");

            ImGui::InputInt("verbose", &verbose);

            static char hostname[128] = "172.30.150.51";
            static Host *host = nullptr;
            ImGui::InputText("hostname", hostname, IM_ARRAYSIZE(hostname));

            if (ImGui::Button("connect")) {
                if (strlen(hostname)) {
                    host = ctx->AddHost(hostname);
                    ImGui::Text("connected");
                }
            }
            if (ImGui::Button("disconnect")) {
                if (strlen(hostname)) {
                    ctx->RemoveHost(hostname);
                    host = nullptr;
                    ImGui::Text("disconnected");
                }
            }

            if (ImGui::Button("sdr list")) {
                if (host) {
                    Job job = {
                        .mType = JobType_listSDR,
                    };
                    host->RequestWork(job);
                }
            }

            if (ImGui::Button("dump")) {
                if (host) {
                    host->Dump();
                }
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // ipmitool
    DestroyContext(ctx);
    // ipmitool

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
