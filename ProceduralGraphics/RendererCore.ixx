module;

#include <glew.h>
#include <glfw3.h>

export module RendererCore;

import DebugUtilities;

// Globals to utilize that are core of the renderer system.
GLFWwindow* MainWindow;

// Function headers.
void Update();
void Render();
void LoadResources();

export bool InitRenderer()
{
	Log("Initializing Renderer...");

	glfwInit();
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	MainWindow = glfwCreateWindow(1920, 1080, "Procedural Graphics", NULL, NULL);
	if (MainWindow == NULL)
	{
		Log("!!!!!!!!GLFW failed to initialize properly!!!!!!!!!!!");

		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(MainWindow);

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

	glViewport(0, 0, 1920, 1080);

	Log("Renderer Succussfully Initialized");
	
	LoadResources();
	return true;
}

export void LoadResources()
{
	Log("Loading Resources...");

	Log("Resources Successfully Loaded");
}
export void RenderLoop()
{
	while (glfwWindowShouldClose(MainWindow) == false)
	{
		Update();

		Render();
	}
}

export void TerminateAndCleanUp()
{
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