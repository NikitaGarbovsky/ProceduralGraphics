module;

#include <vector>
#include <cmath>
#include <algorithm>

#include <glew.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

/// <summary>
/// This module manages the visualization of the 3D bounds that is rendered on top of the models.
/// #TODO: probably move more stuff editor debug stuff into this pass (editor grid, gizmos etc)
/// </summary>
export module RendererPass_DebugBounds;

import RendererData;
import RendererEntitys;
import RendererFrame;
import RendererUtilities;   // For LoadShaderProgram
import DebugUtilities;

static GLuint boundsProgram = 0;
static GLuint boundsVAO = 0, boundsVBO = 0;

static GLint uView = -1;
static GLint uProj = -1;
static GLint uModel = -1;
static GLint uColor = -1;

static bool boundsEnabled = false;
static bool boundsSelectedOnly = true;

export void DebugBoundsSetEnabled(bool _v) { boundsEnabled = _v; }
export void DebugBoundsSetSelectedOnly(bool _v) { boundsSelectedOnly = _v; }

export void InitDebugBoundsPass() {

    // Load the bounds shader
    boundsProgram = LoadShaderProgram(
        "Assets/Shaders/Debug/boundsVisualization.vert",
        "Assets/Shaders/Debug/boundsVisualization.frag"
    );

    // Cache uniforms
    uView = glGetUniformLocation(boundsProgram, "ViewMat");
    uProj = glGetUniformLocation(boundsProgram, "ProjectionMat");
    uModel = glGetUniformLocation(boundsProgram, "ModelMat");
    uColor = glGetUniformLocation(boundsProgram, "Color");

    // build unit circle in XY plane
    constexpr int SEG = 64;
    std::vector<glm::vec3> circle;
    circle.reserve(SEG);

    for (int i = 0; i < SEG; ++i)
    {
        float t = (float)i / (float)SEG * 6.28318530718f; // 2*pi
        circle.push_back(glm::vec3(std::cos(t), std::sin(t), 0.0f));
    }

    glGenVertexArrays(1, &boundsVAO);
    glGenBuffers(1, &boundsVBO);

    glBindVertexArray(boundsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boundsVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(circle.size() * sizeof(glm::vec3)), circle.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
}

export void ShutdownDebugBoundsPass() {
    if (boundsVBO) glDeleteBuffers(1, &boundsVBO), boundsVBO = 0;
    if (boundsVAO) glDeleteVertexArrays(1, &boundsVAO), boundsVAO = 0;
    if (boundsProgram) glDeleteProgram(boundsProgram), boundsProgram = 0;
}

static void DrawSphereAs3Circles(const glm::mat4& _baseModel) {
    // XY circle already.
    glm::mat4 Mxy = _baseModel;

    // XZ: rotate XY circle so its normal becomes Y
    glm::mat4 Mxz = _baseModel * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0));

    // YZ: rotate XY circle so its normal becomes X
    glm::mat4 Myz = _baseModel * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));

    glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(Mxy));
    glDrawArrays(GL_LINE_LOOP, 0, 64);

    glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(Mxz));
    glDrawArrays(GL_LINE_LOOP, 0, 64);

    glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(Myz));
    glDrawArrays(GL_LINE_LOOP, 0, 64);
}

export void DebugBoundsPass_Execute(const FrameCommon& _fcommon, uint32_t _selectedEntity) {
    if (!boundsEnabled || boundsProgram == 0 || boundsVAO == 0) return;

    // Draw into same target as main scene
    glBindFramebuffer(GL_FRAMEBUFFER, ViewportFBO);
    glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);

    glUseProgram(boundsProgram);
    if (uView >= 0) glUniformMatrix4fv(uView, 1, GL_FALSE, glm::value_ptr(_fcommon.view));
    if (uProj >= 0) glUniformMatrix4fv(uProj, 1, GL_FALSE, glm::value_ptr(_fcommon.proj));

    // Constant debug color #TODO: make this editable in editor
    if (uColor >= 0) glUniform4f(uColor, 1, 1, 0, 1);

    glBindVertexArray(boundsVAO);

    // Depth test on so it sits “in” the world, but don't write depth
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    auto drawOne = [&](uint32_t entIndex)
        {
            const REntity& e = CurrentRenderedEntitys[entIndex];
            const Bounds& b = e.localBounds;

            const glm::mat4& M = EntityTransforms.worldMatrix[entIndex];

            glm::vec3 worldCenter = glm::vec3(M * glm::vec4(b.center, 1.0f));

            glm::vec3 s = EntityTransforms.scale[entIndex];
            float maxScale = std::max({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
            float worldRadius = b.radius * maxScale;

            glm::mat4 base =
                glm::translate(glm::mat4(1.0f), worldCenter) *
                glm::scale(glm::mat4(1.0f), glm::vec3(worldRadius));

            DrawSphereAs3Circles(base);
        };

    if (boundsSelectedOnly)
    {
        if (_selectedEntity != 0xFFFFFFFFu && _selectedEntity < CurrentRenderedEntitys.size())
            drawOne(_selectedEntity);
    }
    else
    {
        for (uint32_t i = 0; i < (uint32_t)CurrentRenderedEntitys.size(); ++i)
            drawOne(i);
    }

    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
