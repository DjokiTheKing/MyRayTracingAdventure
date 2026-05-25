#include "ComputeShader.h"

ComputeShader::ComputeShader(const std::string& compute_shader_path)
{
	int status;
	char info[512];

	std::ifstream compute_file(compute_shader_path);

	if (!compute_file.is_open()) {
		std::cout << "Failed to read shader file: " << compute_shader_path << std::endl;
		PID = -1;
		return;
	}

	std::stringstream compute_stream; compute_stream << compute_file.rdbuf();
	std::string compute_code = compute_stream.str();
	const char* c_compute_code = compute_code.c_str();

	unsigned int compute_shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute_shader, 1, &c_compute_code, nullptr);
	glCompileShader(compute_shader);

	glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(compute_shader, 512, nullptr, info);
		std::cout << "Compute shader compilation failed: " << info << std::endl;
		PID = -1;
		return;
	}

	PID = glCreateProgram();
	glAttachShader(PID, compute_shader);
	glLinkProgram(PID);

	glGetProgramiv(PID, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(PID, 512, nullptr, info);
		std::cout << "Compute program linking failed: " << info << std::endl;
		glDeleteProgram(PID);
		PID = -1;
		return;
	}

	// Cleanup
	glDeleteShader(compute_shader);
}

ComputeShader::~ComputeShader()
{
	glDeleteProgram(PID);
}

void ComputeShader::use()
{
	glUseProgram(PID);
}

void ComputeShader::setBool(const std::string& name, bool value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1i(location, (int)value);
}

void ComputeShader::setFloat(const std::string& name, float value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1f(location, value);
}

void ComputeShader::setInt(const std::string& name, int value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1i(location, value);
}

void ComputeShader::setUnsignedInt(const std::string& name, unsigned int value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1ui(location, value);
}

void ComputeShader::setFloat2(const std::string& name, float value0, float value1) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform2f(location, value0, value1);
}

void ComputeShader::setFloat2(const std::string& name, const glm::vec2& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform2fv(location, 1, glm::value_ptr(value));
}

void ComputeShader::setFloat3(const std::string& name, const glm::vec3& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform3fv(location, 1, glm::value_ptr(value));
}

void ComputeShader::setFloat4(const std::string& name, const glm::vec4& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform4fv(location, 1, glm::value_ptr(value));
}

void ComputeShader::setMat4(const std::string& name, const glm::mat4& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}
