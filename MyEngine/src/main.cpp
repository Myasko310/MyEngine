#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <memory>
#include <chrono>
#include <vector>

// Core
#include "core/Input.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"

#include "components/LightComponent.h"

// Components
#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"

// Rendering
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
// Model / serialization
#include "rendering/Model.h"
#include "serialization/SceneSerializer.h"
// Asset manager
#include "core/AssetManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#endif

// Systems
#include "systems/CameraSystem.h"
#include "systems/MeshRendererSystem.h"

using namespace MyEngine;

// Make the initial window larger so UI and scene are more visible by default
static int g_WindowWidth = 1600;
static int g_WindowHeight = 900;

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    g_WindowWidth = width;
    g_WindowHeight = height;

    glViewport(0, 0, width, height);
}

static void APIENTRY OpenGLDebugCallback(
    GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam
)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    std::cerr << "[OpenGL Debug] " << message << std::endl;
}

int main()
{
    // Runtime diagnostics: print exe path, working dir, PATH, and attempt to LoadLibrary
    // for common DLLs so we can see exactly which dependency is missing at startup.
#ifdef _WIN32
    auto DumpRuntimeInfo = []()
    {
        char exePath[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::cout << "[Runtime] Exe Path: " << exePath << std::endl;

        char cwd[MAX_PATH] = {0};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        std::cout << "[Runtime] Current Dir: " << cwd << std::endl;

        const char* pathEnv = getenv("PATH");
        std::cout << "[Runtime] PATH=" << (pathEnv ? pathEnv : "") << std::endl;

        const char* probes[] = {
            "assimp-vc145-mtd.dll",
            "assimp-vc145-mt.dll",
            "glfw3.dll",
            "vcruntime140d.dll",
            "vcruntime140_1d.dll",
            "msvcp140d.dll",
            "ucrtbased.dll"
        };

        for (const char* name : probes)
        {
            HMODULE h = LoadLibraryA(name);
            if (h)
            {
                std::cout << "[Runtime] Successfully loaded: " << name << std::endl;
                FreeLibrary(h);
            }
            else
            {
                DWORD err = GetLastError();
                std::cout << "[Runtime] Failed to load: " << name << " -> GetLastError=" << err << std::endl;
            }
        }
    };
    DumpRuntimeInfo();
#endif
    // ------------------------------------------------------------
    // GLFW init
    // ------------------------------------------------------------
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        g_WindowWidth,
        g_WindowHeight,
        "MyEngine",
        nullptr,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Enable vsync
    glfwSwapInterval(1);



    // ------------------------------------------------------------
    // GLAD init
    // ------------------------------------------------------------
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OpenGLDebugCallback, nullptr);
#endif

    // ------------------------------------------------------------
    // OpenGL state
    // ------------------------------------------------------------
    glViewport(0, 0, g_WindowWidth, g_WindowHeight);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);

    // ------------------------------------------------------------
    // Input init
    // ------------------------------------------------------------
    Input::Init(window, true);

    // ------------------------------------------------------------
    // Scene setup
    // ------------------------------------------------------------
    Scene scene;

    // ------------------------------------------------------------
    // Camera entity
    // ------------------------------------------------------------
    auto cameraEntity = scene.CreateEntity("Camera");

    auto& cameraTransform = cameraEntity->AddComponent<TransformComponent>();
    cameraTransform.position = glm::vec3(0.0f, 1.5f, 5.0f);
    cameraTransform.rotation = glm::vec3(0.0f);
    cameraTransform.scale = glm::vec3(1.0f);

    auto& camera = cameraEntity->AddComponent<CameraComponent>();

    camera.isPrimary = true;

    camera.fov = 60.0f;
    // CameraComponent uses nearPlane / farPlane and mouseSmoothing naming
    camera.nearPlane = 0.1f;
    camera.farPlane = 1000.0f;

    camera.yaw = -90.0f;
    camera.pitch = 0.0f;

    camera.mouseSensitivity = 0.12f;
    camera.mouseSmoothing = 30.0f;

    camera.moveSpeed = 6.0f;
    camera.sprintMultiplier = 3.0f;
    camera.slowMultiplier = 0.25f;

    camera.enableInput = true;
    camera.enableMouseLook = true;
    camera.flyMode = true;

    // ------------------------------------------------------------
    // Systems
    // ------------------------------------------------------------
    CameraSystem cameraSystem;

    MeshRendererSystem renderSystem;

    std::cout << "Controls:\n"
              << "  WASD + mouse: move/look (camera)\n"
              << "  F: toggle wireframe\n"
              << "  TAB: cycle selected cube\n"
              << "  Arrow keys: rotate directional light\n"
              << "  X/Z: increase/decrease light intensity\n"
              << "  U/J I/K O/L: adjust selected cube color (R/G/B)\n"
              << "  M: load model from assets/model.obj\n"
              << "  P: save scene to scene.json\n"
              << "  O: load scene from scene.json\n"
              << std::endl;

    // ------------------------------------------------------------
    // Create simple geometry and place objects in the scene
    // ------------------------------------------------------------
    auto litShader = std::make_shared<MyEngine::Shader>(
        "shaders/lit.vert",
        "shaders/lit.frag"
    );

