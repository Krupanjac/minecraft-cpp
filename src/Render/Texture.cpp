#include "Texture.h"
#include "../Core/Logger.h"
#include <stb_image.h>

Texture::Texture(const std::string& path)
    : rendererID(0), filePath(path), localBuffer(nullptr), width(0), height(0), bpp(0) {
    
    stbi_set_flip_vertically_on_load(1);
    localBuffer = stbi_load(path.c_str(), &width, &height, &bpp, 4);

    if (localBuffer) {
        glGenTextures(1, &rendererID);
        glBindTexture(GL_TEXTURE_2D, rendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, localBuffer);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(localBuffer);
        localBuffer = nullptr;
        LOG_INFO("Loaded texture: " + path);
    } else {
        LOG_ERROR("Failed to load texture: " + path);
        if (stbi_failure_reason())
            LOG_ERROR("STB Reason: " + std::string(stbi_failure_reason()));
    }
}

Texture::Texture(const unsigned char* data, int w, int h, int channels)
    : rendererID(0), filePath("Embed"), localBuffer(nullptr), width(w), height(h), bpp(channels) {
    
    if (data) {
        glGenTextures(1, &rendererID);
        glBindTexture(GL_TEXTURE_2D, rendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLenum format = GL_RGBA;
        if (channels == 3) format = GL_RGB;
        else if (channels == 1) format = GL_RED;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, (channels == 3 ? GL_RGB8 : GL_RGBA8), width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Reset to default
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        LOG_INFO("Created texture from memory (" + std::to_string(w) + "x" + std::to_string(h) + ")");
    }
}

Texture::~Texture() {
    if (rendererID) glDeleteTextures(1, &rendererID);
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, rendererID);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}
