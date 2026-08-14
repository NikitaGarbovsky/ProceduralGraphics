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
import RendererPicking;
import RendererLights;
import RendererTransformUtils;
import RendererSkybox;
import RendererScenes;
import RendererPass_PostProcess;

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

	InitRendererPipeline();
	InitSkybox();

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
	SelectedTintProgram = LoadShaderProgram("Assets/Shaders/Temp/selectedTint.vert","Assets/Shaders/Temp/selectedTint.frag");

	// Register all scenes and queue up the starting one. 
	Scenes_RegisterAll(RenderObjProgram);
	Scene_RequestSwitch(0); // 0 = Default Sandbox Scene

	// Debug
	ValidateREntityArrayAlignment();

	Log("Resources Successfully loaded.");
}

export void RenderLoop() {
	Log("Starting Render Loop...");
	while (glfwWindowShouldClose(MainWindow) == false)
	{
		// Rescale perspective based on main window frame buffer size
		// #Todo, make this only execute on a callback.
		int mainWindowfbW, mainWindowfbH;
		glfwGetFramebufferSize(MainWindow, &mainWindowfbW, &mainWindowfbH);
		if (mainWindowfbH > 0)
			SetPerspective(GEditorCam, GEditorCam.fovDeg, (float)mainWindowfbW / (float)mainWindowfbH,
					GEditorCam.nearPlane, GEditorCam.farPlane);

		// Input is registered at beginning of frame.
		FrameInputReset(MainWindow);

		glfwPollEvents();

		if (KeyDown(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(MainWindow, true);
		if (KeyPressed(GLFW_KEY_F2)) EditorUIEnabled = !EditorUIEnabled; // Enable/Disable Editor UI.
		if (KeyPressed(GLFW_KEY_TAB) && !EditorUI_WantsKeyboard())
			PostFX_CycleNext();
		// F3 flips between the normal camera and the narrower game one.
		if (KeyPressed(GLFW_KEY_F3) && !EditorUI_WantsKeyboard())
			GEditorCam.mode = (GEditorCam.mode == CameraMode::Game) ? CameraMode::Free : CameraMode::Game;

		// Scene switching with the number keys while the editor UI is hidden. When the UI is
		// visible the same keys are handled inside EditorUI instead, where they are blocked
		// while typing in a text field. So no double handling. #TODO: this is a temporary solution, figure out a proper one.
		if (!EditorUIEnabled)
		{
			if (KeyPressed(GLFW_KEY_1)) Scene_RequestSwitch(0);
			if (KeyPressed(GLFW_KEY_2)) Scene_RequestSwitch(1);
			if (KeyPressed(GLFW_KEY_3)) Scene_RequestSwitch(2);
			if (KeyPressed(GLFW_KEY_4)) Scene_RequestSwitch(3);
		}

		UpdateTimeData();

		// Apply any pending scene switch at this safe point, then run the scene's own logic.
		// The very first frame applies the startup request and loads the Sandbox here.
		Scene_ApplyPendingSwitch();
		Scene_Update(gDeltaTime);

		// Update camera
		UpdateEditorCamera(GEditorCam, MainWindow, gDeltaTime, mainWindowfbW, mainWindowfbH);
		GCamera.view = GEditorCam.view;
		// #TODO: When Scene View viewport with imgui is implemented, update proj from that viewport's size.
		GCamera.proj = GEditorCam.proj;

		// Draw 3D Scene
		Render();

		// Draw Editor UI #TODO move ui and gizmos to their own dedicated render pass.
		if(EditorUIEnabled)
			EditorUI_Draw(GCamera.view, GCamera.proj, 1920, 1080, SelectedEntity, SelectedLight);

		// #TODO move this to a central input location
		if (MousePressed(GLFW_MOUSE_BUTTON_LEFT) && !EditorUI_WantsMouse() && EditorUIEnabled)
		{
			if (EditorUIIsPlacingLight())
			{
				// Just simply spawn the light directly in front of the camera a fixed distance.
				glm::vec3 spawnPos(GEditorCam.position.x + GEditorCam.forward.x * 0.5, GEditorCam.position.y + GEditorCam.forward.y * 0.5, GEditorCam.position.z + GEditorCam.forward.z * 0.5);

				uint32_t mode = EditorUIGetPlaceLightMode();
				uint32_t newLight = UINT32_MAX;

				if (mode == 1) newLight = (uint32_t)CreatePointLight(spawnPos);
				if (mode == 2) newLight = (uint32_t)CreateDirectionalLight(spawnPos, glm::vec3(0, 0, 0)); // set rot after if you want
				if (mode == 3) newLight = (uint32_t)CreateSpotLight(spawnPos, glm::vec3(0, 0, 0));

				SelectedLight = newLight;
				SelectedEntity = UINT32_MAX;


				EditorUIClearPlaceLightMode();
			}
			else
			{
				PickingRequest(MainWindow);
			}
		}

		glfwSwapBuffers(MainWindow);

		// Consume any picking result that occured during this frame. 
		if (PickingHasResult())
		{
			PickHit hit = PickingConsumeHit();

			if (hit.kind == PickKind::None)
			{
				SelectedEntity = UINT32_MAX;
				SelectedLight = UINT32_MAX;
			}
			else if (hit.kind == PickKind::Entity)
			{
				SelectedEntity = hit.index;
				SelectedLight = UINT32_MAX;
			}
			else // Light
			{
				SelectedLight = hit.index;
				SelectedEntity = UINT32_MAX;
			}
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