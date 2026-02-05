#include <glfw3.h>
export module RendererPass_SelectedTint;

import RendererFrame;
import RendererEntitys;
import RendererData;
import <gtc/type_ptr.hpp>;
import <glew.h>;

static GLuint sTintInstanceVBO = 0;

export void InitSelectedTintPass()
{
    glGenBuffers(1, &sTintInstanceVBO);
}

export void ShutdownSelectedTintPass()
{
    if (sTintInstanceVBO) glDeleteBuffers(1, &sTintInstanceVBO);
    sTintInstanceVBO = 0;
}

static void DrawSelectedTintOnce(const FrameCommon& f, uint32_t ent, GLuint program)
{
    const REntity& e = CurrentRenderedEntitys[ent];

    for (uint32_t i = 0; i < e.submeshCount; ++i)
    {
        const Submesh& sm = REntitySubmeshes[e.firstSubmesh + i];
        const Mesh& mesh = REntityMeshs[sm.mesh];
        const Material& mat = REntityMaterials[sm.material];

        InstanceData inst{};
        inst.model = EntityTransforms.worldMatrix[ent];
        inst.entityId = ent + 1;

        glBindBuffer(GL_ARRAY_BUFFER, sTintInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), &inst, GL_STREAM_DRAW);

        glUseProgram(program);

        glUniformMatrix4fv(glGetUniformLocation(program, "ViewMat"), 1, GL_FALSE, glm::value_ptr(f.view));
        glUniformMatrix4fv(glGetUniformLocation(program, "ProjectionMat"), 1, GL_FALSE, glm::value_ptr(f.proj));

        // Time + pulse params
        glUniform1f(glGetUniformLocation(program, "TimeSeconds"), (float)glfwGetTime());
        glUniform3fv(glGetUniformLocation(program, "TintColor"), 1, glm::value_ptr(SelectedREntityTintColor));
        glUniform1f(glGetUniformLocation(program, "TintStrength"), 0.65f);
        glUniform1f(glGetUniformLocation(program, "PulseSpeed"), 4.0f);

        // Bind selected entity texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat.tex0);
        glUniform1i(glGetUniformLocation(program, "Tex0"), 0);

        glBindVertexArray(mesh.vao);
        BindInstanceAttribs(mesh.vao, sTintInstanceVBO, 0);

        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, nullptr, 1);
    }
}

export void SelectedTintPass_Execute(const FrameCommon& f, uint32_t selectedEntity, GLuint tintProgram)
{
    if (selectedEntity == 0xFFFFFFFFu) return;
    if (selectedEntity >= CurrentRenderedEntitys.size()) return;
    if (sTintInstanceVBO == 0) return;

    // Blend on, depth test on (so it doesn’t show through other objects)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // Optional: helps if you see z-fighting shimmer on the overlay
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    DrawSelectedTintOnce(f, selectedEntity, tintProgram);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(0);
}
