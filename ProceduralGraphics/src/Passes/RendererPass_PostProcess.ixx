module;

#include <glew.h>

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
constexpr uint32_t kMaxPostParams = 6;
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
    // #TODO: add more here

    PostParam params[kMaxPostParams];
    uint32_t paramCount = 0;
};

// ==========================================================================================
// Module state
// ==========================================================================================
static PostEffect GEffects[kMaxPostEffects];
static uint32_t GEffectCount = 0;
static uint32_t GActiveEffect = 0; // 0 is always the no effect entry.

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

        // The scene texture always sits at 0, so this only needs setting once.
        if (effect.uSceneTex != -1)
            glProgramUniform1i(effect.program, effect.uSceneTex, 0);
    }

    return GEffectCount++;
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

    // #TODO: rain and the pixel effect
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

    const PostEffect& effect = GEffects[GActiveEffect];

    // Back to the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _fcommon.viewportW, _fcommon.viewportH);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glUseProgram(effect.program);

    if (effect.uResolution != -1)
        glUniform2f(effect.uResolution, (float)_fcommon.viewportW, (float)_fcommon.viewportH);

    if (effect.uTime != -1)
        glUniform1f(effect.uTime, (float)gTimeSinceAppStart);

    // Push whatever the editor sliders are currently sitting on.
    for (uint32_t i = 0; i < effect.paramCount; ++i) {
        if (effect.params[i].location != -1)
            glUniform1f(effect.params[i].location, effect.params[i].value);
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
