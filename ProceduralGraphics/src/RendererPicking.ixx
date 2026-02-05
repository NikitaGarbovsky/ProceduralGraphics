module;

#include <cstdint>
#include <algorithm>
#include <glew.h>
#include <glfw3.h>

export module RendererPicking;
export enum class PickKind : uint8_t { None = 0, Entity = 1, Light = 2 };

export struct PickHit {
    PickKind kind = PickKind::None;
    uint32_t index = 0xFFFFFFFFu; // entity index or light index
    uint32_t raw = 0;             // raw id read from buffer
};

// -------------------- Pick ID encoding --------------------
// Lights use the high bit + (index+1).
export constexpr uint32_t PICK_LIGHT_BIT = 0x80000000u;

static inline bool     PickIsNone(uint32_t _raw) { return _raw == 0u; }
static inline bool     PickIsLight(uint32_t _raw) { return (_raw & PICK_LIGHT_BIT) != 0u; }
static inline uint32_t PickDecodeEntityIndex(uint32_t _raw) { return _raw - 1u; }
static inline uint32_t PickDecodeLightIndex(uint32_t _raw) { return (_raw & ~PICK_LIGHT_BIT) - 1u; }

export inline PickHit Pick_Decode(uint32_t _raw) {
    PickHit h{};
    h.raw = _raw;

    if (PickIsNone(_raw)) { h.kind = PickKind::None; h.index = 0xFFFFFFFFu; return h; }

    if (PickIsLight(_raw)) { h.kind = PickKind::Light; h.index = PickDecodeLightIndex(_raw); return h; }

    h.kind = PickKind::Entity;
    h.index = PickDecodeEntityIndex(_raw);
    return h;
}

// -------------------- Request + result state --------------------
static bool pickRequested = false;
static bool hasResult = false;
static int  pickX = 0;
static int  pickY = 0;
static uint32_t rawResult = 0;

// -------------------- Picking render target ownership --------------------
export GLuint PickingFBO = 0;
export GLuint PickingIdTex = 0;     // GL_R32UI
export GLuint PickingDepthTex = 0;  // DEPTH24
export int PickingW = 0, PickingH = 0; // Coordinates set for player REntity picking on mouse click;

static void DestroyPickingTargetInternal() {
    if (PickingDepthTex) { glDeleteTextures(1, &PickingDepthTex); PickingDepthTex = 0; }
    if (PickingIdTex) { glDeleteTextures(1, &PickingIdTex);    PickingIdTex = 0; }
    if (PickingFBO) { glDeleteFramebuffers(1, &PickingFBO);  PickingFBO = 0; }
    PickingW = 0;
    PickingH = 0;
}

export void EnsurePickingTarget(int _w, int _h) {
    if (_w <= 0 || _h <= 0) return;
    // No change, viewport is the same size 
    if (PickingFBO != 0 && PickingW == _w && PickingH == _h) return;

    DestroyPickingTargetInternal(); // Clear prior picking data internals 

    // ======= Validation complete, commence PickingFBO resolve =======
    PickingW = _w;
    PickingH = _h;

    glGenFramebuffers(1, &PickingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, PickingFBO);

    // Color texture (what the editor viewport will display)
    glGenTextures(1, &PickingIdTex);
    glBindTexture(GL_TEXTURE_2D, PickingIdTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, _w, _h, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PickingIdTex, 0);

    // Depth texture
    glGenTextures(1, &PickingDepthTex);
    glBindTexture(GL_TEXTURE_2D, PickingDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _w, _h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, PickingDepthTex, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        DestroyPickingTargetInternal();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

export void ShutdownPickingTarget() {
    DestroyPickingTargetInternal();
}

static void SetCorrectPixelCoordinatesOnClick(GLFWwindow* _window, int &_outX, int &_outY) {
    double mx, my;
    glfwGetCursorPos(_window, &mx, &my);

    int winW, winH, fbW, fbH;
    glfwGetWindowSize(_window, &winW, &winH);
    glfwGetFramebufferSize(_window, &fbW, &fbH);

    double sx = (winW > 0) ? (double)fbW / (double)winW : 1.0;
    double sy = (winH > 0) ? (double)fbH / (double)winH : 1.0;

    int px = (int)(mx * sx);
    int py = (int)(my * sy);

    // OpenGL origin bottom-left:
    py = fbH - 1 - py;

    px = std::max(0, std::min(px, fbW - 1));
    py = std::max(0, std::min(py, fbH - 1));

    _outX = px;
    _outY = py;
}

export void PickingRequest(GLFWwindow* _window) {
    pickRequested = true;
    SetCorrectPixelCoordinatesOnClick(_window, pickX, pickY);
}

export bool PickingIsRequested() {
    return pickRequested;
}

// Called AFTER all pick-geometry is rendered into PickingFBO
export void PickingReadback() {
    if (!pickRequested) return;

    uint32_t id = 0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, PickingFBO);
    glReadPixels(pickX, pickY, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    rawResult = id;
    hasResult = true;
    pickRequested = false;
}

export bool PickingHasResult() {
    return hasResult;
}

export PickHit PickingConsumeHit() {
    hasResult = false;
    return Pick_Decode(rawResult);
}
