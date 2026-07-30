#pragma once

#include "components/MeshComponent.h"
#include "components/MeshRendererComponent.h"
#include "components/TransformComponent.h"
#include "components/CameraComponent.h"

namespace MyEngine {

    class MeshRendererSystem {
    public:
        void Render(MeshComponent& mesh,
            MeshRendererComponent& renderer,
            TransformComponent& transform,
            CameraComponent& camera)
        {
            if (!renderer.visible) return;
            if (!renderer.shader)  return;

            // Upload GPU data if not yet done
            if (!mesh.isUploaded)
                mesh.Upload();

            renderer.shader->Use();

            // Matrices
            renderer.shader->SetMat4("model",
                transform.GetModelMatrix());
            renderer.shader->SetMat4("view",
                camera.viewMatrix);
            renderer.shader->SetMat4("projection",
                camera.projectionMatrix);

            // Tint color
            renderer.shader->SetVec3("objectColor",
                renderer.color.r,
                renderer.color.g,
                renderer.color.b);

            // Wireframe toggle
            if (renderer.wireframe)
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // Draw
            glBindVertexArray(mesh.VAO);

            if (!mesh.indices.empty())
                glDrawElements(GL_TRIANGLES,
                    (GLsizei)mesh.indices.size(),
                    GL_UNSIGNED_INT, 0);
            else
                glDrawArrays(GL_TRIANGLES, 0,
                    (GLsizei)mesh.vertices.size());

            glBindVertexArray(0);

            // Reset wireframe
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    };

} // namespace MyEngine