module;
#include <glfw3.h>

/// <summary> Holds global Renderer data and references. </summary>
export module RendererData;

// Globals to utilize that are core of the Renderer.
export GLFWwindow* MainWindow;
export GLuint RenderObjProgram; 

// Holds the time 
export double gLastTime = 0.0;