export module RendererFrame;

import <vector>;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

import RendererEntitys; // For MaterialID & MeshID

// Contains a big blob of hot-data that will be send to the GPU. 
export struct InstanceData {
	glm::mat4 model;
};

/// <summary> Render queue entry. </summary>
export struct RenderItem {
	MaterialID material;
	MeshID mesh;

	uint32_t indexCount = 0;
	uint32_t instanceIndex = 0;
	uint64_t sortKey = 0;
};

export struct DrawCommand {
	MaterialID material;
	MeshID mesh;

	uint32_t indexCount = 0;
	uint32_t instanceOffset = 0;
	uint32_t instanceCount = 0;
};

export struct FrameContext {
	// Array of ID's referencing visible render entities for this frame.
	std::vector<uint32_t> visible; 
	// Array of big blob of data we send to the GPU 
	std::vector<InstanceData> instances;
	std::vector<RenderItem> items;
	std::vector<DrawCommand> commands;

	GLuint instanceVBO = 0;

	void Clear() {
		visible.clear();
		instances.clear();
		items.clear();
		commands.clear();
	}
};

export void InitFrameContext(FrameContext& _f)
{
	glGenBuffers(1, &_f.instanceVBO);
}

export void ShutDownFrameContext(FrameContext& _f)
{
	if (_f.instanceVBO) glDeleteBuffers(1, &_f.instanceVBO);
	_f.instanceVBO = 0;
}