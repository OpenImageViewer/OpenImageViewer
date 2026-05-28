#pragma once

#include <OIVAppCore/CommandManager.h>

#include <OIVImage/OIVBaseImage.h>

#include <string>

namespace OIV
{
    class ShellCommandHandler
    {
      public:
        static LLUtils::native_string_type Execute(const CommandManager::CommandRequest& request,
                                    const LLUtils::native_string_type& openedFileName,
                                    OIVBaseImageSharedPtr openedImage);
    };
}
