#include "LabelManager.h"

#include <cmath>
#include "EventManager.h"
#include "FreeTypeWrapper/FreeTypeConnector.h"

namespace OIV
{

    LabelManager::LabelManager(FreeType::FreeTypeConnector* freeType)
    {
        fMonitorSubscription = EventManager::GetSingleton().MonitorChange.Subscribe(
            std::bind(&LabelManager::OnMonitorChange, this, std::placeholders::_1));
        fFreeType = freeType;
    }

    void LabelManager::OnMonitorChange(const EventManager::MonitorChangeEventParams& params)
    {
        SetContentScale(params.monitorDesc.contentScale);
    }

    void LabelManager::SetContentScale(const LWS::ContentScale& scale)
    {
        std::get<0>(fDPI) = static_cast<uint16_t>(std::lround(scale.x * 96.0));
        std::get<1>(fDPI) = static_cast<uint16_t>(std::lround(scale.y * 96.0));
        for (auto& [name, text] : fTextLabels)
            text->SetDPI(std::get<0>(fDPI), std::get<1>(fDPI));
    }

    void LabelManager::RemoveAll()
    {
        fTextLabels.clear();
    }

    void LabelManager::Remove(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        if (it != fTextLabels.end())
            fTextLabels.erase(it);
    }

    OIVTextImage* LabelManager::GetTextLabel(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        return it == fTextLabels.end() ? nullptr : it->second.get();
    }
    OIVTextImage* LabelManager::GetOrCreateTextLabel(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        if (it == fTextLabels.end())
        {
            it = fTextLabels.emplace(labelName, CreateTemplatedText()).first;
        }

        return it->second.get();
    }

    OIVTextImageUniquePtr LabelManager::CreateTemplatedText()
    {
        OIVTextImageUniquePtr text = std::make_unique<OIVTextImage>(fFreeType);

        text->SetPosition(LLUtils::PointF64::Zero);
        text->SetScale(LLUtils::PointF64::One);
        text->SetFilterType(OIV_Filter_type::FT_None);
        text->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
        text->SetVisible(true);
        text->SetOpacity(1.0);

        text->SetDPI(std::get<0>(fDPI), std::get<1>(fDPI));
        text->SetFontPath(sFontPath);
        text->SetFontSize(12);
        text->SetOutlineWidth(2);
        // text->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
        text->SetBackgroundColor(LLUtils::Color(0, 0, 0, 180));

        return text;
    }
}  // namespace OIV
