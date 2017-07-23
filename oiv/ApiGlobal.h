#pragma once
#include "Interfaces/IPictureRenderer.h"
#include "Commands/CommandProcessor.h"

namespace OIV
{
    class ApiGlobal
    {
    public:
        static std::unique_ptr<IPictureRenderer> sPictureRenderer;
        static CommandProcessor sCommandProcessor;
    };
}

