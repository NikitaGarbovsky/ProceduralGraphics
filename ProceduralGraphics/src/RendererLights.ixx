module;

#include <cstdint>
#include <vector>
#include <algorithm>

#include <glew.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

/// <summary>
/// Manages creation, manipulation & mutation of lights.
/// Supported lights: Point, Directional, Spot
/// </summary>
export module RendererLights;

import RendererTransformUtils; // For ComposeTRSMatrix
import DebugUtilities;

export using LightID = uint32_t;

// The light types supported.
export enum class LightType : uint32_t {
    Point = 0,
    Directional = 1,
    Spot = 2
};

// SoA for transforms 
export struct LightTransformData {
    std::vector<glm::vec3> position;
    std::vector<glm::vec3> rotation; // Euler degrees #TODO, implement quarternions.
    std::vector<glm::vec3> scale;
};

export LightTransformData LightTransforms;

// Lighting arrays #TODO: maybe put these inside a struct? SoA?
static std::vector<LightType> LightsTypes;
static std::vector<glm::vec3> LightsColors;
static std::vector<float>     LightsIntensity;
static std::vector<float>     LightsRange;        // point/spot
static std::vector<float>     LightsInnerDeg;     // for spotlight
static std::vector<float>     LightsOuterDeg;     // for spotlight

// ---- GPU UBO ----
static GLuint lightsUBO = 0;
static constexpr uint32_t MAX_LIGHTS = 64;
static constexpr GLuint LIGHTS_BINDING_POINT = 3;

struct alignas(16) GPULight {
    glm::vec4 pos_type;       // xyz = position, w = type
    glm::vec4 dir_range;      // xyz = direction (world), w = range
    glm::vec4 color_intensity;// rgb = color, w = intensity
    glm::vec4 spot_cos;       // x = innerCos, y = outerCos, z,w unused
};

struct alignas(16) GPULightBlock {
    glm::ivec4 count_pad;         // x = count, yzw padding
    GPULight lights[MAX_LIGHTS];
};

// ========================== HELPERS for light data ==========================
export void SetLightColor(LightID _id, const glm::vec3& _color) { LightsColors[_id] = _color; }
export void SetLightIntensity(LightID _id, float _value) { LightsIntensity[_id] = _value; }
export void SetLightRange(LightID _id, float _value) { LightsRange[_id] = _value; }
export uint32_t GetLightCount() { return (uint32_t)LightsTypes.size(); }
export LightType GetLightType(LightID _id) { return LightsTypes[_id]; }
export glm::vec3 GetLightColor(LightID _id) { return LightsColors[_id]; }
export float GetLightIntensity(LightID _id) { return LightsIntensity[_id]; }
export float GetLightRange(LightID _id) { return LightsRange[_id]; }
export float GetSpotInnerDeg(LightID _id) { return LightsInnerDeg[_id]; }
export float GetSpotOuterDeg(LightID _id) { return LightsOuterDeg[_id]; }

export void SetSpotInnerOuter(LightID _id, float _innerDeg, float _outerDeg) {
    // enforce sane ordering
    _innerDeg = std::max(0.0f, _innerDeg);
    _outerDeg = std::max(_innerDeg, _outerDeg);
    LightsInnerDeg[_id] = _innerDeg;
    LightsOuterDeg[_id] = _outerDeg;
}

static glm::vec3 EulerDegToForward_ZYX(const glm::vec3& _rDeg) {
    glm::mat4 R(1.0f);
    R = glm::rotate(R, glm::radians(_rDeg.z), glm::vec3(0, 0, 1));
    R = glm::rotate(R, glm::radians(_rDeg.y), glm::vec3(0, 1, 0));
    R = glm::rotate(R, glm::radians(_rDeg.x), glm::vec3(1, 0, 0));

    // Camera uses forward -Z, so keep the same here
    glm::vec3 f = glm::normalize(glm::vec3(R * glm::vec4(0, 0, -1, 0)));
    return f;
}

// =========================^ HELPERS for mutating light data outside of this module ^=========================

