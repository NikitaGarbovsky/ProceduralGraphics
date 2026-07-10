module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Perlin Noise scene. #TODO: implement perlin noise scene as per requirements.
/// </summary>
export module RendererScenes:PerlinNoise;

import :Core;

static void Perlin_DrawUI()
{
	ImGui::TextDisabled("Perlin noise is not implemented yet.");
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_PerlinNoise()
{
	SceneFuncs scene{};
	scene.name = "Perlin Noise";
	scene.description = "Not implemented yet.";
	scene.DrawUI = &Perlin_DrawUI;
	return scene;
}
