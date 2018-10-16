#pragma once
#include <API/defs.h>
#include <API/StringHelper.h>
#include <map>
#include "OIVImage\OIVTextImage.h"
namespace OIV
{
    class LabelManager
    {
    public:
        inline static const OIVString sFontPath = OIV_ToOIVString(L"C:\\Windows\\Fonts\\segoeuib.ttf");
        inline static const OIVString sFixedFontPath = OIV_ToOIVString(L"C:\\Windows\\Fonts\\consola.ttf");
     
        
    public:
        void RemoveAll();
        void Remove(const std::string& labelName);
        OIVTextImage* GetTextLabel(const std::string& labelName);
        OIVTextImage* GetOrCreateTextLabel(const std::string& labelName);

    private:
        OIVTextImageUniquePtr CreateTemplatedText();
        using TextLabels = std::map<std::string, OIVTextImageUniquePtr>;

    private:
        TextLabels fTextLabels;
    };
}
