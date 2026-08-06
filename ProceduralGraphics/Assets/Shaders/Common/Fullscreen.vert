layout (location = 0) in vec2 position; 
layout (location = 1) in vec2 uv;

out vec2 v2Uv;

void main()
{
	v2Uv = uv;
	// Position already in ndc, no matrices needed here
	gl_Position = vec4(position, 0.0, 1.0);
}
