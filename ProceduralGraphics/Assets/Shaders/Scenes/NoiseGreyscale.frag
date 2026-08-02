in vec2 v2Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;

uniform sampler2D Tex0;

out vec4 FragColor;

// Shows the raw noise exactly as generated, no lighting.
void main()
{
	float noise = texture(Tex0, v2Uv).r;
	FragColor = vec4(vec3(noise), 1.0);
}
