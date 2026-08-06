module;

#include <glew.h>
#include <glm.hpp>

/// <summary>
/// Draws the outline around the selected entity.
///
/// How it works: the selected entity gets drawn into a small black and white mask texture,
/// solid white wherever it covers the screen. A fullscreen pass then grows that mask outwards
/// by a few pixels and keeps only the part that grew, which is the outline that is rendrered.
///
/// The mask is multisampled and then resolved, same as the scene target.
///
/// The mask ignores depth on purpose, so a selected object behind a wall still shows its
/// outline.
/// </summary>
export module RendererPass_SelectedOutline;

import RendererFrame;
import RendererEntitys;
import RendererData;      
import RendererUtilities;  
import RendererFullscreenQuad;
import DebugUtilities;
import <gtc/type_ptr.hpp>;

// The mask target. Only color attachments, no depth, because the mask is drawn without any
// depth testing anyway.
static GLuint SMaskFBO = 0;        // Multisampled, the entity gets drawn into this.
static GLuint SMaskColorMS = 0;    // Multisampled color attachment.
static GLuint SMaskResolveFBO = 0; // Single sample.
static GLuint SMaskTex = 0;        // R8, this is what the outline shader samples.

static int SMaskW = 0;
static int SMaskH = 0;
static int SMaskSamples = 0; // What the mask was actually built with.

static GLuint SMaskProgram = 0;    // Draws the entity into the mask.
static GLuint SOutlineProgram = 0; // Grows the mask and draws the outline.

static GLuint SInstanceVBO = 0;

// Cached uniforms
static GLint SMaskViewLoc = -1, SMaskProjLoc = -1;
static GLint SOutMaskLoc = -1, SOutResolutionLoc = -1, SOutColorLoc = -1, SOutWidthLoc = -1;

static void DestroyMaskTarget()
{
    if (SMaskColorMS) { glDeleteTextures(1, &SMaskColorMS); SMaskColorMS = 0; }
    if (SMaskFBO) { glDeleteFramebuffers(1, &SMaskFBO); SMaskFBO = 0; }
    if (SMaskTex) { glDeleteTextures(1, &SMaskTex); SMaskTex = 0; }
    if (SMaskResolveFBO) { glDeleteFramebuffers(1, &SMaskResolveFBO); SMaskResolveFBO = 0; }

    SMaskW = 0;
    SMaskH = 0;
    SMaskSamples = 0;
}

