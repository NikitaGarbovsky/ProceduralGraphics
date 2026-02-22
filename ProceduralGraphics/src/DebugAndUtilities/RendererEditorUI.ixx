module;

// Normal imports
#include <cstdint>
#include <glfw3.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include "imgui.h"
#include "ImGuizmo.h"
#include <string>
#include <filesystem>
#include <imgui_internal.h>
#include <cctype> // tolower

// This module manages the renderers editor user interface, which is using the Dear-Imgui library. 
// Gizmos are created & managed using a seperate library addon for Dear-Imgui. (ImGuizmo)
export module RendererEditorUI;

// Codebase imports
import RendererImgui;
import RendererEntitys;
import RendererTransformUtils;
import RendererData;
import RendererPass_DebugBounds;
import RendererLights;
import DebugUtilities;
import RendererAssetPipeline;
import RendererCamera;

export bool EditorUIEnabled = false;

// State variables for types of input
static ImGuizmo::OPERATION gTformGuizmoCurrentOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE      gAxisType = ImGuizmo::LOCAL;
static bool gWantsMouse = false;
ImFont* glarge_font;
ImFont* gsmall_font;

static bool showBounds = false;
static bool BoundsSelectedOnly = false;

// Asset Browser state
static bool isAssetBrowserOpen = false;
static std::filesystem::path AssetRootPath = "Assets/Models";
static std::filesystem::path CurrentAssetBrowserDirectory = AssetRootPath;
static char AssetSearchArr[128] = "";

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

// ------------------------------ Content Browser helpers ------------------------------
static bool IsModelFile(const std::filesystem::path& p)
{
    auto ext = p.extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return (ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj");
}

static bool PassesSearch(const std::filesystem::path& p)
{
    if (AssetSearchArr[0] == 0) return true;

    std::string name = p.filename().string();
    std::string needle = AssetSearchArr;
    for (char& c : name)   c = (char)std::tolower((unsigned char)c);
    for (char& c : needle) c = (char)std::tolower((unsigned char)c);

    return name.find(needle) != std::string::npos;
}

static void GatherChildDirs(const std::filesystem::path& dir, std::vector<std::filesystem::path>& out)
{
    out.clear();
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec) break;
        if (e.is_directory(ec)) out.push_back(e.path());
    }
    std::sort(out.begin(), out.end(),
        [](auto& a, auto& b) { return a.filename().string() < b.filename().string(); });
}

static void DrawDirNodeRecursive(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> kids;
    GatherChildDirs(dir, kids);

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanFullWidth;

    if (dir == CurrentAssetBrowserDirectory) flags |= ImGuiTreeNodeFlags_Selected;

    const std::string label = dir.filename().empty()
        ? dir.string()
        : dir.filename().string();

    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked())
        CurrentAssetBrowserDirectory = dir;

    if (open)
    {
        for (auto& k : kids)
            DrawDirNodeRecursive(k);

        ImGui::TreePop();
    }
}

