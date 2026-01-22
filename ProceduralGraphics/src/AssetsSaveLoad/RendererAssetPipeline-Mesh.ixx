#include <algorithm>

/// <summary>
/// This module manages the mesh of the asset loading pipeline. It also manages loading the mesh data 
/// to OpenGL
/// #TODO remove the bounds checking code from this module.
/// </summary>
export module RendererAssetPipeline:Mesh;

import RendererEntitys;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

struct VertexLayout
{
    uint32_t strideFloats;      // 8 for P3N3UV2
    uint32_t posOffsetFloats;   // usually 0
};

// Function prototypes
static Bounds ComputeBounds_Sphere(const float* _vertices, uint32_t _vertexCount, VertexLayout _layout);
export constexpr VertexLayout VertexLayout_P3N3UV2{ 8, 0 };

// FOR FRUSTRUM CULLING, calculates a simple sphere bounds from the vertices.
static Bounds ComputeBounds_Sphere(const float* _vertices, uint32_t _vertexCount, VertexLayout _layout)
{
    glm::vec3 minP(FLT_MAX);
    glm::vec3 maxP(-FLT_MAX);

    // Loop through all the vertices and calculate the fartherest away from one another
    for (uint32_t i = 0; i < _vertexCount; i++) {
        const float* v = _vertices + i * _layout.strideFloats + _layout.posOffsetFloats;
        glm::vec3 p(v[0], v[1], v[2]);
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }

    // Define the center point of the bounds (center point of mesh)
    glm::vec3 center = (minP + maxP) * 0.5f;

    // Calculate radius 
    float r2 = 0.0f;
    for (uint32_t i = 0; i < _vertexCount; i++) {
        const float* v = _vertices + i * _layout.strideFloats + _layout.posOffsetFloats;
        // Define the point 
        glm::vec3 p(v[0], v[1], v[2]);
        // Calculate distance from point to precalculated center
        glm::vec3 d = p - center;
        // Check if this is bigger than any existing radius calculated.
        r2 = std::max(r2, glm::dot(d, d));
    }

    Bounds b;
    b.center = center;
    b.radius = std::sqrt(r2);
    return b;
}

// Assembles the mesh & it's bounds and returns its ID. 
MeshID CreateMeshFromData_P3N3Uv2(
    const float* _vertices,
    uint32_t _vertexCount,
    const uint32_t* _indices,
    uint32_t _indexCount)
{
    constexpr uint32_t strideBytes = VertexLayout_P3N3UV2.strideFloats * sizeof(float);

    Mesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(_vertexCount * strideBytes), _vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(_indexCount * sizeof(uint32_t)), _indices, GL_STATIC_DRAW);

    // aPos @ location 0 (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)strideBytes, (void*)0);

    // aNormal @ location 1 (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (GLsizei)strideBytes, (void*)(3 * sizeof(float)));

    // aUV @ location 2 (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (GLsizei)strideBytes, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    mesh.indexCount = _indexCount;
    mesh.localBounds = ComputeBounds_Sphere(_vertices, _vertexCount, VertexLayout_P3N3UV2);

    MeshID id = (MeshID)REntityMeshs.size();
    REntityMeshs.push_back(mesh);
    return id;
}