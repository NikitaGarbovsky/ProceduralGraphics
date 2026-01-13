module;

#include <glew.h>
#include <string>
#include <vector>
#include <fstream>

export module RendererUtilities;
import DebugUtilities;

// Function prototypes
auto CreateShader(GLenum _shaderType, const char* _shaderName) -> GLuint;
auto ReadShaderFile(const char* _filename) -> std::string;
void PrintErrorDetails(bool _isShader, GLuint _id, const char* _name);

export auto LoadShaderProgram(const char* _vertexShaderPath, const char* _fragmentShaderPath) -> GLuint
{
	// Create the shaders from the filepath
	GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, _vertexShaderPath);
	GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, _fragmentShaderPath);

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	// Check for link errors
	int link_result = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &link_result);
	if (link_result == GL_FALSE)
	{
		std::string programName = _vertexShaderPath + *_fragmentShaderPath;
		PrintErrorDetails(false, program, programName.c_str());
		return 0;
	}
}

GLuint CreateShader(GLenum _shaderType, const char* _shaderName)
{
	// Read the shader files and save the source code as strings
	std::string sShader = ReadShaderFile(_shaderName);

	// Create the shader ID and create pointers for source code string and length
	GLuint shaderID = glCreateShader(_shaderType);
	GLsizei count = 1;
	const GLchar* string = sShader.c_str();
	GLint length = sShader.size();

	// Populate the shader Object (ID) and compile
	glShaderSource(shaderID, 1, &string, &length);
	glCompileShader(shaderID);

	// Check for errors
	int compile_result = 0;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compile_result);
	if (compile_result == GL_FALSE)
	{
		PrintErrorDetails(true, shaderID, _shaderName);
		return 0;
	}

	return shaderID;
}

auto ReadShaderFile(const char* _filename) -> std::string
{
	// Open the file for reading
	std::ifstream file(_filename, std::ios::in);
	std::string shaderCode;

	// Ensure the file is open and readable
	if (!file.good()) {
		
		// #TODO maybe change this std::string usage to be the future arena allocators. 

		// Create the error string
		std::string mainString = "Cannot read file: ";
		mainString.append(_filename);

		// Log the error as a warning
		LogWarning(mainString.c_str()); 

		return "";
	}

	// Determine the size of the file in characters and resize the string variable to accomodate
	file.seekg(0, std::ios::end);
	shaderCode.resize((unsigned int)file.tellg());

	// Set the position of the next character to be read back to the beginning
	file.seekg(0, std::ios::beg);
	// Extract the contents of the file and store in the string variable
	file.read(&shaderCode[0], shaderCode.size());
	file.close();
	return shaderCode;
}
void PrintErrorDetails(bool _isShader, GLuint _id, const char* _name)
{
	int infoLogLength = 0;
	// Retrieve the length of characters needed to contain the info log
	(_isShader == true) ? glGetShaderiv(_id, GL_INFO_LOG_LENGTH, &infoLogLength) : glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &infoLogLength);
	std::vector<char> log(infoLogLength);

	// Retrieve the log info and populate log variable
	(_isShader == true) ? glGetShaderInfoLog(_id, infoLogLength, NULL, &log[0]) : glGetProgramInfoLog(_id, infoLogLength, NULL, &log[0]);

	// #TODO maybe change this std::string usage to be the future arena allocators. 

	// Create the error string
	std::string mainString = "Error compiling ";
	mainString.append(((_isShader == true) ? "shader: " : "program: "));
	mainString.append(_name);

	// Log the error as a warning
	LogWarning(mainString.c_str());

	LogWarning(&log[0]);
}

