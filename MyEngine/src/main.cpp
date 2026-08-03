#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
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
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"

// Rendering
#include "rendering/Mesh.h"
#include "rendering/Shader.h"

// Systems
#include "systems/CameraSystem.h"

// If your project has a render system, include it here.
// Change this include/name if your renderer is named differently.
#include "systems/MeshRendererSystem.h"

using namespace MyEngine;

static int g_WindowWidth = 1280;
static int g_WindowHeight = 720;

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

    // ------------------------------------------------------------
    // Create simple geometry and place objects in the scene
    // ------------------------------------------------------------
    auto litShader = std::make_shared<MyEngine::Shader>(
        "shaders/lit.vert",
        "shaders/lit.frag"
    );

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

        auto& mc = planeEntity->AddComponent<MeshComponent>();
        mc.mesh = planeMesh;

        auto& mr = planeEntity->AddComponent<MeshRendererComponent>();
        mr.shader = litShader;
    }

    // Create a few cubes to interact with
    std::vector<std::shared_ptr<Entity>> cubes;
    for (int i = 0; i < 5; ++i)
    {
        auto cubeEntity = scene.CreateEntity("Cube");
        auto& t = cubeEntity->AddComponent<TransformComponent>();
        t.position = glm::vec3(static_cast<float>(i) - 2.0f, 0.5f, -2.0f - static_cast<float>(i));
        t.scale = glm::vec3(1.0f);

        auto& mc = cubeEntity->AddComponent<MeshComponent>();
        mc.mesh = cubeMesh;

        auto& mr = cubeEntity->AddComponent<MeshRendererComponent>();
        mr.shader = litShader;
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

        // Cycle selected cube
        if (Input::IsKeyPressed(GLFW_KEY_TAB))
        {
            if (!cubes.empty())
            {
                selectedCube = (selectedCube + 1) % static_cast<int>(cubes.size());
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

        renderSystem.Render(scene, view, projection);

        glfwSwapBuffers(window);
    }

    // ------------------------------------------------------------
    // Shutdown
    // ------------------------------------------------------------
    Input::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}