#pragma once

#include "LabelManager.h"

#include <functional>
#include <LLUtils/StringUtility.h>
#include <map>
#include <string>

namespace OIV
{
    class VirtualStatusBar
    {
      public:

        using RefreshCallback = std::function<void()>;

        VirtualStatusBar(LabelManager* labelManager, RefreshCallback callback)
        {
            fLabelManager    = labelManager;
            fRefreshCallback = callback;
        }

        TextLabel* GetOrCreateLabel(const std::string& labelName)
        {
            auto label = fMapLabels.find(labelName);
            if (label == fMapLabels.end())
            {
                TextLabel* texelValue = fLabelManager->GetOrCreateTextLabel(labelName);
                texelValue->SetVisible(fVisible);
                texelValue->SetBackgroundColor(LLUtils::Color(0, 0, 0, 192));
                texelValue->SetLineEndFixedWidth(true);
                texelValue->SetTextRenderMode(FreeType::RenderMode::Antialiased);
                texelValue->SetFontSize(11);
                texelValue->SetTextColor({170, 170, 170, 255});
                texelValue->SetOutlineWidth(0);
                label = fMapLabels.emplace(labelName, texelValue).first;
            }

            return label->second;
        }

        void RepositionLabels()
        {
            const LLUtils::PointF64 sizef = static_cast<LLUtils::PointF64>(fClientSize);

            for (auto [name, text] : fMapLabels)
            {
                // labels placement logic, currently hard coded.
                // TOOD: make it dynamic
                const auto metrics = text->GetMetrics();

                if (metrics.pixelSize.x > 0 && text->GetOpacity() > 0.0 && text->GetVisible())  // if visible
                {
                    if (name == "texelValue")
                        text->SetPosition({sizef.x - metrics.pixelSize.x - 10, sizef.y - metrics.pixelSize.y - 5});
                    else if (name == "imageDescription")
                        text->SetPosition({10, sizef.y - metrics.pixelSize.y - 5});
                    else if (name == "texelPos")
                        text->SetPosition({(sizef.x - metrics.pixelSize.x) / 2, sizef.y - metrics.pixelSize.y - 5});
                }
            }

            fRefreshCallback();
        }

        void ClientSizeChanged(const LLUtils::PointI32& size)
        {
            fClientSize = size;
            if (GetVisible())
                RepositionLabels();
        }

        void SetText(std::string elementName, const LLUtils::native_string_type& text)
        {
            TextLabel* texelValue = GetOrCreateLabel(elementName);
            texelValue->SetText(text);

            if (GetVisible())
            {
                RepositionLabels();  // size of text is most likely changed - reposition.
            }
        }

        bool GetVisible() const { return fVisible; }

        void SetVisible(bool visible)
        {
            if (fVisible != visible)
            {
                fVisible = visible;
                for (auto [name, text] : fMapLabels)
                {
                    text->SetVisible(fVisible);
                }

                RepositionLabels();
            }
        }
        /// <summary>
        /// Sets the opacity of a label and return true if update is needed.
        /// </summary>
        /// <param name="elementName"></param>
        /// <param name="opacity"></param>
        /// <returns></returns>
        bool SetOpacity(std::string elementName, double opacity)
        {
            TextLabel* label = GetOrCreateLabel(elementName);
            if (label->GetOpacity() != opacity)
            {
                label->SetOpacity(opacity);
                return true;
            }

            return false;
        }

      private:

        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        std::map<std::string, TextLabel*> fMapLabels;
        LabelManager* fLabelManager;
        RefreshCallback fRefreshCallback;
        bool fVisible = false;
    };
}  // namespace OIV
