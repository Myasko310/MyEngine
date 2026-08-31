#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif
#include <memory>
#include <chrono>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <array>
#include <functional>
#include <fstream>
#include <cmath>
#include <unordered_set>

#include "core/Input.h"
#include "core/InputActions.h"
#include "core/FileDialog.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "editor/EditorContext.h"
#include "editor/EditorUndo.h"
#include "editor/EditorUIState.h"
#include "editor/panels/AnimationStateMachinePanel.h"
#include "editor/panels/AssetBrowserPanel.h"
#include "editor/panels/MaterialBrowserPanel.h"
#include "editor/panels/SceneHierarchyPanel.h"
#include "editor/panels/ScriptingPanel.h"
#include "editor/panels/ToolbarPanel.h"
#include "components/LightComponent.h"
#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/LODComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/PlaneColliderComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/MeshColliderComponent.h"
#include "core/CollisionMatrix.h"
#include "components/CapsuleColliderComponent.h"
#include "components/CharacterControllerComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/AudioListenerComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/JointComponent.h"
#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/PrefabInstanceComponent.h"
#include "components/SkeletonComponent.h"
#include "components/ScriptComponent.h"
#include "audio/AudioEngine.h"
#include "audio/AudioClip.h"
#include <AL/al.h>
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/MeshPrimitives.h"
#include "rendering/Model.h"
#include "serialization/SceneSerializer.h"
#include "animation/AnimationStateMachine.h"
#include "core/AssetManager.h"
#include "core/LayerMask.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "editor/EditorStyle.h"
#endif
#ifdef USE_IMGUIZMO
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#endif
#include <glm/gtc/matrix_transform.hpp>
#include "systems/CameraSystem.h"
#include "systems/MeshRendererSystem.h"
#include "systems/TerrainSystem.h"
#include "systems/NavMeshSystem.h"
#include "components/TerrainComponent.h"
#include "components/NavigationAgentComponent.h"
#include "rendering/PostProcessPipeline.h"
#include "rendering/Skybox.h"
#include "systems/PhysicsSystem.h"
#include "systems/AudioSystem.h"
#include "systems/AnimationSystem.h"
#include "systems/ScriptSystem.h"
#include "systems/ParticleSystem.h"
#include "components/ParticleEmitterComponent.h"
#include "core/Raycast.h"

using namespace MyEngine;
#ifdef USE_IMGUI
using namespace MyEngine::Editor;
#endif

// Tracks the current framebuffer size used for viewport and UI layout.
static int g_WindowWidth = 3200;
static int g_WindowHeight = 1800;

// Keeps the cached window size in sync with the active GLFW framebuffer.
static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    g_WindowWidth = width;
    g_WindowHeight = height;

    glViewport(0, 0, width, height);
}

#ifdef USE_IMGUI
// Returns the world-space scale encoded in a transform matrix.
static glm::vec3 ExtractWorldScaleFromMatrix(const glm::mat4& worldMatrix)
{
    return glm::vec3(
        glm::length(glm::vec3(worldMatrix[0])),
        glm::length(glm::vec3(worldMatrix[1])),
        glm::length(glm::vec3(worldMatrix[2]))
    );
}

// Applies a transform matrix to a point in local space.
static glm::vec3 TransformPointByMatrix(const glm::mat4& worldMatrix, const glm::vec3& point)
{
    return glm::vec3(worldMatrix * glm::vec4(point, 1.0f));
}

// Projects a world-space point into ImGui screen coordinates.
static bool ProjectWorldPointToScreen(
    const glm::vec3& worldPoint,
    const glm::mat4& view,
    const glm::mat4& projection,
    int windowWidth,
    int windowHeight,
    ImVec2& outScreen)
{
    glm::vec3 projected = glm::project(
        worldPoint,
        view,
        projection,
        glm::vec4(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight))
    );

    if (projected.z < 0.0f || projected.z > 1.0f)
        return false;

    outScreen = ImVec2(projected.x, static_cast<float>(windowHeight) - projected.y);
    return true;
}
#endif

// Recent scenes are persisted to a small text file so the file menu can restore them.
static const char* kRecentScenesFile = "recent_scenes.txt";
static const size_t kMaxRecentScenes = 5;

// Loads the recent-scene history from disk.
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

// Writes the recent-scene history back to disk.
static void SaveRecentScenes(const std::vector<std::string>& recents)
{
    std::ofstream ofs(kRecentScenesFile, std::ios::trunc);
    if (!ofs)
        return;

    for (const auto& path : recents)
        ofs << path << "\n";
}

// Moves a scene to the front of the list and persists the update.
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

// Updates the window title to reflect the active scene file.
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

#ifdef USE_IMGUI
#endif

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

    // Action/axis mapping layer: defaults first, then optional user overrides
    MyEngine::InputActions::RegisterDefaults();
    MyEngine::InputActions::LoadBindings("bindings.json");

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
    TerrainSystem terrainSystem;
    terrainSystem.Init();
    NavMeshSystem navMeshSystem;

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
    ApplyEditorTheme();

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
        rb.useGravity = false;
        rb.bounciness = 0.0f;
        rb.isKinematic = true;
        rb.freezePositionX = false;
        rb.freezePositionZ = false;

        auto& controller = playerEntity->AddComponent<MyEngine::CharacterControllerComponent>();
        controller.moveSpeed = 4.5f;
        controller.jumpSpeed = 6.0f;
        controller.maxSlopeAngleDegrees = 50.0f;
        controller.groundSnapDistance = 0.18f;
        controller.skinWidth = 0.03f;
        controller.acceleration = 36.0f;
        controller.airAcceleration = 12.0f;
        controller.braking = 28.0f;
        controller.slideGravityScale = 1.35f;
        controller.orientToMovement = true;

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
        light.castShadows = true;
        // store pointer to the light entity as the first light in scene
    }

    int selectedCube = 0;
    bool wireframe = false;
    bool showUI = true;
    float lightYaw = -90.0f;
    float lightPitch = -20.0f;

    // Scene simulation state
    bool isPlaying = false;

    // Play/edit mode snapshot: the scene is serialized to an in-memory
    // string when entering play mode and restored when stopping, so
    // simulation changes (physics, scripts) don't permanently alter the
    // edited scene.  No temp file needed.
    bool        hasPlaySnapshot   = false;
    std::string playSnapshotJSON;  // in-memory snapshot (replaces temp file)
    bool        showStopPlayPrompt = false;

    // Scene file state
    std::string currentScenePath;
    std::vector<std::string> recentScenes = LoadRecentScenes();

    // UI state
    Entity* selectedEntity = nullptr;
    MyEngine::Editor::UIState editorUI;
    auto& showSceneHierarchy = editorUI.showSceneHierarchy;
    auto& showInspector = editorUI.showInspector;
    auto& showLightingPanel = editorUI.showLightingPanel;
    auto& showInputBindingsPanel = editorUI.showInputBindingsPanel;
    auto& showPostProcessPanel = editorUI.showPostProcessPanel;
    auto& showSkyboxPanel = editorUI.showSkyboxPanel;
    auto& showScriptingPanel = editorUI.showScriptingPanel;
    auto& showPerformancePanel = editorUI.showPerformancePanel;
    auto& showPhysicsPanel = editorUI.showPhysicsPanel;
    auto& showAssetBrowser = editorUI.showAssetBrowser;
    auto& assetBrowserPath = editorUI.assetBrowserPath;
    auto& showMaterialBrowser = editorUI.showMaterialBrowser;
    auto& showIBLPanel = editorUI.showIBLPanel;
    auto& showLayerManager = editorUI.showLayerManager;
    auto& materialBrowserPath = editorUI.materialBrowserPath;
    auto& selectedMaterialPath = editorUI.selectedMaterialPath;
    auto& editingMaterial = editorUI.editingMaterial;
    auto& materialRenameBuffer = editorUI.materialRenameBuffer;
    auto& materialRenameActive = editorUI.materialRenameActive;
    auto& newMaterialNameBuffer = editorUI.newMaterialNameBuffer;
    auto& showNewMaterialDialog = editorUI.showNewMaterialDialog;
    auto& animationStateMachineBrowserPath = editorUI.animationStateMachineBrowserPath;
    auto& selectedAnimationStateMachinePath = editorUI.selectedAnimationStateMachinePath;
    auto& editingAnimationStateMachine = editorUI.editingAnimationStateMachine;
    auto& matBrowserTextures = editorUI.matBrowserTextures;
    auto& matBrowserTexturesScanned = editorUI.matBrowserTexturesScanned;
    auto& globalScripts = editorUI.globalScripts;

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

    // Captures the current scene as a serialized snapshot for undo and play mode.
    auto captureSceneState = [&]()
    {
        return MyEngine::Serialization::SaveSceneToString(scene, globalScripts);
    };

    // Records a scene-state change as an undoable command.
    auto pushSceneStateCommand = [&](const std::string& beforeState, const std::string& afterState)
    {
        undoStack.Push(std::make_unique<EditorUndo::SceneStateCommand>(
            beforeState,
            afterState,
            litShader,
            &globalScripts,
            &selectedEntity,
            &playerEntity
        ));
    };

    // Compares a prefab instance against its source prefab and reports changed fields.
    auto describePrefabOverrides = [&](Entity* entity) -> std::vector<std::string>
    {
        std::vector<std::string> overrides;
        if (!entity || !entity->HasComponent<PrefabInstanceComponent>())
            return overrides;

        const auto& prefab = entity->GetComponent<PrefabInstanceComponent>();
        if (prefab.sourcePrefabPath.empty() || prefab.sourceEntityID == 0)
            return overrides;

        Scene prefabScene;
        if (!MyEngine::Serialization::LoadScene(prefabScene, prefab.sourcePrefabPath, litShader, &globalScripts))
        {
            overrides.push_back("Unable to load source prefab.");
            return overrides;
        }

        auto source = prefabScene.GetEntityByID(prefab.sourceEntityID);
        if (!source)
        {
            overrides.push_back("Missing source entity in prefab.");
            return overrides;
        }

        auto addOverride = [&](const std::string& label) { overrides.push_back(label); };
        auto nearlyEqual = [](float a, float b) { return std::fabs(a - b) < 0.0001f; };
        auto vec3Equal = [&](const glm::vec3& a, const glm::vec3& b)
        {
            return nearlyEqual(a.x, b.x) && nearlyEqual(a.y, b.y) && nearlyEqual(a.z, b.z);
        };

        if (entity->GetName() != source->GetName()) addOverride("Name");
        if (entity->GetTag() != source->GetTag()) addOverride("Tag");
        if (entity->GetLayer() != source->GetLayer()) addOverride("Layer");

        if (entity->HasComponent<TransformComponent>() && source->HasComponent<TransformComponent>())
        {
            const auto& current = entity->GetComponent<TransformComponent>();
            const auto& original = source->GetComponent<TransformComponent>();
            if (!vec3Equal(current.position, original.position) || !vec3Equal(current.rotation, original.rotation) || !vec3Equal(current.scale, original.scale) || current.parentID != original.parentID)
                addOverride("Transform");
        }

        if (entity->HasComponent<MeshRendererComponent>() && source->HasComponent<MeshRendererComponent>())
        {
            const auto& current = entity->GetComponent<MeshRendererComponent>();
            const auto& original = source->GetComponent<MeshRendererComponent>();
            if (current.visible != original.visible || current.materialPath != original.materialPath || current.useTexture != original.useTexture || current.usePBR != original.usePBR)
                addOverride("Mesh Renderer");
        }

        if (entity->HasComponent<LightComponent>() && source->HasComponent<LightComponent>())
        {
            const auto& current = entity->GetComponent<LightComponent>();
            const auto& original = source->GetComponent<LightComponent>();
            if (current.type != original.type || !vec3Equal(current.color, original.color) || !vec3Equal(current.direction, original.direction) || !vec3Equal(current.position, original.position) || !nearlyEqual(current.intensity, original.intensity) || !nearlyEqual(current.range, original.range) || current.castShadows != original.castShadows)
                addOverride("Light");
        }

        if (entity->HasComponent<RigidbodyComponent>() && source->HasComponent<RigidbodyComponent>())
        {
            const auto& current = entity->GetComponent<RigidbodyComponent>();
            const auto& original = source->GetComponent<RigidbodyComponent>();
            if (!vec3Equal(current.velocity, original.velocity) || !vec3Equal(current.acceleration, original.acceleration) || !nearlyEqual(current.mass, original.mass) || current.isKinematic != original.isKinematic)
                addOverride("Rigidbody");
        }

        if (entity->HasComponent<ScriptComponent>() && source->HasComponent<ScriptComponent>())
        {
            const auto& current = entity->GetComponent<ScriptComponent>();
            const auto& original = source->GetComponent<ScriptComponent>();
            if (current.scriptPath != original.scriptPath || current.enabled != original.enabled || current.autoStart != original.autoStart)
                addOverride("Script");
        }

        if (entity->HasComponent<AnimationComponent>() && source->HasComponent<AnimationComponent>())
        {
            const auto& current = entity->GetComponent<AnimationComponent>();
            const auto& original = source->GetComponent<AnimationComponent>();
            if (current.activeClipIndex != original.activeClipIndex || !nearlyEqual(current.time, original.time) || !nearlyEqual(current.playbackSpeed, original.playbackSpeed) || current.playing != original.playing || current.looping != original.looping)
                addOverride("Animation");
        }

        return overrides;
    };

    MyEngine::Editor::Context editorContext{};
    editorContext.scene = &scene;
    editorContext.litShader = litShader;
    editorContext.litSkinnedShader = litSkinnedShader;
    editorContext.selectedEntity = &selectedEntity;
    editorContext.playerEntity = &playerEntity;
    editorContext.ui = &editorUI;
    editorContext.undoStack = &undoStack;
    editorContext.currentScenePath = &currentScenePath;
    editorContext.recentScenes = &recentScenes;
    editorContext.window = window;
    editorContext.addRecentScene = AddRecentScene;
    editorContext.updateWindowTitle = UpdateWindowTitle;
    editorContext.captureSceneState = captureSceneState;
    editorContext.pushSceneStateCommand = pushSceneStateCommand;

    // Restores the edit-time scene snapshot after play mode ends.
    auto restorePlaySnapshot = [&]()
    {
        if (!hasPlaySnapshot)
            return;

        // Preserve the active camera state so the editor view doesn't jump.
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
        MyEngine::Serialization::LoadSceneFromString(scene, playSnapshotJSON, litShader, &globalScripts);
        hasPlaySnapshot = false;
        playSnapshotJSON.clear();

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
    };

    // Central play-state switch: snapshot the scene when starting play,
    // and prompt on stop so runtime changes can optionally be saved as copy.
    auto setPlaying = [&](bool play)
    {
        if (play == isPlaying)
            return;

        if (play)
        {
            playSnapshotJSON = MyEngine::Serialization::SaveSceneToString(scene, globalScripts);
            hasPlaySnapshot = !playSnapshotJSON.empty();
            showStopPlayPrompt = false;
            isPlaying = true;
        }
        else
        {
            isPlaying = false;
#ifdef USE_IMGUI
            if (hasPlaySnapshot)
            {
                showStopPlayPrompt = true;
                return;
            }
#endif
            restorePlaySnapshot();
        }
    };

    editorContext.isPlaying = &isPlaying;
    editorContext.wireframe = &wireframe;
    editorContext.setPlaying = setPlaying;
    editorContext.applyWireframe = [&](bool enabled)
    {
        renderSystem.SetWireframe(enabled);
    };
