#include "rendering/Shader.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

bool MyEngine::Shader::s_AutoHotReloadEnabled = true;

namespace MyEngine
{
    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
        : m_VertexPath(vertexPath), m_FragmentPath(fragmentPath)
    {
        ReloadFromDisk();
    }

    Shader::Shader(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath)
        : m_VertexPath(vertexPath), m_GeometryPath(geometryPath), m_FragmentPath(fragmentPath)
    {
        ReloadFromDisk();
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
        m_GeometryPath = std::move(other.m_GeometryPath);
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
            m_GeometryPath = std::move(other.m_GeometryPath);
            m_FragmentPath = std::move(other.m_FragmentPath);
        }

        return *this;
    }

    void Shader::Use() const
    {
        if (s_AutoHotReloadEnabled)
            const_cast<Shader*>(this)->TryHotReloadFromDisk();
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

    bool Shader::ReloadFromDisk()
    {
        std::string vertSrc;
        std::string geomSrc;
        std::string fragSrc;
        std::filesystem::file_time_type vertWrite;
        std::filesystem::file_time_type geomWrite;
        std::filesystem::file_time_type fragWrite;

        if (!LoadFileAndTimestamp(m_VertexPath, vertSrc, vertWrite) ||
            !LoadFileAndTimestamp(m_FragmentPath, fragSrc, fragWrite))
        {
            m_LastError = "Failed to read shader source files.";
            return false;
        }

        bool hasGeometry = !m_GeometryPath.empty();
        if (hasGeometry && !LoadFileAndTimestamp(m_GeometryPath, geomSrc, geomWrite))
        {
            m_LastError = "Failed to read geometry shader source file.";
            return false;
        }

        m_LastError.clear();

        unsigned int vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
        if (vert == 0)
            return false;

        unsigned int geom = 0;
        if (hasGeometry)
        {
            geom = CompileShader(GL_GEOMETRY_SHADER, geomSrc);
            if (geom == 0)
            {
                glDeleteShader(vert);
                return false;
            }
        }

        unsigned int frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (frag == 0)
        {
            glDeleteShader(vert);
            if (geom != 0) glDeleteShader(geom);
            return false;
        }

        unsigned int newProgram = glCreateProgram();
        glAttachShader(newProgram, vert);
        if (geom != 0) glAttachShader(newProgram, geom);
        glAttachShader(newProgram, frag);
        glLinkProgram(newProgram);

        bool linked = CheckProgramLink(newProgram);

        glDeleteShader(vert);
        if (geom != 0) glDeleteShader(geom);
        glDeleteShader(frag);

        if (!linked)
        {
            glDeleteProgram(newProgram);
            return false;
        }

        if (m_ID != 0)
            glDeleteProgram(m_ID);
        m_ID = newProgram;
        m_VertexWriteTime = vertWrite;
        m_FragmentWriteTime = fragWrite;
        m_GeometryWriteTime = hasGeometry ? geomWrite : std::filesystem::file_time_type{};
        return true;
    }

    bool Shader::TryHotReloadFromDisk()
    {
        if (!ShouldHotReload())
            return false;
        return ReloadFromDisk();
    }

    const std::string& Shader::GetLastError() const
    {
        return m_LastError;
    }

    void Shader::SetAutoHotReloadEnabled(bool enabled)
    {
        s_AutoHotReloadEnabled = enabled;
    }

    bool Shader::GetAutoHotReloadEnabled()
    {
        return s_AutoHotReloadEnabled;
    }

    void Shader::SetBool(const std::string& name, bool value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform1i(location, static_cast<int>(value));
    }

    void Shader::SetInt(const std::string& name, int value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform1i(location, value);
    }

    void Shader::SetFloat(const std::string& name, float value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform1f(location, value);
    }

    void Shader::SetVec2(const std::string& name, float x, float y) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform2f(location, x, y);
    }

    void Shader::SetVec3(const std::string& name, float x, float y, float z) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform3f(location, x, y, z);
    }

    void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform4f(location, x, y, z, w);
    }

    void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform2fv(location, 1, glm::value_ptr(value));
    }

    void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform3fv(location, 1, glm::value_ptr(value));
    }

    void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniform4fv(location, 1, glm::value_ptr(value));
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& mat) const
    {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
    }

    void Shader::SetMat4Array(const std::string& name, const glm::mat4* values, int count) const
    {
        if (!values || count <= 0)
            return;
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        if (location == -1)
            return;
        glUniformMatrix4fv(location, count, GL_FALSE, glm::value_ptr(values[0]));
    }

    bool Shader::LoadFileAndTimestamp(const std::string& path, std::string& outSource, std::filesystem::file_time_type& outWriteTime)
    {
        if (path.empty())
            return false;

        std::ifstream file(path);
        if (!file.is_open())
        {
            m_LastError = "Cannot open file: " + path;
            std::cerr << "[Shader] " << m_LastError << std::endl;
            return false;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        outSource = ss.str();

        std::error_code ec;
        outWriteTime = std::filesystem::last_write_time(path, ec);
        if (ec)
        {
            m_LastError = "Cannot query file timestamp: " + path;
            return false;
        }

        return true;
    }

    std::string Shader::LoadFile(const std::string& path)
    {
        std::string src;
        std::filesystem::file_time_type writeTime;
        if (!LoadFileAndTimestamp(path, src, writeTime))
            return "";
        return src;
    }

    unsigned int Shader::CompileShader(unsigned int type, const std::string& src)
    {
        unsigned int id = glCreateShader(type);

        const char* c = src.c_str();

        glShaderSource(id, 1, &c, nullptr);
        glCompileShader(id);

        int success = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char log[2048];
            glGetShaderInfoLog(id, 2048, nullptr, log);
            std::string typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : (type == GL_GEOMETRY_SHADER ? "GEOMETRY" : "FRAGMENT");
            m_LastError = "Compile error (" + typeName + "):\n" + std::string(log);
            std::cerr << "[Shader] " << m_LastError << std::endl;
            glDeleteShader(id);
            return 0;
        }

        return id;
    }

    bool Shader::CheckProgramLink(unsigned int program)
    {
        int success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success)
        {
            char log[2048];
            glGetProgramInfoLog(program, 2048, nullptr, log);
            m_LastError = "Link error:\n" + std::string(log);
            std::cerr << "[Shader] " << m_LastError << std::endl;
            return false;
        }

        return true;
    }

    bool Shader::ShouldHotReload() const
    {
        std::error_code ec;
        auto vertWrite = std::filesystem::last_write_time(m_VertexPath, ec);
        if (!ec && vertWrite != m_VertexWriteTime)
            return true;

        ec.clear();
        auto fragWrite = std::filesystem::last_write_time(m_FragmentPath, ec);
        if (!ec && fragWrite != m_FragmentWriteTime)
            return true;

        if (!m_GeometryPath.empty())
        {
            ec.clear();
            auto geomWrite = std::filesystem::last_write_time(m_GeometryPath, ec);
            if (!ec && geomWrite != m_GeometryWriteTime)
                return true;
        }

        return false;
    }
}
