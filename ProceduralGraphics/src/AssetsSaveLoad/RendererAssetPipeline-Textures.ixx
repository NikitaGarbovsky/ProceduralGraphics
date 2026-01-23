
// Regular includes
#include <string>
#include <unordered_map>
#include "stb_image.h"
#include <assimp/scene.h>
#include <glew.h>

/// <summary>
/// The :Textures partition of the RendererAssetPipeline module handles loading and managing 
/// texture data from imported assimp files.
/// </summary>
export module RendererAssetPipeline:Textures;

// Imports
import <cstdint>;

// Generated upon loading a assimp file this this importer. Holds common info that is easy to 
// reference for the importing process.
struct ImportContext
{
    const aiScene* scene = nullptr;
    std::string modelDirectory;

    // Cache textures so we don’t create duplicates for the same "*0" or same file
    std::unordered_map<std::string, GLuint> textureCache;
    GLuint whiteTex = 0;
};

// Used as a fallback if no texture is able to be loaded from the file.
static GLuint CreateWhiteTexture2D()
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const uint32_t white = 0xFFFFFFFFu;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// Generates the opengl texture stuff.
static GLuint CreateGLTextureRGBA8(int _width, int _height, const unsigned char* _rgba)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, _rgba);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// Loads the generated texture data from an embedded assimp file, (glb, .fbx ...)
static GLuint LoadEmbeddedAssimpTexture(const aiTexture* _tex)
{
    if (!_tex) return 0;

    int w = 0, h = 0, comp = 0;

    // Compressed blob (common for GLB): mWidth = bytes, mHeight = 0
    if (_tex->mHeight == 0)
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(_tex->pcData);
        const int byteCount = (int)_tex->mWidth;

        unsigned char* rgba = stbi_load_from_memory(bytes, byteCount, &w, &h, &comp, 4);
        if (!rgba) return 0;

        GLuint gltex = CreateGLTextureRGBA8(w, h, rgba);
        stbi_image_free(rgba);
        return gltex;
    }

    // Raw pixels: pcData is aiTexel[w*h]
    w = (int)_tex->mWidth;
    h = (int)_tex->mHeight;

    const unsigned char* rgba = reinterpret_cast<const unsigned char*>(_tex->pcData);
    return CreateGLTextureRGBA8(w, h, rgba);
}

// Helper for checking is any textures exist in certain areas of the assimp aiScene material.
static bool TryGetFirstTexturePath(const aiMaterial* mat, aiString& outPath)
{
    // Common glTF mappings in Assimp
    if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &outPath) == AI_SUCCESS) return true;
    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &outPath) == AI_SUCCESS) return true;
    if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &outPath) == AI_SUCCESS) return true;
    return false;
}

// Generates the opengl texture stuff for color data that is on the material itself (no base texture)
static GLuint CreateSolidTextureRGBA8(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    unsigned char px[4] = { _red, _green, _blue, _alpha };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// Find any textures or colors, then return it.
export GLuint ResolveAlbedoTexture(ImportContext& _ctx, const aiMaterial* _mat)
{
    stbi_set_flip_vertically_on_load(true);
    // 1) Try to load an albedo/basecolor texture
    aiString path;
    if (TryGetFirstTexturePath(_mat, path))
    {
        const char* p = path.C_Str();
        if (p && p[0])
        {
            const std::string key = p;

            // Cached?
            if (auto it = _ctx.textureCache.find(key); it != _ctx.textureCache.end())
                return it->second;

            GLuint tex = 0;

            // Embedded: "*0"
            if (p[0] == '*')
            {
                int idx = std::atoi(p + 1);
                if (idx >= 0 && idx < (int)_ctx.scene->mNumTextures)
                    tex = LoadEmbeddedAssimpTexture(_ctx.scene->mTextures[idx]);
            }
            else
            {
                // External file
                std::string full = _ctx.modelDirectory + p;

                int w = 0, h = 0, comp = 0;
                unsigned char* rgba = stbi_load(full.c_str(), &w, &h, &comp, 4);
                if (rgba)
                {
                    tex = CreateGLTextureRGBA8(w, h, rgba);
                    stbi_image_free(rgba);
                }
                else
                {
                    // 2) If external load failed, try embedded-by-name 
                    if (const aiTexture* emb = _ctx.scene->GetEmbeddedTexture(p))
                    {
                        tex = LoadEmbeddedAssimpTexture(emb);
                    }
                }
            }

            if (tex != 0)
            {
                _ctx.textureCache.emplace(key, tex);
                return tex;
            }
        }
    }

    // 2) No texture — try a constant base color / diffuse color
    aiColor4D col;

#ifdef AI_MATKEY_BASE_COLOR
    if (aiGetMaterialColor(_mat, AI_MATKEY_BASE_COLOR, &col) == AI_SUCCESS)
    {
        return CreateSolidTextureRGBA8(
            (unsigned char)(col.r * 255.0f),
            (unsigned char)(col.g * 255.0f),
            (unsigned char)(col.b * 255.0f),
            (unsigned char)(col.a * 255.0f));
    }
#endif

    if (aiGetMaterialColor(_mat, AI_MATKEY_COLOR_DIFFUSE, &col) == AI_SUCCESS)
    {
        return CreateSolidTextureRGBA8(
            (unsigned char)(col.r * 255.0f),
            (unsigned char)(col.g * 255.0f),
            (unsigned char)(col.b * 255.0f),
            (unsigned char)(col.a * 255.0f));
    }

    // 3) Fallback, no texture or base color was found, use white as placeholder.

    if (_ctx.whiteTex == 0)
        _ctx.whiteTex = CreateWhiteTexture2D();

    return _ctx.whiteTex;
}