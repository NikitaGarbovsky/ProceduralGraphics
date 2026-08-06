module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Post Processing scene. Fills the view with models, and colored lights so
/// the screen effects have something worth looking at.
///
/// The effects themselves live in RendererPass_PostProcess, because they run on the finished
/// frame and not on anything this scene owns. All this scene does is pick which one is running
/// and draw its sliders.
/// </summary>
export module RendererScenes:PostProcess;

import :Core;
import RendererPass_PostProcess;
import RendererAssetPipeline;
import RendererEntitys;
import RendererLights;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;


static void PostFX_Init() {
	// Ground plus a couple of models, same ones the sandbox uses.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/plane.fbx", SceneDefaultProgram, glm::vec3(0, 0, 0));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram, glm::vec3(-4, 1, 0));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram, glm::vec3(4, 1, 0));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram, glm::vec3(10, 1, 0));

	// Two strong colored lights so the color effects have something obvious to change.
	LightID warm = CreatePointLight(glm::vec3(-6.0f, 4.0f, 3.0f));
	SetLightColor(warm, glm::vec3(1.0f, 0.35f, 0.15f));
	SetLightIntensity(warm, 10.0f);
	SetLightRange(warm, 20.0f);

	LightID cool = CreatePointLight(glm::vec3(6.0f, 4.0f, 3.0f));
	SetLightColor(cool, glm::vec3(0.2f, 0.5f, 1.0f));
	SetLightIntensity(cool, 10.0f);
	SetLightRange(cool, 20.0f);
}

static void PostFX_DrawUI() {
	ImGui::TextDisabled("Tab cycles through the effects.");

	const uint32_t active = PostFX_ActiveIndex();

	// The effect list. 
	for (uint32_t i = 0; i < PostFX_Count(); ++i) {
		const bool isActive = (i == active);
		if (isActive)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.45f, 0.85f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.52f, 0.92f, 1.0f));
		}

		// Effects whose shader failed to load get greyed out instead of showing a black screen.
		ImGui::BeginDisabled(PostFX_IsBroken(i));
		if (ImGui::Button(PostFX_Name(i), ImVec2(-1.0f, 0.0f)))
			PostFX_SetActive(i);
		ImGui::EndDisabled();

		if (isActive)
			ImGui::PopStyleColor(2);

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(360.0f);
			ImGui::TextUnformatted(PostFX_Description(i));
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	ImGui::Separator();

	// Sliders for whatever the active effect exposes. 
	PostParam* params = PostFX_ActiveParams();
	const uint32_t paramCount = PostFX_ActiveParamCount();

	if (paramCount == 0)
		ImGui::TextDisabled("No settings for this effect.");

	for (uint32_t i = 0; i < paramCount; ++i)
		ImGui::SliderFloat(params[i].label, &params[i].value, params[i].minValue, params[i].maxValue);
}

static void PostFX_Shutdown() {
	// Currently nothing to clear
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_PostProcess() {
	SceneFuncs scene{};
	scene.name = "Post FX";
	scene.description = "The whole scene is rendered into a framebuffer, then the finished frame runs\nthrough a screen effect. Tab cycles through them.";
	scene.Init = &PostFX_Init;
	scene.DrawUI = &PostFX_DrawUI;
	scene.Shutdown = &PostFX_Shutdown;
	return scene;
}
