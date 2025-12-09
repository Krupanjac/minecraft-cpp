#include "Texture.h"
#include "../Core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture::Texture(const std::string& path)
    : rendererID(0), filePath(path), localBuffer(nullptr), width(0), height(0), bpp(0) {
    
    // Minecraft textures are usually pixel art, so we want nearest neighbor filtering
    // Also, we want to flip vertically because OpenGL expects 0,0 at bottom left
    stbi_set_flip_vertically_on_load(1);
    
    localBuffer = stbi_load(path.c_str(), &width, &height, &bpp, 4);

    if (localBuffer) {
        glGenTextures(1, &rendererID);
        glBindTexture(GL_TEXTURE_2D, rendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, localBuffer);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(localBuffer);
        LOG_INFO("Loaded texture: " + path);
    } else {
        LOG_ERROR("Failed to load texture: " + path);
        LOG_ERROR("STB Reason: " + std::string(stbi_failure_reason()));
    }
}

Texture::~Texture() {
    glDeleteTextures(1, &rendererID);
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, rendererID);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}
