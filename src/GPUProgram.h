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

	struct Shader {
		GLuint id{ 0 };
		ShaderType type{};
		std::optional<Watch> watch;
		std::filesystem::path path;
		struct CallbackInfo {
			GPUProgram* program{ nullptr };
			Shader* shader{ nullptr };
		} callbackInfo;
	};


	struct ShaderUpdateInfo {
		bool needUpdate{ false };
		GPUProgram* program{ nullptr };
		Shader* shader{ nullptr };
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
	static inline ShaderUpdateInfo s_shaderUpdateInfo;
};

