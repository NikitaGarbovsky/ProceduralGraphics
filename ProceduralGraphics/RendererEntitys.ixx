#include <cstdint>
#include <vector>
#include <glew.h>
#include <glm.hpp>

export module RendererEntitys;

using TransformID = uint32_t;
using MeshID = uint32_t;
using MaterialID = uint32_t;

// Each Renderable Entity holds their ID (indicies or index) 
// in the container they're located in.  
struct REntity
{
	TransformID transform;
	MeshID mesh;
	MaterialID material;
};

// SoA holding transform data for each REntity (Renderer Entity)
struct TransformData {
	std::vector<glm::vec3> position;
	std::vector<glm::vec3> rotation;
	std::vector<glm::vec3> scale;
	std::vector<glm::mat4> worldMatrix;
};

struct Mesh {
	GLuint vao;
	uint32_t indexCount;
};

struct Bounds {
	glm::vec3 center;
	float radius;
};

std::vector<REntity> CurrentRenderedEntitys;
std::vector<Mesh> REntityMeshs;
std::vector<Bounds> REntityBounds; // For frustrum culling