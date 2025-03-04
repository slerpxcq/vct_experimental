#include "GPUProgram.h"

#include "utils.h"

#include <FileWatch.hpp>

#include <cassert>

GPUProgram::~GPUProgram()
{
	for (auto shader : m_shaders) {
		if (shader.id != 0)
			glDeleteShader(shader.id);
	}
	glDeleteProgram(m_id);
}

static GLenum ToGLShaderType(GPUProgram::ShaderType type)
{
	switch (type) {
	case GPUProgram::ShaderType::VERTEX:
		return GL_VERTEX_SHADER;
		break;
	case GPUProgram::ShaderType::GEOMETRY:
		return GL_GEOMETRY_SHADER;
		break;
	case GPUProgram::ShaderType::FRAGMENT:
		return GL_FRAGMENT_SHADER;
		break;
	case GPUProgram::ShaderType::COMPUTE:
		return GL_COMPUTE_SHADER;
		break;
	default:
		assert(0 && "Unreachable");
		break;
	}
}

void GPUProgram::AttachShader(const std::filesystem::path& path, ShaderType type)
{
	auto& shader = m_shaders[type];
	shader.type = type;
	shader.id = glCreateShader(ToGLShaderType(type));
	shader.path = path;
	glAttachShader(m_id, shader.id);
	auto result = CompileShader(shader.id, path.string().c_str());
	if (result != std::nullopt) {
		std::cerr << "Failed to compile shader \"" << path << "\"; Message: " << *result << '\n';
	}

	shader.callbackInfo.program = this;
	shader.callbackInfo.shader = &shader;
	// NOTE: DO NOT USE the first argument, it only contains the filename no the path!
	shader.watch.emplace(path.string(), [](const std::string&, const filewatch::Event event, void* user_pointer) {
		auto info = static_cast<Shader::CallbackInfo*>(user_pointer);
		switch (event) {
		case filewatch::Event::modified:
			std::lock_guard l{ s_shaderUpdateMutex };
			s_shaderUpdateInfo.needUpdate = true;
			s_shaderUpdateInfo.program = info->program;
			s_shaderUpdateInfo.shader = info->shader;
		    break;
		}}, &shader.callbackInfo);
}

bool GPUProgram::Link()
{
	auto result = LinkProgram(m_id);
	if (result != std::nullopt) {
		std::cerr << "Failed to link program; Message: " << *result << '\n';
		return false;
	}

	return true;
}

void GPUProgram::CheckShaderSourceUpdate()
{
	std::lock_guard l{ s_shaderUpdateMutex };
	if (!s_shaderUpdateInfo.needUpdate)
		return;

	s_shaderUpdateInfo.needUpdate = false;
	auto shader = s_shaderUpdateInfo.shader;
	auto program = s_shaderUpdateInfo.program;
	GLuint newID = glCreateShader(ToGLShaderType(shader->type));
	assert(glGetError() == GL_NO_ERROR);
	auto result = CompileShader(newID, shader->path.string().c_str());
	if (result != std::nullopt) {
		std::cerr << "Failed to compile shader " << shader->path << "; Message: " << *result << '\n';
	} 
	else {
		glDetachShader(program->m_id, shader->id);
		glDeleteShader(shader->id);
		shader->id = newID;
		glAttachShader(program->m_id, shader->id);
		if (program->Link()) {
			std::cout << "Shader " << shader->path << " has been reloaded successfully\n";
		}
	}
}

