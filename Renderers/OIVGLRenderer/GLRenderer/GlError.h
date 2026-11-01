#pragma once

#include <GL/glew.h>

#include <memory>
#include <string>
#include <vector>

namespace OIV
{
    inline bool IsShaderCompiled(GLuint shader)
    {
        GLint result{};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
        return result == GL_TRUE;
    }

    inline std::string GetShaderCompileError(GLuint shader)
    {
        GLint length{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length <= 1)
            return {};

        std::vector<char> buffer(static_cast<size_t>(length));
        glGetShaderInfoLog(shader, length, nullptr, buffer.data());
        return buffer.data();
    }

    inline std::string GetProgramLinkError(GLuint program)
    {
        GLint length{};
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length <= 1)
            return {};

        std::vector<char> buffer(static_cast<size_t>(length));
        glGetProgramInfoLog(program, length, nullptr, buffer.data());
        return buffer.data();
    }
}  // namespace OIV
