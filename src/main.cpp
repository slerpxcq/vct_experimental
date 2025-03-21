#include "utils.h"
#include "GPUProgram.h"

#include <glad/glad.h>
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
    enum VoxelChannel {
        ALBEDO,
        NORMAL,
        EMISSIVE,
        RADIANCE,
        OCCLUSION
    };

    bool showVoxels{ false };
    int32_t showVoxelsMipLevel{ 0 };
    bool indirectLighting{ false };
    uint32_t voxelChannel{};
    bool showAABB{ false };
    bool showWireframe{ false };
    bool showMesh{ true };
    bool showAxes{ false };
    bool conservativeVoxelization{ false };
};

struct DirectionalLight 
{
    glm::vec4 color;
    glm::vec4 direction;
};

struct GlobalUniformBlock // layout (std140)
{
    float traceOffset{ 1.f };
};

/* -------------- Global constants -------------- */
constexpr uint32_t VOXEL_RESOLUTION = 256;
constexpr GLuint VOXEL_ALBEDO_IMAGE_BINDING = 0;
constexpr GLuint VOXEL_NORMAL_IMAGE_BINDING = 1;
constexpr GLuint VOXEL_EMISSIVE_IMAGE_BINDING = 2;
constexpr GLuint VOXEL_RADIANCE_IMAGE_BINDING = 3;

/* -------------- Global variables -------------- */
GLFWwindow* g_window;
bool g_windowFocused;

Settings g_settings;
std::vector<Material> g_materials;
std::vector<Mesh> g_meshes;
Camera g_camera;
glm::vec3 g_sceneAABB[2];

GPUProgram g_basicProgram;
GPUProgram g_quadProgram;
GPUProgram g_drawAABBProgram;
GPUProgram g_drawAxesProgram;
GPUProgram g_voxelizeProgram;
GPUProgram g_drawVoxelsProgram;
GPUProgram g_voxelDirectLightingProgram;
GPUProgram g_voxelIndirectLightingProgram;

GLuint g_voxelAlbedoTex;
GLuint g_voxelNormalTex;
GLuint g_voxelEmissiveTex;
GLuint g_voxelRadianceTex;

GlobalUniformBlock g_globalUniforms; // CPU side
GLuint g_globalUniformBuffer; // GPU side handle

DirectionalLight g_directionalLight;

bool g_generateMipmapFlag;

void LoadShaders()
{
    static constexpr const char* BASIC_VS_PATH = "resources/shaders/basic.vert";
    static constexpr const char* BASIC_FS_PATH = "resources/shaders/basic.frag";
    static constexpr const char* QUAD_VS_PATH = "resources/shaders/quad.vert";
    static constexpr const char* QUAD_FS_PATH = "resources/shaders/quad.frag";
    static constexpr const char* DRAW_AABB_VS_PATH = "resources/shaders/draw_aabb.vert";
    static constexpr const char* DRAW_AABB_FS_PATH = "resources/shaders/draw_aabb.frag";
    static constexpr const char* DRAW_AXES_VS_PATH = "resources/shaders/draw_axes.vert";
    static constexpr const char* DRAW_AXES_FS_PATH = "resources/shaders/draw_axes.frag";
    static constexpr const char* VOXELIZE_VS_PATH = "resources/shaders/voxelize.vert";
    static constexpr const char* VOXELIZE_GS_PATH = "resources/shaders/voxelize.geom";
    static constexpr const char* VOXELIZE_FS_PATH = "resources/shaders/voxelize.frag";
    static constexpr const char* DRAW_VOXELS_VS_PATH = "resources/shaders/draw_voxels.vert";
    static constexpr const char* DRAW_VOXELS_GS_PATH = "resources/shaders/draw_voxels.geom";
    static constexpr const char* DRAW_VOXELS_FS_PATH = "resources/shaders/draw_voxels.frag";
    static constexpr const char* VOXEL_DIRECT_LIGHTING_CS_PATH = "resources/shaders/voxel_direct_lighting.comp";
    static constexpr const char* VOXEL_INDIRECT_LIGHTING_CS_PATH = "resources/shaders/voxel_indirect_lighting.comp";

	g_basicProgram.Init();
	g_quadProgram.Init();
	g_drawAABBProgram.Init();
	g_drawAxesProgram.Init();
	g_voxelizeProgram.Init();
	g_drawVoxelsProgram.Init();
	g_voxelDirectLightingProgram.Init();
	g_voxelIndirectLightingProgram.Init();

    g_basicProgram.AttachShader(BASIC_VS_PATH, GPUProgram::VERTEX);
    g_basicProgram.AttachShader(BASIC_FS_PATH, GPUProgram::FRAGMENT);
    g_basicProgram.Link();

    g_quadProgram.AttachShader(QUAD_VS_PATH, GPUProgram::VERTEX);
    g_quadProgram.AttachShader(QUAD_FS_PATH, GPUProgram::FRAGMENT);
    g_quadProgram.Link();

    g_drawAABBProgram.AttachShader(DRAW_AABB_VS_PATH, GPUProgram::VERTEX);
    g_drawAABBProgram.AttachShader(DRAW_AABB_FS_PATH, GPUProgram::FRAGMENT);
    g_drawAABBProgram.Link();

    g_drawAxesProgram.AttachShader(DRAW_AXES_VS_PATH, GPUProgram::VERTEX);
    g_drawAxesProgram.AttachShader(DRAW_AXES_FS_PATH, GPUProgram::FRAGMENT);
    g_drawAxesProgram.Link();

    g_voxelizeProgram.AttachShader(VOXELIZE_VS_PATH, GPUProgram::VERTEX);
    g_voxelizeProgram.AttachShader(VOXELIZE_GS_PATH, GPUProgram::GEOMETRY);
    g_voxelizeProgram.AttachShader(VOXELIZE_FS_PATH, GPUProgram::FRAGMENT);
    g_voxelizeProgram.Link();

    g_drawVoxelsProgram.AttachShader(DRAW_VOXELS_VS_PATH, GPUProgram::VERTEX);
    g_drawVoxelsProgram.AttachShader(DRAW_VOXELS_GS_PATH, GPUProgram::GEOMETRY);
    g_drawVoxelsProgram.AttachShader(DRAW_VOXELS_FS_PATH, GPUProgram::FRAGMENT);
    g_drawVoxelsProgram.Link();

    g_voxelDirectLightingProgram.AttachShader(VOXEL_DIRECT_LIGHTING_CS_PATH, GPUProgram::COMPUTE);
    g_voxelDirectLightingProgram.Link();

    g_voxelIndirectLightingProgram.AttachShader(VOXEL_INDIRECT_LIGHTING_CS_PATH, GPUProgram::COMPUTE);
    g_voxelIndirectLightingProgram.Link();
}

