#include "FreeTypeHeaders.h"

#if OIV_BUILD_FREETYPE == 1
#include "FreeTypeConnector.h"
#include "FreeTypeRenderer.h"
#include "Exception.h"
#include "CodePoint.h"
#include <vector>
#include <StringUtility.h>
#include <Utility.h>
#include <Color.h>
#include <locale>
#include <string>
#include <iostream>
#include <Buffer.h>
#include "BlitBox.h"
#include "MetaTextParser.h"



FreeTypeConnector::FreeTypeConnector()
{
    FT_Error  error;
    if (error = FT_Init_FreeType(&fLibrary))
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError,GenerateFreeTypeErrorString("can not load glyph", error));

}

FreeTypeConnector::~FreeTypeConnector()
{
    fFontNameToFont.clear();
    FT_Error error = FT_Done_FreeType(fLibrary);
    if (error)
    {
        //TODO: destory freetype library eariler before object destruction.
        //throw std::logic_error("Can no destroy freetype library");
    }
}


std::string FreeTypeConnector::GenerateFreeTypeErrorString(std::string userMessage, FT_Error  error)
{
    return std::string("FreeType error: " + std::to_string(error) + ", " + userMessage);
}


void FreeTypeConnector::MesaureText(const TextMesureParams& measureParams, TextMesureResult& mesureResult)
{
    using namespace std;
    FT_Face face = measureParams.font->GetFace();
    uint32_t fontSize = measureParams.createParams.fontSize;
    const std::string& text = measureParams.createParams.text;
    const uint32_t rowHeight = ((face->size->metrics.ascender - face->size->metrics.descender) >> 6) + measureParams.createParams.outlineWidth *2;
    
    FT_Error error = 0;

    vector<FormattedTextEntry> formattedText = MetaText::GetFormattedText(text, fontSize);

    int penX = 0;
    int numberOfLines = 1;
    int maxRowWidth = 0;
    for (const FormattedTextEntry& el : formattedText)
    {
        for (const wstring::value_type & e : el.text)
        {
            if (e == '\n')
            {

                numberOfLines++;
                maxRowWidth = std::max(penX, maxRowWidth);
                penX = 0;
                continue;
            }

            //string mbs = conversion.to_bytes(u"\u4f60\u597d");  // ni hao (你好

            const int codePoint = e;  //CodePoint::codepoint(u"\u4f60");
            const FT_UInt glyph_index = FT_Get_Char_Index(face, codePoint);

            if (error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                FT_LOAD_DEFAULT))  /* load flags, see below */
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, GenerateFreeTypeErrorString("can not load glyph", error));
            }

            penX += face->glyph->advance.x >> 6;
        }
    }


    
    maxRowWidth = std::max(penX, maxRowWidth);

    // Add outline width to the final width of the rasterized text.
    maxRowWidth += measureParams.createParams.outlineWidth * 2;

    mesureResult.height = numberOfLines * rowHeight;
    mesureResult.width = maxRowWidth;
    mesureResult.rowHeight = rowHeight;
    mesureResult.descender = (face->size->metrics.descender >> 6) - measureParams.createParams.outlineWidth;
}

FreeTypeFont* FreeTypeConnector::GetOrCreateFont(std::string fontPath)
{
    FreeTypeFont* font = nullptr;

    auto it = fFontNameToFont.find(fontPath);
    if (it != fFontNameToFont.end())
    {
        font = it->second.get();
    }
    else
    {
        font = new FreeTypeFont(fLibrary, fontPath);
        fFontNameToFont.insert(std::make_pair(fontPath, FreeTypeFontUniquePtr(font)));
    }

    return font;
}





void FreeTypeConnector::CreateBitmap(const TextCreateParams& textCreateParams, Bitmap &out_bitmap)
{
    using namespace std;

    std::string text = textCreateParams.text;
    const std::string& fontPath = textCreateParams.fontPath;
    uint16_t fontSize = textCreateParams.fontSize;
    uint32_t OutlineWidth =  textCreateParams.outlineWidth;
    const LLUtils::Color outlineColor = textCreateParams.outlineColor;
    const LLUtils::Color backgroundColor = textCreateParams.backgroundColor;
    RenderMode renderMode = textCreateParams.renderMode;

    //Force normal anti aliasing for now.
    //TODO: Fix subpixel antialiasing rendering 
    renderMode = RenderMode::Antialiased;


    FreeTypeFont* font = GetOrCreateFont(fontPath);
    
    FT_Error error;
    
    if (error = FT_Library_SetLcdFilter(fLibrary, FT_LCD_FILTER_DEFAULT))
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, GenerateFreeTypeErrorString("can not set LCD Filter", error));

    font->SetSize(fontSize, textCreateParams.DPIx, textCreateParams.DPIy);

    TextMesureParams params;
    params.createParams = textCreateParams;
    params.font = font;

    TextMesureResult mesaureResult;

    MesaureText(params, mesaureResult);
    uint32_t destWidth = mesaureResult.width;
    uint32_t destHeight = mesaureResult.height;


    // ****** temporary workaround for outline text.
    //TODO: calculate better the destination width
     destWidth += 1;
    //***************




    vector<FormattedTextEntry> formattedText = MetaText::GetFormattedText(text,fontSize);

    const FT_Render_Mode textRenderMOde = (renderMode == RenderMode::SubpixelAntiAliased ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL);
