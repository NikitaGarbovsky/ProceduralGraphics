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
import RendererCamera;
import RendererUtilities;
import DebugUtilities;
import RendererTransformUtils;
import RendererPicking;

// Render Passes
import RendererPass_Opaque;
import RendererPass_Picking;
import RendererPass_SelectedOutline;
import RendererPass_SelectedTint;
import RendererPass_DebugBounds;
import RendererLights;

// Frame & pass data that is necessary for per-frame rendering.
static FrameCommon fcommon;
static PassContext opaquePassContext;

export void InitRendererPipeline() {
    InitPassContext(opaquePassContext);   // glGenBuffers
    InitSelectedOutlinePass();
    InitSelectedTintPass();
    InitDebugBoundsPass();
    InitLights();
}

export void ShutdownRendererPipeline() {
    ShutdownLights();
    ShutdownDebugBoundsPass();
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
    // ===========^ Compute all the common stuff the rest pipeline may use during this frame ^===========
    
     
    // -------------- Passes in Order -------------- 
    OpaquePass_Build(fcommon, opaquePassContext);
    UpdateLights();
    OpaquePass_Execute(fcommon, opaquePassContext);
    

    if (PickingIsRequested()) {
        // Picking Pass requires opaquePassContext
        PickingPass_Execute(fcommon, opaquePassContext, PickingProgram);

        PickingReadback();
    }

    SelectedTintPass_Execute(fcommon, SelectedEntity, SelectedTintProgram);
    SelectedOutlinePass_Execute(fcommon, SelectedEntity, OutlineProgram);
    DebugBoundsPass_Execute(fcommon, SelectedEntity);

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
