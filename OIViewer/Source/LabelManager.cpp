#include "LabelManager.h"

#include <cmath>
#include "FreeTypeWrapper/FreeTypeConnector.h"

namespace OIV
{

    LabelManager::LabelManager(FreeType::FreeTypeConnector* freeType, const std::filesystem::path& fontPath)
        : fFreeType(freeType), fFontPath(fontPath.native())
    {
    }

    void LabelManager::SetContentScale(const LWS::ContentScale& scale)
    {
        std::get<0>(fDPI) = static_cast<uint16_t>(std::lround(scale.x * 96.0));
        std::get<1>(fDPI) = static_cast<uint16_t>(std::lround(scale.y * 96.0));
        for (auto& [name, text] : fTextLabels)
            text->SetDPI(std::get<0>(fDPI), std::get<1>(fDPI));
    }

    void LabelManager::InitializeRenderer(Renderer& renderer)
    {
        fRenderer = &renderer;
        for (auto* label : fCreationOrder)
            label->EnsureRegistration(renderer);
    }

    void LabelManager::Prepare(Renderer& renderer)
    {
        for (auto* label : fCreationOrder)
            label->EnsureRegistration(renderer);
        for (auto* label : fCreationOrder)
            label->Prepare(renderer);
    }

    void LabelManager::RemoveAll()
    {
        fCreationOrder.clear();
        fTextLabels.clear();
    }

    void LabelManager::Remove(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        if (it != fTextLabels.end())
        {
            std::erase(fCreationOrder, it->second.get());
            fTextLabels.erase(it);
        }
    }

    TextLabel* LabelManager::GetTextLabel(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        return it == fTextLabels.end() ? nullptr : it->second.get();
    }
    TextLabel* LabelManager::GetOrCreateTextLabel(const std::string& labelName)
    {
        auto it = fTextLabels.find(labelName);
        if (it == fTextLabels.end())
        {
            auto label = CreateTemplatedText();
            if (fRenderer)
                label->EnsureRegistration(*fRenderer);
            it = fTextLabels.emplace(labelName, std::move(label)).first;
            try
            {
                fCreationOrder.push_back(it->second.get());
            }
            catch (...)
            {
                // A failed order insertion must not leave an unreachable named label.
                fTextLabels.erase(it);
                throw;
            }
        }

        return it->second.get();
    }

    std::unique_ptr<TextLabel> LabelManager::CreateTemplatedText()
    {
        std::unique_ptr<TextLabel> text = std::make_unique<TextLabel>(fFreeType);

        text->SetPosition(LLUtils::PointF64::Zero);
        text->SetScale(LLUtils::PointF64::One);
        text->SetFilterType(OIV::FilterType::None);
        text->SetImageRenderMode(OIV::ImageRenderMode::Overlay);
        text->SetVisible(true);
        text->SetOpacity(1.0);

        text->SetDPI(std::get<0>(fDPI), std::get<1>(fDPI));
        text->SetFontPath(fFontPath);
        text->SetFontSize(12);
        text->SetOutlineWidth(2);
        text->SetBackgroundColor(LLUtils::Color(0, 0, 0, 180));

        return text;
    }
}  // namespace OIV
