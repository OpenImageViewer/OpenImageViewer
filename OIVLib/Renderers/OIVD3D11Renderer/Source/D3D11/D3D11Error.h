#pragma once
#include <RendererCore/RendererException.h>
#include <windows.h>
#include <string_view>

namespace OIV
{
    class D3D11Error
    {
      public:

        static void HandleError(std::string_view operation)
        {
            throw RendererException(RendererErrorCode::BackendFailure, RendererType::D3D11, operation);
        }

        static RendererErrorCode Classify(HRESULT result) noexcept
        {
            RendererErrorCode code = RendererErrorCode::BackendFailure;
            if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
                result == DXGI_ERROR_DEVICE_HUNG)
                code = RendererErrorCode::DeviceLost;
            else if (result == E_OUTOFMEMORY)
                code = RendererErrorCode::OutOfMemory;
            return code;
        }

        static void HandleDeviceError(HRESULT result, std::string_view operation = "Direct3D operation")
        {
            if (FAILED(result))
                throw RendererException(Classify(result), RendererType::D3D11, operation,
                                        RendererNativeError{RendererNativeDomain::D3D11,
                                                            static_cast<std::int64_t>(result)});
        }
    };
}  // namespace OIV
