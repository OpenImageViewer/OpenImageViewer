#pragma once
#include "Commands/CommandProcessor.h"

namespace OIV
{
    class IPictureRenderer;
    class ApiGlobal
    {
    public:
        static std::unique_ptr<IPictureRenderer> sPictureRenderer;
        static CommandProcessor sCommandProcessor;
    };
}

