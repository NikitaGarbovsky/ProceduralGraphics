#include <vector>
#include "glm.hpp"
#include <glew.h>
#include <string>
#include <stb_image.h>
#include <gtc/type_ptr.hpp>

/// <summary>
/// Simple skybox nothing fancy.
/// </summary>
export module RendererSkybox;

import RendererCamera;
import RendererUtilities;

GLuint skyboxProgram = -1; // Skybox shader program.

GLuint skyboxVAO = -1;
GLuint skyboxVBO = -1;
GLuint skyboxEBO = -1;

int indiciesSize = -1;

GLuint TextureID = -1;

// #TODO: Probably need to put this in a dedicated place.
void CreateTextureCubeMap(std::vector<std::string> _filePaths) {
	glGenTextures(1, &TextureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, TextureID);
	stbi_set_flip_vertically_on_load(false);

	// Storage variables
	int ImageWidth;
	int ImageHeight;
	int ImageComponents;

	for (int i = 0; i < 6; ++i)
	{
		// Load each image separately
		unsigned char* ImageData = stbi_load(_filePaths[i].c_str(),
			&ImageWidth, &ImageHeight, &ImageComponents, 0);

		// Cubemap should be full color (RGB). Image might come with Alpha.
		GLint LoadedComponents = (ImageComponents == 4) ? GL_RGBA : GL_RGB;

		// Populate the texture with the image data
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0, LoadedComponents, ImageWidth, ImageHeight, 0,
			LoadedComponents, GL_UNSIGNED_BYTE, ImageData);

		// Free each image data after creating the OpenGL texture
		stbi_image_free(ImageData);
	}

	// Setting the address mode for this texture (once for the whole cube map)
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Generate the mipmaps and unbind the texture
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

export void InitSkybox() {

	// Vertices creation
	std::vector<glm::vec3> Vertices =
	{
		glm::vec3(-1.0f,  1.0f,  1.0f),
		glm::vec3(-1.0f, -1.0f,  1.0f),
		glm::vec3(1.0f, -1.0f,  1.0f),
		glm::vec3(1.0f,  1.0f,  1.0f),
		glm::vec3(-1.0f,  1.0f, -1.0f),
		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(1.0f, -1.0f, -1.0f),
		glm::vec3(1.0f,  1.0f, -1.0f),
	};

	// Indices creation
	std::vector<GLuint> Indices =
	{
			0, 2, 1,
			0, 3, 2,

			3, 6, 2,
			3, 7, 6,

			7, 5, 6,
			7, 4, 5,

			4, 1, 5,
			4, 0, 1,

			4, 3, 0,
			4, 7, 3,

			1, 6, 5,
			1, 2, 6,
	};

	skyboxProgram = LoadShaderProgram("Assets/Shaders/Common/Skybox.vert", "Assets/Shaders/Common/Skybox.frag");

	// Create the VAO, VBO and EBO
	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)  * Vertices.size(), Vertices.data(), GL_STATIC_DRAW);
	glGenBuffers(1, &skyboxEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * Indices.size(), Indices.data(), GL_STATIC_DRAW);

	indiciesSize = (GLuint)Indices.size();

	// Vertex Information (Position)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		(sizeof(float) * 3),
		(void*)0);
	glEnableVertexAttribArray(0);

	std::string Filepath = "Assets/Skybox/";
	std::vector<std::string> TextureStrings =
	{
		Filepath + "miramar_ft.tga",
		Filepath + "miramar_bk.tga",
		Filepath + "miramar_up.tga",
		Filepath + "miramar_dn.tga",
		Filepath + "miramar_rt.tga",
		Filepath + "miramar_lf.tga",
	};

	CreateTextureCubeMap(TextureStrings);
}

export void RenderSkybox() {
	glUseProgram(skyboxProgram);

	// Bind the Skybox Texture as a cube map
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, TextureID);
	glUniform1i(glGetUniformLocation(skyboxProgram, "Texture_Skybox"), 0);

	// Setup the camera matrices
	glm::mat4 viewNoTrans = glm::mat4(glm::mat3(GCamera.view));
	glm::mat4 CamMatProj = GCamera.proj;
	glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "VP"), 1, GL_FALSE, glm::value_ptr(CamMatProj * viewNoTrans));

	// Render the skybox mesh
	glBindVertexArray(skyboxVAO);
	glDepthFunc(GL_LEQUAL);
	glDrawElements(GL_TRIANGLES, indiciesSize, GL_UNSIGNED_INT, 0);
	glDepthFunc(GL_LESS);

	// Unbind all objects
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glUseProgram(0);
}