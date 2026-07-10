module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Sandbox scene. The default startup content.
/// A free space to test the editor, drag in models, place lights etc.
/// </summary>
export module RendererScenes:Sandbox;

import :Core;
import RendererAssetPipeline;
import <glm.hpp>;

static void Sandbox_Init()
{
	// Load these models on startup
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram, glm::vec3(-1, 1, -1));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram, glm::vec3(-10, 1, -1));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/plane.fbx", SceneDefaultProgram, glm::vec3(0, 0, 0));
}

static void Sandbox_DrawUI()
{
	ImGui::TextDisabled("No scene specific controls.");
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Sandbox()
{
	SceneFuncs scene{};
	scene.name = "Sandbox";
	scene.description = "Free sandbox with the default test models.\nDrag more models in from the Asset Browser.";
	scene.Init = &Sandbox_Init;
	scene.DrawUI = &Sandbox_DrawUI;
	return scene;
}
