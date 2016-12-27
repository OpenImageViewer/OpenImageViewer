#pragma once
#include "API\defs.h"
#pragma once
#include "Interfaces\IPictureRenderer.h"
#include <tchar.h>
namespace OIV
{
    class CommandProcessor
    {
    private:
        static std::unique_ptr<IPictureRenderer> sPictureRenderer;

    public:
        static ResultCode ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData);
        static void Log(OIVCHAR* message);
        static bool IsInitialized()
        {
            return sPictureRenderer.get() != nullptr;
        }

    };
}
