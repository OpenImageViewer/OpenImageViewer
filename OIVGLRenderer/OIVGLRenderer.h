#pragma once
#define GLEW_STATIC

#include <GL/glew.h>
#include <GL/GL.h>
#include "GLRenderer/GLGpuProgram.h"

#pragma comment(lib, "opengl32")
//#pragma comment(lib, "glu32")

#include <FileHelper.h>
#include <Image.h>
#include "GLContext.h"
#include "GLRenderer/GLTexture.h"
#include "../oiv/Interfaces/IRenderer.h"


namespace OIV
{
    class OIVGLRenderer : public IRenderer
    {
    public:

        OIVGLRenderer();
        //void TestRun();

    private:
        void UpdateGpuParams();
        void renderOneFrame();
        void UpdateViewportSize(int x, int y);
        //bool callback(const OIV::Win32::Event* evnt1);
        void PrepareResources();


    public:
        int Init(std::size_t container) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filtertype) override;
        int SetImage(const IMCodec::ImageSharedPtr image) override;
        int SetSelectionRect(SelectionRect selectionRect) override;
        
    private:
        bool fIsParamsDirty;
        GLGpuProgramUniquePtr fProgram;
        GLTextureUniquePtr fTexture;
        GLContext context;
        GLuint vao;
        GLfloat fUVScale[2];
        GLfloat fUVffset[2];
        GLfloat fImageSize[2];
        GLfloat fViewportSize[2];
        GLint fShowGrid;
    };
}
