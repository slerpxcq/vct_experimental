#include "utils.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>

#include <stb_image.h>

#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

// Window dimensions
constexpr GLuint WIDTH = 1280, HEIGHT = 720;
constexpr const char* MODEL_PATH = "resources/models/crytek-sponza";

struct Material
{
    // equals to (aiTextureType_x - 1)
    GLuint maps[AI_TEXTURE_TYPE_MAX - 1]{ 0 };
    bool twoSided{ false };
};

struct Mesh
{
    GLuint vao{ 0 };

    // buffers
    GLuint vbo{ 0 };
    GLuint ebo{ 0 };

    uint32_t vertexCount{ 0 };

    // textures
    uint32_t materialIndex{ 0 };
};

struct Vertex 
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Camera
{
    glm::mat4 matrix;
};

struct Settings
{
    bool showVoxels{ false };
    bool showAABB{ false };
    bool showWireframe{ false };
    bool showMesh{ true };
    bool showAxes{ false };
};

Settings g_settings;
std::vector<Material> g_materials;
std::vector<Mesh> g_meshes;
Camera g_camera;
GLFWwindow* g_window;
glm::vec3 g_sceneAABB[2];

GLuint g_basicProgram;
GLuint g_quadProgram;
GLuint g_drawAABBProgram;
GLuint g_drawAxesProgram;
GLuint g_voxelizeProgram;
GLuint g_drawVoxelsProgram;

GLuint g_voxelTex;

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

// Create and return the shader
GLuint CompileShader(const char* srcPath, GLenum type)
{
    GLuint shader = glCreateShader(type);
    auto src = LoadText(srcPath);
    auto srcCstr = src.c_str();
    glShaderSource(shader, 1, &srcCstr, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Failed to compile shader: " << log << '\n';
        std::terminate();
    }

    return shader;
}

void LinkProgram(GLuint program)
{
    glLinkProgram(program);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Failed to link program: " << log << '\n';
        std::terminate();
    }
}

