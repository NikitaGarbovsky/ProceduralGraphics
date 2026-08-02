in vec2 v2Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;

uniform sampler2D Tex0;
uniform float Time; // Set every frame by the Perlin Noise scene

out vec4 FragColor;

// Maps the noise value through a fire style color gradient,
// black at the bottom up through reds and oranges to white at the top.
vec3 FireGradient(float _noise)
{
	if (_noise < 0.2)      // Black to dark red
		return mix(vec3(0.0, 0.0, 0.0), vec3(0.5, 0.0, 0.0), _noise / 0.2);
	else if (_noise < 0.4) // Dark red to red-orange
		return mix(vec3(0.5, 0.0, 0.0), vec3(1.0, 0.2, 0.0), (_noise - 0.2) / 0.2);
	else if (_noise < 0.6) // Red-orange to orange
		return mix(vec3(1.0, 0.2, 0.0), vec3(1.0, 0.6, 0.0), (_noise - 0.4) / 0.2);
	else if (_noise < 0.8) // Orange to pale yellow
		return mix(vec3(1.0, 0.6, 0.0), vec3(1.0, 1.0, 0.4), (_noise - 0.6) / 0.2);
	else				   // Pale yellow to white
		return mix(vec3(1.0, 1.0, 0.4), vec3(1.0, 1.0, 1.0), (_noise - 0.8) / 0.2);
}

void main()
{
	// Sample the noise 
	float noiseA = texture(Tex0, v2Uv).r;
	float noiseB = texture(Tex0, v2Uv.yx).r; 

	// Slides smoothly between 0 and 1 and back, roughly every 4.19 seconds.
	float t = 0.5 + 0.5 * sin(Time * 1.5); 

	// Blend the noise values based on t
	float noise = mix(noiseA, noiseB, t);

	// Map the noise fragment to a color
	FragColor = vec4(FireGradient(noise), 1.0);
}
