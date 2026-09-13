module;

// Normal imports
#include <vector>
#include "imgui.h"

/// <summary>
/// The Shadows scene. A noise terrain with a few models sat on it, lit by two directional
/// lights that each get their own shadow map.
///
/// The scene owns a lit shader that samples both maps. Everything the depth pass draws
/// casts a shadow, and anything drawn with this shader receives one, so the terrain
/// shadows itself where a hill blocks the light.
///
/// One model orbits on all three axes so the maps can be seen keeping up with it.
/// </summary>
export module RendererScenes:Shadows;

import :Core;
import RendererPass_Shadows;
import RendererAssetPipeline;
import RendererEntitys;
import RendererLights;
import RendererUtilities;
import TerrainGen;
import DebugUtilities;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Scene state
static GLuint SLitProgram = 0;
static GLint SBiasLoc = -1;
static GLint SPcfLoc = -1;

static uint32_t SMoverEntity = 0;
static bool SHasMover = false;
static bool SMoveEnabled = true;
static float STime = 0.0f;

static const glm::vec3 SMoverCenter = glm::vec3(0.0f, 14.0f, 0.0f);

// Makes the ground. Perlin noise straight into a mesh, no raw file like the terrain scene.
static void BuildGround()
{
	NoiseParams params{};
	params.width = 160;
	params.height = 160;
	params.octaves = 5;
	params.wavelength = 70.0f;
	params.seed = 2024; // Fixed seed so its consistent to compare results. 

	NoiseMap map{};
	if (!Noise_Generate(params, map))
		return;

	std::vector<float> heights = map.heights01;
	Terrain_SmoothHeights(heights, map.width, map.height, 3);

	std::vector<float> verts;
	std::vector<uint32_t> indices;
	float minY = 0.0f, maxY = 0.0f;

	// Uv tiling of 24 so the grass repeats instead of stretching over the whole thing.
	Terrain_BuildMeshData(heights, map.width, map.height, 1.0f, 18.0f, 24.0f,
		verts, indices, minY, maxY);

	const MeshID meshId = CreateMeshFromData_P3N3Uv2(
		verts.data(), (uint32_t)(verts.size() / 8),
		indices.data(), (uint32_t)indices.size());

	// From the shared texture cache, so the scene doesn't own or delete it.
	const GLuint grass = LoadTexture2D("Assets/Textures/Terrain/1_grass_mix_d.jpg");
	const MaterialID mat = CreateMaterial(SLitProgram, grass);

	const uint32_t first = (uint32_t)REntitySubmeshes.size();
	REntitySubmeshes.push_back(Submesh{ meshId, mat });

	CreateRenderEntity(first, 1, REntityMeshs[meshId].localBounds, glm::vec3(0.0f));
}

static void ShadowsScene_Init()
{
	STime = 0.0f;
	SHasMover = false;
	SMoveEnabled = true;
	GShadowTuning = ShadowTuning{};

	// 1. Shadow shader everything in this scene gets drawn with. It has to use the same
	// uniform names as model.vert so CreateMaterial can cache them.
	SLitProgram = LoadShaderProgram("Assets/Shaders/Scenes/ShadowLit.vert",
		"Assets/Shaders/Scenes/ShadowLit.frag");

	if (SLitProgram == 0)
	{
		LogWarning("ShadowsScene_Init: lit shader failed, scene will be empty.");
		return;
	}

	// The maps always sit on units 4 and 5, so these only need setting the once.
	glProgramUniform1i(SLitProgram, glGetUniformLocation(SLitProgram, "ShadowMap0"), kShadowMapUnit0);
	glProgramUniform1i(SLitProgram, glGetUniformLocation(SLitProgram, "ShadowMap1"), kShadowMapUnit1);

	SBiasLoc = glGetUniformLocation(SLitProgram, "ShadowBias");
	SPcfLoc = glGetUniformLocation(SLitProgram, "PCFRadius");

	// 2. Create terrain so it can showcase self-shadowing 
	BuildGround();

	// 3. A few models sat on top of it.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SLitProgram, glm::vec3(-18, 20, 4));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SLitProgram, glm::vec3(16, 22, 12));
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SLitProgram, glm::vec3(2, 24, -20));

	// 4. The one that moves. It goes in last so its id is easy to grab.
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", SLitProgram, SMoverCenter);
	SMoverEntity = (uint32_t)CurrentRenderedEntitys.size() - 1;
	SetEntityScale(SMoverEntity, glm::vec3(3.0f));
	SHasMover = true;

	// 5. Two directional lights. The scene system already made one just before Init ran,
	// so that one gets reused and only the second has to be created.
	const LightID sunA = GetLightCount() - 1;
	LightTransforms.position[sunA] = glm::vec3(0.0f, 60.0f, 0.0f);
	LightTransforms.rotation[sunA] = glm::vec3(-50.0f, 35.0f, 0.0f);
	SetLightColor(sunA, glm::vec3(1.0f, 0.94f, 0.82f));
	SetLightIntensity(sunA, 2.0f);

	const LightID sunB = CreateDirectionalLight(glm::vec3(0.0f, 60.0f, 0.0f),
		glm::vec3(-38.0f, -125.0f, 0.0f));
	SetLightColor(sunB, glm::vec3(0.40f, 0.55f, 1.0f));
	SetLightIntensity(sunB, 1.2f);

	// 6. Turn the shadow pass on and tell it what to work with.
	Shadows_Init();
	Shadows_SetLights(sunA, sunB);
	Shadows_SetReceiverProgram(SLitProgram);
}

