module;

// Normal imports
#include <string>

/// <summary>
/// The core of the scene system. Holds the scene table, swaps between scenes, and cleans up
/// everything a scene created when it gets switched.
///
/// How scene switching works: all the renderer tables only ever grow,
/// So right before a scene loads we record how big every table is (the scene preset). When the
/// scene unloads, we shrink the tables back to those sizes and everything the scene made is
/// gone. Anything created before the first scene loaded sits below the scene preset and
/// survives every switch.
///
/// Switches are requested through UI, keys and applied once per frame at
/// the top of the render loop, so the tables never change shape in the middle of a frame.
/// </summary>
export module RendererScenes:Core;

import RendererEntitys;   
import RendererData;     
import RendererLights;    
import DebugUtilities;
import <cstdint>;
import <glew.h>;
import <glm.hpp>;

// Where the generated noise files go. The terrain scene reads noise.raw as its heightmap.
export constexpr const char* kNoiseRawPath = "Resources/Textures/noise.raw";
export constexpr const char* kNoiseJpgPath = "Resources/Textures/noise.jpg";

// A scene consists of pointers to related elements, configured in each scenes respective module. 
export struct SceneFuncs
{
	const char* name = "Unnamed Scene";			// Scene name
	const char* description = "";               // Tooltip on the scene's hotbar button.
	void (*Init)() = nullptr;                   // Creates the scene's content.
	void (*Update)(float _deltaTime) = nullptr; // Per frame logic (timers etc).
	void (*DrawUI)() = nullptr;                 // The scene's own controls, drawn inside the Scene panel.
	void (*Shutdown)() = nullptr;               // Extra cleanup only (e.g. textures the scene made itself).
};

// The default lit shader program, set once by Scenes_RegisterAll. Scenes use this until they
// have dedicated shaders of their own.
export GLuint SceneDefaultProgram = 0;

// Table sizes recorded right before a scene loads. Anything at an index past these counts
// are added to the scene and gets removed when it unloads.
struct ScenePreset
{
	uint32_t entityCount = 0;
	uint32_t submeshCount = 0;
	uint32_t meshCount = 0;
	uint32_t materialCount = 0;
	uint32_t lightCount = 0;
};

// ==========================================================================================
// Module state
// ==========================================================================================
constexpr uint32_t MaxScenes = 7;
constexpr uint32_t kNoSelection = 0xFFFFFFFFu; // means nothing selected.

static SceneFuncs GSceneTable[MaxScenes];
static uint32_t GSceneCount = 0;

static int32_t GActiveScene = -1;    // Index into GSceneTable, -1 means none active yet.
static int32_t GRequestedScene = -1; // Pending switch, -1 means no request.
static ScenePreset GScenePreset;

// ==========================================================================================
// Internal
// ==========================================================================================

// Record the current size of every table the scenes can add to.
static void CaptureScenePreset()
{
	GScenePreset.entityCount = (uint32_t)CurrentRenderedEntitys.size();
	GScenePreset.submeshCount = (uint32_t)REntitySubmeshes.size();
	GScenePreset.meshCount = (uint32_t)REntityMeshs.size();
	GScenePreset.materialCount = (uint32_t)REntityMaterials.size();
	GScenePreset.lightCount = GetLightCount();
}

// Destroy everything the outgoing scene created and shrink the tables back down.
static void RollbackToScenePreset()
{
	// Free the GPU buffers of any meshes the scene created before dropping their entries.
	for (size_t i = GScenePreset.meshCount; i < REntityMeshs.size(); ++i)
	{
		glDeleteVertexArrays(1, &REntityMeshs[i].vao);
		glDeleteBuffers(1, &REntityMeshs[i].vbo);
		glDeleteBuffers(1, &REntityMeshs[i].ebo);
	}

	REntityMeshs.resize(GScenePreset.meshCount);
	REntityMaterials.resize(GScenePreset.materialCount);
	REntitySubmeshes.resize(GScenePreset.submeshCount);

	CurrentRenderedEntitys.resize(GScenePreset.entityCount);
	EntityTransforms.position.resize(GScenePreset.entityCount);
	EntityTransforms.rotation.resize(GScenePreset.entityCount);
	EntityTransforms.scale.resize(GScenePreset.entityCount);
	EntityTransforms.worldMatrix.resize(GScenePreset.entityCount);

	// Do the light reset one seperately, as the light module owns all the lights in the scene.
	Lights_TruncateTo(GScenePreset.lightCount);
}

