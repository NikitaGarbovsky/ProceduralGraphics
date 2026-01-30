module;

// Regular imports.
#include <glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Just used to easily manage dear-imgui state during frames, primarily controlled in RendererEditorUI
export module RendererImgui;

export void ImGui_Init(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

export void ImGui_Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

export void ImGui_BeginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

export void ImGui_EndFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

export bool ImGui_WantsMouse()
{
    return ImGui::GetIO().WantCaptureMouse;
}

export bool ImGui_WantsKeyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}
