in vec2 v2Uv;

uniform sampler2D SceneTex; 

out vec4 FragColor;

// Straight copy of the scene. This is the "no effect" entry in the editorUI.
// This is just here for consistency of the post processing pipeline code.
void main() {
	FragColor = texture(SceneTex, v2Uv);
}
