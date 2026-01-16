export module RendererAssetPipeline;

import RendererEntitys;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

export MeshID CreateMeshFromData_Pos3Uv2(const float* _vertices,
    uint32_t _vertexCount,
    const uint32_t* _indices,
    uint32_t _indexCount)
{
    // Each vertex: 5 floats (pos3 + uv2)
    constexpr uint32_t floatsPerVertex = 5;
    constexpr uint32_t strideBytes = floatsPerVertex * sizeof(float);

    Mesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(_vertexCount * strideBytes),
        _vertices,
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr)(_indexCount * sizeof(uint32_t)),
        _indices,
        GL_STATIC_DRAW);

    // aPos @ location 0 (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        (GLsizei)strideBytes,
        (void*)0);

    // aUV @ location 1 (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        (GLsizei)strideBytes,
        (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    mesh.indexCount = _indexCount;

    // Store mesh in the global mesh table and return its ID (index).
    MeshID id = (MeshID)REntityMeshs.size();
    REntityMeshs.push_back(mesh);
    return id;
}

export MeshID CreateQuadMesh()
{
    // Unit quad centered at origin
    static const float verts[] = {
        // pos                // uv
        -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,    1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,    1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,    0.0f, 1.0f,
    };

    uint32_t idx[] = { 0, 1, 2, 2, 3, 0 };

    return CreateMeshFromData_Pos3Uv2(
        verts, 4,
        idx, 6
    );
}

export MaterialID CreateMaterial(GLuint _program, GLuint _tex0 = 0)
{
    Material m{};
    m.program = _program;
    m.tex0 = _tex0;

    // Cache uniform locations once (so ExecuteCommands doesn't query every frame).
    m.uView = glGetUniformLocation(_program, "ViewMat");
    m.uProj = glGetUniformLocation(_program, "ProjectionMat");

    MaterialID id = (MaterialID)REntityMaterials.size();
    REntityMaterials.push_back(m);
    return id;
}

export void ValidateREntityArrayAlignment()
{
#ifndef NDEBUG
    const size_t n = CurrentRenderedEntitys.size();
    assert(EntityTransforms.position.size() == n);
    assert(EntityTransforms.rotation.size() == n);
    assert(EntityTransforms.scale.size() == n);
    assert(EntityTransforms.worldMatrix.size() == n);
    assert(REntityBounds.size() == n);
#endif
}