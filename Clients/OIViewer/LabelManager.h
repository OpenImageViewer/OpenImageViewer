#pragma once
#include <defs.h>
#include <map>
#include <OIVImage/OIVTextImage.h>
#include "EventManager.h"
namespace FreeType
{
    class FreeTypeConnector;
}

namespace OIV
{
    class LabelManager
    {
    public:
        inline static const std::filesystem::path sFontsFolder = std::filesystem::path(LLUtils::PlatformUtility::GetExeFolder()) / "Resources" / "Fonts";
        inline static const OIVString sFontPath = (sFontsFolder / "CascadiaCode.ttf").wstring();
        inline static const OIVString sFixedFontPath = (sFontsFolder / "CascadiaCode.ttf").wstring();
    public:
        LabelManager(FreeType::FreeTypeConnector* freeType);
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
        FreeType::FreeTypeConnector* fFreeType;
    };
}
