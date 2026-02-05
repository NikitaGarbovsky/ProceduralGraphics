in vec2 v3Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;

uniform sampler2D Tex0;
uniform vec3 CameraPos;

out vec4 FragColor;

const int LIGHT_TYPE_POINT  = 0;
const int LIGHT_TYPE_DIR = 1;
const int LIGHT_TYPE_SPOT  = 2;

struct GPULight {
    vec4 pos_type;        // xyz position, w type,         16 bytes
    vec4 dir_range;       // xyz direction, w range,       16 bytes 
    vec4 color_intensity; // rgb color, a intensity,       16 bytes 
    vec4 spotAngles;      // x cosInner, y cosOuter,       16 bytes
}; 

// UBO for all lights
layout(std140, binding = 3) uniform LightBlock {
    // std140-safe header (16 bytes)
    ivec4 uLightHeader; // x = currentLightCount, y/z/w unused

    // Maximum amount of lights
    GPULight uLights[64];
}; 

// Helper for cutting off distance 
float RangeCutOff(float _dist, float _range) {
    // 1 at center, smoothly to 0 near range
    float t = clamp(1.0 - _dist / _range, 0.0, 1.0);
    
    return t * t;
}
// Drop off distance for lights
float Attenuation(float _dist, float _range) {
    float inv = 1.0 / (1.0 + 0.09 * _dist + 0.032 * _dist * _dist);
    return inv * RangeCutOff(_dist, _range);
}

vec3 CalculateLight(GPULight _light, vec3 V, vec3 _fragPos, vec3 albedo) {
    int type = int(_light.pos_type.w + 0.5);

    vec3 lightRadiance = _light.color_intensity.rgb * _light.color_intensity.a;

    vec3 lightDirection;
    float atten = 1.0; // Default full attenuation (changes if not directional light)
    float spot  = 1.0;

    if (type == LIGHT_TYPE_DIR)
    {
        lightDirection = normalize(-_light.dir_range.xyz);
    }
    else
    {
        vec3 toLight = _light.pos_type.xyz - _fragPos;
        float dist = length(toLight);
        lightDirection = (dist > 1e-6) ? (toLight / dist) : vec3(0,1,0);
        atten = Attenuation(dist,_light.dir_range.w); 

        if (type == LIGHT_TYPE_SPOT)
        {
            vec3 lightToFrag = normalize(_fragPos - _light.pos_type.xyz);
            float cosTheta = dot(lightToFrag, normalize(_light.dir_range.xyz));

            float cosInner = _light.spotAngles.x;
            float cosOuter = _light.spotAngles.y;

            spot = smoothstep(cosOuter, cosInner, cosTheta);
        }
    }

    float NdotL = max(dot(v3Normal, lightDirection), 0.0);
    vec3 diffuse = NdotL * lightRadiance;

    vec3 H = normalize(lightDirection + V);
    float specPow = 64.0; // TODO make this editable in editor UI
    float specStr = 0.25; // TODO make this editable in editor UI
    float spec = pow(max(dot(v3Normal, H), 0.0), specPow);
    vec3 specular = specStr * spec * lightRadiance;

    // Albedo only affects diffuse; spec stays “light-colored”
    return (diffuse * albedo + specular) * atten * spot;
}

void main() { 
    vec3 albedo = texture(Tex0, v3Uv).rgb;

    vec3 V = normalize(CameraPos - v3WorldPos);

    // Small ambient
    vec3 color = albedo * 0.10; // #TODO: make this ambient editable in editor 

    int lightCount = clamp(uLightHeader.x, 0, 64);

    // Loop through all lights and complete each and every one of their calulations
    // #TODO: This will change when you change pipelines
    for (int i = 0; i < lightCount; ++i)
        color += CalculateLight(uLights[i], V, v3WorldPos, albedo);

    FragColor = vec4(color, 1.0);
}