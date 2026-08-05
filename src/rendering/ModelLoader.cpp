#include "model/ModelLoader.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MyEngine
{
    static bool ParseOBJFaceVertex(
        const std::string& token,
        unsigned int& positionIndex
    )
    {
        // Supports:
        // f 1 2 3
        // f 1/1 2/2 3/3
        // f 1/1/1 2/2/2 3/3/3
        // f 1//1 2//2 3//3

        if (token.empty())
            return false;

        std::stringstream ss(token);
        std::string indexString;

        if (!std::getline(ss, indexString, '/'))
            return false;

        if (indexString.empty())
            return false;

        positionIndex = static_cast<unsigned int>(std::stoul(indexString));

        return true;
    }

    std::shared_ptr<Mesh> ModelLoader::LoadOBJ(const std::string& filepath)
    {
        std::ifstream file(filepath);

        if (!file.is_open())
        {
            return nullptr;
        }

        std::vector<glm::vec3> positions;

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        std::string line;

        while (std::getline(file, line))
        {
            std::stringstream ss(line);

            std::string prefix;
            ss >> prefix;

            if (prefix == "v")
            {
                glm::vec3 position{};
                ss >> position.x >> position.y >> position.z;
                positions.push_back(position);
            }
            else if (prefix == "f")
            {
                std::vector<unsigned int> facePositionIndices;

                std::string token;
                while (ss >> token)
                {
                    unsigned int objPositionIndex = 0;

                    if (ParseOBJFaceVertex(token, objPositionIndex))
                    {
                        // OBJ indices start at 1
                        facePositionIndices.push_back(objPositionIndex - 1);
                    }
                }

                if (facePositionIndices.size() < 3)
                    continue;

                // Triangulate polygon fan:
                // 0,1,2 then 0,2,3 etc.
                for (size_t i = 1; i + 1 < facePositionIndices.size(); ++i)
                {
                    unsigned int tri[3] =
                    {
                        facePositionIndices[0],
                        facePositionIndices[i],
                        facePositionIndices[i + 1]
                    };

                    for (unsigned int idx : tri)
                    {
                        if (idx >= positions.size())
                            continue;

                        Vertex vertex;
                        vertex.Position = positions[idx];
                        vertex.Color = glm::vec3(1.0f, 1.0f, 1.0f);

                        vertices.push_back(vertex);
                        indices.push_back(static_cast<unsigned int>(indices.size()));
                    }
                }
            }
        }

        if (vertices.empty() || indices.empty())
        {
            return nullptr;
        }

        return std::make_shared<Mesh>(vertices, indices);
    }
}