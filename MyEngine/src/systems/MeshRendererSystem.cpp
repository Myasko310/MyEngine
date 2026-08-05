#include "systems/MeshRendererSystem.h"

#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/TransformComponent.h"
#include "components/LightComponent.h"
#include "ecs/Scene.h"
#include "components/BoundingSphereComponent.h"

#include <algorithm>
#include <glad/glad.h>
#include "rendering/ShadowMap.h"
#include "rendering/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include "renderer/FrustumCuller.h"

void MeshRendererSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
{
    // Static resources: depth shader and shadow map
    static std::shared_ptr<MyEngine::Shader> depthShader = nullptr;
    static MyEngine::ShadowMap shadowMap;
    static bool initialized = false;
    const unsigned int SHADOW_SIZE = 2048;

    if (!initialized)
    {
        depthShader = std::make_shared<MyEngine::Shader>("shaders/depth.vert", "shaders/depth.frag");
        shadowMap.Init(SHADOW_SIZE, SHADOW_SIZE);
        initialized = true;
    }

    // Find first directional light in the scene (if any)
    glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 ambientColor = glm::vec3(0.08f);

    for (const auto& e : scene.GetEntities())
    {
        if (!e)
            continue;

        if (!e->HasComponent<MyEngine::LightComponent>())
            continue;

        auto& L = e->GetComponent<MyEngine::LightComponent>();
        if (L.type == MyEngine::LightComponent::Type::Directional)
        {
            lightDir = L.direction;
            lightColor = L.color * L.intensity;
            break;
        }
    }

    // Compute light-space matrix for directional light
    float orthoSize = 10.0f;
    float nearPlane = -10.0f;
    float farPlane = 30.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    glm::vec3 lightPos = -lightDir * 10.0f; // position the light back along its direction
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // Update frustum from view-projection for culling
    MyEngine::FrustumCuller culler;
    glm::mat4 vp = projection * view;
    culler.Update(vp);

    // 1) Render depth of scene from light's perspective
    shadowMap.BindForWriting();
    // Cull front faces to reduce peter-panning
    glCullFace(GL_FRONT);
    depthShader->Use();
    for (const auto& entity : scene.GetEntities())
    {
        if (!entity)
            continue;

        if (!entity->HasComponent<TransformComponent>())
            continue;

        // Frustum culling: if entity has a bounding sphere, test it
        if (entity->HasComponent<BoundingSphereComponent>())
        {
            auto& bs = entity->GetComponent<BoundingSphereComponent>();
            glm::vec3 worldCenter = entity->GetComponent<TransformComponent>().position + bs.center;
            float maxScale = glm::compMax(entity->GetComponent<TransformComponent>().scale);
            float worldRadius = bs.radius * maxScale;
            if (!culler.IsSphereVisible(worldCenter, worldRadius))
                continue;
        }

        if (!entity->HasComponent<MeshComponent>())
            continue;

        if (!entity->HasComponent<MeshRendererComponent>())
            continue;

        auto& transform = entity->GetComponent<TransformComponent>();
        auto& meshComponent = entity->GetComponent<MeshComponent>();

        if (!meshComponent.mesh)
            continue;

        depthShader->SetMat4("u_LightSpace", lightSpaceMatrix);
        depthShader->SetMat4("u_Model", transform.GetMatrix());
        meshComponent.mesh->Draw();
    }

    shadowMap.Unbind();
    glCullFace(GL_BACK);




    for (const auto& entity : scene.GetEntities())
    {
        if (!entity)
            continue;

        if (!entity->HasComponent<TransformComponent>())
            continue;

        if (!entity->HasComponent<MeshComponent>())
            continue;

        if (!entity->HasComponent<MeshRendererComponent>())
            continue;

        auto& transform = entity->GetComponent<TransformComponent>();
        auto& meshComponent = entity->GetComponent<MeshComponent>();
        auto& renderer = entity->GetComponent<MeshRendererComponent>();

        if (!renderer.visible)
            continue;

        if (!meshComponent.mesh)
            continue;

        if (!renderer.shader)
            continue;

        renderer.shader->Use();

        renderer.shader->SetMat4("u_Model", transform.GetMatrix());
        renderer.shader->SetMat4("u_View", view);
        renderer.shader->SetMat4("u_Projection", projection);

        // Material uniforms
        renderer.shader->SetVec3("u_MaterialAlbedo", renderer.albedo);
        renderer.shader->SetFloat("u_MaterialShininess", renderer.shininess);

        // Light uniforms
        renderer.shader->SetVec3("u_LightDirection", lightDir.x, lightDir.y, lightDir.z);
        renderer.shader->SetVec3("u_LightColor", lightColor.x, lightColor.y, lightColor.z);
        renderer.shader->SetVec3("u_AmbientColor", ambientColor.x, ambientColor.y, ambientColor.z);

        // Shadow uniforms
        renderer.shader->SetMat4("u_LightSpace", lightSpaceMatrix);
        // bind shadow map to texture unit 1
        shadowMap.BindForReading(1);
        renderer.shader->SetInt("u_ShadowMap", 1);

        meshComponent.mesh->Draw();
    }
}