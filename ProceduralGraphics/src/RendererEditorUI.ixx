module;

// Normal imports
#include <cstdint>
#include <glfw3.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include "imgui.h"
#include "ImGuizmo.h"
#include <string>

// This module manages the renderers editor ui which is using the Dear-Imgui library. 
// Gizmos are created & managed using a seperate library addon for Dear-Imgui. (ImGuizmo)
export module RendererEditorUI;

// Codebase imports
import RendererImgui;
import RendererEntitys;
import RendererTransformUtils;
import RendererData;

// State variables for types of input
static ImGuizmo::OPERATION gTformGuizmoCurrentOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE      gAxisType = ImGuizmo::LOCAL;
static bool gWantsMouse = false;
ImFont* glarge_font;

export void InitEditorUI(GLFWwindow* _window) { 
    ImGui_Init(_window);
    ImGuiIO& io = ImGui::GetIO();
    glarge_font = io.Fonts->AddFontFromFileTTF("Assets/Fonts/AlteHaasGroteskBold.ttf", 24.0f);
}

export void ShutdownEditorUI() { ImGui_Shutdown(); }

// Helper to organize input
static void GizmoShortcuts() {
    if (ImGui_WantsKeyboard()) return;

    if (ImGui::IsKeyPressed(ImGuiKey_W)) gTformGuizmoCurrentOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E)) gTformGuizmoCurrentOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) gTformGuizmoCurrentOperation = ImGuizmo::SCALE;
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) gAxisType = (gAxisType == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

// State set at beginning of Imgui Frame
void EditorUI_BeginFrame() {
    gWantsMouse = false;
    ImGui_BeginFrame();
    ImGuizmo::BeginFrame();
}

// State set at end of Imgui Frame
void EditorUI_EndFrame() {
    ImGui_EndFrame();
}

export void EditorUI_Draw(const glm::mat4& _view, const glm::mat4& _proj, int _viewportW, int _viewportH, uint32_t _selectedEntityID) {
    EditorUI_BeginFrame();

    // Configuration flags for the DearImgui Entity Menu
    ImGuiWindowFlags windowFlags0 = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    // =========== Entity Window ===========
    
    ImGui::PushFont(glarge_font);

    // Tempt scaling calculations to keep shit in frame, #TODO: make some sort of system for scaling and aligning/layout of imgui ui.
    int x1, y1;
    glfwGetFramebufferSize(MainWindow, &x1, &y1);
    x1 -= x1 / 6;
    y1 -= (y1 / 6) *5;
    ImVec2 pos = ImVec2((float)x1, (float)y1);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(250, 450));
    ImGui::Begin("====== Entities ======", nullptr ,windowFlags0);

    ImGui::Text("Total Loaded: %u", CurrentRenderedEntitys.size());
    ImGui::Text("Frustum Culled: % u", FrustrumCulledEntitiesThisFrame);
    
    ImGui::PopFont();
    ImGui::End();

    // Is there an entity selected?  - Display selected entity screen.
    if (_selectedEntityID != UINT32_MAX) {
        ImGui::PushFont(glarge_font);
        std::string str = "Entity: ";
        str += std::to_string(_selectedEntityID + 1);
        ImGui::SetNextWindowSize(ImVec2(250, 200));

        // Positioning Calculations
        int x, y;
        glfwGetFramebufferSize(MainWindow, &x, &y);
        x -= x / 6;
        y -= (y / 5) * 2;

        ImVec2 pos = ImVec2((float)x, (float)y);
        ImGui::SetNextWindowPos(pos);
        ImGui::Begin(str.c_str(), nullptr, windowFlags0);
        
        ImGui::DragFloat("X", &EntityTransforms.position[_selectedEntityID].x);
        ImGui::DragFloat("Y", &EntityTransforms.position[_selectedEntityID].y);
        ImGui::DragFloat("Z", &EntityTransforms.position[_selectedEntityID].z);

        ImGui::PopFont();
        ImGui::End();
    }
    
    
    GizmoShortcuts();

    // =========== Gizmo ===========
    if (_selectedEntityID != 0xFFFFFFFFu) {
        // Build model from SoA TRS
        glm::mat4 modelMatrix = ComposeTRS(
            EntityTransforms.position[_selectedEntityID],
            EntityTransforms.rotation[_selectedEntityID],
            EntityTransforms.scale[_selectedEntityID]
        );

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
        ImGuizmo::SetRect(0.0f, 0.0f, (float)_viewportW, (float)_viewportH);

        bool changed = ImGuizmo::Manipulate(
            glm::value_ptr(_view),
            glm::value_ptr(_proj),
            gTformGuizmoCurrentOperation,
            gAxisType,
            glm::value_ptr(modelMatrix),
            nullptr,
            nullptr
        );

        if (changed || ImGuizmo::IsUsing())
        {
            // Using ImGuizmo’s own decomposition so it matches its manipulation behaviour
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), t, r, s);

            EntityTransforms.position[_selectedEntityID] = { t[0], t[1], t[2] };
            EntityTransforms.rotation[_selectedEntityID] = { r[0], r[1], r[2] }; // degrees
            EntityTransforms.scale[_selectedEntityID] = { s[0], s[1], s[2] };
        }
    }

    gWantsMouse = ImGui_WantsMouse() || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    EditorUI_EndFrame();
}

export bool EditorUI_WantsMouse()
{
    return gWantsMouse;
}