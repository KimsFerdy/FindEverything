// GUI entry point: create an OS window with GLFW, an OpenGL context to draw into,
// and run the Dear ImGui frame loop.

#include "app/MainWindow.h"

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
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    {
        fe::MainWindow app;
        app.startIndexing();

        while (!glfwWindowShouldClose(window)) {
            // Sleep until input arrives (or 100 ms passes, to refresh the indexing
            // counter). Keeps CPU near 0% while idle instead of spinning at 60 fps.
            glfwWaitEventsTimeout(0.1);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            app.draw();
            ImGui::Render();

            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
