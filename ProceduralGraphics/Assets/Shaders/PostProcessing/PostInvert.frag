in vec2 v2Uv;

uniform sampler2D SceneTex;
uniform float Amount; // 0 is the plain scene, 1 is fully inverted

out vec4 FragColor;

void main()
{
	vec3 scene = texture(SceneTex, v2Uv).rgb;

	// Flip each channel by taking it away from 1
	vec3 inverted = 1.0 - scene;

	FragColor = vec4(mix(scene, inverted, Amount), 1.0);
}
