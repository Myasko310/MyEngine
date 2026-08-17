#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <memory>
#include <chrono>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <array>
#include <functional>
#include <fstream>

// Core
#include "core/Input.h"
#include "core/FileDialog.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "editor/EditorUndo.h"

#include "components/LightComponent.h"

// Components
#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/AudioListenerComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/JointComponent.h"
#include "components/AnimationComponent.h"
#include "components/ScriptComponent.h"

// Audio
#include "audio/AudioEngine.h"
#include "audio/AudioClip.h"
#include <AL/al.h>

// Rendering
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/MeshPrimitives.h"
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

#ifdef USE_IMGUIZMO
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#endif

// Systems
#include "systems/CameraSystem.h"
#include "systems/MeshRendererSystem.h"
#include "rendering/PostProcessPipeline.h"
#include "rendering/Skybox.h"
#include "systems/PhysicsSystem.h"
#include "systems/AudioSystem.h"
#include "systems/AnimationSystem.h"
#include "systems/ScriptSystem.h"
#include "systems/ParticleSystem.h"
#include "components/ParticleEmitterComponent.h"

// Core (picking)
#include "core/Raycast.h"

using namespace MyEngine;

// Make the initial window larger so UI and scene are more visible by default
static int g_WindowWidth = 3200;
static int g_WindowHeight = 1800;

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    g_WindowWidth = width;
    g_WindowHeight = height;

    glViewport(0, 0, width, height);
}

// ------------------------------------------------------------
// Recent scenes list persistence (simple one-path-per-line file)
// ------------------------------------------------------------
static const char* kRecentScenesFile = "recent_scenes.txt";
static const size_t kMaxRecentScenes = 5;

static std::vector<std::string> LoadRecentScenes()
{
    std::vector<std::string> recents;
    std::ifstream ifs(kRecentScenesFile);
    if (!ifs)
        return recents;

    std::string line;
    while (std::getline(ifs, line))
    {
        if (!line.empty())
            recents.push_back(line);
    }
    return recents;
}

static void SaveRecentScenes(const std::vector<std::string>& recents)
{
    std::ofstream ofs(kRecentScenesFile, std::ios::trunc);
    if (!ofs)
        return;

    for (const auto& path : recents)
        ofs << path << "\n";
}

static void AddRecentScene(std::vector<std::string>& recents, const std::string& path)
{
    if (path.empty())
        return;

    recents.erase(std::remove(recents.begin(), recents.end(), path), recents.end());
    recents.insert(recents.begin(), path);
    if (recents.size() > kMaxRecentScenes)
        recents.resize(kMaxRecentScenes);

    SaveRecentScenes(recents);
}

