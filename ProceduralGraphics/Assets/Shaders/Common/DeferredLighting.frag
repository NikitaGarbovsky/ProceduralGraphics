in vec2 v2Uv;

// The three pieces the geometry pass wrote out.
uniform sampler2D Texture_Position;
uniform sampler2D Texture_Normal;
uniform sampler2D Texture_AlbedoShininess;

uniform vec3 CameraPos;
uniform float AmbientStrength;

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

// Fades a light out to nothing by the time something hits its range.
float RangeCutOff(float _dist, float _range) {
    float t = clamp(1.0 - _dist / _range, 0.0, 1.0);
    return t * t;
}

// How bright a light is at a given distance.
float Attenuation(float _dist, float _range) {
    float inv = 1.0 / (1.0 + 0.09 * _dist + 0.032 * _dist * _dist);
    return inv * RangeCutOff(_dist, _range);
}

// Same blinn-phong maths as the forward shaders. The only difference is that everything
// it needs comes out of a texture instead of the vertex shader. #TODO: somehow move this to a modular shader function system.
// so other code can reference it.
vec3 CalculateLight(GPULight _light, vec3 _normal, vec3 _dirPtoCam, vec3 _fragPos,
                    vec3 _albedo, float _shininess)
{
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

    // Halfway vector between the light and the eye, that is the blinn part of blinn-phong.
    vec3 H = normalize(lightDirection + _dirPtoCam);
    float spec = pow(max(dot(_normal, H), 0.0), _shininess);
    vec3 specular = 0.25 * spec * lightRadiance;

    return (diffuse * _albedo + specular) * atten * spot;
}

void main()
{
    vec4 positionSample = texture(Texture_Position, v2Uv);

    // Alpha of 0 means the geometry pass never drew here, so throw the pixel away.
    if (positionSample.a < 0.5)
        discard;

    vec3 fragPos = positionSample.xyz;
    vec3 normal  = normalize(texture(Texture_Normal, v2Uv).xyz);

    vec4 albedoShininess = texture(Texture_AlbedoShininess, v2Uv);
    vec3 albedo = albedoShininess.rgb;

    // Stretch the packed value back out into a usable exponent.
    float shininess = max(albedoShininess.a * 256.0, 1.0);

    vec3 V = normalize(CameraPos - fragPos);

    vec3 color = albedo * AmbientStrength;

    int lightCount = clamp(uLightHeader.x, 0, 64);

    // This is the whole point of deferred. One loop over every light, run once per pixel
    // on screen, instead of once per light per object.
    for (int i = 0; i < lightCount; ++i)
        color += CalculateLight(uLights[i], normal, V, fragPos, albedo, shininess);

    FragColor = vec4(color, 1.0);
}