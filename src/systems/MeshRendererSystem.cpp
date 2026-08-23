#include "systems/MeshRendererSystem.h"

#include "rendering/IBLProbe.h"
#include "components/LODComponent.h"

#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/TransformComponent.h"
#include "components/LightComponent.h"
#include "components/AnimationComponent.h"
#include "core/AssetManager.h"
#include "ecs/Scene.h"
#include "ecs/TransformHierarchy.h"
#include "components/BoundingSphereComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    for (auto& cm : m_Impl->cascadeMaps)
    {
        if (cm.GetDepthTexture() == 0)
            cm.Init(size, size);
    }
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

unsigned int MeshRendererSystem::GetCascadeTexture(int cascade) const
{
    if (!m_Impl || cascade < 0 || cascade >= MAX_CASCADES) return 0;
    return m_Impl->cascadeMaps[cascade].GetDepthTexture();
}

unsigned int MeshRendererSystem::GetShadowTexture() const
{
    return GetCascadeTexture(0);
}

unsigned int MeshRendererSystem::GetPointShadowTexture(int lightIndex) const
{
    if (!m_Impl || lightIndex < 0 || lightIndex >= static_cast<int>(m_Impl->pointShadowMaps.size()))
        return 0;
    return m_Impl->pointShadowMaps[lightIndex].GetDepthCubemap();
}

void MeshRendererSystem::SetNumCascades(int n)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->numCascades = std::clamp(n, 1, MAX_CASCADES);
}

int MeshRendererSystem::GetNumCascades() const
{
    return m_Impl ? m_Impl->numCascades : 4;
}

void MeshRendererSystem::SetSplitLambda(float lambda)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->splitLambda = std::clamp(lambda, 0.0f, 1.0f);
}

float MeshRendererSystem::GetSplitLambda() const
{
    return m_Impl ? m_Impl->splitLambda : 0.99f;
}

void MeshRendererSystem::InitIBL(unsigned int skyboxCubemap)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->pendingIBLCubemap = skyboxCubemap;
    m_Impl->iblEnabled = true;
}
void MeshRendererSystem::SetIBLEnabled(bool enabled)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->iblEnabled = enabled;
}
bool MeshRendererSystem::GetIBLEnabled() const { return m_Impl ? m_Impl->iblEnabled : false; }
void MeshRendererSystem::SetIBLIntensity(float intensity)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->iblIntensity = intensity;
}
float MeshRendererSystem::GetIBLIntensity() const { return m_Impl ? m_Impl->iblIntensity : 1.0f; }

void MeshRendererSystem::SetSSAOEnabled(bool enabled)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->ssaoEnabled = enabled;
}
bool MeshRendererSystem::GetSSAOEnabled() const { return m_Impl ? m_Impl->ssaoEnabled : false; }

void MeshRendererSystem::SetSSAORadius(float r)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->ssaoPass.SetRadius(r);
}
float MeshRendererSystem::GetSSAORadius() const { return m_Impl ? m_Impl->ssaoPass.GetRadius() : 0.5f; }

void MeshRendererSystem::SetSSAOBias(float b)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->ssaoPass.SetBias(b);
}
float MeshRendererSystem::GetSSAOBias() const { return m_Impl ? m_Impl->ssaoPass.GetBias() : 0.025f; }

void MeshRendererSystem::SetSSAOPower(float p)
{
    if (!m_Impl) m_Impl = std::make_unique<Impl>();
    m_Impl->ssaoPass.SetPower(p);
}
float MeshRendererSystem::GetSSAOPower() const { return m_Impl ? m_Impl->ssaoPass.GetPower() : 1.5f; }


