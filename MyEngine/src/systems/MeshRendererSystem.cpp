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
#include "MeshRendererSystem_Impl.h"

struct MeshRendererSystem::Impl;

// Definitions for constructor/destructor
MeshRendererSystem::MeshRendererSystem()
    : m_Impl(std::make_unique<Impl>())
{
}

MeshRendererSystem::~MeshRendererSystem() = default;

void MeshRendererSystem::SetShadowsEnabled(bool enabled)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->shadowsEnabled = enabled;
}

bool MeshRendererSystem::GetShadowsEnabled() const
{
    return m_Impl ? m_Impl->shadowsEnabled : false;
}

void MeshRendererSystem::SetShadowSize(unsigned int size)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->shadowSize = size;
    if (m_Impl->shadowMap.GetDepthTexture() == 0)
        m_Impl->shadowMap.Init(size, size);
}

unsigned int MeshRendererSystem::GetShadowSize() const
{
    return m_Impl ? m_Impl->shadowSize : 0;
}

void MeshRendererSystem::SetShadowBias(float bias)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->shadowBias = bias;
}

float MeshRendererSystem::GetShadowBias() const
{
    return m_Impl ? m_Impl->shadowBias : 0.0f;
}

unsigned int MeshRendererSystem::GetShadowTexture() const
{
    return m_Impl ? m_Impl->shadowMap.GetDepthTexture() : 0;
}


void MeshRendererSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
{
    if (!m_Impl)
        m_Impl = std::make_unique<Impl>();

    // Static resources: depth shader and shadow map
    static std::shared_ptr<MyEngine::Shader> depthShader = nullptr;
    static bool initialized = false;

    if (!initialized)
    {
        depthShader = std::make_shared<MyEngine::Shader>("shaders/depth.vert", "shaders/depth.frag");
        // Initialize impl shadow map
        if (!m_Impl)
            m_Impl = std::make_unique<Impl>();
        m_Impl->shadowMap.Init(m_Impl->shadowSize, m_Impl->shadowSize);
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
    // Use a conventional orthographic near/far range (positive distances)
    float orthoSize = 10.0f;
    float nearPlane = 1.0f;
    float farPlane = 50.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    // Place the light further back along its direction to cover the scene from above
    // Ensure the light 'up' is consistent with world up to avoid flipping the
    // shadow projection when the light direction is near-up or near-down.
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (fabs(glm::dot(lightDir, up)) > 0.99f)
    {
        // If nearly parallel, pick a different up vector to avoid singularity.
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::vec3 lightPos = -lightDir * 20.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), up);
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // Prepare frustum cullers: one for the light (depth pass) and one for the
    // camera (main pass). Using the light frustum for the depth pass avoids
    // accidentally skipping objects that are visible to the light but not to
    // the camera.
    MyEngine::FrustumCuller lightCuller;
    glm::mat4 lightVP = lightProjection * lightView;
    lightCuller.Update(lightVP);

    MyEngine::FrustumCuller cameraCuller;
    glm::mat4 cameraVP = projection * view;
    cameraCuller.Update(cameraVP);

    // 1) Render depth of scene from light's perspective
    GLint lastViewport[4];
    glGetIntegerv(GL_VIEWPORT, lastViewport);
    if (m_Impl && m_Impl->shadowsEnabled)
    {
        m_Impl->shadowMap.BindForWriting();
    }
    // Cull front faces to reduce peter-panning and enable polygon offset to
    // further reduce shadow acne.
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    depthShader->Use();
    for (const auto& entity : scene.GetEntities())
    {
        if (!entity)
            continue;

        if (!entity->HasComponent<TransformComponent>())
            continue;

        // Frustum culling against the light frustum: if entity has a bounding
        // sphere, test it against the light's frustum so we only render what
        // the light can see into the shadow map.
        if (entity->HasComponent<BoundingSphereComponent>())
        {
            auto& bs = entity->GetComponent<BoundingSphereComponent>();
            glm::vec3 worldCenter = entity->GetComponent<TransformComponent>().position + bs.center;
            float maxScale = glm::compMax(entity->GetComponent<TransformComponent>().scale);
            float worldRadius = bs.radius * maxScale;
            if (!lightCuller.IsSphereVisible(worldCenter, worldRadius))
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

    if (m_Impl && m_Impl->shadowsEnabled)
    {
        m_Impl->shadowMap.Unbind();
        // Restore the previous viewport (window size)
        glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    }
    // Restore polygon offset and face culling
    glDisable(GL_POLYGON_OFFSET_FILL);
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
        if (m_Impl && m_Impl->shadowsEnabled)
        {
            // Bind shadow map to texture unit 1
            m_Impl->shadowMap.BindForReading(1);
            renderer.shader->SetInt("u_ShadowMap", 1);
            renderer.shader->SetFloat("u_ShadowBias", m_Impl->shadowBias);
        }

        meshComponent.mesh->Draw();
    }
}