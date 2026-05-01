#pragma once
#include <string>
#include <windows.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/Exception.h>

namespace OIV
{
    class D3D11Error
    {
    public:
        static const bool fShowDeviceErrors = false;
        static void HandleError(std::string errorMessage)
        {
            if (fShowDeviceErrors)
                MessageBoxA(static_cast<HWND>(0), errorMessage.c_str(), "Error", MB_OK);
            else
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, errorMessage);
        }

        static void HandleDeviceError(HRESULT result)
        {
            HandleDeviceError(result, "Error");
        }

        static void HandleDeviceError(HRESULT result, const std::string& errorMessage)
        {
            if (SUCCEEDED(result) == false)
            {
                std::string message = "Direct3D11 could complete the operation.";
                if (errorMessage.empty() == false)
                    message += "\n" + errorMessage;
                std::string err = LLUtils::PlatformUtility::GetLastErrorAsString<char>();

                if (err.empty() == false)
                    message += "\n" + err;

                HandleError(message);
            }
        }

        static void HandleDeviceError(HRESULT result, std::string&& errorMessage)
        {
            HandleDeviceError(result, errorMessage);
        }

    };
}
