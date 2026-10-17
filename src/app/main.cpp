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

int main() {
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
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't litter imgui.ini in the working dir

    // Look like the rest of the desktop: system font, dark/light, accent colour.
    // Content scale is >1 on HiDPI screens (from Xft.dpi on X11).
    float scaleX = 1.0f, scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    const fe::SystemAppearance appearance = fe::querySystemAppearance();
    fe::applyTheme(appearance, scaleX);
    fe::loadFonts(appearance, scaleX);
    const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    {
        fe::MainWindow app;
        app.startIndexing();

        while (!glfwWindowShouldClose(window)) {
            // Sleep until input arrives (or 100 ms passes, to refresh the indexing
            // counter). Keeps CPU near 0% while idle instead of spinning at 60 fps.
            glfwWaitEventsTimeout(0.1);

            // Font zoom: ImGui 1.91 bakes fonts into a texture at a fixed size, so
            // a new size means rebuilding the atlas and re-uploading it to the GPU.
            // Must happen between frames.
            if (app.consumeFontZoomChange()) {
                ImGui::GetIO().Fonts->Clear();
                fe::loadFonts(appearance, scaleX * app.fontZoom());
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
