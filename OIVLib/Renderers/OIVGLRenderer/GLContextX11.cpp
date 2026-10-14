#include "GLContext.h"
#include <RendererCore/RendererException.h>

#include <GL/glew.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstring>
#include <stdexcept>

class GLContext::NativeState
{
  public:

    using SwapIntervalProc = void (*)(Display*, GLXDrawable, int);

    ~NativeState()
    {
        if (display != nullptr && renderContext != nullptr)
        {
            glXMakeCurrent(display, None, nullptr);
            glXDestroyContext(display, renderContext);
        }
        if (display != nullptr)
            XCloseDisplay(display);
    }

    Display* display{};
    ::Window window{};
    GLXContext renderContext{};
    SwapIntervalProc swapInterval{};
};

GLContext::GLContext() = default;

GLContext::~GLContext()
{
    Purge();
}

void GLContext::Init(std::uintptr_t window, [[maybe_unused]] void* nativeDisplay)
{
    if (window == 0)
        throw std::invalid_argument("Cannot create an OpenGL context for an empty X11 window");

    Purge();
    auto state     = std::make_unique<NativeState>();
    state->display = XOpenDisplay(nullptr);
    state->window  = static_cast<::Window>(window);
    if (state->display == nullptr)
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not open the X11 display for OpenGL rendering");

    XWindowAttributes attributes{};
    if (XGetWindowAttributes(state->display, state->window, &attributes) == 0)
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not query the X11 window for OpenGL rendering");

    XVisualInfo visualTemplate{.visualid = XVisualIDFromVisual(attributes.visual)};
    int visualCount{};
    XVisualInfo* visualInfo = XGetVisualInfo(state->display, VisualIDMask, &visualTemplate, &visualCount);
    if (visualInfo == nullptr || visualCount == 0)
    {
        if (visualInfo != nullptr)
            XFree(visualInfo);
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not resolve the X11 window visual for OpenGL rendering");
    }

    int supportsOpenGL{};
    const bool compatibleVisual = glXGetConfig(state->display, visualInfo, GLX_USE_GL, &supportsOpenGL) == 0 &&
                                  supportsOpenGL != 0;
    if (compatibleVisual)
        state->renderContext = glXCreateContext(state->display, visualInfo, nullptr, True);
    XFree(visualInfo);

    if (!compatibleVisual || state->renderContext == nullptr ||
        glXMakeCurrent(state->display, state->window, state->renderContext) == False)
    {
        throw OIV::RendererException(OIV::RendererErrorCode::BackendFailure, OIV::RendererType::OpenGL,
                                     "Could not create an OpenGL context for the X11 window");
    }

    const char* extensions = glXQueryExtensionsString(state->display, DefaultScreen(state->display));
    if (extensions != nullptr && std::strstr(extensions, "GLX_EXT_swap_control") != nullptr)
    {
        state->swapInterval = reinterpret_cast<NativeState::SwapIntervalProc>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
    }
    fNativeState = std::move(state);
}

void GLContext::Purge()
{
    fNativeState.reset();
}

void GLContext::Resize([[maybe_unused]] int width, [[maybe_unused]] int height) {}

void GLContext::SetSwapInterval(int interval)
{
    if (fNativeState != nullptr && fNativeState->swapInterval != nullptr)
        fNativeState->swapInterval(fNativeState->display, fNativeState->window, interval);
}

void GLContext::SwapBuffers() const
{
    if (fNativeState != nullptr)
        glXSwapBuffers(fNativeState->display, fNativeState->window);
}
