#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "rendering/Shader.h"
#include "rendering/Texture.h"
#include "rendering/Material.h"

struct MeshRendererComponent
{
    std::shared_ptr<MyEngine::Material> material = nullptr;
    std::string materialPath;
    std::shared_ptr<MyEngine::Shader> shader = nullptr;
    bool visible = true;
    glm::vec3 albedo = glm::vec3(1.0f);
    float shininess = 32.0f;
    std::shared_ptr<MyEngine::Texture> texture = nullptr;
    bool useTexture = false;

    // --- PBR material properties (used when the assigned shader implements PBR, e.g. shaders/pbr.frag) ---
    bool usePBR = false;

    // Scalar factors (multiplied with the corresponding texture map, if bound)
    float metallic = 0.0f;
    float roughness = 0.5f;
    float aoStrength = 1.0f;
    glm::vec3 emissive = glm::vec3(0.0f);

    // Optional PBR texture maps. When a map is null, the corresponding scalar
    // factor above (or albedo, for albedoMap) is used instead.
    std::shared_ptr<MyEngine::Texture> albedoMap = nullptr;
    std::shared_ptr<MyEngine::Texture> normalMap = nullptr;
    std::shared_ptr<MyEngine::Texture> metallicRoughnessMap = nullptr; // G = roughness, B = metallic (glTF convention)
    std::shared_ptr<MyEngine::Texture> aoMap = nullptr;
    std::shared_ptr<MyEngine::Texture> emissiveMap = nullptr;
};