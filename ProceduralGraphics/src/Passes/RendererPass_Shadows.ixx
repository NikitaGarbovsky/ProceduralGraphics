module;

#include <glew.h>
#include <glm.hpp>
#include <vector>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

/// <summary>
/// Draws the scene from each shadow casting light and records how far away the closest
/// thing was into a depth only framebuffer. That depth map is what a lit shader compares
/// against later to work out whether a pixel is blocked from the light.
///
/// ONLY the two directional lights are handled, each with its own map.
///
/// The finished maps are left sitting on texture units 4 and 5.
/// </summary>
export module RendererPass_Shadows;

import RendererData;      // For ViewportFBO
import RendererFrame;     // For FrameCommon, InstanceData, BindInstanceAttribs
import RendererEntitys;
import RendererLights;
import RendererUtilities; // For LoadShaderProgram
import DebugUtilities;
import <cstdint>;

// The maps are square. Bigger looks sharper but costs more to fill each frame.
export constexpr int kShadowMapSize = 4096;
export constexpr uint32_t kMaxShadowLights = 2;

// Texture units the finished maps get left on. A shader that wants to read them points
// its samplers at these.
export constexpr GLint kShadowMapUnit0 = 4;
export constexpr GLint kShadowMapUnit1 = 5;

// Everything a scene panel might want to change.
export struct ShadowTuning {
	float orthoHalfSize = 120.0f; // Half the width of the box the light can see
	float nearPlane = 1.0f;
	float farPlane = 600.0f;
	float eyeDistance = 200.0f;   // How far back along its direction the light sits
	float bias = 0.0015f;         // Offset that stops surfaces shadowing themselves
	int pcfRadius = 1;            // 1 is 3x3, 3 is 7x7
};

export ShadowTuning GShadowTuning{};

// ==========================================================================================
// Module state
// ==========================================================================================

static bool SInitialized = false;

static GLuint SFBO[kMaxShadowLights] = {};
static GLuint STex[kMaxShadowLights] = {};

static GLuint SDepthProgram = 0;
static GLint SDepthLightVPLoc = -1;

static GLuint SInstanceVBO = 0;
static std::vector<InstanceData> SInstances;

// Which lights cast, and the matrix that puts the world into each of their views.
static LightID SLights[kMaxShadowLights] = {};
static glm::mat4 SLightVP[kMaxShadowLights] = { glm::mat4(1.0f), glm::mat4(1.0f) };
static uint32_t SLightCount = 0;

// The shader that reads the maps. 
static GLuint SReceiverProgram = 0;
static GLint SRecvLightVPLoc[kMaxShadowLights] = { -1, -1 };
static GLint SRecvLightIndexLoc[kMaxShadowLights] = { -1, -1 };
static GLint SRecvCountLoc = -1;

// ==========================================================================================
// Internal
// ==========================================================================================

// Makes the depth texture and the framebuffer that writes into it.
// No color attachment, depth is the only thing this pass cares about.
static bool CreateShadowMap(uint32_t _slot)
{
	glGenTextures(1, &STex[_slot]);
	glBindTexture(GL_TEXTURE_2D, STex[_slot]);

	// Data starts empty, the depth pass fills it every frame.
	// A renderbuffer can't be used here because the lit shader has to sample it later.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowMapSize, kShadowMapSize,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Anything outside the box the light can see reads as fully lit. (no wrapping)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	glGenFramebuffers(1, &SFBO[_slot]);
	glBindFramebuffer(GL_FRAMEBUFFER, SFBO[_slot]);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, STex[_slot], 0);

	// No color gets written or read.
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	if (!complete)
		LogWarning("CreateShadowMap: shadow framebuffer failed to initialize correctly.");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return complete;
}

