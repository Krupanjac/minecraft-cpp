#pragma once

#include "../Util/Types.h"
#include "Vertex.h"
#include <glad/glad.h>
#include <vector>

class Mesh {
public:
    Mesh();
    ~Mesh();

    void upload(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
    void bind() const;
    void unbind() const;
    void draw() const;
    
    bool isUploaded() const { return uploaded; }
    size_t getVertexCount() const { return vertexCount; }
    size_t getIndexCount() const { return indexCount; }

private:
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    size_t vertexCount;
    size_t indexCount;
    bool uploaded;
};