void LoadShaders()
{
    constexpr const char* BASIC_VS_PATH = "resources/shaders/basic.vert";
    constexpr const char* BASIC_FS_PATH = "resources/shaders/basic.frag";
    constexpr const char* QUAD_VS_PATH = "resources/shaders/quad.vert";
    constexpr const char* QUAD_FS_PATH = "resources/shaders/quad.frag";
    constexpr const char* DRAW_AABB_VS_PATH = "resources/shaders/draw_aabb.vert";
    constexpr const char* DRAW_AABB_FS_PATH = "resources/shaders/draw_aabb.frag";
    constexpr const char* DRAW_AXES_VS_PATH = "resources/shaders/draw_axes.vert";
    constexpr const char* DRAW_AXES_FS_PATH = "resources/shaders/draw_axes.frag";
    constexpr const char* VOXELIZE_VS_PATH = "resources/shaders/voxelize.vert";
    constexpr const char* VOXELIZE_GS_PATH = "resources/shaders/voxelize.geom";
    constexpr const char* VOXELIZE_FS_PATH = "resources/shaders/voxelize.frag";
    constexpr const char* DRAW_VOXELS_VS_PATH = "resources/shaders/draw_voxels.vert";
    constexpr const char* DRAW_VOXELS_GS_PATH = "resources/shaders/draw_voxels.geom";
    constexpr const char* DRAW_VOXELS_FS_PATH = "resources/shaders/draw_voxels.frag";

    GLuint basicVs = CompileShader(BASIC_VS_PATH, GL_VERTEX_SHADER);
    GLuint basicFs = CompileShader(BASIC_FS_PATH, GL_FRAGMENT_SHADER);
    GLuint quadVs = CompileShader(QUAD_VS_PATH, GL_VERTEX_SHADER);
    GLuint quadFs = CompileShader(QUAD_FS_PATH, GL_FRAGMENT_SHADER);
    GLuint drawAABBVs = CompileShader(DRAW_AABB_VS_PATH, GL_VERTEX_SHADER);
    GLuint drawAABBFs = CompileShader(DRAW_AABB_FS_PATH, GL_FRAGMENT_SHADER);
    GLuint drawAxesVs = CompileShader(DRAW_AXES_VS_PATH, GL_VERTEX_SHADER);
    GLuint drawAxesFs = CompileShader(DRAW_AXES_FS_PATH, GL_FRAGMENT_SHADER);
    GLuint voxelizeVs = CompileShader(VOXELIZE_VS_PATH, GL_VERTEX_SHADER);
    GLuint voxelizeGs = CompileShader(VOXELIZE_GS_PATH, GL_GEOMETRY_SHADER);
    GLuint voxelizeFs = CompileShader(VOXELIZE_FS_PATH, GL_FRAGMENT_SHADER);
    GLuint drawVoxelsVs = CompileShader(DRAW_VOXELS_VS_PATH, GL_VERTEX_SHADER);
    GLuint drawVoxelsGs = CompileShader(DRAW_VOXELS_GS_PATH, GL_GEOMETRY_SHADER);
    GLuint drawVoxelsFs = CompileShader(DRAW_VOXELS_FS_PATH, GL_FRAGMENT_SHADER);

    g_basicProgram = glCreateProgram();
    glAttachShader(g_basicProgram, basicVs);
    glAttachShader(g_basicProgram, basicFs);
    LinkProgram(g_basicProgram);
    glDeleteShader(basicVs);
    glDeleteShader(basicFs);

    g_quadProgram = glCreateProgram();
    glAttachShader(g_quadProgram, quadVs);
    glAttachShader(g_quadProgram, quadFs);
    LinkProgram(g_quadProgram);
    glDeleteShader(quadVs);
    glDeleteShader(quadFs);

    g_drawAABBProgram = glCreateProgram();
    glAttachShader(g_drawAABBProgram, drawAABBVs);
    glAttachShader(g_drawAABBProgram, drawAABBFs);
    LinkProgram(g_drawAABBProgram);
    glDeleteShader(drawAABBVs);
    glDeleteShader(drawAABBFs);

    g_drawAxesProgram = glCreateProgram();
    glAttachShader(g_drawAxesProgram, drawAxesVs);
    glAttachShader(g_drawAxesProgram, drawAxesFs);
    LinkProgram(g_drawAxesProgram);
    glDeleteShader(drawAxesVs);
    glDeleteShader(drawAxesFs);

    g_voxelizeProgram = glCreateProgram();
    glAttachShader(g_voxelizeProgram, voxelizeVs);
    glAttachShader(g_voxelizeProgram, voxelizeGs);
    glAttachShader(g_voxelizeProgram, voxelizeFs);
    LinkProgram(g_voxelizeProgram);
    glDeleteShader(voxelizeVs);
    glDeleteShader(voxelizeGs);
    glDeleteShader(voxelizeFs);

    g_drawVoxelsProgram = glCreateProgram();
    glAttachShader(g_drawVoxelsProgram, drawVoxelsVs);
    glAttachShader(g_drawVoxelsProgram, drawVoxelsGs);
    glAttachShader(g_drawVoxelsProgram, drawVoxelsFs);
    LinkProgram(g_drawVoxelsProgram);
    glDeleteShader(drawVoxelsVs);
    glDeleteShader(drawVoxelsGs);
    glDeleteShader(drawVoxelsFs);
}

void MergeAABB(const aiScene* scene, glm::vec3* outAABB)
{
    glm::vec3 min{ std::numeric_limits<float>::max() };
    glm::vec3 max{ std::numeric_limits<float>::min() };
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        min = glm::min(min, Cast<glm::vec3>(scene->mMeshes[i]->mAABB.mMin));
        max = glm::max(max, Cast<glm::vec3>(scene->mMeshes[i]->mAABB.mMax));
    }
    outAABB[0] = min;
    outAABB[1] = max;
}

