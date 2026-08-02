module;

// Normal imports
#include "imgui.h"

/// <summary>
/// The Perlin Noise scene. Generates a perlin noise texture on load, saves it as noise.raw
/// and noise.jpg, and shows it on three quads: raw greyscale, a fire color gradient, and an
/// animated version that lerps through the noise gradient.
/// </summary>
export module RendererScenes:PerlinNoise;

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
static GLuint SNoiseTex = 0;
static MaterialID SQuadMats[3] = {};
static int SQuadCount = 0;

// The three quad programs, made on load and deleted on shutdown.
static GLuint SProgGreyscale = 0;
static GLuint SProgColor = 0;
static GLuint SProgAnimated = 0;
static GLint STimeLoc = -1;
static float STime = 0.0f;

// Makes a GL texture from the 8 bit grayscale noise data. Stored as a single red channel
// with a swizzle so shaders reading .rgb still show grayscale. The scene owns this texture
// and deletes it itself, its not part of the shared texture cache.
static GLuint CreateGrayscaleTexture(int _width, int _height, const unsigned char* _gray)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, _width, _height, 0, GL_RED, GL_UNSIGNED_BYTE, _gray);

	// Make .g and .b read the red channel too so everything samples as grayscale
	GLint swizzle[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

// Makes a 1x1 quad facing the camera (+Z) with the given program and texture, scaled up
// through the transform. Gives back the material id so the texture can be swapped later.
static uint32_t CreateNoiseQuad(GLuint _program, GLuint _texture, const glm::vec3& _pos,
	const glm::vec3& _scale, MaterialID& _outMaterial)
{
	// P3 N3 UV2, matches CreateMeshFromData_P3N3Uv2's layout
	const float verts[] = {
		-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
	};
	const uint32_t indices[] = { 0, 1, 2,  0, 2, 3 };

	MeshID meshId = CreateMeshFromData_P3N3Uv2(verts, 4, indices, 6);
	_outMaterial = CreateMaterial(_program, _texture);

	const uint32_t first = (uint32_t)REntitySubmeshes.size();
	REntitySubmeshes.push_back(Submesh{ meshId, _outMaterial });

	// The scale happens through the transform, so the bounds radius covers the scaled quad
	// in case the culler doesn't apply scale.
	const float maxScale = glm::max(_scale.x, glm::max(_scale.y, _scale.z));
	Bounds bounds{};
	bounds.center = glm::vec3(0.0f);
	bounds.radius = 0.7072f * maxScale;

	const uint32_t entityId = CreateRenderEntity(first, 1, bounds, _pos);
	SetEntityScale(entityId, _scale);
	return entityId;
}

// Makes a fresh noise map, saves the files, and puts the new texture on the quads.
static void Perlin_Regenerate()
{
	if (!Noise_Generate(SParams, SMap))
		return;

	Noise_SaveRAW(SMap, kNoiseRawPath);
	Noise_SaveJPG(SMap, kNoiseJpgPath);

	// The old texture gets replaced, so delete it and point the quad materials at the new one
	if (SNoiseTex != 0)
		glDeleteTextures(1, &SNoiseTex);
	SNoiseTex = CreateGrayscaleTexture(SMap.width, SMap.height, SMap.gray8.data());

	for (int i = 0; i < SQuadCount; ++i)
		REntityMaterials[SQuadMats[i]].tex0 = SNoiseTex;
}

static void Perlin_Init()
{
	SParams = NoiseParams{}; // Fresh defaults every load
	STime = 0.0f;
	SQuadCount = 0;

	// One program per quad, all reusing the standard instanced vertex shader.
	// If one fails to load, fall back to the default program so the quad still shows.
	SProgGreyscale = LoadShaderProgram("Assets/Shaders/Temp/model.vert", "Assets/Shaders/Scenes/NoiseGreyscale.frag");
	SProgColor = LoadShaderProgram("Assets/Shaders/Temp/model.vert", "Assets/Shaders/Scenes/NoiseColor.frag");
	SProgAnimated = LoadShaderProgram("Assets/Shaders/Temp/model.vert", "Assets/Shaders/Scenes/NoiseAnimated.frag");
	if (SProgGreyscale == 0) SProgGreyscale = SceneDefaultProgram;
	if (SProgColor == 0)     SProgColor = SceneDefaultProgram;
	if (SProgAnimated == 0)  SProgAnimated = SceneDefaultProgram;

	STimeLoc = glGetUniformLocation(SProgAnimated, "Time");

	Perlin_Regenerate();

	// Create the quads for this scene.
	const glm::vec3 scale(10.0f, 10.0f, 1.0f);
	CreateNoiseQuad(SProgGreyscale, SNoiseTex, glm::vec3(-12.0f, 6.0f, 0.0f), scale, SQuadMats[0]);
	CreateNoiseQuad(SProgColor, SNoiseTex, glm::vec3(0.0f, 6.0f, 0.0f), scale, SQuadMats[1]);
	CreateNoiseQuad(SProgAnimated, SNoiseTex, glm::vec3(12.0f, 6.0f, 0.0f), scale, SQuadMats[2]);
	SQuadCount = 3;
}

static void Perlin_Update(float _deltaTime)
{
	STime += _deltaTime;

	// Feed the time into the animated quad's shader
	if (SProgAnimated != 0 && STimeLoc != -1)
		glProgramUniform1f(SProgAnimated, STimeLoc, STime);
}

static void Perlin_DrawUI()
{
	ImGui::DragInt("Width", &SParams.width, 8.0f, 64, 2048);
	ImGui::DragInt("Height", &SParams.height, 8.0f, 64, 2048);
	if (SParams.width > 1024 || SParams.height > 1024)
		ImGui::TextDisabled("(large maps take a few seconds)");

	ImGui::SliderInt("Octaves", &SParams.octaves, 1, 10);
	ImGui::DragFloat("Wavelength", &SParams.wavelength, 1.0f, 8.0f, 512.0f);
	ImGui::SliderFloat("Gain", &SParams.gain, 0.1f, 0.9f);
	ImGui::SliderFloat("Lacunarity", &SParams.lacunarity, 1.5f, 3.0f);

	int seed = (int)SParams.seed;
	if (ImGui::InputInt("Seed (0 = time)", &seed))
		SParams.seed = (uint32_t)(seed < 0 ? 0 : seed); // no negatives allowed

	if (SMap.width > 0)
		ImGui::TextDisabled("Last: seed %u, %.2fs", SMap.seedUsed, SMap.generationSeconds);

	if (ImGui::Button("Regenerate Noise", ImVec2(-1.0f, 0.0f)))
		Perlin_Regenerate();
}

static void Perlin_Shutdown()
{
	// The noise texture and the three programs are the scene's,
	// everything else cleans up automatically.
	if (SNoiseTex != 0)
	{
		glDeleteTextures(1, &SNoiseTex);
		SNoiseTex = 0;
	}

	if (SProgGreyscale != 0 && SProgGreyscale != SceneDefaultProgram) glDeleteProgram(SProgGreyscale);
	if (SProgColor != 0 && SProgColor != SceneDefaultProgram)         glDeleteProgram(SProgColor);
	if (SProgAnimated != 0 && SProgAnimated != SceneDefaultProgram)   glDeleteProgram(SProgAnimated);
	SProgGreyscale = SProgColor = SProgAnimated = 0;

	SQuadCount = 0;
	SMap = NoiseMap{};
}

// Hands the scene system everything it needs to know about this scene.
export SceneFuncs GetScene_PerlinNoise()
{
	SceneFuncs scene{};
	scene.name = "Perlin Noise";
	scene.description = "Generates perlin noise, saves noise.raw + noise.jpg,\nand shows it on three quads: greyscale, gradient, animated.";
	scene.Init = &Perlin_Init;
	scene.Update = &Perlin_Update;
	scene.DrawUI = &Perlin_DrawUI;
	scene.Shutdown = &Perlin_Shutdown;
	return scene;
}
