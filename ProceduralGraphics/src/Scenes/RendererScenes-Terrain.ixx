module;

// Normal imports
#include <fstream>
#include <string>
#include <vector>
#include "imgui.h"

/// <summary>
/// The Terrain scene. Generates a perlin noise heightmap, saves it as noise.raw, then reads
/// that file back and builds a terrain mesh from it.
/// 
/// The heights get smoothed, turned into a centered grid of vertices with central
/// difference normals, and rendered with a splat shader that blends four textures by height,
/// lit by the scene lights.
/// </summary>
export module RendererScenes:Terrain;

import :Core;
import TerrainGen;
import RendererAssetPipeline;
import RendererEntitys;
import RendererUtilities;
import DebugUtilities;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Scene state
static NoiseParams SParams{};
static NoiseMap SMap{};
static int SSmoothIterations = 2;    // 3x3 average passes over the heights
static float SCellSpacing = 1.0f;    // World units between grid vertices
static float SHeightScale = 60.0f;   // World height of a max value pixel

static GLuint STerrainProgram = 0;
static GLint STex1Loc = -1, STex2Loc = -1, STex3Loc = -1;
static GLint SMinHLoc = -1, SMaxHLoc = -1;
static GLuint STerrainTextures[4] = {}; // grass, dirt, rock, snow. 

static MeshID SMeshId = 0;
static uint32_t SEntityId = 0;
static bool SHasTerrain = false;

// Reads the 8 bit RAW heightmap back off disk into [0,1] floats.
// Falls back to the in memory heights if the file can't be read for some reason.
static bool LoadHeightmapRAW(const char* _path, int _width, int _height, std::vector<float>& _out)
{
	const size_t count = (size_t)_width * (size_t)_height;
	std::vector<unsigned char> bytes(count);

	std::ifstream file(_path, std::ios::binary);
	if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), (std::streamsize)count))
	{
		LogWarning("LoadHeightmapRAW: could not read the raw file, using the in memory heights.");
		_out = SMap.heights01;
		return !_out.empty();
	}

	_out.resize(count);
	for (size_t i = 0; i < count; ++i)
		_out[i] = (float)bytes[i] / 255.0f;
	return true;
}

// The full pipeline: generate noise, save the files, read the RAW back, smooth, build the
// mesh, and update the shader's height range. Works for both first build and rebuilds.
static void Terrain_Rebuild()
{
	// 1. Fresh noise, saved to disk
	if (!Noise_Generate(SParams, SMap))
		return;
	Noise_SaveRAW(SMap, kNoiseRawPath);
	Noise_SaveJPG(SMap, kNoiseJpgPath);

	// 2. Read the RAW file back as the heightmap
	std::vector<float> heights;
	if (!LoadHeightmapRAW(kNoiseRawPath, SMap.width, SMap.height, heights))
		return;

	// 3. Smooth it out. The RAW only has 256 height steps so this hides the stepping.
	Terrain_SmoothHeights(heights, SMap.width, SMap.height, SSmoothIterations);

	// 4. Build the vertex grid
	std::vector<float> verts;
	std::vector<uint32_t> indices;
	float minY = 0.0f, maxY = 0.0f;
	Terrain_BuildMeshData(heights, SMap.width, SMap.height, SCellSpacing, SHeightScale, 1.0f,
		verts, indices, minY, maxY);

	// 5. Upload the mesh. First build makes a fresh one. Rebuilds free the old GPU buffers,
	//    then move the newly made mesh into the old slot so the mesh id never changes.
	const MeshID newId = CreateMeshFromData_P3N3Uv2(
		verts.data(), (uint32_t)(verts.size() / 8), indices.data(), (uint32_t)indices.size());

	if (SHasTerrain)
	{
		glDeleteVertexArrays(1, &REntityMeshs[SMeshId].vao);
		glDeleteBuffers(1, &REntityMeshs[SMeshId].vbo);
		glDeleteBuffers(1, &REntityMeshs[SMeshId].ebo);

		REntityMeshs[SMeshId] = REntityMeshs[newId];
		REntityMeshs.pop_back();

		// The size changed so the culling bounds need updating too
		CurrentRenderedEntitys[SEntityId].localBounds = REntityMeshs[SMeshId].localBounds;
	}
	else
	{
		SMeshId = newId;
	}

	// 6. Tell the shader the new height range so the texture bands line up
	glProgramUniform1f(STerrainProgram, SMinHLoc, minY);
	glProgramUniform1f(STerrainProgram, SMaxHLoc, maxY);
}