void MeshRendererSystem::Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection)
{
    if (!m_Impl)
        m_Impl = std::make_unique<Impl>();

    // Static resources: depth shaders and shadow maps
    static std::shared_ptr<MyEngine::Shader> depthShader = nullptr;
    static std::shared_ptr<MyEngine::Shader> depthSkinnedShader = nullptr;
    static std::shared_ptr<MyEngine::Shader> pointDepthShader = nullptr;
    static MyEngine::IBLProbe                s_IBLProbe;
    static bool initialized = false;

    // Bake IBL if a new cubemap was requested via InitIBL()
    if (m_Impl && m_Impl->pendingIBLCubemap != 0)
    {
        s_IBLProbe.Init(m_Impl->pendingIBLCubemap);
        m_Impl->pendingIBLCubemap = 0;
    }

    if (!initialized)
    {
        depthShader = std::make_shared<MyEngine::Shader>("shaders/depth.vert", "shaders/depth.frag");
        depthSkinnedShader = std::make_shared<MyEngine::Shader>("shaders/depth_skinned.vert", "shaders/depth.frag");
        pointDepthShader = std::make_shared<MyEngine::Shader>("shaders/point_depth.vert", "shaders/point_depth.geom", "shaders/point_depth.frag");
        // Initialize impl shadow map
        if (!m_Impl)
            m_Impl = std::make_unique<Impl>();
        for (auto& cm : m_Impl->cascadeMaps)
            cm.Init(m_Impl->shadowSize, m_Impl->shadowSize);
        for (auto& pointShadowMap : m_Impl->pointShadowMaps)
            pointShadowMap.Init(m_Impl->pointShadowSize);
        initialized = true;
    }

    // (Re-)initialize SSAO if viewport size changed or first use
    {
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        unsigned int vpW = static_cast<unsigned int>(vp[2]);
        unsigned int vpH = static_cast<unsigned int>(vp[3]);
        if (vpW > 0 && vpH > 0)
        {
            if (m_Impl->ssaoPass.GetOcclusionTexture() == 0)
                m_Impl->ssaoPass.Init(vpW, vpH);
            else
                m_Impl->ssaoPass.Resize(vpW, vpH);
        }
    }

    // Find first directional light in the scene (if any), and collect
    // point/spot lights (capped to the shader's fixed-size uniform arrays).
    glm::vec3 lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 ambientColor = glm::vec3(0.08f);

    constexpr int kMaxPointLights = 4;
    constexpr int kMaxSpotLights = 4;

    struct PointLightData { glm::vec3 pos; glm::vec3 color; float range; float shadowBias; bool castShadows; };
    struct SpotLightData { glm::vec3 pos; glm::vec3 dir; glm::vec3 color; float range; float innerCos; float outerCos; };

    std::vector<PointLightData> pointLights;
    std::vector<SpotLightData> spotLights;
    std::array<int, kMaxPointLights> pointShadowMapIndices;
    pointShadowMapIndices.fill(-1);
    std::array<float, kMaxPointLights> pointShadowFarPlanes;
    pointShadowFarPlanes.fill(1.0f);
    bool foundDirectional = false;
    bool directionalCastShadows = false;

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
                float dirLen = glm::length(L.direction);
                if (dirLen > 0.0001f)
                    lightDir = L.direction / dirLen;
                else
                    lightDir = glm::vec3(0.0f, -1.0f, 0.0f);

                lightColor = L.color * L.intensity;
                directionalCastShadows = L.castShadows;
                m_Impl->shadowBias = L.shadowBias;
                foundDirectional = true;
            }
        }
        else if (L.type == MyEngine::LightComponent::Type::Point)
        {
            if (static_cast<int>(pointLights.size()) < kMaxPointLights)
                pointLights.push_back({ L.position, L.color * L.intensity, L.range, L.shadowBias, L.castShadows });
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

    // --- Cascaded Shadow Maps ---
    // Compute cascade split distances using the practical split scheme
    // (blend of logarithmic and uniform), then fit a tight light-space
    // ortho frustum around each camera sub-frustum slice.

    const int numCascades = m_Impl ? std::clamp(m_Impl->numCascades, 1, MAX_CASCADES) : 4;
    const float splitLambda = m_Impl ? m_Impl->splitLambda : 0.75f;

    // Reconstruct camera near/far from the projection matrix
    // For a perspective matrix: near = P[3][2] / (P[2][2] - 1), far = P[3][2] / (P[2][2] + 1)
    const float camNear = projection[3][2] / (projection[2][2] - 1.0f);
    const float camFar  = projection[3][2] / (projection[2][2] + 1.0f);

    // Cascade split planes in view space (near to far)
    // splits[0] = camNear, splits[numCascades] = camFar
    std::array<float, MAX_CASCADES + 1> splitDepths;
    splitDepths[0] = camNear;
    for (int i = 1; i <= numCascades; ++i)
    {
        float ratio     = static_cast<float>(i) / static_cast<float>(numCascades);
        float splitLog  = camNear * std::pow(camFar / camNear, ratio);
        float splitUnif = camNear + (camFar - camNear) * ratio;
        splitDepths[i]  = splitLambda * splitLog + (1.0f - splitLambda) * splitUnif;
    }

    // For each cascade build a light-space ortho matrix fitted to the 8
    // corners of the camera sub-frustum slice
    std::array<glm::mat4, MAX_CASCADES> cascadeLightSpaceMatrices;
    std::array<float,     MAX_CASCADES> cascadeSplitFar;   // far plane in view space for each cascade

    // Light direction helpers (same for all cascades)
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (fabs(glm::dot(lightDir, up)) > 0.99f)
        up = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::mat4 invVP = glm::inverse(projection * view);

    for (int ci = 0; ci < numCascades; ++ci)
    {
        float nearSlice = splitDepths[ci];
        float farSlice  = splitDepths[ci + 1];
        cascadeSplitFar[ci] = farSlice;

        // 8 NDC corners of this frustum slice
        glm::vec4 ndcCorners[8] = {
            { -1, -1, -1, 1 }, {  1, -1, -1, 1 }, { -1,  1, -1, 1 }, {  1,  1, -1, 1 },
            { -1, -1,  1, 1 }, {  1, -1,  1, 1 }, { -1,  1,  1, 1 }, {  1,  1,  1, 1 },
        };

        // Reconstruct world-space positions of the 8 frustum corners
        // for the depth range [nearSlice, farSlice].
        // We linearly interpolate along the view-space depth direction.
        glm::mat4 invProj = glm::inverse(projection);
        glm::mat4 invView = glm::inverse(view);

        // Helper: NDC z for a given view-space depth d
        auto viewDepthToNDC = [&](float d) -> float {
            // The reverse: NDC_z such that view-space depth = d
            // For perspective: NDC_z = (far+near)/(far-near) + 2*far*near/((far-near)*d)  -- but easier via projection
            glm::vec4 clip = projection * glm::vec4(0.0f, 0.0f, -d, 1.0f);
            return clip.z / clip.w;
        };

        float ndcNear = viewDepthToNDC(nearSlice);
        float ndcFar  = viewDepthToNDC(farSlice);

        glm::vec3 worldCorners[8];
        for (int k = 0; k < 8; ++k)
        {
            float ndcZ = (k < 4) ? ndcNear : ndcFar;
            glm::vec4 c = { ndcCorners[k].x, ndcCorners[k].y, ndcZ, 1.0f };
            glm::vec4 w = invVP * c;
            worldCorners[k] = glm::vec3(w) / w.w;
        }

        // Centroid in world space
        glm::vec3 center(0.0f);
        for (auto& wc : worldCorners) center += wc;
        center /= 8.0f;

        // Bounding sphere radius for the sub-frustum (stable across rotations)
        float radius = 0.0f;
        for (auto& wc : worldCorners)
            radius = std::max(radius, glm::length(wc - center));
        radius = std::ceil(radius * 16.0f) / 16.0f; // snap to texel grid

        glm::mat4 lightView = glm::lookAt(center - lightDir * radius, center, up);

        // Texel-snapping to eliminate shadow shimmering as the camera moves
        float texelSize = (2.0f * radius) / static_cast<float>(m_Impl->shadowSize);
        glm::mat4 lightViewSnap = lightView;
        glm::vec3 shadowOrigin  = glm::vec3(lightView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        float snapX = std::round(shadowOrigin.x / texelSize) * texelSize - shadowOrigin.x;
        float snapY = std::round(shadowOrigin.y / texelSize) * texelSize - shadowOrigin.y;
        lightViewSnap = glm::translate(glm::mat4(1.0f), glm::vec3(snapX, snapY, 0.0f)) * lightView;

        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius,
                                          -radius * 6.0f,  radius * 6.0f);
        cascadeLightSpaceMatrices[ci] = lightProj * lightViewSnap;
    }

    // Frustum cullers (camera only; cascades use their own matrices)
    MyEngine::FrustumCuller lightCuller;  // kept for depth pass per cascade
    MyEngine::FrustumCuller cameraCuller;
    glm::mat4 cameraVP = projection * view;
    cameraCuller.Update(cameraVP);

    // Extract camera world position
    glm::vec3 viewPos = glm::vec3(glm::inverse(view)[3]);

    // Save caller's FBO and viewport
    GLint callerFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &callerFBO);
    GLint lastViewport[4];
    glGetIntegerv(GL_VIEWPORT, lastViewport);

    // 0) SSAO geometry pre-pass: render view-space positions + normals into g-buffer
    if (m_Impl && m_Impl->ssaoEnabled && m_Impl->ssaoPass.GetOcclusionTexture() != 0)
    {
        MyEngine::Shader* gbufShader = m_Impl->ssaoPass.GetGBufferShader();
        if (gbufShader)
        {
            m_Impl->ssaoPass.BeginGeometryPass();
            glEnable(GL_DEPTH_TEST);
            gbufShader->Use();
            for (const auto& entity : scene.GetEntities())
            {
                if (!entity) continue;
                if (!entity->HasComponent<TransformComponent>()) continue;
                if (!entity->HasComponent<MeshComponent>())      continue;
                if (!entity->HasComponent<MeshRendererComponent>()) continue;
                auto& mr = entity->GetComponent<MeshRendererComponent>();
                if (!mr.visible) continue;
                auto& mc = entity->GetComponent<MeshComponent>();
                if (!mc.mesh) continue;
                glm::mat4 wm = TransformHierarchy::GetWorldMatrix(scene, *entity);
                gbufShader->SetMat4("u_Model",      wm);
                gbufShader->SetMat4("u_View",       view);
                gbufShader->SetMat4("u_Projection", projection);
                mc.mesh->Draw();
            }
            m_Impl->ssaoPass.EndGeometryPass();
            // Run the SSAO + blur passes
            m_Impl->ssaoPass.Compute(projection, view);
            // Restore viewport/FBO for subsequent passes
            glBindFramebuffer(GL_FRAMEBUFFER, callerFBO);
            glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
        }
    }

    // 1) CSM: Render a depth pass for each cascade from the light's perspective
    if (m_Impl && m_Impl->shadowsEnabled && foundDirectional && directionalCastShadows)
    {
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        MyEngine::Shader* activeDepthShader = nullptr;

        for (int ci = 0; ci < numCascades; ++ci)
        {
            m_Impl->cascadeMaps[ci].BindForWriting();

            lightCuller.Update(cascadeLightSpaceMatrices[ci]);

            depthShader->Use();
            activeDepthShader = depthShader.get();

            for (const auto& entity : scene.GetEntities())
            {
                if (!entity)
                    continue;
                if (!entity->HasComponent<TransformComponent>())
                    continue;

                glm::mat4 worldMatrix = TransformHierarchy::GetWorldMatrix(scene, *entity);
                if (entity->HasComponent<BoundingSphereComponent>())
                {
                    auto& bs = entity->GetComponent<BoundingSphereComponent>();
                    glm::vec3 worldCenter = glm::vec3(worldMatrix * glm::vec4(bs.center, 1.0f));
                    glm::vec3 worldScale(
                        glm::length(glm::vec3(worldMatrix[0])),
                        glm::length(glm::vec3(worldMatrix[1])),
                        glm::length(glm::vec3(worldMatrix[2]))
                    );
                    float worldRadius = bs.radius * glm::compMax(worldScale);
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

                bool isAnimated = entity->HasComponent<AnimationComponent>();
                MyEngine::Shader* shaderToUse = isAnimated ? depthSkinnedShader.get() : depthShader.get();
                if (shaderToUse != activeDepthShader)
                {
                    shaderToUse->Use();
                    activeDepthShader = shaderToUse;
                }

                shaderToUse->SetMat4("u_LightSpace", cascadeLightSpaceMatrices[ci]);
                shaderToUse->SetMat4("u_Model", worldMatrix);

                if (isAnimated)
                {
                    auto& anim = entity->GetComponent<AnimationComponent>();
                    int boneCount = std::min(static_cast<int>(anim.boneMatrices.size()), MAX_ANIMATION_BONES);
                    for (int b = 0; b < boneCount; ++b)
                        shaderToUse->SetMat4("u_BoneMatrices[" + std::to_string(b) + "]", anim.boneMatrices[b]);
                }

                meshComponent.mesh->Draw();
            }

            m_Impl->cascadeMaps[ci].Unbind();
        }

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




    // Sort renderable entities by material renderQueue (opaque first, then
    // transparent) so alpha-blended objects composite correctly over opaque ones.
    std::vector<std::shared_ptr<Entity>> sortedEntities;
    for (const auto& entity : scene.GetEntities())
    {
        if (!entity) continue;
        if (!entity->HasComponent<TransformComponent>()) continue;
        if (!entity->HasComponent<MeshComponent>())      continue;
        if (!entity->HasComponent<MeshRendererComponent>()) continue;
        sortedEntities.push_back(entity);
    }
    std::stable_sort(sortedEntities.begin(), sortedEntities.end(),
        [](const std::shared_ptr<Entity>& a, const std::shared_ptr<Entity>& b)
        {
            int qa = 2000, qb = 2000;
            if (a->HasComponent<MeshRendererComponent>())
            {
                const auto& ra = a->GetComponent<MeshRendererComponent>();
                if (ra.material) qa = ra.material->renderQueue;
            }
            if (b->HasComponent<MeshRendererComponent>())
            {
                const auto& rb = b->GetComponent<MeshRendererComponent>();
                if (rb.material) qb = rb.material->renderQueue;
            }
            return qa < qb;
        });

    for (const auto& entity : sortedEntities)
    {
        auto& meshComponent = entity->GetComponent<MeshComponent>();
        auto& renderer = entity->GetComponent<MeshRendererComponent>();

        if (!renderer.visible)
            continue;

        if (!meshComponent.mesh)
            continue;

        // LOD: pick the best mesh level based on camera distance
        if (entity->HasComponent<LODComponent>())
        {
            auto& lod = entity->GetComponent<LODComponent>();
            if (lod.enabled && !lod.levels.empty())
            {
                auto& tc = entity->GetComponent<TransformComponent>();
                float dist = glm::length(viewPos - tc.position);

                int chosen = -1;
                for (int li = 0; li < static_cast<int>(lod.levels.size()); ++li)
                {
                    if (dist <= lod.levels[li].distanceThreshold)
                    {
                        chosen = li;
                        break;
                    }
                }
                if (chosen >= 0 && lod.levels[chosen].mesh)
                {
                    lod.activeLevel = chosen;
                    meshComponent.mesh = lod.levels[chosen].mesh;
                }
            }
        }

        auto activeMaterial = renderer.material;
        const bool isAnimated = entity->HasComponent<AnimationComponent>();

        std::shared_ptr<MyEngine::Shader> activeShader = renderer.shader;
        if (!isAnimated && activeMaterial && activeMaterial->shader)
            activeShader = activeMaterial->shader;
        if (!activeShader && activeMaterial && activeMaterial->shader)
            activeShader = activeMaterial->shader;

        if (isAnimated && activeShader)
        {
            const std::string& vertexPath = activeShader->GetVertexPath();
            if (vertexPath.find("skinned") == std::string::npos)
            {
                std::string fragmentPath = activeShader->GetFragmentPath();
                if (fragmentPath.empty())
                    fragmentPath = "shaders/lit.frag";
                activeShader = MyEngine::AssetManager::LoadShader("shaders/lit_skinned.vert", fragmentPath);
                renderer.shader = activeShader;
            }
        }

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

            // IBL environment lighting
            if (s_IBLProbe.IsReady() && m_Impl && m_Impl->iblEnabled)
            {
                int iblBase = nextTextureUnit;
                nextTextureUnit = s_IBLProbe.BindForPBR(iblBase);
                activeShader->SetInt("u_IrradianceMap", iblBase);
                activeShader->SetInt("u_PrefilterMap",  iblBase + 1);
                activeShader->SetInt("u_BrdfLUT",       iblBase + 2);
                activeShader->SetBool("u_UseIBL",       true);
                activeShader->SetFloat("u_IBLIntensity", m_Impl->iblIntensity);
            }
            else
            {
                activeShader->SetBool("u_UseIBL", false);
                activeShader->SetFloat("u_IBLIntensity", 1.0f);
            }
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
        activeShader->SetFloat("u_ShadowBias", m_Impl ? m_Impl->shadowBias : 0.005f);

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

            // Per-light bias from LightComponent, scaled by global point-shadow
            // bias control. Global 0.005f is neutral (x1.0 scale).
            float globalPointBias = m_Impl ? m_Impl->pointShadowBias : 0.005f;
            float biasScale = globalPointBias / 0.005f;
            float effectivePointBias = std::max(pointLights[i].shadowBias * biasScale, 0.0001f);
            activeShader->SetFloat("u_PointShadowBias[" + idx + "]", effectivePointBias);

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

        // CSM shadow uniforms
        activeShader->SetBool("u_DirectionalShadowsEnabled", m_Impl && m_Impl->shadowsEnabled && foundDirectional && directionalCastShadows);
        activeShader->SetInt("u_NumCascades", numCascades);
        if (m_Impl && m_Impl->shadowsEnabled)
        {
            activeShader->SetFloat("u_ShadowBias", m_Impl->shadowBias);
            for (int ci = 0; ci < numCascades; ++ci)
            {
                std::string idx = std::to_string(ci);
                activeShader->SetMat4("u_CascadeLightSpace[" + idx + "]", cascadeLightSpaceMatrices[ci]);
                activeShader->SetFloat("u_CascadeSplitFar[" + idx + "]", cascadeSplitFar[ci]);
                int shadowUnit = nextTextureUnit + ci;
                m_Impl->cascadeMaps[ci].BindForReading(shadowUnit);
                activeShader->SetInt("u_CascadeShadowMap[" + idx + "]", shadowUnit);
            }
            nextTextureUnit += numCascades;
        }

        // SSAO uniform
        bool ssaoActive = m_Impl && m_Impl->ssaoEnabled && m_Impl->ssaoPass.GetOcclusionTexture() != 0;
        activeShader->SetBool("u_SSAOEnabled", ssaoActive);
        activeShader->SetVec2("u_ScreenSize",
            glm::vec2(static_cast<float>(lastViewport[2]), static_cast<float>(lastViewport[3])));
        if (ssaoActive)
        {
            int ssaoUnit = nextTextureUnit++;
            glActiveTexture(GL_TEXTURE0 + ssaoUnit);
            glBindTexture(GL_TEXTURE_2D, m_Impl->ssaoPass.GetOcclusionTexture());
            activeShader->SetInt("u_SSAOTexture", ssaoUnit);
        }
        if (activeMaterial)
        {
            // Blend mode
            switch (activeMaterial->blendMode)
            {
            case MyEngine::BlendMode::AlphaBlend:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case MyEngine::BlendMode::Additive:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            default: // Opaque
                glDisable(GL_BLEND);
                break;
            }
            // Cull mode (only override when NOT in wireframe, which already disables culling)
            if (!wireframe)
            {
                switch (activeMaterial->cullMode)
                {
                case MyEngine::CullMode::Off:
                    glDisable(GL_CULL_FACE);
                    break;
                case MyEngine::CullMode::Front:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                    break;
                default: // Back
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    break;
                }
            }
            // Depth write / test
            glDepthMask(activeMaterial->depthWrite ? GL_TRUE : GL_FALSE);
            if (activeMaterial->depthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
        }

        meshComponent.mesh->Draw();

        // Restore default GL render state changed by per-material flags
        if (activeMaterial)
        {
            if (activeMaterial->blendMode != MyEngine::BlendMode::Opaque)
            {
                glDisable(GL_BLEND);
            }
            if (activeMaterial->cullMode != MyEngine::CullMode::Back)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
            }
            if (!activeMaterial->depthWrite)
                glDepthMask(GL_TRUE);
            if (!activeMaterial->depthTest)
                glEnable(GL_DEPTH_TEST);
        }
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