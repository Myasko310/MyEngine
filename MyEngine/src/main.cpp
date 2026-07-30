#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>
#include <memory>

#include "core/GameTime.h"
#include "core/Input.h"
#include "rendering/Window.h"
#include "rendering/Shader.h"
#include "rendering/MeshPrimitives.h"
#include "rendering/ModelLoader.h"
#include "components/TransformComponent.h"
#include "components/CameraComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "systems/CameraSystem.h"
#include "systems/MeshRendererSystem.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace MyEngine;

int main()
{
    // ── Window ───────────────────────────────────────────────
    Window window("MyEngine", 800, 600);
    if (!window.Init())
    {
        std::cerr << "Failed to initialize window!" << std::endl;
        return -1;
    }

    // ── Shader ───────────────────────────────────────────────
    auto shader = std::make_shared<Shader>(
        "shaders/vertex.glsl",
        "shaders/fragment.glsl"
    );

    // ── Camera ───────────────────────────────────────────────
    TransformComponent cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 0.0f, 5.0f);
    cameraTransform.rotation = glm::vec3(0.0f);
    cameraTransform.scale = glm::vec3(1.0f);

    CameraComponent cameraComp;
    cameraComp.fov = 45.0f;
    cameraComp.nearPlane = 0.1f;
    cameraComp.farPlane = 100.0f;
    cameraComp.aspectRatio = 800.0f / 600.0f;
    cameraComp.isPrimary = true;

    // ── Cube Entity ──────────────────────────────────────────
    TransformComponent cubeTransform;
    cubeTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    cubeTransform.rotation = glm::vec3(0.0f);
    cubeTransform.scale = glm::vec3(1.0f);

    MeshComponent cubeM = MeshPrimitives::CreateCube();

    MeshRendererComponent cubeR;
    cubeR.shader = shader;
    cubeR.color = glm::vec3(1.0f);
    cubeR.wireframe = false;
    cubeR.visible = true;

    // ── (Optional) Load a model from file ───────────────────
    // auto modelMeshes = ModelLoader::Load("assets/mymodel.obj");
    // for (auto& m : modelMeshes) m.Upload();

    // ── Systems ──────────────────────────────────────────────
    CameraSystem       cameraSystem;
    MeshRendererSystem meshRenderer;

    // ── Time & Input ─────────────────────────────────────────
    GameTime::Init();
    Input::Init();

    bool running = true;

    // ── Main Loop ────────────────────────────────────────────
    while (running)
    {
        GameTime::Update();
        float deltaTime = GameTime::DeltaTime();

        // Events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            Input::ProcessEvent(event);

            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }

        Input::Update();

        // Rotate cube each frame
        cubeTransform.rotation.y += 30.0f * deltaTime;
        cubeTransform.rotation.x += 15.0f * deltaTime;

        // Update camera
        cameraSystem.Update(cameraTransform, cameraComp, deltaTime);

        // Clear
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render cube
        meshRenderer.Render(cubeM, cubeR, cubeTransform, cameraComp);

        window.SwapBuffers();

        fflush(stdout);
    }

    // ── Cleanup ──────────────────────────────────────────────
    cubeM.Free();
    window.Shutdown();

    return 0;
}