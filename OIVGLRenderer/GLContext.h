#pragma once
#include <windows.h>
#include <GL/GL.h>
class GLContext
{
public:

    GLContext()
    {
        reset();
    }

    ~GLContext()
    {
        purge();
    }

    void init(HWND hWnd)
    {
        // remember the window handle (HWND)
        mhWnd = hWnd;

        // get the device context (DC)
        mhDC = GetDC(mhWnd);

        // set the pixel format for the DC
        PIXELFORMATDESCRIPTOR pfd;
        ZeroMemory(&pfd, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
            PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 0;
        pfd.iLayerType = PFD_MAIN_PLANE;
        int format = ChoosePixelFormat(mhDC, &pfd);
        SetPixelFormat(mhDC, format, &pfd);

        // create the render context (RC)
        mhRC = wglCreateContext(mhDC);

        // make it the current render context
        wglMakeCurrent(mhDC, mhRC);
    }

    void purge()
    {
        if (mhRC)
        {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(mhRC);
        }
        if (mhWnd && mhDC)
        {
            ReleaseDC(mhWnd, mhDC);
        }
        reset();
    }

    void swapBuffers()
    {
        SwapBuffers(mhDC);
    }
private:

    void reset()
    {
        mhWnd = NULL;
        mhDC = NULL;
        mhRC = NULL;
    }

    HWND mhWnd;
    HDC mhDC;
    HGLRC mhRC;

};

