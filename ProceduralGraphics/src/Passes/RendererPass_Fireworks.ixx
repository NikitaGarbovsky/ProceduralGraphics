module;

#include <glew.h>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <cstdint>
#include <cstdlib>

/// <summary>
/// This module manages the particle fireworks effect. It uses a compute shader to simulate 
/// a shooting firework shooting into the sky with fed in uniforms from the editor.
/// 
/// Each firework gets its own slice of the buffers and goes through 4 steps:
/// start particles on the ground, climb with a trail, explode, then die out and stop.
/// </summary>
export module RendererPass_Fireworks;

import RendererData;    
import RendererFrame;    
import RendererUtilities; 
import DebugUtilities;

// ==========================================================================================
// Tuning
// ==========================================================================================

constexpr int kWorkGroupSizeX = 128;
constexpr int kGroupCountX = 16;
constexpr int kParticlesPerFirework = kWorkGroupSizeX * kGroupCountX;
constexpr int kMaxFireworks = 10;
constexpr int kVolleySize = 5;

// All the mutable values for the effect. Edited in editor gui.
export struct FireworkTuning {
	float launchSpeedMin = 11.0f;
	float launchSpeedMax = 15.0f;
	float headGravity = 3.0f;
	float trailTimeMin = 1.1f;
	float trailTimeMax = 2.1f;
	float trailLife = 0.45f;
	float trailGravity = 2.0f;
	float burstSpeed = 9.0f;
	float burstLife = 2.2f;
	float burstGravity = 5.5f;
};

// The values the sliders change.
export FireworkTuning GFireworkTuning{};

// One firework. The phase number gets sent straight to the shader as Mode.
// 0 ground, 1 climb, 2 explode, 3 dying.
struct Firework
{
	bool active = false;
	int phase = 0;
	glm::vec3 headPos{ 0.0f };  // Where the rocket is right now
	glm::vec3 headVel{ 0.0f };
	glm::vec3 color{ 1.0f };    // Random color for this one
	float trailTime = 0.0f;     // How long this one climbs for
	float burstLife = 0.0f;     // The spark life it exploded with
	float age = 0.0f;           // Time since launch
	float decayAge = 0.0f;      // Time since it exploded
};

// ==========================================================================================
// Module state
// ==========================================================================================

static bool SInitialized = false;

static GLuint SComputeProgram = 0;
static GLuint SRenderProgram = 0;

static GLuint SVAO = 0;
static GLuint SVBO_PositionLife = 0;
static GLuint SVBO_Velocity = 0;

static Firework SFireworks[kMaxFireworks];

// Compute shader uniforms.
static GLint SCu_Mode = -1;
static GLint SCu_BaseIndex = -1;
static GLint SCu_SeedLife = -1;
static GLint SCu_SeedX = -1;
static GLint SCu_SeedY = -1;
static GLint SCu_SeedZ = -1;
static GLint SCu_VelocityLifeChange = -1;
static GLint SCu_EmitterOrigin = -1;
static GLint SCu_TrailLife = -1;
static GLint SCu_BurstSpeed = -1;
static GLint SCu_BurstLife = -1;

// Render shader uniforms.
static GLint SRu_VP = -1;
static GLint SRu_Color = -1;
static GLint SRu_MaxLife = -1;

// ==========================================================================================
// Internal helpers
// ==========================================================================================

// Random helpers. rand is good enough here.
static float RandFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandRange(float _min, float _max) { return _min + RandFloat01() * (_max - _min); }

// Random seed for the shader. Never zero.
static int RandSeed() { return ((rand() << 15) | rand()) | 1; }

// Random bright color. 
static glm::vec3 RandomFireworkColor() {
	glm::vec3 c(RandRange(0.2f, 1.0f), RandRange(0.2f, 1.0f), RandRange(0.2f, 1.0f));
	float strongest = glm::max(c.r, glm::max(c.g, c.b));
	return c / strongest;
}

// ==========================================================================================
// Public API
// ==========================================================================================

