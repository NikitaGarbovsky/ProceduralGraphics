#include <glew.h>
#include <string>
#include <glfw3.h>

/// <summary>
/// Goal: Submit as few, as well-organized draw calls as possible while drawing
/// exactly & ONLY what the camera needs (sees).
/// 
/// This module manages the render pipeline that is run per frame. (Called in RendererCore, Render())
/// It acts as a schedular and ordering system for each pass, #TODO later when passes reach 8-15, refactor this to work
/// with a rendergraph.
/// </summary>
export module RendererPipeline;

// Includes
import <algorithm>;
import <glm.hpp>;

// Codebase module imports
import RendererEntitys;
import RendererFrame;
import RendererData; // For GCamera
import RendererPass_Opaque;
import RendererPass_Picking;
import DebugUtilities;
import RendererPass_SelectedOutline;
import RendererPass_SelectedTint;
import RendererCamera;
import RendererUtilities;
import RendererTransformUtils;

// Frame & pass data that is necessary for per-frame rendering.
static FrameCommon fcommon;
static PassContext opaquePassContext;

// #TODO: Move all this somewhere outta here.
static bool gPickRequested = false;
static bool gPickHasResult = false;
static int gPickX = 0, gPickY = 0;
static uint32_t gPickResult = 0; // raw ID from buffer

export void InitRendererPipeline() {
    InitPassContext(opaquePassContext);   // glGenBuffers
    InitSelectedOutlinePass();
    InitSelectedTintPass();
}

export void ShutdownRendererPipeline() {
    ShutdownSelectedOutlinePass();
    ShutdownSelectedTintPass();
    ShutDownPassContext(opaquePassContext);
}

// Executes each render pass in sequence per frame.
export void RenderPipeline_RenderFrame(int _viewportW, int _viewportH) {

    // ============ Compute all the common stuff the rest pipeline may use during this frame ============
    
    // Make sure the offscreen viewport target exists at this size
    EnsureViewportTarget(_viewportW, _viewportH);

    // Render scene into the offscreen viewport FBO
    glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
    glViewport(0, 0, _viewportW, _viewportH);
    glStencilMask(0xFF);   // glClear respects stencil mask
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Update frame data
    fcommon.viewportW = _viewportW;
    fcommon.viewportH = _viewportH;
    fcommon.view = GCamera.view;
    fcommon.proj = GCamera.proj;
    fcommon.viewProj = fcommon.proj * fcommon.view;
    glm::mat4 invV = glm::inverse(fcommon.view);
    fcommon.cameraPos = glm::vec3(invV[3]);
    fcommon.cameraForward = glm::normalize(-glm::vec3(invV[2])); // forward is -Z

    ExtractFrustumPlanes(fcommon.viewProj, fcommon.frustrumPlanes);

    opaquePassContext.Clear();
    // ===========^ Compute all the common stuff the pipeline will use ^===========
    
     
    // -------------- Passes in Order -------------- 
    OpaquePass_Build(fcommon, opaquePassContext);
    OpaquePass_Execute(fcommon, opaquePassContext);
    
    // #TODO: Maybe put this in a dedicated picking module.
    if (gPickRequested) {
        // Picking Pass requires opaquePassContext
        PickingPass_Execute(fcommon, opaquePassContext, PickingProgram);

        // Read pixel (note: OpenGL origin is bottom-left)
        uint32_t id = 0;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, PickingFBO);
        glReadPixels(gPickX, gPickY, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        gPickResult = id;        // raw (0 = none)
        gPickHasResult = true;
        gPickRequested = false;  // consume request
    }

    SelectedTintPass_Execute(fcommon, SelectedEntity, SelectedTintProgram);
    SelectedOutlinePass_Execute(fcommon, SelectedEntity, OutlineProgram);


    // =========== RENDERED FRAME COMPLETE ===========
    
    // Now read and draw all that render data for this frame into the frame buffer object
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ViewportFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glBlitFramebuffer(
        0, 0, _viewportW, _viewportH,
        0, 0, _viewportW, _viewportH,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// #TODO: Maybe move all this picking stuff into their own PickingModule.
// ===================== Helpers for picking REntity's =====================

static void SetCorrectPixelCoordinatesOnClick(GLFWwindow* _window, int& _outX, int& _outY) {
    double mx, my;
    glfwGetCursorPos(_window, &mx, &my);

    int winW, winH, fbW, fbH;
    glfwGetWindowSize(_window, &winW, &winH);
    glfwGetFramebufferSize(_window, &fbW, &fbH);

    // Convert window coords to framebuffer pixel coords
    double sx = (winW > 0) ? (double)fbW / (double)winW : 1.0;
    double sy = (winH > 0) ? (double)fbH / (double)winH : 1.0;

    int px = int(mx * sx);
    int py = int(my * sy);

    // OpenGL pixel origin is bottom-left:
    py = fbH - 1 - py;

    // Clamp
    px = std::max(0, std::min(px, fbW - 1));
    py = std::max(0, std::min(py, fbH - 1));

    _outX = px;
    _outY = py;
}

// Returns raw ID, clears it
export uint32_t RenderPipeline_ConsumePickResult() {
    gPickHasResult = false;
    return gPickResult;
}

export void RenderPipeline_RequestPick() {
    gPickRequested = true;
    SetCorrectPixelCoordinatesOnClick(MainWindow, gPickX, gPickY);
}

export bool RenderPipeline_HasPickResult() {
    return gPickHasResult;
}

// ===================== Helpers for picking REntity's =====================