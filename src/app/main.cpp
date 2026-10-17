// GUI entry point: create an OS window with GLFW, an OpenGL context to draw into,
// and run the Dear ImGui frame loop.

#include "app/MainWindow.h"
#include "app/Theme.h"
#include "platform/Appearance.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    bool startMinimized = false;  // passed by the login autostart entry
    for (int i = 1; i < argc; ++i) startMinimized |= std::strcmp(argv[i], "--minimized") == 0;

    glfwSetErrorCallback([](int code, const char* msg) { std::fprintf(stderr, "GLFW error %d: %s\n", code, msg); });
    if (!glfwInit()) return 1;

#ifdef __APPLE__
    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(1100, 700, "FindEverything", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    if (startMinimized) glfwIconifyWindow(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't litter imgui.ini in the working dir

    // Look like the rest of the desktop: system font, dark/light, accent colour.
    // Content scale is >1 on HiDPI screens (from Xft.dpi on X11).
    float scaleX = 1.0f, scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    {
        // The app loads settings and applies the theme (system look + user overrides).
        fe::MainWindow app(fe::querySystemAppearance(), scaleX);
        fe::loadFonts(app.appearance(), app.fontScale());

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glslVersion);
        app.start();

        while (!glfwWindowShouldClose(window)) {
            // Sleep until input arrives (or 100 ms passes, to refresh the indexing
            // counter). Keeps CPU near 0% while idle instead of spinning at 60 fps.
            glfwWaitEventsTimeout(0.1);

            // Font zoom: ImGui 1.91 bakes fonts into a texture at a fixed size, so
            // a new size means rebuilding the atlas and re-uploading it to the GPU.
            // Must happen between frames.
            if (app.consumeFontRebuild()) {
                ImGui::GetIO().Fonts->Clear();
                fe::loadFonts(app.appearance(), app.fontScale());
                ImGui_ImplOpenGL3_DestroyFontsTexture();
                ImGui_ImplOpenGL3_CreateFontsTexture();
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            app.draw();
            ImGui::Render();

            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];  // theme can change at runtime
            glClearColor(bg.x, bg.y, bg.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);

            if (app.quitRequested()) glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