// Works out the matrix that puts the world into one lights point of view.
// A directional light has no position, so we pretend it sits a long way back along its
// direction and looks at the middle of the scene.
static glm::mat4 BuildLightVP(LightID _light)
{
	glm::vec3 dir = glm::normalize(GetLightDirection(_light));

	// Fallback,
	// lookAt falls apart when the light points straight up or down, so swap the up vector.
	glm::vec3 up = (glm::abs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f)
		: glm::vec3(0.0f, 1.0f, 0.0f);

	glm::vec3 eye = -dir * GShadowTuning.eyeDistance;
	glm::mat4 lightView = glm::lookAt(eye, glm::vec3(0.0f), up);

	// Orthographic because the rays of a directional light are parallel.
	const float h = GShadowTuning.orthoHalfSize;
	glm::mat4 lightProj = glm::ortho(-h, h, -h, h, GShadowTuning.nearPlane, GShadowTuning.farPlane);

	return lightProj * lightView;
}

// Everything in the scene casts a shadow, so this walks the whole entity table.
// The camera cull list is purposefully not used, so an object sitting behind the camera can
// still throw a shadow into view.
static void CollectCasters()
{
	SInstances.clear();

	for (uint32_t ent = 0; ent < (uint32_t)CurrentRenderedEntitys.size(); ++ent)
	{
		const REntity& e = CurrentRenderedEntitys[ent];
		for (uint32_t i = 0; i < e.submeshCount; ++i)
		{
			InstanceData inst{};
			inst.model = EntityTransforms.worldMatrix[ent];
			inst.entityId = ent + 1;
			SInstances.push_back(inst);
		}
	}
}

// ==========================================================================================
// Public API
// ==========================================================================================

// Frees everything the pass made. Called by the scene.
export void Shadows_Shutdown()
{
	if (!SInitialized)
		return;

	for (uint32_t i = 0; i < kMaxShadowLights; ++i)
	{
		if (SFBO[i]) { glDeleteFramebuffers(1, &SFBO[i]); SFBO[i] = 0; }
		if (STex[i]) { glDeleteTextures(1, &STex[i]); STex[i] = 0; }
	}

	if (SInstanceVBO) { glDeleteBuffers(1, &SInstanceVBO); SInstanceVBO = 0; }
	if (SDepthProgram) { glDeleteProgram(SDepthProgram); SDepthProgram = 0; }

	SLightCount = 0;
	SReceiverProgram = 0;
	SInstances.clear();
	SInitialized = false;
}

// Makes the depth shader, the instance buffer and both maps.
export void Shadows_Init()
{
	if (SInitialized)
		return;

	SDepthProgram = LoadShaderProgram("Assets/Shaders/Common/ShadowDepth.vert",
		"Assets/Shaders/Common/ShadowDepth.frag");

	if (SDepthProgram == 0)
	{
		LogWarning("Shadows_Init: depth shader failed, nothing will cast a shadow.");
		return;
	}

	SDepthLightVPLoc = glGetUniformLocation(SDepthProgram, "LightVP");

	glGenBuffers(1, &SInstanceVBO);

	bool ok = true;
	for (uint32_t i = 0; i < kMaxShadowLights; ++i)
		ok = CreateShadowMap(i) && ok;

	SLightCount = 0;
	SReceiverProgram = 0;
	SInitialized = true;

	if (!ok)
		Shadows_Shutdown();
}

// Tells the pass which lights cast shadows. Pass the same id twice for a single light.
export void Shadows_SetLights(LightID _lightA, LightID _lightB)
{
	if (!SInitialized)
		return;

	if (GetLightType(_lightA) != LightType::Directional ||
		GetLightType(_lightB) != LightType::Directional)
	{
		LogWarning("Shadows_SetLights: only directional lights are supported.");
	}

	SLights[0] = _lightA;
	SLights[1] = _lightB;
	SLightCount = kMaxShadowLights;
}

