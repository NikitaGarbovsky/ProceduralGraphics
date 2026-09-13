layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

// Model Matrix Columns 
layout (location = 4) in vec4 im0;
layout (location = 5) in vec4 im1;
layout (location = 6) in vec4 im2;
layout (location = 7) in vec4 im3;

uniform mat4 ViewMat;
uniform mat4 ProjectionMat;

// The exact matrices the depth pass drew with. Pushed in by the shadow pass each frame.
uniform mat4 LightVP0;
uniform mat4 LightVP1;

out vec2 v2Uv;
out vec3 v3Normal;
out vec3 v3WorldPos;
out vec4 v4FragPosLight0;
out vec4 v4FragPosLight1;

void main()
{
	mat4 model = mat4(im0, im1, im2, im3);

	vec4 worldPos = model * vec4(position, 1.0);
	v3WorldPos = worldPos.xyz;
	v2Uv = uv;

	mat3 normalMat = transpose(inverse(mat3(model)));
	v3Normal = normalize(normalMat * normal);

	// Where this vertex lands in each lights view. The fragment shader compares this
	// against the depth map to see if something is blocking the light.
	v4FragPosLight0 = LightVP0 * worldPos;
	v4FragPosLight1 = LightVP1 * worldPos;

	gl_Position = ProjectionMat * ViewMat * model * vec4(position, 1.0);
}