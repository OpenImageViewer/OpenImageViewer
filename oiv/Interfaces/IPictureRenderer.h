#pragma once
#include "../API/defs.h"
#include <Windows.h>
namespace OIV
{
    class IPictureRenderer
    {
    public:
        virtual double Zoom(double percentage) = 0;
        virtual int LoadFile(OIVCHAR* filePath) = 0;
        virtual int Init() = 0;
        virtual int SetParent(HWND handle) = 0;
        virtual int Refresh() = 0;

        virtual int GetFileInformation(QryFileInformation& information) = 0;
    };
}