// Points the pass at the shader that reads the maps, so it can push the light matrices in
// at the right moment.
export void Shadows_SetReceiverProgram(GLuint _program)
{
	SReceiverProgram = _program;
	if (_program == 0)
		return;

	// Caches all the uniform location for use.
	SRecvLightVPLoc[0] = glGetUniformLocation(_program, "LightVP0");
	SRecvLightVPLoc[1] = glGetUniformLocation(_program, "LightVP1");
	SRecvLightIndexLoc[0] = glGetUniformLocation(_program, "ShadowLightIndex0");
	SRecvLightIndexLoc[1] = glGetUniformLocation(_program, "ShadowLightIndex1");
	SRecvCountLoc = glGetUniformLocation(_program, "ShadowCount");
}

// Fills both depth maps.
export void ShadowPass_Execute(const FrameCommon& _fcommon)
{
	if (!SInitialized || SLightCount == 0 || SDepthProgram == 0)
		return;

	CollectCasters();
	if (SInstances.empty())
		return;

	// One upload covers every draw below.
	glBindBuffer(GL_ARRAY_BUFFER, SInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER,
		(GLsizeiptr)(SInstances.size() * sizeof(InstanceData)),
		SInstances.data(),
		GL_STREAM_DRAW);

	glUseProgram(SDepthProgram);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	for (uint32_t slot = 0; slot < SLightCount; ++slot)
	{
		SLightVP[slot] = BuildLightVP(SLights[slot]);

		glBindFramebuffer(GL_FRAMEBUFFER, SFBO[slot]);
		glViewport(0, 0, kShadowMapSize, kShadowMapSize);
		glClear(GL_DEPTH_BUFFER_BIT);

		glUniformMatrix4fv(SDepthLightVPLoc, 1, GL_FALSE, glm::value_ptr(SLightVP[slot]));

		// Same walk order CollectCasters used, so the instance index lines up.
		uint32_t instance = 0;
		for (uint32_t ent = 0; ent < (uint32_t)CurrentRenderedEntitys.size(); ++ent)
		{
			const REntity& e = CurrentRenderedEntitys[ent];
			for (uint32_t i = 0; i < e.submeshCount; ++i)
			{
				const Submesh& sm = REntitySubmeshes[e.firstSubmesh + i];
				const Mesh& mesh = REntityMeshs[sm.mesh];

				BindInstanceAttribs(mesh.vao, SInstanceVBO,
					(uintptr_t)instance * sizeof(InstanceData));

				glBindVertexArray(mesh.vao);
				glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indexCount,
					GL_UNSIGNED_INT, nullptr, 1);

				++instance;
			}
		}
	}

	// Hand the matrices to the shader that reads the maps.
	if (SReceiverProgram != 0)
	{
		for (uint32_t slot = 0; slot < SLightCount; ++slot)
		{
			if (SRecvLightVPLoc[slot] >= 0)
				glProgramUniformMatrix4fv(SReceiverProgram, SRecvLightVPLoc[slot], 1, GL_FALSE,
					glm::value_ptr(SLightVP[slot]));

			if (SRecvLightIndexLoc[slot] >= 0)
				glProgramUniform1i(SReceiverProgram, SRecvLightIndexLoc[slot], (int)SLights[slot]);
		}

		if (SRecvCountLoc >= 0)
			glProgramUniform1i(SReceiverProgram, SRecvCountLoc, (int)SLightCount);
	}

	// Leave the maps parked on their units so the lit shader can just read them.
	glActiveTexture(GL_TEXTURE0 + kShadowMapUnit0);
	glBindTexture(GL_TEXTURE_2D, STex[0]);
	glActiveTexture(GL_TEXTURE0 + kShadowMapUnit1);
	glBindTexture(GL_TEXTURE_2D, STex[1]);
	glActiveTexture(GL_TEXTURE0);

	// Put the frame target back the way the rest of the pipeline expects to find it.
	// #TODO: this is a shit, manual, unscalable way to reset pipeline state. opengl statemachine garbage
	glBindVertexArray(0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
	glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);
}