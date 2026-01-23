module;

#include <glfw3.h>
#include <array>
#include <cstdint>
#include <assert.h>

/// <summary>
/// This module mangages the input across the renderer. It contains the global input state (InputState),
/// callback functions that when intialized, acts as an interface layer for a callback to interact with the
/// global input state and mutate it. 
/// 
/// Press button -> Callback executed -> GInput updated
/// 
/// To query input:
/// if (KeyDown/KeyPressed/KeyReleased) etc 
/// </summary> 
export module RendererInput;

// Pointer to the main window so we dont have to 
static GLFWwindow* GWindow = nullptr;

// Holds all the necessary global input state data that we need to access to facilitate input.
struct InputState {
	std::array<uint8_t, GLFW_KEY_LAST + 1> keyDown{};
	std::array<uint8_t, GLFW_KEY_LAST + 1> keyPressed{};
	std::array<uint8_t, GLFW_KEY_LAST + 1> keyReleased{};

	std::array<uint8_t, GLFW_MOUSE_BUTTON_LAST + 1> mouseDown{};
	std::array<uint8_t, GLFW_MOUSE_BUTTON_LAST + 1> mousePressed{};
	std::array<uint8_t, GLFW_MOUSE_BUTTON_LAST + 1> mouseReleased{};

	double mouseX = 0.0, mouseY = 0.0;   // mouse pos x/y
	double mouseDX = 0.0, mouseDY = 0.0; // per-frame delta
	float scrollY = 0.0f;				 // per-frame scroll

	bool cursorCaptured = false;
	bool firstMouseSample = true;		 // avoids jump on capture
};

// Single global input state
export InputState GInput;

// Callback helper to update global input data, executed when a key is pressed.
static void KeyCallback(GLFWwindow*, int _key, int, int _action, int)
{
    if (_key < 0 || _key > GLFW_KEY_LAST) return;

    if (_action == GLFW_PRESS) 
    {
        if (!GInput.keyDown[_key]) GInput.keyPressed[_key] = 1;
        GInput.keyDown[_key] = 1;
    }
    else if (_action == GLFW_RELEASE)
    {
        GInput.keyDown[_key] = 0;
        GInput.keyReleased[_key] = 1;
    }
}

// Callback helper to update global input data, executed when a mouse button is pressed.
static void MouseButtonCallback(GLFWwindow*, int button, int action, int)
{
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return;

    if (action == GLFW_PRESS)
    {
        if (!GInput.mouseDown[button]) GInput.mousePressed[button] = 1;
        GInput.mouseDown[button] = 1;
    }
    else if (action == GLFW_RELEASE)
    {
        GInput.mouseDown[button] = 0;
        GInput.mouseReleased[button] = 1;
    }
}

// Callback helper to update global input data, executed when the cursor is moved.
static void CursorPosCallback(GLFWwindow*, double _x, double _y)
{
    if (GInput.firstMouseSample)
    {
        GInput.mouseX = _x;
        GInput.mouseY = _y;
        GInput.mouseDX = 0.0;
        GInput.mouseDY = 0.0;
        GInput.firstMouseSample = false;
        return;
    }

    GInput.mouseDX += (_x - GInput.mouseX);
    GInput.mouseDY += (_y - GInput.mouseY);
    GInput.mouseX = _x;
    GInput.mouseY = _y;
}

// Callback helper to update global input data, executed when the mouse scroll is used. 
static void ScrollCallback(GLFWwindow*, double /*xoff*/, double yoff)
{
    GInput.scrollY += (float)yoff;
}

// Initializes input for the renderer.
export void InitInput(GLFWwindow* _window)
{
    GWindow = _window;
    assert(GWindow);

    // Initialize all the callbacks.
    glfwSetKeyCallback(GWindow, KeyCallback);
    glfwSetMouseButtonCallback(GWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(GWindow, CursorPosCallback);
    glfwSetScrollCallback(GWindow, ScrollCallback);
}

// Resets some necessary input at the beginning of the frame.
// Must be called once per frame before glfwPollEvents() to clear one-frame edges / deltas
export void FrameInputReset(GLFWwindow* _window) // Window is not used yet
{
    (void)_window;

    // Clear transition states 
    for (auto& v : GInput.keyPressed)  v = 0;
    for (auto& v : GInput.keyReleased) v = 0;
    for (auto& v : GInput.mousePressed) v = 0;
    for (auto& v : GInput.mouseReleased) v = 0;

    // Per-frame deltas
    GInput.mouseDX = 0.0;
    GInput.mouseDY = 0.0;
    GInput.scrollY = 0.0f;
}

// Keyboard Polling functions
export bool KeyDown(int _key) { return (_key >= 0 && _key <= GLFW_KEY_LAST) ? GInput.keyDown[_key] != 0 : false; }
export bool KeyPressed(int _key) { return (_key >= 0 && _key <= GLFW_KEY_LAST) ? GInput.keyPressed[_key] != 0 : false; }
export bool KeyReleased(int _key) { return (_key >= 0 && _key <= GLFW_KEY_LAST) ? GInput.keyReleased[_key] != 0 : false; }

// Mouse Polling functions
export bool MouseDown(int _b) { return (_b >= 0 && _b <= GLFW_MOUSE_BUTTON_LAST) ? GInput.mouseDown[_b] != 0 : false; }
export bool MousePressed(int _b) { return (_b >= 0 && _b <= GLFW_MOUSE_BUTTON_LAST) ? GInput.mousePressed[_b] != 0 : false; }
export bool MouseReleased(int _b) { return (_b >= 0 && _b <= GLFW_MOUSE_BUTTON_LAST) ? GInput.mouseReleased[_b] != 0 : false; }