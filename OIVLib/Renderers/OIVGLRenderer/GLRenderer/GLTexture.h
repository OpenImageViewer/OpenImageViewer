#pragma once

#include <GL/glew.h>
#include <RendererCore/RendererException.h>

#include <cstddef>

namespace OIV
{
    // Owned OpenGL texture whose upload returns only after error validation.
    class GLTexture
    {
      public:

        GLTexture()
        {
            glGenTextures(1, &fTexture);
            if (fTexture == 0)
            {
                const GLenum error = glGetError();
                const auto code    = error == GL_OUT_OF_MEMORY ? RendererErrorCode::OutOfMemory
                                                               : RendererErrorCode::BackendFailure;
                if (error != GL_NO_ERROR)
                    throw RendererException(code, RendererType::OpenGL, "Allocate OpenGL texture",
                                            RendererNativeError{RendererNativeDomain::OpenGL,
                                                                static_cast<std::int64_t>(error)});
                throw RendererException(code, RendererType::OpenGL, "Allocate OpenGL texture");
            }
            Bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        GLTexture(const GLTexture&)            = delete;
        GLTexture& operator=(const GLTexture&) = delete;

        ~GLTexture()
        {
            if (fTexture != 0)
                glDeleteTextures(1, &fTexture);
        }

        void Bind() const { glBindTexture(GL_TEXTURE_2D, fTexture); }

        void SetFilter(GLint filter) const
        {
            Bind();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        }

        void SetRGBATexture(std::size_t width, std::size_t height, const void* buffer) const
        {
            Bind();
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, buffer);
            // A failed upload must leave the backend revision unacknowledged for retry.
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR)
                throw RendererException(
                    error == GL_OUT_OF_MEMORY ? RendererErrorCode::OutOfMemory : RendererErrorCode::BackendFailure,
                    RendererType::OpenGL, "Upload OpenGL texture",
                    RendererNativeError{RendererNativeDomain::OpenGL, static_cast<std::int64_t>(error)});
        }

      private:

        // Owned texture name, zero only before successful allocation.
        GLuint fTexture{};
    };
}  // namespace OIV
