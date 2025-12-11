#pragma once

#include <glad/glad.h>
#include <iostream>

class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    bool init(unsigned int width, unsigned int height);
    void bind();
    void unbind();
    
    GLuint getDepthMap() const { return depthMap; }
    unsigned int getWidth() const { return width; }
    unsigned int getHeight() const { return height; }

private:
    GLuint fbo;
    GLuint depthMap;
    unsigned int width;
    unsigned int height;
};
