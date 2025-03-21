#pragma once

#include <glad/glad.h>
#include <FileWatch.hpp>

#include <filesystem>
#include <optional>
#include <mutex>

// Helper class reloads shader source when updated
class GPUProgram
{
public:
	enum ShaderType {
		VERTEX, GEOMETRY, FRAGMENT, COMPUTE
	};

private:
	using Watch = filewatch::FileWatch<std::string>;

	struct Shader;
	struct CallbackInfo {
		GPUProgram* program{ nullptr };
		Shader* shader{ nullptr };
	};

	struct Shader {
		GLuint id{ 0 };
		ShaderType type{};
		std::filesystem::path path;
		std::optional<Watch> watch;
		CallbackInfo callbackInfo;
	};

public:
	GPUProgram() = default;
	~GPUProgram();
	void Init() { m_id = glCreateProgram(); } 
	void AttachShader(const std::filesystem::path& path, ShaderType type);
	bool Link();
	GLuint GetID() const { return m_id; }
	operator GLuint() { return m_id; }

	static void CheckShaderSourceUpdate();

private:
	Shader m_shaders[4]{ 0 };
	GLuint m_id{ 0 };

	static inline std::mutex s_shaderUpdateMutex;
	static inline bool s_shaderNeedUpdate;
	static inline CallbackInfo* s_shaderCallbackInfo;
};

