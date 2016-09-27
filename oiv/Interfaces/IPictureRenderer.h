#pragma once
#include "../API/defs.h"
#include <Windows.h>
#include "../ImageAbstract.h"
namespace OIV
{
    class IPictureRenderer
    {
    public:
        virtual double Zoom(double percentage) = 0;
        virtual int Pan(double x, double y) = 0;
        virtual int LoadFile(OIVCHAR* filePath, bool onlyRegisteredExtension) = 0;
        virtual int Init() = 0;
        virtual int SetParent(HWND handle) = 0;
        virtual int Refresh() = 0;

        virtual int GetFileInformation(QryFileInformation& information) = 0;

        virtual Image* GetImage() = 0;
        virtual int SetFilterLevel(int filter_level) = 0;
    };
}