uniform vec3 LightColor;
uniform float Opacity;

out vec4 FragColor;

void main()
{
	FragColor = vec4(LightColor, Opacity);
}