#pragma once

#include <oivappcore/CommandManager.h>

#include <OIVImage/OIVBaseImage.h>

#include <string>

namespace OIV
{
    class ShellCommandHandler
    {
      public:
        static std::wstring Execute(const CommandManager::CommandRequest& request,
                                    const std::wstring& openedFileName,
                                    OIVBaseImageSharedPtr openedImage);
    };
}
