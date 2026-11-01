#include "GLContext.h"
#include <RendererCore/RendererException.h>

#include <GL/glew.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <algorithm>
#include <stdexcept>

namespace
{
    [[noreturn]] void ThrowEglFailure(const char* operation)
    {
        const EGLint error          = eglGetError();
        OIV::RendererErrorCode code = OIV::RendererErrorCode::BackendFailure;
        if (error == EGL_CONTEXT_LOST)
            code = OIV::RendererErrorCode::DeviceLost;
        else if (error == EGL_BAD_ALLOC)
            code = OIV::RendererErrorCode::OutOfMemory;
        else if (error == EGL_BAD_SURFACE)
            code = OIV::RendererErrorCode::SurfaceUnavailable;
        if (error == EGL_SUCCESS)
            throw OIV::RendererException(code, OIV::RendererType::OpenGL, operation);
        throw OIV::RendererException(code, OIV::RendererType::OpenGL, operation,
                                     OIV::RendererNativeError{OIV::RendererNativeDomain::EGL,
                                                              static_cast<std::int64_t>(error)});
    }
}  // namespace

class GLContext::NativeState
{
  public:

    ~NativeState()
    {
        if (eglDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eglContext != EGL_NO_CONTEXT)
                eglDestroyContext(eglDisplay, eglContext);
            if (eglSurface != EGL_NO_SURFACE)
                eglDestroySurface(eglDisplay, eglSurface);
        }
        if (eglWindow != nullptr)
            wl_egl_window_destroy(eglWindow);
        if (eglDisplay != EGL_NO_DISPLAY)
            eglTerminate(eglDisplay);
    }

    wl_egl_window* eglWindow{};
    EGLDisplay eglDisplay{EGL_NO_DISPLAY};
    EGLSurface eglSurface{EGL_NO_SURFACE};
    EGLContext eglContext{EGL_NO_CONTEXT};
};

GLContext::GLContext() = default;

GLContext::~GLContext()
{
    Purge();
}

void GLContext::Init(std::uintptr_t window, void* nativeDisplay)
{
    if (nativeDisplay == nullptr || window == 0)
        throw std::invalid_argument("Wayland OpenGL rendering requires a display and surface");

    Purge();
    auto state        = std::make_unique<NativeState>();
    state->eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, nativeDisplay, nullptr);
    EGLint majorVersion{};
    EGLint minorVersion{};
    if (state->eglDisplay == EGL_NO_DISPLAY ||
        eglInitialize(state->eglDisplay, &majorVersion, &minorVersion) == EGL_FALSE ||
        eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        ThrowEglFailure("Could not initialize EGL for Wayland OpenGL rendering");
    }

    constexpr EGLint configAttributes[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };
    EGLConfig config{};
    EGLint configCount{};
    if (eglChooseConfig(state->eglDisplay, configAttributes, &config, 1, &configCount) == EGL_FALSE || configCount == 0)
    {
        ThrowEglFailure("Could not select an EGL framebuffer configuration");
    }

    state->eglWindow = wl_egl_window_create(reinterpret_cast<wl_surface*>(window), 1, 1);
    if (state->eglWindow == nullptr)
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Create EGL window for Wayland surface");

    state->eglSurface = eglCreatePlatformWindowSurface(state->eglDisplay, config, state->eglWindow, nullptr);
    constexpr EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE,
    };
    state->eglContext = eglCreateContext(state->eglDisplay, config, EGL_NO_CONTEXT, contextAttributes);
    if (state->eglSurface == EGL_NO_SURFACE || state->eglContext == EGL_NO_CONTEXT ||
        eglMakeCurrent(state->eglDisplay, state->eglSurface, state->eglSurface, state->eglContext) == EGL_FALSE)
    {
        ThrowEglFailure("Could not create an EGL OpenGL context for the Wayland surface");
    }

    fNativeState = std::move(state);
}

void GLContext::Purge()
{
    fNativeState.reset();
}

void GLContext::Resize(int width, int height)
{
    if (fNativeState != nullptr)
        wl_egl_window_resize(fNativeState->eglWindow, std::max(width, 1), std::max(height, 1), 0, 0);
}

void GLContext::SetSwapInterval(int interval)
{
    if (fNativeState != nullptr)
        eglSwapInterval(fNativeState->eglDisplay, interval);
}

void GLContext::SwapBuffers() const
{
    if (fNativeState != nullptr)
        if (eglSwapBuffers(fNativeState->eglDisplay, fNativeState->eglSurface) == EGL_FALSE)
        {
            ThrowEglFailure("Swap buffers");
        }
}
