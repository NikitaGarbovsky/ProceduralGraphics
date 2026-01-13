layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoords;
layout (location = 2) in vec3 normals;

uniform mat4 ProjectionMat;
uniform mat4 ViewMat;
uniform mat4 ModelMat;

out vec2 FragTexCoords;
out vec3 FragNormal;
out vec3 FragPos;

void main()
{
	vec4 worldPos = ModelMat * vec4(position, 1.0);
	gl_Position = ProjectionMat * ViewMat * worldPos;

	FragTexCoords = texCoords;
	FragNormal = mat3(transpose(inverse(ModelMat * ViewMat * ProjectionMat))) * normals;
	FragPos = vec3((ModelMat * ViewMat * ProjectionMat) * vec4(position, 1.0));
}