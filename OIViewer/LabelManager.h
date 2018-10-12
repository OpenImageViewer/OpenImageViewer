#pragma once
#include <API/defs.h>
#include <API/StringHelper.h>
#include <map>
#include "OIVImage\OIVTextImage.h"
#include "EventManager.h"

namespace OIV
{
    class LabelManager
    {
    public:
        inline static const OIVString sFontPath = OIV_ToOIVString(L"C:\\Windows\\Fonts\\segoeuib.ttf");
        inline static const OIVString sFixedFontPath = OIV_ToOIVString(L"C:\\Windows\\Fonts\\consola.ttf");
     
        
    public:
        LabelManager();
        void RemoveAll();
        void Remove(const std::string& labelName);
        OIVTextImage* GetTextLabel(const std::string& labelName);
        OIVTextImage* GetOrCreateTextLabel(const std::string& labelName);

    private:
        void OnMonitorChange(const EventManager::MonitorChangeEventParams& params);
        OIVTextImageUniquePtr CreateTemplatedText();
        using TextLabels = std::map<std::string, OIVTextImageUniquePtr>;

    private:
        std::tuple<uint16_t, uint16_t> fDPI{ 96,96 };
        TextLabels fTextLabels;
    };
}
