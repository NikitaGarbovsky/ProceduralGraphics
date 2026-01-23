module;
#include <glfw3.h>
#include <glm.hpp>

/// <summary> Holds global Renderer data and references. </summary>
export module RendererData;

// Globals to utilize that are core of the Renderer.
export GLFWwindow* MainWindow;
export GLuint RenderObjProgram; 

// Holds the time 
export double gLastTime = 0.0;

// Simple camera data currently set in Load Resources.
// TODO# Probably move this when you need to make a better camera system.
export struct CameraData {
	glm::mat4 view{ 1.0f };
	glm::mat4 proj{ 1.0f };
};

export CameraData GCamera; // Active camera for the current frame/pass.