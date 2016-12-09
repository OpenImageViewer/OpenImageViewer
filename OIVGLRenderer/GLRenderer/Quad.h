#pragma once
#include <GL/glew.h>

class Quad
{
public:
    Quad()
    {
        static  GLfloat vertices[] =
        {
            -1,  1, // top left corner
            -1, -1, // bottom left corner
            1,  1, // top right corner
            1, -1, // bottom right corner
        };
        
        glGenBuffers(1, &fVbo); // Generate 1 buffer
        glBindBuffer(GL_ARRAY_BUFFER, fVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    }

    void Bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, fVbo);
    }

private:
    GLuint fVbo;
};
