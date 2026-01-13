module;

#include <glew.h>
#include <glfw3.h>
#include <filesystem>

export module RendererCore;

import DebugUtilities;
import RendererData;
import RendererUtilities;

// Function prototypes
void Update();
void Render();
void LoadResources();

export bool InitRenderer()
{
	Log("Initializing Renderer...");

	// Initialize glfw & the main window
	glfwInit();
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
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

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glViewport(0, 0, 1920, 1080);

	Log("Renderer Succussfully Initialized");
	
	LoadResources();
	return true;
}

export void LoadResources()
{
	Log("Loading Resources...");
	Log(std::filesystem::current_path().string().c_str());

	// Create shader programs
	RenderObjProgram = LoadShaderProgram("Assets/Shaders/TextureNormPos.vert", "Assets/Shaders/lit.frag");

	Log("Resources finished loading.");
}

export void RenderLoop()
{
	// Renderer configuration
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	//glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Make cursor disabled upon startup.

	while (glfwWindowShouldClose(MainWindow) == false)
	{
		Update();

		Render();
	}
}

export void CleanUpAndShutdown()
{
	glfwDestroyWindow(MainWindow);
	glfwTerminate();
}

// Private helpers
void Update()
{
	glfwPollEvents();
}

void Render()
{
	glClear(GL_COLOR_BUFFER_BIT);

	glfwSwapBuffers(MainWindow);
}