static void ShadowsScene_Update(float _deltaTime)
{
	if (SLitProgram == 0)
		return;

	// Update any unifrom values that are changed. 
	glProgramUniform1f(SLitProgram, SBiasLoc, GShadowTuning.bias);
	glProgramUniform1i(SLitProgram, SPcfLoc, GShadowTuning.pcfRadius);

	if (!SHasMover || !SMoveEnabled)
		return;

	// Move the one model on all three axes so the maps have to keep up with it.
	STime += _deltaTime;

	glm::vec3 p = SMoverCenter;
	p.x += sinf(STime * 0.6f) * 22.0f;
	p.y += (sinf(STime * 1.3f) * 0.5f + 0.5f) * 10.0f;
	p.z += cosf(STime * 0.6f) * 22.0f;

	SetEntityPosition(SMoverEntity, p);
}

static void ShadowsScene_DrawUI()
{
	if (SLitProgram == 0)
	{
		ImGui::TextDisabled("Lit shader failed to load.");
		return;
	}

	ImGui::TextDisabled("Two directional lights, one shadow map each.");
	ImGui::Checkbox("Move the model", &SMoveEnabled);

	ImGui::SeparatorText("Quality");

	float labelRoom = ImGui::CalcTextSize("Shadow Bias").x + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::PushItemWidth(-labelRoom);

	// Too low and surfaces shadow themselves, too high and shadows detach from the object.
	ImGui::SliderFloat("Shadow Bias", &GShadowTuning.bias, 0.0f, 0.01f, "%.5f");
	ImGui::SliderInt("PCF Radius", &GShadowTuning.pcfRadius, 0, 4);

	ImGui::SeparatorText("Light Box");
	ImGui::TextDisabled("Smaller box means sharper shadows over \nless of the scene.");
	ImGui::SliderFloat("Ortho Size", &GShadowTuning.orthoHalfSize, 30.0f, 300.0f);
	ImGui::SliderFloat("Eye Distance", &GShadowTuning.eyeDistance, 50.0f, 500.0f);
	ImGui::SliderFloat("Far Plane", &GShadowTuning.farPlane, 100.0f, 1500.0f);

	ImGui::PopItemWidth();

	if (ImGui::Button("Reset To Defaults", ImVec2(-1.0f, 0.0f)))
		GShadowTuning = ShadowTuning{};
}

static void ShadowsScene_Shutdown()
{
	// The pass owns its framebuffers and textures, so it gets torn down with the scene.
	Shadows_Shutdown();

	if (SLitProgram != 0)
	{
		glDeleteProgram(SLitProgram);
		SLitProgram = 0;
	}

	SHasMover = false;
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Shadows()
{
	SceneFuncs scene{};
	scene.name = "Shadows";
	scene.description = "Two directional lights, each with its own shadow map rendered from the\nlights point of view.";
	scene.Init = &ShadowsScene_Init;
	scene.Update = &ShadowsScene_Update;
	scene.DrawUI = &ShadowsScene_DrawUI;
	scene.Shutdown = &ShadowsScene_Shutdown;
	return scene;
}