#ifdef USE_IMGUIZMO
    editorContext.getGizmoOperation = [&]() { return static_cast<int>(gizmoOperation); };
    editorContext.setGizmoOperation = [&](int value) { gizmoOperation = static_cast<ImGuizmo::OPERATION>(value); };
    editorContext.getGizmoMode = [&]() { return static_cast<int>(gizmoMode); };
    editorContext.setGizmoMode = [&](int value) { gizmoMode = static_cast<ImGuizmo::MODE>(value); };
#endif

    // ------------------------------------------------------------
    // Timing / profiling
    // ------------------------------------------------------------
    auto previousTime = std::chrono::high_resolution_clock::now();

    static constexpr int kPerfHistory = 180;
    std::array<float, kPerfHistory> frameMsHistory{};
    std::array<float, kPerfHistory> physicsMsHistory{};
    std::array<float, kPerfHistory> animationMsHistory{};
    std::array<float, kPerfHistory> renderMsHistory{};
    int perfHistoryIndex = 0;

    float cpuPhysicsMs = 0.0f;
    float cpuAnimationMs = 0.0f;
    float cpuRenderMs = 0.0f;
    float gpuFrameMs = 0.0f;
    float memoryWorkingSetMB = 0.0f;
    float memoryPrivateMB = 0.0f;

    GLuint gpuFrameQueries[2] = { 0, 0 };
    glGenQueries(2, gpuFrameQueries);
    int gpuQueryWriteIndex = 0;

    // Writes the current performance history buffers to a CSV file.
    auto exportPerformanceCsv = [&]() -> bool
    {
        std::ofstream ofs("performance_metrics.csv", std::ios::trunc);
        if (!ofs)
            return false;

        ofs << "frameIndex,frameMs,physicsMs,animationMs,renderMs\n";
        for (int i = 0; i < kPerfHistory; ++i)
        {
            const int index = (perfHistoryIndex + i) % kPerfHistory;
            ofs << i << ',' << frameMsHistory[index] << ',' << physicsMsHistory[index] << ',' << animationMsHistory[index] << ',' << renderMsHistory[index] << '\n';
        }
        return true;
    };

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
        MyEngine::InputActions::Update();

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
                const std::string beforeState = captureSceneState();
                uint32_t idToDelete = selectedEntity->GetID();
                selectedEntity = nullptr;
                scene.DestroyEntity(idToDelete);
                const std::string afterState = captureSceneState();
                if (!beforeState.empty() && !afterState.empty() && beforeState != afterState)
                    pushSceneStateCommand(beforeState, afterState);
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
                // Auto-create materials from embedded model data
                auto importedMats = MyEngine::AssetManager::ImportModelMaterials(path);
                if (!importedMats.empty() && importedMats[0])
                {
                    auto& mr = ent->GetComponent<MeshRendererComponent>();
                    mr.material     = importedMats[0];
                    mr.materialPath = importedMats[0]->GetPath();
                    if (importedMats[0]->shader && !ent->HasComponent<AnimationComponent>())
                        mr.shader = importedMats[0]->shader;
                }
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
            glm::vec3 controllerCameraForward(0.0f, 0.0f, -1.0f);
            glm::vec3 controllerCameraRight(1.0f, 0.0f, 0.0f);
            for (auto& e : scene.GetEntities())
            {
                if (!e || !e->HasComponent<CameraComponent>() || !e->HasComponent<TransformComponent>())
                    continue;

                auto& cam = e->GetComponent<CameraComponent>();
                if (!cam.isPrimary)
                    continue;

                controllerCameraForward.x = std::cos(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
                controllerCameraForward.y = std::sin(glm::radians(cam.pitch));
                controllerCameraForward.z = std::sin(glm::radians(cam.yaw)) * std::cos(glm::radians(cam.pitch));
                controllerCameraForward = glm::normalize(controllerCameraForward);
                controllerCameraRight = glm::normalize(glm::cross(controllerCameraForward, glm::vec3(0.0f, 1.0f, 0.0f)));
                break;
            }

            {
                auto cpuStart = std::chrono::high_resolution_clock::now();
                physicsSystem.OnUpdate(scene, deltaTime, window, controllerCameraForward, controllerCameraRight);
                cpuPhysicsMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - cpuStart).count();
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

            }

        // Compute bone matrix palettes for any skinned entities every frame,
        // even while paused/in edit mode. Passing deltaTime of 0 when not
        // playing keeps the pose frozen (no playback advance) while still
        // populating AnimationComponent::boneMatrices; otherwise the palette
        // stays empty until Play is pressed and every skinned vertex gets
        // multiplied by a zero bone matrix, collapsing the mesh to the
        // origin (invisible) before Play is hit.
        {
            auto cpuStart = std::chrono::high_resolution_clock::now();
            animationSystem.Update(scene, isPlaying ? deltaTime : 0.0f);
            particleSystem.Update(scene, isPlaying ? deltaTime : 0.0f);
            cpuAnimationMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - cpuStart).count();
        }

        cameraSystem.Update(scene, window, deltaTime, aspectRatio);

        audioSystem.Update(scene, deltaTime);
        scriptSystem.SetGlobalScripts(globalScripts);
        scriptSystem.OnUpdate(scene, deltaTime);
        navMeshSystem.Update(scene, deltaTime);

        glm::mat4 view = cameraSystem.GetViewMatrix();
        glm::mat4 projection = cameraSystem.GetProjectionMatrix();

        // --------------------------------------------------------
        // Render
        // --------------------------------------------------------
        auto cpuRenderStart = std::chrono::high_resolution_clock::now();
        const int gpuQueryReadIndex = (gpuQueryWriteIndex + 1) % 2;
        GLuint gpuQueryAvailable = 0;
        glGetQueryObjectuiv(gpuFrameQueries[gpuQueryReadIndex], GL_QUERY_RESULT_AVAILABLE, &gpuQueryAvailable);
        if (gpuQueryAvailable)
        {
            GLuint64 elapsedNanoseconds = 0;
            glGetQueryObjectui64v(gpuFrameQueries[gpuQueryReadIndex], GL_QUERY_RESULT, &elapsedNanoseconds);
            gpuFrameMs = static_cast<float>(elapsedNanoseconds / 1000000.0);
        }
        glBeginQuery(GL_TIME_ELAPSED, gpuFrameQueries[gpuQueryWriteIndex]);

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

        if (showStopPlayPrompt)
        {
            ImGui::OpenPopup("Stop Play Mode");
            showStopPlayPrompt = false;
        }

        bool keepStopPlayPopupOpen = true;
        if (ImGui::BeginPopupModal("Stop Play Mode", &keepStopPlayPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Play mode changes were detected. Save runtime changes as a separate scene copy before restoring edit snapshot?");
            ImGui::Separator();

            if (ImGui::Button("Save Runtime Scene Copy...", ImVec2(220, 0)))
            {
                std::string saveCopyPath = MyEngine::FileDialog::SaveSceneFile();
                if (!saveCopyPath.empty())
                {
                    MyEngine::Serialization::SaveScene(scene, saveCopyPath, globalScripts);
                    AddRecentScene(recentScenes, saveCopyPath);
                }
                restorePlaySnapshot();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button("Discard Runtime Changes", ImVec2(220, 0)))
            {
                restorePlaySnapshot();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button("Cancel", ImVec2(220, 0)))
            {
                isPlaying = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (showUI)
        {
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
                if (ImGui::MenuItem("Spawn Prefab..."))
                {
                    std::string path = MyEngine::FileDialog::OpenPrefabFile();
                    if (!path.empty())
                    {
                        auto spawned = MyEngine::Serialization::SpawnPrefab(scene, path, litShader);
                        if (spawned)
                            selectedEntity = spawned;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "ESC"))
                {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                const bool canUndo = undoStack.CanUndo();
                const bool canRedo = undoStack.CanRedo();

                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo))
                    undoStack.Undo(scene);
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo))
                    undoStack.Redo(scene);

                ImGui::EndMenu();
            }

                if (ImGui::BeginMenu("View"))
                {
                    ImGui::MenuItem("Scene Hierarchy", nullptr, &showSceneHierarchy);
                    ImGui::MenuItem("Inspector", nullptr, &showInspector);
                    ImGui::MenuItem("Lighting", nullptr, &showLightingPanel);
                    ImGui::MenuItem("Input Bindings", nullptr, &showInputBindingsPanel);
                    ImGui::MenuItem("IBL", nullptr, &showIBLPanel);
                    ImGui::MenuItem("Layer Manager", nullptr, &showLayerManager);
                    ImGui::MenuItem("Post-Processing", nullptr, &showPostProcessPanel);
                    ImGui::MenuItem("Skybox", nullptr, &showSkyboxPanel);
                    ImGui::MenuItem("Scripting", nullptr, &showScriptingPanel);
                    ImGui::MenuItem("Performance", nullptr, &showPerformancePanel);
                    ImGui::MenuItem("Asset Browser", nullptr, &showAssetBrowser);
                    ImGui::MenuItem("Material Browser", nullptr, &showMaterialBrowser);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Bake NavMesh"))
                    {
                        navMeshSystem.Bake(scene);
                    }
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
                            // Auto-create materials from embedded model data
                            auto importedMats = MyEngine::AssetManager::ImportModelMaterials(path);
                            if (!importedMats.empty() && importedMats[0])
                            {
                                auto& mr = ent->GetComponent<MeshRendererComponent>();
                                mr.material     = importedMats[0];
                                mr.materialPath = importedMats[0]->GetPath();
                                if (importedMats[0]->shader && !ent->HasComponent<AnimationComponent>())
                                    mr.shader = importedMats[0]->shader;
                            }
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
                        light.castShadows = true;
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
                        light.castShadows = true;
                        selectedEntity = ent.get();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            // ============================================================
            MyEngine::Editor::Panels::DrawToolbarPanel(editorContext);

            // ============================================================
            // Asset Browser Panel
            // ============================================================
            MyEngine::Editor::Panels::DrawAssetBrowserPanel(editorContext);

            // ============================================================
            // Material Browser Panel
            // ============================================================
            MyEngine::Editor::Panels::DrawMaterialBrowserPanel(editorContext);

            MyEngine::Editor::Panels::DrawAnimationStateMachinePanel(editorContext);
            MyEngine::Editor::Panels::DrawSceneHierarchyPanel(editorContext);

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
                    ImGui::TextUnformatted(selectedEntity->GetName().c_str());
                    ImGui::TextDisabled("Entity ID: %u", selectedEntity->GetID());
                    if (InspectorActionButton("Save as Prefab"))
                    {
                        std::string prefabPath = MyEngine::FileDialog::SavePrefabFile();
                        if (!prefabPath.empty())
                        {
                            if (MyEngine::Serialization::SavePrefab(scene, selectedEntity, prefabPath))
                                ImGui::OpenPopup("PrefabSaved");
                        }
                    }
                    if (ImGui::BeginPopup("PrefabSaved"))
                    {
                        ImGui::Text("Prefab saved successfully.");
                        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }

                    if (selectedEntity->HasComponent<PrefabInstanceComponent>())
                    {
                        InspectorGroupLabel("Prefab Instance");
                        const auto& prefab = selectedEntity->GetComponent<PrefabInstanceComponent>();
                        ImGui::TextWrapped("Source: %s", prefab.sourcePrefabPath.empty() ? "<unsaved>" : prefab.sourcePrefabPath.c_str());
                        ImGui::Text("Source Entity ID: %u", prefab.sourceEntityID);
                        auto overrides = describePrefabOverrides(selectedEntity);
                        if (overrides.empty())
                            ImGui::TextDisabled("No overrides detected.");
                        else
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Overrides:");
                            for (const auto& overrideName : overrides)
                                ImGui::BulletText("%s", overrideName.c_str());
                        }
                    }

                    InspectorGroupLabel("Entity Settings");
                    {
                        static char tagBuf[64] = "";
                        strncpy_s(tagBuf, selectedEntity->GetTag().c_str(), sizeof(tagBuf) - 1);
                        InspectorFullWidth();
                        if (ImGui::InputText("Tag##entityTag", tagBuf, sizeof(tagBuf)))
                            selectedEntity->SetTag(tagBuf);

                        int currentLayer = static_cast<int>(selectedEntity->GetLayer());
                        const auto& layerNames = MyEngine::LayerMask::GetNames();
                        InspectorFullWidth();
                        if (ImGui::BeginCombo("Layer##entityLayer", layerNames[currentLayer].c_str()))
                        {
                            for (int li = 0; li < MyEngine::MAX_LAYERS; ++li)
                            {
                                bool sel = (li == currentLayer);
                                if (ImGui::Selectable(layerNames[li].c_str(), sel))
                                    selectedEntity->SetLayer(static_cast<uint32_t>(li));
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::Spacing();

                    // Transform Component
                    if (selectedEntity->HasComponent<TransformComponent>())
                    {
                        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto& transform = selectedEntity->GetComponent<TransformComponent>();
                            TransformComponent beforeEdit = transform;
                            bool edited = false;
                            edited |= ImGui::DragFloat3("Position##transform", &transform.position.x, 0.1f);
                            edited |= ImGui::DragFloat3("Rotation##transform", &transform.rotation.x, 1.0f);
                            edited |= ImGui::DragFloat3("Scale##transform", &transform.scale.x, 0.1f, 0.01f, 100.0f);
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
                                ImGui::DragFloat3("Direction##light", &light.direction.x, 0.1f);
                                ImGui::Checkbox("Cast Shadows##light", &light.castShadows);
                                ImGui::DragFloat("Shadow Bias##light", &light.shadowBias, 0.0001f, 0.0001f, 0.1f, "%.4f");
                            }
                            else if (light.type == LightComponent::Type::Point || light.type == LightComponent::Type::Spot)
                            {
                                ImGui::DragFloat3("Position##light", &light.position.x, 0.1f);
                                ImGui::DragFloat("Range##light", &light.range, 0.1f, 0.1f, 1000.0f);
                                ImGui::Checkbox("Cast Shadows##light", &light.castShadows);
                                ImGui::DragFloat("Shadow Bias##light", &light.shadowBias, 0.0001f, 0.0001f, 0.1f, "%.4f");

                                ImGui::Separator();
                                ImGui::Text("Per-Light Shadow Overrides");
                                ImGui::TextDisabled("0 or negative values use global lighting panel settings.");

                                if (light.type == LightComponent::Type::Point)
                                {
                                    ImGui::DragInt("Point Shadow Size Override", &light.pointShadowSizeOverride, 1.0f, 0, 4096);
                                    ImGui::DragInt("Point PCF Samples Override", &light.pointShadowPCFSamplesOverride, 1.0f, 0, 20);
                                    ImGui::DragFloat("Point PCF Radius Override", &light.pointShadowPCFRadiusOverride, 0.001f, -1.0f, 0.25f, "%.3f");
                                }

                                if (light.type == LightComponent::Type::Spot)
                                {
                                    ImGui::DragFloat("Inner Cone", &light.innerCone, 1.0f, 0.0f, 89.0f);
                                    ImGui::DragFloat("Outer Cone", &light.outerCone, 1.0f, 0.0f, 90.0f);
                                    ImGui::DragInt("Spot Shadow Size Override", &light.spotShadowSizeOverride, 1.0f, 0, 4096);
                                    ImGui::DragFloat("Spot PCF Radius Override", &light.spotShadowPCFRadiusOverride, 0.01f, -1.0f, 4.0f, "%.2f");
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

                            // --- Material file picker ---
                            static char materialPathBuffer[256] = "";
                            {
                                // Show current path (truncated) as a read-only label
                                const char* displayPath = renderer.materialPath.empty()
                                    ? "(none)"
                                    : renderer.materialPath.c_str();
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
                                ImGui::InputText("##matPickerPath", const_cast<char*>(displayPath),
                                    renderer.materialPath.size() + 1,
                                    ImGuiInputTextFlags_ReadOnly);
                                ImGui::PopItemWidth();

                                ImGui::SameLine();
                                if (ImGui::Button("Browse...##matPicker"))
                                {
                                    std::string picked = MyEngine::FileDialog::OpenMaterialFile();
                                    if (!picked.empty())
                                    {
                                        if (auto mat = MyEngine::AssetManager::LoadMaterial(picked))
                                        {
                                            renderer.material     = mat;
                                            renderer.materialPath = picked;
                                            if (mat->shader)
                                                renderer.shader = mat->shader;
                                            // Keep the legacy path buffer in sync
                                            std::strncpy(materialPathBuffer, picked.c_str(),
                                                sizeof(materialPathBuffer) - 1);
                                            materialPathBuffer[sizeof(materialPathBuffer) - 1] = '\0';
                                        }
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("X##matPickerClear"))
                                {
                                    renderer.material = nullptr;
                                    renderer.materialPath.clear();
                                    materialPathBuffer[0] = '\0';
                                }
                                ImGui::SetItemTooltip("Clear assigned material");
                            }

                            // Secondary actions row
                            if (ImGui::Button("Create From Renderer##matCreate"))
                            {
                                auto material = std::make_shared<MyEngine::Material>();
                                material->shader = renderer.shader;
                                if (renderer.shader)
                                {
                                    material->shaderVertexPath   = renderer.shader->GetVertexPath();
                                    material->shaderFragmentPath = renderer.shader->GetFragmentPath();
                                }
                                material->albedo              = renderer.albedo;
                                material->shininess           = renderer.shininess;
                                material->texture             = renderer.texture;
                                material->useTexture          = renderer.useTexture;
                                material->usePBR              = renderer.usePBR;
                                material->metallic            = renderer.metallic;
                                material->roughness           = renderer.roughness;
                                material->aoStrength          = renderer.aoStrength;
                                material->emissive            = renderer.emissive;
                                material->albedoMap           = renderer.albedoMap;
                                material->normalMap           = renderer.normalMap;
                                material->metallicRoughnessMap = renderer.metallicRoughnessMap;
                                material->aoMap               = renderer.aoMap;
                                material->emissiveMap         = renderer.emissiveMap;
                                renderer.material             = material;
                                // Prompt for a save location immediately
                                std::string savePath = MyEngine::FileDialog::SaveMaterialFile();
                                if (!savePath.empty())
                                {
                                    material->SetPath(savePath);
                                    material->SaveToFile(savePath);
                                    renderer.materialPath = savePath;
                                    std::strncpy(materialPathBuffer, savePath.c_str(),
                                        sizeof(materialPathBuffer) - 1);
                                    materialPathBuffer[sizeof(materialPathBuffer) - 1] = '\0';
                                }
                            }
                            ImGui::SameLine();
                            if (renderer.material && ImGui::Button("Save##matSave"))
                            {
                                std::string savePath = renderer.materialPath;
                                if (savePath.empty())
                                    savePath = MyEngine::FileDialog::SaveMaterialFile();
                                if (!savePath.empty())
                                {
                                    renderer.materialPath = savePath;
                                    renderer.material->SetPath(savePath);
                                    if (renderer.material->shader)
                                    {
                                        renderer.material->shaderVertexPath   = renderer.material->shader->GetVertexPath();
                                        renderer.material->shaderFragmentPath = renderer.material->shader->GetFragmentPath();
                                    }
                                    renderer.material->SaveToFile(savePath);
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
                            if (!editableUsePBR)
                            {
                                auto ensureLitShader = [](std::shared_ptr<MyEngine::Shader>& shaderRef)
                                {
                                    if (shaderRef && shaderRef->GetVertexPath() == "shaders/pbr.vert" && shaderRef->GetFragmentPath() == "shaders/pbr.frag")
                                    {
                                        shaderRef = MyEngine::AssetManager::LoadShader("shaders/lit.vert", "shaders/lit.frag");
                                    }
                                };

                                if (material)
                                {
                                    ensureLitShader(material->shader);
                                    if (material->shader)
                                    {
                                        material->shaderVertexPath = material->shader->GetVertexPath();
                                        material->shaderFragmentPath = material->shader->GetFragmentPath();
                                    }
                                    renderer.shader = material->shader;
                                }
                                else
                                {
                                    ensureLitShader(renderer.shader);
                                }
                            }

                            if (editableUsePBR)
                            {
                                auto ensurePBRShader = [](std::shared_ptr<MyEngine::Shader>& shaderRef)
                                {
                                    if (!shaderRef || shaderRef->GetVertexPath() != "shaders/pbr.vert" || shaderRef->GetFragmentPath() != "shaders/pbr.frag")
                                    {
                                        shaderRef = MyEngine::AssetManager::LoadShader("shaders/pbr.vert", "shaders/pbr.frag");
                                    }
                                };

                                if (material)
                                {
                                    ensurePBRShader(material->shader);
                                    material->shaderVertexPath = "shaders/pbr.vert";
                                    material->shaderFragmentPath = "shaders/pbr.frag";
                                    renderer.shader = material->shader;
                                }
                                else
                                {
                                    ensurePBRShader(renderer.shader);
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

                    // LOD Component
                    if (selectedEntity->HasComponent<LODComponent>())
                    {
                        auto& lod = selectedEntity->GetComponent<LODComponent>();
                        if (ImGui::CollapsingHeader("Level of Detail (LOD)", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Checkbox("Enabled##lod", &lod.enabled);
                            ImGui::Separator();

                            for (int li = 0; li < static_cast<int>(lod.levels.size()); ++li)
                            {
                                auto& lvl = lod.levels[li];
                                ImGui::PushID(li);

                                ImGui::Text("Level %d", li);
                                ImGui::SameLine();
                                if (lod.activeLevel == li)
                                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "(active)");

                                ImGui::DragFloat("Distance##lod", &lvl.distanceThreshold, 1.0f, 0.0f, 10000.0f);

                                // Asset path / mesh picker
                                ImGui::InputText("Mesh Path##lod", lvl.assetPath.data(), lvl.assetPath.capacity() + 1,
                                    ImGuiInputTextFlags_ReadOnly);
                                ImGui::SameLine();
                                if (ImGui::Button("Browse##lod"))
                                {
                                    std::string picked = MyEngine::FileDialog::OpenModelFile();
                                    if (!picked.empty())
                                    {
                                        auto meshes = MyEngine::AssetManager::LoadModel(picked);
                                        if (!meshes.empty())
                                        {
                                            lvl.mesh = meshes[0];
                                            lvl.assetPath = picked;
                                        }
                                    }
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Remove##lod"))
                                {
                                    lod.levels.erase(lod.levels.begin() + li);
                                    ImGui::PopID();
                                    break;
                                }

                                ImGui::PopID();
                                ImGui::Separator();
                            }

                            if (ImGui::Button("Add LOD Level"))
                            {
                                LODComponent::Level lvl;
                                lvl.distanceThreshold = lod.levels.empty() ? 50.0f
                                    : lod.levels.back().distanceThreshold + 50.0f;
                                lod.levels.push_back(std::move(lvl));
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Remove LOD Component"))
                            {
                                selectedEntity->RemoveComponent<LODComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add LOD Component"))
                        {
                            selectedEntity->AddComponent<LODComponent>();
                        }
                    }

                    // ---------------------------------------------------
                    // Terrain Component
                    // ---------------------------------------------------
                    if (selectedEntity->HasComponent<TerrainComponent>())
                    {
                        auto& terrain = selectedEntity->GetComponent<TerrainComponent>();
                        if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            // Heightmap picker
                            ImGui::Text("Heightmap");
                            static char terrainHmBuf[256] = "";
                            strncpy_s(terrainHmBuf, terrain.heightmapPath.c_str(), sizeof(terrainHmBuf) - 1);
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                            if (ImGui::InputText("##terrainHm", terrainHmBuf, sizeof(terrainHmBuf)))
                            {
                                terrain.heightmapPath = terrainHmBuf;
                                terrain.dirty = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Browse##terrainHm"))
                            {
                                std::string p = MyEngine::FileDialog::OpenImageFile();
                                if (!p.empty())
                                {
                                    terrain.heightmapPath = p;
                                    terrain.dirty = true;
                                }
                            }

                            ImGui::Separator();

                            // Dimensions
                            bool dimsChanged = false;
                            dimsChanged |= ImGui::DragFloat("Width##terrain",       &terrain.width,       1.0f, 1.0f, 10000.0f);
                            dimsChanged |= ImGui::DragFloat("Depth##terrain",       &terrain.depth,       1.0f, 1.0f, 10000.0f);
                            dimsChanged |= ImGui::DragFloat("Height Scale##terrain",&terrain.heightScale, 0.5f, 0.0f, 1000.0f);
                            if (ImGui::DragInt("Resolution##terrain", &terrain.resolution, 1.0f, 2, 512))
                                dimsChanged = true;
                            if (dimsChanged)
                                terrain.dirty = true;

                            // Surface texture picker
                            ImGui::Separator();
                            ImGui::Text("Surface Texture");
                            std::string surfName = terrain.surfaceTexture
                                ? terrain.surfaceTexture->GetPath() : "(none)";
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                            ImGui::InputText("##terrainSurf", surfName.data(), surfName.size() + 1,
                                ImGuiInputTextFlags_ReadOnly);
                            ImGui::SameLine();
                            if (ImGui::Button("Browse##terrainSurf"))
                            {
                                std::string p = MyEngine::FileDialog::OpenImageFile();
                                if (!p.empty())
                                {
                                    terrain.surfaceTexture = MyEngine::AssetManager::LoadTexture(p);
                                    terrain.surfaceTexturePath = p;
                                }
                            }

                            // Rebuild button
                            ImGui::Separator();
                            if (ImGui::Button("Rebuild Terrain Mesh"))
                                TerrainSystem::RebuildMesh(terrain);

                            ImGui::SameLine();
                            if (ImGui::Button("Remove Terrain Component"))
                                selectedEntity->RemoveComponent<TerrainComponent>();
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Terrain Component"))
                        {
                            auto& t = selectedEntity->AddComponent<TerrainComponent>();
                            t.dirty = true;
                        }
                    }

                    // ---------------------------------------------------
                    // Navigation Agent Component
                    // ---------------------------------------------------
                    if (selectedEntity->HasComponent<NavigationAgentComponent>())
                    {
                        auto& agent = selectedEntity->GetComponent<NavigationAgentComponent>();
                        if (ImGui::CollapsingHeader("Navigation Agent", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Checkbox("Active##nav", &agent.active);
                            ImGui::DragFloat("Speed##nav",            &agent.speed,           0.1f, 0.0f, 100.0f);
                            ImGui::DragFloat("Stopping Dist##nav",   &agent.stoppingDistance, 0.05f, 0.0f, 10.0f);
                            ImGui::DragFloat3("Target##nav",         &agent.targetPosition.x, 0.1f);

                            std::string agentStatus = agent.arrived ? "Arrived" :
                                ("Moving (wp " + std::to_string(agent.waypointIndex) +
                                 "/" + std::to_string(static_cast<int>(agent.path.size())) + ")");
                            ImGui::Text("Status: %s", agentStatus.c_str());

                            if (ImGui::Button("Set Destination##nav"))
                            {
                                if (selectedEntity->HasComponent<TransformComponent>())
                                {
                                    auto& tc = selectedEntity->GetComponent<TransformComponent>();
                                    agent.path = navMeshSystem.FindPath(tc.position, agent.targetPosition);
                                    agent.waypointIndex = 0;
                                    agent.arrived = agent.path.empty();
                                    agent.active  = !agent.path.empty();
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Stop##nav"))
                            {
                                agent.active  = false;
                                agent.arrived = true;
                                agent.path.clear();
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Remove##nav"))
                                selectedEntity->RemoveComponent<NavigationAgentComponent>();
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Add Nav Agent"))
                            selectedEntity->AddComponent<NavigationAgentComponent>();
                    }

                    // Rigidbody Component
                    if (selectedEntity->HasComponent<RigidbodyComponent>())
                    {
                        if (BeginInspectorSection("Rigidbody"))
                        {
                            auto& rb = selectedEntity->GetComponent<RigidbodyComponent>();

                            InspectorGroupLabel("Material");
                            ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.1f, 1000.0f);
                            ImGui::DragFloat("Drag", &rb.drag, 0.01f, 0.0f, 10.0f);
                            ImGui::SliderFloat("Bounciness", &rb.bounciness, 0.0f, 1.0f);

                            InspectorGroupLabel("Motion");
                            ImGui::Checkbox("Use Gravity", &rb.useGravity);
                            ImGui::DragFloat("Gravity Scale", &rb.gravityScale, 0.1f, -10.0f, 10.0f);
                            ImGui::Checkbox("Kinematic", &rb.isKinematic);
                            ImGui::Checkbox("CCD (Continuous Collision)", &rb.useCCD);
                            ImGui::SetItemTooltip("Sub-steps this body's sweep each tick to prevent tunnelling at high speeds");

                            InspectorGroupLabel("Constraints");
                            ImGui::TextDisabled("Freeze Position");
                            ImGui::Checkbox("X##freezeX", &rb.freezePositionX); ImGui::SameLine();
                            ImGui::Checkbox("Y##freezeY", &rb.freezePositionY); ImGui::SameLine();
                            ImGui::Checkbox("Z##freezeZ", &rb.freezePositionZ);

                            InspectorGroupLabel("Debug");
                            ImGui::Text("Velocity:  %.2f, %.2f, %.2f", rb.velocity.x, rb.velocity.y, rb.velocity.z);

                            auto& transform = selectedEntity->GetComponent<TransformComponent>();
                            ImGui::Text("Position:  %.2f, %.2f, %.2f",
                                transform.position.x, transform.position.y, transform.position.z);

                            if (selectedEntity->HasComponent<BoundingSphereComponent>())
                            {
                                auto& bs = selectedEntity->GetComponent<BoundingSphereComponent>();
                                ImGui::Text("Sphere Radius: %.2f", bs.radius);
                                ImGui::Text("Bottom Y: %.2f", transform.position.y - bs.radius);
                            }
                            if (selectedEntity->HasComponent<BoxColliderComponent>())
                            {
                                auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
                                ImGui::Text("Box Half-Extents: %.2f, %.2f, %.2f", box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
                                ImGui::Text("Bottom Y: %.2f", transform.position.y + box.center.y - box.halfExtents.y);
                            }

                            if (InspectorActionButton("Reset Velocity"))
                                rb.velocity = glm::vec3(0.0f);
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Rigidbody"))
                        {
                            selectedEntity->AddComponent<RigidbodyComponent>();
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
                        if (BeginInspectorSection("Box Collider"))
                        {
                            auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
                            InspectorGroupLabel("Shape");
                            ImGui::DragFloat3("Center Offset", &box.center.x, 0.05f);
                            ImGui::DragFloat3("Half Extents", &box.halfExtents.x, 0.05f, 0.01f, 100.0f);
                            InspectorGroupLabel("Behavior");
                            ImGui::Checkbox("Is Trigger", &box.isTrigger);
                            if (InspectorDangerButton("Remove Box Collider"))
                                selectedEntity->RemoveComponent<BoxColliderComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Box Collider"))
                        {
                            auto& box = selectedEntity->AddComponent<BoxColliderComponent>();
                            box.halfExtents = glm::vec3(0.5f);
                            if (selectedEntity->HasComponent<BoundingSphereComponent>())
                                selectedEntity->RemoveComponent<BoundingSphereComponent>();
                        }
                    }

                    // Capsule Collider Component
                    if (selectedEntity->HasComponent<CapsuleColliderComponent>())
                    {
                        if (BeginInspectorSection("Capsule Collider"))
                        {
                            auto& capsule = selectedEntity->GetComponent<CapsuleColliderComponent>();
                            InspectorGroupLabel("Shape");
                            ImGui::DragFloat3("Point A", &capsule.pointA.x, 0.02f);
                            ImGui::DragFloat3("Point B", &capsule.pointB.x, 0.02f);
                            ImGui::DragFloat("Radius", &capsule.radius, 0.01f, 0.01f, 100.0f, "%.3f");
                            InspectorGroupLabel("Behavior");
                            ImGui::Checkbox("Is Trigger##capsule", &capsule.isTrigger);

                            if (selectedEntity->HasComponent<SkeletonComponent>())
                            {
                                InspectorGroupLabel("Character Tools");
                                if (InspectorActionButton("Auto-Refit From Skeleton##capsule"))
                                {
                                    auto skeleton = selectedEntity->GetComponent<SkeletonComponent>().skeleton;
                                    glm::vec3 fitA(0.0f), fitB(0.0f);
                                    float fitRadius = 0.0f;
                                    if (MyEngine::AssetManager::ComputeCharacterCapsuleFromSkeleton(skeleton, fitA, fitB, fitRadius))
                                    {
                                        capsule.pointA = fitA;
                                        capsule.pointB = fitB;
                                        capsule.radius = fitRadius;
                                    }
                                }
                                ImGui::TextDisabled("Use auto-refit for imported characters, then fine-tune manually.");
                            }

                            if (InspectorDangerButton("Remove Capsule Collider"))
                                selectedEntity->RemoveComponent<CapsuleColliderComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Capsule Collider"))
                        {
                            auto& capsule = selectedEntity->AddComponent<CapsuleColliderComponent>();
                            capsule.pointA = glm::vec3(0.0f, -0.4f, 0.0f);
                            capsule.pointB = glm::vec3(0.0f, 0.4f, 0.0f);
                            capsule.radius = 0.5f;
                            if (selectedEntity->HasComponent<BoundingSphereComponent>())
                                selectedEntity->RemoveComponent<BoundingSphereComponent>();
                        }
                    }

                    // Mesh Collider Component
                    if (selectedEntity->HasComponent<MeshColliderComponent>())
                    {
                        if (BeginInspectorSection("Mesh Collider"))
                        {
                            auto& mesh = selectedEntity->GetComponent<MeshColliderComponent>();
                            InspectorGroupLabel("Source");
                            ImGui::Checkbox("Is Trigger##meshcol", &mesh.isTrigger);
                            ImGui::TextDisabled("Triangles: %d", static_cast<int>(mesh.triangles.size()));
                            if (!mesh.modelPath.empty())
                                ImGui::TextWrapped("Source: %s", mesh.modelPath.c_str());

                            if (InspectorActionButton("Build from Entity Mesh##meshcol"))
                            {
                                if (selectedEntity->HasComponent<MeshComponent>())
                                {
                                    auto& mc = selectedEntity->GetComponent<MeshComponent>();
                                    mesh.triangles.clear();
                                    mesh.modelPath = mc.assetPath;
                                    if (mc.mesh)
                                    {
                                        const auto& verts   = mc.mesh->GetVertices();
                                        const auto& indices = mc.mesh->GetIndices();
                                        for (size_t ti = 0; ti + 2 < indices.size(); ti += 3)
                                        {
                                            std::array<glm::vec3, 3> tri;
                                            tri[0] = verts[indices[ti + 0]].Position;
                                            tri[1] = verts[indices[ti + 1]].Position;
                                            tri[2] = verts[indices[ti + 2]].Position;
                                            mesh.triangles.push_back(tri);
                                        }
                                    }
                                    mesh.RebuildAABB();
                                }
                            }
                            ImGui::SetItemTooltip("Extracts collision triangles from the entity's MeshComponent");

                            if (InspectorActionButton("Clear Triangles##meshcol"))
                            {
                                mesh.triangles.clear();
                                mesh.RebuildAABB();
                            }
                            if (InspectorDangerButton("Remove Mesh Collider##meshcol"))
                                selectedEntity->RemoveComponent<MeshColliderComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Mesh Collider"))
                            selectedEntity->AddComponent<MeshColliderComponent>();
                    }

                    // Collision Events Component (trigger/collision callbacks for gameplay testing)
                    if (selectedEntity->HasComponent<CollisionEventsComponent>())
                    {
                        if (BeginInspectorSection("Collision Events"))
                        {
                            InspectorGroupLabel("Diagnostics");
                            ImGui::TextWrapped("Logs collision and trigger enter/exit events to the console for gameplay debugging.");
                            if (InspectorDangerButton("Remove Collision Events"))
                                selectedEntity->RemoveComponent<CollisionEventsComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Collision Events"))
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
                        if (BeginInspectorSection("Joint"))
                        {
                            auto& joint = selectedEntity->GetComponent<JointComponent>();

                            InspectorGroupLabel("Setup");
                            ImGui::Checkbox("Enabled", &joint.enabled);

                            const char* jointTypeNames[] = { "Fixed", "Spring", "Hinge" };
                            int jointTypeIndex = static_cast<int>(joint.type);
                            if (ImGui::Combo("Type", &jointTypeIndex, jointTypeNames, IM_ARRAYSIZE(jointTypeNames)))
                                joint.type = static_cast<JointType>(jointTypeIndex);

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
                                    joint.connectedEntityID = 0;
                                for (const auto& other : scene.GetEntities())
                                {
                                    if (!other || other->GetID() == selectedEntity->GetID())
                                        continue;
                                    bool isSelected = (other->GetID() == joint.connectedEntityID);
                                    if (ImGui::Selectable(other->GetName().c_str(), isSelected))
                                        joint.connectedEntityID = other->GetID();
                                }
                                ImGui::EndCombo();
                            }

                            InspectorGroupLabel("Anchors");
                            ImGui::DragFloat3("Anchor Offset", &joint.anchor.x, 0.05f);
                            ImGui::DragFloat3("Connected Anchor", &joint.connectedAnchor.x, 0.05f);
                            if (InspectorActionButton("Snap Anchors to Current Offset##snapAnchor"))
                            {
                                if (selectedEntity->HasComponent<TransformComponent>())
                                {
                                    joint.anchor = glm::vec3(0.0f);
                                    if (joint.connectedEntityID != 0)
                                    {
                                        auto connected = TransformHierarchy::FindEntityByID(scene, joint.connectedEntityID);
                                        if (connected && connected->HasComponent<TransformComponent>())
                                        {
                                            auto& selfTf  = selectedEntity->GetComponent<TransformComponent>();
                                            auto& otherTf = connected->GetComponent<TransformComponent>();
                                            joint.connectedAnchor = otherTf.position - selfTf.position;
                                        }
                                    }
                                    else
                                    {
                                        auto& selfTf = selectedEntity->GetComponent<TransformComponent>();
                                        joint.connectedAnchor = selfTf.position;
                                    }
                                }
                            }
                            ImGui::SetItemTooltip("Computes connected anchor as the current relative offset between the two bodies");

                            if (selectedEntity->HasComponent<TransformComponent>())
                            {
                                auto& selfTf = selectedEntity->GetComponent<TransformComponent>();
                                glm::vec3 worldAnchor = selfTf.position + joint.anchor;
                                ImGui::TextDisabled("World Anchor: %.2f, %.2f, %.2f", worldAnchor.x, worldAnchor.y, worldAnchor.z);
                            }

                            InspectorGroupLabel("Constraint");
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

                            InspectorGroupLabel("Break Settings");
                            ImGui::DragFloat("Break Force", &joint.breakForce, 0.5f, 0.0f, 10000.0f);
                            ImGui::SetItemTooltip("Joint is removed when corrective force exceeds this value. 0 = unbreakable.");
                            if (joint.breakForce > 0.0f)
                                ImGui::TextDisabled("Joint will break above %.1f N", joint.breakForce);

                            if (InspectorDangerButton("Remove Joint"))
                                selectedEntity->RemoveComponent<JointComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Joint"))
                            selectedEntity->AddComponent<JointComponent>();
                    }

                    // Animation Component
                    if (selectedEntity->HasComponent<SkeletonComponent>() || selectedEntity->HasComponent<AnimationComponent>())
                    {
                        if (!selectedEntity->HasComponent<AnimationComponent>())
                        {
                            if (ImGui::Button("Add Animation Component"))
                            {
                                auto& anim = selectedEntity->AddComponent<AnimationComponent>();
                                anim.playing = true;
                                anim.looping = true;
                            }
                        }
                        else if (BeginInspectorSection("Animation"))
                        {
                            auto& anim = selectedEntity->GetComponent<AnimationComponent>();
                            std::shared_ptr<MyEngine::Skeleton> skeleton = selectedEntity->HasComponent<SkeletonComponent>()
                                ? selectedEntity->GetComponent<SkeletonComponent>().skeleton
                                : nullptr;
                            const bool hasClips = anim.clips && !anim.clips->empty();
                            AnimationStateMachineComponent* smComponent = selectedEntity->HasComponent<AnimationStateMachineComponent>()
                                ? &selectedEntity->GetComponent<AnimationStateMachineComponent>()
                                : nullptr;

                            InspectorGroupLabel("Playback");
                            ImGui::Checkbox("Playing##anim", &anim.playing);
                            ImGui::SameLine();
                            ImGui::Checkbox("Looping##anim", &anim.looping);
                            ImGui::DragFloat("Playback Speed##anim", &anim.playbackSpeed, 0.01f, 0.0f, 4.0f, "%.2f");

                            if (smComponent)
                            {
                                InspectorGroupLabel("State Machine");
                                ImGui::TextWrapped("State Machine: %s", smComponent->assetPath.empty() ? "<unsaved>" : smComponent->assetPath.c_str());
                                ImGui::Checkbox("Auto Initialize##animsm", &smComponent->autoInitialize);
                                ImGui::SameLine();
                                ImGui::Checkbox("Pause Transitions##animsm", &smComponent->debugPauseTransitions);

                                if (smComponent->stateMachine)
                                {
                                    if (InspectorActionButton("Edit State Machine##animsm"))
                                    {
                                        editingAnimationStateMachine = smComponent->stateMachine;
                                        selectedAnimationStateMachinePath = smComponent->assetPath;
                                    }
                                }
                                if (InspectorDangerButton("Remove State Machine##animsm"))
                                {
                                    selectedEntity->RemoveComponent<AnimationStateMachineComponent>();
                                    smComponent = nullptr;
                                }

                                if (smComponent && smComponent->stateMachine)
                                {
                                    auto& sm = *smComponent->stateMachine;
                                    ImGui::Text("Current State Index: %d", smComponent->currentStateIndex);
                                    if (!smComponent->debugCurrentStateName.empty())
                                        ImGui::Text("Current State: %s", smComponent->debugCurrentStateName.c_str());
                                    else if (sm.IsValidStateIndex(smComponent->currentStateIndex))
                                        ImGui::Text("Current State: %s", sm.states[smComponent->currentStateIndex].name.c_str());

                                    if (!smComponent->debugPendingStateName.empty())
                                        ImGui::Text("Pending Transition: %s", smComponent->debugPendingStateName.c_str());
                                    else
                                        ImGui::TextDisabled("Pending Transition: none");

                                    if (smComponent->debugLastBlockedTransitionIndex >= 0)
                                    {
                                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.35f, 1.0f),
                                            "Blocked Transition [%d]: %s",
                                            smComponent->debugLastBlockedTransitionIndex,
                                            smComponent->debugLastBlockedReason.c_str());
                                    }

                                    if (!smComponent->debugTransitionMessages.empty() &&
                                        ImGui::CollapsingHeader("Transition Debug##animsm", ImGuiTreeNodeFlags_DefaultOpen))
                                    {
                                        for (const auto& message : smComponent->debugTransitionMessages)
                                            ImGui::BulletText("%s", message.c_str());
                                    }

                                    for (size_t paramIndex = 0; paramIndex < sm.parameters.size(); ++paramIndex)
                                    {
                                        if (paramIndex >= smComponent->parameterValues.size())
                                            smComponent->parameterValues.resize(sm.parameters.size());
                                        auto& parameter = sm.parameters[paramIndex];
                                        auto& value = smComponent->parameterValues[paramIndex];
                                        if (parameter.type == MyEngine::AnimationStateMachineParameterType::Bool)
                                        {
                                            ImGui::Checkbox((parameter.name + "##animsmparam").c_str(), &value.boolValue);
                                        }
                                        else if (parameter.type == MyEngine::AnimationStateMachineParameterType::Float)
                                        {
                                            ImGui::DragFloat((parameter.name + "##animsmparam").c_str(), &value.floatValue, 0.01f);
                                        }
                                        else if (parameter.type == MyEngine::AnimationStateMachineParameterType::Trigger)
                                        {
                                            bool triggerPressed = ImGui::Button((parameter.name + "##animsmtrigger").c_str());
                                            ImGui::SameLine();
                                            ImGui::TextDisabled(value.triggerValue ? "armed" : "idle");
                                            if (triggerPressed)
                                                value.triggerValue = true;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                InspectorGroupLabel("State Machine");
                                if (InspectorActionButton("Create State Machine##animsm"))
                                {
                                    auto& sm = selectedEntity->AddComponent<AnimationStateMachineComponent>();
                                    sm.stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
                                    sm.stateMachine->name = selectedEntity->GetName() + " State Machine";
                                    if (hasClips)
                                    {
                                        MyEngine::AnimationStateMachineState idleState;
                                        idleState.name = "Default";
                                        idleState.clipName = (*anim.clips)[std::max(anim.activeClipIndex, 0)].name;
                                        sm.stateMachine->states.push_back(idleState);
                                        sm.stateMachine->defaultStateIndex = 0;
                                    }
                                    editingAnimationStateMachine = sm.stateMachine;
                                    selectedAnimationStateMachinePath.clear();
                                    smComponent = &sm;
                                }
                                if (InspectorActionButton("Assign State Machine...##animsm"))
                                {
                                    std::string path = MyEngine::FileDialog::OpenAnimationStateMachineFile();
                                    if (!path.empty())
                                    {
                                        auto stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
                                        if (stateMachine->LoadFromFile(path))
                                        {
                                            auto& sm = selectedEntity->AddComponent<AnimationStateMachineComponent>();
                                            sm.stateMachine = stateMachine;
                                            sm.assetPath = path;
                                            sm.parameterValues.clear();
                                            sm.currentStateIndex = stateMachine->defaultStateIndex;
                                            editingAnimationStateMachine = stateMachine;
                                            selectedAnimationStateMachinePath = path;
                                            smComponent = &sm;
                                        }
                                    }
                                }
                            }

                            InspectorGroupLabel("Clip Preview");
                            if (hasClips)
                            {
                                if (anim.activeClipIndex < 0 || anim.activeClipIndex >= static_cast<int>(anim.clips->size()))
                                    anim.activeClipIndex = 0;

                                std::string currentClipLabel = (*anim.clips)[anim.activeClipIndex].name;
                                if (currentClipLabel.empty())
                                    currentClipLabel = "Clip " + std::to_string(anim.activeClipIndex);

                                if (ImGui::BeginCombo("Active Clip##anim", currentClipLabel.c_str()))
                                {
                                    for (int clipIndex = 0; clipIndex < static_cast<int>(anim.clips->size()); ++clipIndex)
                                    {
                                        std::string clipLabel = (*anim.clips)[clipIndex].name;
                                        if (clipLabel.empty())
                                            clipLabel = "Clip " + std::to_string(clipIndex);

                                        bool isSelected = (clipIndex == anim.activeClipIndex);
                                        if (ImGui::Selectable(clipLabel.c_str(), isSelected))
                                        {
                                            if (clipIndex != anim.activeClipIndex)
                                                anim.TransitionTo(clipIndex, 0.2f);
                                        }
                                        if (isSelected)
                                            ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }

                                const auto& activeClip = (*anim.clips)[anim.activeClipIndex];
                                float durationSeconds = activeClip.GetDurationSeconds();
                                if (durationSeconds > 0.0001f)
                                {
                                    anim.time = std::clamp(anim.time, 0.0f, durationSeconds);
                                    ImGui::SliderFloat("Time##anim", &anim.time, 0.0f, durationSeconds, "%.2fs");
                                    ImGui::Text("Duration: %.2fs | Ticks/Sec: %.2f | Tracks: %d",
                                        durationSeconds,
                                        activeClip.ticksPerSecond,
                                        static_cast<int>(activeClip.tracks.size()));
                                }
                                else
                                {
                                    ImGui::TextDisabled("Active clip has no duration.");
                                }
                            }
                            else
                            {
                                ImGui::TextDisabled("No animation clips assigned.");
                            }

                            if (InspectorActionButton("Restart Clip##anim"))
                            {
                                anim.time = 0.0f;
                                anim.previousTime = 0.0f;
                                anim.blendElapsed = 0.0f;
                            }
                            if (InspectorActionButton("Pause/Resume##anim"))
                            {
                                anim.playing = !anim.playing;
                            }

                            InspectorGroupLabel("Import");
                            static std::string animationImportStatus;
                            if (InspectorActionButton("Import Animation File...##anim"))
                            {
                                std::string path = MyEngine::FileDialog::OpenModelFile();
                                if (!path.empty())
                                {
                                    auto externalClips = MyEngine::AssetManager::LoadAnimationClips(path);
                                    if (!externalClips || externalClips->empty())
                                    {
                                        animationImportStatus = "No animation clips found in file.";
                                    }
                                    else
                                    {
                                        if (!anim.clips)
                                            anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();

                                        std::filesystem::path sourcePath(path);
                                        std::string sourceStem = sourcePath.stem().string();
                                        int importedCount = 0;
                                        for (const auto& clip : *externalClips)
                                        {
                                            if (!MyEngine::AssetManager::IsAnimationClipCompatible(clip, skeleton))
                                                continue;

                                            MyEngine::AnimationClip importedClip = clip;
                                            std::string baseName = importedClip.name.empty()
                                                ? sourceStem
                                                : importedClip.name;
                                            importedClip.name = baseName;

                                            bool duplicateName = false;
                                            for (const auto& existingClip : *anim.clips)
                                            {
                                                if (existingClip.name == importedClip.name)
                                                {
                                                    duplicateName = true;
                                                    break;
                                                }
                                            }
                                            if (duplicateName)
                                                importedClip.name += " [" + sourceStem + "]";

                                            anim.clips->push_back(std::move(importedClip));
                                            ++importedCount;
                                        }

                                        if (importedCount > 0)
                                        {
                                            if (anim.activeClipIndex < 0)
                                                anim.activeClipIndex = 0;
                                            animationImportStatus = "Imported " + std::to_string(importedCount) + " clip(s).";
                                        }
                                        else
                                        {
                                            animationImportStatus = "No compatible clips found for this skeleton.";
                                        }
                                    }
                                }
                            }
                            if (!animationImportStatus.empty())
                                ImGui::TextWrapped("%s", animationImportStatus.c_str());
                        }
                    }

                    // Audio Source Component
                    if (selectedEntity->HasComponent<AudioSourceComponent>())
                    {
                        if (BeginInspectorSection("Audio Source"))
                        {
                            auto& source = selectedEntity->GetComponent<AudioSourceComponent>();

                            InspectorGroupLabel("Clip");
                            if (source.clip)
                                ImGui::TextWrapped("Clip: %s", source.clipPath.c_str());
                            else
                                ImGui::TextDisabled("No clip loaded");

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

                            if (InspectorActionButton("Rescan Clips##audio"))
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

                            InspectorGroupLabel("Playback");
                            ImGui::SliderFloat("Volume", &source.volume, 0.0f, 1.0f);
                            ImGui::DragFloat("Pitch", &source.pitch, 0.01f, 0.1f, 4.0f);
                            ImGui::Checkbox("Loop", &source.loop);
                            ImGui::Checkbox("Auto Play", &source.autoPlay);

                            InspectorGroupLabel("Spatial");
                            ImGui::Checkbox("Spatial (3D)", &source.spatial);
                            if (source.spatial)
                            {
                                ImGui::DragFloat("Min Distance", &source.minDistance, 0.1f, 0.1f, 1000.0f);
                                ImGui::DragFloat("Max Distance", &source.maxDistance, 1.0f, 1.0f, 10000.0f);
                            }

                            InspectorGroupLabel("Transport");
                            if (source.clip && source.clip->IsValid())
                            {
                                if (!source.isPlaying)
                                {
                                    if (InspectorActionButton("Play##audio"))
                                        source.playRequested = true;
                                }
                                else
                                {
                                    if (InspectorDangerButton("Stop##audio"))
                                        source.stopRequested = true;
                                }
                                ImGui::TextDisabled(source.isPlaying ? "Status: Playing" : "Status: Stopped");
                            }
                            else
                            {
                                ImGui::TextDisabled("Status: waiting for a valid clip");
                            }

                            if (InspectorDangerButton("Remove Audio Source"))
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
                        if (InspectorActionButton("Add Audio Source"))
                        {
                            selectedEntity->AddComponent<AudioSourceComponent>();
                        }
                    }

                    // Script Component
                    if (selectedEntity->HasComponent<ScriptComponent>())
                    {
                        if (BeginInspectorSection("Script"))
                        {
                            auto& script = selectedEntity->GetComponent<ScriptComponent>();

                            InspectorGroupLabel("Runtime");
                            ImGui::Checkbox("Enabled##script", &script.enabled);
                            ImGui::Checkbox("Auto Start##script", &script.autoStart);

                            InspectorGroupLabel("Source");
                            if (script.scriptPath.find("rin_animation_hotkeys.lua") != std::string::npos)
                            {
                                ImGui::TextWrapped("Hotkeys: press 1-9 to switch imported animation clips for this character.");
                            }

                            std::string scriptDisplay = script.scriptPath.empty() ? "(none)" : script.scriptPath;
                            ImGui::TextWrapped("Path: %s", scriptDisplay.c_str());
                            ImGui::TextDisabled("Status: %s", script.scriptPath.empty() ? "No script assigned" : (script.requestReload ? "Reload pending" : "Ready"));

                            if (InspectorActionButton("Browse Script...##script"))
                            {
                                std::string path = MyEngine::FileDialog::OpenScriptFile();
                                if (!path.empty())
                                {
                                    script.scriptPath = path;
                                    script.requestReload = true;
                                }
                            }
                            if (InspectorActionButton("Reload Script##script"))
                            {
                                script.requestReload = true;
                            }

                            if (InspectorActionButton("Clear Script Path##script"))
                            {
                                script.scriptPath.clear();
                                script.requestReload = true;
                            }

                            if (InspectorDangerButton("Remove Script Component"))
                            {
                                selectedEntity->RemoveComponent<ScriptComponent>();
                            }
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Script Component"))
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

                            ImGui::Separator();
                            ImGui::Text("Master Audio");
                            float masterVolume = MyEngine::AudioEngine::GetMasterVolume();
                            if (ImGui::SliderFloat("Master Volume", &masterVolume, 0.0f, 1.0f))
                                MyEngine::AudioEngine::SetMasterVolume(masterVolume);
                            bool muted = MyEngine::AudioEngine::IsMuted();
                            if (ImGui::Checkbox("Mute", &muted))
                                MyEngine::AudioEngine::SetMuted(muted);

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
                        if (BeginInspectorSection("Particle Emitter"))
                        {
                            auto& emitter = selectedEntity->GetComponent<ParticleEmitterComponent>();

                            InspectorGroupLabel("Simulation");
                            bool emitting = emitter.emitting;
                            if (ImGui::Checkbox("Preview Emission", &emitting))
                            {
                                emitter.emitting = emitting;
                                emitter.poolDirty = true;
                            }
                            ImGui::TextDisabled("Toggle live particle spawning for preview.");

                            if (ImGui::SliderInt("Max Particles", &emitter.maxParticles, 1, 10000))
                                emitter.poolDirty = true;

                            ImGui::SliderFloat("Spawn Rate", &emitter.spawnRate, 0.0f, 500.0f);
                            ImGui::SliderFloat("Lifetime", &emitter.lifetime, 0.05f, 20.0f);
                            ImGui::SliderFloat("Lifetime Variance", &emitter.lifetimeVariance, 0.0f, 5.0f);

                            InspectorGroupLabel("Shape");
                            int shape = static_cast<int>(emitter.shape);
                            const char* shapeNames[] = { "Point", "Sphere", "Box", "Cone" };
                            if (ImGui::Combo("Shape", &shape, shapeNames, IM_ARRAYSIZE(shapeNames)))
                            {
                                emitter.shape = static_cast<ParticleEmitterComponent::EmissionShape>(shape);
                            }
                            switch (emitter.shape)
                            {
                            case ParticleEmitterComponent::EmissionShape::Sphere:
                                ImGui::SliderFloat("Radius##shape", &emitter.shapeRadius, 0.0f, 10.0f);
                                break;
                            case ParticleEmitterComponent::EmissionShape::Box:
                                ImGui::SliderFloat3("Box Extents##shape", &emitter.shapeExtents.x, 0.0f, 10.0f);
                                break;
                            case ParticleEmitterComponent::EmissionShape::Cone:
                                ImGui::SliderFloat("Cone Radius##shape", &emitter.shapeRadius, 0.0f, 10.0f);
                                ImGui::SliderFloat("Cone Height##shape", &emitter.shapeHeight, 0.0f, 20.0f);
                                break;
                            case ParticleEmitterComponent::EmissionShape::Point:
                            default:
                                ImGui::TextDisabled("Using emitter origin");
                                break;
                            }

                            InspectorGroupLabel("Rendering");
                            int blendMode = static_cast<int>(emitter.blendMode);
                            const char* blendNames[] = { "Alpha", "Additive" };
                            if (ImGui::Combo("Blend Mode", &blendMode, blendNames, IM_ARRAYSIZE(blendNames)))
                                emitter.blendMode = static_cast<ParticleEmitterComponent::BlendMode>(blendMode);

                            InspectorGroupLabel("Emission");
                            ImGui::SliderFloat3("Direction##emit", &emitter.emitDirection.x, -1.0f, 1.0f);
                            ImGui::SliderFloat("Speed", &emitter.emitSpeed, 0.0f, 30.0f);
                            ImGui::SliderFloat("Speed Variance", &emitter.emitSpeedVariance, 0.0f, 10.0f);
                            ImGui::SliderFloat("Spread Angle", &emitter.spreadAngle, 0.0f, 180.0f);
                            ImGui::SliderFloat3("Gravity##emit", &emitter.gravity.x, -20.0f, 20.0f);

                            InspectorGroupLabel("Appearance");
                            ImGui::ColorEdit4("Color Start", &emitter.colorStart.r);
                            ImGui::ColorEdit4("Color End",   &emitter.colorEnd.r);
                            ImGui::SliderFloat("Size Start", &emitter.sizeStart, 0.0f, 5.0f);
                            ImGui::SliderFloat("Size End",   &emitter.sizeEnd,   0.0f, 5.0f);

                            InspectorGroupLabel("Presets");
                            if (InspectorActionButton("Fire##particlePreset"))
                            {
                                emitter.emitting = true;
                                emitter.shape = ParticleEmitterComponent::EmissionShape::Cone;
                                emitter.shapeRadius = 0.35f;
                                emitter.shapeHeight = 0.6f;
                                emitter.spawnRate = 80.0f;
                                emitter.lifetime = 1.0f;
                                emitter.lifetimeVariance = 0.3f;
                                emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
                                emitter.emitSpeed = 3.5f;
                                emitter.emitSpeedVariance = 1.0f;
                                emitter.spreadAngle = 18.0f;
                                emitter.colorStart = glm::vec4(1.0f, 0.75f, 0.2f, 1.0f);
                                emitter.colorEnd = glm::vec4(0.9f, 0.1f, 0.0f, 0.0f);
                                emitter.sizeStart = 0.25f;
                                emitter.sizeEnd = 0.0f;
                                emitter.gravity = glm::vec3(0.0f, 1.0f, 0.0f);
                                emitter.poolDirty = true;
                            }
                            if (InspectorActionButton("Smoke##particlePreset"))
                            {
                                emitter.emitting = true;
                                emitter.shape = ParticleEmitterComponent::EmissionShape::Sphere;
                                emitter.shapeRadius = 0.4f;
                                emitter.spawnRate = 20.0f;
                                emitter.lifetime = 4.0f;
                                emitter.lifetimeVariance = 1.5f;
                                emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
                                emitter.emitSpeed = 0.8f;
                                emitter.emitSpeedVariance = 0.4f;
                                emitter.spreadAngle = 55.0f;
                                emitter.colorStart = glm::vec4(0.35f, 0.35f, 0.35f, 0.7f);
                                emitter.colorEnd = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
                                emitter.sizeStart = 0.35f;
                                emitter.sizeEnd = 1.2f;
                                emitter.gravity = glm::vec3(0.0f, 0.3f, 0.0f);
                                emitter.poolDirty = true;
                            }
                            if (InspectorActionButton("Sparks##particlePreset"))
                            {
                                emitter.emitting = true;
                                emitter.shape = ParticleEmitterComponent::EmissionShape::Point;
                                emitter.spawnRate = 150.0f;
                                emitter.lifetime = 0.6f;
                                emitter.lifetimeVariance = 0.2f;
                                emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
                                emitter.emitSpeed = 8.0f;
                                emitter.emitSpeedVariance = 3.0f;
                                emitter.spreadAngle = 65.0f;
                                emitter.colorStart = glm::vec4(1.0f, 0.9f, 0.3f, 1.0f);
                                emitter.colorEnd = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);
                                emitter.sizeStart = 0.08f;
                                emitter.sizeEnd = 0.0f;
                                emitter.gravity = glm::vec3(0.0f, -8.0f, 0.0f);
                                emitter.poolDirty = true;
                            }

                            InspectorGroupLabel("Texture");
                            static char particleTexturePathBuf[256] = "";
                            if (particleTexturePathBuf[0] == '\0' && !emitter.texturePath.empty())
                            {
                                std::snprintf(particleTexturePathBuf, sizeof(particleTexturePathBuf), "%s", emitter.texturePath.c_str());
                            }
                            if (ImGui::InputText("Texture Path", particleTexturePathBuf, sizeof(particleTexturePathBuf)))
                            {
                                emitter.texturePath = particleTexturePathBuf;
                                emitter.poolDirty = true;
                            }
                            if (InspectorActionButton("Browse Texture##particle"))
                            {
                                std::string picked = MyEngine::FileDialog::OpenImageFile();
                                if (!picked.empty())
                                {
                                    emitter.texturePath = picked;
                                    std::snprintf(particleTexturePathBuf, sizeof(particleTexturePathBuf), "%s", picked.c_str());
                                    emitter.poolDirty = true;
                                }
                            }
                            if (InspectorActionButton("Clear Texture##particle"))
                            {
                                emitter.texturePath.clear();
                                particleTexturePathBuf[0] = '\0';
                                emitter.poolDirty = true;
                            }

                            if (!emitter.texturePath.empty())
                            {
                                ImGui::TextWrapped("%s", emitter.texturePath.c_str());
                            }

                            InspectorGroupLabel("Debug");
                            int alive = 0;
                            for (const auto& p : emitter.particles)
                                if (p.alive) ++alive;
                            ImGui::Text("Alive: %d / %d", alive, emitter.maxParticles);

                            if (InspectorDangerButton("Remove Particle Emitter"))
                                selectedEntity->RemoveComponent<ParticleEmitterComponent>();
                        }
                    }
                    else
                    {
                        if (InspectorActionButton("Add Particle Emitter"))
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
            // Input Bindings Panel
            // ============================================================
            if (showInputBindingsPanel)
            {
                ImGui::SetNextWindowPos(ImVec2(370, g_WindowHeight - 460), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 440), ImGuiCond_FirstUseEver);
                ImGui::Begin("Input Bindings", &showInputBindingsPanel);

                static char bindingsPath[256] = "bindings.json";
                static char bindingsStatus[128] = "";

                ImGui::InputText("Bindings File", bindingsPath, sizeof(bindingsPath));
                if (ImGui::Button("Load Bindings"))
                {
                    if (MyEngine::InputActions::LoadBindings(bindingsPath))
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Loaded %s", bindingsPath);
                    else
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Failed to load %s", bindingsPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Save Bindings"))
                {
                    if (MyEngine::InputActions::SaveBindings(bindingsPath))
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Saved %s", bindingsPath);
                    else
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Failed to save %s", bindingsPath);
                }
                ImGui::TextUnformatted(bindingsStatus);

                float gamepadDeadzone = MyEngine::InputActions::GetGamepadDeadzone();
                if (ImGui::SliderFloat("Gamepad Deadzone", &gamepadDeadzone, 0.0f, 0.5f, "%.2f"))
                    MyEngine::InputActions::SetGamepadDeadzone(gamepadDeadzone);
                ImGui::Text("Gamepad: %s", MyEngine::InputActions::IsGamepadConnected() ? "Connected" : "Not connected");

                auto profileNames = MyEngine::InputActions::GetProfileNames();
                std::string activeProfile = MyEngine::InputActions::GetActiveProfileName();
                if (activeProfile.empty() && !profileNames.empty())
                    activeProfile = profileNames.front();

                if (ImGui::BeginCombo("Active Profile", activeProfile.c_str()))
                {
                    for (const auto& profileName : profileNames)
                    {
                        bool selected = (profileName == activeProfile);
                        if (ImGui::Selectable(profileName.c_str(), selected))
                            MyEngine::InputActions::SetActiveProfile(profileName);
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                static char newProfileName[64] = "";
                ImGui::InputText("New Profile", newProfileName, sizeof(newProfileName));
                if (ImGui::Button("Create Profile (Copy Current)"))
                {
                    if (MyEngine::InputActions::CreateProfile(newProfileName, true))
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Created profile %s", newProfileName);
                    else
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Failed to create profile %s", newProfileName);
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete Active Profile"))
                {
                    if (MyEngine::InputActions::DeleteProfile(MyEngine::InputActions::GetActiveProfileName()))
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Deleted active profile");
                    else
                        std::snprintf(bindingsStatus, sizeof(bindingsStatus), "Failed to delete active profile");
                }

                static const std::array<std::pair<int, const char*>, 15> keyOptions = {{
                    { -1, "None" },
                    { GLFW_KEY_W, "W" }, { GLFW_KEY_A, "A" }, { GLFW_KEY_S, "S" }, { GLFW_KEY_D, "D" },
                    { GLFW_KEY_Q, "Q" }, { GLFW_KEY_E, "E" }, { GLFW_KEY_R, "R" }, { GLFW_KEY_F, "F" },
                    { GLFW_KEY_SPACE, "Space" }, { GLFW_KEY_LEFT_SHIFT, "Left Shift" }, { GLFW_KEY_LEFT_CONTROL, "Left Ctrl" },
                    { GLFW_KEY_TAB, "Tab" }, { GLFW_KEY_ESCAPE, "Escape" }, { GLFW_KEY_ENTER, "Enter" }
                }};
                static const std::array<std::pair<int, const char*>, 4> mouseOptions = {{
                    { -1, "None" },
                    { GLFW_MOUSE_BUTTON_LEFT, "Mouse Left" },
                    { GLFW_MOUSE_BUTTON_RIGHT, "Mouse Right" },
                    { GLFW_MOUSE_BUTTON_MIDDLE, "Mouse Middle" }
                }};
                static const std::array<std::pair<int, const char*>, 12> gamepadButtonOptions = {{
                    { -1, "None" },
                    { GLFW_GAMEPAD_BUTTON_A, "A" }, { GLFW_GAMEPAD_BUTTON_B, "B" },
                    { GLFW_GAMEPAD_BUTTON_X, "X" }, { GLFW_GAMEPAD_BUTTON_Y, "Y" },
                    { GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, "LB" }, { GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, "RB" },
                    { GLFW_GAMEPAD_BUTTON_BACK, "Back" }, { GLFW_GAMEPAD_BUTTON_START, "Start" },
                    { GLFW_GAMEPAD_BUTTON_LEFT_THUMB, "L3" }, { GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, "R3" },
                    { GLFW_GAMEPAD_BUTTON_DPAD_UP, "DPad Up" }
                }};
                static const std::array<std::pair<int, const char*>, 7> gamepadAxisOptions = {{
                    { -1, "None" },
                    { GLFW_GAMEPAD_AXIS_LEFT_X, "Left Stick X" },
                    { GLFW_GAMEPAD_AXIS_LEFT_Y, "Left Stick Y" },
                    { GLFW_GAMEPAD_AXIS_RIGHT_X, "Right Stick X" },
                    { GLFW_GAMEPAD_AXIS_RIGHT_Y, "Right Stick Y" },
                    { GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, "Left Trigger" },
                    { GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, "Right Trigger" }
                }};

                auto findOptionIndex = [](auto& options, int value)
                {
                    for (int i = 0; i < static_cast<int>(options.size()); ++i)
                    {
                        if (options[i].first == value)
                            return i;
                    }
                    return 0;
                };
                auto findOptionLabel = [](auto& options, int value) -> const char*
                {
                    for (int i = 0; i < static_cast<int>(options.size()); ++i)
                    {
                        if (options[i].first == value)
                            return options[i].second;
                    }
                    return "Unknown";
                };

                const auto actionNames = MyEngine::InputActions::GetActionNames();
                const auto axisNames = MyEngine::InputActions::GetAxisNames();

                // Capture rebinding state
                enum CaptureMode
                {
                    CaptureNone = 0,
                    CaptureActionKey,
                    CaptureActionMouse,
                    CaptureActionGamepad,
                    CaptureAxisPositiveKey,
                    CaptureAxisNegativeKey,
                    CaptureAxisGamepadAxis
                };
                static int captureMode = CaptureNone;
                static std::string captureTarget;

                int capturedKey = -1;
                int capturedMouse = -1;
                int capturedGamepadButton = -1;
                int capturedGamepadAxis = -1;

                for (size_t i = 1; i < keyOptions.size(); ++i)
                {
                    if (Input::IsKeyPressed(keyOptions[i].first))
                    {
                        capturedKey = keyOptions[i].first;
                        break;
                    }
                }
                for (size_t i = 1; i < mouseOptions.size(); ++i)
                {
                    if (Input::IsMouseButtonPressed(mouseOptions[i].first))
                    {
                        capturedMouse = mouseOptions[i].first;
                        break;
                    }
                }
                for (size_t i = 1; i < gamepadButtonOptions.size(); ++i)
                {
                    if (MyEngine::InputActions::IsGamepadButtonPressed(gamepadButtonOptions[i].first))
                    {
                        capturedGamepadButton = gamepadButtonOptions[i].first;
                        break;
                    }
                }
                for (size_t i = 1; i < gamepadAxisOptions.size(); ++i)
                {
                    float axisValue = MyEngine::InputActions::GetGamepadAxisRaw(gamepadAxisOptions[i].first);
                    if (std::fabs(axisValue) > 0.6f)
                    {
                        capturedGamepadAxis = gamepadAxisOptions[i].first;
                        break;
                    }
                }

                if (captureMode != CaptureNone)
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.3f, 1.0f), "Capture armed for %s", captureTarget.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel Capture"))
                    {
                        captureMode = CaptureNone;
                        captureTarget.clear();
                    }
                }

                bool hasConflictWarnings = false;
                for (size_t i = 0; i < actionNames.size(); ++i)
                {
                    MyEngine::InputActions::ActionBinding a;
                    if (!MyEngine::InputActions::TryGetActionBinding(actionNames[i], a))
                        continue;
                    int aKey = a.keys.empty() ? -1 : a.keys[0];
                    int aMouse = a.mouseButtons.empty() ? -1 : a.mouseButtons[0];
                    int aPad = a.gamepadButtons.empty() ? -1 : a.gamepadButtons[0];

                    for (size_t j = i + 1; j < actionNames.size(); ++j)
                    {
                        MyEngine::InputActions::ActionBinding b;
                        if (!MyEngine::InputActions::TryGetActionBinding(actionNames[j], b))
                            continue;
                        int bKey = b.keys.empty() ? -1 : b.keys[0];
                        int bMouse = b.mouseButtons.empty() ? -1 : b.mouseButtons[0];
                        int bPad = b.gamepadButtons.empty() ? -1 : b.gamepadButtons[0];

                        if (aKey >= 0 && aKey == bKey)
                        {
                            hasConflictWarnings = true;
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                "Conflict: %s and %s share key %s",
                                actionNames[i].c_str(), actionNames[j].c_str(), findOptionLabel(keyOptions, aKey));
                        }
                        if (aMouse >= 0 && aMouse == bMouse)
                        {
                            hasConflictWarnings = true;
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                "Conflict: %s and %s share mouse %s",
                                actionNames[i].c_str(), actionNames[j].c_str(), findOptionLabel(mouseOptions, aMouse));
                        }
                        if (aPad >= 0 && aPad == bPad)
                        {
                            hasConflictWarnings = true;
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                "Conflict: %s and %s share gamepad %s",
                                actionNames[i].c_str(), actionNames[j].c_str(), findOptionLabel(gamepadButtonOptions, aPad));
                        }
                    }
                }

                for (size_t i = 0; i < axisNames.size(); ++i)
                {
                    MyEngine::InputActions::AxisBinding a;
                    if (!MyEngine::InputActions::TryGetAxisBinding(axisNames[i], a))
                        continue;
                    int aPos = (!a.keyPairs.empty()) ? a.keyPairs[0].first : -1;
                    int aNeg = (!a.keyPairs.empty()) ? a.keyPairs[0].second : -1;
                    int aAxis = a.gamepadAxes.empty() ? -1 : a.gamepadAxes[0];

                    for (size_t j = i + 1; j < axisNames.size(); ++j)
                    {
                        MyEngine::InputActions::AxisBinding b;
                        if (!MyEngine::InputActions::TryGetAxisBinding(axisNames[j], b))
                            continue;
                        int bPos = (!b.keyPairs.empty()) ? b.keyPairs[0].first : -1;
                        int bNeg = (!b.keyPairs.empty()) ? b.keyPairs[0].second : -1;
                        int bAxis = b.gamepadAxes.empty() ? -1 : b.gamepadAxes[0];

                        if ((aPos >= 0 && (aPos == bPos || aPos == bNeg)) || (aNeg >= 0 && (aNeg == bPos || aNeg == bNeg)))
                        {
                            hasConflictWarnings = true;
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                "Conflict: %s and %s share keyboard axis keys",
                                axisNames[i].c_str(), axisNames[j].c_str());
                        }
                        if (aAxis >= 0 && aAxis == bAxis)
                        {
                            hasConflictWarnings = true;
                            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                                "Conflict: %s and %s share gamepad axis %s",
                                axisNames[i].c_str(), axisNames[j].c_str(), findOptionLabel(gamepadAxisOptions, aAxis));
                        }
                    }
                }

                if (!hasConflictWarnings)
                    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "No binding conflicts detected.");

                if (hasConflictWarnings)
                {
                    if (ImGui::Button("Auto-Fix Conflicts (Keep First)"))
                        MyEngine::InputActions::ResolveConflictsKeepFirst();

                    if (ImGui::Button("Auto-Fix Conflicts (Keep Last)"))
                        MyEngine::InputActions::ResolveConflictsKeepLast();
                }

                ImGui::Separator();

                if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& actionName : actionNames)
                    {
                        MyEngine::InputActions::ActionBinding binding;
                        if (!MyEngine::InputActions::TryGetActionBinding(actionName, binding))
                            continue;

                        int keyCode = binding.keys.empty() ? -1 : binding.keys[0];
                        int mouseCode = binding.mouseButtons.empty() ? -1 : binding.mouseButtons[0];
                        int gamepadCode = binding.gamepadButtons.empty() ? -1 : binding.gamepadButtons[0];
                        int gamepadAxisAction = binding.gamepadAxes.empty() ? -1 : binding.gamepadAxes[0];
                        float actionAxisThreshold = binding.axisThreshold;
                        bool actionInvertAxis = binding.invertAxis;

                        if (ImGui::TreeNode(actionName.c_str()))
                        {
                            int keyIdx = findOptionIndex(keyOptions, keyCode);
                            if (ImGui::BeginCombo("Keyboard", keyOptions[keyIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(keyOptions.size()); ++i)
                                {
                                    bool selected = (i == keyIdx);
                                    if (ImGui::Selectable(keyOptions[i].second, selected))
                                        keyCode = keyOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            int mouseIdx = findOptionIndex(mouseOptions, mouseCode);
                            if (ImGui::BeginCombo("Mouse", mouseOptions[mouseIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(mouseOptions.size()); ++i)
                                {
                                    bool selected = (i == mouseIdx);
                                    if (ImGui::Selectable(mouseOptions[i].second, selected))
                                        mouseCode = mouseOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            int gamepadIdx = findOptionIndex(gamepadButtonOptions, gamepadCode);
                            if (ImGui::BeginCombo("Gamepad", gamepadButtonOptions[gamepadIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(gamepadButtonOptions.size()); ++i)
                                {
                                    bool selected = (i == gamepadIdx);
                                    if (ImGui::Selectable(gamepadButtonOptions[i].second, selected))
                                        gamepadCode = gamepadButtonOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            if (ImGui::Button("Capture Key"))
                            {
                                captureMode = CaptureActionKey;
                                captureTarget = actionName;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Capture Mouse"))
                            {
                                captureMode = CaptureActionMouse;
                                captureTarget = actionName;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Capture Pad"))
                            {
                                captureMode = CaptureActionGamepad;
                                captureTarget = actionName;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Capture Axis"))
                            {
                                captureMode = CaptureAxisGamepadAxis;
                                captureTarget = actionName;
                            }

                            if (captureTarget == actionName)
                            {
                                if (captureMode == CaptureActionKey && capturedKey >= 0)
                                {
                                    keyCode = capturedKey;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                                else if (captureMode == CaptureActionMouse && capturedMouse >= 0)
                                {
                                    mouseCode = capturedMouse;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                                else if (captureMode == CaptureActionGamepad && capturedGamepadButton >= 0)
                                {
                                    gamepadCode = capturedGamepadButton;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                                else if (captureMode == CaptureAxisGamepadAxis && capturedGamepadAxis >= 0)
                                {
                                    gamepadAxisAction = capturedGamepadAxis;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                            }

                            int actionAxisIdx = findOptionIndex(gamepadAxisOptions, gamepadAxisAction);
                            if (ImGui::BeginCombo("Gamepad Axis (Action)", gamepadAxisOptions[actionAxisIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(gamepadAxisOptions.size()); ++i)
                                {
                                    bool selected = (i == actionAxisIdx);
                                    if (ImGui::Selectable(gamepadAxisOptions[i].second, selected))
                                        gamepadAxisAction = gamepadAxisOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SliderFloat("Axis Threshold", &actionAxisThreshold, 0.01f, 1.0f, "%.2f");
                            ImGui::Checkbox("Invert Action Axis", &actionInvertAxis);

                            MyEngine::InputActions::ActionBinding updated;
                            if (keyCode >= 0) updated.keys.push_back(keyCode);
                            if (mouseCode >= 0) updated.mouseButtons.push_back(mouseCode);
                            if (gamepadCode >= 0) updated.gamepadButtons.push_back(gamepadCode);
                            if (gamepadAxisAction >= 0) updated.gamepadAxes.push_back(gamepadAxisAction);
                            updated.axisThreshold = actionAxisThreshold;
                            updated.invertAxis = actionInvertAxis;
                            MyEngine::InputActions::BindAction(actionName, updated);

                            ImGui::Text("Down: %s", MyEngine::InputActions::IsAction(actionName) ? "Yes" : "No");
                            ImGui::TreePop();
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Axes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& axisName : axisNames)
                    {
                        MyEngine::InputActions::AxisBinding binding;
                        if (!MyEngine::InputActions::TryGetAxisBinding(axisName, binding))
                            continue;

                        int positiveKey = -1;
                        int negativeKey = -1;
                        if (!binding.keyPairs.empty())
                        {
                            positiveKey = binding.keyPairs[0].first;
                            negativeKey = binding.keyPairs[0].second;
                        }
                        int gamepadAxis = binding.gamepadAxes.empty() ? -1 : binding.gamepadAxes[0];
                        bool invert = binding.invert;
                        float axisDeadzone = binding.deadzone;
                        float axisSensitivity = binding.sensitivity;

                        if (ImGui::TreeNode(axisName.c_str()))
                        {
                            int posIdx = findOptionIndex(keyOptions, positiveKey);
                            if (ImGui::BeginCombo("Positive Key", keyOptions[posIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(keyOptions.size()); ++i)
                                {
                                    bool selected = (i == posIdx);
                                    if (ImGui::Selectable(keyOptions[i].second, selected))
                                        positiveKey = keyOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            int negIdx = findOptionIndex(keyOptions, negativeKey);
                            if (ImGui::BeginCombo("Negative Key", keyOptions[negIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(keyOptions.size()); ++i)
                                {
                                    bool selected = (i == negIdx);
                                    if (ImGui::Selectable(keyOptions[i].second, selected))
                                        negativeKey = keyOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            int axisIdx = findOptionIndex(gamepadAxisOptions, gamepadAxis);
                            if (ImGui::BeginCombo("Gamepad Axis", gamepadAxisOptions[axisIdx].second))
                            {
                                for (int i = 0; i < static_cast<int>(gamepadAxisOptions.size()); ++i)
                                {
                                    bool selected = (i == axisIdx);
                                    if (ImGui::Selectable(gamepadAxisOptions[i].second, selected))
                                        gamepadAxis = gamepadAxisOptions[i].first;
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            if (ImGui::Button("Capture +Key"))
                            {
                                captureMode = CaptureAxisPositiveKey;
                                captureTarget = axisName;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Capture -Key"))
                            {
                                captureMode = CaptureAxisNegativeKey;
                                captureTarget = axisName;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Capture Axis"))
                            {
                                captureMode = CaptureAxisGamepadAxis;
                                captureTarget = axisName;
                            }

                            if (captureTarget == axisName)
                            {
                                if (captureMode == CaptureAxisPositiveKey && capturedKey >= 0)
                                {
                                    positiveKey = capturedKey;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                                else if (captureMode == CaptureAxisNegativeKey && capturedKey >= 0)
                                {
                                    negativeKey = capturedKey;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                                else if (captureMode == CaptureAxisGamepadAxis && capturedGamepadAxis >= 0)
                                {
                                    gamepadAxis = capturedGamepadAxis;
                                    captureMode = CaptureNone;
                                    captureTarget.clear();
                                }
                            }

                            ImGui::Checkbox("Invert", &invert);
                            ImGui::SliderFloat("Per-Axis Deadzone", &axisDeadzone, -1.0f, 0.5f, "%.2f");
                            ImGui::SliderFloat("Sensitivity", &axisSensitivity, 0.01f, 3.0f, "%.2f");

                            MyEngine::InputActions::AxisBinding updated;
                            if (positiveKey >= 0 || negativeKey >= 0)
                                updated.keyPairs.emplace_back(positiveKey, negativeKey);
                            if (gamepadAxis >= 0)
                                updated.gamepadAxes.push_back(gamepadAxis);
                            updated.invert = invert;
                            updated.deadzone = axisDeadzone;
                            updated.sensitivity = axisSensitivity;
                            MyEngine::InputActions::BindAxis(axisName, updated);

                            ImGui::Text("Value: %.2f", MyEngine::InputActions::GetAxis(axisName));
                            ImGui::TreePop();
                        }
                    }
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

                unsigned int shadowSize = renderSystem.GetShadowSize();
                int shadowSizeInt = static_cast<int>(shadowSize);
                if (ImGui::SliderInt("Directional Shadow Size", &shadowSizeInt, 256, 4096))
                {
                    unsigned int snapped = static_cast<unsigned int>(shadowSizeInt);
                    if (snapped <= 384) snapped = 256;
                    else if (snapped <= 768) snapped = 512;
                    else if (snapped <= 1536) snapped = 1024;
                    else if (snapped <= 3072) snapped = 2048;
                    else snapped = 4096;
                    renderSystem.SetShadowSize(snapped);
                }

                float shadowBias = renderSystem.GetShadowBias();
                if (ImGui::SliderFloat("Directional Shadow Bias", &shadowBias, 0.0001f, 0.02f, "%.4f"))
                    renderSystem.SetShadowBias(shadowBias);

                int numCascades = renderSystem.GetNumCascades();
                if (ImGui::SliderInt("Cascades", &numCascades, 1, 4))
                    renderSystem.SetNumCascades(numCascades);

                float splitLambda = renderSystem.GetSplitLambda();
                if (ImGui::SliderFloat("Cascade Split Lambda", &splitLambda, 0.0f, 1.0f, "%.2f"))
                    renderSystem.SetSplitLambda(splitLambda);

                bool shadowStabilization = renderSystem.GetShadowStabilizationEnabled();
                if (ImGui::Checkbox("Shadow Stabilization (Texel Snap)", &shadowStabilization))
                    renderSystem.SetShadowStabilizationEnabled(shadowStabilization);

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

                bool spotShadowsEnabled = renderSystem.GetSpotShadowsEnabled();
                if (ImGui::Checkbox("Spot Light Shadows", &spotShadowsEnabled))
                    renderSystem.SetSpotShadowsEnabled(spotShadowsEnabled);

                unsigned int spotShadowSize = renderSystem.GetSpotShadowSize();
                int spotShadowSizeInt = static_cast<int>(spotShadowSize);
                if (ImGui::SliderInt("Spot Shadow Size", &spotShadowSizeInt, 256, 2048))
                {
                    unsigned int snapped = static_cast<unsigned int>(spotShadowSizeInt);
                    if (snapped <= 384) snapped = 256;
                    else if (snapped <= 768) snapped = 512;
                    else if (snapped <= 1536) snapped = 1024;
                    else snapped = 2048;
                    renderSystem.SetSpotShadowSize(snapped);
                }

                ImGui::Separator();
                ImGui::Text("Shadow Quality");
                if (ImGui::Button("Low"))
                {
                    renderSystem.SetPointShadowPCFSamples(8);
                    renderSystem.SetPointShadowPCFRadius(0.015f);
                    renderSystem.SetSpotShadowPCFRadius(0.75f);
                    renderSystem.ApplyShadowAutoBudget();
                }
                ImGui::SameLine();
                if (ImGui::Button("Medium"))
                {
                    renderSystem.SetPointShadowPCFSamples(14);
                    renderSystem.SetPointShadowPCFRadius(0.02f);
                    renderSystem.SetSpotShadowPCFRadius(1.0f);
                    renderSystem.ApplyShadowAutoBudget();
                }
                ImGui::SameLine();
                if (ImGui::Button("High"))
                {
                    renderSystem.SetPointShadowPCFSamples(20);
                    renderSystem.SetPointShadowPCFRadius(0.03f);
                    renderSystem.SetSpotShadowPCFRadius(1.35f);
                    renderSystem.ApplyShadowAutoBudget();
                }

                int pointPCFSamples = renderSystem.GetPointShadowPCFSamples();
                if (ImGui::SliderInt("Point PCF Samples", &pointPCFSamples, 1, 20))
                    renderSystem.SetPointShadowPCFSamples(pointPCFSamples);

                float pointPCFRadius = renderSystem.GetPointShadowPCFRadius();
                if (ImGui::SliderFloat("Point PCF Radius", &pointPCFRadius, 0.001f, 0.08f, "%.3f"))
                    renderSystem.SetPointShadowPCFRadius(pointPCFRadius);

                float spotPCFRadius = renderSystem.GetSpotShadowPCFRadius();
                if (ImGui::SliderFloat("Spot PCF Radius", &spotPCFRadius, 0.1f, 4.0f, "%.2f"))
                    renderSystem.SetSpotShadowPCFRadius(spotPCFRadius);

                if (ImGui::Button("Apply Shadow Auto-Budget"))
                    renderSystem.ApplyShadowAutoBudget();
                ImGui::TextDisabled("Auto-budget prioritizes nearest shadowed lights and limits heavy PCF cost.");

                if (ImGui::TreeNode("Shadow Debug View"))
                {
                    const float previewSize = 96.0f;
                    ImVec2 previewDim(previewSize, previewSize);

                    int directionalCasters = 0;
                    int pointCasters = 0;
                    int spotCasters = 0;
                    for (const auto& e : scene.GetEntities())
                    {
                        if (!e || !e->HasComponent<LightComponent>())
                            continue;
                        const auto& l = e->GetComponent<LightComponent>();
                        if (!l.castShadows)
                            continue;
                        if (l.type == LightComponent::Type::Directional) ++directionalCasters;
                        if (l.type == LightComponent::Type::Point) ++pointCasters;
                        if (l.type == LightComponent::Type::Spot) ++spotCasters;
                    }

                    int activeCascadeMaps = 0;
                    for (int i = 0; i < renderSystem.GetNumCascades(); ++i)
                        if (renderSystem.GetCascadeTexture(i) != 0) ++activeCascadeMaps;

                    int activePointMaps = 0;
                    for (int i = 0; i < 4; ++i)
                        if (renderSystem.GetPointShadowTexture(i) != 0) ++activePointMaps;

                    int activeSpotMaps = 0;
                    for (int i = 0; i < 4; ++i)
                        if (renderSystem.GetSpotShadowTexture(i) != 0) ++activeSpotMaps;

                    ImGui::Text("Directional Casters: %d | Active Maps: %d", directionalCasters, activeCascadeMaps);
                    ImGui::Text("Point Casters: %d | Active Maps: %d", pointCasters, activePointMaps);
                    ImGui::Text("Spot Casters: %d | Active Maps: %d", spotCasters, activeSpotMaps);
                    ImGui::Separator();

                    ImGui::Text("Directional Cascades");
                    for (int cascade = 0; cascade < renderSystem.GetNumCascades(); ++cascade)
                    {
                        ImGui::PushID(cascade);
                        unsigned int tex = renderSystem.GetCascadeTexture(cascade);
                        ImGui::Text("Cascade %d", cascade);
                        if (tex != 0)
                            ImGui::Image((ImTextureID)(intptr_t)tex, previewDim, ImVec2(0, 1), ImVec2(1, 0));
                        else
                            ImGui::TextDisabled("Unavailable");
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    ImGui::Text("Point Shadow Cubemaps");
                    for (int i = 0; i < 4; ++i)
                    {
                        ImGui::PushID(i);
                        unsigned int tex = renderSystem.GetPointShadowTexture(i);
                        ImGui::Text("Point Light %d", i);
                        if (tex != 0)
                            ImGui::Image((ImTextureID)(intptr_t)tex, previewDim, ImVec2(0, 1), ImVec2(1, 0));
                        else
                            ImGui::TextDisabled("Unavailable");
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    ImGui::Text("Spot Shadow Maps");
                    for (int i = 0; i < 4; ++i)
                    {
                        ImGui::PushID(i + 100);
                        unsigned int tex = renderSystem.GetSpotShadowTexture(i);
                        ImGui::Text("Spot Light %d", i);
                        if (tex != 0)
                            ImGui::Image((ImTextureID)(intptr_t)tex, previewDim, ImVec2(0, 1), ImVec2(1, 0));
                        else
                            ImGui::TextDisabled("Unavailable");
                        ImGui::PopID();
                    }

                    ImGui::TreePop();
                }

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
            // Layer Manager Panel
            // ============================================================
            if (showLayerManager)
            {
                ImGui::SetNextWindowSize(ImVec2(320, 420), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Layer Manager", &showLayerManager))
                {
                    ImGui::TextDisabled("Rename layers used to filter entities.");
                    ImGui::Separator();
                    for (int li = 0; li < MyEngine::MAX_LAYERS; ++li)
                    {
                        ImGui::PushID(li);
                        char buf[64] = "";
                        strncpy_s(buf, MyEngine::LayerMask::GetName(li).c_str(), sizeof(buf) - 1);
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
                        if (ImGui::InputText("##layerName", buf, sizeof(buf),
                                ImGuiInputTextFlags_EnterReturnsTrue))
                            MyEngine::LayerMask::SetName(li, buf);
                        ImGui::SameLine();
                        ImGui::TextDisabled("[%d]", li);
                        ImGui::PopID();
                    }
                    ImGui::Separator();
                    if (ImGui::Button("Reset to Defaults"))
                        MyEngine::LayerMask::Reset();

                    // Collision Layer Matrix
                    ImGui::Spacing();
                    if (ImGui::CollapsingHeader("Collision Layer Matrix"))
                    {
                        ImGui::TextDisabled("Check a cell to allow those two layers to collide.");
                        ImGui::Spacing();

                        // Count how many layers have non-empty names to keep the grid compact.
                        // We show only the first N named layers (max 16 for readability).
                        const int displayLayers = std::min(MyEngine::CollisionMatrix::NUM_LAYERS, 16);

                        // Column headers (short names / indices)
                        ImGui::Indent(80.0f);
                        for (int col = 0; col < displayLayers; ++col)
                        {
                            ImGui::PushID(col);
                            const std::string& name = MyEngine::LayerMask::GetName(col);
                            std::string label = name.empty() ? std::to_string(col) : name.substr(0, 3);
                            ImGui::TextDisabled("%s", label.c_str());
                            if (col < displayLayers - 1) ImGui::SameLine(0.0f, 4.0f);
                            ImGui::PopID();
                        }
                        ImGui::Unindent(80.0f);

                        for (int row = 0; row < displayLayers; ++row)
                        {
                            ImGui::PushID(row);
                            const std::string& rowName = MyEngine::LayerMask::GetName(row);
                            std::string rowLabel = rowName.empty() ? ("L" + std::to_string(row)) : rowName;
                            ImGui::Text("%-8s", rowLabel.substr(0, 8).c_str());
                            for (int col = row; col < displayLayers; ++col)
                            {
                                ImGui::SameLine(80.0f + col * 22.0f);
                                ImGui::PushID(col);
                                bool canCollide = MyEngine::CollisionMatrix::CanCollide(row, col);
                                if (ImGui::Checkbox("##cm", &canCollide))
                                    MyEngine::CollisionMatrix::SetCollision(row, col, canCollide);
                                ImGui::SetItemTooltip("%s <-> %s", rowLabel.c_str(),
                                    MyEngine::LayerMask::GetName(col).c_str());
                                ImGui::PopID();
                            }
                            ImGui::PopID();
                        }

                        ImGui::Spacing();
                        if (ImGui::Button("Reset Matrix"))
                            MyEngine::CollisionMatrix::Reset();
                        ImGui::SetItemTooltip("Re-enable all layer pairs");
                    }
                }
                ImGui::End();
            }

            // IBL Panel
            // ============================================================
            if (showIBLPanel)
            {
                ImGui::SetNextWindowSize(ImVec2(280, 160), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("IBL##iblWindow", &showIBLPanel))
                {
                    bool iblOn = renderSystem.GetIBLEnabled();
                    if (ImGui::Checkbox("Enable IBL##ibl", &iblOn))
                        renderSystem.SetIBLEnabled(iblOn);

                    float iblIntensity = renderSystem.GetIBLIntensity();
                    if (ImGui::SliderFloat("IBL Intensity##ibl", &iblIntensity, 0.0f, 4.0f))
                        renderSystem.SetIBLIntensity(iblIntensity);

                    ImGui::Separator();
                    if (ImGui::Button("Bake from Skybox##iblBake"))
                    {
                        if (skybox.IsLoaded())
                            renderSystem.InitIBL(skybox.GetCubemapTexture());
                    }
                    ImGui::SetItemTooltip("Convolves the active skybox cubemap to generate irradiance/prefilter/BRDF maps");
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

                ImGui::TextDisabled("Tune full-screen post effects and ambient occlusion.");
                InspectorGroupLabel("General");
                ImGui::Checkbox("Enabled", &postProcessEnabled);

                InspectorGroupLabel("Bloom & Exposure");
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

                InspectorGroupLabel("SSAO");

                bool ssaoEnabled = renderSystem.GetSSAOEnabled();
                if (ImGui::Checkbox("SSAO Enabled", &ssaoEnabled))
                    renderSystem.SetSSAOEnabled(ssaoEnabled);

                float ssaoRadius = renderSystem.GetSSAORadius();
                if (ImGui::SliderFloat("SSAO Radius", &ssaoRadius, 0.05f, 2.0f))
                    renderSystem.SetSSAORadius(ssaoRadius);

                float ssaoBias = renderSystem.GetSSAOBias();
                if (ImGui::SliderFloat("SSAO Bias", &ssaoBias, 0.001f, 0.1f, "%.4f"))
                    renderSystem.SetSSAOBias(ssaoBias);

                float ssaoPower = renderSystem.GetSSAOPower();
                if (ImGui::SliderFloat("SSAO Power", &ssaoPower, 0.5f, 4.0f))
                    renderSystem.SetSSAOPower(ssaoPower);

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

                InspectorGroupLabel("General");
                ImGui::Checkbox("Enabled", &skyboxEnabled);
                ImGui::TextWrapped(skybox.IsLoaded()
                    ? "Cubemap loaded."
                    : "No cubemap loaded - assign all 6 face images below.");

                if (InspectorActionButton("Use assets/skybox defaults##skybox"))
                {
                    skyboxFacePaths[0] = "assets/skybox/right.png";
                    skyboxFacePaths[1] = "assets/skybox/left.png";
                    skyboxFacePaths[2] = "assets/skybox/top.png";
                    skyboxFacePaths[3] = "assets/skybox/bottom.png";
                    skyboxFacePaths[4] = "assets/skybox/front.png";
                    skyboxFacePaths[5] = "assets/skybox/back.png";
                }
                if (InspectorActionButton("Clear Faces##skybox"))
                {
                    for (auto& p : skyboxFacePaths)
                        p.clear();
                }

                InspectorGroupLabel("Faces");
                static const char* faceLabels[6] = { "Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)" };
                bool anyChanged = false;
                for (int i = 0; i < 6; ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text("%s", faceLabels[i]);
                    std::string displayPath = skyboxFacePaths[i].empty() ? "(none)" : skyboxFacePaths[i];
                    ImGui::TextWrapped("%s", displayPath.c_str());
                    if (InspectorActionButton("Browse...##skyface"))
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
                if (InspectorActionButton("Load Skybox##skybox") || (anyChanged && allAssigned))
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
            MyEngine::Editor::Panels::DrawScriptingPanel(editorContext);

            // ============================================================
            // Performance Panel
            // ============================================================
            if (showPerformancePanel)
            {
                ImGui::SetNextWindowPos(ImVec2(10, 440), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
                ImGui::Begin("Performance", &showPerformancePanel);

                float fps = 1.0f / std::max(0.0001f, deltaTime);
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("CPU Frame: %.3f ms", deltaTime * 1000.0f);
                ImGui::Text("GPU Frame: %.3f ms", gpuFrameMs);
                ImGui::Text("CPU Physics: %.3f ms", cpuPhysicsMs);
                ImGui::Text("CPU Animation+Particles: %.3f ms", cpuAnimationMs);
                ImGui::Text("CPU Render: %.3f ms", cpuRenderMs);
#ifdef _WIN32
                ImGui::Text("Memory Working Set: %.1f MB", memoryWorkingSetMB);
                ImGui::Text("Memory Private: %.1f MB", memoryPrivateMB);
#endif

                ImGui::PlotLines("Frame (ms)", frameMsHistory.data(), static_cast<int>(frameMsHistory.size()), perfHistoryIndex, nullptr, 0.0f, 50.0f, ImVec2(0, 70));
                ImGui::PlotLines("Physics (ms)", physicsMsHistory.data(), static_cast<int>(physicsMsHistory.size()), perfHistoryIndex, nullptr, 0.0f, 20.0f, ImVec2(0, 50));
                ImGui::PlotLines("Animation (ms)", animationMsHistory.data(), static_cast<int>(animationMsHistory.size()), perfHistoryIndex, nullptr, 0.0f, 20.0f, ImVec2(0, 50));
                ImGui::PlotLines("Render (ms)", renderMsHistory.data(), static_cast<int>(renderMsHistory.size()), perfHistoryIndex, nullptr, 0.0f, 30.0f, ImVec2(0, 50));
                if (ImGui::Button("Export CSV##perfExport"))
                {
                    if (!exportPerformanceCsv())
                        std::cerr << "Failed to export performance_metrics.csv" << std::endl;
                }

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
        terrainSystem.Render(scene, view, projection, glm::vec3(glm::inverse(view)[3]), litShader);

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

        glEndQuery(GL_TIME_ELAPSED);
        gpuQueryWriteIndex = (gpuQueryWriteIndex + 1) % 2;
        cpuRenderMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - cpuRenderStart).count();

#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX memoryCounters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters)))
        {
            memoryWorkingSetMB = static_cast<float>(memoryCounters.WorkingSetSize / (1024.0 * 1024.0));
            memoryPrivateMB = static_cast<float>(memoryCounters.PrivateUsage / (1024.0 * 1024.0));
        }
#endif

        frameMsHistory[perfHistoryIndex] = deltaTime * 1000.0f;
        physicsMsHistory[perfHistoryIndex] = cpuPhysicsMs;
        animationMsHistory[perfHistoryIndex] = cpuAnimationMs;
        renderMsHistory[perfHistoryIndex] = cpuRenderMs;
        perfHistoryIndex = (perfHistoryIndex + 1) % kPerfHistory;

#ifdef USE_IMGUI
        if (selectedEntity && selectedEntity->HasComponent<TransformComponent>())
        {
            int windowW = 0, windowH = 0;
            glfwGetWindowSize(window, &windowW, &windowH);
            ImDrawList* overlayDrawList = ImGui::GetForegroundDrawList();
            glm::mat4 overlayWorld = TransformHierarchy::GetWorldMatrix(scene, *selectedEntity);
            glm::vec3 overlayScale = ExtractWorldScaleFromMatrix(overlayWorld);
            glm::vec3 overlayRight = glm::normalize(glm::vec3(overlayWorld[0]));
            if (glm::length(overlayRight) < 0.0001f)
                overlayRight = glm::vec3(1.0f, 0.0f, 0.0f);

            auto drawLabelBox = [&](const ImVec2& anchor, const char* text, ImU32 color)
            {
                ImVec2 textSize = ImGui::CalcTextSize(text);
                ImVec2 padding(8.0f, 4.0f);
                ImVec2 boxMin(anchor.x - padding.x, anchor.y - textSize.y - padding.y * 0.5f);
                ImVec2 boxMax(anchor.x + textSize.x + padding.x, anchor.y + padding.y * 0.5f);
                overlayDrawList->AddRectFilled(boxMin, boxMax, IM_COL32(15, 18, 24, 210), 6.0f);
                overlayDrawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 35), 6.0f, 0, 1.0f);
                overlayDrawList->AddText(ImVec2(anchor.x, anchor.y - textSize.y), color, text);
            };

            auto drawCapsuleOverlay = [&](const glm::vec3& localA, const glm::vec3& localB, float localRadius, ImU32 color, const char* label)
            {
                glm::vec3 worldA = TransformPointByMatrix(overlayWorld, localA);
                glm::vec3 worldB = TransformPointByMatrix(overlayWorld, localB);
                float worldRadius = localRadius * std::max(overlayScale.x, overlayScale.z);
                glm::vec3 radiusOffset = overlayRight * worldRadius;

                ImVec2 screenA, screenB, screenARadius, screenBRadius;
                if (!ProjectWorldPointToScreen(worldA, view, projection, windowW, windowH, screenA) ||
                    !ProjectWorldPointToScreen(worldB, view, projection, windowW, windowH, screenB) ||
                    !ProjectWorldPointToScreen(worldA + radiusOffset, view, projection, windowW, windowH, screenARadius) ||
                    !ProjectWorldPointToScreen(worldB + radiusOffset, view, projection, windowW, windowH, screenBRadius))
                {
                    return;
                }

                float radiusA = std::max(4.0f, std::sqrt((screenARadius.x - screenA.x) * (screenARadius.x - screenA.x) + (screenARadius.y - screenA.y) * (screenARadius.y - screenA.y)));
                float radiusB = std::max(4.0f, std::sqrt((screenBRadius.x - screenB.x) * (screenBRadius.x - screenB.x) + (screenBRadius.y - screenB.y) * (screenBRadius.y - screenB.y)));
                overlayDrawList->AddLine(screenA, screenB, color, 2.5f);
                overlayDrawList->AddCircle(screenA, radiusA, color, 24, 2.0f);
                overlayDrawList->AddCircle(screenB, radiusB, color, 24, 2.0f);
                ImVec2 labelPos((screenA.x + screenB.x) * 0.5f + 10.0f, (screenA.y + screenB.y) * 0.5f - 6.0f);
                drawLabelBox(labelPos, label, color);
            };

            if (selectedEntity->HasComponent<CapsuleColliderComponent>())
            {
                const auto& capsule = selectedEntity->GetComponent<CapsuleColliderComponent>();
                drawCapsuleOverlay(capsule.pointA, capsule.pointB, capsule.radius, IM_COL32(80, 200, 255, 255), "Collider Capsule");
            }

            if (selectedEntity->HasComponent<SkeletonComponent>())
            {
                glm::vec3 fitA(0.0f), fitB(0.0f);
                float fitRadius = 0.0f;
                auto skeleton = selectedEntity->GetComponent<SkeletonComponent>().skeleton;
                if (MyEngine::AssetManager::ComputeCharacterCapsuleFromSkeleton(skeleton, fitA, fitB, fitRadius))
                    drawCapsuleOverlay(fitA, fitB, fitRadius, IM_COL32(80, 255, 120, 255), "Auto-Fit Capsule");
            }

            if (selectedEntity->HasComponent<AnimationStateMachineComponent>())
            {
                const auto& sm = selectedEntity->GetComponent<AnimationStateMachineComponent>();
                std::string stateText;
                std::string blockedReason;
                ImU32 textColor = IM_COL32(255, 255, 255, 255);
                if (!sm.debugPendingStateName.empty())
                {
                    stateText = sm.debugCurrentStateName.empty()
                        ? ("Transition -> " + sm.debugPendingStateName)
                        : (sm.debugCurrentStateName + " -> " + sm.debugPendingStateName);
                    textColor = IM_COL32(120, 255, 160, 255);
                }
                else if (!sm.debugCurrentStateName.empty())
                {
                    stateText = "State: " + sm.debugCurrentStateName;
                }

                if (!sm.debugLastBlockedReason.empty())
                    blockedReason = sm.debugLastBlockedReason;

                if (!stateText.empty())
                {
                    float textHeightOffset = 1.5f;
                    if (selectedEntity->HasComponent<CapsuleColliderComponent>())
                    {
                        const auto& capsule = selectedEntity->GetComponent<CapsuleColliderComponent>();
                        textHeightOffset = std::max(capsule.pointA.y, capsule.pointB.y) + capsule.radius + 0.2f;
                    }
                    glm::vec3 textWorld = TransformPointByMatrix(overlayWorld, glm::vec3(0.0f, textHeightOffset, 0.0f));
                    ImVec2 textScreen;
                    if (ProjectWorldPointToScreen(textWorld, view, projection, windowW, windowH, textScreen))
                    {
                        overlayDrawList->AddCircleFilled(textScreen, 5.0f, textColor);
                        overlayDrawList->AddLine(textScreen, ImVec2(textScreen.x + 10.0f, textScreen.y - 2.0f), textColor, 2.0f);

                        ImVec2 labelAnchor(textScreen.x + 14.0f, textScreen.y - 10.0f);
                        drawLabelBox(labelAnchor, stateText.c_str(), textColor);
                        if (!blockedReason.empty())
                        {
                            ImVec2 blockedAnchor(textScreen.x + 14.0f, textScreen.y + 10.0f);
                            drawLabelBox(blockedAnchor, blockedReason.c_str(), IM_COL32(255, 210, 90, 255));
                        }
                    }
                }
            }
        }

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

    if (gpuFrameQueries[0] != 0 || gpuFrameQueries[1] != 0)
        glDeleteQueries(2, gpuFrameQueries);

    Input::Shutdown();

    audioSystem.ReleaseAll(scene);
    AudioEngine::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}