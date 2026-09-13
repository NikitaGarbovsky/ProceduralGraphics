// Only position is needed here, but the layout still has to line up with what the
// mesh vao hands over.
layout (location = 0) in vec3 position;

// Instance model matrix columns. 
layout (location = 4) in vec4 im0;
layout (location = 5) in vec4 im1;
layout (location = 6) in vec4 im2;
layout (location = 7) in vec4 im3;

uniform mat4 LightVP;

void main()
{
	// Model matrix 
	mat4 model = mat4(im0, im1, im2, im3);

	// Draw the object from where the light is standing instead of the camera.
	gl_Position = LightVP * model * vec4(position, 1.0);
}