// Makes the mask targets match the window size and the scene's sample count.
static void EnsureMaskTarget(int _w, int _h)
{
    if (_w <= 0 || _h <= 0) return;
    if (SMaskFBO != 0 && SMaskW == _w && SMaskH == _h && SMaskSamples == ViewportSamples) return;

    DestroyMaskTarget();

    SMaskW = _w;
    SMaskH = _h;
    SMaskSamples = ViewportSamples; 

    // ---------------- The multisampled mask the entity draws into ----------------
    glGenFramebuffers(1, &SMaskFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, SMaskFBO);

    // Single channel is all a black and white mask needs.
    glGenTextures(1, &SMaskColorMS);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, SMaskColorMS);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, SMaskSamples, GL_R8, _w, _h, GL_TRUE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, SMaskColorMS, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LogWarning("EnsureMaskTarget: multisampled selection mask incomplete, outline disabled.");
        DestroyMaskTarget();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // ---------------- The plain mask the outline shader reads ----------------
    glGenFramebuffers(1, &SMaskResolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, SMaskResolveFBO);

    glGenTextures(1, &SMaskTex);
    glBindTexture(GL_TEXTURE_2D, SMaskTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, _w, _h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Clamping stops the outline wrapping around to the other side of the screen when the
    // object is right up against an edge.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SMaskTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LogWarning("EnsureMaskTarget: selection mask resolve target incomplete, outline disabled.");
        DestroyMaskTarget();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

export void InitSelectedOutlinePass()
{
    glGenBuffers(1, &SInstanceVBO);

    SMaskProgram = LoadShaderProgram(
        "Assets/Shaders/Editor/SelectionMask.vert",
        "Assets/Shaders/Editor/SelectionMask.frag");

    SOutlineProgram = LoadShaderProgram(
        kFullscreenVertPath,
        "Assets/Shaders/Editor/SelectionOutline.frag");

    // Cache the mask uniforms.
    if (SMaskProgram != 0) {
        SMaskViewLoc = glGetUniformLocation(SMaskProgram, "ViewMat");
        SMaskProjLoc = glGetUniformLocation(SMaskProgram, "ProjectionMat");
    }

    // Cache the outline uniforms.
    if (SOutlineProgram != 0) {
        SOutMaskLoc = glGetUniformLocation(SOutlineProgram, "MaskTex");
        SOutResolutionLoc = glGetUniformLocation(SOutlineProgram, "Resolution");
        SOutColorLoc = glGetUniformLocation(SOutlineProgram, "OutlineColor");
        SOutWidthLoc = glGetUniformLocation(SOutlineProgram, "OutlineWidth");

        // The mask always sits on unit 0.
        if (SOutMaskLoc != -1)
            glProgramUniform1i(SOutlineProgram, SOutMaskLoc, 0);
    }
}

export void ShutdownSelectedOutlinePass()
{
    if (SInstanceVBO) { glDeleteBuffers(1, &SInstanceVBO); SInstanceVBO = 0; }
    if (SMaskProgram) { glDeleteProgram(SMaskProgram); SMaskProgram = 0; }
    if (SOutlineProgram) { glDeleteProgram(SOutlineProgram); SOutlineProgram = 0; }

    DestroyMaskTarget();
}

// Draws every submesh of the entity into the solid white mask.
static void DrawEntityIntoMask(uint32_t _entID) {
    const REntity& e = CurrentRenderedEntitys[_entID];

    for (uint32_t i = 0; i < e.submeshCount; ++i) {
        const Submesh& sm = REntitySubmeshes[e.firstSubmesh + i];
        const Mesh& mesh = REntityMeshs[sm.mesh];

        InstanceData inst{};
        inst.model = EntityTransforms.worldMatrix[_entID];

        glBindBuffer(GL_ARRAY_BUFFER, SInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), &inst, GL_STREAM_DRAW);

        glBindVertexArray(mesh.vao);
        BindInstanceAttribs(mesh.vao, SInstanceVBO, 0);

        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, nullptr, 1);
    }
}

export void SelectedOutlinePass_Execute(const FrameCommon& _fcommon, uint32_t _selectedEntity)
{
    if (_selectedEntity == UINT32_MAX) return;
    if (_selectedEntity >= CurrentRenderedEntitys.size()) return;
    if (SMaskProgram == 0 || SOutlineProgram == 0 || SInstanceVBO == 0) return;
    if (!FullscreenQuadReady()) return;

    EnsureMaskTarget(_fcommon.viewportW, _fcommon.viewportH);
    if (SMaskFBO == 0) return;

    // ---------------- 1. Draw the selected entity into the mask ----------------
    glBindFramebuffer(GL_FRAMEBUFFER, SMaskFBO);
    glViewport(0, 0, SMaskW, SMaskH);

    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    glUseProgram(SMaskProgram);
    if (SMaskViewLoc != -1) glUniformMatrix4fv(SMaskViewLoc, 1, GL_FALSE, glm::value_ptr(_fcommon.view));
    if (SMaskProjLoc != -1) glUniformMatrix4fv(SMaskProjLoc, 1, GL_FALSE, glm::value_ptr(_fcommon.proj));

    DrawEntityIntoMask(_selectedEntity);

    // ---------------- 2. Flatten it so the outline shader can read it ----------------
    glBindFramebuffer(GL_READ_FRAMEBUFFER, SMaskFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SMaskResolveFBO);

    glBlitFramebuffer(
        0, 0, SMaskW, SMaskH,
        0, 0, SMaskW, SMaskH,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    // ---------------- 3. Grow the mask and lay the outline over the scene ----------------
    glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
    glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(SOutlineProgram);
    if (SOutResolutionLoc != -1)
        glUniform2f(SOutResolutionLoc, (float)_fcommon.viewportW, (float)_fcommon.viewportH);
    if (SOutColorLoc != -1)
        glUniform4fv(SOutColorLoc, 1, glm::value_ptr(SelectedREntityColorOutline));
    if (SOutWidthLoc != -1)
        glUniform1f(SOutWidthLoc, SelectedREntityOutlineWidth);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, SMaskTex);

    DrawFullscreenQuad();

    // Reset all necessary state.
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