export void InitLights() {
    if (lightsUBO) return;

    glGenBuffers(1, &lightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GPULightBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, LIGHTS_BINDING_POINT, lightsUBO);
}

export void ShutdownLights() {
    if (lightsUBO) glDeleteBuffers(1, &lightsUBO);
    lightsUBO = 0;

    LightsTypes.clear(); LightsColors.clear(); LightsIntensity.clear(); LightsRange.clear(); LightsInnerDeg.clear(); LightsOuterDeg.clear();
    LightTransforms.position.clear();
    LightTransforms.rotation.clear();
    LightTransforms.scale.clear();
}

export glm::mat4 GetLightModelMatrix(LightID _id) {
    return ComposeTRSMatrix(LightTransforms.position[_id], LightTransforms.rotation[_id], LightTransforms.scale[_id]);
}

// ================================ Light Creation Functions ================================ // 

// Helper for all 3 lights, regardless of light types, all arrays are pushed data to keep id referencing easy and consistent.
static LightID PushCommonLightArrayData(LightType _lt, const glm::vec3& _pos, const glm::vec3& _rotDeg) {
    LightID id = (LightID)LightsTypes.size();
    LightsTypes.push_back(_lt);

    // defaults
    LightsColors.push_back(glm::vec3(1, 1, 1));
    LightsIntensity.push_back(5.0f);
    LightsRange.push_back(10.0f);
    LightsInnerDeg.push_back(20.0f);
    LightsOuterDeg.push_back(30.0f);

    LightTransforms.position.push_back(_pos);
    LightTransforms.rotation.push_back(_rotDeg);
    LightTransforms.scale.push_back(glm::vec3(1, 1, 1));

    return id;
}

export LightID CreatePointLight(const glm::vec3& _pos) {
    LightID id = PushCommonLightArrayData(LightType::Point, _pos, glm::vec3(0));
    LightsRange[id] = 12.0f;
    LightsIntensity[id] = 8.0f;
    return id;
}

export LightID CreateDirectionalLight(const glm::vec3& _pos, const glm::vec3& _rotDeg) {
    LightID id = PushCommonLightArrayData(LightType::Directional, _pos, _rotDeg);
    LightsIntensity[id] = 2.0f;
    LightsRange[id] = 0.0f;
    return id;
}

export LightID CreateSpotLight(const glm::vec3& _pos, const glm::vec3& _rotDeg) {
    LightID id = PushCommonLightArrayData(LightType::Spot, _pos, _rotDeg);
    LightsRange[id] = 18.0f;
    LightsInnerDeg[id] = 18.0f;
    LightsOuterDeg[id] = 28.0f;
    LightsIntensity[id] = 10.0f;
    return id;
}

// Called per frame between opaque pass build & execute. #TODO, this will work differently 
// once shadows & deferred rendering is implemented.
export void UpdateLights() {
    if (!lightsUBO) return;

    GPULightBlock block{};
    uint32_t currentLightCount = std::min(GetLightCount(), MAX_LIGHTS);
    block.count_pad = glm::ivec4((int)currentLightCount, 0, 0, 0);

    for (uint32_t i = 0; i < currentLightCount; ++i)
    {
        GPULight light{};
        glm::vec3 pos = LightTransforms.position[i];
        glm::vec3 dir = EulerDegToForward_ZYX(LightTransforms.rotation[i]);

        float innerCos = std::cos(glm::radians(LightsInnerDeg[i]));
        float outerCos = std::cos(glm::radians(LightsOuterDeg[i]));

        light.pos_type = glm::vec4(pos, (float)LightsTypes[i]);
        light.dir_range = glm::vec4(dir, LightsRange[i]);
        light.color_intensity = glm::vec4(LightsColors[i], LightsIntensity[i]);
        light.spot_cos = glm::vec4(innerCos, outerCos, 0, 0);

        block.lights[i] = light;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    // Copies bytes from CPU RAM into GPU memory (into UBO’s storage) 
    // (upload the light block once, so all fragment's have access to this buffer of data)
    // #TODO, optimization: dont upload every frame like this, instead upload only when lights change.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPULightBlock), &block);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
