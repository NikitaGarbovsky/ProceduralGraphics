#include <cstdint>
#include <vector>
#include <glew.h>
#include <glm.hpp>

export module RendererEntitys;

// ID's are array indices into our renderer tables.
export using TransformID = uint32_t;
export using MeshID = uint32_t;
export using MaterialID = uint32_t;

/// <summary> Lightweight record that links together transform + mesh + material.
/// Each field is an index into its corresponding table. (array) </summary>
export struct REntity {
	MeshID mesh; // index into REntityMeshs
	MaterialID material; // index into REntityMaterials
};

// Transform table stored in SoA form.
// Index i corresponds to one transform across all arrays.
// worldMatrix is derived data computed each frame from position/rotation/scale.
export struct TransformData {
	std::vector<glm::vec3> position; // Local/World position
	std::vector<glm::vec3> rotation; // Euler, #TODO replace with quarternion later
	std::vector<glm::vec3> scale;	 
	std::vector<glm::mat4> worldMatrix; // (model matrix) computed per frame 
};

// Persistent GPU resource.
// Stored once and referenced by MESHID from entities.
export struct Mesh {
	GLuint vao = 0; // vertex array object (attribute layout + bound buffers)
	GLuint vbo = 0; // vertex buffer object
	GLuint ebo = 0; // element/index buffer object
	uint32_t indexCount = 0; // number of indices to draw (glDrawElements*)
};

// Bounds used for visiblity tests (frustrum culling).
export struct Bounds { // #TODO implement frustrum culling
	glm::vec3 center{0,0,0}; // center of bounding sphere 
	float radius = 1.0; // sphere radius
};

// Render state used to draw something.
// In a scalable renderer this can include: textures, render flags, blend/depth states, parameters.
// For now: one shader program + one texture slot, plus cached uniform locations.
export struct Material {
	GLuint program = 0; // OpenGL shader program handle
	GLuint tex0 = 0; // primary texture (diffuse/albedo) - 0 means no texture
	
	// Cached uniform locations
	GLint uView = -1; // ViewMat
	GLint uProj = -1; // ProjectionMat
};

// Simple camera data currently set in Load Resources.
// Probably move this when you need to
export struct CameraData {
	glm::mat4 view{ 1.0f };
	glm::mat4 proj{ 1.0f };
};

// All render entities in the scene. Indices are stable and referenced by visiblity lists.
export std::vector<REntity> CurrentRenderedEntitys;
export TransformData EntityTransforms; // Transform SoA table. entity.transform indexes into theses arrays.
export std::vector<Mesh> REntityMeshs; // Persistent GPU mesh resources. entity.mesh indexes into this array.
export std::vector<Bounds> REntityBounds; // #TODO for frustrum culling
export std::vector<Material> REntityMaterials; // Material resources used for rendering.
export CameraData GCamera; // Active camera for the current frame/pass.

export uint32_t CreateRenderEntity(MeshID _meshID, MaterialID _materialID, glm::vec3 _startingPosition)
{
	// Create the id of this new render entity. (id = size - 1 or the size of the array before appending)
	uint32_t id = (uint32_t)CurrentRenderedEntitys.size();

	// Append a new renderer entity
	CurrentRenderedEntitys.push_back(REntity{_meshID, _materialID });

	// Append new transform data.
	EntityTransforms.position.push_back(_startingPosition);
	EntityTransforms.rotation.push_back({ 0,0,0 });
	EntityTransforms.scale.push_back({ 1,1,1 });
	EntityTransforms.worldMatrix.push_back(glm::mat4(1.0f));

	// Append new bounds data.
	REntityBounds.push_back(Bounds{ {0,0,0}, 1.0f });

	return id;
}

// Sets an entities transform based on index
export void SetEntityTransform(uint32_t _entityIndex, const glm::vec3 _newPosition = glm::vec3(0), 
												const glm::vec3 _newRotation = glm::vec3(0),		
												const glm::vec3 _newScale = glm::vec3(1))
{
	if(_newPosition != glm::vec3(0)) EntityTransforms.position[_entityIndex] = _newPosition;
	if (_newRotation != glm::vec3(0)) EntityTransforms.rotation[_entityIndex] = _newRotation;
	if (_newScale != glm::vec3(1)) EntityTransforms.scale[_entityIndex] = _newScale;
}