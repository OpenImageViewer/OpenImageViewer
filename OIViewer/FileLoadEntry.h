#pragma once
#include <string>
#include <iomanip>

namespace OIV
{
    class FileLoadEntry
    {
    public:
        std::wstring fileName;  
        OIV_CMD_LoadFile_Response loadResponse = {};
        LLUtils::StopWatch::time_type_real totalLoadTime;
        ImageHandle imageHandle = ImageNullHandle;

        std::wstring GetStringDescriptor() const
        {
            std::wstringstream ss;
            ss  << loadResponse.width << L" X " << loadResponse.height << L" X "
                << loadResponse.bpp << L" BPP | loaded in " << std::fixed << std::setprecision(1)
                << loadResponse.loadTime
                << L"/" << totalLoadTime << L" ms";

            return ss.str();

        }
    };
}

