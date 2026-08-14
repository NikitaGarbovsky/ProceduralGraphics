module;

#include <glew.h>
#include <vector>

/// <summary>
/// The post processing pass. Everything before this point already rendered into the ViewportFBO,
/// so all this pass does is draw one quad over the whole window and run the scene texture
/// through whichever effect is currently picked.
/// 
/// Contains all the post processing effects.
/// </summary>
export module RendererPass_PostProcess;

import RendererData;      // For ViewportFBO, ViewportColorTex, gTimeSinceAppStart
import RendererFrame;
import RendererUtilities; // For LoadShaderProgram
import RendererFullscreenQuad;
import DebugUtilities;
import TerrainGen;        // For PP noise texture generation.
import <cstdint>;

// One tweakable float on an effect. The label shows up in the editor, the uniform name is what
// the shader calls it, and the location gets looked up once when the effect loads.
export struct PostParam {
    const char* label = "";
    const char* uniformName = "";
    GLint location = -1;
    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
};

// #TODO: limiting like this is annoying, add more dynamic elements to the editor UI.
constexpr uint32_t kMaxPostParams = 8;
constexpr uint32_t kMaxPostEffects = 12;

// One post processing effect. Every effect shares the same quad and vertex shader, so the
// fragment shader is the only part that changes.
export struct PostEffect {
    const char* name = "Unnamed Effect";
    const char* description = "";
    GLuint program = 0;

    // Uniforms any effect can use. Left at -1 when the shader doesn't declare them.
    GLint uSceneTex = -1;
    GLint uResolution = -1;
    GLint uTime = -1;
    
    // Optional second texture on unit 1. Left at 0 for effects that don't require it.
    GLuint effectTex = 0;
    GLint uEffectTex = -1;

    PostParam params[kMaxPostParams];
    uint32_t paramCount = 0;
};

// ==========================================================================================
// Module state
// ==========================================================================================
static PostEffect GEffects[kMaxPostEffects];
static uint32_t GEffectCount = 0;
static uint32_t GActiveEffect = 0; // 0 is always the no effect entry.

// Generated noise texture on startup, effects that require the noise (rain) use it.
static GLuint GNoiseTex = 0;

// ==========================================================================================
// Internal
// ==========================================================================================

// Loads an effect's shader and puts it in the table. Gives back the index so params can be
// hung off it straight after.
static uint32_t RegisterEffect(const char* _name, const char* _description, const char* _fragPath)
{
    if (GEffectCount >= kMaxPostEffects) {
        LogWarning("RegisterEffect: post effect table full, effect ignored.");
        return 0;
    }

    PostEffect& effect = GEffects[GEffectCount];
    effect = PostEffect{};
    effect.name = _name;
    effect.description = _description;
    effect.program = LoadShaderProgram(kFullscreenVertPath, _fragPath);

    if (effect.program != 0) {
        effect.uSceneTex = glGetUniformLocation(effect.program, "SceneTex");
        effect.uResolution = glGetUniformLocation(effect.program, "Resolution");
        effect.uTime = glGetUniformLocation(effect.program, "Time");
        effect.uEffectTex = glGetUniformLocation(effect.program, "EffectTex");

        // The scene texture always sits at 0, so this only needs setting once.
        if (effect.uSceneTex != -1)
            glProgramUniform1i(effect.program, effect.uSceneTex, 0);

        if (effect.uEffectTex != -1)
            glProgramUniform1i(effect.program, effect.uEffectTex, 1);
    }

    return GEffectCount++;
}

// Hands an effect a second texture.
static void SetEffectTexture(uint32_t _effect, GLuint _texture) {
    GEffects[_effect].effectTex = _texture;
}

