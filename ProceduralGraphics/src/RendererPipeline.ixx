#include <glew.h>

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

// Helper used for frustrum culling
static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 outPlanes[6]);

// Frame & pass data that is necessary for per-frame rendering.
static FrameCommon fcommon;
static PassContext opaquePassContext;

// Executes each render pass in sequence per frame.
export void RenderPipeline_RenderFrame(int _viewportW, int _viewportH) {
    // ============ Compute all the common stuff the pipeline will use ============
    
    // Assign viewport size this frame.
    fcommon.viewportW = _viewportW;
    fcommon.viewportH = _viewportH;

    // Grab the current camera view and proj
    fcommon.view = GCamera.view;
    fcommon.proj = GCamera.proj;

    // Combine for usage.
    fcommon.viewProj = fcommon.proj * fcommon.view;

    // Calculate the camera pos & camera forward.
    glm::mat4 invV = glm::inverse(fcommon.view);
    fcommon.cameraPos = glm::vec3(invV[3]);
    fcommon.cameraForward = glm::normalize(-glm::vec3(invV[2])); // forward is -Z

    // This caches the view 
    ExtractFrustumPlanes(fcommon.viewProj, fcommon.frustrumPlanes);
    // ============ Compute all the common stuff the pipeline will use ============
    
     
    // -------------- Passes in Order -------------- 
    opaquePassContext.Clear();
    OpaquePass_Build(fcommon, opaquePassContext);
    OpaquePass_Execute(fcommon, opaquePassContext);
}

// #TODO move this to a common shared utilities file
static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 outPlanes[6]) {
    // Build rows from column-major matrix (glm::mat4 is column-major)
    const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

    // Planes: (a,b,c,d) with ax + by + cz + d = 0
    outPlanes[0] = row3 + row0; // Left
    outPlanes[1] = row3 - row0; // Right
    outPlanes[2] = row3 + row1; // Bottom
    outPlanes[3] = row3 - row1; // Top
    outPlanes[4] = row3 + row2; // Near  
    outPlanes[5] = row3 - row2; // Far

    // Normalize planes (so distance tests are correct)
    for (int i = 0; i < 6; i++) {
        glm::vec3 n(outPlanes[i]);
        float len = glm::length(n);
        if (len > 0.0f) outPlanes[i] /= len;
    }
}

export void InitRendererPipeline()
{
    InitPassContext(opaquePassContext);   // glGenBuffers
}

export void ShutdownRendererPipeline()
{
    ShutDownPassContext(opaquePassContext);
}