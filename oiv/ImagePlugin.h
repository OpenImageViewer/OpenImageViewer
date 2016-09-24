#pragma once
#include <string>
#include "ImageProperties.h"
namespace OIV
{
    class ImagePlugin
    {
    public:
        virtual bool LoadImage(std::string filePath, ImageProperies& out_properties) = 0;
        virtual char* GetHint() = 0;
    };
}
