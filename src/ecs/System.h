#pragma once
#include "../ecs/Scene.h"

namespace MyEngine {

    // Base class for all systems
    class System {
    public:
        virtual ~System() = default;

        virtual void OnUpdate(Scene& scene, float deltaTime) = 0;
        virtual void OnStart(Scene& scene) {}
        virtual void OnStop(Scene& scene) {}
    };

} // namespace MyEngine