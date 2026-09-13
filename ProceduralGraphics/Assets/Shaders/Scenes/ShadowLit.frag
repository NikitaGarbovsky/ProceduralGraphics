in vec2 v2Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;
in vec4 v4FragPosLight0;
in vec4 v4FragPosLight1;

uniform sampler2D Tex0;
uniform vec3 CameraPos;

// The two depth maps. The shadow pass leaves them on units 4 and 5.
uniform sampler2D ShadowMap0;
uniform sampler2D ShadowMap1;

// Which lights in the light block the maps belong to, and how many are in use.
uniform int ShadowLightIndex0;
uniform int ShadowLightIndex1;
uniform int ShadowCount;

// Pushes the stored depth back a bit so a surface stops shadowing itself.
uniform float ShadowBias;

// 1 = 3x3 average, 3 = 7x7. Bigger is softer shadows and slower.
uniform int PCFRadius;

out vec4 FragColor;

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

// Fades a light out to nothing by the time you hit its range.
float RangeCutOff(float _dist, float _range) {
    float t = clamp(1.0 - _dist / _range, 0.0, 1.0);
    return t * t;
}

// How bright a light is at a given distance.
float Attenuation(float _dist, float _range) {
    float inv = 1.0 / (1.0 + 0.09 * _dist + 0.032 * _dist * _dist);
    return inv * RangeCutOff(_dist, _range);
}

// Works out what one light adds to this pixel. Same as the other lit shaders, but with
// the specular turned down because a strong highlight fights with the shadow edges.
// #TODO: figure out a way to make this modular, so other shaders can use it.
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
    vec3 specular = 0.15 * spec * lightRadiance;

    return (diffuse * _albedo + specular) * atten * spot;
}

// How much of this pixel is in shadow for one light.
// 0 means fully lit, 1 means fully in shadow.
float CalculateShadow(sampler2D _shadowMap, vec4 _fragPosLightSpace)
{
    // The position is still in clip space, so we do the perspective divide (by w) ourselves.
    // An orthographic projection does not really need it since w stays 1, but doing it
    // anyway keeps this correct if the light ever switches to perspective. 
    // (#TODO: future when other light types are effected by shadows)
    vec3 ndcSpace = _fragPosLightSpace.xyz / _fragPosLightSpace.w;

    // Clip space runs -1 to 1, the depth map runs 0 to 1, so shift it across.
    vec3 projCoords = ndcSpace * 0.5 + 0.5;

    // Past the far plane of the light nothing was ever drawn, so call it lit.
    if (projCoords.z > 1.0)
        return 0.0;

    // How far this pixel is from the light, nudged back by the bias.
    float currentDepth = projCoords.z - ShadowBias;

    // Average a small square of the depth map instead of taking one sample. 
    // softens shadow edges.
    vec2 texelSize = 1.0 / vec2(textureSize(_shadowMap, 0));
    float shadow = 0.0;
    int count = 0;

    for (int row = -PCFRadius; row <= PCFRadius; ++row)
    {
        for (int col = -PCFRadius; col <= PCFRadius; ++col)
        {
            vec2 sampleUv = projCoords.xy + vec2(col, row) * texelSize;
            float closestDepth = texture(_shadowMap, sampleUv).r;

            shadow += (currentDepth > closestDepth) ? 1.0 : 0.0;
            count++;
        }
    }

    return shadow / float(count);
}

void main()
{
    vec3 albedo = texture(Tex0, v2Uv).rgb;
    vec3 norm = normalize(v3Normal);
    vec3 V = normalize(CameraPos - v3WorldPos);

    // Work each shadow out once rather than inside the light loop.
    float shadow0 = (ShadowCount > 0) ? CalculateShadow(ShadowMap0, v4FragPosLight0) : 0.0;
    float shadow1 = (ShadowCount > 1) ? CalculateShadow(ShadowMap1, v4FragPosLight1) : 0.0;

    // Small ambient so shadowed areas are dark but still readable.
    vec3 color = albedo * 0.10;

    int lightCount = clamp(uLightHeader.x, 0, 64);
    for (int i = 0; i < lightCount; ++i)
    {
        vec3 contribution = CalculateLight(uLights[i], norm, V, v3WorldPos, albedo);

        // Only the lights that actually have a depth map get dimmed.
        if (i == ShadowLightIndex0) contribution *= (1.0 - shadow0);
        else if (i == ShadowLightIndex1) contribution *= (1.0 - shadow1);

        color += contribution;
    }

    FragColor = vec4(color, 1.0);
}