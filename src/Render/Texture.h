#pragma once
#include <string>
#include <glad/glad.h>

class Texture {
public:
    Texture(const std::string& path);
    ~Texture();

    void bind(unsigned int slot = 0) const;
    void unbind() const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    unsigned int rendererID;
    std::string filePath;
    unsigned char* localBuffer;
    int width, height, bpp;
};
