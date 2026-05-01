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
    std::wstring ShellCommandHandler::Execute(const CommandManager::CommandRequest& request,
                                              const std::wstring& openedFileName,
                                              OIVBaseImageSharedPtr openedImage)
    {
        const std::string command = request.args.GetArgValue("cmd");
        std::wstring result = LLUtils::StringUtility::ToWString(request.displayName);

        if (command == "newWindow")
        {
            ShellExecute(nullptr,
                         L"open",
                         LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExePath()).c_str(),
                         openedFileName.c_str(),
                         nullptr,
                         SW_SHOWDEFAULT);
        }
        else if (command == "openPhotoshop")
        {
            const std::wstring photoshopApplicationPath = PhotoShopFinder::FindPhotoShop();
            if (photoshopApplicationPath.empty() == false)
                ShellExecute(nullptr, L"open", photoshopApplicationPath.c_str(), openedFileName.c_str(), nullptr,
                             SW_SHOWDEFAULT);
        }
        else if (command == "openWithGoogleMaps")
        {
            if (openedImage != nullptr && openedImage->GetImage() != nullptr &&
                openedImage->GetMetaData()->exifData.latitude != std::numeric_limits<double>::max())
            {
                const auto& exifData = openedImage->GetMetaData()->exifData;
                std::wstringstream stream;
                stream << "https://www.google.com/maps/place/@" << exifData.latitude << "," << exifData.longitude
                       << ",1000m/data=!3m1!1e3";

                ShellExecute(nullptr, L"open", stream.str().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            }
            else
            {
                result = L"No geo location data found";
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
                             L"open",
                             LLUtils::StringUtility::ToNativeString(request.args.GetArgValue("app")).c_str(),
                             openedFileName.c_str(),
                             nullptr,
                             SW_SHOWDEFAULT);
        }

        return result;
    }
}
