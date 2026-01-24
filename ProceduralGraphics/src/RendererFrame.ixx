/// <summary>
/// This module manages the abstraction of a Frame, we use this to fill data into it to
/// efficiently draw only what is necessary to the screen. 
/// </summary>
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

export struct FrameCommon {
	// Per-frame cached camera/frustrum data
	glm::mat4 view{ 1.0f };
	glm::mat4 proj{ 1.0f };
	glm::mat4 viewProj{ 1.0f };

	glm::vec3 cameraPos{ 0.0f };
	glm::vec3 cameraForward{ 0.0f,0.0f,-1.0f };

	glm::vec4 frustrumPlanes[6]{}; // left,right,bottom,top,near,far

	int viewportW = 0;
	int viewportH = 0;
};

export struct PassContext {
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

export void InitPassContext(PassContext& _f)
{
	glGenBuffers(1, &_f.instanceVBO);
}

export void ShutDownPassContext(PassContext& _f)
{
	if (_f.instanceVBO) glDeleteBuffers(1, &_f.instanceVBO);
	_f.instanceVBO = 0;
}