// ==========================================================================================
// Public API
// ==========================================================================================

// Adds a scene to the table and returns its index. Called once per scene at startup.
export uint32_t Scene_Register(const SceneFuncs& _scene)
{
	if (GSceneCount >= MaxScenes)
	{
		LogWarning("Scene_Register: scene table full, registration ignored.");
		return MaxScenes - 1;
	}

	GSceneTable[GSceneCount] = _scene;
	return GSceneCount++;
}

// Asks for a switch to the given scene. The actual switch
// happens at the top of the next frame. Requesting the active scene reloads it.
export void Scene_RequestSwitch(int32_t _sceneIndex)
{
	if (_sceneIndex < 0 || _sceneIndex >= (int32_t)GSceneCount)
	{
		LogWarning("Scene_RequestSwitch: invalid scene index.");
		return;
	}
	GRequestedScene = _sceneIndex;
}

// Tear down and reload the active scene.
export void Scene_RequestReload()
{
	if (GActiveScene >= 0)
		GRequestedScene = GActiveScene;
}

// Applies a pending switch if there is one. Called exactly once per frame in the render
// loop, before scene update and rendering.
export void Scene_ApplyPendingSwitch()
{
	if (GRequestedScene < 0)
		return;

	const int32_t target = GRequestedScene;
	GRequestedScene = -1;

	// 1. Unload the outgoing scene.
	if (GActiveScene >= 0)
	{
		if (GSceneTable[GActiveScene].Shutdown)
			GSceneTable[GActiveScene].Shutdown();

		RollbackToScenePreset();

		// Deselect everything so the editor never points at something that just got removed.
		SelectedEntity = kNoSelection;
		SelectedLight = kNoSelection;
	}

	// 2. Record the table sizes, then bring up the new scene on top of them.
	CaptureScenePreset();
	GActiveScene = target;

	// Every scene starts with one directional light so content is lit out of the box.
	// It's created after the scene preset so it belongs to the scene and gets removed with it.
	{
		LightID sun = CreateDirectionalLight(glm::vec3(0.0f, 40.0f, 0.0f), glm::vec3(-55.0f, 30.0f, 0.0f));
		SetLightIntensity(sun, 1.0f);
	}

	Log((std::string("Scene switch: ") + GSceneTable[target].name).c_str());

	if (GSceneTable[target].Init)
		GSceneTable[target].Init();
}

// Runs the active scene's per frame logic.
export void Scene_Update(float _deltaTime)
{
	if (GActiveScene >= 0 && GSceneTable[GActiveScene].Update)
		GSceneTable[GActiveScene].Update(_deltaTime);
}

// Draws the active scene's controls. Called by the Scene panel in the editor UI.
export void Scene_DrawActiveUI()
{
	if (GActiveScene >= 0 && GSceneTable[GActiveScene].DrawUI)
		GSceneTable[GActiveScene].DrawUI();
}

// ==========================================================================================
// Queries for the editor UI, #TODO: maybe move this into another module?
// ==========================================================================================
export int32_t Scene_ActiveIndex() { return GActiveScene; }
export uint32_t Scene_Count() { return GSceneCount; }

export const char* Scene_Name(uint32_t _index)
{
	return (_index < GSceneCount) ? GSceneTable[_index].name : "<invalid>";
}

export const char* Scene_Description(uint32_t _index)
{
	return (_index < GSceneCount) ? GSceneTable[_index].description : "";
}

// Entities below this count were made at engine startup and survive every scene switch.
// Entities at or past it belong to the active scene. The entity list uses this to group.
export uint32_t Scene_PersistentEntityCount() { return GScenePreset.entityCount; }