void MergeAABB(const aiScene* scene, glm::vec3* outAABB)
{
    glm::vec3 min{ std::numeric_limits<float>::max() };
    glm::vec3 max{ std::numeric_limits<float>::min() };
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        min = glm::min(min, Cast<glm::vec3>(scene->mMeshes[i]->mAABB.mMin));
        max = glm::max(max, Cast<glm::vec3>(scene->mMeshes[i]->mAABB.mMax));
    }
    outAABB[0] = glm::vec3{ std::min({ min.x, min.y, min.z }) };
    outAABB[1] = glm::vec3{ std::max({ max.x, max.y, max.z }) };
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

void InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", nullptr, nullptr);
    glfwMakeContextCurrent(g_window);
    if (g_window == nullptr) {
        std::cerr << "Failed to create GLFW g_window" << std::endl;
        glfwTerminate();
        std::terminate();
    }

    int version = gladLoadGL();
    if (version == 0) {
        std::cerr << "Failed to initialize OpenGL context" << std::endl;
        std::terminate();
    }
}


void VoxelizeScene()
{
    glm::vec4 clearColor{ 0.f };
    glClearTexImage(g_voxelAlbedoTex, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColor));
    glClearTexImage(g_voxelNormalTex, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColor));
    glClearTexImage(g_voxelEmissiveTex, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColor));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glProgramUniform3fv(g_voxelizeProgram, glGetUniformLocation(g_voxelizeProgram, "u_sceneAABB"), 2, glm::value_ptr(g_sceneAABB[0]));
	glProgramUniform1ui(g_voxelizeProgram, glGetUniformLocation(g_voxelizeProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
    glUseProgram(g_voxelizeProgram);
    glViewport(0, 0, VOXEL_RESOLUTION, VOXEL_RESOLUTION);

    if (g_settings.conservativeVoxelization)
        glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);

    // 1 pass for each axis
    for (uint32_t axis = 0; axis < 3; ++axis) {
		glProgramUniform1ui(g_voxelizeProgram, glGetUniformLocation(g_voxelizeProgram, "u_axis"), axis);
        for (uint32_t i = 0; i < g_meshes.size(); ++i) {
            auto mat = g_materials[g_meshes[i].materialIndex];
            for (uint32_t j = 0; j < 8; ++j) {
                auto tex = mat.maps[j];
                if (tex)
                    glBindTextureUnit(j, tex);
            }
            glBindVertexArray(g_meshes[i].vao);
            glDrawElements(GL_TRIANGLES, g_meshes[i].vertexCount, GL_UNSIGNED_INT, 0);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
}

void ComputeVoxelDirectLighting()
{
    constexpr uint32_t WORKGROUP_SIZE = 8;
    glm::vec4 clearColor{ 0.f };
    glClearTexImage(g_voxelRadianceTex, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColor));
    glUseProgram(g_voxelDirectLightingProgram);
    uint32_t workgroupCount = VOXEL_RESOLUTION / WORKGROUP_SIZE;
    glProgramUniform1ui(g_voxelDirectLightingProgram, glGetUniformLocation(g_voxelDirectLightingProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
    glProgramUniform3fv(g_voxelDirectLightingProgram, glGetUniformLocation(g_voxelDirectLightingProgram, "u_lightColor"), 1, glm::value_ptr(g_directionalLight.color));
    glProgramUniform3fv(g_voxelDirectLightingProgram, glGetUniformLocation(g_voxelDirectLightingProgram, "u_lightDirection"), 1, glm::value_ptr(g_directionalLight.direction));
    glDispatchCompute(workgroupCount, workgroupCount, workgroupCount);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    if (g_generateMipmapFlag) {
        glGenerateTextureMipmap(g_voxelRadianceTex);
        g_generateMipmapFlag = false;
    }
}

void ComputeVoxelIndirectLighting()
{
    constexpr uint32_t WORKGROUP_SIZE = 8;
    glm::vec4 clearColor{ 0.f };
    // glClearTexImage(g_voxelRadianceTex, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColor));
    glUseProgram(g_voxelIndirectLightingProgram);
    uint32_t workgroupCount = VOXEL_RESOLUTION / WORKGROUP_SIZE;
    glProgramUniform1ui(g_voxelIndirectLightingProgram, glGetUniformLocation(g_voxelIndirectLightingProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
    glProgramUniform3fv(g_voxelIndirectLightingProgram, glGetUniformLocation(g_voxelIndirectLightingProgram, "u_lightColor"), 1, glm::value_ptr(g_directionalLight.color));
    glProgramUniform3fv(g_voxelIndirectLightingProgram, glGetUniformLocation(g_voxelIndirectLightingProgram, "u_lightIndirection"), 1, glm::value_ptr(g_directionalLight.direction));
    glDispatchCompute(workgroupCount, workgroupCount, workgroupCount);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
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

void CreateTextures()
{
    uint32_t mipLevel = Log2(VOXEL_RESOLUTION);
    assert(mipLevel == 9);

    glCreateTextures(GL_TEXTURE_3D, 1, &g_voxelAlbedoTex);
    glTextureStorage3D(g_voxelAlbedoTex, mipLevel, GL_RGBA16F, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION);
    glBindImageTexture(VOXEL_ALBEDO_IMAGE_BINDING, g_voxelAlbedoTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

    glCreateTextures(GL_TEXTURE_3D, 1, &g_voxelNormalTex);
    glTextureStorage3D(g_voxelNormalTex, mipLevel, GL_RGBA16F, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION);
    glBindImageTexture(VOXEL_NORMAL_IMAGE_BINDING, g_voxelNormalTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

    glCreateTextures(GL_TEXTURE_3D, 1, &g_voxelEmissiveTex);
    glTextureStorage3D(g_voxelEmissiveTex, mipLevel, GL_RGBA16F, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION);
    glBindImageTexture(VOXEL_EMISSIVE_IMAGE_BINDING, g_voxelEmissiveTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

    glCreateTextures(GL_TEXTURE_3D, 1, &g_voxelRadianceTex);
    glTextureStorage3D(g_voxelRadianceTex, mipLevel, GL_RGBA16F, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION, 
                       VOXEL_RESOLUTION);
    glTextureParameteri(g_voxelRadianceTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(g_voxelRadianceTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(g_voxelRadianceTex, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTextureParameteri(g_voxelRadianceTex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(g_voxelRadianceTex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindImageTexture(VOXEL_RADIANCE_IMAGE_BINDING, g_voxelRadianceTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

    assert(glGetError() == GL_NO_ERROR);
}

void InitImGui()
{
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
}

int main()
{ 
    InitWindow();
    LoadShaders();
    LoadScene();

    // VAO with no buffer binding, used when geometry is generated in shaders
    GLuint genericDrawVao;
    glCreateVertexArrays(1, &genericDrawVao);

    glCreateBuffers(1, &g_globalUniformBuffer);
    glNamedBufferStorage(g_globalUniformBuffer, sizeof(GlobalUniformBlock), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_globalUniformBuffer);
    assert(glGetError() == GL_NO_ERROR);

    CreateTextures();

    InitImGui();

    g_camera.matrix = glm::lookAt(glm::vec3{ 0, 0, 0 },
                                glm::vec3{ 0, 0, -1 },
                                glm::vec3{ 0, 1, 0 });

    static float frameTimeMs;
    decltype(std::chrono::high_resolution_clock::now()) last;

    while (!glfwWindowShouldClose(g_window))
    {
		GPUProgram::CheckShaderSourceUpdate();
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

		/********************************************  BEGIN UI  ********************************************/
		ImGui::Begin("Settings");
		ImGui::SeparatorText("Voxelization");
		ImGui::Checkbox("Conservative voxelization", &g_settings.conservativeVoxelization);
		ImGui::Checkbox("Show voxels", &g_settings.showVoxels);
        ImGui::SliderInt("Radiance voxel mip level", &g_settings.showVoxelsMipLevel, 0, 8);
		ImGui::Checkbox("Indirect lighting", &g_settings.indirectLighting);
		if (ImGui::BeginListBox("Voxel channel")) {
			if (ImGui::Selectable("Albedo")) g_settings.voxelChannel = Settings::VoxelChannel::ALBEDO;
			if (ImGui::Selectable("Normal")) g_settings.voxelChannel = Settings::VoxelChannel::NORMAL;
			if (ImGui::Selectable("Emissive")) g_settings.voxelChannel = Settings::VoxelChannel::EMISSIVE;
			if (ImGui::Selectable("Radiance")) g_settings.voxelChannel = Settings::VoxelChannel::RADIANCE;
			if (ImGui::Selectable("Occlusion")) g_settings.voxelChannel = Settings::VoxelChannel::OCCLUSION;
			ImGui::EndListBox();
		}
        if (ImGui::Button("Generate radiance mipmap")) {
            g_generateMipmapFlag = true;
        }
        ImGui::SliderFloat("Trace cone/ray origin offset", &g_globalUniforms.traceOffset, 1.f, 3.f);

		ImGui::SeparatorText("View");
		ImGui::Checkbox("Show mesh", &g_settings.showMesh);
		ImGui::Checkbox("Show wireframe", &g_settings.showWireframe);
		ImGui::Checkbox("Show AABB", &g_settings.showAABB);
		ImGui::Checkbox("Show axes", &g_settings.showAxes);

		ImGui::SeparatorText("Lights");
		ImGui::SliderFloat3("Color", glm::value_ptr(g_directionalLight.color), 0.f, 1.f, "%.2f");
		ImGui::SliderFloat3("Direction", glm::value_ptr(g_directionalLight.direction), -1.f, 1.f, "%.2f");
		ImGui::End();
		/********************************************  END UI ********************************************/

		/******************************************** BEGIN GLOBAL UNIFORM ********************************************/
        glNamedBufferSubData(g_globalUniformBuffer, 0, sizeof(GlobalUniformBlock), &g_globalUniforms);
		/******************************************** END GLOBAL UNIFORM ********************************************/

		/******************************************** BEGIN DRAW ********************************************/
		VoxelizeScene();
		ComputeVoxelDirectLighting();
        if (g_settings.indirectLighting)
            ComputeVoxelIndirectLighting();

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (g_settings.showWireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		if (g_settings.showMesh) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
			glDisable(GL_BLEND);
		}

		if (g_settings.showVoxels) { // draw voxelized scene
			glViewport(0, 0, windowWidth, windowHeight);
			glDisable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glProgramUniform3fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_sceneAABB"), 2, glm::value_ptr(g_sceneAABB[0]));
			glProgramUniform1ui(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_voxelResolution"), VOXEL_RESOLUTION);
			glProgramUniformMatrix4fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
			glProgramUniformMatrix4fv(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
			glProgramUniform1ui(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_drawMode"), g_settings.voxelChannel);
			glProgramUniform1ui(g_drawVoxelsProgram, glGetUniformLocation(g_drawVoxelsProgram, "u_mipLevel"), g_settings.showVoxelsMipLevel);
			glUseProgram(g_drawVoxelsProgram);
			glBindImageTexture(VOXEL_ALBEDO_IMAGE_BINDING, g_voxelAlbedoTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
            glBindTextureUnit(4, g_voxelRadianceTex);
			glBindVertexArray(genericDrawVao);
			glDrawArrays(GL_POINTS, 0, VOXEL_RESOLUTION * VOXEL_RESOLUTION * VOXEL_RESOLUTION);
			glDisable(GL_BLEND);
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
			glLineWidth(1.f);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glProgramUniformMatrix4fv(g_drawAxesProgram, glGetUniformLocation(g_drawAxesProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
			glProgramUniformMatrix4fv(g_drawAxesProgram, glGetUniformLocation(g_drawAxesProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));
			glUseProgram(g_drawAxesProgram);
			glBindVertexArray(genericDrawVao);
			glDrawArrays(GL_LINES, 0, 6);
			glLineWidth(lineWidth);
		}

		/******************************************** END   DRAW ********************************************/
		ImGui::GetIO().DisplaySize = ImVec2(windowWidth, windowHeight);
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
