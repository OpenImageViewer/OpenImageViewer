
#include "FreeTypeConnector.h"
#include "Exception.h"
#if OIV_BUILD_FREETYPE == 1
#include "CodePoint.h"
#include <vector>
#include <StringUtility.h>
#include <Utility.h>
#include <Color.h>


FreeTypeConnector FreeTypeConnector::sInstance;

FreeTypeConnector::FreeTypeConnector()
{
    FT_Error  error = FT_Init_FreeType(&fLibrary);
    if (error)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Can no initialize freetype library");

}

FreeTypeConnector& FreeTypeConnector::GetSingleton()
{
    return sInstance;
}

FreeTypeConnector::Format FreeTypeConnector::Format::Parse(const std::string& format)
{
    using namespace std;
    using namespace LLUtils;

    Format result = {};
    string trimmed = StringUtility::ToLower(format);
    trimmed.erase(trimmed.find_last_not_of(" >") + 1);
    trimmed.erase(0,trimmed.find_first_not_of(" <"));

    ListAString properties = StringUtility::split<char>(trimmed, ';');
    stringstream ss;
    for (const string& prop : properties)
    {
        ListAString trimmedList = StringUtility::split<char>(prop, '=');
        const string& key = trimmedList[0];
        const string& value = trimmedList[1];
        if (key == "textcolor")
        {
            Color c = Color::FromString(value);
            result.color = c.colorValue;
        }
        else if (key == "backgroundcolor")
        {
            Color c = Color::FromString(value);
            result.backgroundColor = c.colorValue;
        }
        else if (key == "textSize")
        {
            result.size = std::atoi(value.c_str());
        }
    }

    return result;
}

FreeTypeConnector::~FreeTypeConnector()
{
    FT_Error error = FT_Done_FreeType(fLibrary);
    if (error)
    {
        //TODO: destory freetype library eariler before object destruction.
        //throw std::logic_error("Can no destroy freetype library");
    }
}

std::vector<FreeTypeConnector::FormattedTextEntry> FreeTypeConnector::GetFormattedText(std::string text, int fontSize)
{
    using namespace std;
    ptrdiff_t beginTag = -1;
    ptrdiff_t endTag = -1;
    vector<FormattedTextEntry> formattedText;
    for (int i = 0; i < text.length(); i++)
    {
        if (text[i] == '<')
        {
            if (endTag != -1)
            {
                string tagContents = text.substr(beginTag, endTag - beginTag + 1);

                string textInsideTag = text.substr(endTag + 1, i - (endTag + 1));
                beginTag = i;
                endTag = -1;

                Format format = Format::Parse(tagContents);
                FormattedTextEntry entry;
                entry.size = fontSize;
                entry.text = textInsideTag;
                entry.color = format.color;
                formattedText.push_back(entry);
            }
            else
            {
                beginTag = i;
            }


        }
        if (text[i] == '>')
        {
            endTag = i;
        }
    }

   
    
        ptrdiff_t i = text.length() - 1;
        string tagContents = text.substr(beginTag, endTag - beginTag + 1);

        string textInsideTag = text.substr(endTag + 1, i - endTag );
        beginTag = i;
        endTag = -1;

        Format format = Format::Parse(tagContents);
        FormattedTextEntry entry;
        entry.size = fontSize;
        entry.text = textInsideTag;
        entry.color = format.color;
        formattedText.push_back(entry);
    
    return formattedText;
}


