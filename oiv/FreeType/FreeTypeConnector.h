#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include <string>
#include <vector>

class FreeTypeConnector
{
public:

    struct Bitmap
    {
        uint32_t width;
        uint32_t height;
        uint8_t* buffer;
    };
    
    
    ~FreeTypeConnector();

    void CreateBitmap(const std::vector<std::string>& text, const std::string& fontPath, uint16_t fontSize, Bitmap &bitmap);
    static FreeTypeConnector& GetSingleton();
private:
    static FreeTypeConnector sInstance;
    FreeTypeConnector();
    

private:
    FT_Library  fLibrary;
    
};

