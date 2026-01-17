module;

// Normal imports
#include <glew.h>
#include <glfw3.h>
#include <glm.hpp>
#include <ext/matrix_clip_space.hpp>

export module RendererCore;

// Codebase module imports
import DebugUtilities;
import RendererData;
import RendererUtilities;
import RendererPipeline;
import RendererEntitys;
import RendererFrame;
import RendererAssetPipeline;

// Function prototypes
void Render();
void LoadResources();

// Global accessible info for the renderer pipeline
static FrameContext GFrame;

export bool InitRenderer()
{
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

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, 1920, 1080);

	// Renderer configuration
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	//glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Make cursor disabled upon startup.

	Log("Renderer Succussfully Initialized");
	
	LoadResources();
	return true;
}

export void LoadResources()
{
	Log("Loading Resources...");

	// --- Set a simple camera ---
	// Identity view, ortho projection covering [-1,1]
	GCamera.view = glm::mat4(1.0f);
	GCamera.proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);

	// Program (shader resource)
	RenderObjProgram = LoadShaderProgram("Assets/Shaders/Temp/quad.vert", "Assets/Shaders/Temp/quad.frag");

	// Create one material for the loaded program (holds shader, texture information)
	MaterialID quadMat = CreateMaterial(RenderObjProgram, 0);

	// Demo, create a basic quad mesh.
	MeshID quadMesh = CreateQuadMesh();

	CreateRenderEntity(quadMesh, quadMat, glm::vec3(0, 0, 0));

	// Debug
	ValidateREntityArrayAlignment();

	// Init frame context (instance VBO etc.)
	InitFrameContext(GFrame);

	Log("Resources finished loading.");
}

export void RenderLoop()
{
	Log("Starting Render Loop...");
	while (glfwWindowShouldClose(MainWindow) == false)
	{
		glfwPollEvents();

		Render();
	}
}

void Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GFrame.Clear();

	// RendererPipeline Steps
	UpdateTransformsAndViewFrustrum(GFrame); // 1 

	/*size_t value1 = CurrentRenderedEntitys.size();
	std::string str1 = "Current rendereable entities: ";
	str1 += std::to_string(value1);
	const char* c_str1 = str1.c_str();
	Log(c_str1);*/

	CheckVisibility(GFrame); // 2

	/*size_t value2 = GFrame.visible.size();
	std::string str = "After Visibility Test: ";
	str += std::to_string(value2);
	const char* c_str2 = str.c_str();
	Log(c_str2);*/

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