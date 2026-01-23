module;

// Normal imports
#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <string>

/// <summary>
/// This module is the "core" of the renderer, containing and combining all renderer logic 
/// in one sole place. Access, initializing & running the render loop takes place here. 
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

// Function prototypes
void Render();
void LoadResources();

// Global accessible info for the renderer pipeline
static FrameContext GFrame;

// Initializies the Renderer & GLFW window.
export bool InitRenderer()
{
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

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, 1920, 1080);

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

	Log("Renderer Succussfully Initialized");
	
	LoadResources();
	return true;
}

export void LoadResources()
{
	Log("Loading Resources...");

	// Create a dumby program thats used for rendering a model.
	RenderObjProgram = LoadShaderProgram("Assets/Shaders/Temp/model.vert",
										 "Assets/Shaders/Temp/model.frag");

	// Load model & create a REntity
	LoadModel_AsREntities_P3N3Uv2("Assets/Models/Soldier.glb", RenderObjProgram, glm::vec3(-1,0,-1));

	// Debug
	ValidateREntityArrayAlignment();

	// Init frame context (instance VBO etc.)
	InitFrameContext(GFrame);

	Log("Resources Successfully loaded.");
}


export void RenderLoop()
{
	gLastTime = glfwGetTime();
	Log("Starting Render Loop...");
	while (glfwWindowShouldClose(MainWindow) == false)
	{
		// Input is registered at beginning of frame.
		FrameInputReset(MainWindow);

		glfwPollEvents();

		// Update delta time
		double now = glfwGetTime();
		float dt = (float)(now - gLastTime);
		gLastTime = now;

		// Update camera
		UpdateEditorCamera(GEditorCam, MainWindow, dt, 1920, 1080);
		GCamera.view = GEditorCam.view;
		// #TODO: When Scene View viewport with imgui is implemented, update proj from that viewport's size.
		GCamera.proj = GEditorCam.proj;

		Render();
	}
}

void Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GFrame.Clear();

	// RendererPipeline Steps
	UpdateTransformsAndViewFrustrum(GFrame); // 1 
	CheckVisibility(GFrame); // 2
	BuildRenderItems(GFrame); // 3 
	SortAndBatch(GFrame); // 4
	ExecuteCommands(GFrame); // 5 

	glfwSwapBuffers(MainWindow);
}

export void CleanUpAndShutdown()
{
	Log("Renderer Shutdown Initiated...");

	glfwDestroyWindow(MainWindow);
	glfwTerminate();

	Log("Renderer Shutdown Successful.");
}