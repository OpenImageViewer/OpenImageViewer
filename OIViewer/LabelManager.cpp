#include "LabelManager.h"
namespace OIV
{
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
        textOptions.backgroundColor = 0; // LLUtils::Color(0, 0, 0, 180).colorValue;
        textOptions.fontPath = sFontPath;
        textOptions.fontSize = 12;
        textOptions.outlineWidth = 2;
        textOptions.renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;

        OIV_CMD_ImageProperties_Request& properties = text->GetImageProperties();
        properties.position = { 0,0 };
        properties.filterType = OIV_Filter_type::FT_None;
        properties.imageRenderMode = OIV_Image_Render_mode::IRM_Overlay;
        properties.scale = 1.0;
        properties.opacity = 1.0;
        return std::move(text);
    }
}