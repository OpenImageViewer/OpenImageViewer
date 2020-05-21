#pragma once
#include "FreeTypeHeaders.h"


#if OIV_BUILD_FREETYPE == 1
#include "FreeTypeFont.h"
#include <map>
#include <cstdint>
#include <string>
#include <vector>
#include <LLUtils/Color.h>
#include <LLUtils/Buffer.h>
#include <LLUtils/Singleton.h>


class FreeTypeConnector : public LLUtils::Singleton<FreeTypeConnector>
{

    ///*** workaround to solve***

    static const size_t ExtraWidth = 1;
    static const size_t ExtraRowHeight = 1;

    friend class LLUtils::Singleton<FreeTypeConnector>;
public:
    ~FreeTypeConnector();

    struct Bitmap
    {
        uint32_t width;
        uint32_t height;
        LLUtils::Buffer buffer;
        uint32_t PixelSize;
        uint32_t rowPitch;
    };
    
    enum class RenderMode
    {
          Default
        , Antialiased
        , SubpixelAntiAliased
    };

    struct TextCreateParams
    {
        std::string fontPath;
        std::string text;
        uint16_t fontSize;
        LLUtils::Color backgroundColor;
        LLUtils::Color outlineColor;
        uint32_t outlineWidth;
        RenderMode renderMode;
        uint16_t DPIx;
        uint16_t DPIy;
    };

    void CreateBitmap(const TextCreateParams& textCreateParams, Bitmap & out_bitmap);


private:
       // private types
    struct TextMesureParams
    {
        TextCreateParams createParams;
        FreeTypeFont* font;
    };

    struct TextMesureResult
    {
        uint32_t width;
        uint32_t height;
        uint32_t rowHeight;
        int32_t descender;
    };
    //private member methods

    FreeTypeConnector();
    FreeTypeFont* GetOrCreateFont(std::string fontPath);
    FT_Stroker GetStroker();
    std::string GenerateFreeTypeErrorString(std::string userMessage, FT_Error error);
    void MesaureText(const TextMesureParams& measureParams, TextMesureResult& out_result);
    

private:
    FT_Library fLibrary;
    FT_Stroker fStroker = nullptr;

    std::map<std::string, FreeTypeFontUniquePtr> fFontNameToFont;
    
};

#endif
