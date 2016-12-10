#pragma once
#include <string>
#include <GL/glew.h>
#include <memory>

namespace OIV
{
    //static GLSuccees
    static bool IsShaderCompiled(GLuint shader)
    {
        GLint result;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
        return result == GL_TRUE;
    }
    static std::string GetShaderCompileError(GLuint shader)
    {
        std::unique_ptr<char> buffer;
        GLint length = -1;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length != -1)
        {
            buffer = std::unique_ptr<char>(new char[length]);
            glGetShaderInfoLog(shader, length, nullptr, buffer.get());
            return std::string(buffer.get());
        }

        return std::string();
    }

    static bool GL_SUCCESS(GLuint result)
    {
        return result != 0;
    }
}
