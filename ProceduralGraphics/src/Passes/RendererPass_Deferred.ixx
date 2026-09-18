module;

#include <glew.h>
#include <glm.hpp>
#include <vector>
#include <cmath>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

/// <summary>
/// The deferred rendering path.
///
/// Instead of lighting every object as it gets drawn, the geometry pass writes down what
/// each pixel is made of into a set of offscreen textures called the geometry buffer. 
/// Then a second pass then draws one quad over the whole screen and lights those textures, so every
/// pixel gets lit exactly once no matter how many objects were stacked behind each other. 
/// Much more efficient for lighting calculations, just by removing most of the calculations.
///
/// Finally, point light sources draw a transparent ball through the normal forward way overtop of them.
///
/// This is turned on and off with Init & Shutdown within this module. (for this renderer application, this
/// pass is only used in the deferred scene.)
/// </summary>
export module RendererPass_Deferred;

import RendererData;           
import RendererFrame;
import RendererEntitys;
import RendererLights;
import RendererUtilities;      
import RendererFullscreenQuad;
import DebugUtilities;
import <cstdint>;
import <string>;

// Position, normal, and albedo plus shininess packed together.
export constexpr uint32_t kGBufferTargetCount = 3;

// Everything a scene panel might want to change.
export struct DeferredTuning {
	float shininess = 48.0f;        // Specular exponent for everything in the scene
	float ambientStrength = 0.06f;
	float proxyRadius = 0.45f;      // Size of the ball drawn at each light
	float proxyOpacity = 0.55f;
	bool drawLightProxies = true;
};

export DeferredTuning GDeferredTuning{};

// ==========================================================================================
// Module state
// ==========================================================================================

static bool SInitialized = false;

static GLuint SGBufferFBO = 0;
static GLuint SGBufferDepth = 0;
static int SGBufferW = 0;
static int SGBufferH = 0;

static GLuint SGBufferTex[kGBufferTargetCount] = {};
// The three g-buffer targets by name
static const char* SGBufferNames[kGBufferTargetCount] = { "Position", "Normal", "AlbedoShininess" };

static GLuint SGeometryProgram = 0;
static GLint SGeoViewLoc = -1, SGeoProjLoc = -1, SGeoTex0Loc = -1, SGeoShininessLoc = -1;

static GLuint SLightingProgram = 0;
static GLint SLitCameraPosLoc = -1, SLitAmbientLoc = -1;

static GLuint SProxyProgram = 0;
static GLint SProxyVPLoc = -1, SProxyModelLoc = -1, SProxyColorLoc = -1, SProxyOpacityLoc = -1;

static GLuint SSphereVAO = 0, SSphereVBO = 0, SSphereEBO = 0;
static uint32_t SSphereIndexCount = 0;

// ==========================================================================================
// Internal
// ==========================================================================================

static void DestroyGBuffer() {
	for (uint32_t i = 0; i < kGBufferTargetCount; ++i)
		if (SGBufferTex[i]) { glDeleteTextures(1, &SGBufferTex[i]); SGBufferTex[i] = 0; }

	if (SGBufferDepth) { glDeleteTextures(1, &SGBufferDepth); SGBufferDepth = 0; }
	if (SGBufferFBO) { glDeleteFramebuffers(1, &SGBufferFBO); SGBufferFBO = 0; }

	SGBufferW = 0;
	SGBufferH = 0;
}

