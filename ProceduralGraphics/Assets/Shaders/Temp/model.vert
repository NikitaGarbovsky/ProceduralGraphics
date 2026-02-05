layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 4) in vec4 im0;
layout (location = 5) in vec4 im1;
layout (location = 6) in vec4 im2;
layout (location = 7) in vec4 im3;

uniform mat4 ViewMat;
uniform mat4 ProjectionMat;

out vec2 v3Uv;
out vec3 v3Normal;
out vec3 v3WorldPos;

void main() {
	mat4 model = mat4(im0, im1, im2, im3);

	vec4 worldPos = model * vec4(position, 1.0);
	v3WorldPos = worldPos.xyz;
	v3Uv = uv;

	mat3 normalMat = transpose(inverse(mat3(model)));
	v3Normal = normalize(normalMat * normal);


	gl_Position = ProjectionMat * ViewMat * model * vec4(position, 1.0);
}