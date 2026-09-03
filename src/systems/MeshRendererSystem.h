#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "ecs/Scene.h"

class MeshRendererSystem
{
public:
    enum class DebugViewMode
    {
        FinalLit = 0,
        Albedo = 1,
        Normal = 2,
        Roughness = 3,
        Metallic = 4,
        AO = 5,
        Emissive = 6,
        Shadow = 7,
        SSAO = 8
    };

    struct ShadowDiagnostics
    {
        float directionalMs = 0.0f;
        float pointMs = 0.0f;
        float spotMs = 0.0f;
        int directionalCasters = 0;
        int pointCasters = 0;
        int spotCasters = 0;
        int shadowedPointLights = 0;
        int shadowedSpotLights = 0;
    };

    struct OcclusionDiagnostics
    {
        int totalCandidates = 0;
        int frustumRejected = 0;
        int occlusionRejected = 0;
        int temporalRejected = 0;
        int querySubmitted = 0;
        int queryVisible = 0;
        int queryHidden = 0;
        int visible = 0;
    };
    MeshRendererSystem();
    ~MeshRendererSystem();

    void Render(Scene& scene, const glm::mat4& view, const glm::mat4& projection);

    // Shadow controls
    void SetShadowsEnabled(bool enabled);
    bool GetShadowsEnabled() const;

    void SetShadowSize(unsigned int size);
    unsigned int GetShadowSize() const;

    void SetShadowBias(float bias);
    float GetShadowBias() const;

    // CSM cascade controls
    void SetNumCascades(int n);
    int  GetNumCascades() const;
    void SetSplitLambda(float lambda);
    float GetSplitLambda() const;
    void SetShadowStabilizationEnabled(bool enabled);
    bool GetShadowStabilizationEnabled() const;
    void SetCascadeBlendFactor(float blend);
    float GetCascadeBlendFactor() const;

    // Point light shadow
    void SetPointShadowsEnabled(bool enabled);
    bool GetPointShadowsEnabled() const;

    void SetPointShadowSize(unsigned int size);
    unsigned int GetPointShadowSize() const;

    void SetPointShadowBias(float bias);
    float GetPointShadowBias() const;
    void SetPointShadowPCFSamples(int samples);
    int GetPointShadowPCFSamples() const;
    void SetPointShadowPCFRadius(float radius);
    float GetPointShadowPCFRadius() const;

    // Spot light shadows
    void SetSpotShadowsEnabled(bool enabled);
    bool GetSpotShadowsEnabled() const;

    void SetSpotShadowSize(unsigned int size);
    unsigned int GetSpotShadowSize() const;
    void SetSpotShadowPCFRadius(float radius);
    float GetSpotShadowPCFRadius() const;
    void ApplyShadowAutoBudget();

    // Wireframe rendering: scoped to only the main color pass (not the
    // shadow depth pass, post-processing, or ImGui) so toggling it doesn't
    // affect unrelated rendering or performance elsewhere.
    void SetWireframe(bool enabled);
    bool GetWireframe() const;
    void SetOcclusionApproximationEnabled(bool enabled);
    bool GetOcclusionApproximationEnabled() const;
    void SetGPUOcclusionQueriesEnabled(bool enabled);
    bool GetGPUOcclusionQueriesEnabled() const;
    void SetOcclusionQueryRecheckFrames(int frames);
    int GetOcclusionQueryRecheckFrames() const;
    void SetDebugViewMode(DebugViewMode mode);
    DebugViewMode GetDebugViewMode() const;

    // IBL (Image-Based Lighting) — call InitIBL with the skybox cubemap GL id
    // to bake irradiance/prefilter/BRDF textures and enable environment lighting.
    void InitIBL(unsigned int skyboxCubemap);
    void SetIBLEnabled(bool enabled);
    bool GetIBLEnabled() const;
    void SetIBLIntensity(float intensity);
    float GetIBLIntensity() const;

    // SSAO (Screen-Space Ambient Occlusion)
    void SetSSAOEnabled(bool enabled);
    bool GetSSAOEnabled() const;
    void SetSSAORadius(float r);
    float GetSSAORadius() const;
    void SetSSAOBias(float b);
    float GetSSAOBias() const;
    void SetSSAOPower(float p);
    float GetSSAOPower() const;

    // Expose cascade GL textures for UI preview
    unsigned int GetCascadeTexture(int cascade) const;
    unsigned int GetShadowTexture() const; // compat: returns cascade 0
    unsigned int GetPointShadowTexture(int lightIndex) const;
    unsigned int GetSpotShadowTexture(int lightIndex) const;
    ShadowDiagnostics GetShadowDiagnostics() const;
    OcclusionDiagnostics GetOcclusionDiagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
