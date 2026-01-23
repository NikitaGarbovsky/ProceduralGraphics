#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm.hpp>
#include <glew.h>

/// <summary>
/// This module partition handles the assimp side of importing its proprietary and useful 
/// aiScene type to efficiently extract data across many different file types. (.obj, .fbx, .glb, etc..)
/// </summary>
export module RendererAssetPipeline:AssimpImport;

import DebugUtilities;
import :Textures;
import :Mesh;

// Holds the standardized mesh information for each 
// mesh/submesh loaded from a file loaded by assimp.
struct ImportedSubmesh_P3N3Uv2 {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    GLuint tex0 = 0;
};

// Finds the directory from the path
static std::string GetDirectoryFromPath(const char* _path)
{
    std::string s = _path ? _path : "";
    size_t slash = s.find_last_of("/\\");
    if (slash == std::string::npos) return std::string{};
    return s.substr(0, slash + 1); // include trailing slash
}

// Helper to convert assimp row major (assimp) format to column major (glm) format
static glm::mat4 assimpAiSceneToGlm(const aiMatrix4x4& _matrix)
{
    glm::mat4 r;
    r[0][0] = _matrix.a1; r[1][0] = _matrix.a2; r[2][0] = _matrix.a3; r[3][0] = _matrix.a4;
    r[0][1] = _matrix.b1; r[1][1] = _matrix.b2; r[2][1] = _matrix.b3; r[3][1] = _matrix.b4;
    r[0][2] = _matrix.c1; r[1][2] = _matrix.c2; r[2][2] = _matrix.c3; r[3][2] = _matrix.c4;
    r[0][3] = _matrix.d1; r[1][3] = _matrix.d2; r[2][3] = _matrix.d3; r[3][3] = _matrix.d4;
    return r;
}

// Traverses an assimp scene and extracts the data from it.
static void ExtractMeshInstance_P3N3Uv2(
    ImportContext& _ctx,
    const aiMesh* _mesh,
    const glm::mat4& _world,
    ImportedSubmesh_P3N3Uv2& _out)
{
    _out.vertices.clear();
    _out.indices.clear();

    const bool hasNormals = _mesh->HasNormals();
    const bool hasUv0 = _mesh->HasTextureCoords(0);

    _out.vertices.reserve((size_t)_mesh->mNumVertices * VertexLayout_P3N3UV2.strideFloats);

    glm::mat3 normalM = glm::transpose(glm::inverse(glm::mat3(_world)));

    for (unsigned v = 0; v < _mesh->mNumVertices; ++v)
    {
        aiVector3D pA = _mesh->mVertices[v];
        aiVector3D nA = hasNormals ? _mesh->mNormals[v] : aiVector3D(0, 1, 0);
        aiVector3D uvA = hasUv0 ? _mesh->mTextureCoords[0][v] : aiVector3D(0, 0, 0);

        glm::vec3 p = _world * glm::vec4(pA.x, pA.y, pA.z, 1.0f);

        //p *= unitScale; // #TODO apply different scales depending on file format
        
        glm::vec3 n = glm::normalize(normalM * glm::vec3(nA.x, nA.y, nA.z));

        _out.vertices.insert(_out.vertices.end(), { p.x, p.y, p.z, n.x, n.y, n.z, uvA.x, uvA.y });
    }

    for (unsigned f = 0; f < _mesh->mNumFaces; ++f)
    {
        const aiFace& face = _mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        _out.indices.push_back((uint32_t)face.mIndices[0]);
        _out.indices.push_back((uint32_t)face.mIndices[1]);
        _out.indices.push_back((uint32_t)face.mIndices[2]);
    }

    const aiMaterial* mat = _ctx.scene->mMaterials[_mesh->mMaterialIndex];
    
    // Debug
    /*Log(("MatIndex=" + std::to_string(_mesh->mMaterialIndex) +
        " BC=" + std::to_string(mat->GetTextureCount(aiTextureType_BASE_COLOR)) +
        " Diff=" + std::to_string(mat->GetTextureCount(aiTextureType_DIFFUSE)) +
        " Emis=" + std::to_string(mat->GetTextureCount(aiTextureType_EMISSIVE)) +
        " Unk=" + std::to_string(mat->GetTextureCount(aiTextureType_UNKNOWN))).c_str());*/

    _out.tex0 = ResolveAlbedoTexture(_ctx, mat);
}

// Traverses through all of the meshes in a given import and extracts the data out of them to load.
static void TraverseAndExtract(
    ImportContext& _ctx,
    const aiNode* _node,
    const glm::mat4& _parentWorld,
    std::vector<ImportedSubmesh_P3N3Uv2>& _outSubmeshes)
{
    glm::mat4 local = assimpAiSceneToGlm(_node->mTransformation);
    glm::mat4 world = _parentWorld * local;

    for (unsigned i = 0; i < _node->mNumMeshes; ++i)
    {
        unsigned meshIndex = _node->mMeshes[i];
        const aiMesh* mesh = _ctx.scene->mMeshes[meshIndex];

        ImportedSubmesh_P3N3Uv2 part;
        ExtractMeshInstance_P3N3Uv2(_ctx, mesh, world, part);

        // Debug
        /*Log(("MeshIndex=" + std::to_string(meshIndex) +
            " UV0=" + std::to_string(mesh->HasTextureCoords(0)) +
            " UV1=" + std::to_string(mesh->HasTextureCoords(1)) +
            " Col0=" + std::to_string(mesh->HasVertexColors(0)) +
            " NumColorCh=" + std::to_string(mesh->GetNumColorChannels())).c_str());*/

        _outSubmeshes.push_back(std::move(part)); // Add the extracted data.
    }

    // Child meshes were found, complete the same process.
    for (unsigned c = 0; c < _node->mNumChildren; ++c)
        TraverseAndExtract(_ctx, _node->mChildren[c], world, _outSubmeshes);
}

// Imports the file at the designated path, including any and all submeshes.
bool ImportGLB_AsSubmeshes_P3N3Uv2(const char* _path, std::vector<ImportedSubmesh_P3N3Uv2>& _outSubmeshes)
{
    std::string out = "Loading file: "; out += _path;
    Log(out.c_str());

    _outSubmeshes.clear();

    Assimp::Importer importer;
    unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_ValidateDataStructure |
        aiProcess_CalcTangentSpace;

    const aiScene* scene = importer.ReadFile(_path, flags);

    // Debug output
    /*Log(("Meshs in file: " + std::to_string(scene->mNumMeshes)).c_str());
    Log(("Textures in file: " + std::to_string(scene->mNumTextures)).c_str());
    
    for (unsigned i = 0; i < scene->mNumTextures; ++i)
    {
        const aiTexture* t = scene->mTextures[i];
        Log(("  Tex[" + std::to_string(i) + "] w=" + std::to_string(t->mWidth) +
            " h=" + std::to_string(t->mHeight) +
            " name=" + std::string(t->mFilename.C_Str())).c_str());
    }*/

    if (!scene || !scene->mRootNode)
        return false;

    ImportContext ctx;
    ctx.scene = scene;
    ctx.modelDirectory = GetDirectoryFromPath(_path); // #TODO not currently used, perhapes sometime in the future I'll need the path for some reason.
    ctx.whiteTex = 0; // Assigned later

    TraverseAndExtract(ctx, scene->mRootNode, glm::mat4(1.0f), _outSubmeshes);
    return true;
}
