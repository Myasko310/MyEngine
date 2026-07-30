#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace MyEngine {

    class Shader {
    public:
        Shader(const std::string& vertexPath,
            const std::string& fragmentPath);
        ~Shader();

        void Use()    const;
        void Bind()   const;
        void Unbind() const;

        void SetBool(const std::string& name, bool  value)  const;
        void SetInt(const std::string& name, int   value)  const;
        void SetFloat(const std::string& name, float value)  const;

        void SetVec2(const std::string& name, float x, float y)              const;
        void SetVec3(const std::string& name, float x, float y, float z)     const;
        void SetVec4(const std::string& name, float x, float y, float z, float w) const;

        void SetMat4(const std::string& name, const glm::mat4& mat) const;

    private:
        unsigned int m_ID = 0;

        std::string   LoadFile(const std::string& path) const;
        unsigned int  CompileShader(unsigned int type,
            const std::string& src) const;
    };

} // namespace MyEngine