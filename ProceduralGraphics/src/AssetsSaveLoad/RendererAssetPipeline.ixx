#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/version.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <unordered_map>

/// <summary>
/// The RendererAssestPipeline module defines a interface to generate a REntity from a loaded assimp file.
/// Generated REntity's Materials & Mesh are automatically pushed to their array tables for referencing through their ID's.
/// </summary>
export module RendererAssetPipeline;

import RendererEntitys; // Used for generating renderer entitys from loaded files
import DebugUtilities; // Logging
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Module partitions
import :Mesh;
import :Textures;
import :AssimpImport;

// Publically exported functions
export MeshID CreateMeshFromData_P3N3Uv2(const float* _vertices, uint32_t _vertexCount, const uint32_t* _indices, uint32_t _indexCount);
export bool LoadModel_AsREntities_P3N3Uv2(const char* _path, GLuint _program, glm::vec3 _spawnPos = glm::vec3(0));

// Generates the material data for the given shader program & texture, cache uniforms. #TODO, probably move this somewhere else
export MaterialID CreateMaterial(GLuint _program, GLuint _tex0 = 0)
{
    Material newMaterial{};
    newMaterial.program = _program;
    newMaterial.tex0 = _tex0;

    // Cache uniform locations once (so ExecuteCommands doesn't query every frame).
    newMaterial.uView = glGetUniformLocation(_program, "ViewMat");
    newMaterial.uProj = glGetUniformLocation(_program, "ProjectionMat");
    newMaterial.uTex0 = glGetUniformLocation(_program, "Tex0");

    MaterialID id = (MaterialID)REntityMaterials.size();
    REntityMaterials.push_back(newMaterial);
    return id;
}

// Validation step to confirm a REntity is generated correctly. #TODO probably remove it.
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

// Main way to parse 3D files (through assimp) and load them into the renderer as REntities
bool LoadModel_AsREntities_P3N3Uv2(const char* _path, GLuint _program, glm::vec3 _spawnpos)
{
    // Create a container to hold all meshes in the model/3Dscene we're going to load 
    std::vector<ImportedSubmesh_P3N3Uv2> modelParts;

    // Fill that model container with all the data from the file we're loading.
    ImportGLB_AsSubmeshes_P3N3Uv2(_path, modelParts, _spawnpos);// #TODO forgot why we pass spawnpos here

    // Load all meshes and their associated materials, then create a RenderEntity.  
    for (auto& part : modelParts)
    {
    		uint32_t vertexCount = (uint32_t)(part.vertices.size() / VertexLayout_P3N3UV2.strideFloats);

            // TODO, might want to return this ID at some stage for immediate access.
    		MeshID meshId = CreateMeshFromData_P3N3Uv2(
    			part.vertices.data(), vertexCount,
    			part.indices.data(), (uint32_t)part.indices.size());

            // TODO, might want to return this ID at some stage for immediate access.
    		MaterialID matId = CreateMaterial(_program, part.tex0);

    		CreateRenderEntity(meshId, matId, _spawnpos);
    	}

    return true;
}