// Make the shaders, the two buffers and the vao.
export void Fireworks_Init() {
	if (SInitialized)
		return;

	SComputeProgram = LoadComputeProgram("Assets/Shaders/Scenes/FireworkParticles.comp");
	SRenderProgram = LoadShaderProgram("Assets/Shaders/Scenes/FireworkParticles.vert",
		"Assets/Shaders/Scenes/FireworkParticles.frag");

	if (SComputeProgram == 0 || SRenderProgram == 0)
		LogWarning("Fireworks_Init: a shader failed to load, fireworks will not show.");

	// Grab the uniform spots once.
	SCu_Mode = glGetUniformLocation(SComputeProgram, "Mode");
	SCu_BaseIndex = glGetUniformLocation(SComputeProgram, "BaseIndex");
	SCu_SeedLife = glGetUniformLocation(SComputeProgram, "SeedLife");
	SCu_SeedX = glGetUniformLocation(SComputeProgram, "SeedX");
	SCu_SeedY = glGetUniformLocation(SComputeProgram, "SeedY");
	SCu_SeedZ = glGetUniformLocation(SComputeProgram, "SeedZ");
	SCu_VelocityLifeChange = glGetUniformLocation(SComputeProgram, "VelocityLifeChange");
	SCu_EmitterOrigin = glGetUniformLocation(SComputeProgram, "EmitterOrigin");
	SCu_TrailLife = glGetUniformLocation(SComputeProgram, "TrailLife");
	SCu_BurstSpeed = glGetUniformLocation(SComputeProgram, "BurstSpeed");
	SCu_BurstLife = glGetUniformLocation(SComputeProgram, "BurstLife");

	SRu_VP = glGetUniformLocation(SRenderProgram, "VP");
	SRu_Color = glGetUniformLocation(SRenderProgram, "Color");
	SRu_MaxLife = glGetUniformLocation(SRenderProgram, "MaxLife");

	// One buffer big enough for every firework. Each one uses its own chunk.
	const GLsizeiptr bufferBytes = sizeof(glm::vec4) * kParticlesPerFirework * kMaxFireworks;

	// Store position and life
	glGenBuffers(1, &SVBO_PositionLife);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SVBO_PositionLife);
	glBufferData(GL_SHADER_STORAGE_BUFFER, bufferBytes, NULL, GL_DYNAMIC_DRAW);

	// Store velocity
	glGenBuffers(1, &SVBO_Velocity);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SVBO_Velocity);
	glBufferData(GL_SHADER_STORAGE_BUFFER, bufferBytes, NULL, GL_DYNAMIC_DRAW);

	// Vao for the normal draw. It reads the same buffer the compute shader writes.
	glGenVertexArrays(1, &SVAO);
	glBindVertexArray(SVAO);

	glBindBuffer(GL_ARRAY_BUFFER, SVBO_PositionLife);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);

	// Unbind
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// Start with nothing in the air.
	for (int i = 0; i < kMaxFireworks; ++i)
		SFireworks[i] = Firework{};

	SInitialized = true;
}

// Clear resources.
export void Fireworks_Shutdown() {
	if (!SInitialized)
		return;

	if (SVAO) { glDeleteVertexArrays(1, &SVAO); SVAO = 0; }
	if (SVBO_PositionLife) { glDeleteBuffers(1, &SVBO_PositionLife); SVBO_PositionLife = 0; }
	if (SVBO_Velocity) { glDeleteBuffers(1, &SVBO_Velocity); SVBO_Velocity = 0; }
	if (SComputeProgram) { glDeleteProgram(SComputeProgram); SComputeProgram = 0; }
	if (SRenderProgram) { glDeleteProgram(SRenderProgram); SRenderProgram = 0; }

	for (int i = 0; i < kMaxFireworks; ++i)
		SFireworks[i] = Firework{};

	SInitialized = false;
}

// Send up a volley. Each one gets a random spot, color, speed and climb time for 
// some variation.
export void Fireworks_Launch() {
	if (!SInitialized)
		return;

	int spawned = 0;
	for (int i = 0; i < kMaxFireworks && spawned < kVolleySize; ++i)
	{
		Firework& fw = SFireworks[i];
		if (fw.active)
			continue;

		fw.active = true;
		fw.phase = 0; // Next dispatch puts the sparks on the ground
		fw.age = 0.0f;
		fw.decayAge = 0.0f;
		fw.headPos = glm::vec3(RandRange(-8.0f, 8.0f), 0.5f, RandRange(-8.0f, 8.0f));
		fw.headVel = glm::vec3(RandRange(-1.5f, 1.5f),
			RandRange(GFireworkTuning.launchSpeedMin, GFireworkTuning.launchSpeedMax),
			RandRange(-1.5f, 1.5f));
		fw.trailTime = RandRange(GFireworkTuning.trailTimeMin, GFireworkTuning.trailTimeMax);
		fw.burstLife = GFireworkTuning.burstLife;
		fw.color = RandomFireworkColor();
		++spawned;
	}
}

