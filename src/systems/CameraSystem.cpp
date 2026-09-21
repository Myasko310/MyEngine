#include "CameraSystem.h"

#include "components/CameraComponent.h"
#include "components/TransformComponent.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/TransformHierarchy.h"
#include "core/Input.h"
#include "core/InputActions.h"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <string>

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

    static std::shared_ptr<Entity> FindBestLockOnTarget(
        Scene& scene,
        const std::shared_ptr<Entity>& followTarget,
        uint32_t existingTargetID,
        const glm::vec3& viewForward,
        float maxDistance,
        float maxAngleDegrees)
    {
        if (!followTarget || !followTarget->HasComponent<TransformComponent>())
            return nullptr;

        const glm::vec3 origin = followTarget->GetComponent<TransformComponent>().position;
        const glm::vec3 flatForward = glm::normalize(glm::vec3(viewForward.x, 0.0f, viewForward.z));
        const float maxDistanceSq = maxDistance * maxDistance;
        const float minDot = std::cos(glm::radians(maxAngleDegrees));

        std::shared_ptr<Entity> best;
        float bestScore = -1.0f;

        for (const auto& candidate : scene.GetEntities())
        {
            if (!candidate || candidate.get() == followTarget.get() || !candidate->HasComponent<TransformComponent>())
                continue;

            const bool isBossLike = (candidate->GetTag() == "Boss") || (candidate->GetName().find("Boss") != std::string::npos);
            if (!isBossLike)
                continue;

            const glm::vec3 toCandidate = candidate->GetComponent<TransformComponent>().position - origin;
            const glm::vec3 flatToCandidate = glm::vec3(toCandidate.x, 0.0f, toCandidate.z);
            const float distSq = glm::dot(flatToCandidate, flatToCandidate);
            if (distSq > maxDistanceSq || distSq < 0.0001f)
                continue;

            const glm::vec3 dir = glm::normalize(flatToCandidate);
            const float dotValue = glm::dot(flatForward, dir);
            if (dotValue < minDot)
                continue;

            float score = dotValue * 2.0f + (1.0f - (distSq / maxDistanceSq));
            if (candidate->GetID() == existingTargetID)
                score += 0.15f;

            if (!best || score > bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }

        return best;
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

            // Save previous frame camera state for interpolation
            m_PreviousCameraState.position = transform.position;
            m_PreviousCameraState.yaw = camera.yaw;
            m_PreviousCameraState.pitch = camera.pitch;

            // Resolve the follow target (if any) up front so both the movement
            // and mouse-look sections below can consult it - third-person mode
            // orbits around this entity's position instead of the camera's own.
            std::shared_ptr<Entity> followTarget;
            if (camera.thirdPerson)
                followTarget = TransformHierarchy::FindEntityByID(scene, camera.followTargetID);
            bool hasFollowTarget = followTarget && followTarget->HasComponent<TransformComponent>();

            // Auto-recover follow target if third-person is enabled but the saved
            // target ID is stale/missing (e.g. after scene reload).
            if (camera.thirdPerson && !hasFollowTarget)
            {
                for (const auto& candidate : scene.GetEntities())
                {
                    if (!candidate || !candidate->HasComponent<TransformComponent>())
                        continue;
                    if (candidate->GetName() != "Player")
                        continue;

                    camera.followTargetID = candidate->GetID();
                    followTarget = candidate;
                    hasFollowTarget = true;
                    break;
                }
            }

            std::shared_ptr<Entity> lockOnTarget;
            if (camera.thirdPerson && camera.lockOnEnabled && hasFollowTarget)
            {
                if (camera.lockOnTargetID != 0)
                {
                    auto candidate = TransformHierarchy::FindEntityByID(scene, camera.lockOnTargetID);
                    if (candidate && candidate->HasComponent<TransformComponent>() && candidate.get() != followTarget.get())
                    {
                        const glm::vec3 origin = followTarget->GetComponent<TransformComponent>().position;
                        const glm::vec3 toTarget = candidate->GetComponent<TransformComponent>().position - origin;
                        const glm::vec3 flatToTarget(toTarget.x, 0.0f, toTarget.z);
                        const float distance = glm::length(flatToTarget);

                        glm::vec3 viewForward = GetForward(camera.yaw, 0.0f);
                        glm::vec3 flatForward(viewForward.x, 0.0f, viewForward.z);
                        if (glm::length(flatForward) > 0.0001f)
                            flatForward = glm::normalize(flatForward);
                        else
                            flatForward = glm::vec3(0.0f, 0.0f, -1.0f);

                        const glm::vec3 dir = distance > 0.0001f ? (flatToTarget / distance) : glm::vec3(0.0f, 0.0f, -1.0f);
                        const float dotValue = glm::dot(flatForward, dir);
                        const float minDot = std::cos(glm::radians(camera.lockOnMaxAngleDegrees));

                        if (distance <= camera.lockOnMaxDistance && dotValue >= minDot)
                            lockOnTarget = candidate;
                    }
                }

                if (!lockOnTarget)
                {
                    glm::vec3 viewForward = GetForward(camera.yaw, 0.0f);
                    lockOnTarget = FindBestLockOnTarget(
                        scene,
                        followTarget,
                        camera.lockOnTargetID,
                        viewForward,
                        camera.lockOnMaxDistance,
                        camera.lockOnMaxAngleDegrees);
                }

                camera.lockOnTargetID = lockOnTarget ? lockOnTarget->GetID() : 0;
            }
            else
            {
                camera.lockOnTargetID = 0;
            }

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

                // Right-stick orbit look for third-person follow mode.
                if (camera.thirdPerson)
                {
                    const float lookX = InputActions::GetAxis("LookX");
                    const float lookY = InputActions::GetAxis("LookY");
                    const float gamepadLookSpeed = 140.0f; // degrees/sec

                    camera.yaw += lookX * gamepadLookSpeed * deltaTime;
                    camera.pitch += lookY * gamepadLookSpeed * deltaTime;
                    camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
                }
            }

            glm::vec3 forward = GetForward(camera.yaw, camera.pitch);
            glm::vec3 lookTarget = transform.position + forward;

            if (hasFollowTarget)
            {
                const auto& targetTransform = followTarget->GetComponent<TransformComponent>();
                glm::vec3 pivot = targetTransform.position + glm::vec3(0.0f, camera.followHeight, 0.0f);

                if (camera.lockOnEnabled && lockOnTarget && lockOnTarget->HasComponent<TransformComponent>())
                {
                    const auto& lockTransform = lockOnTarget->GetComponent<TransformComponent>();
                    glm::vec3 enemyPivot = lockTransform.position + glm::vec3(0.0f, camera.lockOnHeightOffset, 0.0f);

                    glm::vec3 toEnemy = enemyPivot - pivot;
                    glm::vec3 flatToEnemy(toEnemy.x, 0.0f, toEnemy.z);
                    if (glm::length(flatToEnemy) > 0.0001f)
                    {
                        glm::vec3 enemyDir = glm::normalize(flatToEnemy);
                        float desiredYaw = glm::degrees(std::atan2(enemyDir.z, enemyDir.x));
                        const float yawBlend = std::clamp(deltaTime * 10.0f, 0.0f, 1.0f);
                        camera.yaw = glm::mix(camera.yaw, desiredYaw, yawBlend);
                        camera.pitch = glm::mix(camera.pitch, -8.0f, std::clamp(deltaTime * 6.0f, 0.0f, 1.0f));

                        glm::vec3 desiredPos = pivot - enemyDir * camera.lockOnCameraDistance;
                        desiredPos.y += camera.lockOnCameraHeight;
                        const float posBlend = std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
                        transform.position = glm::mix(transform.position, desiredPos, posBlend);

                        lookTarget = glm::mix(pivot, enemyPivot, 0.55f);
                    }
                    else
                    {
                        transform.position = pivot - forward * camera.followDistance;
                        lookTarget = pivot;
                    }
                }
                else
                {
                    // Orbit around the target's position using yaw/pitch (driven by
                    // mouse look above) at a fixed distance/height, then look back
                    // at the target so it stays framed regardless of orbit angle.
                    transform.position = pivot - forward * camera.followDistance;
                    lookTarget = pivot;
                }
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

    void CameraSystem::ApplyCameraInterpolation(Scene& scene, float alpha)
    {
        // Find the primary camera and interpolate its position and rotation
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

            // Clamp alpha to [0, 1)
            alpha = glm::clamp(alpha, 0.0f, 0.9999f);

            // Interpolate position
            glm::vec3 interpolatedPosition = glm::mix(
                m_PreviousCameraState.position,
                transform.position,
                alpha
            );

            // Interpolate yaw and pitch (handling wrap-around at 360 degrees)
            float yawDiff = camera.yaw - m_PreviousCameraState.yaw;
            if (yawDiff > 180.0f)
                yawDiff -= 360.0f;
            else if (yawDiff < -180.0f)
                yawDiff += 360.0f;

            float interpolatedYaw = m_PreviousCameraState.yaw + yawDiff * alpha;

            // Pitch doesn't wrap, so simple lerp
            float interpolatedPitch = glm::mix(
                m_PreviousCameraState.pitch,
                camera.pitch,
                alpha
            );

            // Temporarily update camera for view matrix calculation
            glm::vec3 savedPosition = transform.position;
            float savedYaw = camera.yaw;
            float savedPitch = camera.pitch;

            transform.position = interpolatedPosition;
            camera.yaw = interpolatedYaw;
            camera.pitch = interpolatedPitch;

            // Recalculate view matrix with interpolated values
            glm::vec3 forward;
            forward.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
            forward.y = sin(glm::radians(camera.pitch));
            forward.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
            forward = glm::normalize(forward);

            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));

            m_ViewMatrix = glm::lookAt(transform.position, transform.position + forward, up);

            // Restore actual camera state
            transform.position = savedPosition;
            camera.yaw = savedYaw;
            camera.pitch = savedPitch;

            return;
        }
    }
}