#include "rendering/Shader.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace MyEngine {

    Shader::Shader(const std::string& vertexPath,
        const std::string& fragmentPath)
    {
        std::string vertSrc = LoadFile(vertexPath);
        std::string fragSrc = LoadFile(fragmentPath);

        unsigned int vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
        unsigned int frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

        m_ID = glCreateProgram();
        glAttachShader(m_ID, vert);
        glAttachShader(m_ID, frag);
        glLinkProgram(m_ID);

        int success;
        glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
        if (!success)
        {
            char log[512];
            glGetProgramInfoLog(m_ID, 512, nullptr, log);
            std::cerr << "[Shader] Link error:\n" << log << std::endl;
        }

        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    Shader::~Shader()
    {
        if (m_ID) glDeleteProgram(m_ID);
    }

    void Shader::Use()    const { glUseProgram(m_ID); }
    void Shader::Bind()   const { glUseProgram(m_ID); }
    void Shader::Unbind() const { glUseProgram(0); }

    void Shader::SetBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(m_ID, name.c_str()), (int)value);
    }

    void Shader::SetInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(m_ID, name.c_str()), value);
    }

    void Shader::SetFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(m_ID, name.c_str()), value);
    }

    void Shader::SetVec2(const std::string& name, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(m_ID, name.c_str()), x, y);
    }

    void Shader::SetVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(m_ID, name.c_str()), x, y, z);
    }

    void Shader::SetVec4(const std::string& name,
        float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(m_ID, name.c_str()), x, y, z, w);
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(
            glGetUniformLocation(m_ID, name.c_str()),
            1, GL_FALSE, glm::value_ptr(mat)
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

    unsigned int Shader::CompileShader(unsigned int type,
        const std::string& src) const
    {
        unsigned int id = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(id, 1, &c, nullptr);
        glCompileShader(id);

        int success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[512];
            glGetShaderInfoLog(id, 512, nullptr, log);
            std::cerr << "[Shader] Compile error ("
                << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT")
                << "):\n" << log << std::endl;
        }

        return id;
    }

} // namespace MyEngine