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
