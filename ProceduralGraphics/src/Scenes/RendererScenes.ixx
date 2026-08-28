module;

// Normal imports
#include <glew.h>

/// <summary>
/// The scene system. The :Core partition holds the switching logic, and every
/// scene lives in its own partition so it is easy to find what each scene does & includes.
/// 
/// Add newly created scenes here.
/// </summary>
export module RendererScenes;

export import :Core;
export import :Sandbox;
export import :Terrain;
export import :PerlinNoise;
export import :PostProcess;
export import :Fireworks;

// Registers every scene in order. Called once at startup.
export void Scenes_RegisterAll(GLuint _defaultLitProgram)
{
	SceneDefaultProgram = _defaultLitProgram;

	Scene_Register(GetScene_Sandbox());    
	Scene_Register(GetScene_Terrain());     
	Scene_Register(GetScene_PerlinNoise()); 
	Scene_Register(GetScene_PostProcess());
	Scene_Register(GetScene_Fireworks());
	// #TODO: add future scenes here.
}
