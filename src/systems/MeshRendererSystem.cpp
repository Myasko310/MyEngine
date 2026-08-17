#include "systems/MeshRendererSystem.h"

#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/TransformComponent.h"
#include "components/LightComponent.h"
#include "components/AnimationComponent.h"
#include "ecs/Scene.h"
#include "ecs/TransformHierarchy.h"
#include "components/BoundingSphereComponent.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <glad/glad.h>
#include "rendering/ShadowMap.h"
#include "rendering/PointShadowMap.h"
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

void MeshRendererSystem::SetPointShadowsEnabled(bool enabled)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->pointShadowsEnabled = enabled;
}

bool MeshRendererSystem::GetPointShadowsEnabled() const
{
    return m_Impl ? m_Impl->pointShadowsEnabled : false;
}

void MeshRendererSystem::SetPointShadowSize(unsigned int size)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->pointShadowSize = size;
    for (auto& pointShadowMap : m_Impl->pointShadowMaps)
    {
        if (pointShadowMap.GetDepthCubemap() == 0 || pointShadowMap.GetSize() != size)
            pointShadowMap.Init(size);
    }
}

unsigned int MeshRendererSystem::GetPointShadowSize() const
{
    return m_Impl ? m_Impl->pointShadowSize : 0;
}

void MeshRendererSystem::SetPointShadowBias(float bias)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->pointShadowBias = bias;
}

float MeshRendererSystem::GetPointShadowBias() const
{
    return m_Impl ? m_Impl->pointShadowBias : 0.0f;
}

void MeshRendererSystem::SetWireframe(bool enabled)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->wireframe = enabled;
}

bool MeshRendererSystem::GetWireframe() const
{
    return m_Impl ? m_Impl->wireframe : false;
}

unsigned int MeshRendererSystem::GetShadowTexture() const
{
    return m_Impl ? m_Impl->shadowMap.GetDepthTexture() : 0;
}


void MeshRendererSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
{
    if (!m_Impl)
        m_Impl = std::make_unique<Impl>();

    // Static resources: depth shaders and shadow maps
    static std::shared_ptr<MyEngine::Shader> depthShader = nullptr;
    static std::shared_ptr<MyEngine::Shader> depthSkinnedShader = nullptr;
    static std::shared_ptr<MyEngine::Shader> pointDepthShader = nullptr;
    static bool initialized = false;

    if (!initialized)
    {
        depthShader = std::make_shared<MyEngine::Shader>("shaders/depth.vert", "shaders/depth.frag");
        depthSkinnedShader = std::make_shared<MyEngine::Shader>("shaders/depth_skinned.vert", "shaders/depth.frag");
        pointDepthShader = std::make_shared<MyEngine::Shader>("shaders/point_depth.vert", "shaders/point_depth.geom", "shaders/point_depth.frag");
        // Initialize impl shadow map
        if (!m_Impl)
            m_Impl = std::make_unique<Impl>();
        m_Impl->shadowMap.Init(m_Impl->shadowSize, m_Impl->shadowSize);
        for (auto& pointShadowMap : m_Impl->pointShadowMaps)
            pointShadowMap.Init(m_Impl->pointShadowSize);
        initialized = true;
    }

    // Find first directional light in the scene (if any), and collect
    // point/spot lights (capped to the shader's fixed-size uniform arrays).
    glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 ambientColor = glm::vec3(0.08f);

    constexpr int kMaxPointLights = 4;
    constexpr int kMaxSpotLights = 4;

    struct PointLightData { glm::vec3 pos; glm::vec3 color; float range; bool castShadows; };
    struct SpotLightData { glm::vec3 pos; glm::vec3 dir; glm::vec3 color; float range; float innerCos; float outerCos; };

    std::vector<PointLightData> pointLights;
    std::vector<SpotLightData> spotLights;
    std::array<int, kMaxPointLights> pointShadowMapIndices;
    pointShadowMapIndices.fill(-1);
    std::array<float, kMaxPointLights> pointShadowFarPlanes;
    pointShadowFarPlanes.fill(1.0f);
    bool foundDirectional = false;

    for (const auto& e : scene.GetEntities())
    {
        if (!e)
            continue;

        if (!e->HasComponent<MyEngine::LightComponent>())
            continue;

        auto& L = e->GetComponent<MyEngine::LightComponent>();
        if (L.type == MyEngine::LightComponent::Type::Directional)
        {
            if (!foundDirectional)
            {
                lightDir = L.direction;
                lightColor = L.color * L.intensity;
                foundDirectional = true;
            }
        }
        else if (L.type == MyEngine::LightComponent::Type::Point)
        {
            if (static_cast<int>(pointLights.size()) < kMaxPointLights)
                pointLights.push_back({ L.position, L.color * L.intensity, L.range, L.castShadows });
        }
        else if (L.type == MyEngine::LightComponent::Type::Spot)
        {
            if (static_cast<int>(spotLights.size()) < kMaxSpotLights)
            {
                float innerCos = glm::cos(glm::radians(L.innerCone));
                float outerCos = glm::cos(glm::radians(L.outerCone));
                spotLights.push_back({ L.position, L.direction, L.color * L.intensity, L.range, innerCos, outerCos });
            }
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

    // Extract the camera's world-space position from the inverse view matrix
    // for view-dependent lighting terms (specular half-vector, etc.).
    glm::vec3 viewPos = glm::vec3(glm::inverse(view)[3]);

    // Capture whatever framebuffer the caller had bound (e.g. the HDR
    // post-process target) so we can restore it after the shadow depth
    // pass, which binds its own FBO and would otherwise leave the default
    // framebuffer (0) bound for the main color pass.
    GLint callerFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &callerFBO);

    // 1) Render depth of scene from light's perspective
    GLint lastViewport[4];
    glGetIntegerv(GL_VIEWPORT, lastViewport);
    if (m_Impl && m_Impl->shadowsEnabled)
    {
        m_Impl->shadowMap.BindForWriting();

        // For the shadow depth pass we render both sides (disable face culling)
        // so planar geometry (like the ground) is recorded in the depth map
        // regardless of triangle winding. We still use polygon offset to reduce
        // shadow acne.
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        depthShader->Use();
        MyEngine::Shader* activeDepthShader = depthShader.get();
        for (const auto& entity : scene.GetEntities())
        {
            if (!entity)
                continue;

            if (!entity->HasComponent<TransformComponent>())
                continue;

            // Frustum culling against the light frustum: if entity has a bounding
            // sphere, test it against the light's frustum so we only render what
            // the light can see into the shadow map.
            glm::mat4 worldMatrix = TransformHierarchy::GetWorldMatrix(scene, *entity);
            if (entity->HasComponent<BoundingSphereComponent>())
            {
                auto& bs = entity->GetComponent<BoundingSphereComponent>();
                glm::vec3 worldCenter = glm::vec3(worldMatrix * glm::vec4(bs.center, 1.0f));
                float maxScale = glm::compMax(entity->GetComponent<TransformComponent>().scale);
                float worldRadius = bs.radius * maxScale;
                if (!lightCuller.IsSphereVisible(worldCenter, worldRadius))
                    continue;
            }

            if (!entity->HasComponent<MeshComponent>())
                continue;

            if (!entity->HasComponent<MeshRendererComponent>())
                continue;

            auto& meshComponent = entity->GetComponent<MeshComponent>();

            if (!meshComponent.mesh)
                continue;

            // Skinned meshes need their current animated pose baked into the
            // shadow depth map too, otherwise the shadow is cast from the
            // static bind pose and appears detached from the visible mesh.
            bool isAnimated = entity->HasComponent<AnimationComponent>();
            MyEngine::Shader* shaderToUse = isAnimated ? depthSkinnedShader.get() : depthShader.get();
            if (shaderToUse != activeDepthShader)
            {
                shaderToUse->Use();
                activeDepthShader = shaderToUse;
            }

            shaderToUse->SetMat4("u_LightSpace", lightSpaceMatrix);
            shaderToUse->SetMat4("u_Model", worldMatrix);

            if (isAnimated)
            {
                auto& anim = entity->GetComponent<AnimationComponent>();
                int boneCount = std::min(static_cast<int>(anim.boneMatrices.size()), MAX_ANIMATION_BONES);
                for (int b = 0; b < boneCount; ++b)
                {
                    shaderToUse->SetMat4("u_BoneMatrices[" + std::to_string(b) + "]", anim.boneMatrices[b]);
                }
            }

            meshComponent.mesh->Draw();
        }

        m_Impl->shadowMap.Unbind();
        // Restore the previous viewport (window size)
        glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    }

    // 2) Render point light depth cubemaps (for point lights that cast shadows)
    if (m_Impl && m_Impl->pointShadowsEnabled)
    {
        constexpr float pointNearPlane = 0.1f;
        int pointShadowIndex = 0;

        for (size_t i = 0; i < pointLights.size() && pointShadowIndex < static_cast<int>(m_Impl->pointShadowMaps.size()); ++i)
        {
            if (!pointLights[i].castShadows)
                continue;

            float pointFarPlane = std::max(pointLights[i].range, 1.0f);
            pointShadowMapIndices[i] = pointShadowIndex;
            pointShadowFarPlanes[i] = pointFarPlane;
            glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, pointNearPlane, pointFarPlane);
            std::array<glm::mat4, 6> shadowTransforms = {
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(0.0f, 1.0f, 0.0f),  glm::vec3(0.0f, 0.0f, 1.0f)),
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, -1.0f, 0.0f)),
                shadowProj * glm::lookAt(pointLights[i].pos, pointLights[i].pos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
            };

            m_Impl->pointShadowMaps[pointShadowIndex].BindForWriting();
            glDisable(GL_CULL_FACE);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);

            pointDepthShader->Use();
            for (int face = 0; face < 6; ++face)
            {
                pointDepthShader->SetMat4("u_ShadowMatrices[" + std::to_string(face) + "]", shadowTransforms[face]);
            }
            pointDepthShader->SetVec3("u_LightPos", pointLights[i].pos);
            pointDepthShader->SetFloat("u_FarPlane", pointFarPlane);

            for (const auto& entity : scene.GetEntities())
            {
                if (!entity || !entity->HasComponent<TransformComponent>() || !entity->HasComponent<MeshComponent>() || !entity->HasComponent<MeshRendererComponent>())
                    continue;

                auto& meshComponent = entity->GetComponent<MeshComponent>();
                auto& renderer = entity->GetComponent<MeshRendererComponent>();
                if (!meshComponent.mesh || !renderer.visible)
                    continue;

                glm::mat4 worldMatrix = TransformHierarchy::GetWorldMatrix(scene, *entity);
                pointDepthShader->SetMat4("u_Model", worldMatrix);
                meshComponent.mesh->Draw();
            }

            m_Impl->pointShadowMaps[pointShadowIndex].Unbind();
            ++pointShadowIndex;
        }

        // Restore the previous viewport (window size) after point shadow passes.
        glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    }

    // Restore the caller's framebuffer binding (may be an offscreen HDR
    // target used by the post-process pipeline, not necessarily 0).
    glBindFramebuffer(GL_FRAMEBUFFER, callerFBO);

    // Restore polygon offset and face culling (enable back-face culling for
    // the main pass)
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Wireframe is scoped to only this main color pass so it never affects
    // the shadow depth pass above (already finished) or post-processing/
    // ImGui rendering that happens after Render() returns. Face culling is
    // disabled while in wireframe so back-facing edges are also visible
    // instead of being culled away, which otherwise made meshes look like
    // only their near-side wireframe was drawn.
    bool wireframe = m_Impl && m_Impl->wireframe;
    if (wireframe)
    {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
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

        auto activeMaterial = renderer.material;
        auto activeShader = (activeMaterial && activeMaterial->shader) ? activeMaterial->shader : renderer.shader;
        if (!activeShader)
            continue;

        const glm::vec3& materialAlbedo = activeMaterial ? activeMaterial->albedo : renderer.albedo;
        float materialShininess = activeMaterial ? activeMaterial->shininess : renderer.shininess;
        bool useTexture = activeMaterial ? activeMaterial->useTexture : renderer.useTexture;
        const auto& baseTexture = activeMaterial ? activeMaterial->texture : renderer.texture;
        bool usePBR = activeMaterial ? activeMaterial->usePBR : renderer.usePBR;
        float metallic = activeMaterial ? activeMaterial->metallic : renderer.metallic;
        float roughness = activeMaterial ? activeMaterial->roughness : renderer.roughness;
        float aoStrength = activeMaterial ? activeMaterial->aoStrength : renderer.aoStrength;
        const glm::vec3& emissive = activeMaterial ? activeMaterial->emissive : renderer.emissive;
        const auto& albedoMap = activeMaterial ? activeMaterial->albedoMap : renderer.albedoMap;
        const auto& normalMap = activeMaterial ? activeMaterial->normalMap : renderer.normalMap;
        const auto& metallicRoughnessMap = activeMaterial ? activeMaterial->metallicRoughnessMap : renderer.metallicRoughnessMap;
        const auto& aoMap = activeMaterial ? activeMaterial->aoMap : renderer.aoMap;
        const auto& emissiveMap = activeMaterial ? activeMaterial->emissiveMap : renderer.emissiveMap;

        activeShader->Use();

        activeShader->SetMat4("u_Model", TransformHierarchy::GetWorldMatrix(scene, *entity));
        activeShader->SetMat4("u_View", view);
        activeShader->SetMat4("u_Projection", projection);

        // Skinned meshes upload their computed bone matrix palette (already
        // sampled/animated by AnimationSystem this frame) so the skinning
        // shader can blend vertex positions per bone.
        if (entity->HasComponent<AnimationComponent>())
        {
            auto& anim = entity->GetComponent<AnimationComponent>();
            int boneCount = std::min(static_cast<int>(anim.boneMatrices.size()), MAX_ANIMATION_BONES);
            for (int b = 0; b < boneCount; ++b)
            {
                activeShader->SetMat4("u_BoneMatrices[" + std::to_string(b) + "]", anim.boneMatrices[b]);
            }
        }

        // Material uniforms
        activeShader->SetVec3("u_MaterialAlbedo", materialAlbedo);
        activeShader->SetFloat("u_MaterialShininess", materialShininess);

        int nextTextureUnit = 0;

        if (usePBR)
        {
            activeShader->SetFloat("u_Metallic", metallic);
            activeShader->SetFloat("u_Roughness", roughness);
            activeShader->SetFloat("u_AOStrength", aoStrength);
            activeShader->SetVec3("u_Emissive", emissive);

            auto bindOptionalMap = [&](const std::shared_ptr<MyEngine::Texture>& map, const char* useUniform, const char* samplerUniform)
            {
                if (map)
                {
                    map->Bind(nextTextureUnit);
                    activeShader->SetInt(samplerUniform, nextTextureUnit);
                    activeShader->SetBool(useUniform, true);
                    ++nextTextureUnit;
                }
                else
                {
                    activeShader->SetBool(useUniform, false);
                }
            };

            bindOptionalMap(albedoMap, "u_UseAlbedoMap", "u_AlbedoMap");
            bindOptionalMap(normalMap, "u_UseNormalMap", "u_NormalMap");
            bindOptionalMap(metallicRoughnessMap, "u_UseMetallicRoughnessMap", "u_MetallicRoughnessMap");
            bindOptionalMap(aoMap, "u_UseAOMap", "u_AOMap");
            bindOptionalMap(emissiveMap, "u_UseEmissiveMap", "u_EmissiveMap");
        }
        else
        {
            activeShader->SetInt("u_Texture", 0);
            nextTextureUnit = 1;

            if (useTexture && baseTexture)
            {
                baseTexture->Bind(0);
                activeShader->SetBool("u_UseTexture", true);

                static bool debugOnce = false;
                if (!debugOnce)
                {
                    std::cout << "[MeshRendererSystem] Texture enabled!" << std::endl;
                    std::cout << "  - Texture ID: " << baseTexture->GetID() << std::endl;
                    std::cout << "  - Path: " << baseTexture->GetPath() << std::endl;
                    std::cout << "  - UseTexture uniform: true" << std::endl;
                    std::cout << "  - Texture unit: 0" << std::endl;
                    debugOnce = true;
                }
            }
            else
            {
                activeShader->SetBool("u_UseTexture", false);
            }
        }

        // Light uniforms
        activeShader->SetVec3("u_LightDirection", lightDir.x, lightDir.y, lightDir.z);
        activeShader->SetVec3("u_LightColor", lightColor.x, lightColor.y, lightColor.z);
        activeShader->SetVec3("u_AmbientColor", ambientColor.x, ambientColor.y, ambientColor.z);
        activeShader->SetVec3("u_ViewPos", viewPos);
        activeShader->SetFloat("u_PointShadowBias", m_Impl ? m_Impl->pointShadowBias : 0.02f);

        // Point lights
        activeShader->SetInt("u_NumPointLights", static_cast<int>(pointLights.size()));
        for (size_t i = 0; i < pointLights.size(); ++i)
        {
            std::string idx = std::to_string(i);
            activeShader->SetVec3("u_PointLightPos[" + idx + "]", pointLights[i].pos);
            activeShader->SetVec3("u_PointLightColor[" + idx + "]", pointLights[i].color);
            activeShader->SetFloat("u_PointLightRange[" + idx + "]", pointLights[i].range);
            activeShader->SetBool("u_PointLightCastShadows[" + idx + "]", pointShadowMapIndices[i] >= 0);
            activeShader->SetFloat("u_PointShadowFarPlane[" + idx + "]", pointShadowFarPlanes[i]);

            int pointShadowUnit = std::max(nextTextureUnit, 1);
            activeShader->SetInt("u_PointShadowMap[" + idx + "]", pointShadowUnit);
            if (pointShadowMapIndices[i] >= 0 && m_Impl && m_Impl->pointShadowsEnabled)
            {
                m_Impl->pointShadowMaps[pointShadowMapIndices[i]].BindForReading(pointShadowUnit);
            }
            nextTextureUnit = pointShadowUnit + 1;
        }

        // Spot lights
        activeShader->SetInt("u_NumSpotLights", static_cast<int>(spotLights.size()));
        for (size_t i = 0; i < spotLights.size(); ++i)
        {
            std::string idx = std::to_string(i);
            activeShader->SetVec3("u_SpotLightPos[" + idx + "]", spotLights[i].pos);
            activeShader->SetVec3("u_SpotLightDir[" + idx + "]", spotLights[i].dir);
            activeShader->SetVec3("u_SpotLightColor[" + idx + "]", spotLights[i].color);
            activeShader->SetFloat("u_SpotLightRange[" + idx + "]", spotLights[i].range);
            activeShader->SetFloat("u_SpotLightInnerCos[" + idx + "]", spotLights[i].innerCos);
            activeShader->SetFloat("u_SpotLightOuterCos[" + idx + "]", spotLights[i].outerCos);
        }

        // Shadow uniforms
        activeShader->SetMat4("u_LightSpace", lightSpaceMatrix);
        activeShader->SetBool("u_DirectionalShadowsEnabled", m_Impl && m_Impl->shadowsEnabled);
        if (m_Impl && m_Impl->shadowsEnabled)
        {
            int shadowUnit = nextTextureUnit;
            m_Impl->shadowMap.BindForReading(shadowUnit);
            activeShader->SetInt("u_ShadowMap", shadowUnit);
            activeShader->SetFloat("u_ShadowBias", m_Impl->shadowBias);
        }

        meshComponent.mesh->Draw();
    }

    // Restore fill mode and back-face culling so wireframe never leaks into
    // post-processing's fullscreen quad passes or ImGui rendering, both of
    // which run after this function returns.
    if (wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
}