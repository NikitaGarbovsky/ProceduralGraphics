in vec2 v2Uv;

uniform sampler2D SceneTex;
uniform float Amount; // 0 is the plain scene, 1 is fully grey

out vec4 FragColor;

// Luminosity weights.
const vec3 kLuminosity = vec3(0.3, 0.59, 0.11);

void main() {
	vec3 scene = texture(SceneTex, v2Uv).rgb;

	// Get the greyscale luminosity corrected method.
	float grey = dot(scene, kLuminosity);

	FragColor = vec4(mix(scene, vec3(grey), Amount), 1.0);
}