// Creates a noise texture to be used for post processing effects. 
// This uses the perlin noise generator.
static GLuint CreateNoiseTexture() {
    NoiseParams params{};
    params.width = 256;
    params.height = 256;
    params.octaves = 3;
    params.wavelength = 6.0f; 
    params.gain = 0.5f;
    params.lacunarity = 2.0f;
    params.seed = 1337; // Rain stays the same every run.      

    // Create the first noise map,
    NoiseMap redMap{};
    if (!Noise_Generate(params, redMap)) {
        LogWarning("CreateNoiseTexture: noise generation failed, rain has no texture.");
        return 0;
    }

    // Create second,
    params.seed = 7331; // Different seed, so green is an unrelated pattern
    NoiseMap greenMap{};
    if (!Noise_Generate(params, greenMap)) {
        LogWarning("CreateNoiseTexture: noise generation failed, rain has no texture.");
        return 0;
    }

    // Weave the maps together into one two channel image
    std::vector<unsigned char> pixels(redMap.gray8.size() * 2);
    for (size_t i = 0; i < redMap.gray8.size(); ++i) {
        pixels[i * 2 + 0] = redMap.gray8[i];
        pixels[i * 2 + 1] = greenMap.gray8[i];
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, redMap.width, redMap.height, 0, GL_RG, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // Mirrored rather than plain repeat. This noise doesn't tile, so a straight repeat puts a
    // hard line through the picture every time it wraps. Mirroring makes the edges meet and 
    // no hard line :).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// Adds a slider backed float to an effect.
static void AddParam(uint32_t _effect, const char* _label, const char* _uniformName,
    float _value, float _min, float _max)
{
    PostEffect& effect = GEffects[_effect];

    if (effect.paramCount >= kMaxPostParams)
    {
        LogWarning("AddParam: this effect already has the max number of params.");
        return;
    }

    PostParam& param = effect.params[effect.paramCount++];
    param.label = _label;
    param.uniformName = _uniformName;
    param.value = _value;
    param.minValue = _min;
    param.maxValue = _max;
    param.location = (effect.program != 0) ? glGetUniformLocation(effect.program, _uniformName) : -1;
}

// Used when an effect's shader failed to load, so a broken shader shows the scene instead 
// of a black screen. Reads the resolved target, since the one the scene rendered 
// into is multisampled.
static void CopyViewportToScreen(int _width, int _height) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ViewportResolveFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glBlitFramebuffer(
        0, 0, _width, _height,
        0, 0, _width, _height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ==========================================================================================
// Public API
// ==========================================================================================

export void InitPostProcessPass()
{
    GEffectCount = 0;
    GActiveEffect = 0;

    GNoiseTex = CreateNoiseTexture();

    // ---------------- The effect list. Tab steps through these in order. ----------------

    // Index 0 is the no effect pass. Still goes through the quad so there is only ever one
    // code path to worry about.
    RegisterEffect("None",
        "The scene exactly as rendered, no effect applied.",
        "Assets/Shaders/PostProcessing/PostNone.frag");

    uint32_t effect = RegisterEffect("Invert",
        "Flips every color channel, 1 minus the scene color.",
        "Assets/Shaders/PostProcessing/PostInvert.frag");
    AddParam(effect, "Amount", "Amount", 1.0f, 0.0f, 1.0f);

    effect = RegisterEffect("Greyscale",
        "Drops the color using the luminosity weights.\nGreen counts most, then red, then blue.",
        "Assets/Shaders/PostProcessing/PostGreyscale.frag");
    AddParam(effect, "Amount", "Amount", 1.0f, 0.0f, 1.0f);

    effect = RegisterEffect("Rain",
        "Water running down the glass in front of the camera.\n The drops bend the picture behind them like little lenses.",
        "Assets/Shaders/PostProcessing/PostRain.frag");
    SetEffectTexture(effect, GNoiseTex); // Give the rain the generated noise texture.
    AddParam(effect, "Speed", "Speed", 0.125f, 0.0f, 1.0f);
    AddParam(effect, "Strength", "Strength", 0.04f, 0.0f, 0.2f);
    AddParam(effect, "Tiling", "Tiling", 2.0f, 1.0f, 8.0f);
    AddParam(effect, "Squash", "Squash", 0.1f, 0.02f, 1.0f);
    // #TODO: Pixel effect

    // Debug: Warn if the second sample texture an effect needs isn't set. #TODO: create a better system for different amounts of parameters.
    for (uint32_t i = 0; i < GEffectCount; ++i) {
        if (GEffects[i].uEffectTex != -1 && GEffects[i].effectTex == 0)
            LogWarning("InitPostProcessPass: an effect wants EffectTex but was never given one.");
    }
}

export void ShutdownPostProcessPass()
{
    for (uint32_t i = 0; i < GEffectCount; ++i)
    {
        if (GEffects[i].program != 0)
            glDeleteProgram(GEffects[i].program);

        GEffects[i] = PostEffect{};
    }

    GEffectCount = 0;
    GActiveEffect = 0;

    if (GNoiseTex) { glDeleteTextures(1, &GNoiseTex); GNoiseTex = 0; }
}

// Draws the finished scene onto the window through the active effect. This is the last thing
// the pipeline does each frame.
export void PostProcessPass_Execute(const FrameCommon& _fcommon) {
    // Nothing usable to draw with, fall back to a straight copy.
    if (GEffectCount == 0 || !FullscreenQuadReady() || ViewportColorTex == 0 ||
        GEffects[GActiveEffect].program == 0)
    {
        CopyViewportToScreen(_fcommon.viewportW, _fcommon.viewportH);
        return;
    }

    const PostEffect& ppEffect = GEffects[GActiveEffect];

    // Back to the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glUseProgram(ppEffect.program);

    // Unit 1 first, so unit 0 is the one left selected afterwards.
    if (ppEffect.effectTex != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ppEffect.effectTex);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ViewportColorTex);

    if (ppEffect.uResolution != -1)
        glUniform2f(ppEffect.uResolution, (float)_fcommon.viewportW, (float)_fcommon.viewportH);

    if (ppEffect.uTime != -1)
        glUniform1f(ppEffect.uTime, (float)gTimeSinceAppStart);

    // Push whatever the editor sliders are currently sitting on.
    for (uint32_t i = 0; i < ppEffect.paramCount; ++i) {
        if (ppEffect.params[i].location != -1)
            glUniform1f(ppEffect.params[i].location, ppEffect.params[i].value);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ViewportColorTex);

    DrawFullscreenQuad();

    glUseProgram(0);

    // Put the state back the way the rest of the renderer expects to find it.
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

// ==========================================================================================
// Queries for the editor UI
// ==========================================================================================
export uint32_t PostFX_Count() { return GEffectCount; }
export uint32_t PostFX_ActiveIndex() { return GActiveEffect; }

export const char* PostFX_Name(uint32_t _index) {
    return (_index < GEffectCount) ? GEffects[_index].name : "<invalid>";
}

export const char* PostFX_Description(uint32_t _index) {
    return (_index < GEffectCount) ? GEffects[_index].description : "";
}

// True when an effect's shader failed to load, so the editor can grey that entry out.
export bool PostFX_IsBroken(uint32_t _index) {
    return (_index < GEffectCount) && GEffects[_index].program == 0;
}

export void PostFX_SetActive(uint32_t _index) {
    if (_index >= GEffectCount)
    {
        LogWarning("PostFX_SetActive: invalid effect index.");
        return;
    }
    GActiveEffect = _index;
}

// Steps to the next effect and wraps back to None at the end.
export void PostFX_CycleNext() {
    if (GEffectCount == 0) return;
    GActiveEffect = (GActiveEffect + 1) % GEffectCount;
}

// The active effect's params, handed straight to the editor so its sliders write into them.
export PostParam* PostFX_ActiveParams() { return GEffects[GActiveEffect].params; }
export uint32_t PostFX_ActiveParamCount() { return GEffects[GActiveEffect].paramCount; }
