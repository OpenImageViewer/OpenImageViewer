#pragma once
#include "FreeTypeHeaders.h"
#include <string>
#include <LLUtils/Exception.h>

class FreeTypeFont
{
public:
    FreeTypeFont(FT_Library ftLibrary, std::string fileName)
    {
        fName = fileName;
        fLibrary = ftLibrary;
        FT_Error error = FT_New_Face(fLibrary, LLUtils::StringUtility::ToAString(fName).c_str(), 0, &fFace);
        if (error == FT_Err_Unknown_File_Format)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error Unknown file format");
        else if (error)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::Unknown, "FreeType unkown error Unknown file format");

    }
    ~FreeTypeFont()
    {
        FT_Error error = FT_Done_Face(fFace);
//        if (error)
  //          LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "FreeType error, can not destroy face");
    }

    void SetSize(uint16_t fontSize, uint16_t DPIx, uint16_t DPIy)
    {
        fFontSize = fontSize;
        FT_Error error =
            FT_Set_Char_Size(
                fFace,    /* handle to face object           */
                0,       /* char_width in 1/64th of points  */
                fontSize << 6,   /* char_height in 1/64th of points */
                DPIx,     /* horizontal device resolution    */
                DPIy);   /* vertical device resolution      */

    }

    FT_Face GetFace()
    {
        return fFace;
    }



private:
    std::string fName;
    FT_Face fFace = nullptr;
    FT_Library fLibrary = nullptr;
    uint16_t fFontSize = 0;
};

using FreeTypeFontUniquePtr = std::unique_ptr<FreeTypeFont>;