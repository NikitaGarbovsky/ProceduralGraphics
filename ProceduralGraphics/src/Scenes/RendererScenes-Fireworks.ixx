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

	if (ImGui::Button("Launch Fireworks", ImVec2(-1.0f, 0.0f)))
		Fireworks_Launch();

	ImGui::Separator();
	ImGui::TextDisabled("Gravity and spark life apply live, the rest kick in on the next volley.");

	FireworkTuning& t = GFireworkTuning;

	float labelRoom = ImGui::CalcTextSize("Climb Speed Min").x + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::PushItemWidth(-labelRoom);

	ImGui::TextDisabled("Launch");
	if (ImGui::SliderFloat("Climb Speed Min", &t.launchSpeedMin, 4.0f, 25.0f))
		t.launchSpeedMax = glm::max(t.launchSpeedMax, t.launchSpeedMin);
	if (ImGui::SliderFloat("Climb Speed Max", &t.launchSpeedMax, 4.0f, 25.0f))
		t.launchSpeedMin = glm::min(t.launchSpeedMin, t.launchSpeedMax);
	ImGui::SliderFloat("Head Gravity", &t.headGravity, 0.0f, 10.0f);
	if (ImGui::SliderFloat("Fuse Min", &t.trailTimeMin, 0.3f, 4.0f))
		t.trailTimeMax = glm::max(t.trailTimeMax, t.trailTimeMin);
	if (ImGui::SliderFloat("Fuse Max", &t.trailTimeMax, 0.3f, 4.0f))
		t.trailTimeMin = glm::min(t.trailTimeMin, t.trailTimeMax);

	ImGui::TextDisabled("Trail");
	ImGui::SliderFloat("Trail Spark Life", &t.trailLife, 0.05f, 1.5f);
	ImGui::SliderFloat("Trail Droop", &t.trailGravity, 0.0f, 10.0f);

	ImGui::TextDisabled("Burst");
	ImGui::SliderFloat("Burst Speed", &t.burstSpeed, 2.0f, 20.0f);
	ImGui::SliderFloat("Burst Spark Life", &t.burstLife, 0.5f, 6.0f);
	ImGui::SliderFloat("Burst Gravity", &t.burstGravity, 0.0f, 15.0f);

	ImGui::PopItemWidth();

	// Fresh struct means the shipped defaults come back.
	if (ImGui::Button("Reset To Defaults", ImVec2(-1.0f, 0.0f)))
		GFireworkTuning = FireworkTuning{};
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
