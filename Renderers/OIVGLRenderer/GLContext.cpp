#include "GLContext.h"
#include <RendererCore/RendererException.h>

#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include <cstring>

class GLContext::NativeState
{
  public:

    PFNWGLSWAPINTERVALEXTPROC swapInterval{};
    HWND window{};
    HDC deviceContext{};
    HGLRC renderContext{};
};

GLContext::GLContext() = default;

GLContext::~GLContext()
{
    Purge();
}

void GLContext::Init(std::uintptr_t window, [[maybe_unused]] void* nativeDisplay)
{
    Purge();
    fNativeState       = std::make_unique<NativeState>();
    NativeState& state = *fNativeState;

    state.window        = reinterpret_cast<HWND>(window);
    state.deviceContext = GetDC(state.window);
    if (state.deviceContext == nullptr)
    {
        Purge();
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not acquire an OpenGL device context",
                                     OIV::RendererNativeError{OIV::RendererNativeDomain::Win32,
                                                              static_cast<std::int64_t>(::GetLastError())});
    }

    PIXELFORMATDESCRIPTOR descriptor{
        .nSize      = static_cast<WORD>(sizeof(PIXELFORMATDESCRIPTOR)),
        .nVersion   = 1,
        .dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .iLayerType = PFD_MAIN_PLANE,
    };

    if (GetPixelFormat(state.deviceContext) == 0)
    {
        const int format = ChoosePixelFormat(state.deviceContext, &descriptor);
        if (format == 0 || SetPixelFormat(state.deviceContext, format, &descriptor) == FALSE)
        {
            Purge();
            throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                         "Could not select an OpenGL pixel format",
                                         OIV::RendererNativeError{OIV::RendererNativeDomain::Win32,
                                                                  static_cast<std::int64_t>(::GetLastError())});
        }
    }

    state.renderContext = wglCreateContext(state.deviceContext);
    if (state.renderContext == nullptr || wglMakeCurrent(state.deviceContext, state.renderContext) == FALSE)
    {
        Purge();
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not create an OpenGL rendering context",
                                     OIV::RendererNativeError{OIV::RendererNativeDomain::Win32,
                                                              static_cast<std::int64_t>(::GetLastError())});
    }

    const auto getExtensions = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGEXTPROC>(
        wglGetProcAddress("wglGetExtensionsStringEXT"));
    const char* extensions = getExtensions == nullptr ? nullptr : getExtensions();
    if (extensions != nullptr && std::strstr(extensions, "WGL_EXT_swap_control") != nullptr)
        state.swapInterval = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
}

void GLContext::Purge()
{
    if (fNativeState == nullptr)
        return;

    if (fNativeState->renderContext != nullptr)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(fNativeState->renderContext);
    }
    if (fNativeState->window != nullptr && fNativeState->deviceContext != nullptr)
        ReleaseDC(fNativeState->window, fNativeState->deviceContext);

    fNativeState.reset();
}

void GLContext::SetSwapInterval(int interval)
{
    if (fNativeState != nullptr && fNativeState->swapInterval != nullptr)
        fNativeState->swapInterval(interval);
}

void GLContext::Resize([[maybe_unused]] int width, [[maybe_unused]] int height) {}

void GLContext::SwapBuffers() const
{
    if (fNativeState != nullptr)
        if (::SwapBuffers(fNativeState->deviceContext) == FALSE)
        {
            const DWORD error = ::GetLastError();
            throw OIV::RendererException(
                OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL, "Swap buffers",
                OIV::RendererNativeError{OIV::RendererNativeDomain::Win32, static_cast<std::int64_t>(error)});
        }
}
