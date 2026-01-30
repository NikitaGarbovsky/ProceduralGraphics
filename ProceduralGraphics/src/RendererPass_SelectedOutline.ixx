module;
#include <glew.h>
#include <glm.hpp>

/// 
/// When an REntity is selected on mouse click, this outline, using the stencil pass.
/// 
export module RendererPass_SelectedOutline;

import RendererFrame;
import RendererEntitys;
import RendererData;       // SelectedEntity, etc.
import DebugUtilities;
import <gtc/type_ptr.hpp>;

static GLuint sOutlineInstanceVBO = 0;

export void InitSelectedOutlinePass()
{
    glGenBuffers(1, &sOutlineInstanceVBO);
}

export void ShutdownSelectedOutlinePass()
{
    if (sOutlineInstanceVBO) glDeleteBuffers(1, &sOutlineInstanceVBO);
    sOutlineInstanceVBO = 0;
}

static void DrawSelectedOnce(const FrameCommon& _fcommon, uint32_t _entID, GLuint _program, float _scale)
{
    const REntity& e = CurrentRenderedEntitys[_entID];

    // entity pivot so the whole object “inflates” consistently
    const glm::vec3 pivot = e.localBounds.center;

    for (uint32_t i = 0; i < e.submeshCount; ++i) // #TODO: maybe make this into a helper instead of any pass can use it.
    {
        const Submesh& sm = REntitySubmeshes[e.firstSubmesh + i];
        const Mesh& mesh = REntityMeshs[sm.mesh];

        InstanceData inst{};
        inst.model = EntityTransforms.worldMatrix[_entID];
        inst.entityId = _entID + 1;

        glBindBuffer(GL_ARRAY_BUFFER, sOutlineInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), &inst, GL_STREAM_DRAW);

        glUseProgram(_program);
        // #TODO: Cache uniforms instead of every submesh draw like it is here.
        glUniformMatrix4fv(glGetUniformLocation(_program, "ViewMat"), 1, GL_FALSE, glm::value_ptr(_fcommon.view));
        glUniformMatrix4fv(glGetUniformLocation(_program, "ProjectionMat"), 1, GL_FALSE, glm::value_ptr(_fcommon.proj));
        glUniform3fv(glGetUniformLocation(_program, "Pivot"), 1, glm::value_ptr(pivot));
        glUniform1f(glGetUniformLocation(_program, "OutlineScale"), _scale);

        glBindVertexArray(mesh.vao);
        BindInstanceAttribs(mesh.vao, sOutlineInstanceVBO, 0);

        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, nullptr, 1);
    }
}

export void SelectedOutlinePass_Execute(const FrameCommon& _fcommon, uint32_t _selectedEntity, GLuint _outlineProgram) {
    if (_selectedEntity == UINT32_MAX) return; 
    if (_selectedEntity >= CurrentRenderedEntitys.size()) return; 
    if (sOutlineInstanceVBO == 0) { LogCrash("Outline pass not initialized"); return; }

    glEnable(GL_STENCIL_TEST);

    // -------- Pass A: write stencil where selected object is visible --------
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    DrawSelectedOnce(_fcommon, _selectedEntity, _outlineProgram, 1.0f);

    // -------- Pass B: draw expanded mesh only where stencil != 1 --------
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Front-face cull on expanded mesh
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glUseProgram(_outlineProgram);

    glUniform4fv(glGetUniformLocation(_outlineProgram, "OutlineColor"), 1, glm::value_ptr(SelectedREntityColorOutline));

    DrawSelectedOnce(_fcommon, _selectedEntity, _outlineProgram, SelectedREntityOutlineScale);

    // -------- Restore --------
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}
