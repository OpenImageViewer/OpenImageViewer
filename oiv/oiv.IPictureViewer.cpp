#include "PreCompiled.h"
#include "oiv.h"
#include "StringUtility.h"
#include "OgreHelper.h"
namespace OIV
{
    // IPictureViewr implementation
    int OIV::LoadFile(OIVCHAR* filePath)
    {
        std::string textureName = StringUtility::ToAString(filePath);
        
        bool isLoaded = false;

        TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(textureName);

        isLoaded |= texture.isNull() == false;


        if (isLoaded == false)
        {
            Image image;

            if (OgreHelper::OgreOpenImage(textureName, image))
            {
                OgreHelper::OgreLoadImageToTexture(image, textureName);
                this->fImageOpened = image;
                isLoaded = true;
            }
        }

        if (isLoaded == true)
        {
            fTextureName = textureName;
            fActiveTexture = Ogre::TextureManager::getSingleton().getByName(fTextureName);
            fPass->getTextureUnitState(0)->setTextureName(textureName);
            fScrollState.Refresh();
            return 0;
            
        }
        return 1;

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
        //HandleWindowResize();
        UpdateGpuParams();
        Ogre::Root::getSingleton().renderOneFrame();

        return 0;
    }


    int OIV::GetFileInformation(QryFileInformation& information)
    {
        if (IsImageLoaded())
        {
            information.bitsPerPixel = fImageOpened.getBPP();
            information.height = fImageOpened.getHeight();
            information.width = fImageOpened.getWidth();
            information.numMipMaps = fImageOpened.getNumMipmaps();
            information.rowPitchInBytes = fImageOpened.getRowSpan();
            information.hasTransparency = fImageOpened.getHasAlpha();
            information.imageDataSize = fImageOpened.getSize();
            information.numChannels = 0;
            return 0;
        }
        else
        {
            return 1;
        }
    }
    
}