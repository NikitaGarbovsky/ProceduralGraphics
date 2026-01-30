module;

// Normal imports
#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <string>

/// <summary>
/// This module is the "core" of the renderer, containing and combining all renderer logic 
/// in one single place. Access, initializing & running the render loop (including the rendering pipeline)
/// takes place here. 
/// </summary>
export module RendererCore;

// Codebase module imports
import DebugUtilities;
import RendererData;
import RendererUtilities;
import RendererPipeline;
import RendererEntitys;
import RendererFrame;
import RendererAssetPipeline;
import RendererInput;
import RendererCamera;
import RendererEditorUI;

// Function prototypes
void Render();
void LoadResources();

// Initializies the Renderer & GLFW window.
export bool InitRenderer() {
	Log("Initializing Renderer...");

	// Initialize glfw & the main window
	glfwInit();
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_SAMPLES, 8); // For MSAA

	MainWindow = glfwCreateWindow(1920, 1080, "Procedural Graphics", NULL, NULL);
	if (MainWindow == NULL)
	{
		LogCrash("GLFW failed to Initialize Properly");

		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(MainWindow);

	// Initialize glew
	glewExperimental = GL_TRUE;
	GLenum error = glewInit();
	if (error != GLEW_OK)
	{
		LogCrash("GLEW initialization failed!");
		Log((const char*)glewGetErrorString(error));
		return false;
	}

	// Set Viewport Color
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, 1920, 1080); // #TODO: update these to a setting somewhere.

	// Renderer configuration
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE); // For MSAA

	// Intialize Input & Camera
	InitInput(MainWindow);
	GEditorCam = {};
	SetPerspective(GEditorCam, 90.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

	// Initialize the rendering pipeline.
	InitRendererPipeline();

	// Initialize the EditorUI
	InitEditorUI(MainWindow);

	// Initialize Globals
	gTimeSinceAppStart = glfwGetTime();

	Log("Renderer Succussfully Initialized");
	
	LoadResources();
	return true;
}

export void LoadResources() {
	Log("Loading Resources...");

	// Load each of the shader programs that are used in the renderer.
	RenderObjProgram = LoadShaderProgram("Assets/Shaders/Temp/model.vert","Assets/Shaders/Temp/model.frag");
	PickingProgram = LoadShaderProgram("Assets/Shaders/Temp/picking.vert","Assets/Shaders/Temp/picking.frag");
	OutlineProgram = LoadShaderProgram("Assets/Shaders/Temp/outline.vert","Assets/Shaders/Temp/outline.frag");
	SelectedTintProgram = LoadShaderProgram("Assets/Shaders/Temp/selectedTint.vert","Assets/Shaders/Temp/selectedTint.frag");

	// Load model & create a Temporary REntity
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", RenderObjProgram, glm::vec3(-1,0,-1));

	// Load model & create a Temporary REntity
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", RenderObjProgram, glm::vec3(-10, 0, -1));

	// Debug
	ValidateREntityArrayAlignment();

	Log("Resources Successfully loaded.");
}

export void RenderLoop() {
	Log("Starting Render Loop...");
	while (glfwWindowShouldClose(MainWindow) == false)
	{
		// Input is registered at beginning of frame.
		FrameInputReset(MainWindow);

		glfwPollEvents();

		// #TODO move this to a central input location
		if (KeyDown(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(MainWindow, true);

		UpdateTimeData();

		// Update camera
		UpdateEditorCamera(GEditorCam, MainWindow, gDeltaTime, 1920, 1080);
		GCamera.view = GEditorCam.view;
		// #TODO: When Scene View viewport with imgui is implemented, update proj from that viewport's size.
		GCamera.proj = GEditorCam.proj;

		// Draw 3D Scene
		Render();

		// Draw Editor UI #TODO move ui and gizmos to their own dedicated render pass.
		if(!GInput.currentlyMoving)
			EditorUI_Draw(GCamera.view, GCamera.proj, 1920, 1080, SelectedEntity);

		// #TODO move this to a central input location
		if (MousePressed(GLFW_MOUSE_BUTTON_LEFT) && !EditorUI_WantsMouse())
			RenderPipeline_RequestPick();

		glfwSwapBuffers(MainWindow);

		// Consume any picking result that occured during this frame. 
		if (RenderPipeline_HasPickResult()) {
			uint32_t id = RenderPipeline_ConsumePickResult();
			
			if (id == 0) SelectedEntity = 0xFFFFFFFFu;
			else SelectedEntity = id - 1;
		}
	}
}

// Renders the 3D scene using the outlayed RenderPipeline
void Render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// Grab framebuffer size (main window size)
	int  fbW, fbH;
	glfwGetFramebufferSize(MainWindow, &fbW, &fbH);

	// RendererPipeline RenderFrame, (pass the main window size)
	RenderPipeline_RenderFrame(fbW, fbH);
}

export void CleanUpAndShutdown() {
	Log("Renderer Shutdown Initiated...");

	ShutdownPickingTarget();
	ShutdownViewportTarget();
	ShutdownRendererPipeline();
	glfwDestroyWindow(MainWindow);
	glfwTerminate();

	Log("Renderer Shutdown Successful.");
}