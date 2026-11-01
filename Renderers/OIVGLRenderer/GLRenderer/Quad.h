#pragma once

#include <GL/glew.h>

namespace OIV
{
    class Quad
    {
      public:

        Quad()
        {
            constexpr GLfloat vertices[] = {
                -1.0F, 1.0F,   // top left
                -1.0F, -1.0F,  // bottom left
                1.0F,  1.0F,   // top right
                1.0F,  -1.0F,  // bottom right
            };

            glGenBuffers(1, &fVertexBuffer);
            Bind();
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        }

        Quad(const Quad&)            = delete;
        Quad& operator=(const Quad&) = delete;

        ~Quad()
        {
            if (fVertexBuffer != 0)
                glDeleteBuffers(1, &fVertexBuffer);
        }

        void Bind() const { glBindBuffer(GL_ARRAY_BUFFER, fVertexBuffer); }

      private:

        GLuint fVertexBuffer{};
    };
}  // namespace OIV
