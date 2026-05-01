#include "GLContext.h"

void GLContext::init(HWND hWnd)
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

    //int a = GetPixelFormat(mhDC);
    int format = ChoosePixelFormat(mhDC, &pfd);
    SetPixelFormat(mhDC, format, &pfd);

    // create the render context (RC)
    mhRC = wglCreateContext(mhDC);

    // make it the current render context
    wglMakeCurrent(mhDC, mhRC);

    initExtensions();
}
