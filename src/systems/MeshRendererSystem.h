#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "ecs/Scene.h"

class MeshRendererSystem
{
public:
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

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
