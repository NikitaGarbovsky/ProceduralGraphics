module;

#include <algorithm>
#include <glew.h>
#include <string>

///
/// The main Picking pass of the renderering pipeline.
/// 
export module RendererPass_Picking;

import RendererFrame;
import RendererEntitys;
import <gtc/matrix_transform.hpp>;
import <gtc/type_ptr.hpp>;
import DebugUtilities;
import RendererData;

// Runs the picking pass shader program used to select REntities in the scene.
export void PickingPass_Execute(const FrameCommon& _f, PassContext& _p, GLuint _pickingProgram)
{
    EnsurePickingTarget(_f.viewportW, _f.viewportH);

    glBindFramebuffer(GL_FRAMEBUFFER, PickingFBO);
    glViewport(0, 0, _f.viewportW, _f.viewportH);

    GLuint zero = 0;
    glClearBufferuiv(GL_COLOR, 0, &zero);
    glClear(GL_DEPTH_BUFFER_BIT);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glUseProgram(_pickingProgram);

    // Set uniforms #TODO, use cached uniforms instead.
    glUniformMatrix4fv(glGetUniformLocation(_pickingProgram, "ViewMat"), 1, GL_FALSE, glm::value_ptr(_f.view));
    glUniformMatrix4fv(glGetUniformLocation(_pickingProgram, "ProjectionMat"), 1, GL_FALSE, glm::value_ptr(_f.proj));

    GLuint lastVAO = 0;

    for (auto& cmd : _p.commands)
    {
        const Mesh& mesh = REntityMeshs[cmd.mesh];

        if (mesh.vao != lastVAO)
        {
            BindInstanceAttribs(mesh.vao, _p.instanceVBO, 0);
            lastVAO = mesh.vao;
        }

        uintptr_t base = uintptr_t(cmd.instanceOffset) * sizeof(InstanceData);
        BindInstanceAttribs(mesh.vao, _p.instanceVBO, base);

        glBindVertexArray(mesh.vao);
        glDrawElementsInstanced(GL_TRIANGLES,
            (GLsizei)cmd.indexCount,
            GL_UNSIGNED_INT,
            nullptr,
            (GLsizei)cmd.instanceCount);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}