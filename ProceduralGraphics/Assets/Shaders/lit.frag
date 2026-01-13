in vec2 FragTexCoords;
in vec3 FragNormal;
in vec3 FragPos;

uniform sampler2D Texture0;
uniform float AmbientStrength = 0.55;
uniform vec3 AmbientColor = vec3(1.0, 1.0, 1.0);
uniform vec3 LightColor = vec3(0.5, 0.1, 0.8);
uniform vec3 LightPos = vec3(0.0, 0.0, 100.0);
uniform vec3 CameraPos;
uniform float LightSpecularStrength = 10.0;
uniform float ObjectShinyness = 1000.0;

out vec4 FragColor;

void main()
{
	vec3 lighting = CalculateBlinnPhong(
		FragNormal,
		FragPos,
		CameraPos,
		LightPos,
		LightColor,
		AmbientStrength,
		AmbientColor,
		LightSpecularStrength,
		ObjectShinyness
	);

	FragColor = vec4(lighting, 1.0) * texture(Texture0, FragTexCoords);
}