#ifdef USE_IMGUI
    // Setup ImGui context (after GL is initialized)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Try to make the process per-monitor DPI aware on Windows so framebuffer
    // sizes reported by GLFW reflect the actual DPI scaling.
#ifdef _WIN32
    // This is best-effort; if the symbol is available it will set DPI awareness.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    // Compute DPI scale from GLFW framebuffer vs window size and load an
    // explicit TTF font at a pixel size accounting for DPI. This avoids
    // relying only on global scaling which can cause glyph artifacts.
    int fbw = 0, fbh = 0, ww = 0, wh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &ww, &wh);
    float dpi_scale = 1.0f;
    if (ww > 0 && wh > 0)
        dpi_scale = static_cast<float>(fbw) / static_cast<float>(ww);

    // Load a system font at a reasonable pixel size multiplied by DPI scale.
    const char* fontPath = "C:/Windows/Fonts/arial.ttf";
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, fontPath, "rb");
#else
    f = fopen(fontPath, "rb");
#endif
    if (f)
    {
        fclose(f);
        io.Fonts->AddFontFromFileTTF(fontPath, 16.0f * dpi_scale);
    }
    else
    {
        // Fall back to default font if TTF not found
        io.Fonts->AddFontDefault();
    }

    // Use 1:1 global scale and rely on the font pixel size + framebuffer scale
    // to make ImGui look correct on high-DPI displays.
    io.FontGlobalScale = 1.0f;
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    // Diagnostic output to help debug remaining issues
    std::printf("ImGui diagnostics: FontGlobalScale=%.2f DisplayFramebufferScale=(%.2f,%.2f) dpi_scale=%.2f FontsCount=%d\n",
                io.FontGlobalScale, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y,
                dpi_scale, io.Fonts->Fonts.Size);

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    std::cout << "ImGui: initialized and enabled\n";
#else
    std::cout << "ImGui: not available (build without ImGui)\n";
