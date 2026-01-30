#include <cstdint>
#include <vector>
#include <glew.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

///
/// The REntity module manages the data, references & management of REntitys.
/// 
export module RendererEntitys;

// ID's are array indices into our renderer tables.
export using TransformID = uint32_t;
export using MeshID = uint32_t;
export using MaterialID = uint32_t;
export using ModelID = uint32_t;

// Bounds used for visiblity tests (frustrum culling).
export struct Bounds {
	glm::vec3 center{ 0,0,0 }; // center of bounding sphere 
	float radius = 1.0; // sphere radius
};

export struct Primitive {
	MeshID mesh;
	MaterialID material;
};

export struct Model {
	std::vector<Primitive> primitives;
	Bounds localBounds;
};

export struct Submesh {
	MeshID mesh;
	MaterialID material;
};

// An "object" (transform + range into Submeshes)
export struct REntity {
	uint32_t firstSubmesh = 0; // Id's to find submeshes
	uint32_t submeshCount = 0;

	Bounds localBounds; // bounds for the whole object (all submeshes combined)
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

	Bounds localBounds; // The bounds of this mesh, (used for frustrum culling)
};

// Render state used to draw something.
// TODO# In a scalable renderer this can include: textures, render flags, blend/depth states, parameters.
// For now: one shader program + one texture slot, plus cached uniform locations.
export struct Material {
	GLuint program = 0; // OpenGL shader program handle
	GLuint tex0 = 0; // primary texture (diffuse/albedo) - 0 means no texture
	
	// Cached uniform locations
	GLint uView = -1; // ViewMat
	GLint uProj = -1; // ProjectionMat
	GLint uTex0 = -1; // Texture
};

// All render entities in the scene. Indices are stable and referenced by visiblity lists.
export std::vector<REntity> CurrentRenderedEntitys; // (not necessarily Currently Visible (culled))
export int FrustrumCulledEntitiesThisFrame; // Count of how many entities were frustum cull this frame.
export std::vector<Submesh> REntitySubmeshes;
export std::vector<Model> REntityModels;
export TransformData EntityTransforms; // Transform SoA table. entity.transform indexes into theses arrays.
export std::vector<Mesh> REntityMeshs; // Persistent GPU mesh resources. entity.mesh indexes into this array.
export std::vector<Material> REntityMaterials; // Material resources used for rendering.

// Creates a REntity 
export uint32_t CreateRenderEntity(uint32_t _firstSubmesh,
	uint32_t _submeshCount,
	const Bounds& _localBounds,
	glm::vec3 _startingPosition)
{
	// Create the id of this new render entity. (id = size - 1 or the size of the array before appending)
	uint32_t id = (uint32_t)CurrentRenderedEntitys.size();

	REntity newEntity;
	newEntity.firstSubmesh = _firstSubmesh;
	newEntity.submeshCount = _submeshCount;
	newEntity.localBounds.center = _localBounds.center;
	newEntity.localBounds.radius = _localBounds.radius;

	// Append a new renderer entity
	CurrentRenderedEntitys.push_back(newEntity);

	// Append new transform data.
	EntityTransforms.position.push_back(_startingPosition);
	EntityTransforms.rotation.push_back({ 0,0,0 });
	EntityTransforms.scale.push_back({ 1,1,1 });
	EntityTransforms.worldMatrix.push_back(glm::mat4(1.0f));

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

// Composes a model matrix of an entities data, in use for updating transforms of entities
export glm::mat4 GetEntityModelMatrix(uint32_t entityId) {
	const glm::vec3& p = EntityTransforms.position[entityId];
	const glm::vec3& r = EntityTransforms.rotation[entityId]; // degrees
	const glm::vec3& s = EntityTransforms.scale[entityId];

	glm::mat4 M(1.0f);
	M = glm::translate(M, p);
	// #TODO: Standardise form of rotation matrix across renderer.
	M = glm::rotate(M, glm::radians(r.z), glm::vec3(0, 0, 1));
	M = glm::rotate(M, glm::radians(r.y), glm::vec3(0, 1, 0));
	M = glm::rotate(M, glm::radians(r.x), glm::vec3(1, 0, 0));

	M = glm::scale(M, s);
	return M;
}
