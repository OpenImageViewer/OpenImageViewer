#include "OIVGLRenderer.h"

OIV::OIVGLRenderer::OIVGLRenderer()
{
    memset(&fUVScale, 0, sizeof(GLfloat) * 2);
    memset(&fUVffset, 0, sizeof(GLfloat) * 2);
    memset(&fImageSize, 0, sizeof(GLfloat) * 2);
    memset(&fViewportSize, 0, sizeof(GLfloat) * 2);
    GLint uShowGrid = 0;
    fIsParamsDirty = true;
}


void OIV::OIVGLRenderer::PrepareResources()
{
    using namespace OIV;
    
    // Disable vsync
    context.SetSwapInterval(0);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    std::wstring vertexShaderPath = L"./Resources/programs/quad_vp.shader";
    std::wstring fragmentShaderPath = L"./Resources/programs/quad_fp.shader";

    fProgram = GLGpuProgramUniquePtr(new GLGpuProgram(
        File::ReadAllText(vertexShaderPath).c_str()
        , File::ReadAllText(fragmentShaderPath).c_str()));

    fProgram->Bind();
    fTexture = GLTextureUniquePtr(new GLTexture());

    Quad quad;
    GLint posAttrib = glGetAttribLocation(fProgram->GetProgram(), "position");
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posAttrib);
    glClearColor(0.0f, 0.f, 0.f, 1.0f);
}

int OIV::OIVGLRenderer::Init(size_t container)
{
    context.init(reinterpret_cast<HWND>(container));
    glewInit();
    PrepareResources();
    glClearColor(0.0f, 0.f, 0.f, 0.0f);
    return 0;
}


int OIV::OIVGLRenderer::SetViewParams(const ViewParameters& viewParams)
{
    fUVScale[0] = viewParams.uvscale.x;
    fUVScale[1] = viewParams.uvscale.y;

    fUVffset[0] = viewParams.uvOffset.x;
    fUVffset[1] = viewParams.uvOffset.y;

    UpdateViewportSize(viewParams.uViewportSize.x, viewParams.uViewportSize.y);

    fImageSize[0] = viewParams.uImageSize.x;
    fImageSize[1] = viewParams.uImageSize.y;

    fShowGrid = viewParams.showGrid ? 1 : 0;
    
    fIsParamsDirty = true;
    renderOneFrame();
    return 0;
}

int OIV::OIVGLRenderer::Redraw()
{
    renderOneFrame();
    return 0;
}

int OIV::OIVGLRenderer::SetFilterLevel(int filterLevel)
{
    //TODO: implement
    return 0;
}

int OIV::OIVGLRenderer::SetImage(const ImageSharedPtr image)
{
    fTexture->SetRGBATexture(image->GetWidth(), image->GetHeight(), image->GetBuffer());
    renderOneFrame();
    return 0;
}

void OIV::OIVGLRenderer::renderOneFrame()
{
    UpdateGpuParams();
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    context.swapBuffers();
}

void OIV::OIVGLRenderer::UpdateViewportSize(int x, int y)
{
    glViewport(0, 0, x, y);
    fIsParamsDirty = true;
    fViewportSize[0] = x;
    fViewportSize[1] = y;
    fIsParamsDirty = true;
}

void OIV::OIVGLRenderer::UpdateGpuParams()
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
