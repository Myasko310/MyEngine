#include "CameraSystem.h"

#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "core/Input.h"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace MyEngine
{
    static glm::vec3 GetForward(float yaw, float pitch)
    {
        glm::vec3 forward;

        forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward.y = sin(glm::radians(pitch));
        forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        return glm::normalize(forward);
    }

    static glm::vec3 GetRight(const glm::vec3& forward)
    {
        return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void CameraSystem::Update(Scene& scene, GLFWwindow* window, float deltaTime, float aspectRatio)
    {
        for (auto& entity : scene.GetEntities())
        {
            if (!entity->HasComponent<CameraComponent>())
                continue;

            if (!entity->HasComponent<TransformComponent>())
                continue;

            auto& camera = entity->GetComponent<CameraComponent>();

            if (!camera.isPrimary)
                continue;

            auto& transform = entity->GetComponent<TransformComponent>();

            // Resolve the follow target (if any) up front so both the movement
            // and mouse-look sections below can consult it - third-person mode
            // orbits around this entity's position instead of the camera's own.
            std::shared_ptr<Entity> followTarget;
            if (camera.thirdPerson)
                followTarget = TransformHierarchy::FindEntityByID(scene, camera.followTargetID);
            bool hasFollowTarget = followTarget && followTarget->HasComponent<TransformComponent>();

            if (camera.enableInput)
            {
#ifdef USE_IMGUI
                // Skip keyboard input if ImGui wants to capture it (e.g., typing in a text field)
                ImGuiIO& io = ImGui::GetIO();
                bool allowKeyboardInput = !io.WantCaptureKeyboard;
#else
                bool allowKeyboardInput = true;
#endif

                float speed = camera.moveSpeed;

                // Movement is only active while the mouse is captured (fly mode).
                // When the mouse is released (e.g. to interact with the editor UI
                // or transform gizmos), WASD/E/Q must not move the camera, since
                // those keys are also used as gizmo mode shortcuts (W/E) and would
                // otherwise fight with editor controls and prevent gizmo dragging.
                bool cameraActive = Input::IsMouseCaptured();

                if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
                    speed *= camera.sprintMultiplier;

                if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL))
                    speed *= camera.slowMultiplier;

                glm::vec3 forward = GetForward(camera.yaw, camera.pitch);
                glm::vec3 right = GetRight(forward);
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

                if (!camera.flyMode)
                {
                    forward.y = 0.0f;
                    forward = glm::normalize(forward);
                }

                // Free-fly WASD/E/Q movement only applies when not following a
                // target - third-person mode locks the camera to an orbit around
                // the followed entity instead of letting it move independently.
                if (!hasFollowTarget)
                {
                    glm::vec3 movement = glm::vec3(0.0f);

                    if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_W))
                        movement += forward;

                    if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_S))
                        movement -= forward;

                    if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_D))
                        movement += right;

                    if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_A))
                        movement -= right;

                    if (camera.flyMode)
                    {
                        // Note: SPACE is intentionally not used here because it is the
                        // global Play/Pause shortcut; using it for fly-up would toggle
                        // simulation state while flying. E/Q are dedicated to vertical fly movement.
                        if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_E))
                            movement += up;

                        if (allowKeyboardInput && cameraActive && Input::IsKeyDown(GLFW_KEY_Q))
                            movement -= up;
                    }

                    if (glm::length(movement) > 0.0f)
                        movement = glm::normalize(movement);

                    transform.position += movement * speed * deltaTime;
                }

                // Only process mouse look if mouse is captured (camera control mode)
                if (camera.enableMouseLook && Input::IsMouseCaptured())
                {
                    // Use Input's mouse delta (already accumulated per-frame)
                    float deltaX = Input::GetMouseDeltaX();
                    float deltaY = Input::GetMouseDeltaY();

                    glm::vec2 rawMouseDelta(deltaX, deltaY);

                    float smoothingFactor = 1.0f - std::exp(-camera.mouseSmoothing * deltaTime);

                    camera.smoothedMouseDelta = glm::mix(
                        camera.smoothedMouseDelta,
                        rawMouseDelta,
                        smoothingFactor
                    );

                    camera.yaw += camera.smoothedMouseDelta.x * camera.mouseSensitivity;
                    camera.pitch += camera.smoothedMouseDelta.y * camera.mouseSensitivity;

                    camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
                }
                else if (!Input::IsMouseCaptured())
                {
                    // When mouse is not captured (UI mode), reset smoothed delta
                    // to avoid jumps when returning to camera mode
                    camera.smoothedMouseDelta = glm::vec2(0.0f);
                }
            }

            glm::vec3 forward = GetForward(camera.yaw, camera.pitch);
            glm::vec3 lookTarget = transform.position + forward;

            if (hasFollowTarget)
            {
                // Orbit around the target's position using yaw/pitch (driven by
                // mouse look above) at a fixed distance/height, then look back
                // at the target so it stays framed regardless of orbit angle.
                const auto& targetTransform = followTarget->GetComponent<TransformComponent>();
                glm::vec3 pivot = targetTransform.position + glm::vec3(0.0f, camera.followHeight, 0.0f);

                transform.position = pivot - forward * camera.followDistance;
                lookTarget = pivot;
            }

            m_ViewMatrix = glm::lookAt(
                transform.position,
                lookTarget,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            m_ProjectionMatrix = glm::perspective(
                glm::radians(camera.fov),
                aspectRatio,
                camera.nearPlane,
                camera.farPlane
            );

            return;
        }
    }

    const glm::mat4& CameraSystem::GetViewMatrix() const
    {
        return m_ViewMatrix;
    }

    const glm::mat4& CameraSystem::GetProjectionMatrix() const
    {
        return m_ProjectionMatrix;
    }
}