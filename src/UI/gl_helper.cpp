#include <utility>
#include <string>
#include <stdexcept>

#include <glad/glad.h>

#include "gl_helper.hpp"

/**
 * Shader
 */

Shader::Shader(Shader&& other) noexcept : Shader() {
	*this = std::move(other);
}

Shader::Shader(const char* vert, const char* frag) {
	constexpr GLfloat vertex_buffer_data[] = {
		-1.f,  1.f,
		-1.f, -1.f,
		 1.f, -1.f,

		-1.f,  1.f,
		 1.f, -1.f,
		 1.f,  1.f
	};

	constexpr GLfloat uv_buffer_data[] = {
		0.f, 1.f,
		0.f, 0.f,
		1.f, 0.f,
		0.f, 1.f,
		1.f, 0.f,
		1.f, 1.f
	};

	glGenVertexArrays(1, &vao_id);
	glBindVertexArray(vao_id);

	glGenBuffers(1, &vb_id);
	glBindBuffer(GL_ARRAY_BUFFER, vb_id);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);
	glGenBuffers(1, &uv_id);
	glBindBuffer(GL_ARRAY_BUFFER, uv_id);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data, GL_STATIC_DRAW);

	program = compile_shaders(vert, frag);
}

Shader& Shader::operator=(Shader&& other) noexcept {
	std::swap(vao_id, other.vao_id);
	std::swap(uv_id, other.uv_id);
	std::swap(vb_id, other.vb_id);
	std::swap(program, other.program);
	return *this;
}

Shader::operator bool() const { return program; }

void Shader::set_float(const std::string& name, float val) {
	glUniform1f(glGetUniformLocation(program, name.c_str()), val);
}
void Shader::set_vec_float(const std::string& name, float v0, float v1) {
	glUniform2f(glGetUniformLocation(program, name.c_str()), v0, v1);
}
void Shader::set_int(const std::string& name, int val) {
	glUniform1i(glGetUniformLocation(program, name.c_str()), val);
}
void Shader::set_texture(const std::string& name, GLenum tex) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(glGetUniformLocation(program, name.data()), 0);
}

void Shader::use() {
	glUseProgram(program);
}

void Shader::draw() {
	glBindVertexArray(vao_id);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vb_id);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv_id);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

Shader::~Shader() {
	glDeleteBuffers(1, &vb_id);
	glDeleteBuffers(1, &uv_id);
	glDeleteVertexArrays(1, &vao_id);
	glDeleteProgram(program);
}

GLuint Shader::compile_shaders(const char* vert, const char* frag) {
	GLuint vert_shader_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	GLint result = GL_FALSE;
	int info_log_length;
	glShaderSource(vert_shader_id, 1, &vert, nullptr);
	glCompileShader(vert_shader_id);

	glGetShaderiv(vert_shader_id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(vert_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		std::string info_log(info_log_length, ' ');
		glGetShaderInfoLog(vert_shader_id, info_log_length, nullptr, info_log.data());
		throw std::runtime_error("Failed to compile vertex shader!:\n" + info_log);
	}

	glShaderSource(frag_shader_id, 1, &frag, nullptr);
	glCompileShader(frag_shader_id);

	glGetShaderiv(frag_shader_id, GL_COMPILE_STATUS, &result);
	glGetShaderiv(frag_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		std::string info_log(info_log_length, ' ');
		glGetShaderInfoLog(frag_shader_id, info_log_length, nullptr, info_log.data());
		throw std::runtime_error("Failed to compile fragment shader!:\n" + info_log);
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vert_shader_id);
	glAttachShader(program, frag_shader_id);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &result);
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		std::string info_log(info_log_length, ' ');
		glGetProgramInfoLog(program, info_log_length, nullptr, info_log.data());
		throw std::runtime_error("Failed to compile vertex shader!:\n" + info_log);
	}

	glDetachShader(program, vert_shader_id);
	glDetachShader(program, frag_shader_id);

	glDeleteShader(vert_shader_id);
	glDeleteShader(frag_shader_id);

	return program;
}
