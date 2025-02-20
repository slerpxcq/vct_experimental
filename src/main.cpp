#include <iostream>

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

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>

#include <stb_image.h>

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

std::vector<Material> g_materials;
std::vector<Mesh> g_meshes;
Camera g_camera;

GLuint g_shaderProgram;

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
        std::cerr << "Could not open file " << path << '\n';
        std::terminate();
    }
    
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void CompileShader(GLuint shader, const char* const* src)
{
    glShaderSource(shader, 1, src, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Failed to compile shader: " << log << '\n';
        std::terminate();
    }
}

void LoadShaders()
{
    constexpr const char* VS_PATH = "resources/shaders/basic.vert";
    constexpr const char* FS_PATH = "resources/shaders/basic.frag";

    auto vsSrc = LoadText(VS_PATH);
    auto fsSrc = LoadText(FS_PATH);
    auto vs = glCreateShader(GL_VERTEX_SHADER);
    auto fs = glCreateShader(GL_FRAGMENT_SHADER);
    auto ptr = vsSrc.c_str();
    CompileShader(vs, &ptr);
    ptr = fsSrc.c_str();
    CompileShader(fs, &ptr);
    g_shaderProgram = glCreateProgram();
    glAttachShader(g_shaderProgram, vs);
    glAttachShader(g_shaderProgram, fs);
    glLinkProgram(g_shaderProgram);

    int ok = 0;
    glGetProgramiv(g_shaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(g_shaderProgram, sizeof(log), nullptr, log);
        std::cerr << "Failed to link program: " << log << '\n';
        std::terminate();
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
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
                                             aiProcess_SortByPType);

    if (!scene) {
        std::cerr << importer.GetErrorString() << std::endl;
        std::terminate();
    }

    std::cout << "--- Scene loaded successfully ---\n";
    std::cout << "Mesh count: " << scene->mNumMeshes << '\n';
    std::cout << "Material count: " << scene->mNumMaterials << '\n';
    std::cout << "Texture count: " << scene->mNumTextures << '\n';
    std::cout << "Material count: " << scene->mNumMaterials << '\n';

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

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }

    LoadShaders();
    LoadScene();

    IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImGuiStyle& style = ImGui::GetStyle();
	style.AntiAliasedLines = true;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");

    g_camera.matrix = glm::lookAt(glm::vec3{ 0, 0, 0 },
                                glm::vec3{ 0, 0, -1 },
                                glm::vec3{ 0, 1, 0 });

    static float frameTimeMs;
    decltype(std::chrono::high_resolution_clock::now()) last;

    while (!glfwWindowShouldClose(window))
    {
		int32_t windowWidth, windowHeight;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		glViewport(0, 0, windowWidth, windowHeight);

        static glm::mat4 proj;
        if (windowWidth > 0 && windowHeight > 0)
			proj = glm::perspective(glm::radians(60.f), static_cast<float>(windowWidth) / windowHeight, 0.1f, 10000.f);

        auto now = std::chrono::high_resolution_clock::now();
        frameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count() / 1000.f;
        last = now;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

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

        // Render
        // Clear the colorbuffer
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        /******************************************** BEGIN DRAW ********************************************/
        glProgramUniformMatrix4fv(g_shaderProgram, glGetUniformLocation(g_shaderProgram, "u_proj"), 1, GL_FALSE, glm::value_ptr(proj));
        glProgramUniformMatrix4fv(g_shaderProgram, glGetUniformLocation(g_shaderProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(glm::inverse(g_camera.matrix)));

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
            glProgramUniform1uiv(g_shaderProgram, glGetUniformLocation(g_shaderProgram, "u_hasMap"), 8, hasMap);
            glBindVertexArray(g_meshes[i].vao);
			glUseProgram(g_shaderProgram);
            if (mat.twoSided) {
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CCW);
            }
            glDrawElements(GL_TRIANGLES, g_meshes[i].vertexCount, GL_UNSIGNED_INT, 0);
        }

        ImGui::ShowDemoWindow();
        /******************************************** END   DRAW ********************************************/

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(windowWidth, windowHeight);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap the screen buffers
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

    // Terminates GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();
    return 0;
}