;
    const uint32_t destPixelSize = 4;
    const uint32_t destRowPitch = destWidth * destPixelSize;
    const uint32_t sizeOfDestBuffer = destHeight * destRowPitch;
    const bool renderOutline = OutlineWidth > 0;
    const bool renderText = true;
    
    LLUtils::Buffer textBuffer(sizeOfDestBuffer);

    // when rendering with outline, the outline buffer is the final buffer, otherwise the text buffer is the final buffer.

    //Reset final text buffer to background color.
    for (uint32_t i = 0 ;i < destWidth * destHeight; i++)
        reinterpret_cast<uint32_t*>(textBuffer.GetBuffer())[i] =  backgroundColor.colorValue;

    LLUtils::Buffer outlineBuffer;
    BlitBox  destOutline = {};
    if (renderOutline)
    {
        outlineBuffer.Allocate(sizeOfDestBuffer);
        //Reset outline buffer to background color.
        for (uint32_t i = 0; i < destWidth * destHeight; i++)
            reinterpret_cast<uint32_t*>(outlineBuffer.GetBuffer())[i] = backgroundColor.colorValue;

        destOutline.buffer = outlineBuffer.GetBuffer();
        destOutline.width = destWidth;
        destOutline.height = destHeight;
        destOutline.pixelSizeInbytes = destPixelSize;
        destOutline.rowPitch = destRowPitch;
    }

    
    BlitBox  dest = {};
    dest.buffer = textBuffer.GetBuffer();
    dest.width = destWidth; 
    dest.height = destHeight;
    dest.pixelSizeInbytes = destPixelSize;
    dest.rowPitch = destRowPitch;

    int penX =  OutlineWidth;
    int penY = 0;
    int rowHeight = mesaureResult.rowHeight;
    FT_Face face = font->GetFace();

    for (const FormattedTextEntry& el : formattedText)
    {
        for (const string::value_type & e : el.text)
        {
            if (e == '\n')
            {
                penY += rowHeight;
                penX = OutlineWidth;
                continue;
            }

            int codePoint = e; // CodePoint::codepoint(multiBytesCodePOint);
            const FT_UInt glyph_index = FT_Get_Char_Index(face, codePoint);
            if (error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                FT_LOAD_DEFAULT))/* load flags, see below */
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, GenerateFreeTypeErrorString("can not Load glyph", error));
            }
         
            if (renderOutline) // render outline
            {
                //initialize stroker, so you can create outline font
                FT_Stroker stroker;
                FT_Stroker_New(fLibrary, &stroker);
                //  2 * 64 result in 2px outline
                FT_Stroker_Set(stroker, OutlineWidth * 64, FT_STROKER_LINECAP_SQUARE, FT_STROKER_LINEJOIN_BEVEL, 0);
                FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                FT_Glyph glyph;
                FT_Get_Glyph(face->glyph, &glyph);
                FT_Glyph_StrokeBorder(&glyph, stroker, false, true);
                FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
                FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
                FreeTypeRenderer::BitmapProperties bitmapProperties = FreeTypeRenderer::GetBitmapGlyphProperties(bitmapGlyph->bitmap);
                 
                FreeTypeRenderer::GlyphRGBAParams params = { bitmapGlyph , backgroundColor,outlineColor, bitmapProperties };
                LLUtils::Buffer rasterizedGlyph = std::move(FreeTypeRenderer::RenderGlyphToBuffer(params));

                BlitBox source = {};
                source.buffer = rasterizedGlyph.GetBuffer();
                source.width = bitmapProperties.width;
                source.height = bitmapProperties.height;
                source.pixelSizeInbytes = destPixelSize;
                source.rowPitch = destPixelSize * bitmapProperties.width;

                destOutline.left = penX + bitmapGlyph->left;
                destOutline.top = rowHeight + penY + mesaureResult.descender - bitmapGlyph->top;
                BlitBox::Blit(destOutline, source);
            }

            if (renderText)
            {
             
                FT_Glyph glyph;
                FT_Get_Glyph(face->glyph, &glyph);
                FT_Glyph_To_Bitmap(&glyph, textRenderMOde, nullptr, true);
                FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
                FreeTypeRenderer::BitmapProperties bitmapProperties = FreeTypeRenderer::GetBitmapGlyphProperties(bitmapGlyph->bitmap);
                 
                FreeTypeRenderer::GlyphRGBAParams params = { bitmapGlyph , backgroundColor, el.textColor , bitmapProperties };

                LLUtils::Buffer rasterizedGlyph = std::move(FreeTypeRenderer::RenderGlyphToBuffer(params));

                if (error)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error, unable to render glyph");

                BlitBox source = {};
                source.buffer = rasterizedGlyph.GetBuffer();
                source.width = bitmapProperties.width;
                source.height = bitmapProperties.height;
                source.pixelSizeInbytes = destPixelSize;
                source.rowPitch = destPixelSize * bitmapProperties.width;

                dest.left = penX + bitmapGlyph->left;
                dest.top = rowHeight + penY + mesaureResult.descender - bitmapGlyph->top;
                FT_GlyphSlot  slot = face->glyph;
                penX += slot->advance.x >> 6;

                BlitBox::Blit(dest, source);
            }
        }
    }

    if (renderOutline)
    {
        //Blend text buffer onto outline buffer.
        dest.left = 0;
        dest.top = 0;
        destOutline.left = 0;
        destOutline.top = 0;
        BlitBox::Blit(destOutline, dest);
    }

    out_bitmap.width = destWidth;
    out_bitmap.height = destHeight;
    out_bitmap.buffer = renderOutline ? std::move(outlineBuffer) : std::move(textBuffer);
    out_bitmap.PixelSize = destPixelSize;
    out_bitmap.rowPitch = destRowPitch;
 
}
#endif // endif OIV_BUILD_FREETYPE