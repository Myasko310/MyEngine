#pragma once

#include <string>

#include <glm/glm.hpp>

namespace MyEngine
{
    class Shader
    {
    public:
        Shader(const std::string& vertexPath, const std::string& fragmentPath);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;

        void Use() const;
        void Bind() const;
        void Unbind() const;

        unsigned int GetID() const;

        void SetBool(const std::string& name, bool value) const;
        void SetInt(const std::string& name, int value) const;
        void SetFloat(const std::string& name, float value) const;

        void SetVec2(const std::string& name, float x, float y) const;
        void SetVec3(const std::string& name, float x, float y, float z) const;
        void SetVec4(const std::string& name, float x, float y, float z, float w) const;

        void SetVec2(const std::string& name, const glm::vec2& value) const;
        void SetVec3(const std::string& name, const glm::vec3& value) const;
        void SetVec4(const std::string& name, const glm::vec4& value) const;

        void SetMat4(const std::string& name, const glm::mat4& mat) const;

        const std::string& GetVertexPath() const { return m_VertexPath; }
        const std::string& GetFragmentPath() const { return m_FragmentPath; }

    private:
        unsigned int m_ID = 0;
        std::string m_VertexPath;
        std::string m_FragmentPath;

    private:
        std::string LoadFile(const std::string& path) const;
        unsigned int CompileShader(unsigned int type, const std::string& src) const;
        void CheckProgramLink(unsigned int program) const;
    };
}