static void UpdateWindowTitle(GLFWwindow* window, const std::string& scenePath)
{
    std::string title = "MyEngine";
    if (!scenePath.empty())
    {
        std::filesystem::path p(scenePath);
        title += " - " + p.filename().string();
    }
    glfwSetWindowTitle(window, title.c_str());
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

    auto& audioListener = cameraEntity->AddComponent<AudioListenerComponent>();
    audioListener.isPrimary = true;

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

    // Third-person follow mode is off by default; toggled at runtime with V
    // during play (see main loop below). followTargetID is set to the
    // player entity once it exists.
    camera.thirdPerson = false;
    camera.followDistance = 5.0f;
    camera.followHeight = 2.0f;

    // ------------------------------------------------------------
    // Systems
    // ------------------------------------------------------------
    CameraSystem cameraSystem;

    MeshRendererSystem renderSystem;

    MyEngine::PostProcessPipeline postProcess;
    postProcess.Init(static_cast<unsigned int>(g_WindowWidth), static_cast<unsigned int>(g_WindowHeight));
    bool postProcessEnabled = true;

    // Skybox: rendered after opaque geometry so it only fills background
    // pixels that remain at far depth. Faces are assigned via the Skybox
    // editor panel (right/left/top/bottom/front/back image paths); until
    // then Skybox::IsLoaded() is false and Render() is a no-op, leaving the
    // plain clear color visible as before.
    MyEngine::Skybox skybox;
    bool skyboxEnabled = true;
    std::string skyboxFacePaths[6]; // +X,-X,+Y,-Y,+Z,-Z

    AnimationSystem animationSystem;
    MyEngine::ParticleSystem particleSystem;

    PhysicsSystem physicsSystem;

    AudioSystem audioSystem;

    ScriptSystem scriptSystem;

    if (!AudioEngine::Init())
    {
        std::cerr << "Failed to initialize audio engine; audio playback will be disabled." << std::endl;
    }

    std::cout << "Controls:\n"
              << "  Right-click: toggle mouse (camera control / UI interaction)\n"
              << "  WASD + mouse: move/look (when mouse captured)\n"
              << "  SPACE: play/pause physics simulation\n"
              << "  F1: toggle UI visibility\n"
              << "  F: toggle wireframe\n"
              << "  V: toggle third-person follow camera\n"
              << "  B: cross-fade to next animation clip (skinned entities)\n"
              << "  TAB: cycle selected cube\n"
              << "  DELETE: delete selected entity\n"
              << "  Arrow keys: rotate directional light\n"
              << "  X/Z: increase/decrease light intensity\n"
              << "  M: load model from assets/model.obj\n"
              << "  Ctrl+P: save scene (also in File menu)\n"
              << "  Ctrl+O: open scene (also in File menu)\n"
              << "  ESC: exit\n"
              << std::endl;

    // ------------------------------------------------------------
    // Create simple geometry and place objects in the scene
    // ------------------------------------------------------------
    auto litShader = std::make_shared<MyEngine::Shader>(
        "shaders/lit.vert",
        "shaders/lit.frag"
    );

    // Skinned variant of the lit shader for rigged/animated models (see
    // shaders/lit_skinned.vert). Reuses the same fragment shader since the
    // varyings it consumes are identical between the two vertex shaders.
    auto litSkinnedShader = std::make_shared<MyEngine::Shader>(
        "shaders/lit_skinned.vert",
        "shaders/lit.frag"
    );

    // PBR (physically based) shader for materials that want metallic/
    // roughness workflow, tangent-space normal mapping, etc. See
    // MeshRendererComponent::usePBR and shaders/pbr.vert / pbr.frag.
    auto pbrShader = std::make_shared<MyEngine::Shader>(
        "shaders/pbr.vert",
        "shaders/pbr.frag"
    );

    // Optional animated character demo: if a rigged model is present at this
    // path, load it with its skeleton/animation clips and spawn it into the
    // scene using the skinned shader. If the file is missing, the animation
    // pipeline stays idle (no entity is created) - drop a rigged .fbx/.gltf
    // file at this path to see it in action.
    {
        const std::string animatedModelPath = "assets/models/character_animated.gltf";
        MyEngine::SkinnedModelData skinnedData = MyEngine::AssetManager::LoadSkinnedModel(animatedModelPath);

        if (!skinnedData.meshes.empty() && skinnedData.skeleton && skinnedData.skeleton->GetBoneCount() > 0)
        {
            auto animatedEntity = scene.CreateEntity("AnimatedCharacter");
            auto& animTransform = animatedEntity->AddComponent<TransformComponent>();
            animTransform.position = glm::vec3(3.0f, 0.0f, 0.0f);

            MyEngine::AssetManager::AttachSkinnedModelToEntity(animatedEntity, skinnedData, litSkinnedShader, animatedModelPath);

            std::cout << "[main] Loaded animated character from " << animatedModelPath
                      << " with " << skinnedData.skeleton->GetBoneCount() << " bones and "
                      << (skinnedData.clips ? skinnedData.clips->size() : 0) << " animation clip(s)." << std::endl;
        }
        else
        {
            std::cout << "[main] No animated model found at " << animatedModelPath
                      << " - skeletal animation pipeline is idle. Drop a rigged .fbx/.gltf there to see it in action."
                      << std::endl;
        }
    }

#ifdef USE_IMGUI
    // Setup ImGui context (after GL is initialized)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Custom editor theme: softer dark blue-gray palette, rounded corners,
    // and tighter spacing so panels feel more like a modern editor (e.g.
    // Unity/Unreal/Blender) rather than the default ImGui demo look.
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;

        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
        style.IndentSpacing = 18.0f;
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 10.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.11f, 0.13f, 0.98f);
        colors[ImGuiCol_Border]                 = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.17f, 0.18f, 0.21f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.23f, 0.24f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.48f, 0.80f, 0.55f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.09f, 0.10f, 0.12f, 0.75f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.27f, 0.29f, 0.33f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.34f, 0.37f, 0.42f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.26f, 0.48f, 0.80f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.30f, 0.62f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.30f, 0.62f, 1.00f, 0.85f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.36f, 0.68f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.48f, 0.80f, 0.60f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.26f, 0.48f, 0.80f, 0.90f);
        colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.48f, 0.80f, 0.55f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.48f, 0.80f, 0.80f);
        colors[ImGuiCol_Separator]              = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.30f, 0.62f, 1.00f, 0.60f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.30f, 0.62f, 1.00f, 0.90f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.30f, 0.62f, 1.00f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.30f, 0.62f, 1.00f, 0.55f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.30f, 0.62f, 1.00f, 0.85f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.48f, 0.80f, 0.60f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.36f, 0.60f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.30f, 0.62f, 1.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.86f, 0.60f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.95f, 0.68f, 0.24f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.48f, 0.80f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.95f, 0.75f, 0.20f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.30f, 0.62f, 1.00f, 0.80f);
    }

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

    // Use shared primitive mesh data for cubes so face normals/winding
    // remain consistent with back-face culling.
    auto cubeMesh = MyEngine::MeshPrimitives::CreateCube();
    auto sphereMesh = MyEngine::MeshPrimitives::CreateSphere();
    auto planeMesh = MyEngine::MeshPrimitives::CreatePlane();

    // Create plane entity
    {
        auto planeEntity = scene.CreateEntity("Ground");
        auto& t = planeEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 0.0f, 0.0f);
        t.scale = glm::vec3(1.0f);

        // Auto-attach mesh, renderer and bounding sphere
        MyEngine::AssetManager::AttachMeshToEntity(planeEntity, planeMesh, "primitive_plane", litShader);

        // Remove the bounding sphere - we'll use a plane collider instead
        if (planeEntity->HasComponent<BoundingSphereComponent>())
        {
            planeEntity->RemoveComponent<BoundingSphereComponent>();
        }

        // Make the ground a kinematic rigidbody so objects can collide with it
        auto& rb = planeEntity->AddComponent<RigidbodyComponent>();
        rb.isKinematic = true; // Won't move or respond to forces
        rb.useGravity = false;

        // Add plane collider for infinite flat ground at Y=0
        auto& plane = planeEntity->AddComponent<PlaneColliderComponent>();
        plane.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Pointing up
        plane.distance = 0.0f; // At Y=0
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
        MyEngine::AssetManager::AttachMeshToEntity(cubeEntity, cubeMesh, "primitive_cube", litShader);
        // Set per-cube material color
        auto& mr = cubeEntity->GetComponent<MeshRendererComponent>();
        mr.albedo = glm::vec3(0.6f + 0.1f * i, 0.3f + 0.1f * i, 0.4f);

        // Attach a sample Lua script to the first cube as a scripting demo.
        if (i == 0)
        {
            auto& script = cubeEntity->AddComponent<ScriptComponent>();
            script.scriptPath = "assets/scripts/spin_and_bob.lua";
        }

        cubes.push_back(cubeEntity);
    }

    // ------------------------------------------------------------
    // Player character: a capsule-collider controlled entity that
    // showcases scale-aware colliders, collision events, and joints.
    // Movement/jump input is handled later in the main loop.
    // ------------------------------------------------------------
    std::shared_ptr<Entity> playerEntity;
    {
        playerEntity = scene.CreateEntity("Player");
        auto& t = playerEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 2.0f, 2.0f);
        // Elongate the sphere mesh vertically so it visually approximates a capsule
        // (the engine has no capsule mesh primitive yet; the actual collider below
        // is a proper capsule for physics purposes).
        t.scale = glm::vec3(0.5f, 0.9f, 0.5f);

        MyEngine::AssetManager::AttachMeshToEntity(playerEntity, sphereMesh, "primitive_sphere", litShader);
        auto& mr = playerEntity->GetComponent<MeshRendererComponent>();
        mr.albedo = glm::vec3(0.2f, 0.6f, 1.0f);

        // Replace the auto-added bounding sphere with a capsule collider sized to
        // roughly match the elongated mesh (in local/unscaled units; PhysicsSystem
        // scales these by TransformComponent::scale at runtime).
        if (playerEntity->HasComponent<BoundingSphereComponent>())
        {
            playerEntity->RemoveComponent<BoundingSphereComponent>();
        }
        auto& capsule = playerEntity->AddComponent<CapsuleColliderComponent>();
        capsule.pointA = glm::vec3(0.0f, -0.4f, 0.0f);
        capsule.pointB = glm::vec3(0.0f, 0.4f, 0.0f);
        capsule.radius = 0.5f;

        auto& rb = playerEntity->AddComponent<RigidbodyComponent>();
        rb.mass = 1.0f;
        rb.useGravity = true;
        rb.bounciness = 0.0f;
        rb.freezePositionX = false;
        rb.freezePositionZ = false;

        // Log collisions/triggers so walking into the ground, cubes, or the
        // trigger zone below is visible in the console.
        auto& events = playerEntity->AddComponent<CollisionEventsComponent>();
        events.onCollisionEnter = [](const std::shared_ptr<Entity>& other)
        {
            std::cout << "[Player] collided with " << (other ? other->GetName() : "unknown") << std::endl;
        };
        events.onTriggerEnter = [](const std::shared_ptr<Entity>& other)
        {
            std::cout << "[Player] entered trigger: " << (other ? other->GetName() : "unknown") << std::endl;
        };
        events.onTriggerExit = [](const std::shared_ptr<Entity>& other)
        {
            std::cout << "[Player] exited trigger: " << (other ? other->GetName() : "unknown") << std::endl;
        };
    }

    // Point the primary camera's follow target at the player so the
    // third-person mode (toggled with F during play) has something to orbit.
    camera.followTargetID = playerEntity->GetID();

    // A trigger volume the player can walk through to demonstrate
    // CollisionEventsComponent's OnTriggerEnter/Exit callbacks.
    {
        auto triggerEntity = scene.CreateEntity("TriggerZone");
        auto& t = triggerEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 1.0f, -1.0f);
        t.scale = glm::vec3(1.0f);

        MyEngine::AssetManager::AttachMeshToEntity(triggerEntity, cubeMesh, "primitive_cube", litShader);
        auto& mr = triggerEntity->GetComponent<MeshRendererComponent>();
        mr.albedo = glm::vec3(1.0f, 0.85f, 0.2f);

        if (triggerEntity->HasComponent<BoundingSphereComponent>())
        {
            triggerEntity->RemoveComponent<BoundingSphereComponent>();
        }
        auto& box = triggerEntity->AddComponent<BoxColliderComponent>();
        box.halfExtents = glm::vec3(1.0f, 1.0f, 1.0f);
        box.isTrigger = true;

        auto& events = triggerEntity->AddComponent<CollisionEventsComponent>();
        events.onTriggerEnter = [](const std::shared_ptr<Entity>& other)
        {
            std::cout << "[TriggerZone] enter: " << (other ? other->GetName() : "unknown") << std::endl;
        };
        events.onTriggerExit = [](const std::shared_ptr<Entity>& other)
        {
            std::cout << "[TriggerZone] exit: " << (other ? other->GetName() : "unknown") << std::endl;
        };
    }

    // A ball hanging from a fixed world anchor via a Hinge joint, so it swings
    // freely and can be bumped by the player - demonstrates JointComponent.
    {
        auto swingEntity = scene.CreateEntity("SwingingBall");
        auto& t = swingEntity->AddComponent<TransformComponent>();
        // Offset horizontally from the anchor point below so gravity creates a
        // torque and the ball swings like a pendulum on its own. A ball starting
        // perfectly beneath its anchor has zero tangential velocity and gravity
        // acts purely along the constraint axis, so the hinge would cancel all
        // motion and it would just hang frozen until externally bumped.
        t.position = glm::vec3(3.3f, 3.0f, 2.0f);
        t.scale = glm::vec3(0.4f);

        MyEngine::AssetManager::AttachMeshToEntity(swingEntity, sphereMesh, "primitive_sphere", litShader);
        auto& mr = swingEntity->GetComponent<MeshRendererComponent>();
        mr.albedo = glm::vec3(0.9f, 0.3f, 0.3f);

        auto& rb = swingEntity->AddComponent<RigidbodyComponent>();
        rb.mass = 0.5f;
        rb.useGravity = true;
        rb.drag = 0.1f;

        auto& joint = swingEntity->AddComponent<JointComponent>();
        joint.type = JointType::Hinge;
        joint.connectedEntityID = 0; // Anchor to a fixed world-space point
        joint.connectedAnchor = glm::vec3(2.5f, 4.0f, 2.0f);
        joint.hingeDistance = 1.0f;
    }

    // PBR material demo: a sphere using the metallic/roughness workflow so
    // the PBR pipeline (see shaders/pbr.vert / pbr.frag) is actually
    // exercised. Half-rough copper-ish metal, no texture maps assigned (the
    // scalar factors alone are enough to see the Cook-Torrance highlight).
    {
        auto pbrEntity = scene.CreateEntity("PbrDemoSphere");
        auto& t = pbrEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(-3.0f, 1.0f, 0.0f);

        auto sphereMesh = MyEngine::MeshPrimitives::CreateSphere();
        MyEngine::AssetManager::AttachMeshToEntity(pbrEntity, sphereMesh, "primitive_sphere", pbrShader);

        auto& mr = pbrEntity->GetComponent<MeshRendererComponent>();
        mr.usePBR = true;
        mr.albedo = glm::vec3(0.8f, 0.5f, 0.2f);
        mr.metallic = 0.8f;
        mr.roughness = 0.35f;
        mr.aoStrength = 1.0f;
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

    // Scene simulation state
    bool isPlaying = false;

    // Play/edit mode snapshot: the scene is serialized to a temp file when
    // entering play mode and restored when stopping, so simulation changes
    // (physics, scripts) don't permanently alter the edited scene.
    bool hasPlaySnapshot = false;
    const std::string playSnapshotPath =
        (std::filesystem::temp_directory_path() / "MyEngine_playmode_snapshot.scene").generic_string();

    // Scene file state
    std::string currentScenePath;
    std::vector<std::string> recentScenes = LoadRecentScenes();

    // UI state
    Entity* selectedEntity = nullptr;
    bool showSceneHierarchy = true;
    bool showInspector = true;
    bool showLightingPanel = true;
    bool showPostProcessPanel = true;
    bool showSkyboxPanel = true;
    bool showScriptingPanel = true;
    bool showPerformancePanel = true;
    bool showPhysicsPanel = true;
    bool showAssetBrowser = true;
    std::string assetBrowserPath = "assets";
    std::vector<MyEngine::ScriptSystem::GlobalScriptConfig> globalScripts = { MyEngine::ScriptSystem::GlobalScriptConfig{} };

#ifdef USE_IMGUIZMO
    // Gizmo editor state (Translate/Rotate/Scale)
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
#endif

    // Editor undo/redo state. Transform edits are captured as a single
    // command per gizmo drag (snapshot on drag start, commit on release).
    EditorUndo::UndoStack undoStack;
    bool gizmoWasUsing = false;
    uint32_t gizmoEditEntityID = 0;
    TransformComponent gizmoEditBefore;

    // Central play-state switch: snapshot the scene when starting play,
    // restore the snapshot when stopping so edits made during simulation
    // are discarded.
    auto setPlaying = [&](bool play)
    {
        if (play == isPlaying)
            return;

        if (play)
        {
            hasPlaySnapshot = MyEngine::Serialization::SaveScene(scene, playSnapshotPath, globalScripts);
            isPlaying = true;
        }
        else
        {
            isPlaying = false;
            if (hasPlaySnapshot)
            {
                // Capture the live camera's transform/look state before the scene
                // is torn down so play-mode camera movement (fly/orbit, mouse
                // look, third-person toggle) isn't lost when stopping - only the
                // simulated gameplay entities should revert to their edit-time
                // state, not the editor/gameplay camera the user was just using.
                bool hadCamera = false;
                TransformComponent savedCameraTransform;
                CameraComponent savedCamera;
                for (auto& e : scene.GetEntities())
                {
                    if (e && e->HasComponent<CameraComponent>() && e->GetComponent<CameraComponent>().isPrimary)
                    {
                        if (e->HasComponent<TransformComponent>())
                            savedCameraTransform = e->GetComponent<TransformComponent>();
                        savedCamera = e->GetComponent<CameraComponent>();
                        hadCamera = true;
                        break;
                    }
                }

                selectedEntity = nullptr;
                undoStack.Clear();
                std::vector<uint32_t> ids;
                for (auto& e : scene.GetEntities())
                    if (e) ids.push_back(e->GetID());
                for (uint32_t id : ids)
                    scene.DestroyEntity(id);
                MyEngine::Serialization::LoadScene(scene, playSnapshotPath, litShader, &globalScripts);
                hasPlaySnapshot = false;

                // The player entity was destroyed above along with the rest of the
                // scene; re-resolve it from the freshly loaded entities so gameplay
                // controls keep operating on a valid, live entity instead of a
                // dangling pointer to the destroyed one.
                playerEntity.reset();
                for (auto& e : scene.GetEntities())
                {
                    if (e && e->GetName() == "Player")
                    {
                        playerEntity = e;
                        break;
                    }
                }

                // The camera entity was likewise destroyed and recreated by the
                // reload above, so re-point the fresh camera's follow target at
                // the re-resolved player (the original `camera`/`cameraEntity`
                // references from setup now refer to destroyed objects), then
                // restore the transform/look state captured above so the camera
                // doesn't visually snap back to its edit-time pose on Stop.
                for (auto& e : scene.GetEntities())
                {
                    if (e && e->HasComponent<CameraComponent>())
                    {
                        auto& freshCamera = e->GetComponent<CameraComponent>();
                        if (playerEntity)
                            freshCamera.followTargetID = playerEntity->GetID();

                        if (hadCamera)
                        {
                            if (e->HasComponent<TransformComponent>())
                                e->GetComponent<TransformComponent>() = savedCameraTransform;

                            freshCamera.yaw = savedCamera.yaw;
                            freshCamera.pitch = savedCamera.pitch;
                            freshCamera.thirdPerson = savedCamera.thirdPerson;
                            freshCamera.followDistance = savedCamera.followDistance;
                            freshCamera.followHeight = savedCamera.followHeight;
                            freshCamera.smoothedMouseDelta = savedCamera.smoothedMouseDelta;
                        }
                        break;
                    }
                }
            }
        }
    };

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

        // Toggle mouse capture with right-click to switch between camera and UI control
        if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT))
        {
            bool captured = Input::IsMouseCaptured();
            Input::SetMouseCaptured(!captured);
        }

        // --------------------------------------------------------
        // Debug controls (keyboard)
        // --------------------------------------------------------
        // Gate all global/editor hotkeys behind ImGui keyboard capture so that
        // typing into UI text fields never triggers world/editor shortcuts.
        // Camera movement (WASD/E/Q) is intentionally NOT gated here since it is
        // gated independently inside CameraSystem, and gizmo mode keys (W/E/R)
        // are additionally restricted to when the mouse is not captured (i.e.
        // the editor/gizmo is the active input target rather than the flying camera).
#ifdef USE_IMGUI
        ImGuiIO& io = ImGui::GetIO();
        bool allowGlobalHotkeys = !io.WantCaptureKeyboard;
#else
        bool allowGlobalHotkeys = true;
#endif

        // Toggle wireframe
        if (allowGlobalHotkeys && Input::IsKeyPressed(GLFW_KEY_F))
        {
            wireframe = !wireframe;
            renderSystem.SetWireframe(wireframe);
        }

        // Toggle UI visibility (press F1) - always allowed, even while typing
        if (Input::IsKeyPressed(GLFW_KEY_F1))
        {
#ifdef USE_IMGUI
            showUI = !showUI;
#endif
        }

        // Toggle Play/Pause (press Space)
        if (allowGlobalHotkeys && Input::IsKeyPressed(GLFW_KEY_SPACE))
        {
            setPlaying(!isPlaying);
        }

        // Cycle selected cube
        if (allowGlobalHotkeys && Input::IsKeyPressed(GLFW_KEY_TAB))
        {
            if (!cubes.empty())
            {
                selectedCube = (selectedCube + 1) % static_cast<int>(cubes.size());
            }
        }

        // Delete selected entity (press Delete)
        if (allowGlobalHotkeys && Input::IsKeyPressed(GLFW_KEY_DELETE))
        {
            if (selectedEntity != nullptr)
            {
                uint32_t idToDelete = selectedEntity->GetID();
                selectedEntity = nullptr;
                scene.DestroyEntity(idToDelete);
            }
        }

