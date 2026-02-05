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
import RendererPass_DebugBounds;
import RendererLights;

// State variables for types of input
static ImGuizmo::OPERATION gTformGuizmoCurrentOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE      gAxisType = ImGuizmo::LOCAL;
static bool gWantsMouse = false;
ImFont* glarge_font;
ImFont* gsmall_font;

static bool showBounds = true;
static bool BoundsSelectedOnly = true;

enum class PlaceLightMode : uint32_t { None = 0, Point, Directional, Spot };
static PlaceLightMode gPlaceLightMode = PlaceLightMode::None;

export bool EditorUIIsPlacingLight() { return gPlaceLightMode != PlaceLightMode::None; }
export uint32_t EditorUIGetPlaceLightMode() { return (uint32_t)gPlaceLightMode; }
export void EditorUIClearPlaceLightMode() { gPlaceLightMode = PlaceLightMode::None; }
static const char* LightPrefix(LightType t);

export void InitEditorUI(GLFWwindow* _window) { 
    ImGui_Init(_window);
    ImGuiIO& io = ImGui::GetIO();
    glarge_font = io.Fonts->AddFontFromFileTTF("Assets/Fonts/AlteHaasGroteskBold.ttf", 24.0f);
    gsmall_font = io.Fonts->AddFontFromFileTTF("Assets/Fonts/AlteHaasGroteskRegular.ttf", 20.0f);
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

export void EditorUI_Draw(const glm::mat4& _view, const glm::mat4& _proj, int _viewportW, int _viewportH, uint32_t _selectedEntityID, uint32_t _selectedLight) {
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
    ImGui::PushFont(gsmall_font);
    ImGui::Checkbox("Show Bounds", &showBounds);
    ImGui::Checkbox("Bounds: Selected Only", &BoundsSelectedOnly);
    DebugBoundsSetEnabled(showBounds);
    DebugBoundsSetSelectedOnly(BoundsSelectedOnly);
    if (ImGui::BeginListBox("##entity_list", ImVec2(-1, 120)))
    {
        for (uint32_t i = 0; i < CurrentRenderedEntitys.size(); ++i) {
            std::string label = "Entity: "; label += std::to_string(i + 1);
            bool selected = (_selectedEntityID == i);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                SelectedEntity = i;
                SelectedLight = UINT32_MAX;
            }
        }
        ImGui::EndListBox();
    }

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
    
    // =========== Lights Window ===========
    ImGuiWindowFlags windowFlagsL = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(280, 220));
    ImGui::Begin("Create", nullptr, windowFlagsL);
    ImGui::PushFont(gsmall_font);
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate,
        ImGui::GetIO().Framerate);

    if (ImGui::Button("Point Light", ImVec2(-1, 0))) gPlaceLightMode = PlaceLightMode::Point;
    if (ImGui::Button("Directional Light", ImVec2(-1, 0))) gPlaceLightMode = PlaceLightMode::Directional;
    if (ImGui::Button("Spot Light", ImVec2(-1, 0))) gPlaceLightMode = PlaceLightMode::Spot;

    if (gPlaceLightMode != PlaceLightMode::None) {
        ImGui::Separator();
        ImGui::Text("Placing: %s",
            (gPlaceLightMode == PlaceLightMode::Point) ? "Point" :
            (gPlaceLightMode == PlaceLightMode::Directional) ? "Directional" : "Spot");
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) gPlaceLightMode = PlaceLightMode::None;
    }

    ImGui::PopFont();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(20, 250));
    ImGui::SetNextWindowSize(ImVec2(280, 220));
    ImGui::Begin("Lights", nullptr, windowFlagsL);
    uint32_t lightCount = GetLightCount();
    ImGui::Text("Total Lights: %u", lightCount);

    if (ImGui::BeginListBox("##light_list", ImVec2(-1, 120)))
    {
        for (uint32_t i = 0; i < lightCount; ++i) { 
            LightType t = GetLightType(i);
            std::string label = std::string(LightPrefix(t)) + std::to_string(i);

            bool selected = (_selectedLight == i);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                SelectedLight = i;
                SelectedEntity = UINT32_MAX;
            }
        }
        ImGui::EndListBox();
    }
    ImGui::End();

    if (_selectedLight != UINT32_MAX && _selectedLight < lightCount)
    {
        ImGui::SetNextWindowPos(ImVec2(20, 500));
        ImGui::SetNextWindowSize(ImVec2(350, 350));
        ImGui::Begin("SelectedLight", nullptr, windowFlagsL);

        LightType t = GetLightType(_selectedLight);

        glm::vec3 c = GetLightColor(_selectedLight);
        if (ImGui::ColorEdit3("Color", &c.x)) SetLightColor(_selectedLight, c);

        float intensity = GetLightIntensity(_selectedLight);
        if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 200.0f)) SetLightIntensity(_selectedLight, intensity);

        if (t == LightType::Point || t == LightType::Spot)
        {
            float range = GetLightRange(_selectedLight);
            if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 200.0f)) SetLightRange(_selectedLight, range);
        }

        if (t == LightType::Spot)
        {
            float innerDeg = GetSpotInnerDeg(_selectedLight);
            float outerDeg = GetSpotOuterDeg(_selectedLight);
            bool changed = false;
            changed |= ImGui::DragFloat("Inner (deg)", &innerDeg, 0.1f, 0.0f, 89.0f);
            changed |= ImGui::DragFloat("Outer (deg)", &outerDeg, 0.1f, 0.0f, 89.0f);
            if (changed) SetSpotInnerOuter(_selectedLight, innerDeg, outerDeg);
        }

        ImGui::Separator();
        ImGui::DragFloat3("Pos", &LightTransforms.position[_selectedLight].x, 0.05f);
        ImGui::DragFloat3("Rot", &LightTransforms.rotation[_selectedLight].x, 0.25f);

        ImGui::End();
    }
   

    GizmoShortcuts();

    // =========== Gizmo ===========

    // Entity gizmo
    if (_selectedEntityID != UINT32_MAX)
    {
        glm::mat4 modelMatrix = ComposeTRSMatrix(
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
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), t, r, s);

            EntityTransforms.position[_selectedEntityID] = { t[0], t[1], t[2] };
            EntityTransforms.rotation[_selectedEntityID] = { r[0], r[1], r[2] };
            EntityTransforms.scale[_selectedEntityID] = { s[0], s[1], s[2] };
        }
    }
    // Light gizmo
    else if (_selectedLight != UINT32_MAX)
    {
        glm::mat4 modelMatrix = ComposeTRSMatrix(
            LightTransforms.position[_selectedLight],
            LightTransforms.rotation[_selectedLight],
            LightTransforms.scale[_selectedLight]
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
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), t, r, s);

            LightTransforms.position[_selectedLight] = { t[0], t[1], t[2] };
            LightTransforms.rotation[_selectedLight] = { r[0], r[1], r[2] };
            LightTransforms.scale[_selectedLight] = { s[0], s[1], s[2] };
        }
    }

    gWantsMouse = ImGui_WantsMouse() || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    EditorUI_EndFrame();
}

export bool EditorUI_WantsMouse()
{
    return gWantsMouse;
}

static const char* LightPrefix(LightType _t)
{
    switch (_t)
    {
    case LightType::Point: return "P_";
    case LightType::Directional: return "D_";
    default: return "S_";
    }
}
