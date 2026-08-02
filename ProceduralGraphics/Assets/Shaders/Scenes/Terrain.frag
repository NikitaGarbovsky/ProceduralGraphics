in vec2 v2Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;
in float fLocalY;  

// Tex0 (grass) is bound by the render pass like any other material.
// Tex1 to Tex3 are bound to units 1 to 3 by the terrain scene every frame.
uniform sampler2D Tex0; // grass, low
uniform sampler2D Tex1; // dirt
uniform sampler2D Tex2; // rock
uniform sampler2D Tex3; // snow, high

uniform vec3 CameraPos;

// Model space height range of the terrain, set by the scene after each rebuild.
// Model space on purpose, so the bands don't shift when the terrain gets moved.
uniform float MinHeight;
uniform float MaxHeight;

out vec4 FragColor;

// The layer bands. Each fade finishes before the next one starts, with a gap of
// pure color between them. The weights below only add up to 1 while that stays true.
const float band0Start = 0.20; // grass ends / dirt starts
const float band0End   = 0.35;
const float band1Start = 0.45; // dirt ends / rock starts
const float band1End   = 0.60;
const float band2Start = 0.70; // rock ends / snow starts
const float band2End   = 0.85;

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_DIR   = 1;
const int LIGHT_TYPE_SPOT  = 2;

struct GPULight {
    vec4 pos_type;        // xyz position, w type
    vec4 dir_range;       // xyz direction, w range
    vec4 color_intensity; // rgb color, a intensity
    vec4 spotAngles;      // x cosInner, y cosOuter
};

// UBO for all lights
layout(std140, binding = 3) uniform LightBlock {
    ivec4 uLightHeader; // x = current light count
    GPULight uLights[64];
};

// Fades a light out to nothing by the time you hits its range.
// 1 at the light, 0 at the edge.
float RangeCutOff(float _dist, float _range) {
    float t = clamp(1.0 - _dist / _range, 0.0, 1.0);
    return t * t;
}

// How bright a light is at a given distance. The falloff curve times
// the range cutoff, so it dims naturally and still stops dead at the range.
float Attenuation(float _dist, float _range) {
    float inv = 1.0 / (1.0 + 0.09 * _dist + 0.032 * _dist * _dist);
    return inv * RangeCutOff(_dist, _range);
}

// Works out what one light adds to this pixel.
// Handles all three types: directional, point and spot.
// V is the direction from the pixel to the camera.
vec3 CalculateLight(GPULight _light, vec3 _normal, vec3 _dirPtoCam, vec3 _fragPos, vec3 _albedo) {
    int type = int(_light.pos_type.w + 0.5);

    vec3 lightRadiance = _light.color_intensity.rgb * _light.color_intensity.a;

    vec3 lightDirection;
    float atten = 1.0;
    float spot  = 1.0;

    if (type == LIGHT_TYPE_DIR)
    {
        lightDirection = normalize(-_light.dir_range.xyz);
    }
    else
    {
        vec3 toLight = _light.pos_type.xyz - _fragPos;
        float dist = length(toLight);
        lightDirection = (dist > 1e-6) ? (toLight / dist) : vec3(0, 1, 0);
        atten = Attenuation(dist, _light.dir_range.w);

        if (type == LIGHT_TYPE_SPOT)
        {
            vec3 lightToFrag = normalize(_fragPos - _light.pos_type.xyz);
            float cosTheta = dot(lightToFrag, normalize(_light.dir_range.xyz));
            spot = smoothstep(_light.spotAngles.y, _light.spotAngles.x, cosTheta);
        }
    }

    float NdotL = max(dot(_normal, lightDirection), 0.0);
    vec3 diffuse = NdotL * lightRadiance;

    vec3 H = normalize(lightDirection + _dirPtoCam);
    float spec = pow(max(dot(_normal, H), 0.0), 64.0);
    vec3 specular = 0.05 * spec * lightRadiance; // Terrain doesn't really need specular

    return (diffuse * _albedo + specular) * atten * spot;
}

void main()
{
    // Sample all four layers. Different tiling per layer hides the repetition a bit.
    // #TODO: maybe enable this modification in editor?
    vec3 c0 = texture(Tex0, v2Uv * 32.0).rgb;
    vec3 c1 = texture(Tex1, v2Uv * 32.0).rgb;
    vec3 c2 = texture(Tex2, v2Uv * 32.0).rgb;
    vec3 c3 = texture(Tex3, v2Uv * 16.0).rgb;

    // Normalize the height so the bands work no matter what the height scale is.
    float h = clamp((fLocalY - MinHeight) / (MaxHeight - MinHeight), 0.0, 1.0);

    // Blend weights. Built so they always sum to exactly 1.
    float f0 = smoothstep(band0Start, band0End, h); // grass to dirt fade
    float f1 = smoothstep(band1Start, band1End, h); // dirt to rock fade
    float f2 = smoothstep(band2Start, band2End, h); // rock to snow fade

    float w0 = 1.0 - f0;
    float w1 = f0 * (1.0 - f1);
    float w2 = f1 * (1.0 - f2);
    float w3 = f2;

    vec3 albedo = c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3;

    // Standard lighting loop, same as model.frag
    vec3 norm = normalize(v3Normal);
    vec3 V = normalize(CameraPos - v3WorldPos);

    vec3 color = albedo * 0.10; // small ambient

    int lightCount = clamp(uLightHeader.x, 0, 64);
    for (int i = 0; i < lightCount; ++i)
        color += CalculateLight(uLights[i], norm, V, v3WorldPos, albedo);

    FragColor = vec4(color, 1.0);
}
