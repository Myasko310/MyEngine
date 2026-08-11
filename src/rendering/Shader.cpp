#include "rendering/Shader.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace MyEngine
{
    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
        : m_VertexPath(vertexPath), m_FragmentPath(fragmentPath)
    {
        std::string vertSrc = LoadFile(vertexPath);
        std::string fragSrc = LoadFile(fragmentPath);

        if (vertSrc.empty())
        {
            std::cerr << "[Shader] Vertex shader source is empty: " << vertexPath << std::endl;
        }

        if (fragSrc.empty())
        {
            std::cerr << "[Shader] Fragment shader source is empty: " << fragmentPath << std::endl;
        }

        unsigned int vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
        unsigned int frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

        m_ID = glCreateProgram();

        glAttachShader(m_ID, vert);
        glAttachShader(m_ID, frag);
        glLinkProgram(m_ID);

        CheckProgramLink(m_ID);

        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    Shader::~Shader()
    {
        if (m_ID != 0)
        {
            glDeleteProgram(m_ID);
            m_ID = 0;
        }
    }

    Shader::Shader(Shader&& other) noexcept
    {
        m_ID = other.m_ID;
        other.m_ID = 0;
        m_VertexPath = std::move(other.m_VertexPath);
        m_FragmentPath = std::move(other.m_FragmentPath);
    }

    Shader& Shader::operator=(Shader&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ID != 0)
            {
                glDeleteProgram(m_ID);
            }

            m_ID = other.m_ID;
            other.m_ID = 0;
            m_VertexPath = std::move(other.m_VertexPath);
            m_FragmentPath = std::move(other.m_FragmentPath);
        }

        return *this;
    }

    void Shader::Use() const
    {
        glUseProgram(m_ID);
    }

    void Shader::Bind() const
    {
        Use();
    }

    void Shader::Unbind() const
    {
        glUseProgram(0);
    }

    unsigned int Shader::GetID() const
    {
        return m_ID;
    }

    void Shader::SetBool(const std::string& name, bool value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
        {
            std::cerr << "[Shader] Warning: Uniform '" << name << "' not found in shader program " << m_ID << std::endl;
        }
        glUniform1i(location, static_cast<int>(value));
    }

    void Shader::SetInt(const std::string& name, int value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
        {
            std::cerr << "[Shader] Warning: Uniform '" << name << "' not found in shader program " << m_ID << std::endl;
        }
        glUniform1i(location, value);
    }

    void Shader::SetFloat(const std::string& name, float value) const
    {
        glUniform1f(
            glGetUniformLocation(m_ID, name.c_str()),
            value
        );
    }

    void Shader::SetVec2(const std::string& name, float x, float y) const
    {
        glUniform2f(
            glGetUniformLocation(m_ID, name.c_str()),
            x,
            y
        );
    }

    void Shader::SetVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(
            glGetUniformLocation(m_ID, name.c_str()),
            x,
            y,
            z
        );
    }

    void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) const
    {
        glUniform4f(
            glGetUniformLocation(m_ID, name.c_str()),
            x,
            y,
            z,
            w
        );
    }

    void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
    {
        glUniform2fv(
            glGetUniformLocation(m_ID, name.c_str()),
            1,
            glm::value_ptr(value)
        );
    }

    void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
    {
        glUniform3fv(
            glGetUniformLocation(m_ID, name.c_str()),
            1,
            glm::value_ptr(value)
        );
    }

    void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
    {
        glUniform4fv(
            glGetUniformLocation(m_ID, name.c_str()),
            1,
            glm::value_ptr(value)
        );
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(
            glGetUniformLocation(m_ID, name.c_str()),
            1,
            GL_FALSE,
            glm::value_ptr(mat)
        );
    }

    std::string Shader::LoadFile(const std::string& path) const
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cerr << "[Shader] Cannot open file: " << path << std::endl;
            return "";
        }

        std::stringstream ss;
        ss << file.rdbuf();

        return ss.str();
    }

    unsigned int Shader::CompileShader(unsigned int type, const std::string& src) const
    {
        unsigned int id = glCreateShader(type);

        const char* c = src.c_str();

        glShaderSource(id, 1, &c, nullptr);
        glCompileShader(id);

        int success = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char log[1024];
            glGetShaderInfoLog(id, 1024, nullptr, log);

            std::cerr
                << "[Shader] Compile error ("
                << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT")
                << "):\n"
                << log
                << std::endl;
        }

        return id;
    }

    void Shader::CheckProgramLink(unsigned int program) const
    {
        int success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success)
        {
            char log[1024];
            glGetProgramInfoLog(program, 1024, nullptr, log);

            std::cerr
                << "[Shader] Link error:\n"
                << log
                << std::endl;
        }
    }
}