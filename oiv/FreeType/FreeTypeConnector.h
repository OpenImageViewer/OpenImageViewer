#pragma once
#include "FreeTypeHeaders.h"


#if OIV_BUILD_FREETYPE == 1
#include "FreeTypeFont.h"
#include <map>
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
        std::byte* buffer;
        uint32_t PixelSize;
        uint32_t rowPitch;
    };
    
    struct FormattedTextEntry
    {
        uint32_t color;
        uint32_t size;
        u8string text;
    };


    struct Format
    {
        uint32_t color;
        uint32_t backgroundColor;
        uint32_t size;
        static Format Parse(const u8string& format);
    };

    using ListFormattedTextEntry = std::vector<FormattedTextEntry>;

    using ListListFormattedTextEntry = std::vector<ListFormattedTextEntry>;
    
    ~FreeTypeConnector();
    std::vector<FreeTypeConnector::FormattedTextEntry> GetFormattedText(u8string text, int fontSize);
    
    void CreateBitmap(const u8string& text
        , const u8string& fontPath
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
    std::map<std::string, FreeTypeFontUniquePtr> fFontNameToFont;
    
};

#endif