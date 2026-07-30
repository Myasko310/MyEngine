#pragma once

#include "../ecs/System.h"
#include "../ecs/Scene.h"
#include "../components/MeshComponent.h"
#include "../components/MeshRendererComponent.h"
#include "../components/TransformComponent.h"
#include "../components/CameraComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <cstdio>

namespace MyEngine {

    class MeshRendererSystem : public System {
    public:
        void OnStart(Scene& scene) override {
            // Upload all meshes to GPU on start
            for (auto& entity : scene.GetEntities()) {
                auto* mesh = scene.GetComponent<MeshComponent>(entity);
                if (mesh && !mesh->isUploaded) {
                    mesh->Upload();
                    printf("[MeshRendererSystem] Uploaded mesh: %s\n",
                        mesh->name.c_str());
                }
            }
        }

        void OnUpdate(Scene& scene, float deltaTime) override {
            // Find primary camera
            CameraComponent* cam = nullptr;
            TransformComponent* camTrans = nullptr;

            for (auto& entity : scene.GetEntities()) {
                auto* c = scene.GetComponent<CameraComponent>(entity);
                if (c && c->isPrimary) {
                    cam = c;
                    camTrans = scene.GetComponent<TransformComponent>(entity);
                    break;
                }
            }

            if (!cam) return;

            // Draw each entity with Mesh + MeshRenderer + Transform
            for (auto& entity : scene.GetEntities()) {
                auto* mesh = scene.GetComponent<MeshComponent>(entity);
                auto* renderer = scene.GetComponent<MeshRendererComponent>(entity);
                auto* transform = scene.GetComponent<TransformComponent>(entity);

                if (!mesh || !renderer || !transform) continue;
                if (!renderer->visible)               continue;
                if (!mesh->isUploaded)                continue;
                if (!renderer->shader)                continue;

                renderer->shader->Use();

                // Model matrix
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, transform->position);
                model = glm::rotate(model,
                    glm::radians(transform->rotation.y), { 0,1,0 });
                model = glm::rotate(model,
                    glm::radians(transform->rotation.x), { 1,0,0 });
                model = glm::rotate(model,
                    glm::radians(transform->rotation.z), { 0,0,1 });
                model = glm::scale(model, transform->scale);

                renderer->shader->SetMat4("model", model);
                renderer->shader->SetMat4("view", cam->viewMatrix);
                renderer->shader->SetMat4("projection", cam->projectionMatrix);

                glBindVertexArray(mesh->VAO);
                glDrawElements(GL_TRIANGLES,
                    (GLsizei)mesh->indices.size(),
                    GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
        }

        void OnShutdown(Scene& scene) override {
            for (auto& entity : scene.GetEntities()) {
                auto* mesh = scene.GetComponent<MeshComponent>(entity);
                if (mesh) mesh->Destroy();
            }
        }
    };

} // namespace MyEngine