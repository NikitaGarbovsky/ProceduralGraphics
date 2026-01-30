in vec2 vUV;

uniform sampler2D Tex0;
uniform float TimeSeconds;
uniform vec3 TintColor;    
uniform float TintStrength; 
uniform float PulseSpeed;   

out vec4 FragColor;

void main()
{
    vec3 albedo = texture(Tex0, vUV).rgb;

    float pulse = 0.5 + 0.5 * sin(TimeSeconds * PulseSpeed);
    float k = TintStrength * pulse; // pulsing amount

    // Overlay-style tint: keep texture detail
    vec3 color = mix(albedo, TintColor, k);

    FragColor = vec4(color, 0.3);
}
