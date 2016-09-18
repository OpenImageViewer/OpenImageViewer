#pragma once
#include "ImageAbstract.h"
namespace OIV
{
    class ImageFreeImage : public Image
    {
    private:
        int fWidth;
        int fHeight;
        int fRowPitch;
        int fBitsPerPixel;
        ImageType  fImageType;
        std::string fSourceFileName;
        BYTE*       fBuffer;
        FIBITMAP* fFreeImageHandle;

    public:
        //Base abstract methods
        virtual size_t GetWidth() override { return fWidth; }
        virtual size_t GetHeight()override { return fHeight; }
        virtual size_t GetRowPitchInBytes() override { return fRowPitch; }
        virtual size_t GetBitsPerTexel() override { return fBitsPerPixel; }
        virtual unsigned char* GetBuffer() override { return fBuffer; }
        virtual ImageType GetImageType() override {return fImageType;}




        virtual Image* ConverToRGBA()  override
        {
            ImageFreeImage* converted = new ImageFreeImage();
            converted->fWidth = fWidth;
            converted->fHeight = fHeight;
            converted->fBitsPerPixel = 32;
            converted->fSourceFileName = fSourceFileName;
            converted->fFreeImageHandle = FreeImage_ConvertTo32Bits(fFreeImageHandle);
            converted->fRowPitch = FreeImage_GetPitch(converted->fFreeImageHandle);
            converted->fBuffer = FreeImage_GetBits(converted->fFreeImageHandle);
            return converted;
        }
        virtual void Unload()  override
        {
            if (fFreeImageHandle != NULL)
            {
                FreeImage_Unload(fFreeImageHandle);
                fFreeImageHandle = NULL;
                fBuffer = NULL;
                fSourceFileName = std::string();
            }
        }
        virtual bool LoadImpl(std::string filePath) override
        {
            if (IsOpened())
                Unload();

            bool opened = false;
            FREE_IMAGE_FORMAT format = FreeImage_GetFileType(filePath.c_str());

            if (format != FIF_UNKNOWN)
            {
                fSourceFileName = filePath;
                fFreeImageHandle = FreeImage_Load(format, filePath.c_str());
                BITMAPINFO* imageInfo = FreeImage_GetInfo(fFreeImageHandle);


                FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(fFreeImageHandle);
                switch (imageType)
                {
                case FIT_BITMAP:
                    fImageType = ImageType::IT_BITMAP;
                    break;
                case FIT_FLOAT:
                    fImageType = ImageType::IT_FLOAT;
                    break;
                default:
                    throw std::exception("Image type is currently not suppported");
                }


                const BITMAPINFOHEADER& header = imageInfo->bmiHeader;
                fWidth = header.biWidth;
                fHeight = header.biHeight;
                fBitsPerPixel = header.biBitCount;
                fRowPitch = FreeImage_GetPitch(fFreeImageHandle);
                fBuffer = FreeImage_GetBits(fFreeImageHandle);

                //FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(fFreeImageHandle);
                opened = true;

            }
            return opened;


        }
        virtual bool IsOpened() override
        {
            return fFreeImageHandle != NULL;
        }
        ImageFreeImage()
        {
            fWidth = -1;
            fHeight = -1;
            fRowPitch = -1;
            fBitsPerPixel = -1;
            fFreeImageHandle = NULL;
            fBuffer = NULL;
        }

    };
}