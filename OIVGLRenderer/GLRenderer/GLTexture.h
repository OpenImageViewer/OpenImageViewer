#pragma once
#include <GL/glew.h>

class GLTexture
{
public:
    GLTexture()
    {
        // Create texture
        
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        // Texture address mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        // Border color
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
        //Filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //Load texels into textur object
        /*std::ifstream file("d:/pixels.raw", std::ios::in | std::ios::binary);
        std::size_t imageSize = 800 * 600 * 4;
        char* buffer = new char[imageSize];
        file.read(buffer, imageSize);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);*/

    }

    void SetRGBATexture(std::size_t width, std::size_t height, const void *buffer)
    {
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        
    }
private:
    GLuint tex;

};

typedef std::unique_ptr<GLTexture> GLTextureUniquePtr;
