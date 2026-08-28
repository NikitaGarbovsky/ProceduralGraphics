module;

// Normal imports
#include "imgui.h"
#include <glfw3.h>

/// <summary>
/// The Fireworks scene. Just a ground plane. Press F and a volley of gpu
/// fireworks goes up. All the particle machinery lives in RendererPass_Fireworks, this
/// scene just owns its lifetime, feeds it the frame time, and listens for the input key.
/// </summary>
 
export module RendererScenes:Fireworks;

import :Core;
import RendererPass_Fireworks;
import RendererAssetPipeline;
import RendererInput;
import <glm.hpp>;

static void FireworksScene_Init() {
	// Ground so the fireworks have something to launch from.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/plane.fbx", SceneDefaultProgram, glm::vec3(0, 0, 0));

	// Buffers, programs and the vao all get made here and deleted in Shutdown.
	Fireworks_Init();
}

static void FireworksScene_Update(float _deltaTime) {
	// F sends up a fresh volley.
	if (KeyPressed(GLFW_KEY_F))
		Fireworks_Launch();

	Fireworks_Update(_deltaTime);
}

static void FireworksScene_DrawUI() {
	ImGui::TextDisabled("Press F to launch a volley of fireworks.");
	ImGui::Text("In flight: %d", Fireworks_ActiveCount());

	if (ImGui::Button("Launch Volley", ImVec2(-1.0f, 0.0f)))
		Fireworks_Launch();
}

static void FireworksScene_Shutdown() {
	// The particle system owns gl resources, so it gets torn down with the scene.
	Fireworks_Shutdown();
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Fireworks() {
	SceneFuncs scene{};
	scene.name = "Fireworks";
	scene.description = "A compute shader moves every particle on the gpu.\nPress F to launch a volley, each firework gets a random color and fuse time.";
	scene.Init = &FireworksScene_Init;
	scene.Update = &FireworksScene_Update;
	scene.DrawUI = &FireworksScene_DrawUI;
	scene.Shutdown = &FireworksScene_Shutdown;
	return scene;
}
