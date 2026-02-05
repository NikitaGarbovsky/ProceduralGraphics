module;

#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>

// This module contains a bunch of global accessible data that is used throughout the 
// renderer. 
// 
// #TODO viewport & FBO helper functions maybe need to go into their own module.
export module RendererData;

// Globals to utilize that are core of the Renderer.
export GLFWwindow* MainWindow;
export GLuint RenderObjProgram; 
export GLuint PickingProgram; // SHader that runs the picking program for editor mouse click selection. 
export GLuint OutlineProgram;
export GLuint SelectedTintProgram;
export double gTimeSinceAppStart = 0.0;
export float gDeltaTime = 0.0; // TODO move this when expanding the renderer beyond a renderer.

// Viewport Render Target (Offscreen)
export GLuint ViewportFBO = 0; // Entire scene is rendered to this.
export GLuint ViewportColorTex = 0; // RGBA8
export GLuint ViewportDepthStencilTex = 0; // DEPTH24

// Viewport dimensions (These are automatically evaluated/updated at the start of each frame, no need to set)
export int ViewportW = 0; 
export int ViewportH = 0;

// ID of the REntity that is currently selected.
export uint32_t SelectedEntity = UINT32_MAX;

// ID of the Light that is currently selected.
export uint32_t SelectedLight = UINT32_MAX;

// Editable data for selection 
export float SelectedREntityOutlineScale = 1.02f;
export glm::vec4 SelectedREntityColorOutline(1.0f, 1.0f, 1.0f, 1.0f);
export float SelectedREntityTintPulseSpeed = 4.0f;
export float SelectedREntityTintStrength = 0.65f;
export glm::vec3 SelectedREntityTintColor(1.0f, 1.0f, 1.0f);

// Function prototypes. TODO maybe move these to a dedicated viewport management/utils module? 
export void EnsureViewportTarget(int _w, int _h); // Create or resize viewport render target
export void ShutdownViewportTarget(); // Shutdown / delete viewport resources

static void DestroyViewportTarget_Internal() {
	if (ViewportDepthStencilTex) { glDeleteTextures(1, &ViewportDepthStencilTex); ViewportDepthStencilTex = 0; }
	if (ViewportColorTex) { glDeleteTextures(1, &ViewportColorTex); ViewportColorTex = 0; }
	if (ViewportFBO) { glDeleteFramebuffers(1, &ViewportFBO); ViewportFBO = 0; }

	ViewportW = 0;
	ViewportH = 0;
}

export void EnsureViewportTarget(int _w, int _h) {
	if (_w <= 0 || _h <= 0) return;

	// No change, viewport size is the same as FBO, no need to resolve ViewportFBO 
	if (ViewportFBO != 0 && ViewportW == _w && ViewportH == _h)
		return;

    // Clear prior viewport data internals 
	DestroyViewportTarget_Internal();

    // ======= Validation complete, commence ViewportFBO resolve =======
	ViewportW = _w;
	ViewportH = _h;

    glGenFramebuffers(1, &ViewportFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);

    // Color texture (what the editor viewport will display)
    glGenTextures(1, &ViewportColorTex);
    glBindTexture(GL_TEXTURE_2D, ViewportColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _w, _h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ViewportColorTex, 0);

    // Stencil Depth texture
    glGenTextures(1, &ViewportDepthStencilTex);
    glBindTexture(GL_TEXTURE_2D, ViewportDepthStencilTex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, _w, _h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ViewportDepthStencilTex, 0);

    // Validate
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        DestroyViewportTarget_Internal();
    }

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