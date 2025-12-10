#pragma once

#include <glad/glad.h>
#include <iostream>

class FrameBuffer {
public:
    FrameBuffer(int width, int height);
    ~FrameBuffer();

    void bind();
    void unbind();
    void resize(int width, int height);

    GLuint getTexture() const { return textureColorBuffer; }
    GLuint getDepthTexture() const { return depthTexture; }
    GLuint getID() const { return fbo; }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    GLuint fbo;
    GLuint textureColorBuffer;
    GLuint depthTexture; // Using texture for depth to allow sampling
    int width;
    int height;

    void init();
    void cleanup();
};
