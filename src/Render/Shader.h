#pragma once

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader();
    ~Shader();

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
    
    void use() const;
    void unuse() const;
    
    GLuint getProgram() const { return program; }
    
    // Uniform setters
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    GLuint program;
    
    GLuint compileShader(GLenum type, const std::string& source);
    bool linkProgram(GLuint vertShader, GLuint fragShader);
    std::string readFile(const std::string& path);
};
