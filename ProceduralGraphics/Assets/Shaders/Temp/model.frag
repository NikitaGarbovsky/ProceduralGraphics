in vec2 outUV;
in vec3 outNormal;

uniform sampler2D Tex0;

out vec4 FragColor;

void main()
{
	vec3 albedo = texture(Tex0, outUV).rgb;

	vec3 L = normalize(vec3(0.4, 1.0, 0.2));
	float ndl = max(dot(normalize(outNormal), L), 0.0);

	vec3 color = albedo * (0.85 + 0.85 * ndl);

	FragColor = vec4(color, 1.0);
}