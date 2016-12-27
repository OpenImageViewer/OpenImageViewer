#pragma once
#include <windows.h>
#include <GL/GL.h>
#include "GLRenderer/Win32/wglext.h"


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

    void init(HWND hWnd);

    void initExtensions()
    {
        if (WGLExtensionSupported("WGL_EXT_swap_control"))
        {
            // Extension is supported, init pointers.
            wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));

            // this is another function from WGL_EXT_swap_control extension
            wglGetSwapIntervalEXT = reinterpret_cast<PFNWGLGETSWAPINTERVALEXTPROC>(wglGetProcAddress("wglGetSwapIntervalEXT"));
        }
    }


    bool WGLExtensionSupported(const char *extension_name)
    {
        // this is pointer to function which returns pointer to string with list of all wgl extensions
        PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglGetExtensionsStringEXT = nullptr;

        // determine pointer to wglGetExtensionsStringEXT function
        _wglGetExtensionsStringEXT = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGEXTPROC>(wglGetProcAddress("wglGetExtensionsStringEXT"));

        if (strstr(_wglGetExtensionsStringEXT(), extension_name) == nullptr)
        {
            // string was not found
            return false;
        }

        // extension is supported
        return true;
    }

    

    void purge()
    {
        if (mhRC)
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(mhRC);
        }
        if (mhWnd && mhDC)
        {
            ReleaseDC(mhWnd, mhDC);
        }
        reset();
    }

    

    void SetSwapInterval(int interval)
    {
        if (wglSwapIntervalEXT != nullptr)
            wglSwapIntervalEXT(interval);
    }

    void swapBuffers()
    {
        SwapBuffers(mhDC);
    }
private:

    void reset()
    {
        mhWnd = nullptr;
        mhDC = nullptr;
        mhRC = nullptr;
        wglSwapIntervalEXT = nullptr;
        wglGetSwapIntervalEXT = nullptr;
    }

    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
    PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT;

  


    HWND mhWnd;
    HDC mhDC;
    HGLRC mhRC;

};