void FreeTypeConnector::CreateBitmap(const std::string& text
    , const std::string& fontPath
    , uint16_t fontSize
    , LLUtils::Color backgroundColor
    , Bitmap &out_bitmap
    
)
{
    using namespace std;
    FT_Face face;
    FT_Error error = FT_New_Face(fLibrary, fontPath.c_str() ,0, &face);

    if (error == FT_Err_Unknown_File_Format)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error Unknown file format");
    else if (error)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::Unknown,"FreeType unkown error Unknown file format");


    error = FT_Set_Char_Size(
        face,    /* handle to face object           */
        0,       /* char_width in 1/64th of points  */
        fontSize << 6,   /* char_height in 1/64th of points */
        0,     /* horizontal device resolution    */
        0);   /* vertical device resolution      */

   

              //Height in pixels (using a double for sub-pixel precision)
    double baseline_height = abs(face->descender) * (double)fontSize / face->units_per_EM;
    baseline_height = std::ceil(baseline_height);


     if (error)
         LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error can not set char height");

    const uint32_t baselineHeight = static_cast<uint32_t>(baseline_height);

    const uint32_t rowHeight = (face->size->metrics.height >> 6) + baselineHeight;

    
    vector<FormattedTextEntry> formattedText = GetFormattedText(text,fontSize);

    int penX = 0, penY = 0;
    //First mesure text
    int maxRow = 0;
    for (const FormattedTextEntry& el : formattedText)
    {
        for (const string::value_type & e : el.text)
        {
            if (e == '\n')
            {
                penY += rowHeight;
                maxRow = std::max(penX, maxRow);
                penX = 0;
                continue;
            }

            std::string codePointChar;
            codePointChar.push_back(e);

            const int codePoint = CodePoint::codepoint(codePointChar);
            const FT_UInt glyph_index = FT_Get_Char_Index(face, codePoint);
            error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                FT_LOAD_DEFAULT);  /* load flags, see below */

            if (error)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error can not load glyph");

            penX += face->glyph->advance.x >> 6;
            penY += face->glyph->advance.y >> 6;
            //face->glyph->
            //int height1 = face->max_advance_height >> 6;// glyph->metrics.vertAdvance >> 6;
            int k = 0;
        }
    }
    penY += rowHeight;
    maxRow = std::max(penX, maxRow);


    const uint32_t destWidth = maxRow;
    const uint32_t destHeight = penY;

    const uint32_t destPixelSize = 4;
    const uint32_t destRowPitch = destWidth * destPixelSize;
    const uint32_t sizeOFDestBuffer = destHeight * destRowPitch;
    uint8_t* imageBuffer = new uint8_t[sizeOFDestBuffer];
    for (uint32_t i = 0 ;i < destWidth * destHeight; i++)
    {
        reinterpret_cast<uint32_t*>(imageBuffer)[i] = backgroundColor.colorValue;
    }
    
    LLUtils::Utility::BlitBox  dest = {};

    dest.buffer = imageBuffer;
    dest.width = destWidth;
    dest.height = destHeight;
    dest.pixelSizeInbytes = destPixelSize;
    dest.rowPitch = destRowPitch;

    penX = 0;
    penY = 0;
    
    for (const FormattedTextEntry& el : formattedText)
    {
        for (const string::value_type & e : el.text)
        {
            if (e == '\n')
            {
                penY += rowHeight;
                penX = 0;
                continue;
            }

            string codePointChar;
            codePointChar.push_back(e);


            int codePoint = CodePoint::codepoint(codePointChar);
            const FT_UInt glyph_index = FT_Get_Char_Index(face, codePoint);
            error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                FT_LOAD_DEFAULT);  /* load flags, see below */

            if (error)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error, unable to load glyph");


            error = FT_Render_Glyph(face->glyph,   /* glyph slot  */
                FT_RENDER_MODE_NORMAL); /* render mode */

            if (error)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error, unable to render glyph");


            FT_GlyphSlot  slot = face->glyph;
            FT_Bitmap bitmap = slot->bitmap;

            //Create a copy of the bitmap in RGBA.
            unique_ptr<uint8_t> RGBABitmap = unique_ptr<uint8_t>(new uint8_t[bitmap.width * bitmap.rows * destPixelSize]);
            
            // Fill glyph background with background color.
            uint32_t* RGBABitmapPtr = reinterpret_cast<uint32_t*>(RGBABitmap.get());
            for (uint32_t i = 0; i < bitmap.width * bitmap.rows; i++)
            {
                RGBABitmapPtr[i] = backgroundColor.colorValue;
            }

            
            //Blend source bitmap with the new RGBA bitmap
            for (uint32_t i = 0; i < bitmap.rows * bitmap.width; i++)
            {
                //Make the text overlay color transparent for text blending.
                LLUtils::Color textOverlayColor = el.color;
                textOverlayColor.A = 0;

                LLUtils::Color source(bitmap.buffer[i] << 24 | textOverlayColor.colorValue);
                RGBABitmapPtr[i] = LLUtils::Color(RGBABitmapPtr[i]).Blend(source).colorValue;
            }

            LLUtils::Utility::BlitBox source = {};
            source.buffer = RGBABitmap.get();
            source.width = bitmap.width;
            source.height = bitmap.rows;
            source.pixelSizeInbytes = destPixelSize;
            source.rowPitch = destPixelSize * bitmap.width;//

            int bitmapTop = slot->bitmap_top;

            dest.left = penX + slot->bitmap_left;
            dest.top = penY + rowHeight - bitmapTop  - 1 - baselineHeight;
            penX += slot->advance.x >> 6;
            penY += slot->advance.y >> 6;

            LLUtils::Utility::Blit(dest, source);
        }
    }
    out_bitmap.width = destWidth;
    out_bitmap.height = destHeight;
    out_bitmap.buffer = imageBuffer;
    out_bitmap.PixelSize = destPixelSize;
    out_bitmap.rowPitch = destRowPitch;
    

    error = FT_Done_Face(face);
    if (error)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error, can not destroy face");
 
}
#endif // endif