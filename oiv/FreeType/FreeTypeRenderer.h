#include "FreeTypeHeaders.h"

#if OIV_BUILD_FREETYPE == 1
#pragma once
class FreeTypeRenderer
{
public:
    struct BitmapProperties
    {
        uint32_t width;
        uint32_t height;
        uint32_t numChannels;
        uint32_t bitsPerChannel;
        uint32_t rowpitchInBytes;
    };

    struct GlyphRGBAParams
    {
        FT_BitmapGlyph bitmapGlyph;
        LLUtils::Color backgroudColor;
        LLUtils::Color textColor;
        BitmapProperties bitmapProperties;
    };
    
    

    static BitmapProperties GetBitmapGlyphProperties(const FT_Bitmap bitmap)
    {
        BitmapProperties bitmapProperties;
        
        switch (bitmap.pixel_mode)
        {
        case FT_PIXEL_MODE_GRAY:
            bitmapProperties.numChannels = 1;
            break;
        case FT_PIXEL_MODE_LCD:
            bitmapProperties.numChannels = 3;
            break;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }

        bitmapProperties.bitsPerChannel = static_cast<uint32_t>(std::log2(bitmap.num_grays + 1));
        bitmapProperties.height = bitmap.rows;
        bitmapProperties.width = bitmap.width / bitmapProperties.numChannels;
        bitmapProperties.rowpitchInBytes = bitmap.pitch;

        return bitmapProperties;
    }

    static LLUtils::Buffer RenderGlyphToBuffer(const GlyphRGBAParams& params)
    {
        using namespace LLUtils;

        FT_Bitmap bitmap = params.bitmapGlyph->bitmap;


        const int destPixelSize = 4;
        const int sourcePixelSize = params.bitmapProperties.numChannels;
        const uint32_t HeightInPixels = params.bitmapProperties.height;
        const uint32_t widthInPixels = params.bitmapProperties.width;

        LLUtils::Color backgroudColor = params.backgroudColor;
        LLUtils::Color textColor = params.textColor;

        

        const size_t bufferSize = widthInPixels * HeightInPixels * destPixelSize;
        Buffer RGBABitmap(bufferSize);
        
        // Fill glyph background with background color.
        uint32_t* RGBABitmapPtr = reinterpret_cast<uint32_t*>(RGBABitmap.data());
        for (uint32_t i = 0; i < widthInPixels * HeightInPixels; i++)
        {
            RGBABitmapPtr[i] = backgroudColor.colorValue;
        }

        uint32_t sourceRowStart = 0;
        
        for (uint32_t y = 0 ; y < bitmap.rows; y++)
        {
            for (uint32_t x = 0; x < widthInPixels; x++)
            {
                const uint32_t destPos = y * widthInPixels + x;
                std::uint8_t R;
                std::uint8_t G;
                std::uint8_t B;
                std::uint8_t A;
                switch (params.bitmapProperties.numChannels)
                {
                case 1: // MONOCHROME
                    R = params.textColor.R;
                    G = params.textColor.G;
                    B = params.textColor.B;
                    A = bitmap.buffer[sourceRowStart + x + 0];
                    break; 

                case 3: // RGB
                {
                    uint32_t currentPixelPos = sourceRowStart + x * params.bitmapProperties.numChannels;

                    uint8_t BC = bitmap.buffer[currentPixelPos + 0];
                    uint8_t GC = bitmap.buffer[currentPixelPos + 1];
                    uint8_t RC = bitmap.buffer[currentPixelPos + 2];

                    R = (textColor.R * RC) / 0xFF;
                    G = (textColor.G * GC) / 0xFF;
                    B = (textColor.B * BC) / 0xFF;

                    A = (RC + GC + BC) / 3;
                }
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                }
                
                
                LLUtils::Color source(A << 24 | B << 16 | G << 8 | R);
                RGBABitmapPtr[destPos] =  LLUtils::Color(RGBABitmapPtr[destPos]).Blend(source).colorValue;
            }

            sourceRowStart += params.bitmapProperties.rowpitchInBytes;
        }
        return RGBABitmap;
    }
};
#endif