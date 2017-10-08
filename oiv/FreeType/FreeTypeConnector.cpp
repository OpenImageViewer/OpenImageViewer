#include "FreeTypeConnector.h"
#include "CodePoint.h"
#include <vector>
#include <StringUtility.h>
#include <Utility.h>


FreeTypeConnector FreeTypeConnector::sInstance;

FreeTypeConnector::FreeTypeConnector()
{
    FT_Error  error = FT_Init_FreeType(&fLibrary);
    if (error)
        throw std::logic_error("Can no initialize freetype library");

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

void FreeTypeConnector::CreateBitmap(const std::vector<std::string>& text, const std::string& fontPath,uint16_t fontSize, Bitmap &bitmap)
{
    FT_Face face;
    FT_Error error = FT_New_Face(fLibrary, fontPath.c_str() ,0, &face);

    if (error == FT_Err_Unknown_File_Format)
        throw std::logic_error("Unkown  file    format");
    else if (error)
        throw std::logic_error("Unkown  error");


    error = FT_Set_Char_Size(
        face,    /* handle to face object           */
        0,       /* char_width in 1/64th of points  */
        fontSize << 6,   /* char_height in 1/64th of points */
        0,     /* horizontal device resolution    */
        0);   /* vertical device resolution      */

   

     if (error)
        throw std::logic_error("Can not set char height");

    using namespace std;
    int maxLength = 0;
    for (const string& e : text)
        maxLength = std::max<int>(maxLength, e.length());
    

    const uint32_t maxAdvanceX = face->size->metrics.max_advance >> 6;;
    const uint32_t maxAdvanceY = face->size->metrics.height >> 6;

    uint32_t destWidth = maxAdvanceX * maxLength;
    uint32_t destHeight = maxAdvanceY * text.size();

    uint8_t* imageBuffer = new uint8_t[destWidth * destHeight];
    memset(imageBuffer, 0, destWidth * destHeight);
    LLUtils::Utility::BlitBox  dest = {};

    dest.buffer = imageBuffer;
    dest.width = destWidth;
    dest.height = destHeight;
    dest.pixelSizeInbytes = 1;
    dest.rowPitch = destWidth;
    int penX = 0 , penY = 0;
    
    for (const string& e : text)
    {

        for (int i = 0; i < e.length(); i++)
        {
            char c = e[i];
            std::string codePointChar;
            codePointChar.push_back(c);


            int codePoint = CodePoint::codepoint(codePointChar);
            const FT_UInt glyph_index = FT_Get_Char_Index(face, codePoint);
            error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                FT_LOAD_DEFAULT);  /* load flags, see below */

            if (error)
                throw std::logic_error("unable to load glyph");


            error = FT_Render_Glyph(face->glyph,   /* glyph slot  */
                FT_RENDER_MODE_NORMAL); /* render mode */

            if (error)
                throw std::logic_error("unable to render glyph");


            FT_GlyphSlot  slot = face->glyph;
            FT_Bitmap bitmap = slot->bitmap;
            LLUtils::Utility::BlitBox source = {};
            source.buffer = bitmap.buffer;
            source.width = bitmap.width;
            source.height = bitmap.rows;
            source.pixelSizeInbytes = 1;
            source.rowPitch = bitmap.pitch;

            int bitmapTop = slot->bitmap_top;

            //HACK: clamp bitmap top  to rows, not sure why it happens.
            if (bitmap.rows > slot->bitmap_top)
            {
                bitmapTop = bitmap.rows;
            }

            dest.left = penX + slot->bitmap_left;
            dest.top = penY + maxAdvanceY - bitmapTop - 1;
            penX += slot->advance.x >> 6;
            penY += slot->advance.y >> 6;

            LLUtils::Utility::Blit(dest, source);

        }
        penY += maxAdvanceY;
        penX = 0;
    }

    bitmap.width = destWidth;
    bitmap.height = destHeight;
    bitmap.buffer = imageBuffer;
    

    error = FT_Done_Face(face);
    if (error)
        throw std::logic_error("Can not destroy face");
 
}

FreeTypeConnector& FreeTypeConnector::GetSingleton()
{
    return sInstance;
}

