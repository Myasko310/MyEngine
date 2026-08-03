#include "systems/MeshRendererSystem.h"

#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/TransformComponent.h"
#include "components/LightComponent.h"
#include "ecs/Scene.h"

#include <algorithm>

void MeshRendererSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
{
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

        meshComponent.mesh->Draw();
    }
}