// Move the firework rockets and step each firework to its next phase.
export void Fireworks_Update(float _deltaTime) {
	if (!SInitialized)
		return;

	for (int i = 0; i < kMaxFireworks; ++i)
	{
		Firework& fw = SFireworks[i];
		if (!fw.active)
			continue;

		fw.age += _deltaTime;

		if (fw.phase <= 1) {
			// The rocket is what the trail sparks follow. Gravity slows the climb.
			fw.headVel.y -= GFireworkTuning.headGravity * _deltaTime;
			fw.headPos += fw.headVel * _deltaTime;

			// Time is up, explode where the rocket is now.
			if (fw.phase == 1 && fw.age >= fw.trailTime)
				fw.phase = 2;
		}
		else if (fw.phase == 3) {
			fw.decayAge += _deltaTime;

			// All the sparks are dead, free the slot. 
			if (fw.decayAge >= fw.burstLife)
				fw.active = false;
		}
	}
}

// How many are up right now, for the panel.
export int Fireworks_ActiveCount() {
	int count = 0;
	for (int i = 0; i < kMaxFireworks; ++i)
		if (SFireworks[i].active)
			++count;
	return count;
}

// Run the compute shader on every live firework, then draw them all as points.
export void FireworksPass_Execute(const FrameCommon& _fcommon) {
	if (!SInitialized || SComputeProgram == 0 || SRenderProgram == 0)
		return;

	// Nothing up, nothing to do.
	bool anyActive = false;
	for (int i = 0; i < kMaxFireworks; ++i)
		if (SFireworks[i].active) { anyActive = true; break; }
	if (!anyActive)
		return;

	const float dt = gDeltaTime;

	// ============================ Compute pass ============================

	glUseProgram(SComputeProgram);

	// Hook the buffers up. 
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SVBO_PositionLife);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SVBO_Velocity);

	// Sliders can change these any time, so send them every frame.
	glUniform1f(SCu_TrailLife, GFireworkTuning.trailLife);
	glUniform1f(SCu_BurstSpeed, GFireworkTuning.burstSpeed);
	glUniform1f(SCu_BurstLife, GFireworkTuning.burstLife);

	for (int i = 0; i < kMaxFireworks; ++i) {
		Firework& fw = SFireworks[i];
		if (!fw.active)
			continue;

		// New seeds every time.
		glUniform1i(SCu_SeedLife, RandSeed());
		glUniform1i(SCu_SeedX, RandSeed());
		glUniform1i(SCu_SeedY, RandSeed());
		glUniform1i(SCu_SeedZ, RandSeed());

		// Trail sparks droop a bit, explosion sparks fall properly.
		const float gravity = (fw.phase <= 1) ? GFireworkTuning.trailGravity : GFireworkTuning.burstGravity;
		glUniform4f(SCu_VelocityLifeChange, 0.0f, -gravity * dt, 0.0f, dt);

		glUniform3f(SCu_EmitterOrigin, fw.headPos.x, fw.headPos.y, fw.headPos.z);
		glUniform1i(SCu_Mode, fw.phase);
		glUniform1ui(SCu_BaseIndex, (GLuint)(i * kParticlesPerFirework));

		// Run the compute shader over this fireworks chunk.
		glDispatchCompute(kGroupCountX, 1, 1);

		// The pad and explode steps only run for one frame, so move them on.
		if (fw.phase == 0) fw.phase = 1;
		else if (fw.phase == 2) {
			// Save the life it exploded with.
			fw.burstLife = GFireworkTuning.burstLife;
			fw.phase = 3;
		}
	}

	// Wait for the gpu to finish, the draw below reads what it writes.
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	// ==================== Normal pass, draw the points ====================

	// Draw into the same target as the rest of the scene.
	glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
	glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);

	glUseProgram(SRenderProgram);
	glUniformMatrix4fv(SRu_VP, 1, GL_FALSE, glm::value_ptr(_fcommon.viewProj));

	// Blend so they fade out. Depth test on so they hide behind things, but no depth
	// writing or they block each other.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	// This is neat, Lets the vertex shader set the point size #TODO: pass in a value in editor 
	// instead of hardcoding it in the shader.
	glEnable(GL_PROGRAM_POINT_SIZE);

	glBindVertexArray(SVAO);

	for (int i = 0; i < kMaxFireworks; ++i) {
		Firework& fw = SFireworks[i];
		if (!fw.active)
			continue;

		glUniform3f(SRu_Color, fw.color.r, fw.color.g, fw.color.b);

		// The fade divides by the full life, so send the one this phase uses.
		glUniform1f(SRu_MaxLife, (fw.phase <= 1) ? GFireworkTuning.trailLife : fw.burstLife);

		glDrawArrays(GL_POINTS, i * kParticlesPerFirework, kParticlesPerFirework);
	}

	// Unbind. Leave the viewport fbo bound, the next passes want it.
	// #TODO: this is going to get confusing to keep track of, figure out way to view dependencies of 
	// each render pass.
	glBindVertexArray(0);
	glDisable(GL_PROGRAM_POINT_SIZE);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
	glUseProgram(0);
}