static void DrawAssetBrowser()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();

    // --- Bottom "Content Browser" TAB  ---
    ImVec2 tabPos = ImVec2(vp->Pos.x + 12.0f, vp->Pos.y + vp->Size.y - 40.0f);
    ImVec2 tabSize = ImVec2(160.0f, 28.0f);

    ImGui::SetNextWindowPos(tabPos);
    ImGui::SetNextWindowSize(tabSize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags tabFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##AssetBrowserTab", nullptr, tabFlags);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::PushFont(gsmall_font);
   
    if (ImGui::Button("Asset Browser"))
        isAssetBrowserOpen = !isAssetBrowserOpen;

    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    ImGui::End();

    if (!isAssetBrowserOpen)
        return;

    // --- Popup-like window anchored above the tab ---
    ImVec2 winPos = ImVec2(tabPos.x, tabPos.y - 520.0f);
    ImVec2 winSize = ImVec2(760.0f, 500.0f);

    ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);

    ImGuiWindowFlags winFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Asset Browser", &isAssetBrowserOpen, winFlags);

    ImGui::PushFont(gsmall_font);
    
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##search", "Search...", AssetSearchArr, IM_ARRAYSIZE(AssetSearchArr));
    ImGui::Separator();

    
    float leftW = 240.0f;

    ImGui::BeginChild("##dir_tree", ImVec2(leftW, 0), true);
    {
        std::error_code ec;
        if (!std::filesystem::exists(AssetRootPath, ec) || !std::filesystem::is_directory(AssetRootPath, ec))
        {
            ImGui::TextUnformatted("Assets root missing: Assets/Models");
        }
        else
        {
            ImGuiTreeNodeFlags rootFlags =
                ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanFullWidth;

            if (CurrentAssetBrowserDirectory == AssetRootPath) rootFlags |= ImGuiTreeNodeFlags_Selected;

            bool rootOpen = ImGui::TreeNodeEx("Assets/Models", rootFlags);
            if (ImGui::IsItemClicked()) CurrentAssetBrowserDirectory = AssetRootPath;

            if (rootOpen)
            {
                std::vector<std::filesystem::path> kids;
                GatherChildDirs(AssetRootPath, kids);
                for (auto& k : kids) DrawDirNodeRecursive(k);
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##items", ImVec2(0, 0), true);
    {

        std::vector<std::filesystem::directory_entry> dirs;
        std::vector<std::filesystem::directory_entry> files;

        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(CurrentAssetBrowserDirectory, ec))
        {
            if (ec) break;
            if (e.is_directory(ec)) dirs.push_back(e);
            else files.push_back(e);
        }

        auto sortByName = [](auto& a, auto& b)
            {
                return a.path().filename().string() < b.path().filename().string();
            };

        std::sort(dirs.begin(), dirs.end(), sortByName);
        std::sort(files.begin(), files.end(), sortByName);

        // Files
        for (auto& e : files)
        {
            auto p = e.path();
            if (!IsModelFile(p)) continue;
            if (!PassesSearch(p)) continue;

            const std::string name = p.filename().string();
            const std::string fullPath = p.generic_string();

            ImGui::Selectable(name.c_str(), false);

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("MODEL_PATH", fullPath.c_str(), (size_t)fullPath.size() + 1);
                ImGui::TextUnformatted(name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    ImGui::EndChild();

    // Auto-hide only when NOT pinned 
    bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::PopFont();
    ImGui::End();

    if (isAssetBrowserOpen)
    {
        // Don't auto-close while dragging
        if (!ImGui::IsDragDropActive())
        {
            // Click outside closes
            if (!windowHovered && !windowFocused && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                isAssetBrowserOpen = false;

            // ESC closes, #TODO maybe Re-add this if you remove esc to close renderer application. 
            /*if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                sAssetBrowserOpen = false;*/
        }
    }
}

// Immediate mode UI, draws the entire UI per frame.
export void EditorUI_Draw(const glm::mat4& _view, const glm::mat4& _proj, int _viewportW, int _viewportH, uint32_t _selectedEntityID, uint32_t _selectedLight) {
    EditorUI_BeginFrame();

    ImGuiWindowFlags windowFlags0 = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    // =========== Entity Window ===========
    ImGui::PushFont(glarge_font);

    int x1, y1;
    glfwGetFramebufferSize(MainWindow, &x1, &y1);
    x1 -= x1 / 6;
    y1 -= (y1 / 6) * 5;
    ImVec2 pos = ImVec2((float)x1, (float)y1);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(250, 450));
    ImGui::Begin("Entities", nullptr, windowFlags0);

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

    // =========== Selected Entity Panel ===========
    if (_selectedEntityID != UINT32_MAX) {
        ImGui::PushFont(glarge_font);
        std::string str = "Entity: ";
        str += std::to_string(_selectedEntityID + 1);
        ImGui::SetNextWindowSize(ImVec2(250, 200));

        int x, y;
        glfwGetFramebufferSize(MainWindow, &x, &y);
        x -= x / 6;
        y -= (y / 5) * 2;

        ImVec2 pos2 = ImVec2((float)x, (float)y);
        ImGui::SetNextWindowPos(pos2);
        ImGui::Begin(str.c_str(), nullptr, windowFlags0);

        ImGui::DragFloat("X", &EntityTransforms.position[_selectedEntityID].x);
        ImGui::DragFloat("Y", &EntityTransforms.position[_selectedEntityID].y);
        ImGui::DragFloat("Z", &EntityTransforms.position[_selectedEntityID].z);

        ImGui::PopFont();
        ImGui::End();
    }

    // =========== Lights Windows ===========
    ImGuiWindowFlags windowFlagsL = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(280, 220));
    ImGui::Begin("Create", nullptr, windowFlagsL);
    ImGui::PushFont(gsmall_font);
    ImGui::Text("%.2f ms/frame (%.1f FPS)",
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
    ImGui::PushFont(gsmall_font);
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
    ImGui::PopFont();
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

    // Content Browser 
    DrawAssetBrowser();

    GizmoShortcuts();

    // =========== Gizmos ===========
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

    // Drop target overlay 
    if (ImGui::IsDragDropActive())
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({ 0,0 });
        ImGui::SetNextWindowSize(vp->Size);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

        ImGui::Begin("viewport_drop_target", nullptr, flags);

        ImGui::InvisibleButton("##drop_area", vp->Size);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(
                "MODEL_PATH",
                ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                const char* path = static_cast<const char*>(p->Data);

                if (p->IsDelivery())
                {
                    std::string droppedPath(path ? path : "");
                    glm::vec3 spawnPos = GEditorCam.position + GEditorCam.forward * 10.0f;
                    LoadModel_AsREntities_P3N3Uv2(droppedPath.c_str(), RenderObjProgram, spawnPos);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
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
