#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "precompiled.h"
#include "oiv.h"
#include "Plugins/PluginJpeg.h"
#include "Plugins/PluginPng.h"
#include "Plugins/PluginFreeImage.h"
#include "External\easyexif\exif.h"
#include "Interfaces\IRenderer.h"

#include "../OIVOgreRenderer/OgreRendererFactory.h"
namespace OIV
{
    OIV::OIV() :
          fScrollState(this)
        , fIsRefresing(false)
        , fParent(0)
        , fFilterLevel(2)
        , fShowGrid(false)
        , fClientWidth(-1)
        , fClientHeight(-1)

    {
        fImageLoader.InstallPlugin(new PluginJpeg());
        fImageLoader.InstallPlugin(new PluginPng());
        fImageLoader.InstallPlugin(new PluginFreeImage());

    }

    


    #pragma region ZoomScrollStateListener
    Vector2 OIV::GetImageSize()
    {
        return fOpenedImage.get() ? Vector2(fOpenedImage->GetWidth(), fOpenedImage->GetHeight())
            : Vector2::ZERO;
    }

    Vector2 OIV::GetClientSize()
    {
        return Vector2(fClientWidth, fClientHeight);
    }
    
    void OIV::NotifyDirty()
    {
        Refresh();
    }
    #pragma endregion 

   

    void OIV::UpdateGpuParams()
    {
        Vector2 uvScaleFixed = fScrollState.GetARFixedUVScale();
        Vector2 uvOffset = fScrollState.GetOffset();
        if (fOpenedImage.get() == nullptr)
            uvScaleFixed = Vector2(1000000, 100000);
        
        fViewParams.showGrid = fShowGrid;
        fViewParams.uViewportSize = GetClientSize();
        fViewParams.uvOffset = uvOffset;
        fViewParams.uvscale = uvScaleFixed;
        fViewParams.uImageSize = GetImageSize();
        
        fRenderer->SetViewParams(fViewParams);
    }

    void OIV::HandleWindowResize()
    {
        if (IsImageLoaded())
            fScrollState.Refresh();
    }

    bool OIV::IsImageLoaded() const
    {
        return fOpenedImage.get() != nullptr;
    }

    AxisAlignedRTransform OIV::ResolveExifRotation(unsigned short exifRotation) const
    {
        AxisAlignedRTransform rotation;
            switch (exifRotation)
            {
            case 3:
                rotation = AAT_Rotate180;
                break;
            case 6:
                rotation = AAT_Rotate90CW;
                break;
            case 8:
                rotation = AAT_Rotate90CCW;
                break;
            default:
                rotation = AAT_None;
            }
            return rotation;
    }

#pragma  region IPictureViewer implementation
    // IPictureViewr implementation
    int OIV::LoadFile(void* buffer, size_t size, char* extension, bool onlyRegisteredExtension)
    {

        ImageSharedPtr image = ImageSharedPtr(fImageLoader.LoadImage(buffer, size, extension, onlyRegisteredExtension));

        if (image.get())
        {

            fOpenedImage.swap(image);

            using namespace easyexif;
            EXIFInfo exifInfo;

            if (exifInfo.parseFrom(static_cast<const unsigned char*>(buffer), size) == PARSE_EXIF_SUCCESS)
                fOpenedImage->Transform(ResolveExifRotation(exifInfo.Orientation));


            if (fRenderer->SetImage(fOpenedImage) == RC_Success)
            {
                fScrollState.Reset(true);
                return RC_Success;
            }
        }

        return RC_UknownError;
    }

    double OIV::Zoom(double percentage, int x, int y)
    {
        fScrollState.Zoom(percentage, x, y);
        return 0.0;
    }

    int OIV::Pan(double x, double y)
    {
        fScrollState.Pan(Vector2(x, y));
        return 0.0;
    }

    int OIV::Init()
    {
        fRenderer = OgreRendererFactory::Create();
        fRenderer->Init(fParent);
        return 0;
    }


    int OIV::SetParent(size_t handle)
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
            fRenderer->Redraw();
            fIsRefresing = false;
        }

        return 0;
    }

    Image* OIV::GetImage()
    {
        return fOpenedImage.get();
    }

    int OIV::SetFilterLevel(int filter_level)
    {
        int desiredFilterLevel = filter_level;
        if (desiredFilterLevel >= 0 && desiredFilterLevel <= 1)
        {
            fFilterLevel = desiredFilterLevel;
            fRenderer->SetFilterLevel(fFilterLevel);
            Refresh();
            return RC_Success;
        }

        return RC_WrongParameters;
    }

    int OIV::GetFileInformation(QryFileInformation& information)
    {
        if (IsImageLoaded())
        {

            information.bitsPerPixel = fOpenedImage->GetBitsPerTexel();
            information.height = fOpenedImage->GetHeight();
            information.width = fOpenedImage->GetWidth();
            information.numMipMaps = 0;
            information.rowPitchInBytes = fOpenedImage->GetRowPitchInBytes();
            information.hasTransparency = 1;
            information.imageDataSize = 0;
            information.numChannels = 0;

            return RC_Success;
        }
        else
        {
            return 1;
        }
    }

    int OIV::GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY)
    {
        Vector2 texelPos = this->fScrollState.ClientPosToTexel(Vector2(mouseX, mouseY));
        texelX = texelPos.x;
        texelY = texelPos.y;
        return RC_Success;
    }

    int OIV::SetTexelGrid(double gridSize)
    {
        fShowGrid = gridSize > 0.0;
        Refresh();
        return RC_Success;
    }

    int OIV::GetNumTexelsInCanvas(double &x, double &y)
    {
        Vector2 canvasSize = fScrollState.GetNumTexelsInCanvas();
        x = canvasSize.x;
        y = canvasSize.y;
        return RC_Success;
    }

    int OIV::SetClientSize(uint16_t width, uint16_t height)
    {
        fClientWidth = width;
        fClientHeight = height;
        Refresh();
        return 0;
    }

#pragma endregion

}
