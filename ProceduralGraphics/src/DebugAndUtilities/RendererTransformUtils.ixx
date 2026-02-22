module;

#include <glm.hpp>
#include <glfw3.h>

// Probably should move more stuff into here as needed.
// Helpers mainly.
export module RendererTransformUtils;

import <gtc/matrix_transform.hpp>;
import <gtc/type_ptr.hpp>;

export glm::mat4 ComposeTRSMatrix(const glm::vec3& _position, const glm::vec3& _rotationD, const glm::vec3& _scale)
{
    glm::mat4 M(1.0f);
    M = glm::translate(M, _position);
    M = glm::rotate(M, glm::radians(_rotationD.z), { 0,0,1 });
    M = glm::rotate(M, glm::radians(_rotationD.y), { 0,1,0 });
    M = glm::rotate(M, glm::radians(_rotationD.x), { 1,0,0 });
    M = glm::scale(M, _scale);
    return M;
}

export bool SphereInFrustum(const glm::vec3& _center, float _radius, const glm::vec4 _planes[6])
{
    for (int i = 0; i < 6; i++) {
        const glm::vec4& p = _planes[i];
        float dist = p.x * _center.x + p.y * _center.y + p.z * _center.z + p.w;
        if (dist < -_radius) return false; // completely outside this plane
    }
    return true;
}

export void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 outPlanes[6]) {
	// Build rows from column-major matrix (glm::mat4 is column-major)
	const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
	const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
	const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
	const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

	// Planes: (a,b,c,d) with ax + by + cz + d = 0
	outPlanes[0] = row3 + row0; // Left
	outPlanes[1] = row3 - row0; // Right
	outPlanes[2] = row3 + row1; // Bottom
	outPlanes[3] = row3 - row1; // Top
	outPlanes[4] = row3 + row2; // Near  
	outPlanes[5] = row3 - row2; // Far

	// Normalize planes (so distance tests are correct)
	for (int i = 0; i < 6; i++) {
		glm::vec3 n(outPlanes[i]);
		float len = glm::length(n);
		if (len > 0.0f) outPlanes[i] /= len;
	}
}

// NOT USED CURRENTLY, #TODO: maybe use this when you've implemented the infinite grid 
// + drag and drop so its easy to spawn new objects into the world.
export bool GetCursorWorldOnPlaneY0(GLFWwindow* _window,
    const glm::mat4& _view,
    const glm::mat4& _proj,
    glm::vec3& _outPos)
{
    // Mouse in window coords
    double mx, my;
    glfwGetCursorPos(_window, &mx, &my);

    // Convert to framebuffer pixels (handles DPI scaling)
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(_window, &winW, &winH);
    glfwGetFramebufferSize(_window, &fbW, &fbH);

    double sx = (winW > 0) ? (double)fbW / (double)winW : 1.0;
    double sy = (winH > 0) ? (double)fbH / (double)winH : 1.0;

    double px = mx * sx;
    double py = my * sy;

    // NDC: x in [-1,1], y in [-1,1] with y up
    float x = (float)((px / (double)fbW) * 2.0 - 1.0);
    float y = (float)(1.0 - (py / (double)fbH) * 2.0);

    glm::mat4 invVP = glm::inverse(_proj * _view);

    glm::vec4 nearClip(x, y, -1.0f, 1.0f);
    glm::vec4 farClip(x, y, 1.0f, 1.0f);

    glm::vec4 nearW = invVP * nearClip; nearW /= nearW.w;
    glm::vec4 farW = invVP * farClip;  farW /= farW.w;

    glm::vec3 rayO = glm::vec3(nearW);
    glm::vec3 rayD = glm::normalize(glm::vec3(farW - nearW));

    // Intersect with plane y = 0
    const float denom = rayD.y;
    if (std::abs(denom) < 1e-6f) return false; // parallel

    float t = (0.0f - rayO.y) / denom;
    if (t < 0.0f) return false; // behind camera

    _outPos = rayO + rayD * t;
    return true;
}