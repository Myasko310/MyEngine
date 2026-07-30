#pragma once

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "components/TransformComponent.h"
#include "components/CameraComponent.h"
#include "core/Input.h"

namespace MyEngine {

    class CameraSystem {
    public:
        // Movement & look sensitivity
        float moveSpeed = 5.0f;
        float lookSensitivity = 0.1f;

        // Euler angles (yaw = left/right, pitch = up/down)
        float yaw = -90.0f;
        float pitch = 0.0f;

        void Update(TransformComponent& transform,
            CameraComponent& camera,
            float              deltaTime)
        {
            // ── Mouse Look ───────────────────────────────────
            float dx = Input::GetMouseDeltaX() * lookSensitivity;
            float dy = Input::GetMouseDeltaY() * lookSensitivity;

            yaw += dx;
            pitch -= dy;  // invert Y so moving mouse up looks up

            // Clamp pitch so camera doesn't flip
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            // Recalculate front vector from yaw/pitch
            glm::vec3 front;
            front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            front.y = sin(glm::radians(pitch));
            front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            camera.front = glm::normalize(front);

            // Recalculate right and up
            camera.right = glm::normalize(
                glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f)));
            camera.up = glm::normalize(
                glm::cross(camera.right, camera.front));

            // ── Keyboard Movement ────────────────────────────
            float speed = moveSpeed * deltaTime;

            if (Input::IsKeyDown(SDLK_w))
                transform.position += camera.front * speed;
            if (Input::IsKeyDown(SDLK_s))
                transform.position -= camera.front * speed;
            if (Input::IsKeyDown(SDLK_a))
                transform.position -= camera.right * speed;
            if (Input::IsKeyDown(SDLK_d))
                transform.position += camera.right * speed;
            if (Input::IsKeyDown(SDLK_q))
                transform.position -= camera.up * speed;
            if (Input::IsKeyDown(SDLK_e))
                transform.position += camera.up * speed;

            // ── Update Cached Matrices ───────────────────────
            camera.viewMatrix = camera.GetViewMatrix(transform.position);
            camera.projectionMatrix = camera.GetProjectionMatrix();
        }
    };

} // namespace MyEngine