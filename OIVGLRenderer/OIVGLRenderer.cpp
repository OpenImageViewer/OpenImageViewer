#include "OIVGLRenderer.h"
#include "GLRenderer/Quad.h"
namespace OIV
{
    OIVGLRenderer::OIVGLRenderer()
    {
        memset(&fUVScale, 0, sizeof(GLfloat) * 2);
        memset(&fUVffset, 0, sizeof(GLfloat) * 2);
        memset(&fImageSize, 0, sizeof(GLfloat) * 2);
        memset(&fViewportSize, 0, sizeof(GLfloat) * 2);
        GLint uShowGrid = 0;
        fIsParamsDirty = true;
    }


    void OIVGLRenderer::PrepareResources()
    {
        using namespace OIV;

        // Disable vsync
        context.SetSwapInterval(0);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        std::wstring vertexShaderPath = L"./Resources/programs/quad_vp.shader";
        std::wstring fragmentShaderPath = L"./Resources/programs/quad_fp.shader";

        fProgram = GLGpuProgramUniquePtr(new GLGpuProgram(
            LLUtils::File::ReadAllText(vertexShaderPath).c_str()
            , LLUtils::File::ReadAllText(fragmentShaderPath).c_str()));

        fProgram->Bind();
        fTexture = GLTextureUniquePtr(new GLTexture());

        Quad quad;
        GLint posAttrib = glGetAttribLocation(fProgram->GetProgram(), "position");
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(posAttrib);
        glClearColor(0.0f, 0.f, 0.f, 1.0f);
    }

    int OIVGLRenderer::Init(const OIV_RendererInitializationParams& initParams)
    {
        context.init(reinterpret_cast<HWND>(initParams.container));
        glewInit();
        PrepareResources();
        glClearColor(0.0f, 0.f, 0.f, 0.0f);
        return 0;
    }


    int OIVGLRenderer::SetViewParams(const ViewParameters& viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        fShowGrid = viewParams.showGrid ? 1 : 0;
        fIsParamsDirty = true;
        return 0;
    }

    int OIVGLRenderer::Redraw()
    {
        renderOneFrame();
        return 0;
    }

    int OIVGLRenderer::SetFilterLevel(OIV_Filter_type filterLevel)
    {
        //TODO: implement
        return 0;
    }

    int OIVGLRenderer::SetImageBuffer(uint32_t id, const IMCodec::ImageSharedPtr image) 
    {
        fImageSize[0] = static_cast<GLfloat>(image->GetWidth());
        fImageSize[1] = static_cast<GLfloat>(image->GetHeight());
        fIsParamsDirty = true;
        fTexture->SetRGBATexture(image->GetWidth(), image->GetHeight(), image->GetBuffer());
        return 0;
    }

    int OIVGLRenderer::SetSelectionRect(SelectionRect selectionRect)
    {
        return 0;
    }

    int OIVGLRenderer::SetExposure(const OIV_CMD_ColorExposure_Request & exposure)
    {
        return 0;
    }

    int OIVGLRenderer::SetImageProperties(const OIV_CMD_ImageProperties_Request &)
    {
        return 0;
    }

    int OIVGLRenderer::RemoveImage(uint32_t id)
    {
        return 0;
    }

    void OIVGLRenderer::renderOneFrame()
    {
        UpdateGpuParams();
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        context.swapBuffers();
    }

    void OIVGLRenderer::UpdateViewportSize(int x, int y)
    {
        glViewport(0, 0, x, y);
        fIsParamsDirty = true;
        fViewportSize[0] = static_cast<GLfloat>(x);
        fViewportSize[1] = static_cast<GLfloat>(y);
        fIsParamsDirty = true;
    }

    void OIVGLRenderer::UpdateGpuParams()
    {
        if (fIsParamsDirty == true)
        {
            fProgram->SetUniform2F("uvScale", fUVScale[0], fUVScale[1]);
            fProgram->SetUniform2F("uImageSize", fImageSize[0], fImageSize[1]);
            fProgram->SetUniform2F("uvOffset", fUVffset[0], fUVffset[1]);
            fProgram->SetUniform2F("uViewportSize", fViewportSize[0], fViewportSize[1]);
            fProgram->SetUniform1I("uShowGrid", fShowGrid);
            fIsParamsDirty = false;
        }
    }
}