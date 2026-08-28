// Firework point fragment shader. Every firework instance gets its own color,
// and the alpha fades with the life left so particles go out cleanly.

in float LifeRemaining;

uniform vec3 Color;  
uniform float MaxLife; 

out vec4 FinalColor;

void main()
{
	// Kill dead particles
	if (LifeRemaining <= 0.0)
		discard;

	float Alpha = clamp(LifeRemaining / MaxLife, 0.0, 1.0);
	FinalColor = vec4(Color, Alpha);
}
