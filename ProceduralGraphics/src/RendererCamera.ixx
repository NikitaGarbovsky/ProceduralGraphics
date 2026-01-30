module;

#include <glm.hpp>
#include <ext/matrix_transform.hpp>
#include <ext/matrix_clip_space.hpp>
#include <cstdint>
#include <glfw3.h>

// This module manages the main rendering camera that is controlled through the viewport.
export module RendererCamera;

import RendererInput;
import RendererData;

export enum class CameraMode : uint8_t { Free, Orbit, Ortho }; // #TODO future camera modes. 

// Simple camera data currently set in Load Resources.
export struct CameraData {
    glm::mat4 view{ 1.0f };
    glm::mat4 proj{ 1.0f };
};

export CameraData GCamera; // Active camera for the current frame/pass.

// Camera data that is needed throughout camera manipulation
export struct CameraRig
{
    CameraMode mode = CameraMode::Free;

    glm::vec3 position{ 0, 1.5f, 4.0f };
    float yawDeg = -90.0f; // looking toward -Z initially
    float pitchDeg = 0.0f;

    float fovDeg = 90.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float moveSpeed = 6.0f;
    float fastMultiplier = 5.0f;
    float mouseSensitivity = 0.12f; // deg per pixel

    // Derived:
    glm::vec3 forward{ 0,0,-1 };
    glm::vec3 right{ 1,0,0 };
    glm::vec3 up{ 0,1,0 };

    glm::mat4 view{ 1.0f };
    glm::mat4 proj{ 1.0f };
};

export CameraRig GEditorCam;

// Helper to compute the forward vector of the camera
static glm::vec3 ComputeForward(float _yawDeg, float _pitchDeg)
{
    float yaw = glm::radians(_yawDeg);
    float pitch = glm::radians(_pitchDeg);

    glm::vec3 f;
    f.x = std::cos(yaw) * std::cos(pitch);
    f.y = std::sin(pitch);
    f.z = std::sin(yaw) * std::cos(pitch);
    return glm::normalize(f);
}

// Sets the camera to perspective mode 
export void SetPerspective(CameraRig& _camrig, float _fovDeg, float _aspect, float _nearP, float _farP)
{
    _camrig.fovDeg = _fovDeg;
    _camrig.nearPlane = _nearP;
    _camrig.farPlane = _farP;
    _camrig.proj = glm::perspective(glm::radians(_camrig.fovDeg), _aspect, _camrig.nearPlane, _camrig.farPlane);
}

// Captures the cursor when right click & holding.
static void SetCursorCaptured(GLFWwindow* _window, bool _captured)
{
    glfwSetInputMode(_window, GLFW_CURSOR, _captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // (If supported) raw mouse motion feels better for FPS-style look
    if (_captured && glfwRawMouseMotionSupported())
        glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    GInput.cursorCaptured = _captured;
    GInput.firstMouseSample = true; // Prevents jump on transition
}

// Called per frame before rendering, updates the camera.
export void UpdateEditorCamera(CameraRig& _camrig, GLFWwindow* _window, float _dt, int _viewportW, int _viewportH)
{
    // Look Input
    if (GInput.cursorCaptured)
    {
        _camrig.yawDeg += (float)GInput.mouseDX * _camrig.mouseSensitivity;
        _camrig.pitchDeg -= (float)GInput.mouseDY * _camrig.mouseSensitivity; // invert so moving mouse up looks up
        _camrig.pitchDeg = glm::clamp(_camrig.pitchDeg, -89.0f, 89.0f); // Clamp to avoid gimbal lock
    }

    // Compute all the necessary camera vectors.
    _camrig.forward = ComputeForward(_camrig.yawDeg, _camrig.pitchDeg);
    const glm::vec3 worldUp(0, 1, 0);
    _camrig.right = glm::normalize(glm::cross(_camrig.forward, worldUp));
    _camrig.up = glm::normalize(glm::cross(_camrig.right, _camrig.forward));

    // Hold RMB to take control of camera, WASD moves while RMB held.
    if (MousePressed(GLFW_MOUSE_BUTTON_RIGHT))
        SetCursorCaptured(_window, true);
    if (MouseReleased(GLFW_MOUSE_BUTTON_RIGHT))
        SetCursorCaptured(_window, false);

    // Move
    glm::vec3 move(0.0f);
    if (GInput.cursorCaptured)
    {
        if (KeyDown(GLFW_KEY_W)) move += _camrig.forward; 
        if (KeyDown(GLFW_KEY_S)) move -= _camrig.forward;
        if (KeyDown(GLFW_KEY_D)) move += _camrig.right;
        if (KeyDown(GLFW_KEY_A)) move -= _camrig.right;
        if (KeyDown(GLFW_KEY_E)) move += worldUp;
        if (KeyDown(GLFW_KEY_Q)) move -= worldUp;
        
        if (glm::length(move) > 0.0f) {
            GInput.currentlyMoving = true;
            move = glm::normalize(move);
            float speed = _camrig.moveSpeed;
            if (KeyDown(GLFW_KEY_LEFT_SHIFT)) speed *= _camrig.fastMultiplier;
            _camrig.position += move * speed * _dt;
        }
        else {
            GInput.currentlyMoving = false;
        }
    }

    // View
    _camrig.view = glm::lookAt(_camrig.position, _camrig.position + _camrig.forward, worldUp);

    // Mouse wheel speed ramp #TODO add this as a toast popup when it changes.
    if (GInput.scrollY != 0.0f)
        _camrig.moveSpeed = glm::clamp(_camrig.moveSpeed * (1.0f + GInput.scrollY * 0.1f), 0.1f, 200.0f);
}
