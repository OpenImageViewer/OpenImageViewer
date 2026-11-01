#pragma once

#include <RendererCore/RendererOptions.h>
#include <LLUtils/Exception.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OIV
{
    enum class RendererErrorCode
    {
        DeviceLost,
        OutOfMemory,
        SurfaceUnavailable,
        BackendFailure
    };
    enum class RendererNativeDomain
    {
        D3D11,
        Vulkan,
        Win32,
        EGL,
        GLEW,
        OpenGL
    };

    struct RendererNativeError
    {
        RendererNativeDomain domain;
        std::int64_t value;
    };

    // A backend failure retains portable classification and the original graphics result.
    class RendererException : public LLUtils::Exception
    {
      public:

        RendererException(RendererErrorCode code, RendererType backend, std::string_view operation,
                          std::optional<RendererNativeError> native = std::nullopt, std::string_view detail = {});

        RendererErrorCode GetCode() const noexcept { return fCode; }
        RendererType GetBackend() const noexcept { return fBackend; }
        std::string_view GetOperation() const noexcept { return fOperation; }
        std::optional<RendererNativeError> GetNativeError() const noexcept { return fNative; }

      private:

        RendererErrorCode fCode;
        RendererType fBackend;
        std::string fOperation;
        std::optional<RendererNativeError> fNative;
    };
}  // namespace OIV