#ifdef USE_IMGUIZMO
        // Gizmo operation shortcuts. Only active when not typing into an ImGui
        // text field AND the mouse is not captured by the fly camera (which
        // otherwise conflicts with W/E used for camera movement).
        {
#ifdef USE_IMGUI
            bool allowGizmoShortcuts = !io.WantTextInput && !Input::IsMouseCaptured();
#else
            bool allowGizmoShortcuts = !Input::IsMouseCaptured();
#endif
            if (allowGizmoShortcuts)
            {
                if (Input::IsKeyPressed(GLFW_KEY_W))
                    gizmoOperation = ImGuizmo::TRANSLATE;
                if (Input::IsKeyPressed(GLFW_KEY_E))
                    gizmoOperation = ImGuizmo::ROTATE;
                if (Input::IsKeyPressed(GLFW_KEY_R))
                    gizmoOperation = ImGuizmo::SCALE;
            }
        }
#endif

        // Undo/redo shortcuts (Ctrl+Z / Ctrl+Y)
        {
#ifdef USE_IMGUI
            bool allowUndoShortcuts = !io.WantTextInput;
#else
            bool allowUndoShortcuts = true;
#endif
            bool ctrlDown = Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) || Input::IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
            if (allowUndoShortcuts && ctrlDown)
            {
                if (Input::IsKeyPressed(GLFW_KEY_Z))
                    undoStack.Undo(scene);
                if (Input::IsKeyPressed(GLFW_KEY_Y))
                    undoStack.Redo(scene);
            }
        }

        // Load model from assets
        if (allowGlobalHotkeys && Input::IsKeyPressed(GLFW_KEY_M))
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

        // Save/Open scene hotkeys (Ctrl+P / Ctrl+O, mirrors File menu behavior).
        // Ctrl is required because plain O is already used for cube color adjustment.
        bool ctrlHeld = Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) || Input::IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
        if (allowGlobalHotkeys && ctrlHeld && Input::IsKeyPressed(GLFW_KEY_P))
        {
            if (currentScenePath.empty())
                currentScenePath = MyEngine::FileDialog::SaveSceneFile();

            if (!currentScenePath.empty())
            {
                MyEngine::Serialization::SaveScene(scene, currentScenePath, globalScripts);
                AddRecentScene(recentScenes, currentScenePath);
                UpdateWindowTitle(window, currentScenePath);
            }
        }

        if (allowGlobalHotkeys && ctrlHeld && Input::IsKeyPressed(GLFW_KEY_O))
        {
            std::string path = MyEngine::FileDialog::OpenSceneFile();
            if (!path.empty())
            {
                MyEngine::Serialization::LoadScene(scene, path, litShader, &globalScripts);
                selectedEntity = nullptr;
                currentScenePath = path;
                AddRecentScene(recentScenes, path);
                UpdateWindowTitle(window, currentScenePath);
            }
        }

        // Adjust light direction using arrow keys (yaw/pitch).
        // Disabled while playing so arrow keys exclusively control the
        // player character instead of fighting with light adjustment.
        float lightAdjustSpeed = 60.0f; // degrees per second
        if (allowGlobalHotkeys && !isPlaying && Input::IsKeyDown(GLFW_KEY_LEFT))
            lightYaw -= lightAdjustSpeed * deltaTime;
        if (allowGlobalHotkeys && !isPlaying && Input::IsKeyDown(GLFW_KEY_RIGHT))
            lightYaw += lightAdjustSpeed * deltaTime;
        if (allowGlobalHotkeys && !isPlaying && Input::IsKeyDown(GLFW_KEY_UP))
            lightPitch += lightAdjustSpeed * deltaTime;
        if (allowGlobalHotkeys && !isPlaying && Input::IsKeyDown(GLFW_KEY_DOWN))
            lightPitch -= lightAdjustSpeed * deltaTime;

        // Light intensity
        if (allowGlobalHotkeys && Input::IsKeyDown(GLFW_KEY_X))
        {
            // increase
            for (auto& e : scene.GetEntities())
                if (e && e->HasComponent<LightComponent>())
                    e->GetComponent<LightComponent>().intensity += 1.0f * deltaTime;
        }
        if (allowGlobalHotkeys && Input::IsKeyDown(GLFW_KEY_Z))
        {
            // decrease
            for (auto& e : scene.GetEntities())
                if (e && e->HasComponent<LightComponent>())
                    e->GetComponent<LightComponent>().intensity = std::max(0.0f, e->GetComponent<LightComponent>().intensity - 1.0f * deltaTime);
        }

        // Modify selected cube color (U/J = R up/down, I/K = G up/down, O/L = B up/down)
        if (allowGlobalHotkeys && !cubes.empty())
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

        // Keep point/spot light world position in sync with their entity's
        // transform so moving the light via the gizmo or parenting hierarchy
        // is reflected in the renderer, which reads LightComponent::position.
        for (auto& e : scene.GetEntities())
            if (e && e->HasComponent<LightComponent>() && e->HasComponent<TransformComponent>())
            {
                auto& L = e->GetComponent<LightComponent>();
                if (L.type == LightComponent::Type::Point || L.type == LightComponent::Type::Spot)
                {
                    glm::mat4 worldMatrix = TransformHierarchy::GetWorldMatrix(scene, *e);
                    L.position = glm::vec3(worldMatrix[3]);
                }
            }

        float aspectRatio = 1.0f;
        if (g_WindowHeight > 0)
        {
            aspectRatio = static_cast<float>(g_WindowWidth) /
                          static_cast<float>(g_WindowHeight);
        }

        // Update physics before camera (so camera can follow physics objects)
        if (isPlaying)
        {
            // Simple player character controller: arrow keys move on the XZ plane,
            // Right Control jumps. Restricted to play mode so it never fights with
            // editor hotkeys (arrow keys otherwise adjust the light in edit mode).
            if (playerEntity && playerEntity->HasComponent<RigidbodyComponent>())
            {
                auto& rb = playerEntity->GetComponent<RigidbodyComponent>();

                const float playerSpeed = 4.0f;
                glm::vec3 moveDir(0.0f);

                if (Input::IsKeyDown(GLFW_KEY_UP))    moveDir.z -= 1.0f;
                if (Input::IsKeyDown(GLFW_KEY_DOWN))  moveDir.z += 1.0f;
                if (Input::IsKeyDown(GLFW_KEY_LEFT))  moveDir.x -= 1.0f;
                if (Input::IsKeyDown(GLFW_KEY_RIGHT)) moveDir.x += 1.0f;

                if (glm::length(moveDir) > 0.0f)
                    moveDir = glm::normalize(moveDir);

                rb.velocity.x = moveDir.x * playerSpeed;
                rb.velocity.z = moveDir.z * playerSpeed;

                // Jump: simple heuristic since PhysicsSystem has no dedicated
                // ground-contact flag - only allow jumping when vertical velocity
                // is near zero (i.e. standing on something, not already falling/rising).
                if (Input::IsKeyPressed(GLFW_KEY_RIGHT_CONTROL) && std::abs(rb.velocity.y) < 0.1f)
                {
                    rb.velocity.y = 6.0f;
                }
            }

            // Toggle third-person follow camera with V (F is already bound to
            // wireframe toggle above). Resolved against the live primary
            // camera entity each press (rather than the original `camera`
            // reference) since play/stop reload destroys and recreates
            // entities, leaving the setup-time reference stale.
            if (Input::IsKeyPressed(GLFW_KEY_V))
            {
                for (auto& e : scene.GetEntities())
                {
                    if (e && e->HasComponent<CameraComponent>())
                    {
                        auto& cam = e->GetComponent<CameraComponent>();
                        cam.thirdPerson = !cam.thirdPerson;
                        break;
                    }
                }
            }

            // Cycle through animation clips with a cross-fade blend on any
            // skinned entity (e.g. AnimatedCharacter) using B, demonstrating
            // AnimationComponent::TransitionTo(). No-ops for entities with
            // only one clip (there's nothing to transition to).
            if (Input::IsKeyPressed(GLFW_KEY_B))
            {
                for (auto& e : scene.GetEntities())
                {
                    if (!e || !e->HasComponent<AnimationComponent>())
                        continue;

                    auto& anim = e->GetComponent<AnimationComponent>();
                    if (!anim.clips || anim.clips->size() < 2)
                        continue;

                    int nextClip = (anim.activeClipIndex + 1) % static_cast<int>(anim.clips->size());
                    anim.TransitionTo(nextClip, 0.4f);
                }
            }

            physicsSystem.OnUpdate(scene, deltaTime);
        }

        // Compute bone matrix palettes for any skinned entities every frame,
        // even while paused/in edit mode. Passing deltaTime of 0 when not
        // playing keeps the pose frozen (no playback advance) while still
        // populating AnimationComponent::boneMatrices; otherwise the palette
        // stays empty until Play is pressed and every skinned vertex gets
        // multiplied by a zero bone matrix, collapsing the mesh to the
        // origin (invisible) before Play is hit.
        animationSystem.Update(scene, isPlaying ? deltaTime : 0.0f);
        particleSystem.Update(scene, isPlaying ? deltaTime : 0.0f);

        cameraSystem.Update(scene, window, deltaTime, aspectRatio);

        audioSystem.Update(scene, deltaTime);
        scriptSystem.SetGlobalScripts(globalScripts);
        scriptSystem.OnUpdate(scene, deltaTime);

        glm::mat4 view = cameraSystem.GetViewMatrix();
        glm::mat4 projection = cameraSystem.GetProjectionMatrix();

        // --------------------------------------------------------
        // Render
        // --------------------------------------------------------
        if (postProcessEnabled)
        {
            postProcess.Resize(static_cast<unsigned int>(g_WindowWidth), static_cast<unsigned int>(g_WindowHeight));
            postProcess.BindForWriting();
        }
        else
        {
            // Explicitly bind the default framebuffer when post-processing is
            // disabled. Without this, toggling post-processing off while the
            // offscreen HDR framebuffer is still bound from a previous frame
            // (via BindForWriting()) leaves rendering targeting that
            // invisible offscreen target forever, since nothing else in the
            // non-post-process path rebinds framebuffer 0 - the window
            // appears frozen because the visible framebuffer is never
            // updated again.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, g_WindowWidth, g_WindowHeight);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef USE_IMGUI
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (showUI)
        {
            // Menu Bar (standalone). The Play/Stop control now lives in the
            // toolbar strip directly below the menu bar (see "Toolbar"
            // block after EndMainMenuBar) so it isn't duplicated here.
            if (ImGui::BeginMainMenuBar())
            {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene"))
                {
                    for (auto& e : scene.GetEntities())
                        if (e) scene.DestroyEntity(e->GetID());
                    selectedEntity = nullptr;
                    currentScenePath.clear();
                    UpdateWindowTitle(window, currentScenePath);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                {
                    std::string path = MyEngine::FileDialog::OpenSceneFile();
                    if (!path.empty())
                    {
                        MyEngine::Serialization::LoadScene(scene, path, litShader, &globalScripts);
                        selectedEntity = nullptr;
                        currentScenePath = path;
                        AddRecentScene(recentScenes, path);
                        UpdateWindowTitle(window, currentScenePath);
                    }
                }
                if (ImGui::BeginMenu("Open Recent", !recentScenes.empty()))
                {
                    for (const auto& recentPath : recentScenes)
                    {
                        if (ImGui::MenuItem(recentPath.c_str()))
                        {
                            MyEngine::Serialization::LoadScene(scene, recentPath, litShader, &globalScripts);
                            selectedEntity = nullptr;
                            currentScenePath = recentPath;
                            AddRecentScene(recentScenes, recentPath);
                            UpdateWindowTitle(window, currentScenePath);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Scene", "Ctrl+P"))
                {
                    if (currentScenePath.empty())
                        currentScenePath = MyEngine::FileDialog::SaveSceneFile();

                    if (!currentScenePath.empty())
                    {
                        MyEngine::Serialization::SaveScene(scene, currentScenePath, globalScripts);
                        AddRecentScene(recentScenes, currentScenePath);
                        UpdateWindowTitle(window, currentScenePath);
                    }
                }
                if (ImGui::MenuItem("Save Scene As..."))
                {
                    std::string path = MyEngine::FileDialog::SaveSceneFile();
                    if (!path.empty())
                    {
                        MyEngine::Serialization::SaveScene(scene, path, globalScripts);
                        currentScenePath = path;
                        AddRecentScene(recentScenes, currentScenePath);
                        UpdateWindowTitle(window, currentScenePath);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "ESC"))
                {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }

                if (ImGui::BeginMenu("View"))
                {
                    ImGui::MenuItem("Scene Hierarchy", nullptr, &showSceneHierarchy);
                    ImGui::MenuItem("Inspector", nullptr, &showInspector);
                    ImGui::MenuItem("Lighting", nullptr, &showLightingPanel);
                    ImGui::MenuItem("Post-Processing", nullptr, &showPostProcessPanel);
                    ImGui::MenuItem("Skybox", nullptr, &showSkyboxPanel);
                    ImGui::MenuItem("Scripting", nullptr, &showScriptingPanel);
                    ImGui::MenuItem("Performance", nullptr, &showPerformancePanel);
                    ImGui::MenuItem("Asset Browser", nullptr, &showAssetBrowser);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Toggle Wireframe", "F"))
                    {
                        wireframe = !wireframe;
                        renderSystem.SetWireframe(wireframe);
                    }
#ifdef USE_IMGUIZMO
                    ImGui::Separator();
                    ImGui::Text("Gizmo Mode");
                    if (ImGui::MenuItem("Translate", "W", gizmoOperation == ImGuizmo::TRANSLATE))
                        gizmoOperation = ImGuizmo::TRANSLATE;
                    if (ImGui::MenuItem("Rotate", "E", gizmoOperation == ImGuizmo::ROTATE))
                        gizmoOperation = ImGuizmo::ROTATE;
                    if (ImGui::MenuItem("Scale", "R", gizmoOperation == ImGuizmo::SCALE))
                        gizmoOperation = ImGuizmo::SCALE;
                    ImGui::Separator();
                    bool isWorldSpace = (gizmoMode == ImGuizmo::WORLD);
                    if (ImGui::MenuItem("World Space", nullptr, isWorldSpace))
                        gizmoMode = ImGuizmo::WORLD;
                    if (ImGui::MenuItem("Local Space", nullptr, !isWorldSpace))
                        gizmoMode = ImGuizmo::LOCAL;
#endif
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Create"))
                {
                    if (ImGui::MenuItem("Cube"))
                    {
                        auto ent = scene.CreateEntity("Cube");
                        auto& t = ent->AddComponent<TransformComponent>();
                        t.position = glm::vec3(0.0f, 2.0f, 0.0f); // Start at Y=2 so it falls and lands on ground
                        t.scale = glm::vec3(1.0f);
                        MyEngine::AssetManager::AttachMeshToEntity(ent, 
                            MyEngine::MeshPrimitives::CreateCube(), 
                            "primitive_cube", litShader);
                        // Use a box collider for accurate physics against the cube shape
                        auto& box = ent->AddComponent<BoxColliderComponent>();
                        box.halfExtents = glm::vec3(0.5f);
                        selectedEntity = ent.get();
                    }
                    if (ImGui::MenuItem("Sphere"))
                    {
                        auto ent = scene.CreateEntity("Sphere");
                        auto& t = ent->AddComponent<TransformComponent>();
                        t.position = glm::vec3(0.0f, 1.0f, 0.0f);
                        t.scale = glm::vec3(1.0f);
                        MyEngine::AssetManager::AttachMeshToEntity(ent, 
                            MyEngine::MeshPrimitives::CreateSphere(), 
                            "primitive_sphere", litShader);
                        selectedEntity = ent.get();
                    }
                    if (ImGui::MenuItem("Load Model...", "M"))
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
                            selectedEntity = ent.get();
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Point Light"))
                    {
                        auto ent = scene.CreateEntity("Point Light");
                        auto& t = ent->AddComponent<TransformComponent>();
                        t.position = glm::vec3(0.0f, 2.0f, 0.0f);
                        auto& light = ent->AddComponent<LightComponent>();
                        light.type = LightComponent::Type::Point;
                        light.position = t.position;
                        light.color = glm::vec3(1.0f);
                        light.intensity = 1.0f;
                        light.range = 10.0f;
                        light.castShadows = false;
                        selectedEntity = ent.get();
                    }
                    if (ImGui::MenuItem("Spot Light"))
                    {
                        auto ent = scene.CreateEntity("Spot Light");
                        auto& t = ent->AddComponent<TransformComponent>();
                        t.position = glm::vec3(0.0f, 2.0f, 0.0f);
                        auto& light = ent->AddComponent<LightComponent>();
                        light.type = LightComponent::Type::Spot;
                        light.position = t.position;
                        light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
                        light.color = glm::vec3(1.0f);
                        light.intensity = 1.0f;
                        light.range = 10.0f;
                        light.innerCone = 12.5f;
                        light.outerCone = 17.5f;
                        light.castShadows = false;
                        selectedEntity = ent.get();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            // ============================================================
            // Toolbar: a slim, always-visible strip docked directly under
            // the main menu bar exposing the most frequently used editor
            // controls (play/stop, gizmo mode, gizmo space, wireframe)
            // without needing to open a menu.
            // ============================================================
            {
                float menuBarHeight = ImGui::GetFrameHeight();
                float toolbarHeight = ImGui::GetFrameHeight() + 12.0f;

                ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarHeight));
                ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, toolbarHeight));
                ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.10f, 0.12f, 1.00f));
                ImGui::Begin("##Toolbar", nullptr, toolbarFlags);

                // Helper for a "toggle" style button that stays highlighted
                // while active, matching the accent color used elsewhere.
                auto toolbarToggleButton = [](const char* label, bool active, float width = 0.0f) -> bool
                {
                    bool pressed = false;
                    if (active)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.48f, 0.80f, 0.90f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.55f, 0.88f, 0.95f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.55f, 0.88f, 1.00f));
                    }
                    pressed = ImGui::Button(label, ImVec2(width, 0.0f));
                    if (active)
                        ImGui::PopStyleColor(3);
                    return pressed;
                };

                // --- Play / Stop -------------------------------------------------
                ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? ImVec4(0.80f, 0.30f, 0.30f, 1.00f) : ImVec4(0.30f, 0.75f, 0.35f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isPlaying ? ImVec4(0.88f, 0.36f, 0.36f, 1.00f) : ImVec4(0.36f, 0.82f, 0.42f, 1.00f));
                if (ImGui::Button(isPlaying ? "  [] Stop  " : "  > Play  "))
                {
                    setPlaying(!isPlaying);
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();

#ifdef USE_IMGUIZMO
                // --- Gizmo mode: Translate / Rotate / Scale -----------------------
                if (toolbarToggleButton("Move (W)", gizmoOperation == ImGuizmo::TRANSLATE, 84.0f))
                    gizmoOperation = ImGuizmo::TRANSLATE;
                ImGui::SameLine();
                if (toolbarToggleButton("Rotate (E)", gizmoOperation == ImGuizmo::ROTATE, 84.0f))
                    gizmoOperation = ImGuizmo::ROTATE;
                ImGui::SameLine();
                if (toolbarToggleButton("Scale (R)", gizmoOperation == ImGuizmo::SCALE, 84.0f))
                    gizmoOperation = ImGuizmo::SCALE;

                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();

                // --- Gizmo space: World / Local ------------------------------------
                bool isWorldSpaceToolbar = (gizmoMode == ImGuizmo::WORLD);
                if (toolbarToggleButton("World", isWorldSpaceToolbar, 64.0f))
                    gizmoMode = ImGuizmo::WORLD;
                ImGui::SameLine();
                if (toolbarToggleButton("Local", !isWorldSpaceToolbar, 64.0f))
                    gizmoMode = ImGuizmo::LOCAL;

                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));
                ImGui::SameLine();
#endif

                // --- Wireframe toggle ----------------------------------------------
                if (toolbarToggleButton("Wireframe (F)", wireframe, 108.0f))
                {
                    wireframe = !wireframe;
                    renderSystem.SetWireframe(wireframe);
                }

                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(3);
            }

            // ============================================================
            // Asset Browser Panel
            // ============================================================
            if (showAssetBrowser)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
                ImGui::Begin("Asset Browser", &showAssetBrowser);

                namespace fs = std::filesystem;

                // Breadcrumb: navigate back up to "assets"
                if (assetBrowserPath != "assets")
                {
                    if (ImGui::Button("<- Back"))
                    {
                        fs::path parent = fs::path(assetBrowserPath).parent_path();
                        assetBrowserPath = parent.empty() ? "assets" : parent.generic_string();
                    }
                    ImGui::SameLine();
                }
                ImGui::TextDisabled("%s", assetBrowserPath.c_str());
                ImGui::Separator();

                if (fs::exists(assetBrowserPath) && fs::is_directory(assetBrowserPath))
                {
                    // Directories first
                    for (const auto& entry : fs::directory_iterator(assetBrowserPath))
                    {
                        if (!entry.is_directory())
                            continue;
                        std::string label = "[DIR] " + entry.path().filename().string();
                        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            assetBrowserPath = entry.path().generic_string();
                        }
                    }

                    // Files with type-aware actions
                    for (const auto& entry : fs::directory_iterator(assetBrowserPath))
                    {
                        if (!entry.is_regular_file())
                            continue;

                        std::string filePath = entry.path().generic_string();
                        std::string fileName = entry.path().filename().string();
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                        ImGui::Selectable(fileName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);

                        bool doubleClicked = ImGui::IsItemHovered() &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                        if (doubleClicked)
                        {
                            if (ext == ".obj")
                            {
                                auto meshes = MyEngine::AssetManager::LoadModel(filePath);
                                if (!meshes.empty())
                                {
                                    auto ent = scene.CreateEntity(entry.path().stem().string());
                                    auto& t = ent->AddComponent<TransformComponent>();
                                    t.position = glm::vec3(0.0f, 0.5f, -3.0f);
                                    MyEngine::AssetManager::AttachMeshToEntity(ent, meshes[0], filePath, litShader);
                                    selectedEntity = ent.get();
                                }
                            }
                            else if (ext == ".wav")
                            {
                                if (selectedEntity)
                                {
                                    if (!selectedEntity->HasComponent<AudioSourceComponent>())
                                        selectedEntity->AddComponent<AudioSourceComponent>();
                                    auto& as = selectedEntity->GetComponent<AudioSourceComponent>();
                                    as.clipPath = filePath;
                                    as.clip = MyEngine::AssetManager::LoadAudioClip(filePath);
                                }
                            }
                            else if (ext == ".scene" || ext == ".json")
                            {
                                selectedEntity = nullptr;
                                undoStack.Clear();
                                if (MyEngine::Serialization::LoadScene(scene, filePath, litShader, &globalScripts))
                                {
                                    currentScenePath = filePath;
                                    AddRecentScene(recentScenes, currentScenePath);
                                    UpdateWindowTitle(window, currentScenePath);
                                }
                            }
                        }

                        if (ImGui::IsItemHovered())
                        {
                            if (ext == ".obj")
                                ImGui::SetTooltip("Double-click: add model to scene");
                            else if (ext == ".wav")
                                ImGui::SetTooltip("Double-click: assign clip to selected entity's audio source");
                            else if (ext == ".scene" || ext == ".json")
                                ImGui::SetTooltip("Double-click: open scene");
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("Folder not found: %s", assetBrowserPath.c_str());
                }

                ImGui::End();
            }

            // ============================================================
            // Scene Hierarchy Panel
            // ============================================================
            if (showSceneHierarchy)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
                ImGui::Begin("Scene Hierarchy", &showSceneHierarchy);

                if (ImGui::Button("Create Empty Entity"))
                {
                    auto ent = scene.CreateEntity("Entity");
                    ent->AddComponent<TransformComponent>();
                    selectedEntity = ent.get();
                }

                ImGui::Separator();

                // Recursive hierarchy tree: root entities (parentID == 0) at top
                // level, children nested underneath. Drag an entity onto another
                // to reparent it; drop onto the "(root)" target to unparent.
                std::function<void(const std::shared_ptr<Entity>&)> drawEntityNode =
                    [&](const std::shared_ptr<Entity>& entity)
                {
                    bool hasChildren = false;
                    for (auto& other : scene.GetEntities())
                    {
                        if (other && other != entity && other->HasComponent<TransformComponent>() &&
                            other->GetComponent<TransformComponent>().parentID == entity->GetID())
                        {
                            hasChildren = true;
                            break;
                        }
                    }

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
                    if (!hasChildren)
                        flags |= ImGuiTreeNodeFlags_Leaf;
                    if (selectedEntity == entity.get())
                        flags |= ImGuiTreeNodeFlags_Selected;

                    bool open = ImGui::TreeNodeEx(entity.get(), flags, "%s", entity->GetName().c_str());

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        selectedEntity = entity.get();
                    }

                    // Drag source: carry the entity ID
                    if (ImGui::BeginDragDropSource())
                    {
                        uint32_t draggedID = entity->GetID();
                        ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &draggedID, sizeof(uint32_t));
                        ImGui::Text("%s", entity->GetName().c_str());
                        ImGui::EndDragDropSource();
                    }

                    // Drop target: reparent the dragged entity under this one
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
                        {
                            uint32_t draggedID = *static_cast<const uint32_t*>(payload->Data);
                            auto dragged = TransformHierarchy::FindEntityByID(scene, draggedID);
                            if (dragged && dragged.get() != entity.get())
                                TransformHierarchy::SetParent(scene, *dragged, entity->GetID());
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Right-click context menu for entity
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Unparent", nullptr, false,
                            entity->HasComponent<TransformComponent>() &&
                            entity->GetComponent<TransformComponent>().parentID != 0))
                        {
                            TransformHierarchy::SetParent(scene, *entity, 0);
                        }
                        if (ImGui::MenuItem("Delete"))
                        {
                            uint32_t idToDelete = entity->GetID();
                            if (selectedEntity == entity.get())
                                selectedEntity = nullptr;
                            // Unparent any children so they don't reference a dead ID.
                            for (auto& other : scene.GetEntities())
                            {
                                if (other && other->HasComponent<TransformComponent>() &&
                                    other->GetComponent<TransformComponent>().parentID == idToDelete)
                                    TransformHierarchy::SetParent(scene, *other, 0);
                            }
                            scene.DestroyEntity(idToDelete);
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        for (auto& child : scene.GetEntities())
                        {
                            if (child && child != entity && child->HasComponent<TransformComponent>() &&
                                child->GetComponent<TransformComponent>().parentID == entity->GetID())
                            {
                                drawEntityNode(child);
                            }
                        }
                        ImGui::TreePop();
                    }
                };

                for (auto& entity : scene.GetEntities())
                {
                    if (!entity)
                        continue;
                    uint32_t parentID = entity->HasComponent<TransformComponent>()
                        ? entity->GetComponent<TransformComponent>().parentID
                        : 0;
                    if (parentID == 0)
                        drawEntityNode(entity);
                }

                // Drop area to unparent (drag onto empty space below the tree)
                ImGui::Dummy(ImVec2(-1.0f, 30.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
                    {
                        uint32_t draggedID = *static_cast<const uint32_t*>(payload->Data);
                        auto dragged = TransformHierarchy::FindEntityByID(scene, draggedID);
                        if (dragged)
                            TransformHierarchy::SetParent(scene, *dragged, 0);
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::End();
            }

            // ============================================================
            // Inspector Panel
            // ============================================================
            if (showInspector)
            {
                ImGui::SetNextWindowPos(ImVec2(g_WindowWidth - 410, 30), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
                ImGui::Begin("Inspector", &showInspector);

                if (selectedEntity)
                {
                    ImGui::Text("Entity: %s", selectedEntity->GetName().c_str());
                    ImGui::Separator();

                    // Transform Component
                    if (selectedEntity->HasComponent<TransformComponent>())
                    {
                        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& transform = selectedEntity->GetComponent<TransformComponent>();
                            TransformComponent beforeEdit = transform;
                            bool edited = false;
                            edited |= ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
                            edited |= ImGui::DragFloat3("Rotation", &transform.rotation.x, 1.0f);
                            edited |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.01f, 100.0f);
                            // Commit a single undo command when the drag/edit finishes.
                            if (edited && ImGui::IsItemDeactivatedAfterEdit())
                            {
                                undoStack.Push(std::make_unique<EditorUndo::TransformEditCommand>(
                                    selectedEntity->GetID(), beforeEdit, transform));
                            }
                        }
                    }

                    // Camera Component
                    if (selectedEntity->HasComponent<CameraComponent>())
                    {
                        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& cam = selectedEntity->GetComponent<CameraComponent>();
                            ImGui::Checkbox("Primary Camera", &cam.isPrimary);
                            ImGui::DragFloat("FOV", &cam.fov, 1.0f, 1.0f, 179.0f);
                            ImGui::DragFloat("Near Plane", &cam.nearPlane, 0.01f, 0.01f, 10.0f);
                            ImGui::DragFloat("Far Plane", &cam.farPlane, 10.0f, 10.0f, 10000.0f);
                            ImGui::DragFloat("Move Speed", &cam.moveSpeed, 0.1f, 0.1f, 50.0f);
                            ImGui::DragFloat("Mouse Sensitivity", &cam.mouseSensitivity, 0.01f, 0.01f, 1.0f);
                            ImGui::Checkbox("Enable Input", &cam.enableInput);
                            ImGui::Checkbox("Fly Mode", &cam.flyMode);

                            ImGui::Separator();
                            ImGui::Text("Third-Person Follow");
                            ImGui::Checkbox("Enable Third-Person", &cam.thirdPerson);
                            if (cam.thirdPerson)
                            {
                                ImGui::DragFloat("Follow Distance", &cam.followDistance, 0.1f, 0.5f, 50.0f);
                                ImGui::DragFloat("Follow Height", &cam.followHeight, 0.1f, -10.0f, 20.0f);

                                // Pick the follow target from any entity that has a
                                // TransformComponent (the camera itself is excluded
                                // since following itself would be meaningless).
                                std::string currentLabel = "(none)";
                                if (cam.followTargetID != 0)
                                {
                                    auto target = TransformHierarchy::FindEntityByID(scene, cam.followTargetID);
                                    if (target)
                                        currentLabel = target->GetName();
                                    else
                                        currentLabel = "(missing entity)";
                                }

                                if (ImGui::BeginCombo("Follow Target", currentLabel.c_str()))
                                {
                                    for (auto& e : scene.GetEntities())
                                    {
                                        if (!e || e.get() == selectedEntity || !e->HasComponent<TransformComponent>())
                                            continue;

                                        bool isSelected = (e->GetID() == cam.followTargetID);
                                        if (ImGui::Selectable(e->GetName().c_str(), isSelected))
                                            cam.followTargetID = e->GetID();
                                        if (isSelected)
                                            ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }
                            }
                        }
                    }

                    // Light Component
                    if (selectedEntity->HasComponent<LightComponent>())
                    {
                        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& light = selectedEntity->GetComponent<LightComponent>();

                            const char* lightTypes[] = { "Directional", "Point", "Spot" };
                            int currentType = static_cast<int>(light.type);
                            if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
                            {
                                light.type = static_cast<LightComponent::Type>(currentType);
                            }

                            ImGui::ColorEdit3("Color", &light.color.x);
                            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

                            if (light.type == LightComponent::Type::Directional)
                            {
                                ImGui::DragFloat3("Direction", &light.direction.x, 0.1f);
                                ImGui::Checkbox("Cast Shadows", &light.castShadows);
                            }
                            else if (light.type == LightComponent::Type::Point || light.type == LightComponent::Type::Spot)
                            {
                                ImGui::DragFloat3("Position", &light.position.x, 0.1f);
                                ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 1000.0f);
                                ImGui::Checkbox("Cast Shadows", &light.castShadows);

                                if (light.type == LightComponent::Type::Spot)
                                {
                                    ImGui::DragFloat("Inner Cone", &light.innerCone, 1.0f, 0.0f, 89.0f);
                                    ImGui::DragFloat("Outer Cone", &light.outerCone, 1.0f, 0.0f, 90.0f);
                                }
                            }
                        }
                    }

                    // Mesh Renderer Component
                    if (selectedEntity->HasComponent<MeshRendererComponent>())
                    {
                        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& renderer = selectedEntity->GetComponent<MeshRendererComponent>();
                            ImGui::Checkbox("Visible", &renderer.visible);

                            ImGui::Separator();
                            ImGui::Text("Material Asset");
                            if (renderer.materialPath.empty() && renderer.material && !renderer.material->GetPath().empty())
                            {
                                renderer.materialPath = renderer.material->GetPath();
                            }
                            ImGui::TextWrapped("Current: %s", renderer.materialPath.empty() ? "(inline component material)" : renderer.materialPath.c_str());

                            static char materialPathBuffer[256] = "assets/materials/default.material.json";
                            ImGui::InputText("Material Path", materialPathBuffer, sizeof(materialPathBuffer));
                            if (ImGui::Button("Load Material"))
                            {
                                std::string path = materialPathBuffer;
                                if (!path.empty())
                                {
                                    if (auto material = MyEngine::AssetManager::LoadMaterial(path))
                                    {
                                        renderer.material = material;
                                        renderer.materialPath = path;
                                        if (material->shader)
                                            renderer.shader = material->shader;
                                    }
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Create Material From Renderer"))
                            {
                                auto material = std::make_shared<MyEngine::Material>();
                                material->shader = renderer.shader;
                                if (renderer.shader)
                                {
                                    material->shaderVertexPath = renderer.shader->GetVertexPath();
                                    material->shaderFragmentPath = renderer.shader->GetFragmentPath();
                                }
                                material->albedo = renderer.albedo;
                                material->shininess = renderer.shininess;
                                material->texture = renderer.texture;
                                material->useTexture = renderer.useTexture;
                                material->usePBR = renderer.usePBR;
                                material->metallic = renderer.metallic;
                                material->roughness = renderer.roughness;
                                material->aoStrength = renderer.aoStrength;
                                material->emissive = renderer.emissive;
                                material->albedoMap = renderer.albedoMap;
                                material->normalMap = renderer.normalMap;
                                material->metallicRoughnessMap = renderer.metallicRoughnessMap;
                                material->aoMap = renderer.aoMap;
                                material->emissiveMap = renderer.emissiveMap;
                                renderer.material = material;
                                renderer.materialPath = materialPathBuffer;
                                renderer.material->SetPath(renderer.materialPath);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Clear Material"))
                            {
                                renderer.material = nullptr;
                                renderer.materialPath.clear();
                            }

                            if (renderer.material && ImGui::Button("Save Material"))
                            {
                                std::string path = materialPathBuffer;
                                if (!path.empty())
                                {
                                    renderer.materialPath = path;
                                    renderer.material->SetPath(path);
                                    if (renderer.material->shader)
                                    {
                                        renderer.material->shaderVertexPath = renderer.material->shader->GetVertexPath();
                                        renderer.material->shaderFragmentPath = renderer.material->shader->GetFragmentPath();
                                    }
                                    renderer.material->SaveToFile(path);
                                }
                            }

                            auto* material = renderer.material.get();
                            auto& editableAlbedo = material ? material->albedo : renderer.albedo;
                            float& editableShininess = material ? material->shininess : renderer.shininess;
                            bool& editableUseTexture = material ? material->useTexture : renderer.useTexture;
                            auto& editableTexture = material ? material->texture : renderer.texture;
                            bool& editableUsePBR = material ? material->usePBR : renderer.usePBR;
                            float& editableMetallic = material ? material->metallic : renderer.metallic;
                            float& editableRoughness = material ? material->roughness : renderer.roughness;
                            float& editableAO = material ? material->aoStrength : renderer.aoStrength;
                            auto& editableEmissive = material ? material->emissive : renderer.emissive;
                            auto& editableAlbedoMap = material ? material->albedoMap : renderer.albedoMap;
                            auto& editableNormalMap = material ? material->normalMap : renderer.normalMap;
                            auto& editableMetallicRoughnessMap = material ? material->metallicRoughnessMap : renderer.metallicRoughnessMap;
                            auto& editableAOMap = material ? material->aoMap : renderer.aoMap;
                            auto& editableEmissiveMap = material ? material->emissiveMap : renderer.emissiveMap;

                            ImGui::ColorEdit3("Albedo", &editableAlbedo.x);
                            ImGui::DragFloat("Shininess", &editableShininess, 1.0f, 0.0f, 256.0f);

                            ImGui::Separator();
                            ImGui::Text("Texture");
                            ImGui::Checkbox("Use Texture", &editableUseTexture);

                            if (editableTexture)
                            {
                                ImGui::Text("Path: %s", editableTexture->GetPath().c_str());
                                if (ImGui::Button("Clear Texture"))
                                {
                                    editableTexture = nullptr;
                                    editableUseTexture = false;
                                }
                            }
                            else
                            {
                                ImGui::Text("No texture loaded");
                            }

                            static std::vector<std::string> availableTextures;
                            static bool texturesScanned = false;
                            if (!texturesScanned)
                            {
                                texturesScanned = true;
                                const std::string texturesDir = "assets/textures";
                                std::error_code ec;
                                if (std::filesystem::exists(texturesDir, ec))
                                {
                                    for (const auto& entry : std::filesystem::directory_iterator(texturesDir, ec))
                                    {
                                        if (!entry.is_regular_file())
                                            continue;

                                        std::string ext = entry.path().extension().string();
                                        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                                        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
                                        {
                                            availableTextures.push_back(entry.path().generic_string());
                                        }
                                    }
                                    std::sort(availableTextures.begin(), availableTextures.end());
                                }
                            }

                            if (ImGui::Button("Refresh Texture List"))
                            {
                                texturesScanned = false;
                                availableTextures.clear();
                            }

                            std::string currentTextureName = editableTexture ? editableTexture->GetPath() : "None";
                            if (ImGui::BeginCombo("Select Texture", currentTextureName.c_str()))
                            {
                                for (const auto& texPath : availableTextures)
                                {
                                    bool isSelected = editableTexture && editableTexture->GetPath() == texPath;
                                    if (ImGui::Selectable(texPath.c_str(), isSelected))
                                    {
                                        try
                                        {
                                            editableTexture = MyEngine::AssetManager::LoadTexture(texPath);
                                            editableUseTexture = true;
                                        }
                                        catch (const std::exception& e)
                                        {
                                            std::cerr << "Failed to load texture: " << e.what() << std::endl;
                                        }
                                    }
                                    if (isSelected)
                                        ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            static char texturePath[256] = "";
                            ImGui::InputText("Texture Path", texturePath, sizeof(texturePath));
                            if (ImGui::Button("Load Texture"))
                            {
                                try
                                {
                                    editableTexture = MyEngine::AssetManager::LoadTexture(texturePath);
                                    editableUseTexture = true;
                                }
                                catch (const std::exception& e)
                                {
                                    std::cerr << "Failed to load texture: " << e.what() << std::endl;
                                }
                            }

                            ImGui::Separator();
                            ImGui::Text("PBR Material");
                            ImGui::Checkbox("Use PBR##usePbr", &editableUsePBR);

                            if (editableUsePBR)
                            {
                                if (material)
                                {
                                    if (!material->shader)
                                    {
                                        material->shader = MyEngine::AssetManager::LoadShader("shaders/pbr.vert", "shaders/pbr.frag");
                                        material->shaderVertexPath = "shaders/pbr.vert";
                                        material->shaderFragmentPath = "shaders/pbr.frag";
                                    }
                                    renderer.shader = material->shader;
                                }
                                else if (!renderer.shader)
                                {
                                    renderer.shader = MyEngine::AssetManager::LoadShader("shaders/pbr.vert", "shaders/pbr.frag");
                                }

                                ImGui::SliderFloat("Metallic", &editableMetallic, 0.0f, 1.0f);
                                ImGui::SliderFloat("Roughness", &editableRoughness, 0.04f, 1.0f);
                                ImGui::SliderFloat("AO Strength", &editableAO, 0.0f, 1.0f);
                                ImGui::ColorEdit3("Emissive", &editableEmissive.x);

                                auto pbrMapPicker = [&](const char* label, std::shared_ptr<MyEngine::Texture>& map)
                                {
                                    std::string currentName = map ? map->GetPath() : "None";
                                    if (ImGui::BeginCombo(label, currentName.c_str()))
                                    {
                                        bool noneSelected = !map;
                                        if (ImGui::Selectable("None", noneSelected))
                                        {
                                            map = nullptr;
                                        }

                                        for (const auto& texPath : availableTextures)
                                        {
                                            bool isSelected = map && map->GetPath() == texPath;
                                            if (ImGui::Selectable(texPath.c_str(), isSelected))
                                            {
                                                try
                                                {
                                                    map = MyEngine::AssetManager::LoadTexture(texPath);
                                                }
                                                catch (const std::exception& e)
                                                {
                                                    std::cerr << "Failed to load PBR map: " << e.what() << std::endl;
                                                }
                                            }
                                            if (isSelected)
                                                ImGui::SetItemDefaultFocus();
                                        }
                                        ImGui::EndCombo();
                                    }
                                };

                                pbrMapPicker("Albedo Map", editableAlbedoMap);
                                pbrMapPicker("Normal Map", editableNormalMap);
                                pbrMapPicker("Metallic/Roughness Map", editableMetallicRoughnessMap);
                                pbrMapPicker("AO Map", editableAOMap);
                                pbrMapPicker("Emissive Map", editableEmissiveMap);
                            }
                        }
                    }

                    // Rigidbody Component
                    if (selectedEntity->HasComponent<RigidbodyComponent>())
                    {
                        if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& rb = selectedEntity->GetComponent<RigidbodyComponent>();

                            ImGui::Text("Physics Properties");
                            ImGui::Separator();

                            ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.1f, 1000.0f);
                            ImGui::DragFloat("Drag", &rb.drag, 0.01f, 0.0f, 10.0f);
                            ImGui::SliderFloat("Bounciness", &rb.bounciness, 0.0f, 1.0f);

                            ImGui::Separator();
                            ImGui::Checkbox("Use Gravity", &rb.useGravity);
                            ImGui::DragFloat("Gravity Scale", &rb.gravityScale, 0.1f, -10.0f, 10.0f);

                            ImGui::Separator();
                            ImGui::Checkbox("Kinematic", &rb.isKinematic);
                            ImGui::Text("Freeze Position:");
                            ImGui::Checkbox("X##freezeX", &rb.freezePositionX); ImGui::SameLine();
                            ImGui::Checkbox("Y##freezeY", &rb.freezePositionY); ImGui::SameLine();
                            ImGui::Checkbox("Z##freezeZ", &rb.freezePositionZ);

                            ImGui::Separator();
                            ImGui::Text("Current Velocity:");
                            ImGui::Text("  (%.2f, %.2f, %.2f)", rb.velocity.x, rb.velocity.y, rb.velocity.z);

                            if (ImGui::Button("Reset Velocity"))
                            {
                                rb.velocity = glm::vec3(0.0f);
                            }

                            // Debug info
                            ImGui::Separator();
                            ImGui::Text("Debug Info:");
                            auto& transform = selectedEntity->GetComponent<TransformComponent>();
                            ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                                transform.position.x, transform.position.y, transform.position.z);

                            if (selectedEntity->HasComponent<BoundingSphereComponent>())
                            {
                                auto& bs = selectedEntity->GetComponent<BoundingSphereComponent>();
                                ImGui::Text("Bounding Sphere Radius: %.2f", bs.radius);
                                ImGui::Text("Bottom Y: %.2f", transform.position.y - bs.radius);
                            }
                            if (selectedEntity->HasComponent<BoxColliderComponent>())
                            {
                                auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
                                ImGui::Text("Box Half-Extents: (%.2f, %.2f, %.2f)", box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
                                ImGui::Text("Bottom Y: %.2f", transform.position.y + box.center.y - box.halfExtents.y);
                            }
                        }
                    }
                    else
                    {
                        // Add Rigidbody button
                        if (ImGui::Button("Add Rigidbody"))
                        {
                            selectedEntity->AddComponent<RigidbodyComponent>();
                            // Also ensure a collider shape exists for collisions
                            if (!selectedEntity->HasComponent<BoundingSphereComponent>() &&
                                !selectedEntity->HasComponent<BoxColliderComponent>())
                            {
                                selectedEntity->AddComponent<BoundingSphereComponent>();
                            }
                        }
                    }

                    // Box Collider Component
                    if (selectedEntity->HasComponent<BoxColliderComponent>())
                    {
                        if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
                            ImGui::DragFloat3("Center Offset", &box.center.x, 0.05f);
                            ImGui::DragFloat3("Half Extents", &box.halfExtents.x, 0.05f, 0.01f, 100.0f);
                            ImGui::Checkbox("Is Trigger", &box.isTrigger);

                            if (ImGui::Button("Remove Box Collider"))
                            {
                                selectedEntity->RemoveComponent<BoxColliderComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Box Collider"))
                        {
                            auto& box = selectedEntity->AddComponent<BoxColliderComponent>();
                            box.halfExtents = glm::vec3(0.5f);
                            // A box collider replaces a bounding-sphere collider for physics purposes
                            if (selectedEntity->HasComponent<BoundingSphereComponent>())
                            {
                                selectedEntity->RemoveComponent<BoundingSphereComponent>();
                            }
                        }
                    }

                    // Collision Events Component (trigger/collision callbacks for gameplay testing)
                    if (selectedEntity->HasComponent<CollisionEventsComponent>())
                    {
                        if (ImGui::CollapsingHeader("Collision Events", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::TextWrapped("Logs Enter/Exit events for collisions and triggers to the console.");

                            if (ImGui::Button("Remove Collision Events"))
                            {
                                selectedEntity->RemoveComponent<CollisionEventsComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Collision Events"))
                        {
                            auto& events = selectedEntity->AddComponent<CollisionEventsComponent>();
                            uint32_t selfID = selectedEntity->GetID();
                            std::string selfName = selectedEntity->GetName();
                            events.onCollisionEnter = [selfID, selfName](const std::shared_ptr<Entity>& other)
                            {
                                std::cout << "[Collision] " << selfName << " (id " << selfID << ") entered collision with "
                                    << (other ? other->GetName() : "unknown") << std::endl;
                            };
                            events.onCollisionExit = [selfID, selfName](const std::shared_ptr<Entity>& other)
                            {
                                std::cout << "[Collision] " << selfName << " (id " << selfID << ") exited collision with "
                                    << (other ? other->GetName() : "unknown") << std::endl;
                            };
                            events.onTriggerEnter = [selfID, selfName](const std::shared_ptr<Entity>& other)
                            {
                                std::cout << "[Trigger] " << selfName << " (id " << selfID << ") entered trigger with "
                                    << (other ? other->GetName() : "unknown") << std::endl;
                            };
                            events.onTriggerExit = [selfID, selfName](const std::shared_ptr<Entity>& other)
                            {
                                std::cout << "[Trigger] " << selfName << " (id " << selfID << ") exited trigger with "
                                    << (other ? other->GetName() : "unknown") << std::endl;
                            };
                        }
                    }

                    // Joint Component (Fixed/Spring/Hinge constraints between entities)
                    if (selectedEntity->HasComponent<JointComponent>())
                    {
                        if (ImGui::CollapsingHeader("Joint", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& joint = selectedEntity->GetComponent<JointComponent>();

                            ImGui::Checkbox("Enabled", &joint.enabled);

                            const char* jointTypeNames[] = { "Fixed", "Spring", "Hinge" };
                            int jointTypeIndex = static_cast<int>(joint.type);
                            if (ImGui::Combo("Type", &jointTypeIndex, jointTypeNames, IM_ARRAYSIZE(jointTypeNames)))
                            {
                                joint.type = static_cast<JointType>(jointTypeIndex);
                            }

                            // Connected entity selection (0 = world-space anchor point)
                            std::string connectedLabel = "<World Anchor>";
                            if (joint.connectedEntityID != 0)
                            {
                                auto connected = TransformHierarchy::FindEntityByID(scene, joint.connectedEntityID);
                                connectedLabel = connected ? connected->GetName() : "<Missing Entity>";
                            }
                            if (ImGui::BeginCombo("Connected To", connectedLabel.c_str()))
                            {
                                bool isWorldSelected = (joint.connectedEntityID == 0);
                                if (ImGui::Selectable("<World Anchor>", isWorldSelected))
                                {
                                    joint.connectedEntityID = 0;
                                }
                                for (const auto& other : scene.GetEntities())
                                {
                                    if (!other || other->GetID() == selectedEntity->GetID())
                                        continue;
                                    bool isSelected = (other->GetID() == joint.connectedEntityID);
                                    if (ImGui::Selectable(other->GetName().c_str(), isSelected))
                                    {
                                        joint.connectedEntityID = other->GetID();
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            ImGui::DragFloat3("Anchor Offset", &joint.anchor.x, 0.05f);
                            ImGui::DragFloat3("Connected Anchor", &joint.connectedAnchor.x, 0.05f);

                            if (joint.type == JointType::Spring)
                            {
                                ImGui::DragFloat("Rest Length", &joint.restLength, 0.05f, 0.0f, 100.0f);
                                ImGui::DragFloat("Stiffness", &joint.stiffness, 0.5f, 0.0f, 1000.0f);
                                ImGui::DragFloat("Damping", &joint.damping, 0.1f, 0.0f, 100.0f);
                            }
                            else if (joint.type == JointType::Hinge)
                            {
                                ImGui::DragFloat("Hinge Distance", &joint.hingeDistance, 0.05f, 0.0f, 100.0f);
                            }

                            if (ImGui::Button("Remove Joint"))
                            {
                                selectedEntity->RemoveComponent<JointComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Joint"))
                        {
                            selectedEntity->AddComponent<JointComponent>();
                        }
                    }

                    // Audio Source Component
                    if (selectedEntity->HasComponent<AudioSourceComponent>())
                    {
                        if (ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& source = selectedEntity->GetComponent<AudioSourceComponent>();

                            // Clip selection
                            if (source.clip)
                                ImGui::Text("Clip: %s", source.clipPath.c_str());
                            else
                                ImGui::Text("No clip loaded");

                            // Cache the list of available clips found under assets/audio
                            static std::vector<std::string> availableClips;
                            static bool clipsScanned = false;
                            if (!clipsScanned)
                            {
                                clipsScanned = true;
                                availableClips.clear();
                                const std::string audioDir = "assets/audio";
                                if (std::filesystem::exists(audioDir))
                                {
                                    for (const auto& entry : std::filesystem::directory_iterator(audioDir))
                                    {
                                        if (entry.is_regular_file() && entry.path().extension() == ".wav")
                                            availableClips.push_back(entry.path().generic_string());
                                    }
                                }
                            }

                            if (ImGui::Button("Rescan Clips"))
                                clipsScanned = false;

                            if (ImGui::BeginCombo("Clip##audioClipCombo", source.clipPath.empty() ? "<select clip>" : source.clipPath.c_str()))
                            {
                                for (const auto& clipPath : availableClips)
                                {
                                    bool isSelected = (clipPath == source.clipPath);
                                    if (ImGui::Selectable(clipPath.c_str(), isSelected))
                                    {
                                        auto newClip = AssetManager::LoadAudioClip(clipPath);
                                        if (newClip && newClip->IsValid())
                                        {
                                            // Release the old OpenAL source so it gets recreated
                                            // with the new buffer on the next AudioSystem update.
                                            if (source.sourceID != 0)
                                            {
                                                alSourceStop(source.sourceID);
                                                alDeleteSources(1, &source.sourceID);
                                                source.sourceID = 0;
                                            }
                                            source.clip = newClip;
                                            source.clipPath = clipPath;
                                            source.isPlaying = false;
                                        }
                                    }
                                    if (isSelected)
                                        ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            ImGui::SliderFloat("Volume", &source.volume, 0.0f, 1.0f);
                            ImGui::DragFloat("Pitch", &source.pitch, 0.01f, 0.1f, 4.0f);
                            ImGui::Checkbox("Loop", &source.loop);
                            ImGui::Checkbox("Auto Play", &source.autoPlay);

                            ImGui::Separator();
                            ImGui::Checkbox("Spatial (3D)", &source.spatial);
                            if (source.spatial)
                            {
                                ImGui::DragFloat("Min Distance", &source.minDistance, 0.1f, 0.1f, 1000.0f);
                                ImGui::DragFloat("Max Distance", &source.maxDistance, 1.0f, 1.0f, 10000.0f);
                            }

                            ImGui::Separator();
                            if (source.clip && source.clip->IsValid())
                            {
                                if (!source.isPlaying)
                                {
                                    if (ImGui::Button("Play"))
                                        source.playRequested = true;
                                }
                                else
                                {
                                    if (ImGui::Button("Stop"))
                                        source.stopRequested = true;
                                }
                                ImGui::SameLine();
                                ImGui::Text(source.isPlaying ? "Playing" : "Stopped");
                            }

                            if (ImGui::Button("Remove Audio Source"))
                            {
                                if (source.sourceID != 0)
                                {
                                    alSourceStop(source.sourceID);
                                    alDeleteSources(1, &source.sourceID);
                                    source.sourceID = 0;
                                }
                                selectedEntity->RemoveComponent<AudioSourceComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Audio Source"))
                        {
                            selectedEntity->AddComponent<AudioSourceComponent>();
                        }
                    }

                    // Script Component
                    if (selectedEntity->HasComponent<ScriptComponent>())
                    {
                        if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& script = selectedEntity->GetComponent<ScriptComponent>();

                            ImGui::Checkbox("Enabled##script", &script.enabled);
                            ImGui::Checkbox("Auto Start##script", &script.autoStart);

                            std::string scriptDisplay = script.scriptPath.empty() ? "(none)" : script.scriptPath;
                            ImGui::TextWrapped("Path: %s", scriptDisplay.c_str());

                            if (ImGui::Button("Browse Script..."))
                            {
                                std::string path = MyEngine::FileDialog::OpenScriptFile();
                                if (!path.empty())
                                {
                                    script.scriptPath = path;
                                    script.requestReload = true;
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Reload Script"))
                            {
                                script.requestReload = true;
                            }

                            if (ImGui::Button("Clear Script Path"))
                            {
                                script.scriptPath.clear();
                                script.requestReload = true;
                            }

                            if (ImGui::Button("Remove Script Component"))
                            {
                                selectedEntity->RemoveComponent<ScriptComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Script Component"))
                        {
                            selectedEntity->AddComponent<ScriptComponent>();
                        }
                    }

                    // Audio Listener Component
                    if (selectedEntity->HasComponent<AudioListenerComponent>())
                    {
                        if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& listener = selectedEntity->GetComponent<AudioListenerComponent>();
                            ImGui::Checkbox("Primary Listener", &listener.isPrimary);
                            ImGui::SliderFloat("Gain", &listener.gain, 0.0f, 2.0f);

                            if (ImGui::Button("Remove Audio Listener"))
                            {
                                selectedEntity->RemoveComponent<AudioListenerComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Audio Listener"))
                        {
                            selectedEntity->AddComponent<AudioListenerComponent>();
                        }
                    }

                    // ---- Particle Emitter Component ----
                    if (selectedEntity->HasComponent<ParticleEmitterComponent>())
                    {
                        if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& emitter = selectedEntity->GetComponent<ParticleEmitterComponent>();

                            ImGui::Checkbox("Emitting", &emitter.emitting);

                            if (ImGui::SliderInt("Max Particles", &emitter.maxParticles, 1, 10000))
                                emitter.poolDirty = true;

                            ImGui::SliderFloat("Spawn Rate", &emitter.spawnRate, 0.0f, 500.0f);
                            ImGui::SliderFloat("Lifetime", &emitter.lifetime, 0.05f, 20.0f);
                            ImGui::SliderFloat("Lifetime Variance", &emitter.lifetimeVariance, 0.0f, 5.0f);

                            ImGui::Separator();
                            ImGui::Text("Emission");
                            ImGui::SliderFloat3("Direction##emit", &emitter.emitDirection.x, -1.0f, 1.0f);
                            ImGui::SliderFloat("Speed", &emitter.emitSpeed, 0.0f, 30.0f);
                            ImGui::SliderFloat("Speed Variance", &emitter.emitSpeedVariance, 0.0f, 10.0f);
                            ImGui::SliderFloat("Spread Angle", &emitter.spreadAngle, 0.0f, 180.0f);
                            ImGui::SliderFloat3("Gravity##emit", &emitter.gravity.x, -20.0f, 20.0f);

                            ImGui::Separator();
                            ImGui::Text("Appearance");
                            ImGui::ColorEdit4("Color Start", &emitter.colorStart.r);
                            ImGui::ColorEdit4("Color End",   &emitter.colorEnd.r);
                            ImGui::SliderFloat("Size Start", &emitter.sizeStart, 0.0f, 5.0f);
                            ImGui::SliderFloat("Size End",   &emitter.sizeEnd,   0.0f, 5.0f);

                            // Count alive particles for debug info
                            int alive = 0;
                            for (const auto& p : emitter.particles)
                                if (p.alive) ++alive;
                            ImGui::Text("Alive: %d / %d", alive, emitter.maxParticles);

                            if (ImGui::Button("Remove Particle Emitter"))
                                selectedEntity->RemoveComponent<ParticleEmitterComponent>();
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Particle Emitter"))
                        {
                            auto& emitter = selectedEntity->AddComponent<ParticleEmitterComponent>();
                            emitter.poolDirty = true;
                        }
                    }
                }
                    else
                    {
                        ImGui::Text("No entity selected");
                        ImGui::Text("Select an entity from the hierarchy");
                    }

                    // Delete entity button at bottom of inspector
                    if (selectedEntity)
                    {
                        ImGui::Separator();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                        if (ImGui::Button("Delete Entity (DEL)", ImVec2(-1, 0)))
                        {
                            uint32_t idToDelete = selectedEntity->GetID();
                            selectedEntity = nullptr;
                            scene.DestroyEntity(idToDelete);
                        }
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::End();
                }

            // ============================================================
            // Lighting Panel
            // ============================================================
            if (showLightingPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(10, g_WindowHeight - 360), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
                ImGui::Begin("Lighting", &showLightingPanel);

                ImGui::Text("Directional Light Control");
                ImGui::Separator();

                if (ImGui::SliderFloat("Light Yaw", &lightYaw, -180.0f, 180.0f))
                {
                    // Update will happen in main loop
                }

                if (ImGui::SliderFloat("Light Pitch", &lightPitch, -89.0f, 89.0f))
                {
                    // Update will happen in main loop
                }

                ImGui::Separator();
                ImGui::Text("Shadow Settings");
                bool dirShadowsEnabled = renderSystem.GetShadowsEnabled();
                if (ImGui::Checkbox("Directional Shadows", &dirShadowsEnabled))
                    renderSystem.SetShadowsEnabled(dirShadowsEnabled);

                bool pointShadowsEnabled = renderSystem.GetPointShadowsEnabled();
                if (ImGui::Checkbox("Point Light Shadows", &pointShadowsEnabled))
                    renderSystem.SetPointShadowsEnabled(pointShadowsEnabled);

                unsigned int pointShadowSize = renderSystem.GetPointShadowSize();
                int pointShadowSizeInt = static_cast<int>(pointShadowSize);
                if (ImGui::SliderInt("Point Shadow Size", &pointShadowSizeInt, 256, 2048))
                {
                    unsigned int snapped = static_cast<unsigned int>(pointShadowSizeInt);
                    if (snapped <= 384) snapped = 256;
                    else if (snapped <= 768) snapped = 512;
                    else if (snapped <= 1536) snapped = 1024;
                    else snapped = 2048;
                    renderSystem.SetPointShadowSize(snapped);
                }

                float pointShadowBias = renderSystem.GetPointShadowBias();
                if (ImGui::SliderFloat("Point Shadow Bias", &pointShadowBias, 0.001f, 0.1f, "%.4f"))
                    renderSystem.SetPointShadowBias(pointShadowBias);

                ImGui::Separator();
                ImGui::Text("Scene Lights:");

                int lightIndex = 0;
                for (auto& entity : scene.GetEntities())
                {
                    if (entity->HasComponent<LightComponent>())
                    {
                        auto& light = entity->GetComponent<LightComponent>();
                        ImGui::PushID(lightIndex++);

                        if (ImGui::TreeNode(entity->GetName().c_str()))
                        {
                            ImGui::ColorEdit3("Color", &light.color.x);
                            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }
                }

                ImGui::End();
            }

            // ============================================================
            // Post-Processing Panel
            // ============================================================
            if (showPostProcessPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 800), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
                ImGui::Begin("Post-Processing", &showPostProcessPanel);

                ImGui::Checkbox("Enabled", &postProcessEnabled);

                bool bloom = postProcess.GetBloomEnabled();
                if (ImGui::Checkbox("Bloom", &bloom))
                    postProcess.SetBloomEnabled(bloom);

                float exposure = postProcess.GetExposure();
                if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 5.0f))
                    postProcess.SetExposure(exposure);

                float threshold = postProcess.GetBloomThreshold();
                if (ImGui::SliderFloat("Bloom Threshold", &threshold, 0.0f, 5.0f))
                    postProcess.SetBloomThreshold(threshold);

                float bloomIntensity = postProcess.GetBloomIntensity();
                if (ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 3.0f))
                    postProcess.SetBloomIntensity(bloomIntensity);

                ImGui::End();
            }

            // ============================================================
            // Skybox Panel
            // ============================================================
            if (showSkyboxPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(340, 800), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
                ImGui::Begin("Skybox", &showSkyboxPanel);

                ImGui::Checkbox("Enabled", &skyboxEnabled);
                ImGui::TextWrapped(skybox.IsLoaded()
                    ? "Cubemap loaded."
                    : "No cubemap loaded - assign all 6 face images below.");

                if (ImGui::Button("Use assets/skybox defaults"))
                {
                    skyboxFacePaths[0] = "assets/skybox/right.png";
                    skyboxFacePaths[1] = "assets/skybox/left.png";
                    skyboxFacePaths[2] = "assets/skybox/top.png";
                    skyboxFacePaths[3] = "assets/skybox/bottom.png";
                    skyboxFacePaths[4] = "assets/skybox/front.png";
                    skyboxFacePaths[5] = "assets/skybox/back.png";
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear Faces"))
                {
                    for (auto& p : skyboxFacePaths)
                        p.clear();
                }

                static const char* faceLabels[6] = { "Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)" };
                bool anyChanged = false;
                for (int i = 0; i < 6; ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text("%s", faceLabels[i]);
                    ImGui::SameLine(140);
                    std::string displayPath = skyboxFacePaths[i].empty() ? "(none)" : skyboxFacePaths[i];
                    ImGui::TextWrapped("%s", displayPath.c_str());
                    if (ImGui::Button("Browse..."))
                    {
                        std::string path = MyEngine::FileDialog::OpenImageFile();
                        if (!path.empty())
                        {
                            skyboxFacePaths[i] = path;
                            anyChanged = true;
                        }
                    }
                    ImGui::PopID();
                }

                bool allAssigned = std::all_of(std::begin(skyboxFacePaths), std::end(skyboxFacePaths),
                    [](const std::string& p) { return !p.empty(); });

                if (!allAssigned)
                    ImGui::TextDisabled("Assign all 6 faces to enable loading.");

                ImGui::BeginDisabled(!allAssigned);
                if (ImGui::Button("Load Skybox") || (anyChanged && allAssigned))
                {
                    std::array<std::string, 6> faces = {
                        skyboxFacePaths[0], skyboxFacePaths[1], skyboxFacePaths[2],
                        skyboxFacePaths[3], skyboxFacePaths[4], skyboxFacePaths[5]
                    };
                    if (!skybox.Load(faces))
                    {
                        std::cerr << "[Skybox] Load failed - verify all face paths exist and are valid images." << std::endl;
                    }
                }
                ImGui::EndDisabled();

                ImGui::End();
            }

            // ============================================================
            // Scripting Panel
            // ============================================================
            if (showScriptingPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(710, 800), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
                ImGui::Begin("Scripting", &showScriptingPanel);

                ImGui::TextWrapped("Global scripts run once per frame (OnGlobalStart / OnGlobalUpdate). Entity scripts still run from ScriptComponent.");
                ImGui::Separator();

                for (size_t i = 0; i < globalScripts.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    auto& gs = globalScripts[i];

                    ImGui::Text("Global Script %zu", i + 1);
                    ImGui::Checkbox("Enabled", &gs.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Auto Start", &gs.autoStart);

                    std::string displayPath = gs.scriptPath.empty() ? "(none)" : gs.scriptPath;
                    ImGui::TextWrapped("Path: %s", displayPath.c_str());

                    const bool hasPath = !gs.scriptPath.empty();
                    const char* statusText = !hasPath ? "Missing script path" : (!gs.enabled ? "Disabled" : (gs.requestReload ? "Reload pending" : "Ready"));
                    ImVec4 statusColor = !hasPath ? ImVec4(1.00f, 0.60f, 0.20f, 1.0f) : (!gs.enabled ? ImVec4(0.75f, 0.75f, 0.75f, 1.0f) : (gs.requestReload ? ImVec4(1.00f, 0.90f, 0.20f, 1.0f) : ImVec4(0.30f, 1.00f, 0.50f, 1.0f)));
                    ImGui::TextColored(statusColor, "Status: %s", statusText);

                    if (ImGui::Button("Browse Lua..."))
                    {
                        std::string path = MyEngine::FileDialog::OpenScriptFile();
                        if (!path.empty())
                        {
                            gs.scriptPath = path;
                            gs.requestReload = true;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reload"))
                    {
                        gs.requestReload = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear"))
                    {
                        gs.scriptPath.clear();
                        gs.requestReload = true;
                    }

                    if (globalScripts.size() > 1)
                    {
                        if (i > 0)
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Move Up"))
                            {
                                std::swap(globalScripts[i], globalScripts[i - 1]);
                                ImGui::PopID();
                                break;
                            }
                        }

                        if (i + 1 < globalScripts.size())
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Move Down"))
                            {
                                std::swap(globalScripts[i], globalScripts[i + 1]);
                                ImGui::PopID();
                                break;
                            }
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        globalScripts.erase(globalScripts.begin() + static_cast<std::ptrdiff_t>(i));
                        ImGui::PopID();
                        break;
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (ImGui::Button("Add Global Script"))
                {
                    globalScripts.emplace_back();
                }

                ImGui::End();
            }

            // ============================================================
            // Performance Panel
            // ============================================================
            if (showPerformancePanel)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
                ImGui::Begin("Performance", &showPerformancePanel);

                float fps = 1.0f / std::max(0.0001f, deltaTime);
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);

                // Mouse mode indicator
                ImGui::Separator();
                bool mouseCaptured = Input::IsMouseCaptured();
                if (mouseCaptured)
                {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Mouse: CAMERA CONTROL");
                    ImGui::Text("(Right-click to interact with UI)");
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Mouse: UI INTERACTION");
                    ImGui::Text("(Right-click for camera control)");
                }

                ImGui::Separator();

                ImGui::Text("Scene Stats:");
                ImGui::Text("  Entities: %zu", scene.GetEntities().size());

                int meshCount = 0;
                int lightCount = 0;
                int cameraCount = 0;

                for (auto& entity : scene.GetEntities())
                {
                    if (entity->HasComponent<MeshRendererComponent>()) meshCount++;
                    if (entity->HasComponent<LightComponent>()) lightCount++;
                    if (entity->HasComponent<CameraComponent>()) cameraCount++;
                }

                ImGui::Text("  Meshes: %d", meshCount);
                ImGui::Text("  Lights: %d", lightCount);
                ImGui::Text("  Cameras: %d", cameraCount);

                ImGui::Separator();
                ImGui::Text("Viewport: %dx%d", g_WindowWidth, g_WindowHeight);
                ImGui::Checkbox("Wireframe", &wireframe);
                renderSystem.SetWireframe(wireframe);

                ImGui::End();
            }

            // ============================================================
            // Physics Panel
            // ============================================================
            if (showPhysicsPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 700), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
                ImGui::Begin("Physics", &showPhysicsPanel);

                ImGui::Text("Physics System");
                ImGui::Separator();

                // Play state indicator
                ImGui::PushStyleColor(ImGuiCol_Text, isPlaying ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text(isPlaying ? "RUNNING" : "PAUSED");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button(isPlaying ? "Stop" : "Play"))
                {
                    setPlaying(!isPlaying);
                }
                ImGui::Separator();

                // Gravity controls
                ImGui::DragFloat3("Gravity", &physicsSystem.gravity.x, 0.1f, -50.0f, 50.0f);

                // Timestep controls
                ImGui::DragFloat("Fixed Timestep", &physicsSystem.fixedTimestep, 0.001f, 0.001f, 0.1f, "%.3f");
                ImGui::SliderInt("Max Substeps", &physicsSystem.maxSubsteps, 1, 10);

                // Collision toggle
                ImGui::Checkbox("Enable Collisions", &physicsSystem.enableCollisions);

                ImGui::Separator();
                ImGui::Text("Stats:");
                ImGui::Text("  Collision Checks: %d", physicsSystem.collisionChecks);
                ImGui::Text("  Collisions: %d", physicsSystem.collisionsDetected);

                ImGui::End();
            }
        }
#endif

        #ifdef USE_IMGUIZMO
        // --------------------------------------------------------
        // Transform gizmo for the selected entity
        // --------------------------------------------------------
        // NOTE: This must run BEFORE mouse picking below. ImGuizmo::IsOver()/
        // IsUsing() reflect the state computed by the last Manipulate() call.
        // If picking ran first, it would read stale hover/use state from the
        // previous frame (last frame's mouse position), causing clicks on the
        // gizmo to be misinterpreted as scene picks - deselecting the entity
        // (and thus the gizmo) before it could ever be dragged.
        if (selectedEntity && selectedEntity->HasComponent<TransformComponent>())
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::BeginFrame();
            // SetDrawlist() with no window context (i.e. not inside an active
            // ImGui::Begin()/End() block) does not reliably bind to a valid
            // drawlist, which made the gizmo's interactive draw list not
            // properly overlay the viewport for mouse hit-testing. Explicitly
            // draw to the foreground drawlist, which always exists and is
            // rendered on top of every other window.
            ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

            // ImGuizmo operates in the same coordinate space as ImGui's IO
            // (screen/window coordinates), NOT framebuffer pixel coordinates.
            // g_WindowWidth/g_WindowHeight are updated from the framebuffer size
            // (glfwGetFramebufferSize via FramebufferSizeCallback), which differs
            // from window size on DPI-scaled displays. Using the mismatched size
            // here made the gizmo's hit-testing rect not line up with the mouse
            // cursor position ImGui reports, so it could never be clicked/dragged.
            int windowW = 0, windowH = 0;
            glfwGetWindowSize(window, &windowW, &windowH);
            ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(windowW), static_cast<float>(windowH));

            auto& gizmoTransform = selectedEntity->GetComponent<TransformComponent>();
            // Manipulate in world space; for parented entities the world matrix
            // includes the parent chain, and edits are converted back to local.
            glm::mat4 parentWorld(1.0f);
            if (gizmoTransform.parentID != 0)
            {
                auto parent = TransformHierarchy::FindEntityByID(scene, gizmoTransform.parentID);
                if (parent)
                    parentWorld = TransformHierarchy::GetWorldMatrix(scene, *parent);
            }
            glm::mat4 gizmoMatrix = parentWorld * gizmoTransform.GetMatrix();

            ImGuizmo::Manipulate(
                glm::value_ptr(view),
                glm::value_ptr(projection),
                gizmoOperation,
                gizmoMode,
                glm::value_ptr(gizmoMatrix)
            );

            if (ImGuizmo::IsUsing())
            {
                // Snapshot the transform on the first frame of a drag so the
                // whole drag becomes a single undoable command.
                if (!gizmoWasUsing)
                {
                    gizmoWasUsing = true;
                    gizmoEditEntityID = selectedEntity->GetID();
                    gizmoEditBefore = gizmoTransform;
                }

                glm::mat4 localMatrix = glm::inverse(parentWorld) * gizmoMatrix;

                float translation[3];
                float rotation[3];
                float scale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), translation, rotation, scale);

                gizmoTransform.position = glm::vec3(translation[0], translation[1], translation[2]);
                gizmoTransform.rotation = glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2]));
                gizmoTransform.scale = glm::vec3(scale[0], scale[1], scale[2]);
            }
            else if (gizmoWasUsing)
            {
                gizmoWasUsing = false;
                if (gizmoEditEntityID == selectedEntity->GetID())
                {
                    undoStack.Push(std::make_unique<EditorUndo::TransformEditCommand>(
                        gizmoEditEntityID, gizmoEditBefore, gizmoTransform));
                }
            }
        }
#endif

        // --------------------------------------------------------
        // Mouse picking (viewport click-to-select)
        // --------------------------------------------------------
        {
#ifdef USE_IMGUI
            ImGuiIO& pickIO = ImGui::GetIO();
            bool imguiWantsMouse = pickIO.WantCaptureMouse;
#else
            bool imguiWantsMouse = false;
#endif

#ifdef USE_IMGUIZMO
            bool overGizmo = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
#else
            bool overGizmo = false;
#endif

            // Only pick with left click when the mouse is free (not driving the fly camera)
            // and ImGui/the gizmo isn't already handling the click.
            if (!imguiWantsMouse && !overGizmo && !Input::IsMouseCaptured() &&
                Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
            {
                double mouseX = 0.0, mouseY = 0.0;
                glfwGetCursorPos(window, &mouseX, &mouseY);

                Ray ray = ScreenPointToRay(mouseX, mouseY, g_WindowWidth, g_WindowHeight, view, projection);

                Entity* closestEntity = nullptr;
                float closestDistance = std::numeric_limits<float>::max();

                for (auto& entity : scene.GetEntities())
                {
                    if (!entity || !entity->HasComponent<TransformComponent>())
                        continue;

                    auto& transform = entity->GetComponent<TransformComponent>();
                    glm::mat4 worldMatrix = TransformHierarchy::GetWorldMatrix(scene, *entity);
                    float hitDistance = 0.0f;
                    bool hit = false;

                    if (entity->HasComponent<BoxColliderComponent>())
                    {
                        auto& box = entity->GetComponent<BoxColliderComponent>();
                        glm::vec3 worldCenter = glm::vec3(worldMatrix * glm::vec4(box.center, 1.0f));
                        glm::vec3 worldHalfExtents = box.halfExtents * transform.scale;
                        hit = RayIntersectsAABB(ray, worldCenter, worldHalfExtents, hitDistance);
                    }
                    else if (entity->HasComponent<BoundingSphereComponent>())
                    {
                        auto& sphere = entity->GetComponent<BoundingSphereComponent>();
                        glm::vec3 worldCenter = glm::vec3(worldMatrix * glm::vec4(sphere.center, 1.0f));
                        float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
                        float worldRadius = sphere.radius * maxScale;
                        hit = RayIntersectsSphere(ray, worldCenter, worldRadius, hitDistance);
                    }

                    if (hit && hitDistance < closestDistance)
                    {
                        closestDistance = hitDistance;
                        closestEntity = entity.get();
                    }
                }

                selectedEntity = closestEntity;
            }
        }

        renderSystem.Render(scene, view, projection);

        // Particles are rendered after opaque geometry and before skybox
        // so they alpha-blend correctly over solid surfaces.
        particleSystem.Render(scene, view, projection);

        // Skybox is drawn after opaque geometry
        // is currently bound - the HDR post-process target or the default
        // framebuffer) so only far-depth pixels are filled, minimizing the
        // chance of pass state impacting scene object rendering.
        if (skyboxEnabled && skybox.IsLoaded())
        {
            skybox.Render(view, projection);
        }

        if (postProcessEnabled)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, g_WindowWidth, g_WindowHeight);
            postProcess.Composite();
        }

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

    audioSystem.ReleaseAll(scene);
    AudioEngine::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}