// Builds the g-buffer at the given size, or leaves it alone if it is already right.
static void EnsureGBuffer(int _w, int _h) {
	if (_w <= 0 || _h <= 0) return;
	if (SGBufferFBO != 0 && SGBufferW == _w && SGBufferH == _h) return;

	DestroyGBuffer();

	SGBufferW = _w;
	SGBufferH = _h;

	glGenFramebuffers(1, &SGBufferFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, SGBufferFBO);

	for (uint32_t i = 0; i < kGBufferTargetCount; ++i) { 
		const GLint internalFormat = (i < 2) ? GL_RGBA16F : GL_RGBA8;

		glGenTextures(1, &SGBufferTex[i]);
		glBindTexture(GL_TEXTURE_2D, SGBufferTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, _w, _h, 0, GL_RGBA, GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
			SGBufferTex[i], 0);
	}

	// Depth.
	glGenTextures(1, &SGBufferDepth);
	glBindTexture(GL_TEXTURE_2D, SGBufferDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, _w, _h, 0,
		GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
		SGBufferDepth, 0);

	// Tell opengl the fragment shader writes to all three.
	const GLenum attachments[kGBufferTargetCount] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(kGBufferTargetCount, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		LogWarning("EnsureGBuffer: geometry buffer failed to initialize correctly.");
		DestroyGBuffer();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Builds a plain uv sphere, positions only. Used for the balls that show where each
// point light is sitting.
static void BuildProxySphere() {
	constexpr uint32_t kStacks = 14;
	constexpr uint32_t kSlices = 22;
	constexpr float kPi = 3.14159265358979323846f;

	std::vector<float> verts;
	std::vector<uint32_t> indices;

	// Walk top to bottom, and around at each level. Radius of 1, the draw scales it.
	for (uint32_t stack = 0; stack <= kStacks; ++stack) {
		const float phi = kPi * (float)stack / (float)kStacks;

		for (uint32_t slice = 0; slice <= kSlices; ++slice) {
			const float theta = 2.0f * kPi * (float)slice / (float)kSlices;

			verts.push_back(sinf(phi) * cosf(theta));
			verts.push_back(cosf(phi));
			verts.push_back(sinf(phi) * sinf(theta));
		}
	}

	// Two triangles for every square in that grid.
	for (uint32_t stack = 0; stack < kStacks; ++stack) {
		for (uint32_t slice = 0; slice < kSlices; ++slice) {
			const uint32_t a = stack * (kSlices + 1) + slice;
			const uint32_t b = a + kSlices + 1;

			indices.insert(indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
		}
	}

	SSphereIndexCount = (uint32_t)indices.size();

	glGenVertexArrays(1, &SSphereVAO);
	glGenBuffers(1, &SSphereVBO);
	glGenBuffers(1, &SSphereEBO);

	glBindVertexArray(SSphereVAO);

	glBindBuffer(GL_ARRAY_BUFFER, SSphereVBO);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)),
		verts.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, SSphereEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(uint32_t)),
		indices.data(), GL_STATIC_DRAW);

	// Position only, this thing never gets lit or textured.
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

	glBindVertexArray(0);
}

// Step 1. Draw every visible object into the g-buffer with no lighting at all.
static void GeometryPass(const FrameCommon& _f, PassContext& _p) {
	glBindFramebuffer(GL_FRAMEBUFFER, SGBufferFBO);
	glViewport(0, 0, SGBufferW, SGBufferH);

	// The renderer leaves the clear color at solid black, which would fill the position
	// textures alpha with 1 and make empty background look like real geometry to the
	// lighting pass. Clear to zero here, then put it back.
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glUseProgram(SGeometryProgram);
	glUniformMatrix4fv(SGeoViewLoc, 1, GL_FALSE, glm::value_ptr(_f.view));
	glUniformMatrix4fv(SGeoProjLoc, 1, GL_FALSE, glm::value_ptr(_f.proj));
	glUniform1f(SGeoShininessLoc, GDeferredTuning.shininess);
	glUniform1i(SGeoTex0Loc, 0);

	GLuint lastVAO = 0;
	GLuint lastTex0 = 0;

	for (const auto& cmd : _p.commands) {
		const Material& mat = REntityMaterials[cmd.material];
		const Mesh& mesh = REntityMeshs[cmd.mesh];

		// The materials own program is ignored on purpose. Everything goes through the
		// one geometry shader, so only the texture matters here.
		if (mat.tex0 != lastTex0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mat.tex0);
			lastTex0 = mat.tex0;
		}

		if (mesh.vao != lastVAO) {
			BindInstanceAttribs(mesh.vao, _p.instanceVBO, 0);
			lastVAO = mesh.vao;
		}

		const uintptr_t base = uintptr_t(cmd.instanceOffset) * sizeof(InstanceData);
		BindInstanceAttribs(mesh.vao, _p.instanceVBO, base);

		glBindVertexArray(mesh.vao);
		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)cmd.indexCount, GL_UNSIGNED_INT,
			nullptr, (GLsizei)cmd.instanceCount);
	}

	glBindVertexArray(0);
}

