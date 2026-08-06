module;

#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>

// This module contains a bunch of global accessible data that is used throughout the 
// renderer. 
// 
// #TODO viewport & FBO helper functions maybe need to go into their own module.
export module RendererData;

import DebugUtilities;

// Globals to utilize that are core of the Renderer.
export GLFWwindow* MainWindow;
export GLuint RenderObjProgram;
export GLuint PickingProgram; // SHader that runs the picking program for editor mouse click selection. 
export GLuint SelectedTintProgram;
export double gTimeSinceAppStart = 0.0;
export float gDeltaTime = 0.0; // TODO move this when expanding the renderer beyond a renderer.

// ====================== Viewport Render Target (Offscreen) ======================
// 
// The scene renders into the multisampled target so edges come out smooth, then that gets
// resolved down into a plain texture, because a multisampled one can't be sampled by a normal
// shader and post processing needs to sample it.
export GLuint ViewportFBO = 0;            // Multisampled. Every pass renders into this.
export GLuint ViewportColorMS = 0;        // Multisampled color attachment.
export GLuint ViewportDepthStencilMS = 0; // Multisampled depth + stencil attachment.

export GLuint ViewportResolveFBO = 0;     // Single sample, used for postprocessing.
export GLuint ViewportColorTex = 0;       // RGBA8, this is what post processing samples.

// How many MSAA samples the scene target uses.
export int ViewportSamples = 8;

// The sample count the current target was actually built with, so a change to ViewportSamples
// gets noticed. #TODO: use this for editor changes
static int GBuiltSamples = 8;

// Viewport dimensions (These are automatically evaluated/updated at the start of each frame, no need to set)
export int ViewportW = 0;
export int ViewportH = 0;

// ID of the REntity that is currently selected.
export uint32_t SelectedEntity = UINT32_MAX;

// ID of the Light that is currently selected.
export uint32_t SelectedLight = UINT32_MAX;

// Editable data for selection 
export float SelectedREntityOutlineWidth = 3.0f; // Outline thickness in pixels.
export glm::vec4 SelectedREntityColorOutline(1.0f, 1.0f, 1.0f, 1.0f);
export float SelectedREntityTintPulseSpeed = 4.0f;
export float SelectedREntityTintStrength = 0.75f;
export glm::vec3 SelectedREntityTintColor(1.0f, 1.0f, 1.0f);

/// <summary> Create or resize viewport render target. </summary> 
export void EnsureViewportTarget(int _w, int _h);
/// <summary> Multisampled target down into the plain one </summary> 
export void ResolveViewportTarget();               
/// <summary> Shutdown / delete viewport resources. </summary>
export void ShutdownViewportTarget();              

// Clear any prior viewport data
static void DestroyViewportTarget_Internal() {
	if (ViewportDepthStencilMS) { glDeleteTextures(1, &ViewportDepthStencilMS); ViewportDepthStencilMS = 0; }
	if (ViewportColorMS) { glDeleteTextures(1, &ViewportColorMS); ViewportColorMS = 0; }
	if (ViewportFBO) { glDeleteFramebuffers(1, &ViewportFBO); ViewportFBO = 0; }

	if (ViewportColorTex) { glDeleteTextures(1, &ViewportColorTex); ViewportColorTex = 0; }
	if (ViewportResolveFBO) { glDeleteFramebuffers(1, &ViewportResolveFBO); ViewportResolveFBO = 0; }

	ViewportW = 0;
	ViewportH = 0;
	GBuiltSamples = 0;
}

export void EnsureViewportTarget(int _w, int _h) {
	if (_w <= 0 || _h <= 0) return;

	// No change, the target is already the right size and sample count.
	if (ViewportFBO != 0 && ViewportW == _w && ViewportH == _h && GBuiltSamples == ViewportSamples)
		return;

	// Clear prior viewport data internals 
	DestroyViewportTarget_Internal();

	// ======= Validation complete, commence ViewportFBO resolve =======

	// Asking for more samples than the driver supports fails the whole framebuffer, so clamp. Thanks opengl
	GLint maxSamples = 1;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	if (ViewportSamples < 1) ViewportSamples = 1;
	if (ViewportSamples > (int)maxSamples) ViewportSamples = (int)maxSamples;

	ViewportW = _w;
	ViewportH = _h;
	GBuiltSamples = ViewportSamples;

	// ---------------- The multisampled target everything renders into ----------------
	glGenFramebuffers(1, &ViewportFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);

	// Color. Multisample textures take no filter settings, they are never sampled directly.
	glGenTextures(1, &ViewportColorMS);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ViewportColorMS);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, ViewportSamples, GL_RGBA8, _w, _h, GL_TRUE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ViewportColorMS, 0);

	// Stencil Depth texture
	glGenTextures(1, &ViewportDepthStencilMS);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ViewportDepthStencilMS);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, ViewportSamples, GL_DEPTH24_STENCIL8, _w, _h, GL_TRUE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, ViewportDepthStencilMS, 0);

	// Validate
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		// Error
		LogWarning("EnsureViewportTarget: multisampled framebuffer incomplete, viewport target destroyed.");
		DestroyViewportTarget_Internal();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	// ---------------- The plain target the multisampled one resolves into ----------------
	glGenFramebuffers(1, &ViewportResolveFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ViewportResolveFBO);

	glGenTextures(1, &ViewportColorTex);
	glBindTexture(GL_TEXTURE_2D, ViewportColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _w, _h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ViewportColorTex, 0);

	// Validate
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		// Error
		LogWarning("EnsureViewportTarget: resolve framebuffer incomplete, viewport target destroyed.");
		DestroyViewportTarget_Internal();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Copies the multisampled scene into the plain resolved one, so post processing can use it.
export void ResolveViewportTarget() {
	if (ViewportFBO == 0 || ViewportResolveFBO == 0) return;

	// 1. Bind the source FBO for reading
	glBindFramebuffer(GL_READ_FRAMEBUFFER, ViewportFBO);

	// 2. Bind the destination FBO for drawing
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ViewportResolveFBO);

	// 3. 1:1 direct copy of the color buffer
	glBlitFramebuffer(
		0, 0, ViewportW, ViewportH,
		0, 0, ViewportW, ViewportH,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST
	);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

export void ShutdownViewportTarget() {
	DestroyViewportTarget_Internal();
}


export void UpdateTimeData() {
	double now = glfwGetTime();
	gDeltaTime = (float)(now - gTimeSinceAppStart);
	gTimeSinceAppStart = now;
}
