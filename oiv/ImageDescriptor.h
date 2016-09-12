#pragma once

#pragma once
#include "OgreCommon.h"
#include "libs/FreeImage.h"
namespace OIV
{
    class ImageDescriptor
    {
    public:
        class Properties
        {
        public:
            int width;
            int height;
            int rowPitch;
            int bitsPerPixel;

            size_t GetWidth() const { return width; }
            size_t GetHeight() const { return height; }
            size_t GetRowPitchInBytes() const { return rowPitch; }
            size_t GetRowPitchInTexels() const { return rowPitch / GetBytesPerTexel(); }
            size_t GetSlicePitchInBytes() const { return rowPitch * height; }
            size_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * height; }
            size_t GetTotalPixels() const { return width * height; }
            
            unsigned short GetBytesPerTexel() const
            {
                if (bitsPerPixel % 8 != 0)
                    throw std::exception("Only formats with multiples of 8 pixel depth are supported.");
                    return bitsPerPixel / 8;
            }

            
        };

    public:
        const Properties& GetImageProperties() const
        {
            return fProperties;
        }


        ~ImageDescriptor()
        {
            Unload();
        }
        ImageDescriptor()
        {
            fFreeImageHandle = NULL;
            fBuffer = NULL;
            fSourceFileFormat = Ogre::PF_UNKNOWN;
            memset(&fProperties, 0, sizeof(Properties));

        }

        void Unload()
        {
            if (fFreeImageHandle != NULL)
            {
                FreeImage_Unload(fFreeImageHandle);
                fFreeImageHandle = NULL;
                fBuffer = NULL;
                fSourceFileFormat = Ogre::PF_UNKNOWN;
                memset(&fProperties, 0, sizeof(Properties));
                Ogre::TextureManager::getSingleton().unload(fTexture->getName());
                fTexture.setNull();
                fFileName = Ogre::BLANKSTRING;
            }
        }

        bool OpenFile(const Ogre::String& texture_name)
        {
            if (IsFileOpened())
                Unload();

            return Open(texture_name) && LoadToGpu();
        }
        bool IsFileOpened() const
        {
            return fFreeImageHandle != NULL;
        }
    private: //private methods

        bool Open(const Ogre::String& texture_name)
        {
            using namespace Ogre;
            bool opened = false;
            FREE_IMAGE_FORMAT format = FreeImage_GetFileType(texture_name.c_str());

            if (format != FIF_UNKNOWN)
            {
                fFileName = texture_name;
                fFreeImageHandle = FreeImage_Load(format, texture_name.c_str());
                BITMAPINFO* imageInfo = FreeImage_GetInfo(fFreeImageHandle);
                const BITMAPINFOHEADER& header = imageInfo->bmiHeader;
                fProperties.width = header.biWidth;
                fProperties.height = header.biHeight;
                fProperties.bitsPerPixel = header.biBitCount;
                fProperties.rowPitch = FreeImage_GetPitch(fFreeImageHandle);
                fBuffer = FreeImage_GetBits(fFreeImageHandle);


                FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(fFreeImageHandle);
                opened = true;
                switch (imageType)
                {
                case FIT_BITMAP:
                    switch (fProperties.bitsPerPixel)
                    {
                    case 8:
                        fSourceFileFormat = PF_L8;
                        break;
                    case 16:
                        fSourceFileFormat = PF_R5G6B5;
                        break;
                    case 24:
                        fSourceFileFormat = PF_BYTE_RGB;
                        break;
                    case 32:
                        fSourceFileFormat = PF_BYTE_RGBA;
                        break;
                    }
                    break;
                default:
                    opened = false;

                }
            }
            return opened;
        }

        bool LoadToGpu()
        {
            using namespace Ogre;
            TexturePtr tex = TextureManager::getSingleton().createManual(
                fFileName
                , ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
                , TEX_TYPE_2D
                , fProperties.width  // width
                , fProperties.height // height
                , 1      // depth
                , 0      // num mipmaps
                , Ogre::PF_R8G8B8A8); // pixel format


            HardwarePixelBufferSharedPtr buf = tex->getBuffer();

            PixelBox src(fProperties.GetWidth(), fProperties.GetHeight(), 1, fSourceFileFormat, fBuffer);
            src.rowPitch = fProperties.GetRowPitchInTexels();
            src.slicePitch = fProperties.GetSlicePitchInTexels();
            Image::Box dest(0, 0, fProperties.width, fProperties.height);
            buf->blitFromMemory(src, dest);

            fTexture = tex;
            return true;
        }

         private: // private member fields
             Ogre::String fFileName;
             Properties fProperties;
             BYTE*       fBuffer;
             Ogre::PixelFormat fSourceFileFormat;
             Ogre::TexturePtr fTexture;
             FIBITMAP* fFreeImageHandle;

    };
}
