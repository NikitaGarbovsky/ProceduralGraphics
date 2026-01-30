layout (location = 0) in vec3 position;

layout (location = 4) in vec4 im0;
layout (location = 5) in vec4 im1;
layout (location = 6) in vec4 im2;
layout (location = 7) in vec4 im3;

uniform mat4 ViewMat;
uniform mat4 ProjectionMat;

uniform vec3 Pivot;
uniform float OutlineScale;

void main()
{
	mat4 model = mat4(im0, im1, im2, im3);
	vec3 p = (position - Pivot) * OutlineScale + Pivot;
	gl_Position = ProjectionMat * ViewMat * model * vec4(p, 1.0);
}