// Firework point vertex shader. Reads straight out of the position life buffer,
// xyz is the spark position and w is the life it has left.

layout (location = 0) in vec4 Position;

uniform mat4 VP;

// The fragment shader turns this into the fade out alpha.
out float LifeRemaining;

void main()
{
	LifeRemaining = Position.w;
	gl_Position = VP * vec4(Position.xyz, 1.0);

	// Increase the size a tad so its more visible
	gl_PointSize = 3.0;
}
