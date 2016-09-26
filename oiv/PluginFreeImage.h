#pragma once
#include "ImagePlugin.h"
#include <FreeImage.h>
namespace OIV
{
    class PluginFreeImage : public ImagePlugin
    {

    public:
   
        virtual char* GetHint() override
        {
            return "BMP;ICO;JPEG;JNG;KOALA;LBM;IFF;LBM;MNG;PBM;PBMRAW;PCD;PCX;PGM;PGMRAW;PNG;PPM;PPMRAW;RAS;TGA;TIFF;WBMP;PSD;CUT;XBM;XPM;DDS;GIF;HDR;FAXG3;SGI;EXR;J2K;JP2;PFM;PICT;RAW;WEBP;JXR";
        }


        virtual bool LoadImage(std::string filePath, ImageProperies& properties) override
        {
            FIBITMAP* freeImageHandle;
            bool opened = false;
            FREE_IMAGE_FORMAT format = FreeImage_GetFileType(filePath.c_str());

            if (format != FIF_UNKNOWN)
            {
                freeImageHandle = FreeImage_Load(format, filePath.c_str());
                if (freeImageHandle && FreeImage_FlipVertical(freeImageHandle))
                {
                    
                    BITMAPINFO* imageInfo = FreeImage_GetInfo(freeImageHandle);
                    FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(freeImageHandle);

                    const BITMAPINFOHEADER& header = imageInfo->bmiHeader;

                    properties.Width = header.biWidth;
                    properties.Height = header.biHeight;
                    properties.BitsPerTexel = header.biBitCount;
                    properties.RowPitchInBytes = FreeImage_GetPitch(freeImageHandle);
                    
                    properties.NumSubImages = 0;

                    int imageSizeInMemory = header.biHeight * properties.RowPitchInBytes;
                    properties.ImageBuffer = new char*[imageSizeInMemory];
                    memcpy_s(properties.ImageBuffer, imageSizeInMemory, FreeImage_GetBits(freeImageHandle), imageSizeInMemory);


                    switch (imageType)
                    {
                    case FIT_BITMAP:
                    {
                        switch (header.biBitCount)
                        {
                        case 8:
                            properties.Type = IT_BYTE_8BIT;
                            break;
                        case 32:
                            properties.Type = IT_BYTE_ARGB;
                            break;
                        case 24:
                            properties.Type = IT_BYTE_RGB;
                            break;
                        default:
                            properties.Type = IT_UNKNOWN;

                        }
                    }


                    }

                    if (freeImageHandle != NULL)
                        FreeImage_Unload(freeImageHandle);
                    opened = true;
                }

            }
            return opened;


        }
    };
}