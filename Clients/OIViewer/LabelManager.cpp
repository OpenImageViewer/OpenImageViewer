#include "LabelManager.h"
#include "EventManager.h"

namespace OIV
{

    LabelManager::LabelManager()
    {
        EventManager::GetSingleton().MonitorChange.Add(std::bind(&LabelManager::OnMonitorChange, this,std::placeholders::_1));
    }

    void LabelManager::OnMonitorChange(const EventManager::MonitorChangeEventParams& params)
    {
        std::get<0>(fDPI) = params.monitorDesc.DPIx;
        std::get<1>(fDPI) = params.monitorDesc.DPIy;
        for (auto& [name, text] : fTextLabels)
        {
            text->GetTextOptions().DPIx = std::get<0>(fDPI);
            text->GetTextOptions().DPIy = std::get<1>(fDPI);
            text->Update();
        }
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
        OIVTextImageUniquePtr text = std::make_unique<OIVTextImage>();
        CreateTextParams& textOptions = text->GetTextOptions();
        reinterpret_cast<LLUtils::Color&>(textOptions.backgroundColor) = LLUtils::Color(0, 0, 0, 180);
        textOptions.fontPath = sFontPath;
        textOptions.fontSize = 12;
        textOptions.outlineWidth = 2;
        textOptions.renderMode = OIV_PROP_CreateText_Mode::CTM_AntiAliased;
        textOptions.DPIx = std::get<0>(fDPI);
        textOptions.DPIy = std::get<1>(fDPI);

        OIV_CMD_ImageProperties_Request& properties = text->GetImageProperties();
        properties.position = { 0,0 };
        properties.filterType = OIV_Filter_type::FT_None;
        properties.imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
        properties.scale = 1.0;
        properties.visible = true;
        properties.opacity = 1.0;
        return std::move(text);
    }
}
