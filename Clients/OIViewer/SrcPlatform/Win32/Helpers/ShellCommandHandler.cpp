#include "ShellCommandHandler.h"

#include "PhotoshopFinder.h"

#include <Windows.h>

#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <Win32/Win32Helper.h>

#include <limits>
#include <sstream>

namespace OIV
{
    LLUtils::native_string_type ShellCommandHandler::Execute(const CommandManager::CommandRequest& request,
                                              const LLUtils::native_string_type& openedFileName,
                                              OIVBaseImageSharedPtr openedImage)
    {
        const std::string command = request.args.GetArgValue("cmd");
        LLUtils::native_string_type result = LLUtils::StringUtility::ToWString(request.displayName);

        if (command == "newWindow")
        {
            ShellExecute(nullptr,
                         LLUTILS_TEXT("open"),
                         LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExePath()).c_str(),
                         openedFileName.c_str(),
                         nullptr,
                         SW_SHOWDEFAULT);
        }
        else if (command == "openPhotoshop")
        {
            const LLUtils::native_string_type photoshopApplicationPath = PhotoshopFinder::FindPhotoshop();
            if (photoshopApplicationPath.empty() == false)
                ShellExecute(nullptr, LLUTILS_TEXT("open"), photoshopApplicationPath.c_str(), openedFileName.c_str(), nullptr,
                             SW_SHOWDEFAULT);
        }
        else if (command == "openWithGoogleMaps")
        {
            if (openedImage != nullptr)
            {
                if (openedImage->GetImage() != nullptr &&
                   openedImage->GetMetaData() != nullptr &&
                    openedImage->GetMetaData()->exifData.latitude != std::numeric_limits<double>::max())
                {
                    const auto& exifData = openedImage->GetMetaData()->exifData;
                    LLUtils::native_stringstream stream;
                    stream << "https://www.google.com/maps/place/@" << exifData.latitude << "," << exifData.longitude
                           << ",300m/data=!3m1!1e3";

                    ShellExecute(nullptr, LLUTILS_TEXT("open"), stream.str().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
                }
                else
                {
                    result = LLUTILS_TEXT("No geo location data found");
                }
            }
        }
        else if (command == "containingFolder")
        {
            if (openedFileName.empty() == false)
                ::Win32::Win32Helper::BrowseToFile(openedFileName);
        }
        else if (command == "openWith")
        {
            if (openedFileName.empty() == false)
                ShellExecute(nullptr,
                             LLUTILS_TEXT("open"),
                             LLUtils::StringUtility::ToNativeString(request.args.GetArgValue("app")).c_str(),
                             openedFileName.c_str(),
                             nullptr,
                             SW_SHOWDEFAULT);
        }

        return result;
    }
}
