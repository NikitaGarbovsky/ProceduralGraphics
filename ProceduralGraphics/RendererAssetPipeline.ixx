#include <algorithm>

/// <summary>
/// The RendererAssestPipeline module defines a step by step process of loading REntity's asset data.
/// Generated REntity's Materials & Mesh are automatically pushed to their array tables for referencing.
/// Step 1: Generate Material (returns material id)
/// Step 2: Generate Mesh (return mesh id)
/// Step 3: Generated REntity, input Mesh & Material Id's. (CreateRenderEntity() in RendererEntitys.ixx)
/// </summary>
export module RendererAssetPipeline;

import RendererEntitys;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Function prototypes
static Bounds ComputeBounds_Pos3Uv2(const float* vertices, uint32_t vertexCount);

// No texture, asset generator
export MeshID CreateMeshFromData_Pos3Uv2(const float* _vertices,
    uint32_t _vertexCount,
    const uint32_t* _indices,
    uint32_t _indexCount)
{
    // Each vertex: 5 floats (pos3 + uv2)
    constexpr uint32_t floatsPerVertex = 5;
    constexpr uint32_t strideBytes = floatsPerVertex * sizeof(float);

    // Create a new mesh and assign all its values to all the OpenGL stuff.
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

    // aUV @ location 1 (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)strideBytes, (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    mesh.indexCount = _indexCount;
    mesh.localBounds = ComputeBounds_Pos3Uv2(_vertices, _vertexCount);

    // Store mesh in the global mesh table and return its ID (index).
    MeshID id = (MeshID)REntityMeshs.size();
    REntityMeshs.push_back(mesh);
    return id;
}

// TESTING
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

// Generates the material data for the given shader program & assigns the texture.
export MaterialID CreateMaterial(GLuint _program, GLuint _tex0 = 0)
{
    Material newMaterial{};
    newMaterial.program = _program;
    newMaterial.tex0 = _tex0;

    // Cache uniform locations once (so ExecuteCommands doesn't query every frame).
    newMaterial.uView = glGetUniformLocation(_program, "ViewMat");
    newMaterial.uProj = glGetUniformLocation(_program, "ProjectionMat");

    MaterialID id = (MaterialID)REntityMaterials.size();
    REntityMaterials.push_back(newMaterial);
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
#endif
}

// FOR FRUSTRUM CULLING, calculates a simple sphere bounds from the vertices.
static Bounds ComputeBounds_Pos3Uv2(const float* _vertices, uint32_t _vertexCount)
{
    // vertices layout: [px,py,pz, u,v] * vertexCount
    constexpr uint32_t floatsPerVertex = 5;

    glm::vec3 minP(FLT_MAX);
    glm::vec3 maxP(-FLT_MAX);

    // Loop through all the vertices and calculate the fartherest away from one another
    for (uint32_t i = 0; i < _vertexCount; i++) {
        const float* v = _vertices + i * floatsPerVertex;
        glm::vec3 p(v[0], v[1], v[2]);
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }

    // Define the center point of the bounds (center point of mesh)
    glm::vec3 center = (minP + maxP) * 0.5f;


    // Calculate radius 
    float r2 = 0.0f;
    for (uint32_t i = 0; i < _vertexCount; i++) {
        const float* v = _vertices + i * floatsPerVertex;
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
