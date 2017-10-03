#pragma once

#include <IImagePlugin.h>
#include <FreeImage.h>
namespace IMCodec
{
    class CodecFreeImage : public IImagePlugin
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


        virtual bool LoadImage(const uint8_t* buffer, std::size_t size, ImageProperies& out_properties) override
        {
            FIBITMAP* freeImageHandle;
            bool opened = false;

            FIMEMORY* memStream = FreeImage_OpenMemory(const_cast<BYTE*>(buffer),static_cast<DWORD>(size));
            FREE_IMAGE_FORMAT format =  FreeImage_GetFileTypeFromMemory(memStream, static_cast<int>(size));
            freeImageHandle = FreeImage_LoadFromMemory(format, memStream, 0);
            if (freeImageHandle && FreeImage_FlipVertical(freeImageHandle))
            {

                BITMAPINFO* imageInfo = FreeImage_GetInfo(freeImageHandle);
                FREE_IMAGE_TYPE TexelFormat = FreeImage_GetImageType(freeImageHandle);

                const BITMAPINFOHEADER& header = imageInfo->bmiHeader;

                out_properties.Width = header.biWidth;
                out_properties.Height = header.biHeight;
                out_properties.RowPitchInBytes = FreeImage_GetPitch(freeImageHandle);

                out_properties.NumSubImages = 0;

                std::size_t imageSizeInMemory = header.biHeight * out_properties.RowPitchInBytes;
                out_properties.ImageBuffer = new uint8_t[imageSizeInMemory];
                memcpy_s(out_properties.ImageBuffer, imageSizeInMemory, FreeImage_GetBits(freeImageHandle), imageSizeInMemory);


                switch (TexelFormat)
                {
                case FIT_BITMAP:
                {
                    switch (header.biBitCount)
                    {
                    case 8:
                        out_properties.TexelFormatDecompressed = TF_I_X8;
                        break;
                    case 32:
                        if (format == FIF_BMP)
                        {
                            // Hack: BMP isn't read with an alpha channel.
                            uint32_t* line = (uint32_t*)out_properties.ImageBuffer;
                            for (uint32_t y = 0; y < out_properties.Height; y++)
                            {
                                for (uint32_t x = 0; x < out_properties.Width; x++)
                                    line[x] = line[x] | 0xFF000000;

                                line = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(line) + out_properties.RowPitchInBytes);

                            }
                        }
                        out_properties.TexelFormatDecompressed = TF_I_B8_G8_R8_A8;
                        break;
                    case 24:
                        out_properties.TexelFormatDecompressed = TF_I_B8_G8_R8;
                        break;
                    default:
                        out_properties.TexelFormatDecompressed = TF_UNKNOWN;

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