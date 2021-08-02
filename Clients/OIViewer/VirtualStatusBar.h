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

		OIVTextImage* GetOrCreateLabel(const std::string& labelName)
		{
			auto label = fMapLabels.find(labelName);
			if (label == fMapLabels.end())
			{
				OIVTextImage* texelValue = fLabelManager->GetOrCreateTextLabel(labelName);
                texelValue->GetImageProperties().visible = fVisible;
				texelValue->GetTextOptions().backgroundColor = LLUtils::Color(0, 0, 0, 192).colorValue;
				texelValue->GetTextOptions().fontPath = LabelManager::sFixedFontPath;
				texelValue->GetTextOptions().fontSize = 11;
				texelValue->GetTextOptions().renderMode = OIV_PROP_CreateText_Mode::CTM_SubpixelAntiAliased;
				texelValue->GetTextOptions().outlineWidth = 0;

				label = fMapLabels.emplace(labelName, texelValue).first;
			}

			return label->second;
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
            OIVTextImage* texelValue = GetOrCreateLabel(elementName);
            texelValue->GetTextOptions().text = text;

            if (GetVisible())
            {
                RepositionLabels(); // size of text is most likely changed - reposition.
            }
        }

        bool GetVisible() const
        {
            return fVisible;
        }

        void SetVisible(bool visible)
        {
            if (fVisible != visible)
            {
                fVisible = visible;
                for (auto [name, text] : fMapLabels)
                {
                    text->GetImageProperties().visible = fVisible;
                    text->Update();
                }
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
            OIVTextImage* label = GetOrCreateLabel(elementName);
            if (label->GetImageProperties().opacity != opacity)
            {
                label->GetImageProperties().opacity = opacity;
                label->Update();
                return true;
            }

            return false;
        }

    private:

        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        std::map<std::string, OIVTextImage*> fMapLabels;
        LabelManager* fLabelManager;
        RefreshCallback fRefreshCallback;
        bool fVisible = false;

    };
}
