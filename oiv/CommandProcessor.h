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
        static IPictureRenderer* sPictureRenderer;

    public:
        static ResultCode ProcessCommand(CommandExecute command, size_t commandSize, void* commandData);
        static ResultCode ProcessQuery(CommandQuery command, void* commandData, size_t commandSize, void* output_data, size_t output_size);
        static void Log(OIVCHAR* message);
        static bool IsInitialized()
        {
            return sPictureRenderer != NULL;
        }

    };
}
