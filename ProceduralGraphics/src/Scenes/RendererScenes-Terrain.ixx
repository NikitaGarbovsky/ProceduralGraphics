module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Terrain scene. #TODO: implement terrin gen scene stuff here.
/// </summary>
export module RendererScenes:Terrain;

import :Core;

static void Terrain_DrawUI()
{
	ImGui::TextDisabled("Not implemented yet.");
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Terrain()
{
	SceneFuncs scene{};
	scene.name = "Terrain";
	scene.description = "Not implemented yet.";
	scene.DrawUI = &Terrain_DrawUI;
	return scene;
}
