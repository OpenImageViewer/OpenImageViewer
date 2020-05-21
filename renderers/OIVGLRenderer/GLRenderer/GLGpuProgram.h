#pragma once
#include <GL/glew.h>
#include <string>
#include "GlError.h"

class GLGpuProgram
{
    GLuint fFragmentShader;
    GLuint fVertexShader;
    GLuint fShaderProgram;
public:

    GLuint CreateShader(const char* shadersource, GLint type)
    {
        GLuint shader = 0;
        
        if (OIV::GL_SUCCESS(shader = glCreateShader(type)) == false)
        {
            throw std::exception((std::string("could not compile shader, reason:\n")).c_str());
        }

        //TODO: check all shader source has been uploaded
        glShaderSource(shader, 1, &shadersource, nullptr);
        glCompileShader(shader);
        if (OIV::IsShaderCompiled(shader) == false)
        {
            std::string error = OIV::GetShaderCompileError(shader);
            throw std::exception((std::string("could not compile shader, reason:\n") + error).c_str());
        }

        return shader;
    }

    void Bind() const
    {
        glUseProgram(fShaderProgram);
    }

    void SetUniform1I(std::string name, int i)
    {
        GLint location = glGetUniformLocation(fShaderProgram, name.c_str());
        if (location != -1)
        {
            glUniform1i(location, i);
        }
    }

   void SetUniform2F(std::string name, float f1, float f2)
    {
        GLint location = glGetUniformLocation(fShaderProgram, name.c_str());
        if (location != -1)
        {
            glUniform2f(location, f1, f2);
        }
    }


    GLGpuProgram(std::string vertexShaderSource, std::string fragmentShaderSource)
    {
        if (vertexShaderSource.empty() || fragmentShaderSource.empty())
            throw std::exception("Shaders contents is incomplete");
        

        const std::string prefix = "#version 150\n#define GLSL 1\n";
        fFragmentShader = 0;
        fVertexShader = 0;
        fShaderProgram = 0;
        vertexShaderSource = prefix + vertexShaderSource;
        fragmentShaderSource = prefix + fragmentShaderSource;
        fVertexShader = CreateShader(vertexShaderSource.c_str(), GL_VERTEX_SHADER);
        fFragmentShader = CreateShader(fragmentShaderSource.c_str(), GL_FRAGMENT_SHADER);
        fShaderProgram = glCreateProgram();
        glAttachShader(fShaderProgram, fVertexShader);
        glAttachShader(fShaderProgram, fFragmentShader);
        glBindFragDataLocation(fShaderProgram, 0, "outColor");

        glLinkProgram(fShaderProgram);
    }

    GLuint GetProgram() const
    {
        return fShaderProgram;
    }


};
typedef std::unique_ptr<GLGpuProgram> GLGpuProgramUniquePtr;

