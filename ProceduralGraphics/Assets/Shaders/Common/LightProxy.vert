layout (location = 0) in vec3 position;

uniform mat4 VP;
uniform mat4 ModelMatrix;

void main()
{
	gl_Position = VP * ModelMatrix * vec4(position, 1.0);
}