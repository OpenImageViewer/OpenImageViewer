#include "PreCompiled.h"
#include "oiv.h"
#include "StringUtility.h"
namespace OIV
{
    // IPictureViewr implementation
    int OIV::LoadFile(OIVCHAR* filePath)
    {
        std::string path = StringUtility::ToAString(filePath);
        if (fImageDescriptor.OpenFile(path))
        {
            fPass->getTextureUnitState(0)->setTextureName(path);
            fScrollState.Refresh();
            return true;
        }
        return false;
    }

    double OIV::Zoom(double percentage)
    {
        fScrollState.Zoom(percentage);
        return 0.0;
    }

    int OIV::Pan(double x, double y)
    {
        fScrollState.Pan(Ogre::Vector2(x, y));
        return 0.0;
    }

    int OIV::Init()
    {
        InitAll();
        return 0;
    }
    

    int OIV::SetParent(HWND handle)
    {
        fParent = handle;
        return 0;
    }
    int OIV::Refresh()
    {
        if (fIsRefresing == false)
        {
            fIsRefresing = true;
            HandleWindowResize();
            UpdateGpuParams();
            Ogre::Root::getSingleton().renderOneFrame();
            fIsRefresing = false;
        }

        return 0;
    }

    int OIV::GetFileInformation(QryFileInformation& information)
    {
        if (IsImageLoaded())
        {
            const ImageDescriptor::Properties& props = fImageDescriptor.GetImageProperties();
            information.bitsPerPixel = props.bitsPerPixel;
            information.height = props.height;
            information.width = props.width;
            information.numMipMaps = 0;
            information.rowPitchInBytes = props.rowPitch;
            information.hasTransparency = 1;
            information.imageDataSize = 0;
            information.numChannels = 0;
            return 0;
        }
        else
        {
            return 1;
        }
    }
    
}