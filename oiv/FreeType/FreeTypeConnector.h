#pragma once
#include "Configuration.h"
#if OIV_BUILD_FREETYPE == 1
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <string>
#include <vector>
#include <Color.h>

class FreeTypeConnector
{
public:

    struct Bitmap
    {
        uint32_t width;
        uint32_t height;
        uint8_t* buffer;
        uint32_t PixelSize;
        uint32_t rowPitch;
    };
    
    struct FormattedTextEntry
    {
        uint32_t color;
        uint32_t size;
        std::string text;
    };


    struct Format
    {
        uint32_t color;
        uint32_t backgroundColor;
        uint32_t size;
        static Format Parse(const std::string& format);
    };

    using ListFormattedTextEntry = std::vector<FormattedTextEntry>;

    using ListListFormattedTextEntry = std::vector<ListFormattedTextEntry>;
    
    ~FreeTypeConnector();
    std::vector<FreeTypeConnector::FormattedTextEntry> GetFormattedText(std::string text, int fontSize);
    
    void CreateBitmap(const std::string& text
        , const std::string& fontPath
        , uint16_t fontSize
        , LLUtils::Color color
        , Bitmap &bitmap
        
    );
    static FreeTypeConnector& GetSingleton();
private:
    static FreeTypeConnector sInstance;
    FreeTypeConnector();
    

private:
    FT_Library  fLibrary;
    
};

#endif