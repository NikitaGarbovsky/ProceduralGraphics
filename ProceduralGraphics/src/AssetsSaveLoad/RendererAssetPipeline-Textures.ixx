
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
import DebugUtilities;

// Global, persistent texture cache
static std::unordered_map<std::string, GLuint> GTextureCache;

// Generated upon loading a assimp file this this importer. Holds common info that is easy to 
// reference for the importing process.
struct ImportContext
{
    const aiScene* scene = nullptr;
    std::string modelDirectory;
    std::string modelPath; // Full path of the file being imported.
};

// One single place to flip images if necessary.
static void EnsureStbConfigred() {
    static bool configured = false;
    if (!configured) {
        stbi_set_flip_vertically_on_load(true);
        configured = true;
    }
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

// Creates the raw GL object for a 1x1 solid color texture. 
static GLuint CreateSolidTextureRGBA8(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha) {
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

// Cached access to solid color textures. Every material with the same base color across ALL
// loaded models shares one GL texture.
static GLuint GetSolidTextureRGBA8(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
{
    char key[32];
    std::snprintf(key, sizeof(key), "solid:%u,%u,%u,%u", _red, _green, _blue, _alpha);

    if (auto it = GTextureCache.find(key); it != GTextureCache.end())
        return it->second;

    GLuint tex = CreateSolidTextureRGBA8(_red, _green, _blue, _alpha);
    GTextureCache.emplace(key, tex);
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
static bool TryGetFirstTexturePath(const aiMaterial* _mat, aiString& _outPath)
{
    // Common glTF mappings in Assimp
    if (_mat->GetTexture(aiTextureType_BASE_COLOR, 0, &_outPath) == AI_SUCCESS) return true;
    if (_mat->GetTexture(aiTextureType_DIFFUSE, 0, &_outPath) == AI_SUCCESS) return true;
    if (_mat->GetTexture(aiTextureType_UNKNOWN, 0, &_outPath) == AI_SUCCESS) return true;
    return false;
}

// Find any textures or colors, then return it.
export GLuint ResolveAlbedoTexture(ImportContext& _ctx, const aiMaterial* _mat)
{
    EnsureStbConfigred();

    // 1) Try to load an albedo/basecolor texture
    aiString path;
    if (TryGetFirstTexturePath(_mat, path))
    {
        const char* p = path.C_Str();
        if (p && p[0])
        {
            // Build unique cache key.
            const bool embedded = (p[0] == '*');
            const std::string key = embedded ? (_ctx.modelPath + "|" + p) 
                                             : (_ctx.modelDirectory + p);

            // Cached?
            if (auto it = GTextureCache.find(key); it != GTextureCache.end()) {
                // Console/Debug output
                std::string path = "Found a cached texture: ";
                path += key;
                path += " when loading: ";
                path += _ctx.modelPath;
                Log(path.c_str());
                return it->second;
            }

            GLuint tex = 0;

            // Embedded: "*0"
            if (embedded)
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
                GTextureCache.emplace(key, tex);
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
    return GetSolidTextureRGBA8(255, 255, 255, 255);
}