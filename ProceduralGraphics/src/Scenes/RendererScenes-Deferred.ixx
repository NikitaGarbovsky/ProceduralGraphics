module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Deferred Rendering scene.
/// 
/// As per the required amount of lights, transparent objects & models,
/// all runnning through the deferred path.
///
/// Point lights get a semi-transparent ball using normal forward rendering 
/// after the deferred work is done. (spawned in point lights also spawn with one)
/// </summary>
export module RendererScenes:Deferred;

import :Core;
import RendererPass_Deferred;
import RendererAssetPipeline;
import RendererData;
import RendererEntitys;
import RendererLights;
import DebugUtilities;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Scene state
static int SPreviousSamples = 8;

// How the grid of models is laid out.
constexpr int kGridCols = 5;
constexpr int kGridRows = 4;
constexpr float kGridSpacing = 5.0f;

// The ten light colors, spread across the rainbow so the falloff is easy to see.
static const glm::vec3 SLightColors[10] = {
	glm::vec3(1.0f, 0.2f, 0.2f), glm::vec3(1.0f, 0.6f, 0.1f),
	glm::vec3(1.0f, 1.0f, 0.2f), glm::vec3(0.3f, 1.0f, 0.2f),
	glm::vec3(0.1f, 1.0f, 0.7f), glm::vec3(0.2f, 0.8f, 1.0f),
	glm::vec3(0.3f, 0.3f, 1.0f), glm::vec3(0.7f, 0.2f, 1.0f),
	glm::vec3(1.0f, 0.2f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f)
};

static void Deferred_SceneInit()
{
	GDeferredTuning = DeferredTuning{};

	SPreviousSamples = ViewportSamples;
	ViewportSamples = 1;

	// The ground.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/plane.fbx", SceneDefaultProgram, glm::vec3(0.0f));
	SetEntityScale((uint32_t)CurrentRenderedEntitys.size() - 1, glm::vec3(2.0f));

	// Load the model once.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SceneDefaultProgram,
		glm::vec3(0.0f, 1.0f, 0.0f));

	const REntity source = CurrentRenderedEntitys.back();
	const uint32_t sourceEntity = (uint32_t)CurrentRenderedEntitys.size() - 1;

	// Lay the grid out and move the one that already exists into the first slot. The rest
	// point at the same submeshes, so they all end up in one instanced draw.
	const float halfW = (kGridCols - 1) * kGridSpacing * 0.5f;
	const float halfD = (kGridRows - 1) * kGridSpacing * 0.5f;

	for (int row = 0; row < kGridRows; ++row)
	{
		for (int col = 0; col < kGridCols; ++col)
		{
			const glm::vec3 pos(-halfW + col * kGridSpacing, 1.0f, -halfD + row * kGridSpacing);

			if (row == 0 && col == 0)
			{
				SetEntityPosition(sourceEntity, pos);
				continue;
			}

			CreateRenderEntity(source.firstSubmesh, source.submeshCount, source.localBounds, pos);
		}
	}

	// Ten point lights hovering over the grid.
	for (int i = 0; i < 10; ++i) {
		const float t = (float)i / 10.0f;
		const float angle = t * 6.2831853f;

		const glm::vec3 pos(sinf(angle) * 9.0f, 2.5f, cosf(angle) * 7.0f);

		const LightID light = CreatePointLight(pos);
		SetLightColor(light, SLightColors[i]);
		SetLightIntensity(light, 9.0f);

		SetLightRange(light, 9.0f);
	}

	// The scene system makes a directional light by default just before Init runs. 
	// So just set it to a low value so the point lights in the scene are visible.
	SetLightIntensity(GetLightCount() - 11, 0.12f);

	// Last, switch the path over.
	Deferred_Init();
}

static void Deferred_SceneDrawUI()
{
	if (!Deferred_IsActive())
	{
		ImGui::TextDisabled("Deferred path failed to start.");
		return;
	}

	float labelRoom = ImGui::CalcTextSize("Ambient Strength").x + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::PushItemWidth(-labelRoom);

	ImGui::SeparatorText("Lighting");
	ImGui::SliderFloat("Shininess", &GDeferredTuning.shininess, 1.0f, 256.0f);
	ImGui::SliderFloat("Ambient Strength", &GDeferredTuning.ambientStrength, 0.0f, 0.5f);

	ImGui::SeparatorText("Light Sources");
	ImGui::Checkbox("Draw Light Balls", &GDeferredTuning.drawLightProxies);
	ImGui::SliderFloat("Ball Size", &GDeferredTuning.proxyRadius, 0.1f, 2.0f);
	ImGui::SliderFloat("Ball Opacity", &GDeferredTuning.proxyOpacity, 0.05f, 1.0f);

	ImGui::PopItemWidth();

	if (ImGui::Button("Reset To Defaults", ImVec2(-1.0f, 0.0f)))
		GDeferredTuning = DeferredTuning{};
}

static void Deferred_SceneShutdown()
{
	// Turning this off is what resets the pipeline back on the forward path.
	Deferred_Shutdown();

	ViewportSamples = SPreviousSamples;
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Deferred()
{
	SceneFuncs scene{};
	scene.name = "Deferred";
	scene.description = "All the geometry is written into a set of textures first, then every\npixel gets lit once from those. Ten point lights, each drawn as a\nsee-through ball on top with forward rendering.";
	scene.Init = &Deferred_SceneInit;
	scene.DrawUI = &Deferred_SceneDrawUI;
	scene.Shutdown = &Deferred_SceneShutdown;
	return scene;
}