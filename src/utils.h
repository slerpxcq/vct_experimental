#pragma once

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>

template <typename Dst, typename Src>
Dst Cast(const Src& src);

GLuint UploadTexture(void* img, uint32_t width, uint32_t height, uint32_t channels);
std::string LoadText(const char* path);

std::optional<std::string> CompileShader(GLuint shader, const char* srcPath);
std::optional<std::string> LinkProgram(GLuint program);

// Will return 0 if input is 0
template <typename T>
uint32_t Log2(T x)
{
	static_assert(std::is_unsigned_v<T>, "T must be of unsigned integer type");
	uint32_t result = 0;
	while (x > 0) {
		x >>= 1;
		++result;
	}
	return result;
}


