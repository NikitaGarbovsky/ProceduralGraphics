// Three outputs. The location numbers match the color attachments the
// geometry buffer was built with.
layout (location = 0) out vec4 Texture_Position;
layout (location = 1) out vec4 Texture_Normal;
layout (location = 2) out vec4 Texture_AlbedoShininess;

in vec2 v2Uv;
in vec3 v3Normal;
in vec3 v3WorldPos;

uniform sampler2D Tex0;

// One value for the whole scene. It gets stored per pixel so the lighting pass
// can read it back without needing to know anything about materials.
uniform float ObjectShininess;

void main()
{
	// No lighting happens here. This pass only records what the pixel is made of.

	// Alpha of 1 marks this pixel as having something in it. The lighting pass uses that
	// to tell real geometry apart from empty background.
	Texture_Position = vec4(v3WorldPos, 1.0);

	Texture_Normal = vec4(normalize(v3Normal), 1.0);

	Texture_AlbedoShininess.rgb = texture(Tex0, v2Uv).rgb;

	// This texture is 8 bit so alpha can only hold 0 to 1. Squash the shininess into
	// that range here, and the lighting pass stretches it back out.
	Texture_AlbedoShininess.a = ObjectShininess / 256.0;
}