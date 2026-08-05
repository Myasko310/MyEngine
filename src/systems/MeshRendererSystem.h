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

    // Expose the GL texture for UI preview
    unsigned int GetShadowTexture() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
