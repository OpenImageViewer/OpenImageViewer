#pragma once

#include <OIVAppCore/CommandManager.h>

#include <OIVAppCore/ViewerImage.h>

#include <string>

namespace OIV
{
    class ShellCommandHandler
    {
      public:

        static LLUtils::native_string_type Execute(const CommandManager::CommandRequest& request,
                                                   const LLUtils::native_string_type& openedFileName,
                                                   const ViewerImage* openedImage);
    };
}  // namespace OIV