void LoadScene()
{
    const std::filesystem::path modelPath = MODEL_PATH;
    auto objPath = modelPath;
    objPath /= "sponza.obj";

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(objPath.string(),
                                             aiProcess_CalcTangentSpace |
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_SortByPType |
                                             aiProcess_GenBoundingBoxes);

    if (!scene) {
        std::cerr << importer.GetErrorString() << std::endl;
        std::terminate();
    }

    std::cout << "--- Scene loaded successfully ---\n";
    std::cout << "Mesh count: " << scene->mNumMeshes << '\n';
    std::cout << "Material count: " << scene->mNumMaterials << '\n';
    std::cout << "Texture count: " << scene->mNumTextures << '\n';
    std::cout << "Material count: " << scene->mNumMaterials << '\n';

    MergeAABB(scene, g_sceneAABB);
    std::cout << "Scene AABB: " << glm::to_string(g_sceneAABB[0]) << ", "
        << glm::to_string(g_sceneAABB[1]) << '\n';

    g_materials.resize(scene->mNumMaterials);
    g_meshes.resize(scene->mNumMeshes);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        auto material = scene->mMaterials[i];
        aiString albedoPath, specularPath, normalPath;
        std::cout << "- Material " << i << ": \n";
        for (uint32_t j = 1; j < AI_TEXTURE_TYPE_MAX; ++j) {
            if (material->GetTextureCount(static_cast<aiTextureType>(j))) {
                aiString tmpPath;
                material->GetTexture(static_cast<aiTextureType>(j), 0, &tmpPath);
                std::filesystem::path texPath = modelPath;
                texPath /= tmpPath.C_Str();
                auto texPathStr = texPath.string();
                std::replace(texPathStr.begin(), texPathStr.end(), '\\', '/');
                std::cout << "  Map " << j << ": " << texPathStr << "; ";
                int width = 0, height = 0, channels = 0;
                auto img = stbi_load(texPathStr.c_str(), &width, &height, &channels, 0);
                assert(img);
                std::cout << "channels=" << channels << '\n';
                g_materials[i].maps[j-1] = UploadTexture(img, width, height, channels);
                stbi_image_free(img);
            }
        }
        int32_t twoSided = 0;
        if (material->Get(AI_MATKEY_TWOSIDED, twoSided)) {
            g_materials[i].twoSided = true;
        }
        assert(glGetError() == GL_NO_ERROR);
    }

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        auto mesh = scene->mMeshes[i];

        /* Load vertices */
        std::vector<Vertex> vertices{ mesh->mNumVertices };
        for (uint32_t j = 0; j < mesh->mNumVertices; ++j) {
            vertices[j].position = glm::vec3(mesh->mVertices[j].x, 
                                             mesh->mVertices[j].y, 
                                             mesh->mVertices[j].z);
            vertices[j].normal = glm::vec3(mesh->mNormals[j].x,
                                           mesh->mNormals[j].y,
                                           mesh->mNormals[j].z);
            vertices[j].texCoord = glm::vec2(mesh->mTextureCoords[0][j].x,
                                             1.f - mesh->mTextureCoords[0][j].y);
        }

        /* Load indices */
        std::vector<glm::uvec3> faces{ mesh->mNumFaces };
        for (uint32_t j = 0; j < mesh->mNumFaces; ++j) {
            faces[j][0] = mesh->mFaces[j].mIndices[0];
            faces[j][1] = mesh->mFaces[j].mIndices[1];
            faces[j][2] = mesh->mFaces[j].mIndices[2];
        }

        g_meshes[i].vertexCount = faces.size() * 3;
        g_meshes[i].materialIndex = mesh->mMaterialIndex;
        
        /* Upload data */
        glCreateBuffers(1, &g_meshes[i].vbo);
        glCreateBuffers(1, &g_meshes[i].ebo);
        glCreateVertexArrays(1, &g_meshes[i].vao);
        glNamedBufferStorage(g_meshes[i].vbo, mesh->mNumVertices * sizeof(Vertex), vertices.data(), 0);
        glNamedBufferStorage(g_meshes[i].ebo, mesh->mNumFaces * sizeof(glm::uvec3), faces.data(), 0);

        auto vao = g_meshes[i].vao;
        glVertexArrayVertexBuffer(vao, 0, g_meshes[i].vbo, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(vao, g_meshes[i].ebo);
        glEnableVertexArrayAttrib(vao, 0);
        glEnableVertexArrayAttrib(vao, 1);
        glEnableVertexArrayAttrib(vao, 2);
        glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
        glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
        glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoord));
        glVertexArrayAttribBinding(vao, 0, 0);
        glVertexArrayAttribBinding(vao, 1, 0);
        glVertexArrayAttribBinding(vao, 2, 0);

        assert(glGetError() == GL_NO_ERROR);
    }

	importer.FreeScene();
}

void CreateWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", NULL, NULL);
    glfwMakeContextCurrent(g_window);
    if (g_window == NULL) {
        std::cerr << "Failed to create GLFW g_window" << std::endl;
        glfwTerminate();
        std::terminate();
    }

    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cerr << "Failed to initialize OpenGL context" << std::endl;
        std::terminate();
    }
}

constexpr uint32_t VOXEL_RESOLUTION = 512;

void VoxelizeScene()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glProgramUniform3fv(g_voxelizeProgram, glGetUniformLocation(g_voxelizeProgram, "u_sceneAABB"), 2, glm::value_ptr(g_sceneAABB[0]));
	glProgramUniform1ui(g_voxelizeProgram, glGetUniformLocation(g_voxelizeProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
    glUseProgram(g_voxelizeProgram);
    glViewport(0, 0, VOXEL_RESOLUTION, VOXEL_RESOLUTION);

    for (uint32_t i = 0; i < g_meshes.size(); ++i) {
        glBindVertexArray(g_meshes[i].vao);
        glDrawElements(GL_TRIANGLES, g_meshes[i].vertexCount, GL_UNSIGNED_INT, 0);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void UpdateCamera(float frameTimeMs)
{
	constexpr float MOVE_SPEED = 0.2f;
	constexpr float ROTATE_SPEED = 1.f;

	if (ImGui::IsKeyDown(ImGuiKey_W)) { // -z
		g_camera.matrix[3] += glm::vec4(-MOVE_SPEED * glm::vec3(g_camera.matrix[2]) * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_A)) { // -x
		g_camera.matrix[3] += glm::vec4(-MOVE_SPEED * glm::vec3(g_camera.matrix[0]) * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_S)) { // +z
		g_camera.matrix[3] += glm::vec4(MOVE_SPEED * glm::vec3(g_camera.matrix[2]) * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_D)) { // +x
		g_camera.matrix[3] += glm::vec4(MOVE_SPEED * glm::vec3(g_camera.matrix[0]) * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) { // -y
		g_camera.matrix[3] += glm::vec4(MOVE_SPEED * glm::vec3{ 0, -1, 0 } * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_Space)) { // +y
		g_camera.matrix[3] += glm::vec4(MOVE_SPEED * glm::vec3{ 0, 1, 0 } * frameTimeMs, 0);
	}
	if (ImGui::IsKeyDown(ImGuiKey_Q)) { // rotate ccw along y axis
		g_camera.matrix = glm::rotate(g_camera.matrix, glm::radians(ROTATE_SPEED), glm::vec3{ 0, 1, 0 });
	}
	if (ImGui::IsKeyDown(ImGuiKey_E)) { // rotate cw along y axis
		g_camera.matrix = glm::rotate(g_camera.matrix, -glm::radians(ROTATE_SPEED), glm::vec3{ 0, 1, 0 });
	}
}

int main()
{
    CreateWindow();
    LoadShaders();
    LoadScene();

    // VAO with no buffer binding, used when geometry is generated in shaders
    GLuint genericDrawVao;
    glCreateVertexArrays(1, &genericDrawVao);

    // Create texture for voxelization
    // atomicImageAdd could only operate on integer images 
    constexpr GLuint VOXEL_IMAGE_BINDING = 0;
    glCreateTextures(GL_TEXTURE_3D, 1, &g_voxelTex);
    glTextureStorage3D(g_voxelTex, 1, GL_R32UI, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION);
    glBindImageTexture(VOXEL_IMAGE_BINDING, g_voxelTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    assert(glGetError() == GL_NO_ERROR);


    IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImGuiStyle& style = ImGui::GetStyle();
	style.AntiAliasedLines = true;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(g_window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

    g_camera.matrix = glm::lookAt(glm::vec3{ 0, 0, 0 },
                                glm::vec3{ 0, 0, -1 },
                                glm::vec3{ 0, 1, 0 });

    static float frameTimeMs;
    decltype(std::chrono::high_resolution_clock::now()) last;

    while (!glfwWindowShouldClose(g_window))
    {
		int32_t windowWidth, windowHeight;
		glfwGetWindowSize(g_window, &windowWidth, &windowHeight);

        /******************************************** BEGIN UPDATE ********************************************/
        static glm::mat4 proj;
        if (windowWidth > 0 && windowHeight > 0)
			proj = glm::perspective(glm::radians(60.f), static_cast<float>(windowWidth) / windowHeight, 0.1f, 10000.f);

        auto now = std::chrono::high_resolution_clock::now();
        frameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count() / 1000.f;
        last = now;

        UpdateCamera(frameTimeMs);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        /******************************************** END UPDATE ********************************************/


        /******************************************** BEGIN DRAW ********************************************/
		VoxelizeScene();

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (g_settings.showWireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

        if (g_settings.showMesh) {
            for (uint32_t i = 0; i < g_meshes.size(); ++i) {
                uint32_t hasMap[8] = { 0 };
                auto mat = g_materials[g_meshes[i].materialIndex];
                for (uint32_t j = 0; j < 8; ++j) {
                    auto tex = mat.maps[j];
                    if (tex) {
                        glBindTextureUnit(j, tex);
                        hasMap[j] = 1;
                    }
                }
                glViewport(0, 0, windowWidth, windowHeight);
                glEnable(GL_DEPTH_TEST);
                glProgramUniform1uiv(g_basicProgram, glGetUniformLocation(g_basicProgram, "u_hasMap"), 8, hasMap);
                glProgramUniformMatrix4fv(g_basicProgram, glGetUniformLocation(g_basicProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
                glProgramUniformMatrix4fv(g_basicProgram, glGetUniformLocation(g_basicProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
                glBindVertexArray(g_meshes[i].vao);
                glUseProgram(g_basicProgram);
                if (mat.twoSided) {
                    glDisable(GL_CULL_FACE);
                }
                else {
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CCW);
                }
                glDrawElements(GL_TRIANGLES, g_meshes[i].vertexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (g_settings.showVoxels) { // draw voxelized scene
			glViewport(0, 0, windowWidth, windowHeight);
            // glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
			glProgramUniform3fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_sceneAABB"), 2, glm::value_ptr(g_sceneAABB[0]));
			glProgramUniform1ui(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
			glProgramUniformMatrix4fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
			glProgramUniformMatrix4fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
            glUseProgram(g_drawVoxelsProgram);
			glBindImageTexture(VOXEL_IMAGE_BINDING, g_voxelTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
            glBindVertexArray(genericDrawVao);
            glDrawArrays(GL_POINTS, 0, VOXEL_RESOLUTION * VOXEL_RESOLUTION * VOXEL_RESOLUTION);
        }

        if (g_settings.showAABB) {
			glViewport(0, 0, windowWidth, windowHeight);
            glEnable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
			glProgramUniform3fv(g_drawAABBProgram, glGetUniformLocation(g_drawAABBProgram, "u_sceneAABB"), 2, glm::value_ptr(g_sceneAABB[0]));
			glProgramUniformMatrix4fv(g_drawAABBProgram, glGetUniformLocation(g_drawAABBProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
			glProgramUniformMatrix4fv(g_drawAABBProgram, glGetUniformLocation(g_drawAABBProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
            glUseProgram(g_drawAABBProgram);
            glBindVertexArray(genericDrawVao);
            glDrawArrays(GL_LINES, 0, 24);
        }

        if (g_settings.showAxes) {
			glViewport(0, 0, windowWidth, windowHeight);
            GLfloat lineWidth;
            glGetFloatv(GL_LINE_WIDTH, &lineWidth);
            glLineWidth(5.f);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
			glProgramUniformMatrix4fv(g_drawAxesProgram, glGetUniformLocation(g_drawAxesProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
			glProgramUniformMatrix4fv(g_drawAxesProgram, glGetUniformLocation(g_drawAxesProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
            glUseProgram(g_drawAxesProgram);
            glBindVertexArray(genericDrawVao);
            glDrawArrays(GL_LINES, 0, 6);
            glLineWidth(lineWidth);
        }

        ImGui::Begin("Settings");
        ImGui::Checkbox("Show mesh", &g_settings.showMesh);
        ImGui::Checkbox("Show wireframe", &g_settings.showWireframe);
        ImGui::Checkbox("Show voxels", &g_settings.showVoxels);
        ImGui::Checkbox("Show AABB", &g_settings.showAABB);
        ImGui::Checkbox("Show axes", &g_settings.showAxes);
        ImGui::End();

        /******************************************** END   DRAW ********************************************/

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(windowWidth, windowHeight);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap the screen buffers
        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

    // Terminates GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();
    return 0;
}
