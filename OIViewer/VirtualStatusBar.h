#pragma once
#include "Helpers/OIVHelper.h"
#include "LabelManager.h"

namespace OIV
{
    class VirtualStatusBar
    {
    public:
        using RefreshCallback = std::function<void()>;
        

        VirtualStatusBar(LabelManager* labelManager, RefreshCallback callback)
        {
            fLabelManager = labelManager;
            fRefreshCallback = callback;
        }

        void VirtualStatusBar::Add(std::string labelName)
        {
            OIVTextImage* texelValue = fLabelManager->GetOrCreateTextLabel(labelName);
            texelValue->GetTextOptions().backgroundColor = LLUtils::Color(0_u8, 0, 0, 192).colorValue;
            texelValue->GetTextOptions().fontPath = LabelManager::sFixedFontPath;
            texelValue->GetTextOptions().fontSize = 11;
            texelValue->GetTextOptions().renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;
            texelValue->GetTextOptions().outlineWidth = 0;

            fMapLabels.emplace(labelName, texelValue);
        }


        void RepositionLabels()
        {
            const LLUtils::PointF64 sizef = static_cast<LLUtils::PointF64>(fClientSize);
            
            for (auto [name,text] : fMapLabels)
            {
                // labels placement logic, currently hard coded.
                //TOOD: make it dynamic

                if (text->GetImageProperties().opacity > 0.0) // if visible
                {
                    if (name == "texelValue")
                        text->GetImageProperties().position = { sizef.x - text->GetDescriptor().Width - 10 ,sizef.y - text->GetDescriptor().Height - 5 };
                    else if (name == "imageDescription")
                        text->GetImageProperties().position = { 10 ,sizef.y - text->GetDescriptor().Height - 5 };
                    else if (name == "texelPos")
                        text->GetImageProperties().position = { (sizef.x - text->GetDescriptor().Width) / 2  ,sizef.y - text->GetDescriptor().Height - 5 };

                    text->Update();
                }
            }

            fRefreshCallback();
        }

        void ClientSizeChanged(LLUtils::PointI32& size)
        {
            fClientSize = size;
            RepositionLabels();
        }

        void SetText(std::string elementName, const OIVString& text)
        {
            OIVTextImage* texelValue = fLabelManager->GetOrCreateTextLabel(elementName);
            texelValue->GetTextOptions().text = text;
            RepositionLabels(); // size of text is most likely changed - reposition.
        }

        void SetOpacity(std::string elementName, double opacity)
        {
            OIVTextImage* label = fLabelManager->GetOrCreateTextLabel(elementName);
            label->GetImageProperties().opacity = opacity;
            label->Update();
        }

    private:
        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        std::map<std::string, OIVTextImage*> fMapLabels;
        LabelManager* fLabelManager;
        RefreshCallback fRefreshCallback;

    };
}
