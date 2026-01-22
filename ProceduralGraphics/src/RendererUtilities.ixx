module;

#include <glew.h>
#include <string>
#include <vector>
#include <fstream>
#include <assert.h>

/// <summary>
/// Has all the utility functions that are used in various parts of the renderer codebase.
/// </summary>
export module RendererUtilities;
import DebugUtilities;

// Function prototypes
auto CreateShader(GLenum _shaderType, const char* _shaderName) -> GLuint;
auto ReadShaderFile(const char* _filename) -> std::string;
void PrintErrorDetails(bool _isShader, GLuint _id, const char* _name);

// Loads the shaders to a opengl program and returns it
export auto LoadShaderProgram(const char* _vertexShaderPath, const char* _fragmentShaderPath) -> GLuint
{
	std::string programName = std::string(_vertexShaderPath) + " + " + _fragmentShaderPath;

	// Create the shaders from the filepath
	GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, _vertexShaderPath);
	GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, _fragmentShaderPath);

	if (vertexShader == 0 || fragmentShader == 0)
	{
		std::string msg = "Shader failed - skipping program: ";
		msg += programName;
		LogWarning(msg.c_str());
		if (vertexShader) glDeleteShader(vertexShader);
		if (fragmentShader) glDeleteShader(fragmentShader);
		return 0;
	}

	GLuint program = glCreateProgram();

	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	glLinkProgram(program);

	// Check for link errors
	GLint link_result = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_result);

	GLint logLen = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

	if (logLen > 1) {
		std::string linkLog(logLen, '\0');
		glGetProgramInfoLog(program, logLen, nullptr, linkLog.data());
		Log(("PROGRAM LINK LOG:\n" + linkLog).c_str());
	}

	if (link_result == GL_FALSE) {
		LogWarning("Program link failed");
		return 0;
	}

	std::string successLogMessage = "Successfully Loaded Shader: ";
	successLogMessage += programName;

	Log(successLogMessage.c_str());
	return program;
}

// Creates the shader program, based on type, #TODO edit this when creating modular shaders
GLuint CreateShader(GLenum _shaderType, const char* _shaderName)
{
	std::string shaderSrc = "#version 460 core\n\n";
	//Log(("Shader size: " + std::to_string(shaderSrc.size())).c_str());

	//// =========== This automatically adds the lighting logic to every fragment shader =========== 
	// #TODO, move this to a proper system that will assign any additional module shader 
	// functions 
	if (_shaderType == GL_FRAGMENT_SHADER)
	{
		shaderSrc += ReadShaderFile("Assets/Shaders/Common/lighting.glsl");
		shaderSrc += "\n\n";
	}

	//// Read the shader files and save the source code as strings
	std::string var = ReadShaderFile(_shaderName);
	//Log(("Shader size: " + std::to_string(shaderSrc.size())).c_str());
	shaderSrc.append(var);
	//Log(("Shader size: " + std::to_string(shaderSrc.size())).c_str());

	/*auto nul = shaderSrc.find('\0');
	if (nul != std::string::npos) {
		Log(("Found NUL in shaderSrc at byte " + std::to_string(nul)).c_str());
	}*/
	//// ========= ^^This automatically adds the lighting logic to every fragment shader^^ ========= 
	 
	// Create the shader ID and create pointers for source code string and length
	GLuint shaderID = glCreateShader(_shaderType);
	GLsizei count = 1;
	const GLchar* string = shaderSrc.c_str();
	GLint length = static_cast<GLint>(shaderSrc.size());

	//{
	//	// Count #version occurrences
	//	size_t versions = 0;
	//	size_t pos = 0;
	//	while ((pos = shaderSrc.find("#version", pos)) != std::string::npos) { versions++; pos += 8; }

	//	// Check for main()
	//	bool hasMain = (shaderSrc.find("void main(") != std::string::npos) || (shaderSrc.find("void main") != std::string::npos);

	//	Log(("Diagnostic: shader length = " + std::to_string(shaderSrc.size())).c_str());
	//	Log(("Diagnostic: #version count = " + std::to_string(versions)).c_str());
	//	Log((std::string("Diagnostic: hasMain = ") + (hasMain ? "true" : "false")).c_str());

	//	// Dump the entire shader to disk for manual inspection
	//	std::ofstream out("debug_shader_dump.glsl", std::ios::binary);
	//	out << shaderSrc;
	//	out.close();
	//	Log("Wrote debug_shader_dump.glsl to executable directory.");
	//}

	// Populate the shader Object (ID) and compile
	glShaderSource(shaderID, 1, &string, &length);
	glCompileShader(shaderID);

	/*GLint logLen = 0;
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 1) {
		std::string compileLog(logLen, '\0');
		glGetShaderInfoLog(shaderID, logLen, nullptr, compileLog.data());
		Log(("Shader compile log:\n" + compileLog).c_str());
	}
	else {
		Log("Shader compile log: <empty>");
	}*/

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

// Reads files in binary mode
auto ReadShaderFile(const char* _filename) -> std::string
{
	// Open the file for reading
	std::ifstream file(_filename, std::ios::binary); // <- binary mode (removes some stuff the os does when parsing)

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

	std::string shaderCode((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	/*Log((std::string("Loaded shader file: ") + _filename +
		" (" + std::to_string(shaderCode.size()) + " bytes)").c_str());*/
	/*std::string success = "Loaded shader file: " + std::string(_filename) +
		" (" + std::to_string(size) + " bytes)";
	Log(success.c_str());*/

	return shaderCode;
}

// Structures a error output for debugging of shader purposes
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

