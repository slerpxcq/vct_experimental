#include "utils.h"

#include <glm/glm.hpp>
#include <assimp/Importer.hpp>

template <>
glm::vec3 Cast(const aiVector3D& src)
{
    return glm::vec3{ src.x, src.y, src.z };
}

GLuint UploadTexture(void* img, uint32_t width, uint32_t height, uint32_t channels)
{
    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    switch (channels) {
    case 1:
        glTextureStorage2D(tex, 1, GL_R8, width, height);
        glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, img);
        break;
    case 2:
        glTextureStorage2D(tex, 1, GL_RG8, width, height);
        glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RG, GL_UNSIGNED_BYTE, img);
        break;
    case 3:
        glTextureStorage2D(tex, 1, GL_RGB8, width, height);
        glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, img);
        break;
    case 4:
        glTextureStorage2D(tex, 1, GL_RGBA8, width, height);
        glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, img);
        break;
    default:
        std::cerr << "Unsupported texture channel count\n";
        std::terminate();
        break;
    }

    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

std::string LoadText(const char* path)
{
    std::ifstream ifs{ path };
    if (!ifs.is_open()) {
        std::cerr << "Could not open file \"" << path << "\"\n";
        std::terminate();
    }
    
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// return nullopt in case of success
std::optional<std::string> CompileShader(GLuint shader, const char* srcPath)
{
    auto src = LoadText(srcPath);
    auto srcCstr = src.c_str();
    glShaderSource(shader, 1, &srcCstr, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]{ 0 };
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        return std::string{ log };
    }

    return std::nullopt;
}

// return nullopt in case of success
std::optional<std::string> LinkProgram(GLuint program)
{
    glLinkProgram(program);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        return std::string{ log };
    }

    return std::nullopt;
}
