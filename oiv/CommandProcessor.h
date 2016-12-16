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
        static ResultCode ProcessCommand(CommandExecute command, size_t requestSize, void* requestData, size_t responseSize, void* responseData);
        static void Log(OIVCHAR* message);
        static bool IsInitialized()
        {
            return sPictureRenderer.get() != nullptr;
        }

    };
}
