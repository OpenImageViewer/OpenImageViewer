#include <RendererCore/RendererException.h>
#include <format>

namespace OIV
{
    namespace
    {
        constexpr std::string_view BackendName(RendererType backend)
        {
            switch (backend)
            {
                case RendererType::D3D11:
                    return "D3D11";
                case RendererType::Vulkan:
                    return "Vulkan";
                case RendererType::OpenGL:
                    return "OpenGL";
                case RendererType::Null:
                    return "Null";
            }
            return "Renderer";
        }

        constexpr std::string_view DomainName(RendererNativeDomain domain)
        {
            switch (domain)
            {
                case RendererNativeDomain::D3D11:
                    return "HRESULT";
                case RendererNativeDomain::Vulkan:
                    return "VkResult";
                case RendererNativeDomain::Win32:
                    return "Win32";
                case RendererNativeDomain::EGL:
                    return "EGL";
                case RendererNativeDomain::GLEW:
                    return "GLEW";
                case RendererNativeDomain::OpenGL:
                    return "OpenGL";
            }
            return "Native code";
        }

        std::string Describe(RendererType backend, std::string_view operation,
                             std::optional<RendererNativeError> native, std::string_view detail)
        {
            std::string description = std::format("{} {} failed", BackendName(backend), operation);
            if (native)
                description += std::format(" ({}: {})", DomainName(native->domain), native->value);
            if (!detail.empty())
                description += std::format(": {}", detail);
            return description;
        }
    }  // namespace

    RendererException::RendererException(RendererErrorCode code, RendererType backend, std::string_view operation,
                                         std::optional<RendererNativeError> native, std::string_view detail)
        : LLUtils::Exception(LLUtils::Exception::ErrorCode::RuntimeError, "Renderer",
                             Describe(backend, operation, native, detail), false, LLUtils::Exception::Mode::Exception),
          fCode(code), fBackend(backend), fOperation(operation), fNative(native)
    {
    }
}  // namespace OIV
