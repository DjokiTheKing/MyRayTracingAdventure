#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class ComputeShader {
public:
	ComputeShader() = default;
	ComputeShader(const std::string& compute_shader_path);
	~ComputeShader();

	void use();

	// singles
	void setBool(const std::string& name, bool value) const;
	void setFloat(const std::string& name, float value) const;
	void setInt(const std::string& name, int value) const;
	void setUnsignedInt(const std::string& name, unsigned int value) const;

	// vec 2
	void setFloat2(const std::string& name, float value0, float value1) const;
	void setFloat2(const std::string& name, const glm::vec2& value) const;

	// vec 3 
	void setFloat3(const std::string& name, const glm::vec3& value) const;

	// vec 4 
	void setFloat4(const std::string& name, const glm::vec4& value) const;

	// matrix 4
	void setMat4(const std::string& name, const glm::mat4& value) const;
private:
	unsigned int PID;
};

#endif // !COMPUTE_SHADER_H
