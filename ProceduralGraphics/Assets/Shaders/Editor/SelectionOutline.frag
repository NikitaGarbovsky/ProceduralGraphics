in vec2 v2Uv;

uniform sampler2D MaskTex;
uniform vec2 Resolution;
uniform vec4 OutlineColor;
uniform float OutlineWidth; // Thickness in pixels 

out vec4 FragColor;

// How many directions the mask gets grown in. Fewer and the outline starts looking
// one side per direction once the width goes past a few pixels.
const int kDirectionCount = 16;
const float kTwoPi = 6.28318530718; // Full circle

void main() {
	// Conversion from pixel to texture coordinates
	vec2 texel = 1.0 / Resolution;

	// How much of this pixel the object covers. 0 is empty space, 1 is fully inside it,
	// in between means this pixel sits on the silhouette.
	float center = texture(MaskTex, v2Uv).r;

	// Start at the center point
	float grownAmount = center;

	// Look outwards in kDirectionCount directions from the center,
	for (int i = 0; i < kDirectionCount; ++i)
	{
		float angle = (kTwoPi / float(kDirectionCount)) * float(i);
		vec2 offset = vec2(cos(angle), sin(angle)) * OutlineWidth * texel;

		// and keep the highest mask value found.
		grownAmount = max(grownAmount, texture(MaskTex, v2Uv + offset).r);
		grownAmount = max(grownAmount, texture(MaskTex, v2Uv + offset * 0.5).r);
	}

	// Scale what grew by how much of this pixel the object does not already cover. Inside 
	// always = (0), so only the border survives.
	float ring = grownAmount * (1.0 - center);

	// Nothing to draw here. Either means the empty screen further away than
	// OutlineWidth, or the whole inside of the object where startingPixel is already 1.
	if (ring < 0.01)
		discard;

	FragColor = vec4(OutlineColor.rgb, OutlineColor.a * ring);
}
