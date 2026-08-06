module;

#include <glew.h>

/// <summary>
/// The quad that covers the whole window. Used by post processing, the selection outline 
/// all draw with this one, so there is only ever a single quad sitting on the GPU.
///
/// Passes use vertex shader: kFullscreenVertPath with its own fragment shader.
/// </summary>
export module RendererFullscreenQuad;

// The vertex shader every fullscreen pass uses.
export constexpr const char* kFullscreenVertPath = "Assets/Shaders/Common/Fullscreen.vert";

static GLuint GQuadVAO = 0;
static GLuint GQuadVBO = 0;

// Constructs a fullscreen quad that maps verts & uv's.
export void InitFullscreenQuad() {
    if (GQuadVAO != 0) return; 

    // pos.xy, uv
    const float verts[] = {
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,

        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f,
    };

    glGenVertexArrays(1, &GQuadVAO);
    glGenBuffers(1, &GQuadVBO);

    glBindVertexArray(GQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, GQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    // This quad has its own layout, it does not share the P3 N3 UV2 one the models use.
    constexpr GLsizei stride = sizeof(float) * 4;

    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);

    glEnableVertexAttribArray(1); // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 2));

    glBindVertexArray(0);
}

export void ShutdownFullscreenQuad() {
    if (GQuadVBO) { glDeleteBuffers(1, &GQuadVBO); GQuadVBO = 0; }
    if (GQuadVAO) { glDeleteVertexArrays(1, &GQuadVAO); GQuadVAO = 0; }
}

// True once the quad exists. Passes check this before trying to draw with it.
export bool FullscreenQuadReady() { return GQuadVAO != 0; }

// Draws the quad. Bind program and set uniforms first.
export void DrawFullscreenQuad() {
    if (GQuadVAO == 0) return;

    glBindVertexArray(GQuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
