#pragma once

#include <Defs.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVImage/OIVTextImage.h>

#include "EventManager.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
namespace FreeType
{
    class FreeTypeConnector;
}

namespace OIV
{
    class LabelManager
    {
      public:

        inline static const std::filesystem::path sFontsFolder         = std::filesystem::path(
                                                                             LLUtils::PlatformUtility::GetExeFolder()) /
                                                                         "Resources" / "Fonts";
        inline static const LLUtils::native_string_type sFontPath      = (sFontsFolder / "CascadiaCode.ttf").native();
        inline static const LLUtils::native_string_type sFixedFontPath = (sFontsFolder / "CascadiaCode.ttf").native();

      public:

        LabelManager(FreeType::FreeTypeConnector* freeType);
        void RemoveAll();
        void Remove(const std::string& labelName);
        OIVTextImage* GetTextLabel(const std::string& labelName);
        OIVTextImage* GetOrCreateTextLabel(const std::string& labelName);
        void SetContentScale(const LWS::ContentScale& scale);

      private:

        void OnMonitorChange(const EventManager::MonitorChangeEventParams& params);
        OIVTextImageUniquePtr CreateTemplatedText();
        using TextLabels = std::map<std::string, OIVTextImageUniquePtr>;

      private:

        std::tuple<uint16_t, uint16_t> fDPI{96, 96};
        TextLabels fTextLabels;
        FreeType::FreeTypeConnector* fFreeType;
        EventManager::MonitorChangeEvent::Subscription fMonitorSubscription;
    };
}  // namespace OIV
