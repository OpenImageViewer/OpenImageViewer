#include "PreCompiled.h"
#include "oiv.h"
#include "StringUtility.h"
namespace OIV
{
    // IPictureViewr implementation
    int OIV::LoadFile(OIVCHAR* filePath)
    {
        using namespace Ogre;
        std::string path = StringUtility::ToAString(filePath);

        //refactor out to unload
        if (fImage->IsOpened())
        {
            TextureManager::getSingleton().remove(fCurrentOpenedFile);
        }

        
        if (fImage != fImage32Bit)
        {
            if (fImage32Bit != NULL)
                fImage32Bit->Unload();
            delete fImage32Bit;

            fImage32Bit = NULL;
        }

        
            

        if (fImage->Load(path))
        {
            fCurrentOpenedFile = path;
            fImage32Bit = fImage->NeedConvertionToBYTERGBA() ? fImage->ConverToRGBA() : fImage;

            int width = fImage32Bit->GetWidth();
            int height = fImage32Bit->GetHeight();

                TexturePtr tex = TextureManager::getSingleton().createManual(
                    fCurrentOpenedFile
                    , ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
                    , TEX_TYPE_2D
                    , width// width
                    , height // height
                    , 1      // depth
                    , 0      // num mipmaps
                    , Ogre::PF_R8G8B8A8); // pixel format


           PixelBox src((uint32)width, (uint32)height, 1, PF_A8R8G8B8, fImage32Bit->GetBuffer());
           src.rowPitch = fImage32Bit->GetRowPitchInTexels();
           src.slicePitch = fImage32Bit->GetSlicePitchInTexels();
           Ogre::Image::Box dest(0, 0, width, height);
           HardwarePixelBufferSharedPtr buf = tex->getBuffer();
           buf->blitFromMemory(src, dest);


            fPass->getTextureUnitState(0)->setTextureName(path);
            fScrollState.Reset(true);
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

    Image* OIV::GetImage() 
    {
        return fImage;
    }

    int OIV::GetFileInformation(QryFileInformation& information)
    {
        if (IsImageLoaded())
        {
                    
            information.bitsPerPixel = fImage->GetBitsPerTexel();
            information.height = fImage->GetHeight();
            information.width = fImage->GetWidth();
            information.numMipMaps = 0;
            information.rowPitchInBytes = fImage->GetRowPitchInBytes();
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