static void Terrain_Init()
{
	SParams = NoiseParams{};
	SSmoothIterations = 2;
	SCellSpacing = 1.0f;
	SHeightScale = 60.0f;
	SHasTerrain = false;

	// The terrain gets its own shader program, made on load and deleted on shutdown.
	STerrainProgram = LoadShaderProgram("Assets/Shaders/Scenes/Terrain.vert", "Assets/Shaders/Scenes/Terrain.frag");
	if (STerrainProgram == 0)
	{
		LogWarning("Terrain_Init: terrain shader failed, scene will be empty.");
		return;
	}

	// Tex0 (grass) gets bound by the render pass. The other three samplers point at
	// texture units 1 to 3 which the scene binds itself every frame.
	STex1Loc = glGetUniformLocation(STerrainProgram, "Tex1");
	STex2Loc = glGetUniformLocation(STerrainProgram, "Tex2");
	STex3Loc = glGetUniformLocation(STerrainProgram, "Tex3");
	SMinHLoc = glGetUniformLocation(STerrainProgram, "MinHeight");
	SMaxHLoc = glGetUniformLocation(STerrainProgram, "MaxHeight");
	glProgramUniform1i(STerrainProgram, STex1Loc, 1);
	glProgramUniform1i(STerrainProgram, STex2Loc, 2);
	glProgramUniform1i(STerrainProgram, STex3Loc, 3);

	// The four splat layers, low to high. These come from the shared texture cache so the
	// scene doesn't own or delete them.
	STerrainTextures[0] = LoadTexture2D("Assets/Textures/Terrain/1_grass_mix_d.jpg");
	STerrainTextures[1] = LoadTexture2D("Assets/Textures/Terrain/2_mntn_brown_d.jpg");
	STerrainTextures[2] = LoadTexture2D("Assets/Textures/Terrain/3_mntn_forest_d.jpg");
	STerrainTextures[3] = LoadTexture2D("Assets/Textures/Terrain/4_snow_rough_s.jpg");

	// Build the first terrain, then wrap it in an entity
	Terrain_Rebuild();
	if (SMap.width == 0)
		return;

	MaterialID mat = CreateMaterial(STerrainProgram, STerrainTextures[0]);

	const uint32_t first = (uint32_t)REntitySubmeshes.size();
	REntitySubmeshes.push_back(Submesh{ SMeshId, mat });

	SEntityId = CreateRenderEntity(first, 1, REntityMeshs[SMeshId].localBounds, glm::vec3(0.0f));
	SHasTerrain = true;
}

static void Terrain_Update(float _deltaTime)
{
	// Keep the extra splat textures bound. 
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, STerrainTextures[1]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, STerrainTextures[2]);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, STerrainTextures[3]);
	glActiveTexture(GL_TEXTURE0);
}

static void Terrain_DrawUI()
{
	// Noise settings
	ImGui::DragInt("Width", &SParams.width, 8.0f, 64, 1024);
	ImGui::DragInt("Height", &SParams.height, 8.0f, 64, 1024);
	ImGui::SliderInt("Octaves", &SParams.octaves, 1, 10);
	ImGui::DragFloat("Wavelength", &SParams.wavelength, 1.0f, 8.0f, 512.0f);
	ImGui::SliderFloat("Gain", &SParams.gain, 0.1f, 0.9f);
	ImGui::SliderFloat("Lacunarity", &SParams.lacunarity, 1.5f, 3.0f);

	int seed = (int)SParams.seed;
	if (ImGui::InputInt("Seed (0 = time)", &seed))
		SParams.seed = (uint32_t)(seed < 0 ? 0 : seed); // no negatives allowed

	ImGui::Separator();

	// Terrain settings
	ImGui::SliderInt("Smoothing", &SSmoothIterations, 0, 10);
	ImGui::DragFloat("Cell Spacing", &SCellSpacing, 0.05f, 0.25f, 4.0f);
	ImGui::DragFloat("Height Scale", &SHeightScale, 1.0f, 1.0f, 200.0f);

	if (SMap.width > 0)
		ImGui::TextDisabled("Last: seed %u, %.2fs, %d verts",
			SMap.seedUsed, SMap.generationSeconds, SMap.width * SMap.height);

	if (ImGui::Button("Regenerate Terrain", ImVec2(-1.0f, 0.0f)) && SHasTerrain)
		Terrain_Rebuild();
}

static void Terrain_Shutdown() {
	// The shader program is the scene's own. The mesh, material and entity roll back
	// automatically, and the splat textures belong to the shared cache.
	if (STerrainProgram != 0) {
		glDeleteProgram(STerrainProgram);
		STerrainProgram = 0;
	}
	SHasTerrain = false;
	SMap = NoiseMap{};
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_Terrain() {
	SceneFuncs scene{};
	scene.name = "Terrain";
	scene.description = "Terrain mesh built from a perlin noise heightmap read from noise.raw.\nFour textures blended by height, lit by the scene lights.";
	scene.Init = &Terrain_Init;
	scene.Update = &Terrain_Update;
	scene.DrawUI = &Terrain_DrawUI;
	scene.Shutdown = &Terrain_Shutdown;
	return scene;
}
