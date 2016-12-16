#pragma once
#include "../API/defs.h"
#include <Image/Image.h>
namespace OIV
{
    class IPictureRenderer
    {
    public:
        virtual double Zoom(double percentage,int x, int y) = 0;
        virtual int Pan(double x, double y) = 0;
        virtual int LoadFile(void* buffer, size_t size, char* extension, bool onlyRegisteredExtension) = 0;
        virtual int Init() = 0;
        virtual int SetParent(size_t handle) = 0;
        virtual int Refresh() = 0;

        virtual int GetFileInformation(QryFileInformation& information) = 0;

        virtual Image* GetImage() = 0;
        virtual int SetFilterLevel(int filter_level) = 0;
        virtual int GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY) = 0;
        virtual int SetTexelGrid(double gridSize) = 0;
        virtual int GetNumTexelsInCanvas(double &x, double &y) = 0;
        virtual int SetClientSize(uint16_t width, uint16_t height) = 0;
        virtual ~IPictureRenderer() {};
    };
}