// Step 2. One quad over the whole screen, lighting every pixel from the g-buffer.
static void LightingPass(const FrameCommon& _f)
{
	glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
	glViewport(0, 0, _f.viewportW, _f.viewportH);

	// The quad covers everything, so no deapth.
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	glUseProgram(SLightingProgram);
	glUniform3fv(SLitCameraPosLoc, 1, glm::value_ptr(_f.cameraPos));
	glUniform1f(SLitAmbientLoc, GDeferredTuning.ambientStrength);

	for (uint32_t i = 0; i < kGBufferTargetCount; ++i)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, SGBufferTex[i]);
	}
	glActiveTexture(GL_TEXTURE0);

	DrawFullscreenQuad();

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

// Step 3. Copy the depth the geometry pass worked out over to the frame target.
static void WriteDepthToViewport(const FrameCommon& _f) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, SGBufferFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ViewportFBO);

	// The source & destination
	glBlitFramebuffer(
		0, 0, SGBufferW, SGBufferH,       
		0, 0, _f.viewportW, _f.viewportH, 
		GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
	glViewport(0, 0, _f.viewportW, _f.viewportH);
}

// Step 4. Draw a see-through ball at every point light, the normal forward way. 
// #TODO: maybe move this so all lights use something like this (would be used a basis for the visual gizmo for lights)
static void LightProxyPass(const FrameCommon& _f) {
	if (!GDeferredTuning.drawLightProxies || SProxyProgram == 0 || SSphereVAO == 0)
		return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Depth test on so they hide behind objects, depth write off so overlapping balls
	// don't cut chunks out of each other.
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glUseProgram(SProxyProgram);
	glUniformMatrix4fv(SProxyVPLoc, 1, GL_FALSE, glm::value_ptr(_f.viewProj));
	glUniform1f(SProxyOpacityLoc, GDeferredTuning.proxyOpacity);

	glBindVertexArray(SSphereVAO);

	const uint32_t lightCount = GetLightCount();
	for (uint32_t i = 0; i < lightCount; ++i) {
		// Only point lights
		if (GetLightType(i) != LightType::Point)
			continue;

		glm::mat4 model(1.0f);
		model = glm::translate(model, LightTransforms.position[i]);
		model = glm::scale(model, glm::vec3(GDeferredTuning.proxyRadius));

		glUniformMatrix4fv(SProxyModelLoc, 1, GL_FALSE, glm::value_ptr(model));
		glUniform3fv(SProxyColorLoc, 1, glm::value_ptr(GetLightColor(i)));

		glDrawElements(GL_TRIANGLES, (GLsizei)SSphereIndexCount, GL_UNSIGNED_INT, nullptr);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

// ==========================================================================================
// Public API
// ==========================================================================================

export void Deferred_Shutdown() {
	if (!SInitialized)
		return;

	DestroyGBuffer();

	if (SSphereVAO) { glDeleteVertexArrays(1, &SSphereVAO); SSphereVAO = 0; }
	if (SSphereVBO) { glDeleteBuffers(1, &SSphereVBO); SSphereVBO = 0; }
	if (SSphereEBO) { glDeleteBuffers(1, &SSphereEBO); SSphereEBO = 0; }
	SSphereIndexCount = 0;

	if (SGeometryProgram) { glDeleteProgram(SGeometryProgram); SGeometryProgram = 0; }
	if (SLightingProgram) { glDeleteProgram(SLightingProgram); SLightingProgram = 0; }
	if (SProxyProgram) { glDeleteProgram(SProxyProgram); SProxyProgram = 0; }

	SInitialized = false;
}

export void Deferred_Init() {
	if (SInitialized)
		return;

	SGeometryProgram = LoadShaderProgram("Assets/Shaders/Common/GBuffer.vert",
		"Assets/Shaders/Common/GBuffer.frag");

	SLightingProgram = LoadShaderProgram(kFullscreenVertPath,
		"Assets/Shaders/Common/DeferredLighting.frag");

	SProxyProgram = LoadShaderProgram("Assets/Shaders/Common/LightProxy.vert",
		"Assets/Shaders/Common/LightProxy.frag");

	if (SGeometryProgram == 0 || SLightingProgram == 0)
	{
		LogWarning("Deferred_Init: a shader failed, falling back to the forward path.");
		Deferred_Shutdown();
		return;
	}

	// ====================== Cache uniforms for all the loaded shaders ======================
	
	
	SGeoViewLoc = glGetUniformLocation(SGeometryProgram, "ViewMat");
	SGeoProjLoc = glGetUniformLocation(SGeometryProgram, "ProjectionMat");
	SGeoTex0Loc = glGetUniformLocation(SGeometryProgram, "Tex0");
	SGeoShininessLoc = glGetUniformLocation(SGeometryProgram, "ObjectShininess");

	SLitCameraPosLoc = glGetUniformLocation(SLightingProgram, "CameraPos");
	SLitAmbientLoc = glGetUniformLocation(SLightingProgram, "AmbientStrength");

	// The three g-buffer textures always sit on units 0, 1 and 2, so this is a one off.
	for (uint32_t i = 0; i < kGBufferTargetCount; ++i) {
		const GLint loc = glGetUniformLocation(SLightingProgram,
			(i == 0) ? "Texture_Position" : (i == 1) ? "Texture_Normal" : "Texture_AlbedoShininess");

		if (loc < 0)
			LogWarning((std::string("Deferred_Init: lighting shader is missing the ")
				+ SGBufferNames[i] + " sampler.").c_str());

		glProgramUniform1i(SLightingProgram, loc, (GLint)i);
	}

	SProxyVPLoc = glGetUniformLocation(SProxyProgram, "VP");
	SProxyModelLoc = glGetUniformLocation(SProxyProgram, "ModelMatrix");
	SProxyColorLoc = glGetUniformLocation(SProxyProgram, "LightColor");
	SProxyOpacityLoc = glGetUniformLocation(SProxyProgram, "Opacity");

	BuildProxySphere();

	SInitialized = true;
}

// Is deferred rendering active?
export bool Deferred_IsActive() { return SInitialized; }

// Runs in place of OpaquePass_Execute. Takes the exact same build data, the camera cull
// and batching work is shared between both paths.
export void DeferredPass_Execute(const FrameCommon& _f, PassContext& _p) {
	if (!SInitialized)
		return;

	EnsureGBuffer(_f.viewportW, _f.viewportH);
	if (SGBufferFBO == 0)
		return;

	// The forward path normally uploads this inside ExecuteCommands. Since that is being
	// skipped, it has to happen here, and the picking pass later relies on it too.
	if (!_p.instances.empty() && _p.instanceVBO != 0) {
		glBindBuffer(GL_ARRAY_BUFFER, _p.instanceVBO);
		glBufferData(GL_ARRAY_BUFFER,
			(GLsizeiptr)(_p.instances.size() * sizeof(InstanceData)),
			_p.instances.data(),
			GL_STREAM_DRAW);
	}

	GeometryPass(_f, _p);
	LightingPass(_f);
	WriteDepthToViewport(_f);
	LightProxyPass(_f);

	glUseProgram(0);
}