#endif

    // Cube geometry (8 unique vertices, 36 indices)
    std::vector<MyEngine::Vertex> cubeVertices = {
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, glm::normalize(glm::vec3(-0.5f, -0.5f, -0.5f))},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, glm::normalize(glm::vec3( 0.5f, -0.5f, -0.5f))},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, glm::normalize(glm::vec3( 0.5f,  0.5f, -0.5f))},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, glm::normalize(glm::vec3(-0.5f,  0.5f, -0.5f))},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, glm::normalize(glm::vec3(-0.5f, -0.5f,  0.5f))},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, glm::normalize(glm::vec3( 0.5f, -0.5f,  0.5f))},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, glm::normalize(glm::vec3( 0.5f,  0.5f,  0.5f))},
        {{-0.5f,  0.5f,  0.5f}, {0.2f, 0.6f, 0.8f}, glm::normalize(glm::vec3(-0.5f,  0.5f,  0.5f))}
    };

    std::vector<unsigned int> cubeIndices = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        4,0,3, 3,7,4,
        1,5,6, 6,2,1,
        4,5,1, 1,0,4,
        3,2,6, 6,7,3
    };

    auto cubeMesh = std::make_shared<MyEngine::Mesh>(cubeVertices, cubeIndices);

    // Plane geometry
    std::vector<MyEngine::Vertex> planeVertices = {
        {{-5.0f, 0.0f, -5.0f}, {0.6f, 0.6f, 0.6f}, {0.0f, 1.0f, 0.0f}},
        {{ 5.0f, 0.0f, -5.0f}, {0.6f, 0.6f, 0.6f}, {0.0f, 1.0f, 0.0f}},
        {{ 5.0f, 0.0f,  5.0f}, {0.6f, 0.6f, 0.6f}, {0.0f, 1.0f, 0.0f}},
        {{-5.0f, 0.0f,  5.0f}, {0.6f, 0.6f, 0.6f}, {0.0f, 1.0f, 0.0f}}
    };

    std::vector<unsigned int> planeIndices = { 0,1,2, 2,3,0 };

    auto planeMesh = std::make_shared<MyEngine::Mesh>(planeVertices, planeIndices);

    // Create plane entity
    {
        auto planeEntity = scene.CreateEntity("Ground");
        auto& t = planeEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(1.0f);

        // Auto-attach mesh, renderer and bounding sphere
        MyEngine::AssetManager::AttachMeshToEntity(planeEntity, planeMesh, "", litShader);
        // Optionally override bounding sphere radius for the ground if desired
        // auto& bsPlane = planeEntity->GetComponent<BoundingSphereComponent>();
        // bsPlane.center = glm::vec3(0.0f, 0.0f, 0.0f);
        // bsPlane.radius = 8.0f;
    }

    // Create a few cubes to interact with
    std::vector<std::shared_ptr<Entity>> cubes;
    for (int i = 0; i < 5; ++i)
    {
        auto cubeEntity = scene.CreateEntity("Cube");
        auto& t = cubeEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(static_cast<float>(i) - 2.0f, 0.5f, -2.0f - static_cast<float>(i));
        t.scale = glm::vec3(1.0f);

        // Auto-attach mesh, renderer and bounding sphere
        MyEngine::AssetManager::AttachMeshToEntity(cubeEntity, cubeMesh, "", litShader);
        // Set per-cube material color
        auto& mr = cubeEntity->GetComponent<MeshRendererComponent>();
        mr.albedo = glm::vec3(0.6f + 0.1f * i, 0.3f + 0.1f * i, 0.4f);

        cubes.push_back(cubeEntity);
    }

    // Create a directional light in the scene
    {
        auto lightEntity = scene.CreateEntity("DirectionalLight");
        auto& light = lightEntity->AddComponent<LightComponent>();
        light.type = LightComponent::Type::Directional;
        light.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        light.color = glm::vec3(1.0f);
        light.intensity = 1.0f;
        light.castShadows = false;
        // store pointer to the light entity as the first light in scene
    }

    int selectedCube = 0;
    bool wireframe = false;
    bool showUI = true;
    float lightYaw = -90.0f;
    float lightPitch = -20.0f;

    // ------------------------------------------------------------
    // Timing
    // ------------------------------------------------------------
    auto previousTime = std::chrono::high_resolution_clock::now();

    // ------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        // --------------------------------------------------------
        // Delta time
        // --------------------------------------------------------
        auto currentTime = std::chrono::high_resolution_clock::now();

        float deltaTime = std::chrono::duration<float>(
            currentTime - previousTime
        ).count();

        previousTime = currentTime;

        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

        // --------------------------------------------------------
        // Input
        // IMPORTANT:
        // Input::Update() must happen BEFORE glfwPollEvents()
        // --------------------------------------------------------
        Input::Update();
        glfwPollEvents();

        // --------------------------------------------------------
        // Global controls
        // --------------------------------------------------------
        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE))
        {
            glfwSetWindowShouldClose(window, true);
        }

        // Recapture mouse with left click
        if (!Input::IsMouseCaptured() &&
            Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
        {
            Input::SetMouseCaptured(true);
        }

        // --------------------------------------------------------
        // Debug controls (keyboard)
        // --------------------------------------------------------
        // Toggle wireframe
        if (Input::IsKeyPressed(GLFW_KEY_F))
        {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }

        // Toggle UI visibility (press F1)
        if (Input::IsKeyPressed(GLFW_KEY_F1))
        {
#ifdef USE_IMGUI
            showUI = !showUI;
#endif
        }

        // Cycle selected cube
        if (Input::IsKeyPressed(GLFW_KEY_TAB))
        {
            if (!cubes.empty())
            {
                selectedCube = (selectedCube + 1) % static_cast<int>(cubes.size());
            }
        }

        // Load model from assets
        if (Input::IsKeyPressed(GLFW_KEY_M))
        {
            const std::string path = "assets/model.obj";
            auto meshes = MyEngine::AssetManager::LoadModel(path);
            if (!meshes.empty())
            {
                auto ent = scene.CreateEntity("Model");
                auto& t = ent->AddComponent<TransformComponent>();
                t.position = glm::vec3(0.0f, 0.5f, -3.0f);
                t.scale = glm::vec3(1.0f);
                MyEngine::AssetManager::AttachMeshToEntity(ent, meshes[0], path, litShader);
            }
        }

        // Adjust light direction using arrow keys (yaw/pitch)
        float lightAdjustSpeed = 60.0f; // degrees per second
        if (Input::IsKeyDown(GLFW_KEY_LEFT))
            lightYaw -= lightAdjustSpeed * deltaTime;
        if (Input::IsKeyDown(GLFW_KEY_RIGHT))
            lightYaw += lightAdjustSpeed * deltaTime;
        if (Input::IsKeyDown(GLFW_KEY_UP))
            lightPitch += lightAdjustSpeed * deltaTime;
        if (Input::IsKeyDown(GLFW_KEY_DOWN))
            lightPitch -= lightAdjustSpeed * deltaTime;

        // Light intensity
        if (Input::IsKeyDown(GLFW_KEY_X))
        {
            // increase
            for (auto& e : scene.GetEntities())
                if (e && e->HasComponent<LightComponent>())
                    e->GetComponent<LightComponent>().intensity += 1.0f * deltaTime;
        }
        if (Input::IsKeyDown(GLFW_KEY_Z))
        {
            // decrease
            for (auto& e : scene.GetEntities())
                if (e && e->HasComponent<LightComponent>())
                    e->GetComponent<LightComponent>().intensity = std::max(0.0f, e->GetComponent<LightComponent>().intensity - 1.0f * deltaTime);
        }

        // Modify selected cube color (U/J = R up/down, I/K = G up/down, O/L = B up/down)
        if (!cubes.empty())
        {
            auto selected = cubes[selectedCube];
            if (selected && selected->HasComponent<MeshRendererComponent>())
            {
                auto& mr = selected->GetComponent<MeshRendererComponent>();
                float colorAdjust = 1.0f * deltaTime;
                if (Input::IsKeyDown(GLFW_KEY_U)) mr.albedo.r = std::min(1.0f, mr.albedo.r + colorAdjust);
                if (Input::IsKeyDown(GLFW_KEY_J)) mr.albedo.r = std::max(0.0f, mr.albedo.r - colorAdjust);
                if (Input::IsKeyDown(GLFW_KEY_I)) mr.albedo.g = std::min(1.0f, mr.albedo.g + colorAdjust);
                if (Input::IsKeyDown(GLFW_KEY_K)) mr.albedo.g = std::max(0.0f, mr.albedo.g - colorAdjust);
                if (Input::IsKeyDown(GLFW_KEY_O)) mr.albedo.b = std::min(1.0f, mr.albedo.b + colorAdjust);
                if (Input::IsKeyDown(GLFW_KEY_L)) mr.albedo.b = std::max(0.0f, mr.albedo.b - colorAdjust);
            }
        }

        // --------------------------------------------------------
        // Update camera
        // --------------------------------------------------------
        // Update light direction from yaw/pitch
        for (auto& e : scene.GetEntities())
            if (e && e->HasComponent<LightComponent>())
            {
                auto& L = e->GetComponent<LightComponent>();
                if (L.type == LightComponent::Type::Directional)
                {
                    float y = glm::radians(lightYaw);
                    float p = glm::radians(lightPitch);
                    L.direction.x = cos(y) * cos(p);
                    L.direction.y = sin(p);
                    L.direction.z = sin(y) * cos(p);
                    // only update first directional light
                    break;
                }
            }

        float aspectRatio = 1.0f;
        if (g_WindowHeight > 0)
        {
            aspectRatio = static_cast<float>(g_WindowWidth) /
                          static_cast<float>(g_WindowHeight);
        }

        cameraSystem.Update(scene, window, deltaTime, aspectRatio);

        glm::mat4 view = cameraSystem.GetViewMatrix();
        glm::mat4 projection = cameraSystem.GetProjectionMatrix();

        // --------------------------------------------------------
        // Render
        // --------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef USE_IMGUI
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Example UI: larger, more visible overlay window
        if (showUI)
        {
            // Keep the status overlay pinned top-left with a larger size
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_Always);
            ImGui::Begin("Engine Status", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.1f", 1.0f / std::max(0.0001f, deltaTime));
            ImGui::Text("Entities: %zu", scene.GetEntities().size());
            if (ImGui::Button("Load Model (M)"))
            {
                const std::string path = "assets/model.obj";
                auto meshes = MyEngine::AssetManager::LoadModel(path);
                if (!meshes.empty())
                {
                    auto ent = scene.CreateEntity("Model");
                    auto& t = ent->AddComponent<TransformComponent>();
                    t.position = glm::vec3(0.0f, 0.5f, -3.0f);
                    t.scale = glm::vec3(1.0f);
                    MyEngine::AssetManager::AttachMeshToEntity(ent, meshes[0], path, litShader);
                }
            }
            ImGui::End();
        }
#endif

        renderSystem.Render(scene, view, projection);

#ifdef USE_IMGUI
        // Rendering ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

        glfwSwapBuffers(window);
    }

    // ------------------------------------------------------------
    // Shutdown
    // ------------------------------------------------------------
#ifdef USE_IMGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif

    Input::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}