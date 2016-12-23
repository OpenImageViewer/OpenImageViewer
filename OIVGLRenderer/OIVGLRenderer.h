#pragma once
#define GLEW_STATIC
#include <windows.h>
#include <GL/glew.h>
#include <GL/GL.h>
#include "GLRenderer/GLGpuProgram.h"
#include "GLRenderer/Quad.h"

#pragma comment(lib, "opengl32")
//#pragma comment(lib, "glu32")


#include "../../OpenImageViewer/OIViewer/win32/Win32Window.h"
#include "../../OpenImageViewer/OIViewer/win32/Win32Helper.h"
#include <functional>
#include "GLContext.h"
#include "../OIVUtil/FileHelper.h"
#include "OIVGLRenderer.h"
#include "GLRenderer/GLTexture.h"
#include <Interfaces/IRenderer.h>

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
        int Init(size_t container) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filtertype) override;
        int SetImage(const ImageSharedPtr image) override;
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
