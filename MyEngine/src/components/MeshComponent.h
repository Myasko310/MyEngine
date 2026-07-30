#pragma once

#include "ecs/Component.h"
#include <glad/glad.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <cstddef> // offsetof

namespace MyEngine {

    struct Vertex {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec2 texCoords = glm::vec2(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
    };

    struct MeshComponent : public Component {
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;

        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;

        bool isUploaded = false;

        void Upload()
        {
            if (isUploaded) return;

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER,
                vertices.size() * sizeof(Vertex),
                vertices.data(),
                GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                indices.size() * sizeof(unsigned int),
                indices.data(),
                GL_STATIC_DRAW);

            // Position (location = 0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                sizeof(Vertex), (void*)offsetof(Vertex, position));
            glEnableVertexAttribArray(0);

            // Normal (location = 1)
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(1);

            // TexCoords (location = 2)
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
            glEnableVertexAttribArray(2);

            // Color (location = 3)
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
                sizeof(Vertex), (void*)offsetof(Vertex, color));
            glEnableVertexAttribArray(3);

            glBindVertexArray(0);

            isUploaded = true;
        }

        void Free()
        {
            if (!isUploaded) return;
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            isUploaded = false;
        }
    };

} // namespace MyEngine