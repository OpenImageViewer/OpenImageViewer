#pragma once

#include <RendererCore/RenderTypes.h>
#include <LLUtils/PlatformUtility.h>
#include "TextLabel.h"

#include "EventManager.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
namespace FreeType
{
    class FreeTypeConnector;
}

namespace OIV
{
    // Viewer owner of named overlays and their creation-order preparation.
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
        void InitializeRenderer(Renderer& renderer);
        void Prepare(Renderer& renderer);
        void RemoveAll();
        void Remove(const std::string& labelName);
        TextLabel* GetTextLabel(const std::string& labelName);
        TextLabel* GetOrCreateTextLabel(const std::string& labelName);
        void SetContentScale(const LWS::ContentScale& scale);

      private:

        void OnMonitorChange(const EventManager::MonitorChangeEventParams& params);
        std::unique_ptr<TextLabel> CreateTemplatedText();
        using TextLabels = std::map<std::string, std::unique_ptr<TextLabel>>;

      private:

        // Current horizontal and vertical label DPI, initially 96.
        std::tuple<uint16_t, uint16_t> fDPI{96, 96};
        // Owned named labels, with presentation lifetimes bounded by the renderer.
        TextLabels fTextLabels;
        // Non-owning labels in creation order, including labels created before initialization.
        std::vector<TextLabel*> fCreationOrder;
        // Borrowed font connector, which outlives all labels.
        FreeType::FreeTypeConnector* fFreeType;
        // Borrowed stable facade enables empty registration at label creation after initialization.
        Renderer* fRenderer = nullptr;
        // Owned monitor-change subscription invalidating label DPI.
        EventManager::MonitorChangeEvent::Subscription fMonitorSubscription;
    };
}  // namespace OIV
