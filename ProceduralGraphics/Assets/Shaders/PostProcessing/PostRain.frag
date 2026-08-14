in vec2 v2Uv;

uniform sampler2D SceneTex;
uniform sampler2D EffectTex;

uniform float Time;
uniform float Speed;    // How fast the noise scrolls down the screen
uniform float Strength; // How far the noise is allowed to shove
uniform float Tiling;   // How many times the noise repeats across the screen
uniform float Squash;   // How far the noise gets squashed vertically, smaller means longer streaks

out vec4 FragColor;

void main() {
	vec2 rainUv = vec2(v2Uv.x * Tiling, v2Uv.y * Squash + Time * Speed);

	vec2 rain = (texture(EffectTex, rainUv).rg - 0.5) * Strength;

	FragColor = vec4(texture(SceneTex, v2Uv - rain).rgb, 1.0);
}