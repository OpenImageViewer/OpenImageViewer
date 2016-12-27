#pragma once
#include <Interfaces/IImagePlugin.h>
#include <FreeImage.h>
namespace OIV
{
    class PluginFreeImage : public IImagePlugin
    {

    public:
   
        PluginProperties& GetPluginProperties() override
        {
            static PluginProperties pluginProperties =
            {
                "FreeImage plugin",
                "BMP;ICO;JPEG;JNG;KOALA;LBM;IFF;LBM;MNG;PBM;PBMRAW;PCD;PCX;PGM;PGMRAW;PNG;PPM;PPMRAW;RAS;TGA;TIFF;TIF;WBMP;PSD;CUT;XBM;XPM;DDS;GIF;HDR;FAXG3;SGI;EXR;J2K;JP2;PFM;PICT;RAW;WEBP;JXR"
                };
            
            return pluginProperties;
        }


        virtual bool LoadImage(void* buffer, std::size_t size, ImageProperies& out_properties) override
        {
            FIBITMAP* freeImageHandle;
            bool opened = false;

            FIMEMORY* memStream = FreeImage_OpenMemory(static_cast<BYTE*>(buffer), size);
            FREE_IMAGE_FORMAT format =  FreeImage_GetFileTypeFromMemory(memStream, size);
            freeImageHandle = FreeImage_LoadFromMemory(format, memStream, 0);
            if (freeImageHandle && FreeImage_FlipVertical(freeImageHandle))
            {

                BITMAPINFO* imageInfo = FreeImage_GetInfo(freeImageHandle);
                FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(freeImageHandle);

                const BITMAPINFOHEADER& header = imageInfo->bmiHeader;

                out_properties.Width = header.biWidth;
                out_properties.Height = header.biHeight;
                out_properties.BitsPerTexel = header.biBitCount;
                out_properties.RowPitchInBytes = FreeImage_GetPitch(freeImageHandle);

                out_properties.NumSubImages = 0;

                int imageSizeInMemory = header.biHeight * out_properties.RowPitchInBytes;
                out_properties.ImageBuffer = new uint8_t[imageSizeInMemory];
                memcpy_s(out_properties.ImageBuffer, imageSizeInMemory, FreeImage_GetBits(freeImageHandle), imageSizeInMemory);


                switch (imageType)
                {
                case FIT_BITMAP:
                {
                    switch (header.biBitCount)
                    {
                    case 8:
                        out_properties.Type = IT_BYTE_8BIT;
                        break;
                    case 32:
                        out_properties.Type = IT_BYTE_ARGB;
                        break;
                    case 24:
                        out_properties.Type = IT_BYTE_RGB;
                        break;
                    default:
                        out_properties.Type = IT_UNKNOWN;

                    }
                }

                }

                if (freeImageHandle != nullptr)
                    FreeImage_Unload(freeImageHandle);
                opened = true;

            }

            if (memStream != nullptr)
                FreeImage_CloseMemory(memStream);
                
            return opened;
        }
    };
}