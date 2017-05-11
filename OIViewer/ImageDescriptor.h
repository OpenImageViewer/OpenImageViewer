#pragma once
#include <string>
#include <iomanip>

namespace OIV
{
     enum ImageSource
     {
           IS_None
         , IS_File
         , IS_Clipboard

     };
    class ImageDescriptor
    {
    public:
        ImageHandle imageHandle = ImageNullHandle;
        ImageSource source = IS_None;
        std::wstring fileName;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bpp = 0;
        double loadTime = 0.0;
        LLUtils::StopWatch::time_type_real displayTime = 0.0;
        

        const std::wstring& GetName() const
        {

            static std::wstring clipboard = L"[Clipboard]";

            if (source == IS_File)
                return  fileName;
            else
                return clipboard;
        }



        std::wstring GetDescription() const
        {
            std::wstringstream ss;
            ss  << width << L" X " << height << L" X "
                << bpp << L" BPP | loaded in " << std::fixed << std::setprecision(1)
                << loadTime
                << L"/" << displayTime + loadTime << L" ms";

            return ss.str();

        }
    };
}

